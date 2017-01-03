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

#import "ProEXR_EZ_In_Controller.h"

@implementation ProEXR_EZ_In_Controller

- (id)init:(ProEXR_EZ_In_Prefs *)prefs {

	if(prefs == NULL)
		return nil;
	
	self = [super init];
	
	if(!([NSBundle loadNibNamed:@"ProEXR_EZ_In" owner:self]))
		return nil;
		
	[theWindow center];
	
	[alphaModeRadioGroup selectCellAtRow:([prefs getAlphaMode] - 1) column:0];
	[unMultCheck setState:([prefs getUnMult] ? NSOnState : NSOffState)];
	[memoryMapCheck setState:([prefs getMemoryMap] ? NSOnState : NSOffState)];
	[alwaysDialogCheck setState:([prefs getAlwaysDialog] ? NSOnState : NSOffState)];
	
	[self trackAlphaMode:nil];
	
	prefs_obj = prefs;
	
	return self;
}

- (IBAction)clickedCancel:(id)sender {
    [NSApp abortModal];
}

- (IBAction)clickedOK:(id)sender {
    [prefs_obj setAlphaMode:([alphaModeRadioGroup selectedRow] == 1 ? PREFS_ALPHA_SEPARATE : PREFS_ALPHA_TRANSPARENCY)];
	[prefs_obj setUnMult:([unMultCheck state] == NSOnState)];
	[prefs_obj setMemoryMap:([memoryMapCheck state] == NSOnState)];
	[prefs_obj setAlwaysDialog:([alwaysDialogCheck state] == NSOnState)];

    [NSApp stopModal];
}

- (IBAction)setDefaults:(id)sender {
    [prefs_obj setAlphaMode:([alphaModeRadioGroup selectedRow] == 1 ? PREFS_ALPHA_SEPARATE : PREFS_ALPHA_TRANSPARENCY)];
	[prefs_obj setUnMult:([unMultCheck state] == NSOnState)];
	[prefs_obj setMemoryMap:([memoryMapCheck state] == NSOnState)];
	[prefs_obj setAlwaysDialog:([alwaysDialogCheck state] == NSOnState)];
	
	[prefs_obj writePrefs];
}

- (IBAction)trackAlphaMode:(id)sender {
    [unMultCheck setEnabled:([alphaModeRadioGroup selectedRow] == 1)];
}

- (NSWindow *)getWindow {
	return theWindow;
}

@end
