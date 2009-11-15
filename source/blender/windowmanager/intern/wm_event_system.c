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
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "GHOST_C-api.h"

#include "BLI_blenlib.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"
#include "BKE_pointcache.h"

#include "ED_fileselect.h"
#include "ED_info.h"
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

void wm_event_free(wmEvent *event)
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
void WM_event_add_notifier(const bContext *C, unsigned int type, void *reference)
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

void WM_main_add_notifier(unsigned int type, void *reference)
{
	Main *bmain= G.main;
	wmWindowManager *wm= bmain->wm.first;

	if(wm) {
		wmNotifier *note= MEM_callocN(sizeof(wmNotifier), "notifier");
		
		note->wm= wm;
		BLI_addtail(&note->wm->queue, note);
		
		note->category= type & NOTE_CATEGORY;
		note->data= type & NOTE_DATA;
		note->subtype= type & NOTE_SUBTYPE;
		note->action= type & NOTE_ACTION;
		
		note->reference= reference;
	}
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
	wmNotifier *note, *next;
	wmWindow *win;
	
	if(wm==NULL)
		return;
	
	/* cache & catch WM level notifiers, such as frame change, scene/screen set */
	for(win= wm->windows.first; win; win= win->next) {
		int do_anim= 0;
		
		CTX_wm_window_set(C, win);
		
		for(note= wm->queue.first; note; note= next) {
			next= note->next;

			if(note->category==NC_WM) {
				if( ELEM(note->data, ND_FILEREAD, ND_FILESAVE)) {
					wm->file_saved= 1;
					wm_window_title(wm, win);
				}
				else if(note->data==ND_DATACHANGED)
					wm_window_title(wm, win);
			}
			if(note->window==win) {
				if(note->category==NC_SCREEN) {
					if(note->data==ND_SCREENBROWSE) {
						ED_screen_set(C, note->reference);	// XXX hrms, think this over!
						printf("screen set %p\n", note->reference);
					}
					else if(note->data==ND_SCREENDELETE) {
						ED_screen_delete(C, note->reference);	// XXX hrms, think this over!
						printf("screen delete %p\n", note->reference);
					}
				}
				else if(note->category==NC_SCENE) {
					if(note->data==ND_SCENEBROWSE) {
						ED_screen_set_scene(C, note->reference);	// XXX hrms, think this over!
						printf("scene set %p\n", note->reference);
					}
					if(note->data==ND_SCENEDELETE) {
						ED_screen_delete_scene(C, note->reference);	// XXX hrms, think this over!
						printf("scene delete %p\n", note->reference);
					}
					else if(note->data==ND_FRAME)
						do_anim= 1;
				}
			}
			if(ELEM4(note->category, NC_SCENE, NC_OBJECT, NC_GEOM, NC_SCENE)) {
				ED_info_stats_clear(CTX_data_scene(C));
				WM_event_add_notifier(C, NC_SPACE|ND_SPACE_INFO, NULL);
			}
		}
		if(do_anim) {
			/* depsgraph gets called, might send more notifiers */
			ED_update_for_newframe(C, 1);
		}
	}
	
	/* the notifiers are sent without context, to keep it clean */
	while( (note=wm_notifier_next(wm)) ) {
		wmWindow *win;
		
		for(win= wm->windows.first; win; win= win->next) {
			
			/* filter out notifiers */
			if(note->category==NC_SCREEN && note->reference && note->reference!=win->screen);
			else if(note->category==NC_SCENE && note->reference && note->reference!=win->screen->scene);
			else {
				ScrArea *sa;
				ARegion *ar;

				/* XXX context in notifiers? */
				CTX_wm_window_set(C, win);

				/* printf("notifier win %d screen %s cat %x\n", win->winid, win->screen->id.name+2, note->category); */
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
		}
		
		MEM_freeN(note);
	}
	
	/* cached: editor refresh callbacks now, they get context */
	for(win= wm->windows.first; win; win= win->next) {
		Scene *sce, *scene= win->screen->scene;
		ScrArea *sa;
		Base *base;
		
		CTX_wm_window_set(C, win);
		for(sa= win->screen->areabase.first; sa; sa= sa->next) {
			if(sa->do_refresh) {
				CTX_wm_area_set(C, sa);
				ED_area_do_refresh(C, sa);
			}
		}
		
		if(G.rendering==0) { // XXX make lock in future, or separated derivedmesh users in scene
			
			/* update all objects, drivers, matrices, displists, etc. Flags set by depgraph or manual, 
				no layer check here, gets correct flushed */
			/* sets first, we allow per definition current scene to have dependencies on sets */
			if(scene->set) {
				for(SETLOOPER(scene->set, base))
					object_handle_update(scene, base->object);
			}
			
			for(base= scene->base.first; base; base= base->next) {
				object_handle_update(scene, base->object);
			}

			BKE_ptcache_quick_cache_all(scene);
		}		
	}
	CTX_wm_window_set(C, NULL);
}

/* ********************* operators ******************* */

int WM_operator_poll(bContext *C, wmOperatorType *ot)
{
	wmOperatorTypeMacro *otmacro;
	
	for(otmacro= ot->macro.first; otmacro; otmacro= otmacro->next) {
		wmOperatorType *ot= WM_operatortype_find(otmacro->idname, 0);
		
		if(0==WM_operator_poll(C, ot))
			return 0;
	}
	
	/* python needs operator type, so we added exception for it */
	if(ot->pyop_poll)
		return ot->pyop_poll(C, ot);
	else if(ot->poll)
		return ot->poll(C);

	return 1;
}

/* if repeat is true, it doesn't register again, nor does it free */
static int wm_operator_exec(bContext *C, wmOperator *op, int repeat)
{
	int retval= OPERATOR_CANCELLED;
	
	if(op==NULL || op->type==NULL)
		return retval;
	
	if(0==WM_operator_poll(C, op->type))
		return retval;
	
	if(op->type->exec)
		retval= op->type->exec(C, op);
	
	if(!(retval & OPERATOR_RUNNING_MODAL))
		if(op->reports->list.first)
			uiPupMenuReports(C, op->reports);
	
	if(retval & OPERATOR_FINISHED) {
		op->customdata= NULL;

		if(op->type->flag & OPTYPE_UNDO)
			ED_undo_push_op(C, op);
		
		if(repeat==0) {
			if((op->type->flag & OPTYPE_REGISTER) || (G.f & G_DEBUG))
				wm_operator_register(C, op);
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
		op->reports= reports; /* must be initialized already */
	}
	else {
		op->reports= MEM_mallocN(sizeof(ReportList), "wmOperatorReportList");
		BKE_reports_init(op->reports, RPT_STORE|RPT_FREE);
	}
	
	/* recursive filling of operator macro list */
	if(ot->macro.first) {
		static wmOperator *motherop= NULL;
		wmOperatorTypeMacro *otmacro;
		
		/* ensure all ops are in execution order in 1 list */
		if(motherop==NULL) 
			motherop= op;
		
		for(otmacro= ot->macro.first; otmacro; otmacro= otmacro->next) {
			wmOperatorType *otm= WM_operatortype_find(otmacro->idname, 0);
			wmOperator *opm= wm_operator_create(wm, otm, otmacro->ptr, NULL);
			
			BLI_addtail(&motherop->macro, opm);
			opm->opm= motherop; /* pointer to mom, for modal() */
		}
		
		motherop= NULL;
	}
	
	return op;
}

static void wm_operator_print(wmOperator *op)
{
	char *buf = WM_operator_pystring(NULL, op->type, op->ptr, 1);
	printf("%s\n", buf);
	MEM_freeN(buf);
}

static void wm_region_mouse_co(bContext *C, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	if(ar) {
		/* compatibility convention */
		event->mval[0]= event->x - ar->winrct.xmin;
		event->mval[1]= event->y - ar->winrct.ymin;
	}
}

static int wm_operator_invoke(bContext *C, wmOperatorType *ot, wmEvent *event, PointerRNA *properties, ReportList *reports)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	int retval= OPERATOR_PASS_THROUGH;

	if(WM_operator_poll(C, ot)) {
		wmOperator *op= wm_operator_create(wm, ot, properties, reports); /* if reports==NULL, theyll be initialized */
		
		if((G.f & G_DEBUG) && event && event->type!=MOUSEMOVE)
			printf("handle evt %d win %d op %s\n", event?event->type:0, CTX_wm_screen(C)->subwinactive, ot->idname); 
		
		if(op->type->invoke && event) {
			wm_region_mouse_co(C, event);
			retval= op->type->invoke(C, op, event);
		}
		else if(op->type->exec)
			retval= op->type->exec(C, op);
		else
			printf("invalid operator call %s\n", ot->idname); /* debug, important to leave a while, should never happen */

		/* Note, if the report is given as an argument then assume the caller will deal with displaying them
		 * currently python only uses this */
		if(!(retval & OPERATOR_RUNNING_MODAL) && reports==NULL) {
			if(op->reports->list.first) /* only show the report if the report list was not given in the function */
				uiPupMenuReports(C, op->reports);
		
		if (retval & OPERATOR_FINISHED) /* todo - this may conflict with the other wm_operator_print, if theres ever 2 prints for 1 action will may need to add modal check here */
			if(G.f & G_DEBUG)
				wm_operator_print(op);
		}

		if(retval & OPERATOR_FINISHED) {
			op->customdata= NULL;

			if(ot->flag & OPTYPE_UNDO)
				ED_undo_push_op(C, op);
			
			if((ot->flag & OPTYPE_REGISTER) || (G.f & G_DEBUG))
				wm_operator_register(C, op);
			else
				WM_operator_free(op);
		}
		else if(retval & OPERATOR_RUNNING_MODAL) {
			/* grab cursor during blocking modal ops (X11) */
			if(ot->flag & OPTYPE_BLOCKING) {
				int bounds[4] = {-1,-1,-1,-1};
				int wrap = (U.uiflag & USER_CONTINUOUS_MOUSE) && ((op->flag & OP_GRAB_POINTER) || (ot->flag & OPTYPE_GRAB_POINTER));

				if(wrap) {
					ARegion *ar= CTX_wm_region(C);
					if(ar) {
						bounds[0]= ar->winrct.xmin;
						bounds[1]= ar->winrct.ymax;
						bounds[2]= ar->winrct.xmax;
						bounds[3]= ar->winrct.ymin;
					}
				}

				WM_cursor_grab(CTX_wm_window(C), wrap, FALSE, bounds);
			}
		}
		else
			WM_operator_free(op);
	}

	return retval;
}

/* WM_operator_name_call is the main accessor function
 * this is for python to access since its done the operator lookup
 * 
 * invokes operator in context */
static int wm_operator_call_internal(bContext *C, wmOperatorType *ot, int context, PointerRNA *properties, ReportList *reports)
{
	wmWindow *window= CTX_wm_window(C);
	wmEvent *event;
	
	int retval;

	/* dummie test */
	if(ot && C) {
		switch(context) {
			case WM_OP_INVOKE_DEFAULT:
			case WM_OP_INVOKE_REGION_WIN:
			case WM_OP_INVOKE_AREA:
			case WM_OP_INVOKE_SCREEN:
				/* window is needed for invoke, cancel operator */
				if (window == NULL)
					return 0;
				else
					event= window->eventstate;
				break;
			default:
				event = NULL;
		}

		switch(context) {
			
			case WM_OP_EXEC_REGION_WIN:
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
				
				retval= wm_operator_invoke(C, ot, event, properties, reports);
				
				/* set region back */
				CTX_wm_region_set(C, ar);
				
				return retval;
			}
			case WM_OP_EXEC_AREA:
			case WM_OP_INVOKE_AREA:
			{
					/* remove region from context */
				ARegion *ar= CTX_wm_region(C);

				CTX_wm_region_set(C, NULL);
				retval= wm_operator_invoke(C, ot, event, properties, reports);
				CTX_wm_region_set(C, ar);

				return retval;
			}
			case WM_OP_EXEC_SCREEN:
			case WM_OP_INVOKE_SCREEN:
			{
				/* remove region + area from context */
				ARegion *ar= CTX_wm_region(C);
				ScrArea *area= CTX_wm_area(C);

				CTX_wm_region_set(C, NULL);
				CTX_wm_area_set(C, NULL);
				retval= wm_operator_invoke(C, ot, event, properties, reports);
				CTX_wm_region_set(C, ar);
				CTX_wm_area_set(C, area);

				return retval;
			}
			case WM_OP_EXEC_DEFAULT:
			case WM_OP_INVOKE_DEFAULT:
				return wm_operator_invoke(C, ot, event, properties, reports);
		}
	}
	
	return 0;
}


/* invokes operator in context */
int WM_operator_name_call(bContext *C, const char *opstring, int context, PointerRNA *properties)
{
	wmOperatorType *ot= WM_operatortype_find(opstring, 0);
	if(ot)
		return wm_operator_call_internal(C, ot, context, properties, NULL);

	return 0;
}

/* Similar to WM_operator_name_call called with WM_OP_EXEC_DEFAULT context.
   - wmOperatorType is used instead of operator name since python alredy has the operator type
   - poll() must be called by python before this runs.
   - reports can be passed to this function (so python can report them as exceptions)
*/
int WM_operator_call_py(bContext *C, wmOperatorType *ot, int context, PointerRNA *properties, ReportList *reports)
{
	int retval= OPERATOR_CANCELLED;

#if 0
	wmOperator *op;
	wmWindowManager *wm=	CTX_wm_manager(C);
	op= wm_operator_create(wm, ot, properties, reports);

	if (op->type->exec)
		retval= op->type->exec(C, op);
	else
		printf("error \"%s\" operator has no exec function, python cannot call it\n", op->type->name);
#endif

	retval= wm_operator_call_internal(C, ot, context, properties, reports);
	
	/* keep the reports around if needed later */
	if (retval & OPERATOR_RUNNING_MODAL || ot->flag & OPTYPE_REGISTER)
	{
		reports->flag |= RPT_FREE;
	}
	
	return retval;
}


/* ********************* handlers *************** */

/* future extra customadata free? */
static void wm_event_free_handler(wmEventHandler *handler)
{
	MEM_freeN(handler);
}

/* only set context when area/region is part of screen */
static void wm_handler_op_context(bContext *C, wmEventHandler *handler)
{
	bScreen *screen= CTX_wm_screen(C);
	
	if(screen && handler->op) {
		if(handler->op_area==NULL)
			CTX_wm_area_set(C, NULL);
		else {
			ScrArea *sa;
			
			for(sa= screen->areabase.first; sa; sa= sa->next)
				if(sa==handler->op_area)
					break;
			if(sa==NULL) {
				/* when changing screen layouts with running modal handlers (like render display), this
				   is not an error to print */
				if(handler->op==NULL)
					printf("internal error: handler (%s) has invalid area\n", handler->op->type->idname);
			}
			else {
				ARegion *ar;
				CTX_wm_area_set(C, sa);
				for(ar= sa->regionbase.first; ar; ar= ar->next)
					if(ar==handler->op_region)
						break;
				/* XXX no warning print here, after full-area and back regions are remade */
				if(ar)
					CTX_wm_region_set(C, ar);
			}
		}
	}
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
				
				wm_handler_op_context(C, handler);

				handler->op->type->cancel(C, handler->op);

				CTX_wm_area_set(C, area);
				CTX_wm_region_set(C, region);
			}

			WM_cursor_ungrab(CTX_wm_window(C));
			WM_operator_free(handler->op);
		}
		else if(handler->ui_remove) {
			ScrArea *area= CTX_wm_area(C);
			ARegion *region= CTX_wm_region(C);
			ARegion *menu= CTX_wm_menu(C);
			
			if(handler->ui_area) CTX_wm_area_set(C, handler->ui_area);
			if(handler->ui_region) CTX_wm_region_set(C, handler->ui_region);
			if(handler->ui_menu) CTX_wm_menu_set(C, handler->ui_menu);

			handler->ui_remove(C, handler->ui_userdata);

			CTX_wm_area_set(C, area);
			CTX_wm_region_set(C, region);
			CTX_wm_menu_set(C, menu);
		}

		wm_event_free_handler(handler);
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

static int wm_eventmatch(wmEvent *winevent, wmKeyMapItem *kmi)
{
	int kmitype= wm_userdef_event_map(kmi->type);

	if(kmi->flag & KMI_INACTIVE) return 0;

	/* exception for middlemouse emulation */
	if((U.flag & USER_TWOBUTTONMOUSE) && (kmi->type == MIDDLEMOUSE)) {
		if(winevent->type == LEFTMOUSE && winevent->alt) {
			wmKeyMapItem tmp= *kmi;

			tmp.type= winevent->type;
			tmp.alt= winevent->alt;
			if(wm_eventmatch(winevent, &tmp))
				return 1;
		}
	}

	/* the matching rules */
	if(kmitype==KM_TEXTINPUT)
		if(ISTEXTINPUT(winevent->type) && winevent->ascii) return 1;
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
		
	/* key modifiers always check when event has it */
	/* otherwise regular keypresses with keymodifier still work */
	if(winevent->keymodifier)
		if(ISTEXTINPUT(winevent->type)) 
			if(winevent->keymodifier!=kmi->keymodifier) return 0;
	
	return 1;
}

static int wm_event_always_pass(wmEvent *event)
{
	/* some events we always pass on, to ensure proper communication */
	return ELEM4(event->type, TIMER, TIMER0, TIMER1, TIMER2);
}

/* operator exists */
static void wm_event_modalkeymap(const bContext *C, wmOperator *op, wmEvent *event)
{
	if(op->type->modalkeymap) {
		wmKeyMap *keymap= WM_keymap_active(CTX_wm_manager(C), op->type->modalkeymap);
		wmKeyMapItem *kmi;

		for(kmi= keymap->items.first; kmi; kmi= kmi->next) {
			if(wm_eventmatch(event, kmi)) {
					
				event->type= EVT_MODAL_MAP;
				event->val= kmi->propvalue;
			}
		}
	}
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
			
			wm_handler_op_context(C, handler);
			wm_region_mouse_co(C, event);
			wm_event_modalkeymap(C, op, event);
			
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
				op->customdata= NULL;

				if(ot->flag & OPTYPE_UNDO)
					ED_undo_push_op(C, op);
				
				if((ot->flag & OPTYPE_REGISTER) || (G.f & G_DEBUG))
					wm_operator_register(C, op);
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
				WM_cursor_ungrab(CTX_wm_window(C));

				BLI_remlink(handlers, handler);
				wm_event_free_handler(handler);
				
				/* prevent silly errors from operator users */
				//retval &= ~OPERATOR_PASS_THROUGH;
			}
			
		}
		else
			printf("wm_handler_operator_call error\n");
	}
	else {
		wmOperatorType *ot= WM_operatortype_find(event->keymap_idname, 0);

		if(ot)
			retval= wm_operator_invoke(C, ot, event, properties, NULL);
	}

	if(retval & OPERATOR_PASS_THROUGH)
		return WM_HANDLER_CONTINUE;

	return WM_HANDLER_BREAK;
}

static int wm_handler_ui_call(bContext *C, wmEventHandler *handler, wmEvent *event)
{
	ScrArea *area= CTX_wm_area(C);
	ARegion *region= CTX_wm_region(C);
	ARegion *menu= CTX_wm_menu(C);
	int retval, always_pass;
			
	/* we set context to where ui handler came from */
	if(handler->ui_area) CTX_wm_area_set(C, handler->ui_area);
	if(handler->ui_region) CTX_wm_region_set(C, handler->ui_region);
	if(handler->ui_menu) CTX_wm_menu_set(C, handler->ui_menu);

	/* in advance to avoid access to freed event on window close */
	always_pass= wm_event_always_pass(event);

	retval= handler->ui_handle(C, event, handler->ui_userdata);

	/* putting back screen context */
	if((retval != WM_UI_HANDLER_BREAK) || always_pass) {
		CTX_wm_area_set(C, area);
		CTX_wm_region_set(C, region);
		CTX_wm_menu_set(C, menu);
	}
	else {
		/* this special cases is for areas and regions that get removed */
		CTX_wm_area_set(C, NULL);
		CTX_wm_region_set(C, NULL);
		CTX_wm_menu_set(C, NULL);
	}

	if(retval == WM_UI_HANDLER_BREAK)
		return WM_HANDLER_BREAK;

	return WM_HANDLER_CONTINUE;
}

/* fileselect handlers are only in the window queue, so it's save to switch screens or area types */
static int wm_handler_fileselect_call(bContext *C, ListBase *handlers, wmEventHandler *handler, wmEvent *event)
{
	SpaceFile *sfile;
	int action= WM_HANDLER_CONTINUE;
	
	if(event->type != EVT_FILESELECT)
		return action;
	if(handler->op != (wmOperator *)event->customdata)
		return action;
	
	switch(event->val) {
		case EVT_FILESELECT_OPEN: 
		case EVT_FILESELECT_FULL_OPEN: 
			{	
				if(event->val==EVT_FILESELECT_OPEN)
					ED_area_newspace(C, handler->op_area, SPACE_FILE);
				else
					ED_screen_full_newspace(C, handler->op_area, SPACE_FILE);
				
				/* settings for filebrowser, sfile is not operator owner but sends events */
				sfile= (SpaceFile*)CTX_wm_space_data(C);
				sfile->op= handler->op;

				ED_fileselect_set_params(sfile);
				
				action= WM_HANDLER_BREAK;
			}
			break;
			
		case EVT_FILESELECT_EXEC:
		case EVT_FILESELECT_CANCEL:
			{
				/* XXX validate area and region? */
				bScreen *screen= CTX_wm_screen(C);
				char *path= RNA_string_get_alloc(handler->op->ptr, "path", NULL, 0);
				
				if(screen != handler->filescreen)
					ED_screen_full_prevspace(C);
				else
					ED_area_prevspace(C);
				
				/* remlink now, for load file case */
				BLI_remlink(handlers, handler);
				
				wm_handler_op_context(C, handler);

				if(event->val==EVT_FILESELECT_EXEC) {
					/* a bit weak, might become arg for WM_event_fileselect? */
					/* XXX also extension code in image-save doesnt work for this yet */
					if(strncmp(handler->op->type->name, "Save", 4)==0) {
						/* this gives ownership to pupmenu */
						uiPupMenuSaveOver(C, handler->op, (path)? path: "");
					}
					else {
						int retval= handler->op->type->exec(C, handler->op);
						
						if (retval & OPERATOR_FINISHED)
							if(G.f & G_DEBUG)
								wm_operator_print(handler->op);
						
						WM_operator_free(handler->op);
					}
				}
				else {
					if(handler->op->type->cancel)
						handler->op->type->cancel(C, handler->op);

					WM_operator_free(handler->op);
				}

				CTX_wm_area_set(C, NULL);
				
				wm_event_free_handler(handler);
				if(path)
					MEM_freeN(path);
				
				action= WM_HANDLER_BREAK;
			}
			break;
	}
	
	return action;
}

static int handler_boundbox_test(wmEventHandler *handler, wmEvent *event)
{
	if(handler->bbwin) {
		if(handler->bblocal) {
			rcti rect= *handler->bblocal;
			BLI_translate_rcti(&rect, handler->bbwin->xmin, handler->bbwin->ymin);

			if(BLI_in_rcti(&rect, event->x, event->y))
				return 1;
			else if(event->type==MOUSEMOVE && BLI_in_rcti(&rect, event->prevx, event->prevy))
				return 1;
			else
				return 0;
		}
		else {
			if(BLI_in_rcti(handler->bbwin, event->x, event->y))
				return 1;
			else if(event->type==MOUSEMOVE && BLI_in_rcti(handler->bbwin, event->prevx, event->prevy))
				return 1;
			else
				return 0;
		}
	}
	return 1;
}

static int wm_handlers_do(bContext *C, wmEvent *event, ListBase *handlers)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmEventHandler *handler, *nexthandler;
	int action= WM_HANDLER_CONTINUE;
	int always_pass;

	if(handlers==NULL) return action;

	/* modal handlers can get removed in this loop, we keep the loop this way */
	for(handler= handlers->first; handler; handler= nexthandler) {
		nexthandler= handler->next;

		/* optional boundbox */
		if(handler_boundbox_test(handler, event)) {
			/* in advance to avoid access to freed event on window close */
			always_pass= wm_event_always_pass(event);
		
			/* modal+blocking handler */
			if(handler->flag & WM_HANDLER_BLOCKING)
				action= WM_HANDLER_BREAK;

			if(handler->keymap) {
				wmKeyMap *keymap= WM_keymap_active(wm, handler->keymap);
				wmKeyMapItem *kmi;
				
				if(!keymap->poll || keymap->poll(C)) {
					for(kmi= keymap->items.first; kmi; kmi= kmi->next) {
						if(wm_eventmatch(event, kmi)) {
							
							event->keymap_idname= kmi->idname;	/* weak, but allows interactive callback to not use rawkey */
							
							action= wm_handler_operator_call(C, handlers, handler, event, kmi->ptr);
							if(action==WM_HANDLER_BREAK)  /* not always_pass here, it denotes removed handler */
								break;
						}
					}
				}
			}
			else if(handler->ui_handle) {
				action= wm_handler_ui_call(C, handler, event);
			}
			else if(handler->type==WM_HANDLER_FILESELECT) {
				/* screen context changes here */
				action= wm_handler_fileselect_call(C, handlers, handler, event);
			}
			else {
				/* modal, swallows all */
				action= wm_handler_operator_call(C, handlers, handler, event, NULL);
			}

			if(action==WM_HANDLER_BREAK) {
				if(always_pass)
					action= WM_HANDLER_CONTINUE;
				else
					break;
			}
		}
		
		/* fileread case */
		if(CTX_wm_window(C)==NULL)
			break;
	}
	return action;
}

static int wm_event_inside_i(wmEvent *event, rcti *rect)
{
	if(wm_event_always_pass(event))
		return 1;
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
			
			/* first we do priority handlers, modal + some limited keymaps */
			action= wm_handlers_do(C, event, &win->modalhandlers);
			
			/* fileread case */
			if(CTX_wm_window(C)==NULL)
				return;
			
			/* builtin tweak, if action is break it removes tweak */
			wm_tweakevent_test(C, event, action);

			if(action==WM_HANDLER_CONTINUE) {
				ScrArea *sa;
				ARegion *ar;
				int doit= 0;
				
				/* XXX to solve, here screen handlers? */
				if(event->type==MOUSEMOVE) {
					/* state variables in screen, cursors */
					ED_screen_set_subwinactive(win, event);	
					/* for regions having custom cursors */
					wm_paintcursor_test(C, event);
				}

				for(sa= win->screen->areabase.first; sa; sa= sa->next) {
					if(wm_event_inside_i(event, &sa->totrct)) {
						CTX_wm_area_set(C, sa);

						if(action==WM_HANDLER_CONTINUE) {
							for(ar=sa->regionbase.first; ar; ar= ar->next) {
								if(wm_event_inside_i(event, &ar->winrct)) {
									CTX_wm_region_set(C, ar);
									action= wm_handlers_do(C, event, &ar->handlers);

									doit |= (BLI_in_rcti(&ar->winrct, event->x, event->y));
									
									if(action==WM_HANDLER_BREAK)
										break;
								}
							}
						}

						CTX_wm_region_set(C, NULL);

						if(action==WM_HANDLER_CONTINUE)
							action= wm_handlers_do(C, event, &sa->handlers);

						CTX_wm_area_set(C, NULL);

						/* NOTE: do not escape on WM_HANDLER_BREAK, mousemove needs handled for previous area */
					}
				}
				
				if(action==WM_HANDLER_CONTINUE) {
					/* also some non-modal handlers need active area/region */
					CTX_wm_area_set(C, area_event_inside(C, event->x, event->y));
					CTX_wm_region_set(C, region_event_inside(C, event->x, event->y));

					action= wm_handlers_do(C, event, &win->handlers);

					/* fileread case */
					if(CTX_wm_window(C)==NULL)
						return;
				}

				/* XXX hrmf, this gives reliable previous mouse coord for area change, feels bad? 
				   doing it on ghost queue gives errors when mousemoves go over area borders */
				if(doit && win->screen && win->screen->subwinactive != win->screen->mainwin) {
					win->eventstate->prevx= event->x;
					win->eventstate->prevy= event->y;
				}
			}
			
			/* unlink and free here, blender-quit then frees all */
			BLI_remlink(&win->queue, event);
			wm_event_free(event);
			
		}
		
		/* only add mousemove when queue was read entirely */
		if(win->addmousemove && win->eventstate) {
			wmEvent event= *(win->eventstate);
			event.type= MOUSEMOVE;
			event.prevx= event.x;
			event.prevy= event.y;
			wm_event_add(win, &event);
			win->addmousemove= 0;
		}
		
		CTX_wm_window_set(C, NULL);
	}
}

/* ********** filesector handling ************ */

void WM_event_fileselect_event(bContext *C, void *ophandle, int eventval)
{
	/* add to all windows! */
	wmWindow *win;
	
	for(win= CTX_wm_manager(C)->windows.first; win; win= win->next) {
		wmEvent event= *win->eventstate;
		
		event.type= EVT_FILESELECT;
		event.val= eventval;
		event.customdata= ophandle;		// only as void pointer type check

		wm_event_add(win, &event);
	}
}

/* operator is supposed to have a filled "path" property */
/* optional property: filetype (XXX enum?) */

/* Idea is to keep a handler alive on window queue, owning the operator.
   The filewindow can send event to make it execute, thus ensuring
   executing happens outside of lower level queues, with UI refreshed. 
   Should also allow multiwin solutions */

void WM_event_add_fileselect(bContext *C, wmOperator *op)
{
	wmEventHandler *handler= MEM_callocN(sizeof(wmEventHandler), "fileselect handler");
	wmWindow *win= CTX_wm_window(C);
	int full= 1;	// XXX preset?
	
	handler->type= WM_HANDLER_FILESELECT;
	handler->op= op;
	handler->op_area= CTX_wm_area(C);
	handler->op_region= CTX_wm_region(C);
	handler->filescreen= CTX_wm_screen(C);
	
	BLI_addhead(&win->modalhandlers, handler);
	
	WM_event_fileselect_event(C, op, full?EVT_FILESELECT_FULL_OPEN:EVT_FILESELECT_OPEN);
}

/* lets not expose struct outside wm? */
void WM_event_set_handler_flag(wmEventHandler *handler, int flag)
{
	handler->flag= flag;
}

wmEventHandler *WM_event_add_modal_handler(bContext *C, wmOperator *op)
{
	wmEventHandler *handler= MEM_callocN(sizeof(wmEventHandler), "event modal handler");
	wmWindow *win= CTX_wm_window(C);
	
	/* operator was part of macro */
	if(op->opm) {
		/* give the mother macro to the handler */
		handler->op= op->opm;
		/* mother macro opm becomes the macro element */
		handler->op->opm= op;
	}
	else
		handler->op= op;
	
	handler->op_area= CTX_wm_area(C);		/* means frozen screen context for modal handlers! */
	handler->op_region= CTX_wm_region(C);
	
	BLI_addhead(&win->modalhandlers, handler);

	return handler;
}

wmEventHandler *WM_event_add_keymap_handler(ListBase *handlers, wmKeyMap *keymap)
{
	wmEventHandler *handler;

	if(!keymap) {
		printf("WM_event_add_keymap_handler called with NULL keymap\n");
		return NULL;
	}

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
wmEventHandler *WM_event_add_keymap_handler_priority(ListBase *handlers, wmKeyMap *keymap, int priority)
{
	wmEventHandler *handler;
	
	WM_event_remove_keymap_handler(handlers, keymap);
	
	handler= MEM_callocN(sizeof(wmEventHandler), "event keymap handler");
	BLI_addhead(handlers, handler);
	handler->keymap= keymap;
	
	return handler;
}

wmEventHandler *WM_event_add_keymap_handler_bb(ListBase *handlers, wmKeyMap *keymap, rcti *bblocal, rcti *bbwin)
{
	wmEventHandler *handler= WM_event_add_keymap_handler(handlers, keymap);
	
	if(handler) {
		handler->bblocal= bblocal;
		handler->bbwin= bbwin;
	}
	return handler;
}

void WM_event_remove_keymap_handler(ListBase *handlers, wmKeyMap *keymap)
{
	wmEventHandler *handler;
	
	for(handler= handlers->first; handler; handler= handler->next) {
		if(handler->keymap==keymap) {
			BLI_remlink(handlers, handler);
			wm_event_free_handler(handler);
			break;
		}
	}
}

wmEventHandler *WM_event_add_ui_handler(const bContext *C, ListBase *handlers, wmUIHandlerFunc func, wmUIHandlerRemoveFunc remove, void *userdata)
{
	wmEventHandler *handler= MEM_callocN(sizeof(wmEventHandler), "event ui handler");
	handler->ui_handle= func;
	handler->ui_remove= remove;
	handler->ui_userdata= userdata;
	handler->ui_area= (C)? CTX_wm_area(C): NULL;
	handler->ui_region= (C)? CTX_wm_region(C): NULL;
	handler->ui_menu= (C)? CTX_wm_menu(C): NULL;
	
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
			break;
		}
	}
}

void WM_event_add_mousemove(bContext *C)
{
	wmWindow *window= CTX_wm_window(C);
	
	window->addmousemove= 1;
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
	if ((td != NULL) && td->Active != GHOST_kTabletModeNone) {
		struct wmTabletData *wmtab= MEM_mallocN(sizeof(wmTabletData), "customdata tablet");
		
		wmtab->Active = (int)td->Active;
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
#if defined(__APPLE__) && defined(GHOST_COCOA)
				//Cocoa already uses coordinates with y=0 at bottom, and returns inwindow coordinates on mouse moved event
				event.type= MOUSEMOVE;
				event.x= evt->x = cd->x;
				event.y = evt->y = cd->y;
#else
				int cx, cy;
				GHOST_ScreenToClient(win->ghostwin, cd->x, cd->y, &cx, &cy);
				event.type= MOUSEMOVE;
				event.x= evt->x= cx;
				event.y= evt->y= (win->sizey-1) - cy;
#endif
				update_tablet_data(win, &event);
				wm_event_add(win, &event);
			}
			break;
		}
		/* mouse button */
		case GHOST_kEventButtonDown:
		case GHOST_kEventButtonUp: {
			GHOST_TEventButtonData *bd= customdata;
			event.val= (type==GHOST_kEventButtonDown) ? KM_PRESS:KM_RELEASE; /* Note!, this starts as 0/1 but later is converted to KM_PRESS/KM_RELEASE by tweak */
			
			if (bd->button == GHOST_kButtonMaskLeft)
				event.type= LEFTMOUSE;
			else if (bd->button == GHOST_kButtonMaskRight)
				event.type= RIGHTMOUSE;
			else if (bd->button == GHOST_kButtonMaskButton4)
				event.type= BUTTON4MOUSE;
			else if (bd->button == GHOST_kButtonMaskButton5)
				event.type= BUTTON5MOUSE;
			else
				event.type= MIDDLEMOUSE;
			
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
			event.val= (type==GHOST_kEventKeyDown)?KM_PRESS:KM_RELEASE;
			
			/* exclude arrow keys, esc, etc from text input */
			if(type==GHOST_kEventKeyUp || (event.ascii<32 && event.ascii>0))
				event.ascii= '\0';
			
			/* modifiers */
			if (event.type==LEFTSHIFTKEY || event.type==RIGHTSHIFTKEY) {
				event.shift= evt->shift= (event.val==KM_PRESS);
				if(event.val==KM_PRESS && (evt->ctrl || evt->alt || evt->oskey))
				   event.shift= evt->shift = 3;		// define?
			} 
			else if (event.type==LEFTCTRLKEY || event.type==RIGHTCTRLKEY) {
				event.ctrl= evt->ctrl= (event.val==KM_PRESS);
				if(event.val==KM_PRESS && (evt->shift || evt->alt || evt->oskey))
				   event.ctrl= evt->ctrl = 3;		// define?
			} 
			else if (event.type==LEFTALTKEY || event.type==RIGHTALTKEY) {
				event.alt= evt->alt= (event.val==KM_PRESS);
				if(event.val==KM_PRESS && (evt->ctrl || evt->shift || evt->oskey))
				   event.alt= evt->alt = 3;		// define?
			} 
			else if (event.type==COMMANDKEY) {
				event.oskey= evt->oskey= (event.val==KM_PRESS);
				if(event.val==KM_PRESS && (evt->ctrl || evt->alt || evt->shift))
				   event.oskey= evt->oskey = 3;		// define?
			}
			else {
				if(event.val==KM_PRESS && event.keymodifier==0)
					evt->keymodifier= event.type; /* only set in eventstate, for next event */
				else if(event.val==KM_RELEASE && event.keymodifier==event.type)
					event.keymodifier= evt->keymodifier= 0;
			}

			/* this case happens on some systems that on holding a key pressed,
			   generate press events without release, we still want to keep the
			   modifier in win->eventstate, but for the press event of the same
			   key we don't want the key modifier */
			if(event.keymodifier == event.type)
				event.keymodifier= 0;
			
			/* if test_break set, it catches this. XXX Keep global for now? */
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
