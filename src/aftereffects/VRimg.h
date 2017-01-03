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


#ifndef VRIMG_H
#define VRIMG_H

#include "VRimg_FrameSeq.h"


#define PLUGIN_NAME		"VRimg"




typedef struct VRimg_inData
{
	A_u_long			real_channels; // not compound
	A_u_char			reserved[28]; // saving 32 bytes
	A_u_long			channels;
	PF_ChannelDesc		channel[1];
} VRimg_inData;



A_Err
VRimg_Init(struct SPBasicSuite *pica_basicP);

A_Err
VRimg_DeathHook(const SPBasicSuite *pica_basicP);

A_Err
VRimg_IdleHook(const SPBasicSuite *pica_basicP);

A_Err
VRimg_PurgeHook(const SPBasicSuite *pica_basicP);


A_Err
VRimg_ConstructModuleInfo(
	AEIO_ModuleInfo	*info);

A_Err	
VRimg_GetInSpecInfo(
	const A_PathType	*file_pathZ,
	VRimg_inData		*options,
	AEIO_Verbiage		*verbiageP);

A_Err	
VRimg_VerifyFile(
	const A_PathType		*file_pathZ, 
	A_Boolean				*importablePB);

A_Err
VRimg_FileInfo(
	AEIO_BasicData		*basic_dataP,
	const A_PathType	*file_pathZ,
	FrameSeq_Info		*info,
	AEIO_Handle			optionsH);

A_Err	
VRimg_DrawSparseFrame(
	AEIO_BasicData					*basic_dataP,
	const AEIO_DrawSparseFramePB	*sparse_framePPB, 
	PF_EffectWorld					*wP,
	AEIO_DrawingFlags				*draw_flagsP,
	const A_PathType				*file_pathZ,
	FrameSeq_Info					*info,
	VRimg_inData					*options);
	

A_Err	
VRimg_InitInOptions(
	VRimg_inData	*options);

A_Err	
VRimg_ReadOptionsDialog(
	AEIO_BasicData	*basic_dataP,
	VRimg_inData	*options,
	A_Boolean		*user_interactedPB0);
	
A_Err
VRimg_FlattenInputOptions(
	AEIO_BasicData	*basic_dataP,
	AEIO_Handle		optionsH);

A_Err
VRimg_InflateInputOptions(
	AEIO_BasicData	*basic_dataP,
	AEIO_Handle		optionsH);




A_Err	
VRimg_GetNumAuxChannels(
	AEIO_BasicData		*basic_dataP,
	AEIO_Handle			optionsH,
	const A_PathType	*file_pathZ,
	A_long				*num_channelsPL);
									
A_Err	
VRimg_GetAuxChannelDesc(
	AEIO_BasicData		*basic_dataP,
	AEIO_Handle			optionsH,
	const A_PathType	*file_pathZ,
	A_long				chan_indexL,
	PF_ChannelDesc		*descP);
																
A_Err	
VRimg_DrawAuxChannel(
	AEIO_BasicData		*basic_dataP,
	VRimg_inData		*options,
	const A_PathType	*file_pathZ,
	A_long				chan_indexL,
	const AEIO_DrawFramePB	*pbP,
	PF_Point			scale,
	PF_ChannelChunk		*chunkP);



#endif // VRIMG_H
