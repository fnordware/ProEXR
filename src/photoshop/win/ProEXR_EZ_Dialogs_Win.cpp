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

#include "ProEXR_EZ.h"

#include <Windows.h>



// dialog comtrols
enum {
	OUT_noUI = -1,
	OUT_OK = IDOK,
	OUT_Cancel = IDCANCEL,
	OUT_Compression_Menu = 3,
	OUT_LumiChrom_Check,
	OUT_Float_Check,
	OUT_Alpha_None,
	OUT_Alpha_Transparency,
	OUT_Alpha_Channel
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
	OUT_B44A_COMPRESSION,	// B44 with extra region goodness
	OUT_DWAA_COMPRESSION,	// DCT compression
	OUT_DWAB_COMPRESSION,	// DCT compression with 256-pixel blocks

    OUT_NUM_COMPRESSION_METHODS	// number of different compression methods
};


extern HANDLE hDllInstance;

static WORD	g_item_clicked = 0;

static const char *g_alpha_name = NULL;
static bool g_have_transparency = false;
static bool g_greyscale = false;

static A_u_char		g_Compression	= OUT_PIZ_COMPRESSION;
static A_Boolean	g_lumi_chrom	= FALSE;
static A_Boolean	g_32bit_float	= FALSE;
static WriteAlphaMode	g_alpha_mode = WRITE_ALPHA_TRANSPARENCY;


#define GET_ITEM(ITEM)	GetDlgItem(hwndDlg, (ITEM))

#define SET_CHECK(ITEM, VAL)	SendMessage(GET_ITEM(ITEM), BM_SETCHECK, (WPARAM)(VAL), (LPARAM)0)
#define GET_CHECK(ITEM)			SendMessage(GET_ITEM(ITEM), BM_GETCHECK, (WPARAM)0, (LPARAM)0)

// thanks, Photoshop 6 SDK!
static void PS6CenterDialog(HWND hDlg)
{
	int  nHeight;
    int  nWidth;
    int  nTitleBits;
    RECT rcDialog;
    RECT rcParent;
    int  xOrigin;
    int  yOrigin;
    int  xScreen;
    int  yScreen;

    HWND hParent = GetParent(hDlg);

    if  (hParent == NULL)
        hParent = GetDesktopWindow();

    GetClientRect(hParent, &rcParent);
    ClientToScreen(hParent, (LPPOINT)&rcParent.left);  // point(left,  top)
    ClientToScreen(hParent, (LPPOINT)&rcParent.right); // point(right, bottom)

    // Center on Title: title bar has system menu, minimize,  maximize bitmaps
    // Width of title bar bitmaps - assumes 3 of them and dialog has a sysmenu
    nTitleBits = GetSystemMetrics(SM_CXSIZE);



    // If dialog has no sys menu compensate for odd# bitmaps by sub 1 bitwidth
    if  ( ! (GetWindowLong(hDlg, GWL_STYLE) & WS_SYSMENU))
        nTitleBits -= nTitleBits / 3;


    GetWindowRect(hDlg, &rcDialog);
    nWidth  = rcDialog.right  - rcDialog.left;
    nHeight = rcDialog.bottom - rcDialog.top;


    xOrigin = MAX(rcParent.right - rcParent.left - nWidth, 0) / 2
            + rcParent.left - nTitleBits;

    xScreen = GetSystemMetrics(SM_CXSCREEN);

    if  (xOrigin + nWidth > xScreen)
        xOrigin = MAX(0, xScreen - nWidth);

	yOrigin = MAX(rcParent.bottom - rcParent.top - nHeight, 0) / 3
            + rcParent.top;

    yScreen = GetSystemMetrics(SM_CYSCREEN);

    if  (yOrigin + nHeight > yScreen)
        yOrigin = MAX(0 , yScreen - nHeight);

    SetWindowPos(hDlg, NULL, xOrigin, yOrigin, nWidth, nHeight, SWP_NOZORDER);
}


static void TrackLumiChrom(HWND hwndDlg)
{
	BOOL enable_state = !SendMessage(GetDlgItem(hwndDlg, OUT_LumiChrom_Check), BM_GETCHECK, (WPARAM)0, (LPARAM)0);
	EnableWindow(GetDlgItem(hwndDlg, OUT_Float_Check), enable_state);
}


static BOOL CALLBACK DialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    BOOL fError; 

    switch (message) 
    { 
		case WM_INITDIALOG:
			PS6CenterDialog(hwndDlg);

			do{
				// set up the menu
				// I prefer to do it programatically to insure that the compression types match the index
				const char *opts[] = {	"None",
										"RLE",
										"Zip",
										"Zip16",
										"Piz",
										"PXR24",
										"B44",
										"B44A",
										"DWAA",
										"DWAB" };

				HWND menu = GetDlgItem(hwndDlg, OUT_Compression_Menu);

				int i;

				for(int i=OUT_NO_COMPRESSION; i < OUT_NUM_COMPRESSION_METHODS; i++)
				{
					SendMessage(menu,( UINT)CB_ADDSTRING, (WPARAM)wParam, (LPARAM)(LPCTSTR)opts[i] );
					SendMessage( menu,(UINT)CB_SETITEMDATA, (WPARAM)i, (LPARAM)(DWORD)i); // this is the compresion number

					if(i == g_Compression)
						SendMessage( menu, CB_SETCURSEL, (WPARAM)i, (LPARAM)0);
				}
			}while(0);

			SET_CHECK(OUT_Float_Check, g_32bit_float);

			if(g_greyscale) // disable luminance/chroma check
			{
				EnableWindow(GetDlgItem(hwndDlg, OUT_LumiChrom_Check), FALSE);
				g_lumi_chrom = false;
			}

			SET_CHECK(OUT_LumiChrom_Check, g_lumi_chrom);
			TrackLumiChrom(hwndDlg);

			// alpha radio buttons
			switch(g_alpha_mode)
			{
				case WRITE_ALPHA_NONE:			SET_CHECK(OUT_Alpha_None, TRUE);			break;
				case WRITE_ALPHA_TRANSPARENCY:	SET_CHECK(OUT_Alpha_Transparency, TRUE);	break;
				case WRITE_ALPHA_CHANNEL:		SET_CHECK(OUT_Alpha_Channel, TRUE);			break;
			}
			
			if(!g_have_transparency)
			{
				if(GET_CHECK(OUT_Alpha_Transparency))
				{
					SET_CHECK(OUT_Alpha_Transparency, FALSE);
					
					if(g_alpha_name == NULL)
						SET_CHECK(OUT_Alpha_None, TRUE);
					else
						SET_CHECK(OUT_Alpha_Channel, TRUE);
				}
				
				EnableWindow(GET_ITEM(OUT_Alpha_Transparency), FALSE);
			}

			if(g_alpha_name == NULL)
			{
				if(GET_CHECK(OUT_Alpha_Channel))
				{
					SET_CHECK(OUT_Alpha_Channel, FALSE);
					
					if(!g_have_transparency)
						SET_CHECK(OUT_Alpha_None, TRUE);
					else
						SET_CHECK(OUT_Alpha_Transparency, TRUE);
				}
				
				SetDlgItemText(hwndDlg, OUT_Alpha_Channel, "Channel Not Available");
				
				EnableWindow(GET_ITEM(OUT_Alpha_Channel), FALSE);
			}
			else
			{
				std::string title = std::string("Channel \"") + g_alpha_name + "\"";

				SetDlgItemText(hwndDlg, OUT_Alpha_Channel, title.c_str());
			}

			return TRUE;

        case WM_COMMAND: 
			g_item_clicked = LOWORD(wParam);

            switch (LOWORD(wParam)) 
            { 
                case OUT_OK: 
				case OUT_Cancel:  // do the same thing, but g_item_clicked will be different
					do{

						// get the compression index associated with the selected menu item
						HWND menu = GetDlgItem(hwndDlg, OUT_Compression_Menu);
						LRESULT cur_sel = SendMessage(menu,(UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
						g_Compression = SendMessage(menu,(UINT)CB_GETITEMDATA, (WPARAM)cur_sel, (LPARAM)0);

						g_lumi_chrom = GET_CHECK(OUT_LumiChrom_Check);
						g_32bit_float = GET_CHECK(OUT_Float_Check);

						g_alpha_mode = GET_CHECK(OUT_Alpha_Transparency) ? WRITE_ALPHA_TRANSPARENCY :
										GET_CHECK(OUT_Alpha_Channel) ? WRITE_ALPHA_CHANNEL :
										WRITE_ALPHA_NONE;

					}while(0);

					//PostMessage((HWND)hwndDlg, WM_QUIT, (WPARAM)WA_ACTIVE, lParam);
					EndDialog(hwndDlg, 0);
                    //DestroyWindow(hwndDlg); 

                    return TRUE;

				case OUT_LumiChrom_Check:
					TrackLumiChrom(hwndDlg);
					return TRUE;
            } 
    } 

    return FALSE; 
} 


bool ProEXR_Write_Dialog(HWND hwndOwner, ProEXR_outData *options, const char *alpha_name, bool have_transparency, bool greyscale)
{
	bool hit_ok = false;
	
	// set globals
	g_Compression = options->compression_type;
	g_lumi_chrom = options->luminance_chroma;
	g_32bit_float = options->float_not_half;
	g_alpha_mode = options->alpha_mode;

	g_alpha_name = alpha_name;
	g_have_transparency = have_transparency;
	g_greyscale = greyscale;


	int status = DialogBox((HINSTANCE)hDllInstance, (LPSTR)"OUTDIALOG", hwndOwner, (DLGPROC)DialogProc);


	if(g_item_clicked == OUT_OK)
	{
		options->compression_type = g_Compression;
		options->luminance_chroma = g_lumi_chrom;
		options->float_not_half = g_32bit_float;
		options->alpha_mode = g_alpha_mode;

		hit_ok = true;
	}

	return hit_ok;
}


#define PRO_EXR_PREFIX			"Software\\fnord\\ProEXR_EZ"

#define ALPHA_ACTION_KEY		"Alpha Action"
#define UNMULT_KEY				"UnMult"
#define MEMORY_MAP_KEY			"Memory Map"
#define ALWAYS_DIALOG_KEY		"Always Dialog"

static void SetImportPrefs(ProEXR_inData *in_options, bool *always_dialog)
{
	HKEY proexr_hkey;

	// get key
	LONG reg_error = RegCreateKeyEx(HKEY_CURRENT_USER, PRO_EXR_PREFIX, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &proexr_hkey, NULL);

	if(reg_error == ERROR_SUCCESS)
	{
		DWORD dword_val;

		if(in_options)
		{
			dword_val = in_options->alpha_mode;
			reg_error = RegSetValueEx(proexr_hkey, ALPHA_ACTION_KEY, NULL, REG_DWORD, (const BYTE *)&dword_val, sizeof(DWORD));

			dword_val = in_options->unmult;
			reg_error = RegSetValueEx(proexr_hkey, UNMULT_KEY, NULL, REG_DWORD, (const BYTE *)&dword_val, sizeof(DWORD));

			dword_val = in_options->memory_map;
			reg_error = RegSetValueEx(proexr_hkey, MEMORY_MAP_KEY, NULL, REG_DWORD, (const BYTE *)&dword_val, sizeof(DWORD));
		}
	    
	    
		if(always_dialog)
		{
			dword_val = *always_dialog;
			reg_error = RegSetValueEx(proexr_hkey, ALWAYS_DIALOG_KEY, NULL, REG_DWORD, (const BYTE *)&dword_val, sizeof(DWORD));
		}


		reg_error = RegCloseKey(proexr_hkey);
	}
}

static void GetImportPrefs(ProEXR_inData *in_options, bool *always_dialog)
{
	HKEY proexr_hkey;
	
	bool empty_prefs = false;

	LONG reg_error = RegOpenKeyEx(HKEY_CURRENT_USER, PRO_EXR_PREFIX, 0, KEY_READ, &proexr_hkey);

	if(reg_error == ERROR_SUCCESS)
	{
		DWORD siz;
		DWORD type;
		DWORD dword_val;

		if(in_options)
		{
			reg_error = RegQueryValueEx(proexr_hkey, ALPHA_ACTION_KEY, NULL, &type, (LPBYTE)&dword_val, &siz);

			if(reg_error == ERROR_SUCCESS)
				in_options->alpha_mode = dword_val;
			else
				empty_prefs = true;


			reg_error = RegQueryValueEx(proexr_hkey, UNMULT_KEY, NULL, &type, (LPBYTE)&dword_val, &siz);

			if(reg_error == ERROR_SUCCESS)
				in_options->unmult = dword_val;
			else
				empty_prefs = true;

			reg_error = RegQueryValueEx(proexr_hkey, MEMORY_MAP_KEY, NULL, &type, (LPBYTE)&dword_val, &siz);

			if(reg_error == ERROR_SUCCESS)
				in_options->memory_map = dword_val;
			else
				empty_prefs = true;
		}
	    
	    
		if(always_dialog)
		{
			reg_error = RegQueryValueEx(proexr_hkey, ALWAYS_DIALOG_KEY, NULL, &type, (LPBYTE)&dword_val, &siz);

			if(reg_error == ERROR_SUCCESS)
				*always_dialog = (dword_val == TRUE);
			else
				empty_prefs = true;
		}
	}
	else
		empty_prefs = true;
	

	if(empty_prefs)
		SetImportPrefs(in_options, always_dialog);
}




// dialog comtrols
enum {
	INnoUI = -1,
	IN_OK = IDOK,
	IN_Cancel = IDCANCEL,
	IN_Set_Defaults = 3,
	IN_Transparency_Radio,
	IN_Seperate_Radio,
	IN_UnMult_Check,
	IN_Memory_Map_Check,
	IN_Always_Dialog_Check
};


AlphaMode	g_in_alpha_mode;
A_Boolean	g_unmult;
A_Boolean	g_memory_map;
A_Boolean	g_always_dialog;

static BOOL CALLBACK InDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    BOOL fError; 

    switch (message) 
    { 
		case WM_INITDIALOG:
			PS6CenterDialog(hwndDlg);

			if(g_in_alpha_mode == ALPHA_TRANSPARENCY)
			{
				CheckRadioButton(hwndDlg, IN_Transparency_Radio, IN_Seperate_Radio, IN_Transparency_Radio);
				EnableWindow(GetDlgItem(hwndDlg, IN_UnMult_Check), FALSE);
			}
			else
			{
				CheckRadioButton(hwndDlg, IN_Transparency_Radio, IN_Seperate_Radio, IN_Seperate_Radio);
				EnableWindow(GetDlgItem(hwndDlg, IN_UnMult_Check), TRUE);
			}

			CheckDlgButton(hwndDlg, IN_UnMult_Check, g_unmult);
			CheckDlgButton(hwndDlg, IN_Memory_Map_Check, g_memory_map);
			CheckDlgButton(hwndDlg, IN_Always_Dialog_Check, g_always_dialog);

			return TRUE;

        case WM_COMMAND: 
			g_item_clicked = LOWORD(wParam);

            switch (LOWORD(wParam)) 
            { 
                case OUT_OK: 
				case OUT_Cancel:  // do the same thing, but g_item_clicked will be different

					g_in_alpha_mode = (IsDlgButtonChecked(hwndDlg, IN_Transparency_Radio) ? ALPHA_TRANSPARENCY : ALPHA_SEPERATE);
					g_unmult = IsDlgButtonChecked(hwndDlg, IN_UnMult_Check);
					g_memory_map = IsDlgButtonChecked(hwndDlg, IN_Memory_Map_Check);
					g_always_dialog = IsDlgButtonChecked(hwndDlg, IN_Always_Dialog_Check);

					//PostMessage((HWND)hwndDlg, WM_QUIT, (WPARAM)WA_ACTIVE, lParam);
					EndDialog(hwndDlg, 0);
                    //DestroyWindow(hwndDlg); 

                    return TRUE;

				case IN_Set_Defaults:
					do{
						ProEXR_inData in_options_temp = {
                            (IsDlgButtonChecked(hwndDlg, IN_Transparency_Radio) ? ALPHA_TRANSPARENCY : ALPHA_SEPERATE),
                            IsDlgButtonChecked(hwndDlg, IN_UnMult_Check),
							IsDlgButtonChecked(hwndDlg, IN_Memory_Map_Check) };
                
						bool always_dialog_temp = IsDlgButtonChecked(hwndDlg, IN_Always_Dialog_Check);
                
						SetImportPrefs(&in_options_temp, &always_dialog_temp);
                
						MessageBeep(0xFFFFFFFF);

					}while(0);

					return TRUE;

				case IN_Transparency_Radio:
				case IN_Seperate_Radio:

					CheckRadioButton(hwndDlg, IN_Transparency_Radio, IN_Seperate_Radio, g_item_clicked);
					EnableWindow(GetDlgItem(hwndDlg, IN_UnMult_Check), (LOWORD(wParam) == IN_Seperate_Radio));

					return TRUE;
            } 
    } 

    return FALSE; 
} 

static bool ProEXR_DoReadDialog(HWND hwndOwner, ProEXR_inData *in_options, bool always_dialog)
{
	bool hit_ok = false;
	
	// set globals
	g_in_alpha_mode = in_options->alpha_mode;
	g_unmult = in_options->unmult;
	g_memory_map = in_options->memory_map;
	g_always_dialog = always_dialog;


	int status = DialogBox((HINSTANCE)hDllInstance, (LPSTR)"INDIALOG", hwndOwner, (DLGPROC)InDialogProc);


	if(g_item_clicked == IN_OK)
	{
		in_options->alpha_mode = g_in_alpha_mode;
		in_options->unmult = g_unmult;
		in_options->memory_map = g_memory_map;
		always_dialog = g_always_dialog;

		SetImportPrefs(NULL, &always_dialog);

		hit_ok = true;
	}

	return hit_ok;
}

static inline bool KeyIsDown(int vKey)
{
	return (GetAsyncKeyState(vKey) & 0x8000);
}

bool ProEXR_Read_Dialog(HWND hwndOwner, ProEXR_inData *in_options)
{
	bool always_dialog = false;
	
	GetImportPrefs(in_options, &always_dialog);
    
	// check for that alt key
	bool alt_key = ( KeyIsDown(VK_LSHIFT) || KeyIsDown(VK_RSHIFT) || KeyIsDown(VK_LMENU) || KeyIsDown(VK_RMENU) );
    
    if(alt_key || always_dialog)
        return ProEXR_DoReadDialog(hwndOwner, in_options, always_dialog);
    else
        return true;
}


