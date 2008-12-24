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
#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_global.h"

#include "ED_screen.h"
#include "ED_space_api.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_window.h"
#include "wm_event_system.h"
#include "wm_event_types.h"

/* ************ event management ************** */

void wm_event_add(wmWindow *win, wmEvent *event_to_add)
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
	if(event->customdata && event->customdatafree)
		MEM_freeN(event->customdata);
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

/* XXX: in future, which notifiers to send to other windows? */
void WM_event_add_notifier(bContext *C, int type, int value, void *data)
{
	wmNotifier *note= MEM_callocN(sizeof(wmNotifier), "notifier");
	
	BLI_addtail(&CTX_wm_manager(C)->queue, note);
	
	note->window= CTX_wm_window(C);
	if(CTX_wm_region(C))
		note->swinid= CTX_wm_region(C)->swinid;
	note->type= type;
	note->value= value;
	note->data= data;
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
	wmWindowManager *wm= CTX_wm_manager(C);
	wmNotifier *note;
	
	while( (note=wm_notifier_next(wm)) ) {
		wmWindow *win;
		
		for(win= wm->windows.first; win; win= win->next) {
			ScrArea *sa;
			ARegion *ar;

			/* XXX context in notifiers? */
			CTX_wm_window_set(C, win);

			/* printf("notifier win %d screen %s\n", win->winid, win->screen->id.name+2); */
			ED_screen_do_listen(win, note);

			for(ar=win->screen->regionbase.first; ar; ar= ar->next) {
				ED_region_do_listen(ar, note);
			}
			
			for(sa= win->screen->areabase.first; sa; sa= sa->next) {
				for(ar=sa->regionbase.first; ar; ar= ar->next) {
					ED_region_do_listen(ar, note);
				}
			}

			CTX_wm_window_set(C, NULL);
		}
		
		MEM_freeN(note);
	}	
}

/* mark regions to redraw if overlapped with rect */
static void wm_flush_regions(bScreen *screen, rcti *dirty)
{
	ScrArea *sa;
	ARegion *ar;
	
	for(ar= screen->regionbase.first; ar; ar= ar->next)
		if(BLI_isect_rcti(dirty, &ar->winrct, NULL))
			ar->do_draw= 1;

	for(sa= screen->areabase.first; sa; sa= sa->next)
		for(ar= sa->regionbase.first; ar; ar= ar->next)
			if(BLI_isect_rcti(dirty, &ar->winrct, NULL))
				ar->do_draw= 1;
}

/* all the overlay management, menus, actionzones, region tabs, etc */
static void wm_flush_draw_update(bContext *C)
{
	ARegion *ar;
	bScreen *screen= CTX_wm_screen(C);
	
	/* flush redraws of screen regions (menus) down */
	for(ar= screen->regionbase.last; ar; ar= ar->prev) {
		if(ar->swinid && ar->do_draw) {
			wm_flush_regions(screen, &ar->winrct);
		}
	}
	
	/* sets redraws for Azones, future region tabs, etc */
	ED_area_overdraw_flush(C);
}

/* quick test to prevent changing window drawable */
static int wm_draw_update_test_window(wmWindow *win)
{
	ScrArea *sa;
	ARegion *ar;
	
	if(win->screen->do_refresh)
		return 1;
	if(win->screen->do_draw)
		return 1;
	if(win->screen->do_gesture)
		return 1;
	
	for(ar= win->screen->regionbase.first; ar; ar= ar->next)
		if(ar->swinid && ar->do_draw)
			return 1;
		
	for(sa= win->screen->areabase.first; sa; sa= sa->next)
		for(ar=sa->regionbase.first; ar; ar= ar->next)
			if(ar->swinid && ar->do_draw)
				return 1;

	return 0;
}

void wm_draw_update(bContext *C)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmWindow *win;
	
	for(win= wm->windows.first; win; win= win->next) {
		if(wm_draw_update_test_window(win)) {
			ScrArea *sa;
			ARegion *ar;

			CTX_wm_window_set(C, win);
			
			/* sets context window+screen */
			wm_window_make_drawable(C, win);
			
			/* notifiers for screen redraw */
			if(win->screen->do_refresh)
				ED_screen_refresh(wm, win);
			
			/* flush draw updates for multiple layers */
			wm_flush_draw_update(C);

			for(sa= win->screen->areabase.first; sa; sa= sa->next) {
				
				CTX_wm_area_set(C, sa);
				
				for(ar=sa->regionbase.first; ar; ar= ar->next) {
					CTX_wm_region_set(C, ar);
					
					if(ar->swinid && ar->do_draw)
						ED_region_do_draw(C, ar);
					
					CTX_wm_region_set(C, NULL);
				}
				
				CTX_wm_area_set(C, NULL);
			}
			
			/* move this here so we can do area 'overlay' drawing */
			if(win->screen->do_draw)
				ED_screen_draw(win);
			
			ED_area_overdraw(C);

			/* regions are menus here */
			for(ar=win->screen->regionbase.first; ar; ar= ar->next) {
				CTX_wm_region_set(C, ar);
				
				if(ar->swinid && ar->do_draw)
					ED_region_do_draw(C, ar);

				CTX_wm_region_set(C, NULL);
			}
			
			if(win->screen->do_gesture)
				wm_gesture_draw(win);

			wm_window_swap_buffers(win);

			CTX_wm_window_set(C, NULL);
		}
	}
}

/* ********************* operators ******************* */

/* for running operators with frozen context (modal handlers, menus) */
int WM_operator_call(bContext *C, wmOperator *op)
{
	int retval= OPERATOR_CANCELLED;
	
	if(op->type->exec)
		retval= op->type->exec(C, op);
	
	if((retval & OPERATOR_FINISHED) && (op->type->flag & OPTYPE_REGISTER)) {
		wm_operator_register(CTX_wm_manager(C), op);
	}
	else
		WM_operator_free(op);	
	
	return retval;
}

static int wm_operator_invoke(bContext *C, wmOperatorType *ot, wmEvent *event, IDProperty *properties)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	int retval= OPERATOR_PASS_THROUGH;

	if(ot->poll==NULL || ot->poll(C)) {
		wmOperator *op= MEM_callocN(sizeof(wmOperator), ot->idname);	/* XXX operatortype names are static still. for debug */

		if(properties)
			op->properties= IDP_CopyProperty(properties);

		/* XXX adding new operator could be function, only happens here now */
		op->type= ot;
		BLI_strncpy(op->idname, ot->idname, OP_MAX_TYPENAME);
		
		op->ptr= MEM_callocN(sizeof(PointerRNA), "wmOperatorPtrRNA");
		RNA_pointer_create(&RNA_WindowManager, &wm->id, ot->srna, &op->properties, op->ptr);

		if(op->type->invoke)
			retval= (*op->type->invoke)(C, op, event);
		else if(op->type->exec)
			retval= op->type->exec(C, op);

		if((retval & OPERATOR_FINISHED) && (ot->flag & OPTYPE_REGISTER)) {
			wm_operator_register(wm, op);
		}
		else if(!(retval & OPERATOR_RUNNING_MODAL)) {
			WM_operator_free(op);
		}
	}

	return retval;
}

/* invokes operator in context */
int WM_operator_name_call(bContext *C, const char *opstring, int context, IDProperty *properties)
{
	wmOperatorType *ot= WM_operatortype_find(opstring);
	wmWindow *window= CTX_wm_window(C);
	int retval;

	/* dummie test */
	if(ot && C && window) {
		if(context == WM_OP_REGION_WIN) {
			/* forces event to go to the region window, for header menus */
			ARegion *ar= CTX_wm_region(C);
			ScrArea *area= CTX_wm_area(C);
			
			if(area) {
				ARegion *ar1= area->regionbase.first;
				for(; ar1; ar1= ar1->next)
					if(ar1->regiontype==RGN_TYPE_WINDOW)
						break;
				if(ar1)
					CTX_wm_region_set(C, ar1);
			}
			
			retval= wm_operator_invoke(C, ot, window->eventstate, properties);
			
			/* set region back */
			CTX_wm_region_set(C, ar);
			
			return retval;
		}
		else if(context == WM_OP_AREA) {
			/* remove region from context */
			ARegion *ar= CTX_wm_region(C);

			CTX_wm_region_set(C, NULL);
			retval= wm_operator_invoke(C, ot, window->eventstate, properties);
			CTX_wm_region_set(C, ar);

			return retval;
		}
		else if(context == WM_OP_SCREEN) {
			/* remove region + area from context */
			ARegion *ar= CTX_wm_region(C);
			ScrArea *area= CTX_wm_area(C);

			CTX_wm_region_set(C, NULL);
			CTX_wm_area_set(C, NULL);
			retval= wm_operator_invoke(C, ot, window->eventstate, properties);
			CTX_wm_region_set(C, ar);
			CTX_wm_area_set(C, area);

			return retval;
		}
		else
			return wm_operator_invoke(C, ot, window->eventstate, properties);
	}
	
	return 0;
}

/* ********************* handlers *************** */


/* not handler itself, is called by UI to move handlers to other queues, so don't close modal ones */
static void wm_event_free_handler(wmEventHandler *handler)
{
	
}

/* called on exit or remove area, only here call cancel callback */
void WM_event_remove_handlers(bContext *C, ListBase *handlers)
{
	wmEventHandler *handler;
	
	/* C is zero on freeing database, modal handlers then already were freed */
	while((handler=handlers->first)) {
		BLI_remlink(handlers, handler);
		
		if(handler->op) {
			if(handler->op->type->cancel) {
				ScrArea *area= CTX_wm_area(C);
				ARegion *region= CTX_wm_region(C);
				
				CTX_wm_area_set(C, handler->op_area);
				CTX_wm_region_set(C, handler->op_region);

				handler->op->type->cancel(C, handler->op);

				CTX_wm_area_set(C, area);
				CTX_wm_region_set(C, region);
			}

			WM_operator_free(handler->op);
		}
		else if(handler->ui_remove) {
			ScrArea *area= CTX_wm_area(C);
			ARegion *region= CTX_wm_region(C);
			
			if(handler->ui_area) CTX_wm_area_set(C, handler->ui_area);
			if(handler->ui_region) CTX_wm_region_set(C, handler->ui_region);

			handler->ui_remove(C, handler->ui_userdata);

			CTX_wm_area_set(C, area);
			CTX_wm_region_set(C, region);
		}

		wm_event_free_handler(handler);
		MEM_freeN(handler);
	}
}

static int wm_eventmatch(wmEvent *winevent, wmKeymapItem *kmi)
{
	int kmitype= kmi->type;
	
	/* first do default mappings */
	if(kmitype==SELECTMOUSE) {
		if(U.flag & USER_LMOUSESELECT)
			kmitype= LEFTMOUSE;
		else
			kmitype= RIGHTMOUSE;
	}
	
	/* the matching rules */
	if(winevent->type!=kmitype) return 0;
	
	if(kmi->val!=KM_ANY)
		if(winevent->val!=kmi->val) return 0;
	if(kmi->shift!=KM_ANY)
		if(winevent->shift!=kmi->shift) return 0;
	if(kmi->ctrl!=KM_ANY)
		if(winevent->ctrl!=kmi->ctrl) return 0;
	if(kmi->alt!=KM_ANY)
		if(winevent->alt!=kmi->alt) return 0;
	if(kmi->oskey!=KM_ANY)
		if(winevent->oskey!=kmi->oskey) return 0;
	if(kmi->keymodifier)
		if(winevent->keymodifier!=kmi->keymodifier) return 0;
	
	
	return 1;
}

static int wm_event_always_pass(wmEvent *event)
{
	/* some events we always pass on, to ensure proper communication */
	return (event->type == TIMER);
}

/* Warning: this function removes a modal handler, when finished */
static int wm_handler_operator_call(bContext *C, ListBase *handlers, wmEventHandler *handler, wmEvent *event, IDProperty *properties)
{
	int retval= OPERATOR_PASS_THROUGH;
	
	/* derived, modal or blocking operator */
	if(handler->op) {
		wmOperator *op= handler->op;
		wmOperatorType *ot= op->type;

		if(ot->modal) {
			/* we set context to where modal handler came from */
			ScrArea *area= CTX_wm_area(C);
			ARegion *region= CTX_wm_region(C);
			
			CTX_wm_area_set(C, handler->op_area);
			CTX_wm_region_set(C, handler->op_region);
			
			retval= ot->modal(C, op, event);

			/* putting back screen context */
			if((retval & OPERATOR_PASS_THROUGH) || wm_event_always_pass(event)) {
				CTX_wm_area_set(C, area);
				CTX_wm_region_set(C, region);
			}
			else {
				/* this special cases is for areas and regions that get removed */
				CTX_wm_area_set(C, NULL);
				CTX_wm_region_set(C, NULL);
			}
			
			if((retval & OPERATOR_FINISHED) && (ot->flag & OPTYPE_REGISTER)) {
				wm_operator_register(CTX_wm_manager(C), op);
				handler->op= NULL;
			}
			else if(retval & (OPERATOR_CANCELLED|OPERATOR_FINISHED)) {
				WM_operator_free(op);
				handler->op= NULL;
			}
			
			/* remove modal handler, operator itself should have been cancelled and freed */
			if(retval & (OPERATOR_CANCELLED|OPERATOR_FINISHED)) {
				BLI_remlink(handlers, handler);
				wm_event_free_handler(handler);
				MEM_freeN(handler);
				
				/* prevent silly errors from operator users */
				retval &= ~OPERATOR_PASS_THROUGH;
			}
			
		}
		else
			printf("wm_handler_operator_call error\n");
	}
	else {
		wmOperatorType *ot= WM_operatortype_find(event->keymap_idname);

		if(ot)
			retval= wm_operator_invoke(C, ot, event, properties);
	}

	if(retval & OPERATOR_PASS_THROUGH)
		return WM_HANDLER_CONTINUE;

	return WM_HANDLER_BREAK;
}

static int wm_handler_ui_call(bContext *C, wmEventHandler *handler, wmEvent *event)
{
	ScrArea *area= CTX_wm_area(C);
	ARegion *region= CTX_wm_region(C);
	int retval;
			
	/* we set context to where ui handler came from */
	if(handler->ui_area) CTX_wm_area_set(C, handler->ui_area);
	if(handler->ui_region) CTX_wm_region_set(C, handler->ui_region);

	retval= handler->ui_handle(C, event, handler->ui_userdata);

	/* putting back screen context */
	CTX_wm_area_set(C, area);
	CTX_wm_region_set(C, region);

	if(retval == WM_UI_HANDLER_BREAK)
		return WM_HANDLER_BREAK;

	return WM_HANDLER_CONTINUE;
}

static int handler_boundbox_test(wmEventHandler *handler, wmEvent *event)
{
	if(handler->bbwin) {
		if(handler->bblocal) {
			rcti rect= *handler->bblocal;
			BLI_translate_rcti(&rect, handler->bbwin->xmin, handler->bbwin->ymin);
			return BLI_in_rcti(&rect, event->x, event->y);
		}
		else 
			return BLI_in_rcti(handler->bbwin, event->x, event->y);
	}
	return 1;
}

static int wm_handlers_do(bContext *C, wmEvent *event, ListBase *handlers)
{
	wmEventHandler *handler, *nexthandler;
	int action= WM_HANDLER_CONTINUE;

	if(handlers==NULL) return action;
	
	/* modal handlers can get removed in this loop, we keep the loop this way */
	for(handler= handlers->first; handler; handler= nexthandler) {
		nexthandler= handler->next;

		/* optional boundbox */
		if(handler_boundbox_test(handler, event)) {
		
			/* modal+blocking handler */
			if(handler->flag & WM_HANDLER_BLOCKING)
				action= WM_HANDLER_BREAK;

			if(handler->keymap) {
				wmKeymapItem *kmi;
				
				for(kmi= handler->keymap->first; kmi; kmi= kmi->next) {
					if(wm_eventmatch(event, kmi)) {
						if((G.f & G_DEBUG) && event->type!=MOUSEMOVE)
							printf("handle evt %d win %d op %s\n", event->type, CTX_wm_screen(C)->subwinactive, kmi->idname); 
						
						event->keymap_idname= kmi->idname;	/* weak, but allows interactive callback to not use rawkey */
						
						action= wm_handler_operator_call(C, handlers, handler, event, kmi->properties);
						if(action==WM_HANDLER_BREAK)  /* not wm_event_always_pass(event) here, it denotes removed handler */
							break;
					}
				}
			}
			else if(handler->ui_handle) {
				action= wm_handler_ui_call(C, handler, event);
			}
			else {
				/* modal, swallows all */
				action= wm_handler_operator_call(C, handlers, handler, event, NULL);
			}

			if(!wm_event_always_pass(event) && action==WM_HANDLER_BREAK)
				break;
		}
		
	}
	return action;
}

static int wm_event_inside_i(wmEvent *event, rcti *rect)
{
	return BLI_in_rcti(rect, event->x, event->y);
}

static int wm_event_prev_inside_i(wmEvent *event, rcti *rect)
{
	if(BLI_in_rcti(rect, event->x, event->y))
	   return 1;
	if(event->type==MOUSEMOVE) {
		if( BLI_in_rcti(rect, event->prevx, event->prevy)) {
			return 1;
		}
		return 0;
	}
	return 0;
}

static ScrArea *area_event_inside(bContext *C, wmEvent *event)
{
	bScreen *screen= CTX_wm_screen(C);
	ScrArea *sa;
	
	if(screen)
		for(sa= screen->areabase.first; sa; sa= sa->next)
			if(BLI_in_rcti(&sa->totrct, event->x, event->y))
				return sa;
	return NULL;
}

static ARegion *region_event_inside(bContext *C, wmEvent *event)
{
	bScreen *screen= CTX_wm_screen(C);
	ScrArea *area= CTX_wm_area(C);
	ARegion *ar;
	
	if(screen && area)
		for(ar= area->regionbase.first; ar; ar= ar->next)
			if(BLI_in_rcti(&ar->winrct, event->x, event->y))
				return ar;
	return NULL;
}


/* called in main loop */
/* goes over entire hierarchy:  events -> window -> screen -> area -> region */
void wm_event_do_handlers(bContext *C)
{
	wmWindow *win;

	for(win= CTX_wm_manager(C)->windows.first; win; win= win->next) {
		wmEvent *event;
		
		if( win->screen==NULL )
			wm_event_free_all(win);
		
		while( (event=wm_event_next(win)) ) {
			int action;

			CTX_wm_window_set(C, win);
			CTX_wm_area_set(C, area_event_inside(C, event));
			CTX_wm_region_set(C, region_event_inside(C, event));
			
			/* MVC demands to not draw in event handlers... but we need to leave it for ogl selecting etc */
			wm_window_make_drawable(C, win);
			
			action= wm_handlers_do(C, event, &win->handlers);
			
			if(wm_event_always_pass(event) || action==WM_HANDLER_CONTINUE) {
				ScrArea *sa;
				ARegion *ar;
				int doit= 0;
				
				/* XXX to solve, here screen handlers? */
				if(!wm_event_always_pass(event))
					ED_screen_set_subwinactive(win, event);	/* state variables in screen */
				
				for(sa= win->screen->areabase.first; sa; sa= sa->next) {
					if(wm_event_always_pass(event) || wm_event_prev_inside_i(event, &sa->totrct)) {
						doit= 1;
						CTX_wm_area_set(C, sa);
						action= wm_handlers_do(C, event, &sa->handlers);

						if(wm_event_always_pass(event) || action==WM_HANDLER_CONTINUE) {
							for(ar=sa->regionbase.first; ar; ar= ar->next) {
								if(wm_event_always_pass(event) || wm_event_inside_i(event, &ar->winrct) || wm_event_prev_inside_i(event, &ar->winrct)) {
									CTX_wm_region_set(C, ar);
									action= wm_handlers_do(C, event, &ar->handlers);
									CTX_wm_region_set(C, NULL);

									if(!wm_event_always_pass(event)) {
										if(action==WM_HANDLER_BREAK)
											break;
									}
								}
							}
						}

						CTX_wm_area_set(C, NULL);
						/* NOTE: do not escape on WM_HANDLER_BREAK, mousemove needs handled for previous area */
					}
				}
				/* XXX hrmf, this gives reliable previous mouse coord for area change, feels bad? 
				   doing it on ghost queue gives errors when mousemoves go over area borders */
				if(doit) {
					CTX_wm_window(C)->eventstate->prevx= event->x;
					CTX_wm_window(C)->eventstate->prevy= event->y;
				}
			}
			wm_event_free(event);
			
			CTX_wm_window_set(C, NULL);
		}
	}
}

/* lets not expose struct outside wm? */
void WM_event_set_handler_flag(wmEventHandler *handler, int flag)
{
	handler->flag= flag;
}

wmEventHandler *WM_event_add_modal_handler(bContext *C, ListBase *handlers, wmOperator *op)
{
	wmEventHandler *handler= MEM_callocN(sizeof(wmEventHandler), "event modal handler");
	handler->op= op;
	handler->op_area= CTX_wm_area(C);		/* means frozen screen context for modal handlers! */
	handler->op_region= CTX_wm_region(C);
	
	BLI_addhead(handlers, handler);

	return handler;
}

wmEventHandler *WM_event_add_keymap_handler(ListBase *handlers, ListBase *keymap)
{
	wmEventHandler *handler;
	
	/* only allow same keymap once */
	for(handler= handlers->first; handler; handler= handler->next)
		if(handler->keymap==keymap)
			return handler;

	handler= MEM_callocN(sizeof(wmEventHandler), "event keymap handler");
	BLI_addtail(handlers, handler);
	handler->keymap= keymap;

	return handler;
}

wmEventHandler *WM_event_add_keymap_handler_bb(ListBase *handlers, ListBase *keymap, rcti *bblocal, rcti *bbwin)
{
	wmEventHandler *handler= WM_event_add_keymap_handler(handlers, keymap);
	handler->bblocal= bblocal;
	handler->bbwin= bbwin;
	
	return handler;
}

void WM_event_remove_keymap_handler(ListBase *handlers, ListBase *keymap)
{
	wmEventHandler *handler;
	
	for(handler= handlers->first; handler; handler= handler->next) {
		if(handler->keymap==keymap) {
			BLI_remlink(handlers, handler);
			wm_event_free_handler(handler);
			MEM_freeN(handler);
			break;
		}
	}
}

wmEventHandler *WM_event_add_ui_handler(bContext *C, ListBase *handlers, wmUIHandlerFunc func, wmUIHandlerRemoveFunc remove, void *userdata)
{
	wmEventHandler *handler= MEM_callocN(sizeof(wmEventHandler), "event ui handler");
	handler->ui_handle= func;
	handler->ui_remove= remove;
	handler->ui_userdata= userdata;
	handler->ui_area= (C)? CTX_wm_area(C): NULL;
	handler->ui_region= (C)? CTX_wm_region(C): NULL;
	
	BLI_addhead(handlers, handler);
	
	return handler;
}

void WM_event_remove_ui_handler(ListBase *handlers, wmUIHandlerFunc func, wmUIHandlerRemoveFunc remove, void *userdata)
{
	wmEventHandler *handler;
	
	for(handler= handlers->first; handler; handler= handler->next) {
		if(handler->ui_handle == func && handler->ui_remove == remove && handler->ui_userdata == userdata) {
			BLI_remlink(handlers, handler);
			wm_event_free_handler(handler);
			MEM_freeN(handler);
			break;
		}
	}
}

void WM_event_add_mousemove(bContext *C)
{
	wmWindow *window= CTX_wm_window(C);
	wmEvent event= *(window->eventstate);
	event.type= MOUSEMOVE;
	event.prevx= event.x;
	event.prevy= event.y;
	wm_event_add(window, &event);
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
		
		event->custom= EVT_DATA_TABLET;
		event->customdata= wmtab;
		event->customdatafree= 1;
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
			event.val= (type==GHOST_kEventKeyDown); /* XXX eventmatch uses defines, bad code... */
			
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
		case GHOST_kEventTimer: {
			event.type= TIMER;
			event.custom= EVT_DATA_TIMER;
			event.customdata= customdata;
			wm_event_add(win, &event);

			break;
		}

		case GHOST_kEventUnknown:
		case GHOST_kNumEventTypes:
			break;
	}
}

