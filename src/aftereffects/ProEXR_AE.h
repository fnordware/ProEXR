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

#ifndef ProEXR_AE_H
#define ProEXR_AE_H

#include "ProEXR_AE_FrameSeq.h"


#define PLUGIN_NAME		"ProEXR Layers"



#define MAX_CHANNELS	128




typedef struct ProEXR_outData
{
	A_u_char	compression_type;
	A_Boolean	float_not_half;
	A_Boolean	layer_composite;
	A_Boolean	hidden_layers;
	char		reserved[60]; // total of 64 bytes
} ProEXR_outData;


A_Err
ProEXR_Init(struct SPBasicSuite *pica_basicP);


A_Err
ProEXR_ConstructModuleInfo(
	AEIO_ModuleInfo	*info);


A_Err	
ProEXR_GetDepths(
	AEIO_SupportedDepthFlags		*which);
	

A_Err	
ProEXR_InitOutOptions(
	ProEXR_outData	*options);

A_Err
ProEXR_OutputFile(
	AEIO_BasicData		*basic_dataP,
	AEIO_OutSpecH		outH,
	const A_PathType	*file_pathZ,
	FrameSeq_Info		*info,
	ProEXR_outData	*options,
	PF_EffectWorld		*wP);

A_Err	
ProEXR_WriteOptionsDialog(
	AEIO_BasicData		*basic_dataP,
	ProEXR_outData	*options,
	A_Boolean			*user_interactedPB0);

A_Err	
ProEXR_GetOutSpecInfo(
	const A_PathType	*file_pathZ,
	ProEXR_outData		*options,
	AEIO_Verbiage		*verbiageP);

A_Err
ProEXR_FlattenOutputOptions(
	ProEXR_outData	*options);

A_Err
ProEXR_InflateOutputOptions(
	ProEXR_outData	*options);
	
	
A_Err
ProEXR_DeathHook(const SPBasicSuite *pica_basicP);

#endif // ProEXR_AE_H
