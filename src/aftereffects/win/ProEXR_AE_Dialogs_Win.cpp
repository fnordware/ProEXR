
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


#include "ProEXR_AE_Dialogs.h"

#include <windows.h>
//#include <Shlwapi.h>
#include <ShlObj.h>

#include <sstream>

#define GET_ITEM(ITEM)	GetDlgItem(hwndDlg, (ITEM))

#define SET_CHECK(ITEM, VAL)	SendMessage(GET_ITEM(ITEM), BM_SETCHECK, (WPARAM)(VAL), (LPARAM)0)
#define GET_CHECK(ITEM)			SendMessage(GET_ITEM(ITEM), BM_GETCHECK, (WPARAM)0, (LPARAM)0)

#define SET_FIELD(ITEM, VAL)	SetDlgItemInt(hwndDlg, (ITEM), (VAL), FALSE)
#define GET_FIELD(ITEM)			GetDlgItemInt(hwndDlg, (ITEM), NULL, FALSE)

#define SET_FIELD_TXT(ITEM, STR)		SetDlgItemText(hwndDlg, (ITEM), STR)
#define GET_FIELD_TXT(ITEM, STR, LEN)	GetDlgItemText(hwndDlg, (ITEM), STR, (LEN))

#define SET_SLIDER(ITEM, VAL)	SendMessage(GET_ITEM(ITEM),(UINT)TBM_SETPOS, (WPARAM)(BOOL)TRUE, (LPARAM)(VAL));
#define GET_SLIDER(ITEM)		SendMessage(GET_ITEM(ITEM), TBM_GETPOS, (WPARAM)0, (LPARAM)0 )

#define ADD_MENU_ITEM(MENU, INDEX, STRING, VALUE, SELECTED) \
				SendMessage(GET_ITEM(MENU),( UINT)CB_ADDSTRING, (WPARAM)wParam, (LPARAM)(LPCTSTR)STRING ); \
				SendMessage(GET_ITEM(MENU),(UINT)CB_SETITEMDATA, (WPARAM)INDEX, (LPARAM)(DWORD)VALUE); \
				if(SELECTED) \
					SendMessage(GET_ITEM(MENU), CB_SETCURSEL, (WPARAM)INDEX, (LPARAM)0);

#define GET_MENU_VALUE(MENU)		SendMessage(GET_ITEM(MENU), (UINT)CB_GETITEMDATA, (WPARAM)SendMessage(GET_ITEM(MENU),(UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0), (LPARAM)0)

#define ENABLE_ITEM(ITEM, ENABLE)	EnableWindow(GetDlgItem(hwndDlg, (ITEM)), (ENABLE));


// dialog comtrols
enum {
	OUT_noUI = -1,
	OUT_OK = IDOK,
	OUT_Cancel = IDCANCEL,
	OUT_Compression_Menu = 3,
	OUT_Float_Check,
	OUT_Composite_Check,
	OUT_Hidden_Layers_Check
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


HINSTANCE hDllInstance = NULL;

static WORD	g_item_clicked = 0;

static int	g_Compression	= OUT_PIZ_COMPRESSION;
static bool	g_32bit_float	= FALSE;
static bool	g_composite		= FALSE;
static bool	g_hidden_layers	= FALSE;


static BOOL CALLBACK DialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    BOOL fError; 

    switch (message) 
    { 
		case WM_INITDIALOG:

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

				for(int i=OUT_NO_COMPRESSION; i < OUT_NUM_COMPRESSION_METHODS; i++)
				{
					SendMessage(menu,( UINT)CB_ADDSTRING, (WPARAM)wParam, (LPARAM)(LPCTSTR)opts[i] );
					SendMessage( menu,(UINT)CB_SETITEMDATA, (WPARAM)i, (LPARAM)(DWORD)i); // this is the compresion number

					if(i == g_Compression)
						SendMessage( menu, CB_SETCURSEL, (WPARAM)i, (LPARAM)0);
				}
			}while(0);

			SendMessage(GetDlgItem(hwndDlg, OUT_Float_Check), BM_SETCHECK, (WPARAM)g_32bit_float, (LPARAM)0);
			SendMessage(GetDlgItem(hwndDlg, OUT_Composite_Check), BM_SETCHECK, (WPARAM)g_composite, (LPARAM)0);
			SendMessage(GetDlgItem(hwndDlg, OUT_Hidden_Layers_Check), BM_SETCHECK, (WPARAM)g_hidden_layers, (LPARAM)0);


			return TRUE;

        case WM_COMMAND: 
			g_item_clicked = LOWORD(wParam);

            switch (LOWORD(wParam)) 
            { 
                case OUT_OK: 
				case OUT_Cancel:  // do the same thing, but g_item_clicked will be different
					do{
						HWND menu = GetDlgItem(hwndDlg, OUT_Compression_Menu);

						// get the channel index associated with the selected menu item
						LRESULT cur_sel = SendMessage(menu,(UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);

						g_Compression = SendMessage(menu,(UINT)CB_GETITEMDATA, (WPARAM)cur_sel, (LPARAM)0);
						g_32bit_float = SendMessage(GetDlgItem(hwndDlg, OUT_Float_Check), BM_GETCHECK, (WPARAM)0, (LPARAM)0);
						g_composite = SendMessage(GetDlgItem(hwndDlg, OUT_Composite_Check), BM_GETCHECK, (WPARAM)0, (LPARAM)0);
						g_hidden_layers = SendMessage(GetDlgItem(hwndDlg, OUT_Hidden_Layers_Check), BM_GETCHECK, (WPARAM)0, (LPARAM)0);

					}while(0);

					//PostMessage((HWND)hwndDlg, WM_QUIT, (WPARAM)WA_ACTIVE, lParam);
					EndDialog(hwndDlg, 0);
                    //DestroyWindow(hwndDlg); 

                    return TRUE;
            } 
    } 

    return FALSE; 
}

bool
ProEXR_AE_Out(
	ProEXR_AE_Out_Data	*params,
	const void		*plugHndl,
	const void		*mwnd)
{
	bool hit_ok = false;
	
	// set globals
	g_Compression = params->compression;
	g_32bit_float = params->float_not_half;
	g_composite = params->layer_composite;
	g_hidden_layers = params->hidden_layers;


	int status = DialogBox((HINSTANCE)hDllInstance, (LPSTR)"OUTDIALOG", (HWND)mwnd, (DLGPROC)DialogProc);


	if(g_item_clicked == OUT_OK)
	{
		params->compression = g_Compression;
		params->float_not_half = g_32bit_float;
		params->layer_composite = g_composite;
		params->hidden_layers = g_hidden_layers;

		hit_ok = true;
	}

	return hit_ok;
}

enum {
	GUI_noUI = -1,
	GUI_OK = IDOK,
	GUI_Cancel = IDCANCEL,
	GUI_Path_Field = 3,
	GUI_Browse_Button,
	GUI_Compression_Menu,
	GUI_Float_Check,
	GUI_Composite_Check,
	GUI_Hidden_Layers_Check,
	GUI_Time_Span_Menu,
	GUI_Start_Frame_Field,
	GUI_Start_Frame_Label,
	GUI_End_Frame_Field,
	GUI_End_Frame_Label
};

static std::string g_path;
static TimeSpan g_timespan;
static int g_start_frame;
static int g_end_frame;

static int g_current_frame;
static int g_work_start;
static int g_work_end;
static int g_comp_start;
static int g_comp_end;

static void TrackTimeSpan(HWND hwndDlg)
{
	const TimeSpan timeSpan = (TimeSpan)GET_MENU_VALUE(GUI_Time_Span_Menu);

	if(timeSpan == TIMESPAN_CURRENT_FRAME)
	{
		SET_FIELD(GUI_Start_Frame_Field, g_current_frame);
		SET_FIELD(GUI_End_Frame_Field, g_current_frame);
	}
	else if(timeSpan == TIMESPAN_WORK_AREA)
	{
		SET_FIELD(GUI_Start_Frame_Field, g_work_start);
		SET_FIELD(GUI_End_Frame_Field, g_work_end);
	}
	else if(timeSpan == TIMESPAN_FULL_COMP)
	{
		SET_FIELD(GUI_Start_Frame_Field, g_comp_start);
		SET_FIELD(GUI_End_Frame_Field, g_comp_end);
	}

	const bool custom_enabled = (timeSpan == TIMESPAN_CUSTOM);

	ENABLE_ITEM(GUI_Start_Frame_Field, custom_enabled);
	ENABLE_ITEM(GUI_Start_Frame_Label, custom_enabled);
	ENABLE_ITEM(GUI_End_Frame_Field, custom_enabled);
	ENABLE_ITEM(GUI_End_Frame_Label, custom_enabled);
}

// Oy, what a pain in the neck.  AE uses a 4 byte struct alignment, which messes with
// GetSaveFileName because it's struct sensitive (especially the part about the struct size).
// This pragma temporarily sets us back to 8 bytes, but since OPENFILENAME has already been
// victimized, we copy it here and replace it with our own myOPENFILENAME.
#pragma pack(push, 8)
typedef struct mytagOFNA {
   DWORD        lStructSize;
   HWND         hwndOwner;
   HINSTANCE    hInstance;
   LPCSTR       lpstrFilter;
   LPSTR        lpstrCustomFilter;
   DWORD        nMaxCustFilter;
   DWORD        nFilterIndex;
   LPSTR        lpstrFile;
   DWORD        nMaxFile;
   LPSTR        lpstrFileTitle;
   DWORD        nMaxFileTitle;
   LPCSTR       lpstrInitialDir;
   LPCSTR       lpstrTitle;
   DWORD        Flags;
   WORD         nFileOffset;
   WORD         nFileExtension;
   LPCSTR       lpstrDefExt;
   LPARAM       lCustData;
   LPOFNHOOKPROC lpfnHook;
   LPCSTR       lpTemplateName;
#ifdef _MAC
   LPEDITMENU   lpEditInfo;
   LPCSTR       lpstrPrompt;
#endif
#if (_WIN32_WINNT >= 0x0500)
   void *        pvReserved;
   DWORD        dwReserved;
   DWORD        FlagsEx;
#endif // (_WIN32_WINNT >= 0x0500)
} myOPENFILENAMEA, *myLPOPENFILENAMEA;
typedef struct mytagOFNW {
   DWORD        lStructSize;
   HWND         hwndOwner;
   HINSTANCE    hInstance;
   LPCWSTR      lpstrFilter;
   LPWSTR       lpstrCustomFilter;
   DWORD        nMaxCustFilter;
   DWORD        nFilterIndex;
   LPWSTR       lpstrFile;
   DWORD        nMaxFile;
   LPWSTR       lpstrFileTitle;
   DWORD        nMaxFileTitle;
   LPCWSTR      lpstrInitialDir;
   LPCWSTR      lpstrTitle;
   DWORD        Flags;
   WORD         nFileOffset;
   WORD         nFileExtension;
   LPCWSTR      lpstrDefExt;
   LPARAM       lCustData;
   LPOFNHOOKPROC lpfnHook;
   LPCWSTR      lpTemplateName;
#ifdef _MAC
   LPEDITMENU   lpEditInfo;
   LPCSTR       lpstrPrompt;
#endif
#if (_WIN32_WINNT >= 0x0500)
   void *        pvReserved;
   DWORD        dwReserved;
   DWORD        FlagsEx;
#endif // (_WIN32_WINNT >= 0x0500)
} myOPENFILENAMEW, *myLPOPENFILENAMEW;
#ifdef UNICODE
typedef myOPENFILENAMEW myOPENFILENAME;
typedef myLPOPENFILENAMEW myLPOPENFILENAME;
#else
typedef myOPENFILENAMEA myOPENFILENAME;
typedef myLPOPENFILENAMEA myLPOPENFILENAME;
#endif // UNICODE

static void BrowseFile(HWND hwndDlg)
{
	char path[256];
	GET_FIELD_TXT(GUI_Path_Field, path, 255);

	//char initial_filename[256];
	//GET_FIELD_TXT(GUI_Path_Field, initial_filename, 255);
	//PathStripPath(initial_filename);

	const char *my_lpstrDefExt = "exr";
	const char *my_lpstrFilter = "ProEXR (*.exr)\0*.exr\0\0\0";
	const char *my_lpstrTitle = "ProEXR";

	myOPENFILENAME lpofn;

	lpofn.lStructSize = sizeof(myOPENFILENAME);
	lpofn.hwndOwner = hwndDlg;
	lpofn.hInstance = (HINSTANCE)hDllInstance;
	lpofn.lpstrFilter = my_lpstrFilter;
	lpofn.lpstrCustomFilter = NULL;
	lpofn.nMaxCustFilter = 0;
	lpofn.nFilterIndex = 0;
	lpofn.lpstrFile = path;
	lpofn.nMaxFile = 255;
	lpofn.lpstrFileTitle = NULL;
	lpofn.nMaxFileTitle = 0;
	lpofn.lpstrInitialDir = NULL;
	lpofn.lpstrTitle = my_lpstrTitle;
	lpofn.Flags = OFN_LONGNAMES |
					OFN_HIDEREADONLY | 
					OFN_PATHMUSTEXIST |
					OFN_OVERWRITEPROMPT;
	lpofn.nFileOffset = 0;
	lpofn.nFileExtension = 0;
	lpofn.lpstrDefExt = my_lpstrDefExt;
	lpofn.lCustData = 0;
	lpofn.lpfnHook = NULL;
	lpofn.lpTemplateName = NULL;

	const BOOL result = GetSaveFileName((LPOPENFILENAME)&lpofn);

	if(result)
	{
		SET_FIELD_TXT(GUI_Path_Field, path);
	}
	else
	{
		DWORD err = CommDlgExtendedError();

		if(err != 0)
		{
			if(err == CDERR_STRUCTSIZE)
			{
				MessageBox(hwndDlg, "Bad Struct size", "WTF", MB_OK);
			}
			else
				MessageBox(hwndDlg, "WTF", "WTF", MB_OK);
		}
	}
}
#pragma pack(pop)

static BOOL CALLBACK GUIDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    BOOL fError; 

    switch (message) 
    { 
		case WM_INITDIALOG:

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

				HWND comp_menu = GetDlgItem(hwndDlg, GUI_Compression_Menu);

				for(int i=OUT_NO_COMPRESSION; i < OUT_NUM_COMPRESSION_METHODS; i++)
				{
					SendMessage(comp_menu,( UINT)CB_ADDSTRING, (WPARAM)wParam, (LPARAM)(LPCTSTR)opts[i] );
					SendMessage(comp_menu,(UINT)CB_SETITEMDATA, (WPARAM)i, (LPARAM)(DWORD)i); // this is the compresion number

					if(i == g_Compression)
						SendMessage(comp_menu, CB_SETCURSEL, (WPARAM)i, (LPARAM)0);
				}
				

				const char *timeSpans[] = { "Current Frame", "Work Area", "Full Comp", "Custom" };

				HWND timespan_menu = GetDlgItem(hwndDlg, GUI_Time_Span_Menu);

				for(int i=TIMESPAN_CURRENT_FRAME; i <= TIMESPAN_CUSTOM; i++)
				{
					SendMessage(timespan_menu,( UINT)CB_ADDSTRING, (WPARAM)wParam, (LPARAM)(LPCTSTR)timeSpans[i] );
					SendMessage(timespan_menu,(UINT)CB_SETITEMDATA, (WPARAM)i, (LPARAM)(DWORD)i); // this is the timespan number

					if(i == g_timespan)
						SendMessage(timespan_menu, CB_SETCURSEL, (WPARAM)i, (LPARAM)0);
				}

			}while(0);
			
			SET_FIELD_TXT(GUI_Path_Field, g_path.c_str());
			SET_CHECK(GUI_Float_Check, g_32bit_float);
			SET_CHECK(GUI_Composite_Check, g_composite);
			SET_CHECK(GUI_Hidden_Layers_Check, g_hidden_layers);
			SET_FIELD(GUI_Start_Frame_Field, g_start_frame);
			SET_FIELD(GUI_End_Frame_Field, g_end_frame);

			TrackTimeSpan(hwndDlg);

			if(g_path[1] != ':')
				BrowseFile(hwndDlg);

			return TRUE;

        case WM_COMMAND: 
			g_item_clicked = LOWORD(wParam);

            switch (LOWORD(wParam)) 
            { 
                case GUI_OK: 
				case GUI_Cancel:  // do the same thing, but g_item_clicked will be different
					do{
						char path[256];
						GET_FIELD_TXT(GUI_Path_Field, path, 255);
						
						g_path = path;
						g_Compression = GET_MENU_VALUE(GUI_Compression_Menu);
						g_32bit_float = GET_CHECK(GUI_Float_Check);
						g_composite = GET_CHECK(GUI_Composite_Check);
						g_hidden_layers = GET_CHECK(GUI_Hidden_Layers_Check);
						g_timespan = (TimeSpan)GET_MENU_VALUE(GUI_Time_Span_Menu);
						g_start_frame = GET_FIELD(GUI_Start_Frame_Field);
						g_end_frame = GET_FIELD(GUI_End_Frame_Field);

					}while(0);

					//PostMessage((HWND)hwndDlg, WM_QUIT, (WPARAM)WA_ACTIVE, lParam);
					EndDialog(hwndDlg, 0);
                    //DestroyWindow(hwndDlg); 

                    return TRUE;

				case GUI_Time_Span_Menu:
					TrackTimeSpan(hwndDlg);
					return TRUE;

				case GUI_Browse_Button:
					BrowseFile(hwndDlg);
					return TRUE;
            } 
    } 

    return FALSE; 
}

bool
ProEXR_AE_GUI(
	ProEXR_AE_GUI_Data	*params,
	std::string		&path,
	int				current_frame,
	int				work_start,
	int				work_end,
	int				comp_start,
	int				comp_end,
	const void		*plugHndl,
	const void		*mwnd)
{
	bool hit_ok = false;
	
	// set globals
	g_path = path;
	g_Compression = params->compression;
	g_32bit_float = params->float_not_half;
	g_composite = params->layer_composite;
	g_hidden_layers = params->hidden_layers;
	g_timespan = params->timeSpan;
	g_start_frame = params->start_frame;
	g_end_frame = params->end_frame;

	g_current_frame = current_frame;
	g_work_start = work_start;
	g_work_end = work_end;
	g_comp_start = comp_start;
	g_comp_end = comp_end;



	int status = DialogBox((HINSTANCE)hDllInstance, (LPSTR)"GUIDIALOG", (HWND)mwnd, (DLGPROC)GUIDialogProc);


	if(g_item_clicked == GUI_OK)
	{
		path = g_path;
		params->compression = g_Compression;
		params->float_not_half = g_32bit_float;
		params->layer_composite = g_composite;
		params->hidden_layers = g_hidden_layers;
		params->timeSpan = g_timespan;
		params->start_frame = g_start_frame;
		params->end_frame = g_end_frame;

		hit_ok = true;
	}

	return hit_ok;
}


static IProgressDialog *g_progress_dlog = NULL;

bool
ProEXR_AE_Update_Progress(
	int				current_frame,
	int				total_frames,
	const void		*plugHndl,
	const void		*mwnd)
{
	if(g_progress_dlog == NULL)
	{
		CoCreateInstance(CLSID_ProgressDialog, NULL, CLSCTX_INPROC_SERVER, IID_IProgressDialog, (void **)&g_progress_dlog);

		if(g_progress_dlog != NULL)
		{
			g_progress_dlog->SetTitle(L"ProEXR");
			g_progress_dlog->StartProgressDialog((HWND)mwnd, NULL, PROGDLG_MODAL | PROGDLG_AUTOTIME, NULL);
			g_progress_dlog->SetCancelMsg(L"Please wait while ProEXR renders the current frame", NULL);
		}
	}

	if(g_progress_dlog != NULL)
	{
		g_progress_dlog->SetProgress(current_frame - 1, total_frames);

		std::wstringstream sstream;
		sstream << "Rendering frame " << current_frame << " of " << total_frames;
		g_progress_dlog->SetLine(2, sstream.str().c_str(), FALSE, NULL);

		return !g_progress_dlog->HasUserCancelled();
	}

	return true;
}


void
ProEXR_AE_End_Progress()
{
	if(g_progress_dlog != NULL)
	{
        g_progress_dlog->StopProgressDialog();

        g_progress_dlog->Release();

		g_progress_dlog = NULL;
	}
}


void
ProEXR_CopyPluginPath(
	char				*pathZ,
	int					max_len)
{
	DWORD result = GetModuleFileName(hDllInstance, pathZ, max_len);
}


BOOL WINAPI DllMain(HANDLE hInstance, DWORD fdwReason, LPVOID lpReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
		hDllInstance = (HINSTANCE)hInstance;

	return TRUE;   // Indicate that the DLL was initialized successfully.
}
