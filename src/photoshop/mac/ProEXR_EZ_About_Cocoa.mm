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

#include "ProEXR_EZ_About_Controller.h"

#include "ProEXR_Version.h"


// ==============
// Only compiled on 64-bit
// ==============
#if __LP64__

void ProEXR_About(void)
{
	Class about_controller_class = [[NSBundle bundleWithIdentifier:@"com.fnordware.Photoshop.ProEXR_EZ"]
									classNamed:@"ProEXR_EZ_About_Controller"];
	
	if(about_controller_class)
	{
		ProEXR_EZ_About_Controller *about_controller = [[about_controller_class alloc]
														init:@ProEXR_Version_String " - " ProEXR_Build_Date];
		
		if(about_controller)
		{
			NSWindow *the_window = [about_controller getWindow];
			
			if(the_window)
			{
				[NSApp runModalForWindow:the_window];
				
				[the_window close];
			}
			
			[about_controller release];
		}
	}
}


#endif //__LP64__
