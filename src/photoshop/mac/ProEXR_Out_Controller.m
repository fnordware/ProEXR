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

#import "ProEXR_Out_Controller.h"

@implementation ProEXR_Out_Controller

- (id)init:(int)compression lumiChrom:(BOOL)lumiChromVal useFloat:(BOOL)floatVal composite:(BOOL)compositeVal hidden:(BOOL)hiddenVal
				layers:(int)numLayers hiddenLayers:(BOOL)haveHiddenLayers
{
	self = [super init];
	
	if(!([NSBundle loadNibNamed:@"ProEXR_Out" owner:self]))
		return nil;
	
	[theWindow center];
	
	[compressionMenu removeAllItems];
	[compressionMenu addItemsWithTitles:
		[NSArray arrayWithObjects:@"None", @"RLE", @"Zip", @"Zip16", @"Piz", @"PXR24", @"B44", @"B44A", @"DWAA", @"DWAB", nil]];
	[compressionMenu selectItem:[compressionMenu itemAtIndex:compression]];
	
	[lumiChromCheck setState:(lumiChromVal ? NSOnState : NSOffState)];
	[floatCheck setState:(floatVal ? NSOnState : NSOffState)];
	[compositeCheck setState:(compositeVal ? NSOnState : NSOffState)];
	[hiddenCheck setState:(hiddenVal ? NSOnState : NSOffState)];
	
	if(numLayers > 1)
	{
		[lumiChromCheck setEnabled:FALSE];
	}
	else
	{
		[self trackLumiChrom:nil];
		[compositeCheck setEnabled:FALSE];
	}
	
	if(!haveHiddenLayers)
		[hiddenCheck setEnabled:FALSE];
	
	return self;
}

- (IBAction)clickedCancel:(id)sender {
    [NSApp abortModal];
}

- (IBAction)clickedOK:(id)sender {
    [NSApp stopModal];
}

- (IBAction)trackLumiChrom:(id)sender {
    [floatCheck setEnabled:([lumiChromCheck state] == NSOffState)];
}

- (NSInteger)getCompression {
	return [compressionMenu indexOfSelectedItem];
}

- (BOOL)getLumiChrom {
	return ([lumiChromCheck state] == NSOnState);
}

- (BOOL)getFloat {
	return ([floatCheck state] == NSOnState);
}

- (BOOL)getComposite {
	return ([compositeCheck state] == NSOnState);
}

- (BOOL)getHidden {
	return ([hiddenCheck state] == NSOnState);
}

- (NSWindow *)getWindow {
	return theWindow;
}

@end
