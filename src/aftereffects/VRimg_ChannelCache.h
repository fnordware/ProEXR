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

#ifndef VRIMG_CHANNEL_CACHE_H
#define VRIMG_CHANNEL_CACHE_H

#include "VRimgInputFile.h"

#include "OpenEXR_PlatformIO.h"

#include "fnord_SuiteHandler.h"

#include <list>
#include <time.h>


class CancelExc : public std::exception
{
  public:
	CancelExc(A_Err err) throw() : _err(err) {}
	virtual ~CancelExc() throw() {}
	
	A_Err err() const throw() { return _err; }
	virtual const char* what() const throw() { return "User cancelled"; }

  private:
	A_Err _err;
};


class VRimg_ChannelCache
{
  public:
	VRimg_ChannelCache(const SPBasicSuite *pica_basicP, const AEIO_InterruptFuncs *inter,
							VRimg::InputFile &in, const IStreamPlatform &stream);
	~VRimg_ChannelCache();
	
	void copyLayerToBuffer(const std::string &name, void *buf, size_t rowbytes);
	
	const PathString & getPath() const { return _path; }
	DateTime getModTime() const { return _modtime; }
	
	double cacheAge() const;
	bool cacheIsStale(int timeout) const;
	
  private:
	AEGP_SuiteHandler suites;
	
	int _width;
	int _height;
	
	typedef struct ChannelCache {
		VRimg::PixelType	pix_type;
		int					dimensions;
		AEIO_Handle			bufH;
		
		ChannelCache(VRimg::PixelType t=VRimg::FLOAT, int d=1, AEIO_Handle b=NULL) : pix_type(t), dimensions(d), bufH(b) {}
	} ChannelCache;
	
	typedef std::map<std::string, ChannelCache> ChannelMap;
	ChannelMap _cache;
	
	PathString _path;
	DateTime _modtime;

	time_t _last_access;
	void updateCacheTime();
};


class VRimg_CachePool
{
  public:
	VRimg_CachePool();
	~VRimg_CachePool();
	
	void configurePool(int max_caches, const SPBasicSuite *pica_basicP=NULL);
	
	VRimg_ChannelCache *findCache(const IStreamPlatform &stream) const;
	
	VRimg_ChannelCache *addCache(VRimg::InputFile &in, const IStreamPlatform &stream, const AEIO_InterruptFuncs *inter);
	
	void deleteStaleCaches(int timeout);
	
  private:
	int _max_caches;
	const SPBasicSuite *_pica_basicP;
	std::list<VRimg_ChannelCache *> _pool;
};


#endif // VRIMG_CHANNEL_CACHE_H
