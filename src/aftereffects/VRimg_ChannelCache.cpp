/* ---------------------------------------------------------------------
// 
// ProEXR - OpenEXR plug-ins for Photoshop and After Effects
// Copyright (c) 2007-2017,  Brendan Bolles, http://www.fnordware.com
// 
// This file is part of ProEXR.
//
// ProEXR is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// -------------------------------------------------------------------*/

#include "VRimg_ChannelCache.h"

#include <IexBaseExc.h>
#include <IlmThreadPool.h>

#include <assert.h>

#include <vector>


using namespace VRimg;
using namespace std;
using namespace Iex;
using namespace IlmThread;

extern AEGP_PluginID S_mem_id;



VRimg_ChannelCache::VRimg_ChannelCache(const SPBasicSuite *pica_basicP, const AEIO_InterruptFuncs *inter,
											InputFile &in, const IStreamPlatform &stream) :
	suites(pica_basicP),
	_path(stream.getPath()),
	_modtime(stream.getModTime())
{
    const Header &head = in.header();
	
	_width = head.width();
	_height = head.height();
	
	
	try
	{
		InputFile::BufferMap BufMap;
		
		const Header::LayerMap &layer_map = head.layers();
		
		for(Header::LayerMap::const_iterator i = layer_map.begin(); i != layer_map.end(); ++i)
		{
			const string &name = i->first;
			const Layer &layer = i->second;
			
			const size_t colbytes = sizeof(float) * layer.dimensions;
			const size_t rowbytes = colbytes * _width;
			const size_t data_size = rowbytes * _height;
			
			
			AEIO_Handle bufH = NULL;
			
			suites.MemorySuite()->AEGP_NewMemHandle(S_mem_id, "Channel Cache",
													data_size,
													AEGP_MemFlag_CLEAR, &bufH);
			
			if(bufH == NULL)
				throw NullExc("Can't allocate a channel cache handle like I need to.");
			
			
			_cache[ name ] = ChannelCache(layer.type, layer.dimensions, bufH);
			
			
			char *buf = NULL;
			
			suites.MemorySuite()->AEGP_LockMemHandle(bufH, (void**)&buf);
			
			if(buf == NULL)
				throw NullExc("Why is the locked handle NULL?");
			
			
			BufMap[ name ] = buf;
		}
		
		
		in.loadFromFile( &BufMap );
	}
	catch(IoExc) {} // we catch these so that partial files are read partially without error
	catch(InputExc) {}
	catch(...)
	{
		// out of memory or worse; kill anything that got allocated
		for(ChannelMap::iterator i = _cache.begin(); i != _cache.end(); ++i)
		{
			if(i->second.bufH != NULL)
			{
				suites.MemorySuite()->AEGP_FreeMemHandle(i->second.bufH);
				
				i->second.bufH = NULL;
			}
		}
		
		throw; // re-throw the exception
	}
	

	
	for(ChannelMap::const_iterator i = _cache.begin(); i != _cache.end(); ++i)
	{
		if(i->second.bufH != NULL)
			suites.MemorySuite()->AEGP_UnlockMemHandle(i->second.bufH);
	}
	
	
	updateCacheTime();
}


VRimg_ChannelCache::~VRimg_ChannelCache()
{
	for(ChannelMap::iterator i = _cache.begin(); i != _cache.end(); ++i)
	{
		if(i->second.bufH != NULL)
		{
			suites.MemorySuite()->AEGP_FreeMemHandle(i->second.bufH);
			
			i->second.bufH = NULL;
		}
	}
}


class CopyCacheTask : public Task
{
  public:
	CopyCacheTask(TaskGroup *group,
					const char *buf, int width, VRimg::PixelType pix_type,
					char *out_buf, size_t rowbytes, int row);
	virtual ~CopyCacheTask() {}
	
	virtual void execute();
	
	template <typename PIXTYPE>
	static void CopyRow(const char *in, char *out, int width);
	
  private:
	const char *_buf;
	int _width;
	VRimg::PixelType _pix_type;
	char *_out_buf;
	size_t _rowbytes;
	int _row;
};


CopyCacheTask::CopyCacheTask(TaskGroup *group,
								const char *buf, int width, VRimg::PixelType pix_type,
								char *out_buf, size_t rowbytes, int row) :
	Task(group),
	_buf(buf),
	_width(width),
	_pix_type(pix_type),
	_out_buf(out_buf),
	_rowbytes(rowbytes),
	_row(row)
{

}


void
CopyCacheTask::execute()
{
	const size_t pix_size =	_pix_type == VRimg::FLOAT ? sizeof(float) :
							_pix_type == VRimg::INT ? sizeof(int) :
							sizeof(float);
							
	const size_t rowbytes = pix_size * _width;
	
	const char *in_row = _buf + (rowbytes * _row);
				
	char *out_row = _out_buf + (_rowbytes * _row);
	
	if(_pix_type == VRimg::FLOAT)
	{
		CopyRow<float>(in_row, out_row, _width);
	}
	else if(_pix_type == VRimg::INT)
	{
		CopyRow<int>(in_row, out_row, _width);
	}
}


template <typename PIXTYPE>
void
CopyCacheTask::CopyRow(const char *in, char *out, int width)
{
	PIXTYPE *i = (PIXTYPE *)in;
	PIXTYPE *o = (PIXTYPE *)out;
	
	while(width--)
	{
		*o++ = *i++;
	}
}


void
VRimg_ChannelCache::copyLayerToBuffer(const string &name, void *buf, size_t rowbytes)
{
	if(_cache.find(name) == _cache.end())
		return; // don't have this layer in my cache
		
	
	vector<AEIO_Handle> locked_handles;

	if(true) // making a scope for TaskGroup
	{
		const ChannelCache &cache = _cache[ name ];
		
		if(cache.bufH == NULL)
			throw NullExc("Why is the handle NULL?");
		
		
		const char *cache_buf = NULL;
		
		suites.MemorySuite()->AEGP_LockMemHandle(cache.bufH, (void **)&cache_buf);
		
		
		if(cache_buf == NULL)
			throw NullExc("Why is the locked handle NULL?");
		
		
		locked_handles.push_back(cache.bufH);
		
		
		TaskGroup group;
		
		for(int y=0; y < _height; y++)
		{
			ThreadPool::addGlobalTask(new CopyCacheTask(&group,
													cache_buf, _width * cache.dimensions, cache.pix_type,
													(char *)buf, rowbytes, y) );
		}
	}
	
	
	for(vector<AEIO_Handle>::const_iterator i = locked_handles.begin(); i != locked_handles.end(); ++i)
	{
		suites.MemorySuite()->AEGP_UnlockMemHandle(*i);
	}
	
	
	updateCacheTime();
}


void
VRimg_ChannelCache::updateCacheTime()
{
	_last_access = time(NULL);
}


double
VRimg_ChannelCache::cacheAge() const
{
	return difftime(time(NULL), _last_access);
}


bool
VRimg_ChannelCache::cacheIsStale(int timeout) const
{
	return (cacheAge() > timeout);
}



VRimg_CachePool::VRimg_CachePool() :
	_max_caches(0),
	_pica_basicP(NULL)
{

}


VRimg_CachePool::~VRimg_CachePool()
{
	configurePool(0, NULL);
}


static bool
compare_age(const VRimg_ChannelCache *first, const VRimg_ChannelCache *second)
{
	return first->cacheAge() > second->cacheAge();
}


void
VRimg_CachePool::configurePool(int max_caches, const SPBasicSuite *pica_basicP)
{
	_max_caches = max_caches;
	
	if(pica_basicP)
		_pica_basicP = pica_basicP;
	
	_pool.sort(compare_age);

	while(_pool.size() > _max_caches)
	{
		delete _pool.front();
		
		_pool.pop_front();
	}
}


static bool
MatchDateTime(const DateTime &d1, const DateTime &d2)
{
#ifdef __APPLE__
	return (d1.fraction == d2.fraction &&
			d1.lowSeconds == d2.lowSeconds &&
			d1.highSeconds == d2.highSeconds);
#else
	return (d1.dwHighDateTime == d2.dwHighDateTime &&
			d1.dwLowDateTime == d2.dwLowDateTime);
#endif
}


VRimg_ChannelCache *
VRimg_CachePool::findCache(const IStreamPlatform &stream) const
{
	for(list<VRimg_ChannelCache *>::const_iterator i = _pool.begin(); i != _pool.end(); ++i)
	{
		if( MatchDateTime(stream.getModTime(), (*i)->getModTime()) &&
			stream.getPath() == (*i)->getPath() )
		{
			return *i;
		}
	}
	
	return NULL;
}


VRimg_ChannelCache *
VRimg_CachePool::addCache(InputFile &in, const IStreamPlatform &stream, const AEIO_InterruptFuncs *inter)
{
	_pool.sort(compare_age);
	
	while(_pool.size() && _pool.size() >= _max_caches)
	{
		delete _pool.front();
		
		_pool.pop_front();
	}
	
	if(_max_caches > 0)
	{
		try
		{
			VRimg_ChannelCache *new_cache = new VRimg_ChannelCache(_pica_basicP, inter, in, stream);
			
			_pool.push_back(new_cache);
			
			return new_cache;
		}
		catch(CancelExc) { throw; }
		catch(...) {}
	}
	
	return NULL;
}


void
VRimg_CachePool::deleteStaleCaches(int timeout)
{
	if(_pool.size() > 0)
	{
		_pool.sort(compare_age);
		
		if( _pool.front()->cacheIsStale(timeout) )
		{
			delete _pool.front(); // just going to delete one cache per cycle, the oldest one
			
			_pool.pop_front();
		}
	}
}


