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

#ifndef __ProEXRTerminology_H__
#define __ProEXRTerminology_H__


//-------------------------------------------------------------------------------
//	Definitions -- Scripting keys
//-------------------------------------------------------------------------------

// output
#define keyEXRcompression		'exrC'
#define keyEXRfloat				'exrF'
#define keyEXRlumichrom			'exrY'
#define keyEXRcomposite			'exrT'
#define keyEXRhidden			'exrH'
#define keyEXRalpha				'exrL'

// compression enum
#define typeCompression			'enuC'

#define compressionNone			'cNon'
#define compressionRLE			'cRLE'
#define compressionZip			'cZip'
#define compressionZip16		'cZ16'
#define compressionPiz			'cPiz'
#define compressionPXR24		'cP24'
#define compressionB44			'cB44'
#define compressionB44A			'cB4A'
#define compressionDWAA			'cDWA'
#define compressionDWAB			'cDWB'

// alpha enum (ProEXR EZ)
#define typeAlphaChannel		'enuT'

#define alphaChannelNone		'alfN'
#define alphaChannelTransparency 'alfT'
#define alphaChannelChannel		'alfC'


// input
#define keyEXRalphamode         'exrM'
#define keyEXRunmult            'exrU'
#define keyEXRignore            'exrI'

// alpha mode enum
#define typeAlphaMode           'enuA'

#define alphamodeTransparency   'aTrn'
#define alphamodeSeperate       'aSep'

// export
#define keyIn					'In  '


#endif // __ProEXRTerminology_H__
