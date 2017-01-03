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

#include "ProEXR.h"

#include "ProEXR_Attributes.h"
#include "ProEXR_Color.h"
#include "ProEXR_About.h"

#include <ImfStdIO.h>
#include <ImfStandardAttributes.h>



#define gStuff				(globals->exportParamBlock)



using namespace Imf;
using namespace Imath;
using namespace std;

// shared function prototypes

void GlobalSetup(void);

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


static void ImportXMPdescription(ExGPtr globals, Header &head)
{
	Handle xmp_handle = NULL;

	PIGetProp(kPhotoshopSignature, 'xmpd', 0, NULL, &xmp_handle); // propXMP
	
	if(xmp_handle != NULL)
	{
		const size_t xmp_size = PIGetHandleSize(xmp_handle);
		const char *xmp_text = PILockHandle(xmp_handle, true);
		
		if(xmp_text != NULL && xmp_size > 0)
		{
			const int width = gStuff->documentInfo->bounds.right - gStuff->documentInfo->bounds.left;
			const int height = gStuff->documentInfo->bounds.bottom - gStuff->documentInfo->bounds.top;
			
			ParseXMPdescription(head, width, height, xmp_text, xmp_size);
		}
		
		PIUnlockHandle(xmp_handle);
	}
}

static void SetEXRcolor(ExGPtr globals, Header &head)
{
	if(gStuff->iCCprofileData && gStuff->iCCprofileSize)
	{
		void *ps_profile = PILockHandle(gStuff->iCCprofileData, true);
		
		SetEXRcolor(ps_profile, gStuff->iCCprofileSize, head);
		
		PIUnlockHandle(gStuff->iCCprofileData);
	}
}

static void ReportException(ExGPtr globals, Iex::BaseExc &e)
{
	Str255 p_str;
	
	string err_str = e;
	int size = MIN(255, err_str.size());
	
	p_str[0] = size;
	strncpy((char *)&p_str[1], err_str.c_str(), size);
	
	PIReportError(p_str);
	gResult = errReportString;
}



static void DoExAbout(AboutRecordPtr aboutP)
{
#ifdef __PIMac__
	ProEXR_About();
#else
	ProEXR_About((HWND)((PlatformData *)aboutP->platformData)->hwnd);
#endif
}

static void InitGlobals (Ptr globalPtr)
{	
	// create "globals" as a our struct global pointer so that any
	// macros work:
	ExGPtr globals = (ExGPtr)globalPtr;
	
	gAliasHandle = nil; // no handle, yet
	
	gOptions.compression_type 		= Imf::PIZ_COMPRESSION;
	gOptions.float_not_half 		= FALSE;
	gOptions.luminance_chroma		= FALSE;
	gOptions.layer_composite		= TRUE;

} // end InitGlobals


static void DoPrepare(ExGPtr globals)
{

}

static void DoStart(ExGPtr globals)
{
	char file_path[256];
	file_path[0] = '\0';
		
	GlobalSetup();
	
	// read scripting
	Boolean do_dialogs = ReadScriptParamsOnWrite(globals);
	
	if(!do_dialogs && gAliasHandle)
	{
		// assuming something must have been put in gAliasHandle
	#ifdef __PIMac__
		FSRef fsref;
		Boolean wasChanged;
		
		if(noErr == FSResolveAlias(NULL, gAliasHandle, &fsref, &wasChanged))
		{
			FSRefMakePath(&fsref, (UInt8 *)file_path, 255);
		}
		else
		{
			gResult = paramErr;
			return;
		}
	#else
		strncpy(file_path, PILockHandle(gAliasHandle, true), 255);
		PIUnlockHandle(gAliasHandle);
	#endif
	}
	else
	{
		// get the doc name
		Handle doc_titleH = NULL;
		PIGetProp(kPhotoshopSignature, 'titl', 0, NULL, &doc_titleH); // propTitle
		
		if(doc_titleH)
		{
			size_t len = PIGetHandleSize(doc_titleH);

			if(len < 255)
			{
				Ptr doc_title = PILockHandle(doc_titleH, true);
				
				memcpy(file_path, doc_title, len);

				file_path[len] = '\0';
				
				// fix the extension
				if( strlen(file_path) > 4 && file_path[ strlen(file_path) - 4 ] == '.' )
				{
					file_path[ strlen(file_path) - 3 ] = 'e';
					file_path[ strlen(file_path) - 2 ] = 'x';
					file_path[ strlen(file_path) - 1 ] = 'r';
				}
				else
					strcat(file_path, ".exr");
			}

			PIDisposeHandle(doc_titleH);
		}
	}
	
		
	// bring up dialogs
	if( !do_dialogs || ProEXR_SaveAs(WIN_HANDLE_ARG file_path, 255) )
	{
		int total_layers = (gStuff->documentInfo->layersDescriptor &&
								gStuff->documentInfo->layersDescriptor->next) ? 2 : 1; // just a quick guess

		// look for hidden layers
		bool hidden_layers = false;
		ReadLayerDesc *search_layer = gStuff->documentInfo->layersDescriptor;
		
		while(search_layer && !hidden_layers)
		{
			if(!search_layer->isVisible)
				hidden_layers = true;
				
			search_layer = search_layer->next;
		}


		// options dialog
		if( !do_dialogs || ProEXR_Write_Dialog(WIN_HANDLE_ARG &gOptions, total_layers, hidden_layers) )
		{
			try{
		
			auto_ptr<OStream> out_stream;
			
			if(gOptions.layer_composite)
				out_stream.reset( new StdOFStream(file_path) );
			else
				out_stream.reset( new OStreamPS(0, "Bogus File") ); // make sure you don't try to write the actual doc!
				

			PS_callbacks ps_calls = { globals->result, gStuff->channelPortProcs,
									gStuff->abortProc, gStuff->progressProc, gStuff->handleProcs, gStuff->advanceState,
									&gStuff->data, &gStuff->theRect, &gStuff->theRect32, &gStuff->loPlane, &gStuff->hiPlane,
									NULL, &gStuff->rowBytes, NULL };
									
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
			
			
			// make a copy before the layered doc gets it
			Header layerfile_header = header;
			
			
			// set up a big doc object
			Imf::PixelType pixelType = gOptions.float_not_half ? Imf::FLOAT : Imf::HALF;
		
			const bool do_layers = (gStuff->documentInfo->layersDescriptor != NULL);

			ProEXRdoc_writePS out_file(*out_stream, header, pixelType, do_layers, gOptions.hidden_layers, &ps_calls, gStuff->documentInfo, gStuff->documentInfo->mergedTransparency);
			
			out_file.loadFromPhotoshop();
			
			
			// now write each layer
			string path_base(file_path);
			string extension("");
			
			// parse path extension
			if(path_base.find_last_of('.') >= path_base.size() - 5)
			{
				extension = path_base.substr(path_base.find_last_of('.'), path_base.size() - path_base.find_last_of('.'));
				path_base.resize(path_base.size() - extension.size());
			}

			if(out_file.layers().size() > 1)
			{
				assert(do_layers);

				for(int i=0; i < out_file.layers().size(); i++)
				{
					const ProEXRlayer_writePS &ps_layer = dynamic_cast<const ProEXRlayer_writePS &>( *out_file.layers().at(i) );

					string path = path_base + "_" + ps_layer.name() + extension;
					
					StdOFStream layer_out_stream(path.c_str());
					
					ps_layer.writeLayerFile(layer_out_stream, layerfile_header, pixelType);
				}
			}
			
			
			// write layered file
			if(gOptions.layer_composite || !do_layers)
			{
				if(out_file.layers().size() > 1) // otherwise this layer will already be here
				{
					assert(do_layers);

					out_file.addMainLayer(gStuff->documentInfo->mergedCompositeChannels,
												gStuff->documentInfo->mergedTransparency,
												gStuff->documentInfo->mergedTransparency,
												NULL, pixelType);
				}
				
				out_file.writeFile();
			}
			
			}		
			catch(AbortExc)
			{
				gResult = userCanceledErr;
			}
			catch(PhotoshopExc) {} // leave error as-is
			catch(Iex::BaseExc &e)
			{
				ReportException(globals, e);
			}
			catch(...) { gResult = formatBadParameters; }
			

			if(gResult == noErr && gAliasHandle == NULL)
			{
			#ifdef __PIMac__
				ProEXR_MakeAliasFromPath(file_path, &gAliasHandle);
			#else
				// Windows
				if(gAliasHandle)
					PISetHandleSize(gAliasHandle, (strlen(file_path)+1) * sizeof(char));
				else	
					gAliasHandle = PINewHandle( (strlen(file_path)+1) * sizeof(char));
					
				char *path_buf = PILockHandle(gAliasHandle, true);
				strcpy(path_buf, file_path);
				PIUnlockHandle(gAliasHandle);
			#endif
			}
			
			if(gResult == noErr)
				WriteScriptParamsOnWrite(globals);
		}
		else
			gResult = userCanceledErr; 
	}
	else
		gResult = userCanceledErr; 
	
	
	if(gAliasHandle)
	{
		PIDisposeHandle((Handle)gAliasHandle);
		gAliasHandle = NULL;
	}
}


static void DoContinue(ExGPtr globals)
{

}


static void DoFinish(ExGPtr globals)
{

}


DLLExport MACPASCAL void ExPluginMain (const short selector,
						             ExportRecord *exportParamBlock,
						             entryData *data,
						             short *result)
{
	//---------------------------------------------------------------------------
	//	(1) Check for about box request.
	//
	// 	The about box is a special request; the parameter block is not filled
	// 	out, none of the callbacks or standard data is available.  Instead,
	// 	the parameter block points to an AboutRecord, which is used
	// 	on Windows.
	//---------------------------------------------------------------------------

	if (selector == exportSelectorAbout)
	{
		sSPBasic = ((AboutRecord*)exportParamBlock)->sSPBasic;

	#ifdef __PIWin__
		if(hDllInstance == NULL)
			hDllInstance = GetDLLInstance(NULL);
	#endif

		
		DoExAbout((AboutRecordPtr)exportParamBlock);
	}
	else
	{ // do the rest of the process as normal:

		sSPBasic = exportParamBlock->sSPBasic;

	#ifdef __PIWin__
		if(hDllInstance == NULL)
			hDllInstance = GetDLLInstance((SPPluginRef)exportParamBlock->plugInRef);
	#endif

		Ptr globalPtr = NULL;		// Pointer for global structure
		ExGPtr globals = NULL; 		// actual globals

		//-----------------------------------------------------------------------
		//	(2) Allocate and initalize globals.
		//
		// 	AllocateGlobals requires the pointer to result, the pointer to the
		// 	parameter block, a pointer to the handle procs, the size of our local
		// 	"Globals" structure, a pointer to the long *data, a Function
		// 	Proc (FProcP) to the InitGlobals routine.  It automatically sets-up,
		// 	initializes the globals (if necessary), results result to 0, and
		// 	returns with a valid pointer to the locked globals handle or NULL.
		//-----------------------------------------------------------------------
		
		globalPtr = AllocateGlobals ((allocateGlobalsPointer)result,
									 (allocateGlobalsPointer)exportParamBlock,
									 ((ExportRecordPtr)exportParamBlock)->handleProcs,
									 sizeof(ExGlobals),
						 			 data,
						 			 InitGlobals);
		
		if (globalPtr == NULL)
		{ // Something bad happened if we couldn't allocate our pointer.
		  // Fortunately, everything's already been cleaned up,
		  // so all we have to do is report an error.
		  
		  *result = memFullErr;
		  return;
		}
		
		// Get our "globals" variable assigned as a Global Pointer struct with the
		// data we've returned:
		globals = (ExGPtr)globalPtr;

		//-----------------------------------------------------------------------
		//	(3) Dispatch selector.
		//-----------------------------------------------------------------------

		switch (selector)
		{
			case exportSelectorPrepare:
				DoPrepare(globals);
				break;
			case exportSelectorStart:
				DoStart(globals);
				break;
			case exportSelectorContinue:
				DoContinue(globals);
				break;
			case exportSelectorFinish:
				DoFinish(globals);
				break;
		}
							
		//-----------------------------------------------------------------------
		//	(4) Unlock data, and exit resource.
		//
		//	Result is automatically returned in *result, which is
		//	pointed to by gResult.
		//-----------------------------------------------------------------------	
		
		// unlock handle pointing to parameter block and data so it can move
		// if memory gets shuffled:
		if ((Handle)*data != NULL)
			PIUnlockHandle((Handle)*data);
	
	} // about selector special		
	
} // end ExPluginMain

