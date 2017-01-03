
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

#ifndef PROEXR_AE_DIALOGS_H
#define PROEXR_AE_DIALOGS_H

#include <string>

typedef struct {
	int compression;
	bool float_not_half;
	bool layer_composite;
	bool hidden_layers;
} ProEXR_AE_Out_Data;

bool
ProEXR_AE_Out(
	ProEXR_AE_Out_Data	*params,
	const void		*plugHndl,
	const void		*mwnd);


enum TimeSpan {
	TIMESPAN_CURRENT_FRAME = 0,
	TIMESPAN_WORK_AREA,
	TIMESPAN_FULL_COMP,
	TIMESPAN_CUSTOM
};

typedef struct {
	int compression;
	bool float_not_half;
	bool layer_composite;
	bool hidden_layers;
	TimeSpan timeSpan;
	int start_frame;
	int end_frame;
} ProEXR_AE_GUI_Data;

bool
ProEXR_AE_GUI(
	ProEXR_AE_GUI_Data	*params,
	std::string		&path,
	int				current_frame,
	int				work_start,
	int				work_end,
	int				comp_start,
	int				comp_end,
	const void		*plugHndl,
	const void		*mwnd);

bool
ProEXR_AE_Update_Progress(
	int				current_frame,
	int				total_frames,
	const void		*plugHndl,
	const void		*mwnd);

void
ProEXR_AE_End_Progress();

void ProEXR_CopyPluginPath(char *pathZ, int max_len);

#endif // PROEXR_AE_DIALOGS_H