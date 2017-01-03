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

@interface ProEXR_Out_Controller : NSObject {
    IBOutlet NSButton *compositeCheck;
    IBOutlet NSPopUpButton *compressionMenu;
    IBOutlet NSButton *floatCheck;
    IBOutlet NSButton *hiddenCheck;
    IBOutlet NSButton *lumiChromCheck;
    IBOutlet NSWindow *theWindow;
}
- (id)init:(int)compression lumiChrom:(BOOL)lumiChromVal useFloat:(BOOL)floatVal composite:(BOOL)compositeVal hidden:(BOOL)hiddenVal
				layers:(int)numLayers hiddenLayers:(BOOL)haveHiddenLayers;

- (IBAction)clickedCancel:(id)sender;
- (IBAction)clickedOK:(id)sender;
- (IBAction)trackLumiChrom:(id)sender;

- (NSInteger)getCompression;
- (BOOL)getLumiChrom;
- (BOOL)getFloat;
- (BOOL)getComposite;
- (BOOL)getHidden;

- (NSWindow *)getWindow;
@end
