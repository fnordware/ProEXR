
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


#include "ProEXR_AE_Dialogs.h"

#import <Cocoa/Cocoa.h>

#import "ProEXR_AE_Out_Controller.h"

#import "ProEXR_AE_GUI_Controller.h"

#import "ProEXR_AE_Progress_Controller.h"

#include <string>

using namespace std;


bool
ProEXR_AE_Out(
	ProEXR_AE_Out_Data	*params,
	const void		*plugHndl,
	const void		*mwnd)
{
	bool result = false;
	
	NSApplicationLoad();
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	NSString *bundle_id = [NSString stringWithUTF8String:(const char *)plugHndl];
	
	Class ui_controller_class = [[NSBundle bundleWithIdentifier:bundle_id]
									classNamed:@"ProEXR_AE_Out_Controller"];
	
	if(ui_controller_class)
	{
		ProEXR_AE_Out_Controller *ui_controller = [[ui_controller_class alloc] init:params->compression
																				useFloat:params->float_not_half
																				composite:params->layer_composite
																				hidden:params->hidden_layers];
		
		if(ui_controller)
		{
			
			NSWindow *my_window = [ui_controller getWindow];
							
			if(my_window)
			{
				[my_window makeKeyAndOrderFront:nil];
				
				NSInteger modal_result;
				DialogResult dialog_result;

				// because we're running a modal on top of a modal, we have to do our own							
				// modal loop that we can exit without calling [NSApp endModal], which will also							
				// kill AE's modal dialog.
				NSModalSession modal_session = [NSApp beginModalSessionForWindow:my_window];
				
				do{
					modal_result = [NSApp runModalSession:modal_session];

					dialog_result = [ui_controller getResult];
				}
				while(dialog_result == DIALOG_RESULT_CONTINUE && modal_result == NSRunContinuesResponse);
				
				[NSApp endModalSession:modal_session];

				if(dialog_result == DIALOG_RESULT_OK || modal_result == NSRunStoppedResponse)
				{
					params->compression = [ui_controller getCompression];
					params->float_not_half = [ui_controller getFloat];
					params->layer_composite = [ui_controller getComposite];
					params->hidden_layers = [ui_controller getHidden];
					
					result = true;
				}

				[my_window close];
			}
			
			[ui_controller release];
		}
	}
	
	if(pool)
		[pool release];
	
	return result;
}


bool
ProEXR_AE_GUI(
	ProEXR_AE_GUI_Data	*params,
	std::string		&path,
	int				current_frame,
	int				work_start,
	int				work_end,
	int				comp_start,
	int				comp_end,
	const void		*plugHndl,
	const void		*mwnd)
{
	bool result = false;
	
	NSApplicationLoad();
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	ProEXR_AE_GUI_Controller *controller = [[ProEXR_AE_GUI_Controller alloc]
												init:[NSString stringWithUTF8String:path.c_str()]
												compression:params->compression
												useFloat:params->float_not_half
												composite:params->layer_composite 
												hidden:params->hidden_layers
												timeSpan:(DialogTimeSpan)params->timeSpan
												startFrame:params->start_frame
												endFrame:params->end_frame
												currentFrame:current_frame
												workStart:work_start
												workEnd:work_end
												compStart:comp_start
												compEnd:comp_end
											];
	if(controller)
	{
		NSWindow *the_window = [controller window];
		
		if(the_window)
		{
			NSInteger modal_result = [NSApp runModalForWindow:the_window];
			
			if(modal_result == NSRunStoppedResponse)
			{
				path = [[controller getPath] UTF8String];
				
				params->compression = [controller getCompression];
				params->float_not_half = [controller getFloat];
				params->layer_composite = [controller getComposite];
				params->hidden_layers = [controller getHidden];
				params->timeSpan = (TimeSpan)[controller getTimeSpan];
				params->start_frame = [controller getStart];
				params->end_frame = [controller getEnd];
			
				result = true;
			}
			else
				result = false;
			
			[the_window close];
		}
		
		[controller release];
	}
	
	if(pool)
		[pool release];
	
	return result;
}


static ProEXR_AE_Progress_Controller *g_progressController = nil;
static NSModalSession g_modal_session = NULL;

bool
ProEXR_AE_Update_Progress(
	int				current_frame,
	int				total_frames,
	const void		*plugHndl,
	const void		*mwnd)
{
	if(g_progressController == nil)
	{
		g_progressController = [[ProEXR_AE_Progress_Controller alloc] init:current_frame totalFrames:total_frames];
		
		if(g_progressController)
		{
			assert(g_modal_session == NULL);
		
			g_modal_session = [NSApp beginModalSessionForWindow:[g_progressController window]];
		}
		else
			return false;
	}
	
	if(g_progressController)
	{
		BOOL continueProgress = [g_progressController updateProgress:current_frame];
	
		NSInteger modal_result = [NSApp runModalSession:g_modal_session];
		
		return (continueProgress && modal_result == NSRunContinuesResponse);
	}
	else
		return false;
}


void
ProEXR_AE_End_Progress()
{
	if(g_modal_session != NULL)
	{
		[NSApp endModalSession:g_modal_session];
		
		g_modal_session = NULL;
	}
	
	if(g_progressController != nil)
	{
		[g_progressController close];
		
		[g_progressController release];
		
		g_progressController = nil;
	}
}


void
ProEXR_CopyPluginPath(
	char				*pathZ,
	int					max_len)
{
	CFBundleRef bundle_ref = CFBundleGetBundleWithIdentifier(CFSTR("com.fnordware.AfterEffects.ProEXR_AE"));
	CFRetain(bundle_ref);
	
	CFURLRef plug_in_url = CFBundleCopyBundleURL(bundle_ref);
	
	Boolean got_path = CFURLGetFileSystemRepresentation(plug_in_url, TRUE, (UInt8 *)pathZ, max_len);
	
	CFRelease(plug_in_url);
	CFRelease(bundle_ref);
}
