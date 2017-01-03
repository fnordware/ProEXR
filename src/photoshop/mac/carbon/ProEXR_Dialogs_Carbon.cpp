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


#include "ProEXR.h"

#define gStuff				(globals->formatParamBlock)


enum {
	DIALOG_ID = 16100,
	ABOUT_ID,
	REG_ID,
    IMPORT_ID
};



enum {
	DIALOG_OK = 1,
	DIALOG_Cancel,
	DIALOG_Compression_Label,
	DIALOG_Compression_Menu,
	DIALOG_LumiChrom,
	DIALOG_float,
	DIALOG_Composite,
	DIALOG_Hidden,
	DIALOG_Banner
};


enum
{
    OUT_NO_COMPRESSION  = 0,	// no compression
    OUT_RLE_COMPRESSION,	// run length encoding
    OUT_ZIPS_COMPRESSION,	// zlib compression, one scan line at a time
    OUT_ZIP_COMPRESSION,	// zlib compression, in blocks of 16 scan lines
    OUT_PIZ_COMPRESSION,	// piz-based wavelet compression
    OUT_PXR24_COMPRESSION,	// lossy 24-bit float compression
	OUT_B44_COMPRESSION,	// lossy 16-bit float compression
	OUT_B44A_COMPRESSION,	// B44 with region shrinkage
	OUT_DWAA_COMPRESSION,	// lossy DCT, 32 scanlines
	OUT_DWAB_COMPRESSION,	// lossy DCT, 32 scanlines

    OUT_NUM_COMPRESSION_METHODS	// number of different compression methods
};

#pragma mark-

// ==============
// Only compiled on 32-bit
// ==============
#if !__LP64__


bool ProEXR_Write_Dialog(ProEXR_outData *options, int layers, bool hidden_layers)
{
	bool hit_ok = false;
	
	SetThemeCursor(kThemeArrowCursor); // for some reason, Photoshop is giving me a watch
	
	DialogRef dp = GetNewDialog (DIALOG_ID, nil, (WindowPtr) -1); // instantiate the dialog
	
	if(dp)
	{
		ControlRef	compression_label = NULL,
					compression_menu = NULL,
					lumichrom_check = NULL,
					float_check = NULL,
					composite_check = NULL,
					hidden_check = NULL;
				
		GetDialogItemAsControl(dp, DIALOG_Compression_Label, &compression_label);
		GetDialogItemAsControl(dp, DIALOG_Compression_Menu, &compression_menu);
		GetDialogItemAsControl(dp, DIALOG_LumiChrom, &lumichrom_check);
		GetDialogItemAsControl(dp, DIALOG_float, &float_check);
		GetDialogItemAsControl(dp, DIALOG_Composite, &composite_check);
		GetDialogItemAsControl(dp, DIALOG_Hidden, &hidden_check);
		
		
		if(layers > 1) // disable luni/chrom check
			DisableControl(lumichrom_check);
			
		if(options->luminance_chroma) // disable float check
			DisableControl(float_check);
		
		if(layers < 2)
			DisableControl(composite_check);
			
		if(!hidden_layers)
			DisableControl(hidden_check);
		
		
		// justify compression label
		ControlFontStyleRec font_style = {kControlUseJustMask, 0, 0, 0, 0, teJustRight, {0,0,0}, {0,0,0} };
		SetControlFontStyle(compression_label, &font_style);
		
		// fill in menu
		do{
			MenuRef menu = GetControlPopupMenuHandle(compression_menu);
			
			Str255 options[] = {
									"\pNone", // already provided in the resource
									"\pRLE",
									"\pZip",
									"\pZip16",
									"\pPiz",
									"\pPXR24",
									"\pB44",
									"\pB44A",
									"\pDWAA",
									"\pDWAB"	};

			short menuCount = CountMenuItems(menu);
			
			for(int i=OUT_RLE_COMPRESSION; i < OUT_NUM_COMPRESSION_METHODS; i++)
			{		
				InsertMenuItem(menu, options[i], menuCount++);
			}
			
			// the control needs to know it can go higher
			SetControlMaximum(compression_menu, OUT_NUM_COMPRESSION_METHODS);
		}while(0);
		
		
		// set control values
		SetControlValue(compression_menu, options->compression_type + 1);
		SetControlValue(lumichrom_check, options->luminance_chroma);
		SetControlValue(float_check, options->float_not_half);
		SetControlValue(composite_check, options->layer_composite);
		SetControlValue(hidden_check, options->hidden_layers);
		
		
		
		ShowWindow((WindowRef)dp);
		DrawDialog(dp);

		SetDialogCancelItem(dp, DIALOG_Cancel);
		
		short item;
		do{
			ModalDialog(NULL, &item);
			
			if(item == DIALOG_LumiChrom)
			{
				SetControlValue(lumichrom_check, !GetControlValue(lumichrom_check));
				
				if( GetControlValue(lumichrom_check) )
					DisableControl(float_check);
				else
					EnableControl(float_check);
			}
			else if(item == DIALOG_float)
				SetControlValue(float_check, !GetControlValue(float_check));
			else if(item == DIALOG_Composite)
				SetControlValue(composite_check, !GetControlValue(composite_check));
			else if(item == DIALOG_Hidden)
				SetControlValue(hidden_check, !GetControlValue(hidden_check));
			
		}while(item > DIALOG_Cancel);
		

		
		if(item == DIALOG_OK)
		{
			options->compression_type = GetControlValue(compression_menu) - 1;
			options->luminance_chroma = GetControlValue(lumichrom_check);
			options->float_not_half = GetControlValue(float_check);
			options->layer_composite = GetControlValue(composite_check);
			options->hidden_layers = GetControlValue(hidden_check);
			
			hit_ok = true;
		}		

		DisposeDialog(dp);
	}
	
	return hit_ok;
}

#pragma mark-

bool ProEXR_SaveAs(char *path, int buf_len)
{
	bool result = false;
	
	//Str255 prompt;
	OSErr err = noErr;
	NavDialogRef dialogRef;
	NavReplyRecord navReply;
	NavDialogCreationOptions navOptions;
	NavUserAction action;
	//int32 nameLength = 0;
	
	// gReply->validRecord = false;

	// this needs to happen first ( setup the dialog creation options )
	err = NavGetDefaultDialogCreationOptions( &navOptions );

	// setup the strings
	navOptions.windowTitle = CFSTR( "ProEXR Layer Export" );
	navOptions.message = CFSTR( "Supply Base Filename for EXR Layer Files" );
	
	if( strlen(path) )
		navOptions.saveFileName = CFStringCreateWithCString(NULL, path, kCFStringEncodingASCII);
	else
		navOptions.saveFileName = CFStringCreateWithCString(NULL, "Untitled.exr", kCFStringEncodingASCII);
		

	// Setup flags for the default supports we want
	navOptions.optionFlags |= kNavNoTypePopup;
	navOptions.preferenceKey =  'pEXR';
	navOptions.modality = kWindowModalityAppModal;

	err = NavCreatePutFileDialog( 	&navOptions,
									'EXR ',
									'8BIM',
									NULL,
									NULL,
									&dialogRef );
									
	err = NavDialogRun ( dialogRef );

	err = NavDialogGetReply( dialogRef, &navReply );

	// what did the user do?
	action = NavDialogGetUserAction( dialogRef );

	if( navReply.validRecord && action == kNavUserActionSaveAs )
	{
		FSRef resultFSRef;
		AEKeyword dummyKeyword = 0L;
		DescType  dummyType;
		Size      dummySize;

		// move the data into a real FSSpec
		err = AEGetNthPtr ( &(navReply.selection),
							1,
							typeFSRef,
							&dummyKeyword, 
							&dummyType,	
							&resultFSRef, 
							sizeof(FSRef), 
							&dummySize);

		if( noErr == err )
		{
			// have resultFSRef, navReply.saveFileName, and FSRefMakePath
			
			// make a path, copy it up
			char folder_path[256];
			char file_name[256];
			
			FSRefMakePath(&resultFSRef, (UInt8 *)folder_path, 255);
			
			CFStringGetCString(navReply.saveFileName, file_name, 255, kCFStringEncodingASCII);
			
			strncat(folder_path, "/", 255 - strlen(folder_path) );
			strncat(folder_path, file_name, 255 - strlen(folder_path) );
			
			if(strlen(folder_path) < buf_len)
			{
				strcpy(path, folder_path);
				
				result = true;
			}
		}
	}


	//CFRelease(navOptions.message);
	//CFRelease(navOptions.windowTitle);
	CFRelease(navOptions.saveFileName);
	
	//navOptions.message = NULL;
	//navOptions.windowTitle = NULL;
	//navOptions.saveFileName = NULL;
	
	// clean up the reply
	err = NavDisposeReply( &navReply );
	
	// clean up the nav dialog
	NavDialogDispose( dialogRef );
	
	return result;
}

void ProEXR_MakeAliasFromPath(char *file_path, PIPlatformFileHandle *aliasHandle)
{
	FSRef temp_fsref;
	
	if(noErr == FSPathMakeRef((UInt8 *)file_path, &temp_fsref, NULL) )
	{
		FSNewAlias(NULL, &temp_fsref, aliasHandle);
	}
	else
	{
		// ack, make a new file, then an alias, then delete
		std::string path(file_path);
		std::string dir_path = path.substr(0, path.find_last_of('/'));
		std::string filename = path.substr(path.find_last_of('/') + 1);
		
		FSRef dir_ref;
		
		if(noErr == FSPathMakeRef((UInt8 *)dir_path.c_str(), &dir_ref, NULL) )
		{
			CFStringRef cf_file = CFStringCreateWithCString(kCFAllocatorDefault, filename.c_str(), kCFStringEncodingASCII );
			UniChar uni_file[255];
			
			CFStringGetCharacters(cf_file, CFRangeMake(0, CFStringGetLength(cf_file)), uni_file);
			
			FSRef file_ref;
			
			if(noErr == FSCreateFileUnicode(&dir_ref, CFStringGetLength(cf_file), uni_file,
												kFSCatInfoNone, NULL, &file_ref, NULL) )
			{
				FSNewAlias(NULL, &file_ref, aliasHandle);
				
				FSDeleteObject(&file_ref);
			}
		}
	}
}

#pragma mark-

enum {
	IMPORT_OK = 1,
    IMPORT_Cancel,
    IMPORT_Set_Defaults,
    IMPORT_Alpha_Transparent,
    IMPORT_Alpha_Seperate,
    IMPORT_UnMult,
    IMPORT_Ignore_LayerText,
	IMPORT_Memory_Map,
    IMPORT_Always_Dialog
};


#define PRO_EXR_PREFS_ID    "com.fnordware.ProEXR"
#define PRO_EXR_PREF_ALPHA  "Alpha Mode"
#define PRO_EXR_PREF_UNMULT "UnMult"
#define PRO_EXR_PREF_IGNORE "Ignore LayerText"
#define PRO_EXR_PREF_MEMMAP	"Memory Map"
#define PRO_EXR_PREF_DIALOG "Do Dialog"

static void SetImportPrefs(ProEXR_inData *in_options, bool *always_dialog)
{
	CFStringRef prefs_id = CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREFS_ID, kCFStringEncodingASCII);
	
	CFStringRef alpha_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_ALPHA, kCFStringEncodingASCII);
	CFStringRef unmult_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_UNMULT, kCFStringEncodingASCII);
	CFStringRef ignore_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_IGNORE, kCFStringEncodingASCII);
	CFStringRef mem_map_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_MEMMAP, kCFStringEncodingASCII);
	CFStringRef always_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_DIALOG, kCFStringEncodingASCII);
	
    if(in_options)
    {
        char alphaMode_char = in_options->alpha_mode;
        
        CFNumberRef alphaMode = CFNumberCreate(kCFAllocatorDefault, kCFNumberCharType, &alphaMode_char);
        CFBooleanRef unMult =  in_options->unmult ? kCFBooleanTrue : kCFBooleanFalse;
        CFBooleanRef ignoreLayerText = in_options->ignore_layertext ? kCFBooleanTrue : kCFBooleanFalse;
		CFBooleanRef mem_map = in_options->memory_map ? kCFBooleanTrue : kCFBooleanFalse;
                                                            
		CFPreferencesSetValue(alpha_id, alphaMode, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
		CFPreferencesSetValue(unmult_id, unMult, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
		CFPreferencesSetValue(ignore_id, ignoreLayerText, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
		CFPreferencesSetValue(mem_map_id, mem_map, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
		
		Boolean success = CFPreferencesSynchronize(CFSTR(PRO_EXR_PREFS_ID),
										kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
        
                                                        
        CFRelease(alphaMode);
        CFRelease(unMult);
        CFRelease(ignoreLayerText);
		CFRelease(mem_map);
    }
    
    
    if(always_dialog)
    {
        CFBooleanRef doDialog = *always_dialog ? kCFBooleanTrue : kCFBooleanFalse;
        
		CFPreferencesSetValue(always_id, doDialog, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
		
		Boolean success = CFPreferencesSynchronize(CFSTR(PRO_EXR_PREFS_ID), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
        
        CFRelease(doDialog);
    }
	
	
	CFRelease(prefs_id);
	
	CFRelease(alpha_id);
	CFRelease(unmult_id);
	CFRelease(ignore_id);
	CFRelease(mem_map_id);
	CFRelease(always_id);
}

static void GetImportPrefs(ProEXR_inData *in_options, bool *always_dialog)
{
	CFStringRef prefs_id = CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREFS_ID, kCFStringEncodingASCII);
	
	CFStringRef alpha_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_ALPHA, kCFStringEncodingASCII);
	CFStringRef unmult_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_UNMULT, kCFStringEncodingASCII);
	CFStringRef ignore_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_IGNORE, kCFStringEncodingASCII);
	CFStringRef mem_map_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_MEMMAP, kCFStringEncodingASCII);
	CFStringRef always_id	= CFStringCreateWithCString(kCFAllocatorDefault, PRO_EXR_PREF_DIALOG, kCFStringEncodingASCII);

	bool empty_prefs = false;
	
    if(in_options)
    {
		CFPropertyListRef alphaMode			= CFPreferencesCopyValue(alpha_id, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
		CFPropertyListRef unMult			= CFPreferencesCopyValue(unmult_id, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
		CFPropertyListRef ignoreLayerText	= CFPreferencesCopyValue(ignore_id, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
		CFPropertyListRef mem_map			= CFPreferencesCopyValue(mem_map_id, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
        
        if(alphaMode)
        {
            char alphaMode_char;
            
            if( CFNumberGetValue((CFNumberRef)alphaMode, kCFNumberCharType, &alphaMode_char) )
                in_options->alpha_mode = alphaMode_char;
            
            CFRelease(alphaMode);
        }
		else
			empty_prefs = true;
        
        if(unMult)
        {
            in_options->unmult = CFBooleanGetValue((CFBooleanRef)unMult);
            
            CFRelease(unMult);
        }
		else
			empty_prefs = true;
        
        if(ignoreLayerText)
        {
            in_options->ignore_layertext = CFBooleanGetValue((CFBooleanRef)ignoreLayerText);
            
            CFRelease(ignoreLayerText);
        }
		else
			empty_prefs = true;
		
		if(mem_map)
		{
			in_options->memory_map = CFBooleanGetValue((CFBooleanRef)mem_map);
			
			CFRelease(mem_map);
		}
		else
			empty_prefs = true;
    }
    
    if(always_dialog)
    {
		CFPropertyListRef doDialog = CFPreferencesCopyValue(always_id, prefs_id, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
        
        if(doDialog)
        {
            *always_dialog = CFBooleanGetValue((CFBooleanRef)doDialog);
            
            CFRelease(doDialog);
        }
		else
			empty_prefs = true;
    }
	
	if(empty_prefs)
	{
		SetImportPrefs(in_options, always_dialog);
	}
	
	
	CFRelease(prefs_id);
	
	CFRelease(alpha_id);
	CFRelease(unmult_id);
	CFRelease(ignore_id);
	CFRelease(mem_map_id);
	CFRelease(always_id);
}

static bool ReadDialog(ProEXR_inData *in_options, bool always_dialog)
{
	bool hit_ok = false;
	
	SetThemeCursor(kThemeArrowCursor); // for some reason, Photoshop is giving me a watch
	
	DialogRef dp = GetNewDialog (IMPORT_ID, nil, (WindowPtr) -1); // instantiate the dialog
	
	if(dp)
	{
		ControlRef	set_defaults_button = NULL,
					alpha_transparent_radio = NULL,
					alpha_seperate_radio = NULL,
					unmult_check = NULL,
					ignore_layertext_check = NULL,
					memory_map_check = NULL,
					always_dialog_check = NULL;
				
		GetDialogItemAsControl(dp, IMPORT_Set_Defaults, &set_defaults_button);
		GetDialogItemAsControl(dp, IMPORT_Alpha_Transparent, &alpha_transparent_radio);
		GetDialogItemAsControl(dp, IMPORT_Alpha_Seperate, &alpha_seperate_radio);
		GetDialogItemAsControl(dp, IMPORT_UnMult, &unmult_check);
		GetDialogItemAsControl(dp, IMPORT_Ignore_LayerText, &ignore_layertext_check);
		GetDialogItemAsControl(dp, IMPORT_Memory_Map, &memory_map_check);
		GetDialogItemAsControl(dp, IMPORT_Always_Dialog, &always_dialog_check);
		
		
		if(in_options->alpha_mode != ALPHA_SEPERATE) // disable unmult
			DisableControl(unmult_check);
		
		
		// set control values
        if(in_options->alpha_mode == ALPHA_TRANSPARENCY)
            SetControlValue(alpha_transparent_radio, TRUE);
        else if(in_options->alpha_mode == ALPHA_SEPERATE)
            SetControlValue(alpha_seperate_radio, TRUE);
            
		SetControlValue(unmult_check, in_options->unmult);
		SetControlValue(ignore_layertext_check, in_options->ignore_layertext);
		SetControlValue(memory_map_check, in_options->memory_map);
		SetControlValue(always_dialog_check, always_dialog);
		
		
		
		ShowWindow((WindowRef)dp);
		DrawDialog(dp);

		SetDialogCancelItem(dp, DIALOG_Cancel);
		
		short item;
		do{
			ModalDialog(NULL, &item);
			
			if(item == IMPORT_Set_Defaults)
            {
                ProEXR_inData in_options_temp = {
                            GetControlValue(alpha_seperate_radio) ? ALPHA_SEPERATE : ALPHA_TRANSPARENCY,
                            GetControlValue(unmult_check),
                            GetControlValue(ignore_layertext_check),
							GetControlValue(memory_map_check) };
                
                bool always_dialog_temp = GetControlValue(always_dialog_check);
                
                SetImportPrefs(&in_options_temp, &always_dialog_temp);
                
                SysBeep(30);
            }
            else if(item == IMPORT_Alpha_Transparent)
            {
                SetControlValue(alpha_transparent_radio, TRUE);
                SetControlValue(alpha_seperate_radio, FALSE);
                DisableControl(unmult_check);
            }
            else if(item == IMPORT_Alpha_Seperate)
			{
                SetControlValue(alpha_transparent_radio, FALSE);
                SetControlValue(alpha_seperate_radio, TRUE);
                EnableControl(unmult_check);
			}
			else if(item == IMPORT_UnMult)
				SetControlValue(unmult_check, !GetControlValue(unmult_check));
			else if(item == IMPORT_Ignore_LayerText)
				SetControlValue(ignore_layertext_check, !GetControlValue(ignore_layertext_check));
			else if(item == IMPORT_Always_Dialog)
				SetControlValue(always_dialog_check, !GetControlValue(always_dialog_check));
			else if(item == IMPORT_Memory_Map)
				SetControlValue(memory_map_check, !GetControlValue(memory_map_check));
			
		}while(item > DIALOG_Cancel);
		

		
		if(item == DIALOG_OK)
		{
            in_options->alpha_mode = GetControlValue(alpha_seperate_radio) ? ALPHA_SEPERATE : ALPHA_TRANSPARENCY;
			in_options->unmult = GetControlValue(unmult_check);
			in_options->ignore_layertext = GetControlValue(ignore_layertext_check);
			in_options->memory_map = GetControlValue(memory_map_check);
			always_dialog = GetControlValue(always_dialog_check);
            
            SetImportPrefs(NULL, &always_dialog);
			
			hit_ok = true;
		}		

		DisposeDialog(dp);
	}
	
	return hit_ok;
}


bool ProEXR_Read_Dialog(ProEXR_inData *in_options)
{
	bool always_dialog = false;
	
	GetImportPrefs(in_options, &always_dialog);
    
	// check for that option key
    UInt32 keys = GetCurrentEventKeyModifiers();
	//UInt32 keys = GetCurrentKeyModifiers();
	
	bool option_key = ( (keys & shiftKey) || (keys & rightShiftKey) || (keys & optionKey) || (keys & rightOptionKey) );
    
    if(option_key || always_dialog)
        return ReadDialog(in_options, always_dialog);
    else
        return true;
}

#endif //!__LP64__
