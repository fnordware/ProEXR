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

#include "ProEXR_Version.h"

#define OutID	16100
#define AboutID	16101
#define RegID	16102
#define InID    16103



resource 'DLOG' (AboutID, "About", purgeable)
{
	{797, 504, 1016, 794},
	movableDBoxProc,
	visible,
	noGoAway,
	0x0,
	AboutID,
	"About ProEXR",
	alertPositionMainScreen	// Universal 3.0 headers.
};

resource 'dlgx' (AboutID) {
	versionZero {
		kDialogFlagsHandleMovableModal + kDialogFlagsUseThemeControls + kDialogFlagsUseThemeBackground + kDialogFlagsUseControlHierarchy
	}
};

resource 'DITL' (AboutID, "About Items", purgeable)
{
	{
		{179, 110, 199, 179},	Button { enabled, "OK" },
		{63, 20, 79, 270},		StaticText { disabled, "ProEXR" },
		{79, 20, 95, 270},		StaticText { disabled, "by Brendan Bolles" },
		{95, 20, 111, 270},		StaticText { disabled, "www.fnordware.com" },
		{130, 20, 146, 270},	StaticText { disabled, "v" ProEXR_Version_String " - " ProEXR_Build_Date_Manual },
		{146, 20, 175, 270},	StaticText { disabled, "Portions copyright Industrial Light + Magic.\rSee documentation for details." },
		{5, 20, 55, 270},		Picture { disabled, 1901 },
	}
};


