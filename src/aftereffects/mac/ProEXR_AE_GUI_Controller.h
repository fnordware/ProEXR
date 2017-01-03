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
	DIALOG_TIMESPAN_CURRENT_FRAME = 0,
	DIALOG_TIMESPAN_WORK_AREA,
	DIALOG_TIMESPAN_FULL_COMP,
	DIALOG_TIMESPAN_CUSTOM
} DialogTimeSpan;

@interface ProEXR_AE_GUI_Controller : NSWindowController
{
	IBOutlet NSTextField *pathField;
	IBOutlet NSPopUpButton *timeSpanMenu;
	IBOutlet NSTextField *startField;
	IBOutlet NSTextField *startLabel;
	IBOutlet NSTextField *endField;
	IBOutlet NSTextField *endLabel;
	IBOutlet NSPopUpButton *compressionMenu;
	IBOutlet NSButton *floatButton;
	IBOutlet NSButton *compositeButton;
	IBOutlet NSButton *hiddenButton;
	
	NSInteger current_frame;
	NSInteger work_start;
	NSInteger work_end;
	NSInteger comp_start;
	NSInteger comp_end;
}

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
	compEnd:(NSInteger)compEnd;
	

- (IBAction)clickedRender:(id)sender;
- (IBAction)clickedCancel:(id)sender;

- (IBAction)browsePath:(id)sender;

- (IBAction)trackTimeFrame:(id)sender;

- (NSInteger)getCompression;
- (BOOL)getFloat;
- (BOOL)getComposite;
- (BOOL)getHidden;
- (NSString *)getPath;
- (DialogTimeSpan)getTimeSpan;
- (NSInteger)getStart;
- (NSInteger)getEnd;

@end
