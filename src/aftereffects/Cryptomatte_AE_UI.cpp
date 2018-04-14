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
	
	// Selecting step 3: We have the selected items, now change the Param with AEGP suites (!)
	
	CryptomatteSequenceData *seq_data = (CryptomatteSequenceData *)PF_LOCK_HANDLE(in_data->sequence_data);
	
	if(seq_data && seq_data->sendingClickedItems && seq_data->clickedItems != NULL && in_data->global_data != NULL)
	{
		const std::set<std::string> *clickedItems = (const std::set<std::string> *)seq_data->clickedItems;
		
		if(params[CRYPTO_DATA]->u.arb_d.value != NULL)
		{
			CryptomatteArbitraryData *arb_data = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(params[CRYPTO_DATA]->u.arb_d.value);
				
			std::set<std::string> newSelection;
			
		#define ALL_MODIFIERS (PF_Mod_CMD_CTRL_KEY | PF_Mod_SHIFT_KEY | PF_Mod_OPT_ALT_KEY | PF_Mod_MAC_CONTROL_KEY)

			if((seq_data->modifiers & ALL_MODIFIERS) == 0) // select
			{
				for(std::set<std::string>::iterator i = clickedItems->begin(); i != clickedItems->end(); ++i)
				{
					std::string item = *i;
					
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
					
				if(seq_data->modifiers & PF_Mod_SHIFT_KEY) // add
				{
					for(std::set<std::string>::const_iterator i = clickedItems->begin(); i != clickedItems->end(); ++i)
					{
						std::string item = *i;
						
						if(item.find(" ") != std::string::npos)
							item = "\"" + item + "\"";
						
						newSelection.insert(item);
					}
				}
				else // remove
				{
					for(std::set<std::string>::const_iterator i = clickedItems->begin(); i != clickedItems->end(); ++i)
					{
						const std::string &item = *i;
						
						for(std::set<std::string>::iterator j = newSelection.begin(); j != newSelection.end(); ++j)
						{
							const std::string &currItem = *j;
							
							if(item == currItem || item == CryptomatteContext::deQuote(currItem))
							{
								newSelection.erase(j);
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
					selectedString += ", ";
				
				selectedString += *i;
			}
			
			
			CryptomatteGlobalData *global_data = (CryptomatteGlobalData *)PF_LOCK_HANDLE(in_data->global_data);
			
			if(global_data)
			{
				AEGP_EffectRefH effectRefH = NULL;
				
				suites.PFInterfaceSuite()->AEGP_GetNewEffectForEffect(global_data->pluginID, in_data->effect_ref, &effectRefH);
				
				if(effectRefH)
				{
					AEGP_StreamRefH streamRefH = NULL;
					
					suites.StreamSuite()->AEGP_GetNewEffectStreamByIndex(global_data->pluginID, effectRefH, CRYPTO_DATA, &streamRefH);
					
					if(streamRefH)
					{
						const size_t oldArbSize = PF_GET_HANDLE_SIZE(params[CRYPTO_DATA]->u.arb_d.value);
						
						PF_Handle newArbH = PF_NEW_HANDLE(oldArbSize);
						
						if(newArbH)
						{
							CryptomatteArbitraryData *oldArb = arb_data;
							CryptomatteArbitraryData *newArb = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(newArbH);
							
							memcpy(newArb, oldArb, oldArbSize);
							
							PF_UNLOCK_HANDLE(newArbH);
							
							
							SetArbSelection(in_data, (PF_ArbitraryH *)&newArbH, selectedString);
							
							
							AEGP_StreamValue2 val;
							val.val.arbH = newArbH;
							
							A_long keyframes = 0;
							suites.KeyframeSuite()->AEGP_GetStreamNumKFs(streamRefH, &keyframes);
							
							if(keyframes > 0)
							{
								A_Time frame_time;
								suites.PFInterfaceSuite()->AEGP_ConvertEffectToCompTime(in_data->effect_ref, in_data->current_time, in_data->time_scale, &frame_time);
								
								AEGP_KeyframeIndex key_index;
								suites.KeyframeSuite()->AEGP_InsertKeyframe(streamRefH, AEGP_LTimeMode_LayerTime, &frame_time, &key_index);
								
								suites.KeyframeSuite()->AEGP_SetKeyframeValue(streamRefH, key_index, &val);
							}
							else
								suites.StreamSuite()->AEGP_SetStreamValue(global_data->pluginID, streamRefH, &val);
						}
						else
							assert(FALSE);
						
						suites.StreamSuite()->AEGP_DisposeStream(streamRefH);
					}
					else
						assert(FALSE);
					
					// How we used to do it...
					//SetArbSelection(in_data, &params[CRYPTO_DATA]->u.arb_d.value, selectedString);
					//params[CRYPTO_DATA]->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
					
					if(!(seq_data->modifiers & PF_Mod_CAPS_LOCK_KEY))
						seq_data->selectionChanged = TRUE;
				}
				
				PF_UNLOCK_HANDLE(in_data->global_data);
				
				PF_UNLOCK_HANDLE(params[CRYPTO_DATA]->u.arb_d.value);
			}
			else
				assert(FALSE);
		}
		else
			assert(FALSE); // don't expect ArbData to ever be NULL, but...
		
		
		delete clickedItems;
		
		seq_data->clickedItems = NULL;
		seq_data->sendingClickedItems = FALSE;
		seq_data->modifiers = PF_Mod_NONE;
		
		PF_UNLOCK_HANDLE(in_data->sequence_data);
	}
	
	
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
	if(in_data->sequence_data)
	{
		// Selecting Step 1: Store the click coordinates, tell comp to re-render
		
		// See the section on "Custom UI Implementation for Color Sampling, Using Keyframes" in the SDK guide
	
		//CryptomatteArbitraryData *arb_data = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(params[CRYPTO_DATA]->u.arb_d.value);
		CryptomatteSequenceData *seq_data = (CryptomatteSequenceData *)PF_LOCK_HANDLE(in_data->sequence_data);
		
		if(seq_data)
		{
			PF_Point mouse_downPt = *(reinterpret_cast<PF_Point*>(&event_extra->u.do_click.screen_point));
			
			PF_FixedPoint mouse_downFixPt;
			mouse_downFixPt.x = INT2FIX(mouse_downPt.h);
			mouse_downFixPt.y = INT2FIX(mouse_downPt.v);
			
			event_extra->cbs.frame_to_source(event_extra->cbs.refcon, event_extra->contextH, &mouse_downFixPt);
			
			if((*event_extra->contextH)->w_type == PF_Window_COMP)
				event_extra->cbs.comp_to_layer(event_extra->cbs.refcon, event_extra->contextH, in_data->current_time, in_data->time_scale, &mouse_downFixPt);
			
			seq_data->clickPoint.h = FIX2INT(mouse_downFixPt.x);
			seq_data->clickPoint.v = FIX2INT(mouse_downFixPt.y);
			
			seq_data->sendingClickPoint = TRUE;
			
			seq_data->modifiers = event_extra->u.do_click.modifiers;
			
			//event_extra->evt_out_flags |= PF_EO_ALWAYS_UPDATE;
			
			out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
			
			if(!(seq_data->modifiers & PF_Mod_CAPS_LOCK_KEY))
				seq_data->selectionChanged = TRUE;
			
			event_extra->u.do_click.send_drag = TRUE; // drag not really working now
		}
		
		//PF_UNLOCK_HANDLE(params[CRYPTO_DATA]->u.arb_d.value);
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
		extra->evt_out_flags = PF_EO_NONE;
		
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
					extra->evt_out_flags |= PF_EO_HANDLED_EVENT;
				}
				else if((*extra->contextH)->w_type != PF_Window_NONE)
				{
					err = DoClick(in_data, out_data, params, output, extra);
					extra->evt_out_flags |= PF_EO_HANDLED_EVENT;
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
					if((extra->u.adjust_cursor.modifiers & ALL_MODIFIERS) == 0)
						extra->u.adjust_cursor.set_cursor = PF_Cursor_HOLLOW_ARROW;
					else if(extra->u.adjust_cursor.modifiers & PF_Mod_SHIFT_KEY)
						extra->u.adjust_cursor.set_cursor = PF_Cursor_HOLLOW_ARROW_PLUS;
					else
						extra->u.adjust_cursor.set_cursor = PF_Cursor_SCISSORS;
				}
				
				extra->evt_out_flags |= PF_EO_HANDLED_EVENT;
				break;
		}
	}
	
	return err;
}
