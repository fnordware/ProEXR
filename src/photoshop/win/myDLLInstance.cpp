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


//-------------------------------------------------------------------------------
//	Includes
//-------------------------------------------------------------------------------
#include "myDLLInstance.h"

namespace myDLLInstance 
{
	HINSTANCE dllInstance = NULL;
}

using namespace myDLLInstance;

HINSTANCE GetDLLInstance(SPPluginRef plugin)
{
	// Photoshop's PIDLLInstance.cpp was doing some fancy stuff that
	// involved the AutoSuite and seemed to cause problems.
	// We'll just keep it simple.
	
	return dllInstance;
}

//-------------------------------------------------------------------------------
//
// DllMain
//
// Initialization and termination code for Windows DLLs
//
//-------------------------------------------------------------------------------


extern "C" BOOL APIENTRY DllMain(HANDLE hModule,
					             DWORD ul_reason_for_call,
					             LPVOID /*lpReserved*/);

BOOL APIENTRY DllMain(HANDLE hModule,
					  DWORD ul_reason_for_call,
					  LPVOID /*lpReserved*/)
{
	// very odd, when running SimpleFormat and selecting multiple images
	// in the open dialog I get dettach messages at about the 7th image ?????
	// the detach messages come with a valid hModule so no if ?????
	// if (ul_reason_for_call == DLL_PROCESS_ATTACH ||
	// 	   ul_reason_for_call == DLL_THREAD_ATTACH)
		dllInstance = static_cast<HINSTANCE>(hModule);
	// else
	// dllInstance = NULL;
	return  true;
}
// end PIDLLInstance.cpp