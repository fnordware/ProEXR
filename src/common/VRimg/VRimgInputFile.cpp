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


#include "VRimgInputFile.h"

#include "VRimgXdr.h"

#include "zlib.h"


#define USE_ILMTHREAD 1


#ifdef USE_ILMTHREAD
#include <IlmThreadPool.h>
using namespace IlmThread;
#endif


using namespace std;

namespace VRimg {


InputFile::InputFile(Imf::IStream &is) :
	_is(is)
{
	_is.seekg(0);
	
	_header.readFrom(_is);
}


InputFile::~InputFile()
{
	freeBuffers();
}


// VRimg files use Intel byte order
static inline unsigned int Platform(unsigned int in)
{
#if defined(__powerc) || defined(__POWERPC__)
	return ((in >> 24) | ((in >> 8) & 0xff00) | ((in << 8) & 0xff0000) | (in << 24));
#else
	return in;
#endif
}

static inline int Platform(int in)
{
#if defined(__powerc) || defined(__POWERPC__)
	unsigned int *p = (unsigned int *)&in;
	
	*p = Platform(*p);
	
	return in;
#else
	return in;
#endif
}

static inline float Platform(float in)
{
#if defined(__powerc) || defined(__POWERPC__)
	unsigned int *p = (unsigned int *)&in;
	
	*p = Platform(*p);
	
	return in;
#else
	return in;
#endif
}


#ifdef USE_ILMTHREAD
class ReadTagTask : public Task
{
  public:
	ReadTagTask(TaskGroup *group,
					Imf::IStream &is, const Header &head, const Layer &layer, const RIF_TAG &tag,
					void *out_buf, size_t rowbytes);
	virtual ~ReadTagTask();
	
	virtual void execute();
	
	
  private:
	const int _dimensions;
	const unsigned int _tag_id;
	void *_out_buf;
	const size_t _rowbytes;
	
	const unsigned int _x_pos;
	const unsigned int _y_pos;
	const unsigned int _tile_width;
	const unsigned int _tile_height;
	
	void *_uncompressed_buf;
	void *_compressed_buf;
	size_t _compressed_size;
};


ReadTagTask::ReadTagTask(TaskGroup *group,
					Imf::IStream &is, const Header &head, const Layer &layer, const RIF_TAG &tag,
					void *out_buf, size_t rowbytes) :
	Task(group),
	_dimensions(layer.dimensions),
	_tag_id(tag.tag),
	_out_buf(out_buf),
	_rowbytes(rowbytes),
	_x_pos(tag.p1),
	_y_pos(tag.p2),
	_tile_width(tag.p3),
	_tile_height(tag.p4),
	_uncompressed_buf(NULL),
	_compressed_buf(NULL),
	_compressed_size(0)
{
	const size_t bytes_per_channel = (_tag_id == RIT_CHANI ? sizeof(int) : sizeof(float));

	const size_t tile_rowbytes = bytes_per_channel * _dimensions * _tile_width;

	const size_t full_size = tile_rowbytes * _tile_height;

	_uncompressed_buf = malloc(full_size);

	if(_uncompressed_buf)
	{
		if( head.isCompressed() )
		{
			_compressed_size = tag.tagsize - sizeof(tag);
			
			_compressed_buf = malloc(_compressed_size);
			
			if(_compressed_buf)
			{
				bool did_read = is.read((char *)_compressed_buf, _compressed_size);
			}
		}
		else
		{
			const size_t uncompressed_size = tag.tagsize - sizeof(tag);
			
			if(uncompressed_size <= full_size)
			{
				bool did_read = is.read((char *)_uncompressed_buf, uncompressed_size);
			}
		}
	}
}


ReadTagTask::~ReadTagTask()
{
	if(_uncompressed_buf)
		free(_uncompressed_buf);
	
	if(_compressed_buf)
		free(_compressed_buf);
}


void
ReadTagTask::execute()
{
	if(!_uncompressed_buf)
		return;

	const size_t bytes_per_channel = (_tag_id == RIT_CHANI ? sizeof(int) : sizeof(float));

	const size_t tile_rowbytes = bytes_per_channel * _dimensions * _tile_width;

	if(_compressed_buf)
	{
		const size_t full_size = tile_rowbytes * _tile_height;
		
		uLongf the_full_size = full_size;
		
		int z_result = uncompress((Bytef *)_uncompressed_buf, &the_full_size, (Bytef *)_compressed_buf, _compressed_size);
	}
	
	
	char *source_row = (char *)_uncompressed_buf;
	char *dest_row = (char *)_out_buf + (_rowbytes * _y_pos) + (_dimensions * bytes_per_channel * _x_pos);
	
	for(int y=0; y < _tile_height; y++)
	{
		if(_tag_id == RIT_CHANI)
		{
			int *source_pix = (int *)source_row;
			int *dest_pix = (int *)dest_row;
			
			for(int x=0; x < (_tile_width * _dimensions); x++)
			{
				*dest_pix++ = Platform( *source_pix++ );
			}
			
			source_row += tile_rowbytes;
			dest_row += _rowbytes;
		}
		else
		{
			float *source_pix = (float *)source_row;
			float *dest_pix = (float *)dest_row;
			
			for(int x=0; x < (_tile_width * _dimensions); x++)
			{
				*dest_pix++ = Platform( *source_pix++ );
			}
			
			source_row += tile_rowbytes;
			dest_row += _rowbytes;
		}
	}
}

#endif // USE_ILMTHREAD


void
InputFile::loadFromFile(BufferMap *buf_map)
{
	try{
		const Header &head = header();
		const Header::LayerMap &layer_map = head.layers();
		
		const int width = head.width();
		const int height = head.height();
		
		
		map<unsigned int, string>index_map;
		
		// allocate buffers and fill in indices
		for(Header::LayerMap::const_iterator i = layer_map.begin(); i != layer_map.end(); ++i)
		{
			const string &name = i->first;
			const Layer &layer = i->second;
			
			if(buf_map == NULL)
			{
				const size_t colbytes = sizeof(float) * layer.dimensions;
				const size_t rowbytes = colbytes * width;
				const size_t buf_size = rowbytes * height;
				
				void *buf = malloc(buf_size);
				
				if(buf == NULL)
					throw bad_alloc();
				
				_map[name] = buf;
			}
			else
			{
				if(buf_map->find(name) == buf_map->end())
					throw Iex::LogicExc("buf_map missing layers");
			}
			
			index_map[layer.index] = name;
		}
		
		
		BufferMap &BufMap = (buf_map == NULL ? _map : *buf_map);
		
		
		// build a description while we're loading the file
		Rope newline("&#xA;");
		Rope xmp;

		xmp += Rope("VRimg File Description") + newline + newline;
		
		
		_is.seekg(0);
		
		
		// header
		unsigned int magic, versionmajor, versionminor, indexPosLS, indexPosMS, flags, res1, res2;
		
		Xdr::read<Imf::StreamIO>(_is, magic);
		Xdr::read<Imf::StreamIO>(_is, versionmajor);
		Xdr::read<Imf::StreamIO>(_is, versionminor);
		Xdr::read<Imf::StreamIO>(_is, indexPosLS);
		Xdr::read<Imf::StreamIO>(_is, indexPosMS);
		Xdr::read<Imf::StreamIO>(_is, flags);
		Xdr::read<Imf::StreamIO>(_is, res1);
		Xdr::read<Imf::StreamIO>(_is, res2);
		
		xmp += Rope("=Attributes=") + newline;
		
		stringstream version;
		version << versionmajor << "." << versionminor;
		
		xmp += Rope("Version: ") + Rope( version.str().c_str() ) + newline;
		
		xmp += Rope("Compressed: ") + Rope((flags & RIF_FLAG_COMPRESSION) ? "true" : "false") + newline;
		
		
	#ifdef USE_ILMTHREAD
		TaskGroup group;
	#endif
	
		// read through each tag
		try{
			while(1)
			{
				Imf::Int64 start_pos = _is.tellg();
				
				RIF_TAG tag;
				
				Xdr::read<Imf::StreamIO>(_is, tag.tag);
				Xdr::read<Imf::StreamIO>(_is, tag.tagsize);
				Xdr::read<Imf::StreamIO>(_is, tag.p0);
				Xdr::read<Imf::StreamIO>(_is, tag.p1);
				Xdr::read<Imf::StreamIO>(_is, tag.p2);
				Xdr::read<Imf::StreamIO>(_is, tag.p3);
				Xdr::read<Imf::StreamIO>(_is, tag.p4);
				Xdr::read<Imf::StreamIO>(_is, tag.p5);
				Xdr::read<Imf::StreamIO>(_is, tag.p6);
				Xdr::read<Imf::StreamIO>(_is, tag.p7);
				
				if(tag.tag == RIT_CHAN3F || tag.tag == RIT_CHAN2F || tag.tag == RIT_CHANI || tag.tag == RIT_CHANF)
				{
					const string &layer_name = index_map[tag.p7];
					void *buf = BufMap[layer_name];
					
					const Layer *the_layer = head.findLayer(layer_name);
					
					if(the_layer == NULL || buf == NULL)
						throw Iex::LogicExc("Problem with layer index.");
					
					const int dimensions = the_layer->dimensions;
					
					const size_t bytes_per_channel = (tag.tag == RIT_CHANI ? sizeof(int) : sizeof(float));
					
					const size_t rowbytes = bytes_per_channel * dimensions * width;
					
				#ifdef USE_ILMTHREAD
					ThreadPool::addGlobalTask(new ReadTagTask(&group,
															_is, head, *the_layer, tag,
															buf, rowbytes) );
				#else
					const int x_pos = tag.p1;
					const int y_pos = tag.p2;
					
					const int tile_width = tag.p3;
					const int tile_height = tag.p4;
					
					const size_t tile_rowbytes = bytes_per_channel * dimensions * tile_width;
					
					const size_t full_size = tile_rowbytes * tile_height;
					
					void *uncompressed_buf = malloc(full_size);
					
					if(uncompressed_buf)
					{
						if( head.isCompressed() )
						{
							size_t compressed_size = tag.tagsize - sizeof(tag);
							
							void *compressed_buf = malloc(compressed_size);
							
							if(compressed_buf)
							{
								bool did_read = _is.read((char *)compressed_buf, compressed_size);
								
								uLongf the_full_size = full_size;
								
								int z_result = uncompress((Bytef *)uncompressed_buf, &the_full_size, (Bytef *)compressed_buf, compressed_size);
								
								free(compressed_buf);
							}
						}
						else
						{
							size_t uncompressed_size = tag.tagsize - sizeof(tag);
							
							if(uncompressed_size <= full_size)
							{
								bool did_read = _is.read((char *)uncompressed_buf, uncompressed_size);
							}
						}
						
						
						char *source_row = (char *)uncompressed_buf;
						char *dest_row = (char *)buf + (rowbytes * y_pos) + (dimensions * bytes_per_channel * x_pos);
						
						for(int y=0; y < tile_height; y++)
						{
							if(tag.tag == RIT_CHANI)
							{
								int *source_pix = (int *)source_row;
								int *dest_pix = (int *)dest_row;
								
								for(int x=0; x < (tile_width * dimensions); x++)
								{
									*dest_pix++ = Platform( *source_pix++ );
								}
								
								source_row += tile_rowbytes;
								dest_row += rowbytes;
							}
							else
							{
								float *source_pix = (float *)source_row;
								float *dest_pix = (float *)dest_row;
								
								for(int x=0; x < (tile_width * dimensions); x++)
								{
									*dest_pix++ = Platform( *source_pix++ );
								}
								
								source_row += tile_rowbytes;
								dest_row += rowbytes;
							}
						}
						
						free(uncompressed_buf);
					}
				#endif
				}
				else
				{
					_is.seekg(start_pos);
					
					DescribeTag(xmp);
				}
				
				_is.seekg(start_pos + tag.tagsize);
			}
		}
		catch(Iex::IoExc &e) {}
		
		
		// channel info
		xmp += newline + Rope("=Channels=") + newline;
		
		for(Header::LayerMap::const_iterator i = layer_map.begin(); i != layer_map.end(); i++)
		{
			xmp += i->first.c_str();
			
			const Layer &layer = i->second;
			
			if(layer.type == VRimg::FLOAT)
			{
				if(layer.dimensions == 1)
					xmp += " (float)";
				else if(layer.dimensions == 2)
					xmp += " (float2)";
				else if(layer.dimensions == 3)
					xmp += " (float3)";
			}
			else if(layer.type == VRimg::INT)
			{
				xmp += " (int)";
			}		
			
			xmp += newline;
		}
		
		_desc = xmp;
	}
	catch(...)
	{
		freeBuffers();
	}
}

void
InputFile::copyLayerToBuffer(const std::string &name, void *buf, size_t rowbytes)
{
	if(buf == NULL)
		throw Iex::NullExc("buf is NULL");
		
	const Layer *the_layer = header().findLayer(name);
	
	if(the_layer == NULL)
		return;

	if( !_map.empty() && (_map.find(name) != _map.end()) && (_map[name] != NULL) )
	{
		// layer has already been loaded into memory
		const Header &head = header();
		
		int width = head.width();
		int height = head.height();
		
		const void *layer_buf = _map[name];
		size_t layer_rowbytes = sizeof(float) * the_layer->dimensions * width;
		
		if(layer_rowbytes == rowbytes)
		{
			size_t mem_size = layer_rowbytes * height;
			
			memcpy(buf, layer_buf, mem_size);
		}
		else
		{
			for(int y=0; y < height; y++)
			{
				const void *layer_row = (char *)layer_buf + (y * layer_rowbytes);
				void *output_row = (char *)buf + (y * rowbytes);
				
				memcpy(output_row, layer_row, layer_rowbytes);
			}
		}
	}
	else
	{
		_is.seekg(0);
		
		// cruise through header
		unsigned int magic, versionmajor, versionminor, indexPosLS, indexPosMS, flags, res1, res2;
		
		Xdr::read<Imf::StreamIO>(_is, magic);
		Xdr::read<Imf::StreamIO>(_is, versionmajor);
		Xdr::read<Imf::StreamIO>(_is, versionminor);
		Xdr::read<Imf::StreamIO>(_is, indexPosLS);
		Xdr::read<Imf::StreamIO>(_is, indexPosMS);
		Xdr::read<Imf::StreamIO>(_is, flags);
		Xdr::read<Imf::StreamIO>(_is, res1);
		Xdr::read<Imf::StreamIO>(_is, res2);
		
	#ifdef USE_ILMTHREAD
		TaskGroup group;
	#endif
	
		try{
			while(1)
			{
				Imf::Int64 start_pos = _is.tellg();
				
				RIF_TAG tag;
				
				Xdr::read<Imf::StreamIO>(_is, tag.tag);
				Xdr::read<Imf::StreamIO>(_is, tag.tagsize);
				Xdr::read<Imf::StreamIO>(_is, tag.p0);
				Xdr::read<Imf::StreamIO>(_is, tag.p1);
				Xdr::read<Imf::StreamIO>(_is, tag.p2);
				Xdr::read<Imf::StreamIO>(_is, tag.p3);
				Xdr::read<Imf::StreamIO>(_is, tag.p4);
				Xdr::read<Imf::StreamIO>(_is, tag.p5);
				Xdr::read<Imf::StreamIO>(_is, tag.p6);
				Xdr::read<Imf::StreamIO>(_is, tag.p7);
				
				if( (tag.tag == RIT_CHAN3F || tag.tag == RIT_CHAN2F || tag.tag == RIT_CHANI || tag.tag == RIT_CHANF) &&
					(tag.p7 == the_layer->index) )
				{
				#ifdef USE_ILMTHREAD
					ThreadPool::addGlobalTask(new ReadTagTask(&group,
															_is, header(), *the_layer, tag,
															buf, rowbytes) );
				#else
					int dimensions = the_layer->dimensions;
					
					int x_pos = tag.p1;
					int y_pos = tag.p2;
					
					int tile_width = tag.p3;
					int tile_height = tag.p4;
					
					size_t bytes_per_channel = (tag.tag == RIT_CHANI ? sizeof(int) : sizeof(float));
					
					size_t tile_rowbytes = bytes_per_channel * dimensions * tile_width;
					
					size_t full_size = tile_rowbytes * tile_height;
					
					void *uncompressed_buf = malloc(full_size);
					
					if(uncompressed_buf)
					{
						if( header().isCompressed() )
						{
							size_t compressed_size = tag.tagsize - sizeof(tag);
							
							void *compressed_buf = malloc(compressed_size);
							
							if(compressed_buf)
							{
								bool did_read = _is.read((char *)compressed_buf, compressed_size);
								
								uLongf the_full_size = full_size;
								
								int z_result = uncompress((Bytef *)uncompressed_buf, &the_full_size, (Bytef *)compressed_buf, compressed_size);
								
								free(compressed_buf);
							}
						}
						else
						{
							size_t uncompressed_size = tag.tagsize - sizeof(tag);
							
							if(uncompressed_size <= full_size)
							{
								bool did_read = _is.read((char *)uncompressed_buf, uncompressed_size);
							}
						}
						
						
						char *source_row = (char *)uncompressed_buf;
						char *dest_row = (char *)buf + (rowbytes * y_pos) + (dimensions * bytes_per_channel * x_pos);
						
						for(int y=0; y < tile_height; y++)
						{
							if(tag.tag == RIT_CHANI)
							{
								int *source_pix = (int *)source_row;
								int *dest_pix = (int *)dest_row;
								
								for(int x=0; x < (tile_width * dimensions); x++)
								{
									*dest_pix++ = Platform( *source_pix++ );
								}
								
								source_row += tile_rowbytes;
								dest_row += rowbytes;
							}
							else
							{
								float *source_pix = (float *)source_row;
								float *dest_pix = (float *)dest_row;
								
								for(int x=0; x < (tile_width * dimensions); x++)
								{
									*dest_pix++ = Platform( *source_pix++ );
								}
								
								source_row += tile_rowbytes;
								dest_row += rowbytes;
							}
						}
						
						free(uncompressed_buf);
					}
				#endif
				}
				
				_is.seekg(start_pos + tag.tagsize);
			}
		}
		catch(Iex::IoExc &e) {}
	}
}

Rope
InputFile::getXMPdescription() const
{
	Rope XMPtext;

	char unicode_nbsp[4] = { 0xef, 0xbb, 0xbf, '\0' };
	
	// make a basic XMP block with one main value - the description
	// we'd break this up into something nicer, but XMP is a little rigid
	// this guarantees that it'll show up in Photoshop File Info
	XMPtext += "<?xpacket begin=\"";
	XMPtext += Rope(unicode_nbsp) + "\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n";
	XMPtext += "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"3.1.1-112\">\n";
	XMPtext += "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n";
	XMPtext += "<rdf:Description rdf:about=\"\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n";
	//XMPtext += "<dc:format>image/exr</dc:format>\n";
	XMPtext += "<dc:description>\n";
	XMPtext += "<rdf:Alt>\n";

	XMPtext += "<rdf:li xml:lang=\"x-default\">";
	
	// here's where the description goes
	AddDescription(XMPtext);
	
	XMPtext += "</rdf:li>\n";

	XMPtext += "</rdf:Alt>\n";
	XMPtext += "</dc:description>\n";
	XMPtext += "</rdf:Description>\n";
	XMPtext += "</rdf:RDF>\n";
	XMPtext += "</x:xmpmeta>\n";

	return XMPtext;
}


void
InputFile::AddDescription(Rope &xmp) const
{
	if( !_desc.empty() )
	{
		xmp += _desc;
		return;
	}
	
	
	Rope newline("&#xA;");

	xmp += Rope("VRimg File Description") + newline + newline;
	
	
	_is.seekg(0);
	
	
	// header
	unsigned int magic, versionmajor, versionminor, indexPosLS, indexPosMS, flags, res1, res2;
	
	Xdr::read<Imf::StreamIO>(_is, magic);
	Xdr::read<Imf::StreamIO>(_is, versionmajor);
	Xdr::read<Imf::StreamIO>(_is, versionminor);
	Xdr::read<Imf::StreamIO>(_is, indexPosLS);
	Xdr::read<Imf::StreamIO>(_is, indexPosMS);
	Xdr::read<Imf::StreamIO>(_is, flags);
	Xdr::read<Imf::StreamIO>(_is, res1);
	Xdr::read<Imf::StreamIO>(_is, res2);
	
	xmp += Rope("=Attributes=") + newline;
	
	stringstream version;
	version << versionmajor << "." << versionminor;
	
	xmp += Rope("Version: ") + Rope( version.str().c_str() ) + newline;
	
	xmp += Rope("Compressed: ") + Rope((flags & RIF_FLAG_COMPRESSION) ? "true" : "false") + newline;
	
	
	// read through all the tags the brute force way
	try{
		while(1)
		{
			DescribeTag(xmp);
		}
	}
	catch(Iex::IoExc &e) {}
	
	
	// channel info
	xmp += newline + Rope("=Channels=") + newline;
	
	
	const Header::LayerMap &layers = header().layers();
	
	for(Header::LayerMap::const_iterator i = layers.begin(); i != layers.end(); i++)
	{
		xmp += i->first.c_str();
		
		const Layer &layer = i->second;
		
		if(layer.type == VRimg::FLOAT)
		{
			if(layer.dimensions == 1)
				xmp += " (float)";
			else if(layer.dimensions == 2)
				xmp += " (float2)";
			else if(layer.dimensions == 3)
				xmp += " (float3)";
		}
		else if(layer.type == VRimg::INT)
		{
			xmp += " (int)";
		}		
		
		xmp += newline;
	}
}


union IntAndFloat {
	float f;
	int i;
};


void
InputFile::DescribeTag(Rope &xmp) const 
{
	Rope newline("&#xA;");
	
	Imf::Int64 start_pos = _is.tellg();

	RIF_TAG tag;
	
	Xdr::read<Imf::StreamIO>(_is, tag.tag);
	Xdr::read<Imf::StreamIO>(_is, tag.tagsize);
	Xdr::read<Imf::StreamIO>(_is, tag.p0);
	Xdr::read<Imf::StreamIO>(_is, tag.p1);
	Xdr::read<Imf::StreamIO>(_is, tag.p2);
	Xdr::read<Imf::StreamIO>(_is, tag.p3);
	Xdr::read<Imf::StreamIO>(_is, tag.p4);
	Xdr::read<Imf::StreamIO>(_is, tag.p5);
	Xdr::read<Imf::StreamIO>(_is, tag.p6);
	Xdr::read<Imf::StreamIO>(_is, tag.p7);
	
	switch(tag.tag)
	{
		case RIT_RESOLUTION:
			do{
				xmp += "Resolution { ";
				
				stringstream res;
				
				res << "width: " << tag.p0 << "  ";
				res << "height: " << tag.p1;
				
				IntAndFloat bitAspect;
				bitAspect.f = 1.0f;
				
				if(tag.p2)
				{
					bitAspect.i = tag.p2;
				
					res << "  pixelAspectRatio: " << bitAspect.f;
				}
				
				xmp += res.str().c_str();
				
				xmp += Rope(" }") + newline;
			}while(0);
			break;
			
		
		case RIT_REGIONSINFO:
			do{
				xmp += "Regions Info { ";
				
				stringstream info;
				
				info << "regWidth: " << tag.p0 << "  ";
				info << "regHeight: " << tag.p1 << "  ";
				info << "whatsXY: " << (tag.p2 ? "Number" : "MaxSize") << " ";
				
				xmp += info.str().c_str();
				
				xmp += Rope("}") + newline;
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
				
				Xdr::read<Imf::StreamIO>(_is, renderRegion);
				Xdr::read<Imf::StreamIO>(_is, reg_xmin);
				Xdr::read<Imf::StreamIO>(_is, reg_ymin);
				Xdr::read<Imf::StreamIO>(_is, reg_width);
				Xdr::read<Imf::StreamIO>(_is, reg_height);
				
				xmp += "Render Region { ";
				
				stringstream region;
				
				region << "renderRegion: " << (renderRegion ? "true" : "false") << "  ";
				region << "xmin: " << reg_xmin << "  ";
				region << "ymin: " << reg_ymin << "  ";
				region << "width: " << reg_width << "  ";
				region << "height: " << reg_height << " ";
				
				xmp += region.str().c_str();
				
				xmp += Rope("}") + newline;
			}while(0);
			break;
			
		
		case RIT_CAMERA_INFO:
			do{
				// RICameraInfo in rawimage.h
				float transform[4][4]; ///< 4x4 camera transformation. You can use the setCameraTransform() method to convert a Transform to this.
				int projection; ///< Projection type: - 0 - perspective; - 1 - parallel.
				float aperture; ///< Horizontal film gate in mm.
				float fov; ///< Camera field of view (in degrees).
				float targetDistance; ///< Camera target distance (normally the focus distance).
				float nearRange; ///< Camera near range.
				float farRange; ///< Camera far range.
				float nearClip; ///< Camera near clipping distance.
				float farClip; ///< Camera far clipping distance.
				float focalLength; ///< Camera focal length in mm.
				float fNumber; ///< Camera f-number in f-stops.
				
				for(int y=0; y < 4; y++)
					for(int x=0; x < 4; x++)
						Xdr::read<Imf::StreamIO>(_is, transform[y][x]);
				
				Xdr::read<Imf::StreamIO>(_is, projection);
				Xdr::read<Imf::StreamIO>(_is, aperture);
				Xdr::read<Imf::StreamIO>(_is, fov);
				Xdr::read<Imf::StreamIO>(_is, targetDistance);
				Xdr::read<Imf::StreamIO>(_is, nearRange);
				Xdr::read<Imf::StreamIO>(_is, farRange);
				Xdr::read<Imf::StreamIO>(_is, nearClip);
				Xdr::read<Imf::StreamIO>(_is, farClip);
				Xdr::read<Imf::StreamIO>(_is, focalLength);
				Xdr::read<Imf::StreamIO>(_is, fNumber);
				
				
				xmp += "Camera Info { ";
				
				stringstream camera;
				
				camera << "transform: matrix  ";
				camera << "projection: " << (projection ? "parallel" : "perspective") << "  ";
				camera << "aperture: " << aperture << "  ";
				camera << "fov: " << fov << "  ";
				camera << "targetDistance: " << targetDistance << "  ";
				camera << "nearRange: " << nearRange << "  ";
				camera << "nearClip: " << nearClip << "  ";
				camera << "farClip: " << farClip << "  ";
				camera << "focalLength: " << focalLength << "  ";
				camera << "fNumber: " << fNumber << " ";
				
				xmp += camera.str().c_str();
				
				xmp += Rope("}") + newline;
			}while(0);
			break;
			
			
		case RIT_SCENE_INFO:
			do{
				int renderTime;
				
				Xdr::read<Imf::StreamIO>(_is, renderTime);
				
				int namesize =  tag.tagsize - sizeof(RIF_TAG) - sizeof(int);
				
				char *name = new char[namesize];
				
				Xdr::read<Imf::StreamIO>(_is, name, namesize);
				
				
				xmp += "Scene { ";
				
				stringstream scene;
				
				scene << "renderTime: " << renderTime << "  ";
				scene << "name: \"" << string(name, 0, namesize) << "\" ";
				
				xmp += scene.str().c_str();
				
				xmp += Rope("}") + newline;
				
				
				delete [] name;
			}while(0);
			break;
		
		
		case RIT_NOTE:
			do{
				int notesize = tag.tagsize - sizeof(RIF_TAG);
				
				char *note = new char[notesize];
				
				Xdr::read<Imf::StreamIO>(_is, note, notesize);
				
				
				xmp += "Note: \"";
				
				xmp += string(note, 0, notesize).c_str();
				
				xmp += Rope("\"") + newline;
				
				
				delete [] note;
			}while(0);
			break;
	}
	
	
	_is.seekg(start_pos + tag.tagsize);
}


void
InputFile::freeBuffers()
{
	for(BufferMap::iterator i = _map.begin(); i != _map.end(); ++i)
	{
		void *buf = i->second;
		
		if(buf != NULL)
			free(buf);
	}
	
	_map.clear();
}

} // namespace VRimg