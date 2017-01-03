
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

#include "ProEXR_Version.h"

#define plugInName			"ProEXR Deep"
#define plugInCopyrightYear	ProEXR_Copyright_Year
#define plugInDescription ProEXR_Description
#define VersionString 	ProEXR_Version_String
#define ReleaseString	ProEXR_Build_Date_Manual
#define CurrentYear		ProEXR_Build_Year

//-------------------------------------------------------------------------------
//	Definitions -- Required by other resources in this rez file.
//-------------------------------------------------------------------------------

// Dictionary (aete) resources:

#define vendorName			"fnord"
#define plugInAETEComment 	"†berEXR format"

#define plugInSuiteID		'EXRd'
#define plugInClassID		'dEXR'
#define plugInEventID		typeNull // must be this

//-------------------------------------------------------------------------------
//	Set up included files for Macintosh and Windows.
//-------------------------------------------------------------------------------

#include "PIDefines.h"

#ifdef __PIMac__
	#include "Types.r"
	#include "SysTypes.r"
	#include "PIGeneral.r"
	//#include "PIUtilities.r"
	//#include "DialogUtilities.r"
#elif defined(__PIWin__)
	#include "PIGeneral.h"
	//#include "PIUtilities.r"
	//#include "WinDialogUtils.r"
#endif

#ifndef ResourceID
	#define ResourceID		16000
#endif


#include "PITerminology.h"
#include "PIActions.h"

#include "ProEXR_Terminology.h"	// Terminology for plug-in.

//-------------------------------------------------------------------------------
//	PiPL resource
//-------------------------------------------------------------------------------

resource 'PiPL' (ResourceID, plugInName " PiPL", purgeable)
{
    {
		Kind { ImageFormat },
		Name { plugInName },
		
		Version { (latestFormatVersion << 16) | latestFormatSubVersion },

		#ifdef __PIMac__
			#if (defined(__x86_64__))
				CodeMacIntel64 { "PluginMain" },
			#endif
			#if (defined(__i386__))
				CodeMacIntel32 { "PluginMain" },
			#endif
			#if (defined(__ppc__))
				CodeMachOPowerPC { 0, 0, "PluginMain" },
			#endif
		#else
			#if defined(_WIN64)
				CodeWin64X86 { "PluginMain" },
			#else
				CodeWin32X86 { "PluginMain" },
			#endif
		#endif
	
		HasTerminology { plugInClassID, plugInEventID, ResourceID, vendorName " " plugInName },
		
		SupportedModes
		{
			noBitmap, doesSupportGrayScale,
			noIndexedColor, doesSupportRGBColor,
			noCMYKColor, noHSLColor,
			noHSBColor, noMultichannel,
			noDuotone, noLABColor
		},
			
		EnableInfo { "in (PSHOP_ImageMode, RGB96Mode)" },
		
		FmtFileType { 'EXR ', '8BIM' },
		WriteExtensions { { 'EXR ' } },
		FilteredExtensions { { 'EXR ' } },
		FormatFlags { fmtSavesImageResources, //(by saying we do, PS won't store them, which is causing problems)
		              fmtCannotRead, 
					  fmtCanWrite, 
					  fmtCanWriteIfRead, 
					  fmtCanWriteTransparency,
					  fmtCannotCreateThumbnail },
		PlugInMaxSize { 2147483647, 2147483647 }, // Photoshop 8
		FormatMaxSize { { 32767, 32767 } },
		FormatMaxChannels { {   0, 0, 0, 5, 0, 0, 
							   0, 0, 0, 0, 0, 0 } },
		FormatICCFlags { 	iccCannotEmbedGray,
							iccCannotEmbedIndexed,
							iccCanEmbedRGB,
							iccCannotEmbedCMYK },
							
        //XMPWrite{ }, XMPRead{ } // hopefully prevents Photoshop from writing XMP to a resource
	}
};



//-------------------------------------------------------------------------------
//	Dictionary (scripting) resource
//-------------------------------------------------------------------------------

resource 'aete' (ResourceID, plugInName " dictionary", purgeable)
{
	1, 0, english, roman,									/* aete version and language specifiers */
	{
		vendorName,											/* vendor suite name */
		"Extended EXR format",								/* optional description */
		plugInSuiteID,										/* suite ID */
		1,													/* suite code, must be 1 */
		1,													/* suite level, must be 1 */
		{},													/* structure for filters */
		{													/* non-filter plug-in class here */
			//vendorName " simpleFormat",
			plugInName,										/* unique class name */
			plugInClassID,									/* class ID, must be unique or Suite ID */
			plugInAETEComment,								/* optional description */
			{												/* define inheritance */
				"$$$/private/AETE/Inheritance=<Inheritance>",							/* must be exactly this */
				keyInherits,								/* must be keyInherits */
				classFormat,								/* parent: Format, Import, Export */
				"parent class format",						/* optional description */
				flagsSingleProperty,						/* if properties, list below */
							
				"Compression",
				keyEXRcompression,
				typeEnumerated,
				"EXR compression used",
				flagsSingleProperty,
				
				"32-bit float",
				keyEXRfloat,
				typeBoolean,
				"32-bit instead of 16-bit float",
				flagsSingleProperty,

				"Luminance / Chroma",
				keyEXRlumichrom,
				typeBoolean,
				"Luminance / Chroma and chroma subsampling",
				flagsSingleProperty,

				"Alpha Channel",
				keyEXRalpha,
				typeEnumerated,
				"Source of the alpha channel",
				flagsSingleProperty,

				"Alpha Mode",
                keyEXRalphamode,
                typeEnumerated,
                "The way to treat Alpha when the file is read",
                flagsSingleProperty,
                
                "UnMultiply",
                keyEXRunmult,
                typeBoolean,
                "UnMultiply the RGB pixels by the Alpha",
                flagsSingleProperty,
                
                "Ignore Layer Attribute",
                keyEXRignore,
                typeBoolean,
                "Ignore any ProEXR layer information embedded in the file",
                flagsSingleProperty
                /* no properties */
			},
			{}, /* elements (not supported) */
			/* class descriptions */
		},
		{}, /* comparison ops (not supported) */
		{	/* any enumerations */
			typeCompression,
			{
				"None",
				compressionNone,
				"No compression",
				
				"RLE",
				compressionRLE,
				"RLE compression",
				
				"Zip",
				compressionZip,
				"Zip compression",
				
				"Zip16",
				compressionZip16,
				"Zip16 compression",
				
				"Piz",
				compressionPiz,
				"Piz compression",
				
				"PXR24",
				compressionPXR24,
				"PXR24 compression",
				
				"B44",
				compressionB44,
				"B44 Compression",

				"B44A",
				compressionB44A,
				"B44A Compression"
			},
            
            typeAlphaMode,
            {
                "Transparency",
                alphamodeTransparency,
                "Create transparent layers with Alpha",
                
                "Seperate",
                alphamodeSeperate,
                "Load Alphas in seperate channels"
            },
			
			typeAlphaChannel,
			{
                "None",
                alphaChannelNone,
                "No alpha channel",

                "Transparency",
                alphaChannelTransparency,
                "Get alpha from Transparency",

                "Channel",
                alphaChannelChannel,
                "Get alpha from channels palette",
			}
		}
	}
};





#ifdef __PIMac__

//-------------------------------------------------------------------------------
//	Version 'vers' resources.
//-------------------------------------------------------------------------------

resource 'vers' (1, plugInName " Version", purgeable)
{
	5, 0x50, final, 0, verUs,
	VersionString,
	VersionString
	"\n©" plugInCopyrightYear " fnord"
};

resource 'vers' (2, plugInName " Version", purgeable)
{
	5, 0x50, final, 0, verUs,
	VersionString,
	"by Brendan Bolles"
};


#endif // __PIMac__


