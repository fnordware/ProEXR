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

#include "AEConfig.h"

#ifdef USE_AE_EFFECT_VERS
	#include "AE_EffectVers.h"
#else
	#define PF_PLUG_IN_VERSION			12
	#define PF_PLUG_IN_SUBVERS			12
#endif

#ifndef AE_OS_WIN
	#include "AE_General.r"
#endif

resource 'PiPL' (16000) {
	{	/* array properties: 12 elements */
		/* [1] */
		Kind {
			AEEffect
		},
		/* [2] */
		Name {
			"NaNny"
		},
		/* [3] */
		Category {
            "Utility"
		},
		
#ifdef AE_OS_WIN
	#ifdef AE_PROC_INTELx64
		CodeWin64X86 {"PluginMain"},
	#else
		CodeWin32X86 {"PluginMain"},
	#endif	
#else
	#ifdef AE_PROC_INTELx64
		CodeMacIntel64 {"PluginMain"},
	#else
		CodeMachOPowerPC {"PluginMain"},
		CodeMacIntel32 {"PluginMain"},
	#endif
#endif		/* [6] */
		AE_PiPL_Version {
			2,
			0
		},
		/* [7] */
		AE_Effect_Spec_Version {
			PF_PLUG_IN_VERSION,
			PF_PLUG_IN_SUBVERS
		},
		/* [8] */
		AE_Effect_Version {
			198144 /* 2.0 */
		},
		/* [9] */
		AE_Effect_Info_Flags {
			0
		},
		/* [10] */
		AE_Effect_Global_OutFlags {
			33555520
		},
		AE_Effect_Global_OutFlags_2 {
			5120
		},
		/* [11] */
		AE_Effect_Match_Name {
			"NaNny"
		},
		/* [12] */
		AE_Reserved_Info {
			0
		}
	}
};



#ifdef AE_OS_MAC

#include "MacTypes.r"

#define NAME				"NaNny"
#define VERSION_STRING		"0.5"
resource 'vers' (1, NAME " Version", purgeable)
{
	5, 0x50, final, 0, verUs,
	VERSION_STRING,
	VERSION_STRING
	"\n\xA9 2011 fnord"
};

resource 'vers' (2, NAME " Version", purgeable)
{
	5, 0x50, final, 0, verUs,
	VERSION_STRING,
	"by Brendan Bolles"
};

#endif

