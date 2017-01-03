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

#pragma once 


#ifdef MSWindows
#include <windows.h>
#endif

#include "AEConfig.h"
#include "entry.h"
#include "AE_IO.h"
#include "AE_Macros.h"
#include "AE_EffectCBSuites.h"
#include "fnord_SuiteHandler.h"

#ifdef __cplusplus
extern "C" {
#endif


DllExport A_Err
VRimg_IO(
	struct SPBasicSuite		*pica_basicP,			/* >> */
	A_long				 	major_versionL,			/* >> */		
	A_long					minor_versionL,			/* >> */		
#if PF_AE_PLUG_IN_VERSION < PF_AE100_PLUG_IN_VERSION
	const A_char		 	*file_pathZ,				/* >> platform-specific delimiters */
	const A_char		 	*res_pathZ,				/* >> platform-specific delimiters */
#endif
	AEGP_PluginID			aegp_plugin_id,			/* >> */
	void					*global_refconPV);			/* << */


#ifdef __cplusplus
}
#endif

