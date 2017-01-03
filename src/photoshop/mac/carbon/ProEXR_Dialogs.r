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


#include "Types.r"
#include "SysTypes.r"


#define OutID	16100
#define AboutID	16101
#define RegID	16102
#define InID    16103

resource 'DLOG' (OutID, "Save Options", purgeable)
{
	{449, 541, 754, 831},
	movableDBoxProc,
	visible,
	noGoAway,
	0x0,
	OutID,
	"ProEXR Options",
	alertPositionMainScreen	// Universal 3.0 headers.
};

resource 'dlgx' (OutID) {
	versionZero {
		kDialogFlagsHandleMovableModal + kDialogFlagsUseThemeControls + kDialogFlagsUseThemeBackground + kDialogFlagsUseControlHierarchy
	}
};

#define MenuID	2001


resource 'DITL' (OutID, "Dialog Items", purgeable)
{
	{
		{265, 201, 285, 270},	Button { enabled, "OK" },
		{265, 120, 285, 189}, 	Button { enabled, "Cancel" },
		{82, 21, 98, 123},		StaticText { disabled, "Compression:" },
		{81, 131, 101, 251},	Control {enabled, MenuID},
		{121, 43, 139, 188},	CheckBox { enabled, "Luminance/Chroma"},
		{145, 43, 163, 141},	CheckBox { enabled, "32-bit float"},
		{195, 43, 213, 219},	CheckBox { enabled, "Include layer composite"},
		{219, 43, 237, 219},	CheckBox { enabled, "Include hidden layers"},
		{5, 20, 55, 270},		Picture { disabled, 1901 },
	}
};

resource 'CNTL' (MenuID, purgeable) {
	{81, 131, 101, 251},
	0,
	visible,
	0,
	MenuID,
	1008,
	0,
	""
};

resource 'MENU' (MenuID) {
	MenuID,
	textMenuProc,
	allEnabled,
	enabled,
	"Compression",
	{	/* array: 4 elements */
		/* [1] */
		"None", noIcon, noKey, noMark, plain,
	}
};


resource 'DLOG' (InID, "Import Options", purgeable)
{
	{493, 665, 826, 1020},
	movableDBoxProc,
	visible,
	noGoAway,
	0x0,
	InID,
	"ProEXR Import Options",
	alertPositionMainScreen	// Universal 3.0 headers.
};

resource 'dlgx' (InID) {
	versionZero {
		kDialogFlagsHandleMovableModal + kDialogFlagsUseThemeControls + kDialogFlagsUseThemeBackground + kDialogFlagsUseControlHierarchy
	}
};

resource 'DITL' (InID, "Import Items", purgeable)
{
	{
		{293, 249, 313, 335},	Button { enabled, "OK" },
		{293, 151, 313, 237}, 	Button { enabled, "Cancel" },
        {293, 20, 313, 124}, 	Button { enabled, "Set Defaults" },
		{79, 62, 98, 302},		RadioButton { enabled, "Alpha makes layers transparent" },
        {104, 62, 123, 302},	RadioButton { enabled, "Alpha appears on separate layers" },
		{131, 86, 149, 286},	CheckBox { enabled, "UnMultiply RGB using Alpha"},
		{177, 62, 195, 286},	CheckBox { enabled, "Ignore custom layering attribute"},
		{202, 62, 220, 286},	CheckBox { enabled, "Memory mapping"},
		{249, 62, 267, 286},	CheckBox { enabled, "Always bring up this dialog"},
		{7, 52, 57, 302},		Picture { disabled, 1901 },
	}
};
