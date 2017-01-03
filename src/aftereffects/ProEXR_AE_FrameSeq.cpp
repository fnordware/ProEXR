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

#include "ProEXR_AE_FrameSeq.h"

#include "ProEXR_AE.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "ProEXR_UTF.h"

#include <assert.h>

#ifdef WIN_ENV
#include <Windows.h>
#endif

extern AEGP_PluginID			S_mem_id;


// edit these typedefs for each custom file importer
typedef	ProEXR_outData		format_outData;


static A_Err
EasyCopy(AEIO_BasicData	*basic_dataP, PF_EffectWorld *src, PF_EffectWorld *dest, PF_Boolean hq)
{
	A_Err err =	A_Err_NONE;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);

	if(hq)
		err = suites.PFWorldTransformSuite()->copy_hq(NULL, src, dest, NULL, NULL);
	else
		err = suites.PFWorldTransformSuite()->copy(NULL, src, dest, NULL, NULL);
	
	return err;
}

static A_Err
SmartCopyWorld(
	AEIO_BasicData		*basic_dataP,
	PF_EffectWorld 		*source_World,
	PF_EffectWorld 		*dest_World,
	PF_Boolean 			source_ext,
	PF_Boolean	 		dest_ext, // _ext means 16bpc is true 16-bit (the EXTernal meaning of 16-bit)
	PF_Boolean			source_writeable)
{
	// this is the world copy function AE should have provided
	// with additional attention to external (true) 16-bit worlds
	A_Err err =	A_Err_NONE;
	
	AEGP_SuiteHandler suites(basic_dataP->pica_basicP);
	
	PF_PixelFormat	source_format, dest_format;

	suites.PFWorldSuite()->PF_GetPixelFormat(source_World, &source_format);
	suites.PFWorldSuite()->PF_GetPixelFormat(dest_World, &dest_format);
	
	PF_Boolean hq = FALSE;			
	
	// can we just copy?
	if( (source_format == dest_format) && (source_ext == dest_ext) ) // make sure you are always setting ext's
	{
		EasyCopy(basic_dataP, source_World, dest_World, hq);
	}
	else
	{
		// copy to a buffer of the same size, different bit depth
		PF_EffectWorld temp_World_data;
		PF_EffectWorld *temp_World = &temp_World_data;
		PF_EffectWorld *out_world = dest_World;
		
		
		// if out world is the same size, we'll copy directly, otherwise need temp
		if( (source_World->height == dest_World->width) &&
			(source_World->width  == dest_World->width) )
		{
			temp_World = dest_World;
		}
		else
		{
			suites.PFWorldSuite()->PF_NewWorld(NULL, source_World->width, source_World->height,
													FALSE, dest_format, temp_World);
		}

		char *in_row = (char *)source_World->data,
			*out_row = (char *)temp_World->data;

		int height = source_World->height,	height4 = height * 4;
		int width  = source_World->width,	width4  = width * 4;

		for(int y=0; y < height; y++)
		{
			if(source_format == PF_PixelFormat_ARGB128)
			{
				PF_FpShort *in = (PF_FpShort *)in_row;

				if(dest_format == PF_PixelFormat_ARGB64)
				{
					A_u_short *out = (A_u_short *)out_row;

					if(dest_ext)
						for(int x=0; x < width4; x++)
						{	*out = FLOAT_TO_SIXTEEN(*in); out++; in++;	}
					else
						for(int x=0; x < width4; x++)
						{	*out = FLOAT_TO_AE(*in); out++; in++;	}
				}
				else if(dest_format == PF_PixelFormat_ARGB32)
				{
					A_u_char *out = (A_u_char *)out_row;

					for(int x=0; x < width4; x++)
					{	*out = FLOAT_TO_AE8(*in); out++; in++;	}
				}
			}
			else if(source_format == PF_PixelFormat_ARGB64)
			{
				A_u_short *in = (A_u_short *)in_row;

				if(source_ext)
				{
					if(dest_format == PF_PixelFormat_ARGB128)
					{
						PF_FpShort *out = (PF_FpShort *)out_row;

						for(int x=0; x < width4; x++)
						{	*out = SIXTEEN_TO_FLOAT(*in); out++; in++;	}
					}
					else if(dest_format == PF_PixelFormat_ARGB32)
					{
						A_u_char *out = (A_u_char *)out_row;

						for(int x=0; x < width4; x++)
						{	*out = *in >> 8; out++; in++;	}
					}
				}
				else
				{
					if(dest_format == PF_PixelFormat_ARGB128)
					{
						PF_FpShort *out = (PF_FpShort *)out_row;

						for(int x=0; x < width4; x++)
						{	*out = AE_TO_FLOAT(*in); out++; in++;	}
					}
					else if(dest_format == PF_PixelFormat_ARGB32)
					{
						A_u_char *out = (A_u_char *)out_row;

						for(int x=0; x < width4; x++)
						{	*out = CONVERT16TO8(*in); out++; in++;	}
					}
				}

			}
			else if(source_format == PF_PixelFormat_ARGB32)
			{
				A_u_char *in = (A_u_char *)in_row;

				if(dest_format == PF_PixelFormat_ARGB128)
				{
					PF_FpShort *out = (PF_FpShort *)out_row;

					for(int x=0; x < width4; x++)
					{	*out = AE8_TO_FLOAT(*in); out++; in++;	}
				}
				else if(dest_format == PF_PixelFormat_ARGB64)
				{
					A_u_char *out = (A_u_char *)out_row;

					if(dest_ext)
						for(int x=0; x < width4; x++)
						{	*out = ( ((long)*in * 0xFFFF) + PF_HALF_CHAN8 ) / PF_MAX_CHAN8; out++; in++;	} // uhh, I think this works
					else
						for(int x=0; x < width4; x++)
						{	*out = CONVERT8TO16(*in); out++; in++;	}
				}
			}

			in_row += source_World->rowbytes;
			out_row += temp_World->rowbytes;
		}

		// copy from temp world if necessary, dispose temp buffer
		if(temp_World != dest_World)
		{
			EasyCopy(basic_dataP, temp_World, dest_World, hq);

			suites.PFWorldSuite()->PF_DisposeWorld(NULL, temp_World);
		}
	}

	return err;
}

#ifdef AE_HFS_PATHS
// convert from HFS paths (fnord:Users:mrb:) to Unix paths (/Users/mrb/)
static int ConvertPath(const char * inPath, char * outPath, int outPathMaxLen)
{
	CFStringRef inStr = CFStringCreateWithCString(kCFAllocatorDefault, inPath ,kCFStringEncodingMacRoman);
	if (inStr == NULL)
		return -1;
	CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, inStr, kCFURLHFSPathStyle,0);
	if (url == NULL)
		return -1;
	CFStringRef outStr = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
	if (!CFStringGetCString(outStr, outPath, outPathMaxLen, kCFStringEncodingMacRoman))
		return -1;
	CFRelease(outStr);
	CFRelease(url);
	CFRelease(inStr);
	return 0;
}
#endif // __MACH__


static void
SetRenderInfo(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH	outH,
	Render_Info		*render_info)
{
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);
	
	render_info->proj_name[0] = '\0';
	render_info->comp_name[0] = '\0';
	render_info->comment_str[0] = '\0';
	render_info->computer_name[0] = '\0';
	render_info->user_name[0] = '\0';
	render_info->framerate.value = 0;
	
	AEGP_RQItemRefH rq_itemH = NULL;
	AEGP_OutputModuleRefH outmodH = NULL;
	
	// apparently 0x00 is a valid value for rq_itemH and outmodH
	A_Err err = suites.IOOutSuite()->AEGP_GetOutSpecOutputModule(outH, &rq_itemH, &outmodH);
	
	if(!err)
	{
		// get the comment
		suites.RQItemSuite()->AEGP_GetComment(rq_itemH, render_info->comment_str);
		
		// comp name
		AEGP_CompH compH = NULL;
		suites.RQItemSuite()->AEGP_GetCompFromRQItem(rq_itemH, &compH);
		
		if(compH) // should never fail, but to be safe
		{
			AEGP_ItemH itemH = NULL;
			
			suites.CompSuite()->AEGP_GetItemFromComp(compH, &itemH);
			
			if(itemH)
			{
			#ifdef AE_UNICODE_NAMES
				AEGP_MemHandle nameH = NULL;
				
				suites.ItemSuite()->AEGP_GetItemName(S_mem_id, itemH, &nameH);
				
				if(nameH)
				{
					AEGP_MemSize size = 0;
					suites.MemorySuite()->AEGP_GetMemHandleSize(nameH, &size);
					
					A_NameType *nameP = NULL;
					suites.MemorySuite()->AEGP_LockMemHandle(nameH, (void **)&nameP);
					
					std::string comp_name = UTF16toUTF8(nameP);
					
					strncpy(render_info->comp_name, comp_name.c_str(), COMP_NAME_SIZE-1);
					
					suites.MemorySuite()->AEGP_FreeMemHandle(nameH);
				}
			#else
				suites.ItemSuite()->AEGP_GetItemName(itemH, render_info->comp_name);
			#endif
			}
			
			// frame duration
			if(!err)
			{
				AEGP_MemHandle formatH = NULL, infoH = NULL;
				A_Boolean is_sequence = TRUE, multi_frame = TRUE;
			
				suites.OutputModuleSuite()->AEGP_GetExtraOutputModuleInfo(rq_itemH, outmodH,
														&formatH, &infoH, &is_sequence, &multi_frame);
				
				if(multi_frame)
				{
					A_Time frame_duration;
					suites.CompSuite()->AEGP_GetCompFrameDuration(compH, &frame_duration);
					
					A_Fixed fixed_fps; // not using - prefer the rational frame_duration
					suites.IOOutSuite()->AEGP_GetOutSpecFPS(outH, &fixed_fps);
					
					// frame rate = 1 / frame duration
					render_info->framerate.value = frame_duration.scale;
					render_info->framerate.scale = frame_duration.value;
				}
				
				if(formatH) // just says "OpenEXR"
					suites.MemorySuite()->AEGP_FreeMemHandle(formatH);

				if(infoH) // says something like "B44A compression\nLuminance/Chroma"
					suites.MemorySuite()->AEGP_FreeMemHandle(infoH);
			}
		}
	}	
		
	// project name
	A_long projects = 0;
	suites.ProjSuite()->AEGP_GetNumProjects(&projects);
	
	if(projects == 1)
	{
		AEGP_ProjectH projH = NULL;
		
		suites.ProjSuite()->AEGP_GetProjectByIndex(0, &projH);
		
		if(projH)
			suites.ProjSuite()->AEGP_GetProjectName(projH, render_info->proj_name);
	}

#ifdef MAC_ENV
	// user and computer name
	CFStringRef user_name = CSCopyUserName(FALSE);  // set this to TRUE for shorter unix-style name
	CFStringRef comp_name = CSCopyMachineName();
	
	if(user_name)
	{
		CFStringGetCString(user_name, render_info->user_name, COMPUTER_NAME_SIZE-1, kCFStringEncodingMacRoman);
		CFRelease(user_name);
	}
	
	if(comp_name)
	{
		CFStringGetCString(comp_name, render_info->computer_name, COMPUTER_NAME_SIZE-1, kCFStringEncodingMacRoman);
		CFRelease(comp_name);
	}
#else // WIN_ENV
	DWORD buf_size = COMPUTER_NAME_SIZE-1;
	GetComputerName(render_info->computer_name, &buf_size);
	buf_size = COMPUTER_NAME_SIZE-1;
	GetUserName(render_info->user_name, &buf_size);
#endif
}


#pragma mark-


A_Err
FrameSeq_Init(struct SPBasicSuite *pica_basicP)
{
	return ProEXR_Init(pica_basicP);
}


A_Err
FrameSeq_ConstructModuleInfo(
	AEIO_ModuleInfo	*info)
{
	// tell AE all about our capabilities
	return ProEXR_ConstructModuleInfo(info);
}


#pragma mark-


A_Err	
FrameSeq_GetDepths(
	AEIO_BasicData			*basic_dataP,
	AEIO_OutSpecH			outH, 
	AEIO_SupportedDepthFlags	*which)
{
	// just tell us what depths to enable in the menu
	return ProEXR_GetDepths(which);
}


A_Err	
FrameSeq_InitOutputSpec(
	AEIO_BasicData			*basic_dataP,
	AEIO_OutSpecH			outH, 
	A_Boolean				*user_interacted)
{
	// pass a new handle with output options
	// you may have to initialize the data, but you probably have
	// an old options handle to read from (and then dispose actually)
	
	A_Err err						=	A_Err_NONE;

	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);

	AEIO_Handle		optionsH		=	NULL,
					old_optionsH	=	NULL,
					old_old_optionsH=	NULL;
	format_outData	*options			=	NULL,
					*old_options		=	NULL;


	AEGP_MemSize mem_size;

	// make new options handle
	suites.MemorySuite()->AEGP_NewMemHandle( S_mem_id, "Output Options",
											sizeof(format_outData),
											AEGP_MemFlag_CLEAR, &optionsH);
	
	err = suites.MemorySuite()->AEGP_GetMemHandleSize(optionsH, &mem_size);
	
	err = suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);

	
	// get old options
	suites.IOOutSuite()->AEGP_GetOutSpecOptionsHandle(outH, (void**)&old_optionsH);
	
	
	if(old_optionsH)
	{
		AEGP_MemSize old_size;
		
		err = suites.MemorySuite()->AEGP_GetMemHandleSize(old_optionsH, &old_size);
		
		if(old_size < mem_size)
		{
			suites.MemorySuite()->AEGP_UnlockMemHandle(old_optionsH);
			suites.MemorySuite()->AEGP_ResizeMemHandle("Old Handle Resize", mem_size, old_optionsH);
		}
		
		suites.MemorySuite()->AEGP_LockMemHandle(old_optionsH, (void**)&old_options);
		
		// first we copy the data
		memcpy((char*)options, (char*)old_options, mem_size);
		
		// then we inflate it
		ProEXR_InflateOutputOptions((format_outData *)options);

		suites.MemorySuite()->AEGP_UnlockMemHandle(old_optionsH);
	}
	else
	{
		err = ProEXR_InitOutOptions(options);
	}

	suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	
	
	// set the options handle
	suites.IOOutSuite()->AEGP_SetOutSpecOptionsHandle(outH, (void*)optionsH, (void**)&old_old_optionsH);
	
	
	// so now AE wants me to delete this. whatever.
	if(old_old_optionsH)
		suites.MemorySuite()->AEGP_FreeMemHandle(old_old_optionsH);

	
	return err;
}


A_Err	
FrameSeq_OutputFrame(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH			outH, 
	const PF_EffectWorld	*wP)
{
	// write that frame
	// again, we'll get the info and provide a suitable buffer
	// you just write the file, big guy
	
	A_Err err						=	A_Err_NONE;

	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);


	AEIO_Handle			optionsH		=	NULL;
	format_outData		*options		=	NULL;

	PF_EffectWorld		temp_World_data;
	
	PF_EffectWorld		*temp_World		=	&temp_World_data,
						*active_World	=	NULL;

	PF_Pixel 			premult_color = {0, 0, 0, 255};
	AEIO_AlphaLabel		alpha;
	FIEL_Label			field;
	
	A_Chromaticities	chrom;
	
	Render_Info			render_info;
	
	A_short				depth;
	
	FrameSeq_Info		info;
	
	
	// get options data
	suites.IOOutSuite()->AEGP_GetOutSpecOptionsHandle(outH, (void**)&optionsH);
	
	if(optionsH)
		suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
	
	
	// get file path
#ifdef AE_UNICODE_PATHS
	AEGP_MemHandle pathH = NULL;
	A_PathType *file_pathZ = NULL;
	
	A_Boolean file_reservedPB = FALSE; // WTF?
	suites.IOOutSuite()->AEGP_GetOutSpecFilePath(outH, &pathH, &file_reservedPB);
	
	if(pathH)
	{
		suites.MemorySuite()->AEGP_LockMemHandle(pathH, (void **)&file_pathZ);
	}
	else
		return AEIO_Err_BAD_FILENAME; 
#else
	A_PathType file_pathZ[AEGP_MAX_PATH_SIZE+1];
	
	A_Boolean file_reservedPB = FALSE; // WTF?
	suites.IOOutSuite()->AEGP_GetOutSpecFilePath(outH, file_pathZ, &file_reservedPB);
#endif
	
#ifdef AE_HFS_PATHS	
	// convert the path format
	if(A_Err_NONE != ConvertPath(file_pathZ, file_pathZ, AEGP_MAX_PATH_SIZE-1) )
		return AEIO_Err_BAD_FILENAME; 
#endif
	
	// get dimensions
	suites.IOOutSuite()->AEGP_GetOutSpecDimensions(outH, &info.width, &info.height);
	
	
	// get depth
	suites.IOOutSuite()->AEGP_GetOutSpecDepth(outH, &depth);

	// translate to planes & depth - negative depths are greyscale
	info.planes = (depth == 32 || depth == 64 || depth == 128) ?  4 : (depth == AEIO_InputDepth_GRAY_8 || depth == AEIO_InputDepth_GRAY_16 || depth == AEIO_InputDepth_GRAY_32) ? 1 : 3;
	info.depth  = (depth == AEIO_InputDepth_GRAY_16 || depth == 48 || depth == 64) ? 16 : (depth == 96 || depth == 128 || depth == AEIO_InputDepth_GRAY_32) ? 32 : 8;
	
	
	// get pixel aspect ratio
	suites.IOOutSuite()->AEGP_GetOutSpecHSF(outH, &info.pixel_aspect_ratio);

	
	
	// get alpha info
	suites.IOOutSuite()->AEGP_GetOutSpecAlphaLabel(outH, &alpha);
	
	info.alpha_type = alpha.alpha;
	
	premult_color.red   = alpha.red;
	premult_color.green = alpha.green;
	premult_color.blue  = alpha.blue;
	info.premult_color = &premult_color;


	// get field info
	suites.IOOutSuite()->AEGP_GetOutSpecInterlaceLabel(outH, &field);
	
	info.field_label = &field;
	
	
	// ICC Profile
	AEGP_ColorProfileP ae_color_profile = NULL;
	AEGP_MemHandle icc_profileH = NULL;
	info.icc_profile = NULL;
	info.icc_profile_len = 0;
	info.chromaticities = NULL;
	
	A_Boolean embed_profile = FALSE;
	suites.IOOutSuite()->AEGP_GetOutSpecShouldEmbedICCProfile(outH, &embed_profile);
	
	if(embed_profile)
	{
		suites.IOOutSuite()->AEGP_GetNewOutSpecColorProfile(S_mem_id, outH, &ae_color_profile);
		
		if(ae_color_profile)
		{
			suites.ColorSettingsSuite()->AEGP_GetNewICCProfileFromColorProfile(S_mem_id, ae_color_profile, &icc_profileH);
			
			if(icc_profileH)
			{
				AEGP_MemSize mem_size;
				void *icc = NULL;
		
				suites.MemorySuite()->AEGP_GetMemHandleSize(icc_profileH, &mem_size);
				suites.MemorySuite()->AEGP_LockMemHandle(icc_profileH, (void**)&icc);
				
				if(icc)
				{
					info.icc_profile = icc;
					info.icc_profile_len = mem_size;
					
					Init_Chromaticities(&chrom);
					Profile_To_Chromaticities(info.icc_profile, info.icc_profile_len, &chrom);
					
					info.chromaticities = &chrom;
				}
			}
		}
	}

			
	// render info
	SetRenderInfo(basic_dataP, outH, &render_info);
	info.render_info = &render_info;
		
	
	// for EXR, we always want to pass a float buffer
	PF_PixelFormat	pixel_format;

	suites.PFWorldSuite()->PF_GetPixelFormat(wP, &pixel_format);

	if( pixel_format == PF_PixelFormat_ARGB128 )
	{
		active_World = (PF_EffectWorld *)wP;
		
		temp_World = NULL;
	}
	else
	{
		// make our own float PF_EffectWorld
		suites.PFWorldSuite()->PF_NewWorld(NULL, info.width, info.height, FALSE,
												PF_PixelFormat_ARGB128, temp_World);
		
		// copy the world
		SmartCopyWorld(basic_dataP, (PF_EffectWorld *)wP, temp_World, FALSE, FALSE, FALSE);
			
		active_World = temp_World;
	}


	// write out image (finally)
	err = ProEXR_OutputFile(basic_dataP, outH, file_pathZ, &info, options, active_World);

	
	// dispose temp world if we made one	
	if(temp_World)
		suites.PFWorldSuite()->PF_DisposeWorld(NULL, temp_World);


	// dispose color profile stuff
	if(icc_profileH)
		suites.MemorySuite()->AEGP_FreeMemHandle(icc_profileH);
	
	if(ae_color_profile)
		suites.ColorSettingsSuite()->AEGP_DisposeColorProfile(ae_color_profile);


#ifdef AE_UNICODE_PATHS
	if(pathH)
		suites.MemorySuite()->AEGP_FreeMemHandle(pathH);
#endif

	// dispose options
	if(optionsH)
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	
	return err;
}


A_Err	
FrameSeq_UserOptionsDialog(
	AEIO_BasicData		*basic_dataP,
	AEIO_OutSpecH		outH, 
	const PF_EffectWorld	*sample0,
	A_Boolean			*user_interacted0)
{
	// output options dialog here
	// we'll give you the options data, you change it
	
	A_Err err						=	A_Err_NONE;

	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);

	AEIO_Handle			optionsH		=	NULL;
	format_outData	*options			=	NULL;
	
	// get options handle
	err = suites.IOOutSuite()->AEGP_GetOutSpecOptionsHandle(outH, (void**)&optionsH);

	if(optionsH)
		err = suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);


	if(!err && options)
	{
		// do a dialog and change those output options
		err = ProEXR_WriteOptionsDialog(basic_dataP, options, user_interacted0);
	}


	// done with options
	if(optionsH)
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	
	return err;
}


A_Err	
FrameSeq_GetOutputInfo(
	AEIO_BasicData		*basic_dataP,
	AEIO_OutSpecH		outH,
	AEIO_Verbiage		*verbiageP)
{ 
	// all this function does is print details about our output options
	// in the output module window
	// or rather, gets the options so your function can do that

	A_Err err			=	A_Err_NONE;
	
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);

	AEIO_Handle		optionsH		=	NULL;
	format_outData	*options			=	NULL;


	// get file path (but don't freak out if it's an invalid path (do you really need a path here?))
#ifdef AE_UNICODE_PATHS
	AEGP_MemHandle pathH = NULL;
	A_PathType *file_pathZ = NULL;
	
	A_Boolean file_reservedPB = FALSE; // WTF?
	suites.IOOutSuite()->AEGP_GetOutSpecFilePath(outH, &pathH, &file_reservedPB);
	
	if(pathH)
	{
		suites.MemorySuite()->AEGP_LockMemHandle(pathH, (void **)&file_pathZ);
	}
	else
		return AEIO_Err_BAD_FILENAME; 
#else
	A_PathType file_pathZ[AEGP_MAX_PATH_SIZE+1];
	
	A_Boolean file_reservedPB = FALSE; // WTF?
	suites.IOOutSuite()->AEGP_GetOutSpecFilePath(outH, file_pathZ, &file_reservedPB);
#endif
	
#ifdef AE_HFS_PATHS	
	// convert the path format
	ConvertPath(file_pathZ, file_pathZ, AEGP_MAX_PATH_SIZE-1);
#endif


	// get options handle
	err = suites.IOOutSuite()->AEGP_GetOutSpecOptionsHandle(outH, (void**)&optionsH);

	if(optionsH)
		err = suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
		
	
	// initialize the verbiage
	verbiageP->type[0] = '\0';
	verbiageP->sub_type[0] = '\0';

	if(!err)
	{
		err = ProEXR_GetOutSpecInfo(file_pathZ, options, verbiageP);
		
		// done with options
		if(optionsH)
			suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	}

#ifdef AE_UNICODE_PATHS
	if(pathH)
		suites.MemorySuite()->AEGP_FreeMemHandle(pathH);
#endif

	return err;
}


A_Err	
FrameSeq_DisposeOutputOptions(
	AEIO_BasicData	*basic_dataP,
	void			*optionsPV) // what the...?
{ 
	// the options gotta go sometime
	// couldn't you just say optionsPV was a handle?
	
	A_Err				err			=	A_Err_NONE; 
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);	

	// here's our options handle apparently
	//AEIO_Handle		optionsH	=	reinterpret_cast<AEIO_Handle>(optionsPV);
	AEIO_Handle		optionsH	=	static_cast<AEIO_Handle>(optionsPV);
	
	if (!err && optionsH)
	{
		err = suites.MemorySuite()->AEGP_FreeMemHandle(optionsH);
	}
	return err;
}


A_Err	
FrameSeq_GetFlatOutputOptions(
	AEIO_BasicData	*basic_dataP,
	AEIO_OutSpecH	outH, 
	AEIO_Handle		*flat_optionsPH)
{
	// give AE and handle with flat options for saving
	// but don't delete the old handle, AE still wants it
	
	A_Err				err			=	A_Err_NONE; 
	AEIO_Handle			optionsH	=	NULL;
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);	

	
	// get the options for flattening
	err = suites.IOOutSuite()->AEGP_GetOutSpecOptionsHandle(outH, reinterpret_cast<void**>(&optionsH));

	if (!err && optionsH)
	{
		AEGP_MemSize mem_size;
		
		format_outData *round_data, *flat_data;
		
		// make a new handle that's the same size
		// we're assuming that the options are already flat
		// although they may need byte flippage
		suites.MemorySuite()->AEGP_GetMemHandleSize(optionsH, &mem_size);
		
		suites.MemorySuite()->AEGP_NewMemHandle( S_mem_id, "Flat Options",
												mem_size,
												AEGP_MemFlag_CLEAR, flat_optionsPH);
		
		suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&round_data);
		suites.MemorySuite()->AEGP_LockMemHandle(*flat_optionsPH, (void**)&flat_data);
		
		// first we copy
		memcpy((char*)flat_data, (char*)round_data, mem_size);
		
		// then we flatten
		ProEXR_FlattenOutputOptions((format_outData *)flat_data);

		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
		suites.MemorySuite()->AEGP_UnlockMemHandle(*flat_optionsPH);
		
		// just because we're flattening the options doesn't mean we're done with them
		//suites.MemorySuite()->AEGP_FreeMemHandle(optionsH);
	}
	
	return err;
}


A_Err
FrameSeq_DeathHook(const SPBasicSuite *pica_basicP)
{
	return ProEXR_DeathHook(pica_basicP);
}
