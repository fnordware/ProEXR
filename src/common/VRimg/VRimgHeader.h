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


#ifndef VRIMG_HEADER_H
#define VRIMG_HEADER_H

#include <map>
#include <string>

#include <ImfStdIO.h>


namespace VRimg {

typedef enum
{
    FLOAT = 2,		// float (32 bit floating point)
    INT  = 4,		// int (32 bit)

    NUM_PIXELTYPES = 2	// number of different pixel types
} PixelType;

#define RIF_FLAG_COMPRESSION	0x00000001

typedef enum  {
	RIT_RESOLUTION=0x100, ///< A tag with this ID contains information about the image resolution.
	RIT_REGIONSINFO, ///< A tag with this ID contains information about the regions (buckets) of the image.
	RIT_CHAN3F, ///< A tag with this ID contains pixel color information in floating-point RGB format, or 3D vector information in XYZ format.
	RIT_CHAN2F, ///< A tag with this ID contains pixel 2D vector information in floating-point XY format.
	RIT_CHANF, ///< A tag with this ID contains pixel floating point information.
	RIT_CHANI, ///< A tag with this ID contains pixel 32-bit integer information.
	RIT_CHAN_INFO, ///< A tag with ID contains information about the channels in the image.
	RIT_INDEX, ///< A tag with this ID contains the offset of the tag table in the file.
	RIT_CAMERA_INFO, ///< A tag with this ID contains information about the camera.
	RIT_RENDER_REGION, ///< A tag with information about the render region (data window).
	RIT_PREVIEW, ///< A tag for storing image preview directly in the file.
	RIT_SCENE_INFO,///< A tag with this ID contains information about the scene.
	RIT_STAMP, ///< A tag with this ID contains the stamp for the image.
	RIT_NOTE, ///< A tag with this ID contains user note about the image,
} RIF_TAG_ID;
  
 /// A single tag in the file. The tag is immediately followed by any data associated with it.
struct RIF_TAG {
	unsigned int tag; ///< The tag identifier. See RIF_TAG_ID for more information.
	unsigned int tagsize; ///< The size of the tag in bytes. This includes the size of this structure plus the size in bytes of any additional data for the tag.
	unsigned int p0;
	unsigned int p1;
	unsigned int p2;
	unsigned int p3;
	unsigned int p4;
	unsigned int p5;
	unsigned int p6;
	unsigned int p7;
	RIF_TAG() { tag=0; tagsize=0; p0=p1=p2=p3=p4=p5=p6=p7=0; }
};

typedef struct Layer
{
	int index;
	PixelType type;
	int alias;
	unsigned int flags;
	
	int dimensions;
	
	Layer(int i = 0, PixelType t = FLOAT, int a = 0, unsigned int f = 0, int d = 0) { index = i; type = t; alias = a; flags = f; dimensions = d; }
} Layer;


class Header
{
  public:
  
	Header(int width = 64, int height = 64);
	~Header();
	
	unsigned int width() const { return _width; }
	unsigned int height() const { return _height; }
	float pixelAspectRatio() const { return _pixelAspectRatio; }
	
	bool isCompressed() const { return (_flags & RIF_FLAG_COMPRESSION); }
	
	void readFrom(Imf::IStream &is);
	
	
	typedef std::map<std::string, Layer> LayerMap;
	
	const Layer *findLayer(std::string name) const;
	
	const LayerMap & layers() const { return _map; }
	
	
  private:
  
	bool moveToTag(Imf::IStream &is, RIF_TAG_ID tag, bool brute_force);
	bool parseTag(Imf::IStream &is, RIF_TAG_ID tag, bool brute_force = false);
  
	void parseTag(Imf::IStream &is, bool skip_ahead = true);
	
	Imf::Int64 _indexPos;
	unsigned int _flags;
	
	unsigned int _width;
	unsigned int _height;
	float _pixelAspectRatio;
	
	LayerMap _map;
};


} // namespace VRimg


#endif // VRIMG_HEADER_H