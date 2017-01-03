
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

#include "ProEXR_EZ.h"

#include "ProEXR_Attributes.h"
#include "ProEXR_Color.h"
#include "ProEXR_About.h"

#include <ImfVersion.h>
#include <ImfStandardAttributes.h>


// un-comment this to do multi-threading (which we're still not sure about)
#define PROEXR_MULTITHREAD

#ifdef PROEXR_MULTITHREAD
#ifdef MAC_ENV
	#include <mach/mach.h>
#endif
	#include <IlmThread.h>
	#include <IlmThreadPool.h>
#endif


using namespace Imf;
using namespace Imath;
using namespace std;



#ifdef __PIWin__
#include "myDLLInstance.h"
HANDLE hDllInstance = NULL;

#define WIN_HANDLE	(HWND)((PlatformData *)gStuff->platformData)->hwnd
#define WIN_HANDLE_ARG	WIN_HANDLE##,
#else
#define WIN_HANDLE
#define WIN_HANDLE_ARG
#endif

SPBasicSuite * sSPBasic = NULL;

SPPluginRef gPlugInRef = NULL;


static Handle myNewHandle(GPtr globals, const int32 inSize)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->newProc != NULL)
	{
		return gStuff->handleProcs->newProc(inSize);
	}
	else
	{
		return PINewHandle(inSize);
	}
}

static Ptr myLockHandle(GPtr globals, Handle h)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->lockProc)
	{
		return gStuff->handleProcs->lockProc(h, TRUE);
	}
	else
	{
		return PILockHandle(h, TRUE);
	}
}

static void myUnlockHandle(GPtr globals, Handle h)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->unlockProc)
	{
		gStuff->handleProcs->unlockProc(h);
	}
	else
	{
		PIUnlockHandle(h);
	}
}

static int32 myGetHandleSize(GPtr globals, Handle h)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->getSizeProc)
	{
		return gStuff->handleProcs->getSizeProc(h);
	}
	else
	{
		return PIGetHandleSize(h);
	}
}

static void mySetHandleSize(GPtr globals, Handle h, const int32 inSize)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->setSizeProc)
	{
		gStuff->handleProcs->setSizeProc(h, inSize);
	}
	else
	{
		PISetHandleSize(h, inSize);
	}
}

static void myDisposeHandle(GPtr globals, Handle h)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->newProc != NULL)
	{
		gStuff->handleProcs->disposeProc(h);
	}
	else
	{
		PIDisposeHandle(h);
	}
}

#pragma mark-

static void DoAbout(AboutRecordPtr aboutP)
{
#ifdef __PIMac__
	ProEXR_About();
#else
	ProEXR_About((HWND)((PlatformData *)aboutP->platformData)->hwnd);
#endif
}

static void InitGlobals(Ptr globalPtr)
{	
	// create "globals" as a our struct global pointer so that any
	// macros work:
	GPtr globals = (GPtr)globalPtr;
		
	gOptions.compression_type 		= PIZ_COMPRESSION;
	gOptions.float_not_half 		= FALSE;
	gOptions.luminance_chroma		= FALSE;
	gOptions.alpha_mode				= WRITE_ALPHA_TRANSPARENCY;
    
    gInOptions.alpha_mode           = ALPHA_TRANSPARENCY;
    gInOptions.unmult               = FALSE;
	gInOptions.memory_map			= FALSE;
	
	globals->ps_in					= NULL;
	globals->doc_in					= NULL;
}

#pragma mark-

static void CreateXMPdescription(GPtr globals, const ProEXRdoc_read &file)
{
	string XMPtext = CreateXMPdescription(file);
	
	Handle xmp_handle = myNewHandle(globals, XMPtext.size() );
	char *xmp_ptr = (char *)myLockHandle(globals, xmp_handle);
	
	// turns out it's important that the handle not include the '\0' character at the end
	// Photoshop is sooooo picky
	memcpy(xmp_ptr, XMPtext.c_str(), XMPtext.size() );
	
	myUnlockHandle(globals, xmp_handle);
	
	PISetProp(kPhotoshopSignature, 'xmpd', 0, NULL, xmp_handle); // propXMP
}

static void ImportXMPdescription(GPtr globals, Header &head)
{
	Handle xmp_handle = NULL;

	PIGetProp(kPhotoshopSignature, 'xmpd', 0, NULL, &xmp_handle); // propXMP
	
	if(xmp_handle != NULL)
	{
		const size_t xmp_size = myGetHandleSize(globals, xmp_handle);
		const char *xmp_text = myLockHandle(globals, xmp_handle);
		
		if(xmp_text != NULL && xmp_size > 0)
		{
			const int width = gStuff->documentInfo->bounds.right - gStuff->documentInfo->bounds.left;
			const int height = gStuff->documentInfo->bounds.bottom - gStuff->documentInfo->bounds.top;
			
			ParseXMPdescription(head, width, height, xmp_text, xmp_size);
		}
		
		myUnlockHandle(globals, xmp_handle);
	}
}

static void GetEXRcolor(GPtr globals, const Header &head)
{
	size_t prof_len = 0;
	void *icc = NULL;
	
	GetEXRcolor(head, &icc, &prof_len);
	
	if(icc && prof_len)
	{
		gStuff->iCCprofileSize = prof_len;
		gStuff->iCCprofileData = myNewHandle(globals, prof_len);
		
		void *ps_profile = myLockHandle(globals, gStuff->iCCprofileData);
		
		memcpy(ps_profile, icc, prof_len);
		
		myUnlockHandle(globals, gStuff->iCCprofileData);
	
		free(icc);
	}
}

static void SetEXRcolor(GPtr globals, Header &head)
{
	if(gStuff->iCCprofileData && gStuff->iCCprofileSize)
	{
		void *ps_profile = myLockHandle(globals, gStuff->iCCprofileData);
		
		SetEXRcolor(ps_profile, gStuff->iCCprofileSize, head);
		
		myUnlockHandle(globals, gStuff->iCCprofileData);
	}
}


#pragma mark-


bool g_done_global_setup = false;

void GlobalSetup(void)
{
	if(!g_done_global_setup)
	{
	#ifdef PROEXR_MULTITHREAD
		using namespace Imf;
		using namespace IlmThread;

		if( supportsThreads() )
		{
	#ifdef MAC_ENV
			// get number of CPUs using Mach calls
			host_basic_info_data_t hostInfo;
			mach_msg_type_number_t infoCount;
			
			infoCount = HOST_BASIC_INFO_COUNT;
			host_info(mach_host_self(), HOST_BASIC_INFO, 
					  (host_info_t)&hostInfo, &infoCount);
			
			setGlobalThreadCount(hostInfo.max_cpus);
	#else // WIN_ENV
			SYSTEM_INFO systemInfo;
			GetSystemInfo(&systemInfo);

			setGlobalThreadCount(systemInfo.dwNumberOfProcessors);
	#endif
		}
	#endif
		staticInitialize();
		
		initializeEXRcolor();
	
		g_done_global_setup = true;
	}
}


static void ReportException(GPtr globals, exception &e)
{
	Str255 p_str;
	
	string err_str = e.what();
	int size = MIN(255, err_str.size());
	
	p_str[0] = size;
	strncpy((char *)&p_str[1], err_str.c_str(), size);
	
	PIReportError(p_str);
	gResult = errReportString;
}

#pragma mark-

// These transfer settings to and from gStuff->revertInfo

static void TwiddleOptions(ProEXR_inData *options)
{
#ifndef __PIMac__
#endif
}

static void TwiddleOptions(ProEXR_outData *options)
{
#ifndef __PIMac__
#endif
}

template <typename T>
static A_Boolean ReadParams(GPtr globals, T *options)
{
	A_Boolean found_revert = FALSE;
	
	if( gStuff->revertInfo != NULL )
	{
		if( myGetHandleSize(globals, gStuff->revertInfo) == sizeof(T) )
		{
			T *flat_options = (T *)myLockHandle(globals, gStuff->revertInfo);
			
			// flatten and copy
			TwiddleOptions(flat_options);
			
			memcpy((char*)options, (char*)flat_options, sizeof(T) );
			
			TwiddleOptions(flat_options);
			
			myUnlockHandle(globals, gStuff->revertInfo);
			
			found_revert = TRUE;
		}
	}
	
	return found_revert;
}

template <typename T>
static void WriteParams(GPtr globals, T *options)
{
	T *flat_options = NULL;
	
	if(gStuff->hostNewHdl != NULL) // we have the handle function
	{
		if(gStuff->revertInfo == NULL)
		{
			gStuff->revertInfo = myNewHandle(globals, sizeof(T) );
		}
		else
		{
			if(myGetHandleSize(globals, gStuff->revertInfo) != sizeof(T)  )
				mySetHandleSize(globals, gStuff->revertInfo, sizeof(T) );
		}
		
		flat_options = (T *)myLockHandle(globals, gStuff->revertInfo);
		
		// flatten and copy
		TwiddleOptions(flat_options);
		
		memcpy((char*)flat_options, (char*)options, sizeof(T) );	
		
		TwiddleOptions(flat_options);
			
		
		myUnlockHandle(globals, gStuff->revertInfo);
	}
}

#pragma mark-

static void DoFilterFile(GPtr globals)
{
	// Start at the top and read 4 bytes.
#define TEST_BYTES	4

	char buf[TEST_BYTES];
	   
	try{
		IStreamPS in_file(gStuff->dataFork, "Photoshop Input");

		bool read_result = in_file.read(buf, TEST_BYTES);

		if(!read_result)
			gResult = formatCannotRead;
	}
	catch(...) { gResult = formatCannotRead; }
	
	// Check the identifier
	if( gResult != noErr || !isImfMagic(buf) )
	{
		gResult = formatCannotRead;
	}
}

static void DoReadPrepare(GPtr globals)
{
	gStuff->maxData = 0;
}

static void DoReadStart(GPtr globals)
{	
	GlobalSetup();
	
	// Photoshop scripting is messing things up on import
	// it wants to remember the previous setting and force the dialog to come up
	// if someone wants it really bad, we could enable it with a preference
	//Boolean showDialog = ReadScriptParamsOnRead(globals);
	A_Boolean reverting = ReadParams(globals, &gInOptions);
	
	// import dialog, check for Cancel
	if( reverting || ProEXR_Read_Dialog(WIN_HANDLE_ARG &gInOptions) )
	{
		//WriteScriptParamsOnRead(globals);
		if(!reverting)
			WriteParams(globals, &gInOptions);
	}
	else
	{
		gResult = userCanceledErr;
		return;
	}
    
    
	try{
	
	globals->ps_in = new IStreamPS(gStuff->dataFork, "Photoshop Input");
	
	if(gInOptions.memory_map)
		globals->ps_in->memoryMap();
	
	PS_callbacks ps_calls = { globals->result, gStuff->channelPortProcs,
							gStuff->abortProc, gStuff->progressProc, gStuff->handleProcs, gStuff->advanceState,
							&gStuff->data, &gStuff->theRect, &gStuff->theRect32, &gStuff->loPlane, &gStuff->hiPlane,
							&gStuff->colBytes, &gStuff->rowBytes, &gStuff->planeBytes };

	const bool unMult = (gInOptions.alpha_mode == ALPHA_TRANSPARENCY || gInOptions.unmult);
	const bool clip_alpha = gInOptions.alpha_mode == ALPHA_TRANSPARENCY;
					
	globals->doc_in = new ProEXRdoc_readPS( *(globals->ps_in), &ps_calls, unMult, clip_alpha, false);
	
	ProEXRdoc_readPS *exr = globals->doc_in;
	
	gStuff->PluginUsing32BitCoordinates = TRUE;
	gStuff->imageSize.h = gStuff->imageSize32.h = exr->width();
	gStuff->imageSize.v = gStuff->imageSize32.v = exr->height();
	
	gStuff->imageMode = exr->ps_mode();
	gStuff->depth = 32;
	gStuff->planes = exr->ps_planes();
		
	gStuff->imageHRes = exr->ps_res().x;
	gStuff->imageVRes = exr->ps_res().y;
	

	// XMP description
	CreateXMPdescription(globals, *exr);
	
	// got chromaticities?
	if(gStuff->planes >= 3)
		GetEXRcolor(globals, exr->header());
	
	
	if( ( (gStuff->imageMode == plugInModeGrayScale && gStuff->planes >= 2) || gStuff->planes >= 4 ) &&
			gInOptions.alpha_mode == ALPHA_TRANSPARENCY)
	{
		gStuff->transparencyPlane = gStuff->planes - 1;
		gStuff->transparencyMatting = 0; // 0 = straight (do nothing), 1 = premultiplied
	}
	else
	{
		gStuff->transparencyPlane = 0;
		gStuff->transparencyMatting = 0;
	}
	

	// get the Photoshop version - we need PS9 to do float
/*	int32 version = 0;
	PIGetProp(kPhotoshopSignature, 'vers', 0, &version, NULL); // propVersion
	
	int version_major = (version & 0xffff0000) >> 16;
			
	if(head.channels().findChannel("A"))
	{
		//gStuff->planes++;
		
		if(version_major >= 10)
		{
			gStuff->transparencyPlane = gStuff->planes - 1; // but if no layers, no transparency
			gStuff->transparencyMatting = 1;
		}
	}
*/
	
	gStuff->data = (void *)1; // just to keep it going
	
	}
	catch(AbortExc)
	{
		gResult = userCanceledErr;
	}
	catch(PhotoshopExc) {} // leave error as-is
	catch(exception &e)
	{
		ReportException(globals, e);
	}
	catch(...) { gResult = formatCannotRead; }
}

static void DoReadContinue (GPtr globals)
{
	try{

	if(globals->doc_in == NULL)
	{
		gResult = paramErr;
		gStuff->data = NULL;
		return;
	}
	
	ProEXRdoc_readPS *exr = globals->doc_in;
	
	PS_callbacks ps_calls = { globals->result, gStuff->channelPortProcs,
							gStuff->abortProc, gStuff->progressProc, gStuff->handleProcs, gStuff->advanceState,
							&gStuff->data, &gStuff->theRect, &gStuff->theRect32, &gStuff->loPlane, &gStuff->hiPlane,
							&gStuff->colBytes, &gStuff->rowBytes, &gStuff->planeBytes };
							
	exr->set_ps_calls(&ps_calls);
	
	exr->copyMainToPhotoshop();

	
	}
	catch(AbortExc)
	{
		gResult = userCanceledErr;
	}
	catch(PhotoshopExc) {} // leave error as-is
	catch(exception &e)
	{
		ReportException(globals, e);
	}
	catch(...) { gResult = formatCannotRead; }
	
	
	// very important!
	gStuff->data = NULL;
}

static void DoReadFinish (GPtr globals)
{	
	if(globals->doc_in)
	{
		delete globals->doc_in;
		globals->doc_in = NULL;
	}
	
	if(globals->ps_in)
	{
		delete globals->ps_in;
		globals->ps_in = NULL;
	}
}

#pragma mark-

static void DoOptionsPrepare (GPtr globals)
{
	gStuff->maxData = 0;
}


static void DoOptionsStart(GPtr globals)
{
	ReadParams(globals, &gOptions);
	
	if( ReadScriptParamsOnWrite(globals) )
	{
		bool greyscale = (gStuff->documentInfo->imageMode == plugInModeGrayScale || gStuff->documentInfo->imageMode == plugInModeGray32);
		
		const char *alpha_name = NULL;
		
		if(gStuff->documentInfo && gStuff->documentInfo->alphaChannels)
			alpha_name = gStuff->documentInfo->alphaChannels->name;
		
		bool have_transparency = (gStuff->documentInfo && gStuff->documentInfo->mergedTransparency);

		if( ProEXR_Write_Dialog(WIN_HANDLE_ARG &gOptions, alpha_name, have_transparency, greyscale) )
		{
			WriteParams(globals, &gOptions);
			WriteScriptParamsOnWrite(globals);
		}
		else
			gResult = userCanceledErr; //gResult = 'STOP'; //kUserCancel;
	}
}

static void DoOptionsContinue (GPtr globals)
{

}

static void DoOptionsFinish (GPtr globals)
{

}

#pragma mark-

static void DoEstimatePrepare (GPtr globals)
{
	gStuff->maxData = 0;
}

static void DoEstimateStart (GPtr globals)
{
	long long dataBytes;
	
	if(gStuff->HostSupports32BitCoordinates && gStuff->PluginUsing32BitCoordinates)
	{
		dataBytes = (long long)gStuff->imageSize32.h * (long long)gStuff->imageSize32.v *
							(long long)gStuff->planes * (long long)(gStuff->depth >> 3);
	}
	else
	{
		dataBytes = (long long)gStuff->imageSize.h * (long long)gStuff->imageSize.v *
							(long long)gStuff->planes * (long long)(gStuff->depth >> 3);
	}

	// if we're doing 16-bit float, there goes half our size right away
	if(!gOptions.float_not_half)
		dataBytes /= 2;

	// here we guess how much disk space will be needed
	// somewhere between 0% and 50% compression rate sounds good
	gStuff->minDataBytes = MIN(dataBytes / 2, INT_MAX);
	gStuff->maxDataBytes = MIN(dataBytes, INT_MAX);
	
	gStuff->data = NULL;
}

static void DoEstimateContinue (GPtr globals)
{

}

static void DoEstimateFinish (GPtr globals)
{

}

#pragma mark-

static void DoWritePrepare (GPtr globals)
{
	gStuff->maxData = 0;
}

static void DoWriteStart (GPtr globals)
{
	GlobalSetup();
	
	ReadParams(globals, &gOptions);
	ReadScriptParamsOnWrite(globals);

	gStuff->data = (void *)1; // just to keep it going
}

static void DoWriteContinue (GPtr globals)
{
	try{
	
	int data_width, display_width, data_height, display_height;
	
	data_width = display_width = gStuff->documentInfo->bounds.right - gStuff->documentInfo->bounds.left;
	data_height = display_height = gStuff->documentInfo->bounds.bottom - gStuff->documentInfo->bounds.top;
	
	
	// set up header
	Header header(	Box2i( V2i(0,0), V2i(display_width-1, display_height-1) ),
					Box2i( V2i(0,0), V2i(data_width-1, data_height-1) ),
					(float)gStuff->imageVRes / (float)gStuff->imageHRes,
					V2f(0, 0),
					1,
					INCREASING_Y,
					(Compression)globals->options.compression_type);

	// dpi
	addXDensity(header, (float)gStuff->imageHRes / 65536.0f);

	// save chromaticities
	if(gStuff->imageMode != plugInModeGrayScale )
		SetEXRcolor(globals, header);
	
	// extra attributes
	AddExtraAttributes(header);
	
	// graffiti
	header.insert("writer", StringAttribute( string("ProEXR EZ for Photoshop") ) );
	
	// import the description
	ImportXMPdescription(globals, header);
	
	
	// now make the file
	OStreamPS ps_out(gStuff->dataFork, "Photoshop Output");
	
	PS_callbacks ps_calls = { globals->result, gStuff->channelPortProcs,
							gStuff->abortProc, gStuff->progressProc, gStuff->handleProcs, gStuff->advanceState,
							&gStuff->data, &gStuff->theRect, &gStuff->theRect32, &gStuff->loPlane, &gStuff->hiPlane,
							&gStuff->colBytes, &gStuff->rowBytes, &gStuff->planeBytes };

	// get the alpha channel to use based on options
	ReadChannelDesc *alpha_channel = NULL;
	
	if(gOptions.alpha_mode == WRITE_ALPHA_TRANSPARENCY)
		alpha_channel = gStuff->documentInfo->mergedTransparency;
	else if(gOptions.alpha_mode == WRITE_ALPHA_CHANNEL)
		alpha_channel = gStuff->documentInfo->alphaChannels;
	
	bool greyscale = (gStuff->documentInfo->imageMode == plugInModeGrayScale || gStuff->documentInfo->imageMode == plugInModeGray32);

	if(globals->options.luminance_chroma && !greyscale)
	{
		RgbaChannels mode = alpha_channel ? WRITE_YCA : WRITE_YC;

		ProEXRdoc_writePS_RGBA output_file(ps_out, header, mode,
										&ps_calls, gStuff->documentInfo, alpha_channel);
		
		output_file.loadFromPhotoshop(); // won't load if insufficient memory
		
		output_file.writeFile();
	}
	else
	{
		Imf::PixelType pixelType = globals->options.float_not_half ? Imf::FLOAT : Imf::HALF;
		
		ProEXRdoc_writePS output_file(ps_out, header, pixelType, false, false,
										&ps_calls, gStuff->documentInfo, alpha_channel);
										
		output_file.loadFromPhotoshop();
		
		output_file.writeFile();
	}
	
	}
	catch(AbortExc)
	{
		gResult = userCanceledErr;
	}
	catch(PhotoshopExc) {} // leave error as-is
	catch(exception &e)
	{
		ReportException(globals, e);
	}
	catch(...) { gResult = formatBadParameters; }


	// very important
	gStuff->data = NULL;
}

static void DoWriteFinish (GPtr globals)
{
	WriteScriptParamsOnWrite(globals);
}

#pragma mark-

// --------------------------------------------------
// PluginMain
//
// Entry Point
//

DLLExport MACPASCAL void PluginMain (const short selector,
						             FormatRecord *formatParamBlock,
						             entryData *data,
						             short *result)
{
	//---------------------------------------------------------------------------
	//	(1) Enter code resource if Mac 68k.
	//---------------------------------------------------------------------------
	
	#ifdef UseA4				// Are we in 68k Mac MW?
		EnterCodeResource(); 	// A4-globals
	#endif
	
	//---------------------------------------------------------------------------
	//	(2) Check for about box request.
	//
	// 	The about box is a special request; the parameter block is not filled
	// 	out, none of the callbacks or standard data is available.  Instead,
	// 	the parameter block points to an AboutRecord, which is used
	// 	on Windows.
	//---------------------------------------------------------------------------

	if (selector == formatSelectorAbout)
	{
		sSPBasic = ((AboutRecordPtr)formatParamBlock)->sSPBasic;

	#ifdef __PIWin__
		if(hDllInstance == NULL)
			hDllInstance = GetDLLInstance(NULL);
	#endif

		DoAbout((AboutRecordPtr)formatParamBlock);
	}
	else
	{
		sSPBasic = formatParamBlock->sSPBasic;  //thanks Tom
		
		gPlugInRef = (SPPluginRef)formatParamBlock->plugInRef;
				
		
	// preferred way to set this
	#ifdef __PIWin__
		if(hDllInstance == NULL)
			hDllInstance = GetDLLInstance((SPPluginRef)formatParamBlock->plugInRef);
	#endif
		
		
		Ptr globalPtr = NULL;		// Pointer for global structure
		GPtr globals = NULL; 		// actual globals

		//-----------------------------------------------------------------------
		//	(4) Allocate and initalize globals.
		//
		// 	AllocateGlobals requires the pointer to result, the pointer to the
		// 	parameter block, a pointer to the handle procs, the size of our local
		// 	"Globals" structure, a pointer to the long *data, a Function
		// 	Proc (FProc) to the InitGlobals routine.  It automatically sets-up,
		// 	initializes the globals (if necessary), results result to 0, and
		// 	returns with a valid pointer to the locked globals handle or NULL.
		//-----------------------------------------------------------------------
		
		if(formatParamBlock->handleProcs)
		{
			bool must_init = false;
			
			if(*data == NULL)
			{
				*data = (entryData)formatParamBlock->handleProcs->newProc(sizeof(Globals));
				
				must_init = true;
			}

			if(*data != NULL)
			{
				globalPtr = formatParamBlock->handleProcs->lockProc((Handle)*data, TRUE);
				
				if(must_init)
					InitGlobals(globalPtr);
			}
			else
			{
				*result = memFullErr;
				return;
			}

			globals = (GPtr)globalPtr;
			
			globals->result = result;
			globals->formatParamBlock = formatParamBlock;
		}
		else
		{
			// old lame way
			globalPtr = AllocateGlobals((allocateGlobalsPointer)result,
										 (allocateGlobalsPointer)formatParamBlock,
										 formatParamBlock->handleProcs,
										 sizeof(Globals),
						 				 data,
						 				 InitGlobals);

			if(globalPtr == NULL)
			{ // Something bad happened if we couldn't allocate our pointer.
			  // Fortunately, everything's already been cleaned up,
			  // so all we have to do is report an error.
			  
			  *result = memFullErr;
			  return;
			}
			
			// Get our "globals" variable assigned as a Global Pointer struct with the
			// data we've returned:
			globals = (GPtr)globalPtr;
		}

		//-----------------------------------------------------------------------
		//	(5) Dispatch selector.
		//-----------------------------------------------------------------------


		switch(selector)
		{
			case formatSelectorReadPrepare:			DoReadPrepare(globals);				break;
			case formatSelectorReadStart:			DoReadStart(globals);				break;
			case formatSelectorReadContinue:		DoReadContinue(globals);			break;
			case formatSelectorReadFinish:			DoReadFinish(globals);				break;
			
			case formatSelectorOptionsPrepare:		DoOptionsPrepare(globals);			break;
			case formatSelectorOptionsStart:		DoOptionsStart(globals);			break;
			case formatSelectorOptionsContinue:		DoOptionsContinue(globals);			break;
			case formatSelectorOptionsFinish:		DoOptionsFinish(globals);			break;
			
			case formatSelectorEstimatePrepare:		DoEstimatePrepare(globals);			break;
			case formatSelectorEstimateStart:		DoEstimateStart(globals);			break;
			case formatSelectorEstimateContinue:	DoEstimateContinue(globals);		break;
			case formatSelectorEstimateFinish:		DoEstimateFinish(globals);			break;
			
			case formatSelectorWritePrepare:		DoWritePrepare(globals);			break;
			case formatSelectorWriteStart:			DoWriteStart(globals);				break;
			case formatSelectorWriteContinue:		DoWriteContinue(globals);			break;
			case formatSelectorWriteFinish:			DoWriteFinish(globals);				break;

			case formatSelectorFilterFile:			DoFilterFile(globals);				break;

			default:
				gResult = formatBadParameters;
				break;
		}
		
		//-----------------------------------------------------------------------
		//	(6) Unlock data, and exit resource.
		//
		//	Result is automatically returned in *result, which is
		//	pointed to by gResult.
		//-----------------------------------------------------------------------	
		
		// unlock handle pointing to parameter block and data so it can move
		// if memory gets shuffled:
		if((Handle)*data != NULL)
		{
			if(formatParamBlock->handleProcs)
			{
				formatParamBlock->handleProcs->unlockProc((Handle)*data);
			}
			else
			{
				PIUnlockHandle((Handle)*data);
			}
		}
	
	} // about selector special		
	
	#ifdef UseA4			// Are we in 68k Mac MW?
		ExitCodeResource(); // A4-globals
	#endif
	
} // end PluginMain
