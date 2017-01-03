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

#import <Cocoa/Cocoa.h>

#if !NSINTEGER_DEFINED
typedef int NSInteger;
#define NSINTEGER_DEFINED 1
#endif

typedef enum {
	DIALOG_RESULT_CONTINUE = 0,
	DIALOG_RESULT_OK,
	DIALOG_RESULT_CANCEL
} DialogResult;

@interface ProEXR_AE_Out_Controller : NSObject {
    IBOutlet NSPopUpButton *compressionMenu;
    IBOutlet NSButton *floatCheck;
    IBOutlet NSButton *compositeCheck;
    IBOutlet NSButton *hiddenCheck;
    IBOutlet NSWindow *theWindow;
	
	DialogResult theResult;
}

- (id)init:(NSInteger)compression
	useFloat:(BOOL)floatVal
	composite:(BOOL)compositeVal 
	hidden:(BOOL)hiddenVal;
				
- (IBAction)clickedCancel:(id)sender;
- (IBAction)clickedOK:(id)sender;
- (DialogResult)getResult;

- (NSInteger)getCompression;
- (BOOL)getFloat;
- (BOOL)getComposite;
- (BOOL)getHidden;

- (NSWindow *)getWindow;

@end
