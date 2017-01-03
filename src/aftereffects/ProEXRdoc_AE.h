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

#ifndef __ProEXRdoc_AE_H__
#define __ProEXRdoc_AE_H__

#include "ProEXRdoc.h"

#include "ProEXRdoc_PS.h"

#include "AEConfig.h"
#include "entry.h"
#include "AE_IO.h"
#include "AE_Macros.h"
#include "AE_EffectCBSuites.h"
#include "fnord_SuiteHandler.h"


// exception meaning AE gave us an error
DEFINE_EXC(AfterEffectsExc, Iex::BaseExc)

extern AEGP_PluginID S_mem_id;


class AE_Layer
{
  public:
	AE_Layer(const SPBasicSuite *pica_basicP, AEGP_LayerH layerH);
	~AE_Layer() {}
	
	void restore() const;
	
	AEGP_LayerH getLayer() const { return _layerH; }

  private:
	AEGP_SuiteHandler suites;
	AEGP_LayerH _layerH;
	
	bool _visibility;
	bool _solo;
	bool _adjustment_layer;
	
	AEGP_LayerTransferMode _layer_mode;
};

class AE_Layers_State
{
  public:
	AE_Layers_State(AEIO_BasicData *basic_dataP, AEIO_OutSpecH outH);
	AE_Layers_State(const SPBasicSuite *pica_basicP, AEGP_CompH compH);
	~AE_Layers_State() {}
	
	void isolate(AEGP_LayerH layerH) const;
	void restore() const;

  private:
	AEGP_SuiteHandler suites;
	
	void setup(const SPBasicSuite *pica_basicP, AEGP_CompH compH);
	
	std::vector<AE_Layer> _layers;
};


typedef struct {
	A_Time		time;
	A_Time		time_step;
	A_short		xDownsample;
	A_short		yDownsample;
	A_LRect		roi;
} RenderParams;


class ProEXRchannel_writeAE : public ProEXRchannel
{
  public:
	ProEXRchannel_writeAE(std::string name, Imf::PixelType pixelType=Imf::HALF);
	virtual ~ProEXRchannel_writeAE();
	
	void loadFromAE(float *world, size_t rowbytes);
};


class ProEXRlayer_writeAE : public ProEXRlayer
{
  public:
	ProEXRlayer_writeAE(AEGP_SuiteHandler &sh, AEGP_LayerH layerH, std::string name, Imf::PixelType pixelType=Imf::HALF);
	ProEXRlayer_writeAE(AEGP_SuiteHandler &sh, PF_PixelFloat *buf, size_t rowbytes, std::string name, Imf::PixelType pixelType=Imf::HALF);

	virtual ~ProEXRlayer_writeAE();
	
	std::string layerString() const;
	
	void loadFromAE(RenderParams *params);
	
	AEGP_LayerH getLayer() const { return _layerH; }

  private:
	AEGP_SuiteHandler &suites;
	
	AEGP_LayerH _layerH;
	
	// these will be non-null only if this is a composite layer
	PF_PixelFloat *_composite_buf;
	size_t _composite_rowbytes;
  
	void setupLayer(Imf::PixelType pixelType);
	bool isCompositeLayer() { return (_composite_buf != NULL && _composite_rowbytes > 0); }

	bool _visibility;
	bool _adjustment_layer;
	TransferMode _transfer_mode;
	bool _preserve_transparency;
	AEGP_TrackMatte _track_matte;
};


class ProEXRdoc_writeAE : public ProEXRdoc_write
{
  public:
	ProEXRdoc_writeAE(Imf::OStream &os, Imf::Header &header, AEIO_BasicData *basic_dataP, AEIO_OutSpecH outH,
						Imf::PixelType pixelType, bool hidden_layers);
						
	ProEXRdoc_writeAE(Imf::OStream &os, Imf::Header &header, const SPBasicSuite *pica_basicP, AEGP_CompH compH,
						Imf::PixelType pixelType, bool hidden_layers);
	
	virtual ~ProEXRdoc_writeAE();
	
	void addMainLayer(Imf::PixelType pixelType);
	void addMainLayer(PF_PixelFloat *base_addr, size_t rowbytes, Imf::PixelType pixelType);

	void loadFromAE(RenderParams *params) const;
	
	AEGP_ItemH getItem() const { return _itemH; }
	
	void isolateLayer(AEGP_LayerH layerH) const { _layers_state.isolate(layerH); };
	void restoreLayers() const { _layers_state.restore(); }
	
  private:
	AEGP_SuiteHandler suites;
	
	AEGP_ItemH _itemH;
	
	AE_Layers_State _layers_state;
	
	std::string layersString() const;
	void setupComp(AEGP_CompH compH, Imf::PixelType pixelType, bool hidden_layers);
};





#endif // __ProEXRdoc_AE_H__
