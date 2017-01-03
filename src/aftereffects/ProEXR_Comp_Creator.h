
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

#ifndef PROEXR_COMP_CREATOR_H
#define PROEXR_COMP_CREATOR_H


#pragma once

#ifdef MSWindows
	#include <windows.h>
	#include <io.h>
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "stdlib.h"

#include "AEConfig.h"
#include "entry.h"
#include "AE_GeneralPlug.h"
#include "A.h"
#include "AE_EffectUI.h"

#include "SPSuites.h"
#include "AE_AdvEffectSuites.h"
#include "AE_EffectCBSuites.h"
#include "AE_Macros.h"
#include "fnord_SuiteHandler.h"


#define COMP_CREATOR_MENU_STR		"Create ProEXR Layer Comps"
#define COMP_CREATOR_ALT_MENU_STR	"ProEXR DisplayWindow Comp"

A_Err
GPCommandHook(
	AEGP_GlobalRefcon	plugin_refconPV,
	AEGP_CommandRefcon	refconPV,
	AEGP_Command		command,
	AEGP_HookPriority	hook_priority,
	A_Boolean			already_handledB,
	A_Boolean			*handledPB);
	
A_Err
UpdateMenuHook(
	AEGP_GlobalRefcon		plugin_refconPV,
	AEGP_UpdateMenuRefcon	refconPV,
	AEGP_WindowType			active_window);

void GetControlKeys(SPBasicSuite *pica_basicP);


#endif // PROEXR_COMP_CREATOR_H
