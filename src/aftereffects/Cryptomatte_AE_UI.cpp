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
			
			/*
			bot.MoveTo(panel_left, panel_top + bot.FontSize());
			bot.SetColor(PF_App_Color_TEXT_DISABLED);
			bot.DrawString("Layer:", kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_EndEllipsis, panel_width);

			bot.Move(0, labelSpace);
			bot.SetColor(PF_App_Color_TEXT);
			bot.DrawString(GetLayer(arb_data), kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_EndEllipsis, panel_width);
			
			bot.Move(0, itemSpace);
			bot.SetColor(PF_App_Color_TEXT_DISABLED);
			bot.DrawString("Selection:", kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_EndEllipsis, panel_width);

			bot.Move(0, labelSpace);
			bot.SetColor(PF_App_Color_TEXT);
			bot.DrawString(GetSelection(arb_data), kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_EndEllipsis, panel_width);

			bot.Move(0, itemSpace);
			bot.SetColor(PF_App_Color_TEXT_DISABLED);
			bot.DrawString("Manifest:", kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_EndEllipsis, panel_width);

			bot.Move(0, labelSpace);
			bot.SetColor(PF_App_Color_GRAY);
			bot.DrawString(GetManifest(arb_data), kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_EndEllipsis, panel_width);
			*/
			
			bot.SetColor(PF_App_Color_TEXT_DISABLED);
			
			bot.MoveTo(panel_left, panel_top + bot.FontSize());
			bot.DrawString("Layer:", kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_EndEllipsis, panel_width);

			bot.Move(0, itemSpace);
			bot.DrawString("Selection:", kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_EndEllipsis, panel_width);
			
			bot.Move(0, itemSpace);
			bot.DrawString("Manifest:", kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_EndEllipsis, panel_width);

			
			const float valuePos = (panel_left + (bot.FontSize() * 6));
			const float valueSpace = (event_extra->effect_win.current_frame.right - valuePos);
			
			bot.SetColor(PF_App_Color_TEXT);
			
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
		CryptomatteArbitraryData *arb_data = (CryptomatteArbitraryData *)PF_LOCK_HANDLE(params[CRYPTO_DATA]->u.arb_d.value);
		CryptomatteSequenceData *seq_data = (CryptomatteSequenceData *)PF_LOCK_HANDLE(in_data->sequence_data);
		
		if(arb_data && seq_data && seq_data->context)
		{
			CryptomatteContext *context = (CryptomatteContext *)seq_data->context;
			
			if( context->Valid() )
			{
				PF_Point mouse_downPt = *(reinterpret_cast<PF_Point*>(&event_extra->u.do_click.screen_point));
				
				PF_FixedPoint mouse_downFixPt;
				mouse_downFixPt.x = INT2FIX(mouse_downPt.h);
				mouse_downFixPt.y = INT2FIX(mouse_downPt.v);
				
				event_extra->cbs.frame_to_source(event_extra->cbs.refcon, event_extra->contextH, &mouse_downFixPt);
				
				event_extra->cbs.comp_to_layer(event_extra->cbs.refcon, event_extra->contextH, in_data->current_time, in_data->time_scale, &mouse_downFixPt);
				
				mouse_downPt.h = FIX2INT(mouse_downFixPt.x);
				mouse_downPt.v = FIX2INT(mouse_downFixPt.y);
				
				
				if(mouse_downPt.h > 0 && mouse_downPt.h < context->Width() &&
					mouse_downPt.v > 0 && mouse_downPt.v < context->Height())
				{
					std::set<std::string> clickedItems = context->GetItems(mouse_downPt.h, mouse_downPt.v);
					
					std::set<std::string> newSelection;
					
					if((event_extra->u.do_click.modifiers & (PF_Mod_SHIFT_KEY | PF_Mod_OPT_ALT_KEY)) == 0) // select
					{
						for(std::set<std::string>::iterator i = clickedItems.begin(); i != clickedItems.end(); ++i)
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
							
						if(event_extra->u.do_click.modifiers & PF_Mod_SHIFT_KEY) // add
						{
							for(std::set<std::string>::const_iterator i = clickedItems.begin(); i != clickedItems.end(); ++i)
							{
								std::string item = *i;
								
								if(item.find(" ") != std::string::npos)
									item = "\"" + item + "\"";
								
								newSelection.insert(item);
							}
						}
						else // remove
						{
							for(std::set<std::string>::const_iterator i = clickedItems.begin(); i != clickedItems.end(); ++i)
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
					
					
					SetArbSelection(in_data, &params[CRYPTO_DATA]->u.arb_d.value, selectedString);
					
					params[CRYPTO_DATA]->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
					
					if(!(event_extra->u.do_click.modifiers & PF_Mod_CAPS_LOCK_KEY))
						seq_data->selectionChanged = TRUE;
				}
			}
		}
		
		PF_UNLOCK_HANDLE(params[CRYPTO_DATA]->u.arb_d.value);
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
					if((extra->u.adjust_cursor.modifiers & (PF_Mod_SHIFT_KEY | PF_Mod_OPT_ALT_KEY)) == 0)
						extra->u.adjust_cursor.set_cursor = PF_Cursor_HOLLOW_ARROW;
					else if(extra->u.adjust_cursor.modifiers & (PF_Mod_SHIFT_KEY))
						extra->u.adjust_cursor.set_cursor = PF_Cursor_HOLLOW_ARROW_PLUS;
					else
						extra->u.adjust_cursor.set_cursor = PF_Cursor_SCISSORS;
				}
				
				extra->evt_out_flags = PF_EO_HANDLED_EVENT;
				break;
		}
	}
	
	return err;
}
