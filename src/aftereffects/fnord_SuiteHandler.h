
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
//
//	A multi-version SuiteHandler.  Inspired by the the Adobe SDK verison.
//

#ifndef _FNORD_SUITEHANDLER_H_
#define _FNORD_SUITEHANDLER_H_

#include <AE_GeneralPlug.h>
#include <AE_EffectSuites.h>
#include <AE_AdvEffectSuites.h>
#include <AE_EffectCBSuites.h>
#include <AE_ChannelSuites.h>
#include <AE_EffectSuitesHelper.h>

// Here's a SuiteHandler that can work in several different versions of AE.

#ifndef PF_AE105_PLUG_IN_VERSION
#define PF_AE105_PLUG_IN_VERSION	13
#define PF_AE105_PLUG_IN_SUBVERS	1
#endif

#ifndef PF_AE100_PLUG_IN_VERSION
#define PF_AE100_PLUG_IN_VERSION	13
#endif

#if PF_AE_PLUG_IN_VERSION < PF_AE100_PLUG_IN_VERSION

// Suites to use before AE10
typedef		AEIO_FunctionBlock2				Current_AEIO_FunctionBlock;
typedef		A_char							A_PathType;
typedef		A_char							A_NameType;

#define		kAEGPRegisterSuiteVersion		kAEGPRegisterSuiteVersion3
typedef		AEGP_RegisterSuite3				AEGP_RegisterSuite;
#define		kAEGPUtilitySuiteVersion		kAEGPUtilitySuiteVersion3
typedef		AEGP_UtilitySuite3				AEGP_UtilitySuite;
#define		kAEGPFIMSuiteVersion			kAEGPFIMSuiteVersion2
typedef		AEGP_FIMSuite2					AEGP_FIMSuite;
#define		kAEGPIOInSuiteVersion			kAEGPIOInSuiteVersion2
typedef		AEGP_IOInSuite2					AEGP_IOInSuite;
#define		kAEGPIOOutSuiteVersion			kAEGPIOOutSuiteVersion3
typedef		AEGP_IOOutSuite3				AEGP_IOOutSuite;
#define		kAEGPOutputModuleSuiteVersion	kAEGPOutputModuleSuiteVersion2
typedef		AEGP_OutputModuleSuite2			AEGP_OutputModuleSuite;
#define		kAEGPProjSuiteVersion			kAEGPProjSuiteVersion4
typedef		AEGP_ProjSuite4					AEGP_ProjSuite;
#define		kAEGPFootageSuiteVersion		kAEGPFootageSuiteVersion4
typedef		AEGP_FootageSuite4				AEGP_FootageSuite;
#define		kPFAppSuiteVersion				kPFAppSuiteVersion3
typedef		PFAppSuite3						PFAppSuite;
#define		kAEGPPersistentDataSuiteVersion	kAEGPPersistentDataSuiteVersion2
typedef		AEGP_PersistentDataSuite2		AEGP_PersistentDataSuite;
#define		kAEGPItemSuiteVersion			kAEGPItemSuiteVersion7
typedef		AEGP_ItemSuite7					AEGP_ItemSuite;
#define		kAEGPCompSuiteVersion			kAEGPCompSuiteVersion6
typedef		AEGP_CompSuite6					AEGP_CompSuite;
#define		kAEGPLayerSuiteVersion			kAEGPLayerSuiteVersion5
typedef		AEGP_LayerSuite5				AEGP_LayerSuite;
#define		kPFParamUtilsSuiteVersion		kPFParamUtilsSuiteVersion2
typedef		PF_ParamUtilsSuite2				PF_ParamUtilsSuite;

#else

// Suites to use from AE10 onward, when a lot of AEIO stuff changed

typedef		AEIO_FunctionBlock4				Current_AEIO_FunctionBlock;
typedef		A_UTF16Char						A_PathType;
#define		AE_UNICODE_PATHS				1
typedef		A_UTF16Char						A_NameType;
#define		AE_UNICODE_NAMES				1

#define		kAEGPRegisterSuiteVersion		kAEGPRegisterSuiteVersion5
typedef		AEGP_RegisterSuite5				AEGP_RegisterSuite;
#define		kAEGPUtilitySuiteVersion		kAEGPUtilitySuiteVersion5
typedef		AEGP_UtilitySuite5				AEGP_UtilitySuite;
#define		kAEGPFIMSuiteVersion			kAEGPFIMSuiteVersion3
typedef		AEGP_FIMSuite3					AEGP_FIMSuite;
#define		kAEGPIOInSuiteVersion			kAEGPIOInSuiteVersion4
typedef		AEGP_IOInSuite4					AEGP_IOInSuite;
#define		kAEGPIOOutSuiteVersion			kAEGPIOOutSuiteVersion4
typedef		AEGP_IOOutSuite4				AEGP_IOOutSuite;
#define		kAEGPOutputModuleSuiteVersion	kAEGPOutputModuleSuiteVersion4
typedef		AEGP_OutputModuleSuite4			AEGP_OutputModuleSuite;
#define		kAEGPProjSuiteVersion			kAEGPProjSuiteVersion5
typedef		AEGP_ProjSuite5					AEGP_ProjSuite;
#define		kAEGPFootageSuiteVersion		kAEGPFootageSuiteVersion5
typedef		AEGP_FootageSuite5				AEGP_FootageSuite;
#define		kPFAppSuiteVersion				kPFAppSuiteVersion4
typedef		PFAppSuite4						PFAppSuite;
#define		kAEGPPersistentDataSuiteVersion	kAEGPPersistentDataSuiteVersion3
typedef		AEGP_PersistentDataSuite3		AEGP_PersistentDataSuite;
#define		kAEGPItemSuiteVersion			kAEGPItemSuiteVersion8
typedef		AEGP_ItemSuite8					AEGP_ItemSuite;
#define		kAEGPCompSuiteVersion			kAEGPCompSuiteVersion7
typedef		AEGP_CompSuite7					AEGP_CompSuite;
#define		kAEGPLayerSuiteVersion			kAEGPLayerSuiteVersion6
typedef		AEGP_LayerSuite6				AEGP_LayerSuite;
#define		kPFParamUtilsSuiteVersion		kPFParamUtilsSuiteVersion3
typedef		PF_ParamUtilsSuite3				PF_ParamUtilsSuite;


// only available in >=10.0
#include <adobesdk/DrawbotSuite.h>

#define		kDRAWBOT_DrawbotSuite_Version	kDRAWBOT_DrawSuite_VersionCurrent
typedef		DRAWBOT_DrawbotSuiteCurrent		DB_DrawbotSuite;
#define		kDB_SupplierSuite_Version		kDRAWBOT_SupplierSuite_VersionCurrent
typedef		DRAWBOT_SupplierSuiteCurrent	DB_SupplierSuite;
#define		kDB_SurfaceSuite_Version		kDRAWBOT_SurfaceSuite_VersionCurrent
typedef		DRAWBOT_SurfaceSuiteCurrent		DB_SurfaceSuite;
#define		kDB_PathSuite_Version			kDRAWBOT_PathSuite_VersionCurrent
typedef		DRAWBOT_PathSuiteCurrent		DB_PathSuite;
#define		kPFEffectCustomUISuiteVersion	kPFEffectCustomUISuiteVersion1
typedef		PF_EffectCustomUISuite1			PF_EffectCustomUISuite;
#define		kCustomUIThemeSuite				kPFEffectCustomUIOverlayThemeSuite
#define		kCustomUIThemeSuiteVersion		kPFEffectCustomUIOverlayThemeSuiteVersion1
typedef		PF_EffectCustomUIOverlayThemeSuite1	PF_CustomUIThemeSuite;

#endif

// Suites that aren't effected by the AE10 change
#define		kAEGPCommandSuiteVersion		kAEGPCommandSuiteVersion1
typedef		AEGP_CommandSuite1				AEGP_CommandSuite;
#define		kAEGPWorldSuiteVersion			kAEGPWorldSuiteVersion3
typedef		AEGP_WorldSuite3				AEGP_WorldSuite;
#define		kPFWorldSuiteVersion			kPFWorldSuiteVersion2
typedef		PF_WorldSuite2					PF_WorldSuite;
#define		kPFWorldTransformSuiteVersion	kPFWorldTransformSuiteVersion1
typedef		PF_WorldTransformSuite1			PF_WorldTransformSuite;
#define		kPFFillMatteSuiteVersion		kPFFillMatteSuiteVersion2
typedef		PF_FillMatteSuite2				PF_FillMatteSuite;
#define		kPFChannelSuiteVersion			kPFChannelSuiteVersion1
typedef		PF_ChannelSuite1				PF_ChannelSuite;
#define		kPFIterate8SuiteVersion			kPFIterate8SuiteVersion1
typedef		PF_Iterate8Suite1				PF_Iterate8Suite;
#define		kPFIterate16SuiteVersion		kPFIterate16SuiteVersion1
typedef		PF_iterate16Suite1				PF_Iterate16Suite;
#define		kPFIterateFloatSuiteVersion		kPFIterateFloatSuiteVersion1
typedef		PF_iterateFloatSuite1			PF_IterateFloatSuite;
#define		kAEGPIterateSuiteVersion		kAEGPIterateSuiteVersion1
typedef		AEGP_IterateSuite1				AEGP_IterateSuite;
#define		kAEGPRQItemSuiteVersion			kAEGPRQItemSuiteVersion3
typedef		AEGP_RQItemSuite3				AEGP_RQItemSuite;
#define		kAEGPRenderOptionsSuiteVersion	kAEGPRenderOptionsSuiteVersion3
typedef		AEGP_RenderOptionsSuite3		AEGP_RenderOptionsSuite;
#define		kAEGPRenderSuiteVersion			kAEGPRenderSuiteVersion2
typedef		AEGP_RenderSuite2				AEGP_RenderSuite;
#define		kAEGPCameraSuiteVersion			kAEGPCameraSuiteVersion2
typedef		AEGP_CameraSuite2				AEGP_CameraSuite;
#define		kAEGPCollectionSuiteVersion		kAEGPCollectionSuiteVersion2
typedef		AEGP_CollectionSuite2			AEGP_CollectionSuite;
#define		kAEGPStreamSuiteVersion			kAEGPStreamSuiteVersion3
typedef		AEGP_StreamSuite3				AEGP_StreamSuite;
#define		kAEGPDynamicStreamSuiteVersion	kAEGPDynamicStreamSuiteVersion3
typedef		AEGP_DynamicStreamSuite3		AEGP_DynamicStreamSuite;
#define		kAEGPKeyframeSuiteVersion		kAEGPKeyframeSuiteVersion4
typedef		AEGP_KeyframeSuite4				AEGP_KeyframeSuite;
#define		kAEGPEffectSuiteVersion			kAEGPEffectSuiteVersion3
typedef		AEGP_EffectSuite3				AEGP_EffectSuite;
#define		kPFColorParamSuiteVersion		kPFColorParamSuiteVersion1
typedef		PF_ColorParamSuite1				PF_ColorParamSuite;
#define		kAEGPMemorySuiteVersion			kAEGPMemorySuiteVersion1
typedef		AEGP_MemorySuite1				AEGP_MemorySuite;
#define		kPFHandleSuiteVersion			kPFHandleSuiteVersion1
typedef		PF_HandleSuite1					PF_HandleSuite;
#define		kAEGPColorSettingsSuiteVersion	kAEGPColorSettingsSuiteVersion2
typedef		AEGP_ColorSettingsSuite2		AEGP_ColorSettingsSuite;
#define		kPFAdvAppSuiteVersion			kPFAdvAppSuiteVersion1
typedef		PF_AdvAppSuite1					PFAdvAppSuite;
#define		kAEGPTextDocumentSuiteVersion	kAEGPTextDocumentSuiteVersion1
typedef		AEGP_TextDocumentSuite1			AEGP_TextDocumentSuite;

// Suite registration and handling object
class AEGP_SuiteHandler {

private:
	// basic suite pointer
	const SPBasicSuite				*i_pica_basicP;

	// Suites we can register. These are mutable because they are demand loaded using a const object.

	struct Suites {
		AEGP_RegisterSuite			*register_suiteP;
		AEGP_UtilitySuite			*utility_suiteP;
		AEGP_CommandSuite			*command_suiteP;
		AEGP_FIMSuite				*fim_suiteP;
		AEGP_WorldSuite				*aegp_world_suiteP;
		PF_WorldSuite				*pf_world_suiteP;
		PF_WorldTransformSuite		*pf_world_transform_suiteP;
		PF_FillMatteSuite			*pf_fill_matte_suiteP;
		PF_ChannelSuite				*pf_channel_suiteP;
		PF_Iterate8Suite			*pf_iterate8_suiteP;
		PF_Iterate16Suite			*pf_iterate16_suiteP;
		PF_IterateFloatSuite		*pf_iterateFloat_suiteP;
		AEGP_IterateSuite			*aegp_iterate_suiteP;
		AEGP_IOInSuite				*io_in_suiteP;
		AEGP_IOOutSuite				*io_out_suiteP;
		AEGP_RQItemSuite			*rq_item_suiteP;
		AEGP_RenderOptionsSuite		*render_options_suiteP;
		AEGP_RenderSuite			*render_suiteP;
		AEGP_CompSuite				*comp_suiteP;
		AEGP_LayerSuite				*layer_suiteP;
		PF_ParamUtilsSuite			*pf_param_utils_suiteP;
		AEGP_CameraSuite			*camera_suiteP;
		AEGP_CollectionSuite		*collection_suiteP;
		AEGP_StreamSuite			*stream_suiteP;
		AEGP_DynamicStreamSuite		*dyn_stream_suiteP;
		AEGP_KeyframeSuite			*keyframe_suiteP;
		AEGP_EffectSuite			*effect_suiteP;
		PF_ColorParamSuite			*color_param_suiteP;
		AEGP_ItemSuite				*item_suiteP;
		AEGP_FootageSuite			*footage_suiteP;
		AEGP_OutputModuleSuite		*output_module_suiteP;
		AEGP_MemorySuite			*memory_suiteP;
		PF_HandleSuite				*handle_suiteP;
		AEGP_ProjSuite				*proj_suiteP;
		AEGP_ColorSettingsSuite		*color_settings_suiteP;
		PFAppSuite					*app_suiteP;
		PFAdvAppSuite				*adv_app_suiteP;
		AEGP_PersistentDataSuite	*persistent_data_suiteP;
		AEGP_TextDocumentSuite		*text_document_suiteP;

	#if PF_AE_PLUG_IN_VERSION >= PF_AE100_PLUG_IN_VERSION
		DB_DrawbotSuite				*db_drawbot_suiteP;
		DB_SupplierSuite			*db_supplier_suiteP;
		DB_SurfaceSuite				*db_surface_suiteP;
		DB_PathSuite				*db_path_suiteP;
		PF_EffectCustomUISuite		*pf_effect_customUI_suiteP;
		PF_CustomUIThemeSuite		*pf_customUI_theme_suiteP;
		
		// if we need the new timecode stuff
		#if PF_AE_PLUG_IN_VERSION > PF_AE105_PLUG_IN_VERSION || PF_AE_PLUG_IN_SUBVERS >= PF_AE105_PLUG_IN_SUBVERS
		#define AE105_TIMECODE_SUITES
		AEGP_ProjSuite6				*proj_suite6P;
		AEGP_CompSuite8				*comp_suite8P;
		#endif
	#endif
	};

	mutable Suites i_suites;

	// private methods
	// I had to make this inline by moving the definition from the .cpp file
	// CW mach-o target was freaking otherwise when seeing the call to this
	// function in inlined public suite accessors below

	void *LoadSuite(A_char *nameZ, A_long versionL) const
	{
		const void *suiteP;
		A_long err = i_pica_basicP->AcquireSuite(nameZ, versionL, &suiteP);

		if (err || !suiteP) {
			MissingSuiteError();
		}

		return (void*)suiteP;
	}
	
	void ReleaseSuite(A_char *nameZ, A_long versionL);
	void ReleaseAllSuites()
	{
		#define	AEGP_SUITE_RELEASE_BOILERPLATE(MEMBER_NAME, kSUITE_NAME, kVERSION_NAME)		\
			if (i_suites.MEMBER_NAME) {														\
				ReleaseSuite(kSUITE_NAME, kVERSION_NAME);									\
			}
		
		AEGP_SUITE_RELEASE_BOILERPLATE(register_suiteP, kAEGPRegisterSuite, kAEGPRegisterSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(utility_suiteP, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(command_suiteP, kAEGPCommandSuite, kAEGPCommandSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(fim_suiteP, kAEGPFIMSuite, kAEGPFIMSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(aegp_world_suiteP, kAEGPWorldSuite, kAEGPWorldSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(pf_world_suiteP, kPFWorldSuite, kPFWorldSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(pf_world_transform_suiteP, kPFWorldTransformSuite, kPFWorldTransformSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(pf_fill_matte_suiteP, kPFFillMatteSuite, kPFFillMatteSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(pf_channel_suiteP, kPFChannelSuite1, kPFChannelSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(pf_iterate8_suiteP, kPFIterate8Suite, kPFIterate8SuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(pf_iterate16_suiteP, kPFIterate16Suite, kPFIterate16SuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(pf_iterateFloat_suiteP, kPFIterateFloatSuite, kPFIterateFloatSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(aegp_iterate_suiteP, kAEGPIterateSuite, kAEGPIterateSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(io_out_suiteP, kAEGPIOOutSuite, kAEGPIOOutSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(io_in_suiteP, kAEGPIOInSuite, kAEGPIOInSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(rq_item_suiteP, kAEGPRQItemSuite, kAEGPRQItemSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(render_options_suiteP, kAEGPRenderOptionsSuite, kAEGPRenderOptionsSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(render_suiteP, kAEGPRenderSuite, kAEGPRenderSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(comp_suiteP, kAEGPCompSuite, kAEGPCompSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(layer_suiteP, kAEGPLayerSuite, kAEGPLayerSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(pf_param_utils_suiteP, kPFParamUtilsSuite, kPFParamUtilsSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(camera_suiteP, kAEGPCameraSuite, kAEGPCameraSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(collection_suiteP, kAEGPCollectionSuite, kAEGPCollectionSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(stream_suiteP, kAEGPStreamSuite, kAEGPStreamSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(dyn_stream_suiteP, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(keyframe_suiteP, kAEGPKeyframeSuite, kAEGPKeyframeSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(effect_suiteP, kAEGPEffectSuite, kAEGPEffectSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(color_param_suiteP, kPFColorParamSuite, kPFColorParamSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(item_suiteP, kAEGPItemSuite, kAEGPItemSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(footage_suiteP, kAEGPFootageSuite, kAEGPFootageSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(output_module_suiteP, kAEGPOutputModuleSuite, kAEGPOutputModuleSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(memory_suiteP, kAEGPMemorySuite, kAEGPMemorySuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(handle_suiteP, kPFHandleSuite, kPFHandleSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(proj_suiteP, kAEGPProjSuite, kAEGPProjSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(color_settings_suiteP, kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(app_suiteP, kPFAppSuite, kPFAppSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(adv_app_suiteP, kPFAdvAppSuite, kPFAdvAppSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(persistent_data_suiteP, kAEGPPersistentDataSuite, kAEGPPersistentDataSuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(text_document_suiteP, kAEGPTextDocumentSuite, kAEGPTextDocumentSuiteVersion);

	#if PF_AE_PLUG_IN_VERSION >= PF_AE100_PLUG_IN_VERSION
		AEGP_SUITE_RELEASE_BOILERPLATE(db_drawbot_suiteP, kDRAWBOT_DrawSuite, kDRAWBOT_DrawbotSuite_Version);
		AEGP_SUITE_RELEASE_BOILERPLATE(db_supplier_suiteP, kDRAWBOT_SupplierSuite, kDB_SupplierSuite_Version);
		AEGP_SUITE_RELEASE_BOILERPLATE(db_surface_suiteP, kDRAWBOT_SurfaceSuite, kDB_SurfaceSuite_Version);
		AEGP_SUITE_RELEASE_BOILERPLATE(db_path_suiteP, kDRAWBOT_PathSuite, kDB_PathSuite_Version);
		AEGP_SUITE_RELEASE_BOILERPLATE(pf_effect_customUI_suiteP, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion);
		AEGP_SUITE_RELEASE_BOILERPLATE(pf_customUI_theme_suiteP, kCustomUIThemeSuite, kCustomUIThemeSuiteVersion);
		
		#ifdef AE105_TIMECODE_SUITES
		AEGP_SUITE_RELEASE_BOILERPLATE(proj_suite6P, kAEGPProjSuite, kAEGPProjSuiteVersion6);
		AEGP_SUITE_RELEASE_BOILERPLATE(comp_suite8P, kAEGPCompSuite, kAEGPCompSuiteVersion8);
		#endif
	#endif
	}

	// Here is the error handling function which must be defined.
	// It must exit by throwing an exception, it cannot return.
	void MissingSuiteError() const;

public:
	// To construct, pass pica_basicP
	AEGP_SuiteHandler(const SPBasicSuite *pica_basicP);
	~AEGP_SuiteHandler();
	
	const SPBasicSuite *Pica()	const	{ return i_pica_basicP; }

	#define	AEGP_SUITE_ACCESS_BOILERPLATE(SUITE_NAME, SUITE_TYPE, MEMBER_NAME, kSUITE_NAME, kVERSION_NAME)	\
		SUITE_TYPE *SUITE_NAME() const											\
	{																															\
		if (i_suites.MEMBER_NAME == NULL) {																						\
			i_suites.MEMBER_NAME = (SUITE_TYPE*)LoadSuite(kSUITE_NAME, kVERSION_NAME);											\
		}																														\
		return i_suites.MEMBER_NAME;																							\
	}
	
	AEGP_SUITE_ACCESS_BOILERPLATE(RegisterSuite, AEGP_RegisterSuite, register_suiteP, kAEGPRegisterSuite, kAEGPRegisterSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(UtilitySuite, AEGP_UtilitySuite, utility_suiteP, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(CommandSuite, AEGP_CommandSuite, command_suiteP, kAEGPCommandSuite, kAEGPCommandSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(FIMSuite, AEGP_FIMSuite, fim_suiteP, kAEGPFIMSuite, kAEGPFIMSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(AEGPWorldSuite, AEGP_WorldSuite, aegp_world_suiteP, kAEGPWorldSuite, kAEGPWorldSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(PFWorldSuite, PF_WorldSuite, pf_world_suiteP, kPFWorldSuite, kPFWorldSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(PFWorldTransformSuite, PF_WorldTransformSuite, pf_world_transform_suiteP, kPFWorldTransformSuite, kPFWorldTransformSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(PFFillMatteSuite, PF_FillMatteSuite, pf_fill_matte_suiteP, kPFFillMatteSuite, kPFFillMatteSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(PFChannelSuite, PF_ChannelSuite, pf_channel_suiteP, kPFChannelSuite1, kPFChannelSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(PFIterate8Suite, PF_Iterate8Suite, pf_iterate8_suiteP, kPFIterate8Suite, kPFIterate8SuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(PFIterate16Suite, PF_Iterate16Suite, pf_iterate16_suiteP, kPFIterate16Suite, kPFIterate16SuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(PFIterateFloatSuite, PF_IterateFloatSuite, pf_iterateFloat_suiteP, kPFIterateFloatSuite, kPFIterateFloatSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(AEGPIterateSuite, AEGP_IterateSuite, aegp_iterate_suiteP, kAEGPIterateSuite, kAEGPIterateSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(IOInSuite, AEGP_IOInSuite, io_in_suiteP, kAEGPIOInSuite, kAEGPIOInSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(IOOutSuite, AEGP_IOOutSuite, io_out_suiteP, kAEGPIOOutSuite, kAEGPIOOutSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(RQItemSuite, AEGP_RQItemSuite, rq_item_suiteP, kAEGPRQItemSuite, kAEGPRQItemSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(RenderOptionsSuite, AEGP_RenderOptionsSuite, render_options_suiteP, kAEGPRenderOptionsSuite, kAEGPRenderOptionsSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(RenderSuite, AEGP_RenderSuite, render_suiteP, kAEGPRenderSuite, kAEGPRenderSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(CompSuite, AEGP_CompSuite, comp_suiteP, kAEGPCompSuite, kAEGPCompSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(LayerSuite, AEGP_LayerSuite, layer_suiteP, kAEGPLayerSuite, kAEGPLayerSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(PFParamUtilsSuite, PF_ParamUtilsSuite, pf_param_utils_suiteP, kPFParamUtilsSuite, kPFParamUtilsSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(CameraSuite, AEGP_CameraSuite, camera_suiteP, kAEGPCameraSuite, kAEGPCameraSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(CollectionSuite, AEGP_CollectionSuite, collection_suiteP, kAEGPCollectionSuite, kAEGPCollectionSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(StreamSuite, AEGP_StreamSuite, stream_suiteP, kAEGPStreamSuite, kAEGPStreamSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(DynamicStreamSuite, AEGP_DynamicStreamSuite, dyn_stream_suiteP, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(KeyframeSuite, AEGP_KeyframeSuite, keyframe_suiteP, kAEGPKeyframeSuite, kAEGPKeyframeSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(EffectSuite, AEGP_EffectSuite, effect_suiteP, kAEGPEffectSuite, kAEGPEffectSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(ColorParamSuite, PF_ColorParamSuite, color_param_suiteP, kPFColorParamSuite, kPFColorParamSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(ItemSuite, AEGP_ItemSuite, item_suiteP, kAEGPItemSuite, kAEGPItemSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(FootageSuite, AEGP_FootageSuite, footage_suiteP, kAEGPFootageSuite, kAEGPFootageSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(OutputModuleSuite, AEGP_OutputModuleSuite, output_module_suiteP, kAEGPOutputModuleSuite, kAEGPOutputModuleSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(MemorySuite, AEGP_MemorySuite, memory_suiteP, kAEGPMemorySuite, kAEGPMemorySuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(HandleSuite, PF_HandleSuite, handle_suiteP, kPFHandleSuite, kPFHandleSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(ProjSuite, AEGP_ProjSuite, proj_suiteP, kAEGPProjSuite, kAEGPProjSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(ColorSettingsSuite, AEGP_ColorSettingsSuite, color_settings_suiteP, kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(AppSuite, PFAppSuite, app_suiteP, kPFAppSuite, kPFAppSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(AdvAppSuite, PFAdvAppSuite, adv_app_suiteP, kPFAdvAppSuite, kPFAdvAppSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(PersistentDataSuite, AEGP_PersistentDataSuite, persistent_data_suiteP, kAEGPPersistentDataSuite, kAEGPPersistentDataSuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(TextDocumentSuite, AEGP_TextDocumentSuite, text_document_suiteP, kAEGPTextDocumentSuite, kAEGPTextDocumentSuiteVersion);
	
#if PF_AE_PLUG_IN_VERSION >= PF_AE100_PLUG_IN_VERSION
	AEGP_SUITE_ACCESS_BOILERPLATE(DBDrawbotSuite, DB_DrawbotSuite, db_drawbot_suiteP, kDRAWBOT_DrawSuite, kDRAWBOT_DrawbotSuite_Version);
	AEGP_SUITE_ACCESS_BOILERPLATE(DBSupplierSuite, DB_SupplierSuite, db_supplier_suiteP, kDRAWBOT_SupplierSuite, kDB_SupplierSuite_Version);
	AEGP_SUITE_ACCESS_BOILERPLATE(DBSurfaceSuite, DB_SurfaceSuite, db_surface_suiteP, kDRAWBOT_SurfaceSuite, kDB_SurfaceSuite_Version);
	AEGP_SUITE_ACCESS_BOILERPLATE(DBPathSuite, DB_PathSuite, db_path_suiteP, kDRAWBOT_PathSuite, kDB_PathSuite_Version);
	AEGP_SUITE_ACCESS_BOILERPLATE(PFEffectCustomUISuite, PF_EffectCustomUISuite, pf_effect_customUI_suiteP, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion);
	AEGP_SUITE_ACCESS_BOILERPLATE(PFCustomUIThemeSuite, PF_CustomUIThemeSuite, pf_customUI_theme_suiteP, kCustomUIThemeSuite, kCustomUIThemeSuiteVersion);
	
	#ifdef AE105_TIMECODE_SUITES
	AEGP_SUITE_ACCESS_BOILERPLATE(ProjSuite6, AEGP_ProjSuite6, proj_suite6P, kAEGPProjSuite, kAEGPProjSuiteVersion6);
	AEGP_SUITE_ACCESS_BOILERPLATE(CompSuite8, AEGP_CompSuite8, comp_suite8P, kAEGPCompSuite, kAEGPCompSuiteVersion8);
	#endif
#endif
};

#endif // _FNORD_SUITEHANDLER_H_
