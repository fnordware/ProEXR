//
//	Cryptomatte AE plug-in
//		by Brendan Bolles <brendan@fnordware.com>
//
//
//	Part of ProEXR
//		http://www.fnordware.com/ProEXR
//
//
//	mmm...our custom UI
//
//

#include "Cryptomatte_AE.h"

#include "DrawbotBot.h"

#include <assert.h>

extern AEGP_PluginID gAEGPPluginID;


static PF_Err
DrawEvent(	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*event_extra)
{
	PF_Err			err		=	PF_Err_NONE;
	
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	event_extra->evt_out_flags = 0;

	if(!(event_extra->evt_in_flags & PF_EI_DONT_DRAW) && params[CRYPTO_DATA]->u.arb_d.value != NULL)
	{
		if(PF_EA_CONTROL == event_extra->effect_win.area)
		{
			CryptomatteArbitraryData *arb_data = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(params[CRYPTO_DATA]->u.arb_d.value);
			
			DrawbotBot bot(in_data->pica_basicP, event_extra->contextH, in_data->appl_id);
			
            const int panel_left = event_extra->effect_win.current_frame.left;
            const int panel_top = event_extra->effect_win.current_frame.top;
            const int panel_width = event_extra->effect_win.current_frame.right - panel_left;
            const int panel_height = event_extra->effect_win.current_frame.bottom - panel_top;
			
			const float labelSpace = bot.FontSize() * 1.4;
			const float itemSpace = bot.FontSize() * 2.0;
			
			bot.SetColor(PF_App_Color_TEXT_DISABLED);
			
			bot.MoveTo(panel_left, panel_top + bot.FontSize());
			bot.DrawString("Layer:", kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_EndEllipsis, panel_width);

			bot.Move(0, itemSpace);
			bot.DrawString("Selection:", kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_EndEllipsis, panel_width);
			
			bot.Move(0, itemSpace);
			bot.DrawString("Manifest:", kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_EndEllipsis, panel_width);

			
			const float valuePos = (panel_left + (bot.FontSize() * 6));
			const float valueSpace = (event_extra->effect_win.current_frame.right - valuePos);
			
			bot.SetColor(PF_App_Color_WHITE);
			
			bot.MoveTo(valuePos, panel_top + bot.FontSize());
			bot.DrawString(GetLayer(arb_data), kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_EndEllipsis, valueSpace);

			bot.Move(0, itemSpace);
			bot.DrawString(GetSelection(arb_data), kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_EndEllipsis, valueSpace);

			bot.Move(0, itemSpace);
			bot.SetColor(PF_App_Color_GRAY);
			bot.DrawString(GetManifest(arb_data), kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_EndEllipsis, valueSpace);


			event_extra->evt_out_flags = PF_EO_HANDLED_EVENT;

			PF_UNLOCK_HANDLE(params[CRYPTO_DATA]->u.arb_d.value);
		}
	}

	return err;
}


static PF_Err
DoClick(	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*event_extra)
{
	if(params[CRYPTO_DATA]->u.arb_d.value)
	{
		AEGP_SuiteHandler suites(in_data->pica_basicP);

		CryptomatteArbitraryData *arb_data = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(params[CRYPTO_DATA]->u.arb_d.value);
		CryptomatteSequenceData *seq_data = (CryptomatteSequenceData *)PF_LOCK_HANDLE(in_data->sequence_data);

		const bool renderThreadMadness = (in_data->version.major > PF_AE135_PLUG_IN_VERSION || in_data->version.minor >= PF_AE135_PLUG_IN_SUBVERS);

		if(renderThreadMadness && arb_data && seq_data)
		{
			if(seq_data->context == NULL)
			{
				seq_data->context = new CryptomatteContext(arb_data);
			}
			else
			{
				CryptomatteContext *context = (CryptomatteContext *)seq_data->context;

				context->Update(arb_data);
			}
		}
		
		if(arb_data && seq_data && seq_data->context)
		{
			CryptomatteContext *context = (CryptomatteContext *)seq_data->context;
			
			bool validClick = false;
			std::set<std::string> clickedItems;

			PF_Point mouse_downPt = *(reinterpret_cast<PF_Point*>(&event_extra->u.do_click.screen_point));
			
			PF_FixedPoint mouse_downFixPt;
			mouse_downFixPt.x = INT2FIX(mouse_downPt.h);
			mouse_downFixPt.y = INT2FIX(mouse_downPt.v);
			
			event_extra->cbs.frame_to_source(event_extra->cbs.refcon, event_extra->contextH, &mouse_downFixPt);
			
			if((*event_extra->contextH)->w_type == PF_Window_COMP)
				event_extra->cbs.comp_to_layer(event_extra->cbs.refcon, event_extra->contextH, in_data->current_time, in_data->time_scale, &mouse_downFixPt);
			

			if(renderThreadMadness)
			{
				if(params[CRYPTO_SELECTION_MODE]->u.bd.value)
				{
					mouse_downPt.h = FIX2INT(mouse_downFixPt.x);
					mouse_downPt.v = FIX2INT(mouse_downFixPt.y);

					AEGP_EffectRefH effectRefH = NULL;
					
					suites.PFInterfaceSuite()->AEGP_GetNewEffectForEffect(gAEGPPluginID, in_data->effect_ref, &effectRefH);

					AEGP_LayerRenderOptionsH optionsH = NULL;
					suites.LayerRenderOptionsSuite()->AEGP_NewFromDownstreamOfEffect(gAEGPPluginID,	effectRefH,	&optionsH);

					suites.LayerRenderOptionsSuite()->AEGP_SetWorldType(optionsH, AEGP_WorldType_32);

					suites.LayerRenderOptionsSuite()->AEGP_SetDownsampleFactor(optionsH, in_data->downsample_x.num, in_data->downsample_y.num);
					
					AEGP_FrameReceiptH frameReceiptH = NULL;
					A_Err renderErr = suites.RenderSuite5()->AEGP_RenderAndCheckoutLayerFrame(optionsH,	NULL, NULL, &frameReceiptH);

					if(renderErr == A_Err_NONE && frameReceiptH != NULL)
					{
						AEGP_WorldH worldH = NULL;
						suites.RenderSuite5()->AEGP_GetReceiptWorld(frameReceiptH, &worldH);

						if(worldH != NULL)
						{
							PF_EffectWorld effectWorld;
							suites.AEGPWorldSuite()->AEGP_FillOutPFEffectWorld(worldH, &effectWorld);

							if(mouse_downPt.h >= 0 && mouse_downPt.h < effectWorld.width &&
								mouse_downPt.v >= 0 && mouse_downPt.v < effectWorld.height)
							{
								PF_PixelFormat format = PF_PixelFormat_INVALID;
								suites.PFWorldSuite()->PF_GetPixelFormat(&effectWorld, &format);
								assert(format == PF_PixelFormat_ARGB128);

								PF_PixelFloat *row = (PF_PixelFloat *)((char *)effectWorld.data + (effectWorld.rowbytes * mouse_downPt.v));

								clickedItems = context->GetItemsFromSelectionColor( row[mouse_downPt.h] );

								validClick = true;
							}
						}

						suites.RenderSuite5()->AEGP_CheckinFrame(frameReceiptH);
					}

					suites.LayerRenderOptionsSuite()->AEGP_Dispose(optionsH);

					suites.EffectSuite()->AEGP_DisposeEffect(effectRefH);
				}
				else
					suites.AdvAppSuite()->PF_InfoDrawText("Cryptomatte plug-in not in", "selection mode");
			}
			else
			{
				assert(!params[CRYPTO_SELECTION_MODE]->u.bd.value);

				if( context->Valid() )
				{
					mouse_downPt.h = FIX2INT(mouse_downFixPt.x * context->DownsampleX().num / context->DownsampleX().den);
					mouse_downPt.v = FIX2INT(mouse_downFixPt.y * context->DownsampleY().num / context->DownsampleY().den);

					if(mouse_downPt.h > 0 && mouse_downPt.h < context->Width() &&
						mouse_downPt.v > 0 && mouse_downPt.v < context->Height())
					{
						validClick = true;

						clickedItems = context->GetItems(mouse_downPt.h, mouse_downPt.v);
					}
				}
			}

			if(validClick)
			{
				std::set<std::string> newSelection;
				std::string selectionInfo, selectionInfoList; 
				
			#define ALL_MODIFIERS (PF_Mod_CMD_CTRL_KEY | PF_Mod_SHIFT_KEY | PF_Mod_OPT_ALT_KEY | PF_Mod_MAC_CONTROL_KEY)
			
				if((event_extra->u.do_click.modifiers & ALL_MODIFIERS) == 0) // select
				{
					if(clickedItems.size() > 0)
						selectionInfo = "Cryptomatte selection set to ";
					else
						selectionInfo = "Cryptomatte selection emptied.";

					for(std::set<std::string>::iterator i = clickedItems.begin(); i != clickedItems.end(); ++i)
					{
						std::string item = *i;

						if(i != clickedItems.begin())
							selectionInfoList += ", ";

						selectionInfoList += item;
						
						if(item.find(" ") != std::string::npos)
							item = "\"" + item + "\"";
						
						newSelection.insert(item);
					}
				}
				else
				{
					std::vector<std::string> currenSelection;
					CryptomatteContext::quotedTokenize(GetSelection(arb_data), currenSelection, ", ");
					
					for(std::vector<std::string>::const_iterator i = currenSelection.begin(); i != currenSelection.end(); ++i)
					{
						newSelection.insert(*i);
					}
						
					if(event_extra->u.do_click.modifiers & PF_Mod_SHIFT_KEY) // add
					{
						if(clickedItems.size() > 0)
							selectionInfo = "Cryptomatte selection added ";
						else
							selectionInfo = "Cryptomatte selection added nothing.";

						for(std::set<std::string>::const_iterator i = clickedItems.begin(); i != clickedItems.end(); ++i)
						{
							std::string item = *i;
							
							if(i != clickedItems.begin())
								selectionInfoList += ", ";

							selectionInfoList += item;
							
							if(item.find(" ") != std::string::npos)
								item = "\"" + item + "\"";
							
							newSelection.insert(item);
						}
					}
					else // remove
					{
						if(clickedItems.size() > 0)
							selectionInfo = "Cryptomatte selection removed ";
						else
							selectionInfo = "Cryptomatte selection removed nothing.";

						for(std::set<std::string>::const_iterator i = clickedItems.begin(); i != clickedItems.end(); ++i)
						{
							const std::string &item = *i;
							
							for(std::set<std::string>::iterator j = newSelection.begin(); j != newSelection.end(); ++j)
							{
								const std::string &currItem = *j;
								
								if(item == currItem || item == CryptomatteContext::deQuote(currItem))
								{
									newSelection.erase(j);

									if(i != clickedItems.begin())
										selectionInfoList += ", ";
									
									selectionInfoList += item;

									break;
								}
							}
						}
					}
				}
				
				
				std::string selectedString;
				
				for(std::set<std::string>::const_iterator i = newSelection.begin(); i != newSelection.end(); ++i)
				{
					if( !selectedString.empty() )
					{
						selectedString += ", ";
					}
					
					selectedString += *i;
				}
				
				
				SetArbSelection(in_data, &params[CRYPTO_DATA]->u.arb_d.value, selectedString);
				
				params[CRYPTO_DATA]->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
				
				if(!(event_extra->u.do_click.modifiers & PF_Mod_CAPS_LOCK_KEY) && !renderThreadMadness)
					seq_data->selectionChanged = TRUE;

				suites.AdvAppSuite()->PF_InfoDrawText(selectionInfo.c_str(), selectionInfoList.c_str());
					
				if(!renderThreadMadness)
					event_extra->u.do_click.send_drag = TRUE;
			}
		}
		
		PF_UNLOCK_HANDLE(params[CRYPTO_DATA]->u.arb_d.value);
		PF_UNLOCK_HANDLE(in_data->sequence_data);
	}
	
	return PF_Err_NONE;
}


PF_Err
HandleEvent ( 
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*extra )
{
	PF_Err		err		= PF_Err_NONE;
	
	if (!err) 
	{
		switch(extra->e_type) 
		{
			case PF_Event_DRAW:
				err = DrawEvent(in_data, out_data, params, output, extra);
				break;
			
			case PF_Event_DO_CLICK:
			case PF_Event_DRAG:
				if((*extra->contextH)->w_type == PF_Window_EFFECT)
				{
					err = DoDialog(in_data, out_data, params, output);
					extra->evt_out_flags = PF_EO_HANDLED_EVENT;
				}
				else if((*extra->contextH)->w_type != PF_Window_NONE)
				{
					err = DoClick(in_data, out_data, params, output, extra);
					extra->evt_out_flags = PF_EO_HANDLED_EVENT;
				}
				break;
				
			case PF_Event_ADJUST_CURSOR:
				if((*extra->contextH)->w_type == PF_Window_EFFECT)
				{
				#if defined(MAC_ENV)
					#if PF_AE_PLUG_IN_VERSION >= PF_AE100_PLUG_IN_VERSION
					SetMickeyCursor(); // the cute mickey mouse hand
					#else
					SetThemeCursor(kThemePointingHandCursor);
					#endif
					extra->u.adjust_cursor.set_cursor = PF_Cursor_CUSTOM;
				#else
					extra->u.adjust_cursor.set_cursor = PF_Cursor_FINGER_POINTER;
				#endif
				}
				else if((*extra->contextH)->w_type != PF_Window_NONE)
				{
					const bool renderThreadMadness = (in_data->version.major > PF_AE135_PLUG_IN_VERSION || in_data->version.minor >= PF_AE135_PLUG_IN_SUBVERS);

					if(!renderThreadMadness || params[CRYPTO_SELECTION_MODE]->u.bd.value)
					{
						if((extra->u.adjust_cursor.modifiers & ALL_MODIFIERS) == 0)
							extra->u.adjust_cursor.set_cursor = PF_Cursor_HOLLOW_ARROW;
						else if(extra->u.adjust_cursor.modifiers & PF_Mod_SHIFT_KEY)
							extra->u.adjust_cursor.set_cursor = PF_Cursor_HOLLOW_ARROW_PLUS;
						else
							extra->u.adjust_cursor.set_cursor = PF_Cursor_SCISSORS;
					}
					else
						extra->u.adjust_cursor.set_cursor = PF_Cursor_COLOR_CUBE_CROSS_SECTION;
				}
				
				extra->evt_out_flags = PF_EO_HANDLED_EVENT;
				break;
		}
	}
	
	return err;
}
