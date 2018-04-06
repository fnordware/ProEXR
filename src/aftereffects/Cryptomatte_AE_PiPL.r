//
//	EXtractoR
//		by Brendan Bolles <brendan@fnordware.com>
//
//	extract float channels
//
//	Part of the fnord OpenEXR tools
//		http://www.fnordware.com/OpenEXR/
//
//

#include "AEConfig.h"

#include "AE_EffectVers.h"

#ifndef AE_OS_WIN
	#include "AE_General.r"
#endif

resource 'PiPL' (16000) {
	{	/* array properties: 12 elements */
		/* [1] */
		Kind {
			AEEffect
		},
		/* [2] */
		Name {
			"Cryptomatte"
		},
		/* [3] */
		Category {
	#ifdef NO_ZSTRINGS
            "3D Channel"
	#else
            "$$$/AE/Effect/Category/3DChannel=3D Channel" // Adobe localization
	#endif
		},
		
#ifdef AE_OS_WIN
	#ifdef AE_PROC_INTELx64
		CodeWin64X86 {"PluginMain"},
	#else
		CodeWin32X86 {"PluginMain"},
	#endif	
#else
	#ifdef AE_PROC_INTELx64
		CodeMacIntel64 {"PluginMain"},
	#else
		CodeMachOPowerPC {"PluginMain"},
		CodeMacIntel32 {"PluginMain"},
	#endif
#endif		/* [6] */
		AE_PiPL_Version {
			2,
			0
		},
		/* [7] */
		AE_Effect_Spec_Version {
			PF_PLUG_IN_VERSION,
			PF_PLUG_IN_SUBVERS
		},
		/* [8] */
		AE_Effect_Version {
			820736
		},
		/* [9] */
		AE_Effect_Info_Flags {
			0
		},
		/* [10] */
#ifdef AE_OS_MAC
#define RESOURCE_FLAG 0
#else
#define RESOURCE_FLAG 1
#endif
		AE_Effect_Global_OutFlags {
			33588288 + RESOURCE_FLAG
		},
		AE_Effect_Global_OutFlags_2 {
			5128
		},
		/* [11] */
		AE_Effect_Match_Name {
			"Cryptomatte"
		},
		/* [12] */
		AE_Reserved_Info {
			0
		}
	}
};



#ifdef AE_OS_MAC

#include "MacTypes.r"

#define NAME				"EXtractoR"
#define VERSION_STRING		"1.8"
resource 'vers' (1, NAME " Version", purgeable)
{
	5, 0x50, final, 0, verUs,
	VERSION_STRING,
	VERSION_STRING
	"\n©2007-2013 fnord"
};

resource 'vers' (2, NAME " Version", purgeable)
{
	5, 0x50, final, 0, verUs,
	VERSION_STRING,
	"by Brendan Bolles"
};

#endif

