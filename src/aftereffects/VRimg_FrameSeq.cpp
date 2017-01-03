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

#include "VRimg_FrameSeq.h"

#include "VRimg.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <assert.h>

#ifdef WIN_ENV
#include <Windows.h>
#endif

extern AEGP_PluginID			S_mem_id;


// edit these typedefs for each custom file importer
typedef	VRimg_inData		format_inData;


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

// platform-specific calls to get the file size
static void GetFileSize(const A_PathType *path, A_long *size)
{
#ifdef MAC_ENV
	OSStatus status = noErr;
	FSRef fsref;
	
	// expecting a POSIX path here
#ifdef AE_UNICODE_PATHS	
	int len = 0;
	while(path[len++] != 0);
	
	CFStringRef inStr = CFStringCreateWithCharacters(kCFAllocatorDefault, path, len);
	if (inStr == NULL)
		return;
		
	CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, inStr, kCFURLPOSIXPathStyle, 0);
	CFRelease(inStr);
	if (url == NULL)
		return;
	
	Boolean success = CFURLGetFSRef(url, &fsref);
	CFRelease(url);
	if(!success)
		return;
#else	
	status = FSPathMakeRef((const UInt8 *)path, &fsref, false);	
#endif

	if(status == noErr)
	{
		OSErr result = noErr;

	#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050
		typedef SInt16 FSIORefNum;
	#endif
		
		HFSUniStr255 dataForkName;
		FSIORefNum refNum;
		SInt64 fork_size;
	
		result = FSGetDataForkName(&dataForkName);

		result = FSOpenFork(&fsref, dataForkName.length, dataForkName.unicode, fsRdPerm, &refNum);
		
		result = FSGetForkSize(refNum, &fork_size);
		
		result = FSCloseFork(refNum);
		
		*size = MIN(INT_MAX, fork_size);
	}
#else
#ifdef AE_UNICODE_PATHS
	HANDLE hFile = CreateFileW((LPCWSTR)path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#else
	HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#endif

	if(hFile)
	{
		*size = GetFileSize(hFile, NULL);

		CloseHandle(hFile);
	}
#endif
}

#pragma mark-

A_Err
VRimg_FrameSeq_Init(struct SPBasicSuite *pica_basicP)
{
	return VRimg_Init(pica_basicP);
}

A_Err
VRimg_FrameSeq_DeathHook(const SPBasicSuite *pica_basicP)
{
	return VRimg_DeathHook(pica_basicP);
}


A_Err
VRimg_FrameSeq_IdleHook(const SPBasicSuite *pica_basicP, A_long *max_sleepPL)
{
	return VRimg_IdleHook(pica_basicP);
}


A_Err
VRimg_FrameSeq_PurgeHook(const SPBasicSuite *pica_basicP)
{
	return VRimg_PurgeHook(pica_basicP);
}


A_Err
VRimg_FrameSeq_ConstructModuleInfo(
	AEIO_ModuleInfo	*info)
{
	// tell AE all about our capabilities
	return VRimg_ConstructModuleInfo(info);
}

#pragma mark-

A_Err	
VRimg_FrameSeq_VerifyFileImportable(
	AEIO_BasicData			*basic_dataP,
	AEIO_ModuleSignature	sig, 
	const A_PathType		*file_pathZ, 
	A_Boolean				*importablePB)
{ 
	// quick check to see if file is really in our format
#ifdef AE_HFS_PATHS	
	A_char				pathZ[AEGP_MAX_PATH_SIZE];

	// convert the path format
	if(A_Err_NONE != ConvertPath(file_pathZ, pathZ, AEGP_MAX_PATH_SIZE-1) )
		return AEIO_Err_BAD_FILENAME;
#else
	const A_PathType *pathZ = file_pathZ;
#endif

	return VRimg_VerifyFile(pathZ, importablePB);
}		


A_Err	
VRimg_FrameSeq_GetInSpecInfo(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH, 
	AEIO_Verbiage	*verbiageP)
{ 
	// all this does it print the info (verbiage) about the file in the project window
	// you can also mess with file name and type if you must
	
	A_Err err			=	A_Err_NONE;
	
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);

	A_PathType			file_nameZ[AEGP_MAX_PATH_SIZE] = {'\0'};
	
	AEIO_Handle		optionsH		=	NULL;
	format_inData	*options			=	NULL;


	// get file path (or not - this can cause errors if the file lived on a drive that's been unmounted)
	//err = suites.IOInSuite2()->AEGP_GetInSpecFilePath(specH, file_nameZ);


	// get options handle
	err = suites.IOInSuite()->AEGP_GetInSpecOptionsHandle(specH, (void**)&optionsH);

	if(optionsH)
		err = suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
		
	
	// initialize the verbiage
	verbiageP->type[0] = '\0';
	verbiageP->sub_type[0] = '\0';
	
	
	if(!err)
	{
		err = VRimg_GetInSpecInfo(file_nameZ, options, verbiageP);
		
		// done with options
		if(optionsH)
			suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	}

	return err;
}


A_Err	
VRimg_FrameSeq_InitInSpecFromFile(
	AEIO_BasicData		*basic_dataP,
	const A_PathType	*file_pathZ,
	AEIO_InSpecH		specH)
{ 
	// tell AE all the necessary information
	// we pass our format function a nice, easy struct to populate and then deal with it
	
	A_Err err						=	A_Err_NONE;

	AEIO_Handle		optionsH		=	NULL,
					old_optionsH	=	NULL;
	format_inData	*options			=	NULL;

	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);
	
	A_long			file_size = 0;
	

	// fill out the data structure for our function
	PF_Pixel 	premult_color = {255, 0, 0, 0};
	FIEL_Label	field_label;

	FrameSeq_Info info; // this is what we must tell AE about a frame
	
	//A_Chromaticities chrm;
	
#ifdef AE_HFS_PATHS	
	A_char		pathZ[AEGP_MAX_PATH_SIZE];

	// convert the path format
	if(A_Err_NONE != ConvertPath(file_pathZ, pathZ, AEGP_MAX_PATH_SIZE-1) )
		return AEIO_Err_BAD_FILENAME;
#else
	const A_PathType *pathZ = file_pathZ;
#endif

	// fill in the file size
	GetFileSize(pathZ, &file_size);
	
	
	AEFX_CLR_STRUCT(field_label);
	field_label.order		=	FIEL_Order_LOWER_FIRST;
	field_label.type		=	FIEL_Type_UNKNOWN;
	field_label.version		=	FIEL_Label_VERSION;
	field_label.signature	=	FIEL_Tag;
	
	
	info.width			= 0;
	info.height			= 0;
	info.planes			= 0;
	info.depth			= 8;
	info.alpha_type		= AEIO_Alpha_UNKNOWN;
	info.premult_color	= &premult_color;
	info.field_label	= &field_label;
	info.pixel_aspect_ratio.num = info.pixel_aspect_ratio.den = 1;
	
	info.icc_profile = NULL;
	info.icc_profile_len = 0;
	
	//Init_Chromaticities(&chrm);
	//info.chromaticities = &chrm;
	
	//info.render_info = NULL;
	
	
	// set up options data
	suites.MemorySuite()->AEGP_NewMemHandle( S_mem_id, "Input Options",
											sizeof(format_inData),
											AEGP_MemFlag_CLEAR, &optionsH);

	err = suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
	
	// set input defaults
	VRimg_InitInOptions(options);

	
	// run the function
	err = VRimg_FileInfo(basic_dataP, pathZ, &info, optionsH);

	
	// communicate into to AE
	if(!err)
	{
		// Tell AE about our file
		A_Time			dur;
		A_Boolean		has_alpha;
			
		A_short			depth = info.planes < 3 ? (info.depth == 32 ? AEIO_InputDepth_GRAY_32 : (info.depth == 16 ? AEIO_InputDepth_GRAY_16 : AEIO_InputDepth_GRAY_8)) : // greyscale
								info.planes * info.depth; // not

		err = suites.IOInSuite()->AEGP_SetInSpecDepth(specH, depth);

		err = suites.IOInSuite()->AEGP_SetInSpecDimensions(specH, info.width, info.height);
		
		// have to set duration.scale or AE complains
		dur.value = 0;
		dur.scale = 100;
		err = suites.IOInSuite()->AEGP_SetInSpecDuration(specH, &dur);

		// might get a zero denominator error without this
		//suites.IOInSuite2()->AEGP_SetInSpecNativeFPS(specH, FLOAT_2_FIX(24.0) );


		has_alpha = ( (info.planes == 4) || (info.planes == 2) ) &&
						(info.alpha_type != AEIO_Alpha_NONE);

		if (!err && info.alpha_type != AEIO_Alpha_UNKNOWN)
		{
			AEIO_AlphaLabel	alpha;
			AEFX_CLR_STRUCT(alpha);

			alpha.alpha		=	has_alpha ? info.alpha_type : AEIO_Alpha_NONE;
			alpha.flags		=	(alpha.alpha == AEIO_Alpha_PREMUL) ? AEIO_AlphaPremul : 0;
			alpha.version	=	AEIO_AlphaLabel_VERSION;
			alpha.red		=	premult_color.red;
			alpha.green		=	premult_color.green;
			alpha.blue		=	premult_color.blue;
			
			err = suites.IOInSuite()->AEGP_SetInSpecAlphaLabel(specH, &alpha);
		}
		
		if (!err && field_label.type != FIEL_Type_UNKNOWN)
		{
			err = suites.IOInSuite()->AEGP_SetInSpecInterlaceLabel(specH, &field_label);
		}
		
		if(info.pixel_aspect_ratio.num != info.pixel_aspect_ratio.den)
		{
			err = suites.IOInSuite()->AEGP_SetInSpecHSF(specH, &info.pixel_aspect_ratio);
		}
		
		if(file_size != 0)
		{
			err = suites.IOInSuite()->AEGP_SetInSpecSize(specH, file_size);
		}
		
		// set options handle
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
		
		suites.IOInSuite()->AEGP_SetInSpecOptionsHandle(specH, (void*)optionsH, (void**)&old_optionsH);
		
		// see ya, old options
		// wait, actually, DON'T delete this, or AE will crash when reading cross platform projects
		//if(old_optionsH)
		//	suites.MemorySuite1()->AEGP_FreeMemHandle(old_optionsH);
	}
	else
	{
		// get rid of the options handle because we got an error
		if(optionsH)
			suites.MemorySuite()->AEGP_FreeMemHandle(optionsH);
	}


	return err;
}


A_Err	
VRimg_FrameSeq_DrawSparseFrame(
	AEIO_BasicData					*basic_dataP,
	AEIO_InSpecH					specH, 
	const AEIO_DrawSparseFramePB	*sparse_framePPB, 
	PF_EffectWorld					*wP,
	AEIO_DrawingFlags				*draw_flagsP)
{ 
	// we'll give a file-size buffer for filling and then fit it
	// to what AE wants
	
	A_Err err						=	A_Err_NONE;

	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);

	AEIO_Handle		optionsH		=	NULL;
	format_inData	*options			=	NULL;

	
	// file path
#ifdef AE_UNICODE_PATHS
	AEGP_MemHandle pathH = NULL;
	A_PathType *file_nameZ = NULL;
	
	suites.IOInSuite()->AEGP_GetInSpecFilePath(specH, &pathH);
	
	if(pathH)
	{
		suites.MemorySuite()->AEGP_LockMemHandle(pathH, (void **)&file_nameZ);
	}
	else
		return AEIO_Err_BAD_FILENAME; 
#else
	A_PathType				file_nameZ[AEGP_MAX_PATH_SIZE];
	
	suites.IOInSuite()->AEGP_GetInSpecFilePath(specH, file_nameZ);
#endif
	
#ifdef AE_HFS_PATHS	
	// convert the path format
	if(A_Err_NONE != ConvertPath(file_nameZ, file_nameZ, AEGP_MAX_PATH_SIZE-1) )
		return AEIO_Err_BAD_FILENAME; 
#endif

	// fill out the data structure for our function
	// should match what we previously filled
	// although this time we're asking AE for the info
	FrameSeq_Info info;
	
	A_long width, height;
	A_short depth;

	PF_Pixel 	premult_color = {0, 0, 0, 255};
	FIEL_Label	field_label;

	AEIO_AlphaLabel alphaL;
	
	// get all that info from AE
	suites.IOInSuite()->AEGP_GetInSpecDimensions(specH, &width, &height);
	suites.IOInSuite()->AEGP_GetInSpecDepth(specH, &depth);
	suites.IOInSuite()->AEGP_GetInSpecAlphaLabel(specH, &alphaL);
	suites.IOInSuite()->AEGP_GetInSpecInterlaceLabel(specH, &field_label);
	
	// set the info struct
	info.width = width;
	info.height = height;
	info.planes = (depth == 32 || depth == 64 || depth == 128) ? 4 : (depth == AEIO_InputDepth_GRAY_8 || depth == AEIO_InputDepth_GRAY_16 || depth == AEIO_InputDepth_GRAY_32) ? 1 : 3;
	info.depth  = (depth == 48 || depth == 64 || depth == AEIO_InputDepth_GRAY_16) ? 16 : (depth == 96 || depth == 128 || depth == AEIO_InputDepth_GRAY_32) ? 32 : 8;
	
	info.alpha_type = alphaL.alpha;
	premult_color.red = alphaL.red;
	premult_color.green = alphaL.green;
	premult_color.blue = alphaL.blue;
	info.premult_color = &premult_color;
	
	info.field_label = &field_label;

	
	// get options handle
	err = suites.IOInSuite()->AEGP_GetInSpecOptionsHandle(specH, (void**)&optionsH);


	if(optionsH)
		err = suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
	
	
	if (!err) // so far so good?
	{
		PF_EffectWorld	temp_World_data;
		
		PF_EffectWorld	*temp_World = &temp_World_data,
					*active_World = NULL;
		
		PF_PixelFormat	pixel_format;

		suites.PFWorldSuite()->PF_GetPixelFormat(wP, &pixel_format);

		A_long		wP_depth =	(pixel_format == PF_PixelFormat_ARGB128)? 32 :
								(pixel_format == PF_PixelFormat_ARGB64) ? 16 : 8;
		

		// here's the only time we won't need to make our own buffer
		if(	(info.width == wP->width) && (info.height == wP->height) && (wP_depth == 32) )
		{
			active_World = wP; // just use the PF_EffectWorld AE gave us
			
			temp_World = NULL; // won't be needing this
		}
		else
		{
			// make our own PF_EffectWorld
			suites.PFWorldSuite()->PF_NewWorld(NULL, info.width, info.height, FALSE,
													PF_PixelFormat_ARGB128, temp_World);
			
			active_World = temp_World;
		}


		// should always pass a full-sized float world to write into (using options we pass)
		err = VRimg_DrawSparseFrame(basic_dataP, sparse_framePPB, active_World,
										draw_flagsP, file_nameZ, &info, options);

		

		if(temp_World)
		{
			SmartCopyWorld(basic_dataP, temp_World, wP, FALSE, FALSE, TRUE);

			suites.PFWorldSuite()->PF_DisposeWorld(NULL, temp_World);
		}


#ifdef AE_UNICODE_PATHS
	if(pathH)
		suites.MemorySuite()->AEGP_FreeMemHandle(pathH);
#endif

		// done with options
		if(optionsH)
			suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	}


	return err;
}


A_Err	
VRimg_FrameSeq_SeqOptionsDlg(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,  
	A_Boolean		*user_interactedPB0)
{
	A_Err err						=	A_Err_NONE;

	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);

	AEIO_Handle		optionsH		=	NULL;
	format_inData	*options			=	NULL;
	
	// get options handle
	err = suites.IOInSuite()->AEGP_GetInSpecOptionsHandle(specH, (void**)&optionsH);

	if(optionsH)
		err = suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
	

	if(!err && options)
	{
		// pop up a dialog and change those read options
		err = VRimg_ReadOptionsDialog(basic_dataP, options, user_interactedPB0);
	}


	// done with options
	if(optionsH)
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	

	return err;
}


A_Err
VRimg_FrameSeq_DisposeInSpec(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH)
{ 
	// dispose input options
	
	A_Err				err			=	A_Err_NONE; 
	AEIO_Handle			optionsH	=	NULL;
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);	

	// I guess we'll dump the handle if we've got it
	err = suites.IOInSuite()->AEGP_GetInSpecOptionsHandle(specH, reinterpret_cast<void**>(&optionsH));

	if (!err && optionsH)
	{
		err = suites.MemorySuite()->AEGP_FreeMemHandle(optionsH);
	}
	return err;
}


A_Err
VRimg_FrameSeq_FlattenOptions(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,
	AEIO_Handle		*flat_optionsPH)
{ 
	// pass a new handle of flattened options for saving
	// but no, don't delete the handle we're using
	
	A_Err				err			=	A_Err_NONE; 
	AEIO_Handle			optionsH	=	NULL;
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);	

	
	// get the options for flattening
	err = suites.IOInSuite()->AEGP_GetInSpecOptionsHandle(specH, reinterpret_cast<void**>(&optionsH));

	if (!err && optionsH)
	{
		AEGP_MemSize mem_size;
		
		void *round_data, *flat_data;
		
		// make a new handle that's the same size
		suites.MemorySuite()->AEGP_GetMemHandleSize(optionsH, &mem_size);
		
		suites.MemorySuite()->AEGP_NewMemHandle( S_mem_id, "Flat Options",
												mem_size,
												AEGP_MemFlag_CLEAR, flat_optionsPH);
		
		suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&round_data);
		suites.MemorySuite()->AEGP_LockMemHandle(*flat_optionsPH, (void**)&flat_data);
		
		// copy data
		memcpy((char*)flat_data, (char*)round_data, mem_size);
		
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
		suites.MemorySuite()->AEGP_UnlockMemHandle(*flat_optionsPH);
		
		// fun our flatten function
		VRimg_FlattenInputOptions(basic_dataP, *flat_optionsPH);
		
		// just because we're flattening the options doesn't mean we're done with them
		//suites.MemorySuite()->AEGP_FreeMemHandle(optionsH);
	}
	
	return err;
}		

A_Err
VRimg_FrameSeq_InflateOptions(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,
	AEIO_Handle		flat_optionsH)
{ 
	// take flat options handle and then set a new inflated options handle
	// AE wants to take care of the flat one for you this time, so no delete
	A_Err				err			=	A_Err_NONE; 
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);	

	
#if PF_AE_PLUG_IN_VERSION < PF_AE100_PLUG_IN_VERSION
	A_char				file_pathZ[AEGP_MAX_PATH_SIZE];
	
	// file path
	suites.IOInSuite()->AEGP_GetInSpecFilePath(specH, file_pathZ);
#else
	AEGP_MemHandle	pathH = NULL;
	A_PathType		*file_pathZ = NULL;
	
	suites.IOInSuite()->AEGP_GetInSpecFilePath(specH, &pathH);
	
	if(pathH)
	{
		suites.MemorySuite()->AEGP_LockMemHandle(pathH, (void **)&file_pathZ);
	}
#endif
	
	if(flat_optionsH)
	{
		AEGP_MemSize	mem_size;
		AEIO_Handle		optionsH	=	NULL,
						old_optionsH =	NULL;
		
		void *round_data, *flat_data;
		
		// make a new handle that's the same size
		suites.MemorySuite()->AEGP_GetMemHandleSize(flat_optionsH, &mem_size);

		suites.MemorySuite()->AEGP_NewMemHandle( S_mem_id, "Round Options",
												mem_size,
												AEGP_MemFlag_CLEAR, &optionsH);
		
		suites.MemorySuite()->AEGP_LockMemHandle(flat_optionsH, (void**)&flat_data);
		suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&round_data);
		
		// copy it
		memcpy((char*)round_data, (char*)flat_data, mem_size);
		
		suites.MemorySuite()->AEGP_UnlockMemHandle(flat_optionsH);
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
		
		// inflate copied data
		if(true) // file_pathZ && file_pathZ[0] != '\0') // turns out we can't rely on this path - sequences will get a bogus path here
		{
			VRimg_InflateInputOptions(basic_dataP, optionsH);
		}

		suites.IOInSuite()->AEGP_SetInSpecOptionsHandle(specH, (void*)optionsH, (void**)&old_optionsH);

		// we'll let AE get rid of the flat options handle for us
		//suites.MemorySuite()->AEGP_FreeMemHandle(flat_optionsH);
	}


#ifdef AE_UNICODE_PATHS
	if(pathH)
		suites.MemorySuite()->AEGP_FreeMemHandle(pathH);
#endif

	return err;
}		

#pragma mark-

A_Err	
VRimg_FrameSeq_GetNumAuxChannels(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,
	A_long			*num_channelsPL)
{ 
	// tell AE how many Aux channels we have (with options help)
	A_Err err						=	A_Err_NONE;

	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);

	AEIO_Handle		optionsH		=	NULL;
	

	// file path
#ifdef AE_UNICODE_PATHS
	AEGP_MemHandle pathH = NULL;
	A_PathType *file_nameZ = NULL;
	
	suites.IOInSuite()->AEGP_GetInSpecFilePath(specH, &pathH);
	
	if(pathH)
	{
		suites.MemorySuite()->AEGP_LockMemHandle(pathH, (void **)&file_nameZ);
	}
	else
		return AEIO_Err_BAD_FILENAME; 
#else
	A_PathType				file_nameZ[AEGP_MAX_PATH_SIZE];
	
	suites.IOInSuite()->AEGP_GetInSpecFilePath(specH, file_nameZ);
#endif

#ifdef AE_HFS_PATHS	
	// convert the path format
	if(A_Err_NONE != ConvertPath(file_nameZ, file_nameZ, AEGP_MAX_PATH_SIZE-1) )
		return AEIO_Err_BAD_FILENAME; 
#endif

	// get options handle
	err = suites.IOInSuite()->AEGP_GetInSpecOptionsHandle(specH, (void**)&optionsH);

	assert(optionsH); // this is a bug in AE otherwise

	if(optionsH)
	{
		// read the number of Auxiliary channels
		err = VRimg_GetNumAuxChannels(basic_dataP, optionsH, file_nameZ, num_channelsPL);
	}
	else
	{
		// gotta do it with fake options
		AEIO_Handle		fake_optionsH	= NULL;
		format_inData	*fake_options	= NULL;
	
		suites.MemorySuite()->AEGP_NewMemHandle( S_mem_id, "Round Options",
															sizeof(format_inData),
															AEGP_MemFlag_CLEAR, &fake_optionsH);
															
		err = suites.MemorySuite()->AEGP_LockMemHandle(fake_optionsH, (void**)&fake_options);
		
		VRimg_InitInOptions((format_inData *)fake_options);
		
		VRimg_GetNumAuxChannels(basic_dataP, fake_optionsH, file_nameZ, num_channelsPL);
		
		suites.MemorySuite()->AEGP_FreeMemHandle(fake_optionsH);
	}
	
#ifdef AE_UNICODE_PATHS
	if(pathH)
		suites.MemorySuite()->AEGP_FreeMemHandle(pathH);
#endif

	// done with options
	if(optionsH)
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	
	return err;
}

									
A_Err	
VRimg_FrameSeq_GetAuxChannelDesc(	
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,
	A_long			chan_indexL,
	PF_ChannelDesc	*descP)
{ 
	// describe one of our AUX channels
	A_Err err						=	A_Err_NONE;

	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);

	AEIO_Handle		optionsH		=	NULL;


	// file path
#ifdef AE_UNICODE_PATHS
	AEGP_MemHandle pathH = NULL;
	A_PathType *file_nameZ = NULL;
	
	suites.IOInSuite()->AEGP_GetInSpecFilePath(specH, &pathH);
	
	if(pathH)
	{
		suites.MemorySuite()->AEGP_LockMemHandle(pathH, (void **)&file_nameZ);
	}
	else
		return AEIO_Err_BAD_FILENAME; 
#else
	A_PathType				file_nameZ[AEGP_MAX_PATH_SIZE];
	
	suites.IOInSuite()->AEGP_GetInSpecFilePath(specH, file_nameZ);
#endif

#ifdef AE_HFS_PATHS	
	// convert the path format
	if(A_Err_NONE != ConvertPath(file_nameZ, file_nameZ, AEGP_MAX_PATH_SIZE-1) )
		return AEIO_Err_BAD_FILENAME; 
#endif

	// get options handle
	err = suites.IOInSuite()->AEGP_GetInSpecOptionsHandle(specH, (void**)&optionsH);

	assert(optionsH); // this is a bug in AE otherwise

	if(optionsH)
	{
		// get the channel description
		err = VRimg_GetAuxChannelDesc(basic_dataP, optionsH, file_nameZ, chan_indexL, descP);
	}
	else
	{
		// gotta do it with fake options
		AEIO_Handle		fake_optionsH	= NULL;
		format_inData	*fake_options	= NULL;
		
		suites.MemorySuite()->AEGP_NewMemHandle( S_mem_id, "Round Options",
															sizeof(format_inData),
															AEGP_MemFlag_CLEAR, &fake_optionsH);
															
		err = suites.MemorySuite()->AEGP_LockMemHandle(fake_optionsH, (void**)&fake_options);
		
		VRimg_InitInOptions((format_inData *)fake_options);
		
		err = VRimg_GetAuxChannelDesc(basic_dataP, fake_optionsH, file_nameZ, chan_indexL, descP);
		
		suites.MemorySuite()->AEGP_FreeMemHandle(fake_optionsH);
	}


#ifdef AE_UNICODE_PATHS
	if(pathH)
		suites.MemorySuite()->AEGP_FreeMemHandle(pathH);
#endif

	// done with options
	if(optionsH)
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
	
	return err;
}

																
A_Err	
VRimg_FrameSeq_DrawAuxChannel(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH			specH,
	A_long					chan_indexL,
	const AEIO_DrawFramePB	*pbP,
	PF_ChannelChunk			*chunkP)
{ 
	// read in the aux channel
	// unfortunately, you'll have to handle scaling yourself
	// I don't know how to scale float worlds for you easily
	
	A_Err err						=	A_Err_NONE;

	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);

	AEIO_Handle		optionsH		=	NULL;
	format_inData	*options			=	NULL;

		
	// file path
#ifdef AE_UNICODE_PATHS
	AEGP_MemHandle pathH = NULL;
	A_PathType *file_nameZ = NULL;
	
	suites.IOInSuite()->AEGP_GetInSpecFilePath(specH, &pathH);
	
	if(pathH)
	{
		suites.MemorySuite()->AEGP_LockMemHandle(pathH, (void **)&file_nameZ);
	}
	else
		return AEIO_Err_BAD_FILENAME; 
#else
	A_PathType				file_nameZ[AEGP_MAX_PATH_SIZE];
	
	suites.IOInSuite()->AEGP_GetInSpecFilePath(specH, file_nameZ);
#endif
	
#ifdef AE_HFS_PATHS	
	// convert the path format
	if(A_Err_NONE != ConvertPath(file_nameZ, file_nameZ, AEGP_MAX_PATH_SIZE-1) )
		return AEIO_Err_BAD_FILENAME; 
#endif

	
	A_long width, height;

	
	// get all that info from AE
	suites.IOInSuite()->AEGP_GetInSpecDimensions(specH, &width, &height);
	
	
	// get options handle
	err = suites.IOInSuite()->AEGP_GetInSpecOptionsHandle(specH, (void**)&optionsH);

	if(optionsH)
		err = suites.MemorySuite()->AEGP_LockMemHandle(optionsH, (void**)&options);
	else
		return err; // we need options

	
	// use our own function to get the ChannelDesc (good thinking!)
	PF_ChannelDesc desc;
	VRimg_FrameSeq_GetAuxChannelDesc(basic_dataP, specH, chan_indexL, &desc);

	
	// crap, we have to scale ourselves (for a channel like object ID, antialiasing makes no sense)
	PF_Point scale;
	scale.h = pbP->rs.x.den / pbP->rs.x.num; // scale.h = 2 means 1/2 x scale
	scale.v = pbP->rs.y.den / pbP->rs.y.num;
	
	
	// setup the Channel Chunk and allocate memory
	chunkP->widthL = ceil( (float)width / (float)scale.h );
	chunkP->heightL = ceil( (float)height / (float)scale.v );
	chunkP->dimensionL = desc.dimension;
	
	chunkP->data_type = desc.data_type;


	// pretty table of the various sizeof()'s
	A_u_char bytes_per_pixel = (	chunkP->data_type == PF_DataType_FLOAT			? 4 :
									chunkP->data_type == PF_DataType_DOUBLE			? 8 :
									chunkP->data_type == PF_DataType_LONG			? 4 :
									chunkP->data_type == PF_DataType_SHORT			? 2 :
									chunkP->data_type == PF_DataType_FIXED_16_16	? 4 :
									chunkP->data_type == PF_DataType_CHAR			? 1 :
									chunkP->data_type == PF_DataType_U_BYTE			? 1 :
									chunkP->data_type == PF_DataType_U_SHORT		? 2 :
									chunkP->data_type == PF_DataType_U_FIXED_16_16	? 4 :
									chunkP->data_type == PF_DataType_RGB			? 3 :
									4); // else?


	chunkP->row_bytesL = chunkP->widthL * chunkP->dimensionL * bytes_per_pixel;
	
	
	// in other words, we'll need
	AEGP_MemSize mem_size = chunkP->row_bytesL * chunkP->heightL;
	
	
	// for PF_Handles we use HandleSuite!
	chunkP->dataH = suites.HandleSuite()->host_new_handle(mem_size);
	
	chunkP->dataPV = suites.HandleSuite()->host_lock_handle(chunkP->dataH);
	

	err = VRimg_DrawAuxChannel(basic_dataP, options, file_nameZ, chan_indexL, pbP, scale, chunkP);

	
	suites.HandleSuite()->host_unlock_handle(chunkP->dataH);
	
#ifdef AE_UNICODE_PATHS
	if(pathH)
		suites.MemorySuite()->AEGP_FreeMemHandle(pathH);
#endif

	// done with options
	if(optionsH)
		suites.MemorySuite()->AEGP_UnlockMemHandle(optionsH);
		
	return err;
}


A_Err	
VRimg_FrameSeq_FreeAuxChannel(	
	AEIO_BasicData		*basic_dataP,
	AEIO_InSpecH		specH,
	PF_ChannelChunk		*chunkP)
{ 
	A_Err err	=	A_Err_NONE;
	
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);

	if(chunkP->dataH)
	{
		// yep, PF_Handles
		suites.HandleSuite()->host_unlock_handle(chunkP->dataH);
		
		suites.HandleSuite()->host_dispose_handle(chunkP->dataH); // run free!
	}
	
	return err; 
}

