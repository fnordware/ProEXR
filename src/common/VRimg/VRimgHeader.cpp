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


#include "VRimgHeader.h"

#include "VRimgVersion.h"

#include "VRimgXdr.h"

using namespace std;

namespace VRimg {

union IntAndFloat {
	float f;
	int i;
};



Header::Header(int width, int height) :
	_indexPos(0),
	_flags(0),
	_width(width),
	_height(height),
	_pixelAspectRatio(1.f)
{

}


Header::~Header()
{

}


void
Header::readFrom(Imf::IStream &is)
{
	unsigned int magic;
	
	Xdr::read<Imf::StreamIO>(is, magic);
	
	if(magic != MAGIC)
		throw Iex::InputExc("File is not an VRimg file.");
	
	unsigned int versionmajor, versionminor;
	
	Xdr::read<Imf::StreamIO>(is, versionmajor);
	Xdr::read<Imf::StreamIO>(is, versionminor);
	
	unsigned int indexPosLS, indexPosMS;
	
	Xdr::read<Imf::StreamIO>(is, indexPosLS);
	Xdr::read<Imf::StreamIO>(is, indexPosMS);
	
	_indexPos = ( ((Imf::Int64)indexPosMS << 32) | indexPosLS );
	
	
	Xdr::read<Imf::StreamIO>(is, _flags);
	
	
	unsigned int res1, res2;
	
	Xdr::read<Imf::StreamIO>(is, res1);
	Xdr::read<Imf::StreamIO>(is, res2);
	
	
	// get required tags
	// not using the RIT_INDEX (brute_force = true) because these tags are
	// usually near the beginning and RIT_CHAN_INFO not listed in index for some reason
	if( !parseTag(is, RIT_RESOLUTION, true) )
		throw Iex::InputExc("Resolution not found.");
	
	if( !parseTag(is, RIT_CHAN_INFO, true) )
		throw Iex::InputExc("Channel list not found.");
		
	
	if(_map.size() < 1)
		throw Iex::LogicExc("No channels in file.");
}


const Layer *
Header::findLayer(string name) const
{
	LayerMap::const_iterator the_layer = _map.find(name);
	
	if(the_layer == _map.end())
	{
		return NULL;
	}
	else
		return &the_layer->second;
}


bool
Header::moveToTag(Imf::IStream &is, RIF_TAG_ID tag, bool brute_force)
{
	if(_indexPos != 0 && !brute_force)
	{
		is.seekg(_indexPos);
		
		RIF_TAG index;
		
		Xdr::read<Imf::StreamIO>(is, index.tag);
		Xdr::read<Imf::StreamIO>(is, index.tagsize);
		Xdr::read<Imf::StreamIO>(is, index.p0);
		Xdr::read<Imf::StreamIO>(is, index.p1);
		Xdr::read<Imf::StreamIO>(is, index.p2);
		Xdr::read<Imf::StreamIO>(is, index.p3);
		Xdr::read<Imf::StreamIO>(is, index.p4);
		Xdr::read<Imf::StreamIO>(is, index.p5);
		Xdr::read<Imf::StreamIO>(is, index.p6);
		Xdr::read<Imf::StreamIO>(is, index.p7);
		
		if(index.tag != RIT_INDEX)
			throw Iex::InputExc ("Bogus index tag entry.");
		
		int num_tags = index.p0;
		
		while(num_tags--)
		{
			RIF_TAG index_tag;
			
			Xdr::read<Imf::StreamIO>(is, index_tag.tag);
			Xdr::read<Imf::StreamIO>(is, index_tag.tagsize);
			Xdr::read<Imf::StreamIO>(is, index_tag.p0);
			Xdr::read<Imf::StreamIO>(is, index_tag.p1);
			Xdr::read<Imf::StreamIO>(is, index_tag.p2);
			Xdr::read<Imf::StreamIO>(is, index_tag.p3);
			Xdr::read<Imf::StreamIO>(is, index_tag.p4);
			Xdr::read<Imf::StreamIO>(is, index_tag.p5);
			Xdr::read<Imf::StreamIO>(is, index_tag.p6);
			Xdr::read<Imf::StreamIO>(is, index_tag.p7);
			
			Imf::Int64 file_offset;
			
			Xdr::read<Imf::StreamIO>(is, file_offset);
			
			
			if(index_tag.tag == tag)
			{
				is.seekg(file_offset);
				
				return true;
			}
		}
		
		// Guess we'll have to try brute_force after all
		return moveToTag(is, tag, true);
	}
	else
	{
		is.seekg(0);
		
		// cruise through header
		unsigned int magic, versionmajor, versionminor, indexPosLS, indexPosMS, flags, res1, res2;
		
		Xdr::read<Imf::StreamIO>(is, magic);
		Xdr::read<Imf::StreamIO>(is, versionmajor);
		Xdr::read<Imf::StreamIO>(is, versionminor);
		Xdr::read<Imf::StreamIO>(is, indexPosLS);
		Xdr::read<Imf::StreamIO>(is, indexPosMS);
		Xdr::read<Imf::StreamIO>(is, flags);
		Xdr::read<Imf::StreamIO>(is, res1);
		Xdr::read<Imf::StreamIO>(is, res2);
		
		// we just keep going until an exception is thrown by seekg or read
		try{
			do{
				Imf::Int64 start_pos = is.tellg();

				RIF_TAG file_tag;
				
				Xdr::read<Imf::StreamIO>(is, file_tag.tag);
				Xdr::read<Imf::StreamIO>(is, file_tag.tagsize);
				Xdr::read<Imf::StreamIO>(is, file_tag.p0);
				Xdr::read<Imf::StreamIO>(is, file_tag.p1);
				Xdr::read<Imf::StreamIO>(is, file_tag.p2);
				Xdr::read<Imf::StreamIO>(is, file_tag.p3);
				Xdr::read<Imf::StreamIO>(is, file_tag.p4);
				Xdr::read<Imf::StreamIO>(is, file_tag.p5);
				Xdr::read<Imf::StreamIO>(is, file_tag.p6);
				Xdr::read<Imf::StreamIO>(is, file_tag.p7);
				
				if(file_tag.tag == tag)
				{
					is.seekg(start_pos);
					
					return true;
				}
				else
				{
					is.seekg(start_pos + file_tag.tagsize);
				}
				
			}while(1);
		}
		catch(Iex::IoExc &e) {}
		
		return false;
	}
}


bool
Header::parseTag(Imf::IStream &is, RIF_TAG_ID tag, bool brute_force)
{
	if( moveToTag(is, tag, brute_force) )
	{
		parseTag(is, false);
		
		return true;
	}
	else
		return false;
}


void
Header::parseTag(Imf::IStream &is, bool skip_ahead)
{
	Imf::Int64 start_pos = is.tellg();

	RIF_TAG tag;
	
	Xdr::read<Imf::StreamIO>(is, tag.tag);
	Xdr::read<Imf::StreamIO>(is, tag.tagsize);
	Xdr::read<Imf::StreamIO>(is, tag.p0);
	Xdr::read<Imf::StreamIO>(is, tag.p1);
	Xdr::read<Imf::StreamIO>(is, tag.p2);
	Xdr::read<Imf::StreamIO>(is, tag.p3);
	Xdr::read<Imf::StreamIO>(is, tag.p4);
	Xdr::read<Imf::StreamIO>(is, tag.p5);
	Xdr::read<Imf::StreamIO>(is, tag.p6);
	Xdr::read<Imf::StreamIO>(is, tag.p7);
	
	switch(tag.tag)
	{
		case RIT_RESOLUTION:
			do{
				_width = tag.p0;
				_height = tag.p1;
				
				IntAndFloat bitAspect;
				bitAspect.f = 1.0f;
				
				if(tag.p2)
					bitAspect.i = tag.p2;
				
				_pixelAspectRatio = bitAspect.f;
			}while(0);
			break;
			
/* -- commenting out tags we don't actually parse into useful information
		case RIT_REGIONSINFO:
			do{
				unsigned int regWidth = tag.p0; // regWidth The width of a regions.
				unsigned int regHeight = tag.p1; // regHeight The height of the regions.
				unsigned int whatisxy = tag.p2; // If this is 0, the regWidth and regHeight parameters are the maximum width and height of regions in pixels.
												// If whatisxy is 1, then regWidth and regHeight are number of regions horizontally and vertically for the image.
			}while(0);
			break;
		
		
		case RIT_RENDER_REGION:
			do{
				// RIRegionInfo in rawimgae.h
				int renderRegion; ///< true if the image contains a region render, or false if the entire image is valid.
				int reg_xmin; ///< Left end of the region (0 if doing a full render).
				int reg_ymin; ///< Top end of the region (0 if doing a full render).
				int reg_width; ///< Width of the region (equal to the image width if doing a full render).
				int reg_height; ///< Height of the region (equal to the image height if doing a full render).
				
				Xdr::read<StreamIO>(is, renderRegion);
				Xdr::read<StreamIO>(is, reg_xmin);
				Xdr::read<StreamIO>(is, reg_ymin);
				Xdr::read<StreamIO>(is, reg_width);
				Xdr::read<StreamIO>(is, reg_height);
			}while(0);
			break;
			
*/		
		case RIT_CHAN_INFO:
			do{
				int num_channels = tag.p0;
				
				while(num_channels--)
				{
					// RIF_ChannelInfo in rawimge.h
					int index;
					int type;
					int alias;
					unsigned int flags;
					char name[64];
					
					Imf::Int64 start_chan = is.tellg();
					
					Xdr::read<Imf::StreamIO>(is, index);
					Xdr::read<Imf::StreamIO>(is, type);
					Xdr::read<Imf::StreamIO>(is, alias);
					Xdr::read<Imf::StreamIO>(is, flags);
					Xdr::read<Imf::StreamIO>(is, name, 64);
					
					// here's what type appears to mean, looking at rawimage.cpp
					// 1: RDCT_FLOAT
					// 2: RDCT_3FLOAT
					// 3: RDCT_2FLOAT
					// 4: RDCT_INT
					// 5: RDCT_3FLOAT_SIGNED
					
					PixelType enum_type = (type == 4 ? INT : FLOAT);
					
					int dimensions = (	type == 5 ? 3 :
										type == 2 ? 3 :
										type == 3 ? 2 :
										1);
					
					_map[ string(name) ] = Layer(index, enum_type, alias, flags, dimensions);
					
					is.seekg(start_chan + tag.p1);
				}
				
			}while(0);
			break;
		
	}
	
	
	if(skip_ahead)
		is.seekg(start_pos + tag.tagsize);
}


} // namespace VRimg
