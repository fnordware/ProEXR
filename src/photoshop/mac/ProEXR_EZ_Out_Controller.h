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

typedef enum {
	DIALOG_WRITE_ALPHA_NONE = 1,
    DIALOG_WRITE_ALPHA_TRANSPARENCY,
    DIALOG_WRITE_ALPHA_CHANNEL
} DialogAlphaMode;


@interface ProEXR_EZ_Out_Controller : NSObject {
    IBOutlet NSMatrix *alphaRadioGroup;
    IBOutlet NSPopUpButton *compressionPulldown;
    IBOutlet NSButton *floatCheck;
    IBOutlet NSTextField *floatLabel;
    IBOutlet NSButton *lumiChromCheck;
    IBOutlet NSWindow *theWindow;
}
- (id)init:(int)compression lumiChrom:(BOOL)lumiChromVal useFloat:(BOOL)floatVal alphaMode:(DialogAlphaMode)alphaModeVal
		alphaName:(const char *)alphaNameVal haveTransparency:(BOOL)haveTrans greyscale:(BOOL)isGrey;

- (int)getCompression;
- (BOOL)getLumiChrom;
- (BOOL)getFloat;
- (DialogAlphaMode)getAlphaMode;

- (IBAction)clickedCancel:(id)sender;
- (IBAction)clickedOK:(id)sender;
- (IBAction)trackLumiChrom:(id)sender;

- (NSWindow *)getWindow;
@end
