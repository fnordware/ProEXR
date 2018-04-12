
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

#ifndef __ProEXRdoc_H__
#define __ProEXRdoc_H__

#include <vector>
#include <map>

#include <ImfRgbaFile.h>
#include "ImfHybridInputFile.h"
#include <ImfOutputFile.h>
#include <ImfChannelList.h>
#include <ImfStandardAttributes.h>

#include <IexBaseExc.h>

#include <assert.h>

// exception to throw when queryAbort() returns yes
DEFINE_EXC(AbortExc, Iex::BaseExc)


enum ChannelType
{
	CHANNEL_RESERVED = 0,
	CHANNEL_SINGLE,
	CHANNEL_LAYER
};

enum ChanTag
{
	CHAN_GENERAL = 0,
	CHAN_R = 1,
	CHAN_G,
	CHAN_B,
	CHAN_Y,
	CHAN_RY,
	CHAN_BY,
	CHAN_A,
	CHAN_AR,
	CHAN_AG,
	CHAN_AB
};


struct ProEXRbuffer {
	Imf::PixelType type;
	void *buf;
	int width;
	int height;
	size_t colbytes;
	size_t rowbytes;
};


class ProEXRdoc; // forward declaration

class ProEXRchannel
{
  public:
	ProEXRchannel(std::string name, Imf::PixelType pixelType=Imf::HALF);
	virtual ~ProEXRchannel();
	
	std::string name() const { return _name; } // R, Transparency, layer.R, 
	std::string layerName() const; // (invalid-reserved), [Transparency], layer
	std::string channelName() const; // R, Transparency, R
	void incrementName();
	ChanTag channelTag() const;
	
	const Imf::PixelType pixelType() const { return _pixelType; }
	ChannelType channelType() const;
	
	void assignDoc(ProEXRdoc *doc);
	ProEXRdoc *doc() const { return _doc; }
	
	void allocateBuffers(bool allocate_half=false);
	void freeBuffers();
	
	template <class ChannelType>
	std::vector<ChannelType *> getUintRGBchannels(); // the recipient is responsible for deleting these channels
	
	ProEXRbuffer getBufferDesc(bool use_half=false);
	
	bool loaded() const { return _loaded; }
	void setLoaded(bool loaded, bool premultiplied=true) { _loaded = loaded; _premultiplied = premultiplied; }
	
	void fill(float val);
	void premultiply(ProEXRchannel *alpha, bool force=false);
	void unMult(ProEXRchannel *alpha);
	void alphaClip();
	void killNaN();

  protected:
	void queryAbort();

  private:
	void channelParts(std::string &layerName, std::string &channelName) const;
	void incrementText(std::string &name);
	
	void copyToHalf();
	
	std::string _name;
	Imf::PixelType _pixelType;
	
	ProEXRdoc *_doc;

	bool _loaded;
	bool _premultiplied;
	
	int _width, _height;
	
	void *_data;
	void *_half_data;
	
	size_t _rowbytes, _half_rowbytes;
};

class ProEXRchannel_read : public ProEXRchannel
{
  public:
	ProEXRchannel_read(std::string name, Imf::PixelType pixelType=Imf::HALF);
	virtual ~ProEXRchannel_read();
	
	void loadFromFile();
	
  private:
};

class ProEXRlayer
{
  public:
	ProEXRlayer(std::string name="");
	virtual ~ProEXRlayer();
	
	std::string name() const;		// RGBA, [Transparency], layer
	std::string ps_name() const;	// RGBA, [Transparency], layer.RGBA
	void incrementChannels();

	std::vector<ProEXRchannel *> & channels() { return _channels; }
	const std::vector<ProEXRchannel *> & channels() const { return _channels; }
	ProEXRchannel *findChannel(std::string channelName) const;
	void addChannel(ProEXRchannel *channel, bool sort=true);
	
	ProEXRchannel *alphaChannel() const;
	void assignAlpha(ProEXRchannel *externalAlpha, bool force=false); // looks for an alpha internally first (unless force)
	std::vector<ProEXRchannel *> getNonAlphaChannels() const;
	
	void assignDoc(ProEXRdoc *doc);
	ProEXRdoc *doc() const { return _doc; }
	
	const std::string & manifest() const { return _manifest; } // for Cryptomatte
	void setManifest(const std::string &s) { _manifest = s; }
	const std::string & manif_file() const { return _manif_file; }
	void setManif_File(const std::string &s) { _manif_file = s; }
	
	bool loaded() const;
	
	void premultiply(ProEXRchannel *shared_alpha);
	void unMult(ProEXRchannel *shared_alpha);
	
	void freeBuffers();
	
  protected:
	void sortChannels();
	void queryAbort();
	
	void setName(std::string name) { _name = name; }
	
  private:
	std::string _name;
	std::vector<ProEXRchannel *> _channels;
	
	std::string _manifest;
	std::string _manif_file;
	
	ProEXRchannel *_alpha;
	
	ProEXRdoc *_doc;
};

class ProEXRlayer_read : public ProEXRlayer
{
  public:
	ProEXRlayer_read(std::string name="");
	virtual ~ProEXRlayer_read();
	
	bool loadAsLayer() const { return _load_as_layer; }
	void setLoadAsLayer(bool val) { _load_as_layer = val; }
	
	void loadFromFile();
	
  private:
	void loadFromRGBA();
	
	bool _load_as_layer;
};

class ProEXRdoc 
{
  public:
	ProEXRdoc();
	virtual ~ProEXRdoc();
	
	virtual int width() const = 0;
	virtual int height() const = 0;
	
	ProEXRchannel *getAlphaChannel() const;
	
	template <class ChannelType>
	ChannelType *getBlackChannel();
	
	template <class ChannelType>
	ChannelType *getWhiteChannel();

	virtual void queryAbort() {}

	std::vector<ProEXRchannel *> & channels() { return _channels; }
	const std::vector<ProEXRchannel *> & channels() const { return _channels; }
	std::vector<ProEXRlayer *> & layers() { return _layers; }
	const std::vector<ProEXRlayer *> & layers() const { return _layers; }
	std::vector<ProEXRlayer *> & cryptoLayers() { return _cryptoLayers; }
	const std::vector<ProEXRlayer *> & cryptoLayers() const { return _cryptoLayers; }
	
	ProEXRchannel *findChannel(std::string channelName) const;
	ProEXRlayer *findLayer(std::string layerName) const;
	ProEXRlayer *findMainLayer(bool be_flexible=true, bool force=true) const;
	
	bool loaded() const;
	Imath::Int64 memorySize() const; // the amount this would take up if it were fully loaded
	void freeBuffers() const;
	
  protected:
	void premultiply();
	void unMult();
	
	template <class NewLayerType>
	void seperateAlphas();

	template <class NewLayerType>
	void overflowChannels();

	template <class ChannelType>
	ChannelType *newConstantChannel(float val);
	
  private:
	std::vector<ProEXRchannel *> _channels;
	std::vector<ProEXRlayer *> _layers;
	std::vector<ProEXRlayer *> _cryptoLayers;
  
	ProEXRchannel *_black_channel;
	ProEXRchannel *_white_channel;
};

class ProEXRdoc_read : public ProEXRdoc
{
  public:
	ProEXRdoc_read(Imf::IStream &is, bool clip_alpha=true, bool renameFirstPart=false, bool set_up=true);
	virtual ~ProEXRdoc_read();
	
	Imf::IStream & stream() const { return _in_stream; }
	
	const int parts() const { return _in_file.parts(); }
	const Imf::Header & header(int n) const { return _in_file.header(n); }
	
	Imf::HybridInputFile & file() { return _in_file; }
	const Imf::HybridInputFile & file() const { return _in_file; }
	const Imf::Header & header() const { return _in_file.header(0); }
	
	virtual int width() const;
	virtual int height() const;
	
	bool getClipAlpha() const { return _clipAlpha; }
	
	void loadFromFile();
	
	virtual void queryAbort() {}
	
  protected:
	template<class ChannelType>
 	void initChannels();
	
	typedef struct {
		std::string manifest;
		std::string manif_file;
	} CryptoManifest;
	
	template<class LayerType>
	void buildLayers(bool split_alpha=false);

  private:
	void setupDoc();
	
	const bool _clipAlpha;
  
	Imf::IStream &_in_stream;
	Imf::HybridInputFile _in_file;
};

class ProEXRdoc_write_base : public ProEXRdoc
{
  public:
	ProEXRdoc_write_base(Imf::OStream &os, Imf::Header &header);
	virtual ~ProEXRdoc_write_base();
	
	Imf::OStream & stream() const { return _out_stream; }
	Imf::Header & header() { return _header; }
	const Imf::Header & header() const { return _header; }
	
	virtual int width() const;
	virtual int height() const;

	void addChannel(ProEXRchannel *channel);
	void addLayer(ProEXRlayer *layer);
	
	virtual void writeFile() = 0;
	
	virtual void queryAbort() {}
	
  protected:
  
  private:
	Imf::OStream &_out_stream;
	Imf::Header &_header;
};

class ProEXRdoc_write : public ProEXRdoc_write_base
{
  public:
	ProEXRdoc_write(Imf::OStream &os, Imf::Header &header);
	virtual ~ProEXRdoc_write();
  
	virtual void writeFile();
	
	virtual void queryAbort() {}
	
  protected:
  
  private:
};

class ProEXRdoc_writeRGBA : public ProEXRdoc_write_base
{
  public:
	ProEXRdoc_writeRGBA(Imf::OStream &os, Imf::Header &header, Imf::RgbaChannels mode);
	virtual ~ProEXRdoc_writeRGBA();
  
	virtual void writeFile();
	
	virtual void queryAbort() {}
	
	Imf::RgbaChannels mode() const { return _mode; }
	
  protected:
  
  private:
	Imf::RgbaChannels _mode;
};

// shared function used when channels are loaded
void writeRGBAfile(Imf::OStream &os, Imf::Header &header, Imf::RgbaChannels mode,
					ProEXRchannel *r_chan, ProEXRchannel *g_chan, ProEXRchannel *b_chan, ProEXRchannel *a_chan);


#ifndef MAX
	#define MAX(A,B)	((A) > (B) ? (A) : (B))
#endif

#ifndef MIN
	#define MIN(A,B)	((A) < (B) ? (A) : (B))
#endif

typedef struct {
	float	r;
	float	g;
	float	b;
} FloatPixel;

FloatPixel Uint2rgb(unsigned int input);

void KillNaN(float &in);

template <class ChannelType>
std::vector<ChannelType *>
ProEXRchannel::getUintRGBchannels()
{
	assert(_pixelType == Imf::UINT);
	assert(_doc);
	assert(_loaded && _data && _width && _height && _rowbytes);
	
	ChannelType *red = new ChannelType(_name + ".R", Imf::FLOAT);
	ChannelType *green = new ChannelType(_name + ".G", Imf::FLOAT);
	ChannelType *blue = new ChannelType(_name + ".B", Imf::FLOAT);
	
	red->assignDoc(_doc);
	green->assignDoc(_doc);
	blue->assignDoc(_doc);
	
	red->allocateBuffers(false);
	green->allocateBuffers(false);
	blue->allocateBuffers(false);

	ProEXRbuffer red_buf = red->getBufferDesc(false);
	ProEXRbuffer green_buf = green->getBufferDesc(false);
	ProEXRbuffer blue_buf = blue->getBufferDesc(false);
	
	assert(red_buf.buf && green_buf.buf && blue_buf.buf);
	
	char *uint_row = (char *)_data;
	char *r_row = (char *)red_buf.buf;
	char *g_row = (char *)green_buf.buf;
	char *b_row = (char *)blue_buf.buf;
	
	for(int y=0; y < _height; y++)
	{
		unsigned int *uint_pix = (unsigned int *)uint_row;
		float *r_pix = (float *)r_row;
		float *g_pix = (float *)g_row;
		float *b_pix = (float *)b_row;
		
		for(int x=0; x < _width; x++)
		{
			FloatPixel p = Uint2rgb(*uint_pix++);
			
			*r_pix++ = p.r;
			*g_pix++ = p.g;
			*b_pix++ = p.b;
		}
		
		uint_row += _rowbytes;
		r_row += red_buf.rowbytes;
		g_row += green_buf.rowbytes;
		b_row += blue_buf.rowbytes;
	}
	
	// mark as loaded, but ID channels are basically not premultiplied
	red->setLoaded(true, false);
	green->setLoaded(true, false);
	blue->setLoaded(true, false);
	
	// send them out as a vector
	std::vector<ChannelType *> channel_vec;
	
	channel_vec.push_back(red);
	channel_vec.push_back(green);
	channel_vec.push_back(blue);

	return channel_vec;
}

template <class ChannelType>
ChannelType *
ProEXRdoc::getBlackChannel()
{
	if(_black_channel == NULL)
		_black_channel = newConstantChannel<ChannelType>(0.0f);
	
	assert(_black_channel);
	assert(dynamic_cast<ChannelType *>(_black_channel) != NULL);
	
	return (ChannelType *)_black_channel;
}

template <class ChannelType>
ChannelType *
ProEXRdoc::getWhiteChannel()
{
	if(_white_channel == NULL)
		_white_channel = newConstantChannel<ChannelType>(1.0f);
	
	assert(_white_channel);
	assert(dynamic_cast<ChannelType *>(_white_channel) != NULL);
	
	return (ChannelType *)_white_channel;
}

template <class NewLayerType>
void
ProEXRdoc::seperateAlphas()
{
	bool start_over = true;
	
	// keep running until we don't find any alphas to split
	while(start_over)
	{
		start_over = false;
		
		for(std::vector<ProEXRlayer *>::iterator i = layers().begin(); !start_over && i != layers().end(); ++i)
		{
			ProEXRlayer *layer = *i;
		
			if( layer->channels().size() > 1 )
			{
				for(std::vector<ProEXRchannel *>::iterator j = layer->channels().begin(); !start_over && j != layer->channels().end(); ++j)
				{
					ProEXRchannel *chan = *j;
					
					std::string ch_name = chan->name();
					
					if(chan->channelTag() == CHAN_A)
					{
						// remove the alpha from this layer
						layer->channels().erase(j);
						
						// but remember the alpha in case we have to UnMult
						layer->assignAlpha(chan);
						
						// make a new layer with the alpha, and insert it
						NewLayerType *new_layer = new NewLayerType;
						
						new_layer->assignDoc(this);
						
						new_layer->addChannel(chan);
						
						layers().insert(i, new_layer);
						
						start_over = true;
						break;
					}
				}
			}
			
			if(start_over)
				break;
		}
	}
}

template <class NewLayerType>
void
ProEXRdoc::overflowChannels()
{
	bool start_over = true;
	
	// keep running until we don't find any layers to overflow
	while(start_over)
	{
		start_over = false;
		
		for(std::vector<ProEXRlayer *>::iterator i = layers().begin(); !start_over && i != layers().end(); ++i)
		{
			ProEXRlayer *layer = *i;
		
			if( layer->getNonAlphaChannels().size() > 3 )
			{
				for(std::vector<ProEXRchannel *>::iterator j = layer->channels().begin(); !start_over && j != layer->channels().end(); ++j)
				{
					ProEXRchannel *chan = *j;
					
					if(chan->channelTag() == CHAN_GENERAL)
					{
						// remove the channel from this layer
						layer->channels().erase(j);
												
						// make a new layer with the channel, and insert it
						NewLayerType *new_layer = new NewLayerType;
						
						new_layer->assignDoc(this);
						
						new_layer->addChannel(chan);
						
						layers().insert(i, new_layer);
						
						start_over = true;
						break;
					}
				}
			}
			
			if(start_over)
				break;
		}
	}
}

template <class ChannelType>
ChannelType *
ProEXRdoc::newConstantChannel(float val)
{
	ChannelType *chan = new ChannelType("const_channel", Imf::FLOAT);
	
	chan->assignDoc(this);
	
	chan->fill(val);
	
	chan->setLoaded(true);
	
	return chan;
}

template<class ChannelType>
void
ProEXRdoc_read::initChannels()
{
	const Imf::ChannelList &exr_chans = file().channels();

	for (Imf::ChannelList::ConstIterator i = exr_chans.begin(); i != exr_chans.end(); ++i)
	{
		ChannelType *chan = new ChannelType(i.name(), i.channel().type);
		
		chan->assignDoc(this);
		
		channels().push_back( chan );
	}
}

template<class LayerType>
void
ProEXRdoc_read::buildLayers(bool split_alpha)
{
	assert( channels().size() );
	
	// reserved_channels
	bool have_R = false;
	bool have_G = false;
	bool have_B = false;
	bool have_A = false;
	bool have_Y = false;
	bool have_RY = false;
	bool have_BY = false;
	bool have_AR = false;
	bool have_AG = false;
	bool have_AB = false;
	
	
	std::map<std::string, CryptoManifest> cryptoNames;
	
	for(int i=0; i < parts(); i++)
	{
		const Imf::Header &head = header(i);
		
		for(Imf::Header::ConstIterator j = head.begin(); j != head.end(); ++j)
		{
			const Imf::Attribute &attrib = j.attribute();
			
			const std::string attribName = j.name();
			
			if(std::string(attrib.typeName()) == "string" &&
				attribName.substr(0, 12) == "cryptomatte/" &&
				attribName.substr(attribName.size() - 5) == "/name")
			{
				const Imf::StringAttribute &a = dynamic_cast<const Imf::StringAttribute &>(attrib);
				
				CryptoManifest manifest;
				
				const std::string manifestAttribName = attribName.substr(0, attribName.size() - 5) + "/manifest";
				const Imf::StringAttribute *manifestAttrib = head.findTypedAttribute<Imf::StringAttribute>(manifestAttribName.c_str());
				if(manifestAttrib)
					manifest.manifest = manifestAttrib->value();
					
				const std::string manif_fileAttribName = attribName.substr(0, attribName.size() - 11) + "/manif_file";
				const Imf::StringAttribute *manif_fileAttrib = head.findTypedAttribute<Imf::StringAttribute>(manif_fileAttribName.c_str());
				if(manif_fileAttrib)
					manifest.manif_file = manif_fileAttrib->value();
				
				cryptoNames[_in_file.partPrefix(i) + a.value()] = manifest;
			}
		}
	}
	
	// make the regular layers
	for(std::vector<ProEXRchannel *>::iterator i = channels().begin(); i != channels().end(); ++i)
	{
		ProEXRchannel *chan = *i;
		
		if(chan->channelType() == CHANNEL_RESERVED)
		{
			// keep track of which reserved channels we see
			if( chan->name() == "R" ) have_R = true;
			else if ( chan->name() == "G" ) have_G = true;
			else if ( chan->name() == "B" ) have_B = true;
			else if ( chan->name() == "A" ) have_A = true;
			else if ( chan->name() == "Y" ) have_Y = true;
			else if ( chan->name() == "RY" ) have_RY = true;
			else if ( chan->name() == "BY" ) have_BY = true;
			else if ( chan->name() == "AR" ) have_AR = true;
			else if ( chan->name() == "AG" ) have_AG = true;
			else if ( chan->name() == "AB" ) have_AB = true;
		}
		else
		{
			ProEXRlayer *layer = findLayer( chan->layerName() );
			
			if(layer && chan->pixelType() != Imf::UINT)
			{
				layer->addChannel( chan );
			}
			else
			{
				bool isCryptoChannel = false;
				
				for(std::map<std::string, CryptoManifest>::const_iterator j = cryptoNames.begin(); j != cryptoNames.end(); ++j)
				{
					const std::string &cryptoName = j->first;
					
					const std::string layerName = chan->layerName();
					
					if(layerName.size() == (cryptoName.size() + 2) &&
						layerName.substr(0, layerName.size() - 2) == cryptoName &&
						chan->channelType() == CHANNEL_LAYER)
					{
						LayerType *l = new LayerType(cryptoName);
						
						l->setManifest(j->second.manifest);
						l->setManif_File(j->second.manif_file);
						
						cryptoLayers().push_back(l);
						
						isCryptoChannel = true;
						
						break;
					}
				}
				
				if(!isCryptoChannel)
				{
					LayerType *l = NULL;
					
					if(chan->pixelType() == Imf::UINT)
						l = new LayerType(chan->name()); // because this layer won't really match the channel
					else
						l = new LayerType;
					
					l->addChannel( chan );
					
					l->assignDoc(this);
				
					layers().push_back(l);
				}
			}
		}
	}
	
	// make the reserved layers
	if(have_AR || have_AG || have_AB)
	{
		// really? a colored alpha?
		LayerType *l = new LayerType;
		
		l->addChannel( findChannel("AR") );
		l->addChannel( findChannel("AG") );
		l->addChannel( findChannel("AB") );
		
		l->assignDoc(this);
		
		layers().push_back(l);
	}
	
	if(have_Y || have_RY || have_BY)
	{
		// Y[RY][BY]
		LayerType *l = new LayerType;
		
		l->addChannel( findChannel("Y") );
		l->addChannel( findChannel("RY") );
		l->addChannel( findChannel("BY") );
		
		if(have_A && !have_R && !have_G && !have_B)
			l->addChannel( findChannel("A") );
		
		l->assignDoc(this);
		
		if(have_Y && have_RY && have_BY)
			l->setLoadAsLayer(true);
		
		layers().push_back(l);
	}
	
	if(have_R || have_G || have_B || (have_A && !have_Y && !have_RY && !have_BY) )
	{
		// RGBA
		LayerType *l = new LayerType;
		
		l->addChannel( findChannel("R") );
		l->addChannel( findChannel("G") );
		l->addChannel( findChannel("B") );
		l->addChannel( findChannel("A") );
		
		l->assignDoc(this);
		
		layers().push_back(l);
	}
	
	// assign alphas
	//for(std::vector<ProEXRlayer *>::iterator i = layers().begin(); i != layers().end(); ++i)
	//{
	//	(*i)->assignAlpha( findChannel("A") );
	//}
	
	
	// if a layer has been assigned more than 3 channels + alpha,
	// we'll "overflow" those extra channels into new layers
	overflowChannels<LayerType>();
	
	
	if(split_alpha)
	{
		seperateAlphas<LayerType>();
	}
}

#endif // __ProEXRdoc_H__
