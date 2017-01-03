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

// NaNny
//
// AE plug-in to kill NaN and inf floating point values
//

#ifndef NANNY_H
#define NANNY_H

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


#define NAME				"NaNny"
#define DESCRIPTION			"Deal with evil floating point values NaN and inf."
#define RELEASE_DATE		__DATE__
#define COPYRIGHT           "\xA9 2011-2012 fnord"
#define WEBSITE				"www.fnordware.com"
#define	MAJOR_VERSION		0
#define	MINOR_VERSION		6
#define	BUG_VERSION			0
#define	STAGE_VERSION		PF_Stage_RELEASE
#define	BUILD_VERSION		0


#if PF_AE_PLUG_IN_VERSION < PF_AE100_PLUG_IN_VERSION
typedef A_long	IteratorRefcon;
#else
typedef void *	IteratorRefcon;
#endif



enum {
	NANNY_INPUT = 0,
    NANNY_MODE,
    NANNY_NAN_VALUE,
    NANNY_INF_VALUE,
	NANNY_NIN_VALUE,
	NANNY_NUM_PARAMS
};

enum {
	MODE_ID = 1,
	NAN_ID,
	INF_ID,
	NIN_ID
};

enum {
	MODE_ALL = 1,
	MODE_NAN,
	MODE_INF,
	MODE_NIN,
	MODE_DIVIDER,
	MODE_DIAGNOSTIC,
	//MODE_MAKE_NAN,
	MODE_NUM_OPTIONS = MODE_DIAGNOSTIC
};
#define MODE_MENU_STRING	"All|NaN|inf|-inf|(-|Diagnostic"


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


#endif  // NANNY_H
