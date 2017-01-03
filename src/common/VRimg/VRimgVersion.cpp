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


#include "VRimgVersion.h"

namespace VRimg {

bool isVRimgMagic(const char bytes[4])
{
    return bytes[0] == ((MAGIC >>  0) & 0x00ff) &&
	   bytes[1] == ((MAGIC >>  8) & 0x00ff) &&
	   bytes[2] == ((MAGIC >> 16) & 0x00ff) &&
	   bytes[3] == ((MAGIC >> 24) & 0x00ff);
}

} // namespace VRimg
