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

#import "ProEXR_EZ_Out_Controller.h"

@implementation ProEXR_EZ_Out_Controller

- (id)init:(int)compression lumiChrom:(BOOL)lumiChromVal useFloat:(BOOL)floatVal alphaMode:(DialogAlphaMode)alphaModeVal
		alphaName:(const char *)alphaNameVal haveTransparency:(BOOL)haveTrans greyscale:(BOOL)isGrey
{
	self = [super init];
	
	if(!([NSBundle loadNibNamed:@"ProEXR_EZ_Out" owner:self]))
		return nil;
		
	[theWindow center];
	
	[compressionPulldown removeAllItems];
	[compressionPulldown addItemsWithTitles:
		[NSArray arrayWithObjects:@"None", @"RLE", @"Zip", @"Zip16", @"Piz", @"PXR24", @"B44", @"B44A", @"DWAA", @"DWAB", nil]];
	[compressionPulldown selectItem:[compressionPulldown itemAtIndex:compression]];
	
	[lumiChromCheck setState:(lumiChromVal ? NSOnState : NSOffState)];
	
	if(isGrey)
		[lumiChromCheck setEnabled:FALSE];
	else
		[self trackLumiChrom:nil];
	
	[floatCheck setState:(floatVal ? NSOnState : NSOffState)];
	
	if(!haveTrans)
	{
		if(alphaModeVal == DIALOG_WRITE_ALPHA_TRANSPARENCY)
			alphaModeVal = (alphaNameVal != NULL ? DIALOG_WRITE_ALPHA_CHANNEL : DIALOG_WRITE_ALPHA_NONE);
			
		[[alphaRadioGroup cellAtRow:1 column:0] setEnabled:FALSE];
	}

	if(alphaNameVal == NULL)
	{
		if(alphaModeVal == DIALOG_WRITE_ALPHA_CHANNEL)
			alphaModeVal = (haveTrans ? DIALOG_WRITE_ALPHA_TRANSPARENCY : DIALOG_WRITE_ALPHA_NONE);
			
		[[alphaRadioGroup cellAtRow:2 column:0] setEnabled:FALSE];
	}
	else
	{
		[[alphaRadioGroup cellAtRow:2 column:0] setTitle:[NSString stringWithFormat:@"Channel \"%s\"", alphaNameVal]];
	}
	
	[alphaRadioGroup selectCellAtRow:(alphaModeVal - 1) column:0];
	
	return self;
}

- (int)getCompression {
	return [compressionPulldown indexOfSelectedItem];
}

- (BOOL)getLumiChrom {
	return ([lumiChromCheck state] == NSOnState);
}

- (BOOL)getFloat {
	return ([floatCheck state] == NSOnState);
}

- (DialogAlphaMode)getAlphaMode {
	return ([alphaRadioGroup selectedRow] + 1);
}

- (IBAction)clickedCancel:(id)sender {
    [NSApp abortModal];
}

- (IBAction)clickedOK:(id)sender {
    [NSApp stopModal];
}

- (IBAction)trackLumiChrom:(id)sender {
	BOOL enabled =  ([lumiChromCheck state] == NSOffState);
	NSColor *label_color = (enabled ? [NSColor textColor] : [NSColor disabledControlTextColor]);
	
    [floatCheck setEnabled:enabled];
	[floatLabel setTextColor:label_color];
	
	[label_color release];
}

- (NSWindow *)getWindow {
	return theWindow;
}

@end
