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
	{357, 769, 714, 1059},
	movableDBoxProc,
	visible,
	noGoAway,
	0x0,
	OutID,
	"ProEXR EZ Options",
	alertPositionMainScreen	// Universal 3.0 headers.
};

resource 'dlgx' (OutID) {
	versionZero {
		kDialogFlagsHandleMovableModal + kDialogFlagsUseThemeControls + kDialogFlagsUseThemeBackground + kDialogFlagsUseControlHierarchy
	}
};

#define MenuID	2001

#define GroupID	2222

resource 'DITL' (OutID, "Dialog Items", purgeable)
{
	{
		{317, 201, 337, 270},	Button { enabled, "OK" },
		{317, 120, 337, 189}, 	Button { enabled, "Cancel" },
		{82, 21, 98, 123},		StaticText { disabled, "Compression:" },
		{81, 131, 101, 251},	Control { enabled, MenuID },
		{121, 43, 139, 188},	CheckBox { enabled, "Luminance/Chroma"},
		{145, 43, 163, 141},	CheckBox { enabled, "32-bit float"},
		{161, 63, 172, 177},	StaticText { disabled, "(not recommended)" },
		{189, 31, 287, 251},	Control { enabled, GroupID },
		{210, 46, 230, 103},	RadioButton { enabled, "None" },
		{233, 46, 253, 152},	RadioButton { enabled, "Transparency" },
		{256, 46, 276, 247},	RadioButton { enabled, "Channel Blah" },
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
	"",
	{	/* array: 4 elements */
		/* [1] */
		"None", noIcon, noKey, noMark, plain,
	}
};

resource 'CNTL' (GroupID) {
	{189, 31, 287, 251},
	0,
	visible,
	0,
	0,
	kControlGroupBoxTextTitleProc,
	0,
	"Alpha Channel"
};
 
// http://developer.apple.com/DOCUMENTATION/macos8/HumanInterfaceToolbox/ControlManager/ControlMgr8Ref/ControlMgrRef.10.html



resource 'DLOG' (InID, "Import Options", purgeable)
{
	{493, 665, 772, 1020},
	movableDBoxProc,
	visible,
	noGoAway,
	0x0,
	InID,
	"ProEXR EZ Import Options",
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
		{244, 249, 264, 335},	Button { enabled, "OK" },
		{244, 151, 264, 237}, 	Button { enabled, "Cancel" },
        {244, 20, 264, 124}, 	Button { enabled, "Set Defaults" },
		{79, 55, 98, 308},		RadioButton { enabled, "Alpha makes layer transparent" },
        {104, 55, 123, 308},	RadioButton { enabled, "Alpha appears as a separate channel" },
		{131, 86, 149, 286},	CheckBox { enabled, "UnMultiply RGB using Alpha"},
		{165, 55, 183, 286},	CheckBox { enabled, "Memory mapping"},
		{204, 55, 222, 286},	CheckBox { enabled, "Always bring up this dialog"},
		{7, 52, 57, 302},		Picture { disabled, 1901 },
	}
};
