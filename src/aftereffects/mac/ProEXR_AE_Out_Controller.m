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

#import "ProEXR_AE_Out_Controller.h"

@implementation ProEXR_AE_Out_Controller

- (id)init:(NSInteger)compression
	useFloat:(BOOL)floatVal
	composite:(BOOL)compositeVal 
	hidden:(BOOL)hiddenVal
{
	self = [super init];
	
	if(!([NSBundle loadNibNamed:@"ProEXR_AE_Out" owner:self]))
		return nil;
	
	[theWindow center];
	
	[compressionMenu removeAllItems];
	[compressionMenu addItemsWithTitles:
		[NSArray arrayWithObjects:@"None", @"RLE", @"Zip", @"Zip16", @"Piz", @"PXR24", @"B44", @"B44A", @"DWAA", @"DWAB", nil]];
	[compressionMenu selectItem:[compressionMenu itemAtIndex:compression]];
	
	[floatCheck setState:(floatVal ? NSOnState : NSOffState)];
	[compositeCheck setState:(compositeVal ? NSOnState : NSOffState)];
	[hiddenCheck setState:(hiddenVal ? NSOnState : NSOffState)];
	
	theResult = DIALOG_RESULT_CONTINUE;
		
	return self;
}

- (IBAction)clickedCancel:(id)sender {
    theResult = DIALOG_RESULT_CANCEL;
}

- (IBAction)clickedOK:(id)sender {
    theResult = DIALOG_RESULT_OK;
}

- (DialogResult)getResult {
	return theResult;
}

- (NSInteger)getCompression {
	return [compressionMenu indexOfSelectedItem];
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
