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


#include "ProEXR_EZ.h"

#import "ProEXR_EZ_In_Prefs.h"
#import "ProEXR_EZ_In_Controller.h"

#import "ProEXR_EZ_Out_Controller.h"


// ==============
// Only compiled on 64-bit
// ==============
#if __LP64__


bool ProEXR_Write_Dialog(ProEXR_outData *options, const char *alpha_name, bool have_transparency, bool greyscale)
{
	bool hit_ok = true;

	Class out_controller_class = [[NSBundle bundleWithIdentifier:@"com.fnordware.Photoshop.ProEXR_EZ"]
									classNamed:@"ProEXR_EZ_Out_Controller"];
	
	if(out_controller_class)
	{
		ProEXR_EZ_Out_Controller *out_controller = [[out_controller_class alloc]
													init:options->compression_type
													lumiChrom:options->luminance_chroma
													useFloat:options->float_not_half
													alphaMode:(DialogAlphaMode)options->alpha_mode
													alphaName:alpha_name
													haveTransparency:have_transparency
													greyscale:greyscale
												];
		
		if(out_controller)
		{
			NSWindow *the_window = [out_controller getWindow];
			
			if(the_window)
			{
				NSInteger modal_result = [NSApp runModalForWindow:the_window];
				
				if(modal_result == NSRunStoppedResponse)
				{
					hit_ok = true;
					
					options->compression_type = [out_controller getCompression];
					options->luminance_chroma = [out_controller getLumiChrom];
					options->float_not_half = [out_controller getFloat];
					options->alpha_mode = [out_controller getAlphaMode];
				}
				else
					hit_ok = false;
				
				[the_window close];
			}
			
			[out_controller release];
		}
	}

	return hit_ok;
}


bool ProEXR_Read_Dialog(ProEXR_inData *in_options)
{
	bool hit_ok = true;
	
    UInt32 keys = GetCurrentEventKeyModifiers();
	bool option_key = ( (keys & shiftKey) || (keys & rightShiftKey) || (keys & optionKey) || (keys & rightOptionKey) );
	
	ProEXR_EZ_In_Prefs *prefs_obj = [[ProEXR_EZ_In_Prefs alloc] init];
	
	if(prefs_obj && (option_key || [prefs_obj getAlwaysDialog]))
	{
		Class in_controller_class = [[NSBundle bundleWithIdentifier:@"com.fnordware.Photoshop.ProEXR_EZ"]
										classNamed:@"ProEXR_EZ_In_Controller"];
		
		if(in_controller_class)
		{
			ProEXR_EZ_Out_Controller *in_controller = [[in_controller_class alloc] init:prefs_obj];
			
			if(in_controller)
			{
				NSWindow *the_window = [in_controller getWindow];
				
				if(the_window)
				{
					NSInteger modal_result = [NSApp runModalForWindow:the_window];
					
					if(modal_result == NSRunStoppedResponse)
					{
						hit_ok = true;
						
						[prefs_obj writeAlways];
					}
					else
						hit_ok = false;
					
					[the_window close];
				}
				
				[in_controller release];
			}
		}
	}
	
	if(prefs_obj)
	{			
		in_options->alpha_mode = [prefs_obj getAlphaMode];
		in_options->unmult = [prefs_obj getUnMult];
		in_options->memory_map = [prefs_obj getMemoryMap];
						
		[prefs_obj release];
	}

	
	return hit_ok;
}



#endif //__LP64__
