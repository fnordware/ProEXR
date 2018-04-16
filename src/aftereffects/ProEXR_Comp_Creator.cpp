
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

#include "ProEXR_Comp_Creator.h"

#include "OpenEXR_PlatformIO.h"
#include "ProEXRdoc_PS.h"

#include "VRimgVersion.h"
#include "VRimgInputFile.h"

#include "ProEXR_AE_Dialogs.h"

#include "ProEXR_UTF.h"

#include <ImfVersion.h>
//#include <ImfStandardAttributes.h>

#include <ImfChannelList.h>
#include <ImfFloatAttribute.h>
#include <ImfDoubleAttribute.h>
#include <ImfIntAttribute.h>
#include <ImfBoxAttribute.h>
#include <ImfVecAttribute.h>
#include <ImfCompressionAttribute.h>
#include <ImfLineOrderAttribute.h>
#include <ImfTileDescriptionAttribute.h>
#include <ImfEnvmapAttribute.h>
#include <ImfStringVectorAttribute.h>
#include <ImfFloatVectorAttribute.h>


#include <list>


using namespace std;
using namespace Imf;
using namespace Imath;



extern AEGP_PluginID S_mem_id;
extern AEGP_Command gCompCreatorCmd;

static SPBasicSuite			*sP							=	NULL;

AEGP_InstalledEffectKey		gEXtractoR_key				=	NULL;
AEGP_InstalledEffectKey		gIDentifier_key				=	NULL;
AEGP_InstalledEffectKey		gCryptomatte_key			=	NULL;

static	A_char				S_path[AEGP_MAX_PATH_SIZE+1]	=	{'\0'};


enum {
	DO_EXTRACT = 1,
	DO_COPY,
	DO_FULL_ON,
	DO_FULL_OFF,
	DO_NOTHING
};
typedef A_char	ExtractAction;

#define MAX_CHANNEL_NAME_LEN 127

typedef struct {
	ExtractAction	action;
	A_long			index; // 0-based index in the file
	char			reserved[27]; // total of 32 bytes up to here
	A_char			name[MAX_CHANNEL_NAME_LEN+1];
} ChannelData;


typedef struct {
	A_u_char		version; // version of this data structure
	A_Boolean		single_channel;
	char			reserved[30]; // total of 32 bytes up to here
	ChannelData		red;
	ChannelData		green;
	ChannelData		blue;
	ChannelData		alpha;
} EXtractoRArbitraryData;

#define EXTRACTOR_ARB_INDEX		1
#define EXTRACTOR_UNMULT_INDEX	5


typedef struct {
	A_u_char		version; // version of this data structure
	A_long			index; // 0-based index in the file
	char			reserved[11]; // total of 16 bytes up to here
	A_char			name[MAX_CHANNEL_NAME_LEN+1];
} IDentifierArbitraryData;

#define IDENTIFIER_ARB_INDEX	1


#ifndef MAX_LAYER_NAME_LEN
#define MAX_LAYER_NAME_LEN 63 // same as PF_CHANNEL_NAME_LEN
#endif

typedef struct {
	char		magic[4]; // "cry1"
	char		reserved[28]; // 32 bytes at this point
	char		layer[MAX_LAYER_NAME_LEN + 1];
	A_u_long	manifest_size; // including null character
	A_u_long	manifest_hash;
	A_u_long	selection_size;
	A_u_long	selection_hash;
	char		data[4]; // manifest string + selection string 
} CryptomatteArbitraryData;

#define CRYPTOMATTE_ARB_INDEX	1

#ifndef SWAP_LONG
#define SWAP_LONG(a)		((a >> 24) | ((a >> 8) & 0xff00) | ((a << 8) & 0xff0000) | (a << 24))
#endif


static void 
SwapArbData(CryptomatteArbitraryData *arb_data)
{
	arb_data->manifest_size = SWAP_LONG(arb_data->manifest_size);
	arb_data->manifest_hash = SWAP_LONG(arb_data->manifest_hash);
	arb_data->selection_size = SWAP_LONG(arb_data->selection_size);
	arb_data->selection_size = SWAP_LONG(arb_data->selection_size);
}


static A_u_long
djb2(const A_u_char *data, size_t len)
{
	A_u_long hash = 5381;
	
	while(len--)
		hash = ((hash << 5) + hash) + *data++; // hash * 33 + c
	
	return hash;
}


static void
HashManifest(CryptomatteArbitraryData *arb)
{
	arb->manifest_hash = djb2((A_u_char *)&arb->data[0], arb->manifest_size);
}


static void
HashSelection(CryptomatteArbitraryData *arb)
{
	arb->selection_hash = djb2((A_u_char *)&arb->data[arb->manifest_size], arb->selection_size);
}


const char *
GetLayer(const CryptomatteArbitraryData *arb)
{
	return arb->layer;
}


const char *
GetSelection(const CryptomatteArbitraryData *arb)
{
	return &arb->data[arb->manifest_size];
}


const char *
GetManifest(const CryptomatteArbitraryData *arb)
{
	return &arb->data[0];
}


static A_Handle MakeCryptomatteHandle(AEGP_SuiteHandler &suites, std::string layer, const std::string &manifest)
{
	const std::string selection("");
	
	assert(sizeof(CryptomatteArbitraryData) == 116);
	
	const size_t siz = sizeof(CryptomatteArbitraryData) + manifest.size() + selection.size();
	
	A_Handle handle = (A_Handle)suites.HandleSuite()->host_new_handle( siz );
	
	CryptomatteArbitraryData *arb = (CryptomatteArbitraryData *)suites.HandleSuite()->host_lock_handle((PF_Handle)handle);
	
	arb->magic[0] = 'c';
	arb->magic[1] = 'r';
	arb->magic[2] = 'y';
	arb->magic[3] = '1';

	if(layer.size() > MAX_LAYER_NAME_LEN)
		layer.resize(MAX_LAYER_NAME_LEN);
	
	strncpy(arb->layer, layer.c_str(), MAX_LAYER_NAME_LEN + 1);
	
	arb->manifest_size = manifest.size() + 1;
	arb->selection_size = selection.size() + 1;
	
	strncpy(&arb->data[0], manifest.c_str(), arb->manifest_size);
	strncpy(&arb->data[arb->manifest_size], selection.c_str(), arb->selection_size);
	
	HashManifest(arb);
	HashSelection(arb);
	
	#ifdef AE_BIG_ENDIAN
		// really, you're compiling this for PPC?
		SwapArbData(arb);
	#endif
	
	suites.HandleSuite()->host_unlock_handle((PF_Handle)handle);
	
	return handle;
}


#ifdef AE_HFS_PATHS
// convert from HFS paths (fnord:Users:mrb:) to Unix paths (/Users/mrb/)
static int ConvertPath(const char * inPath, char * outPath, int outPathMaxLen)
{
	CFStringRef inStr = CFStringCreateWithCString(kCFAllocatorDefault, inPath ,kCFStringEncodingMacRoman);
	if (inStr == NULL)
		return -1;
	CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, inStr, kCFURLHFSPathStyle,0);
	CFStringRef outStr = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
	if (!CFStringGetCString(outStr, outPath, outPathMaxLen, kCFURLPOSIXPathStyle))
		return -1;
	CFRelease(outStr);
	CFRelease(url);
	CFRelease(inStr);
	return 0;
}
#endif // AE_HFS_PATHS

#if WIN_ENV
static inline bool KeyIsDown(int vKey)
{
	return (GetAsyncKeyState(vKey) & 0x8000);
}
#endif

static inline bool OptionKeyHeld(void)
{
#if MAC_ENV
    UInt32 keys = GetCurrentEventKeyModifiers();
	
	return ( (keys & optionKey) || (keys & rightOptionKey) );
#endif

#if WIN_ENV
	return ( KeyIsDown(VK_LMENU) || KeyIsDown(VK_RMENU) );
#endif
}


static inline bool ShiftKeyHeld(void)
{
#if MAC_ENV
    UInt32 keys = GetCurrentEventKeyModifiers();
	
	return ( (keys & shiftKey) || (keys & rightShiftKey) );
#endif

#if WIN_ENV
	return ( KeyIsDown(VK_LSHIFT) || KeyIsDown(VK_RSHIFT) );
#endif
}

#pragma mark-

// like a thicker string, but not quite a rope, get it?
template <typename T>
class yarn
{
  public:
	yarn();
	yarn(const yarn &other);
	yarn(const T *ptr);
	yarn(const string &s); 
	~yarn();
	
	yarn & operator = (const yarn &other);
	
	const T *get() const { return _p; }
	operator const T * () const { return get(); }
	
	string str() const;
	operator string () const { return str(); }
	
	size_t size() const;
	
  private:
	T *_p;
};


template <typename T>
yarn<T>::yarn()
{
	_p = new T[1];
	_p[0] = '\0';
}


template <typename T>
yarn<T>::yarn(const yarn &other)
{
	size_t len = other.size() + 1;

	_p = new T[len];
	
	memcpy(_p, other.get(), len * sizeof(T));
}


template <typename T>
yarn<T>::yarn(const T *ptr)
{
	const T *p = ptr;
	size_t len = 0;
	
	while(*p++ != '\0')
		len++;
		
	len++; // make space for the null

	_p = new T[len];
	
	memcpy(_p, ptr, len * sizeof(T));
}


template <>
yarn<char>::yarn(const string &s)
{
	size_t len = s.size() + 1;

	_p = new char[len];
	
	strncpy(_p, s.c_str(), len);
}


template <>
yarn<utf16_char>::yarn(const string &s)
{
	size_t len = s.size() + 1;

	_p = new utf16_char[len];
	
	UTF8toUTF16(s, _p, len);
}


template <typename T>
yarn<T> &
yarn<T>::operator = (const yarn &other)
{
	if(this != &other) // prevent self-assignment
	{
		delete [] _p;
		
		size_t len = other.size() + 1;

		_p = new T[len];
		
		memcpy(_p, other.get(), len * sizeof(T));
	}
	
	return *this;
}


template <typename T>
yarn<T>::~yarn()
{
	delete [] _p;
}


template<>
string
yarn<char>::str() const
{
	return string(_p);
}


template<>
string
yarn<utf16_char>::str() const
{
	return UTF16toUTF8(_p);
}


template <typename T>
size_t
yarn<T>::size() const
{
	T *p = _p;
	size_t len = 0;
	
	while(*p++ != '\0')
		len++;
	
	return len;
}

typedef yarn<A_NameType> AeName;


#pragma mark-


static void ResizeName(string &str, unsigned int len)
{
	// only applies to pre-unicode names
#ifndef AE_UNICODE_NAMES
	if(str.size() > len)
		str.resize(len);
#endif
}


static AeName GetName(AEGP_SuiteHandler &suites, AEGP_ItemH itemH)
{
	AeName name;
	
#ifdef AE_UNICODE_NAMES
	AEGP_MemHandle nameH = NULL;
	
	suites.ItemSuite()->AEGP_GetItemName(S_mem_id, itemH, &nameH);
	
	if(nameH)
	{
		AEGP_MemSize size = 0;
		suites.MemorySuite()->AEGP_GetMemHandleSize(nameH, &size);
		
		A_NameType *nameP = NULL;
		suites.MemorySuite()->AEGP_LockMemHandle(nameH, (void **)&nameP);
		
		name = nameP;
		
		suites.MemorySuite()->AEGP_FreeMemHandle(nameH);
	}
#else
	A_NameType nameP[AEGP_MAX_ITEM_NAME_SIZE];
	
	suites.ItemSuite()->AEGP_GetItemName(itemH, nameP);
	
	name = nameP;
#endif

	return name;
}


static string GetBaseName(AEGP_SuiteHandler &suites, AEGP_ItemH itemH)
{
	string base_name = GetName(suites, itemH).str();
	
	
	size_t last_dot = base_name.find_last_of('.');
	
	if(last_dot != string::npos && last_dot > 0 && last_dot < base_name.size() - 1)
	{
		const string extension = base_name.substr(last_dot + 1);
		
		if(extension == "exr" || extension == "vri" || extension == "vrimg")
			base_name = base_name.substr(0, last_dot);
	}
	
	
	last_dot = base_name.find_last_of('.');
	
	if(last_dot != string::npos && last_dot > 0 && last_dot < base_name.size() - 2)
	{
		if(base_name[last_dot + 1] == '[')
			base_name = base_name.substr(0, last_dot);
	}

	
	return base_name;
}


static PF_TransferMode EnumToAEmode(TransferMode mode)
{
	switch(mode)
	{
		case MODE_Normal:				return PF_Xfer_IN_FRONT;
		case MODE_Dissolve:				return PF_Xfer_DISSOLVE;
		case MODE_Darken:				return PF_Xfer_DARKEN;
		case MODE_Multiply:				return PF_Xfer_MULTIPLY;
		case MODE_ColorBurn:			return PF_Xfer_COLOR_BURN2;
		case MODE_LinearBurn:			return PF_Xfer_LINEAR_BURN;
		case MODE_DarkerColor:			return PF_Xfer_DARKER_COLOR;
		case MODE_Lighten:				return PF_Xfer_LIGHTEN;
		case MODE_Screen:				return PF_Xfer_SCREEN;
		case MODE_ColorDodge:			return PF_Xfer_COLOR_DODGE2;
		case MODE_LinearDodge:			return PF_Xfer_LINEAR_DODGE;
		case MODE_LighterColor:			return PF_Xfer_LIGHTER_COLOR;
		case MODE_Overlay:				return PF_Xfer_OVERLAY;
		case MODE_SoftLight:			return PF_Xfer_SOFT_LIGHT;
		case MODE_HardLight:			return PF_Xfer_HARD_LIGHT;
		case MODE_VividLight:			return PF_Xfer_VIVID_LIGHT;
		case MODE_LinearLight:			return PF_Xfer_LINEAR_LIGHT;
		case MODE_PinLight:				return PF_Xfer_PIN_LIGHT;
		case MODE_HardMix:				return PF_Xfer_HARD_MIX;
		case MODE_Difference:			return PF_Xfer_DIFFERENCE2;
		case MODE_Exclusion:			return PF_Xfer_EXCLUSION;
		case MODE_Hue:					return PF_Xfer_HUE;
		case MODE_Saturation:			return PF_Xfer_SATURATION;
		case MODE_Color:				return PF_Xfer_COLOR;
		case MODE_Luminosity:			return PF_Xfer_LUMINOSITY;

		case MODE_DancingDissolve:		return PF_Xfer_DISSOLVE; // there is a flag to set, but...
		case MODE_ClassicColorBurn:		return PF_Xfer_COLOR_BURN;
		case MODE_Add:					return PF_Xfer_ADD;
		case MODE_ClassicColorDodge:	return PF_Xfer_COLOR_DODGE;
		case MODE_ClassicDifference:	return PF_Xfer_DIFFERENCE;
		case MODE_StencilAlpha:			return PF_Xfer_MULTIPLY_ALPHA;
		case MODE_StencilLuma:			return PF_Xfer_MULTIPLY_ALPHA_LUMA;
		case MODE_SilhouetteAlpha:		return PF_Xfer_MULTIPLY_NOT_ALPHA;
		case MODE_SilhouetteLuma:		return PF_Xfer_MULTIPLY_NOT_ALPHA_LUMA;
		case MODE_AlphaAdd:				return PF_Xfer_ALPHA_ADD;
		case MODE_LuminescentPremul:	return PF_Xfer_MULTIPLY_NOT_ALPHA_LUMA;
		
		#ifdef PF_ENABLE_PS12_MODES
		case MODE_Subtract:				return PF_Xfer_SUBTRACT;
		case MODE_Divide:				return PF_Xfer_DIVIDE;
		#endif
		
		default:						return PF_Xfer_IN_FRONT;
	}
}

template <class AttrType>
static void PrintMatrixValue(string &desc, const Attribute &attrib, int n)
{
	const AttrType &a = dynamic_cast<const AttrType &>(attrib);
	
	stringstream s;
	
	for(int r=0; r < n; r++)
	{
		if(r == 0)
			s << "[";
		else
			s << ", ";
		
		for(int c=0; c < n; c++)
		{
			if(c == 0)
				s << "{";
			else
				s << ", ";
			
			s << a.value().x[c][r];
			
			if(c == (n-1))
				s << "}";
		}
		
		if(r == (n-1))
			s << "]";
	}
	
	desc += ": " + s.str();
}

static void PrintAttributeValue(string &desc, const Attribute &attrib)
{
	const string &typeName = attrib.typeName();

	// for the attributes we know, cat the value onto desc
	// using dynamic_cast - don't forget RTTI (actually OpenEXR doesn't work at all without RTTI)
	if(typeName == "string")
	{
		const StringAttribute &a = dynamic_cast<const StringAttribute &>(attrib);
		string val = a.value();
		
		if(val.size() > 128)
			val = val.substr(0, 128) + "...";

		desc += ": \"" + val + "\"";
	}
	else if(typeName == "float")
	{
		const FloatAttribute &a = dynamic_cast<const FloatAttribute &>(attrib);
		
		char num_str[64];
		sprintf(num_str, "%f", a.value() );
		
		desc += ": " + string(num_str);
	}
	else if(typeName == "double")
	{
		const DoubleAttribute &a = dynamic_cast<const DoubleAttribute &>(attrib);
		
		char num_str[64];
		sprintf(num_str, "%f", a.value() );
		
		desc += ": " + string(num_str);
	}
	else if(typeName == "int")
	{
		const IntAttribute &a = dynamic_cast<const IntAttribute &>(attrib);
		
		char num_str[64];
		sprintf(num_str, "%d", a.value() );
		
		desc += ": " + string(num_str);
	}
	else if(typeName == "rational")
	{
		const RationalAttribute &a = dynamic_cast<const RationalAttribute &>(attrib);
		
		char num_str[128];
		sprintf(num_str, "%d / %d", a.value().n, a.value().d );
		
		desc += ": " + string(num_str);
	}
	else if(typeName == "box2i")
	{
		const Box2iAttribute &a = dynamic_cast<const Box2iAttribute &>(attrib);
		
		char num_str[128];
		sprintf(num_str, "[%d, %d, %d, %d]", a.value().min.x, a.value().min.y, a.value().max.x, a.value().max.y );
		
		desc += ": " + string(num_str);
	}
	else if(typeName == "box2f")
	{
		const Box2fAttribute &a = dynamic_cast<const Box2fAttribute &>(attrib);
		
		char num_str[128];
		sprintf(num_str, "[%f, %f, %f, %f]", a.value().min.x, a.value().min.y, a.value().max.x, a.value().max.y );
		
		desc += ": " + string(num_str);
	}
	else if(typeName == "v2i")
	{
		const V2iAttribute &a = dynamic_cast<const V2iAttribute &>(attrib);
		
		char num_str[64];
		sprintf(num_str, "[%d, %d]", a.value().x, a.value().y );
		
		desc += ": " + string(num_str);
	}
	else if(typeName == "v2f")
	{
		const V2fAttribute &a = dynamic_cast<const V2fAttribute &>(attrib);
		
		char num_str[64];
		sprintf(num_str, "[%f, %f]", a.value().x, a.value().y );
		
		desc += ": " + string(num_str);
	}
	else if(typeName == "v2d")
	{
		const V2dAttribute &a = dynamic_cast<const V2dAttribute &>(attrib);
		
		stringstream s;
		s << "[" << a.value().x << ", " << a.value().y << "]";
		
		desc += ": " + s.str();
	}
	else if(typeName == "v3i")
	{
		const V3iAttribute &a = dynamic_cast<const V3iAttribute &>(attrib);
		
		char num_str[128];
		sprintf(num_str, "[%d, %d, %d]", a.value().x, a.value().y, a.value().z );
		
		desc += ": " + string(num_str);
	}
	else if(typeName == "v3f")
	{
		const V3fAttribute &a = dynamic_cast<const V3fAttribute &>(attrib);
		
		char num_str[128];
		sprintf(num_str, "[%f, %f, %f]", a.value().x, a.value().y, a.value().z );
		
		desc += ": " + string(num_str);
	}
	else if(typeName == "v3d")
	{
		const V3dAttribute &a = dynamic_cast<const V3dAttribute &>(attrib);
		
		stringstream s;
		s << "[" << a.value().x << ", " << a.value().y << ", " <<  a.value().z << "]";
		
		desc += ": " + s.str();
	}
	else if(typeName == "m33f")
	{
		PrintMatrixValue<M33fAttribute>(desc, attrib, 3);
	}
	else if(typeName == "m33d")
	{
		PrintMatrixValue<M33dAttribute>(desc, attrib, 3);
	}
	else if(typeName == "m44f")
	{
		PrintMatrixValue<M44fAttribute>(desc, attrib, 4);
	}
	else if(typeName == "m44d")
	{
		PrintMatrixValue<M44dAttribute>(desc, attrib, 4);
	}
	else if(typeName == "compression")
	{
		const CompressionAttribute &a = dynamic_cast<const CompressionAttribute &>(attrib);
		
		desc += ": ";
		
		switch( a.value() )
		{
			case NO_COMPRESSION:		desc += "None";			break;
			case RLE_COMPRESSION:		desc += "RLE";			break;
			case ZIPS_COMPRESSION:		desc += "Zip";			break;
			case ZIP_COMPRESSION:		desc += "Zip16";		break;
			case PIZ_COMPRESSION:		desc += "Piz";			break;
			case PXR24_COMPRESSION:		desc += "PXR24";		break;
			case B44_COMPRESSION:		desc += "B44";			break;
			case B44A_COMPRESSION:		desc += "B44A";			break;
			case DWAA_COMPRESSION:		desc += "DWAA";			break;
			case DWAB_COMPRESSION:		desc += "DWAB";			break;
		}
	}
	else if(typeName == "lineOrder")
	{
		const LineOrderAttribute &a = dynamic_cast<const LineOrderAttribute &>(attrib);
		
		desc += ": ";
		
		switch( a.value() )
		{
			case INCREASING_Y:		desc += "Increasing Y";			break;
			case DECREASING_Y:		desc += "Decreasing Y";			break;
			case RANDOM_Y:			desc += "Random Y";				break;
		}
	}
	else if(typeName == "chromaticities")
	{
		const ChromaticitiesAttribute &a = dynamic_cast<const ChromaticitiesAttribute &>(attrib);
		
		char num_str[256];
		sprintf(num_str, "r(%f, %f) g(%f, %f) b(%f, %f) w(%f, %f)",
					a.value().red.x, a.value().red.y, a.value().green.x, a.value().green.y,
					a.value().blue.x, a.value().blue.y, a.value().white.x, a.value().white.y );
		
		desc += ": " + string(num_str);
	}
	else if(typeName == "tiledesc")
	{
		const TileDescriptionAttribute &a = dynamic_cast<const TileDescriptionAttribute &>(attrib);
		
		char num_str[64];
		sprintf(num_str, "[%d, %d]", a.value().xSize, a.value().ySize);
		
		desc += ": " + string(num_str);
		
		if(a.value().mode == MIPMAP_LEVELS)
			desc += ", MipMap";
		else if(a.value().mode == RIPMAP_LEVELS)
			desc += ", RipMap";
	}
	else if(typeName == "envmap")
	{
		const EnvmapAttribute &a = dynamic_cast<const EnvmapAttribute &>(attrib);
		
		if(a.value() == ENVMAP_LATLONG)
			desc += ": Lat-Long";
		else if(a.value() == ENVMAP_CUBE)
			desc += ": Cube";
	}
	else if(typeName == "stringvector")
	{
		const StringVectorAttribute &a = dynamic_cast<const StringVectorAttribute &>(attrib);
		
		string val;

		val += ": [";
		
		bool first_one = true;

		for(StringVector::const_iterator i = a.value().begin(); i != a.value().end(); ++i)
		{
			if(first_one)
				first_one = false;
			else
				val += ", ";
				
			val += "\"" + *i + "\"";
		}

		val += "]";

		desc += val;
	}
	else if(typeName == "floatvector")
	{
		const FloatVectorAttribute &a = dynamic_cast<const FloatVectorAttribute &>(attrib);
		
		stringstream s;

		s << ": [";
		
		bool first_one = true;

		for(FloatVector::const_iterator i = a.value().begin(); i != a.value().end(); ++i)
		{
			if(first_one)
				first_one = false;
			else
				s << ", ";
				
			s << "\"" << *i << "\"";
		}

		s << "]";

		desc += s.str();
	}
	else if(typeName == "timecode")
	{
		const TimeCodeAttribute &a = dynamic_cast<const TimeCodeAttribute &>(attrib);
		
		const TimeCode &v = a.value();
		
		string sep = (v.dropFrame() ? ";" : ":");
		
		stringstream s;
		
		s << ": ";
		
		s << v.hours() << sep;
		s << setfill('0') << setw(2) << v.minutes() << sep;
		s << setfill('0') << setw(2) << v.seconds() << sep;
		s << setfill('0') << setw(2) << v.frame();
		
		if( v.colorFrame() )
			s << ", Color";
		
		if( v.fieldPhase() )
			s << ", Field";
		
		if( v.bgf0() )
			s << ", bgf0";
			
		if( v.bgf1() )
			s << ", bgf1";

		if( v.bgf2() )
			s << ", bgf2";
		
		for(int i=1; i <= 8; i++)
		{
			if(v.binaryGroup(i) != 0)
				s << ", Group " << i << ": " << v.binaryGroup(i);
		}
		
		desc += s.str();
	}
	else if(typeName == "keycode")
	{
		const KeyCodeAttribute &a = dynamic_cast<const KeyCodeAttribute &>(attrib);
		
		const KeyCode &v = a.value();
		
		stringstream s;
		
		s << ": ";
		
		s << "filmMfcCode: " << v.filmMfcCode() << " ";
		s << "filmType: " << v.filmType() << " ";
		s << "prefix: " << v.prefix() << " ";
		s << "count: " << v.count() << " ";
		s << "perfOffset: " << v.perfOffset() << " ";
		s << "perfsPerFrame: " << v.perfsPerFrame() << " ";
		s << "perfsPerCount: " << v.perfsPerCount() << " ";
		
		desc += s.str();
	}
}

static void AddDescription(string &desc, const ProEXRdoc_read &file)
{
	string newline("\r");
	
	// header
	desc += "ProEXR File Description" + newline + newline;
	
	int parts = file.parts();
	
	for(int i=0; i < parts; i++)
	{
		if(parts > 1)
		{
			if(i != 0)
				desc += newline + newline;
			
			stringstream s;
			s << "++Part " << (i + 1) << "++";
			
			desc += s.str() + newline + newline;
		}
		
		const Header &head = file.header(i);
		
		// Attribtes
		desc += "=Attributes=" + newline;
		
		for(Header::ConstIterator j = head.begin(); j != head.end(); ++j)
		{
			const Attribute &attrib = j.attribute();
			
			string attr_type(attrib.typeName());
			string attr_name(j.name());
			
			
			desc += attr_name + " (" + attr_type + ")";
			
			// print out attribute values if we know how
			PrintAttributeValue(desc, attrib);
			
			desc += newline;
		}

			
		// Channels
		desc += newline + "=Channels=" + newline;
		
		const ChannelList &channels = head.channels();

		for(ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i)
		{
			// things we'll want to know about the channels
			string channel_name(i.name());
			Imf::PixelType pix_type = i.channel().type;
			
			desc += channel_name;
			
			if(pix_type == Imf::HALF)
				desc += " (half)" + newline;
			else if(pix_type == Imf::FLOAT)
				desc += " (float)" + newline;
			else if(pix_type == Imf::UINT)
				desc += " (uint)" + newline;
		}
	}
}


static A_Boolean ItemIsEXR(AEGP_ItemH itemH, A_Boolean require_single)
{
	AEGP_SuiteHandler	suites(sP);
	
	A_Boolean isEXR = FALSE;
	
	AEGP_ItemType type;
	suites.ItemSuite()->AEGP_GetItemType(itemH, &type);
	
	if(type == AEGP_ItemType_FOOTAGE)
	{
		AEGP_ItemFlags flagsH;
		
		suites.ItemSuite()->AEGP_GetItemFlags(itemH, &flagsH);
		
		if( !(flagsH & AEGP_ItemFlag_MISSING) )
		{
			AEGP_FootageH footH;
			suites.FootageSuite()->AEGP_GetMainFootageFromItem(itemH, &footH);
			
			if(require_single)
			{
				A_long num_files, files_per_frame;
				
				suites.FootageSuite()->AEGP_GetFootageNumFiles(footH, &num_files, &files_per_frame);
				
				if(num_files > 1)
					return FALSE;
			}
			
			
			PathString path;
			string char_path;
			
		#ifdef AE_UNICODE_PATHS	
			AEGP_MemHandle u_pathH = NULL;
			A_PathType *file_pathZ = NULL;
		
			suites.FootageSuite()->AEGP_GetFootagePath(footH, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, &u_pathH);
			
			if(u_pathH)
			{
				suites.MemorySuite()->AEGP_LockMemHandle(u_pathH, (void **)&file_pathZ);
				
				path = file_pathZ;
				char_path = UTF16toUTF8(file_pathZ);
				
				suites.MemorySuite()->AEGP_FreeMemHandle(u_pathH);
			}
		#else
			A_char pathZ[AEGP_MAX_PATH_SIZE+1];
			suites.FootageSuite()->AEGP_GetFootagePath(footH, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, pathZ);

		#ifdef AE_HFS_PATHS
			if(pathZ[0] != '\0')
				ConvertPath(pathZ, pathZ, AEGP_MAX_PATH_SIZE);
		#endif
		
			path = pathZ;
			char_path = pathZ;
		#endif
			
			if(char_path.length() > 0)
			{
				string the_extension = char_path.substr( char_path.size() - 3, 3 );
				
				if(the_extension == "exr" || the_extension == "EXR")
				{
					try{
					
					// got this off the OpenEXR site
					IStreamPlatform f(path.string());

					char bytes[4];
					f.read(bytes, sizeof(bytes));

					// here's the check
					if( isImfMagic(bytes) )
					{
						isEXR = TRUE;
					}
					
					}catch(...) {}
				}
			}
		}
	}
	
	return isEXR;
}


static A_Boolean ItemIsVRimg(AEGP_ItemH itemH, A_Boolean require_single)
{
	AEGP_SuiteHandler	suites(sP);
	
	A_Boolean isVRimg = FALSE;
	
	AEGP_ItemType type;
	suites.ItemSuite()->AEGP_GetItemType(itemH, &type);
	
	if(type == AEGP_ItemType_FOOTAGE)
	{
		AEGP_ItemFlags flagsH;
		
		suites.ItemSuite()->AEGP_GetItemFlags(itemH, &flagsH);
		
		if( !(flagsH & AEGP_ItemFlag_MISSING) )
		{
			AEGP_FootageH footH;
			suites.FootageSuite()->AEGP_GetMainFootageFromItem(itemH, &footH);
			
			if(require_single)
			{
				A_long num_files, files_per_frame;
				
				suites.FootageSuite()->AEGP_GetFootageNumFiles(footH, &num_files, &files_per_frame);
				
				if(num_files > 1)
					return FALSE;
			}
			
			
			PathString path;
			string char_path;
			
		#ifdef AE_UNICODE_PATHS	
			AEGP_MemHandle u_pathH = NULL;
			A_PathType *file_pathZ = NULL;
		
			suites.FootageSuite()->AEGP_GetFootagePath(footH, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, &u_pathH);
			
			if(u_pathH)
			{
				suites.MemorySuite()->AEGP_LockMemHandle(u_pathH, (void **)&file_pathZ);
				
				path = file_pathZ;
				char_path = UTF16toUTF8(file_pathZ);
				
				suites.MemorySuite()->AEGP_FreeMemHandle(u_pathH);
			}
		#else
			A_char pathZ[AEGP_MAX_PATH_SIZE+1];
			suites.FootageSuite()->AEGP_GetFootagePath(footH, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, pathZ);

		#ifdef AE_HFS_PATHS
			if(pathZ[0] != '\0')
				ConvertPath(pathZ, pathZ, AEGP_MAX_PATH_SIZE);
		#endif
		
			path = pathZ;
			char_path = pathZ;
		#endif
			
			if(char_path.length() > 0)
			{
				string the_extension = char_path.substr( char_path.size() - 3, 3 );
				
				if(the_extension == "vri" || the_extension == "VRI" ||
					the_extension == "img" || the_extension == "IMG")
				{
					try{
					
					// got this off the OpenEXR site
					IStreamPlatform f(path.string());

					char bytes[4];
					f.read(bytes, sizeof(bytes));

					// here's the check
					if( VRimg::isVRimgMagic(bytes) )
					{
						isVRimg = TRUE;
					}
					
					}catch(...) {}
				}
			}
		}
	}
	
	return isVRimg;
}


static A_Err NewCompFromFootageItem(AEGP_ItemH itemH, AEGP_LayerH *layerH)
{
	A_Err			err 		= A_Err_NONE;
	
	AEGP_SuiteHandler	suites(sP);
	

	AEGP_ItemType type;
	AEGP_ItemFlags flagsH;

	suites.ItemSuite()->AEGP_GetItemType(itemH, &type);
	suites.ItemSuite()->AEGP_GetItemFlags(itemH, &flagsH);

	if(type == AEGP_ItemType_FOOTAGE && !(flagsH & AEGP_ItemFlag_MISSING) )
	{
		// parent folder
		AEGP_ItemH parent_folderH;
		suites.ItemSuite()->AEGP_GetItemParentFolder(itemH, &parent_folderH);
		
		// put the item in a new folder
		AeName name = GetBaseName(suites, itemH);
		
		AEGP_ItemH exr_folderH;
		suites.ItemSuite()->AEGP_CreateNewFolder(name, parent_folderH, &exr_folderH );
		
		// move footage in
		suites.ItemSuite()->AEGP_SetItemParentFolder(itemH, exr_folderH);
		
		
		// everything you need to know about the footage
		A_long width, height;
		A_Ratio pixel_aspect;
		A_Time duration;
		AEGP_FootageInterp interp;
		A_Ratio framerate;
		suites.ItemSuite()->AEGP_GetItemDimensions(itemH, &width, &height);
		suites.ItemSuite()->AEGP_GetItemPixelAspectRatio(itemH, &pixel_aspect);
		suites.ItemSuite()->AEGP_GetItemDuration(itemH, &duration);
		suites.FootageSuite()->AEGP_GetFootageInterpretation(itemH, FALSE, &interp);
		
		if(interp.native_fpsF < 0.1 || duration.value == 0)
		{
			// get the sequence frame rate from preferences
			AEGP_PersistentBlobH blobH = NULL;
			A_FpLong framerate = 24.0;
			
			suites.PersistentDataSuite()->AEGP_GetApplicationBlob(&blobH);
			suites.PersistentDataSuite()->AEGP_GetFpLong(blobH, "General Section", "Default Import Sequence FPS", 24.0, &framerate);

			interp.native_fpsF = framerate;
		}
			
		framerate.den = 1000; framerate.num = (1000.0 * interp.native_fpsF) + 0.5;
		
		if(duration.value == 0)
			duration.value = 10 * duration.scale;
		
		// assemble comp
		string master_comp_name = name;
		
		ResizeName(master_comp_name, (AEGP_MAX_ITEM_NAME_SIZE-10));
		
		master_comp_name += " assemble";
		
		AeName master_comp_nameAE = master_comp_name;

		AEGP_CompH master_compH;
		suites.CompSuite()->AEGP_CreateComp(exr_folderH, master_comp_nameAE, width, height,
												&pixel_aspect, &duration, &framerate, &master_compH);

		// add footage to source comp
		suites.LayerSuite()->AEGP_AddLayer(itemH, master_compH, layerH);
	}
	
	return err;
}


static A_long CalculateStartFrame(const TimeCode &time_code, int time_base)
{
	int dropped_frames = 0;
	
	if( time_code.dropFrame() )
	{
		int minutes = (60 * time_code.hours()) + time_code.minutes();
		
		int m = 0;
		
		while(m <= minutes)
		{
			if( m % 10 != 0 )
			{
				dropped_frames += (time_base == 60 ? 4 : 2);
			}
			
			m++;
		}
	}
	
	return ((60 * 60 * time_base * time_code.hours()) +
			(60 * time_base * time_code.minutes()) +
			(time_base * time_code.seconds()) +
			time_code.frame() -
			dropped_frames );
}


static A_Err NewCompFromExrFootageItem(AEGP_ItemH itemH, AEGP_LayerH *layerH)
{
	A_Err			err 		= A_Err_NONE;
	
	AEGP_SuiteHandler	suites(sP);
	
	try{
	
	// let's check that EXR
	// we need the path so we can read the file, duh
	AEGP_FootageH footH;
	suites.FootageSuite()->AEGP_GetMainFootageFromItem(itemH, &footH);
	
#ifdef AE_UNICODE_PATHS
	AEGP_MemHandle u_pathH = NULL;
	A_PathType *path = NULL;

	suites.FootageSuite()->AEGP_GetFootagePath(footH, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, &u_pathH);
	
	if(u_pathH)
	{
		suites.MemorySuite()->AEGP_LockMemHandle(u_pathH, (void **)&path);
	}
	else
		return AEIO_Err_BAD_FILENAME; 
#else
	A_char path[AEGP_MAX_PATH_SIZE+1];
	
	suites.FootageSuite()->AEGP_GetFootagePath(footH, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, path);
#endif
	
#ifdef AE_HFS_PATHS
	ConvertPath(path, path, AEGP_MAX_PATH_SIZE);
#endif

	IStreamPlatform instream(path);
	HybridInputFile in(instream);
	
#ifdef AE_UNICODE_PATHS
	suites.MemorySuite()->AEGP_FreeMemHandle(u_pathH);
#endif

	// if we have an embedded frame rate, make sure it gets used
	if( hasFramesPerSecond( in.header(0) ) )
	{
		const Rational &fps = framesPerSecond( in.header(0) );
		
		AEGP_FootageInterp interp;
		suites.FootageSuite()->AEGP_GetFootageInterpretation(itemH, FALSE, &interp);
		
		if(interp.native_fpsF != fps)
		{
			interp.native_fpsF = fps;
			
			suites.FootageSuite()->AEGP_SetFootageInterpretation(itemH, FALSE, &interp);
		}
	}
	

	// create an assemble comp the regular way
	err = NewCompFromFootageItem(itemH, layerH);
	
	
	if(!err && *layerH)
	{
		A_long exr_width, exr_height;
		suites.ItemSuite()->AEGP_GetItemDimensions(itemH, &exr_width, &exr_height);
		
		// get the comp that was just created (from the layer handle we have)
		AEGP_CompH assemble_compH;
		AEGP_ItemH assemble_comp_itemH;
		suites.LayerSuite()->AEGP_GetLayerParentComp(*layerH, &assemble_compH);
		suites.CompSuite()->AEGP_GetItemFromComp(assemble_compH, &assemble_comp_itemH);
		
		A_Time frame_dur;
		suites.CompSuite()->AEGP_GetCompFrameDuration(assemble_compH, &frame_dur);
		
		// frame_rate = 1 / frame_dur
		A_Ratio frame_rate;
		frame_rate.num = frame_dur.scale;
		frame_rate.den = frame_dur.value;
		
		int time_base = ((double)frame_rate.num / (double)frame_rate.den) + 0.5;

		A_Time start_time;
		suites.CompSuite()->AEGP_GetCompDisplayStartTime(assemble_compH, &start_time);
		
		// if we have timcode, use it to set the start frame
		if( hasTimeCode( in.header(0) ) )
		{
			const TimeCode &time_code = timeCode( in.header(0) );
			
			// AE 10.5 (CS5.5) timecode stuff
		#ifdef AE105_TIMECODE_SUITES
			AEGP_CompSuite8 *cs8P = NULL;
			
			try{
				cs8P = suites.CompSuite8();
			}catch(...) {}
			
			if(cs8P)
			{
				cs8P->AEGP_SetCompDisplayDropFrame(assemble_compH, time_code.dropFrame());
			}
			else
			{
		#endif
				// AE CS5 and earlier set their timecode base project-wide
				AEGP_ProjectH projH = NULL;
				suites.ProjSuite()->AEGP_GetProjectByIndex(0, &projH);
				
				AEGP_TimeDisplay2 time_dis;
				suites.ProjSuite()->AEGP_GetProjectTimeDisplay(projH, &time_dis);
				
				time_dis.time_display_type = AEGP_TimeDisplayType_TIMECODE;
				time_dis.timebaseC = time_base;
				time_dis.non_drop_30B = !time_code.dropFrame();
				//time_dis.starting_frameL = 0;
				//time_dis.auto_timecode_baseB = TRUE;
				
				suites.ProjSuite()->AEGP_SetProjectTimeDisplay(projH, &time_dis);
		#ifdef AE105_TIMECODE_SUITES
			}
		#endif
		
			start_time.value = CalculateStartFrame(time_code, time_base) * frame_dur.value;
			start_time.scale = frame_dur.scale;
			
			suites.CompSuite()->AEGP_SetCompDisplayStartTime(assemble_compH, &start_time);
		}
		
		// set comp size from the display window
		const Box2i &data_window = in.dataWindow();
		const Box2i &display_window = in.displayWindow();
		
		const int data_width = (data_window.max.x - data_window.min.x) + 1;
		const int data_height = (data_window.max.y - data_window.min.y) + 1;
		
		// this assumes that dataWindow and displayWindow will be different sizes, which might not be true
		const bool using_dataWindow = (exr_width == data_width) && (exr_height == data_height);
		
		// do we need to make a display comp?
		if((data_window != display_window) && using_dataWindow)
		{
			AEGP_ItemH parent_folderH;
			suites.ItemSuite()->AEGP_GetItemParentFolder(assemble_comp_itemH, &parent_folderH);
			
			// get everything but dimensions from AE
			A_Ratio pixel_aspect;
			A_Time duration;
			//A_FpLong framerate;
			//A_Ratio framerateRatio;
			suites.ItemSuite()->AEGP_GetItemPixelAspectRatio(assemble_comp_itemH, &pixel_aspect);
			suites.ItemSuite()->AEGP_GetItemDuration(assemble_comp_itemH, &duration);
			//suites.CompSuite()->AEGP_GetCompFramerate(assemble_compH, &framerate);
			
			//framerateRatio.den = 1000; framerateRatio.num = (framerateRatio.den * framerate) + 0.5;
			
			// displayWindow comp size from EXR
			A_long comp_width = (display_window.max.x - display_window.min.x) + 1;
			A_long comp_height = (display_window.max.y - display_window.min.y) + 1;
			
			
			// display comp
			AeName name = GetName(suites, assemble_comp_itemH);
			
			string display_comp_name = name;
			display_comp_name.resize( display_comp_name.size() - 9 ); // I know there's an " assemble" at the end
			
			display_comp_name += " display";
			
			AeName display_comp_nameAE = display_comp_name;

			AEGP_CompH display_compH;
			suites.CompSuite()->AEGP_CreateComp(parent_folderH, display_comp_nameAE, comp_width, comp_height,
													&pixel_aspect, &duration, &frame_rate, &display_compH);

			suites.CompSuite()->AEGP_SetCompDisplayStartTime(assemble_compH, &start_time);
			
			// add assemble comp to display comp
			AEGP_LayerH assemble_layerH;
			suites.LayerSuite()->AEGP_AddLayer(assemble_comp_itemH, display_compH, &assemble_layerH);

			// adjust the position based on the display window
			AEGP_StreamRefH position_stream;
			AEGP_StreamValue2 position_value;
			A_Time time_begin = {0, 1};
			
			suites.StreamSuite()->AEGP_GetNewLayerStream(S_mem_id, assemble_layerH, AEGP_LayerStream_POSITION, &position_stream);
			suites.StreamSuite()->AEGP_GetNewStreamValue(S_mem_id, position_stream, AEGP_LTimeMode_LayerTime, &time_begin, TRUE, &position_value);
			
			// to start, the middle of data_window will be in the middle of display_window
			// so lets offset it
			V2d data_center, display_center;
			
			data_center.x = ((double)data_window.min.x + (double)data_window.max.x + 1.0) / 2.0;
			data_center.y = ((double)data_window.min.y + (double)data_window.max.y + 1.0) / 2.0;
			
			display_center.x = ((double)display_window.min.x + (double)display_window.max.x + 1.0) / 2.0;
			display_center.y = ((double)display_window.min.y + (double)display_window.max.y + 1.0) / 2.0;
			
			
			position_value.val.two_d.x += data_center.x - display_center.x;
			position_value.val.two_d.y += data_center.y - display_center.y;
			
			suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, position_stream, &position_value);
			suites.StreamSuite()->AEGP_DisposeStream(position_stream);
		}
	}
	
	}catch(...) {}
	
	return err;
}


static A_Err BuildEXRcompsFromLayer(AEGP_LayerH layerH)
{
	A_Err			err 		= A_Err_NONE;
	
	AEGP_SuiteHandler	suites(sP);

	try{
	

	AEGP_ItemH layer_itemH = NULL;
	suites.LayerSuite()->AEGP_GetLayerSourceItem(layerH, &layer_itemH);
	
	if(layer_itemH && ItemIsEXR(layer_itemH, FALSE) )
	{
		// basic stuff we'll be needing
		AEGP_CompH master_compH;
		AEGP_ItemH master_comp_itemH;
		suites.LayerSuite()->AEGP_GetLayerParentComp(layerH, &master_compH);
		suites.CompSuite()->AEGP_GetItemFromComp(master_compH, &master_comp_itemH); 

		AEGP_ItemH parent_folderH;
		suites.ItemSuite()->AEGP_GetItemParentFolder(master_comp_itemH, &parent_folderH);
		
		
		// we need the path so we can read the file, duh
		AEGP_FootageH footH;
		suites.FootageSuite()->AEGP_GetMainFootageFromItem(layer_itemH, &footH);
		
		//A_char path[AEGP_MAX_PATH_SIZE+1];
	
	#ifdef AE_UNICODE_PATHS	
		AEGP_MemHandle u_pathH = NULL;
		A_PathType *file_pathZ = NULL;
		suites.FootageSuite()->AEGP_GetFootagePath(footH, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, &u_pathH);
		
		if(u_pathH)
		{
			suites.MemorySuite()->AEGP_LockMemHandle(u_pathH, (void **)&file_pathZ);
			
			//string char_path = UTF16toUTF8(file_pathZ);
			
			//strncpy(path, char_path.c_str(), AEGP_MAX_PATH_SIZE);
		}
	#else
		A_char path[AEGP_MAX_PATH_SIZE+1];
		
		suites.FootageSuite()->AEGP_GetFootagePath(footH, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, path);
		
		A_PathType *file_pathZ = path;
	#endif
		
	#ifdef AE_HFS_PATHS
		ConvertPath(path, path, AEGP_MAX_PATH_SIZE);
	#endif

	#ifdef WIN_ENV
		#define PATH_DELIMITER	'\\'
	#else
		#define PATH_DELIMITER	'/'
	#endif

		const string filename = GetBaseName(suites, layer_itemH);
		

		// get the path for channel mapping
		//string chan_map_path(S_path);
		//chan_map_path.resize( chan_map_path.find_last_of( PATH_DELIMITER ) + 1 );
		//chan_map_path += "OpenEXR_channel_map.txt"; // the file we're looking for (but not using at all right now)
		

		// get info from the EXR
		IStreamPlatform in_stream(file_pathZ);
		
		ProEXRdoc_readPS in_file(in_stream, NULL, true, true, true, false, true);

		
		// create a folder to hold source comps
		AEGP_ItemH comps_folderH;
		
		string comps_folder_name = filename;

		ResizeName(comps_folder_name, (AEGP_MAX_ITEM_NAME_SIZE-14));
			
		comps_folder_name += " source comps";
		
		AeName comps_folder_nameAE = comps_folder_name;
		
		suites.ItemSuite()->AEGP_CreateNewFolder(comps_folder_nameAE, parent_folderH, &comps_folderH );
		
		
		// everything you need to know about the master comp
		A_long width, height;
		A_Ratio pixel_aspect;
		A_Time duration;
		//A_FpLong framerate;
		A_Ratio framerateRatio;
		A_Time frame_dur;
		A_Time start_time;
		
		suites.ItemSuite()->AEGP_GetItemDimensions(master_comp_itemH, &width, &height);
		suites.ItemSuite()->AEGP_GetItemPixelAspectRatio(master_comp_itemH, &pixel_aspect);
		suites.ItemSuite()->AEGP_GetItemDuration(master_comp_itemH, &duration);
		//suites.CompSuite()->AEGP_GetCompFramerate(master_compH, &framerate);
		suites.CompSuite()->AEGP_GetCompFrameDuration(master_compH, &frame_dur);
		suites.CompSuite()->AEGP_GetCompDisplayStartTime(master_compH, &start_time);
		
		//framerateRatio.den = 1000; framerateRatio.num = (framerateRatio.den * framerate) + 0.5;
		framerateRatio.num = frame_dur.scale;
		framerateRatio.den = frame_dur.value;
		
		
	#ifdef AE105_TIMECODE_SUITES
		AEGP_CompSuite8 *cs8P = NULL;
		A_Boolean drop_frame = FALSE;
		
		try{
			cs8P = suites.CompSuite8();
		}catch(...) {}
		
		if(cs8P)
		{
			cs8P->AEGP_GetCompDisplayDropFrame(master_compH, &drop_frame);
		}
	#endif
			
		// everything you need to know about the source layer
		AEGP_StreamRefH stream_refH; // this is used later on down
		AEGP_StreamRefH anchor_stream, position_stream, rotation_stream, scale_stream;
		AEGP_StreamValue2 anchor_value, position_value, rotation_value, scale_value;
		AEGP_MemHandle anchor_expH, position_expH, rotation_expH, scale_expH; // expression handles
		A_Boolean anchor_exp_enabled, position_exp_enabled, rotaiton_exp_enabled, scale_exp_enabled;
		A_Time time_begin = {0, 1};

	#define GET_LAYER_STREAM_VALUE(LAYER, PARAM, STREAM, VALP, EXPH, EXPBOOL) \
		suites.StreamSuite()->AEGP_GetNewLayerStream(S_mem_id, layerH, PARAM, &STREAM); \
		suites.StreamSuite()->AEGP_GetNewStreamValue(S_mem_id, STREAM, AEGP_LTimeMode_LayerTime, &time_begin, TRUE, VALP); \
		EXPH = NULL; EXPBOOL = TRUE; \
		suites.StreamSuite()->AEGP_GetExpression(S_mem_id, STREAM, &EXPH); \
		if(EXPH){ suites.StreamSuite()->AEGP_GetExpressionState(S_mem_id, STREAM, &EXPBOOL); }
		
		
		GET_LAYER_STREAM_VALUE(layerH, AEGP_LayerStream_ANCHORPOINT, anchor_stream, &anchor_value, anchor_expH, anchor_exp_enabled)
		GET_LAYER_STREAM_VALUE(layerH, AEGP_LayerStream_POSITION, position_stream, &position_value, position_expH, position_exp_enabled);
		GET_LAYER_STREAM_VALUE(layerH, AEGP_LayerStream_ROTATION, rotation_stream, &rotation_value, rotation_expH, rotaiton_exp_enabled);
		GET_LAYER_STREAM_VALUE(layerH, AEGP_LayerStream_SCALE, scale_stream, &scale_value, scale_expH, scale_exp_enabled);
		

		// are we building a Contact sheet?
	#define PREFS_SECTION		"ProEXR"
	#define CONTACT_SHEET_SIZE	"Contact Sheet Size"
		AEGP_PersistentBlobH blobH = NULL;
		suites.PersistentDataSuite()->AEGP_GetApplicationBlob(&blobH);
		
		A_long contact_sheet_mult = 2;
		suites.PersistentDataSuite()->AEGP_GetLong(blobH, PREFS_SECTION, CONTACT_SHEET_SIZE, contact_sheet_mult, &contact_sheet_mult);
		
		AEGP_CompH contact_sheet_compH = NULL;
		int contact_tiles_x = 1, contact_tiles_y = 1;
		
		const int total_layers = in_file.layers().size() + in_file.cryptoLayers().size();
	
		if(contact_sheet_mult > 0 && total_layers > 1)
		{
			while(contact_tiles_x * contact_tiles_y < total_layers)
			{
				if(contact_tiles_y > contact_tiles_x)
					contact_tiles_x++;
				else
					contact_tiles_y++;
			}
			
			if(contact_tiles_x < 2)
				contact_sheet_mult = 1;
			
			const int contact_width = width * contact_sheet_mult;
			const int contact_height = height * contact_sheet_mult * contact_tiles_y / contact_tiles_x;
				
			string contact_sheet_name = filename;

			ResizeName(contact_sheet_name, (AEGP_MAX_ITEM_NAME_SIZE-14));
				
			contact_sheet_name += " contact sheet";
			
			AeName comps_folder_nameAE = contact_sheet_name;
		
			suites.CompSuite()->AEGP_CreateComp(parent_folderH, comps_folder_nameAE, contact_width, contact_height,
													&pixel_aspect, &duration, &framerateRatio, &contact_sheet_compH);
		}
		
		
		
		// create source comps, add to master comp
		for(int i=0; i < total_layers; i++)
		{
			const bool cryptoLayer = (i < in_file.cryptoLayers().size());
			
			const ProEXRlayer_readPS &layer = cryptoLayer ?
												dynamic_cast<const ProEXRlayer_readPS &>( *in_file.cryptoLayers().at(i) ) :
												dynamic_cast<const ProEXRlayer_readPS &>( *in_file.layers().at(i - in_file.cryptoLayers().size()) );
			
			string source_comp_name = layer.name() + (cryptoLayer ? " Cryptomatte source" : " source");
			
			ResizeName(source_comp_name, (AEGP_MAX_ITEM_NAME_SIZE-1));

			// source comp
			AEGP_CompH source_compH;
			
			AeName source_comp_nameAE = source_comp_name;

			suites.CompSuite()->AEGP_CreateComp(comps_folderH, source_comp_nameAE, width, height,
													&pixel_aspect, &duration, &framerateRatio, &source_compH);
													
			suites.CompSuite()->AEGP_SetCompDisplayStartTime(source_compH, &start_time);
			
		#ifdef AE105_TIMECODE_SUITES
			if(cs8P)
			{
				cs8P->AEGP_SetCompDisplayDropFrame(source_compH, drop_frame);
			}
		#endif
		
			// source comp item
			AEGP_ItemH source_comp_itemH;
			suites.CompSuite()->AEGP_GetItemFromComp(source_compH, &source_comp_itemH);
			
			
			
			// add footage to source comp
			AEGP_LayerH footage_layerH;
			suites.LayerSuite()->AEGP_AddLayer(layer_itemH, source_compH, &footage_layerH);
			
			
			// copy settings from original layer
	#define SET_LAYER_STREAM_VAL(LAYER, PARAM, VALP, EXPH, EXPBOOL) \
			suites.StreamSuite()->AEGP_GetNewLayerStream(S_mem_id, LAYER, PARAM, &stream_refH); \
			suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, stream_refH, VALP); \
			if(EXPH) \
			{ \
				A_char *exp = NULL; \
				suites.MemorySuite()->AEGP_LockMemHandle(EXPH, (void**)&exp); \
				suites.StreamSuite()->AEGP_SetExpression(S_mem_id, stream_refH, exp); \
				suites.StreamSuite()->AEGP_SetExpressionState(S_mem_id, stream_refH, EXPBOOL); \
				/*suites.MemorySuite()->AEGP_FreeMemHandle(EXPH); apparently this is too soon */ \
			} \
			suites.StreamSuite()->AEGP_DisposeStream(stream_refH);
			
			
			SET_LAYER_STREAM_VAL(footage_layerH, AEGP_LayerStream_ANCHORPOINT, &anchor_value, anchor_expH, anchor_exp_enabled);
			SET_LAYER_STREAM_VAL(footage_layerH, AEGP_LayerStream_POSITION, &position_value, position_expH, position_exp_enabled);
			SET_LAYER_STREAM_VAL(footage_layerH, AEGP_LayerStream_ROTATION, &rotation_value, rotation_expH, rotaiton_exp_enabled);
			SET_LAYER_STREAM_VAL(footage_layerH, AEGP_LayerStream_SCALE, &scale_value, scale_expH, scale_exp_enabled);
			

			// apply EXtractoR or IDentifier (unless the layer is just "RGB" or "Y[RY][BY]")
			if( !(layer.channels().size() >= 3 &&
				((layer.channels().at(0)->name() == "R" && layer.channels().at(1)->name() == "G" && layer.channels().at(2)->name() == "B") ||
				(layer.channels().at(0)->name() == "Y" && layer.channels().at(1)->name() == "RY" && layer.channels().at(2)->name() == "BY")) ) )
			{
				// add effect to layer
				if(cryptoLayer)
				{
					if(gCryptomatte_key)
					{
						std::string manifest = layer.manifest();
						
						if(manifest.empty() && !layer.manif_file().empty())
						{
						#ifdef AE_UNICODE_PATHS	
							std::string manifest_path = UTF16toUTF8(file_pathZ);
						#else
							std::string manifest_path = file_pathZ;
						#endif
							manifest_path.resize( manifest_path.find_last_of( PATH_DELIMITER ) + 1 );
							manifest_path += layer.manif_file();
							
							std::ifstream f(manifest_path.c_str());
							
							f.seekg(0, std::ios::end);
							manifest.reserve(f.tellg());
							f.seekg(0, std::ios::beg);
							
							manifest.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
						}
						
						A_Handle handle = MakeCryptomatteHandle(suites, layer.name(), manifest);
						
						AEGP_EffectRefH new_effectH;
						AEGP_StreamRefH arb_stream;
						AEGP_StreamValue2 val;
						
						val.val.arbH = handle;
						
						suites.EffectSuite()->AEGP_ApplyEffect(S_mem_id, footage_layerH, gCryptomatte_key, &new_effectH);
						
						
						suites.StreamSuite()->AEGP_GetNewEffectStreamByIndex(S_mem_id,
											new_effectH, CRYPTOMATTE_ARB_INDEX, &arb_stream);
						
						suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, arb_stream, &val);
						
						//suites.StreamSuite()->AEGP_DisposeStreamValue(&val);
						
						suites.StreamSuite()->AEGP_DisposeStream(arb_stream);
						
						suites.EffectSuite()->AEGP_DisposeEffect(new_effectH);
					}
				}
				else if(layer.channels().at(0)->pixelType() == Imf::UINT && gIDentifier_key)
				{
					// set up the parameter data
					IDentifierArbitraryData arb;
					
					arb.version = 1;
					arb.index = 0;
					strncpy(arb.name, layer.channels().at(0)->name().c_str(), MAX_CHANNEL_NAME_LEN);
					
					// copy to a handle
					A_Handle handle = (A_Handle)suites.HandleSuite()->host_new_handle( sizeof(IDentifierArbitraryData) );
					void *ptr = suites.HandleSuite()->host_lock_handle((PF_Handle)handle);
					
					memcpy(ptr, &arb, sizeof(IDentifierArbitraryData) );
					
					suites.HandleSuite()->host_unlock_handle((PF_Handle)handle);
					
					
					// apply effect, assign handle
					AEGP_EffectRefH new_effectH;
					AEGP_StreamRefH arb_stream;
					AEGP_StreamValue2 val;
					
					val.val.arbH = handle;
					
					suites.EffectSuite()->AEGP_ApplyEffect(S_mem_id, footage_layerH, gIDentifier_key, &new_effectH);
					
					
					suites.StreamSuite()->AEGP_GetNewEffectStreamByIndex(S_mem_id,
										new_effectH, IDENTIFIER_ARB_INDEX, &arb_stream);
					
					suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, arb_stream, &val);
					
					//suites.StreamSuite()->AEGP_DisposeStreamValue(&val);
					
					suites.StreamSuite()->AEGP_DisposeStream(arb_stream);
					
					suites.EffectSuite()->AEGP_DisposeEffect(new_effectH);
				}
				else if(layer.channels().at(0)->pixelType() != Imf::UINT && gEXtractoR_key)
				{
					// set up the parameter data
					EXtractoRArbitraryData arb;
					ChannelData *c_ptr[] = { &arb.red, &arb.green, &arb.blue, &arb.alpha };
					
					arb.version = 1;
					arb.red.action = arb.green.action = arb.blue.action = arb.alpha.action = DO_COPY;
					arb.red.index = arb.green.index = arb.blue.index = arb.alpha.index = 0;
					
					int nonAlphaChannels = layer.channels().size() - (layer.alphaChannel() ? 1 : 0);
					
					int c = 0;
						
					if( nonAlphaChannels == 1 )
					{
						string channel = layer.channels().at(c++)->name();
						
						strncpy(c_ptr[0]->name, channel.c_str(), MAX_CHANNEL_NAME_LEN);	c_ptr[0]->action = DO_EXTRACT;
						strncpy(c_ptr[1]->name, channel.c_str(), MAX_CHANNEL_NAME_LEN);	c_ptr[1]->action = DO_EXTRACT;
						strncpy(c_ptr[2]->name, channel.c_str(), MAX_CHANNEL_NAME_LEN);	c_ptr[2]->action = DO_EXTRACT;
					}
					else
					{
						// get the non-alphas
						while(c < nonAlphaChannels && c < 4)
						{
							strncpy(c_ptr[c]->name, layer.channels().at(c)->name().c_str(), MAX_CHANNEL_NAME_LEN);
							
							c_ptr[c]->action = DO_EXTRACT;
							
							c++;
						}
					}
						
					// then get the alpha (so that layer.A always goes in alpha)
					if(c < 4 && layer.alphaChannel() )
					{
						strncpy(c_ptr[3]->name, layer.alphaChannel()->name().c_str(), MAX_CHANNEL_NAME_LEN);
						
						c_ptr[3]->action = DO_EXTRACT;
						
						c++;
					}
					
					
					// copy to a handle
					A_Handle handle = (A_Handle)suites.HandleSuite()->host_new_handle( sizeof(EXtractoRArbitraryData) );
					void *ptr = suites.HandleSuite()->host_lock_handle((PF_Handle)handle);
					
					memcpy(ptr, &arb, sizeof(EXtractoRArbitraryData) );
					
					suites.HandleSuite()->host_unlock_handle((PF_Handle)handle);
					
					
					// apply effect, assign handle
					AEGP_EffectRefH new_effectH;
					AEGP_StreamRefH arb_stream;
					AEGP_StreamValue2 val;
					
					val.val.arbH = handle;
					
					suites.EffectSuite()->AEGP_ApplyEffect(S_mem_id, footage_layerH, gEXtractoR_key, &new_effectH);
					
					
					suites.StreamSuite()->AEGP_GetNewEffectStreamByIndex(S_mem_id,
										new_effectH, EXTRACTOR_ARB_INDEX, &arb_stream);
					
					suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, arb_stream, &val);
					
					// UnMult
					if(nonAlphaChannels >= 3 &&
						layer.channels().at(0)->channelTag() == CHAN_R &&
						layer.channels().at(1)->channelTag() == CHAN_G &&
						layer.channels().at(2)->channelTag() == CHAN_B )
					{
						AEGP_StreamRefH unmult_stream;
						AEGP_StreamValue2 u_val;
						
						u_val.val.one_d = 1.0;

						suites.StreamSuite()->AEGP_GetNewEffectStreamByIndex(S_mem_id,
											new_effectH, EXTRACTOR_UNMULT_INDEX, &unmult_stream);
						
						suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, unmult_stream, &u_val);
					
						suites.StreamSuite()->AEGP_DisposeStream(unmult_stream);
					}
					
					//suites.StreamSuite()->AEGP_DisposeStreamValue(&val);
					
					suites.StreamSuite()->AEGP_DisposeStream(arb_stream);
					
					suites.EffectSuite()->AEGP_DisposeEffect(new_effectH);
				}
			}

								
			// add source comp to master comp
			AEGP_LayerH master_comp_layerH;
			suites.LayerSuite()->AEGP_AddLayer(source_comp_itemH, master_compH, &master_comp_layerH);
			
			
			// set various layer parameters (if applicable)
			string layer_name = layer.ps_name();

			ResizeName(layer_name, (AEGP_MAX_LAYER_NAME_SIZE-1));
			
			AeName layer_nameAE = layer_name;
						
			suites.LayerSuite()->AEGP_SetLayerName(master_comp_layerH, layer_nameAE);
			
			if(!layer.visibility())
				suites.LayerSuite()->AEGP_SetLayerFlag(master_comp_layerH, AEGP_LayerFlag_VIDEO_ACTIVE, FALSE);
			
			if(layer.adjustment_layer())
				suites.LayerSuite()->AEGP_SetLayerFlag(master_comp_layerH, AEGP_LayerFlag_ADJUSTMENT_LAYER, TRUE);
				
			if(layer.opacity() != 255)
			{
				//AEGP_StreamRefH stream_refH; // already declared above (we only use it temporarily)
				AEGP_StreamValue2 val;
				
				suites.StreamSuite()->AEGP_GetNewLayerStream(S_mem_id, master_comp_layerH, AEGP_LayerStream_OPACITY, &stream_refH);
				
				val.streamH = stream_refH;  // necessary?
				val.val.one_d = 100.0 * (double)layer.opacity() / 255.0;
				
				suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, stream_refH, &val);
				
				suites.StreamSuite()->AEGP_DisposeStream(stream_refH);
			}
			
			if(layer.transfer_mode() != MODE_Normal)
			{
				AEGP_LayerTransferMode mode = { EnumToAEmode(layer.transfer_mode()), 0, AEGP_TrackMatte_NO_TRACK_MATTE};
				
				suites.LayerSuite()->AEGP_SetLayerTransferMode(master_comp_layerH, &mode);
			}
			
			
			// add source comp to contact sheet
			if(contact_sheet_mult > 0 && contact_sheet_compH != NULL)
			{
				const int tile_pos = total_layers - 1 - i;
				
				const int tile_y = tile_pos / contact_tiles_x;
				const int tile_x = tile_pos % contact_tiles_x;
				
				
				AEGP_LayerH contact_comp_layerH;
				suites.LayerSuite()->AEGP_AddLayer(source_comp_itemH, contact_sheet_compH, &contact_comp_layerH);
				
				
				AEGP_StreamRefH position_stream, scale_stream;
				AEGP_StreamValue2 position_value, scale_value;
				const A_Time time_begin = {0, 1};
				
				suites.StreamSuite()->AEGP_GetNewLayerStream(S_mem_id, contact_comp_layerH, AEGP_LayerStream_POSITION, &position_stream);
				suites.StreamSuite()->AEGP_GetNewLayerStream(S_mem_id, contact_comp_layerH, AEGP_LayerStream_SCALE, &scale_stream);
				
				suites.StreamSuite()->AEGP_GetNewStreamValue(S_mem_id, position_stream, AEGP_LTimeMode_LayerTime, &time_begin, TRUE, &position_value);
				suites.StreamSuite()->AEGP_GetNewStreamValue(S_mem_id, scale_stream, AEGP_LTimeMode_LayerTime, &time_begin, TRUE, &scale_value);
				
				const float tile_width = (float)contact_sheet_mult * (float)width / (float)contact_tiles_x;
				const float tile_height = (float)contact_sheet_mult * (float)height / (float)contact_tiles_x;
				
				position_value.val.two_d.x = ((float)tile_x * tile_width) + (tile_width / 2.f);
				position_value.val.two_d.y = ((float)tile_y * tile_height) + (tile_height / 2.f);
				
				scale_value.val.two_d.x = scale_value.val.two_d.y = 100.f * contact_sheet_mult / (float)contact_tiles_x;
				
				suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, position_stream, &position_value);
				suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, scale_stream, &scale_value);
				
				suites.StreamSuite()->AEGP_DisposeStream(position_stream);
				suites.StreamSuite()->AEGP_DisposeStream(scale_stream);
			}
		}
		
		
		// dispose these stream values and streams from the original layer
	#define DISPOSE_STREAM(STREAM, VALUE, EXPH) \
		if(EXPH) { suites.MemorySuite()->AEGP_FreeMemHandle(EXPH); } \
		suites.StreamSuite()->AEGP_DisposeStreamValue(&VALUE); \
		suites.StreamSuite()->AEGP_DisposeStream(STREAM);
		
		DISPOSE_STREAM(anchor_stream, anchor_value, anchor_expH);
		DISPOSE_STREAM(position_stream, position_value, position_expH);
		DISPOSE_STREAM(rotation_stream, rotation_value, rotation_expH);
		DISPOSE_STREAM(scale_stream, scale_value, scale_expH);
		
		
	#define FILE_DESCRIPTION "File Description"	
		A_long file_description = 1;
		suites.PersistentDataSuite()->AEGP_GetLong(blobH, PREFS_SECTION, FILE_DESCRIPTION, file_description, &file_description);
		
		// add text layer with ProEXR File Description
		if(file_description > 0)
		{
			const AEGP_CompH description_compH = (file_description == 2 && contact_sheet_compH != NULL) ? contact_sheet_compH:
													master_compH;
		
			AEGP_LayerH text_layerH = NULL;
			suites.CompSuite()->AEGP_CreateTextLayerInComp(description_compH, &text_layerH);

			AeName text_layer_nameAE = string("ProEXR File Description");
			suites.LayerSuite()->AEGP_SetLayerName(text_layerH, text_layer_nameAE);
			
			AEGP_StreamRefH text_streamH = NULL;
			suites.StreamSuite()->AEGP_GetNewLayerStream(S_mem_id, text_layerH, AEGP_LayerStream_SOURCE_TEXT, &text_streamH);
			
			A_long footage_num_frames = 0, footage_num_files = 0;
			suites.FootageSuite()->AEGP_GetFootageNumFiles(footH, &footage_num_frames, &footage_num_files);
		
			if(footage_num_frames > 1 && ShiftKeyHeld()) // hold down shift to make a file description for each frame
			{
				A_Time frame_duration = {0, 0};
				suites.CompSuite()->AEGP_GetCompFrameDuration(description_compH, &frame_duration);
				
				for(int i=0; i < footage_num_frames; i++)
				{
					A_PathType *file_fpathZ = NULL;
					
				#ifdef AE_UNICODE_PATHS	
					AEGP_MemHandle u_fpathH = NULL;
					suites.FootageSuite()->AEGP_GetFootagePath(footH, i, AEGP_FOOTAGE_MAIN_FILE_INDEX, &u_fpathH);
					
					if(u_pathH)
					{
						suites.MemorySuite()->AEGP_LockMemHandle(u_fpathH, (void **)&file_fpathZ);
					}
				#else
					A_char fpath[AEGP_MAX_PATH_SIZE+1];
					
					suites.FootageSuite()->AEGP_GetFootagePath(footH, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, fpath);
					
				#ifdef AE_HFS_PATHS
					ConvertPath(fpath, fpath, AEGP_MAX_PATH_SIZE);
				#endif
				
					file_fpathZ = fpath;
				#endif
				
					if(file_fpathZ)
					{
						try{
						
						IStreamPlatform in_fstream(file_fpathZ);
						ProEXRdoc_read in_frame(in_fstream);
						
						A_Time frame_time;
						frame_time.value = (i * frame_duration.value);
						frame_time.scale = frame_duration.scale;
						
						AEGP_KeyframeIndex key_index;
						suites.KeyframeSuite()->AEGP_InsertKeyframe(text_streamH, AEGP_LTimeMode_LayerTime, &frame_time, &key_index);
						
						AEGP_StreamValue2 text_value;
						suites.KeyframeSuite()->AEGP_GetNewKeyframeValue(S_mem_id, text_streamH, key_index, &text_value);


						string description;
						AddDescription(description, in_frame);
						
						
						A_u_short *u_string = new A_u_short[ description.size() + 1 ];
						
						UTF8toUTF16(description, u_string, description.size() + 1);
						
						
						suites.TextDocumentSuite()->AEGP_SetText(text_value.val.text_documentH, u_string, description.size());
						
						delete[] u_string;
						
						suites.KeyframeSuite()->AEGP_SetKeyframeValue(text_streamH, key_index, &text_value);
						
						suites.StreamSuite()->AEGP_DisposeStreamValue(&text_value);
						
						}catch(...) {}
					}
				
				#ifdef AE_UNICODE_PATHS	
					if(u_fpathH)
						suites.MemorySuite()->AEGP_FreeMemHandle(u_fpathH);
				#endif
				}
			}
			else
			{
				AEGP_StreamValue2 text_value;
				suites.StreamSuite()->AEGP_GetNewStreamValue(S_mem_id, text_streamH, AEGP_LTimeMode_LayerTime, &time_begin, TRUE, &text_value);
				
				
				string description;
				AddDescription(description, in_file);
				
				
				A_u_short *u_string = new A_u_short[ description.size() + 1 ];
				
				UTF8toUTF16(description, u_string, description.size() + 1);
				
				
				suites.TextDocumentSuite()->AEGP_SetText(text_value.val.text_documentH, u_string, description.size());
				
				delete[] u_string;
				
				suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, text_streamH, &text_value);
				
				suites.StreamSuite()->AEGP_DisposeStreamValue(&text_value);
			}
			
			suites.StreamSuite()->AEGP_DisposeStream(text_streamH);
			
			suites.LayerSuite()->AEGP_SetLayerFlag(text_layerH, AEGP_LayerFlag_VIDEO_ACTIVE, FALSE);
			suites.LayerSuite()->AEGP_SetLayerFlag(text_layerH, AEGP_LayerFlag_GUIDE_LAYER, TRUE);
			suites.LayerSuite()->AEGP_SetLayerFlag(text_layerH, AEGP_LayerFlag_SHY, TRUE);
		}
		
		
		// now delete the original layer
		suites.LayerSuite()->AEGP_DeleteLayer(layerH);
		
		
	#ifdef AE_UNICODE_PATHS	
		if(u_pathH)
			suites.MemorySuite()->AEGP_FreeMemHandle(u_pathH);
	#endif
	}
	
	}catch(...) {}
	
	return err;
}


static A_Err BuildVRimgCompsFromLayer(AEGP_LayerH layerH)
{
	A_Err			err 		= A_Err_NONE;
	
	AEGP_SuiteHandler	suites(sP);

	try{
	

	AEGP_ItemH layer_itemH = NULL;
	suites.LayerSuite()->AEGP_GetLayerSourceItem(layerH, &layer_itemH);
	
	if( layer_itemH && ItemIsVRimg(layer_itemH, FALSE) )
	{
		// basic stuff we'll be needing
		AEGP_CompH master_compH;
		AEGP_ItemH master_comp_itemH;
		suites.LayerSuite()->AEGP_GetLayerParentComp(layerH, &master_compH);
		suites.CompSuite()->AEGP_GetItemFromComp(master_compH, &master_comp_itemH); 

		AEGP_ItemH parent_folderH;
		suites.ItemSuite()->AEGP_GetItemParentFolder(master_comp_itemH, &parent_folderH);
		
		
		// we need the path so we can read the file, duh
		AEGP_FootageH footH;
		suites.FootageSuite()->AEGP_GetMainFootageFromItem(layer_itemH, &footH);
		
		// get interpretation for Alpha purposes
		AEGP_FootageInterp interp;
		suites.FootageSuite()->AEGP_GetFootageInterpretation(layer_itemH, FALSE, &interp);
		
		
		A_char path[AEGP_MAX_PATH_SIZE+1];
	
	#ifdef AE_UNICODE_PATHS	
		AEGP_MemHandle u_pathH = NULL;
		A_PathType *file_pathZ = NULL;
		suites.FootageSuite()->AEGP_GetFootagePath(footH, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, &u_pathH);
		
		if(u_pathH)
		{
			suites.MemorySuite()->AEGP_LockMemHandle(u_pathH, (void **)&file_pathZ);
			
			string char_path = UTF16toUTF8(file_pathZ);
			
			strncpy(path, char_path.c_str(), AEGP_MAX_PATH_SIZE);
		}
	#else
		suites.FootageSuite()->AEGP_GetFootagePath(footH, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, path);
		
		A_PathType *file_pathZ = path;
	#endif
		
	#ifdef AE_HFS_PATHS
		ConvertPath(path, path, AEGP_MAX_PATH_SIZE);
	#endif

		
		const string filename = GetBaseName(suites, layer_itemH);
		

		// get info from the VRimg
		IStreamPlatform in_stream(file_pathZ);
		
		VRimg::InputFile input(in_stream);
		
	#ifdef AE_UNICODE_PATHS	
		if(u_pathH)
			suites.MemorySuite()->AEGP_FreeMemHandle(u_pathH);
	#endif
	
		// create a folder to hold source comps
		AEGP_ItemH comps_folderH;
		
		string comps_folder_name = filename;
		if(comps_folder_name.size() > (AEGP_MAX_ITEM_NAME_SIZE-14) )
			comps_folder_name.resize(AEGP_MAX_ITEM_NAME_SIZE-14);
			
		comps_folder_name += " source comps";
		
		AeName comps_folder_nameAE = comps_folder_name;
		
		suites.ItemSuite()->AEGP_CreateNewFolder(comps_folder_nameAE, parent_folderH, &comps_folderH );
		
		
		// everything you need to know about the master comp
		A_long width, height;
		A_Ratio pixel_aspect;
		A_Time duration;
		A_FpLong framerate;
		A_Ratio framerateRatio;
		
		suites.ItemSuite()->AEGP_GetItemDimensions(master_comp_itemH, &width, &height);
		suites.ItemSuite()->AEGP_GetItemPixelAspectRatio(master_comp_itemH, &pixel_aspect);
		suites.ItemSuite()->AEGP_GetItemDuration(master_comp_itemH, &duration);
		suites.CompSuite()->AEGP_GetCompFramerate(master_compH, &framerate);
		
		framerateRatio.den = 1000; framerateRatio.num = (framerateRatio.den * framerate) + 0.5;
		
		
		// everything you need to know about the source layer
		AEGP_StreamRefH stream_refH; // this is used later on down
		AEGP_StreamRefH anchor_stream, position_stream, rotation_stream, scale_stream;
		AEGP_StreamValue2 anchor_value, position_value, rotation_value, scale_value;
		AEGP_MemHandle anchor_expH, position_expH, rotation_expH, scale_expH; // expression handles
		A_Boolean anchor_exp_enabled, position_exp_enabled, rotaiton_exp_enabled, scale_exp_enabled;
		A_Time time_begin = {0, 1};
		
		
		GET_LAYER_STREAM_VALUE(layerH, AEGP_LayerStream_ANCHORPOINT, anchor_stream, &anchor_value, anchor_expH, anchor_exp_enabled)
		GET_LAYER_STREAM_VALUE(layerH, AEGP_LayerStream_POSITION, position_stream, &position_value, position_expH, position_exp_enabled);
		GET_LAYER_STREAM_VALUE(layerH, AEGP_LayerStream_ROTATION, rotation_stream, &rotation_value, rotation_expH, rotaiton_exp_enabled);
		GET_LAYER_STREAM_VALUE(layerH, AEGP_LayerStream_SCALE, scale_stream, &scale_value, scale_expH, scale_exp_enabled);
		
		
		// build a list of layer names
		vector<string> layer_list;
		
		bool have_rgb = false;
		bool have_alpha = false;
		
		const VRimg::Header::LayerMap &layers = input.header().layers();
		
		for(VRimg::Header::LayerMap::const_iterator i = layers.begin(); i != layers.end(); i++)
		{
			string channel_name = i->first;
			
			if(channel_name == "RGB color")
				have_rgb = true;
			else if(channel_name == "Alpha")
				have_alpha = true;
			else
				layer_list.push_back(channel_name);
		}
		
		// making sure these appear at the top
		if(have_alpha)
			layer_list.push_back("Alpha");
		
		if(have_rgb)
			layer_list.push_back("RGB color");
		

		if(layer_list.size() < 1)
			throw Iex::LogicExc("No layers found.");
		
		

		// are we building a Contact sheet?
	#define PREFS_SECTION		"ProEXR"
	#define CONTACT_SHEET_SIZE	"Contact Sheet Size"
		AEGP_PersistentBlobH blobH = NULL;
		suites.PersistentDataSuite()->AEGP_GetApplicationBlob(&blobH);
		
		A_long contact_sheet_mult = 2;
		suites.PersistentDataSuite()->AEGP_GetLong(blobH, PREFS_SECTION, CONTACT_SHEET_SIZE, contact_sheet_mult, &contact_sheet_mult);
		
		AEGP_CompH contact_sheet_compH = NULL;
		int contact_tiles_x = 1, contact_tiles_y = 1;
		
		if(contact_sheet_mult > 0 && layer_list.size() > 1)
		{
			const int total_tiles = layer_list.size();
		
			while(contact_tiles_x * contact_tiles_y < total_tiles)
			{
				if(contact_tiles_y > contact_tiles_x)
					contact_tiles_x++;
				else
					contact_tiles_y++;
			}
			
			if(contact_tiles_x < 2)
				contact_sheet_mult = 1;
			
			const int contact_width = width * contact_sheet_mult;
			const int contact_height = height * contact_sheet_mult * contact_tiles_y / contact_tiles_x;
				
			string contact_sheet_name = filename;

			ResizeName(contact_sheet_name, (AEGP_MAX_ITEM_NAME_SIZE-14));
				
			contact_sheet_name += " contact sheet";
			
			AeName comps_folder_nameAE = contact_sheet_name;
		
			suites.CompSuite()->AEGP_CreateComp(parent_folderH, comps_folder_nameAE, contact_width, contact_height,
													&pixel_aspect, &duration, &framerateRatio, &contact_sheet_compH);
		}
		
		
		// create source comps, add to master comp
		for(int i=0; i < layer_list.size(); i++)
		{
			string layer_name = layer_list[i];
			
			const VRimg::Layer *layer = input.header().findLayer( layer_name );
			
			if(layer == NULL)
				throw Iex::LogicExc("Layer is NULL.");

			// source comp
			AEGP_CompH source_compH;
			
			string source_comp_name = layer_name + " source";
			
			ResizeName(source_comp_name, (AEGP_MAX_ITEM_NAME_SIZE-1));
			
			AeName source_comp_nameAE = source_comp_name;

			suites.CompSuite()->AEGP_CreateComp(comps_folderH, source_comp_nameAE, width, height,
													&pixel_aspect, &duration, &framerateRatio, &source_compH);
			
			// source comp item
			AEGP_ItemH source_comp_itemH;
			suites.CompSuite()->AEGP_GetItemFromComp(source_compH, &source_comp_itemH);
			
			// add footage to source comp
			AEGP_LayerH footage_layerH;
			suites.LayerSuite()->AEGP_AddLayer(layer_itemH, source_compH, &footage_layerH);
			
			
			// copy settings from original layer
			SET_LAYER_STREAM_VAL(footage_layerH, AEGP_LayerStream_ANCHORPOINT, &anchor_value, anchor_expH, anchor_exp_enabled);
			SET_LAYER_STREAM_VAL(footage_layerH, AEGP_LayerStream_POSITION, &position_value, position_expH, position_exp_enabled);
			SET_LAYER_STREAM_VAL(footage_layerH, AEGP_LayerStream_ROTATION, &rotation_value, rotation_expH, rotaiton_exp_enabled);
			SET_LAYER_STREAM_VAL(footage_layerH, AEGP_LayerStream_SCALE, &scale_value, scale_expH, scale_exp_enabled);
			

			// apply EXtractoR or IDentifier (unless the layer is just "RGB" or "Y[RY][BY]")
			if(layer_name != "RGB color")
			{
				// add effect to layer
				if(layer->type == VRimg::INT && gIDentifier_key)
				{
					// set up the parameter data
					IDentifierArbitraryData arb;
					
					arb.version = 1;
					arb.index = 0;
					strncpy(arb.name, layer_name.c_str(), MAX_CHANNEL_NAME_LEN);
					
					// copy to a handle
					A_Handle handle = (A_Handle)suites.HandleSuite()->host_new_handle( sizeof(IDentifierArbitraryData) );
					void *ptr = suites.HandleSuite()->host_lock_handle((PF_Handle)handle);
					
					memcpy(ptr, &arb, sizeof(IDentifierArbitraryData) );
					
					suites.HandleSuite()->host_unlock_handle((PF_Handle)handle);
					
					
					// apply effect, assign handle
					AEGP_EffectRefH new_effectH;
					AEGP_StreamRefH arb_stream;
					AEGP_StreamValue2 val;
					
					val.val.arbH = handle;
					
					suites.EffectSuite()->AEGP_ApplyEffect(S_mem_id, footage_layerH, gIDentifier_key, &new_effectH);
					
					
					suites.StreamSuite()->AEGP_GetNewEffectStreamByIndex(S_mem_id,
										new_effectH, IDENTIFIER_ARB_INDEX, &arb_stream);
					
					suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, arb_stream, &val);
					
					//suites.StreamSuite()->AEGP_DisposeStreamValue(&val);
					
					suites.StreamSuite()->AEGP_DisposeStream(arb_stream);
					
					suites.EffectSuite()->AEGP_DisposeEffect(new_effectH);
				}
				else if(layer->type != VRimg::INT && gEXtractoR_key)
				{
					// set up the parameter data
					EXtractoRArbitraryData arb;
					ChannelData *c_ptr[] = { &arb.red, &arb.green, &arb.blue, &arb.alpha };
					
					arb.version = 1;
					arb.red.action = arb.green.action = arb.blue.action = arb.alpha.action = DO_COPY;
					arb.red.index = arb.green.index = arb.blue.index = arb.alpha.index = 0;
					
					int c = 0;
						
					if( layer->dimensions == 1 )
					{
						string channel = layer_name;
						
						strncpy(c_ptr[0]->name, channel.c_str(), MAX_CHANNEL_NAME_LEN);	c_ptr[0]->action = DO_EXTRACT;
						strncpy(c_ptr[1]->name, channel.c_str(), MAX_CHANNEL_NAME_LEN);	c_ptr[1]->action = DO_EXTRACT;
						strncpy(c_ptr[2]->name, channel.c_str(), MAX_CHANNEL_NAME_LEN);	c_ptr[2]->action = DO_EXTRACT;
					}
					else
					{
						string chan_names[4] = { layer_name + ".R", layer_name + ".G", layer_name + ".B", layer_name + ".A" };
						
						// get the non-alphas
						while(c < layer->dimensions && c < 4)
						{
							strncpy(c_ptr[c]->name, chan_names[c].c_str(), MAX_CHANNEL_NAME_LEN);
							
							c_ptr[c]->action = DO_EXTRACT;
							
							c++;
						}
					}
					
					
					// copy to a handle
					A_Handle handle = (A_Handle)suites.HandleSuite()->host_new_handle( sizeof(EXtractoRArbitraryData) );
					void *ptr = suites.HandleSuite()->host_lock_handle((PF_Handle)handle);
					
					memcpy(ptr, &arb, sizeof(EXtractoRArbitraryData) );
					
					suites.HandleSuite()->host_unlock_handle((PF_Handle)handle);
					
					
					// apply effect, assign handle
					AEGP_EffectRefH new_effectH;
					AEGP_StreamRefH arb_stream;
					AEGP_StreamValue2 val;
					
					val.val.arbH = handle;
					
					suites.EffectSuite()->AEGP_ApplyEffect(S_mem_id, footage_layerH, gEXtractoR_key, &new_effectH);
					
					
					suites.StreamSuite()->AEGP_GetNewEffectStreamByIndex(S_mem_id,
										new_effectH, EXTRACTOR_ARB_INDEX, &arb_stream);
					
					suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, arb_stream, &val);
					
					
					// UnMult if footage is interpreted as Premultipled
					if(interp.al.flags == AEGP_AlphaPremul)
					{
						AEGP_StreamRefH unmult_stream;
						AEGP_StreamValue2 u_val;
						
						u_val.val.one_d = 1.0;

						suites.StreamSuite()->AEGP_GetNewEffectStreamByIndex(S_mem_id,
											new_effectH, EXTRACTOR_UNMULT_INDEX, &unmult_stream);
						
						suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, unmult_stream, &u_val);
					
						suites.StreamSuite()->AEGP_DisposeStream(unmult_stream);
					}
					
					suites.StreamSuite()->AEGP_DisposeStream(arb_stream);
					
					suites.EffectSuite()->AEGP_DisposeEffect(new_effectH);
				}
			}

								
			// add source comp to master comp
			if( layer_name != "Alpha" || (interp.al.flags & AEGP_AlphaIgnore)) // not alpha unless not reading it as part of the footage
			{
				AEGP_LayerH master_comp_layerH;
				suites.LayerSuite()->AEGP_AddLayer(source_comp_itemH, master_compH, &master_comp_layerH);
				
				
				// set various layer parameters (if applicable)
				if(layer_name.size() > (AEGP_MAX_LAYER_NAME_SIZE-1))
					layer_name.resize(AEGP_MAX_LAYER_NAME_SIZE-1);
				
				AeName layer_nameAE = layer_name;

				suites.LayerSuite()->AEGP_SetLayerName(master_comp_layerH, layer_nameAE);
			}
			
			
			// add source comp to contact sheet
			if(contact_sheet_mult > 0 && contact_sheet_compH != NULL)
			{
				const int total_tiles = layer_list.size();
			
				const int tile_pos = total_tiles - 1 - i;
				
				const int tile_y = tile_pos / contact_tiles_x;
				const int tile_x = tile_pos % contact_tiles_x;
				
				
				AEGP_LayerH contact_comp_layerH;
				suites.LayerSuite()->AEGP_AddLayer(source_comp_itemH, contact_sheet_compH, &contact_comp_layerH);
				
				
				AEGP_StreamRefH position_stream, scale_stream;
				AEGP_StreamValue2 position_value, scale_value;
				const A_Time time_begin = {0, 1};
				
				suites.StreamSuite()->AEGP_GetNewLayerStream(S_mem_id, contact_comp_layerH, AEGP_LayerStream_POSITION, &position_stream);
				suites.StreamSuite()->AEGP_GetNewLayerStream(S_mem_id, contact_comp_layerH, AEGP_LayerStream_SCALE, &scale_stream);
				
				suites.StreamSuite()->AEGP_GetNewStreamValue(S_mem_id, position_stream, AEGP_LTimeMode_LayerTime, &time_begin, TRUE, &position_value);
				suites.StreamSuite()->AEGP_GetNewStreamValue(S_mem_id, scale_stream, AEGP_LTimeMode_LayerTime, &time_begin, TRUE, &scale_value);
				
				const float tile_width = (float)contact_sheet_mult * (float)width / (float)contact_tiles_x;
				const float tile_height = (float)contact_sheet_mult * (float)height / (float)contact_tiles_x;
				
				position_value.val.two_d.x = ((float)tile_x * tile_width) + (tile_width / 2.f);
				position_value.val.two_d.y = ((float)tile_y * tile_height) + (tile_height / 2.f);
				
				scale_value.val.two_d.x = scale_value.val.two_d.y = 100.f * contact_sheet_mult / (float)contact_tiles_x;
				
				suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, position_stream, &position_value);
				suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, scale_stream, &scale_value);
				
				suites.StreamSuite()->AEGP_DisposeStream(position_stream);
				suites.StreamSuite()->AEGP_DisposeStream(scale_stream);
			}
		}
		
		
		// dispose these stream values and streams from the original layer
		DISPOSE_STREAM(anchor_stream, anchor_value, anchor_expH);
		DISPOSE_STREAM(position_stream, position_value, position_expH);
		DISPOSE_STREAM(rotation_stream, rotation_value, rotation_expH);
		DISPOSE_STREAM(scale_stream, scale_value, scale_expH);
		
		
		// now delete the original layer itself
		suites.LayerSuite()->AEGP_DeleteLayer(layerH);
	}
	
	}catch(...) {}
	
	return err;
}


static A_Err MakeDisplayWindowComp(vector<AEGP_ItemH> &exr_list)
{
	A_Err			err 		= A_Err_NONE;
	
	AEGP_SuiteHandler	suites(sP);
	
	vector<Box2i> data_windows, display_windows;
	
	float exr_framerate = -1.f;
	
	try{
	
	// build lists of data and display windows
	for(vector<AEGP_ItemH>::const_iterator i = exr_list.begin(); i != exr_list.end(); i++)
	{
		AEGP_FootageH footH;
		suites.FootageSuite()->AEGP_GetMainFootageFromItem(*i, &footH);
		
	#ifdef AE_UNICODE_PATHS
		AEGP_MemHandle u_pathH = NULL;
		A_PathType *path = NULL;
	
		suites.FootageSuite()->AEGP_GetFootagePath(footH, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, &u_pathH);
		
		if(u_pathH)
		{
			suites.MemorySuite()->AEGP_LockMemHandle(u_pathH, (void **)&path);
		}
		else
			return AEIO_Err_BAD_FILENAME; 
	#else
		A_char path[AEGP_MAX_PATH_SIZE+1];
		
		suites.FootageSuite()->AEGP_GetFootagePath(footH, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, path);
	#endif
		
	#ifdef AE_HFS_PATHS
		ConvertPath(path, path, AEGP_MAX_PATH_SIZE);
	#endif

		IStreamPlatform instream(path);
		HybridInputFile in(instream);
		
	#ifdef AE_UNICODE_PATHS
		suites.MemorySuite()->AEGP_FreeMemHandle(u_pathH);
	#endif

		const Box2i &data_window = in.dataWindow();
		const Box2i &display_window = in.displayWindow();

		data_windows.push_back(data_window);
		display_windows.push_back(display_window);
		
		
		if(exr_framerate < 0.f && hasFramesPerSecond( in.header(0) ))
		{
			const Rational &fps = framesPerSecond( in.header(0) );
			
			exr_framerate = fps;
		}
	}
	
	}catch(...) {}
	
	
	if( (data_windows.size() == exr_list.size()) && (display_windows.size() == exr_list.size()) )
	{
		// make sure all the DisplayWindows are the same size
		A_Boolean same_dw = TRUE;
		
		Box2i sample_dw = display_windows.front();
		
		for(vector<Box2i>::const_iterator i = display_windows.begin(); i != display_windows.end(); i++)
		{
			if(*i != sample_dw)
				same_dw = FALSE;
		}
		
		if(same_dw)
		{
			// make a new comp to the DisplayWindow size
			
			// use the first EXR as the template
			AEGP_ItemH itemH = exr_list.front();
			
			// item name
			AeName name = GetName(suites, itemH);
			
			// parent folder
			AEGP_ItemH parent_folderH;
			suites.ItemSuite()->AEGP_GetItemParentFolder(itemH, &parent_folderH);
			
			
			// everything you need to know about the footage
			A_long width, height;
			A_Ratio pixel_aspect;
			A_Ratio framerate;
			A_Time duration;
			
			width = 1 + sample_dw.max.x - sample_dw.min.x;
			height = 1 + sample_dw.max.y - sample_dw.min.y;
			
			suites.ItemSuite()->AEGP_GetItemPixelAspectRatio(itemH, &pixel_aspect);
			
			
			A_FpLong float_framerate = 24.0;
			
			if(exr_framerate > 0.f)
			{
				// use the frame rate from the EXR
				float_framerate = exr_framerate;
			}
			else
			{
				// get the sequence frame rate from preferences
				AEGP_PersistentBlobH blobH = NULL;
				suites.PersistentDataSuite()->AEGP_GetApplicationBlob(&blobH);
				
				A_Boolean new_pref = FALSE;
				suites.PersistentDataSuite()->AEGP_DoesKeyExist(blobH, "Import Options Preference Section", "Import Options Default Sequence FPS", &new_pref);
				
				if(new_pref)
				{
					suites.PersistentDataSuite()->AEGP_GetFpLong(blobH, "Import Options Preference Section", "Import Options Default Sequence FPS", 24.0, &float_framerate);
				}
				else			
					suites.PersistentDataSuite()->AEGP_GetFpLong(blobH, "General Section", "Default Import Sequence FPS", 24.0, &float_framerate);
			}

			framerate.den = 1000; framerate.num = (1000.0 * float_framerate) + 0.5;
			
			duration.scale = framerate.num;
			
			if(exr_list.size() == 1)
				duration.value = 10 * framerate.den;
			else
				duration.value = exr_list.size() * framerate.den;
			
			
			string comp_name = name;
			comp_name.resize( comp_name.size() - 4 ); // I know there's an .exr at the end
			
			ResizeName(comp_name, (AEGP_MAX_ITEM_NAME_SIZE-9));
			
			comp_name += " display";
			
			AeName comp_nameAE = comp_name;

			AEGP_CompH display_compH;
			suites.CompSuite()->AEGP_CreateComp(parent_folderH, comp_nameAE, width, height,
													&pixel_aspect, &duration, &framerate, &display_compH);
			
			// add each EXR and place it appropriately
			for(int i=0; i < exr_list.size(); i++)
			{
				A_long exr_width, exr_height;
				suites.ItemSuite()->AEGP_GetItemDimensions(exr_list[i], &exr_width, &exr_height);
				
				// add item to comp
				AEGP_LayerH layerH;
				suites.LayerSuite()->AEGP_AddLayer(exr_list[i], display_compH, &layerH);
				
				suites.LayerSuite()->AEGP_ReorderLayer(layerH, i);
				
				if(exr_list.size() > 1)
				{
					A_Time begin_time = {i * framerate.den, framerate.num};
					A_Time one_frame = {1 * framerate.den, framerate.num};
					suites.LayerSuite()->AEGP_SetLayerInPointAndDuration(layerH, AEGP_LTimeMode_CompTime, &begin_time, &one_frame);
				}

				const int data_width = (data_windows[i].max.x - data_windows[i].min.x) + 1;
				const int data_height = (data_windows[i].max.y - data_windows[i].min.y) + 1;
				
				// this assumes that dataWindow and displayWindow will be different sizes, which might not be true
				const bool using_dataWindow = (exr_width == data_width) && (exr_height == data_height);
				
				if(using_dataWindow)
				{
					// adjust the position based on the display window
					AEGP_StreamRefH position_stream;
					AEGP_StreamValue2 position_value;
					A_Time time_begin = {0, 1};
					
					suites.StreamSuite()->AEGP_GetNewLayerStream(S_mem_id, layerH, AEGP_LayerStream_POSITION, &position_stream);
					suites.StreamSuite()->AEGP_GetNewStreamValue(S_mem_id, position_stream, AEGP_LTimeMode_LayerTime, &time_begin, TRUE, &position_value);
					
					// to start, the middle of data_window will be in the middle of display_window
					// so lets offset it
					V2d data_center, display_center;
					
					data_center.x = ((double)data_windows[i].min.x + (double)data_windows[i].max.x + 1.0) / 2.0;
					data_center.y = ((double)data_windows[i].min.y + (double)data_windows[i].max.y + 1.0) / 2.0;
					
					display_center.x = ((double)sample_dw.min.x + (double)sample_dw.max.x + 1.0) / 2.0;
					display_center.y = ((double)sample_dw.min.y + (double)sample_dw.max.y + 1.0) / 2.0;
					
					
					position_value.val.two_d.x += data_center.x - display_center.x;
					position_value.val.two_d.y += data_center.y - display_center.y;
					
					suites.StreamSuite()->AEGP_SetStreamValue(S_mem_id, position_stream, &position_value);
					suites.StreamSuite()->AEGP_DisposeStream(position_stream);
				}
			}
		}
		else
		{
			suites.UtilitySuite()->AEGP_ReportInfo(S_mem_id, "Display Windows are not the same for selected EXR files.");
		}
	}
	else
	{
		suites.UtilitySuite()->AEGP_ReportInfo(S_mem_id, "Error reading EXR files.");
	}
	
	
	return err;
}


A_Err
GPCommandHook(
	AEGP_GlobalRefcon	plugin_refconPV,
	AEGP_CommandRefcon	refconPV,
	AEGP_Command		command,
	AEGP_HookPriority	hook_priority,
	A_Boolean			already_handledB,
	A_Boolean			*handledPB)
{
	A_Err			err 		= A_Err_NONE;
	AEGP_SuiteHandler	suites(sP);
	
	if(command == gCompCreatorCmd)
	{
		try{

		// fix the auto cache threshold for the poor user, seeing as the Plabt refuses to fix their sequence settings bug
	#define PREFS_AUTO_CACHE "Auto Cache Threshold"
		
		AEGP_PersistentBlobH blobH = NULL;
		suites.PersistentDataSuite()->AEGP_GetApplicationBlob(&blobH);
		
		A_long auto_cache_channels = 5;
		suites.PersistentDataSuite()->AEGP_GetLong(blobH, PREFS_SECTION, PREFS_AUTO_CACHE, auto_cache_channels, &auto_cache_channels);
		
		if(auto_cache_channels == 0)
		{
			suites.PersistentDataSuite()->AEGP_SetLong(blobH, PREFS_SECTION, PREFS_AUTO_CACHE, 5);
			
			//suites.UtilitySuite()->AEGP_ReportInfo(S_mem_id, "Your OpenEXR Auto Cache Threshold has been set to 5. You're welcome!");
			//suites.UtilitySuite()->AEGP_ExecuteScript(S_mem_id, "alert(\"Your OpenEXR Auto Cache Threshold has been set to 5. You're welcome!\\n\\n(You might want to re-import existing footage though.)\")", FALSE, NULL, NULL);
		}
		
		
		A_Boolean easter_egg = OptionKeyHeld();
		
		AEGP_ProjectH projH;
		suites.ProjSuite()->AEGP_GetProjectByIndex(0, &projH);
		
		if(easter_egg)
		{
			// undo
			suites.UtilitySuite()->AEGP_StartUndoGroup("DisplayWindow Comp");
			
			AEGP_ItemH itemH;
			suites.ItemSuite()->AEGP_GetFirstProjItem(projH, &itemH);
			
			vector<AEGP_ItemH> exr_list;
			
			// look for selected EXR files
			do{
				A_Boolean is_selected;
				suites.ItemSuite()->AEGP_IsItemSelected(itemH, &is_selected);
			
				if(itemH && is_selected && ItemIsEXR(itemH, TRUE))
				{
					exr_list.push_back(itemH);
				}
			}while( !suites.ItemSuite()->AEGP_GetNextProjItem(projH, itemH, &itemH) && itemH );
			
			
			if(exr_list.size() > 0)
				MakeDisplayWindowComp(exr_list);
		}
		else
		{
			// undo
			suites.UtilitySuite()->AEGP_StartUndoGroup("ProEXR Layer Comps");
			
			A_Boolean found = FALSE;
			
			// look for a selected EXR layer in a comp
			if(!found)
			{
				AEGP_ItemH itemH = NULL;
				suites.ItemSuite()->AEGP_GetActiveItem(&itemH);
				
				if(itemH)
				{
					AEGP_ItemType type;
					suites.ItemSuite()->AEGP_GetItemType(itemH, &type);
					
					if(type == AEGP_ItemType_COMP)
					{
						AEGP_CompH compH;
						AEGP_Collection2H collH = NULL;
						
						suites.CompSuite()->AEGP_GetCompFromItem(itemH, &compH);
						suites.CompSuite()->AEGP_GetNewCollectionFromCompSelection(S_mem_id, compH, &collH);
						
						if(collH)
						{
							A_u_long num_items;
							
							suites.CollectionSuite()->AEGP_GetCollectionNumItems(collH, &num_items);
							
							for(int i=0; i < num_items; i++)
							{
								AEGP_CollectionItemV2 coll_item;
								suites.CollectionSuite()->AEGP_GetCollectionItemByIndex(collH, i, &coll_item);
								
								if(coll_item.type == AEGP_CollectionItemType_LAYER)
								{
									AEGP_ItemH layer_itemH;
									suites.LayerSuite()->AEGP_GetLayerSourceItem(coll_item.u.layer.layerH, &layer_itemH);
									
									if(layer_itemH)
									{
										if( ItemIsEXR(layer_itemH, FALSE) )
										{
											// replace EXR footage with source comps, etc
											BuildEXRcompsFromLayer(coll_item.u.layer.layerH);
											
											found = TRUE;
										}
										else if( ItemIsVRimg(layer_itemH, FALSE) )
										{
											BuildVRimgCompsFromLayer(coll_item.u.layer.layerH);
											
											found = TRUE;
										}
									}
								}
							}
						
							suites.CollectionSuite()->AEGP_DisposeCollection(collH);
						}
					}
				}
			}


			if(!found)
			{
				AEGP_ItemH itemH;
				suites.ItemSuite()->AEGP_GetFirstProjItem(projH, &itemH);
				
				list<AEGP_ItemH> exr_list, vrimg_list;
				
				// look for selected EXR and VRimg files
				do{
					A_Boolean is_selected;
					suites.ItemSuite()->AEGP_IsItemSelected(itemH, &is_selected);
				
					if(itemH && is_selected)
					{
						if( ItemIsEXR(itemH, FALSE) )
						{
							exr_list.push_back(itemH);
							
							found = TRUE;
						}
						else if( ItemIsVRimg(itemH, FALSE) )
						{
							vrimg_list.push_back(itemH);
							
							found = TRUE;
						}
					}
				}while( !suites.ItemSuite()->AEGP_GetNextProjItem(projH, itemH, &itemH) && itemH );
				
				// got EXR items (without messing with project ordering yet), now iterate
				for(list<AEGP_ItemH>::const_iterator i = exr_list.begin(); i != exr_list.end(); i++)
				{
					AEGP_LayerH layerH = NULL;
					
					// make a new comp with this footage...
					NewCompFromExrFootageItem(*i, &layerH);
					
					// ...then replace the footage with source comps, etc.
					if(layerH != NULL)
						BuildEXRcompsFromLayer(layerH);
				}
				
				// repeat for VRimg
				for(list<AEGP_ItemH>::const_iterator i = vrimg_list.begin(); i != vrimg_list.end(); i++)
				{
					AEGP_LayerH layerH = NULL;
					
					// make a new comp with this footage...
					NewCompFromFootageItem(*i, &layerH);
					
					// ...then replace the footage with source comps, etc.
					if(layerH != NULL)
						BuildVRimgCompsFromLayer(layerH);
				}
			}
		}
		
		
		// well it IS exr, you know, so let's do 32-bit (wish I could set a linear color space too)
		// maybe not
		//AEGP_ProjBitDepth depthH;
		//suites.ProjSuite()->AEGP_GetProjectBitDepth(projH, &depthH);
		
		//if(depthH != AEGP_ProjBitDepth_32)
		//	suites.ProjSuite()->AEGP_SetProjectBitDepth(projH, AEGP_ProjBitDepth_32);
		
		
		// undo
		suites.UtilitySuite()->AEGP_EndUndoGroup();

		}catch(...) {}
		
		// I handled it, right?
		*handledPB = TRUE;
	}
		
	return err;

}




A_Err
UpdateMenuHook(
	AEGP_GlobalRefcon		plugin_refconPV,
	AEGP_UpdateMenuRefcon	refconPV,
	AEGP_WindowType			active_window)
{
	A_Err 				err 			=	A_Err_NONE;		
	AEGP_SuiteHandler	suites(sP);

	try{
	
	A_Boolean found = FALSE;
	
	A_Boolean easter_egg = OptionKeyHeld();
	
	// the "Easter Egg" is that we will build a DisplayWindow comp from selected footage items
	if(easter_egg)
		suites.CommandSuite()->AEGP_SetMenuCommandName(gCompCreatorCmd, COMP_CREATOR_ALT_MENU_STR);
	else
		suites.CommandSuite()->AEGP_SetMenuCommandName(gCompCreatorCmd, COMP_CREATOR_MENU_STR);
	
	
	// look for a selected EXR layer in a comp
	if(!found)
	{
		AEGP_ItemH itemH = NULL;
		suites.ItemSuite()->AEGP_GetActiveItem(&itemH);
		
		if(itemH)
		{
			AEGP_ItemType type;
			suites.ItemSuite()->AEGP_GetItemType(itemH, &type);
			
			if(type == AEGP_ItemType_COMP)
			{
				AEGP_CompH compH;
				AEGP_Collection2H collH = NULL;
				
				suites.CompSuite()->AEGP_GetCompFromItem(itemH, &compH);
				suites.CompSuite()->AEGP_GetNewCollectionFromCompSelection(S_mem_id, compH, &collH);
				
				if(collH && !easter_egg)
				{
					A_u_long num_items;
					
					suites.CollectionSuite()->AEGP_GetCollectionNumItems(collH, &num_items);
					
					for(int i=0; i < num_items && !found; i++)
					{
						AEGP_CollectionItemV2 coll_item;
						
						suites.CollectionSuite()->AEGP_GetCollectionItemByIndex(collH, i, &coll_item);
						
						if(coll_item.type == AEGP_CollectionItemType_LAYER)
						{
							AEGP_ItemH layer_itemH;
							
							suites.LayerSuite()->AEGP_GetLayerSourceItem(coll_item.u.layer.layerH, &layer_itemH);
							
							if(layer_itemH && (ItemIsEXR(layer_itemH, FALSE) || ItemIsVRimg(layer_itemH, FALSE)))
								found = TRUE;
						}
					}
				}
				
				if(collH)
					suites.CollectionSuite()->AEGP_DisposeCollection(collH);
			}
		}
	}
	
	
	// check project for selected EXR footage items
	if(!found)
	{
		AEGP_ProjectH projH;
		AEGP_ItemH itemH;
		
		suites.ProjSuite()->AEGP_GetProjectByIndex(0, &projH);
		suites.ItemSuite()->AEGP_GetFirstProjItem(projH, &itemH);
		
		do{
			if(itemH)
			{
				A_Boolean is_selected;
				suites.ItemSuite()->AEGP_IsItemSelected(itemH, &is_selected);
				
				if(is_selected && (ItemIsEXR(itemH, easter_egg) || ItemIsVRimg(itemH, FALSE)))
					found = TRUE;
			}

		}while( !suites.ItemSuite()->AEGP_GetNextProjItem(projH, itemH, &itemH) && itemH && !found);
	}


	if(found)
		suites.CommandSuite()->AEGP_EnableCommand(gCompCreatorCmd);
	

	}catch(...) {}
		
	return err;
}


void GetControlKeys(SPBasicSuite *pica_basicP)
{
	// also do other inits here
	sP = pica_basicP;
	ProEXR_CopyPluginPath(S_path, AEGP_MAX_PATH_SIZE);
	

	AEGP_InstalledEffectKey next_key, prev_key = AEGP_InstalledEffectKey_NONE;
	A_char	plug_name[PF_MAX_EFFECT_NAME_LEN + 1];

	AEGP_SuiteHandler suites(sP);
	
#define EXTRACTOR_KEY "EXtractoR"
#define IDENTIFIER_KEY "IDentifier"
#define CRYPTOMATTE_KEY "Cryptomatte"

	suites.EffectSuite()->AEGP_GetNextInstalledEffect(prev_key, &next_key);
	
	while( (!gEXtractoR_key || !gIDentifier_key) && next_key)
	{
		suites.EffectSuite()->AEGP_GetEffectMatchName(next_key, plug_name);
		
		if(string(plug_name) == string(EXTRACTOR_KEY) )
			gEXtractoR_key = next_key;
		else if(string(plug_name) == string(IDENTIFIER_KEY) )
			gIDentifier_key = next_key;
		else if(string(plug_name) == string(CRYPTOMATTE_KEY) )
			gCryptomatte_key = next_key;
				
		prev_key = next_key;

		suites.EffectSuite()->AEGP_GetNextInstalledEffect(prev_key, &next_key);
	}
}

