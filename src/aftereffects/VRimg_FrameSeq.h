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

#ifndef VRIMG_FRAMESEQ_H
#define VRIMG_FRAMESEQ_H

#include "AEConfig.h"
#include "AE_IO.h"
#include "AE_Macros.h"
#include "fnord_SuiteHandler.h"

#ifndef MAX
#define MAX(A,B)			( (A) > (B) ? (A) : (B))
#endif
#define AE_RANGE(NUM)		(A_u_short)MIN( MAX( (NUM), 0 ), PF_MAX_CHAN16 )
#define AE8_RANGE(NUM)		(A_u_char)MIN( MAX( (NUM), 0 ), PF_MAX_CHAN8 )
#define SIXTEEN_RANGE(NUM)	(A_u_short)MIN( MAX( (NUM), 0 ), 0xFFFF )

#define AE_TO_FLOAT(NUM)		( (float)(NUM) / (float)PF_MAX_CHAN16 )
#define FLOAT_TO_AE(NUM)		AE_RANGE( ( (NUM) * (float)PF_MAX_CHAN16 ) + 0.5)

#define AE8_TO_FLOAT(NUM)		( (float)(NUM) / (float)PF_MAX_CHAN8 )
#define FLOAT_TO_AE8(NUM)		AE8_RANGE( ( (NUM) * (float)PF_MAX_CHAN8 ) + 0.5)

#define SIXTEEN_TO_FLOAT(NUM)		( (float)(NUM) / (float)0xFFFF )
#define FLOAT_TO_SIXTEEN(NUM)		SIXTEEN_RANGE( ( (NUM) * (float)0xFFFF ) + 0.5)

#ifndef CONVERT16TO8
#define CONVERT16TO8(A)		( (((long)(A) * PF_MAX_CHAN8) + PF_HALF_CHAN16) / PF_MAX_CHAN16)
#endif

// addition to AEIO_AlphaType
#define AEIO_Alpha_UNKNOWN	99

// addition to FIEL_Type
#define FIEL_Type_UNKNOWN	88


/*
#define CHROMATICITIES_MAX_NAME_LEN	255
typedef struct {
	A_char			color_space_name[CHROMATICITIES_MAX_NAME_LEN + 1];
	A_FloatPoint	red;
	A_FloatPoint	green;
	A_FloatPoint	blue;
	A_FloatPoint	white;
} A_Chromaticities;
*/


typedef struct
{
	A_long			width;
	A_long			height;
	A_long			planes;
	A_long			depth;		// 8 or 16 or 32!
	AEIO_AlphaType	alpha_type;
	A_Ratio			pixel_aspect_ratio;
	PF_Pixel		*premult_color;
	FIEL_Label		*field_label;	// if so inclined
	void			*icc_profile;
	size_t			icc_profile_len;
	//A_Chromaticities *chromaticities;
	//Render_Info		*render_info;
} FrameSeq_Info;




A_Err
VRimg_FrameSeq_Init(struct SPBasicSuite *pica_basicP);

A_Err
VRimg_FrameSeq_DeathHook(const SPBasicSuite *pica_basicP);

A_Err
VRimg_FrameSeq_IdleHook(const SPBasicSuite *pica_basicP, A_long *max_sleepPL);

A_Err
VRimg_FrameSeq_PurgeHook(const SPBasicSuite *pica_basicP);

A_Err
VRimg_FrameSeq_ConstructModuleInfo(
	AEIO_ModuleInfo	*info);

A_Err	
VRimg_FrameSeq_GetInSpecInfo(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH, 
	AEIO_Verbiage	*verbiageP);

A_Err	
VRimg_FrameSeq_VerifyFileImportable(
	AEIO_BasicData			*basic_dataP,
	AEIO_ModuleSignature	sig, 
	const A_PathType		*file_pathZ,
	A_Boolean				*importablePB);

A_Err	
VRimg_FrameSeq_InitInSpecFromFile(
	AEIO_BasicData	*basic_dataP,
	const A_PathType		*file_pathZ,
	AEIO_InSpecH	specH);

A_Err	
VRimg_FrameSeq_DrawSparseFrame(
	AEIO_BasicData					*basic_dataP,
	AEIO_InSpecH					specH, 
	const AEIO_DrawSparseFramePB	*sparse_framePPB, 
	PF_EffectWorld					*wP,
	AEIO_DrawingFlags				*draw_flagsP);
	
A_Err	
VRimg_FrameSeq_SeqOptionsDlg(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,  
	A_Boolean		*user_interactedPB0);

A_Err
VRimg_FrameSeq_DisposeInSpec(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH);

A_Err
VRimg_FrameSeq_FlattenOptions(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,
	AEIO_Handle		*flat_optionsPH);

A_Err
VRimg_FrameSeq_InflateOptions(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,
	AEIO_Handle		flat_optionsH);


A_Err	
VRimg_FrameSeq_GetNumAuxChannels(
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,
	A_long			*num_channelsPL);
									
A_Err	
VRimg_FrameSeq_GetAuxChannelDesc(	
	AEIO_BasicData	*basic_dataP,
	AEIO_InSpecH	specH,
	A_long			chan_indexL,
	PF_ChannelDesc	*descP);
																
A_Err	
VRimg_FrameSeq_DrawAuxChannel(
	AEIO_BasicData			*basic_dataP,
	AEIO_InSpecH			specH,
	A_long					chan_indexL,
	const AEIO_DrawFramePB	*pbP,
	PF_ChannelChunk			*chunkP);

A_Err	
VRimg_FrameSeq_FreeAuxChannel(	
	AEIO_BasicData			*basic_dataP,
	AEIO_InSpecH			specH,
	PF_ChannelChunk			*chunkP);

#endif // VRIMG_FRAMESEQ_H
