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


#include "ProEXR_AE.h"
#include "OpenEXR_PlatformIO.h"
#include "iccProfileAttribute.h"

#include "ProEXRdoc_AE.h"
#include "ProEXR_AE_Dialogs.h"

#include <ImfStandardAttributes.h>

#include <IlmThread.h>
#include <IlmThreadPool.h>
	

#include <assert.h>
#include <time.h>
#include <sys/timeb.h>

#ifdef MAC_ENV
	#include <mach/mach.h>
#endif



using namespace std;
using namespace Imf;
using namespace Imath;
using namespace Iex;


extern AEGP_PluginID S_mem_id;

unsigned int gNumCPUs = 1;


// our prefs
A_Boolean gStorePersonal = FALSE;
A_Boolean gStoreMachine = FALSE;


A_Err
ProEXR_Init(struct SPBasicSuite *pica_basicP)
{
	A_Err err = A_Err_NONE;
	
	// get our (secret) prefs
#define PREFS_SECTION		"ProEXR"
#define PREFS_PERSONAL_INFO	"Store Personal Info"
#define PREFS_MACHINE_INFO	"Store Machine Info"
	
	AEGP_SuiteHandler suites(pica_basicP);
	
	AEGP_PersistentBlobH blobH = NULL;
	suites.PersistentDataSuite()->AEGP_GetApplicationBlob(&blobH);
	
	A_long store_personal = 0;
	A_long store_machine = 0;
	A_long file_description = 1;
	
	suites.PersistentDataSuite()->AEGP_GetLong(blobH, PREFS_SECTION, PREFS_PERSONAL_INFO, store_personal, &store_personal);
	suites.PersistentDataSuite()->AEGP_GetLong(blobH, PREFS_SECTION, PREFS_MACHINE_INFO, store_machine, &store_machine);
	
	gStorePersonal = (store_personal ? TRUE : FALSE);
	gStoreMachine = (store_machine ? TRUE : FALSE);

	if( IlmThread::supportsThreads() )
	{
#ifdef MAC_ENV
		// get number of CPUs using Mach calls
		host_basic_info_data_t hostInfo;
		mach_msg_type_number_t infoCount;
		
		infoCount = HOST_BASIC_INFO_COUNT;
		host_info(mach_host_self(), HOST_BASIC_INFO, 
				  (host_info_t)&hostInfo, &infoCount);
		
		gNumCPUs = hostInfo.max_cpus;
#else // WIN_ENV
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);

		gNumCPUs = systemInfo.dwNumberOfProcessors;
#endif
	}

	return err;
}


A_Err
ProEXR_DeathHook(const SPBasicSuite *pica_basicP)
{
	try {
		if( IlmThread::supportsThreads() )
			setGlobalThreadCount(0);
		
		DeleteFileCache(pica_basicP);
	}catch(...) { return A_Err_PARAMETER; }
	
	return A_Err_NONE;
}


A_Err
ProEXR_ConstructModuleInfo(
	AEIO_ModuleInfo	*info)
{
	// tell AE all about our plug-in

	A_Err err = A_Err_NONE;
	
	if (info)
	{
		info->sig						=	'pEXR';
		info->max_width					=	2147483647;
		info->max_height				=	2147483647;
		info->num_filetypes				=	1;
		info->num_extensions			=	3;
		info->num_clips					=	0;
		
		info->create_kind.type			=	'EXR ';
		info->create_kind.creator		=	'8BIM';

		info->create_ext.pad			=	'.';
		info->create_ext.extension[0]	=	'e';
		info->create_ext.extension[1]	=	'x';
		info->create_ext.extension[2]	=	'r';
		

		strcpy(info->name, PLUGIN_NAME);
		
		info->num_aux_extensionsS		=	0;

		info->flags						=	AEIO_MFlag_OUTPUT			| 
											AEIO_MFlag_FILE				|
											AEIO_MFlag_STILL			| 
											AEIO_MFlag_NO_TIME;

		info->flags2					=	AEIO_MFlag2_SUPPORTS_ICC_PROFILES;
											
		info->read_kinds[0].mac.type			=	'EXR ';
		info->read_kinds[0].mac.creator			=	AEIO_ANY_CREATOR;
		info->read_kinds[1].ext					=	info->create_ext; // .exr
		info->read_kinds[2].ext.pad				=	'.';
		info->read_kinds[2].ext.extension[0]	=	's';
		info->read_kinds[2].ext.extension[1]	=	'x';
		info->read_kinds[2].ext.extension[2]	=	'r';
		info->read_kinds[3].ext.pad				=	'.';
		info->read_kinds[3].ext.extension[0]	=	'm';
		info->read_kinds[3].ext.extension[1]	=	'x';
		info->read_kinds[3].ext.extension[2]	=	'r';
	}
	else
	{
		err = A_Err_STRUCT;
	}
	return err;
}


#pragma mark-


A_Err	
ProEXR_GetDepths(
	AEIO_SupportedDepthFlags		*which)
{
	// these options will be avialable in the depth menu

	*which = AEIO_SupportedDepthFlags_DEPTH_96 | AEIO_SupportedDepthFlags_DEPTH_128;
	
	// Wanted to turn off the no-alpha version, but it caused weird stuff to happen

	return A_Err_NONE;
}


A_Err	
ProEXR_InitOutOptions(
	ProEXR_outData	*options)
{
	// initialize the options when they're first created
	// will probably do this only once per AE user per version
	
	A_Err err						=	A_Err_NONE;
	
	memset(options, 0, sizeof(ProEXR_outData));
	
	options->compression_type = Imf::PIZ_COMPRESSION;
	options->float_not_half = FALSE;
	options->layer_composite = TRUE;
	options->hidden_layers = FALSE;

	return err;
}


static TimeCode
CalculateTimeCode(int frame_num, int frame_rate, bool drop_frame)
{
	// the easiest way to do this is just count!
	int h = 0,
		m = 0,
		s = 0,
		f = 0;
	
	// skip ahead quickly
	int frames_per_ten_mins = (frame_rate * 60 * 10) - (drop_frame ? 9 * (frame_rate == 60 ? 4 : 2) : 0);
	int frames_per_hour = 6 * frames_per_ten_mins;
	
	while(frame_num >= frames_per_hour)
	{
		h++;
		
		frame_num -= frames_per_hour;
	}
	
	while(frame_num >= frames_per_ten_mins)
	{
		m += 10;
		
		frame_num -= frames_per_ten_mins;
	}
	
	// now count out the rest
	int frame = 0;
	
	while(frame++ < frame_num)
	{
		if(f < frame_rate - 1)
		{
			f++;
		}
		else
		{
			f = 0;
			
			if(s < 59)
			{
				s++;
			}
			else
			{
				s = 0;
				
				if(m < 59)
				{
					m++;
					
					if(drop_frame && (m % 10) != 0) // http://en.wikipedia.org/wiki/SMPTE_timecode
					{
						f += (frame_rate == 60 ? 4 : 2);
					}
				}
				else
				{
					m = 0;
					
					h++;
				}
			}
		}
	}
	
	return TimeCode(h, m, s, f, drop_frame);
}


static inline bool isaNumber(char c)
{
	return (c >= '0' && c <= '9');
}

static void GetFrameNumber(const A_PathType *file_pathZ, A_long &frame_num)
{
	// that's right, we're parsing the path for a frame number, if any
	char path[AEGP_MAX_PATH_SIZE+1];
	
	// poor man's unicode copy
	char *p = path;
	
	while(*file_pathZ != '\0')
	{
		*p++ = *file_pathZ++;
	}
	
	*p = '\0';
	
	
	// isolate the string between the last appearing number and the first character before that
	int i = strlen(path) - 1;
	
	bool fixed_number = false;
	
	while(i >= 0 && !fixed_number)
	{
		if( isaNumber(path[i]) && path[i+1] == '.' )
		{
			path[i+1] = '\0';
			fixed_number = true;
		}
		
		i--;
	}
	
	if(fixed_number)
	{
		char *num_string = NULL;
		
		while(i >= 0 && num_string == NULL)
		{
			if( !isaNumber(path[i]) )
			{
				num_string = &path[i + 1];
			}
			else if(i == 0)
			{
				num_string = path;
			}
			
			i--;
		}
		
		int frame = -1;
		
		sscanf(num_string, "%d", &frame);
		
		if(frame >= 0)
			frame_num = frame;
	}
}


static bool possibleDropFrame(A_Time frame_duration)
{
	// AE will tell us if the comp is set to drop frame, even if that parameter
	// is greyed out because it's on a frame rate like 24.  So we'll check if
	// the framerate is one that's possible to be a drop frame.
	double fps = (double)frame_duration.scale / (double)frame_duration.value;
	
	return ( fabs(fps - 29.97) < 0.01 ) || ( fabs(fps - 59.94) < 0.01 );
}


A_Err
ProEXR_OutputFile(
	AEIO_BasicData		*basic_dataP,
	AEIO_OutSpecH		outH,
	const A_PathType	*file_pathZ,
	FrameSeq_Info		*info,
	ProEXR_outData		*options,
	PF_EffectWorld		*wP)
{
	// write da file, mon

	A_Err err						=	A_Err_NONE;
	
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);

	if(options == NULL) // we'll need this
		return AEIO_Err_INCONSISTENT_PARAMETERS;
	

	try{
	
	if( IlmThread::supportsThreads() )
		setGlobalThreadCount(gNumCPUs);
		
		
	
	// set up header
	Header header(	info->width,
					info->height,
					(float)info->pixel_aspect_ratio.num / (float)info->pixel_aspect_ratio.den,
					V2f(0, 0),
					1,
					INCREASING_Y,
					(Compression)options->compression_type);


	// store the actual ratio as a custom attribute
	if(info->pixel_aspect_ratio.num != info->pixel_aspect_ratio.den)
	{
	#define PIXEL_ASPECT_RATIONAL_KEY	"pixelAspectRatioRational"
		header.insert(PIXEL_ASPECT_RATIONAL_KEY,
						RationalAttribute( Rational(info->pixel_aspect_ratio.num, info->pixel_aspect_ratio.den) ) );
	}
	
	// store ICC Profile
	if(info->icc_profile && info->icc_profile_len)
	{
		addICCprofile(header, info->icc_profile, info->icc_profile_len);
	}
	
	// store chromaticities
	if(info->chromaticities)
	{
		// function from ImfStandardAttributes
		addChromaticities(header, Chromaticities(
									V2f(info->chromaticities->red.x, info->chromaticities->red.y),
									V2f(info->chromaticities->green.x, info->chromaticities->green.y),
									V2f(info->chromaticities->blue.x, info->chromaticities->blue.y),
									V2f(info->chromaticities->white.x, info->chromaticities->white.y) ) );

		// store the color space name in the file
	#define COLOR_SPACE_NAME_KEY "chromaticitiesName"	
		if( strlen(info->chromaticities->color_space_name) && strcmp(info->chromaticities->color_space_name, "OpenEXR") != 0 )
			header.insert(COLOR_SPACE_NAME_KEY, StringAttribute( string(info->chromaticities->color_space_name) ) );
	}
	
	
	// extra AE/platform attributes
	if(info->render_info)
	{
		if( strlen(info->render_info->proj_name) )
			header.insert("AfterEffectsProject", StringAttribute( string(info->render_info->proj_name) ) );

		if( strlen(info->render_info->comp_name) )
			header.insert("AfterEffectsComp", StringAttribute( string(info->render_info->comp_name) ) );

		if( strlen(info->render_info->comment_str) )
			addComments(header, string(info->render_info->comment_str) );

		if( gStoreMachine && strlen(info->render_info->computer_name) )
			header.insert("computerName", StringAttribute( string(info->render_info->computer_name) ) );

		if( gStorePersonal && strlen(info->render_info->user_name) )
			addOwner(header, string(info->render_info->user_name) );
		
		if( info->render_info->framerate.value )
			addFramesPerSecond(header, Rational(info->render_info->framerate.value, info->render_info->framerate.scale) );
	}
	
	
	// add time attributes
	time_t the_time = time(NULL);
	tm *local_time = localtime(&the_time);
	
	if(local_time)
	{
		char date_string[256];
		sprintf(date_string, "%04d:%02d:%02d %02d:%02d:%02d", local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
													local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
		
		addCapDate(header, date_string );
#ifdef WIN_ENV
		_timeb win_time;
		_ftime(&win_time);
		addUtcOffset(header, (float)( (win_time.timezone - (win_time.dstflag ? 60 : 0) ) * 60));
#else
		addUtcOffset(header, (float)-local_time->tm_gmtoff);
#endif
	}
	
	
	// graffiti
	header.insert("writer", StringAttribute( "ProEXR Layers Export for After Effects" ) );


	RenderParams params;
	
	// to get any ROI, have to go to the output module
	params.roi.left = params.roi.top = params.roi.right = params.roi.bottom = 0;
	
	// ROI
	AEGP_RQItemRefH rq_itemH = NULL;
	AEGP_OutputModuleRefH outmodH = NULL;
	
	// apparently 0x00 is a valid value for rq_itemH and outmodH
	err = suites.IOOutSuite()->AEGP_GetOutSpecOutputModule(outH, &rq_itemH, &outmodH);
	
	if(!err)
	{
		A_Boolean is_enabled;
		A_Rect roi;
		suites.OutputModuleSuite()->AEGP_GetCropInfo(rq_itemH, outmodH, &is_enabled, &roi);
		
		if(is_enabled)
		{
			params.roi.left		= roi.left;
			params.roi.top		= roi.top;
			params.roi.right	= roi.right;
			params.roi.bottom	= roi.bottom;
		}
	}
	
	
	// Downsampling
	params.yDownsample = params.xDownsample = 1;
	
	AEGP_CompH compH = NULL;
	suites.RQItemSuite()->AEGP_GetCompFromRQItem(rq_itemH, &compH);
	
	if(compH)
	{
		AEGP_ItemH itemH = NULL;
		suites.CompSuite()->AEGP_GetItemFromComp(compH, &itemH);
		
		if(itemH)
		{
			A_long comp_width, comp_height;
			
			suites.ItemSuite()->AEGP_GetItemDimensions(itemH, &comp_width, &comp_height);
			
			params.xDownsample = comp_width / info->width;
			params.yDownsample = comp_height / info->height;
		}
	}
	

	// Time
	A_long frame_num;
	suites.IOOutSuite()->AEGP_GetOutSpecStartFrame(outH, &frame_num);
	
	GetFrameNumber(file_pathZ, frame_num); // yes, I'm really parsing the file path to get the frame number
	
	AEGP_ProjectH projH;
	suites.ProjSuite()->AEGP_GetProjectByIndex(0, &projH);
	
	AEGP_TimeDisplay2 time_dis;
	suites.ProjSuite()->AEGP_GetProjectTimeDisplay(projH, &time_dis);
	//suites.IOOutSuite()->AEGP_GetOutSpecDuration(outH, &params.time_step);
	
	if(compH)
	{
		suites.CompSuite()->AEGP_GetCompFrameDuration(compH, &params.time_step);
	}
	else
	{
		A_Fixed fps;
		suites.IOOutSuite()->AEGP_GetOutSpecFPS(outH, &fps);

		if(fps % 65536 == 0)
		{
			params.time_step.value = 1;
			params.time_step.scale = fps / 65536;
		}
		else
		{
			params.time_step.value = 1001;
			params.time_step.scale = FIX_2_FLOAT(fps) * 1001.0 + 0.5;
		}

	}

	
	if(time_dis.auto_timecode_baseB)
		time_dis.timebaseC = ((double)params.time_step.scale / (double)params.time_step.value) + 0.5;
	
	A_long start_frame = 0;

	if(compH)
	{
		A_Time start_time;
		suites.CompSuite()->AEGP_GetCompDisplayStartTime(compH, &start_time);
		
		A_u_longlong start_num = ((A_u_longlong)start_time.value * (A_u_longlong)params.time_step.scale);
		A_u_longlong start_den = ((A_u_longlong)start_time.scale * (A_u_longlong)params.time_step.value);

		start_frame = start_num / start_den;
	}

	params.time.value = ((frame_num - start_frame) - time_dis.starting_frameL) * params.time_step.value;
	params.time.scale = params.time_step.scale;
	
	
	if(time_dis.time_display_type == AEGP_TimeDisplayType_TIMECODE)
	{
		if(time_dis.timebaseC != 30)
			time_dis.non_drop_30B = TRUE;
	
		addTimeCode(header, CalculateTimeCode(frame_num,
												time_dis.timebaseC,
												!time_dis.non_drop_30B && possibleDropFrame(params.time_step)));
	}
	
	// AE 10.5 (CS5.5) timecode stuff
#ifdef AE105_TIMECODE_SUITES
	AEGP_ProjSuite6 *ps6P = NULL;
	AEGP_CompSuite8 *cs8P = NULL;
	
	try{
		ps6P = suites.ProjSuite6();
		cs8P = suites.CompSuite8();
	}catch(...) {}
	
	if(ps6P && cs8P)
	{
		AEGP_TimeDisplay3 time_dis3;
		ps6P->AEGP_GetProjectTimeDisplay(projH, &time_dis3);
		
		A_long starting_frame = (	time_dis3.frames_display_mode == AEGP_Frames_ZERO_BASED ? 0 :
									time_dis3.frames_display_mode == AEGP_Frames_ONE_BASED ? 1 :
									0 ); // AEGP_Frames_TIMECODE_CONVERSION
		
		int timebase = ((double)params.time_step.scale / (double)params.time_step.value) + 0.5;
		
		A_Boolean drop_frame = true;

		if(compH)
			cs8P->AEGP_GetCompDisplayDropFrame(compH, &drop_frame);
		
		addTimeCode(header, CalculateTimeCode(frame_num - starting_frame,
												timebase,
												drop_frame && possibleDropFrame(params.time_step)));
	}
#endif

	Imf::PixelType pixel_type = (options->float_not_half ? Imf::FLOAT : Imf::HALF);
	
	
	
	// write file
	OStreamPlatform outstream(file_pathZ);
	
	ProEXRdoc_writeAE outputFile(outstream, header, basic_dataP, outH, pixel_type, options->hidden_layers);
	
	if(options->layer_composite)
		outputFile.addMainLayer((PF_PixelFloat *)wP->data, wP->rowbytes, pixel_type);
	
	outputFile.loadFromAE(&params);
	
	outputFile.writeFile();
	
	outputFile.restoreLayers();
	
	}catch(...) { err = AEIO_Err_DISK_FULL; }
	
	return err;
}


A_Err	
ProEXR_WriteOptionsDialog(
	AEIO_BasicData		*basic_dataP,
	ProEXR_outData		*options,
	A_Boolean			*user_interactedPB0)
{
	ProEXR_AE_Out_Data params;
	
	params.compression		= options->compression_type;
	params.float_not_half	= options->float_not_half;
	params.layer_composite	= options->layer_composite;
	params.hidden_layers	= options->hidden_layers;

#ifdef MAC_ENV
	const char *plugHndl = "com.fnordware.AfterEffects.ProEXR_AE";
	const void *hwnd = NULL;
#else
	AEGP_SuiteHandler	suites(basic_dataP->pica_basicP);
	
	// get platform handles
	const char *plugHndl = NULL;
	HWND hwnd = NULL;
	suites.UtilitySuite()->AEGP_GetMainHWND((void *)&hwnd);
#endif

	*user_interactedPB0 = ProEXR_AE_Out(&params, plugHndl, hwnd);
	
	if(*user_interactedPB0)
	{
		options->compression_type	= params.compression;
		options->float_not_half		= params.float_not_half;
		options->layer_composite	= params.layer_composite;
		options->hidden_layers		= params.hidden_layers;
	}


	return A_Err_NONE;
}


A_Err	
ProEXR_GetOutSpecInfo(
	const A_PathType	*file_pathZ,
	ProEXR_outData		*options,
	AEIO_Verbiage		*verbiageP)
{ 
	// describe out output options state in English (or another language if you prefer)
	// only sub-type appears to work (but with carriage returs (\r) )
	A_Err err			=	A_Err_NONE;

	// actually, this shows up in the template
	strcpy(verbiageP->type, PLUGIN_NAME);

	switch(options->compression_type)
	{
		case Imf::NO_COMPRESSION:
			strcpy(verbiageP->sub_type, "No compression");
			break;

		case Imf::RLE_COMPRESSION:
			strcpy(verbiageP->sub_type, "RLE compression");
			break;

		case Imf::ZIPS_COMPRESSION:
			strcpy(verbiageP->sub_type, "Zip compression");
			break;

		case Imf::ZIP_COMPRESSION:
			strcpy(verbiageP->sub_type, "Zip compression\nin blocks of 16 scan lines");
			break;

		case Imf::PIZ_COMPRESSION:
			strcpy(verbiageP->sub_type, "Piz compression");
			break;

		case Imf::PXR24_COMPRESSION:
			strcpy(verbiageP->sub_type, "PXR24 compression");
			break;

		case Imf::B44_COMPRESSION:
			strcpy(verbiageP->sub_type, "B44 compression");
			break;
			
		case Imf::B44A_COMPRESSION:
			strcpy(verbiageP->sub_type, "B44A compression");
			break;

		case Imf::DWAA_COMPRESSION:
			strcpy(verbiageP->sub_type, "DWAA compression");
			break;

		case Imf::DWAB_COMPRESSION:
			strcpy(verbiageP->sub_type, "DWAB compression");
			break;

		default:
			strcpy(verbiageP->sub_type, "unknown compression!");
			break;
	}
	
	
	if(options->hidden_layers)
		strcat(verbiageP->sub_type, "\nInclude Hidden Layers");
		
	if(options->layer_composite)
		strcat(verbiageP->sub_type, "\nInclude Layer Composite");
	else
		strcat(verbiageP->sub_type, "\nNo Layer Composite");
		
	if(options->float_not_half)
		strcat(verbiageP->sub_type, "\n32-bit float");
	
	
	return err;
}


A_Err
ProEXR_FlattenOutputOptions(
	ProEXR_outData	*options)
{
	// no need to flatten or byte-flip or whatever
	
	return A_Err_NONE;
}

A_Err
ProEXR_InflateOutputOptions(
	ProEXR_outData	*options)
{
	// just flip again
	return ProEXR_FlattenOutputOptions(options);
}
