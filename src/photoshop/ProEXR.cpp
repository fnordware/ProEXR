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

// ProEXR
//  by Brendan Bolles <brendan@fnordware.com>
//		created 21 June 2007
//		re-created 26 April 2009
//
//
// "I can't believe this worked." -The Author
//
// Things to add:
//
// generally more robust when people duplicate base names, special case names, etc
// vector mask support (?)
// Luminance/Chroma for layers
// any kind of metadata I'm missing
// tiles
// mip mapping
// preview image
//
//
// known issues:
//
// can't create a background layer
// can't make a layered greyscale image
// opening/saving is "dirty"
// error message on quit in Windows (sometimes)


#include "ProEXR.h"

#include "PITerminology.h"
#include "PIStringTerminology.h"

#include "ProEXR_Attributes.h"
#include "ProEXR_Color.h"
#include "ProEXR_About.h"

#include "ProEXR_UTF.h"

#include <ImfVersion.h>
#include <ImfStandardAttributes.h>

#ifdef MAC_ENV
	#include <mach/mach.h>
#endif

#include <IlmThread.h>
#include <IlmThreadPool.h>

using namespace Imf;
using namespace Imath;
using namespace std;

#define gStuff				(globals->formatParamBlock)


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


// moving target transfer modes
static PIType modeLinearDodge	= 0x66c;
static PIType modeLighterColor	= 0x672;
static PIType modeDarkerColor	= 0x673;
static PIType modeSubtract		= 0x6ec;
static PIType modeDivide		= 0x6ed;

#ifndef kblendSubtractionStr
#define kblendSubtractionStr	"blendSubtraction"
#endif

#ifndef kblendDivideStr
#define kblendDivideStr	"blendDivide"
#endif


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

static void DoAbout (AboutRecordPtr aboutP)
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

	gOptions.compression_type 		= Imf::PIZ_COMPRESSION;
	gOptions.float_not_half 		= FALSE;
	gOptions.luminance_chroma		= FALSE;
	gOptions.layer_composite		= TRUE;
	gOptions.hidden_layers			= FALSE;

	gInOptions.alpha_mode           = ALPHA_TRANSPARENCY;
	gInOptions.unmult               = FALSE;
	gInOptions.ignore_layertext     = FALSE;
	gInOptions.memory_map			= FALSE;
		
	globals->ps_in = NULL;
	globals->doc_in = NULL;
	globals->total_layers = 0;
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


static bool g_done_global_setup = false;

void GlobalSetup()
{
	if(!g_done_global_setup)
	{
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
		
		staticInitialize();
		
		initializeEXRcolor();
		
		// get identifiers for some mysterious transfer mode non-constants
		PSBasicActionControlProcs *sPSBasicActionControl = NULL;
		sSPBasic->AcquireSuite(kPSBasicActionControlSuite, kPSBasicActionControlSuiteVersion, (const void **)&sPSBasicActionControl);
	
		if(sPSBasicActionControl)
		{
			sPSBasicActionControl->StringIDToTypeID(klinearDodgeStr, (DescriptorTypeID *)&modeLinearDodge);
			sPSBasicActionControl->StringIDToTypeID(klighterColorStr, (DescriptorTypeID *)&modeLighterColor);
			sPSBasicActionControl->StringIDToTypeID(kdarkerColorStr, (DescriptorTypeID *)&modeDarkerColor);
			sPSBasicActionControl->StringIDToTypeID(kblendSubtractionStr, (DescriptorTypeID *)&modeSubtract);
			sPSBasicActionControl->StringIDToTypeID(kblendDivideStr, (DescriptorTypeID *)&modeDivide);
			
			SetNonConstantTransferModes(modeLinearDodge, modeLighterColor, modeDarkerColor, modeSubtract, modeDivide);
			
			sSPBasic->ReleaseSuite(kPSBasicActionControlSuite, kPSBasicActionControlSuiteVersion);
		}
		
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
	if( gResult != noErr || !Imf::isImfMagic(buf) )
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
	const bool split_alpha = (gInOptions.alpha_mode == ALPHA_SEPERATE);
	const bool clip_alpha = !split_alpha;
	
	globals->doc_in = new ProEXRdoc_readPS( *(globals->ps_in), &ps_calls, unMult, clip_alpha, true, split_alpha, !gInOptions.ignore_layertext, true);
	
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
	GetEXRcolor(globals, exr->header());
	
	// load everything (will not actually load if insuffiecient memory)
	exr->loadFromFile();

	gStuff->layerData = exr->layers().size();
	
		
	gStuff->data = (void *)1; // just to keep it going
	
	}
	catch(AbortExc)
	{
		gResult = userCanceledErr;
	}
	catch(PhotoshopExc) {} // leave gResult as-is
	catch(exception &e)
	{
		ReportException(globals, e);
	}
	catch(...) { gResult = formatCannotRead; }
}


static void DoReadContinue(GPtr globals)
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


// used to store layer names I pass in during DoReadLayer
// I can't free that memory until DoReadFinish (as opposed to DoReadLayerFinish)
static vector<uint16 *> g_layer_names;


static void DoReadFinish(GPtr globals)
{	
	try{
	
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
	
	for(vector<uint16 *>::iterator i = g_layer_names.begin(); i != g_layer_names.end(); ++i)
	{
		if(*i)
		{
			free(*i);
			*i = NULL;
		}
	}
	
	g_layer_names.clear();
		
	}catch(...) {}
}

#pragma mark-

static void DoOptionsPrepare(GPtr globals)
{
	gStuff->maxData = 0;
}


static bool hasRGBAlayer(GPtr globals)
{
	bool result = false;
	
	try{
	
	Header fake_header(	Box2i( V2i(0,0), V2i(1, 1) ),
						Box2i( V2i(0,0), V2i(1, 1) ),
						1.0f,
						V2f(0, 0),
						1,
						INCREASING_Y,
						NO_COMPRESSION);

	// fake output file
	OStreamPS fake_out(0, "Dummy Output File");
	
	ProEXRdoc_writePS fake_file(fake_out, fake_header, Imf::HALF, true, true,
									NULL, gStuff->documentInfo, NULL);

	result = fake_file.hasRGBAlayer();
	
	}catch(...) {}
	
	return result;
}


static bool hasHiddenLayers(GPtr globals)
{
	ReadLayerDesc *current_layer = gStuff->documentInfo->layersDescriptor;
	
	while(current_layer)
	{
		if(!current_layer->isVisible)
			return true;
			
		current_layer = current_layer->next;
	}
	
	return false;
}


static void DoOptionsStart(GPtr globals)
{
	ReadParams(globals, &gOptions);
	
	// set defaults to smart choice
	gOptions.layer_composite = !hasRGBAlayer(globals);

	if( ReadScriptParamsOnWrite(globals) )
	{
		if( ProEXR_Write_Dialog(WIN_HANDLE_ARG &gOptions, gStuff->layerData, hasHiddenLayers(globals) ) )
		{
			WriteParams(globals, &gOptions);
			WriteScriptParamsOnWrite(globals);
		}
		else
			gResult = userCanceledErr; //gResult = 'STOP'; //kUserCancel;
	}
}


static void DoOptionsContinue(GPtr globals)
{

}


static void DoOptionsFinish(GPtr globals)
{

}

#pragma mark-

static void DoEstimatePrepare(GPtr globals)
{
	gStuff->maxData = 0;
}


static void DoEstimateStart(GPtr globals)
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


static void DoEstimateContinue(GPtr globals)
{

}


static void DoEstimateFinish(GPtr globals)
{

}

#pragma mark-

static void DoWritePrepare(GPtr globals)
{
	gStuff->maxData = 0;
}


static void DoWriteStart(GPtr globals)
{
	GlobalSetup();
	
	globals->total_layers = gStuff->layerData;

	ReadParams(globals, &gOptions);
	ReadScriptParamsOnWrite(globals);

	gStuff->data = (void *)1; // just to keep it going
}


static void DoWriteContinue(GPtr globals)
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
	header.insert("writer", StringAttribute( string("ProEXR for Photoshop") ) );

	// import the description
	ImportXMPdescription(globals, header);
	
	
	// now make the file
	OStreamPS ps_out(gStuff->dataFork, "Photoshop Output");
	
	PS_callbacks ps_calls = { globals->result, gStuff->channelPortProcs,
							gStuff->abortProc, gStuff->progressProc, gStuff->handleProcs, gStuff->advanceState,
							&gStuff->data, &gStuff->theRect, &gStuff->theRect32, &gStuff->loPlane, &gStuff->hiPlane,
							&gStuff->colBytes, &gStuff->rowBytes, &gStuff->planeBytes };

	// get the alpha channel to use based on options
	ReadChannelDesc *alpha_channel = gStuff->documentInfo->mergedTransparency ?
										gStuff->documentInfo->mergedTransparency :
										gStuff->documentInfo->alphaChannels;

	bool greyscale = (gStuff->documentInfo->imageMode == plugInModeGrayScale || gStuff->documentInfo->imageMode == plugInModeGray32);

	if(globals->options.luminance_chroma && !greyscale)
	{
		RgbaChannels mode = alpha_channel ? WRITE_YCA : WRITE_YC;

		ProEXRdoc_writePS_RGBA output_file(ps_out, header, mode,
										&ps_calls, gStuff->documentInfo, alpha_channel);
		
		output_file.loadFromPhotoshop(); // won't load if insuffiecient memory
		
		output_file.writeFile();
	}
	else
	{
		Imf::PixelType pixelType = globals->options.float_not_half ? Imf::FLOAT : Imf::HALF;
		
		ProEXRdoc_writePS output_file(ps_out, header, pixelType, false, false,
										&ps_calls, gStuff->documentInfo, alpha_channel);
										
		output_file.loadFromPhotoshop(); // won't load if insufficient memory
		
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


static void DoWriteFinish(GPtr globals)
{
	WriteScriptParamsOnWrite(globals);
}


#pragma mark-


static void DoReadLayerStart(GPtr globals)
{

}


static PIType EnumToPSBlendMode(TransferMode mode)
{
	switch(mode)
	{
		case MODE_Normal:			return enumNormal;
		case MODE_Dissolve:			return enumDissolve;
		case MODE_Darken:			return enumDarken;
		case MODE_Multiply:			return enumMultiply;
		case MODE_ColorBurn:		return enumMultiply;	// enumColorBurn not in PS float
		//case MODE_LinearBurn;		return ???				// not in PS float
		case MODE_DarkerColor:		return modeDarkerColor;	// moving target
		case MODE_Lighten:			return enumLighten;
		case MODE_Screen:			return modeLinearDodge;	// enumScreen not in PS float
		case MODE_ColorDodge:		return enumMultiply;	// enumColorDodge not in PS float
		case MODE_LinearDodge:		return modeLinearDodge;	// moving target
		case MODE_LighterColor:		return modeLighterColor;// moving target
		case MODE_Overlay:			return enumLighten;		// enumOverlay not in PS float
		case MODE_SoftLight:		return enumLighten;		// enumSoftLight not in PS float
		case MODE_HardLight:		return enumDarken;		// enumHardLight not in PS float
		//case MODE_VividLight		return ???				// not in PS float
		//case MODE_LinearLight		return ???				// not in PS float
		//case MODE_PinLight		return ???				// not in PS float
		//case MODE_HardMix			return ???				// not in PS float
		case MODE_Difference:		return enumDifference;
		//case MODE_Exclusion:		return enumExclusion;	// enumExclusion not in PS float
		case MODE_Hue:				return enumHue;
		case MODE_Saturation:		return enumSaturation;
		case MODE_Color:			return enumColor;
		case MODE_Luminosity:		return enumLuminosity;
		case MODE_Subtract:			return modeSubtract;
		case MODE_Divide:			return modeDivide;
		
		default:					return enumNormal;
	}
}


static uint16* myUTF16ptr(const char *str)
{
	unsigned int len = strlen(str) + 1;
	
	uint16 *out = (uint16 *)malloc( len * sizeof(uint16) );
	
	UTF8toUTF16(str, out, len);
		
	return out;
}


static void DoReadLayerContinue(GPtr globals)
{
	try{
	
	if(globals->doc_in == NULL)
	{
		gResult = paramErr;
		gStuff->data = NULL;
		return;
	}
	
	PS_callbacks ps_calls = { globals->result, gStuff->channelPortProcs,
							gStuff->abortProc, gStuff->progressProc, gStuff->handleProcs, gStuff->advanceState,
							&gStuff->data, &gStuff->theRect, &gStuff->theRect32, &gStuff->loPlane, &gStuff->hiPlane,
							&gStuff->colBytes, &gStuff->rowBytes, &gStuff->planeBytes };
	
	globals->doc_in->set_ps_calls(&ps_calls);
	
	globals->doc_in->copyLayerToPhotoshop(gStuff->layerData, (gInOptions.alpha_mode == ALPHA_TRANSPARENCY));
	
	// put up our fancy layer name
	gStuff->layerName = myUTF16ptr( globals->doc_in->layers().at(gStuff->layerData)->ps_name().c_str() );
	
	g_layer_names.push_back(gStuff->layerName); // remember pointers so we can free them later
	
	
	// set layer attributes
	ProEXRlayer_readPS *layer = dynamic_cast<ProEXRlayer_readPS *>( globals->doc_in->layers().at(gStuff->layerData) );
	
	if(layer)
	{
		gStuff->isVisible = layer->visibility();
		gStuff->blendMode = EnumToPSBlendMode(layer->transfer_mode() );
		gStuff->opacity = layer->opacity();
	}
	
	
	// progress
	PIUpdateProgress(gStuff->layerData + 1, globals->doc_in->layers().size() );

	
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
	
	
	// very important!
	gStuff->data = NULL;
}


static void DoReadLayerFinish(GPtr globals)
{
	if(gStuff->layerName)
	{
		//free(gStuff->layerName);  // Hmm, would like to delete this, but marking it NULL causes Photoshop
		//gStuff->layerName = NULL; // to forget the layer name, so is it still being used?
		
		// the solution is we can free it during DoReadFinish
	}
}


#pragma mark-


static void DoWriteLayerStart(GPtr globals)
{
	gStuff->data = (void *)1; // just to keep it going
}


static void DoWriteLayerContinue(GPtr globals)
{
	if(gStuff->layerData == 0 && globals->total_layers == 1 && gOptions.luminance_chroma)
	{
		// we can only do Luminance/Chroma to one layer now
		// because we're using ILM's code
		DoWriteContinue(globals);
	}
	else if(gStuff->layerData == 0 &&  // we're gonna do this all at once
		gStuff->channelPortProcs && gStuff->documentInfo) // but not without these
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
		header.insert("writer", StringAttribute( string("ProEXR for Photoshop") ) );
		
		// import the description
		ImportXMPdescription(globals, header);
		
		
		// now make the file
		OStreamPS ps_out(gStuff->dataFork, "Photoshop Output");
		
		PS_callbacks ps_calls = { globals->result, gStuff->channelPortProcs,
								gStuff->abortProc, gStuff->progressProc, gStuff->handleProcs, gStuff->advanceState,
								&gStuff->data, &gStuff->theRect, &gStuff->theRect32, &gStuff->loPlane, &gStuff->hiPlane,
								&gStuff->colBytes, &gStuff->rowBytes, &gStuff->planeBytes };

		Imf::PixelType pixelType = gOptions.float_not_half ? Imf::FLOAT : Imf::HALF;
		
		ProEXRdoc_writePS output_file(ps_out, header, pixelType, true, gOptions.hidden_layers,
										&ps_calls, gStuff->documentInfo, NULL);
		
		if(gOptions.layer_composite)
			output_file.addMainLayer(gStuff->documentInfo->mergedCompositeChannels,
										gStuff->documentInfo->mergedTransparency,
										gStuff->documentInfo->mergedTransparency,
										NULL, pixelType);
																										
		output_file.loadFromPhotoshop(); // won't load if insufficient memory
		
		output_file.writeFile();
			
		}
		catch(AbortExc)
		{
			gResult = userCanceledErr;
		}
		catch(PhotoshopExc) {} // leave gResult as-is
		catch(exception &e)
		{
			ReportException(globals, e);
		}
		catch(...) { gResult = formatBadParameters; }
	}


	// always
	gStuff->data = NULL;
}


static void DoWriteLayerFinish(GPtr globals)
{

}

#pragma mark-

// --------------------------------------------------
// PluginMain
//
// Entry Point
//

DLLExport MACPASCAL void PluginMain(const short selector,
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


#define formatSelectorReadLayerStart		35
#define formatSelectorReadLayerContinue		36
#define formatSelectorReadLayerFinish		37

#define formatSelectorWriteLayerStart		38
#define formatSelectorWriteLayerContinue	39
#define formatSelectorWriteLayerFinish		40

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
			
			
			case formatSelectorReadLayerStart:		DoReadLayerStart(globals);			break;
			case formatSelectorReadLayerContinue:	DoReadLayerContinue(globals);		break;
			case formatSelectorReadLayerFinish:		DoReadLayerFinish(globals);			break;

			case formatSelectorWriteLayerStart:		DoWriteLayerStart(globals);			break;
			case formatSelectorWriteLayerContinue:	DoWriteLayerContinue(globals);		break;
			case formatSelectorWriteLayerFinish:	DoWriteLayerFinish(globals);		break;
						
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
