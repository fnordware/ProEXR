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

#import "ProEXR_AE_GUI_Controller.h"

#import <Foundation/NSPathUtilities.h>

@implementation ProEXR_AE_GUI_Controller

- (id)init:(NSString *)path
	compression:(NSInteger)compression
	useFloat:(BOOL)floatVal
	composite:(BOOL)compositeVal 
	hidden:(BOOL)hiddenVal
	timeSpan:(DialogTimeSpan)timeSpan
	startFrame:(NSInteger)startFrame
	endFrame:(NSInteger)endFrame
	currentFrame:(NSInteger)currentFrame
	workStart:(NSInteger)workStart
	workEnd:(NSInteger)workEnd
	compStart:(NSInteger)compStart
	compEnd:(NSInteger)compEnd
{
	self = [super init];
	
	if(!([NSBundle loadNibNamed:@"ProEXR_AE_GUI" owner:self]))
		return nil;
	
	if(self)
	{
		[pathField setStringValue:path];
	
		[compressionMenu removeAllItems];
		[compressionMenu addItemsWithTitles:
			[NSArray arrayWithObjects:@"None", @"RLE", @"Zip", @"Zip16", @"Piz", @"PXR24", @"B44", @"B44A", @"DWAA", @"DWAB", nil]];
		[compressionMenu selectItem:[compressionMenu itemAtIndex:compression]];
	
		[floatButton setState:(floatVal ? NSOnState : NSOffState)];
		[compositeButton setState:(compositeVal ? NSOnState : NSOffState)];
		[hiddenButton setState:(hiddenVal ? NSOnState : NSOffState)];
		
		[timeSpanMenu selectItemWithTag:timeSpan];
		
		current_frame = currentFrame;
		work_start = workStart;
		work_end = workEnd;
		comp_start = compStart;
		comp_end = compEnd;
		
		[[startField formatter] setMinimum:[NSNumber numberWithInteger:comp_start]];
		[[startField formatter] setMaximum:[NSNumber numberWithInteger:comp_end]];
		
		[[endField formatter] setMinimum:[NSNumber numberWithInteger:comp_start]];
		[[endField formatter] setMaximum:[NSNumber numberWithInteger:comp_end]];
		
		[self trackTimeFrame:nil];
		
		[[self window] center];
		
		if(![path isAbsolutePath])
			[self browsePath:nil];
	}
	
	return self;
}
	
- (IBAction)clickedRender:(id)sender
{
	[NSApp stopModal];
}

- (IBAction)clickedCancel:(id)sender
{
	[NSApp abortModal];
}

- (IBAction)browsePath:(id)sender
{
	NSString *path = [self getPath];
	
	NSString *directory = [path stringByDeletingPathExtension];
	NSString *file = [path lastPathComponent];
	
	if([file length] == 0)
	{
		file = [NSString stringWithString:@"MyComp.[####].exr"];
	}
	
	NSSavePanel *panel = [NSSavePanel savePanel];
	
	[panel setAllowedFileTypes:[NSArray arrayWithObject:[NSString stringWithString:@"exr"]]];

#if __LP64__
	[panel beginSheetForDirectory:directory
			file:file
			modalForWindow:[self window]
			modalDelegate:self
			didEndSelector:@selector(savePanelDidEnd:returnCode:contextInfo:)
			contextInfo:nil];
#else
	const int result = [panel runModalForDirectory:directory file:file];
	
	if(result == NSFileHandlingPanelOKButton)
	{
		[pathField setStringValue:[[panel URL] path]];
	}
#endif
}

#if __LP64__
- (void)savePanelDidEnd:(NSSavePanel *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
	if(returnCode == NSFileHandlingPanelOKButton)
	{
		[pathField setStringValue:[[sheet URL] path]];
	}
}
#endif

- (IBAction)trackTimeFrame:(id)sender
{
	const DialogTimeSpan timeSpan = [self getTimeSpan];
	
	if(timeSpan == DIALOG_TIMESPAN_CURRENT_FRAME)
	{
		[startField setIntegerValue:current_frame];
		[endField setIntegerValue:current_frame];
	}
	else if(timeSpan == DIALOG_TIMESPAN_WORK_AREA)
	{
		[startField setIntegerValue:work_start];
		[endField setIntegerValue:work_end];
	}
	else if(timeSpan == DIALOG_TIMESPAN_FULL_COMP)
	{
		[startField setIntegerValue:comp_start];
		[endField setIntegerValue:comp_end];
	}
	
	const BOOL fieldsEnabled = (timeSpan == DIALOG_TIMESPAN_CUSTOM);
	
	[startField setEnabled:fieldsEnabled];
	[endField setEnabled:fieldsEnabled];
}

- (NSInteger)getCompression
{
	return [compressionMenu indexOfSelectedItem];
}

- (BOOL)getFloat
{
	return ([floatButton state] == NSOnState);
}

- (BOOL)getComposite
{
	return ([compositeButton state] == NSOnState);
}

- (BOOL)getHidden
{
	return ([hiddenButton state] == NSOnState);
}

- (NSString *)getPath
{
	return [pathField stringValue];
}

- (DialogTimeSpan)getTimeSpan
{
	return [timeSpanMenu indexOfSelectedItem];
}

- (NSInteger)getStart
{
	return [startField integerValue];
}

- (NSInteger)getEnd
{
	return [endField integerValue];
}

@end
