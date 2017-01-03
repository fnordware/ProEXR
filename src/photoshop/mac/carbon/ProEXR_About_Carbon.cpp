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


#include "ProEXR_About.h"

#include <Carbon.h>

enum {
	DIALOG_ID = 16100,
	ABOUT_ID,
	REG_ID,
    IMPORT_ID
};


enum {
	ABOUT_OK = 1,
	ABOUT_NAME,
	ABOUT_BY,
	ABOUT_WEBSITE,
	ABOUT_VERSION,
	ABOUT_COPYRIGHT1
};

// ==============
// Only compiled on 32-bit
// ==============
#if !__LP64__


void ProEXR_About(void)
{
	DialogRef dp = GetNewDialog (ABOUT_ID, nil, (WindowPtr) -1); // instantiate the dialog
	
	if(dp)
	{
		ControlRef	name_ctl = NULL,
					by_ctl = NULL,
					website_ctl = NULL,
					version_ctl = NULL,
					copyright1_ctl = NULL;
		
		GetDialogItemAsControl(dp, ABOUT_NAME, &name_ctl);
		GetDialogItemAsControl(dp, ABOUT_BY, &by_ctl);
		GetDialogItemAsControl(dp, ABOUT_WEBSITE, &website_ctl);
		GetDialogItemAsControl(dp, ABOUT_VERSION, &version_ctl);
		GetDialogItemAsControl(dp, ABOUT_COPYRIGHT1, &copyright1_ctl);
		
		// justify copyright
		ControlFontStyleRec normal_style = {kControlUseJustMask, 0, 0, 0, 0, teCenter, {0,0,0}, {0,0,0} };
		ControlFontStyleRec title_style = {kControlUseJustMask | kControlUseFaceMask, 0, 0, 1, 0, teCenter, {0,0,0}, {0,0,0} };
		ControlFontStyleRec version_style = {kControlUseJustMask | kControlUseSizeMask, 0, 11, 0, 0, teCenter, {0,0,0}, {0,0,0} };
		ControlFontStyleRec copyright_style = {kControlUseJustMask | kControlUseSizeMask, 0, 9, 0, 0, teCenter, {0,0,0}, {0,0,0} };

		SetControlFontStyle(name_ctl, &title_style);
		SetControlFontStyle(by_ctl, &version_style);
		SetControlFontStyle(website_ctl, &version_style);
		SetControlFontStyle(version_ctl, &version_style);
		SetControlFontStyle(copyright1_ctl, &copyright_style);
		
	
		ShowWindow((WindowRef)dp);
		DrawDialog(dp);
		
		short item;
		do{
			ModalDialog(NULL, &item);
		}while(item > ABOUT_OK);

		DisposeDialog(dp);
	}
}


#endif //!__LP64__
