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


#include "ProEXR_About.h"

extern HANDLE hDllInstance;

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


    xOrigin = max(rcParent.right - rcParent.left - nWidth, 0) / 2
            + rcParent.left - nTitleBits;

    xScreen = GetSystemMetrics(SM_CXSCREEN);

    if  (xOrigin + nWidth > xScreen)
        xOrigin = max (0, xScreen - nWidth);

	yOrigin = max(rcParent.bottom - rcParent.top - nHeight, 0) / 3
            + rcParent.top;

    yScreen = GetSystemMetrics(SM_CYSCREEN);

    if  (yOrigin + nHeight > yScreen)
        yOrigin = max(0 , yScreen - nHeight);

    SetWindowPos(hDlg, NULL, xOrigin, yOrigin, nWidth, nHeight, SWP_NOZORDER);
}

BOOL CALLBACK AboutProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    BOOL fError; 

    switch (message) 
    {
		case WM_INITDIALOG:
			PS6CenterDialog(hwndDlg);
			break;

        case WM_COMMAND: 

            switch (LOWORD(wParam)) 
            { 
                case IDOK: 
					//PostMessage((HWND)hwndDlg, WM_QUIT, (WPARAM)WA_ACTIVE, lParam);
					EndDialog(hwndDlg, 0);
                    //DestroyWindow(hwndDlg); 

                    return TRUE;
            } 
    } 

    return FALSE; 
}


void ProEXR_About(HWND hwndOwner)
{
	int status = DialogBox((HINSTANCE)hDllInstance, (LPSTR)"ABOUTDIALOG", hwndOwner, (DLGPROC)AboutProc);
}
