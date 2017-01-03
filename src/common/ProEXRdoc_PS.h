
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

#ifndef __ProEXRdoc_PS_H__
#define __ProEXRdoc_PS_H__

#include "ProEXRdoc.h"

#include "PIGeneral.h"

// exception meaning Photoshop gave us an error (so leave gResult as is)
DEFINE_EXC(PhotoshopExc, Iex::BaseExc)

#define PS_LAYERS_KEY	"PSlayers"

// Our PS layer text format:
//
// bridge[visible:true, mode:Normal, opacity:125]{r:bridge.R, g:bridge.G, b:bridge.B, a:bridge.A},
// depthz[visible:false, mode:Add]{r:Z},
// "My Layer"[visible:true, mode:Normal]{r:"My Layer.R" g:anotherLayer.G b:"s p a c e"}
// "Sock \"it\" to me"[visible:true]{r:"Sock \"it\" to me.R" g:"Sock \"it\" to me.G" b:"Sock \"it\" to me.B"}


typedef enum {
	MODE_Normal = 1,	// PS transfer modes
	MODE_Dissolve,
	MODE_Darken,
	MODE_Multiply,
	MODE_ColorBurn,	
	MODE_LinearBurn,		
	MODE_DarkerColor,
	MODE_Lighten,
	MODE_Screen,
	MODE_ColorDodge,
	MODE_LinearDodge,	// (Add)
	MODE_LighterColor,
	MODE_Overlay,
	MODE_SoftLight,
	MODE_HardLight,
	MODE_VividLight,
	MODE_LinearLight,
	MODE_PinLight,
	MODE_HardMix,
	MODE_Difference,
	MODE_Exclusion,
	MODE_Hue,
	MODE_Saturation,
	MODE_Color,
	MODE_Luminosity,
	
	MODE_DancingDissolve,			// extra AE transfer modes
	MODE_ClassicColorBurn,
	MODE_Add,
	MODE_ClassicColorDodge,
	MODE_ClassicDifference,
	MODE_StencilAlpha,
	MODE_StencilLuma,
	MODE_SilhouetteAlpha,
	MODE_SilhouetteLuma,
	MODE_AlphaAdd,
	MODE_LuminescentPremul,
	MODE_Subtract,
	MODE_Divide

} TransferMode;


typedef unsigned int transMode;

void SetNonConstantTransferModes(transMode linearDodge, transMode lighterColor, transMode darkerColor, transMode subtract, transMode divide);

std::string EnumToStringBlendMode(TransferMode mode); // share this

typedef struct PS_callbacks {
	short				*result;
	ChannelPortProcs	*channelPortProcs;
	TestAbortProc		abortProc;
	ProgressProc		progressProc;
	HandleProcs *		handleProcs;
	AdvanceStateProc	advanceState;
	void *				*data;
	Rect				*theRect;
	VRect				*theRect32;
	int16				*loPlane;
	int16				*hiPlane;
	int16				*colBytes;
	int32				*rowBytes;
	int32				*planeBytes;
} PS_callbacks;

class ProEXRchannel_readPS : public ProEXRchannel_read
{
  public:
	ProEXRchannel_readPS(std::string name, Imf::PixelType pixelType=Imf::HALF);
	virtual ~ProEXRchannel_readPS();
	
	void copyToPhotoshop(int16 channel, ProEXRchannel *alpha, int num_channels=1);
  
  private:
};

class ProEXRchannel_writePS : public ProEXRchannel
{
  public:
	ProEXRchannel_writePS(std::string name, ReadChannelDesc *desc, Imf::PixelType pixelType=Imf::HALF);
	virtual ~ProEXRchannel_writePS();
	
	ReadChannelDesc *desc() { return _desc; }
	
	void loadFromPhotoshop(bool is_premultiplied);
	
	ProEXRbuffer getLoadedLineBufferDesc(int start_scanline, int end_scanline, bool use_half);
	void assignTransparency(ReadChannelDesc *transparency, ReadChannelDesc *layermask) { _transparency = transparency; _layermask = layermask; }
	
  private:
	ReadChannelDesc *_desc;
	
	// these for our line buffer
	int _width, _height;
	
	void *_data;
	void *_half_data;
	
	size_t _rowbytes, _half_rowbytes;
	
	ReadChannelDesc *_transparency;
	ReadChannelDesc *_layermask;
};


class ProEXRlayer_readPS : public ProEXRlayer_read
{
  public:
	ProEXRlayer_readPS(std::string name="");
	virtual ~ProEXRlayer_readPS();

	void setupWithLayerString(std::string &layerString);
	
	void copyToPhotoshop(int required_rgb_channels, int required_alpha_channels, ProEXRchannel_readPS *shared_alpha, bool unMult) const;

	bool visibility() const { return _visibility; }
	bool adjustment_layer() const { return _adjustment_layer; }
	unsigned8 opacity() const { return _opacity; }
	TransferMode transfer_mode() const { return _transfer_mode; }
	
  protected:

  private:
	bool _visibility;
	bool _adjustment_layer;
	unsigned8 _opacity;
	TransferMode _transfer_mode;
};

class ProEXRlayer_writePS : public ProEXRlayer
{
  public:
	ProEXRlayer_writePS(ReadLayerDesc *layerInfo, std::string name, Imf::PixelType pixelType=Imf::HALF);
	ProEXRlayer_writePS(ReadChannelDesc *channel_list, ReadChannelDesc *alpha_chan,
						ReadChannelDesc *transparency_chan, ReadChannelDesc *layermask_chan, std::string name, Imf::PixelType pixelType=Imf::HALF);
	virtual ~ProEXRlayer_writePS();
	
	std::string layerString() const;
	
	void loadFromPhotoshop() const;
	
	void writeLayerFile(Imf::OStream &os, const Imf::Header &header, Imf::PixelType pixelType) const;
	void writeLayerFileRGBA(Imf::OStream &os, const Imf::Header &header, Imf::RgbaChannels mode) const;
	void assignMyTransparency(ProEXRchannel_writePS *channel) const { channel->assignTransparency(_transparency_chan, _layermask_chan); }

  private:
	void setupLayer(ReadChannelDesc *channel_list, ReadChannelDesc *alpha_chan,
						ReadChannelDesc *transparency_chan, ReadChannelDesc *layermask_chan, Imf::PixelType pixelType);
  
	ReadLayerDesc *_layerInfo;
	ReadChannelDesc *_transparency_chan;
	ReadChannelDesc *_layermask_chan;

	bool _visibility;
	bool _adjustment_layer;
	unsigned8 _opacity;
	TransferMode _transfer_mode;
};


class ProEXRdoc_readPS : public ProEXRdoc_read
{
  public:
	ProEXRdoc_readPS(Imf::IStream &is, PS_callbacks *ps_calls, bool unMult, bool clip_alpha, bool do_layers, bool split_alpha=false, bool use_layers_string=false, bool renameFirstPart=false, bool set_up=true);
	virtual ~ProEXRdoc_readPS();

	PS_callbacks *ps_calls() const { return _ps_calls; }
	void set_ps_calls(PS_callbacks *ps_calls) { _ps_calls = ps_calls; }
	
	int16 ps_mode() const;
	int16 ps_planes() const;
	Imath::Vec2<Fixed> ps_res() const;
	
	void loadFromFile(bool force=false);
	
	void copyMainToPhotoshop() const;
	void copyLayerToPhotoshop(int index, bool use_shared_alpha);
	
	void copyConstantChannelToPhotoshop(int16 channel, float value) const;
	void copyWhiteChannelToPhotoshop(int16 channel) const { copyConstantChannelToPhotoshop(channel, 1.0f); }
	void copyBlackChannelToPhotoshop(int16 channel) const { copyConstantChannelToPhotoshop(channel, 0.0f); }
	
	size_t safeLines() const { return _safeLines; }
	
	virtual void queryAbort();
	
  protected:
  
  private:	
	void setupDoc(bool split_alpha, bool use_layers_string);
	
	void unMult();	// doing a weird thing here where this is declared public in the superclass but private here
					// want to encourage/force putting the unMult paramater in the constuctor
					
	void calculateSafeLines();

	PS_callbacks *_ps_calls;
	const bool _unMult;
	const bool _ps_layers;
	bool _used_layers_string;
	
	unsigned int _safeLines; // how many lines of this file we should be holding under cheap memory circumstances
};

class ProEXRdoc_writePS_base
{
  public:
	ProEXRdoc_writePS_base(PS_callbacks *ps_calls) : _ps_calls(ps_calls) {}
	virtual ~ProEXRdoc_writePS_base() {}
	
	PS_callbacks *ps_calls() const { return _ps_calls; }
	
  private:
	PS_callbacks *_ps_calls;
};

class ProEXRdoc_writePS : public ProEXRdoc_write, public ProEXRdoc_writePS_base
{
  public:
	ProEXRdoc_writePS(Imf::OStream &os, Imf::Header &header, Imf::PixelType pixelType, bool do_layers, bool hidden_layers,
							PS_callbacks *ps_calls, ReadImageDocumentDesc *documentInfo, ReadChannelDesc *alpha_chan);
	ProEXRdoc_writePS(Imf::OStream &os, Imf::Header &header, Imf::PixelType pixelType,
							PS_callbacks *ps_calls, ReadLayerDesc *layerInfo, ReadChannelDesc *alpha_chan);
	virtual ~ProEXRdoc_writePS();
	
	void addMainLayer(ReadChannelDesc *channel_list, ReadChannelDesc *alpha_chan,
					ReadChannelDesc *transparency_chan, ReadChannelDesc *layermask_chan, Imf::PixelType pixelType);
	
	bool hasRGBAlayer() const { return (findMainLayer(false, false) != NULL); }

	void loadFromPhotoshop(bool force=false) const;
	
	virtual void writeFile();
	
	std::string layersString() const;
	
	size_t safeLines() const { return _safeLines; }
	
	virtual void queryAbort();

  private:
	void setupDocLayers(ReadImageDocumentDesc *documentInfo, Imf::PixelType pixelType, bool hidden_layers);
	void setupDocSimple(ReadChannelDesc *channel_list, ReadChannelDesc *alpha_chan,
					ReadChannelDesc *transparency_chan, ReadChannelDesc *layermask_chan, Imf::PixelType pixelType, bool greyscale=false);
					
	void calculateSafeLines();
					
	unsigned int _safeLines; // how many lines of this file we should be holding under cheap memory circumstances
};

class ProEXRdoc_writePS_RGBA : public ProEXRdoc_writeRGBA, public ProEXRdoc_writePS_base
{
  public:
	ProEXRdoc_writePS_RGBA(Imf::OStream &os, Imf::Header &header, Imf::RgbaChannels mode,
							PS_callbacks *ps_calls, ReadImageDocumentDesc *documentInfo, ReadChannelDesc *alpha_chan);
	ProEXRdoc_writePS_RGBA(Imf::OStream &os, Imf::Header &header, Imf::RgbaChannels mode,
							PS_callbacks *ps_calls, ReadLayerDesc *layerInfo, ReadChannelDesc *alpha_chan);
	virtual ~ProEXRdoc_writePS_RGBA();
	
	void loadFromPhotoshop(bool force=false);
	
	virtual void writeFile();
	
	size_t safeLines() const { return _safeLines; }
	
	virtual void queryAbort();

  private:
	void setupDoc(ReadChannelDesc *channel_list, ReadChannelDesc *alpha_chan,
					ReadChannelDesc *transparency_chan, ReadChannelDesc *layermask_chan);
					
	void calculateSafeLines();
	
	size_t _safeLines;
};

class ProEXRdoc_writePS_Deep : public ProEXRdoc_writePS
{
  public:
	ProEXRdoc_writePS_Deep(Imf::OStream &os, Imf::Header &header, bool hidden_layers,
							PS_callbacks *ps_calls, ReadImageDocumentDesc *documentInfo);
	virtual ~ProEXRdoc_writePS_Deep();
	
	virtual void writeFile();
};


#endif
