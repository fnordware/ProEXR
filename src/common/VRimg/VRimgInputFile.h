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


#ifndef VRIMG_INPUT_FILE_H
#define VRIMG_INPUT_FILE_H

#include "VRimgHeader.h"

#ifdef USE_ROPE
#include <ext/rope>
typedef __gnu_cxx::crope Rope;
#else
typedef std::string Rope;
#endif


namespace VRimg {

class InputFile
{
  public:
	InputFile(Imf::IStream &is);
	~InputFile();

	const Header & header() const { return _header; }
	
	// used for pre-loading the whole image
	typedef std::map<std::string, void *> BufferMap;
	
	void loadFromFile(BufferMap *buf_map=NULL);
	
	void copyLayerToBuffer(const std::string &name, void *buf, size_t rowbytes);
	
	Rope getXMPdescription() const;
	
  private:
	void AddDescription(Rope &xmp) const;
	void DescribeTag(Rope &xmp) const;
  
	Header _header;

	Imf::IStream &_is;
	
	BufferMap _map;
	Rope _desc;
	
	void freeBuffers();
};

} // namespace VRimg

#endif // VRIMG_INPUT_FILE_H
