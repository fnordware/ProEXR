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

#include "ProEXR_AE_GUI.h"

#include "ProEXRdoc_AE.h"
#include "ProEXR_AE_Dialogs.h"

#include "OpenEXR_PlatformIO.h"
#include "iccProfileAttribute.h"

#include <ImfStandardAttributes.h>

#include "ProEXR_AE_FrameSeq.h"

#include <iomanip>
//#include <sstream>

#include <assert.h>
#include <time.h>
#include <sys/timeb.h>

//using namespace std;
using namespace Imf;
using namespace Imath;
using namespace Iex;

extern AEGP_PluginID S_mem_id;
extern AEGP_Command gSaveAsFileCmd;

static SPBasicSuite *sP = NULL;


extern A_Boolean gStorePersonal;
extern A_Boolean gStoreMachine;


// to store params between applications
static ProEXR_AE_GUI_Data g_params = { Imf::PIZ_COMPRESSION, false, true, false, TIMESPAN_CURRENT_FRAME, 1, 2 };
static std::string g_path("");


static std::string
GetFilenameFromPath(const std::string &path)
{
#ifdef __APPLE__
	const char sep = '/';
#else
	const char sep = '\\';
#endif

	const size_t last_sep = path.find_last_of(sep);
	
	if(last_sep == std::string::npos)
		return path;
	else
		return path.substr(last_sep + 1);
}

static std::string
MakeTokenString(size_t digits)
{
	std::string token_str = "[";
	
	for(int i = 0; i < digits; i++)
		token_str += "#";
	
	token_str += "]";
	
	return token_str;
}

static size_t
FrameTokenDigits(const std::string &path)
{
	const std::string filename = GetFilenameFromPath(path);
	
	for(int i = 1; i <= 8; i++)
	{
		std::string token_str = MakeTokenString(i);
		
		const size_t token_pos = filename.rfind(token_str);
		
		if(token_pos != std::string::npos)
			return i;
	}
	
	return 0;
}

static std::string
ReplaceToken(const std::string &path, size_t frame_number)
{
	const size_t token_digits = FrameTokenDigits(path);
	
	if(token_digits > 0)
	{
		const std::string token_string = MakeTokenString(token_digits);
		
		std::stringstream frame_str;
		
		frame_str << std::setw(token_digits) << std::setfill('0') << frame_number;
		
		const size_t token_pos = path.rfind(token_string);
		
		if(token_pos != std::string::npos)
		{
			std::string path_to_return = path;
			
			path_to_return.replace(token_pos, token_string.length(), frame_str.str());
			
			return path_to_return;
		}
		else
			return path;
	}
	else
		return path;
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

static bool
possibleDropFrame(A_Time frame_duration)
{
	// AE will tell us if the comp is set to drop frame, even if that parameter
	// is greyed out because it's on a frame rate like 24.  So we'll check if
	// the framerate is one that's possible to be a drop frame.
	double fps = (double)frame_duration.scale / (double)frame_duration.value;
	
	return ( fabs(fps - 29.97) < 0.01 ) || ( fabs(fps - 59.94) < 0.01 );
}

static TimeCode
GetTimeCode(AEGP_SuiteHandler &suites, AEGP_CompH compH, int frame)
{
	TimeCode result;

	AEGP_ProjectH projH;
	suites.ProjSuite()->AEGP_GetProjectByIndex(0, &projH);
	
	AEGP_TimeDisplay2 time_dis;
	suites.ProjSuite()->AEGP_GetProjectTimeDisplay(projH, &time_dis);
	//suites.IOOutSuite()->AEGP_GetOutSpecDuration(outH, &params.time_step);
	
	A_Time frame_duration;
	suites.CompSuite()->AEGP_GetCompFrameDuration(compH, &frame_duration);
	
	if(time_dis.auto_timecode_baseB)
		time_dis.timebaseC = ((double)frame_duration.scale / (double)frame_duration.value) + 0.5;
	
	A_Time start_time;
	suites.CompSuite()->AEGP_GetCompDisplayStartTime(compH, &start_time);
	
	//A_u_longlong start_num = ((A_u_longlong)start_time.value * (A_u_longlong)frame_duration.scale);
	//A_u_longlong start_den = ((A_u_longlong)start_time.scale * (A_u_longlong)frame_duration.value);
	//A_long start_frame = start_num / start_den;

	//A_Time time;
	//time.value = ((frame - start_frame) - time_dis.starting_frameL) * frame_duration.value;
	//time.scale = frame_duration.scale;
	
	
	if(time_dis.time_display_type == AEGP_TimeDisplayType_TIMECODE)
	{
		if(time_dis.timebaseC != 30)
			time_dis.non_drop_30B = TRUE;
	
		result = CalculateTimeCode(frame,
									time_dis.timebaseC,
									!time_dis.non_drop_30B && possibleDropFrame(frame_duration));
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
		
		int timebase = ((double)frame_duration.scale / (double)frame_duration.value) + 0.5;
		
		A_Boolean drop_frame;
		cs8P->AEGP_GetCompDisplayDropFrame(compH, &drop_frame);
		
		result = CalculateTimeCode(frame - starting_frame,
									timebase,
									drop_frame && possibleDropFrame(frame_duration));
	}
#endif

	return result;
}

A_Err
ProEXR_GPCommandHook(
	AEGP_GlobalRefcon	plugin_refconPV,
	AEGP_CommandRefcon	refconPV,
	AEGP_Command		command,
	AEGP_HookPriority	hook_priority,
	A_Boolean			already_handledB,
	A_Boolean			*handledPB)
{
	A_Err			err 		= A_Err_NONE;
	
	if(command == gSaveAsFileCmd)
	{
		if(sP == NULL)
			sP = (SPBasicSuite *)refconPV;
		
		AEGP_SuiteHandler	suites(sP);
		
		try{

		AEGP_ItemH itemH = NULL;
		suites.ItemSuite()->AEGP_GetActiveItem(&itemH);
		
		if(itemH)
		{
			AEGP_ItemType type;
			suites.ItemSuite()->AEGP_GetItemType(itemH, &type);
			
			if(type == AEGP_ItemType_COMP)
			{
			#ifdef MAC_ENV
				const char *plugHndl = "com.fnordware.AfterEffects.ProEXR_AE";
				const void *hwnd = NULL;
			#else
				// get platform handles
				const char *plugHndl = NULL;
				HWND hwnd = NULL;
				suites.UtilitySuite()->AEGP_GetMainHWND((void *)&hwnd);
			#endif
				
				
				A_Time item_duration;
				A_Time current_time;
				suites.ItemSuite()->AEGP_GetItemDuration(itemH, &item_duration);
				suites.ItemSuite()->AEGP_GetItemCurrentTime(itemH, &current_time);
				
				if( g_path.empty() )
				{
				#ifdef AE_UNICODE_PATHS
					AEGP_MemHandle nameH = NULL;
					suites.ItemSuite()->AEGP_GetItemName(S_mem_id, itemH, &nameH);
					
					if(nameH != NULL)
					{
						A_PathType *nameZ = NULL;
						suites.MemorySuite()->AEGP_LockMemHandle(nameH, (void **)&nameZ);
						
						if(nameZ != NULL)
						{
							while(*nameZ != '\0')
							{
								g_path += std::string(1, *nameZ);
							
								nameZ++;
							}
						}
						
						suites.MemorySuite()->AEGP_FreeMemHandle(nameH);
					}
				#else
					A_PathType nameZ[AEGP_MAX_ITEM_NAME_SIZE+1];
					suites.ItemSuite()->AEGP_GetItemName(itemH, nameZ);
					
					g_path = nameZ;
				#endif // AE_UNICODE_PATHS
				
					g_path += ".[####].exr";
				}
				
				AEGP_CompH compH = NULL;
				suites.CompSuite()->AEGP_GetCompFromItem(itemH, &compH);
				
				A_FpLong frame_rate = 0;
				A_Time frame_duration;
				A_Time start_time;
				A_Time work_area_start;
				A_Time work_area_duration;
				
				suites.CompSuite()->AEGP_GetCompFramerate(compH, &frame_rate);
				suites.CompSuite()->AEGP_GetCompFrameDuration(compH, &frame_duration);
				suites.CompSuite()->AEGP_GetCompDisplayStartTime(compH, &start_time);
				suites.CompSuite()->AEGP_GetCompWorkAreaStart(compH, &work_area_start);
				suites.CompSuite()->AEGP_GetCompWorkAreaDuration(compH, &work_area_duration);
				
				AEGP_ProjectH projH = NULL;
				suites.ProjSuite()->AEGP_GetProjectByIndex(0, &projH);
				
				AEGP_TimeDisplay2 time_dis;
				suites.ProjSuite()->AEGP_GetProjectTimeDisplay(projH, &time_dis);
				
			#ifdef AE105_TIMECODE_SUITES
				const bool embed_timcode = true;
			#else
				const bool embed_timcode = (time_dis.time_display_type == AEGP_TimeDisplayType_TIMECODE);
			#endif

				const int project_start = (time_dis.time_display_type != AEGP_TimeDisplayType_TIMECODE ? time_dis.starting_frameL : 0);
				const int comp_start = ((double)start_time.value * (double)frame_duration.scale) / ((double)start_time.scale * (double)frame_duration.value);
				const int comp_duration = ((double)item_duration.value * (double)frame_duration.scale) / ((double)item_duration.scale * (double)frame_duration.value);
				const int comp_end = comp_start + comp_duration - 1;
				const int current_frame = (((double)current_time.value * (double)frame_duration.scale) / ((double)current_time.scale * (double)frame_duration.value)) + comp_start;
				const int work_start = (((double)work_area_start.value * (double)frame_duration.scale) / ((double)work_area_start.scale * (double)frame_duration.value)) + comp_start;
				const int work_duration = ((double)work_area_duration.value * (double)frame_duration.scale) / ((double)work_area_duration.scale * (double)frame_duration.value);
				const int work_end = work_start + work_duration - 1;
				
				bool ok = ProEXR_AE_GUI(&g_params,
										g_path,
										current_frame + project_start,
										work_start + project_start,
										work_end + project_start,
										comp_start + project_start,
										comp_end + project_start,
										plugHndl,
										hwnd);
				
				if(ok)
				{
					suites.UtilitySuite()->AEGP_StartUndoGroup("ProEXR Export"); // we're about to do a bunch of stuff
					
					const std::string &path = g_path;
					const ProEXR_AE_GUI_Data &params = g_params;
					
					int start_frame = current_frame;
					int end_frame = current_frame;
					
					if(params.timeSpan == TIMESPAN_WORK_AREA)
					{
						start_frame = work_start;
						end_frame = work_end;
					}
					else if(params.timeSpan == TIMESPAN_FULL_COMP)
					{
						start_frame = comp_start;
						end_frame = comp_end;
					}
					else if(params.timeSpan == TIMESPAN_CUSTOM)
					{
						start_frame = params.start_frame - project_start;
						end_frame = params.end_frame - project_start;
					}
					
					if(end_frame < start_frame)
					{
						const int temp_f = start_frame;
						
						start_frame = end_frame;
						end_frame = temp_f;
					}
					
					const bool path_has_frame_token = (FrameTokenDigits(path) > 0);
					
					if(!path_has_frame_token)
					{
						end_frame = start_frame;
					}
					
					
					
					A_long width;
					A_long height;
					A_Ratio pixel_aspect_ratio;
					
					suites.ItemSuite()->AEGP_GetItemDimensions(itemH, &width, &height);
					suites.ItemSuite()->AEGP_GetItemPixelAspectRatio(itemH, &pixel_aspect_ratio);
					
					
					void *ae_profile_data = NULL;
					AEGP_MemSize ae_profile_size = 0;
					A_Boolean have_chromaticities = FALSE;
					A_Chromaticities chromaticities;
					
					AEGP_ColorProfileP ae_profile = NULL;
					AEGP_MemHandle ae_profileH = NULL;
					suites.ColorSettingsSuite()->AEGP_GetNewWorkingSpaceColorProfile(S_mem_id, compH, &ae_profile);
					
					if(ae_profile)
					{
						suites.ColorSettingsSuite()->AEGP_GetNewICCProfileFromColorProfile(S_mem_id, ae_profile, &ae_profileH);
						
						if(ae_profileH)
						{
							suites.MemorySuite()->AEGP_LockMemHandle(ae_profileH, &ae_profile_data);
							
							suites.MemorySuite()->AEGP_GetMemHandleSize(ae_profileH, &ae_profile_size);
							
							have_chromaticities = Profile_To_Chromaticities(ae_profile_data, ae_profile_size, &chromaticities);
							
							assert(have_chromaticities == TRUE);
						}
					}
					
				
					// set up header
					Header header_template(	width,
											height,
											(float)pixel_aspect_ratio.num / (float)pixel_aspect_ratio.den,
											V2f(0, 0),
											1,
											INCREASING_Y,
											(Compression)params.compression);


					// store the actual ratio as a custom attribute
					if(pixel_aspect_ratio.num != pixel_aspect_ratio.den)
					{
					#define PIXEL_ASPECT_RATIONAL_KEY	"pixelAspectRatioRational"
						header_template.insert(PIXEL_ASPECT_RATIONAL_KEY,
										RationalAttribute( Rational(pixel_aspect_ratio.num, pixel_aspect_ratio.den) ) );
					}
					
					// store ICC Profile
					if(ae_profile_data && ae_profile_size)
					{
						addICCprofile(header_template, ae_profile_data, ae_profile_size);
					}
					
					// store chromaticities
					if(have_chromaticities)
					{
						// function from ImfStandardAttributes
						addChromaticities(header_template, Chromaticities(
													V2f(chromaticities.red.x, chromaticities.red.y),
													V2f(chromaticities.green.x, chromaticities.green.y),
													V2f(chromaticities.blue.x, chromaticities.blue.y),
													V2f(chromaticities.white.x, chromaticities.white.y) ) );

						// store the color space name in the file
					#define COLOR_SPACE_NAME_KEY "chromaticitiesName"	
						if( strlen(chromaticities.color_space_name) && strcmp(chromaticities.color_space_name, "OpenEXR") != 0 )
							header_template.insert(COLOR_SPACE_NAME_KEY, StringAttribute( string(chromaticities.color_space_name) ) );
					}
					
					const Rational frames_per_second(frame_duration.scale, frame_duration.value);
					
					addFramesPerSecond(header_template, frames_per_second);
					
					// graffiti
					header_template.insert("writer", StringAttribute( "ProEXR Layers Export for After Effects" ) );
					
					
					bool not_canceled = true;
					
					for(int frame = start_frame;
						frame <= end_frame &&
							(not_canceled = ProEXR_AE_Update_Progress(1 + frame - start_frame, 1 + end_frame - start_frame, plugHndl, hwnd));
						frame++)
					{
						const std::string frame_path = ReplaceToken(path, frame + project_start);
						
						Header header = header_template;
						
						if(embed_timcode)
							addTimeCode(header, GetTimeCode(suites, compH, frame + project_start));
						
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
						
						
						RenderParams render_params;
						
						const int frame_in_comp = (frame - comp_start);
						render_params.time.value = (frame_in_comp * frame_duration.value);
						render_params.time.scale = frame_duration.scale;
						
						render_params.time_step.value = frame_duration.value;
						render_params.time_step.scale = frame_duration.scale;
						
						render_params.roi.left = render_params.roi.top = render_params.roi.right = render_params.roi.bottom = 0;
						
						render_params.yDownsample = render_params.xDownsample = 1;
						
						
						const Imf::PixelType pixelType = (params.float_not_half ? Imf::FLOAT : Imf::HALF);
						
						
						OStreamPlatform outstream(frame_path.c_str());
						
						ProEXRdoc_writeAE outputFile(outstream, header, sP, compH, pixelType, params.hidden_layers);
						
						if(params.layer_composite)
							outputFile.addMainLayer(pixelType);
						
						outputFile.loadFromAE(&render_params);
						
						outputFile.writeFile();
						
						outputFile.restoreLayers();
					}
					
					//if(not_canceled)
					//{
					//	suites.UtilitySuite()->AEGP_ReportInfo(S_mem_id, "ProEXR export completed");
					//} // looks like an error
					
					
					if(ae_profileH)
					{
						suites.MemorySuite()->AEGP_FreeMemHandle(ae_profileH);
					}
					
					if(ae_profile)
					{
						suites.ColorSettingsSuite()->AEGP_DisposeColorProfile(ae_profile);
					}
					
					suites.UtilitySuite()->AEGP_EndUndoGroup();
				}
			}
			else
				throw AfterEffectsExc("Comp not the active item");
		}
		else
			throw AfterEffectsExc("No active item");

		// I handled it, right?
		*handledPB = TRUE;
		
		
		}
		catch(std::exception &e)
		{
			suites.UtilitySuite()->AEGP_ReportInfo(S_mem_id, e.what());
		}
		catch(...)
		{
			suites.UtilitySuite()->AEGP_ReportInfo(S_mem_id, "Really weird exception!");
		}
		
		ProEXR_AE_End_Progress();
	}
		
	return err;

}


A_Err
ProEXR_UpdateMenuHook(
	AEGP_GlobalRefcon		plugin_refconPV,
	AEGP_UpdateMenuRefcon	refconPV,
	AEGP_WindowType			active_window)
{
	A_Err 				err 			=	A_Err_NONE;
	
	if(sP == NULL)
		sP = (SPBasicSuite *)refconPV;
	
	AEGP_SuiteHandler	suites(sP);

	// see if we're currently in a comp with a layer
	AEGP_ItemH itemH = NULL;
	suites.ItemSuite()->AEGP_GetActiveItem(&itemH);
	
	if(itemH)
	{
		AEGP_ItemType type;
		suites.ItemSuite()->AEGP_GetItemType(itemH, &type);
		
		if(type == AEGP_ItemType_COMP)
		{
			AEGP_CompH compH = NULL;
			
			suites.CompSuite()->AEGP_GetCompFromItem(itemH, &compH);
			
			assert(compH != NULL);
			
			A_long num_layersL = 0;
			
			suites.LayerSuite()->AEGP_GetCompNumLayers(compH, &num_layersL);
			
			if(num_layersL > 0)
				suites.CommandSuite()->AEGP_EnableCommand(gSaveAsFileCmd);
		}
	}
		
	return err;
}
