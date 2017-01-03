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


#include "ProEXR.h"

#import "ProEXR_In_Prefs.h"
#import "ProEXR_In_Controller.h"

#import "ProEXR_Out_Controller.h"


// ==============
// Only compiled on 64-bit
// ==============
#if __LP64__


bool ProEXR_Write_Dialog(ProEXR_outData *options, int layers, bool hidden_layers)
{
	bool hit_ok = true;

	Class out_controller_class = [[NSBundle bundleWithIdentifier:@"com.fnordware.Photoshop.ProEXR"]
									classNamed:@"ProEXR_Out_Controller"];
	
	if(out_controller_class)
	{
		ProEXR_Out_Controller *out_controller = [[out_controller_class alloc]
													init:options->compression_type
													lumiChrom:options->luminance_chroma
													useFloat:options->float_not_half
													composite:options->layer_composite
													hidden:options->hidden_layers
													layers:layers
													hiddenLayers:hidden_layers
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
					options->float_not_half = [out_controller getFloat];
					options->luminance_chroma = [out_controller getLumiChrom];
					options->layer_composite = [out_controller getComposite];
					options->hidden_layers = [out_controller getHidden];
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

#pragma mark-

bool ProEXR_SaveAs(char *path, int buf_len)
{
	NSSavePanel *sp = [NSSavePanel savePanel];
	 
	[sp setTitle:@"ProEXR Layer Export"];
	[sp setMessage:@"Supply Base Filename for EXR Layer Files"];
	[sp setRequiredFileType:@"exr"];
	
	int result;
	
	if( strlen(path) )
	{
		std::string file_path(path);
		std::string dir_path = file_path.substr(0, file_path.find_last_of('/'));
		std::string filename = file_path.substr(file_path.find_last_of('/') + 1);
		
		result = [sp runModalForDirectory:[NSString stringWithUTF8String:dir_path.c_str()] file:[NSString stringWithUTF8String:filename.c_str()]];
	}
	else
		result = [sp runModalForDirectory:NSHomeDirectory() file:@"Untitled.exr"];
	 
	 
	if(result == NSOKButton)
	{
		return [[sp filename] getCString:path maxLength:buf_len encoding:NSASCIIStringEncoding];
	}
	else
		return false;
}


void ProEXR_MakeAliasFromPath(char *file_path, PIPlatformFileHandle *aliasHandle)
{
	FSRef temp_fsref;
	
	if(noErr == FSPathMakeRef((UInt8 *)file_path, &temp_fsref, NULL) )
	{
		FSNewAlias(NULL, &temp_fsref, aliasHandle);
	}
	else
	{
		// ack, make a new file, then an alias, then delete
		std::string path(file_path);
		std::string dir_path = path.substr(0, path.find_last_of('/'));
		std::string filename = path.substr(path.find_last_of('/') + 1);
		
		FSRef dir_ref;
		
		if(noErr == FSPathMakeRef((UInt8 *)dir_path.c_str(), &dir_ref, NULL) )
		{
			CFStringRef cf_file = CFStringCreateWithCString(kCFAllocatorDefault, filename.c_str(), kCFStringEncodingASCII );
			UniChar uni_file[255];
			
			CFStringGetCharacters(cf_file, CFRangeMake(0, CFStringGetLength(cf_file)), uni_file);
			
			FSRef file_ref;
			
			if(noErr == FSCreateFileUnicode(&dir_ref, CFStringGetLength(cf_file), uni_file,
												kFSCatInfoNone, NULL, &file_ref, NULL) )
			{
				FSNewAlias(NULL, &file_ref, aliasHandle);
				
				FSDeleteObject(&file_ref);
			}
		}
	}
}

#pragma mark-


bool ProEXR_Read_Dialog(ProEXR_inData *in_options)
{
	bool hit_ok = true;
	
    UInt32 keys = GetCurrentEventKeyModifiers();
	bool option_key = ( (keys & shiftKey) || (keys & rightShiftKey) || (keys & optionKey) || (keys & rightOptionKey) );
	
	ProEXR_In_Prefs *prefs_obj = [[ProEXR_In_Prefs alloc] init];
	
	if(prefs_obj && (option_key || [prefs_obj getAlwaysDialog]))
	{
		Class in_controller_class = [[NSBundle bundleWithIdentifier:@"com.fnordware.Photoshop.ProEXR"]
										classNamed:@"ProEXR_In_Controller"];
		
		if(in_controller_class)
		{
			ProEXR_Out_Controller *in_controller = [[in_controller_class alloc] init:prefs_obj];
			
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
		in_options->ignore_layertext = [prefs_obj getIgnoreLayerText];
		in_options->memory_map = [prefs_obj getMemoryMap];
						
		[prefs_obj release];
	}

	
	return hit_ok;
}


#endif //__LP64__
