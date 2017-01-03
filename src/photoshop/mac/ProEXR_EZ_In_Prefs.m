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


#import "ProEXR_EZ_In_Prefs.h"

#define PRO_EXR_PREFS_ID    "com.fnordware.ProEXR_EZ"
#define PRO_EXR_PREF_ALPHA  "Alpha Mode"
#define PRO_EXR_PREF_UNMULT "UnMult"
#define PRO_EXR_PREF_MEMMAP "Memory Map"
#define PRO_EXR_PREF_DIALOG "Do Dialog"

@implementation ProEXR_EZ_In_Prefs

- (id)init {
	self = [super init];
	
	alpha_mode = PREFS_ALPHA_TRANSPARENCY;
	unmult = FALSE;
	memory_map = FALSE;
	always_dialog = FALSE;
	
	[self readPrefs];
	
	return self;
}

- (PrefsAlphaMode)getAlphaMode {
	return alpha_mode;
}

- (BOOL)getUnMult {
	return unmult;
}

- (BOOL)getMemoryMap {
	return memory_map;
}

- (BOOL)getAlwaysDialog {
	return always_dialog;
}

- (void)setAlphaMode:(PrefsAlphaMode)alphaMode {
	alpha_mode = alphaMode;
}

- (void)setUnMult:(BOOL)un_mult {
	unmult = un_mult;
}

- (void)setMemoryMap:(BOOL)mem_map {
	memory_map = mem_map;
}

- (void)setAlwaysDialog:(BOOL)alwaysDialog {
	always_dialog = alwaysDialog;
}

- (void)readPrefs {
	CFStringRef prefs_id = CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREFS_ID, kCFStringEncodingASCII);

	CFStringRef alpha_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_ALPHA, kCFStringEncodingASCII);
	CFStringRef unmult_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_UNMULT, kCFStringEncodingASCII);
	CFStringRef mem_map_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_MEMMAP, kCFStringEncodingASCII);
	CFStringRef always_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_DIALOG, kCFStringEncodingASCII);

    
	BOOL empty_prefs = false;
	
	CFPropertyListRef alphaMode	= CFPreferencesCopyValue(alpha_id, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
	CFPropertyListRef un_Mult	= CFPreferencesCopyValue(unmult_id, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
	CFPropertyListRef mem_map	= CFPreferencesCopyValue(mem_map_id, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
		
	if(alphaMode)
	{
		char alphaMode_char;
		
		if( CFNumberGetValue((CFNumberRef)alphaMode, kCFNumberCharType, &alphaMode_char) )
			alpha_mode = alphaMode_char;
		
		CFRelease(alphaMode);
	}
	else
		empty_prefs = TRUE;
	
	if(un_Mult)
	{
		unmult = CFBooleanGetValue((CFBooleanRef)un_Mult);
		
		CFRelease(un_Mult);
	}
	else
		empty_prefs = TRUE;

	if(mem_map)
	{
		memory_map = CFBooleanGetValue((CFBooleanRef)mem_map);
		
		CFRelease(mem_map);
	}
	else
		empty_prefs = TRUE;


	CFPropertyListRef doDialog	= CFPreferencesCopyValue(always_id, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
	
	if(doDialog)
	{
		always_dialog = CFBooleanGetValue((CFBooleanRef)doDialog);
		
		CFRelease(doDialog);
	}
	else
		empty_prefs = TRUE;
	
	if(empty_prefs)
	{
		[self writePrefs];
	}


	CFRelease(prefs_id);
	
	CFRelease(alpha_id);
	CFRelease(unmult_id);
	CFRelease(mem_map_id);
	CFRelease(always_id);
}

- (void)writePrefs {

	CFStringRef prefs_id = CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREFS_ID, kCFStringEncodingASCII);

	CFStringRef alpha_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_ALPHA, kCFStringEncodingASCII);
	CFStringRef unmult_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_UNMULT, kCFStringEncodingASCII);
	CFStringRef mem_map_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_MEMMAP, kCFStringEncodingASCII);

	char alphaMode_char = alpha_mode;
	
	CFNumberRef alphaMode = CFNumberCreate(kCFAllocatorDefault, kCFNumberCharType, &alphaMode_char);
	CFBooleanRef un_Mult =  unmult ? kCFBooleanTrue : kCFBooleanFalse;
	CFBooleanRef mem_map =  memory_map ? kCFBooleanTrue : kCFBooleanFalse;
	
	CFPreferencesSetValue(alpha_id, alphaMode, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
	CFPreferencesSetValue(unmult_id, un_Mult, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
	CFPreferencesSetValue(mem_map_id, mem_map, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
							
	Boolean success = CFPreferencesSynchronize(prefs_id,
									kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
	
													
	CFRelease(alphaMode);
	CFRelease(un_Mult);
	CFRelease(mem_map);

	[self writeAlways];


	CFRelease(prefs_id);
	
	CFRelease(alpha_id);
	CFRelease(unmult_id);
	CFRelease(mem_map_id);
}

- (void)writeAlways {

	CFStringRef prefs_id = CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREFS_ID, kCFStringEncodingASCII);
	CFStringRef always_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_DIALOG, kCFStringEncodingASCII);

	CFBooleanRef doDialog = always_dialog ? kCFBooleanTrue : kCFBooleanFalse;
	
	CFPreferencesSetValue(always_id, doDialog, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
	
	Boolean success = CFPreferencesSynchronize(prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
	
	CFRelease(doDialog);
	
	CFRelease(prefs_id);
	CFRelease(always_id);
}

@end
