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


#include "ProEXR_Color.h"

#include "lcms2_internal.h"

#include <ImfStandardAttributes.h>
#include <ImfAcesFile.h>

#include "ProEXR_Version.h"
#include "iccProfileAttribute.h"

using namespace Imf;

// these stolen from an earlier version of Little CMS and private parts of the current version

static cmsBool
SetTextTags(cmsHPROFILE hProfile, const wchar_t* Description)
{
    cmsMLU *DescriptionMLU, *CopyrightMLU;
    cmsBool  rc = FALSE;
    cmsContext ContextID = cmsGetProfileContextID(hProfile);

    DescriptionMLU  = cmsMLUalloc(ContextID, 1);
    CopyrightMLU    = cmsMLUalloc(ContextID, 1);

    if (DescriptionMLU == NULL || CopyrightMLU == NULL) goto Error;

    if (!cmsMLUsetWide(DescriptionMLU,  "en", "US", Description)) goto Error;
    if (!cmsMLUsetWide(CopyrightMLU,    "en", "US", L"fnord software")) goto Error;

    if (!cmsWriteTag(hProfile, cmsSigProfileDescriptionTag,  DescriptionMLU)) goto Error;
    if (!cmsWriteTag(hProfile, cmsSigCopyrightTag,           CopyrightMLU)) goto Error;

    rc = TRUE;

Error:

    if (DescriptionMLU)
        cmsMLUfree(DescriptionMLU);
    if (CopyrightMLU)
        cmsMLUfree(CopyrightMLU);
    return rc;
}

static int
ReadICCXYZ(cmsHPROFILE hProfile, cmsTagSignature sig, cmsCIEXYZ *Value, cmsBool lIsFatal)
{
	cmsCIEXYZ *Tag = (cmsCIEXYZ*)cmsReadTag(hProfile, sig);
	
	if(Tag)
	{
		*Value = *Tag;
		return TRUE;
	}
	else
		return -1;
}

static cmsBool
cmsTakeColorants(cmsCIEXYZTRIPLE *Dest, cmsHPROFILE hProfile)
{
       if (ReadICCXYZ(hProfile, cmsSigRedColorantTag, &Dest -> Red, TRUE) < 0) return FALSE;
       if (ReadICCXYZ(hProfile, cmsSigGreenColorantTag, &Dest -> Green, TRUE) < 0) return FALSE;
       if (ReadICCXYZ(hProfile, cmsSigBlueColorantTag, &Dest -> Blue, TRUE) < 0) return FALSE;

       return TRUE;
}

static
cmsBool ComputeChromaticAdaptation(cmsMAT3* Conversion,
                                const cmsCIEXYZ* SourceWhitePoint,
                                const cmsCIEXYZ* DestWhitePoint,
                                const cmsMAT3* Chad)
{

    cmsMAT3 Chad_Inv;
    cmsVEC3 ConeSourceXYZ, ConeSourceRGB;
    cmsVEC3 ConeDestXYZ, ConeDestRGB;
    cmsMAT3 Cone, Tmp;


    Tmp = *Chad;
    if (!_cmsMAT3inverse(&Tmp, &Chad_Inv)) return FALSE;

    _cmsVEC3init(&ConeSourceXYZ, SourceWhitePoint -> X,
                             SourceWhitePoint -> Y,
                             SourceWhitePoint -> Z);

    _cmsVEC3init(&ConeDestXYZ,   DestWhitePoint -> X,
                             DestWhitePoint -> Y,
                             DestWhitePoint -> Z);

    _cmsMAT3eval(&ConeSourceRGB, Chad, &ConeSourceXYZ);
    _cmsMAT3eval(&ConeDestRGB,   Chad, &ConeDestXYZ);

    // Build matrix
    _cmsVEC3init(&Cone.v[0], ConeDestRGB.n[0]/ConeSourceRGB.n[0],    0.0,  0.0);
    _cmsVEC3init(&Cone.v[1], 0.0,   ConeDestRGB.n[1]/ConeSourceRGB.n[1],   0.0);
    _cmsVEC3init(&Cone.v[2], 0.0,   0.0,   ConeDestRGB.n[2]/ConeSourceRGB.n[2]);


    // Normalize
    _cmsMAT3per(&Tmp, &Cone, Chad);
    _cmsMAT3per(Conversion, &Chad_Inv, &Tmp);

    return TRUE;
}


static cmsBool
cmsAdaptationMatrix(cmsMAT3* r, const cmsMAT3* ConeMatrix, const cmsCIEXYZ* FromIll, const cmsCIEXYZ* ToIll)
{
    cmsMAT3 LamRigg   = {{ // Bradford matrix
        {{  0.8951,  0.2664, -0.1614 }},
        {{ -0.7502,  1.7135,  0.0367 }},
        {{  0.0389, -0.0685,  1.0296 }}
    }};

    if (ConeMatrix == NULL)
        ConeMatrix = &LamRigg;

    return ComputeChromaticAdaptation(r, FromIll, ToIll, ConeMatrix);
}


static cmsBool
cmsReadMediaWhitePoint(cmsCIEXYZ* Dest, cmsHPROFILE hProfile)
{
    cmsCIEXYZ* Tag;

    _cmsAssert(Dest != NULL);

    Tag = (cmsCIEXYZ*) cmsReadTag(hProfile, cmsSigMediaWhitePointTag);

    // If no wp, take D50
    if (Tag == NULL) {
        *Dest = *cmsD50_XYZ();
        return TRUE;
    }

    // V2 display profiles should give D50
    if (cmsGetEncodedICCversion(hProfile) < 0x4000000) {

        if (cmsGetDeviceClass(hProfile) == cmsSigDisplayClass) {
            *Dest = *cmsD50_XYZ();
            return TRUE;
        }
    }

    // All seems ok
    *Dest = *Tag;
    return TRUE;
}


static cmsBool
cmsAdaptMatrixFromD50(cmsMAT3 *r, cmsCIExyY *DestWhitePt)
{
        cmsCIEXYZ Dn;       
        cmsMAT3 Bradford;
        cmsMAT3 Tmp;

        cmsxyY2XYZ(&Dn, DestWhitePt);
        
        cmsAdaptationMatrix(&Bradford, NULL, cmsD50_XYZ(), &Dn);

        Tmp = *r;
        _cmsMAT3per(r, &Bradford, &Tmp);

        return TRUE;
}

#pragma mark-

static bool
sRGB_test(cmsCIExyYTRIPLE *colorants, cmsCIExyY *white_point, double leeway)
{
	// compare with known sRGB chromaticities
	return	fabs(colorants->Red.x	- 0.6400) < leeway &&
			fabs(colorants->Red.y	- 0.3300) < leeway &&
			fabs(colorants->Green.x	- 0.3000) < leeway &&
			fabs(colorants->Green.y	- 0.6000) < leeway &&
			fabs(colorants->Blue.x	- 0.1500) < leeway &&
			fabs(colorants->Blue.y	- 0.0600) < leeway &&
			fabs(white_point->x		- 0.3127) < leeway &&
			fabs(white_point->y		- 0.3290) < leeway;
}


static bool
ACES_test(const Chromaticities &chrm, float leeway)
{
	const Chromaticities &aces = acesChromaticities();
	
	return (fabs(chrm.red.x		- aces.red.x)	< leeway &&
			fabs(chrm.red.y		- aces.red.y)	< leeway &&
			fabs(chrm.green.x	- aces.green.x)	< leeway &&
			fabs(chrm.green.y	- aces.green.y)	< leeway &&
			fabs(chrm.blue.x	- aces.blue.x)	< leeway &&
			fabs(chrm.blue.y	- aces.blue.y)	< leeway &&
			fabs(chrm.white.x	- aces.white.x)	< leeway &&
			fabs(chrm.white.y	- aces.white.y)	< leeway);
}


void
initializeEXRcolor()
{
	static bool initialized = false;
	
	if(!initialized)
	{
		iccProfileAttribute::registerAttributeType();
	}
}

static inline cmsUInt16Number
SwapEndian(cmsUInt16Number in)
{
#ifdef __ppc__
	return in;
#else
	return ( (in >> 8) | (in << 8) );
#endif
}

void
GetEXRcolor(const Header &header, void **icc, size_t *prof_len)
{
	*icc = NULL;
	*prof_len = 0;
	
	if( hasICCprofile(header) )
	{
		const iccProfileAttribute &attr = ICCprofileAttribute(header);
		
		size_t len = 0;
		const void *prof = attr.value(len);
		
		*icc = malloc(len);
		memcpy(*icc, prof, len);
		*prof_len = len;
	}
	else if( hasChromaticities(header) )
	{
		const Chromaticities &chrm = chromaticities(header);

		cmsCIExyY white_point;
		cmsCIExyYTRIPLE colorants;
		
		// values from the file							   							   
		colorants.Red.x = chrm.red.x;			colorants.Red.y = chrm.red.y;
		colorants.Green.x = chrm.green.x;		colorants.Green.y = chrm.green.y;
		colorants.Blue.x = chrm.blue.x;			colorants.Blue.y = chrm.blue.y;
		white_point.x = chrm.white.x;			white_point.y = chrm.white.y;

		colorants.Red.Y = colorants.Green.Y = colorants.Blue.Y = white_point.Y = 1.0;

		// linear gamma, of course
		cmsToneCurve *Gamma1[3];
		//double Params[] = {1.0}; // gamma 1.0
		Gamma1[0] = Gamma1[1] = Gamma1[2] = cmsBuildGamma(NULL, 1.0);
		//Gamma1[0]->nEntries = Gamma1[1]->nEntries = Gamma1[2]->nEntries = 0; // this makes a linear table (no entries)

		
		cmsHPROFILE myProfile = cmsCreateRGBProfile(&white_point, &colorants, Gamma1);

		cmsFreeToneCurve(Gamma1[0]);
		
		if(myProfile)
		{
			std::string prof_desc("");
			
		#define COLOR_SPACE_NAME_KEY "chromaticitiesName"	
			const StringAttribute *chrom_name = header.findTypedAttribute<StringAttribute>(COLOR_SPACE_NAME_KEY);
			
			if(chrom_name)
			{
				prof_desc = chrom_name->value();
				prof_desc += " Chromaticities";
			}
			else if( sRGB_test(&colorants, &white_point, 0.01) )
			{
				prof_desc = "sRGB Chromaticities";
			}
			else if( ACES_test(chrm, 0.0001) )
			{
				prof_desc = "ACES Chromaticities";
			}
			else
				prof_desc = "OpenEXR Chromaticities";
			
			
			// VERY convoluted way to get our description into the profile with lcms2
			wchar_t *desc_w = new wchar_t[prof_desc.size() + 1];
			
			const char *c = prof_desc.c_str();
			wchar_t *w = desc_w;
			
			for(int i=0; i < prof_desc.size() + 1; i++)
				*w++ = *c++; // poor-man's UTF8 to UTF16/32
			
			SetTextTags(myProfile, desc_w);
			
			delete [] desc_w;
			

			cmsUInt32Number bytes_needed;
			
			if( cmsSaveProfileToMem(myProfile, NULL, &bytes_needed) )
			{
				*icc = malloc(bytes_needed);
				
				cmsSaveProfileToMem(myProfile, *icc, &bytes_needed);
				
				*prof_len = bytes_needed;
				
				// The profile written to memory is given a timestamp, which will make
				// profiles that should be identical slightly different, which will
				// mess with AE.  We're going to hard code our own time instead.
				
				cmsICCHeader *icc_header = (cmsICCHeader *)*icc;
				
				icc_header->date.year	= SwapEndian(2012);
				icc_header->date.month	= SwapEndian(12);
				icc_header->date.day	= SwapEndian(12);
				icc_header->date.hours	= SwapEndian(12);
				icc_header->date.minutes= SwapEndian(12);
				icc_header->date.seconds= SwapEndian(12);
			}
			
			cmsCloseProfile(myProfile);
		}
	}
}


void
SetEXRcolor(void *icc, size_t prof_len, Header &header)
{
	// save chromaticities based on ICC profile
	if(icc != NULL)
	{
		Chromaticities chrom;

		cmsHPROFILE iccH = cmsOpenProfileFromMem(icc, prof_len);
	
		// all that just to get a profile into memory and point lcms to it
		if(iccH)
		{
			cmsCIEXYZ white_point;
			cmsCIEXYZTRIPLE colorants;

			if( cmsReadMediaWhitePoint(&white_point, iccH) && cmsTakeColorants(&colorants, iccH) )
			{
				cmsCIExyYTRIPLE xyColorants;
				cmsCIExyY xyWhitePoint;
				
				// make a big matrix so we can run cmsAdaptMatrixFromD50() on it
				// before grabbing the chromaticities 
				cmsMAT3 MColorants;
				
				MColorants.v[0].n[0] = colorants.Red.X;
				MColorants.v[1].n[0] = colorants.Red.Y;
				MColorants.v[2].n[0] = colorants.Red.Z;
				
				MColorants.v[0].n[1] = colorants.Green.X;
				MColorants.v[1].n[1] = colorants.Green.Y;
				MColorants.v[2].n[1] = colorants.Green.Z;
				
				MColorants.v[0].n[2] = colorants.Blue.X;
				MColorants.v[1].n[2] = colorants.Blue.Y;
				MColorants.v[2].n[2] = colorants.Blue.Z;
				
			
				cmsXYZ2xyY(&xyWhitePoint, &white_point);
				
				// apparently I have to do this to get the right YXZ values
				// for my chromaticities - anyone know why?
				cmsAdaptMatrixFromD50(&MColorants, &xyWhitePoint);
				
				// set the colrants back and convert to xyY
				colorants.Red.X = MColorants.v[0].n[0];
				colorants.Red.Y = MColorants.v[1].n[0];
				colorants.Red.Z = MColorants.v[2].n[0];

				colorants.Green.X = MColorants.v[0].n[1];
				colorants.Green.Y = MColorants.v[1].n[1];
				colorants.Green.Z = MColorants.v[2].n[1];

				colorants.Blue.X = MColorants.v[0].n[2];
				colorants.Blue.Y = MColorants.v[1].n[2];
				colorants.Blue.Z = MColorants.v[2].n[2];
				
				cmsXYZ2xyY(&xyColorants.Red, &colorants.Red);
				cmsXYZ2xyY(&xyColorants.Green, &colorants.Green);
				cmsXYZ2xyY(&xyColorants.Blue, &colorants.Blue);
				
				
				// put them in our structure
				chrom.red.x = xyColorants.Red.x;		chrom.red.y = xyColorants.Red.y;
				chrom.green.x = xyColorants.Green.x;	chrom.green.y = xyColorants.Green.y;
				chrom.blue.x = xyColorants.Blue.x;		chrom.blue.y = xyColorants.Blue.y;
				chrom.white.x = xyWhitePoint.x;			chrom.white.y = xyWhitePoint.y;
				
				
				// chromaticity attribute
				addChromaticities(header, chrom);
				
				
				// get the profile description
				char name[256];
				cmsUInt32Number namelen = cmsGetProfileInfoASCII(iccH, cmsInfoDescription,
																"en", cmsNoCountry, name, 255);
				if(namelen)
				{
					std::string desc(name);
					
					if(desc.size() > 9 && (std::string(" (linear)") == desc.substr(desc.size() - 9) ) )
						desc.resize(desc.size() - 9); // take "(linear)" off the end
					if(desc.size() > 21 && (std::string(" (Linear RGB Profile)") == desc.substr(desc.size() - 21) ) )
						desc.resize(desc.size() - 21); // take "(Linear RGB Profile)" off the end
					if(desc.size() > 15 && (std::string(" Chromaticities") == desc.substr(desc.size() - 15) ) )
						desc.resize(desc.size() - 15); // take "Chromaticities" off the end
					
					// chromaticity name attribute
					if(desc != "OpenEXR")
						header.insert(COLOR_SPACE_NAME_KEY, StringAttribute( desc ) );
				}
			}
			
			cmsCloseProfile(iccH);
		}
		
		// iccProfile attribute
		addICCprofile(header, icc, prof_len);
	}
}
