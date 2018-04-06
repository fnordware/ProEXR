//
//	Cryptomatte AE plug-in
//		by Brendan Bolles <brendan@fnordware.com>
//
//
//	Part of ProEXR
//		http://www.fnordware.com/ProEXR
//
//

#include "Cryptomatte_AE_Dialog.h"

#import "Cryptomatte_AE_Dialog_Controller.h"

bool Cryptomatte_Dialog(
	std::string			&layer,
	std::string			&selection,
	std::string			&manifest,
	const char			*plugHndl,
	const void			*mwnd)
{
	bool clicked_ok = false;

	NSApplicationLoad();
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	NSString *bundle_id = [NSString stringWithUTF8String:plugHndl];

	Class ui_controller_class = [[NSBundle bundleWithIdentifier:bundle_id]
									classNamed:@"Cryptomatte_AE_Dialog_Controller"];

	if(ui_controller_class)
	{
		Cryptomatte_AE_Dialog_Controller *ui_controller = [[ui_controller_class alloc]
															init:[NSString stringWithUTF8String:layer.c_str()]
															selection:[NSString stringWithUTF8String:selection.c_str()]
															manifest:[NSString stringWithUTF8String:manifest.c_str()]];
		if(ui_controller)
		{
			NSWindow *my_window = [ui_controller window];
			
			if(my_window)
			{
				NSInteger result = [NSApp runModalForWindow:my_window];
				
				if(result == NSRunStoppedResponse)
				{
					layer = [[ui_controller getLayer] UTF8String];
					selection = [[ui_controller getSelection] UTF8String];
					manifest = [[ui_controller getManifest] UTF8String];
					
					clicked_ok = true;
				}
				
				[my_window release];
			}
			
			[ui_controller release];
		}
	}
	
	if(pool)
		[pool release];

	return clicked_ok;
}


void SetMickeyCursor()
{
	[[NSCursor pointingHandCursor] set];
}
