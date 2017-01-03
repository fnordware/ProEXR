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


#include "VRimg.h"

#include "VRimgVersion.h"
#include "VRimgInputFile.h"
#include "VRimg_ChannelCache.h"

#include "ProEXR_AE_Dialogs.h"

#include "OpenEXR_PlatformIO.h"
#include "Iex.h"
#include "OpenEXR_ChannelMap.h"

#include <IlmThread.h>
#include <IlmThreadPool.h>

#include <string>
#include <vector>

#include <assert.h>


using namespace VRimg;
using namespace std;


extern AEGP_PluginID S_mem_id;

extern unsigned int gNumCPUs;

static A_long gChannelCaches = 3;
static A_long gCacheTimeout = 30;
static A_Boolean gMemoryMap = FALSE;


static VRimg_CachePool gCachePool;


A_Err
VRimg_Init(struct SPBasicSuite *pica_basicP)
{
	A_Err err = A_Err_NONE;

	AEGP_SuiteHandler suites(pica_basicP);
	
	AEGP_PersistentBlobH blobH = NULL;
	suites.PersistentDataSuite()->AEGP_GetApplicationBlob(&blobH);

#define PREFS_SECTION	"VRimg"
#define PREFS_CHANNEL_CACHES "Channel Caches Number"
#define PREFS_CACHE_EXPIRATION "Channel Cache Expiration"
#define PREFS_MEMORY_MAP	"Memory Map"


	A_long channel_caches = gChannelCaches;
	A_long cache_timeout = gCacheTimeout;
	A_long memory_map = gMemoryMap;
	
	suites.PersistentDataSuite()->AEGP_GetLong(blobH, PREFS_SECTION, PREFS_CHANNEL_CACHES, channel_caches, &channel_caches);
	suites.PersistentDataSuite()->AEGP_GetLong(blobH, PREFS_SECTION, PREFS_CACHE_EXPIRATION, cache_timeout, &cache_timeout);
	suites.PersistentDataSuite()->AEGP_GetLong(blobH, PREFS_SECTION, PREFS_MEMORY_MAP, memory_map, &memory_map);

	gChannelCaches = channel_caches;
	gCacheTimeout = cache_timeout;
	gMemoryMap = (memory_map ? TRUE : FALSE);
	
	gCachePool.configurePool(gChannelCaches, pica_basicP);
	
	return err;
}


A_Err
VRimg_DeathHook(const SPBasicSuite *pica_basicP)
{
	try
	{
		gCachePool.configurePool(0);
	}
	catch(...) { return A_Err_PARAMETER; }
	
	return A_Err_NONE;
}


A_Err
VRimg_IdleHook(const SPBasicSuite *pica_basicP)
{
	if(gCacheTimeout > 0)
	{
		gCachePool.deleteStaleCaches(gCacheTimeout);
		
		DeleteFileCache(pica_basicP, gCacheTimeout);
	}

	return A_Err_NONE;
}


A_Err
VRimg_PurgeHook(const SPBasicSuite *pica_basicP)
{
	gCachePool.configurePool(0);
	gCachePool.configurePool(gChannelCaches);
	
	DeleteFileCache(pica_basicP, 0);
	
	return A_Err_NONE;
}


A_Err
VRimg_ConstructModuleInfo(
	AEIO_ModuleInfo	*info)
{
	// tell AE all about our plug-in

	A_Err err = A_Err_NONE;
	
	if (info)
	{
		info->sig						=	'VRim';
		info->max_width					=	2147483647;
		info->max_height				=	2147483647;
		info->num_filetypes				=	1;
		info->num_extensions			=	1;
		info->num_clips					=	0;
		
		info->create_kind.type			=	'VRim';
		info->create_kind.creator		=	'8BIM';

		info->create_ext.pad			=	'.';
		info->create_ext.extension[0]	=	'v';
		info->create_ext.extension[1]	=	'r';
		info->create_ext.extension[2]	=	'i';
		

		strcpy(info->name, PLUGIN_NAME);
		
		info->num_aux_extensionsS		=	0;

		info->flags						=	AEIO_MFlag_INPUT			| 
											AEIO_MFlag_FILE				|
											AEIO_MFlag_STILL			| 
											AEIO_MFlag_NO_TIME			| 
											AEIO_MFlag_CAN_DRAW_DEEP	|
											AEIO_MFlag_HAS_AUX_DATA;

		info->flags2					=	AEIO_MFlag2_CAN_DRAW_FLOAT;
											
		info->read_kinds[0].mac.type			=	'VRim';
		info->read_kinds[0].mac.creator			=	AEIO_ANY_CREATOR;
		info->read_kinds[1].ext					=	info->create_ext;
	}
	else
	{
		err = A_Err_STRUCT;
	}
	return err;
}


A_Err	
VRimg_GetInSpecInfo(
	const A_PathType	*file_pathZ,
	VRimg_inData		*options,
	AEIO_Verbiage		*verbiageP)
{ 
	A_Err err			=	A_Err_NONE;
	
	//strcpy(verbiageP->name, "some name"); // AE wil insert file name here for us

	strcpy(verbiageP->type, PLUGIN_NAME); // how could we tell a sequence from a file here?
		


	string info("");
	
	if(options) // can't assume there are options, or can we?
	{
		char chan_num[5];
		
		sprintf(chan_num, "%d", options->real_channels);
		
		info += chan_num;
		info += " channels: ";
		
		
		A_Boolean first_one = TRUE;
			
		for(int i=0; i < options->real_channels; i++)
		{
			if(first_one)
				first_one = FALSE;
			else
				info += ", ";
			
			info += options->channel[i].name;
		}
	}
	
			
	if(info.length() > AEIO_MAX_MESSAGE_LEN)
	{
		info.resize(AEIO_MAX_MESSAGE_LEN-3);
		info += "...";
	}

	strcpy(verbiageP->sub_type, info.c_str());


	return err;
}


A_Err	
VRimg_VerifyFile(
	const A_PathType		*file_pathZ, 
	A_Boolean				*importablePB)
{
	A_Err err			=	A_Err_NONE;
 
	try {
		IStreamPlatform instream(file_pathZ);

		char bytes[4];
		instream.read(bytes, sizeof(bytes));

		*importablePB = isVRimgMagic(bytes);
	}
	catch(...){ *importablePB = FALSE; }
	
	return err;
}


static void
ResizeOptionsHandle(
	AEIO_BasicData		*basic_dataP,
	AEIO_Handle			optionsH,
	VRimg_inData		**options,
	A_u_long			num_channels)
{
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);
	
	AEGP_MemSize mem_size = sizeof(VRimg_inData) + (sizeof(VRimg_inData) * (MAX(num_channels, 1) - 1));
	
	suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	
	suites.MemorySuite()->AEGP_ResizeMemHandle("Old Handle Resize", mem_size, optionsH);
	
	suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)options);
}


A_Err
VRimg_FileInfo(
	AEIO_BasicData		*basic_dataP,
	const A_PathType	*file_pathZ,
	FrameSeq_Info		*info,
	AEIO_Handle			optionsH)
{
	// read a file and pass AE the basic info
	
	A_Err err						=	A_Err_NONE;
	
	try{
	
	VRimg_inData *options = NULL;
	
	
	// we're going to increase the options handle size as needed and then shrink it at the end
	// will start with and increment by 64
	A_u_long options_size = 64;
	
	ResizeOptionsHandle(basic_dataP, optionsH, &options, options_size);
	
	
	// read the VRimg
	IStreamPlatform instream(file_pathZ);
	
	if(gMemoryMap)
		instream.memoryMap();
	
	InputFile input(instream);
	
	
	info->width = input.header().width();
	info->height = input.header().height();
	
	info->planes = (input.header().findLayer("Alpha") ? 4 : 3);
	
	// actually, V-Ray tends to act like straight alpha
	// let's leave this alone for now
	//if(info->planes == 4)
	//	info->alpha_type = AEIO_Alpha_PREMUL;
	
	info->depth = 32;
	
	
	float pixel_aspect_ratio = input.header().pixelAspectRatio();
	
	// try to be smart making this into a rational scale
	if(pixel_aspect_ratio == 1.0f)
		info->pixel_aspect_ratio.num = info->pixel_aspect_ratio.den = 1; // probably could do nothing, actually
	else
	{
		info->pixel_aspect_ratio.den = 1000;
		info->pixel_aspect_ratio.num = (pixel_aspect_ratio * (float)info->pixel_aspect_ratio.den) + 0.5f;
	}
	
	
	// split layers into channels and add to the list
	const Header::LayerMap &layers = input.header().layers();
	
	const int max_possible_channels = layers.size() * 5; // the compound layer plus 4 channels
	ResizeOptionsHandle(basic_dataP, optionsH, &options, max_possible_channels);
	
	
	A_long compound_channel_index = 0;
	A_long single_channel_index = layers.size();
	
	for(Header::LayerMap::const_iterator i = layers.begin(); i != layers.end(); i++)
	{
		string channel_name = i->first;
		
		const int dimensions = i->second.dimensions;
		
		PF_DataType data_type = (i->second.type == VRimg::FLOAT ? PF_DataType_FLOAT : PF_DataType_LONG);
		
		PF_ChannelType channel_type = PF_ChannelType_UNKNOWN;
		
		
		// hard code some tags
		if(channel_name == "VRayZDepth")
			channel_type = PF_ChannelType_DEPTH;
		//else if(channel_name == "VRayUnclampedColor") // not cool with PF_DataType_RGB data types
		//	channel_type = PF_ChannelType_UNCLAMPED;
		else if(channel_name == "VRayVelocity")
			channel_type = PF_ChannelType_MOTIONVECTOR;
		//else if(channel_name == "VRayBackground")
		//	channel_type = PF_ChannelType_BK_COLOR;
		else if(channel_name == "VRayNormals")
			channel_type = PF_ChannelType_NORMALS;
		else if(channel_name == "VRayObjectID")
		{
			data_type = PF_DataType_U_SHORT;
			channel_type = PF_ChannelType_OBJECTID;
		}
		else if(channel_name == "VRayMtlID")
		{
			data_type = PF_DataType_U_BYTE;
			channel_type = PF_ChannelType_MATERIAL;
		}
		
		
		if(dimensions == 1)
		{
			if(channel_name.size() < PF_CHANNEL_NAME_LEN)
			{
				options->channel[compound_channel_index].channel_type = channel_type;
				options->channel[compound_channel_index].dimension = 1;
				strncpy(options->channel[compound_channel_index].name, channel_name.c_str(), PF_CHANNEL_NAME_LEN);
				options->channel[compound_channel_index].name[PF_CHANNEL_NAME_LEN] = '\0';
				options->channel[compound_channel_index].data_type = data_type;
				
				compound_channel_index++;
			}
		}
		else
		{
			static const char * chan_names[4] = { "R", "G", "B", "A" };
			
			string compound_extension;
			
			for(int n=0; n < dimensions && n < 4; n++)
			{
				const string single_channel_name = channel_name + "." + chan_names[n];
			
				options->channel[single_channel_index].channel_type = channel_type;
				options->channel[single_channel_index].dimension = 1;
				strncpy(options->channel[single_channel_index].name, single_channel_name.c_str(), PF_CHANNEL_NAME_LEN);
				options->channel[single_channel_index].name[PF_CHANNEL_NAME_LEN] = '\0';
				options->channel[single_channel_index].data_type = data_type;
				
				single_channel_index++;
				
			
				if(n == 3)
				{
					compound_extension = chan_names[n] + compound_extension; // put A in the beginning
				}
				else
					compound_extension += chan_names[n];
			}
			
			
			const string compound_name = channel_name + "." + compound_extension;
				
			options->channel[compound_channel_index].channel_type = channel_type;
			options->channel[compound_channel_index].dimension = dimensions;
			strncpy(options->channel[compound_channel_index].name, compound_name.c_str(), PF_CHANNEL_NAME_LEN);
			options->channel[compound_channel_index].name[PF_CHANNEL_NAME_LEN] = '\0';
			options->channel[compound_channel_index].data_type = data_type;
			
			compound_channel_index++;
		}
	}
	
	options->channels = single_channel_index;
	options->real_channels = compound_channel_index;
	
	// shrink options handle back down to size we actually need
	ResizeOptionsHandle(basic_dataP, optionsH, &options, options->channels);

    }catch(...) { err = AEIO_Err_PARSING; }
	
	return err;
}


static void
RefreshChannels(
	AEIO_BasicData		*basic_dataP,
	AEIO_Handle			optionsH,
	const A_PathType	*file_pathZ)
{
	// run through FileInfo process again to fill in out channel info

	// here's a fake FrameSeq_Info
	FrameSeq_Info info;
	
	PF_Pixel 	premult_color = {255, 0, 0, 0};
	FIEL_Label	field_label;

	info.premult_color	= &premult_color;
	info.field_label	= &field_label;
	
	info.icc_profile	= NULL;
	
	
	VRimg_FileInfo(basic_dataP, file_pathZ, &info, optionsH);
	
	
	if(info.icc_profile)
		free(info.icc_profile);
}


typedef struct {
	PF_FpShort red, green, blue;
} RGBPixel;

A_Err	
VRimg_DrawSparseFrame(
	AEIO_BasicData					*basic_dataP,
	const AEIO_DrawSparseFramePB	*sparse_framePPB, 
	PF_EffectWorld					*wP,
	AEIO_DrawingFlags				*draw_flagsP,
	const A_PathType				*file_pathZ,
	FrameSeq_Info					*info,
	VRimg_inData					*options)
{ 
	// read pixels into the float buffer

	A_Err	err		= A_Err_NONE,
			err2	= A_Err_NONE;
	
	
	try{
	
	if( IlmThread::supportsThreads() )
		IlmThread::ThreadPool::globalThreadPool().setNumThreads(gNumCPUs);
	
	
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);
	
	IStreamPlatform instream(file_pathZ, basic_dataP->pica_basicP);
	InputFile input(instream);
	
	
	
#ifdef NDEBUG
	#define CONT()	( (sparse_framePPB && sparse_framePPB->inter.abort0) ? !(err2 = sparse_framePPB->inter.abort0(sparse_framePPB->inter.refcon) ) : TRUE)
	#define PROG(COUNT, TOTAL)	( (sparse_framePPB && sparse_framePPB->inter.progress0) ? !(err2 = sparse_framePPB->inter.progress0(sparse_framePPB->inter.refcon, COUNT, TOTAL) ) : TRUE)
	const AEIO_InterruptFuncs *inter = (sparse_framePPB ? &sparse_framePPB->inter : NULL);
#else
	#define CONT()					TRUE
	#define PROG(COUNT, TOTAL)		TRUE
	const AEIO_InterruptFuncs *inter = NULL;
#endif

	VRimg_ChannelCache *cache = gCachePool.findCache(instream);
	
	if(cache == NULL && gChannelCaches > 0)
	{
		cache = gCachePool.addCache(input, instream, inter);
	}
	
	
	string rgb_layer_name = "RGB color";
	
	const Layer *rgb_layer = input.header().findLayer(rgb_layer_name);
	
	if(rgb_layer && rgb_layer->type == VRimg::FLOAT && rgb_layer->dimensions == 3)
	{
		if(cache != NULL)
			cache->copyLayerToBuffer(rgb_layer_name, wP->data, wP->rowbytes);
		else
			input.copyLayerToBuffer(rgb_layer_name, wP->data, wP->rowbytes);
		
		// that's going to be rows of RGBRGB, must fill gaps to make it ARGBARGB
		for(int y = wP->height; y > 0; y--)
		{
			char *row = (char *)wP->data + ((y - 1) * wP->rowbytes);
		
			RGBPixel *src = (RGBPixel *)(row + (sizeof(RGBPixel) * (wP->width - 1)));
			PF_Pixel32 *dest = (PF_Pixel32 *)(row + (sizeof(PF_Pixel32) * (wP->width - 1)));
			
			for(int x = wP->width; x > 0; x--)
			{
				dest->blue = src->blue;
				dest->green = src->green;
				dest->red = src->red;
				dest->alpha = 1.f;
				
				src--;
				dest--;
			}
		}
	}
	
	
	string alpha_layer_name = "Alpha";
	
	const Layer *alpha_layer = input.header().findLayer(alpha_layer_name);
	
	if(alpha_layer && alpha_layer->type == VRimg::FLOAT)
	{
		// have to make a buffer
		// for some reason getting 3 dimensional alphas?
		AEIO_Handle alphaH = NULL;
		
		size_t rowbytes = sizeof(float) * alpha_layer->dimensions * wP->width;
		
		AEGP_MemSize mem_size = rowbytes * wP->height;
		
		suites.MemorySuite()->AEGP_NewMemHandle(S_mem_id, "Alpha Buffer", mem_size, AEGP_MemFlag_CLEAR, &alphaH);
		
		if(alphaH)
		{
			void *alpha_buf = NULL;
			
			suites.MemorySuite()->AEGP_LockMemHandle(alphaH, &alpha_buf);
			
			if(cache != NULL)
				cache->copyLayerToBuffer(alpha_layer_name, alpha_buf, rowbytes);
			else
				input.copyLayerToBuffer(alpha_layer_name, alpha_buf, rowbytes);
			
			
			for(int y=0; y < wP->height; y++)
			{
				PF_FpShort *src = (PF_FpShort *)((char *)alpha_buf + (y * rowbytes));
				PF_Pixel32 *dest = (PF_Pixel32 *)((char *)wP->data + (y * wP->rowbytes));
			
				for(int x=0; x < wP->width; x++)
				{
					dest->alpha = *src;
					
					src += alpha_layer->dimensions;
					dest++;
				}
			}
			
			suites.MemorySuite()->AEGP_FreeMemHandle(alphaH);
		}
	}
	
	
	}
	catch(Iex::IoExc) {}
	catch(Iex::InputExc) {}
	catch(CancelExc &e) { err2 = e.err(); }
	catch(...) { err = AEIO_Err_PARSING; }

	
	if(err2) { err = err2; }
	
	return err;
}


A_Err	
VRimg_InitInOptions(
	VRimg_inData	*options)
{
	// initialize the options when they're first created
	A_Err err =	A_Err_NONE;
	
	options->real_channels = options->channels = 0; // until they get filled in

	return err;
}


A_Err	
VRimg_ReadOptionsDialog(
	AEIO_BasicData	*basic_dataP,
	VRimg_inData	*options,
	A_Boolean		*user_interactedPB0)
{
	// this is old, it just lists channels
	// one day we'll have something really informative here
	
	string info("");
	
	int i;
	
	for(i=0; i < options->channels; i++)
	{
		char num[5];
		
		sprintf(num, "%d", i+1);
		
		info += num;
		info += ": ";
		
		info += options->channel[i].name;
		
		info += "\n";
	}
	
	if(info.length() > 511)
	{
		info.resize(508);
		info += "...";
	}
	
	basic_dataP->msg_func(0, info.c_str() );
	
	return A_Err_NONE;
}


A_Err
VRimg_FlattenInputOptions(
	AEIO_BasicData	*basic_dataP,
	AEIO_Handle		optionsH)
{
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);
	
	// we don't flatten, we practically destroy our options handle
	// because we're really using it to hold information about the channels
	// don't want it to really persist in the project
	// and want to update it in case the file has changed
	
	VRimg_inData *options = NULL;
	
	ResizeOptionsHandle(basic_dataP, optionsH, &options, 0);
	
	if(options)
	{
		options->real_channels = options->channels = 0;
		
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	}
	
	return A_Err_NONE;
}

A_Err
VRimg_InflateInputOptions(
	AEIO_BasicData	*basic_dataP,
	AEIO_Handle		optionsH)
{
	// same as flatten - just set to channels to 0 to force a reload
	return VRimg_FlattenInputOptions(basic_dataP, optionsH);
}


#pragma mark-


A_Err	
VRimg_GetNumAuxChannels(
	AEIO_BasicData		*basic_dataP,
	AEIO_Handle			optionsH,
	const A_PathType	*file_pathZ,
	A_long				*num_channelsPL)
{
	A_Err err						=	A_Err_NONE;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);
	
	
	VRimg_inData *options = NULL;
	
	suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
	

	// might have to get this info from the file again
	if(options->channels == 0) // should always have at least one
	{
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);

		RefreshChannels(basic_dataP, optionsH, file_pathZ);
		
		suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
	}


	*num_channelsPL = options->channels;

	return err;
}

									
A_Err	
VRimg_GetAuxChannelDesc(	
	AEIO_BasicData		*basic_dataP,
	AEIO_Handle			optionsH,
	const A_PathType	*file_pathZ,
	A_long				chan_indexL,
	PF_ChannelDesc		*descP)
{
	A_Err err						=	A_Err_NONE;

	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);
	
	
	VRimg_inData *options = NULL;
	
	suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);


	// might have to get this info from the file again
	if(options->channels == 0) // should always have at least one
	{
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);

		RefreshChannels(basic_dataP, optionsH, file_pathZ);
		
		suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
	}


	// just copy the appropriate ChannelDesc
	*descP = options->channel[chan_indexL];

	return err;
}


typedef struct {
	const AEIO_DrawFramePB	*pbP;
	void *in;
	size_t in_rowbytes;
	int in_channels;
	void *out;
	size_t out_rowbytes;
	int out_channels;
	int width;
	PF_Point scale;
	PF_Point shift;
} IterateData;

template <typename InFormat, typename OutFormat>
static A_Err CopyBufferIterate(	void 	*refconPV,
								A_long 	thread_indexL,
								A_long 	i,
								A_long 	iterationsL)
{
	A_Err err = A_Err_NONE;
	
	IterateData *i_data = (IterateData *)refconPV;
	
	InFormat *in_pix = (InFormat *)((char *)i_data->in + (i * i_data->in_rowbytes * i_data->scale.v) + (i_data->in_rowbytes * i_data->shift.v) + (i_data->in_channels * sizeof(InFormat) * i_data->shift.h));
	OutFormat *out_pix = (OutFormat *)((char *)i_data->out + (i * i_data->out_rowbytes));
	
	
	int copy_channels = i_data->in_channels;
	int skip_in = 0;
	int skip_out = 0;
	
	if(i_data->in_channels > i_data->out_channels)
	{
		copy_channels = i_data->out_channels;
		
		skip_in = i_data->in_channels - copy_channels;
	}
	else if(i_data->out_channels > i_data->in_channels)
	{
		copy_channels = i_data->in_channels;
		
		skip_out = i_data->out_channels - copy_channels;
	}
	
	
	for(int x=0; x < i_data->width; x++)
	{
		for(int i=0; i < copy_channels; i++)
		{
			*out_pix++ = *in_pix++;
		}
		
		in_pix += skip_in;
		out_pix += skip_out;
		
		in_pix += (i_data->scale.v - 1) * i_data->in_channels;
	}

#ifdef NDEBUG
	if(thread_indexL == 0 && i_data->pbP && i_data->pbP->inter.abort0 && i_data->pbP->inter.refcon)
		err = i_data->pbP->inter.abort0(i_data->pbP->inter.refcon);
#endif

	return err;
}


A_Err	
VRimg_DrawAuxChannel(
	AEIO_BasicData		*basic_dataP,
	VRimg_inData		*options,
	const A_PathType	*file_pathZ,
	A_long				chan_indexL,
	const AEIO_DrawFramePB	*pbP,  // but cancel/progress seems to break here
	PF_Point			scale,
	PF_ChannelChunk		*chunkP)
{
	A_Err	err		= A_Err_NONE,
			err2	= A_Err_NONE;


	// get the channel record
	PF_ChannelDesc desc = options->channel[chan_indexL];

	
	try{
	
	if( IlmThread::supportsThreads() )
		IlmThread::ThreadPool::globalThreadPool().setNumThreads(gNumCPUs);
	
	
	// figure out the layer name we're getting
	string layer_name;
	
	int channel_index = 0;
	bool rotate_channels = false;
	
	ChannelEntry channel_entry(desc.name, desc.channel_type, chunkP->data_type);
	
	if(channel_entry.dimensions() == chunkP->dimensionL)
	{
		// long-form compound name
		string key_channel = channel_entry.key_name();
		
		if(key_channel.size() > 2 && key_channel[ key_channel.size() - 2 ] == '.')
		{
			char chan_name = key_channel[ key_channel.size() - 1 ];
			
			if(chan_name == 'R' || chan_name == 'G' || chan_name == 'B' || chan_name == 'A')
			{
				layer_name = key_channel.substr(0, key_channel.size() - 2);
				
				channel_index = (	chan_name == 'R' ? 0 :
									chan_name == 'G' ? 1 :
									chan_name == 'B' ? 2 :
									chan_name == 'A' ? 3 :
									0);
				
				if(chunkP->dimensionL == 4 && key_channel[ key_channel.size() - 1 ] == 'A')
					rotate_channels = true;
			}
			else
				layer_name = key_channel;
		}
		else
			layer_name = key_channel;
	}
	else
	{
		assert(chunkP->dimensionL > 1);
		
		// short-form compound
		
		string this_compound_layer_name = desc.name;
		
		string::size_type dot_pos = this_compound_layer_name.find_last_of('.');
		
		if(dot_pos != string::npos && dot_pos < (this_compound_layer_name.size() - 1))
		{
			layer_name = this_compound_layer_name.substr(0, dot_pos);
			
			if(chunkP->dimensionL == 4 && this_compound_layer_name[ this_compound_layer_name.size() - 1 ] == 'A')
				rotate_channels = true;
		}
		else
		{
			assert(FALSE); // we didn't have short-form after all
			
			layer_name = this_compound_layer_name;
		}
	}
	
	assert( !layer_name.empty() );
	
	
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);

	IStreamPlatform instream(file_pathZ, basic_dataP->pica_basicP);
	
	if(gMemoryMap)
		instream.memoryMap();
		
	InputFile input(instream);
	
	
	#ifdef NDEBUG
		#define CONT2()	( (pbP && pbP->inter.abort0) ? !(err2 = pbP->inter.abort0(pbP->inter.refcon) ) : TRUE)
		#define PROG2(COUNT, TOTAL)	( (pbP && pbP->inter.progress0) ? !(err2 = pbP->inter.progress0(pbP->inter.refcon, COUNT, TOTAL) ) : TRUE)
		const AEIO_InterruptFuncs *interP = (pbP ? &pbP->inter : NULL);
	#else
		#define CONT2()					TRUE
		#define PROG2(COUNT, TOTAL)		TRUE
		const AEIO_InterruptFuncs *interP = NULL;
	#endif
	
	VRimg_ChannelCache *cache = gCachePool.findCache(instream);
	
	if(cache == NULL && gChannelCaches > 0)
	{
		cache = gCachePool.addCache(input, instream, interP);
	}
	
	
	const Layer *layer = input.header().findLayer(layer_name);
	
	if(layer)
	{
		if(chunkP->dimensionL > 1)
			assert(chunkP->dimensionL == layer->dimensions);
	
		PF_DataType chan_type = (layer->type == VRimg::FLOAT ? PF_DataType_FLOAT : PF_DataType_LONG);
		size_t pix_size = (chan_type == PF_DataType_FLOAT ? sizeof(float) : sizeof(int)); // yeah, probably the same either way, I know
		
		int width = input.header().width();
		int height = input.header().height();
		
		// If AE is at partial resolution, we use this to shift the pixel we sample instead of getting the upper right one
		// We're trying to get closer to AE's scaling algorithm, which we can only use on full ARGB buffers
		PF_Point shift;
		shift.h = (width % scale.h == 0) ? (scale.h - 1) : (width % scale.h - 1);
		shift.v = (height % scale.v == 0) ? (scale.v - 1) : (height % scale.v - 1);
		
		
		void *vrimg_buffer = NULL;
		size_t rowbytes = 0;
		
		// do we need to make a new buffer for the file to read into?
		PF_Handle temp_handle = NULL;
		void *temp_buffer = NULL;
		
		if(width != chunkP->widthL || height != chunkP->heightL || chunkP->dimensionL != layer->dimensions || chan_type != chunkP->data_type)
		{
			rowbytes = pix_size * layer->dimensions * width;
			
			temp_handle = suites.HandleSuite()->host_new_handle(rowbytes * height);
			
			if(temp_handle == NULL)
				throw Iex::NullExc("Can't allocate a temp buffer like I need to.");
			
			vrimg_buffer = temp_buffer = suites.HandleSuite()->host_lock_handle(temp_handle);
		}
		else
		{
			rowbytes = chunkP->row_bytesL;
			
			vrimg_buffer = chunkP->dataPV;
		}
		
		
		if(cache != NULL)
			cache->copyLayerToBuffer(layer_name, vrimg_buffer, rowbytes);
		else
			input.copyLayerToBuffer(layer_name, vrimg_buffer, rowbytes);
		
		
		// if we get a channel with dimension of 4, the last one will be Alpha - AE needs it first
		// actually, this doesn't appear to be possible in VRimg
		if(chunkP->dimensionL == 4 && rotate_channels)
		{
			char *row = (char *)vrimg_buffer;
			
			for(int y=0; y < height; y++)
			{
				PF_Pixel32 *pix = (PF_Pixel32 *)row;
				
				for(int x=0; x < width; x++)
				{
					float temp = pix->blue;
					
					pix->blue = pix->green;
					pix->green = pix->red;
					pix->red = pix->alpha;
					pix->alpha = temp;
					
					pix++;
				}
				
				row += rowbytes;
			}
		}
		
		

		if(temp_buffer != NULL)
		{
			char *channel_input = (char *)vrimg_buffer;
			
			if(layer->dimensions > 1 && chunkP->dimensionL == 1)
				channel_input += pix_size * channel_index;
			
			IterateData i_data = { pbP, (void *)channel_input, rowbytes, layer->dimensions, (void *)chunkP->dataPV, chunkP->row_bytesL, chunkP->dimensionL, MIN(width, chunkP->widthL), scale, shift };
			
			if(chunkP->data_type == PF_DataType_FLOAT)
				err2 = suites.AEGPIterateSuite()->AEGP_IterateGeneric(MIN(height, chunkP->heightL), (void *)&i_data, CopyBufferIterate<float, float>);
			else if(chunkP->data_type == PF_DataType_U_BYTE || chunkP->data_type == PF_DataType_CHAR)
				err2 = suites.AEGPIterateSuite()->AEGP_IterateGeneric(MIN(height, chunkP->heightL), (void *)&i_data, CopyBufferIterate<int, char>);
			else if(chunkP->data_type == PF_DataType_U_SHORT)
				err2 = suites.AEGPIterateSuite()->AEGP_IterateGeneric(MIN(height, chunkP->heightL), (void *)&i_data, CopyBufferIterate<int, unsigned short>);
			else if(chunkP->data_type == PF_DataType_SHORT)
				err2 = suites.AEGPIterateSuite()->AEGP_IterateGeneric(MIN(height, chunkP->heightL), (void *)&i_data, CopyBufferIterate<int, short>);
			else if(chunkP->data_type == PF_DataType_LONG)
				err2 = suites.AEGPIterateSuite()->AEGP_IterateGeneric(MIN(height, chunkP->heightL), (void *)&i_data, CopyBufferIterate<int, int>);
			
			
			suites.HandleSuite()->host_dispose_handle(temp_handle);
		}
		
	}
	else
		err = AEIO_Err_PARSING;

	
	
    }
	catch(CancelExc &e) { err2 = e.err(); }
	catch(...) { err = AEIO_Err_PARSING; }

	if(err2) { err = err2; }
	return err;
}

