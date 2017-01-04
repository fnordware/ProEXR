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

#ifndef __ProEXR_H__
#define __ProEXR_H__


// weird problem I'm having with A.h in Xcode
#ifdef __GNUC__
typedef unsigned long long	uint64_t;
#endif


#include "A.h"
#include "PIDefines.h"				// New in 7.0 SDK
#include "PIFormat.h"				// Format Photoshop header file.
#include "PIExport.h"				// Export Photoshop header file.
#include "PIUtilities.h"			// SDK Utility library.
#include "FileUtilities.h"

#include "ProEXR_PSIO.h"
#include "ProEXRdoc_PS.h"


enum {
    ALPHA_TRANSPARENCY = 1,
    ALPHA_SEPERATE
};
typedef A_u_char AlphaMode ;

typedef struct ProEXR_inData
{
	AlphaMode	alpha_mode;
	A_Boolean	unmult;
	A_Boolean	ignore_layertext;
	A_Boolean	memory_map;
	char		reserved[28]; // total of 32 bytes
} ProEXR_inData;


typedef struct ProEXR_outData
{
	A_u_char	compression_type;
	A_Boolean	float_not_half;
	A_Boolean	luminance_chroma;
	A_Boolean	layer_composite;
	A_Boolean	hidden_layers;
	char		reserved[59]; // total of 64 bytes
} ProEXR_outData;


typedef struct Globals
{ // This is our structure that we use to pass globals between routines:

	short				*result;			// Must always be first in Globals.
	FormatRecord		*formatParamBlock;	// Must always be second in Globals.

	ProEXR_outData		options;
    ProEXR_inData       in_options;
	
	IStreamPS			*ps_in;
	ProEXRdoc_readPS	*doc_in;
	
	int					total_layers; // for progress when writing

} Globals, *GPtr, **GHdl;				// *GPtr = global pointer; **GHdl = global handle


typedef struct ExGlobals
{ // This is our structure that we use to pass globals between routines:

	short					*result;				// Must always be first in Globals.
	ExportRecord			*exportParamBlock;		// Must always be second in Globals.


	// AliasHandle on Mac, Handle on Windows:
	PIPlatformFileHandle	aliasHandle;

	ProEXR_outData			options;

} ExGlobals, *ExGPtr, **ExGHdl;
	
//-------------------------------------------------------------------------------
//	Globals -- definitions and macros
//-------------------------------------------------------------------------------

#define gResult				(*(globals->result))
//#define gStuff				(globals->formatParamBlock)

#define gPixelBuffer		(globals->pixelBuffer)
#define gPixelData			(globals->pixelData)
#define gRowBytes			(globals->rowBytes)

#define gAliasHandle		(globals->aliasHandle)

#define gOptions			(globals->options)
#define gInOptions			(globals->in_options)


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

DLLExport MACPASCAL void ExPluginMain (const short selector,
						             ExportRecord *exportParamBlock,
						             entryData *data,
						             short *result);

// UI functions living in other files
#ifdef __PIWin__
bool ProEXR_Write_Dialog(HWND hwndOwner, ProEXR_outData *options, int layers, bool hidden_layers);
bool ProEXR_SaveAs(HWND hwndOwner, char *path, int buf_len);
void ProEXR_MakeAliasFromPath(HWND hwndOwner, char *path, PIPlatformFileHandle *aliasHandle);
bool ProEXR_Read_Dialog(HWND hwndOwner, ProEXR_inData *in_options);
#else
bool ProEXR_Write_Dialog(ProEXR_outData *options, int layers, bool hidden_layers);
bool ProEXR_SaveAs(char *path, int buf_len);
void ProEXR_MakeAliasFromPath(char *path, PIPlatformFileHandle *aliasHandle);
bool ProEXR_Read_Dialog(ProEXR_inData *in_options);
#endif



// During write phase:
Boolean ReadScriptParamsOnWrite (GPtr globals);	// Read any scripting params.
OSErr WriteScriptParamsOnWrite (GPtr globals);	// Write any scripting params.

Boolean ReadScriptParamsOnWrite (ExGPtr globals);	// Read any scripting params.
OSErr WriteScriptParamsOnWrite (ExGPtr globals);	// Write any scripting params.


//-------------------------------------------------------------------------------

#endif // __ProEXR_H__
