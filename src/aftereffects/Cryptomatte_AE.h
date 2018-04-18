//
//	Cryptomatte AE plug-in
//		by Brendan Bolles <brendan@fnordware.com>
//
//
//	Part of ProEXR
//		http://www.fnordware.com/ProEXR
//
//

#pragma once

#ifndef _CRYPTOMATTE_H_
#define _CRYPTOMATTE_H_


#define PF_DEEP_COLOR_AWARE 1

#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_ChannelSuites.h"
#include "AE_EffectCBSuites.h"
#include "String_Utils.h"
#include "AE_GeneralPlug.h"
#include "fnord_SuiteHandler.h"

#ifdef MSWindows
	#include <Windows.h>
#else 
	#ifndef __MACH__
		#include <string.h>
	#endif
#endif	


// Versioning information 

#define NAME				"Cryptomatte"
#define DESCRIPTION			"Better ID Mattes"
#define RELEASE_DATE		__DATE__
#define AUTHOR				"Brendan Bolles"
#define COPYRIGHT			"(c) 2018 fnord"
#define WEBSITE				"www.fnordware.com"
#define	MAJOR_VERSION		2
#define	MINOR_VERSION		0
#define	BUG_VERSION			0
#define	STAGE_VERSION		PF_Stage_RELEASE
#define	BUILD_VERSION		0


enum {
	CRYPTO_INPUT = 0,
	CRYPTO_DATA,
	CRYPTO_DISPLAY,
	CRYPTO_SELECTION_MODE,
	
	CRYPTO_NUM_PARAMS
};

enum {
	ARBITRARY_DATA_ID = 1,
	DISPLAY_ID,
	SELECTION_MODE_ID
};


enum {
	DISPLAY_COLORS = 1,
	DISPLAY_MATTED_RGBA,
	DISPLAY_MATTE_ONLY,
	DISPLAY_NUM_OPTIONS = DISPLAY_MATTE_ONLY
};

#define DISPLAY_MENU_STR "Colors|Matted RGBA|Matte Only"


typedef uint32_t Hash;


#define MAX_LAYER_NAME_LEN 63 // same as PF_CHANNEL_NAME_LEN

typedef struct {
	char		magic[4]; // "cry1"
	char		reserved[28]; // 32 bytes at this point
	char		layer[MAX_LAYER_NAME_LEN + 1];
	A_u_long	manifest_size; // including null character
	Hash		manifest_hash;
	A_u_long	selection_size;
	Hash		selection_hash;
	char		data[4]; // manifest string + selection string 
} CryptomatteArbitraryData;


typedef struct {
	void		*context;
	A_Boolean	selectionChanged;
} CryptomatteSequenceData;


#define kUI_CONTROL_HEIGHT	70
#define kUI_CONTROL_WIDTH	0

#ifdef __cplusplus

#include <string>
#include <vector>
#include <set>
#include <map>

class CryptomatteException : public std::exception
{
  public:
	CryptomatteException(const std::string &what) throw() : _what(what) {}
	virtual ~CryptomatteException() throw() {}
	
	virtual const char* what() const throw() { return _what.c_str(); }

  private:
	std::string _what;
};


class CryptomatteContext
{
  public:
	CryptomatteContext(CryptomatteArbitraryData *arb);
	~CryptomatteContext();

	void Update(CryptomatteArbitraryData *arb);
	
	void LoadLevels(PF_InData *in_data);
	
	bool Valid() const { return _levels.size() > 0; }
	
	float GetCoverage(int x, int y) const;
	
	PF_PixelFloat GetColor(int x, int y) const;
	PF_PixelFloat GetSelectionColor(int x, int y) const;
	
	std::set<std::string> GetItems(int x, int y) const;
	std::set<std::string> GetItemsFromSelectionColor(const PF_PixelFloat &pixel) const;
	
	int Width() const;
	int Height() const;
	
	const PF_RationalScale & DownsampleX() const { return _downsampleX; }
	const PF_RationalScale & DownsampleY() const { return _downsampleY; }
	const A_long & CurrentTime() const { return _currentTime; }
	
	
	static std::string searchReplace(const std::string &str, const std::string &search, const std::string &replace);
	static std::string deQuote(const std::string &s);
	static void quotedTokenize(const std::string &str, std::vector<std::string> &tokens, const std::string& delimiters = " ");

  private:
	Hash _manifestHash;
	Hash _selectionHash;
	
	std::string _layer;
	std::map<std::string, Hash> _manifest;
	std::set<Hash> _selection;
	
	class Level
	{
	  public:
		Level(PF_InData *in_data, PF_ChannelRef &hash, PF_ChannelRef &coverage);
		Level(PF_InData *in_data, PF_ChannelRef &four, bool secondHalf);
		~Level();
		
		float GetCoverage(const std::set<Hash> &selection, int x, int y, bool &levelsEnd) const;
		float GetCoverage(int x, int y) const;
		
		PF_PixelFloat GetColor(int x, int y) const;
		
		Hash GetHash(int x, int y) const;
		
		inline int Width() const { return (_hash ? _hash->Width() : 0); }
		inline int Height() const { return (_hash ? _hash->Height() : 0); }
		
	  private:		
		class FloatBuffer
		{
		  public:
			FloatBuffer(PF_InData *in_data, char *origin, int width, int height, ptrdiff_t xStride, ptrdiff_t yStride);
			~FloatBuffer();
			
			inline const float & Get(int x, int y) const { return *((float *)_buf + (_width * y) + x); }
			
			inline const int & Width() const { return _width; }
			inline const int & Height() const { return _height; }
		
		  private:
			char *_buf;
			int _width;
			int _height;
		};
		
		FloatBuffer *_hash;
		FloatBuffer *_coverage;
	};
	
	std::vector<Level *> _levels;
	
	PF_RationalScale _downsampleX;
	PF_RationalScale _downsampleY;
	A_long _currentTime;
	
	std::string ItemForHash(const Hash &hash) const;
	
	enum NamingStyle {
		NAMING_RGBA,
		NAMING_rgba,
		NAMING_redgreenbluealpha,
		
		NAMING_BEGIN = NAMING_RGBA,
		NAMING_END = NAMING_redgreenbluealpha
	};

	void CalculateNextNames(std::string &nextHashName, std::string &nextCoverageName, NamingStyle style) const;
	void CalculateNext4Name(std::string &fourName, NamingStyle style) const;
};

extern "C" {
#endif


// Prototypes

DllExport	PF_Err 
PluginMain (	
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void			*extra) ;


PF_Err 
DoDialog (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output );


PF_Err
HandleEvent ( 
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*extra );


PF_Err
ArbNewDefault( // needed by ParamSetup()
	PF_InData			*in_data,
	PF_OutData			*out_data,
	void				*refconPV,
	PF_ArbitraryH		*arbPH);

PF_Err 
HandleArbitrary(
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ParamDef			*params[],
	PF_LayerDef			*output,
	PF_ArbParamsExtra	*extra);

const char *
GetLayer(const CryptomatteArbitraryData *arb);

const char *
GetSelection(const CryptomatteArbitraryData *arb);

const char *
GetManifest(const CryptomatteArbitraryData *arb);

void
SetArb(PF_InData *in_data, PF_ArbitraryH *arbH, const std::string &layer, const std::string &selection, const std::string &manifest);

void
SetArbSelection(PF_InData *in_data, PF_ArbitraryH *arbH, const std::string &selection);


#if defined(MAC_ENV) && PF_AE_PLUG_IN_VERSION >= PF_AE100_PLUG_IN_VERSION
void SetMickeyCursor(); // love our Mickey cursor, but we need an Objectice-C call in Cocoa
#endif

#ifdef __cplusplus
}
#endif



#endif // _CRYPTOMATTE_H_
