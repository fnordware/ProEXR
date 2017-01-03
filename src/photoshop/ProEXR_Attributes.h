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

#ifndef __ProEXR_Attributes__
#define __ProEXR_Attributes__


#include <ProEXRdoc.h>

void AddExtraAttributes(Imf::Header &header);
std::string CreateXMPdescription(const ProEXRdoc_read &file);
void ParseXMPdescription(Imf::Header &header, int width, int height, const char *xmp_text, size_t xmp_size);

#endif // __ProEXR_Attributes__