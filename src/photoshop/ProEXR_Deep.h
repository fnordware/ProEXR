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

#ifndef __ProEXR_Deep_H__
#define __ProEXR_Deep_H__


// weird problem I'm having with A.h in Xcode
#ifdef __GNUC__
typedef unsigned long long	uint64_t;
#endif


#include "A.h"
#include "PIDefines.h"				// New in 7.0 SDK
#include "PIFormat.h"				// Format Photoshop header file.
#include "PIExport.h"				// Export Photoshop header file.
#include "PIUtilities.h"			// SDK Utility library.

#include "ProEXR_PSIO.h"
#include "ProEXRdoc_PS.h"

/*
enum {
    ALPHA_TRANSPARENCY = 1,
    ALPHA_SEPERATE
};
typedef A_u_char AlphaMode ;

typedef struct ProEXR_inData
{
	AlphaMode	alpha_mode;
	A_Boolean	unmult;
	A_Boolean	memory_map;
	char		reserved[29]; // total of 32 bytes
} ProEXR_inData;

enum {
	WRITE_ALPHA_NONE = 1,
    WRITE_ALPHA_TRANSPARENCY,
    WRITE_ALPHA_CHANNEL
};
typedef A_u_char WriteAlphaMode;

typedef struct ProEXR_outData
{
	A_u_char		compression_type;
	A_Boolean		float_not_half;
	A_Boolean		luminance_chroma;
	WriteAlphaMode	alpha_mode;
	char			reserved[60]; // total of 64 bytes
} ProEXR_outData;
*/

typedef struct Globals
{ // This is our structure that we use to pass globals between routines:

	short				*result;			// Must always be first in Globals.
	FormatRecord		*formatParamBlock;	// Must always be second in Globals.

	//ProEXR_outData		options;
    //ProEXR_inData       in_options;
	
	//IStreamPS			*ps_in;
	//ProEXRdoc_readPS	*doc_in;
	
	bool				done_reg;
	
} Globals, *GPtr, **GHdl;				// *GPtr = global pointer; **GHdl = global handle


//-------------------------------------------------------------------------------
//	Globals -- definitions and macros
//-------------------------------------------------------------------------------

#define gResult				(*(globals->result))
#define gStuff				(globals->formatParamBlock)

#define gPixelBuffer		(globals->pixelBuffer)
#define gPixelData			(globals->pixelData)
#define gRowBytes			(globals->rowBytes)

#define gAliasHandle		(globals->aliasHandle)

#define gOptions			(globals->options)
#define gInOptions			(globals->in_options)

#define gDone_Reg			globals->done_reg

#ifndef MIN
	#define MIN(A,B)	( (A) < (B) ? (A) : (B) )
	#define MAX(A,B)	( (A) > (B) ? (A) : (B) )
#endif

//-------------------------------------------------------------------------------
//	Prototypes
//-------------------------------------------------------------------------------

#ifdef PS_CS4_SDK
typedef intptr_t entryData;
typedef void * allocateGlobalsPointer;
#else
typedef long entryData;
typedef uint32 allocateGlobalsPointer;
#endif

// Everything comes in and out of PluginMain. It must be first routine in source (really?):
DLLExport MACPASCAL void PluginMain (const short selector,
					  	             FormatRecord *formatParamBlock,
						             entryData *data,
						             short *result);

// dialogs

#ifdef __PIWin__
//bool ProEXR_Write_Dialog(HWND hwndOwner, ProEXR_outData *options, const char *alpha_name, bool have_transparency, bool greyscale);
//bool ProEXR_Read_Dialog(HWND hwndOwner, ProEXR_inData *in_options);
#else
//bool ProEXR_Write_Dialog(ProEXR_outData *options, const char *alpha_name, bool have_transparency, bool greyscale);
//bool ProEXR_Read_Dialog(ProEXR_inData *in_options);
#endif

// scripting calls
// During read phase:
//Boolean ReadScriptParamsOnRead (GPtr globals);
//OSErr WriteScriptParamsOnRead (GPtr globals);

// During write phase:
//Boolean ReadScriptParamsOnWrite (GPtr globals);
//OSErr WriteScriptParamsOnWrite (GPtr globals);


//-------------------------------------------------------------------------------

#endif // __ProEXR_Deep_H__
