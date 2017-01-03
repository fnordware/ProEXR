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

#ifndef __VRIMG_H__
#define __VRIMG_H__


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
#include "VRimgInputFile.h"
#include <vector>
#include <string>




typedef struct Globals
{ // This is our structure that we use to pass globals between routines:

	short						*result;			// Must always be first in Globals.
	FormatRecord				*formatParamBlock;	// Must always be second in Globals.

	IStreamPS					*ps_in;
	VRimg::InputFile			*doc_in;
	std::vector<std::string>	*layer_list;
	
	bool						done_reg;
	
	BufferID					blackID;
	Ptr							blackBuf;
	int							blackHeight;
	size_t						blackRowbytes;

	BufferID					whiteID;
	Ptr							whiteBuf;
	int							whiteHeight;
	size_t						whiteRowbytes;

} Globals, *GPtr, **GHdl;				// *GPtr = global pointer; **GHdl = global handle

	
//-------------------------------------------------------------------------------
//	Globals -- definitions and macros
//-------------------------------------------------------------------------------

#define gResult				(*(globals->result))
//#define gStuff				(globals->formatParamBlock)

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
DLLExport MACPASCAL void VRimgMain (const short selector,
					  	             FormatRecord *formatParamBlock,
						             entryData *data,
						             short *result);


#endif // __VRIMG_H__
