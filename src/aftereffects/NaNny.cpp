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

// NaNny
//
// AE plug-in to kill NaN and inf floating point values
//


#include "NaNny.h"

#ifdef MAC_ENV
#include <pthread.h>
#endif

#include <float.h>

static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err err = PF_Err_NONE;

	PF_SPRINTF(	out_data->return_msg, 
				"%s - %s\r\rwritten by Brendan Bolles\r\rv%d.%d - %s\r\r%s\r%s",
				NAME,
				DESCRIPTION, 
				MAJOR_VERSION, 
				MINOR_VERSION,
				RELEASE_DATE,
				COPYRIGHT,
				WEBSITE);


	return err;
}


static PF_Err 
GlobalSetup (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err err = PF_Err_NONE;

	out_data->out_flags		=	PF_OutFlag_USE_OUTPUT_EXTENT |
                                PF_OutFlag_PIX_INDEPENDENT |
								PF_OutFlag_DEEP_COLOR_AWARE;

	out_data->out_flags2 	=	PF_OutFlag2_SUPPORTS_SMART_RENDER |
								PF_OutFlag2_FLOAT_COLOR_AWARE;
                                
								
	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,	
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);

	return err;
}


static PF_Err
ParamsSetup(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	PF_Err 			err = PF_Err_NONE;
	PF_ParamDef		def;


	AEFX_CLR_STRUCT(def);
	PF_ADD_POPUP(	"Mode",
					MODE_NUM_OPTIONS, //number of choices
					MODE_ALL, //default
					MODE_MENU_STRING,
					MODE_ID);

	AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDER( "NaN value",
                    -FLT_MAX, FLT_MAX, 0.0, 1000.0, // ranges
                    0, // curve tolderance?
                    12.0, // default
                    2, 0, 0, // precision, display flags, want phase
                    NAN_ID);

	AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDER( "inf value",
                    -FLT_MAX, FLT_MAX, 0.0, 1000.0, // ranges
                    0, // curve tolderance?
                    123.0, // default
                    2, 0, 0, // precision, display flags, want phase
                    INF_ID);

	AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDER( "-inf value",
                    -FLT_MAX, FLT_MAX, -1000.0, 1000.0, // ranges
                    0, // curve tolderance?
                    0.0, // default
                    2, 0, 0, // precision, display flags, want phase
                    NIN_ID);

	out_data->num_params = NANNY_NUM_PARAMS;

	return err;
}



static
PF_Boolean IsEmptyRect(const PF_LRect *r){
	return (r->left >= r->right) || (r->top >= r->bottom);
}

#ifndef mmin
	#define mmin(a,b) ((a) < (b) ? (a) : (b))
	#define mmax(a,b) ((a) > (b) ? (a) : (b))
#endif

static
void UnionLRect(const PF_LRect *src, PF_LRect *dst)
{
	if (IsEmptyRect(dst)) {
		*dst = *src;
	} else if (!IsEmptyRect(src)) {
		dst->left 	= mmin(dst->left, src->left);
		dst->top  	= mmin(dst->top, src->top);
		dst->right 	= mmax(dst->right, src->right);
		dst->bottom = mmax(dst->bottom, src->bottom);
	}
}

static PF_Err
PreRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_PreRenderExtra		*extra)
{
	PF_Err err = PF_Err_NONE;
	PF_RenderRequest req = extra->input->output_request;
	PF_CheckoutResult in_result;
	
	req.preserve_rgb_of_zero_alpha = TRUE;	//	Hey, we care.

	ERR(extra->cb->checkout_layer(	in_data->effect_ref,
									NANNY_INPUT,
									NANNY_INPUT,
									&req,
									in_data->current_time,
									in_data->time_step,
									in_data->time_scale,
									&in_result));


	UnionLRect(&in_result.result_rect, 		&extra->output->result_rect);
	UnionLRect(&in_result.max_result_rect, 	&extra->output->max_result_rect);	
	
	//	Notice something missing, namely the PF_CHECKIN_PARAM to balance
	//	the old-fashioned PF_CHECKOUT_PARAM, above? 
	
	//	For SmartFX, AE automagically checks in any params checked out 
	//	during PF_Cmd_SMART_PRE_RENDER, new or old-fashioned.
	
	return err;
}

#pragma mark-

#ifdef MAC_ENV
typedef pthread_mutex_t MutualExclusion;

static void InitializeMutex(MutualExclusion *mutex)
{
	pthread_mutex_init(mutex, 0);
}

static void DeleteMutex(MutualExclusion *mutex)
{
	pthread_mutex_destroy(mutex);
}

static void LockMutex(MutualExclusion *mutex)
{
	pthread_mutex_lock(mutex);
}

static void UnlockMutex(MutualExclusion *mutex)
{
	pthread_mutex_unlock(mutex);
}
#else
typedef CRITICAL_SECTION MutualExclusion;

static void InitializeMutex(MutualExclusion *mutex)
{
	InitializeCriticalSection(mutex);
}

static void DeleteMutex(MutualExclusion *mutex)
{
	DeleteCriticalSection(mutex);
}

static void LockMutex(MutualExclusion *mutex)
{
	EnterCriticalSection(mutex);
}

static void UnlockMutex(MutualExclusion *mutex)
{
	LeaveCriticalSection(mutex);
}
#endif

#pragma mark-

inline bool
IsNan(float f)
{
	u_int l = *(u_long*)&f;
	
	return ((l & 0x7f800000) == 0x7f800000) && ((l & 0x007fffff) != 0);	
}

inline bool
IsInfinite(float f)
{
	u_int l = *(u_long*)&f;
	
	return ((l & 0xff800000) == 0x7f800000) && ((l & 0x007fffff) == 0);	
}

inline bool
IsNegInfinite(float f)
{
	u_int l = *(u_long*)&f;
	
	return ((l & 0xff800000) == 0xff800000) && ((l & 0x007fffff) == 0);	
}

#pragma mark-

typedef struct {
	PF_InData			*in_data;
	A_long				width;
    
	int					Mode;
	PF_FpShort			NaN_value;
    PF_FpShort			inf_value;
	PF_FpShort			nin_value;
	
	int					bad_pixels;
	MutualExclusion		mutex;
} IterateData, *IteratePtr, **IterateHndl;



static PF_Err
ProcessRow(
	IteratorRefcon refcon, 
	A_long 		x, 
	A_long 		y, 
	PF_Pixel32 	*in, 
	PF_Pixel32 	*out)
{
	PF_Err err = PF_Err_NONE;

	IteratePtr i_ptr = (IteratePtr)refcon;
	
	if(i_ptr->Mode == MODE_DIAGNOSTIC)
	{
		int bad_pixels = 0;
		
		for(int i=0; i < i_ptr->width; i++)
		{
			if(	IsNan(in->red) ||
				IsNan(in->green) ||
				IsNan(in->blue) ||
				IsInfinite(in->red) ||
				IsInfinite(in->green) ||
				IsInfinite(in->blue) ||
				IsNegInfinite(in->red) ||
				IsNegInfinite(in->green) ||
				IsNegInfinite(in->blue) )
			{
				bad_pixels++;
				
				// white
				out->red	= 1.0f;
				out->green	= 1.0f;
				out->blue	= 1.0f;
				out->alpha	= 1.0f;
			}
			else
			{
				// black
				out->red	= 0.0f;
				out->green	= 0.0f;
				out->blue	= 0.0f;
				out->alpha	= 1.0f;
			}
			
			in++;
			out++;
		}
		
		if(bad_pixels > 0)
		{
			LockMutex(&i_ptr->mutex);
			i_ptr->bad_pixels += bad_pixels;
			UnlockMutex(&i_ptr->mutex);
		}
	}
	else
	{
		bool do_NaN = (i_ptr->Mode == MODE_NAN || i_ptr->Mode == MODE_ALL);
		bool do_inf = (i_ptr->Mode == MODE_INF || i_ptr->Mode == MODE_ALL);
		bool do_nin = (i_ptr->Mode == MODE_NIN || i_ptr->Mode == MODE_ALL);
		
		for(int i=0; i < i_ptr->width; i++)
		{
			*out = *in;
			
			if(false) //(i_ptr->Mode == MODE_MAKE_NAN)
			{
				// This lets you insert NaNs and infs and -infs if you want to make an image with these values.
				// Oddly, AE doesn't show inf in the info palette unless the pixel is all by itself.
				// A field of infs will appear as NaN.
				
				float NaN, inf, nin;
				
				u_int *NaNP = (u_int *)&NaN;
				u_int *infP = (u_int *)&inf;
				u_int *ninP = (u_int *)&nin;
				
				*NaNP = 0x7fC00000;
				*infP = 0x7f800000;
				*ninP = 0xff800000;

				if(out->red == 1.0f && out->green == 0.0f && out->blue == 0.0f)
				{
					out->red = out->green = out->blue = NaN;
				}
				else if(out->red == 0.0f && out->green == 1.0f && out->blue == 0.0f)
				{
					out->red = out->green = out->blue = inf;
				}
				else if(out->red == 0.0f && out->green == 0.0f && out->blue == 1.0f)
				{
					out->red = out->green = out->blue = nin;
				}
			}
			else
			{
				if(do_NaN)
				{
					if( IsNan(out->red) )
						out->red = i_ptr->NaN_value;
					
					if( IsNan(out->green) )
						out->green = i_ptr->NaN_value;
						
					if( IsNan(out->blue) )
						out->blue = i_ptr->NaN_value;
				}
				
				if(do_inf)
				{
					if( IsInfinite(out->red) )
						out->red = i_ptr->inf_value;
					
					if( IsInfinite(out->green) )
						out->green = i_ptr->inf_value;
						
					if( IsInfinite(out->blue) )
						out->blue = i_ptr->inf_value;
				}

				if(do_nin)
				{
					if( IsNegInfinite(out->red) )
						out->red = i_ptr->nin_value;
					
					if( IsNegInfinite(out->green) )
						out->green = i_ptr->nin_value;
						
					if( IsNegInfinite(out->blue) )
						out->blue = i_ptr->nin_value;
				}
			}
			
			in++;
			out++;
		}
	}
		
	
	return err;
}


static PF_Err
DoRender(
		PF_InData		*in_data,
		PF_EffectWorld 	*input,
		PF_ParamDef		*NANNY_Mode,
        PF_ParamDef		*NANNY_NaN_value,
        PF_ParamDef		*NANNY_inf_value,
		PF_ParamDef		*NANNY_nin_value,
		PF_OutData		*out_data,
		PF_EffectWorld	*output)
{
	PF_Err				err 	= PF_Err_NONE,
						err2 	= PF_Err_NONE;
	PF_Point			origin;
	PF_Rect				src_rect, areaR;
	PF_PixelFormat		format	=	PF_PixelFormat_INVALID;
	
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	PF_WorldSuite			*wsP	= suites.PFWorldSuite();
    PF_IterateFloatSuite	*i32sP	= suites.PFIterateFloatSuite();
	PF_WorldTransformSuite	*pfwtsP	= suites.PFWorldTransformSuite();
	PFAdvAppSuite			*advapP = suites.AdvAppSuite();
 
 
    err = wsP->PF_GetPixelFormat(output, &format);
    
    
	if(!err)
    {
		if( format == PF_PixelFormat_ARGB128 ) // only meaningful in float
		{
			bool diagnostic_mode = (NANNY_Mode->u.pd.value == MODE_DIAGNOSTIC);

			PF_Rect  areaR;
			areaR.top = 0;
			areaR.left = 0;
			areaR.right = 1;
			areaR.bottom = output->height;

			IterateData i_data = {in_data, output->width,
									NANNY_Mode->u.pd.value,
									NANNY_NaN_value->u.fs_d.value,
									NANNY_inf_value->u.fs_d.value,
									NANNY_nin_value->u.fs_d.value,
									0};
									
									
			if(diagnostic_mode)
				InitializeMutex(&i_data.mutex);
			
			
			err2 = i32sP->iterate(in_data,
									0, output->height,
									input,
									&areaR,
									(IteratorRefcon)(&i_data),
									ProcessRow,
									output);
									
									
			if(diagnostic_mode)
				DeleteMutex(&i_data.mutex);
			
			
			if(!err2 && diagnostic_mode)
			{
				char message[128];
				
				PF_SPRINTF(message, "NaNny found %d bad pixels.", i_data.bad_pixels);
				
				advapP->PF_InfoDrawText(message, NULL);
			}
		}
		else
		{
			err = pfwtsP->copy(in_data->effect_ref, input, output, NULL, NULL);
			
			advapP->PF_InfoDrawText("NaNny only works in float.", NULL);
			
			// Q: Isn't there a flag I can put up to tell AE I didn't do anything?
			// A: Nope.  Used to be PF_OutFlag_NOP_RENDER in PF_OutFlags in PF_Cmd_FRAME_SETUP, but no longer used.
		}
	}


	// error pecking order
	if(err2)
		err = err2;
		

	return err;
}

static PF_Err
SmartRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_SmartRenderExtra		*extra)

{

	PF_Err			err		= PF_Err_NONE,
					err2 	= PF_Err_NONE;
					
	PF_EffectWorld *input, *output;
	
	// checkout input & output buffers.
	ERR(	extra->cb->checkout_layer_pixels( in_data->effect_ref, NANNY_INPUT, &input)	);
	ERR(	extra->cb->checkout_output(	in_data->effect_ref, &output)	);


	// bail before param checkout
	if(err)
		return err;


	PF_ParamDef NANNY_Mode, NANNY_NaN_value, NANNY_inf_value, NANNY_nin_value;
				
	// zero-out parameters
	AEFX_CLR_STRUCT(NANNY_Mode);
	AEFX_CLR_STRUCT(NANNY_NaN_value);
	AEFX_CLR_STRUCT(NANNY_inf_value);
	AEFX_CLR_STRUCT(NANNY_nin_value);
	
    
#define PF_CHECKOUT_PARAM_NOW( PARAM, DEST )	PF_CHECKOUT_PARAM(	in_data, (PARAM), in_data->current_time, in_data->time_step, in_data->time_scale, DEST )

	// checkout the required params
	ERR(	PF_CHECKOUT_PARAM_NOW( NANNY_MODE, &NANNY_Mode )	);
	ERR(	PF_CHECKOUT_PARAM_NOW( NANNY_NAN_VALUE, &NANNY_NaN_value )	);
    ERR(	PF_CHECKOUT_PARAM_NOW( NANNY_INF_VALUE, &NANNY_inf_value )	);
	ERR(	PF_CHECKOUT_PARAM_NOW( NANNY_NIN_VALUE, &NANNY_nin_value )	);

	ERR(	DoRender(	in_data, 
						input,
						&NANNY_Mode, 
						&NANNY_NaN_value,
                        &NANNY_inf_value,
						&NANNY_nin_value,
						out_data, 
						output) );

	// Always check in, no matter what the error condition!
	ERR2(	PF_CHECKIN_PARAM(in_data, &NANNY_Mode )	);
	ERR2(	PF_CHECKIN_PARAM(in_data, &NANNY_NaN_value )	);
    ERR2(	PF_CHECKIN_PARAM(in_data, &NANNY_inf_value )	);
	ERR2(	PF_CHECKIN_PARAM(in_data, &NANNY_nin_value )	);


	return err;
}


static PF_Err	
DumbRender(	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	PF_Err				err = PF_Err_NONE,
                        err2 = PF_Err_NONE;
    
    
    err = DoRender(in_data,
                    &params[NANNY_INPUT]->u.ld,
                    params[NANNY_MODE],
                    params[NANNY_NAN_VALUE],
                    params[NANNY_INF_VALUE],
					params[NANNY_NIN_VALUE],
                    out_data,
                    output);

    
    return err;
}



DllExport PF_Err 
PluginMain (	
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void			*extra )
{
		
	PF_Err		err = PF_Err_NONE;

	try	{
		switch (cmd) {
		
		case PF_Cmd_ABOUT:
			err = About(in_data, out_data, params, output);
			break;
		case PF_Cmd_GLOBAL_SETUP:
			err = GlobalSetup(in_data, out_data, params, output);
			break;
		case PF_Cmd_PARAMS_SETUP:
			err = ParamsSetup(in_data, out_data, params, output);
			break;
		case PF_Cmd_RENDER:
			err = DumbRender(in_data, out_data, params, output);
			break;
		case PF_Cmd_SMART_PRE_RENDER:
			err = PreRender(in_data, out_data, (PF_PreRenderExtra*)extra);
			break;
		case PF_Cmd_SMART_RENDER:
			err = SmartRender(in_data, out_data, (PF_SmartRenderExtra*)extra);
			break;
		default:
			break;
		}
	}
	catch(PF_Err &thrown_err) { err = thrown_err; }
	catch(...) { err = PF_Err_INTERNAL_STRUCT_DAMAGED; }

	return err;
}


