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

// exrdisplay
//
// AE plug-in to replicate the exrdisplay controls
//

#ifndef EXRDISPLAY_H
#define EXRDISPLAY_H

#pragma once 

#include "AEConfig.h"
#include "Entry.h"
#include "AE_EffectUI.h"
#include "AE_EffectCBSuites.h"
#include "AE_AdvEffectSuites.h"
#include "String_Utils.h"
#include "Param_Utils.h"
#include "AE_Macros.h"
#include "fnord_SuiteHandler.h"


#ifdef macintosh
#define Macintosh
#endif


#ifdef MSWindows
	#include <windows.h>
#endif


#define NAME				"exrdisplay"
#define DESCRIPTION			"exrdisplay emulation."
#define RELEASE_DATE		__DATE__
#define COPYRIGHT           "\xA9 2008 fnord"
#define WEBSITE				"www.fnordware.com"
#define	MAJOR_VERSION		1
#define	MINOR_VERSION		0
#define	BUG_VERSION			0
#define	STAGE_VERSION		PF_Stage_RELEASE
#define	BUILD_VERSION		0


#if PF_AE_PLUG_IN_VERSION < PF_AE100_PLUG_IN_VERSION
typedef A_long	IteratorRefcon;
#else
typedef void *	IteratorRefcon;
#endif

/*
#ifndef Macintosh
typedef unsigned char                   UInt8;
typedef signed char                     SInt8;
typedef unsigned short                  UInt16;
typedef signed short                    SInt16;
typedef unsigned long                   UInt32;
typedef signed long                     SInt32;
typedef char *							Ptr;
#endif
*/



enum {
	EXRDISPLAY_INPUT = 0,
    EXRDISPLAY_GAMMA,
    EXRDISPLAY_EXPOSURE,
    EXRDISPLAY_DEFOG,
    EXRDISPLAY_KNEELOW,
    EXRDISPLAY_KNEEHIGH,
    EXRDISPLAY_INVERSE,
    EXRDISPLAY_DITHER,
	EXRDISPLAY_NUM_PARAMS
};

enum {
	GAMMA_ID = 1,
	EXPOSURE_ID,
	DEFOG_ID,
	KNEELOW_ID,
    KNEEHIGH_ID,
    DITHER_ID,
    INVERSE_ID
};


#ifndef MAX
	#define MAX(A,B)			((A) > (B) ? (A) : (B))
#endif	


#ifdef __cplusplus
	extern "C" {
#endif



DllExport PF_Err PluginMain (	PF_Cmd			cmd,
                                PF_InData		*in_data,
                                PF_OutData		*out_data,
                                PF_ParamDef		*params[],
                                PF_LayerDef		*output,
                                void			*extra );


#ifdef __cplusplus
	}
#endif


#endif  // EXRDISPLAY_H
