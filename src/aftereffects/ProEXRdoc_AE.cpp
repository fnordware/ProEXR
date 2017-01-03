
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

#include "ProEXRdoc_AE.h"

#include <assert.h>

#include <Iex.h>

#include <ImfStandardAttributes.h>
#include <ImfArray.h>

#include "ProEXR_UTF.h"

#include "PITerminology.h"

using namespace Imf;
using namespace Imath;
using namespace Iex;
using namespace std;


extern AEGP_PluginID S_mem_id;


static string
searchReplace(const string &str, const string &search, const string &replace)
{
	string s = str;
	
	// locate the search strings
	vector<string::size_type> positions;

	string::size_type last_pos = 0;

	while(last_pos != string::npos && last_pos < s.size())
	{
		last_pos = s.find(search, last_pos);

		if(last_pos != string::npos)
		{
			positions.push_back(last_pos);
		
			last_pos += search.size();
		}
	}

	// replace with the replace string, starting from the end
	int i = positions.size();

	while(i--)
	{
		s.erase(positions[i], search.size());
		s.insert(positions[i], replace);
	}
	
	return s;
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


static string
enQuote(const string &s)
{
	return string("\"") + searchReplace(s, "\"", "\\\"") + string("\"");
}


static string
enQuoteIfNecessary(const string &s, const string &quoteChars = "\" ")
{
	bool isNecessary = (string::npos != s.find_first_of(quoteChars));
	
	if(isNecessary)
	{
		return enQuote(s);
	}
	else
		return s;
}


static void
bracketTextSplit(vector<string> &tokens, const string &str)
{
	// this takes the tail end of a Photoshop layer name and converts it to an array of channels
	// only works if channels are in bracket format
	// layer.RGBA -> {}
	// layer.[R][G][B][A] -> {R, G, B, A}
	// layer.[R]G[B] -> {R, B} (ignored non-bracket, but hopefully people won't do this
	// layer.[X][X][X] -> {X} (we are going to stop repeat channels right here)
	// layer.[red][green][blue] -> {red, green, blue}
	
	int i=0;
	
	bool in_brackets = false;
	
	string bracket_channel("");
	
	while(i < str.size())
	{
		if(in_brackets)
		{
			if(str[i] == ']')
			{
				in_brackets = false;
				
				if(bracket_channel.size())
				{
					bool duplicate = false;
					
					for(int n=0; n < tokens.size(); n++)
					{
						if(tokens[n] == bracket_channel)
							duplicate = true;
					}
					
					if(!duplicate)
						tokens.push_back(bracket_channel);
				}
				
				bracket_channel = "";
			}
			else
				bracket_channel += str[i];
		}
		else
		{
			if(str[i] == '[')
				in_brackets = true;
		}
		
		i++;
	}
}


static TransferMode AEtoEnumBlendMode(PF_TransferMode ae_mode)
{
	switch(ae_mode)
	{
		//case PF_Xfer_NONE:
		//case PF_Xfer_COPY:						
		//case PF_Xfer_BEHIND:
		case PF_Xfer_IN_FRONT:					return MODE_Normal; // this is what AE calls "Normal"
		case PF_Xfer_DISSOLVE:					return MODE_Dissolve;
		case PF_Xfer_ADD:						return MODE_Add;
		case PF_Xfer_MULTIPLY:					return MODE_Multiply;
		case PF_Xfer_SCREEN:					return MODE_Screen;
		case PF_Xfer_OVERLAY:					return MODE_Overlay;
		case PF_Xfer_SOFT_LIGHT:				return MODE_SoftLight;
		case PF_Xfer_HARD_LIGHT:				return MODE_HardLight;
		case PF_Xfer_DARKEN:					return MODE_Darken;
		case PF_Xfer_LIGHTEN:					return MODE_Lighten;
		case PF_Xfer_DIFFERENCE:				return MODE_Difference; // original < PS5.5 Difference
		case PF_Xfer_HUE:						return MODE_Hue;
		case PF_Xfer_SATURATION:				return MODE_Saturation;
		case PF_Xfer_COLOR:						return MODE_Color;
		case PF_Xfer_LUMINOSITY:				return MODE_Luminosity;
		case PF_Xfer_MULTIPLY_ALPHA:			return MODE_StencilAlpha;
		case PF_Xfer_MULTIPLY_ALPHA_LUMA:		return MODE_StencilLuma;
		case PF_Xfer_MULTIPLY_NOT_ALPHA:		return MODE_SilhouetteAlpha;
		case PF_Xfer_MULTIPLY_NOT_ALPHA_LUMA:	return MODE_SilhouetteLuma;
		case PF_Xfer_ADDITIVE_PREMUL:			return MODE_LuminescentPremul;
		case PF_Xfer_ALPHA_ADD:					return MODE_AlphaAdd;
		case PF_Xfer_COLOR_DODGE:				return MODE_ClassicColorDodge;
		case PF_Xfer_COLOR_BURN:				return MODE_ClassicColorBurn;
		//case PF_Xfer_EXCLUSION:

		case PF_Xfer_DIFFERENCE2:				return MODE_Difference;
		case PF_Xfer_COLOR_DODGE2:				return MODE_ColorDodge;
		case PF_Xfer_COLOR_BURN2:				return MODE_ColorBurn;
		
		case PF_Xfer_LINEAR_DODGE:				return MODE_LinearDodge;
		case PF_Xfer_LINEAR_BURN:				return MODE_LinearBurn;
		case PF_Xfer_LINEAR_LIGHT:				return MODE_LinearLight;
		case PF_Xfer_VIVID_LIGHT:				return MODE_VividLight;
		case PF_Xfer_PIN_LIGHT:					return MODE_PinLight;
		
		case PF_Xfer_HARD_MIX:					return MODE_HardMix;
		
		case PF_Xfer_LIGHTER_COLOR:				return MODE_LighterColor;
		case PF_Xfer_DARKER_COLOR:				return MODE_DarkerColor;
		
		#ifdef PF_ENABLE_PS12_MODES
		case PF_Xfer_SUBTRACT:					return MODE_Subtract;
		case PF_Xfer_DIVIDE:					return MODE_Divide;
		#endif
	
		default:								return MODE_Normal;
	}
}



#pragma mark-

AE_Layer::AE_Layer(const SPBasicSuite *pica_basicP, AEGP_LayerH layerH) :
	suites(pica_basicP),
	_layerH(layerH)
{
	AEGP_LayerFlags layer_flags;
	suites.LayerSuite()->AEGP_GetLayerFlags(_layerH, &layer_flags);
	
	_visibility = (layer_flags & AEGP_LayerFlag_VIDEO_ACTIVE);
	_solo = (layer_flags & AEGP_LayerFlag_SOLO);
	_adjustment_layer = (layer_flags & AEGP_LayerFlag_ADJUSTMENT_LAYER);
	
	
	suites.LayerSuite()->AEGP_GetLayerTransferMode(_layerH, &_layer_mode);
}


void
AE_Layer::restore() const
{
	suites.LayerSuite()->AEGP_SetLayerFlag(_layerH, AEGP_LayerFlag_VIDEO_ACTIVE, _visibility);
	suites.LayerSuite()->AEGP_SetLayerFlag(_layerH, AEGP_LayerFlag_SOLO, _solo);
	suites.LayerSuite()->AEGP_SetLayerFlag(_layerH, AEGP_LayerFlag_ADJUSTMENT_LAYER, _adjustment_layer);
	suites.LayerSuite()->AEGP_SetLayerTransferMode(_layerH, &_layer_mode);
}


AE_Layers_State::AE_Layers_State(AEIO_BasicData *basic_dataP, AEIO_OutSpecH outH) :
	suites(basic_dataP->pica_basicP)
{
	AEGP_RQItemRefH rq_itemH = NULL;
	AEGP_OutputModuleRefH outmodH = NULL;
	
	// apparently 0x00 is a valid value for rq_itemH and outmodH
	A_Err err = suites.IOOutSuite()->AEGP_GetOutSpecOutputModule(outH, &rq_itemH, &outmodH);
	
	if(!err)
	{
		AEGP_CompH compH = NULL;
		suites.RQItemSuite()->AEGP_GetCompFromRQItem(rq_itemH, &compH);
		
		if(compH)
		{
			setup(basic_dataP->pica_basicP, compH);
		}
		else
			throw AfterEffectsExc("Couldn't get compH");
	}
	else
		throw AfterEffectsExc("Couldn't get rq_itemH");
	
	
}

AE_Layers_State::AE_Layers_State(const SPBasicSuite *pica_basicP, AEGP_CompH compH) :
	suites(pica_basicP)
{
	setup(pica_basicP, compH);
}


void
AE_Layers_State::isolate(AEGP_LayerH layerH) const
{
	for(vector<AE_Layer>::const_iterator i = _layers.begin(); i != _layers.end(); i++)
	{
		AEGP_LayerH this_layerH = i->getLayer();
		
		
		if(this_layerH == layerH)
		{
			suites.LayerSuite()->AEGP_SetLayerFlag(this_layerH, AEGP_LayerFlag_VIDEO_ACTIVE, TRUE);
			
			AEGP_LayerTransferMode mode;
			
			mode.mode = PF_Xfer_IN_FRONT;
			mode.flags = 0;
			mode.track_matte = AEGP_TrackMatte_NO_TRACK_MATTE;
			
			suites.LayerSuite()->AEGP_SetLayerTransferMode(this_layerH, &mode);
			
			
			suites.LayerSuite()->AEGP_SetLayerFlag(this_layerH, AEGP_LayerFlag_ADJUSTMENT_LAYER, FALSE);
		}
		else
		{
			suites.LayerSuite()->AEGP_SetLayerFlag(this_layerH, AEGP_LayerFlag_VIDEO_ACTIVE, FALSE);
		}
	}
}


void
AE_Layers_State::restore() const
{
	for(vector<AE_Layer>::const_iterator i = _layers.begin(); i != _layers.end(); i++)
	{
		i->restore();
	}
}


void
AE_Layers_State::setup(const SPBasicSuite *pica_basicP, AEGP_CompH compH)
{
	A_long comp_layers;
	suites.LayerSuite()->AEGP_GetCompNumLayers(compH, &comp_layers);
	
	for(int i = (comp_layers - 1); i >= 0; i--)
	{
		AEGP_LayerH layerH = NULL;
		suites.LayerSuite()->AEGP_GetCompLayerByIndex(compH, i, &layerH);
		
		if(layerH)
		{
			AEGP_LayerFlags layer_flags;
			suites.LayerSuite()->AEGP_GetLayerFlags(layerH, &layer_flags);
			
			AEGP_ObjectType object_type;
			suites.LayerSuite()->AEGP_GetLayerObjectType(layerH, &object_type);
			
			// leave nulls, cameras, and lights (especially lights) alone
			if( !(layer_flags & AEGP_LayerFlag_NULL_LAYER) &&
				!(object_type == AEGP_ObjectType_CAMERA) &&
				!(object_type == AEGP_ObjectType_LIGHT) )
			{
				_layers.push_back( AE_Layer(pica_basicP, layerH) );
			}
		}
	}
}


#pragma mark-


ProEXRchannel_writeAE::ProEXRchannel_writeAE(string name, Imf::PixelType pixelType) :
	ProEXRchannel(name, pixelType)
{

}


ProEXRchannel_writeAE::~ProEXRchannel_writeAE()
{
	
}


void
ProEXRchannel_writeAE::loadFromAE(float *world, size_t rowbytes)
{
	if( !loaded() )
	{
		if(doc() == NULL)
			throw BaseExc("doc() is NULL.");
		
		ProEXRbuffer buffer = getBufferDesc(false);
		
		assert( buffer.width == doc()->width() );
		assert( buffer.height == doc()->height() );
		assert( buffer.type == Imf::FLOAT );
		
		if(buffer.buf == NULL)
			throw BaseExc("buffer.buf is NULL.");
		
		
		char *ae_row = (char *)world;
		char *buf_row = (char *)buffer.buf;
		
		for(int y=0; y < buffer.height; y++)
		{
			float *ae_pix = (float *)ae_row;
			float *buf_pix = (float *)buf_row;
			
			for(int x=0; x < buffer.width; x++)
			{
				*buf_pix = *ae_pix;
				
				ae_pix += 4;
				buf_pix++;
			}
			
			ae_row += rowbytes;
			buf_row += buffer.rowbytes;
		}
		
		setLoaded(true, true);
	}
}


ProEXRlayer_writeAE::ProEXRlayer_writeAE(AEGP_SuiteHandler &sh, AEGP_LayerH layerH, string name, Imf::PixelType pixelType) :
	ProEXRlayer(name),
	suites(sh),
	_layerH(layerH),
	_composite_buf(NULL),
	_composite_rowbytes(0)
{
	if(_layerH)
	{
		// get layer properties
		AEGP_LayerFlags layer_flags;
		suites.LayerSuite()->AEGP_GetLayerFlags(_layerH, &layer_flags);
		
		_visibility = (layer_flags & AEGP_LayerFlag_VIDEO_ACTIVE);
		_adjustment_layer = (layer_flags & AEGP_LayerFlag_ADJUSTMENT_LAYER);
		
		
		AEGP_LayerTransferMode transfer_mode;
		suites.LayerSuite()->AEGP_GetLayerTransferMode(_layerH, &transfer_mode);
		
		_transfer_mode = AEtoEnumBlendMode(transfer_mode.mode);
		
		_preserve_transparency = (transfer_mode.flags & AEGP_TransferFlag_PRESERVE_ALPHA);
		
		_track_matte = transfer_mode.track_matte;
	}
	else
	{
		_visibility = true;
		_adjustment_layer = false;
		_transfer_mode = MODE_Normal;
		_preserve_transparency = false;
		_track_matte = AEGP_TrackMatte_NO_TRACK_MATTE;
	}
	
	setupLayer(pixelType);
}


ProEXRlayer_writeAE::ProEXRlayer_writeAE(AEGP_SuiteHandler &sh, PF_PixelFloat *buf, size_t rowbytes, string name, Imf::PixelType pixelType) :
	ProEXRlayer(name),
	suites(sh),
	_layerH(0),
	_composite_buf(buf),
	_composite_rowbytes(rowbytes)
{
	_visibility = true;
	_adjustment_layer = false;
	_transfer_mode = MODE_Normal;
	
	setupLayer(pixelType);
}


ProEXRlayer_writeAE::~ProEXRlayer_writeAE()
{

}


string
ProEXRlayer_writeAE::layerString() const
{
	string layerString = enQuoteIfNecessary(name(), " \"[]{}");	
	
	// basic layer info
	layerString += "[";
	layerString += "visible:" + (_visibility ? string("true") : string("false"));
	layerString += ", mode:" + EnumToStringBlendMode(_transfer_mode);
	
	if(_adjustment_layer)
		layerString += ", adjustment_layer:true";

	if(_preserve_transparency)
		layerString += ", preserve_transparency:true";

	if(_track_matte != AEGP_TrackMatte_NO_TRACK_MATTE)
	{
		layerString += ", track_matte:";
		
		if(_track_matte == AEGP_TrackMatte_ALPHA)
			layerString += "Alpha";
		else if(_track_matte == AEGP_TrackMatte_NOT_ALPHA)
			layerString += "InverseAlpha";
		else if(_track_matte == AEGP_TrackMatte_LUMA)
			layerString += "Luma";
		else if(_track_matte == AEGP_TrackMatte_NOT_LUMA)
			layerString += "InverseLuma";
	}

	layerString += "]";
	
	// channels
	ProEXRchannel *red = NULL;
	ProEXRchannel *green = NULL;
	ProEXRchannel *blue = NULL;
	ProEXRchannel *alpha = NULL;
	
	for(vector<ProEXRchannel *>::const_iterator i = channels().begin(); i != channels().end(); ++i)
	{
		if( (*i)->channelTag() == CHAN_A && alpha == NULL )
			alpha = *i;
		else
		{
			if(red == NULL)
				red = *i;
			else if(green == NULL)
				green = *i;
			else if(blue == NULL)
				blue = *i;
			else if(alpha == NULL)
				alpha = *i;
		}
	}
	
	if(alpha && !red && !green && !blue && !_adjustment_layer)
	{
		red = alpha;
		alpha = NULL;
	}
	
	layerString += "{";
	
	if(red)
	{
		layerString += "r:" + enQuoteIfNecessary(red->name(), " \",:[]{}");
	}
	
	if(green)
	{
		layerString += ", g:" + enQuoteIfNecessary(green->name(), " \",:[]{}");
	}

	if(blue)
	{
		layerString += ", b:" + enQuoteIfNecessary(blue->name(), " \",:[]{}");
	}

	if(alpha)
	{
		layerString += ", a:" + enQuoteIfNecessary(alpha->name(), " \",:[]{}");
	}
	
	layerString += "}";
	
	
	return layerString;
}


void
ProEXRlayer_writeAE::loadFromAE(RenderParams *params)
{
	if( !loaded() )
	{
		if( isCompositeLayer() )
		{
			for(int i=0; i < channels().size(); i++)
			{
				ProEXRchannel_writeAE &writeAE_channel = dynamic_cast<ProEXRchannel_writeAE &>( *channels().at(i) );
				
				// the channel naming converntion is RGBA, but AE is ARGB
				int channel_index = (i == 3 ? 0 : i + 1);
				
				// we want to assure that an alpha channel is always coming from the alpha, such as layer.[U][V][A]
				if( writeAE_channel.channelTag() == CHAN_A)
					channel_index = 0;
				
				float *origin = (float *)_composite_buf + channel_index;
				
				writeAE_channel.loadFromAE(origin, _composite_rowbytes);
			}
		}
		else
		{
			if(doc() == NULL)
				throw BaseExc("doc() is NULL.");
				
			ProEXRdoc_writeAE &writeAE_doc = dynamic_cast<ProEXRdoc_writeAE &>( *doc() );
			
			// hide all other layers
			if(_layerH)
				writeAE_doc.isolateLayer(_layerH);
			else
				writeAE_doc.restoreLayers();
			
			
			// render
				
			AEGP_RenderOptionsH render_optionsH = NULL;
			suites.RenderOptionsSuite()->AEGP_NewFromItem(S_mem_id, writeAE_doc.getItem(), &render_optionsH);
			
			if(render_optionsH)
			{
				suites.RenderOptionsSuite()->AEGP_SetTime(render_optionsH, params->time);
				suites.RenderOptionsSuite()->AEGP_SetTimeStep(render_optionsH, params->time_step);
				suites.RenderOptionsSuite()->AEGP_SetFieldRender(render_optionsH, PF_Field_FRAME);
				suites.RenderOptionsSuite()->AEGP_SetWorldType(render_optionsH, AEGP_WorldType_32);
				suites.RenderOptionsSuite()->AEGP_SetDownsampleFactor(render_optionsH, params->xDownsample, params->yDownsample);
				suites.RenderOptionsSuite()->AEGP_SetRegionOfInterest(render_optionsH, &params->roi);
				suites.RenderOptionsSuite()->AEGP_SetMatteMode(render_optionsH, AEGP_MatteMode_PREMUL_BLACK);
				suites.RenderOptionsSuite()->AEGP_SetChannelOrder(render_optionsH, AEGP_ChannelOrder_ARGB);
				suites.RenderOptionsSuite()->AEGP_SetRenderGuideLayers(render_optionsH, FALSE);

				AEGP_FrameReceiptH render_receiptH = NULL;
				
				A_Err err = suites.RenderSuite()->AEGP_RenderAndCheckoutFrame(render_optionsH, NULL, NULL, &render_receiptH);
				
				if(!err && render_receiptH)
				{
					AEGP_WorldH worldH = NULL;
					suites.RenderSuite()->AEGP_GetReceiptWorld(render_receiptH, &worldH);
					
					if(worldH)
					{
						A_long width, height;
						A_u_long rowbytes;
						PF_PixelFloat *base_addr = NULL;
						
						suites.AEGPWorldSuite()->AEGP_GetSize(worldH, &width, &height);
						suites.AEGPWorldSuite()->AEGP_GetRowBytes(worldH, &rowbytes);
						suites.AEGPWorldSuite()->AEGP_GetBaseAddr32(worldH, &base_addr);
						
						assert(width == writeAE_doc.width() && height == writeAE_doc.height());
						
						for(int i=0; i < channels().size(); i++)
						{
							ProEXRchannel_writeAE &writeAE_channel = dynamic_cast<ProEXRchannel_writeAE &>( *channels().at(i) );
							
							// the channel naming converntion is RGBA, but AE is ARGB
							int channel_index = (i == 3 ? 0 : i + 1);
							
							// we want to assure that an alpha channel is always coming from the alpha, such as layer.[U][V][A]
							if( writeAE_channel.channelTag() == CHAN_A)
								channel_index = 0;

							float *origin = (float *)base_addr + channel_index;
							
							writeAE_channel.loadFromAE(origin, rowbytes);
						}
					}
					
					suites.RenderSuite()->AEGP_CheckinFrame(render_receiptH);
				}
				
				suites.RenderOptionsSuite()->AEGP_Dispose(render_optionsH);
				
				if(err)
					throw AfterEffectsExc("Error rendering.");
			}
		}
	}
}


void
ProEXRlayer_writeAE::setupLayer(Imf::PixelType pixelType)
{
	// now parse the layer name to set up channels, or maybe just RGBA
	// this code copied from ProEXRlayer_writePS
	// if I was a real programmer, I'd find a way to keep this in one place
	vector<string> channel_vec;
	
	string layer_name = name();
	
	// we allow layers to end with .RGBA and .RGB, which we replace with [R][G][B]([A])
	// if we don't have the bracket format after that, we assume they want RGBA added
	// this means that most random layer names will get RGBA appended
	searchReplaceEnd(layer_name, ".RGBA", ".[R][G][B][A]");
	searchReplaceEnd(layer_name, ".RGB", ".[R][G][B]");
	
	if(layer_name == "RGBA")
		layer_name = "[R][G][B][A]";
	else if(layer_name == "RGB")
		layer_name = "[R][G][B]";
	
	
	int dot_pos = layer_name.find_last_of('.');
	
	if(layer_name.size() > 1 && layer_name[layer_name.size() - 1] == ']' &&
	  (	(dot_pos == string::npos && layer_name[0] == '[') ||
		(layer_name[dot_pos + 1] == '[') ) )
	{
		// we've got the bracket format
		string layerName, channelsText;
		
		if(dot_pos == string::npos)
		{
			// so just straight up channels like [R][G][B][A]
			layerName = "";
			channelsText = layer_name;
		}
		else
		{
			// layer.[chan]
			layerName = layer_name.substr(0, dot_pos) + ".";
			channelsText = layer_name.substr(dot_pos + 1);
		}
	
		vector<string> channel_names;
		
		bracketTextSplit(channel_names, channelsText);
		
		for(int i=0; i < channel_names.size(); i++)
		{
			channel_vec.push_back(layerName + channel_names[i]);
		}
	}
	else
	{
		// unlabeled, add .RGBA
		channel_vec.push_back(layer_name + ".R");
		channel_vec.push_back(layer_name + ".G");
		channel_vec.push_back(layer_name + ".B");
		channel_vec.push_back(layer_name + ".A");
	}
	
	
	// we should have a vector of channels, now where to assign them?
	string red, green, blue, alpha;
	
	for(int i=0; i < channel_vec.size(); i++)
	{
		ProEXRchannel temp_channel(channel_vec[i]);
		
		if(temp_channel.channelTag() == CHAN_A)
			alpha = channel_vec[i];
		else
		{
			if( red.empty() )
				red = channel_vec[i];
			else if( green.empty() )
				green = channel_vec[i];
			else if( blue.empty() )
				blue = channel_vec[i];
			else if( alpha.empty() )
				alpha = channel_vec[i];
		}
	}
	
	// now set up channels with our names
	if( !red.empty() )
		channels().push_back(new ProEXRchannel_writeAE(red, pixelType) );
	
	if( !green.empty() )
		channels().push_back(new ProEXRchannel_writeAE(green, pixelType) );

	if( !blue.empty() )
		channels().push_back(new ProEXRchannel_writeAE(blue, pixelType) );
		
	if( !alpha.empty() )
		channels().push_back(new ProEXRchannel_writeAE(alpha, pixelType) );
}


ProEXRdoc_writeAE::ProEXRdoc_writeAE(OStream &os, Header &header, AEIO_BasicData *basic_dataP, AEIO_OutSpecH outH,
										Imf::PixelType pixelType, bool hidden_layers) :
	ProEXRdoc_write(os, header),
	suites(basic_dataP->pica_basicP),
	_itemH(NULL),
	_layers_state(basic_dataP, outH)
{
	// get AE handles for the doc
	
	AEGP_RQItemRefH rq_itemH = NULL;
	AEGP_OutputModuleRefH outmodH = NULL;
	
	AEGP_CompH compH = NULL;
	
	// apparently 0x00 is a valid value for rq_itemH and outmodH
	A_Err err = suites.IOOutSuite()->AEGP_GetOutSpecOutputModule(outH, &rq_itemH, &outmodH);
	
	if(!err)
	{
		suites.RQItemSuite()->AEGP_GetCompFromRQItem(rq_itemH, &compH);
		
		if(compH) // should never fail, but to be safe
		{
			suites.CompSuite()->AEGP_GetItemFromComp(compH, &_itemH);
			
			if(!_itemH)
				throw AfterEffectsExc("Couldn't get itemH");
		}
		else
			throw AfterEffectsExc("Couldn't get compH");
	}
	else
		throw AfterEffectsExc("Couldn't get rq_itemH");
	
	
	setupComp(compH, pixelType, hidden_layers);
	
	
	// store our layer information
	header.insert(PS_LAYERS_KEY, StringAttribute( layersString() ) );
}


ProEXRdoc_writeAE::ProEXRdoc_writeAE(Imf::OStream &os, Imf::Header &header, const SPBasicSuite *pica_basicP, AEGP_CompH compH,
						Imf::PixelType pixelType, bool hidden_layers) :
	ProEXRdoc_write(os, header),
	suites(pica_basicP),
	_itemH(NULL),
	_layers_state(pica_basicP, compH)
{
	if(compH)
	{
		suites.CompSuite()->AEGP_GetItemFromComp(compH, &_itemH);
		
		if(!_itemH)
			throw AfterEffectsExc("Couldn't get itemH");
	}
	else
		throw AfterEffectsExc("compH is NULL");
	
	
	setupComp(compH, pixelType, hidden_layers);
	
	
	// store our layer information
	header.insert(PS_LAYERS_KEY, StringAttribute( layersString() ) );
}


ProEXRdoc_writeAE::~ProEXRdoc_writeAE()
{

}


void
ProEXRdoc_writeAE::addMainLayer(Imf::PixelType pixelType)
{
	ProEXRlayer *old_main = findMainLayer(true, false);
	
	if(old_main)
		old_main->incrementChannels();
	
	addLayer(new ProEXRlayer_writeAE(suites, 0, "RGBA", pixelType));
}


void
ProEXRdoc_writeAE::addMainLayer(PF_PixelFloat *base_addr, size_t rowbytes, Imf::PixelType pixelType)
{
	ProEXRlayer *old_main = findMainLayer(true, false);
	
	if(old_main)
		old_main->incrementChannels();
		
	addLayer(new ProEXRlayer_writeAE(suites, base_addr, rowbytes, "RGBA", pixelType) );
}


void
ProEXRdoc_writeAE::loadFromAE(RenderParams *params) const
{
	for(vector<ProEXRlayer *>::const_iterator i = layers().begin(); i != layers().end(); ++i)
	{
		ProEXRlayer_writeAE &the_layer = dynamic_cast<ProEXRlayer_writeAE &>( **i );
		
		the_layer.loadFromAE(params);
	}
}


string
ProEXRdoc_writeAE::layersString() const
{
	string layers_string;
	
	bool first_one = true;
	
	for(vector<ProEXRlayer *>::const_iterator i = layers().begin(); i != layers().end(); ++i)
	{
		ProEXRlayer_writeAE &the_layer = dynamic_cast<ProEXRlayer_writeAE &>( **i );
		
		if(first_one)
			first_one = false;
		else
			layers_string += ", ";
		
		layers_string += the_layer.layerString();
	}
	
	return layers_string;
}


void
ProEXRdoc_writeAE::setupComp(AEGP_CompH compH, Imf::PixelType pixelType, bool hidden_layers)
{
	// setup the layers
	A_long comp_layers;
	suites.LayerSuite()->AEGP_GetCompNumLayers(compH, &comp_layers);
	
	for(int i = (comp_layers - 1); i >= 0; i--)
	{
		AEGP_LayerH layerH = NULL;
		suites.LayerSuite()->AEGP_GetCompLayerByIndex(compH, i, &layerH);
		
		if(layerH)
		{
			AEGP_LayerFlags layer_flags;
			suites.LayerSuite()->AEGP_GetLayerFlags(layerH, &layer_flags);
			
			AEGP_ObjectType object_type;
			suites.LayerSuite()->AEGP_GetLayerObjectType(layerH, &object_type);
			
			
			if(	((layer_flags & AEGP_LayerFlag_VIDEO_ACTIVE) || hidden_layers) &&
				!(layer_flags & AEGP_LayerFlag_GUIDE_LAYER) &&
				!(layer_flags & AEGP_LayerFlag_ADJUSTMENT_LAYER) &&
				!(layer_flags & AEGP_LayerFlag_NULL_LAYER) &&
				!(object_type == AEGP_ObjectType_CAMERA) &&
				!(object_type == AEGP_ObjectType_LIGHT) )
			{
			#if PF_AE_PLUG_IN_VERSION >= PF_AE100_PLUG_IN_VERSION
				AEGP_MemHandle layer_nameH = NULL;
				AEGP_MemHandle source_nameH = NULL;
				
				suites.LayerSuite()->AEGP_GetLayerName(S_mem_id, layerH, &layer_nameH, &source_nameH);
				
				
				string layer_name, source_name;
				
				if(layer_nameH)
				{
					A_UTF16Char *layer_nameP = NULL;
				
					suites.MemorySuite()->AEGP_LockMemHandle(layer_nameH, (void **)&layer_nameP);
					
					layer_name = UTF16toUTF8(layer_nameP);
					
					suites.MemorySuite()->AEGP_FreeMemHandle(layer_nameH);
				}
				
				if(source_nameH)
				{
					A_UTF16Char *source_nameP = NULL;
				
					suites.MemorySuite()->AEGP_LockMemHandle(source_nameH, (void **)&source_nameP);
					
					source_name = UTF16toUTF8(source_nameP);
					
					suites.MemorySuite()->AEGP_FreeMemHandle(source_nameH);
				}
				
				
				string &name = layer_name.empty() ? source_name : layer_name;
			#else
				A_char layer_name[AEGP_MAX_LAYER_NAME_SIZE + 1] = { '\0' };
				A_char source_name[AEGP_MAX_LAYER_NAME_SIZE + 1] = { '\0' };
				
				suites.LayerSuite()->AEGP_GetLayerName(layerH, layer_name, source_name);
				
				A_char *name = (layer_name[0] != '\0') ? layer_name : source_name;
			#endif
				
				addLayer( new ProEXRlayer_writeAE(suites, layerH, name, pixelType) );
			}
		}
	}
}