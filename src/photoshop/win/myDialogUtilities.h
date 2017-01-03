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


#ifndef __MyDialogUtilities_H__ 	// Has this not been defined yet?
#define __MyDialogUtilities_H__ 	// Only include this once by predefining it

#include "PIDefines.h"

#ifdef __PIWin__ // Windows only
	#include <limits.h>
	#include <stdlib.h>
	#include <string.h>
	#include <stdio.h>
	#include <windows.h>
	#include <windowsx.h>
	#include <errno.h>

	typedef HWND DialogPtr;
	typedef HWND DialogTHndl;
#else // Macintosh and others
	#include <Dialogs.h>			// Macintosh standard dialogs
	#include <Types.h>				// Macintosh standard types
	#include <Gestalt.h>			// Macintosh gestalt routines
	#include <TextUtils.h>			// Macintosh text utilities (GetString())
	#include <OSUtils.h>			// Macintosh OS Utilities (Delay())
	#include <Menus.h>				// Macintosh Menu Manager routines (DelMenuItem())
#endif

#include "PITypes.h"			// Photoshop types
#include "PIGeneral.h"			// Photoshop general routines
#include "PIAbout.h"
#include "PIUtilities.h"		// SDK Utility routines
#include "PIUSuites.h"			// AutoSuite definition
#include "PIGetFileListSuite.h" // URL browser function
#include "PIUIHooksSuite.h"		// idle event processing function
#include <stdio.h>

//-------------------------------------------------------------------------------
// C++ wrapper
//-------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

//-------------------------------------------------------------------------------
//	Prototypes -- Alerts
//-------------------------------------------------------------------------------

// Display a cross-platform alert with a version number:
short ShowVersionAlert (DialogPtr dp,
						const short alertID, 
						const short stringID,
						Str255 versText1,
						Str255 versText2);
#endif