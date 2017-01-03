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

// exrdisplay
//
// ILM display function
//


#include "exrdisplay.h"

#include <math.h>

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
    PF_ADD_FLOAT_SLIDER(	"Gamma",
                    0.0, 10.0, 0.0, 10.0, // ranges
                    0, // curve tolderance?
                    2.2, // default
                    2, 0, 0, // precision, display flags, want phase
                    GAMMA_ID);

	AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDER(	"Exposure",
                    -10.0, 10.0, -10.0, 10.0, // ranges
                    0, // curve tolderance?
                    0, // default
                    2, 0, 0, // precision, display flags, want phase
                    EXPOSURE_ID);

	AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDER(	"Defog",
                    0.0, 1.0, 0.0, 0.01, // ranges
                    0, // curve tolderance?
                    0, // default
                    3, 0, 0, // precision, display flags, want phase
                    DEFOG_ID);

	AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDER(	"Knee Low",
                    -10.0, 10.0, -3.0, 3.0, // ranges
                    0, // curve tolderance?
                    0, // default
                    2, 0, 0, // precision, display flags, want phase
                    KNEELOW_ID);

	AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDER(	"Knee High",
                    0.0, 10.0, 3.5, 7.5, // ranges
                    0, // curve tolderance?
                    5.0, // default
                    2, 0, 0, // precision, display flags, want phase
                    KNEEHIGH_ID);
	
	AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("", "Inverse",
                    FALSE, 0,
                    INVERSE_ID);

	AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("", "Dither",
                    FALSE, 0,
                    DITHER_ID);
					

	out_data->num_params = EXRDISPLAY_NUM_PARAMS;

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
									EXRDISPLAY_INPUT,
									EXRDISPLAY_INPUT,
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

typedef struct {
	A_u_long				alpha, red, green, blue;
} PixelLong;


template <typename PIXTYPE>
struct PixelTraits;

template <>
struct PixelTraits<PF_Pixel> {
    enum { MinChan = 0 };
    enum { MaxChan = PF_MAX_CHAN8 };
    typedef A_u_long            UnboundType;
    typedef PixelLong           UnboundPixel;
    static const PF_FpShort     Rounder;
    static const bool           Clamp = true;
};
const PF_FpShort PixelTraits<PF_Pixel>::Rounder = (PF_FpShort)0.5f;

template <>
struct PixelTraits<PF_Pixel16> {
    enum { MinChan = 0 };
    enum { MaxChan = PF_MAX_CHAN16 };
    typedef A_u_long            UnboundType;
    typedef PixelLong           UnboundPixel;
    static const PF_FpShort     Rounder;
    static const bool           Clamp = true;
};
const PF_FpShort PixelTraits<PF_Pixel16>::Rounder = (PF_FpShort)0.5f;

template <>
struct PixelTraits<PF_Pixel32> {
    enum { MinChan = 0 };
    enum { MaxChan = 1 };
    typedef PF_FpShort          UnboundType;
    typedef PF_PixelFloat       UnboundPixel;
    static const PF_FpShort     Rounder;
    static const bool           Clamp = false;
};
const PF_FpShort PixelTraits<PF_Pixel32>::Rounder = (PF_FpShort)0.0f;


// the below ripped right from ImageView.cpp - part of exrdisplay
static float
knee (double x, double f)
{
    return (float) (log (x * f + 1) / f);
}

static float
inv_knee (double x, double f)
{
    return (float) (exp (x * f) - 1) / f;
}


static float
findKneeF (float x, float y)
{
    float f0 = 0;
    float f1 = 1;

    while (knee (x, f1) > y)
    {
	f0 = f1;
	f1 = f1 * 2;
    }

    for (int i = 0; i < 30; ++i)
    {
	float f2 = (f0 + f1) / 2;
	float y2 = knee (x, f2);

	if (y2 < y)
	    f1 = f2;
	else
	    f0 = f2;
    }

    return (f0 + f1) / 2;
}


template<typename PIXTYPE>
static void
computeFogColor(PF_EffectWorld *input, PF_PixelFloat *color)
{
    typename PixelTraits<PIXTYPE>::UnboundPixel pixel_sum = {0, 0, 0, 0};
    
    unsigned char *row = (unsigned char *)input->data;
    
    for(int y=0; y < input->height; y++)
    {
        PIXTYPE *pix = (PIXTYPE *)row;
        
        for(int x=0; x < input->width; x++)
        {
            pixel_sum.red += pix->red;
            pixel_sum.green += pix->green;
            pixel_sum.blue += pix->blue;
            
            pix++;
        }
        
        row += input->rowbytes;
    }
    
    color->red =   ( (float)pixel_sum.red   / PixelTraits<PIXTYPE>::MaxChan ) / (input->width * input->height);
    color->green = ( (float)pixel_sum.green / PixelTraits<PIXTYPE>::MaxChan ) / (input->width * input->height);
    color->blue =  ( (float)pixel_sum.blue  / PixelTraits<PIXTYPE>::MaxChan ) / (input->width * input->height);
}


struct Gamma
{
    float g, m, d, kl, f, s, i;

    Gamma (float gamma,
	   float exposure,
	   float defog,
	   float kneeLow,
	   float kneeHigh,
       float maxChan,
       bool inverse);

    float operator () (float h);
};


Gamma::Gamma
    (float gamma,
     float exposure,
     float defog,
     float kneeLow,
     float kneeHigh,
     float maxChan,
     bool inverse)
:
    g (gamma),

    m (pow (2.0, exposure + 2.47393)),

    d (defog),

    kl (pow (2.0f, kneeLow)),

    f (findKneeF (pow (2.0f, kneeHigh) - kl, 

		  pow (2.0, 3.5) - kl)),

    s (maxChan * pow (2.0, -3.5 * g)),

    i (inverse)
{}


float
Gamma::operator () (float h)
{
    if(i)
    {
        // this is the inverse
        float x = MAX(h, 0.f) / s;
        
        x = pow (x, 1.f / g);
        
        if (x > kl)
        x = kl + inv_knee (x - kl, f);
        
        x /= m;
        
        x += d;
        
        return x;
    }
    else
    {
        //
        // Defog
        //

        float x = MAX(0.f, (h - d));

        //
        // Exposure
        //

        x *= m;

        //
        // Knee
        //

        if (x > kl)
        x = kl + knee (x - kl, f);

        //
        // Gamma
        //

        x = pow (x, g);

        //
        // Scale and NO clamp
        //

        return MAX(x * s, 0.f);
    }
}


static float
dither (float v, int x, int y)
{
    static const float d[4][4] =
    {
	 0.f / (16 * 255),  8.f / (16 * 255),  2.f / (16 * 255), 10.f / (16 * 255),
	12.f / (16 * 255),  4.f / (16 * 255), 14.f / (16 * 255),  6.f / (16 * 255),
	 3.f / (16 * 255), 11.f / (16 * 255),  1.f / (16 * 255),  9.f / (16 * 255),
	15.f / (16 * 255),  7.f / (16 * 255), 13.f / (16 * 255),  5.f / (16 * 255),
    };

    return (v + d[y & 3][x & 3]);
}

#pragma mark-

typedef struct {
	PF_InData			*in_data;
	A_long				width;
    
	PF_FpShort			gamma;
	PF_FpShort			exposure;
    PF_FpShort			defog;
    PF_FpShort			knee_low;
    PF_FpShort			knee_high;
    PF_Boolean          inverse;
    PF_Boolean          dither;
    
    PF_Pixel32          fog_color;
} IterateData, *IteratePtr, **IterateHndl;



template<typename PIXTYPE>
static PF_Err
ProcessRow(
	IteratorRefcon refcon, 
	A_long 		x, 
	A_long 		y, 
	PIXTYPE 	*in, 
	PIXTYPE 	*out)
{
	PF_Err err = PF_Err_NONE;

	IteratePtr i_ptr = (IteratePtr)refcon;
	

	#define PIXTYPE_TO_FLOAT(IN) ( i_ptr->inverse ? (float)(IN) : ( (float)(IN) / (float)PixelTraits<PIXTYPE>::MaxChan) )
    #define SCALED_FLOAT_TO_PIXTYPE(IN) ( (PixelTraits<PIXTYPE>::Clamp ? MIN( (IN), (float)PixelTraits<PIXTYPE>::MaxChan) : (IN) ) * (i_ptr->inverse ? (float)PixelTraits<PIXTYPE>::MaxChan : 1.f) )
	
    
    Gamma rGamma(1.f / i_ptr->gamma, i_ptr->exposure, i_ptr->defog * i_ptr->fog_color.red, i_ptr->knee_low, i_ptr->knee_high, PixelTraits<PIXTYPE>::MaxChan, i_ptr->inverse);
    Gamma gGamma(1.f / i_ptr->gamma, i_ptr->exposure, i_ptr->defog * i_ptr->fog_color.green, i_ptr->knee_low, i_ptr->knee_high, PixelTraits<PIXTYPE>::MaxChan, i_ptr->inverse);
    Gamma bGamma(1.f / i_ptr->gamma, i_ptr->exposure, i_ptr->defog * i_ptr->fog_color.blue, i_ptr->knee_low, i_ptr->knee_high, PixelTraits<PIXTYPE>::MaxChan, i_ptr->inverse);
	
    
	for(int i=0; i < i_ptr->width; i++)
	{
        // convert to float
        PF_Pixel32 pix = {  PIXTYPE_TO_FLOAT(in->alpha),
                            PIXTYPE_TO_FLOAT(in->red),
                            PIXTYPE_TO_FLOAT(in->green),
                            PIXTYPE_TO_FLOAT(in->blue) };
        
        
        // ILM gamma func
        pix.red   = rGamma(pix.red);
        pix.green = gGamma(pix.green);
        pix.blue  = bGamma(pix.blue);
        
        
        // dither
        if(i_ptr->dither)
        {
            pix.red   = dither(pix.red, x + i, y);
            pix.green = dither(pix.green, x + i, y);
            pix.blue  = dither(pix.blue, x + i, y);
        }
        
        
        // back to PIXTYPE
        out->alpha = in->alpha;
        out->red   = SCALED_FLOAT_TO_PIXTYPE(pix.red);
        out->green = SCALED_FLOAT_TO_PIXTYPE(pix.green);
        out->blue  = SCALED_FLOAT_TO_PIXTYPE(pix.blue);		
	
	
		in++;
		out++;
	}
		
	
	return err;
}


static PF_Err
DoRender(
		PF_InData		*in_data,
		PF_EffectWorld 	*input,
		PF_ParamDef		*EXRDISPLAY_gamma,
        PF_ParamDef		*EXRDISPLAY_exposure,
        PF_ParamDef		*EXRDISPLAY_defog,
        PF_ParamDef		*EXRDISPLAY_kneelow,
        PF_ParamDef		*EXRDISPLAY_kneehigh,
        PF_ParamDef		*EXRDISPLAY_inverse,
        PF_ParamDef		*EXRDISPLAY_dither,
		PF_OutData		*out_data,
		PF_EffectWorld	*output)
{
	PF_Err				err 	= PF_Err_NONE,
						err2 	= PF_Err_NONE;
	PF_Point			origin;
	PF_Rect				src_rect, areaR;
	PF_PixelFormat		format	=	PF_PixelFormat_INVALID;
	
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	PF_WorldSuite		*wsP	=	suites.PFWorldSuite();
	PF_Iterate8Suite	*i8sP	=	suites.PFIterate8Suite();
	PF_Iterate16Suite	*i16sP	=	suites.PFIterate16Suite();
    PF_IterateFloatSuite *i32sP =	suites.PFIterateFloatSuite();

 
    ERR( wsP->PF_GetPixelFormat(output, &format) );
    
    
	if (!err)
    {
        IterateData i_data = {in_data, output->width,
                                EXRDISPLAY_gamma->u.fs_d.value,
                                EXRDISPLAY_exposure->u.fs_d.value,
                                EXRDISPLAY_defog->u.fs_d.value,
                                EXRDISPLAY_kneelow->u.fs_d.value,
                                EXRDISPLAY_kneehigh->u.fs_d.value,
                                EXRDISPLAY_inverse->u.bd.value,
                                EXRDISPLAY_dither->u.bd.value,
                                {0.f, 0.f, 0.f, 0.f} };
        
        PF_Rect  areaR;
		areaR.top = 0;
		areaR.left = 0;
		areaR.right = 1;
		areaR.bottom = output->height;
        
        // iterate
        if( format == PF_PixelFormat_ARGB32 )
        {
            if(EXRDISPLAY_defog->u.fs_d.value > 0.f)
                computeFogColor<PF_Pixel>(input, &i_data.fog_color);
            
            err2 = i8sP->iterate(in_data, 0, output->height, input, &areaR, (IteratorRefcon)(&i_data), ProcessRow<PF_Pixel>, output);
        }
        else if( format == PF_PixelFormat_ARGB64 )
        {
            if(EXRDISPLAY_defog->u.fs_d.value > 0.f)
                computeFogColor<PF_Pixel16>(input, &i_data.fog_color);
            
            err2 = i16sP->iterate(in_data, 0, output->height, input, &areaR, (IteratorRefcon)(&i_data), ProcessRow<PF_Pixel16>, output);
        }
        else if( format == PF_PixelFormat_ARGB128 )
        {
            if(EXRDISPLAY_defog->u.fs_d.value > 0.f)
                computeFogColor<PF_Pixel32>(input, &i_data.fog_color);
            
            err2 = i32sP->iterate(in_data, 0, output->height, input, &areaR, (IteratorRefcon)(&i_data), ProcessRow<PF_Pixel32>, output);
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
	ERR(	extra->cb->checkout_layer_pixels( in_data->effect_ref, EXRDISPLAY_INPUT, &input)	);
	ERR(	extra->cb->checkout_output(	in_data->effect_ref, &output)	);


	// bail before param checkout
	if(err)
		return err;


	PF_ParamDef EXRDISPLAY_gamma, EXRDISPLAY_exposure, EXRDISPLAY_defog,
                EXRDISPLAY_kneelow, EXRDISPLAY_kneehigh, EXRDISPLAY_inverse, EXRDISPLAY_dither;
				
	// zero-out parameters
	AEFX_CLR_STRUCT(EXRDISPLAY_gamma);
	AEFX_CLR_STRUCT(EXRDISPLAY_exposure);
	AEFX_CLR_STRUCT(EXRDISPLAY_defog);
	AEFX_CLR_STRUCT(EXRDISPLAY_kneelow);
	AEFX_CLR_STRUCT(EXRDISPLAY_kneehigh);
	AEFX_CLR_STRUCT(EXRDISPLAY_inverse);
	AEFX_CLR_STRUCT(EXRDISPLAY_dither);
	
    
#define PF_CHECKOUT_PARAM_NOW( PARAM, DEST )	PF_CHECKOUT_PARAM(	in_data, (PARAM), in_data->current_time, in_data->time_step, in_data->time_scale, DEST )

	// checkout the required params
	ERR(	PF_CHECKOUT_PARAM_NOW( EXRDISPLAY_GAMMA, &EXRDISPLAY_gamma )	);
	ERR(	PF_CHECKOUT_PARAM_NOW( EXRDISPLAY_EXPOSURE, &EXRDISPLAY_exposure )	);
    ERR(	PF_CHECKOUT_PARAM_NOW( EXRDISPLAY_DEFOG, &EXRDISPLAY_defog )	);
    ERR(	PF_CHECKOUT_PARAM_NOW( EXRDISPLAY_KNEELOW, &EXRDISPLAY_kneelow )	);
    ERR(	PF_CHECKOUT_PARAM_NOW( EXRDISPLAY_KNEEHIGH, &EXRDISPLAY_kneehigh )	);
    ERR(	PF_CHECKOUT_PARAM_NOW( EXRDISPLAY_INVERSE, &EXRDISPLAY_inverse )	);
    ERR(	PF_CHECKOUT_PARAM_NOW( EXRDISPLAY_DITHER, &EXRDISPLAY_dither )	);

	ERR(	DoRender(	in_data, 
						input,
						&EXRDISPLAY_gamma, 
						&EXRDISPLAY_exposure,
                        &EXRDISPLAY_defog,
                        &EXRDISPLAY_kneelow,
                        &EXRDISPLAY_kneehigh,
                        &EXRDISPLAY_inverse,
                        &EXRDISPLAY_dither,
						out_data, 
						output) );

	// Always check in, no matter what the error condition!
	ERR2(	PF_CHECKIN_PARAM(in_data, &EXRDISPLAY_gamma )	);
	ERR2(	PF_CHECKIN_PARAM(in_data, &EXRDISPLAY_exposure )	);
    ERR2(	PF_CHECKIN_PARAM(in_data, &EXRDISPLAY_defog )	);
    ERR2(	PF_CHECKIN_PARAM(in_data, &EXRDISPLAY_kneelow )	);
    ERR2(	PF_CHECKIN_PARAM(in_data, &EXRDISPLAY_kneehigh )	);
    ERR2(	PF_CHECKIN_PARAM(in_data, &EXRDISPLAY_inverse )	);
    ERR2(	PF_CHECKIN_PARAM(in_data, &EXRDISPLAY_dither )	);


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
                    &params[EXRDISPLAY_INPUT]->u.ld,
                    params[EXRDISPLAY_GAMMA],
                    params[EXRDISPLAY_EXPOSURE],
                    params[EXRDISPLAY_DEFOG],
                    params[EXRDISPLAY_KNEELOW],
                    params[EXRDISPLAY_KNEEHIGH],
                    params[EXRDISPLAY_INVERSE],
                    params[EXRDISPLAY_DITHER],
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


