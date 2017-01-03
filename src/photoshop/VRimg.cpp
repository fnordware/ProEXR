
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



#include "VRimg.h"

#include "VRimgVersion.h"

#include "ProEXR_About.h"

#include "Iex.h"

#include "ProEXR_UTF.h"

#include <assert.h>

#ifdef MAC_ENV
	#include <mach/mach.h>
#endif

#include <IlmThread.h>
#include <IlmThreadPool.h>



using namespace VRimg;
using namespace std;

#define gStuff				(globals->formatParamBlock)


#ifdef __PIWin__
#include "myDLLInstance.h"
extern HANDLE hDllInstance;

#define WIN_HANDLE	(HWND)((PlatformData *)gStuff->platformData)->hwnd
#define WIN_HANDLE_ARG	WIN_HANDLE##,
#else
#define WIN_HANDLE
#define WIN_HANDLE_ARG
#endif

extern SPBasicSuite * sSPBasic;

extern SPPluginRef gPlugInRef;



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
	
	globals->ps_in = NULL;
	globals->doc_in = NULL;
	globals->layer_list = NULL;
	
	globals->blackID = 0;
	globals->blackBuf = NULL;
	globals->blackHeight = 0;
	globals->blackRowbytes = 0;
	
	globals->whiteID = 0;
	globals->whiteBuf = NULL;
	globals->whiteHeight = 0;
	globals->whiteRowbytes = 0;
	
	gDone_Reg = false;
}


#pragma mark-


static void CreateXMPdescription(GPtr globals, const InputFile &file)
{
	Rope XMPtext = file.getXMPdescription();
	
	Handle xmp_handle = PINewHandle( XMPtext.size() );
	char *xmp_ptr = (char *)PILockHandle(xmp_handle, true);
	
	// turns out it's important that the handle not include the '\0' character at the end
	// Photoshop is sooooo picky
	memcpy(xmp_ptr, XMPtext.c_str(), XMPtext.size() );
	
	PIUnlockHandle(xmp_handle);
	
	PISetProp(kPhotoshopSignature, 'xmpd', 0, NULL, xmp_handle); // propXMP
}


static bool g_done_global_setup = false;

static void VRimgGlobalSetup()
{
	if(!g_done_global_setup)
	{
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
			
			ThreadPool::globalThreadPool().setNumThreads(hostInfo.max_cpus);
		#else // WIN_ENV
			SYSTEM_INFO systemInfo;
			GetSystemInfo(&systemInfo);

			ThreadPool::globalThreadPool().setNumThreads(systemInfo.dwNumberOfProcessors);
		#endif
		}
		
		g_done_global_setup = true;
	}
}


static OSErr myAllocateBuffer(GPtr globals, const int32 inSize, BufferID *outBufferID)
{
	*outBufferID = 0;
	
	if(gStuff->bufferProcs != NULL && gStuff->bufferProcs->numBufferProcs >= 4 && gStuff->bufferProcs->allocateProc != NULL)
		gResult = gStuff->bufferProcs->allocateProc(inSize, outBufferID);
	else
		gResult = memFullErr;

	return gResult;
}

static Ptr myLockBuffer(GPtr globals, const BufferID inBufferID, Boolean inMoveHigh)
{
	if(gStuff->bufferProcs != NULL && gStuff->bufferProcs->numBufferProcs >= 4 && gStuff->bufferProcs->lockProc != NULL)
		return gStuff->bufferProcs->lockProc(inBufferID, inMoveHigh);
	else
		return NULL;
}

static void myFreeBuffer(GPtr globals, const BufferID inBufferID)
{
	if(gStuff->bufferProcs != NULL && gStuff->bufferProcs->numBufferProcs >= 4 && gStuff->bufferProcs->freeProc != NULL)
		gStuff->bufferProcs->freeProc(inBufferID);
}


#pragma mark-


static void ReadBlack(GPtr globals, int channel)
{
	if(globals->blackID == 0 || globals->blackBuf == NULL)
	{
		globals->blackRowbytes = sizeof(float) * gStuff->imageSize32.h;
		globals->blackHeight = MAX(gStuff->tileHeight, 64);
		
		OSErr result = myAllocateBuffer(globals, globals->blackRowbytes * globals->blackHeight, &globals->blackID);
		
		if(globals->blackID != 0 && result == noErr)
		{
			globals->blackBuf = myLockBuffer(globals, globals->blackID, TRUE);
			
			float *pix = (float *)globals->blackBuf;
			
			int64 samples = gStuff->imageSize32.h * globals->blackHeight;
			
			while(samples--)
				*pix++ = 0.f;
		}
	}
	
 	gStuff->planeBytes = sizeof(float);
	gStuff->colBytes = sizeof(float);
	gStuff->rowBytes = globals->blackRowbytes;
	
	gStuff->loPlane = channel;
	gStuff->hiPlane = channel;
	 		
	gStuff->theRect.left = gStuff->theRect32.left = 0;
	gStuff->theRect.right = gStuff->theRect32.right = gStuff->imageSize32.h;
	
	gStuff->data = globals->blackBuf;
	
	
	int y = 0;
	
	while(y < gStuff->imageSize32.v && gResult == noErr)
	{
		int high_scanline = MIN(y + globals->blackHeight - 1, gStuff->imageSize32.v - 1);
		
		gStuff->theRect.top = gStuff->theRect32.top = y;
		gStuff->theRect.bottom = gStuff->theRect32.bottom = high_scanline + 1;
		
		gResult = AdvanceState();
		
		y = high_scanline + 1;
	}
}


static void ReadWhite(GPtr globals, int channel)
{
	if(globals->whiteID == 0 || globals->whiteBuf == NULL)
	{
		globals->whiteRowbytes = sizeof(float) * gStuff->imageSize32.h;
		globals->whiteHeight = MAX(gStuff->tileHeight, 64);
		
		OSErr result = myAllocateBuffer(globals, globals->whiteRowbytes * globals->whiteHeight, &globals->whiteID);
		
		if(globals->whiteID != 0 && result == noErr)
		{
			globals->whiteBuf = myLockBuffer(globals, globals->whiteID, TRUE);
			
			float *pix = (float *)globals->whiteBuf;
			
			int64 samples = gStuff->imageSize32.h * globals->whiteHeight;
			
			while(samples--)
				*pix++ = 1.f;
		}
	}
	
 	gStuff->planeBytes = sizeof(float);
	gStuff->colBytes = sizeof(float);
	gStuff->rowBytes = globals->whiteRowbytes;
	
	gStuff->loPlane = channel;
	gStuff->hiPlane = channel;
	 		
	gStuff->theRect.left = gStuff->theRect32.left = 0;
	gStuff->theRect.right = gStuff->theRect32.right = gStuff->imageSize32.h;
	
	gStuff->data = globals->whiteBuf;
	
	
	int y = 0;
	
	while(y < gStuff->imageSize32.v && gResult == noErr)
	{
		int high_scanline = MIN(y + globals->whiteHeight - 1, gStuff->imageSize32.v - 1);
		
		gStuff->theRect.top = gStuff->theRect32.top = y;
		gStuff->theRect.bottom = gStuff->theRect32.bottom = high_scanline + 1;
		
		gResult = AdvanceState();
		
		y = high_scanline + 1;
	}
}



#pragma mark-


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
	if( gResult != noErr || !isVRimgMagic(buf) )
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
	VRimgGlobalSetup();
	
	try{
	
	globals->ps_in = new IStreamPS(gStuff->dataFork, "Photoshop Input");
	
	globals->doc_in = new InputFile(*globals->ps_in);
	
	InputFile *img = globals->doc_in;
	

	// build a list of layers in order
	globals->layer_list = new vector<string>;
	
	bool have_rgb = false;
	bool have_alpha = false;
	
	const Header::LayerMap &layers = img->header().layers();
	
	for(Header::LayerMap::const_iterator i = layers.begin(); i != layers.end(); i++)
	{
		string channel_name = i->first;
		
		if(channel_name == "RGB color")
			have_rgb = true;
		else if(channel_name == "Alpha")
			have_alpha = true;
		else
			globals->layer_list->push_back(channel_name);
	}
	
	// making sure these appear at the top
	if(have_alpha)
		globals->layer_list->push_back("Alpha");
	
	if(have_rgb)
		globals->layer_list->push_back("RGB color");
	

	if(globals->layer_list->size() < 1)
		throw Iex::LogicExc("No layers found.");
		
	
	gStuff->PluginUsing32BitCoordinates = TRUE;
	gStuff->imageSize.h = gStuff->imageSize32.h = img->header().width();
	gStuff->imageSize.v = gStuff->imageSize32.v = img->header().height();
	
	gStuff->imageMode = plugInModeRGBColor;
	gStuff->depth = 32;
	gStuff->planes = 4;
		

	// pixel aspect ratio
	float h_dpi = 72.0f;
	
	gStuff->imageHRes = (h_dpi * 65536.0f) + 0.5f; // 72 dpi Fixed
	gStuff->imageVRes = ( (h_dpi * 65536.0f) * img->header().pixelAspectRatio() ) + 0.5f;
	

	// load everything into memory
	img->loadFromFile();
	

	// XMP description
	CreateXMPdescription(globals, *img);
	

	gStuff->layerData = globals->layer_list->size();
	
		
	gStuff->data = (void *)1; // just to keep it going
	
	}
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
	
	int channels_read = 0;
	
	InputFile *img = globals->doc_in;
	
	string layer_name = globals->layer_list->back();
	
	const Layer *layer = img->header().findLayer(layer_name);
	
	if(layer && layer->type == VRimg::FLOAT)
	{
		int width = img->header().width();
		int height = img->header().height();
		
		size_t pix_size = sizeof(float);
		
		size_t rowbytes = pix_size * layer->dimensions * width;
	
	
		BufferID bufferID = 0;
		void *buffer = NULL;
		
		gResult = myAllocateBuffer(globals, rowbytes * height, &bufferID);
		
		if(gResult == noErr && bufferID != 0)
		{
			gStuff->data = buffer = myLockBuffer(globals, bufferID, TRUE);
			
			img->copyLayerToBuffer(layer_name, buffer, rowbytes);
			
			
			gStuff->planeBytes = pix_size;
			gStuff->colBytes = pix_size * layer->dimensions;
			gStuff->rowBytes = rowbytes;
			
			gStuff->loPlane = 0;
			gStuff->hiPlane = layer->dimensions - 1;
					
			gStuff->theRect.left = gStuff->theRect32.left = 0;
			gStuff->theRect.right = gStuff->theRect32.right = width;
			
			gStuff->theRect.top = gStuff->theRect32.top = 0;
			gStuff->theRect.bottom = gStuff->theRect32.bottom = height;

			gResult = AdvanceState();
			
			channels_read += layer->dimensions;
			
			
			// if one channel, read it into G and B too
			if(layer->dimensions == 1)
			{
				for(int c = layer->dimensions; c < 3 && gResult == noErr; c++)
				{
					gStuff->hiPlane = gStuff->loPlane = c;
					
					gResult = AdvanceState();
					
					channels_read++;
				}
			}
			
			
			myFreeBuffer(globals, bufferID);
		}
	}
	
	if( TestAbort() )
	{
		gResult = userCanceledErr;
		gStuff->data = NULL;
		return;
	}

	while(channels_read < 4 && gResult == noErr)
	{
		if(channels_read == 3)
		{
			ReadWhite(globals, channels_read);
		}
		else
		{
			ReadBlack(globals, channels_read);
		}
		
		channels_read++;
	}

	
	}
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
	
	if(globals->layer_list)
	{
		delete globals->layer_list;
		globals->layer_list = NULL;
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
	
	if(globals->blackID != 0)
	{
		myFreeBuffer(globals, globals->blackID);
		
		globals->blackID = 0;
		globals->blackBuf = NULL;
		globals->blackHeight = 0;
		globals->blackRowbytes = 0;
	}
	
	if(globals->whiteID != 0)
	{
		myFreeBuffer(globals, globals->whiteID);
		
		globals->whiteID = 0;
		globals->whiteBuf = NULL;
		globals->whiteHeight = 0;
		globals->whiteRowbytes = 0;
	}
	
	}catch(...) {}
}


#pragma mark-


static void DoReadLayerStart(GPtr globals)
{

}


static uint16* myUTF16ptr(const char *str)
{
	unsigned int len = strlen(str) + 1;
	
	uint16 *out = (uint16 *)malloc( len * sizeof(uint16) );
	
	UTF8toUTF16(str, out, len);
		
	return out;
}


static inline uint32 GetBit(uint32 src, int b)
{
	return ((src & (1L << b)) >> b);
}

static inline uint32 BitPattern(uint32 src, int b1, int b2, int b3, int b4, int b5)
{
	return	(GetBit(src, b1) << 4) |
			(GetBit(src, b2) << 3) |
			(GetBit(src, b3) << 2) |
			(GetBit(src, b4) << 1) |
			(GetBit(src, b5) << 0);
}


typedef struct {
	float	r;
	float	g;
	float	b;
} Pixel;


static void ReadIntChannel(GPtr globals, int *buffer, int width, int height, size_t rowbytes)
{
	size_t pix_size = sizeof(float);
	
	const int dimensions = 3;
	
	size_t rgb_rowbytes = pix_size * dimensions * width;
	
	BufferID bufferID = 0;
	void *float_buffer = NULL;
	
	OSErr result = myAllocateBuffer(globals, rgb_rowbytes * height, &bufferID);
	
	if(result == noErr && bufferID != 0)
	{
		static const uint32 num_colors = 32; // 2 ^ color_bits (5 in this case)
		static const float max_val = num_colors - 1;
			
		static const unsigned int total_colors = num_colors * num_colors * num_colors;

		static vector<Pixel> colors;
		
		static bool initialized = false;
		
		if(!initialized)
		{
			colors.resize(total_colors);
			
			for(uint32 i=0; i < total_colors; i++)
			{
				uint32 r = BitPattern(i, 0, 3, 6,  9, 12);
				uint32 g = BitPattern(i, 1, 4, 7, 10, 13);
				uint32 b = BitPattern(i, 2, 5, 8, 11, 14);
				
				colors[i].r = (float)r / max_val;
				colors[i].g = (float)g / max_val;
				colors[i].b = (float)b / max_val;
			}
			
			initialized = true;
		}


		gStuff->data = float_buffer = myLockBuffer(globals, bufferID, TRUE);
		
		
		char *uint_row = (char *)buffer;
		char *float_row = (char *)float_buffer;
		
		for(int y=0; y < height; y++)
		{
			int *upix = (int *)uint_row;
			Pixel *pix = (Pixel *)float_row;
			
			for(int x=0; x < width; x++)
			{
				assert( (*upix % total_colors) >= 0 );
			
				*pix++ = colors[ *upix++ % total_colors ];
			}
			
			uint_row += rowbytes;
			float_row += rgb_rowbytes;
		}
		
		
		gStuff->planeBytes = pix_size;
		gStuff->colBytes = pix_size * dimensions;
		gStuff->rowBytes = rgb_rowbytes;
		
		gStuff->loPlane = 0;
		gStuff->hiPlane = dimensions - 1;
				
		gStuff->theRect.left = gStuff->theRect32.left = 0;
		gStuff->theRect.right = gStuff->theRect32.right = width;
		
		gStuff->theRect.top = gStuff->theRect32.top = 0;
		gStuff->theRect.bottom = gStuff->theRect32.bottom = height;

		gResult = AdvanceState();
		
		
		myFreeBuffer(globals, bufferID);
	}
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
	
	if( TestAbort() )
	{
		gResult = userCanceledErr;
		gStuff->data = NULL;
		return;
	}
	
	int channels_read = 0;
	
	InputFile *img = globals->doc_in;
	
	string layer_name = globals->layer_list->at( gStuff->layerData );
	
	const Layer *layer = img->header().findLayer(layer_name);
	
	if(layer)
	{
		int width = img->header().width();
		int height = img->header().height();
		
		size_t pix_size = (layer->type == VRimg::FLOAT ? sizeof(float) : sizeof(int));
		
		size_t rowbytes = pix_size * layer->dimensions * width;
	
	
		BufferID bufferID = 0;
		void *buffer = NULL;
		
		gResult = myAllocateBuffer(globals, rowbytes * height, &bufferID);
		
		if(gResult == noErr && bufferID != 0)
		{
			gStuff->data = buffer = myLockBuffer(globals, bufferID, TRUE);
			
			img->copyLayerToBuffer(layer_name, buffer, rowbytes);
			
			
			if(layer->type == VRimg::INT)
			{
				ReadIntChannel(globals, (int *)buffer, width, height, rowbytes);
				
				channels_read += 3;
			}
			else
			{
				gStuff->planeBytes = pix_size;
				gStuff->colBytes = pix_size * layer->dimensions;
				gStuff->rowBytes = rowbytes;
				
				gStuff->loPlane = 0;
				gStuff->hiPlane = layer->dimensions - 1;
						
				gStuff->theRect.left = gStuff->theRect32.left = 0;
				gStuff->theRect.right = gStuff->theRect32.right = width;
				
				gStuff->theRect.top = gStuff->theRect32.top = 0;
				gStuff->theRect.bottom = gStuff->theRect32.bottom = height;

				gResult = AdvanceState();
				
				
				channels_read += layer->dimensions;
				
				
				// if one channel, read it into G and B too
				if(layer->dimensions == 1)
				{
					for(int c = layer->dimensions; c < 3 && gResult == noErr; c++)
					{
						gStuff->hiPlane = gStuff->loPlane = c;
						
						gResult = AdvanceState();
						
						channels_read++;
					}
				}
			}
			
			
			myFreeBuffer(globals, bufferID);
		}
	}
	
	if( TestAbort() )
	{
		gResult = userCanceledErr;
		gStuff->data = NULL;
		return;
	}

	while(channels_read < 4 && gResult == noErr)
	{
		if(channels_read == 3)
		{
			ReadWhite(globals, channels_read);
		}
		else
		{
			ReadBlack(globals, channels_read);
		}
		
		channels_read++;
	}
	
	
	gStuff->layerName = myUTF16ptr( layer_name.c_str() );
	
	g_layer_names.push_back(gStuff->layerName); // remember pointers so we can free them later
	
	
	// progress
	PIUpdateProgress(gStuff->layerData + 1, globals->layer_list->size());

	}
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

// --------------------------------------------------
// PluginMain
//
// Entry Point
//

DLLExport MACPASCAL void VRimgMain (const short selector,
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

		switch(selector)
		{
			case formatSelectorReadPrepare:			DoReadPrepare(globals);				break;
			case formatSelectorReadStart:			DoReadStart(globals);				break;
			case formatSelectorReadContinue:		DoReadContinue(globals);			break;
			case formatSelectorReadFinish:			DoReadFinish(globals);				break;
			
			case formatSelectorFilterFile:			DoFilterFile(globals);				break;
			
			case formatSelectorReadLayerStart:		DoReadLayerStart(globals);			break;
			case formatSelectorReadLayerContinue:	DoReadLayerContinue(globals);		break;
			case formatSelectorReadLayerFinish:		DoReadLayerFinish(globals);			break;

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
		if ((Handle)*data != NULL)
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
