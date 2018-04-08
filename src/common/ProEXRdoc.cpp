
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

#include "ProEXRdoc.h"

#include <assert.h>

#include <Iex.h>

#include <IlmThread.h>
#include <IlmThreadPool.h>

#include <ImfStandardAttributes.h>
#include <ImfTileDescriptionAttribute.h>
#include <ImfArray.h>


using namespace Imf;
using namespace Imath;
using namespace Iex;
using namespace IlmThread;
using namespace std;


class PremultiplyRowTask : public Task
{
  public:
	PremultiplyRowTask(TaskGroup *group, float *color_row, float *alpha_row, int length);
	virtual ~PremultiplyRowTask() {}
	
	virtual void execute();

  private:
	float *_color_row;
	float *_alpha_row;
	int _length;
};

PremultiplyRowTask::PremultiplyRowTask(TaskGroup *group, float *color_row, float *alpha_row, int length) :
	Task(group),
	_color_row(color_row),
	_alpha_row(alpha_row),
	_length(length)
{

}

void
PremultiplyRowTask::execute()
{
	float *color = _color_row;
	float *alpha = _alpha_row;
	
	for(int x=0; x < _length; x++)
	{
		if(*alpha < 1.f)
			*color *= *alpha;
		
		color++;
		alpha++;
	}
}


class UnMultiplyRowTask : public Task
{
  public:
	UnMultiplyRowTask(TaskGroup *group, float *color_row, float *alpha_row, int length);
	virtual ~UnMultiplyRowTask() {}
	
	virtual void execute();

  private:
	float *_color_row;
	float *_alpha_row;
	int _length;
};

UnMultiplyRowTask::UnMultiplyRowTask(TaskGroup *group, float *color_row, float *alpha_row, int length) :
	Task(group),
	_color_row(color_row),
	_alpha_row(alpha_row),
	_length(length)
{

}

void
UnMultiplyRowTask::execute()
{
	float *color = _color_row;
	float *alpha = _alpha_row;
	
	for(int x=0; x < _length; x++)
	{
		if(*alpha > 0.f && *alpha < 1.f)
			*color /= *alpha;
		
		color++;
		alpha++;
	}
}


class AlphaClipRowTask : public Task
{
  public:
	AlphaClipRowTask(TaskGroup *group, float *alpha_row, int length);
	virtual ~AlphaClipRowTask() {}
	
	virtual void execute();

  private:
	float *_alpha_row;
	int _length;
};

AlphaClipRowTask::AlphaClipRowTask(TaskGroup *group, float *alpha_row, int length) :
	Task(group),
	_alpha_row(alpha_row),
	_length(length)
{

}

void
AlphaClipRowTask::execute()
{
	float *alpha = _alpha_row;
	
	for(int x=0; x < _length; x++)
	{
		if(*alpha < 0.f)
			*alpha = 0.f;
		else if(*alpha > 1.f)
			*alpha = 1.f;
			
		alpha++;
	}
}


class KillNaNRowTask : public Task
{
  public:
	KillNaNRowTask(TaskGroup *group, float *float_row, int length);
	virtual ~KillNaNRowTask() {}
	
	virtual void execute();

  private:
	float *_float_row;
	int _length;
};

KillNaNRowTask::KillNaNRowTask(TaskGroup *group, float *float_row, int length) :
	Task(group),
	_float_row(float_row),
	_length(length)
{

}

void
KillNaNRowTask::execute()
{
	float *pix = _float_row;
	
	for(int x=0; x < _length; x++)
	{
		KillNaN(*pix);
			
		pix++;
	}
}


class ConvertFloatRowTask : public Task
{
  public:
	ConvertFloatRowTask(TaskGroup *group, float *input_row, half *output_row, int length);
	virtual ~ConvertFloatRowTask() {}
	
	virtual void execute();

  private:
	float *_input_row;
	half *_output_row;
	int _length;
};

ConvertFloatRowTask::ConvertFloatRowTask(TaskGroup *group, float *input_row, half *output_row, int length) :
	Task(group),
	_input_row(input_row),
	_output_row(output_row),
	_length(length)
{

}

void
ConvertFloatRowTask::execute()
{
	float *in = _input_row;
	half *out = _output_row;
	
	for(int x=0; x < _length; x++)
	{
		*out++ = *in++;
	}
}

static int
ScanlineBlockSize(const HybridInputFile &in)
{
	int block_size = 128;
	
	if(in.parts() > 1)
	{
		// read the whole thing to minimize bouncing around
		const Box2i &dw = in.dataWindow();
		
		block_size = dw.max.y - dw.min.y + 1;
	}
	else if( isTiled( in.version() ) )
	{
		const TileDescriptionAttribute *tiles = in.header(0).findTypedAttribute<TileDescriptionAttribute>("tiles");
		
		if(tiles)
		{
			block_size = tiles->value().ySize;
			
			const Box2i &dataW = in.dataWindow();
			const Box2i &dispW = in.displayWindow();
			
			if(dataW != dispW)
				block_size *= 2; // might not be on tile boundries
		}
	}
	
	return block_size;
}

#pragma mark-

ProEXRchannel::ProEXRchannel(string name, Imf::PixelType pixelType) :
	_name(name),
	_pixelType(pixelType),
	_doc(NULL),
	_loaded(false),
	_premultiplied(true),
	_width(0),
	_height(0),
	_data(NULL),
	_half_data(NULL),
	_rowbytes(0),
	_half_rowbytes(0)
{

}

ProEXRchannel::~ProEXRchannel()
{
	freeBuffers();
}

string
ProEXRchannel::layerName() const
{
	assert( channelType() != CHANNEL_RESERVED);
	
	if( channelType() == CHANNEL_LAYER )
	{
		string layerName, channelName;
		
		channelParts(layerName, channelName);
		
		return layerName;
	}
	else if( channelType() == CHANNEL_SINGLE )
	{
		return string("[") + _name + string("]");
	}
	else
		return _name;
}

string
ProEXRchannel::channelName() const
{
	if( channelType() == CHANNEL_LAYER )
	{
		string layerName, channelName;
		
		channelParts(layerName, channelName);
		
		return channelName;
	}
	else
		return _name;
}

void
ProEXRchannel::incrementName()
{
	if( channelType() == CHANNEL_LAYER )
	{
		string layerName, channelName;
		
		channelParts(layerName, channelName);
		
		incrementText(layerName);
		
		_name = layerName + "." + channelName;
	}
	else if( channelType() == CHANNEL_SINGLE )
	{
		incrementText(_name);
	}
	else // reserved, I guess
	{
		_name = "layer1." + _name;
	}
}

ChanTag
ProEXRchannel::channelTag() const
{
	string n = channelName();
	
	     if(n == "R" || n == "r" || n == "RED" || n == "Red" || n == "red") return CHAN_R;
	else if(n == "G" || n == "g" || n == "GREEN" || n == "Green" || n == "green") return CHAN_G;
	else if(n == "B" || n == "b" || n == "BLUE" || n == "Blue" || n == "blue") return CHAN_B;
	else if(n == "A" || n == "a" || n == "ALPHA" || n == "Alpha" || n == "alpha") return CHAN_A;
	else if(n == "Y") return CHAN_Y;
	else if(n == "RY") return CHAN_RY;
	else if(n == "BY") return CHAN_BY;
	else if(n == "AR") return CHAN_AR;
	else if(n == "AG") return CHAN_AG;
	else if(n == "AB") return CHAN_AB;
	else
		return CHAN_GENERAL;
}

ChannelType
ProEXRchannel::channelType() const
{
	if(	_name == "R" ||	_name == "G" ||	_name == "B" ||	_name == "A" ||
		_name == "Y" || _name == "RY"||	_name == "BY"||
		_name == "AR"||	_name == "AG"||	_name == "AB"	)
	{
		return CHANNEL_RESERVED;
	}
	else
	{
		string layerName, channelName;
		
		channelParts(layerName, channelName);
		
		if(layerName == "")
		{
			// this channel not part of a layer
			return CHANNEL_SINGLE;
		}
		else
			return CHANNEL_LAYER;
	}
}

void
ProEXRchannel::assignDoc(ProEXRdoc *doc)
{
	_doc = doc;
	
	_width = doc->width();
	_height = doc->height();
}

void
ProEXRchannel::allocateBuffers(bool allocate_half)
{
	assert(_width && _height);
	
	// we always allocate the full-size FLOAT/UINT buffer
	// only allocate half if we have to (usually for writing half channels only)
	
	size_t colbytes, half_colbytes;
	size_t buf_size, half_buf_size;
	
	colbytes = 4;
	_rowbytes = colbytes * _width;
	buf_size = _rowbytes * _height;
	
	if(allocate_half)
	{
		assert(_pixelType == Imf::HALF);
		
		half_colbytes = 2;
		_half_rowbytes = half_colbytes * _width;
		half_buf_size = _half_rowbytes * _height;
	}
	else
	{
		half_colbytes = 0;
		_half_rowbytes = 0;
		half_buf_size = 0;
	}
	
	queryAbort();
	
	if(_data == NULL)
	{
		_data = malloc(buf_size);
		
		if(_data == NULL)
			throw bad_alloc();
			
		memset(_data, 0, buf_size);
	}
	
	queryAbort();
	
	if(half_buf_size && _half_data == NULL)
	{
		_half_data = malloc(half_buf_size);
		
		if(_half_data == NULL)
			throw bad_alloc();
			
		memset(_half_data, 0, half_buf_size);
	}
	
	queryAbort();
}

void
ProEXRchannel::freeBuffers()
{
	if(_data)
	{
		free(_data);
		_data = NULL;
		_rowbytes = 0;
	}
	
	if(_half_data)
	{
		free(_half_data);
		_half_data = NULL;
		_half_rowbytes = 0;
	}
	
	_loaded = false;
}


ProEXRbuffer
ProEXRchannel::getBufferDesc(bool use_half)
{
	// allocate half if we're getting a float buffer that will
	// get converted to half for writing
	assert(_width && _height);
	 
	if(_data == NULL || (use_half && _half_data == NULL) )
		allocateBuffers(use_half && _pixelType == Imf::HALF);
	
	assert(_data);
	
	if(_pixelType == Imf::HALF && use_half)
	{
		assert(_half_data);
		assert(_loaded);
		
		copyToHalf();
		
		ProEXRbuffer desc = { _pixelType, _half_data, _width, _height, sizeof(half), _half_rowbytes };
		
		return desc;
	}
	else
	{
		Imf::PixelType pixelType = (_pixelType == Imf::UINT ? Imf::UINT : Imf::FLOAT);
	
		ProEXRbuffer desc = { pixelType, _data, _width, _height, sizeof(float), _rowbytes };
		
		return desc;
	}
}

void
ProEXRchannel::fill(float val)
{
	if(_data == NULL)
		allocateBuffers();
		
	char *buf_row = (char *)_data;
	
	for(int y=0; y < _height; y++)
	{
		float *buf_pix = (float *)buf_row;
		
		for(int x=0; x < _width; x++)
			*buf_pix++ = val;
		
		buf_row += _rowbytes;
	}
}

void
ProEXRchannel::premultiply(ProEXRchannel *alpha, bool force)
{
	if(_loaded && alpha && alpha->_data && _data)
	{
		assert(_width == alpha->_width);
		assert(_height == alpha->_height);
		assert(alpha->_pixelType != Imf::UINT);
		
		if(_pixelType != Imf::UINT)
		{
			if(!_premultiplied || force)
			{
				TaskGroup taskGroup;
				
				char *buf_row = (char *)_data;
				char *alpha_row = (char *)alpha->_data;
				
				for(int y=0; y < _height; y++)
				{
					ThreadPool::addGlobalTask(new PremultiplyRowTask(&taskGroup,
																		(float *)buf_row,
																		(float *)alpha_row,
																		_width) );
					buf_row += _rowbytes;
					alpha_row += alpha->_rowbytes;
				}
				
				_premultiplied = true;
			}
		}
	}
}

void
ProEXRchannel::unMult(ProEXRchannel *alpha)
{
	if(_loaded && alpha && alpha->_data && _data)
	{
		assert(_width == alpha->_width);
		assert(_height == alpha->_height);
		assert(alpha->_pixelType != Imf::UINT);
		
		if(_pixelType != Imf::UINT)
		{
			if(_premultiplied)
			{
				TaskGroup taskGroup;
				
				char *buf_row = (char *)_data;
				char *alpha_row = (char *)alpha->_data;
				
				for(int y=0; y < _height; y++)
				{
					ThreadPool::addGlobalTask(new UnMultiplyRowTask(&taskGroup,
																		(float *)buf_row,
																		(float *)alpha_row,
																		_width) );
					buf_row += _rowbytes;
					alpha_row += alpha->_rowbytes;
				}
				
				_premultiplied = false;
			}
		}
		
		queryAbort();
	}
}

void
ProEXRchannel::alphaClip()
{
	assert(channelTag() == CHAN_A);

	if(_loaded && _data)
	{
		if(_pixelType != Imf::UINT)
		{
			TaskGroup taskGroup;
			
			char *buf_row = (char *)_data;
			
			for(int y=0; y < _height; y++)
			{
				ThreadPool::addGlobalTask(new AlphaClipRowTask(&taskGroup,
																(float *)buf_row,
																_width) );
				buf_row += _rowbytes;
			}
		}
		
		queryAbort();
	}
}

void
ProEXRchannel::killNaN()
{
	if(_loaded && _data)
	{
		TaskGroup taskGroup;
		
		char *buf_row = (char *)_data;
		
		for(int y=0; y < _height; y++)
		{
			ThreadPool::addGlobalTask(new KillNaNRowTask(&taskGroup,
															(float *)buf_row,
															_width) );
			buf_row += _rowbytes;
		}
		
		queryAbort();
	}
}

void
ProEXRchannel::queryAbort()
{
	if(_doc)
		_doc->queryAbort();
}

void
ProEXRchannel::channelParts(string &layerName, string &channelName) const
{
	int dot_pos = _name.find_last_of('.');//, _name.size()-1);
	
	if(dot_pos == string::npos || dot_pos == 0 || dot_pos == _name.size()-1)
	{
		layerName = "";
		channelName = _name;
	}
	else
	{
		layerName = _name.substr(0, dot_pos);
		channelName = _name.substr(dot_pos + 1, _name.size() - dot_pos);
	}
}

void
ProEXRchannel::incrementText(string &name)
{
	int num_numbers = 0;
	
	while(string::npos != name.find_last_of("1234567890", name.size() - num_numbers - 1, 1) )
		num_numbers++;
	
	if(num_numbers)
	{
		string base = name.substr(0, name.size() - num_numbers);
		string num = name.substr(name.size() - num_numbers, num_numbers);
		
		int n = 0;
		
		istringstream num_ist( num );
		num_ist >> n;
		
		n++;
		
		ostringstream num_ost;
		num_ost << n;
		string new_num = num_ost.str();
		
		name = base + new_num;
	}
	else
		name += "2";
}

void
ProEXRchannel::copyToHalf()
{
	if(_data && _half_data)
	{
		if(_width == 0 || _height == 0)
			throw BaseExc("Image has no size.");
		
		TaskGroup taskGroup;
		
		char *float_row = (char *)_data;
		char *half_row = (char *)_half_data;
		
		for(int y=0; y < _height; y++)
		{
			ThreadPool::addGlobalTask(new ConvertFloatRowTask(&taskGroup,
																(float *)float_row,
																(half *)half_row,
																_width) );
			float_row += _rowbytes;
			half_row += _half_rowbytes;
		}
	}
	
	queryAbort();
}

ProEXRchannel_read::ProEXRchannel_read(string name, Imf::PixelType pixelType) :
	ProEXRchannel(name, pixelType)
{

}

ProEXRchannel_read::~ProEXRchannel_read()
{

}

void
ProEXRchannel_read::loadFromFile()
{
	if(doc() == NULL)
		throw BaseExc("doc() is NULL");
	
	assert( !loaded() );
	
	ProEXRdoc_read &read_doc = dynamic_cast<ProEXRdoc_read &>( *doc() );
	
	ProEXRbuffer buf = getBufferDesc(false);
			
	// EXR calls
	const Box2i &dw = read_doc.file().dataWindow();
	
	assert(buf.width == (dw.max.x - dw.min.x) + 1);
	assert(buf.height == (dw.max.y - dw.min.y) + 1);
	
	FrameBuffer frameBuffer;
	
	if(buf.type == Imf::UINT)
	{
		char *exr_buf_origin = (char *)buf.buf - (sizeof(unsigned int) * dw.min.x) - (buf.rowbytes * dw.min.y);
	
		frameBuffer.insert(name().c_str(), Slice(Imf::UINT, exr_buf_origin, buf.colbytes, buf.rowbytes, 1, 1, 0.0f));
	}
	else
	{
		float fill_val = (channelTag() == CHAN_A ? 1.0f : 0.0f);
		
		char *exr_buf_origin = (char *)buf.buf - (sizeof(float) * dw.min.x) - (buf.rowbytes * dw.min.y);
	
		frameBuffer.insert(name().c_str(), Slice(Imf::FLOAT, exr_buf_origin, buf.colbytes, buf.rowbytes, 1, 1, fill_val));
	}

	// now read the file, pausing to abort if asked
	HybridInputFile &in_file = read_doc.file();
	
	in_file.setFrameBuffer(frameBuffer);
	
	try{
		int block_size = ScanlineBlockSize( read_doc.file() );
	
		int y = dw.min.y;
		
		while(y <= dw.max.y)
		{
			const int high_scanline = MIN(y + block_size - 1, dw.max.y);
			
			in_file.readPixels(y, high_scanline);
			
			y = high_scanline + 1;
			
			queryAbort();
		}
	}
	catch(Iex::InputExc) {}
	catch(Iex::IoExc) {}
	
	setLoaded(true);
	
	killNaN();
	
	// clip alpha
	if(channelTag() == CHAN_A && read_doc.getClipAlpha())
		alphaClip();
}

ProEXRlayer::ProEXRlayer(string name) :
	_name(name),
	_alpha(NULL),
	_doc(NULL)
{

}

ProEXRlayer::~ProEXRlayer()
{

}

string
ProEXRlayer::name() const
{
	if(_name == "")
	{
		assert( channels().size() );
		
		const vector<ProEXRchannel *> &chans = channels();
		
		if(chans[0]->channelType() == CHANNEL_LAYER)
		{
			return chans[0]->layerName();
		}
		else
			return ps_name();
	}
	else
		return _name;
}

static void
searchReplaceEnd(string &str, const string &search, const string &replace)
{
	if(str.size() >= search.size() && str.substr(str.size() - search.size()) == search)
	{
		str.erase(str.size() - search.size());
		
		str.append(replace);
	}
}

string
ProEXRlayer::ps_name() const
{
	if(_name == "")
	{
		assert( channels().size() );
		
		const vector<ProEXRchannel *> &chans = channels();
		
		if(chans[0]->channelType() == CHANNEL_LAYER)
		{
			string ps_name = chans[0]->layerName();
			
			ps_name += ".";
			
			string channel_text("");
			
			for(int i=0; i < chans.size(); i++)
			{
				string chan_name = chans[i]->channelName();
				
				channel_text += string("[") + chan_name + string("]");
			}
			
			ps_name += channel_text;
			
			// in general, multiple channels in a layer will be indicated by
			// layer.[chan][other][another] at the end
			// we will only make exceptions for layer.[R][G][B]([A])
			searchReplaceEnd(ps_name, "[R][G][B][A]", "RGBA");
			searchReplaceEnd(ps_name, "[R][G][B]", "RGB");
			
			return ps_name;
		}
		else
		{
			string layer_name("");
			
			for(vector<ProEXRchannel *>::const_iterator i = chans.begin(); i != chans.end(); ++i)
			{
				ProEXRchannel *c = *i;
				
				string chan_name = c->channelName();
				
				layer_name += "[" + c->name() + "]";
			}
			
			searchReplaceEnd(layer_name, "[R][G][B][A]", "RGBA");
			searchReplaceEnd(layer_name, "[R][G][B]", "RGB");
			
			return layer_name;
		}
	}
	else
		return _name;
}

void
ProEXRlayer::incrementChannels()
{
	for(vector<ProEXRchannel *>::iterator i = channels().begin(); i != channels().end(); ++i)
	{
		(*i)->incrementName();
	}
}

ProEXRchannel *
ProEXRlayer::findChannel(string channelName) const
{
	for(vector<ProEXRchannel *>::const_iterator i = channels().begin(); i != channels().end(); ++i)
	{
		if( (*i)->name() == channelName )
			return *i;
	}
	
	return NULL;
}

void
ProEXRlayer::addChannel(ProEXRchannel *channel, bool sort)
{
	if(channel)
	{
		channels().push_back(channel);
		
		if(sort)
			sortChannels();
	}
}

ProEXRchannel *
ProEXRlayer::alphaChannel() const
{
	if(_alpha != NULL)
		return _alpha;
	
	for(vector<ProEXRchannel *>::const_iterator i = channels().begin(); i != channels().end(); ++i)
	{
		ProEXRchannel *chan = *i;
		
		if(chan->channelTag() == CHAN_A)
			return chan;
	}
	
	return NULL;
}

void
ProEXRlayer::assignAlpha(ProEXRchannel *externalAlpha, bool force)
{
	if(force) // forced
	{
		_alpha = externalAlpha;
	}
	else if(_alpha == NULL)
	{
		_alpha = alphaChannel(); // from internal layer
		
		if(_alpha == NULL)
		{
			_alpha = externalAlpha; // from external source
		}
	}
}

vector<ProEXRchannel *>
ProEXRlayer::getNonAlphaChannels() const
{
	vector<ProEXRchannel *> chans;
	
	for(vector<ProEXRchannel *>::const_iterator i = channels().begin(); i != channels().end(); ++i)
	{
		ProEXRchannel *chan = *i;
		
		if(chan->channelTag() != CHAN_A)
			chans.push_back(chan);
	}
	
	return chans;
}

void ProEXRlayer::assignDoc(ProEXRdoc *doc)
{
	_doc = doc;
	
	for(vector<ProEXRchannel *>::iterator i = channels().begin(); i != channels().end(); ++i)
	{
		(*i)->assignDoc(doc);
	}
}

bool
ProEXRlayer::loaded() const
{
	bool loaded = true;

	for(vector<ProEXRchannel *>::const_iterator j = channels().begin(); j != channels().end() && loaded; ++j)
	{
		loaded = (*j)->loaded();
	}
	
	return loaded;
}

void
ProEXRlayer::premultiply(ProEXRchannel *shared_alpha)
{
	assert( channels().size() );
	
	ProEXRchannel *alpha = alphaChannel();
	
	vector<ProEXRchannel *> my_channels = channels(); // working copy
	
	// don't act on alphas
	for(vector<ProEXRchannel *>::iterator i = my_channels.begin(); i != my_channels.end(); ++i)
	{
		if( (*i)->channelTag() == CHAN_A )
		{
			assert(alpha != NULL); // because we would have found it already
			my_channels.erase(i); // take out of the working copy
			break;
		}
	}
	
	if(alpha == NULL)
		alpha = shared_alpha;
	
	if(alpha)
	{
		for(vector<ProEXRchannel *>::iterator i = my_channels.begin(); i != my_channels.end(); ++i)
			(*i)->premultiply(alpha);
	}
}

void
ProEXRlayer::unMult(ProEXRchannel *shared_alpha)
{
	assert( channels().size() );
	
	ProEXRchannel *alpha = alphaChannel();
	
	vector<ProEXRchannel *>my_channels = channels(); // working copy
	
	// don't act on alphas
	for(vector<ProEXRchannel *>::iterator i = my_channels.begin(); i != my_channels.end(); ++i)
	{
		if( (*i)->channelTag() == CHAN_A )
		{
			assert(alpha != NULL); // because we would have found it already
			my_channels.erase(i); // take out of the working copy
			break;
		}
	}
	
	if(alpha == NULL)
		alpha = shared_alpha;
	
	if(alpha)
	{
		for(vector<ProEXRchannel *>::iterator i = my_channels.begin(); i != my_channels.end(); ++i)
			(*i)->unMult(alpha);
	}
}

void
ProEXRlayer::freeBuffers()
{
	for(vector<ProEXRchannel *>::iterator j = channels().begin(); j != channels().end(); ++j)
	{
		(*j)->freeBuffers();
	}
}

void
ProEXRlayer::sortChannels()
{
	vector<ProEXRchannel *> &chans = channels();

	for(int i=0; i < chans.size() - 1; i++)
	{
		if( (
			(chans[i]->channelTag() == CHAN_GENERAL || chans[i+1]->channelTag() == CHAN_GENERAL) && // untagged case
			(chans[i]->channelName() > chans[i+1]->channelName() )
			)
			||
			(
			(chans[i]->channelTag() != CHAN_GENERAL && chans[i+1]->channelTag() != CHAN_GENERAL) && // tagged case
			(chans[i]->channelTag() > chans[i+1]->channelTag() )
			)
		)
		{
			// swap
			ProEXRchannel *temp = chans[i];
			
			chans[i] = chans[i+1];
			chans[i+1] = temp;
			
			i = -1; // start over
		}
	}
}

void
ProEXRlayer::queryAbort()
{
	if(_doc)
		_doc->queryAbort();
}


ProEXRlayer_read::ProEXRlayer_read(string name) :
	ProEXRlayer(name),
	_load_as_layer(false)
{
	
}

ProEXRlayer_read::~ProEXRlayer_read()
{

}

void
ProEXRlayer_read::loadFromFile()
{
	if(doc() == NULL)
		throw BaseExc("doc() is NULL");
	
	ProEXRdoc_read &read_doc = dynamic_cast<ProEXRdoc_read &>( *doc() );
	
	if( loadAsLayer() ) // aka, Y[RY][BY]
	{
		loadFromRGBA();
	}
	else
	{
		HybridInputFile &in_file = read_doc.file();
		
		try{
			vector<ProEXRchannel *> alphas_to_clip;
		
			// EXR calls
			Box2i dw = read_doc.file().dataWindow();
			
			FrameBuffer frameBuffer;
			
			for(vector<ProEXRchannel *>::iterator i = channels().begin(); i != channels().end(); ++i)
			{
				ProEXRchannel *chan = *i;
				
				if( chan->loaded() == false )
				{
					ProEXRbuffer desc = chan->getBufferDesc(false);
						
					char *exr_buf_origin = (char *)desc.buf - (desc.colbytes * dw.min.x) - (desc.rowbytes * dw.min.y);

					float fillValue = (chan->channelTag() == CHAN_A ? 1.f : 0.f);

					frameBuffer.insert(chan->name().c_str(), Slice(desc.type, exr_buf_origin, desc.colbytes, desc.rowbytes, 1, 1, fillValue));
					
					chan->setLoaded(true); // yes, I know we haven't actually done it yet
					
					if(chan->channelTag() == CHAN_A && read_doc.getClipAlpha())
						alphas_to_clip.push_back(chan);
				}
			}

			if(frameBuffer.begin() != frameBuffer.end()) // i.e. not empty
			{
				in_file.setFrameBuffer(frameBuffer);
						
				try{
					const int block_size = ScanlineBlockSize( read_doc.file() );
				
					int y = dw.min.y;
					
					while(y <= dw.max.y)
					{
						int high_scanline = MIN(y + block_size - 1, dw.max.y);
						
						in_file.readPixels(y, high_scanline);
						
						y = high_scanline + 1;
						
						queryAbort();
					}
				}
				catch(Iex::InputExc) {}
				catch(Iex::IoExc) {}
			}
			
			// kill NaN
			for(vector<ProEXRchannel *>::iterator i = channels().begin(); i != channels().end(); ++i)
			{
				ProEXRchannel *chan = *i;
				
				chan->killNaN();
			}
			
			// clip alpha
			for(vector<ProEXRchannel *>::iterator i = alphas_to_clip.begin(); i != alphas_to_clip.end(); ++i)
			{
				ProEXRchannel *chan = *i;
				
				chan->alphaClip();
			}
		}
		catch(bad_alloc)
		{
			// we ran out of memory, unload everything
			freeBuffers();
		}
	}
}

void
ProEXRlayer_read::loadFromRGBA()
{
	try{
		if(doc() == NULL)
			throw BaseExc("doc() is NULL.");
	
		ProEXRdoc_read &read_doc = dynamic_cast<ProEXRdoc_read &>( *doc() );
		
		vector<ProEXRchannel *> &chans = channels();
		
		sortChannels();
		
		assert(chans.size() >= 3);
		assert(chans[0]->name() == "Y");
		assert(chans[1]->name() == "RY");
		assert(chans[2]->name() == "BY");
		
		bool have_a = (chans.size() >= 4 && chans[3]->name() == "A");
		
		ProEXRbuffer r_desc = chans[0]->getBufferDesc(); // should also allocate float buffer
		ProEXRbuffer g_desc = chans[1]->getBufferDesc();
		ProEXRbuffer b_desc = chans[2]->getBufferDesc();
		
		ProEXRbuffer a_desc = {Imf::HALF, NULL, 0, 0, 0, 0};
		
		if(have_a)
			a_desc = chans[3]->getBufferDesc();
			
		int buf_width = r_desc.width;
		int buf_height = r_desc.height;
		
		assert(r_desc.width == g_desc.width && g_desc.width == b_desc.width);
		assert(r_desc.height == g_desc.height && g_desc.height == b_desc.height);
		
		// now use the Rgba interface
		Imf::IStream &file_in = read_doc.stream();
		file_in.seekg(0); // expected to be at the beginning, apparently
		
		RgbaInputFile inputFile( file_in );
		
		Box2i dw = inputFile.dataWindow();
		int width = (dw.max.x - dw.min.x) + 1;
		int height = (dw.max.y - dw.min.y) + 1;
		
		Array2D<Rgba> half_buffer(height, width);
		
		inputFile.setFrameBuffer(&half_buffer[-dw.min.y][-dw.min.x], 1, width);
		
		try{
			int y = dw.min.y;
			
			while(y <= dw.max.y)
			{
				int high_scanline = MIN(y + 127, dw.max.y);
				
				inputFile.readPixels(y, high_scanline);
				
				y = high_scanline + 1;
				
				queryAbort();
			}
		}
		catch(Iex::InputExc) {}
		catch(Iex::IoExc) {}
		
		// copy from big half buffer to our channel buffers
		char *r_row = (char *)r_desc.buf;
		char *g_row = (char *)g_desc.buf;
		char *b_row = (char *)b_desc.buf;
		char *a_row = (char *)a_desc.buf;
		
		for(int y=0; y < buf_height; y++)
		{
			float *r_pix = (float *)r_row;
			float *g_pix = (float *)g_row;
			float *b_pix = (float *)b_row;
			float *a_pix = (float *)a_row;
			
			for(int x=0; x < buf_width; x++)
			{
				*r_pix++ = half_buffer[y][x].r;
				*g_pix++ = half_buffer[y][x].g;
				*b_pix++ = half_buffer[y][x].b;
				
				if(have_a)
					*a_pix++ = half_buffer[y][x].a;
			}
			
			r_row += r_desc.rowbytes;
			g_row += g_desc.rowbytes;
			b_row += b_desc.rowbytes;
			a_row += a_desc.rowbytes;
		}
		
		// mark as loaded
		chans[0]->setLoaded(true);
		chans[1]->setLoaded(true);
		chans[2]->setLoaded(true);
		
		if(have_a)
			chans[3]->setLoaded(true);
		
		// kill NaN
		chans[0]->killNaN();
		chans[1]->killNaN();
		chans[2]->killNaN();
		
		if(have_a)
			chans[3]->killNaN();
		
		// clip alpha
		if(have_a && read_doc.getClipAlpha())
			chans[3]->alphaClip();
		
		queryAbort();
	}
	catch(bad_alloc)
	{
		// we ran out of memory, unload everything
		freeBuffers();
	}
}

ProEXRdoc::ProEXRdoc() :
	_black_channel(NULL),
	_white_channel(NULL)
{

}

ProEXRdoc::~ProEXRdoc()
{
	if(_black_channel) delete _black_channel;
	if(_white_channel) delete _white_channel;
	
	for(vector<ProEXRlayer *>::iterator i = layers().begin(); i != layers().end(); ++i)
	{
		if(*i)
		{
			delete *i;
			*i = NULL;
		}
	}

	for(vector<ProEXRchannel *>::iterator i = channels().begin(); i != channels().end(); ++i)
	{
		if(*i)
		{
			delete *i;
			*i = NULL;
		}
	}
}

ProEXRchannel *
ProEXRdoc::getAlphaChannel() const
{
	return findChannel("A");
}

ProEXRchannel *
ProEXRdoc::findChannel(string channelName) const
{
	for(vector<ProEXRchannel *>::const_iterator i = channels().begin(); i != channels().end(); ++i)
	{
		if( (*i)->name() == channelName )
			return *i;
	}
	
	return NULL;
}

ProEXRlayer *
ProEXRdoc::findLayer(string layerName) const
{
	for(vector<ProEXRlayer *>::const_iterator i = layers().begin(); i != layers().end(); ++i)
	{
		if( (*i)->name() == layerName )
			return *i;
	}
	
	for(vector<ProEXRlayer *>::const_iterator i = cryptoLayers().begin(); i != cryptoLayers().end(); ++i)
	{
		if( layerName.size() > 2 && (*i)->name() == layerName.substr(0, layerName.size() - 2) )
			return *i;
	}
	
	return NULL;
}

ProEXRlayer *
ProEXRdoc::findMainLayer(bool be_flexible, bool force) const
{
	assert( layers().size() );
	
	if( findLayer("RGB") )				return findLayer("RGB");
	if( findLayer("RGBA") )				return findLayer("RGBA");
	if( findLayer("Y") )				return findLayer("Y");
	if( findLayer("[Y]") )				return findLayer("[Y]");
	if( findLayer("YA") )				return findLayer("YA");
	if( findLayer("[Y][A]") )			return findLayer("[Y][A]");
	if( findLayer("[Y][RY][BY]") )		return findLayer("[Y][RY][BY]");
	if( findLayer("[Y][RY][BY][A]") )	return findLayer("[Y][RY][BY][A]");
	
	
	if(be_flexible)
	{
		// ok, then we'll look for a layer that has at least one standard channel
		vector<string> standard_channels;
		
		standard_channels.push_back("R");
		standard_channels.push_back("G");
		standard_channels.push_back("B");
		standard_channels.push_back("A");
		
		for(vector<string>::const_iterator j = standard_channels.begin(); j != standard_channels.end(); ++j)
		{
			for(vector<ProEXRlayer *>::const_iterator i = layers().begin(); i != layers().end(); ++i)
			{
				if( (*i)->findChannel( *j ) != NULL )
					return *i;
			}
		}
	}
	
	// ummm, ok, forced to come up with a layer of some kind?
	if(force && layers().size())
		return layers().at(0);
	else
		return NULL;
}

bool
ProEXRdoc::loaded() const
{
	bool loaded = true;

	for(vector<ProEXRchannel *>::const_iterator j = channels().begin(); j != channels().end() && loaded; ++j)
	{
		if( !(*j)->loaded() )
			loaded = false;
	}
	
	return loaded;
}

Int64
ProEXRdoc::memorySize() const
{
	return ( (Int64)sizeof(float) * (Int64)width() * (Int64)height() * (Int64)channels().size() );
}

void
ProEXRdoc::freeBuffers() const
{
	for(vector<ProEXRchannel *>::const_iterator j = channels().begin(); j != channels().end(); ++j)
	{
		(*j)->freeBuffers();
	}
}

void
ProEXRdoc::premultiply()
{
	for(vector<ProEXRlayer *>::iterator i = layers().begin(); i != layers().end(); ++i)
		(*i)->premultiply( getAlphaChannel() );
}

void
ProEXRdoc::unMult()
{
	for(vector<ProEXRlayer *>::iterator i = layers().begin(); i != layers().end(); ++i)
		(*i)->unMult( getAlphaChannel() );
}

ProEXRdoc_read::ProEXRdoc_read(Imf::IStream &is, bool clip_alpha, bool renameFirstPart, bool set_up) :
	_in_stream(is),
	_in_file(is, renameFirstPart),
	_clipAlpha(clip_alpha)
{
	if(set_up)
		setupDoc();
}

ProEXRdoc_read::~ProEXRdoc_read()
{

}

int
ProEXRdoc_read::width() const
{
	const Box2i &dw = file().dataWindow();
			
	return ((dw.max.x - dw.min.x) + 1);
}

int
ProEXRdoc_read::height() const
{
	const Box2i &dw = file().dataWindow();
	
	return ((dw.max.y - dw.min.y) + 1);
}

void
ProEXRdoc_read::loadFromFile()
{
	try{
		vector<ProEXRchannel *> alphas_to_clip;
		
		// EXR calls
		const Box2i &dw = file().dataWindow();
		
		FrameBuffer frameBuffer;

		vector<ProEXRlayer_read *> layers_to_load;

		for(vector<ProEXRlayer *>::iterator i = layers().begin(); i != layers().end(); ++i)
		{
			ProEXRlayer_read &read_layer = dynamic_cast<ProEXRlayer_read &>( **i );
			
			if( read_layer.loadAsLayer() )
			{
				layers_to_load.push_back(&read_layer);
			}
			else
			{
				vector<ProEXRchannel *> &chans = read_layer.channels();
			
				for(vector<ProEXRchannel *>::iterator j = chans.begin(); j != chans.end(); ++j)
				{
					ProEXRchannel *chan = *j;
					
					if( chan->loaded() == false )
					{
						const bool this_is_alpha = (chan->channelTag() == CHAN_A);
					
						ProEXRbuffer desc = chan->getBufferDesc(false);
							
						char *exr_buf_origin = (char *)desc.buf - (desc.colbytes * dw.min.x) - (desc.rowbytes * dw.min.y);

						float fillValue = (this_is_alpha ? 1.0f : 0.0f);

						frameBuffer.insert(chan->name().c_str(), Slice(desc.type, exr_buf_origin, desc.colbytes, desc.rowbytes, 1, 1, fillValue));
						
						chan->setLoaded(true); // yes, I know we haven't actually done it yet
						
						if(this_is_alpha && getClipAlpha())
							alphas_to_clip.push_back(chan);
					}
				}
			}
		}

		if(frameBuffer.begin() != frameBuffer.end()) // i.e. not empty
		{
			file().setFrameBuffer(frameBuffer);
					
			try{
				const int block_size = ScanlineBlockSize( file() );
				
				int y = dw.min.y;
				
				while(y <= dw.max.y)
				{
					int high_scanline = MIN(y + block_size - 1, dw.max.y);
					
					file().readPixels(y, high_scanline);
					
					y = high_scanline + 1;
					
					queryAbort();
				}
			}
			catch(Iex::InputExc) {}
			catch(Iex::IoExc) {}
		}
		
		// kill NaN
		for(vector<ProEXRlayer *>::iterator i = layers().begin(); i != layers().end(); ++i)
		{
			ProEXRlayer_read &read_layer = dynamic_cast<ProEXRlayer_read &>( **i );
			
			vector<ProEXRchannel *> &chans = read_layer.channels();
		
			for(vector<ProEXRchannel *>::iterator j = chans.begin(); j != chans.end(); ++j)
			{
				ProEXRchannel *chan = *j;
				
				chan->killNaN();
			}
		}
		
		// clip alpha
		for(vector<ProEXRchannel *>::iterator i = alphas_to_clip.begin(); i != alphas_to_clip.end(); ++i)
		{
			ProEXRchannel *chan = *i;
			
			chan->alphaClip();
		}
		
		// any layers to load all together?
		if( layers_to_load.size() )
		{
			for(vector<ProEXRlayer_read *>::iterator i = layers_to_load.begin(); i != layers_to_load.end(); ++i)
			{
				(*i)->loadFromFile();
			}
		}
	}
	catch(bad_alloc)
	{
		// we ran out of memory, unload everything
		freeBuffers();
	}
}

void
ProEXRdoc_read::setupDoc()
{
	initChannels<ProEXRchannel_read>();
	
	buildLayers<ProEXRlayer_read>();
}

ProEXRdoc_write_base::ProEXRdoc_write_base(OStream &os, Header &header) :
	_out_stream(os),
	_header(header)
{

}

ProEXRdoc_write_base::~ProEXRdoc_write_base()
{

}

int
ProEXRdoc_write_base::width() const
{
	const Box2i &dw = header().dataWindow();
	
	return ((dw.max.x - dw.min.x) + 1);
}

int
ProEXRdoc_write_base::height() const
{
	const Box2i &dw = header().dataWindow();
	
	return ((dw.max.y - dw.min.y) + 1);
}

void
ProEXRdoc_write_base::addChannel(ProEXRchannel *channel)
{
	// don't want a duplicate channel
	while( findChannel( channel->name() ) )
		channel->incrementName();
	
	if( channel->doc() == NULL )
		channel->assignDoc(this);
	else
		assert(channel->doc() == this);
	
	channels().push_back(channel);
}

void
ProEXRdoc_write_base::addLayer(ProEXRlayer *layer)
{
	assert(layer->doc() == NULL);
	
	// make sure we're not duplicating any channels
	bool found_no_duplicates = false;
	
	while(!found_no_duplicates)
	{
		bool must_increment = false;
		
		for(vector<ProEXRchannel *>::iterator i = layer->channels().begin(); i != layer->channels().end(); ++i)
		{
			assert( (*i)->doc() == NULL ); // channels should be unattached
			
			if( findChannel( (*i)->name() ) )
				must_increment = true;
		}
		
		if(must_increment)
			layer->incrementChannels();
		else
			found_no_duplicates = true;
	}
	
	// now that we know all channels are unique
	for(vector<ProEXRchannel *>::iterator i = layer->channels().begin(); i != layer->channels().end(); ++i)
		addChannel( *i );
	
	// add that layer
	layer->assignDoc(this);
	
	layers().push_back(layer);
}

ProEXRdoc_write::ProEXRdoc_write(OStream &os, Header &header) :
	ProEXRdoc_write_base(os, header)
{

}

ProEXRdoc_write::~ProEXRdoc_write()
{
	
}

void
ProEXRdoc_write::writeFile()
{
	Header &head = header();
	vector<ProEXRchannel *> &chans = channels();
	
	assert( head.channels().begin() == head.channels().end() ); // i.e., there are no channels in the header now
	
	Box2i dw = head.dataWindow();
	int dw_height = (dw.max.y - dw.min.y) + 1;
	
	FrameBuffer frameBuffer;
	
	for(int i=0; i < chans.size(); i++)
	{
		ProEXRchannel *chan = chans[i];
		
		if( chan->loaded() )
		{
			head.channels().insert(chan->name().c_str(), chan->pixelType() );
			
			ProEXRbuffer buffer = chan->getBufferDesc(chan->pixelType() == Imf::HALF);
			
			if(buffer.buf == NULL)
				throw BaseExc("buffer.buf is NULL.");
			
			char *exr_origin = (char *)buffer.buf - (dw.min.y * buffer.rowbytes) - (dw.min.x * buffer.colbytes);
			
			frameBuffer.insert(chan->name().c_str(),
						Slice(chan->pixelType(), exr_origin, buffer.colbytes, buffer.rowbytes) );
		}
	}
	
	OutputFile file(stream(), head);
	
	file.setFrameBuffer(frameBuffer);
	
	file.writePixels(dw_height);
}


ProEXRdoc_writeRGBA::ProEXRdoc_writeRGBA(OStream &os, Header &header, RgbaChannels mode) :
	ProEXRdoc_write_base(os, header),
	_mode(mode)
{

}

ProEXRdoc_writeRGBA::~ProEXRdoc_writeRGBA()
{
	
}

void
ProEXRdoc_writeRGBA::writeFile()
{
	ProEXRchannel *r_chan = findChannel("R");
	ProEXRchannel *g_chan = findChannel("G");
	ProEXRchannel *b_chan = findChannel("B");
	ProEXRchannel *a_chan = findChannel("A");
	
	writeRGBAfile(stream(), header(), _mode, r_chan, g_chan, b_chan, a_chan);
}

void writeRGBAfile(Imf::OStream &os, Imf::Header &header, Imf::RgbaChannels mode,
					ProEXRchannel *r_chan, ProEXRchannel *g_chan, ProEXRchannel *b_chan, ProEXRchannel *a_chan)
{
	if(r_chan == NULL || g_chan == NULL || b_chan == NULL) // just doing RGB folks, but write any mode you want
		throw BaseExc("channels not properly set.");
	
	assert(r_chan->loaded() && g_chan->loaded() && b_chan->loaded());
	
	if(mode & WRITE_A)
	{
		if(a_chan == NULL)
			throw BaseExc("a_chan not set.");
		
		assert( a_chan->loaded() );
	}
	
	
	// sorting out dimensions
	Box2i &dw = header.dataWindow();
	
	int width = (dw.max.x - dw.min.x) + 1;
	int height = (dw.max.y - dw.min.y) + 1;
	
	// if YCC, must have even dimensions
	if(mode & WRITE_YC)
	{
		int x_expand = width % 2;
		int y_expand = height % 2;
		
		if(x_expand)
		{
			width += x_expand;
			dw.max.x += x_expand;
		}
		
		if(y_expand)
		{
			height += y_expand;
			dw.max.y += y_expand;
		}
	}
	
	ProEXRbuffer r_desc = r_chan->getBufferDesc(true); // may as well get half
	ProEXRbuffer g_desc = g_chan->getBufferDesc(true);
	ProEXRbuffer b_desc = b_chan->getBufferDesc(true);
	
	if(r_desc.buf == NULL || g_desc.buf == NULL || b_desc.buf == NULL)
		throw BaseExc("missing buffers.");
	
	assert(r_desc.type == Imf::HALF && g_desc.type == Imf::HALF && b_desc.type == Imf::HALF);
	assert(r_desc.width == g_desc.width && g_desc.width == b_desc.width);
	assert(r_desc.height == g_desc.height && g_desc.height == b_desc.height);
	
	int buf_width = r_desc.width;
	int buf_height = r_desc.height;
	
	int fill_cols = width - buf_width;
	int fill_rows = height - buf_height;
	
	// must fill this Rgba buffer
	Array2D<Rgba> half_buffer(height, width);
	
	char *r_row = (char *)r_desc.buf;
	char *g_row = (char *)g_desc.buf;
	char *b_row = (char *)b_desc.buf;
	
	char *a_row = NULL;
	size_t a_rowbytes = 0;
	
	if(a_chan)
	{
		ProEXRbuffer a_desc = a_chan->getBufferDesc(true);
		
		if(a_desc.buf == NULL)
			throw BaseExc("missing buffer.");
		
		assert(a_desc.type == Imf::HALF);
		assert(a_desc.width == buf_width);
		assert(a_desc.height == buf_height);
		
		a_row = (char *)a_desc.buf;
		a_rowbytes = a_desc.rowbytes;
	}
	
	for(int y=0; y < buf_height; y++)
	{
		half *r = (half *)r_row;
		half *g = (half *)g_row;
		half *b = (half *)b_row;
		half *a = (half *)a_row;
	
		for(int x=0; x < buf_width; x++)
		{
			half_buffer[y][x].r = *r++;
			half_buffer[y][x].g = *g++;
			half_buffer[y][x].b = *b++;
			
			if(a)
				half_buffer[y][x].a = *a++;
			else
				half_buffer[y][x].a = half(1.0f);
		}
		
		for(int c=1; c <= fill_cols; c++)
			half_buffer[y][(buf_width-1)+c] = half_buffer[y][buf_width-1];
		
		r_row += r_desc.rowbytes;
		g_row += g_desc.rowbytes;
		b_row += b_desc.rowbytes;
		a_row += a_rowbytes;
	}
	
	for(int r=1; r <= fill_rows; r++)
		for(int x=0; x < width; x++)
			half_buffer[(buf_height-1)+r][x] = half_buffer[buf_height-1][x];
	
	
	// now write our file
	RgbaOutputFile file(os, header, mode);
	
	assert(dw.min.x == 0 && dw.min.y == 0);
	
	file.setFrameBuffer(&half_buffer[0][0], 1, width);
	
	file.writePixels(height);
}


static inline unsigned int GetBit(unsigned int src, int b)
{
	return ((src & (1L << b)) >> b);
}

static inline unsigned int BitPattern(unsigned int src, int b1, int b2, int b3, int b4, int b5)
{
	return	(GetBit(src, b1) << 4) |
			(GetBit(src, b2) << 3) |
			(GetBit(src, b3) << 2) |
			(GetBit(src, b4) << 1) |
			(GetBit(src, b5) << 0);
}

FloatPixel Uint2rgb(unsigned int input)
{
	static const unsigned int num_colors = 32; // 2 ^ color_bits (5 in this case)
	static const float max_val = num_colors - 1;
		
	static const unsigned int total_colors = num_colors * num_colors * num_colors;

	static vector<FloatPixel> colors;
	
	static bool initialized = false;
	
	if(!initialized)
	{
		colors.resize(total_colors);
		
		for(unsigned int i=0; i < total_colors; i++)
		{
			unsigned int r = BitPattern(i, 0, 3, 6,  9, 12);
			unsigned int g = BitPattern(i, 1, 4, 7, 10, 13);
			unsigned int b = BitPattern(i, 2, 5, 8, 11, 14);
			
			colors[i].r = (float)r / max_val;
			colors[i].g = (float)g / max_val;
			colors[i].b = (float)b / max_val;
		}
		
		initialized = true;
	}
	
	return colors[input % total_colors];
}

void KillNaN(float &in)
{
	const unsigned int l = *(unsigned int *)&in;
	
	if( (l & 0x7f800000) == 0x7f800000 && (l & 0x007fffff) != 0 )
	{
		// NaN
		in = 12.f;
	}
	else if( (l & 0xff800000) == 0x7f800000 && (l & 0x007fffff) == 0 )
	{
		// inf
		in = 123.f;
	}
	else if( (l & 0xff800000) == 0xff800000 && (l & 0x007fffff) != 0 )
	{
		// -inf
		in = 0.f;
	}
}
