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
#include "BKE_report.h"
#include "BKE_utildefines.h"

#include "ED_anim_api.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_util.h"

#include "RNA_access.h"

#include "UI_interface.h"

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
void WM_event_add_notifier(bContext *C, unsigned int type, void *reference)
{
	wmNotifier *note= MEM_callocN(sizeof(wmNotifier), "notifier");
	
	note->wm= CTX_wm_manager(C);
	BLI_addtail(&note->wm->queue, note);
	
	note->window= CTX_wm_window(C);
	
	if(CTX_wm_region(C))
		note->swinid= CTX_wm_region(C)->swinid;
	
	note->category= type & NOTE_CATEGORY;
	note->data= type & NOTE_DATA;
	note->subtype= type & NOTE_SUBTYPE;
	note->action= type & NOTE_ACTION;
	
	note->reference= reference;
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
	wmWindow *win;
	
	if(wm==NULL)
		return;
	
	/* cache & catch WM level notifiers, such as frame change */
	/* XXX todo, multiwindow scenes */
	for(win= wm->windows.first; win; win= win->next) {
		int do_anim= 0;
		
		for(note= wm->queue.first; note; note= note->next) {
			if(note->window==win)
				if(note->category==NC_SCENE)
					if(note->data==ND_FRAME)
						do_anim= 1;
		}
		if(do_anim) {
			/* depsgraph gets called, might send more notifiers */
			CTX_wm_window_set(C, win);
			ED_update_for_newframe(C, 1);
		}
	}
	
	/* the notifiers are sent without context, to keep it clean */
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
				ED_area_do_listen(sa, note);
				for(ar=sa->regionbase.first; ar; ar= ar->next) {
					ED_region_do_listen(ar, note);
				}
			}
		}
		
		CTX_wm_window_set(C, NULL);
		
		MEM_freeN(note);
	}
	
	/* cached: editor refresh callbacks now, they get context */
	for(win= wm->windows.first; win; win= win->next) {
		ScrArea *sa;
		CTX_wm_window_set(C, win);
		for(sa= win->screen->areabase.first; sa; sa= sa->next) {
			if(sa->do_refresh) {
				CTX_wm_area_set(C, sa);
				ED_area_do_refresh(C, sa);
			}
		}
	}
	CTX_wm_window_set(C, NULL);
}

/* ********************* operators ******************* */

/* if repeat is true, it doesn't register again, nor does it free */
static int wm_operator_exec(bContext *C, wmOperator *op, int repeat)
{
	int retval= OPERATOR_CANCELLED;
	
	if(op==NULL || op->type==NULL)
		return retval;
	
	if(op->type->poll && op->type->poll(C)==0)
		return retval;
	
	if(op->type->exec)
		retval= op->type->exec(C, op);
	
	if(!(retval & OPERATOR_RUNNING_MODAL))
		if(op->reports->list.first)
			uiPupMenuReports(C, op->reports);
	
	if(retval & OPERATOR_FINISHED) {
		if(op->type->flag & OPTYPE_UNDO)
			ED_undo_push_op(C, op);
		
		if(repeat==0) {
			if(op->type->flag & OPTYPE_REGISTER)
				wm_operator_register(CTX_wm_manager(C), op);
			else
				WM_operator_free(op);
		}
	}
	else if(repeat==0)
		WM_operator_free(op);
	
	return retval;
	
}

/* for running operators with frozen context (modal handlers, menus) */
int WM_operator_call(bContext *C, wmOperator *op)
{
	return wm_operator_exec(C, op, 0);
}

/* do this operator again, put here so it can share above code */
int WM_operator_repeat(bContext *C, wmOperator *op)
{
	return wm_operator_exec(C, op, 1);
}

static wmOperator *wm_operator_create(wmWindowManager *wm, wmOperatorType *ot, PointerRNA *properties, ReportList *reports)
{
	wmOperator *op= MEM_callocN(sizeof(wmOperator), ot->idname);	/* XXX operatortype names are static still. for debug */
	
	/* XXX adding new operator could be function, only happens here now */
	op->type= ot;
	BLI_strncpy(op->idname, ot->idname, OP_MAX_TYPENAME);
	
	/* initialize properties, either copy or create */
	op->ptr= MEM_callocN(sizeof(PointerRNA), "wmOperatorPtrRNA");
	if(properties && properties->data) {
		op->properties= IDP_CopyProperty(properties->data);
	}
	else {
		IDPropertyTemplate val = {0};
		op->properties= IDP_New(IDP_GROUP, val, "wmOperatorProperties");
	}
	RNA_pointer_create(&wm->id, ot->srna, op->properties, op->ptr);

	/* initialize error reports */
	if (reports) {
		op->reports= reports; /* must be initialized alredy */
	}
	else {
		op->reports= MEM_mallocN(sizeof(ReportList), "wmOperatorReportList");
		BKE_reports_init(op->reports, RPT_STORE);
	}
	
	return op;
}

static void wm_operator_print(wmOperator *op)
{
	char *buf = WM_operator_pystring(op);
	printf("%s\n", buf);
	MEM_freeN(buf);
}

static int wm_operator_invoke(bContext *C, wmOperatorType *ot, wmEvent *event, PointerRNA *properties)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	int retval= OPERATOR_PASS_THROUGH;

	if(ot->poll==NULL || ot->poll(C)) {
		wmOperator *op= wm_operator_create(wm, ot, properties, NULL);
		
		if((G.f & G_DEBUG) && event && event->type!=MOUSEMOVE)
			printf("handle evt %d win %d op %s\n", event?event->type:0, CTX_wm_screen(C)->subwinactive, ot->idname); 
		
		if(op->type->invoke && event)
			retval= (*op->type->invoke)(C, op, event);
		else if(op->type->exec)
			retval= op->type->exec(C, op);
		else
			printf("invalid operator call %s\n", ot->idname); /* debug, important to leave a while, should never happen */

		if(!(retval & OPERATOR_RUNNING_MODAL)) {
			if(op->reports->list.first) /* only show the report if the report list was not given in the function */
				uiPupMenuReports(C, op->reports);
		
		if (retval & OPERATOR_FINISHED) /* todo - this may conflict with the other wm_operator_print, if theres ever 2 prints for 1 action will may need to add modal check here */
			if(G.f & G_DEBUG)
				wm_operator_print(op);
		}

		if(retval & OPERATOR_FINISHED) {
			if(ot->flag & OPTYPE_UNDO)
				ED_undo_push_op(C, op);
			
			if(ot->flag & OPTYPE_REGISTER)
				wm_operator_register(wm, op);
			else
				WM_operator_free(op);
		}
		else if(!(retval & OPERATOR_RUNNING_MODAL)) {
			WM_operator_free(op);
		}
	}

	return retval;
}

/* invokes operator in context */
int WM_operator_name_call(bContext *C, const char *opstring, int context, PointerRNA *properties)
{
	wmOperatorType *ot= WM_operatortype_find(opstring);
	wmWindow *window= CTX_wm_window(C);
	wmEvent *event;
	
	int retval;

	/* dummie test */
	if(ot && C && window) {
		event= window->eventstate;
		switch(context) {
			
			case WM_OP_EXEC_REGION_WIN:
				event= NULL;	/* pass on without break */
			case WM_OP_INVOKE_REGION_WIN: 
			{
				/* forces operator to go to the region window, for header menus */
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
				
				retval= wm_operator_invoke(C, ot, event, properties);
				
				/* set region back */
				CTX_wm_region_set(C, ar);
				
				return retval;
			}
			case WM_OP_EXEC_AREA:
				event= NULL;	/* pass on without break */
			case WM_OP_INVOKE_AREA:
			{
					/* remove region from context */
				ARegion *ar= CTX_wm_region(C);

				CTX_wm_region_set(C, NULL);
				retval= wm_operator_invoke(C, ot, event, properties);
				CTX_wm_region_set(C, ar);

				return retval;
			}
			case WM_OP_EXEC_SCREEN:
				event= NULL;	/* pass on without break */
			case WM_OP_INVOKE_SCREEN:
			{
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
			case WM_OP_EXEC_DEFAULT:
				event= NULL;	/* pass on without break */
			case WM_OP_INVOKE_DEFAULT:
				return wm_operator_invoke(C, ot, event, properties);
		}
	}
	
	return 0;
}

/* Similar to WM_operator_name_call called with WM_OP_EXEC_DEFAULT context.
   - wmOperatorType is used instead of operator name since python alredy has the operator type
   - poll() must be called by python before this runs.
   - reports can be passed to this function (so python can report them as exceptions)
*/
int WM_operator_call_py(bContext *C, wmOperatorType *ot, PointerRNA *properties, ReportList *reports)
{
	wmWindowManager *wm=	CTX_wm_manager(C);
	wmOperator *op=			wm_operator_create(wm, ot, properties, reports);
	int retval=				op->type->exec(C, op);
	
	if (reports)
		op->reports= NULL; /* dont let the operator free reports passed to this function */
	WM_operator_free(op);
	
	return retval;
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

/* do userdef mappings */
static int wm_userdef_event_map(int kmitype)
{
	switch(kmitype) {
		case SELECTMOUSE:
			if(U.flag & USER_LMOUSESELECT)
				return LEFTMOUSE;
			else
				return RIGHTMOUSE;
			
		case ACTIONMOUSE:
			if(U.flag & USER_LMOUSESELECT)
				return RIGHTMOUSE;
			else
				return LEFTMOUSE;
			
		case WHEELOUTMOUSE:
			if(U.uiflag & USER_WHEELZOOMDIR)
				return WHEELUPMOUSE;
			else
				return WHEELDOWNMOUSE;
			
		case WHEELINMOUSE:
			if(U.uiflag & USER_WHEELZOOMDIR)
				return WHEELDOWNMOUSE;
			else
				return WHEELUPMOUSE;
			
		case EVT_TWEAK_A:
			if(U.flag & USER_LMOUSESELECT)
				return EVT_TWEAK_R;
			else
				return EVT_TWEAK_L;
			
		case EVT_TWEAK_S:
			if(U.flag & USER_LMOUSESELECT)
				return EVT_TWEAK_L;
			else
				return EVT_TWEAK_R;
	}
	
	return kmitype;
}

static int wm_eventmatch(wmEvent *winevent, wmKeymapItem *kmi)
{
	int kmitype= wm_userdef_event_map(kmi->type);

	/* the matching rules */
	if(kmitype==KM_TEXTINPUT)
		if(ISKEYBOARD(winevent->type)) return 1;
	if(kmitype!=KM_ANY)
		if(winevent->type!=kmitype) return 0;
	
	if(kmi->val!=KM_ANY)
		if(winevent->val!=kmi->val) return 0;
	
	/* modifiers also check bits, so it allows modifier order */
	if(kmi->shift!=KM_ANY)
		if(winevent->shift != kmi->shift && !(winevent->shift & kmi->shift)) return 0;
	if(kmi->ctrl!=KM_ANY)
		if(winevent->ctrl != kmi->ctrl && !(winevent->ctrl & kmi->ctrl)) return 0;
	if(kmi->alt!=KM_ANY)
		if(winevent->alt != kmi->alt && !(winevent->alt & kmi->alt)) return 0;
	if(kmi->oskey!=KM_ANY)
		if(winevent->oskey != kmi->oskey && !(winevent->oskey & kmi->oskey)) return 0;
	if(kmi->keymodifier)
		if(winevent->keymodifier!=kmi->keymodifier) return 0;
	
	return 1;
}

static int wm_event_always_pass(wmEvent *event)
{
	/* some events we always pass on, to ensure proper communication */
	return ELEM5(event->type, TIMER, TIMER0, TIMER1, TIMER2, TIMERJOBS);
}

/* Warning: this function removes a modal handler, when finished */
static int wm_handler_operator_call(bContext *C, ListBase *handlers, wmEventHandler *handler, wmEvent *event, PointerRNA *properties)
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

			/* putting back screen context, reval can pass trough after modal failures! */
			if((retval & OPERATOR_PASS_THROUGH) || wm_event_always_pass(event)) {
				CTX_wm_area_set(C, area);
				CTX_wm_region_set(C, region);
			}
			else {
				/* this special cases is for areas and regions that get removed */
				CTX_wm_area_set(C, NULL);
				CTX_wm_region_set(C, NULL);
			}

			if(!(retval & OPERATOR_RUNNING_MODAL))
				if(op->reports->list.first)
					uiPupMenuReports(C, op->reports);

			if (retval & OPERATOR_FINISHED) {
				if(G.f & G_DEBUG)
					wm_operator_print(op); /* todo - this print may double up, might want to check more flags then the FINISHED */
			}			

			if(retval & OPERATOR_FINISHED) {
				if(ot->flag & OPTYPE_UNDO)
					ED_undo_push_op(C, op);
				
				if(ot->flag & OPTYPE_REGISTER)
					wm_operator_register(CTX_wm_manager(C), op);
				else
					WM_operator_free(op);
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
				//retval &= ~OPERATOR_PASS_THROUGH;
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
	if((retval != WM_UI_HANDLER_BREAK) || wm_event_always_pass(event)) {
		CTX_wm_area_set(C, area);
		CTX_wm_region_set(C, region);
	}
	else {
		/* this special cases is for areas and regions that get removed */
		CTX_wm_area_set(C, NULL);
		CTX_wm_region_set(C, NULL);
	}

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
						
						event->keymap_idname= kmi->idname;	/* weak, but allows interactive callback to not use rawkey */
						
						action= wm_handler_operator_call(C, handlers, handler, event, kmi->ptr);
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
		
		/* fileread case */
		if(CTX_wm_window(C)==NULL)
			break;
	}
	return action;
}

static int wm_event_inside_i(wmEvent *event, rcti *rect)
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

static ScrArea *area_event_inside(bContext *C, int x, int y)
{
	bScreen *screen= CTX_wm_screen(C);
	ScrArea *sa;
	
	if(screen)
		for(sa= screen->areabase.first; sa; sa= sa->next)
			if(BLI_in_rcti(&sa->totrct, x, y))
				return sa;
	return NULL;
}

static ARegion *region_event_inside(bContext *C, int x, int y)
{
	bScreen *screen= CTX_wm_screen(C);
	ScrArea *area= CTX_wm_area(C);
	ARegion *ar;
	
	if(screen && area)
		for(ar= area->regionbase.first; ar; ar= ar->next)
			if(BLI_in_rcti(&ar->winrct, x, y))
				return ar;
	return NULL;
}

static void wm_paintcursor_tag(bContext *C, wmPaintCursor *pc, ARegion *ar)
{
	if(ar) {
		for(; pc; pc= pc->next) {
			if(pc->poll(C)) {
				wmWindow *win= CTX_wm_window(C);
				win->screen->do_draw_paintcursor= 1;

				if(win->drawmethod != USER_DRAW_TRIPLE)
					ED_region_tag_redraw(ar);
			}
		}
	}
}

/* called on mousemove, check updates for paintcursors */
/* context was set on active area and region */
static void wm_paintcursor_test(bContext *C, wmEvent *event)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	
	if(wm->paintcursors.first) {
		ARegion *ar= CTX_wm_region(C);
		if(ar)
			wm_paintcursor_tag(C, wm->paintcursors.first, ar);
		
		/* if previous position was not in current region, we have to set a temp new context */
		if(ar==NULL || !BLI_in_rcti(&ar->winrct, event->prevx, event->prevy)) {
			ScrArea *sa= CTX_wm_area(C);
			
			CTX_wm_area_set(C, area_event_inside(C, event->prevx, event->prevy));
			CTX_wm_region_set(C, region_event_inside(C, event->prevx, event->prevy));

			wm_paintcursor_tag(C, wm->paintcursors.first, CTX_wm_region(C));
			
			CTX_wm_area_set(C, sa);
			CTX_wm_region_set(C, ar);
		}
	}
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
		
		while( (event= win->queue.first) ) {
			int action;
			
			CTX_wm_window_set(C, win);
			
			/* we let modal handlers get active area/region, also wm_paintcursor_test needs it */
			CTX_wm_area_set(C, area_event_inside(C, event->x, event->y));
			CTX_wm_region_set(C, region_event_inside(C, event->x, event->y));
			
			/* MVC demands to not draw in event handlers... but we need to leave it for ogl selecting etc */
			wm_window_make_drawable(C, win);
			
			action= wm_handlers_do(C, event, &win->handlers);
			
			/* fileread case */
			if(CTX_wm_window(C)==NULL) {
				return;
			}
			
			/* builtin tweak, if action is break it removes tweak */
			if(!wm_event_always_pass(event))
				wm_tweakevent_test(C, event, action);
			
			if(wm_event_always_pass(event) || action==WM_HANDLER_CONTINUE) {
				ScrArea *sa;
				ARegion *ar;
				int doit= 0;
				
				/* XXX to solve, here screen handlers? */
				if(!wm_event_always_pass(event)) {
					if(event->type==MOUSEMOVE) {
						/* state variables in screen, cursors */
						ED_screen_set_subwinactive(win, event);	
						/* for regions having custom cursors */
						wm_paintcursor_test(C, event);
					}
				}
				
				for(sa= win->screen->areabase.first; sa; sa= sa->next) {
					if(wm_event_always_pass(event) || wm_event_inside_i(event, &sa->totrct)) {
						
						CTX_wm_area_set(C, sa);
						CTX_wm_region_set(C, NULL);
						action= wm_handlers_do(C, event, &sa->handlers);

						if(wm_event_always_pass(event) || action==WM_HANDLER_CONTINUE) {
							for(ar=sa->regionbase.first; ar; ar= ar->next) {
								if(wm_event_always_pass(event) || wm_event_inside_i(event, &ar->winrct)) {
									CTX_wm_region_set(C, ar);
									action= wm_handlers_do(C, event, &ar->handlers);

									doit |= (BLI_in_rcti(&ar->winrct, event->x, event->y));
									
									if(!wm_event_always_pass(event)) {
										if(action==WM_HANDLER_BREAK)
											break;
									}
								}
							}
						}
						/* NOTE: do not escape on WM_HANDLER_BREAK, mousemove needs handled for previous area */
					}
				}
				
				/* XXX hrmf, this gives reliable previous mouse coord for area change, feels bad? 
				   doing it on ghost queue gives errors when mousemoves go over area borders */
				if(doit && win->screen->subwinactive != win->screen->mainwin) {
					win->eventstate->prevx= event->x;
					win->eventstate->prevy= event->y;
				}
			}
			
			/* unlink and free here, blender-quit then frees all */
			BLI_remlink(&win->queue, event);
			wm_event_free(event);
			
		}
		CTX_wm_window_set(C, NULL);
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

/* priorities not implemented yet, for time being just insert in begin of list */
wmEventHandler *WM_event_add_keymap_handler_priority(ListBase *handlers, ListBase *keymap, int priority)
{
	wmEventHandler *handler;
	
	WM_event_remove_keymap_handler(handlers, keymap);
	
	handler= MEM_callocN(sizeof(wmEventHandler), "event keymap handler");
	BLI_addhead(handlers, handler);
	handler->keymap= keymap;
	
	return handler;
}

wmEventHandler *WM_event_add_keymap_handler_bb(ListBase *handlers, ListBase *keymap, rcti *bblocal, rcti *bbwin)
{
	wmEventHandler *handler= WM_event_add_keymap_handler(handlers, keymap);
	
	if(handler) {
		handler->bblocal= bblocal;
		handler->bbwin= bbwin;
	}
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

/* for modal callbacks, check configuration for how to interpret exit with tweaks  */
int WM_modal_tweak_exit(wmEvent *evt, int tweak_event)
{
	/* user preset or keymap? dunno... */
	int tweak_modal= (U.flag & USER_DRAGIMMEDIATE)==0;
	
	switch(tweak_event) {
		case EVT_TWEAK_L:
		case EVT_TWEAK_M:
		case EVT_TWEAK_R:
			if(evt->val==tweak_modal)
				return 1;
		default:
			/* this case is when modal callcback didnt get started with a tweak */
			if(evt->val)
				return 1;
	}
	return 0;
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
			
			/* exclude arrow keys, esc, etc from text input */
			if(type==GHOST_kEventKeyUp || (event.ascii<32 && event.ascii>14))
				event.ascii= '\0';
			
			/* modifiers */
			if (event.type==LEFTSHIFTKEY || event.type==RIGHTSHIFTKEY) {
				event.shift= evt->shift= event.val;
				if(event.val && (evt->ctrl || evt->alt || evt->oskey))
				   event.shift= evt->shift = 3;		// define?
			} 
			else if (event.type==LEFTCTRLKEY || event.type==RIGHTCTRLKEY) {
				event.ctrl= evt->ctrl= event.val;
				if(event.val && (evt->shift || evt->alt || evt->oskey))
				   event.ctrl= evt->ctrl = 3;		// define?
			} 
			else if (event.type==LEFTALTKEY || event.type==RIGHTALTKEY) {
				event.alt= evt->alt= event.val;
				if(event.val && (evt->ctrl || evt->shift || evt->oskey))
				   event.alt= evt->alt = 3;		// define?
			} 
			else if (event.type==COMMANDKEY) {
				event.oskey= evt->oskey= event.val;
				if(event.val && (evt->ctrl || evt->alt || evt->shift))
				   event.oskey= evt->oskey = 3;		// define?
			}
			
			/* if test_break set, it catches this. Keep global for now? */
			if(event.type==ESCKEY)
				G.afbreek= 1;
			
			wm_event_add(win, &event);
			
			break;
		}
			
		case GHOST_kEventWheel:	{
			GHOST_TEventWheelData* wheelData = customdata;
			
			if (wheelData->z > 0)
				event.type= WHEELUPMOUSE;
			else
				event.type= WHEELDOWNMOUSE;
			
			event.val= KM_PRESS;
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

