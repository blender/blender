/**
 * $Id:
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_listBase.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_userdef_types.h"	/* U.flag & TWOBUTTONMOUSE */

#include "MEM_guardedalloc.h"

#include "GHOST_C-api.h"

#include "BLI_blenlib.h"

#include "BKE_blender.h"
#include "BKE_global.h"

#include "ED_screen.h"
#include "ED_area.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_window.h"
#include "wm_event_system.h"
#include "wm_event_types.h"

/* ************ event management ************** */

static void wm_event_add(wmWindow *win, wmEvent *event_to_add)
{
	wmEvent *event= MEM_callocN(sizeof(wmEvent), "event");
	
	*event= *event_to_add;
	BLI_addtail(&win->queue, event);
}

wmEvent *wm_event_next(wmWindow *win)
{
	wmEvent *event= win->queue.first;

	if(event) BLI_remlink(&win->queue, event);
	return event;
}

static void wm_event_free(wmEvent *event)
{
	if(event->customdata) MEM_freeN(event->customdata);
	MEM_freeN(event);
}

void wm_event_free_all(wmWindow *win)
{
	wmEvent *event;
	
	while((event= win->queue.first)) {
		BLI_remlink(&win->queue, event);
		wm_event_free(event);
	}
}

/* ********************* notifiers, listeners *************** */

/* win and swinid are optional context limitors */
void WM_event_add_notifier(wmWindowManager *wm, wmWindow *window, int swinid, int type, int value)
{
	wmNotifier *note= MEM_callocN(sizeof(wmNotifier), "notifier");
	
	BLI_addtail(&wm->queue, note);
	
	note->window= window;
	note->swinid= swinid;
	note->type= type;
	note->value= value;
}

static wmNotifier *wm_notifier_next(wmWindowManager *wm)
{
	wmNotifier *note= wm->queue.first;
	
	if(note) BLI_remlink(&wm->queue, note);
	return note;
}

/* called in mainloop */
void wm_event_do_notifiers(bContext *C)
{
	wmNotifier *note;
	
	while( (note=wm_notifier_next(C->wm)) ) {
		wmWindow *win;
		
		for(win= C->wm->windows.first; win; win= win->next) {
			ScrArea *sa;
			
			if(note->window && note->window!=win)
				continue;
			if(win->screen==NULL)
				continue;
			printf("notifier win %d screen %s\n", win->winid, win->screen->id.name+2);
			ED_screen_do_listen(win->screen, note);
			
			for(sa= win->screen->areabase.first; sa; sa= sa->next) {
				ARegion *ar= sa->regionbase.first;
				
				for(; ar; ar= ar->next) {
					if(note->swinid && note->swinid!=ar->swinid)
						continue;
					ED_region_do_listen(ar, note);
				}
			}
		}
		MEM_freeN(note);
	}	
}

/* quick test to prevent changing window drawable */
static int wm_draw_update_test_window(wmWindow *win)
{
	ScrArea *sa;
	
	if(win->screen->do_refresh)
		return 1;
	if(win->screen->do_draw)
		return 1;
	
	for(sa= win->screen->areabase.first; sa; sa= sa->next) {
		ARegion *ar= sa->regionbase.first;
		
		for(; ar; ar= ar->next) {
			/* cached notifiers */
			if(ar->do_refresh)
				return 1;
			if(ar->swinid && ar->do_draw)
				return 1;
		}
	}
	return 0;
}

void wm_draw_update(bContext *C)
{
	wmWindow *win;
	
	for(win= C->wm->windows.first; win; win= win->next) {
		if(wm_draw_update_test_window(win)) {
			ScrArea *sa;
			
			/* sets context window+screen */
			wm_window_make_drawable(C, win);
			
			/* notifiers for screen redraw */
			if(win->screen->do_refresh)
				ED_screen_refresh(C->wm, win);
			if(win->screen->do_draw)
				ED_screen_draw(win);
			
			for(sa= win->screen->areabase.first; sa; sa= sa->next) {
				ARegion *ar= sa->regionbase.first;
				int hasdrawn= 0;
				
				for(; ar; ar= ar->next) {
					hasdrawn |= ar->do_draw;
					
					/* cached notifiers */
					if(ar->do_refresh)
						ED_region_do_refresh(C, ar);
					
					if(ar->swinid && ar->do_draw)
						ED_region_do_draw(C, ar);
				}
			}
			wm_window_swap_buffers(win);
		}
	}
}

/* ********************* handlers *************** */

/* not handler itself */
static void wm_event_free_handler(wmEventHandler *handler)
{
	if(handler->op)
		MEM_freeN(handler->op);
}

void wm_event_free_handlers(ListBase *lb)
{
	wmEventHandler *handler;
	
	for(handler= lb->first; handler; handler= handler->next)
		wm_event_free_handler(handler);
	
	BLI_freelistN(lb);
}

static int wm_eventmatch(wmEvent *winevent, wmKeymapItem *km)
{
	if(winevent->type!=km->type) return 0;
	
	if(km->val) /* KM_PRESS, KM_RELEASE */
		if(winevent->val!=km->val-1) return 0;
	
	if(winevent->shift!=km->shift) return 0;
	if(winevent->ctrl!=km->ctrl) return 0;
	if(winevent->alt!=km->alt) return 0;
	if(winevent->oskey!=km->oskey) return 0;
	if(km->keymodifier)
		if(winevent->keymodifier!=km->keymodifier) return 0;
	
	/* optional boundbox */
	
	return 1;
}

static int wm_handler_operator_call(bContext *C, wmEventHandler *handler, wmEvent *event)
{
	int retval= 0;
	
	/* derived, modal or blocking operator */
	if(handler->op) {
		if(handler->op->type->modal)
			retval= handler->op->type->modal(C, handler->op, event);
		else
			printf("wm_handler_operator_call error\n");
	}
	else {
		wmOperatorType *ot= WM_operatortype_find(event->keymap_idname);
		if(ot) {
			if(ot->poll==NULL || ot->poll(C)) {
				/* operator on stack, register or new modal handle malloc-copies */
				wmOperator op;
				
				memset(&op, 0, sizeof(wmOperator));
				op.type= ot;

				if(op.type->invoke)
					retval= (*op.type->invoke)(C, &op, event);
				else if(&op.type->exec)
					retval= op.type->exec(C, &op);
				
				if( ot->flag & OPTYPE_REGISTER)
					wm_operator_register(C->wm, &op);
			}
		}
	}
	if(retval)
		return WM_HANDLER_BREAK;
	
	return WM_HANDLER_CONTINUE;
}

static int wm_handlers_do(bContext *C, wmEvent *event, ListBase *handlers)
{
	wmEventHandler *handler;
	int action= WM_HANDLER_CONTINUE;
	
	if(handlers==NULL) return action;
	
	for(handler= handlers->first; handler; handler= handler->next) {
		if(handler->keymap) {
			wmKeymapItem *km;
			
			for(km= handler->keymap->first; km; km= km->next) {
				if(wm_eventmatch(event, km)) {
					
					if(event->type!=MOUSEMOVE)
						printf("handle evt %d win %d op %s\n", event->type, C->window->winid, km->idname);
					
					event->keymap_idname= km->idname;	/* weak, but allows interactive callback to not use rawkey */
					
					action= wm_handler_operator_call(C, handler, event);
				}
			}
			if(action==WM_HANDLER_BREAK)
				break;
		}
		else {
			/* modal, swallows all */
			action= wm_handler_operator_call(C, handler, event);
			if(action==WM_HANDLER_BREAK)
				break;
		}
		
		/* modal+blocking handler */
		if(handler->flag & WM_HANDLER_BLOCKING)
			action= WM_HANDLER_BREAK;
	}
	return action;
}

static int wm_event_inside_i(wmEvent *event, rcti *rect)
{
	return BLI_in_rcti(rect, event->x, event->y);
}


/* called in main loop */
/* goes over entire hierarchy:  events -> window -> screen -> area -> region */
void wm_event_do_handlers(bContext *C)
{
	wmWindow *win;
	
	for(win= C->wm->windows.first; win; win= win->next) {
		wmEvent *event;
		
		if( win->screen==NULL )
			wm_event_free_all(win);
		
		while( (event=wm_event_next(win)) ) {
			int action;
			
			/* MVC demands to not draw in event handlers... for now we leave it */
			/* it also updates context (win, screen) */
			wm_window_make_drawable(C, win);
			
			action= wm_handlers_do(C, event, &win->handlers);
			
			if(action==WM_HANDLER_CONTINUE) {
				ScrArea *sa= win->screen->areabase.first;
				
				for(; sa; sa= sa->next) {
					if(wm_event_inside_i(event, &sa->totrct)) {
						
						C->curarea= sa;
						action= wm_handlers_do(C, event, &sa->handlers);
						if(action==WM_HANDLER_CONTINUE) {
							ARegion *ar= sa->regionbase.first;
							
							for(; ar; ar= ar->next) {
								if(wm_event_inside_i(event, &ar->winrct)) {
									C->region= ar;
									action= wm_handlers_do(C, event, &ar->handlers);
									if(action==WM_HANDLER_BREAK)
										break;
								}
							}
						}
						if(action==WM_HANDLER_BREAK)
							break;
					}
				}
			}
			wm_event_free(event);
		}
	}
}

/* lets not expose struct outside wm? */
void WM_event_set_handler_flag(wmEventHandler *handler, int flag)
{
	handler->flag= flag;
}

wmEventHandler *WM_event_add_modal_handler(ListBase *handlers, wmOperator *op)
{
	/* debug test; operator not in registry */
	if(op->type->flag & OPTYPE_REGISTER) {
		printf("error: handler (%s) cannot be modal, was registered\n", op->type->idname);
	}
	else {
		wmEventHandler *handler= MEM_callocN(sizeof(wmEventHandler), "event handler");
		wmOperator *opc= MEM_mallocN(sizeof(wmOperator), "operator modal");
		
		BLI_addhead(handlers, handler);
		*opc= *op;
		handler->op= opc;
		
		return handler;
	}
	return NULL;
}

void WM_event_remove_modal_handler(ListBase *handlers, wmOperator *op)
{
	wmEventHandler *handler;
	
	for(handler= handlers->first; handler; handler= handler->next) {
		if(handler->op==op) {
			BLI_remlink(handlers, handler);
			wm_event_free_handler(handler);
			MEM_freeN(handler);
			break;
		}
	}
}

wmEventHandler *WM_event_add_keymap_handler(ListBase *keymap, ListBase *handlers)
{
	wmEventHandler *handler= MEM_callocN(sizeof(wmEventHandler), "event handler");
	
	BLI_addtail(handlers, handler);
	handler->keymap= keymap;
	
	return handler;
}


/* ********************* ghost stuff *************** */

static int convert_key(GHOST_TKey key) 
{
	if (key>=GHOST_kKeyA && key<=GHOST_kKeyZ) {
		return (AKEY + ((int) key - GHOST_kKeyA));
	} else if (key>=GHOST_kKey0 && key<=GHOST_kKey9) {
		return (ZEROKEY + ((int) key - GHOST_kKey0));
	} else if (key>=GHOST_kKeyNumpad0 && key<=GHOST_kKeyNumpad9) {
		return (PAD0 + ((int) key - GHOST_kKeyNumpad0));
	} else if (key>=GHOST_kKeyF1 && key<=GHOST_kKeyF12) {
		return (F1KEY + ((int) key - GHOST_kKeyF1));
	} else {
		switch (key) {
			case GHOST_kKeyBackSpace:		return BACKSPACEKEY;
			case GHOST_kKeyTab:				return TABKEY;
			case GHOST_kKeyLinefeed:		return LINEFEEDKEY;
			case GHOST_kKeyClear:			return 0;
			case GHOST_kKeyEnter:			return RETKEY;
				
			case GHOST_kKeyEsc:				return ESCKEY;
			case GHOST_kKeySpace:			return SPACEKEY;
			case GHOST_kKeyQuote:			return QUOTEKEY;
			case GHOST_kKeyComma:			return COMMAKEY;
			case GHOST_kKeyMinus:			return MINUSKEY;
			case GHOST_kKeyPeriod:			return PERIODKEY;
			case GHOST_kKeySlash:			return SLASHKEY;
				
			case GHOST_kKeySemicolon:		return SEMICOLONKEY;
			case GHOST_kKeyEqual:			return EQUALKEY;
				
			case GHOST_kKeyLeftBracket:		return LEFTBRACKETKEY;
			case GHOST_kKeyRightBracket:	return RIGHTBRACKETKEY;
			case GHOST_kKeyBackslash:		return BACKSLASHKEY;
			case GHOST_kKeyAccentGrave:		return ACCENTGRAVEKEY;
				
			case GHOST_kKeyLeftShift:		return LEFTSHIFTKEY;
			case GHOST_kKeyRightShift:		return RIGHTSHIFTKEY;
			case GHOST_kKeyLeftControl:		return LEFTCTRLKEY;
			case GHOST_kKeyRightControl:	return RIGHTCTRLKEY;
			case GHOST_kKeyCommand:			return COMMANDKEY;
			case GHOST_kKeyLeftAlt:			return LEFTALTKEY;
			case GHOST_kKeyRightAlt:		return RIGHTALTKEY;
				
			case GHOST_kKeyCapsLock:		return CAPSLOCKKEY;
			case GHOST_kKeyNumLock:			return 0;
			case GHOST_kKeyScrollLock:		return 0;
				
			case GHOST_kKeyLeftArrow:		return LEFTARROWKEY;
			case GHOST_kKeyRightArrow:		return RIGHTARROWKEY;
			case GHOST_kKeyUpArrow:			return UPARROWKEY;
			case GHOST_kKeyDownArrow:		return DOWNARROWKEY;
				
			case GHOST_kKeyPrintScreen:		return 0;
			case GHOST_kKeyPause:			return PAUSEKEY;
				
			case GHOST_kKeyInsert:			return INSERTKEY;
			case GHOST_kKeyDelete:			return DELKEY;
			case GHOST_kKeyHome:			return HOMEKEY;
			case GHOST_kKeyEnd:				return ENDKEY;
			case GHOST_kKeyUpPage:			return PAGEUPKEY;
			case GHOST_kKeyDownPage:		return PAGEDOWNKEY;
				
			case GHOST_kKeyNumpadPeriod:	return PADPERIOD;
			case GHOST_kKeyNumpadEnter:		return PADENTER;
			case GHOST_kKeyNumpadPlus:		return PADPLUSKEY;
			case GHOST_kKeyNumpadMinus:		return PADMINUS;
			case GHOST_kKeyNumpadAsterisk:	return PADASTERKEY;
			case GHOST_kKeyNumpadSlash:		return PADSLASHKEY;
				
			case GHOST_kKeyGrLess:		    return GRLESSKEY; 
				
			default:
				return UNKNOWNKEY;	/* GHOST_kKeyUnknown */
		}
	}
}

/* adds customdata to event */
static void update_tablet_data(wmWindow *win, wmEvent *event)
{
	const GHOST_TabletData *td= GHOST_GetTabletData(win->ghostwin);
	
	/* if there's tablet data from an active tablet device then add it */
	if ((td != NULL) && td->Active) {
		struct wmTabletData *wmtab= MEM_mallocN(sizeof(wmTabletData), "customdata tablet");
		
		wmtab->Active = td->Active;
		wmtab->Pressure = td->Pressure;
		wmtab->Xtilt = td->Xtilt;
		wmtab->Ytilt = td->Ytilt;
		
		event->custom= EVT_TABLET;
		event->customdata= wmtab;
	} 
}


/* windows store own event queues, no bContext here */
void wm_event_add_ghostevent(wmWindow *win, int type, void *customdata)
{
	wmEvent event, *evt= win->eventstate;
	
	/* initialize and copy state (only mouse x y and modifiers) */
	event= *evt;
	
	switch (type) {
		/* mouse move */
		case GHOST_kEventCursorMove: {
			if(win->active) {
				GHOST_TEventCursorData *cd= customdata;
				int cx, cy;

				GHOST_ScreenToClient(win->ghostwin, cd->x, cd->y, &cx, &cy);
				
				event.type= MOUSEMOVE;
				event.x= evt->x= cx;
				event.y= evt->y= (win->sizey-1) - cy;
				
				ED_screen_set_subwinactive(win);	/* state variables in screen */
				
				update_tablet_data(win, &event);
				wm_event_add(win, &event);
			}
			break;
		}
		/* mouse button */
		case GHOST_kEventButtonDown:
		case GHOST_kEventButtonUp: {
			GHOST_TEventButtonData *bd= customdata;
			event.val= (type==GHOST_kEventButtonDown);
			
			if (bd->button == GHOST_kButtonMaskLeft)
				event.type= LEFTMOUSE;
			else if (bd->button == GHOST_kButtonMaskRight)
				event.type= RIGHTMOUSE;
			else
				event.type= MIDDLEMOUSE;
			
			if(event.val)
				event.keymodifier= evt->keymodifier= event.type;
			else
				event.keymodifier= evt->keymodifier= 0;
			
			update_tablet_data(win, &event);
			wm_event_add(win, &event);
			
			break;
		}
		/* keyboard */
		case GHOST_kEventKeyDown:
		case GHOST_kEventKeyUp: {
			GHOST_TEventKeyData *kd= customdata;
			event.type= convert_key(kd->key);
			event.ascii= kd->ascii;
			event.val= (type==GHOST_kEventKeyDown);
			
			/* modifiers */
			if (event.type==LEFTSHIFTKEY || event.type==RIGHTSHIFTKEY) {
				event.shift= evt->shift= event.val;
			} else if (event.type==LEFTCTRLKEY || event.type==RIGHTCTRLKEY) {
				event.ctrl= evt->ctrl= event.val;
			} else if (event.type==LEFTALTKEY || event.type==RIGHTALTKEY) {
				event.alt= evt->alt= event.val;
			} else if (event.type==COMMANDKEY) {
				event.oskey= evt->oskey= event.val;
			}
			
			wm_event_add(win, &event);
			
			break;
		}
			
		case GHOST_kEventWheel:	{
			GHOST_TEventWheelData* wheelData = customdata;
			
			if (wheelData->z > 0)
				event.type= WHEELUPMOUSE;
			else
				event.type= WHEELDOWNMOUSE;
			
			event.val= wheelData->z;	/* currently -1 or +1, see ghost for improvements here... */
			wm_event_add(win, &event);
			
			break;
		}
		case GHOST_kEventUnknown:
		case GHOST_kNumEventTypes:
			break;
	}
}

