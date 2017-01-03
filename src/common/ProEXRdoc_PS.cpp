
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


#include "ProEXRdoc_PS.h"

#include <assert.h>

#include <Iex.h>

#include <ImfStandardAttributes.h>
#include <ImfArray.h>

#include <ImfDeepScanLineOutputFile.h>
#include <ImfDeepTiledOutputFile.h>
#include <ImfDeepFrameBuffer.h>

#include "ProEXR_UTF.h"

#include "PITerminology.h"


using namespace Imf;
using namespace Imath;
using namespace Iex;
using namespace std;


// moving target transfer modes
static transMode modeLinearDodge	= 0x66c;
static transMode modeLighterColor	= 0x672;
static transMode modeDarkerColor	= 0x673;
static transMode modeSubtract		= 0x6ec;
static transMode modeDivide			= 0x6ed;


void SetNonConstantTransferModes(transMode linearDodge, transMode lighterColor, transMode darkerColor, transMode subtract, transMode divide)
{
	modeLinearDodge = linearDodge;
	modeLighterColor = lighterColor;
	modeDarkerColor = darkerColor;
	modeSubtract = subtract;
	modeDivide = divide;
}


#ifdef __APPLE__
#include <mach/host_info.h>
#include <mach/mach_host.h>

typedef size_t size_m;

static size_m
SafeAvailableMemory(bool include_inactive)
{
	vm_statistics_data_t page_info;
	vm_size_t pagesize;
	mach_msg_type_number_t count;
	kern_return_t kret;

	pagesize = 0;
	kret = host_page_size(mach_host_self(), &pagesize);

	// vm stats
	count = HOST_VM_INFO_COUNT;
	kret = host_statistics (mach_host_self(), HOST_VM_INFO, (host_info_t)&page_info, &count);
	
	assert(kret == KERN_SUCCESS);
	
	size_m safe_mem = 0;
	
	if(kret == KERN_SUCCESS)
	{
		safe_mem = (page_info.free_count * pagesize) + (include_inactive ? (page_info.inactive_count * pagesize) : 0);
	}
	
#if !__x86_64__
	safe_mem = MIN(UINT_MAX, safe_mem);
#endif
	
	return MAX( safe_mem, (4 * 1024 * 1024) );
}

#else
typedef DWORDLONG size_m;

static size_m
SafeAvailableMemory(bool include_inactive)
{
	MEMORYSTATUSEX mem_status;

	mem_status.dwLength = sizeof(mem_status);

	GlobalMemoryStatusEx(&mem_status);

	size_m safe_mem = mem_status.ullAvailPhys + (include_inactive ? mem_status.ullAvailVirtual : 0);

#if !__x86_64__
	safe_mem = MIN(UINT_MAX, safe_mem);
#endif

	return MAX( safe_mem, (4 * 1024 * 1024) );
}

#endif

// this sort of does what auto_ptr does, except it calls delete [] for an array
// can't copy around like an auto_ptr, but it handles the scope stuff
template <typename T>
class AutoArray
{
  public:
	AutoArray() : _ptr(NULL) {}
	AutoArray(T *p) : _ptr(p) {}
	~AutoArray()
	{
		if(_ptr)
			delete [] _ptr;
	}
	
	AutoArray & operator = (T *p) { _ptr = p; return *this; }
	
	operator T * () const { return _ptr; }
	T * get() const { return _ptr; }

  private:
	T *_ptr;
};

#pragma mark-

static TransferMode StringToEnumBlendMode(string mode)
{
	map<string, TransferMode> mode_map;

#define STRING_2_ENUM(name)		mode_map[string( #name )] = MODE_##name;

	STRING_2_ENUM(Normal);				
	STRING_2_ENUM(Dissolve);				
	STRING_2_ENUM(Darken);				
	STRING_2_ENUM(Multiply);				
	STRING_2_ENUM(ColorBurn);			
	STRING_2_ENUM(LinearBurn);			
	STRING_2_ENUM(DarkerColor);			
	STRING_2_ENUM(Lighten);				
	STRING_2_ENUM(Screen);				
	STRING_2_ENUM(ColorDodge);			
	STRING_2_ENUM(LinearDodge);			
	STRING_2_ENUM(LighterColor);			
	STRING_2_ENUM(Overlay);				
	STRING_2_ENUM(SoftLight);			
	STRING_2_ENUM(HardLight);			
	STRING_2_ENUM(VividLight);			
	STRING_2_ENUM(LinearLight);			
	STRING_2_ENUM(PinLight);				
	STRING_2_ENUM(HardMix);				
	STRING_2_ENUM(Difference);			
	STRING_2_ENUM(Exclusion);			
	STRING_2_ENUM(Hue);					
	STRING_2_ENUM(Saturation);			
	STRING_2_ENUM(Color);				
	STRING_2_ENUM(Luminosity);			

	STRING_2_ENUM(DancingDissolve);		
	STRING_2_ENUM(ClassicColorBurn);	
	STRING_2_ENUM(Add);					
	STRING_2_ENUM(ClassicColorDodge);
	STRING_2_ENUM(ClassicDifference);
	STRING_2_ENUM(StencilAlpha);			
	STRING_2_ENUM(StencilLuma);			
	STRING_2_ENUM(SilhouetteAlpha);		
	STRING_2_ENUM(SilhouetteLuma);		
	STRING_2_ENUM(AlphaAdd);				
	STRING_2_ENUM(LuminescentPremul);
	STRING_2_ENUM(Subtract);
	STRING_2_ENUM(Divide);

	if( mode_map.find(mode) != mode_map.end() )
		return mode_map[mode];
	else
		return MODE_Normal;
}

string EnumToStringBlendMode(TransferMode mode)
{
	switch(mode)
	{
#define ENUM_2_STRING(name)		case MODE_##name :		return string( #name );

		ENUM_2_STRING(Normal);				
		ENUM_2_STRING(Dissolve);				
		ENUM_2_STRING(Darken);				
		ENUM_2_STRING(Multiply);				
		ENUM_2_STRING(ColorBurn);			
		ENUM_2_STRING(LinearBurn);			
		ENUM_2_STRING(DarkerColor);			
		ENUM_2_STRING(Lighten);				
		ENUM_2_STRING(Screen);				
		ENUM_2_STRING(ColorDodge);			
		ENUM_2_STRING(LinearDodge);			
		ENUM_2_STRING(LighterColor);			
		ENUM_2_STRING(Overlay);				
		ENUM_2_STRING(SoftLight);			
		ENUM_2_STRING(HardLight);			
		ENUM_2_STRING(VividLight);			
		ENUM_2_STRING(LinearLight);			
		ENUM_2_STRING(PinLight);				
		ENUM_2_STRING(HardMix);				
		ENUM_2_STRING(Difference);			
		ENUM_2_STRING(Exclusion);			
		ENUM_2_STRING(Hue);					
		ENUM_2_STRING(Saturation);			
		ENUM_2_STRING(Color);				
		ENUM_2_STRING(Luminosity);			

		ENUM_2_STRING(DancingDissolve);		
		ENUM_2_STRING(ClassicColorBurn);	
		ENUM_2_STRING(Add);					
		ENUM_2_STRING(ClassicColorDodge);
		ENUM_2_STRING(ClassicDifference);
		ENUM_2_STRING(StencilAlpha);			
		ENUM_2_STRING(StencilLuma);			
		ENUM_2_STRING(SilhouetteAlpha);		
		ENUM_2_STRING(SilhouetteLuma);		
		ENUM_2_STRING(AlphaAdd);				
		ENUM_2_STRING(LuminescentPremul);
		ENUM_2_STRING(Subtract);
		ENUM_2_STRING(Divide);
		
		default:	return string("Unknown");
	}
}

static TransferMode PStoEnumBlendMode(PIType ps_mode)
{
	// weird non-constant transer modes
	if(ps_mode == modeLinearDodge)
		return MODE_LinearDodge;
	
	if(ps_mode == modeLighterColor)
		return MODE_LighterColor;
		
	if(ps_mode == modeDarkerColor)
		return MODE_DarkerColor;

	if(ps_mode == modeSubtract)
		return MODE_Subtract;
	
	if(ps_mode == modeDivide)
		return MODE_Divide;

	switch(ps_mode)
	{
		// these are for PIBlendMode?
		case PIBlendNormal:			return MODE_Normal;
		case PIBlendDarken:			return MODE_Darken;
		case PIBlendLighten:		return MODE_Lighten;
		case PIBlendHue:			return MODE_Hue;
		case PIBlendSaturation:		return MODE_Saturation;
		case PIBlendColor:			return MODE_Color;
		case PIBlendLuminosity:		return MODE_Luminosity;
		case PIBlendMultiply:		return MODE_Multiply;
		case PIBlendScreen:			return MODE_Screen;
		case PIBlendDissolve:		return MODE_Dissolve;
		case PIBlendOverlay:		return MODE_Overlay;
		case PIBlendHardLight:		return MODE_HardLight;
		case PIBlendSoftLight:		return MODE_SoftLight;
		case PIBlendDifference:		return MODE_Difference;
		case PIBlendExclusion:		return MODE_Exclusion;
		case PIBlendColorDodge:		return MODE_ColorDodge;
		case PIBlendColorBurn:		return MODE_ColorBurn;
		case PIBlendLinearDodge:	return MODE_LinearDodge;
		case PIBlendLinearBurn:		return MODE_LinearBurn;
		case PIBlendLinearLight:	return MODE_LinearLight;
		case PIBlendVividLight:		return MODE_VividLight;
		case PIBlendPinLight:		return MODE_PinLight;
		case PIBlendHardMix:		return MODE_HardMix;
		
		// this is what I'm really getting in the LayerDescriptor
		case enumNormal:			return MODE_Normal;
		case enumDissolve:			return MODE_Dissolve;
		case enumDarken:			return MODE_Darken;
		case enumMultiply:			return MODE_Multiply;
		case enumColorBurn:			return MODE_ColorBurn;		// not in PS float
		//case						return MODE_LinearBurn;		// not in PS float
		case 0x44e:
		case 0x598:
		case 0x607:
		case 0x673:					return MODE_DarkerColor;	// \000\000\004N
		case enumLighten:			return MODE_Lighten;
		case enumScreen:			return MODE_Screen;			// not in PS float
		case enumColorDodge:		return MODE_ColorDodge;		// not in PS float
		case 0x447:
		case 0x591:
		case 0x600:
		case 0x66c:					return MODE_LinearDodge;	// \000\000\004G, \000\000\006l
		case 0x44d:
		case 0x597:
		case 0x606:
		case 0x672:					return MODE_LighterColor;	// \000\000\004M
		case enumOverlay:			return MODE_Overlay;		// not in PS float
		case enumSoftLight:			return MODE_SoftLight;		// not in PS float
		case enumHardLight:			return MODE_HardLight;		// not in PS float
		//case						return MODE_VividLight;		// not in PS float
		//case						return MODE_LinearLight;	// not in PS float
		//case						return MODE_PinLight;		// not in PS float
		//case						return MODE_HardMix;		// not in PS float
		case enumDifference:		return MODE_Difference;
		case enumExclusion:			return MODE_Exclusion;		// not in PS float
		case enumHue:				return MODE_Hue;
		case enumSaturation:		return MODE_Saturation;
		case enumColor:				return MODE_Color;
		case enumLuminosity:		return MODE_Luminosity;
		
		default:					return MODE_Normal;
	}
}

#pragma mark-

static void
quotedTokenize(const string& str,
				  vector<string>& tokens,
				  const string& delimiters = " ")
{
	// this function will respect quoted strings when tokenizing
	// the quotes will be included in the returned strings
	
	int i = 0;
	bool in_quotes = false;
	
	// if there are un-quoted delimiters in the beginning, skip them
	while(i < str.size() && str[i] != '\"' && string::npos != delimiters.find(str[i]) )
		i++;
	
	string::size_type lastPos = i;
	
	while(i < str.size())
	{
		if(str[i] == '\"' && (i == 0 || str[i-1] != '\\'))
			in_quotes = !in_quotes;
		else if(!in_quotes)
		{
			if( string::npos != delimiters.find(str[i]) )
			{
				tokens.push_back(str.substr(lastPos, i - lastPos));
				
				lastPos = i + 1;
				
				// if there are more delimiters ahead, push forward
				while(lastPos < str.size() && (str[lastPos] != '\"' || str[lastPos-1] != '\\') && string::npos != delimiters.find(str[lastPos]) )
					lastPos++;
					
				i = lastPos;
				continue;
			}
		}
		
		i++;
	}
	
	if(in_quotes)
		throw BaseExc("Quoted tokenize error.");
	
	// we're at the end, was there anything left?
	if(str.size() - lastPos > 0)
		tokens.push_back( str.substr(lastPos) );
}

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

static string
deQuote(const string &s)
{
	string::size_type start_pos = (s[0] == '\"' ? 1 : 0);
	string::size_type end_pos = ( (s.size() >= 2 && s[s.size()-1] == '\"' && s[s.size()-2] != '\\') ? s.size()-2 : s.size()-1);

	return searchReplace(s.substr(start_pos, end_pos + 1 - start_pos), "\\\"", "\"");
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

static vector<string>
splitLayersString(string &s)
{
	vector<string> layerStrings;
	
	bool in_quotes = false;
	bool in_brackets = false;
	bool in_braces = false;
	
	int layer_begin = 0;
	
	int i = 0;
	while(i < s.size())
	{
		if(s[i] == '\"' && (i == 0 || s[i-1] != '\\'))
			in_quotes = !in_quotes;
		else if(!in_quotes)
		{
			if(s[i] == '[')
				in_brackets = true;
			else if(s[i] == ']')
				in_brackets = false;
			else if(s[i] == '{')
				in_braces = true;
			else if(s[i] == '}')
				in_braces = false;
				
			if(s[i] == ',' && !in_brackets && !in_braces)
			{
				string layerString = s.substr(layer_begin, i - layer_begin);
				
				// we'll just double-check that it splits into three parts like it should
				vector<string> layer_parts;
				quotedTokenize(layerString, layer_parts, "[]{}");

				if(layer_parts.size() == 3)
					layerStrings.push_back(layerString);
				
				string::size_type next_pos = s.find_first_not_of(", ", i);
				
				if(next_pos != string::npos)
				{
					i = layer_begin = s.find_first_not_of(", ", i);
					continue;
				}
			}
		}
		
		i++;
	}
	
	
	// push back the last one, which will not end with a comma
	// would be at least 10 characters long
	if(s.size() - layer_begin >= 10)
	{
		string layerString = s.substr(layer_begin);
		
		vector<string> layer_parts;
		quotedTokenize(layerString, layer_parts, "[]{}");
		
		if(layer_parts.size() == 3)
			layerStrings.push_back(layerString);
	}
	
	return layerStrings;
}

#pragma mark-

ProEXRchannel_readPS::ProEXRchannel_readPS(string name, Imf::PixelType pixelType) :
	ProEXRchannel_read(name, pixelType)
{

}

ProEXRchannel_readPS::~ProEXRchannel_readPS()
{

}

void
ProEXRchannel_readPS::copyToPhotoshop(int16 channel, ProEXRchannel *alpha, int num_channels)
{
	if(doc() == NULL)
		throw BaseExc("doc() is NULL");
	
	ProEXRdoc_readPS &readPS_doc = dynamic_cast<ProEXRdoc_readPS &>( *doc() );
	
	size_t cheapNumRows = MIN(doc()->height(), readPS_doc.safeLines());
	
	PS_callbacks *ps_calls = readPS_doc.ps_calls();
	
	if(ps_calls == NULL || ps_calls->advanceState == NULL)
		throw BaseExc("Bad ps_calls.");
	
	if( loaded() )
	{
		// quickly copy pre-loaded channel
		ProEXRbuffer buf = getBufferDesc(false);
		
		assert(pixelType() != Imf::UINT);
		assert(buf.type == Imf::FLOAT);
		
		*ps_calls->planeBytes = sizeof(float);
		*ps_calls->colBytes = *ps_calls->planeBytes;
		*ps_calls->rowBytes = buf.rowbytes;

		ps_calls->theRect->left = ps_calls->theRect32->left = 0;
		ps_calls->theRect->right = ps_calls->theRect32->right = doc()->width();
		ps_calls->theRect->top = ps_calls->theRect32->top = 0;
		ps_calls->theRect->bottom = ps_calls->theRect32->bottom = doc()->height();
		
		*ps_calls->data = buf.buf;
		
		for(int c = channel; c < channel + num_channels; c++)
		{
			*ps_calls->loPlane = *ps_calls->hiPlane = c;
			
			*ps_calls->result = ps_calls->advanceState();
			
			if(*ps_calls->result != noErr)
				throw PhotoshopExc("Photoshop error.");
				
			queryAbort();
		}
	}
	else
	{
		assert(pixelType() != Imf::UINT);
		
		const bool this_is_alpha = (channelTag() == CHAN_A);
		
		// load float pixels, being cheap with memory
		const Box2i &dw = readPS_doc.file().dataWindow();
		
		int width = (dw.max.x - dw.min.x) + 1;
		
		size_t float_rowbytes = sizeof(float) * width;
		AutoArray<float> float_buf = new float[width * cheapNumRows];
		
		size_t alpha_rowbytes = sizeof(float) * width;
		AutoArray<float> alpha_buf = NULL;
		
		if(alpha && !alpha->loaded() )
		{
			alpha_buf = new float[width * cheapNumRows];
		}
		

		try{
			for(int y = dw.min.y; y < dw.min.y + readPS_doc.height() && *ps_calls->result == noErr; y += cheapNumRows)
			{
				if(readPS_doc.parts() > 1)
				{
					memset(float_buf.get(), 0, sizeof(float) * width * cheapNumRows);
					
					if(alpha_buf)
						memset(alpha_buf.get(), 0, sizeof(float) * width * cheapNumRows);
				}
				
				// read from the EXR
				char *float_row = (char *)float_buf.get();
				
				char *float_origin = float_row - (y * float_rowbytes) - (dw.min.x * sizeof(float));
				
				FrameBuffer frameBuffer;
				frameBuffer.insert(name(), Slice(Imf::FLOAT, float_origin, sizeof(float), float_rowbytes, 1, 1, 0.0f) );
				
				// set up our alpha
				char *alpha_row = NULL;
				
				if(alpha)
				{
					if( alpha->loaded() )
					{
						// we might already have a loaded alpha channel if nothing else
						ProEXRbuffer alpha_desc = alpha->getBufferDesc(false);
						
						alpha_row = (char *)alpha_desc.buf; // this will be the first row
						
						alpha_row += alpha_desc.rowbytes * (y - dw.min.y); // so we move down to the right row
						
						alpha_rowbytes = alpha_desc.rowbytes;
					}
					else
					{
						assert(alpha_buf);
						alpha_row = (char *)alpha_buf.get();
						
						char *alpha_origin = alpha_row - (y * alpha_rowbytes) - (dw.min.x * sizeof(float));
						
						frameBuffer.insert(alpha->name(), Slice(Imf::FLOAT, alpha_origin, sizeof(float), alpha_rowbytes, 1, 1, 1.0f) );
					}
				}
				
				int end_scanline = MIN(y + cheapNumRows - 1, dw.min.y + readPS_doc.height() - 1);
				
				readPS_doc.file().setFrameBuffer(frameBuffer);
				readPS_doc.file().readPixels(y, end_scanline);
				
				// kill NaN
				if(float_row)
				{
					float *float_pix = (float *)float_row;
					
					for(int x=0; x < width * (1 + end_scanline - y); x++)
					{
						KillNaN(*float_pix);
						
						float_pix++;
					}
				}
				
				// UnMult
				if(alpha)
				{
					float *float_pix = (float *)float_row;
					float *alpha_pix = (float *)alpha_row;
					
					for(int x=0; x < width * (1 + end_scanline - y); x++)
					{
						if(*alpha_pix > 0.0f && *alpha_pix < 1.0f && *float_pix != 0.0f)
							*float_pix /= *alpha_pix;
						
						float_pix++;
						alpha_pix++;
					}
				}
				
				// clip alpha
				if(this_is_alpha && readPS_doc.getClipAlpha())
				{
					assert(alpha == NULL);
					
					float *float_pix = (float *)float_row;
					
					for(int x=0; x < width * (1 + end_scanline - y); x++)
					{
						if(*float_pix < 0.f)
							*float_pix = 0.f;
						else if(*float_pix > 1.f)
							*float_pix = 1.f;
						
						float_pix++;
					}
				}
				
				// hand off to Photoshop
				*ps_calls->planeBytes = sizeof(float);
				*ps_calls->colBytes = *ps_calls->planeBytes;
				*ps_calls->rowBytes = float_rowbytes;

				ps_calls->theRect->left = ps_calls->theRect32->left = 0;
				ps_calls->theRect->right = ps_calls->theRect32->right = doc()->width();
				ps_calls->theRect->top = ps_calls->theRect32->top = y - dw.min.y;
				ps_calls->theRect->bottom = ps_calls->theRect32->bottom = 1 + end_scanline - dw.min.y;
				
				for(int c = channel; c < channel + num_channels && *ps_calls->result == noErr; c++)
				{
					*ps_calls->loPlane = *ps_calls->hiPlane = c;
					
					*ps_calls->data = float_buf;
					
					*ps_calls->result = ps_calls->advanceState();
					
					queryAbort();
				}
			}
		}
		catch(Iex::InputExc) {}
		catch(Iex::IoExc) {}
				
		
		if(*ps_calls->result != noErr)
			throw PhotoshopExc("Photoshop error.");
	}
}

ProEXRchannel_writePS::ProEXRchannel_writePS(string name, ReadChannelDesc *desc, Imf::PixelType pixelType) :
	ProEXRchannel(name, pixelType)
{
	_desc = desc;

	_width = _height = 0;
	_data = _half_data = NULL;
	_rowbytes = _half_rowbytes = 0;	
	_transparency = _layermask = NULL;
}

ProEXRchannel_writePS::~ProEXRchannel_writePS()
{
	if(_data)
		free(_data);
		
	if(_half_data)
		free(_half_data);
}

void
ProEXRchannel_writePS::loadFromPhotoshop(bool is_premultiplied)
{
	if( !loaded() )
	{
		if(doc() == NULL)
			throw BaseExc("doc() is NULL.");
		
		// woo hoo, multiple inheritance!
		ProEXRdoc_writePS_base &writePS_base = dynamic_cast<ProEXRdoc_writePS_base &>( *doc() );
		
		if(writePS_base.ps_calls() == NULL || writePS_base.ps_calls()->channelPortProcs == NULL)
			throw BaseExc("bad ps_calls.");
		
		ReadPixelsProc ReadProc = writePS_base.ps_calls()->channelPortProcs->readPixelsProc;
		
		// now let's get our buffer and load the channel
		ProEXRbuffer buffer = getBufferDesc(false);
		
		assert( buffer.width == doc()->width() );
		assert( buffer.height == doc()->height() );
		assert( buffer.type == Imf::FLOAT );
		
		if(buffer.buf == NULL)
			throw BaseExc("buffer.buf is NULL.");

		if(_desc == NULL)
			throw BaseExc("_desc is NULL.");
		
		VRect wroteRect;
		VRect writeRect = { 0, 0, buffer.height, buffer.width };
		PSScaling scaling = { writeRect, writeRect };
		PixelMemoryDesc memDesc = { buffer.buf, buffer.rowbytes * 8, buffer.colbytes * 8, 0, 32 };

		OSErr err = ReadProc(_desc->port, &scaling, &writeRect, &memDesc, &wroteRect);
		
		if(err != noErr)
		{
			PS_callbacks *ps_calls = writePS_base.ps_calls();
			*ps_calls->result = err;
			throw PhotoshopExc("Photoshop error.");
		}
			
		queryAbort();
			
		assert(wroteRect.top == writeRect.top);
		assert(wroteRect.left == writeRect.left);
		assert(wroteRect.bottom == writeRect.bottom);
		assert(wroteRect.right == writeRect.right);
		
		if(err == noErr)
			setLoaded(true, is_premultiplied);
	}
}

ProEXRbuffer
ProEXRchannel_writePS::getLoadedLineBufferDesc(int start_scanline, int end_scanline, bool use_half)
{
	if(doc() == NULL)
		throw BaseExc("doc() is NULL.");
	
	ProEXRdoc_writePS_base &writePS_base = dynamic_cast<ProEXRdoc_writePS_base &>( *doc() );
	
	if(writePS_base.ps_calls() == NULL || writePS_base.ps_calls()->channelPortProcs == NULL)
		throw BaseExc("bad ps_calls.");
	
	ReadPixelsProc ReadProc = writePS_base.ps_calls()->channelPortProcs->readPixelsProc;
	
	int lines = (end_scanline - start_scanline) + 1;
	
	// set up buffers
	if(_data == NULL || _height != lines)
	{
		_rowbytes = sizeof(float) * doc()->width();
	
		if(_data)
			free(_data);
		
		_data = malloc(_rowbytes * lines);
		
		if(_data == NULL)
			throw bad_alloc();
	}
	
	if(use_half && (_half_data == NULL || _height != lines))
	{
		_half_rowbytes = sizeof(half) * doc()->width();
	
		if(_half_data)
			free(_half_data);
		
		_half_data = malloc(_half_rowbytes * lines);
		
		if(_half_data == NULL)
			throw bad_alloc();
	}
	
	_height = lines;
	_width = doc()->width();
	
	size_t colbytes = sizeof(float);
	
	// shared ReadProc stuff
	VRect wroteRect;
	VRect writeRect = { start_scanline, 0, end_scanline + 1, _width };
	PSScaling scaling = { writeRect, writeRect };
	
	// if this is an alpha, don't use  the layer's transparency on itself
	if(_desc == _transparency)
	{
		_transparency = _layermask;
		_layermask = NULL;
	}

	// load transparency and layermask first
	AutoArray<float> transparency_buf = NULL;
	
	if(_transparency)
	{
		transparency_buf = new float[_width * _height];
		
		PixelMemoryDesc memDesc = { transparency_buf, _rowbytes * 8, colbytes * 8, 0, 32 };
	
		OSErr err = ReadProc(_transparency->port, &scaling, &writeRect, &memDesc, &wroteRect);
		
		if(err != noErr)
		{
			PS_callbacks *ps_calls = writePS_base.ps_calls();
			*ps_calls->result = err;
			throw PhotoshopExc("Photoshop error.");
		}
			
		queryAbort();
	}

	if(_layermask)
	{
		AutoArray<float> layermask_buf = new float[_width * _height];
		
		PixelMemoryDesc memDesc = { layermask_buf, _rowbytes * 8, colbytes * 8, 0, 32 };
	
		OSErr err = ReadProc(_layermask->port, &scaling, &writeRect, &memDesc, &wroteRect);
		
		if(err != noErr)
		{
			PS_callbacks *ps_calls = writePS_base.ps_calls();
			*ps_calls->result = err;
			throw PhotoshopExc("Photoshop error.");
		}
			
		queryAbort();
		
		// multiply transparency by layermask
		assert(_transparency);
		
		float *transparency_pix = transparency_buf;
		float *layermask_pix = layermask_buf;
		
		for(int x=0; x < _width * _height; x++)
			*transparency_pix++ *= *layermask_pix++;
	}
	
	
	// now get the buffer we came for
	PixelMemoryDesc memDesc = { _data, _rowbytes * 8, colbytes * 8, 0, 32 };
	
	OSErr err = ReadProc(_desc->port, &scaling, &writeRect, &memDesc, &wroteRect);
	
	if(err != noErr)
	{
		PS_callbacks *ps_calls = writePS_base.ps_calls();
		*ps_calls->result = err;
		throw PhotoshopExc("Photoshop error.");
	}
		
	queryAbort();
	
	// multiply by the alpha
	if(_transparency)
	{
		float *channel_pix = (float *)_data;
		float *transparency_pix = transparency_buf;
		
		for(int x=0; x < _width * _height; x++)
			*channel_pix++ *= *transparency_pix++;
	}
	
	
	// return the buffer
	if(use_half)
	{
		// copy float to half
		float *channel_pix = (float *)_data;
		half *half_pix = (half *)_half_data;
		
		for(int x=0; x < _width * _height; x++)
			*half_pix++ = *channel_pix++;
			
		size_t half_colbytes = sizeof(half);
		
		ProEXRbuffer half_buf = {Imf::HALF, _half_data, _width, _height, half_colbytes, _half_rowbytes};
		
		return half_buf;
	}
	else
	{
		ProEXRbuffer buf = {Imf::FLOAT, _data, _width, _height, colbytes, _rowbytes};
		
		return buf;
	}
}

ProEXRlayer_readPS::ProEXRlayer_readPS(string name) :
	ProEXRlayer_read(name),
	_visibility(true),
	_adjustment_layer(false),
	_opacity(255),
	_transfer_mode(MODE_Normal)
{

}

ProEXRlayer_readPS::~ProEXRlayer_readPS()
{

}

void
ProEXRlayer_readPS::setupWithLayerString(string &layerString)
{
	// split the layer string into its three parts
	vector<string> layer_parts;
	
	quotedTokenize(layerString, layer_parts, "[]{}");
	
	if(layer_parts.size() != 3)
		throw BaseExc("Bogus layer string.");
	
	// got the name
	setName( deQuote(layer_parts[0]) );

	
	// split the prop part
	vector<string> prop_parts;
	
	quotedTokenize(layer_parts[1], prop_parts, ", ");
	
	for(vector<string>::iterator i = prop_parts.begin(); i != prop_parts.end(); ++i)
	{
		vector<string> prop_key;
		
		quotedTokenize(*i, prop_key, ":");
		
		if(prop_key.size() == 2)
		{
			if(prop_key[0] == "visible" && prop_key[1] == "false")
				_visibility = false;
			else if(prop_key[0] == "adjustment_layer" && prop_key[1] == "true")
				_adjustment_layer = true;
			else if(prop_key[0] == "mode" && prop_key[1] != "Normal")
				_transfer_mode = StringToEnumBlendMode( prop_key[1] );
			else if(prop_key[0] == "opacity" && prop_key[1] != "255")
			{
				int scan = -1;
				
				sscanf(prop_key[1].c_str(), "%d", &scan);
				
				if(scan >= 0 && scan <= 255)
					_opacity = scan;
			}
		}
	}
	
	// split the channel part
	vector<string> channel_parts;
	
	quotedTokenize(layer_parts[2], channel_parts, ", ");
	
	for(vector<string>::iterator i = channel_parts.begin(); i != channel_parts.end(); ++i)
	{
		// not paying attention to the channel assignments, actually
		string channel_part = *i;
		
		if(channel_part.size() < 3 || channel_part[1] != ':')
			throw BaseExc("Bogus channel part.");
		
		string chan_key = channel_part.substr(2);
		string chan_name = deQuote( chan_key );
		
		if(!doc())
			throw LogicExc("Wait, why don't I have a doc?");
		
		ProEXRchannel *c = doc()->findChannel(chan_name);
		
		if(c)
			addChannel(c, false);
	}
}

void
ProEXRlayer_readPS::copyToPhotoshop(int required_rgb_channels, int required_alpha_channels, ProEXRchannel_readPS *shared_alpha, bool unMult) const
{
	if(doc() == NULL)
		throw BaseExc("doc() is NULL.");
		
	ProEXRdoc_readPS &readPS_doc = dynamic_cast<ProEXRdoc_readPS &>( *doc() );
	
	size_t cheapNumRows = MIN(doc()->height(), readPS_doc.safeLines());

	const vector<ProEXRchannel *> &chans = channels();
	
	assert( chans.size() );
	
	if(chans[0]->pixelType() == Imf::UINT)
	{
		assert(chans.size() == 1);
		assert(required_rgb_channels == 3);
		
		if( chans[0]->loaded() )
		{
			try{
				vector<ProEXRchannel_readPS *> rgb_chans = chans[0]->getUintRGBchannels<ProEXRchannel_readPS>();
				
				rgb_chans[0]->copyToPhotoshop(0, NULL);
				rgb_chans[1]->copyToPhotoshop(1, NULL);
				rgb_chans[2]->copyToPhotoshop(2, NULL);
				
				delete rgb_chans[0];
				delete rgb_chans[1];
				delete rgb_chans[2];
				
			}catch(...)
			{
				// try again the cheap way
				chans[0]->freeBuffers();
				return copyToPhotoshop(required_rgb_channels, required_alpha_channels, shared_alpha, unMult);
			}
		}
		else
		{
			// load and copy uint pixels, being cheap with memory
			PS_callbacks *ps_calls = readPS_doc.ps_calls();
			
			if(ps_calls == NULL || ps_calls->advanceState == NULL)
				throw BaseExc("bad ps_calls");
			
			const Box2i &dw = readPS_doc.file().dataWindow();
			
			int width = (dw.max.x - dw.min.x) + 1;
			
			size_t uint_rowbytes = sizeof(unsigned int) * width;
			AutoArray<unsigned int> uint_buf = new unsigned int[width * cheapNumRows];
			
			size_t float_rowbytes = sizeof(FloatPixel) * width;
			AutoArray<FloatPixel> float_buf = new FloatPixel[width * cheapNumRows];
			
			
			try{
				for(int y = dw.min.y; y < dw.min.y + readPS_doc.height() && *ps_calls->result == noErr; y += cheapNumRows)
				{
					if(readPS_doc.parts() > 1)
					{
						memset(uint_buf, 0, sizeof(unsigned int) * width * cheapNumRows);
						memset(float_buf, 0, sizeof(FloatPixel) * width * cheapNumRows);
					}
					
					// read from the EXR
					char *uint_row = (char *)uint_buf.get();
					char *float_row = (char *)float_buf.get();
					
					char *uint_origin = uint_row - (y * uint_rowbytes) - (dw.min.x * sizeof(unsigned int));
					
					FrameBuffer frameBuffer;
					frameBuffer.insert(chans[0]->name(), Slice(Imf::UINT, uint_origin, sizeof(unsigned int), uint_rowbytes, 1, 1, 0.0) );
					
					int end_scanline = MIN(y + cheapNumRows - 1, dw.min.y + readPS_doc.height() - 1);
					
					readPS_doc.file().setFrameBuffer(frameBuffer);
					readPS_doc.file().readPixels(y, end_scanline);
					
					// copy UINT to FLOAT
					for(int local_y=y; local_y <= end_scanline; local_y++)
					{
						unsigned int *uint_pix = (unsigned int *)uint_row;
						FloatPixel *float_pix = (FloatPixel *)float_row;
						
						for(int x=0; x < readPS_doc.width(); x++)
						{
							*float_pix++ = Uint2rgb(*uint_pix++);
						}
						
						uint_row += uint_rowbytes;
						float_row += float_rowbytes;
					}
					
					// hand off to Photoshop
					*ps_calls->planeBytes = sizeof(float);
					*ps_calls->colBytes = sizeof(FloatPixel);
					*ps_calls->rowBytes = float_rowbytes;

					ps_calls->theRect->left = ps_calls->theRect32->left = 0;
					ps_calls->theRect->right = ps_calls->theRect32->right = readPS_doc.width();
					ps_calls->theRect->top = ps_calls->theRect32->top = y - dw.min.y;
					ps_calls->theRect->bottom = ps_calls->theRect32->bottom = 1 + end_scanline - dw.min.y;
					
					*ps_calls->loPlane = 0;
					*ps_calls->hiPlane = 2;
					
					*ps_calls->data = float_buf;
					
					*ps_calls->result = ps_calls->advanceState();
				}
			}
			catch(Iex::InputExc) {}
			catch(Iex::IoExc) {}
			
			if(*ps_calls->result != noErr)
				throw PhotoshopExc("Photoshop error.");
		}
		
		// and now the alpha
		if(required_alpha_channels)
		{
			if(shared_alpha)
				shared_alpha->copyToPhotoshop(3, NULL);
			else
				readPS_doc.copyWhiteChannelToPhotoshop(3);
		}
	}
	else if( loadAsLayer() && !chans[0]->loaded() ) // aka, Y[RY][BY], not loaded
	{
		// load and copy uint pixels, being cheap with memory
		PS_callbacks *ps_calls = readPS_doc.ps_calls();
		
		if(ps_calls == NULL || ps_calls->advanceState == NULL)
			throw BaseExc("bad ps_calls");
		
		const vector<ProEXRchannel *> &chans = channels();
		
		assert(chans.size() >= 3);
		assert(chans[0]->name() == "Y");
		assert(chans[1]->name() == "RY");
		assert(chans[2]->name() == "BY");
		
		bool have_a = (chans.size() >= 4 && chans[3]->name() == "A");

		// now use the Rgba interface
		Imf::IStream &file_in = readPS_doc.stream();
		file_in.seekg(0); // expected to be at the beginning, apparently
		
		RgbaInputFile inputFile( file_in );
		
		Box2i dw = inputFile.dataWindow();
		int width = (dw.max.x - dw.min.x) + 1;
		
		// buffers
		Array2D<Rgba> half_buffer(cheapNumRows, width);
		
		size_t float_rowbytes = sizeof(float) * width * 4;
		AutoArray<float> float_buf = new float[width * 4 * cheapNumRows];
		

		try{
			for(int y = dw.min.y; y < dw.min.y + readPS_doc.height() && *ps_calls->result == noErr; y += cheapNumRows)
			{
				if(readPS_doc.parts() > 1)
					memset(float_buf.get(), 0, sizeof(float) * width * 4 * cheapNumRows);
			
				// read from the EXR
				char *float_row = (char *)float_buf.get();
				
				inputFile.setFrameBuffer(&half_buffer[-y][-dw.min.x], 1, width);
				
				int end_scanline = MIN(y + cheapNumRows - 1, dw.min.y + readPS_doc.height() - 1);
				
				inputFile.readPixels(y, end_scanline);
				
				// copy HALF to FLOAT
				for(int local_y=y; local_y <= end_scanline; local_y++)
				{
					float *float_pix = (float *)float_row;
					
					for(int x=0; x < readPS_doc.width(); x++)
					{
						Rgba &rgba_pix = half_buffer[local_y - y][x];
						
						if(readPS_doc.getClipAlpha())
						{
							if(rgba_pix.a < 0.f)
								rgba_pix.a = 0.f;
							else if(rgba_pix.a > 1.f)
								rgba_pix.a = 1.f;
						}
						
						if(unMult && have_a && rgba_pix.a > 0.0f && rgba_pix.a < 1.0f)
						{
							*float_pix++ = (float)rgba_pix.r / (float)rgba_pix.a;
							*float_pix++ = (float)rgba_pix.g / (float)rgba_pix.a;
							*float_pix++ = (float)rgba_pix.b / (float)rgba_pix.a;
							*float_pix++ = rgba_pix.a;
						}
						else
						{
							*float_pix++ = rgba_pix.r;
							*float_pix++ = rgba_pix.g;
							*float_pix++ = rgba_pix.b;
							*float_pix++ = rgba_pix.a;
						}
					}
					
					float_row += float_rowbytes;
				}
				
				// hand off to Photoshop
				*ps_calls->planeBytes = sizeof(float);
				*ps_calls->colBytes = sizeof(float) * 4;
				*ps_calls->rowBytes = float_rowbytes;

				ps_calls->theRect->left = ps_calls->theRect32->left = 0;
				ps_calls->theRect->right = ps_calls->theRect32->right = readPS_doc.width();
				ps_calls->theRect->top = ps_calls->theRect32->top = y - dw.min.y;
				ps_calls->theRect->bottom = ps_calls->theRect32->bottom = 1 + end_scanline - dw.min.y;
				
				*ps_calls->loPlane = 0;
				*ps_calls->hiPlane = have_a ? 3 : 2;
				
				*ps_calls->data = float_buf;
				
				*ps_calls->result = ps_calls->advanceState();
			}
			
			if(required_alpha_channels && !have_a)
				readPS_doc.copyWhiteChannelToPhotoshop(3);
				
		}
		catch(Iex::InputExc) {}
		catch(Iex::IoExc) {}
	}
	else
	{
		ProEXRchannel_readPS *r = NULL;
		ProEXRchannel_readPS *g = NULL;
		ProEXRchannel_readPS *b = NULL;
		ProEXRchannel_readPS *a = NULL;
		
		ProEXRchannel_readPS *unMult_alpha = NULL;
		
		vector<ProEXRchannel *> my_channels = chans; // working copy
		
		// find an alpha
		for(vector<ProEXRchannel *>::iterator i = my_channels.begin(); i != my_channels.end(); ++i)
		{
			if( (*i)->channelTag() == CHAN_A )
			{
				a = dynamic_cast<ProEXRchannel_readPS *>( *i );
				assert(a);
				
				my_channels.erase(i); // take out of the working copy
				break;
			}
		}
		
		if( my_channels.size() )
		{
			// the other channels will go into rgb
			for(int i=0; i < my_channels.size(); i++)
			{
				if(r == NULL)
				{
					r = dynamic_cast<ProEXRchannel_readPS *>( my_channels[i] );
					assert(r);
				}
				else if(g == NULL)
				{
					g = dynamic_cast<ProEXRchannel_readPS *>( my_channels[i] );
					assert(g);
				}
				else if(b == NULL)
				{
					b = dynamic_cast<ProEXRchannel_readPS *>( my_channels[i] );
					assert(b);
				}
			}
		}
		else
		{
			// whoops, guess we have a split-off alpha layer
			r = a;
			
			if(required_alpha_channels)
			{
				if(r == NULL)
					throw BaseExc("r is NULL");
					
				const bool cheap_with_memory = !r->loaded();
			
				if(cheap_with_memory)
				{
					int alpha_chan_number = required_alpha_channels;
					
					readPS_doc.copyWhiteChannelToPhotoshop(alpha_chan_number);
					
					a = NULL;
				}
				else
					a = readPS_doc.getWhiteChannel<ProEXRchannel_readPS>();
			}
		}
		
		if(r == NULL)
			throw BaseExc("r is NULL");
					
		bool cheap_with_memory = !r->loaded();
		
		// maybe we only have one or two?
		if(r != NULL && g == NULL && b == NULL && required_rgb_channels >= 3)
		{
			g = b = r; // copy r values
		}
		else if(r != NULL && g != NULL && b == NULL && required_rgb_channels >= 3)
		{
			if(cheap_with_memory)
				readPS_doc.copyBlackChannelToPhotoshop(2);
			else
				b = readPS_doc.getBlackChannel<ProEXRchannel_readPS>(); // fill with black
		}
		
		if(unMult)
		{
			if(a)
				unMult_alpha = a;
			else
				unMult_alpha = dynamic_cast<ProEXRchannel_readPS *>( alphaChannel() ); // a disconnected alpha, perhaps
		}
		
		if(a == NULL && required_alpha_channels)
		{
			if(shared_alpha)
			{
				a = shared_alpha;
				
				if(unMult)
					unMult_alpha = a;
			}
			else
			{
				if(cheap_with_memory)
					readPS_doc.copyWhiteChannelToPhotoshop(3);
				else
					a = readPS_doc.getWhiteChannel<ProEXRchannel_readPS>();
			}
		}
		
		// temporarily load the layer's alpha, shared among the channels
		bool unload_unMult = false;
		
		if( unMult_alpha && !unMult_alpha->loaded() )
		{
			try{
				unMult_alpha->loadFromFile();
				unload_unMult = true;
			}
			catch(bad_alloc) {}
		}
		
		// copy channels intelligently if possible
		if(r == g && r != NULL)
		{
			if(r == b)
			{
				r->copyToPhotoshop(0, unMult_alpha, 3);
			}
			else
			{
				r->copyToPhotoshop(0, unMult_alpha, 2);
				if(b && required_rgb_channels > 2) b->copyToPhotoshop(2, unMult_alpha);
			}
		}
		else
		{
			if(r && required_rgb_channels > 0) r->copyToPhotoshop(0, unMult_alpha);
			if(g && required_rgb_channels > 1) g->copyToPhotoshop(1, unMult_alpha);
			if(b && required_rgb_channels > 2) b->copyToPhotoshop(2, unMult_alpha);
		}
		
		if(a && required_alpha_channels > 0)
			a->copyToPhotoshop(required_rgb_channels, NULL);
		
		// we said this was temporary
		if(unload_unMult && unMult_alpha)
			unMult_alpha->freeBuffers();
	}
}

ProEXRlayer_writePS::ProEXRlayer_writePS(ReadLayerDesc *layerInfo, string name, Imf::PixelType pixelType) :
	ProEXRlayer(name)
{
	if(layerInfo == NULL)
		throw BaseExc("layerInfo is NULL.");
	
	_layerInfo = layerInfo;
	
	_visibility = layerInfo->isVisible;
	_adjustment_layer = layerInfo->isAdjustor;

	// merge two 0-255 opacities into one
	_opacity =	(layerInfo->fillOpacity == 255) ? layerInfo->opacity :
				(layerInfo->opacity == 255) ? layerInfo->fillOpacity :
				( ( ((float)layerInfo->fillOpacity / 255.0f) * ((float)layerInfo->opacity / 255.0f) ) * 255.0f) + 0.5f;
	
	_transfer_mode = PStoEnumBlendMode(layerInfo->blendMode);
	
	setupLayer(layerInfo->compositeChannelsList, layerInfo->transparency, layerInfo->transparency, layerInfo->layerMask, pixelType);
}

ProEXRlayer_writePS::ProEXRlayer_writePS(ReadChannelDesc *channel_list, ReadChannelDesc *alpha_chan,
						ReadChannelDesc *transparency_chan, ReadChannelDesc *layermask_chan, string name, Imf::PixelType pixelType) :
	ProEXRlayer(name),
	_layerInfo(NULL),
	_visibility(true),
	_adjustment_layer(false),
	_opacity(255),
	_transfer_mode(MODE_Normal)
{
	setupLayer(channel_list, alpha_chan, transparency_chan, layermask_chan, pixelType);
}

ProEXRlayer_writePS::~ProEXRlayer_writePS()
{

}

string
ProEXRlayer_writePS::layerString() const
{
	string layerString = enQuoteIfNecessary(name(), " \"[]{}");	
	
	// basic layer info
	layerString += "[";
	layerString += "visible:" + (_visibility ? string("true") : string("false"));
	layerString += ", mode:" + EnumToStringBlendMode(_transfer_mode);
	
	if(_opacity != 255)
	{
		char num_str[5];
		sprintf(num_str, "%d", _opacity);
		layerString += ", opacity:" + string(num_str);
	}
	
	if(_adjustment_layer)
		layerString += ", adjustment_layer:true";
	
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
ProEXRlayer_writePS::loadFromPhotoshop() const
{
	// see if we have an alpha channel here and load it first
	ProEXRchannel_writePS *alpha = dynamic_cast<ProEXRchannel_writePS *>( alphaChannel() );
	
	if(alpha)
		alpha->loadFromPhotoshop(true);
	
	
	// the alpha channel and the premult channel aren't necessarily the same
	ProEXRchannel_writePS *premult = NULL;
	
	// get transparency channel
	auto_ptr<ProEXRchannel_writePS> temp_transparency;
	
	if(alpha && alpha->desc() == _transparency_chan)
	{
		premult = alpha;
	}
	else if(_transparency_chan)
	{
		temp_transparency.reset( new ProEXRchannel_writePS("temp transparency", _transparency_chan, Imf::FLOAT) );
		premult = temp_transparency.get();
		assert(premult);
		
		assert( doc() );
		temp_transparency->assignDoc( doc() );
		
		temp_transparency->loadFromPhotoshop(true);
		
		alpha = temp_transparency.get();
	}
	
	// get layer mask
	if(_layermask_chan)
	{
		auto_ptr<ProEXRchannel_writePS> layermask( new ProEXRchannel_writePS("temp layer mask", _layermask_chan, Imf::FLOAT) );
		
		if(layermask.get() == NULL)
			throw BaseExc("bad layermask.");
		
		assert( doc() );
		layermask->assignDoc( doc() );
		
		layermask->loadFromPhotoshop(true);
		
		if(premult == NULL)
			throw BaseExc("bad premult."); // should never have a layermask without transparency
		
		// multiply the transparency by the layer mask
		premult->premultiply(layermask.get(), true);
	}
	
	// now get the channels and premultiply them (except for alpha)
	for(vector<ProEXRchannel *>::const_iterator i = channels().begin(); i != channels().end(); ++i)
	{
		ProEXRchannel_writePS &ps_channel = dynamic_cast<ProEXRchannel_writePS &>( **i );
		
		if(ps_channel.channelTag() != CHAN_A)
		{
			ps_channel.loadFromPhotoshop(false);
			
			if(premult)
				ps_channel.premultiply(premult);
		}
	}
}

void 
ProEXRlayer_writePS::writeLayerFile(OStream &os, const Header &header, Imf::PixelType pixelType) const
{
	Header head = header; // make a copy of the header
	assert(head.channels().begin() == head.channels().end()); // i.e. no channels in the header yet
	
	if(doc() == NULL)
		throw BaseExc("doc() is NULL.");
	
	if(doc()->loaded() && channels().size() >= 4)
	{
		try{
			// use the pre-existing channels
			//sortChannels();
			
			ProEXRchannel *red = channels().at(0);
			ProEXRchannel *green = channels().at(1);
			ProEXRchannel *blue = channels().at(2);
			ProEXRchannel *alpha = channels().at(3);
			
			assert(red && green && blue && alpha);
			
			head.channels().insert("R", pixelType);
			head.channels().insert("G", pixelType);
			head.channels().insert("B", pixelType);
			head.channels().insert("A", pixelType);

			ProEXRbuffer r_buffer = red->getBufferDesc(pixelType == Imf::HALF);
			ProEXRbuffer g_buffer = green->getBufferDesc(pixelType == Imf::HALF);
			ProEXRbuffer b_buffer = blue->getBufferDesc(pixelType == Imf::HALF);
			ProEXRbuffer a_buffer = alpha->getBufferDesc(pixelType == Imf::HALF);
			
			
			Box2i &dw = head.dataWindow();
			
			char *r_origin = (char *)r_buffer.buf - (dw.min.y * r_buffer.rowbytes) - (dw.min.x * r_buffer.colbytes);
			char *g_origin = (char *)g_buffer.buf - (dw.min.y * g_buffer.rowbytes) - (dw.min.x * g_buffer.colbytes);
			char *b_origin = (char *)b_buffer.buf - (dw.min.y * b_buffer.rowbytes) - (dw.min.x * b_buffer.colbytes);
			char *a_origin = (char *)a_buffer.buf - (dw.min.y * a_buffer.rowbytes) - (dw.min.x * a_buffer.colbytes);
			
			
			FrameBuffer frameBuffer;
			
			frameBuffer.insert("R", Slice(r_buffer.type, r_origin, r_buffer.colbytes, r_buffer.rowbytes) );
			frameBuffer.insert("G", Slice(g_buffer.type, g_origin, g_buffer.colbytes, g_buffer.rowbytes) );
			frameBuffer.insert("B", Slice(b_buffer.type, b_origin, b_buffer.colbytes, b_buffer.rowbytes) );
			frameBuffer.insert("A", Slice(a_buffer.type, a_origin, a_buffer.colbytes, a_buffer.rowbytes) );
			
			OutputFile file(os, head);

			file.setFrameBuffer(frameBuffer);
			
			file.writePixels(r_buffer.height);
		}
		catch(bad_alloc)
		{
			// free buffers and try again
			doc()->freeBuffers();
			writeLayerFile(os, header, pixelType);
		}
	}
	else
	{
		// create a new doc (which will also create new buffers)
		ProEXRdoc_writePS *main_doc = dynamic_cast<ProEXRdoc_writePS *>( doc() );

		if(_layerInfo == NULL)
			throw LogicExc("_layerInfo is NULL");
	
		ProEXRdoc_writePS file(os, head, pixelType, main_doc->ps_calls(), _layerInfo, _layerInfo->transparency);
		
		file.writeFile();
	}
}

void 
ProEXRlayer_writePS::writeLayerFileRGBA(OStream &os, const Header &header, RgbaChannels mode) const
{
	Header head = header; // make a copy of the header
	assert(head.channels().begin() == head.channels().end()); // i.e. no channels in the header yet
	
	if(doc() == NULL)
		throw BaseExc("doc() is NULL.");
	
	if(doc()->loaded() && channels().size() >= 4)
	{
		try{
			// use the pre-existing channels
			//sortChannels();
			
			ProEXRchannel *red = channels().at(0);
			ProEXRchannel *green = channels().at(1);
			ProEXRchannel *blue = channels().at(2);
			ProEXRchannel *alpha = channels().at(3);
			
			assert(red && green && blue && alpha);
			
			writeRGBAfile(os, head, mode, red, green, blue, alpha);
		}
		catch(bad_alloc)
		{
			// free buffers and try again
			doc()->freeBuffers();
			writeLayerFileRGBA(os, header, mode);
		}
	}
	else
	{
		// create a new doc (which will also create new buffers)
		ProEXRdoc_writePS_base &main_doc = dynamic_cast<ProEXRdoc_writePS &>( *doc() );
	
		ProEXRdoc_writePS_RGBA file(os, head, mode, main_doc.ps_calls(), _layerInfo, _layerInfo->transparency);
		
		file.writeFile();
	}
}

void
ProEXRlayer_writePS::setupLayer(ReadChannelDesc *channel_list, ReadChannelDesc *alpha_chan,
							ReadChannelDesc *transparency_chan, ReadChannelDesc *layermask_chan, Imf::PixelType pixelType)
{
	_transparency_chan = transparency_chan;
	_layermask_chan = layermask_chan;
	
	vector<string> channel_vec;
	
	string layer_name = name();
	
	// we allow layers to end with .RGBA and .RGB, which we replace with [R][G][B]([A])
	// if we don't have the bracket format after that, we assume they want RGBA added
	// this means that most random layer names will get RGBA appended
	searchReplaceEnd(layer_name, ".RGBA", ".[R][G][B][A]");
	searchReplaceEnd(layer_name, ".RGB", ".[R][G][B]");
	searchReplaceEnd(layer_name, ".YA", ".[Y][A]");
	searchReplaceEnd(layer_name, ".Y", ".[Y]");
	
	if(layer_name == "RGBA")
		layer_name = "[R][G][B][A]";
	else if(layer_name == "RGB")
		layer_name = "[R][G][B]";
	else if(layer_name == "YA")
		layer_name = "[Y][A]";
	else if(layer_name == "Y")
		layer_name = "[Y]";
	
	
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
	
	
	// phew, we should have a vector of channels, now where to assign them?
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
	
	// this function is getting longer and less elegant
		
	// did we get an alpha only? then alpha is red
	if(!alpha.empty() && red.empty() && green.empty() && blue.empty())	
	{	
		red = alpha;	
		alpha = "";
	}

	// now set up channels with our names
	ReadChannelDesc *current_channel = channel_list;
	
	if(current_channel)
	{
		if( !red.empty() )
			channels().push_back(new ProEXRchannel_writePS(red, current_channel, pixelType) );
		
		current_channel = current_channel->next;
	}

	if(current_channel)
	{
		if( !green.empty() )
			channels().push_back(new ProEXRchannel_writePS(green, current_channel, pixelType) );
		
		current_channel = current_channel->next;
	}

	if(current_channel)
	{
		if( !blue.empty() )
			channels().push_back(new ProEXRchannel_writePS(blue, current_channel, pixelType) );
		
		current_channel = current_channel->next;
	}

	if( alpha_chan && !alpha.empty() )
		channels().push_back(new ProEXRchannel_writePS(alpha, alpha_chan, pixelType) );
}


ProEXRdoc_readPS::ProEXRdoc_readPS(Imf::IStream &is, PS_callbacks *ps_calls, bool unMult, bool clip_alpha, bool do_layers, bool split_alpha, bool use_layers_string, bool renameFirstPart, bool set_up) :
	ProEXRdoc_read(is, clip_alpha, renameFirstPart, false),
	_ps_calls(ps_calls),
	_unMult(unMult),
	_ps_layers(do_layers),
	_used_layers_string(false)
{
	if(set_up)
		setupDoc(split_alpha, use_layers_string);
	
	calculateSafeLines();
}

ProEXRdoc_readPS::~ProEXRdoc_readPS()
{

}

int16
ProEXRdoc_readPS::ps_mode() const
{
	return (ps_planes() <= 2 ? plugInModeGrayScale : plugInModeRGBColor); // Photoshop doesn't like me saying plugInModeRGB96
}

int16
ProEXRdoc_readPS::ps_planes() const
{
	if(_ps_layers)
		return 4;
	else
	{
		ProEXRlayer *layer = findMainLayer();
		
		if(layer == NULL)
			throw BaseExc("layer is NULL.");
		
		int main_chans = layer->channels().size();
		bool has_alpha = (layer->alphaChannel() != NULL);
		
		if(main_chans == 1 && !has_alpha && getAlphaChannel() != NULL)
		{
			main_chans++;  // the shared alpha did not get connected with this layer for some reason
			has_alpha = true;
			assert(FALSE); // really, this should never happen
		}
		
		if(main_chans == 1 || (main_chans == 2 && has_alpha) )
		{
			return main_chans;
		}
		else
		{
			if(main_chans >= 4)
				return 4;
			else
				return 3;
		}
	}
}

Vec2<Fixed>
ProEXRdoc_readPS::ps_res() const
{
	// dpi
	float h_dpi = 72.0f;
	
	if( hasXDensity( header() ) )
		h_dpi = xDensity( header() );
		
	Vec2<Fixed> res;
	
	// pixel aspect ratio
	res.x = (h_dpi * 65536.0f) + 0.5f; // 72 dpi Fixed
	res.y = ( (h_dpi * 65536.0f) * header().pixelAspectRatio() ) + 0.5;
	
	return res;
}

void
ProEXRdoc_readPS::loadFromFile(bool force)
{
	if(SafeAvailableMemory(true) > (memorySize() * 2) || force)
	{
		ProEXRdoc_read::loadFromFile();
		
		if(_unMult)
			unMult(); // won't do anything if the file failed to load
	}
}

void
ProEXRdoc_readPS::copyMainToPhotoshop() const
{
	ProEXRlayer_readPS *main = dynamic_cast<ProEXRlayer_readPS *>( findMainLayer() );
	
	if(main == NULL)
		throw BaseExc("no main.");
	
	ProEXRchannel *alph = getAlphaChannel();
	ProEXRchannel_readPS *alpha = dynamic_cast<ProEXRchannel_readPS *>( alph );
	
	if(alph != NULL && alpha == NULL)
		throw BaseExc("dynamic_cast problem.");
	
	// it was probably not loaded previously if we're calling this
	main->loadFromFile();
	
	if(_unMult && alpha)
		main->unMult( alpha );
	
	if(_ps_layers)
		main->copyToPhotoshop(3, 1, alpha, _unMult);
	else
	{
		int rgb_channels = (ps_mode() == plugInModeGrayScale ? 1 : 3);
		int alpha_channels = (alpha != NULL ? 1 : 0);
		
		main->copyToPhotoshop(rgb_channels, alpha_channels, alpha, _unMult);
	}
}

void
ProEXRdoc_readPS::copyLayerToPhotoshop(int index, bool use_shared_alpha)
{
	ProEXRlayer_readPS &layer = dynamic_cast<ProEXRlayer_readPS &>( *layers().at(index) );

	ProEXRchannel_readPS *alpha = NULL;
	
	if(use_shared_alpha && !_used_layers_string)
	{
		ProEXRchannel *alph = getAlphaChannel();
		
		alpha = dynamic_cast<ProEXRchannel_readPS *>( alph );
		
		if(alph != NULL && alpha == NULL)
			throw BaseExc("dynamic_cast problem.");
		
		// let's at least load the shared alpha if nothing else
		if(alpha && !alpha->loaded() )
		{
			try{
				alpha->loadFromFile();
			}
			catch(bad_alloc) {}
		}
	}
	
	calculateSafeLines();
	
	if(_ps_layers)
		layer.copyToPhotoshop(3, 1, alpha, _unMult);
	else
		layer.copyToPhotoshop(3, 0, alpha, _unMult);
}

void
ProEXRdoc_readPS::copyConstantChannelToPhotoshop(int16 channel, float value) const
{
	// the idea here is that we're being cheap with memory
	size_t cheapNumRows = MIN(height(), safeLines());
	
	if(ps_calls() == NULL || ps_calls()->advanceState == NULL)
		throw BaseExc("ps_calls problem.");
	
	size_t float_rowbytes = sizeof(float) * width();
	AutoArray<float> float_buf = new float[width() * cheapNumRows];
	
	
	// fill the buffer
	float *fill_pix = float_buf;
	
	for(int x=0; x < width() * cheapNumRows; x++)
		*fill_pix++ = value;
	
	for(int y=0; y < height() && *ps_calls()->result == noErr; y += cheapNumRows)
	{
		// hand off to Photoshop repeatedly
		int end_scanline = MIN(y + cheapNumRows - 1, height() - 1);
		
		*ps_calls()->planeBytes = sizeof(float);
		*ps_calls()->colBytes = *ps_calls()->planeBytes;
		*ps_calls()->rowBytes = float_rowbytes;

		ps_calls()->theRect->left = ps_calls()->theRect32->left = 0;
		ps_calls()->theRect->right = ps_calls()->theRect32->right = width();
		ps_calls()->theRect->top = ps_calls()->theRect32->top = y;
		ps_calls()->theRect->bottom = ps_calls()->theRect32->bottom = 1 + end_scanline;
		
		*ps_calls()->loPlane = *ps_calls()->hiPlane = channel;
		
		*ps_calls()->data = float_buf;
		
		*ps_calls()->result = ps_calls()->advanceState();
	}
	
	if(*ps_calls()->result != noErr)
		throw PhotoshopExc("Photoshop error.");
}

void
ProEXRdoc_readPS::queryAbort()
{
	if(ps_calls() && ps_calls()->abortProc)
	{
		if( ps_calls()->abortProc() )
			throw AbortExc("User Abort");
	}
}

void
ProEXRdoc_readPS::setupDoc(bool split_alpha, bool use_layers_string)
{
	initChannels<ProEXRchannel_readPS>();
	
	const StringAttribute *ps_layers = header().findTypedAttribute<StringAttribute>(PS_LAYERS_KEY);
	
	if(use_layers_string && ps_layers && !split_alpha)
	{
		try
		{
			string layersString = ps_layers->value();
			
			vector<string> layerStrings = splitLayersString(layersString);
			
			if( layerStrings.size() )
			{
				for(vector<string>::iterator i = layerStrings.begin(); i != layerStrings.end(); ++i)
				{
					string layerString = *i;
					
					ProEXRlayer_readPS *new_layer = new ProEXRlayer_readPS;
					
					new_layer->assignDoc(this);
					
					new_layer->setupWithLayerString(layerString);
					
					new_layer->assignAlpha(NULL);
					
					layers().push_back( new_layer ); 
				}

				_used_layers_string = true;
			}
			else
				buildLayers<ProEXRlayer_readPS>(split_alpha);
		}
		catch(BaseExc)
		{
			buildLayers<ProEXRlayer_readPS>(split_alpha);
		}
	}
	else
		buildLayers<ProEXRlayer_readPS>(split_alpha);
}

void
ProEXRdoc_readPS::unMult()
{
	if(_used_layers_string)
	{
		// don't want to unMult with a shared alpha in this case
		for(vector<ProEXRlayer *>::iterator i = layers().begin(); i != layers().end(); ++i)
			(*i)->unMult(NULL);
	}
	else
	{
		ProEXRdoc::unMult();
	}
}

void
ProEXRdoc_readPS::calculateSafeLines()
{
	size_m free_mem = SafeAvailableMemory(false);
	
	_safeLines = MIN( MAX( ( free_mem / (sizeof(float) * width() * 2) ), 16), 256);
}

ProEXRdoc_writePS::ProEXRdoc_writePS(OStream &os, Header &header, Imf::PixelType pixelType, bool do_layers, bool hidden_layers,
							PS_callbacks *ps_calls, ReadImageDocumentDesc *documentInfo, ReadChannelDesc *alpha_chan) :
	ProEXRdoc_write(os, header),
	ProEXRdoc_writePS_base(ps_calls)
{
	if(do_layers)
	{
		setupDocLayers(documentInfo, pixelType, hidden_layers);
		
		// only add this if we really had layers
		if(documentInfo->layersDescriptor)
			header.insert(PS_LAYERS_KEY, StringAttribute( layersString() ) );
	}
	else
	{
		bool greyscale = (documentInfo->imageMode == plugInModeGrayScale || documentInfo->imageMode == plugInModeGray32);
		
		setupDocSimple(documentInfo->mergedCompositeChannels, alpha_chan, documentInfo->mergedTransparency, NULL, pixelType, greyscale);
	}
	
	calculateSafeLines();
}

ProEXRdoc_writePS::ProEXRdoc_writePS(OStream &os, Header &header, Imf::PixelType pixelType,
							PS_callbacks *ps_calls, ReadLayerDesc *layerInfo, ReadChannelDesc *alpha_chan) :
	ProEXRdoc_write(os, header),
	ProEXRdoc_writePS_base(ps_calls)
{
	setupDocSimple(layerInfo->compositeChannelsList, alpha_chan, layerInfo->transparency, layerInfo->layerMask, pixelType);
	
	_safeLines = MAX( ( SafeAvailableMemory(false) / (sizeof(float) * width() * channels().size() * 2 * (pixelType == Imf::HALF ? 2 : 1) ) ), 4);
}

ProEXRdoc_writePS::~ProEXRdoc_writePS()
{

}

void
ProEXRdoc_writePS::addMainLayer(ReadChannelDesc *channel_list, ReadChannelDesc *alpha_chan,
								ReadChannelDesc *transparency_chan, ReadChannelDesc *layermask_chan, Imf::PixelType pixelType)
{
	ProEXRlayer *old_main = findMainLayer(true, false);
	
	if(old_main)
		old_main->incrementChannels();
	
	string layer_name = alpha_chan ? "RGBA" : "RGB"; // yes, this matters
	
	addLayer(new ProEXRlayer_writePS(channel_list, alpha_chan, transparency_chan, layermask_chan, layer_name, pixelType) );
}

void
ProEXRdoc_writePS::loadFromPhotoshop(bool force) const
{
	if(SafeAvailableMemory(true) > (memorySize() * 2) || force)
	{
		try{
			for(vector<ProEXRlayer *>::const_iterator i = layers().begin(); i != layers().end(); ++i)
			{
				ProEXRlayer_writePS &the_layer = dynamic_cast<ProEXRlayer_writePS &>( **i );
				
				the_layer.loadFromPhotoshop();
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
ProEXRdoc_writePS::writeFile()
{
	if( loaded() )
	{
		try
		{
			ProEXRdoc_write::writeFile();
		}
		catch(bad_alloc)
		{
			// we ran out of memory, unload everything and try again
			freeBuffers();
			writeFile();
		}
	}
	else
	{
		// line-by-line cheap memory method
		unsigned int cheapNumWriteRows = safeLines();
		
		// assign alpha channels to each channel
		for(vector<ProEXRlayer *>::iterator i = layers().begin(); i != layers().end(); ++i)
		{
			ProEXRlayer_writePS &writePS_layer = dynamic_cast<ProEXRlayer_writePS &>( **i );
			
			vector<ProEXRchannel *> non_alpha_channels = writePS_layer.getNonAlphaChannels();
			
			if(non_alpha_channels.size() == 0)
			{
				assert( writePS_layer.channels().size() > 0 );
				non_alpha_channels.push_back( writePS_layer.channels().at(0) ); // in case of alpha only layer
			}

			vector<ProEXRchannel *> &channels = writePS_layer.channels();
			
			for(vector<ProEXRchannel *>::iterator j = channels.begin(); j != channels.end(); ++j)
			{
				ProEXRchannel_writePS &writePS_channel = dynamic_cast<ProEXRchannel_writePS &>( **j );
			
				writePS_layer.assignMyTransparency( &writePS_channel );
			}
		}
		
		
		Header &head = header();
		vector<ProEXRchannel *> &chans = channels();
		
		assert( head.channels().begin() == head.channels().end() ); // i.e., there are no channels in the header now

		// insert channels into header
		for(int i=0; i < chans.size(); i++)
			head.channels().insert( chans[i]->name().c_str(),  chans[i]->pixelType() );

		
		Box2i dw = head.dataWindow();
		int dw_height = (dw.max.y - dw.min.y) + 1;
		
		
		OutputFile file(stream(), head);
		
		
		for(int y=0; y < dw_height; y += cheapNumWriteRows)
		{
			int end_scanline = MIN(y + cheapNumWriteRows - 1, dw_height - 1);
			
			FrameBuffer frameBuffer;
			
			for(int i=0; i < chans.size(); i++)
			{
				ProEXRchannel_writePS &chan = dynamic_cast<ProEXRchannel_writePS &>( *chans[i] );
				
				ProEXRbuffer buffer = chan.getLoadedLineBufferDesc(y, end_scanline, chan.pixelType() == Imf::HALF);
					
				if(buffer.buf == NULL)
					throw BaseExc("buffer.buf is NULL.");
				
				char *buffer_origin = (char *)buffer.buf - (dw.min.y * buffer.rowbytes) - (dw.min.x * buffer.colbytes);
				
				frameBuffer.insert(chan.name().c_str(),
							Slice(buffer.type, buffer_origin - (y * buffer.rowbytes), buffer.colbytes, buffer.rowbytes) );
			}
			
			file.setFrameBuffer(frameBuffer);
			
			file.writePixels(1 + end_scanline - y);

			ps_calls()->progressProc(end_scanline, dw_height);
		}
	}
}

string
ProEXRdoc_writePS::layersString() const
{
	string layers_string;
	
	bool first_one = true;
	
	for(vector<ProEXRlayer *>::const_iterator i = layers().begin(); i != layers().end(); ++i)
	{
		ProEXRlayer_writePS &the_layer = dynamic_cast<ProEXRlayer_writePS &>( **i );
		
		if(first_one)
			first_one = false;
		else
			layers_string += ", ";
		
		layers_string += the_layer.layerString();
	}
	
	return layers_string;
}

void
ProEXRdoc_writePS::queryAbort()
{
	if(ps_calls() && ps_calls()->abortProc)
	{
		if( ps_calls()->abortProc() )
			throw AbortExc("User Abort");
	}
}

void
ProEXRdoc_writePS::setupDocLayers(ReadImageDocumentDesc *documentInfo, Imf::PixelType pixelType, bool hidden_layers)
{
	if(documentInfo->layersDescriptor)
	{
		ReadLayerDesc *current_layer = documentInfo->layersDescriptor;
		
		while(current_layer)
		{
			if(current_layer->isPixelBased && (current_layer->isVisible || hidden_layers) )
				addLayer(new ProEXRlayer_writePS(current_layer, UTF16toUTF8(current_layer->unicodeName), pixelType) );
			
			current_layer = current_layer->next;
		}
	}
	else
		setupDocSimple(documentInfo->mergedCompositeChannels, documentInfo->mergedTransparency, documentInfo->mergedTransparency, NULL, pixelType);
}

void
ProEXRdoc_writePS::setupDocSimple(ReadChannelDesc *channel_list, ReadChannelDesc *alpha_chan,
					ReadChannelDesc *transparency_chan, ReadChannelDesc *layermask_chan, Imf::PixelType pixelType, bool greyscale)
{
	// setup just one layer
	// in this case we just use the merged image
	string layer_name;
	
	if(greyscale)
		layer_name = alpha_chan ? "YA" : "Y"; // yes, this matters
	else
		layer_name = alpha_chan ? "RGBA" : "RGB"; // yes, this matters
	
	addLayer(new ProEXRlayer_writePS(channel_list, alpha_chan, transparency_chan, layermask_chan, layer_name, pixelType) );
}

void
ProEXRdoc_writePS::calculateSafeLines()
{
	size_m free_mem = SafeAvailableMemory(false);
	
	_safeLines = MIN( MAX( ( free_mem / (sizeof(float) * width() * channels().size() * 2) ), 4), 256);
}


ProEXRdoc_writePS_RGBA::ProEXRdoc_writePS_RGBA(OStream &os, Header &header, RgbaChannels mode,
							PS_callbacks *ps_calls, ReadImageDocumentDesc *documentInfo, ReadChannelDesc *alpha_chan) :
	ProEXRdoc_writeRGBA(os, header, mode),
	ProEXRdoc_writePS_base(ps_calls)
{
	setupDoc(documentInfo->mergedCompositeChannels, alpha_chan, documentInfo->mergedTransparency, NULL);
	
	calculateSafeLines();
}

ProEXRdoc_writePS_RGBA::ProEXRdoc_writePS_RGBA(OStream &os, Header &header, RgbaChannels mode,
							PS_callbacks *ps_calls, ReadLayerDesc *layerInfo, ReadChannelDesc *alpha_chan) :
	ProEXRdoc_writeRGBA(os, header, mode),
	ProEXRdoc_writePS_base(ps_calls)
{
	setupDoc(layerInfo->compositeChannelsList, alpha_chan, layerInfo->transparency, layerInfo->layerMask);
	
	calculateSafeLines();
}

ProEXRdoc_writePS_RGBA::~ProEXRdoc_writePS_RGBA()
{

}

void
ProEXRdoc_writePS_RGBA::loadFromPhotoshop(bool force)
{
	assert(layers().size() == 1);
	
	ProEXRlayer_writePS &the_layer = dynamic_cast<ProEXRlayer_writePS &>( *layers().at(0) );
	
	if(SafeAvailableMemory(true) > (memorySize() * 2) || force)
	{
		try{
			the_layer.loadFromPhotoshop();
		}
		catch(bad_alloc)
		{
			the_layer.freeBuffers();
		}
	}
}

void
ProEXRdoc_writePS_RGBA::writeFile()
{
	if( loaded() )
	{
		try
		{
			ProEXRdoc_writeRGBA::writeFile();
		}
		catch(bad_alloc)
		{
			// we ran out of memory, unload everything and try again
			freeBuffers();
			writeFile();
		}
	}
	else
	{
		// line-by-line cheap memory method
		unsigned int cheapNumWriteRows = safeLines();
		
		// assign alpha channels to channels in layer
		assert(layers().size() == 1);
		
		ProEXRlayer_writePS &writePS_layer = dynamic_cast<ProEXRlayer_writePS &>( *layers().at(0) );
		
		vector<ProEXRchannel *> non_alpha_channels = writePS_layer.getNonAlphaChannels();
		
		for(vector<ProEXRchannel *>::iterator j = non_alpha_channels.begin(); j != non_alpha_channels.end(); ++j)
		{
			ProEXRchannel_writePS &writePS_channel = dynamic_cast<ProEXRchannel_writePS &>( **j );
		
			writePS_layer.assignMyTransparency( &writePS_channel );
		}
		
		// now write the RGBA file
		assert(channels().size() >= 3);
		
		ProEXRchannel_writePS *r_chan = dynamic_cast<ProEXRchannel_writePS *>( findChannel("R") );
		ProEXRchannel_writePS *g_chan = dynamic_cast<ProEXRchannel_writePS *>( findChannel("G") );
		ProEXRchannel_writePS *b_chan = dynamic_cast<ProEXRchannel_writePS *>( findChannel("B") );
		ProEXRchannel_writePS *a_chan = dynamic_cast<ProEXRchannel_writePS *>( findChannel("A") );
		
		assert(r_chan && g_chan && b_chan); // just doing RGB folks, but write any mode you want
		assert(!r_chan->loaded() && !g_chan->loaded() && !b_chan->loaded());
		
		if(mode() & WRITE_A)
			assert(a_chan != NULL && !a_chan->loaded());
		
		
		// sorting out dimensions
		Box2i &dw = header().dataWindow();
		
		int width = (dw.max.x - dw.min.x) + 1;
		int height = (dw.max.y - dw.min.y) + 1;
		
		// if YCC, must have even dimensions
		if(mode() & WRITE_YC)
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
		
		int buf_width = this->width();
		int buf_height = this->height();
		
		int fill_cols = width - buf_width;
		int fill_rows = height - buf_height;
		
		// our buffer to fill
		Array2D<Rgba> half_buffer(cheapNumWriteRows, width);
		
		
		RgbaOutputFile file(stream(), header(), mode());
		
		
		int last_buffer_row_used = 0;
		
		for(int y=0; y < buf_height; y += cheapNumWriteRows)
		{
			int end_scanline = MIN(y + cheapNumWriteRows - 1, buf_height - 1);
			
			FrameBuffer frameBuffer;
			
			ProEXRbuffer r_buffer = r_chan->getLoadedLineBufferDesc(y, end_scanline, false);
			ProEXRbuffer g_buffer = g_chan->getLoadedLineBufferDesc(y, end_scanline, false);
			ProEXRbuffer b_buffer = b_chan->getLoadedLineBufferDesc(y, end_scanline, false);
			
			ProEXRbuffer a_buffer = {Imf::FLOAT, NULL, 0, 0, 0, 0};
			
			if(a_chan)
				a_buffer = a_chan->getLoadedLineBufferDesc(y, end_scanline, false);
			
			// copy FLOAT to HALF
			char *r_row = (char *)r_buffer.buf;
			char *g_row = (char *)g_buffer.buf;
			char *b_row = (char *)b_buffer.buf;
			char *a_row = (char *)a_buffer.buf;
			
			for(int local_y=y; local_y <= end_scanline; local_y++)
			{
				float *r_pix = (float *)r_row;
				float *g_pix = (float *)g_row;
				float *b_pix = (float *)b_row;
				float *a_pix = (float *)a_row;
				
				for(int x=0; x < buf_width; x++)
				{
					Rgba &rgba_pix = half_buffer[local_y - y][x];
					
					rgba_pix.r = *r_pix++;
					rgba_pix.g = *g_pix++;
					rgba_pix.b = *b_pix++;
					
					if(a_chan)
						rgba_pix.a = *a_pix++;
					else
						rgba_pix.a = 1.0f;
				}
				
				for(int c=1; c <= fill_cols; c++)
					half_buffer[local_y - y][(buf_width-1)+c] = half_buffer[local_y - y][buf_width-1];
					
				r_row += r_buffer.rowbytes;
				g_row += g_buffer.rowbytes;
				b_row += b_buffer.rowbytes;
				a_row += a_buffer.rowbytes;
			}
			
			file.setFrameBuffer(&half_buffer[-y][0], 1, width);
			
			file.writePixels(1 + end_scanline - y);

			ps_calls()->progressProc(end_scanline, buf_height);
			
			last_buffer_row_used = end_scanline - y;
		}
		
		for(int r=1; r <= fill_rows; r++)
		{
			Array2D<Rgba> fill_buffer(1, width);
			
			for(int x=0; x < width; x++)
				fill_buffer[0][x] = half_buffer[last_buffer_row_used][x];
			
			file.setFrameBuffer(&fill_buffer[-(buf_height+r)][0], 1, width);
			
			file.writePixels(1);
		}
	}
}

void
ProEXRdoc_writePS_RGBA::queryAbort()
{
	if(ps_calls() && ps_calls()->abortProc)
	{
		if( ps_calls()->abortProc() )
			throw AbortExc("User Abort");
	}
}

void
ProEXRdoc_writePS_RGBA::setupDoc(ReadChannelDesc *channel_list, ReadChannelDesc *alpha_chan,
									ReadChannelDesc *transparency_chan, ReadChannelDesc *layermask_chan)
{
	// setup just one layer
	// in this case we just use the merged image
	string layer_name = alpha_chan ? "RGBA" : "RGB"; // yes, this matters
	
	addLayer(new ProEXRlayer_writePS(channel_list, alpha_chan, transparency_chan, layermask_chan, layer_name, Imf::HALF) );
}

void
ProEXRdoc_writePS_RGBA::calculateSafeLines()
{
	size_m free_mem = SafeAvailableMemory(false);
	
	_safeLines = MIN( MAX( ( free_mem / (sizeof(float) * width() * 4 * 2 * 2) ), 4), 256);
}


ProEXRdoc_writePS_Deep::ProEXRdoc_writePS_Deep(Imf::OStream &os, Imf::Header &header, bool hidden_layers,
							PS_callbacks *ps_calls, ReadImageDocumentDesc *documentInfo) :
	ProEXRdoc_writePS(os, header, Imf::HALF, true, hidden_layers, ps_calls, documentInfo, NULL)
{

}

ProEXRdoc_writePS_Deep::~ProEXRdoc_writePS_Deep()
{

}

void
ProEXRdoc_writePS_Deep::writeFile()
{
	if( loaded() )
	{
		//try
		//{
		//	ProEXRdoc_write::writeFile();
		//}
		//catch(bad_alloc)
		//{
			assert(FALSE);
			
			// not currently writing deep files from a loaded ProEXRdoc
			freeBuffers();
			writeFile();
		//}
	}
	else
	{
		// line-by-line cheap memory method
		const unsigned int cheapNumWriteRows = safeLines();
		
		// assign alpha channels to each channel
		for(vector<ProEXRlayer *>::iterator i = layers().begin(); i != layers().end(); ++i)
		{
			ProEXRlayer_writePS &writePS_layer = dynamic_cast<ProEXRlayer_writePS &>( **i );
			
			vector<ProEXRchannel *> non_alpha_channels = writePS_layer.getNonAlphaChannels();
			
			if(non_alpha_channels.size() == 0)
			{
				assert( writePS_layer.channels().size() > 0 );
				non_alpha_channels.push_back( writePS_layer.channels().at(0) ); // in case of alpha only layer
			}

			vector<ProEXRchannel *> &channels = writePS_layer.channels();
			
			for(vector<ProEXRchannel *>::iterator j = channels.begin(); j != channels.end(); ++j)
			{
				ProEXRchannel_writePS &writePS_channel = dynamic_cast<ProEXRchannel_writePS &>( **j );
			
				writePS_layer.assignMyTransparency( &writePS_channel );
			}
		}
		
		
		Header &head = header();
		vector<ProEXRlayer *> &lays = layers();
		
		head.erase(PS_LAYERS_KEY);
		
		assert( head.channels().begin() == head.channels().end() ); // i.e., there are no channels in the header now

		// insert channels into header
		head.channels().insert("R", Imf::HALF);
		head.channels().insert("G", Imf::HALF);
		head.channels().insert("B", Imf::HALF);
		head.channels().insert("A", Imf::HALF);
		head.channels().insert("Z", Imf::HALF);

		
		const Box2i dw = head.dataWindow();
		const int dw_width = (dw.max.x - dw.min.x) + 1;
		const int dw_height = (dw.max.y - dw.min.y) + 1;
		
		
		if(false) // In case you wanted to make a tiled image
		{
			const int tileSize = 256;
		
			head.setTileDescription( TileDescription(tileSize, tileSize) );
		
			DeepTiledOutputFile file(stream(), head);
			
			int tile_y = 0;
			
			for(int y=0; y < dw_height; y += tileSize)
			{
				const int end_scanline = MIN(y + tileSize - 1, dw_height - 1);
				
				const int block_height = 1 + end_scanline - y;
				
				
				// get all the pixel buffers
				vector< vector<ProEXRbuffer> > layer_bufs( lays.size() );
				
				for(int i=0; i < lays.size(); i++)
				{
					const ProEXRlayer *layer = lays[lays.size() - i - 1]; // Photoshop gives us layers from bottom up, for deep we sort top down
					const vector<ProEXRchannel *> &chans = layer->channels();
					
					layer_bufs[i].resize( chans.size() );
					
					for(int j=0; j < chans.size(); j++)
					{
						ProEXRchannel_writePS &chan = dynamic_cast<ProEXRchannel_writePS &>( *chans[j] );
					
						layer_bufs[i][j] = chan.getLoadedLineBufferDesc(y, end_scanline, true);
					}
				}
				
				// make buffers
				Array2D<unsigned int> countbuf(block_height, dw_width);
				Array2D<half *> r_buf(block_height, dw_width);
				Array2D<half *> g_buf(block_height, dw_width);
				Array2D<half *> b_buf(block_height, dw_width);
				Array2D<half *> a_buf(block_height, dw_width);
				Array2D<half *> z_buf(block_height, dw_width);
				
				
				for(int dy=0; dy < block_height; dy++)
				{
					for(int dx=0; dx < dw_width; dx++)
					{
						countbuf[dy][dx] = 0;
						r_buf[dy][dx] = g_buf[dy][dx] = b_buf[dy][dx] = a_buf[dy][dx] = z_buf[dy][dx] = NULL;
					}
				}
				
				
				for(int dy=0; dy < block_height; dy++)
				{
					// get row pointers for each channel
					vector< vector<half *> > layer_ptrs( lays.size() );
				
					for(int i=0; i < layer_bufs.size(); i++)
					{
						layer_ptrs[i].resize(layer_bufs[i].size());
						
						for(int j=0; j < layer_bufs[i].size(); j++)
						{
							assert(layer_bufs[i][j].type == Imf::HALF);
						
							layer_ptrs[i][j] = (half *)((char *)layer_bufs[i][j].buf + (dy * layer_bufs[i][j].rowbytes));
						}
					}
					
					// count how many samples we'll need for each deep pixel
					for(int i=0; i < layer_bufs.size(); i++)
					{
						const vector<half *> &layer_vec = layer_ptrs[i];
						
						const half *a = layer_vec[ layer_vec.size() - 1 ];
						
						for(int dx=0; dx < dw_width; dx++)
						{
							if(a[dx] != 0.f)
							{
								countbuf[dy][dx]++;
							}
						}
					}
					
					// allocate samples and enter pixel data
					for(int dx=0; dx < dw_width; dx++)
					{
						if(countbuf[dy][dx] > 0)
						{
							r_buf[dy][dx] = new half[ countbuf[dy][dx] ];
							g_buf[dy][dx] = new half[ countbuf[dy][dx] ];
							b_buf[dy][dx] = new half[ countbuf[dy][dx] ];
							a_buf[dy][dx] = new half[ countbuf[dy][dx] ];
							z_buf[dy][dx] = new half[ countbuf[dy][dx] ];
							
							int n = 0;
							
							for(int i=0; i < layer_ptrs.size(); i++)
							{
								const vector<half *> &layer_vec = layer_ptrs[i];
							
								const half *a = layer_vec[ layer_vec.size() - 1 ];
								
								if(a && a[dx] != 0.f)
								{
									if(layer_vec.size() > 0)
										r_buf[dy][dx][n] = layer_vec[0][dx];

									if(layer_vec.size() > 1)
										g_buf[dy][dx][n] = layer_vec[1][dx];

									if(layer_vec.size() > 2)
										b_buf[dy][dx][n] = layer_vec[2][dx];

									a_buf[dy][dx][n] = a[dx];
									
									z_buf[dy][dx][n] = (1 + i) * 100;

									n++;
									
									assert(n <= countbuf[dy][dx]);
								}
							}
							
							assert(n == countbuf[dy][dx]);
						}
					}
				}
				
				
				char *countOrigin = (char *)&countbuf[-y][0] - (dw.min.y * sizeof(unsigned int) * dw_width) - (dw.min.x * sizeof(unsigned int));
				
				char *r_origin = (char *)&r_buf[-y][0] - (dw.min.y * sizeof(half *) * dw_width) - (dw.min.x * sizeof(half *));
				char *g_origin = (char *)&g_buf[-y][0] - (dw.min.y * sizeof(half *) * dw_width) - (dw.min.x * sizeof(half *));
				char *b_origin = (char *)&b_buf[-y][0] - (dw.min.y * sizeof(half *) * dw_width) - (dw.min.x * sizeof(half *));
				char *a_origin = (char *)&a_buf[-y][0] - (dw.min.y * sizeof(half *) * dw_width) - (dw.min.x * sizeof(half *));
				char *z_origin = (char *)&z_buf[-y][0] - (dw.min.y * sizeof(half *) * dw_width) - (dw.min.x * sizeof(half *));
				
				
				DeepFrameBuffer frameBuffer;
				
				frameBuffer.insertSampleCountSlice( Slice(Imf::UINT, countOrigin, sizeof(unsigned int), sizeof(unsigned int) * dw_width) );
				
				frameBuffer.insert("R", DeepSlice(Imf::HALF, r_origin, sizeof(half *), sizeof(half *) * dw_width, sizeof(half)) );
				frameBuffer.insert("G", DeepSlice(Imf::HALF, g_origin, sizeof(half *), sizeof(half *) * dw_width, sizeof(half)) );
				frameBuffer.insert("B", DeepSlice(Imf::HALF, b_origin, sizeof(half *), sizeof(half *) * dw_width, sizeof(half)) );
				frameBuffer.insert("A", DeepSlice(Imf::HALF, a_origin, sizeof(half *), sizeof(half *) * dw_width, sizeof(half)) );
				frameBuffer.insert("Z", DeepSlice(Imf::HALF, z_origin, sizeof(half *), sizeof(half *) * dw_width, sizeof(half)) );
				
				
				file.setFrameBuffer(frameBuffer);
				
				
				assert(tile_y < file.numYTiles());

				file.writeTiles(0, file.numXTiles() - 1, tile_y, tile_y);
				
				
				// delete all those deep pixels
				for(int dy=0; dy < block_height; dy++)
				{
					for(int dx=0; dx < dw_width; dx++)
					{
						delete [] r_buf[dy][dx];
						delete [] g_buf[dy][dx];
						delete [] b_buf[dy][dx];
						delete [] a_buf[dy][dx];
						delete [] z_buf[dy][dx];
					}
				}

				ps_calls()->progressProc(end_scanline, dw_height);
				
				tile_y++;
			}
		}
		else
		{
			DeepScanLineOutputFile file(stream(), head);
			
			
			for(int y=0; y < dw_height; y += cheapNumWriteRows)
			{
				const int end_scanline = MIN(y + cheapNumWriteRows - 1, dw_height - 1);
				
				const int block_height = 1 + end_scanline - y;
				
				
				// get all the pixel buffers
				vector< vector<ProEXRbuffer> > layer_bufs( lays.size() );
				
				for(int i=0; i < lays.size(); i++)
				{
					const ProEXRlayer *layer = lays[lays.size() - i - 1]; // Photoshop gives us layers from bottom up, for deep we sort top down
					const vector<ProEXRchannel *> &chans = layer->channels();
					
					layer_bufs[i].resize( chans.size() );
					
					for(int j=0; j < chans.size(); j++)
					{
						ProEXRchannel_writePS &chan = dynamic_cast<ProEXRchannel_writePS &>( *chans[j] );
					
						layer_bufs[i][j] = chan.getLoadedLineBufferDesc(y, end_scanline, true);
					}
				}
				
				// make buffers
				Array2D<unsigned int> countbuf(block_height, dw_width);
				Array2D<half *> r_buf(block_height, dw_width);
				Array2D<half *> g_buf(block_height, dw_width);
				Array2D<half *> b_buf(block_height, dw_width);
				Array2D<half *> a_buf(block_height, dw_width);
				Array2D<half *> z_buf(block_height, dw_width);
				
				
				for(int dy=0; dy < block_height; dy++)
				{
					for(int dx=0; dx < dw_width; dx++)
					{
						countbuf[dy][dx] = 0;
						r_buf[dy][dx] = g_buf[dy][dx] = b_buf[dy][dx] = a_buf[dy][dx] = z_buf[dy][dx] = NULL;
					}
				}
				
				
				for(int dy=0; dy < block_height; dy++)
				{
					// get row pointers for each channel
					vector< vector<half *> > layer_ptrs( lays.size() );
				
					for(int i=0; i < layer_bufs.size(); i++)
					{
						layer_ptrs[i].resize(layer_bufs[i].size());
						
						for(int j=0; j < layer_bufs[i].size(); j++)
						{
							assert(layer_bufs[i][j].type == Imf::HALF);
						
							layer_ptrs[i][j] = (half *)((char *)layer_bufs[i][j].buf + (dy * layer_bufs[i][j].rowbytes));
						}
					}
					
					// count how many samples we'll need for each deep pixel
					for(int i=0; i < layer_bufs.size(); i++)
					{
						const vector<half *> &layer_vec = layer_ptrs[i];
						
						const half *a = layer_vec[ layer_vec.size() - 1 ];
						
						for(int dx=0; dx < dw_width; dx++)
						{
							if(a[dx] != 0.f)
							{
								countbuf[dy][dx]++;
							}
						}
					}
					
					// allocate samples and enter pixel data
					for(int dx=0; dx < dw_width; dx++)
					{
						if(countbuf[dy][dx] > 0)
						{
							r_buf[dy][dx] = new half[ countbuf[dy][dx] ];
							g_buf[dy][dx] = new half[ countbuf[dy][dx] ];
							b_buf[dy][dx] = new half[ countbuf[dy][dx] ];
							a_buf[dy][dx] = new half[ countbuf[dy][dx] ];
							z_buf[dy][dx] = new half[ countbuf[dy][dx] ];
							
							int n = 0;
							
							for(int i=0; i < layer_ptrs.size(); i++)
							{
								const vector<half *> &layer_vec = layer_ptrs[i];
							
								const half *a = layer_vec[ layer_vec.size() - 1 ];
								
								if(a && a[dx] != 0.f)
								{
									if(layer_vec.size() > 0)
										r_buf[dy][dx][n] = layer_vec[0][dx];

									if(layer_vec.size() > 1)
										g_buf[dy][dx][n] = layer_vec[1][dx];

									if(layer_vec.size() > 2)
										b_buf[dy][dx][n] = layer_vec[2][dx];

									a_buf[dy][dx][n] = a[dx];
									
									z_buf[dy][dx][n] = (1 + i) * 100;

									n++;
									
									assert(n <= countbuf[dy][dx]);
								}
							}
							
							assert(n == countbuf[dy][dx]);
						}
					}
				}
				
				
				char *countOrigin = (char *)&countbuf[-y][0] - (dw.min.y * sizeof(unsigned int) * dw_width) - (dw.min.x * sizeof(unsigned int));
				
				char *r_origin = (char *)&r_buf[-y][0] - (dw.min.y * sizeof(half *) * dw_width) - (dw.min.x * sizeof(half *));
				char *g_origin = (char *)&g_buf[-y][0] - (dw.min.y * sizeof(half *) * dw_width) - (dw.min.x * sizeof(half *));
				char *b_origin = (char *)&b_buf[-y][0] - (dw.min.y * sizeof(half *) * dw_width) - (dw.min.x * sizeof(half *));
				char *a_origin = (char *)&a_buf[-y][0] - (dw.min.y * sizeof(half *) * dw_width) - (dw.min.x * sizeof(half *));
				char *z_origin = (char *)&z_buf[-y][0] - (dw.min.y * sizeof(half *) * dw_width) - (dw.min.x * sizeof(half *));
				
				
				DeepFrameBuffer frameBuffer;
				
				frameBuffer.insertSampleCountSlice( Slice(Imf::UINT, countOrigin, sizeof(unsigned int), sizeof(unsigned int) * dw_width) );
				
				frameBuffer.insert("R", DeepSlice(Imf::HALF, r_origin, sizeof(half *), sizeof(half *) * dw_width, sizeof(half)) );
				frameBuffer.insert("G", DeepSlice(Imf::HALF, g_origin, sizeof(half *), sizeof(half *) * dw_width, sizeof(half)) );
				frameBuffer.insert("B", DeepSlice(Imf::HALF, b_origin, sizeof(half *), sizeof(half *) * dw_width, sizeof(half)) );
				frameBuffer.insert("A", DeepSlice(Imf::HALF, a_origin, sizeof(half *), sizeof(half *) * dw_width, sizeof(half)) );
				frameBuffer.insert("Z", DeepSlice(Imf::HALF, z_origin, sizeof(half *), sizeof(half *) * dw_width, sizeof(half)) );
				
				
				file.setFrameBuffer(frameBuffer);
				
				file.writePixels(block_height);
				
				
				// delete all those deep pixels
				for(int dy=0; dy < block_height; dy++)
				{
					for(int dx=0; dx < dw_width; dx++)
					{
						delete [] r_buf[dy][dx];
						delete [] g_buf[dy][dx];
						delete [] b_buf[dy][dx];
						delete [] a_buf[dy][dx];
						delete [] z_buf[dy][dx];
					}
				}

				ps_calls()->progressProc(end_scanline, dw_height);
			}
		}
	}
}
