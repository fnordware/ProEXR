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

#import "ProEXR_AE_Progress_Controller.h"


@implementation ProEXR_AE_Progress_Controller

- (id)init:(NSInteger)startFrame totalFrames:(NSInteger)totalFrames;
{
	self = [super init];
	
	if(!([NSBundle loadNibNamed:@"ProEXR_AE_Progress" owner:self]))
		return nil;
	
	if(self)
	{
		total_frames = totalFrames;
		
		userCancelled = NO;
		
		[self updateProgress:startFrame];
		
		[[self window] center];
	}
	
	return self;
}

- (IBAction)clickedCancel:(id)sender
{
	userCancelled = YES;
}

- (BOOL)updateProgress:(NSInteger)currentFrame
{
	NSAssert([progressBar minValue] == 0.0, @"Min value should be 0.0");

	[progressBar setDoubleValue:([progressBar maxValue] * ((double)(currentFrame - 1) / (double)total_frames))];
	
	[textProgress setStringValue:[NSString stringWithFormat:@"Rendering frame %ld of %ld", currentFrame, total_frames]];
	
	return !userCancelled;
}

@end
