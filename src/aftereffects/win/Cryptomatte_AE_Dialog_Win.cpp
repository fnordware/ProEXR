
//
//	Cryptomatte AE plug-in
//		by Brendan Bolles <brendan@fnordware.com>
//
//
//	Part of ProEXR
//		http://www.fnordware.com/ProEXR
//
//


#include "Cryptomatte_AE_Dialog.h"

#include <windows.h>
//#include <Shlwapi.h>
//#include <ShlObj.h>

//#include <sstream>

#include <assert.h>



// dialog comtrols
enum {
	DLOG_noUI = -1,
	DLOG_OK = IDOK,
	DLOG_Cancel = IDCANCEL,
	DLOG_Layer_Text = 3,
	DLOG_Selection_Text,
	DLOG_Manifest_Text
};


static HINSTANCE hDllInstance = NULL;

static WORD	g_item_clicked = 0;

static std::string g_layer;
static std::string g_selection;
static std::string g_manifest;


static void SetStrFromField(HWND hDlg, int nIDDlgItem, std::string &str)
{
	const int len = SendMessage(GetDlgItem(hDlg, nIDDlgItem), WM_GETTEXTLENGTH, (WPARAM)0, (LPARAM)0) + 1;

	char *buf = (char *)malloc(len * sizeof(char));

	if(buf)
	{
		const UINT copied = SendMessage(GetDlgItem(hDlg, nIDDlgItem), WM_GETTEXT, (WPARAM)len, (LPARAM)buf);

		assert(copied == len - 1);

		str = buf;

		free(buf);
	}
}


static BOOL CALLBACK DialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    BOOL fError; 

    switch(message) 
    { 
		case WM_INITDIALOG:

			SetDlgItemText(hwndDlg, DLOG_Layer_Text, g_layer.c_str());
			SetDlgItemText(hwndDlg, DLOG_Selection_Text, g_selection.c_str());
			SetDlgItemText(hwndDlg, DLOG_Manifest_Text, g_manifest.c_str());

			return TRUE;

        case WM_COMMAND: 
			const WORD item_clicked = LOWORD(wParam);

			if(item_clicked == DLOG_OK || item_clicked == DLOG_Cancel)
			{
				g_item_clicked = item_clicked;

				switch(item_clicked) 
				{ 
					case DLOG_OK: 
						SetStrFromField(hwndDlg, DLOG_Layer_Text, g_layer);
						SetStrFromField(hwndDlg, DLOG_Selection_Text, g_selection);
						SetStrFromField(hwndDlg, DLOG_Manifest_Text, g_manifest);

					case DLOG_Cancel:

						//PostMessage((HWND)hwndDlg, WM_QUIT, (WPARAM)WA_ACTIVE, lParam);
						EndDialog(hwndDlg, 0);
						//DestroyWindow(hwndDlg); 

						return TRUE;
				} 
			}
    } 

    return FALSE; 
}

bool Cryptomatte_Dialog(
	std::string			&layer,
	std::string			&selection,
	std::string			&manifest,
	const char			*plugHndl,
	const void			*mwnd)
{
	bool hit_ok = false;
	
	// set globals
	g_layer = layer;
	g_selection = selection;
	g_manifest = manifest;


	int status = DialogBox((HINSTANCE)hDllInstance, (LPSTR)"DIALOG", (HWND)mwnd, (DLGPROC)DialogProc);


	if(g_item_clicked == DLOG_OK)
	{
		layer = g_layer;
		selection = g_selection;
		manifest = g_manifest;

		hit_ok = true;
	}

	return hit_ok;
}

BOOL WINAPI DllMain(HANDLE hInstance, DWORD fdwReason, LPVOID lpReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
		hDllInstance = (HINSTANCE)hInstance;

	return TRUE;   // Indicate that the DLL was initialized successfully.
}
