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
    PREFS_ALPHA_TRANSPARENCY = 1,
    PREFS_ALPHA_SEPARATE
} PrefsAlphaMode;


@interface ProEXR_In_Prefs : NSObject {
	PrefsAlphaMode alpha_mode;
	BOOL unmult;
	BOOL ignore_layertext;
	BOOL memory_map;
	BOOL always_dialog;
}

- (id)init;

- (PrefsAlphaMode)getAlphaMode;
- (BOOL)getUnMult;
- (BOOL)getIgnoreLayerText;
- (BOOL)getMemoryMap;
- (BOOL)getAlwaysDialog;

- (void)setAlphaMode:(PrefsAlphaMode)alphaMode;
- (void)setUnMult:(BOOL)un_mult;
- (void)setIgnoreLayerText:(BOOL)ignoreLayerText;
- (void)setMemoryMap:(BOOL)mem_map;
- (void)setAlwaysDialog:(BOOL)alwaysDialog;

- (void)readPrefs;
- (void)writePrefs;
- (void)writeAlways;

@end
