/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_event_system.c
 *  \ingroup wm
 *
 * Handle events and notifiers from GHOST input (mouse, keyboard, tablet, ndof).
 *
 * Also some operator reports utility functions.
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
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "BKE_sound.h"

#include "ED_fileselect.h"
#include "ED_info.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_util.h"

#include "RNA_access.h"

#include "BIF_gl.h"

#include "UI_interface.h"

#include "PIL_time.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_window.h"
#include "wm_event_system.h"
#include "wm_event_types.h"
#include "wm_draw.h"

#ifndef NDEBUG
#  include "RNA_enum_types.h"
#endif

static void wm_notifier_clear(wmNotifier *note);
static void update_tablet_data(wmWindow *win, wmEvent *event);

static int wm_operator_call_internal(bContext *C, wmOperatorType *ot, PointerRNA *properties, ReportList *reports,
                                     const short context, const bool poll_only);

/* ************ event management ************** */

void wm_event_add(wmWindow *win, const wmEvent *event_to_add)
{
	wmEvent *event = MEM_mallocN(sizeof(wmEvent), "wmEvent");
	
	*event = *event_to_add;

	update_tablet_data(win, event);

	BLI_addtail(&win->queue, event);
}

void wm_event_free(wmEvent *event)
{
	if (event->customdata) {
		if (event->customdatafree) {
			/* note: pointer to listbase struct elsewhere */
			if (event->custom == EVT_DATA_LISTBASE)
				BLI_freelistN(event->customdata);
			else
				MEM_freeN(event->customdata);
		}
	}

	if (event->tablet_data) {
		MEM_freeN(event->tablet_data);
	}

	MEM_freeN(event);
}

void wm_event_free_all(wmWindow *win)
{
	wmEvent *event;
	
	while ((event = BLI_pophead(&win->queue))) {
		wm_event_free(event);
	}
}

void wm_event_init_from_window(wmWindow *win, wmEvent *event)
{
	/* make sure we don't copy any owned pointers */
	BLI_assert(win->eventstate->tablet_data == NULL);

	*event = *(win->eventstate);
}

/* ********************* notifiers, listeners *************** */

static bool wm_test_duplicate_notifier(wmWindowManager *wm, unsigned int type, void *reference)
{
	wmNotifier *note;

	for (note = wm->queue.first; note; note = note->next)
		if ((note->category | note->data | note->subtype | note->action) == type && note->reference == reference)
			return 1;
	
	return 0;
}

/* XXX: in future, which notifiers to send to other windows? */
void WM_event_add_notifier(const bContext *C, unsigned int type, void *reference)
{
	ARegion *ar;
	wmWindowManager *wm = CTX_wm_manager(C);
	wmNotifier *note;

	if (wm_test_duplicate_notifier(wm, type, reference))
		return;

	note = MEM_callocN(sizeof(wmNotifier), "notifier");
	
	note->wm = wm;
	BLI_addtail(&note->wm->queue, note);
	
	note->window = CTX_wm_window(C);
	
	ar = CTX_wm_region(C);
	if (ar)
		note->swinid = ar->swinid;
	
	note->category = type & NOTE_CATEGORY;
	note->data = type & NOTE_DATA;
	note->subtype = type & NOTE_SUBTYPE;
	note->action = type & NOTE_ACTION;
	
	note->reference = reference;
}

void WM_main_add_notifier(unsigned int type, void *reference)
{
	Main *bmain = G.main;
	wmWindowManager *wm = bmain->wm.first;
	wmNotifier *note;

	if (!wm || wm_test_duplicate_notifier(wm, type, reference))
		return;

	note = MEM_callocN(sizeof(wmNotifier), "notifier");
	
	note->wm = wm;
	BLI_addtail(&note->wm->queue, note);
	
	note->category = type & NOTE_CATEGORY;
	note->data = type & NOTE_DATA;
	note->subtype = type & NOTE_SUBTYPE;
	note->action = type & NOTE_ACTION;
	
	note->reference = reference;
}

/**
 * Clear notifiers by reference, Used so listeners don't act on freed data.
 */
void WM_main_remove_notifier_reference(const void *reference)
{
	Main *bmain = G.main;
	wmWindowManager *wm = bmain->wm.first;
	if (wm) {
		wmNotifier *note, *note_next;

		for (note = wm->queue.first; note; note = note_next) {
			note_next = note->next;

			if (note->reference == reference) {
				/* don't remove because this causes problems for #wm_event_do_notifiers
				 * which may be looping on the data (deleting screens) */
				wm_notifier_clear(note);
			}
		}
	}
}

static void wm_notifier_clear(wmNotifier *note)
{
	/* NULL the entire notifier, only leaving (next, prev) members intact */
	memset(((char *)note) + sizeof(Link), 0, sizeof(*note) - sizeof(Link));
}

/* called in mainloop */
void wm_event_do_notifiers(bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmNotifier *note, *next;
	wmWindow *win;
	uint64_t win_combine_v3d_datamask = 0;
	
	if (wm == NULL)
		return;
	
	/* cache & catch WM level notifiers, such as frame change, scene/screen set */
	for (win = wm->windows.first; win; win = win->next) {
		bool do_anim = false;
		
		CTX_wm_window_set(C, win);
		
		for (note = wm->queue.first; note; note = next) {
			next = note->next;

			if (note->category == NC_WM) {
				if (ELEM(note->data, ND_FILEREAD, ND_FILESAVE)) {
					wm->file_saved = 1;
					wm_window_title(wm, win);
				}
				else if (note->data == ND_DATACHANGED)
					wm_window_title(wm, win);
			}
			if (note->window == win) {
				if (note->category == NC_SCREEN) {
					if (note->data == ND_SCREENBROWSE) {
						/* free popup handlers only [#35434] */
						UI_remove_popup_handlers_all(C, &win->modalhandlers);


						ED_screen_set(C, note->reference);  // XXX hrms, think this over!
						if (G.debug & G_DEBUG_EVENTS)
							printf("%s: screen set %p\n", __func__, note->reference);
					}
					else if (note->data == ND_SCREENDELETE) {
						ED_screen_delete(C, note->reference);   // XXX hrms, think this over!
						if (G.debug & G_DEBUG_EVENTS)
							printf("%s: screen delete %p\n", __func__, note->reference);
					}
				}
			}

			if (note->window == win ||
			    (note->window == NULL && (note->reference == NULL || note->reference == win->screen->scene)))
			{
				if (note->category == NC_SCENE) {
					if (note->data == ND_FRAME)
						do_anim = true;
				}
			}
			if (ELEM5(note->category, NC_SCENE, NC_OBJECT, NC_GEOM, NC_SCENE, NC_WM)) {
				ED_info_stats_clear(win->screen->scene);
				WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO, NULL);
			}
		}
		if (do_anim) {

			/* XXX, quick frame changes can cause a crash if framechange and rendering
			 * collide (happens on slow scenes), BKE_scene_update_for_newframe can be called
			 * twice which can depgraph update the same object at once */
			if (G.is_rendering == false) {

				/* depsgraph gets called, might send more notifiers */
				ED_update_for_newframe(CTX_data_main(C), win->screen->scene, 1);
			}
		}
	}
	
	/* the notifiers are sent without context, to keep it clean */
	while ((note = BLI_pophead(&wm->queue))) {
		for (win = wm->windows.first; win; win = win->next) {
			
			/* filter out notifiers */
			if (note->category == NC_SCREEN && note->reference && note->reference != win->screen) {
				/* pass */
			}
			else if (note->category == NC_SCENE && note->reference && note->reference != win->screen->scene) {
				/* pass */
			}
			else {
				ScrArea *sa;
				ARegion *ar;

				/* XXX context in notifiers? */
				CTX_wm_window_set(C, win);

				/* printf("notifier win %d screen %s cat %x\n", win->winid, win->screen->id.name + 2, note->category); */
				ED_screen_do_listen(C, note);

				for (ar = win->screen->regionbase.first; ar; ar = ar->next) {
					ED_region_do_listen(win->screen, NULL, ar, note);
				}
				
				for (sa = win->screen->areabase.first; sa; sa = sa->next) {
					ED_area_do_listen(win->screen, sa, note);
					for (ar = sa->regionbase.first; ar; ar = ar->next) {
						ED_region_do_listen(win->screen, sa, ar, note);
					}
				}
			}
		}
		
		MEM_freeN(note);
	}
	
	/* combine datamasks so 1 win doesn't disable UV's in another [#26448] */
	for (win = wm->windows.first; win; win = win->next) {
		win_combine_v3d_datamask |= ED_view3d_screen_datamask(win->screen);
	}

	/* cached: editor refresh callbacks now, they get context */
	for (win = wm->windows.first; win; win = win->next) {
		ScrArea *sa;
		
		CTX_wm_window_set(C, win);
		for (sa = win->screen->areabase.first; sa; sa = sa->next) {
			if (sa->do_refresh) {
				CTX_wm_area_set(C, sa);
				ED_area_do_refresh(C, sa);
			}
		}
		
		/* XXX make lock in future, or separated derivedmesh users in scene */
		if (G.is_rendering == false) {
			/* depsgraph & animation: update tagged datablocks */
			Main *bmain = CTX_data_main(C);

			/* copied to set's in scene_update_tagged_recursive() */
			win->screen->scene->customdata_mask = win_combine_v3d_datamask;

			/* XXX, hack so operators can enforce datamasks [#26482], gl render */
			win->screen->scene->customdata_mask |= win->screen->scene->customdata_mask_modal;

			BKE_scene_update_tagged(bmain->eval_ctx, bmain, win->screen->scene);
		}
	}

	CTX_wm_window_set(C, NULL);
}

static int wm_event_always_pass(wmEvent *event)
{
	/* some events we always pass on, to ensure proper communication */
	return ISTIMER(event->type) || (event->type == WINDEACTIVATE) || (event->type == EVT_BUT_OPEN);
}

/* ********************* ui handler ******************* */

static int wm_handler_ui_call(bContext *C, wmEventHandler *handler, wmEvent *event, int always_pass)
{
	ScrArea *area = CTX_wm_area(C);
	ARegion *region = CTX_wm_region(C);
	ARegion *menu = CTX_wm_menu(C);
	static bool do_wheel_ui = true;
	const bool is_wheel = ELEM3(event->type, WHEELUPMOUSE, WHEELDOWNMOUSE, MOUSEPAN);
	int retval;
	
	/* UI code doesn't handle return values - it just always returns break. 
	 * to make the DBL_CLICK conversion work, we just don't send this to UI, except mouse clicks */
	if (event->type != LEFTMOUSE && event->val == KM_DBL_CLICK)
		return WM_HANDLER_CONTINUE;
	
	/* UI is quite aggressive with swallowing events, like scrollwheel */
	/* I realize this is not extremely nice code... when UI gets keymaps it can be maybe smarter */
	if (do_wheel_ui == false) {
		if (is_wheel)
			return WM_HANDLER_CONTINUE;
		else if (wm_event_always_pass(event) == 0)
			do_wheel_ui = true;
	}
	
	/* we set context to where ui handler came from */
	if (handler->ui_area) CTX_wm_area_set(C, handler->ui_area);
	if (handler->ui_region) CTX_wm_region_set(C, handler->ui_region);
	if (handler->ui_menu) CTX_wm_menu_set(C, handler->ui_menu);

	retval = handler->ui_handle(C, event, handler->ui_userdata);

	/* putting back screen context */
	if ((retval != WM_UI_HANDLER_BREAK) || always_pass) {
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
	
	if (retval == WM_UI_HANDLER_BREAK)
		return WM_HANDLER_BREAK;
	
	/* event not handled in UI, if wheel then we temporarily disable it */
	if (is_wheel)
		do_wheel_ui = false;
	
	return WM_HANDLER_CONTINUE;
}

static void wm_handler_ui_cancel(bContext *C)
{
	wmWindow *win = CTX_wm_window(C);
	ARegion *ar = CTX_wm_region(C);
	wmEventHandler *handler, *nexthandler;

	if (!ar)
		return;

	for (handler = ar->handlers.first; handler; handler = nexthandler) {
		nexthandler = handler->next;

		if (handler->ui_handle) {
			wmEvent event;

			wm_event_init_from_window(win, &event);
			event.type = EVT_BUT_CANCEL;
			handler->ui_handle(C, &event, handler->ui_userdata);
		}
	}
}

/* ********************* operators ******************* */

int WM_operator_poll(bContext *C, wmOperatorType *ot)
{
	wmOperatorTypeMacro *otmacro;
	
	for (otmacro = ot->macro.first; otmacro; otmacro = otmacro->next) {
		wmOperatorType *ot_macro = WM_operatortype_find(otmacro->idname, 0);
		
		if (0 == WM_operator_poll(C, ot_macro))
			return 0;
	}
	
	/* python needs operator type, so we added exception for it */
	if (ot->pyop_poll)
		return ot->pyop_poll(C, ot);
	else if (ot->poll)
		return ot->poll(C);

	return 1;
}

/* sets up the new context and calls 'wm_operator_invoke()' with poll_only */
int WM_operator_poll_context(bContext *C, wmOperatorType *ot, short context)
{
	return wm_operator_call_internal(C, ot, NULL, NULL, context, true);
}

static void wm_operator_print(bContext *C, wmOperator *op)
{
	/* context is needed for enum function */
	char *buf = WM_operator_pystring(C, op, false, true);
	puts(buf);
	MEM_freeN(buf);
}

/**
 * Sets the active region for this space from the context.
 *
 * \see #BKE_area_find_region_active_win
 */
void WM_operator_region_active_win_set(bContext *C)
{
	ScrArea *sa = CTX_wm_area(C);
	if (sa) {
		ARegion *ar = CTX_wm_region(C);
		if (ar && ar->regiontype == RGN_TYPE_WINDOW) {
			sa->region_active_win = BLI_findindex(&sa->regionbase, ar);
		}
	}
}

/* for debugging only, getting inspecting events manually is tedious */
#ifndef NDEBUG

void WM_event_print(const wmEvent *event)
{
	if (event) {
		const char *unknown = "UNKNOWN";
		const char *type_id = unknown;
		const char *val_id = unknown;

		RNA_enum_identifier(event_type_items, event->type, &type_id);
		RNA_enum_identifier(event_value_items, event->val, &val_id);

		printf("wmEvent  type:%d / %s, val:%d / %s, \n"
		       "         shift:%d, ctrl:%d, alt:%d, oskey:%d, keymodifier:%d, \n"
		       "         mouse:(%d,%d), ascii:'%c', utf8:'%.*s', "
		       "         keymap_idname:%s, pointer:%p\n",
		       event->type, type_id, event->val, val_id,
		       event->shift, event->ctrl, event->alt, event->oskey, event->keymodifier,
		       event->x, event->y, event->ascii,
		       BLI_str_utf8_size(event->utf8_buf), event->utf8_buf,
		       event->keymap_idname, (void *)event);

		if (ISNDOF(event->type)) {
			const wmNDOFMotionData *ndof = event->customdata;
			if (event->type == NDOF_MOTION) {
				printf("   ndof: rot: (%.4f %.4f %.4f),\n"
				       "          tx: (%.4f %.4f %.4f),\n"
				       "          dt: %.4f, progress: %d\n",
				       UNPACK3(ndof->rvec),
				       UNPACK3(ndof->tvec),
				       ndof->dt, ndof->progress);
			}
			else {
				/* ndof buttons printed already */
			}
		}
	}
	else {
		printf("wmEvent - NULL\n");
	}
}

#endif /* NDEBUG */

static void wm_add_reports(const bContext *C, ReportList *reports)
{
	/* if the caller owns them, handle this */
	if (reports->list.first && (reports->flag & RPT_OP_HOLD) == 0) {

		wmWindowManager *wm = CTX_wm_manager(C);
		ReportList *wm_reports = CTX_wm_reports(C);
		ReportTimerInfo *rti;

		/* add reports to the global list, otherwise they are not seen */
		BLI_movelisttolist(&wm_reports->list, &reports->list);
		
		/* After adding reports to the global list, reset the report timer. */
		WM_event_remove_timer(wm, NULL, wm_reports->reporttimer);
		
		/* Records time since last report was added */
		wm_reports->reporttimer = WM_event_add_timer(wm, CTX_wm_window(C), TIMERREPORT, 0.05);
		
		rti = MEM_callocN(sizeof(ReportTimerInfo), "ReportTimerInfo");
		wm_reports->reporttimer->customdata = rti;
	}
}

void WM_report(const bContext *C, ReportType type, const char *message)
{
	ReportList reports;

	BKE_reports_init(&reports, RPT_STORE);
	BKE_report(&reports, type, message);

	wm_add_reports(C, &reports);

	BKE_reports_clear(&reports);
}

void WM_reportf(const bContext *C, ReportType type, const char *format, ...)
{
	DynStr *ds;
	va_list args;

	ds = BLI_dynstr_new();
	va_start(args, format);
	BLI_dynstr_vappendf(ds, format, args);
	va_end(args);

	WM_report(C, type, BLI_dynstr_get_cstring(ds));

	BLI_dynstr_free(ds);
}

/* (caller_owns_reports == true) when called from python */
static void wm_operator_reports(bContext *C, wmOperator *op, int retval, bool caller_owns_reports)
{
	if (caller_owns_reports == false) { /* popup */
		if (op->reports->list.first) {
			/* FIXME, temp setting window, see other call to uiPupMenuReports for why */
			wmWindow *win_prev = CTX_wm_window(C);
			ScrArea *area_prev = CTX_wm_area(C);
			ARegion *ar_prev = CTX_wm_region(C);

			if (win_prev == NULL)
				CTX_wm_window_set(C, CTX_wm_manager(C)->windows.first);

			uiPupMenuReports(C, op->reports);

			CTX_wm_window_set(C, win_prev);
			CTX_wm_area_set(C, area_prev);
			CTX_wm_region_set(C, ar_prev);
		}
	}
	
	if (retval & OPERATOR_FINISHED) {
		if (G.debug & G_DEBUG_WM) {
			/* todo - this print may double up, might want to check more flags then the FINISHED */
			wm_operator_print(C, op);
		}

		if (caller_owns_reports == false) {
			BKE_reports_print(op->reports, RPT_DEBUG); /* print out reports to console. */
		}

		if (op->type->flag & OPTYPE_REGISTER) {
			if (G.background == 0) { /* ends up printing these in the terminal, gets annoying */
				/* Report the python string representation of the operator */
				char *buf = WM_operator_pystring(C, op, false, true);
				BKE_report(CTX_wm_reports(C), RPT_OPERATOR, buf);
				MEM_freeN(buf);
			}
		}
	}

	/* if the caller owns them, handle this */
	wm_add_reports(C, op->reports);
}

/* this function is mainly to check that the rules for freeing
 * an operator are kept in sync.
 */
static bool wm_operator_register_check(wmWindowManager *wm, wmOperatorType *ot)
{
	return wm && (wm->op_undo_depth == 0) && (ot->flag & OPTYPE_REGISTER);
}

static void wm_operator_finished(bContext *C, wmOperator *op, const bool repeat)
{
	wmWindowManager *wm = CTX_wm_manager(C);

	op->customdata = NULL;

	/* we don't want to do undo pushes for operators that are being
	 * called from operators that already do an undo push. usually
	 * this will happen for python operators that call C operators */
	if (wm->op_undo_depth == 0)
		if (op->type->flag & OPTYPE_UNDO)
			ED_undo_push_op(C, op);
	
	if (repeat == 0) {
		if (G.debug & G_DEBUG_WM) {
			char *buf = WM_operator_pystring(C, op, false, true);
			BKE_report(CTX_wm_reports(C), RPT_OPERATOR, buf);
			MEM_freeN(buf);
		}

		if (wm_operator_register_check(wm, op->type)) {
			/* take ownership of reports (in case python provided own) */
			op->reports->flag |= RPT_FREE;

			wm_operator_register(C, op);
			WM_operator_region_active_win_set(C);
		}
		else {
			WM_operator_free(op);
		}
	}
}

/* if repeat is true, it doesn't register again, nor does it free */
static int wm_operator_exec(bContext *C, wmOperator *op, const bool repeat, const bool store)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	int retval = OPERATOR_CANCELLED;
	
	CTX_wm_operator_poll_msg_set(C, NULL);
	
	if (op == NULL || op->type == NULL)
		return retval;
	
	if (0 == WM_operator_poll(C, op->type))
		return retval;
	
	if (op->type->exec) {
		if (op->type->flag & OPTYPE_UNDO)
			wm->op_undo_depth++;

		retval = op->type->exec(C, op);
		OPERATOR_RETVAL_CHECK(retval);

		if (op->type->flag & OPTYPE_UNDO && CTX_wm_manager(C) == wm)
			wm->op_undo_depth--;
	}
	
	/* XXX Disabled the repeat check to address part 2 of #31840.
	 *     Carefully checked all calls to wm_operator_exec and WM_operator_repeat, don't see any reason
	 *     why this was needed, but worth to note it in case something turns bad. (mont29) */
	if (retval & (OPERATOR_FINISHED | OPERATOR_CANCELLED)/* && repeat == 0 */)
		wm_operator_reports(C, op, retval, false);
	
	if (retval & OPERATOR_FINISHED) {
		if (store) {
			if (wm->op_undo_depth == 0) { /* not called by py script */
				WM_operator_last_properties_store(op);
			}
		}
		wm_operator_finished(C, op, repeat);
	}
	else if (repeat == 0) {
		WM_operator_free(op);
	}
	
	return retval | OPERATOR_HANDLED;
	
}

/* simply calls exec with basic checks */
static int wm_operator_exec_notest(bContext *C, wmOperator *op)
{
	int retval = OPERATOR_CANCELLED;

	if (op == NULL || op->type == NULL || op->type->exec == NULL)
		return retval;

	retval = op->type->exec(C, op);
	OPERATOR_RETVAL_CHECK(retval);

	return retval;
}

/**
 * for running operators with frozen context (modal handlers, menus)
 *
 * \param store Store settings for re-use.
 *
 * warning: do not use this within an operator to call its self! [#29537] */
int WM_operator_call_ex(bContext *C, wmOperator *op,
                        const bool store)
{
	return wm_operator_exec(C, op, false, store);
}

int WM_operator_call(bContext *C, wmOperator *op)
{
	return WM_operator_call_ex(C, op, false);
}

/* this is intended to be used when an invoke operator wants to call exec on its self
 * and is basically like running op->type->exec() directly, no poll checks no freeing,
 * since we assume whoever called invoke will take care of that */
int WM_operator_call_notest(bContext *C, wmOperator *op)
{
	return wm_operator_exec_notest(C, op);
}

/* do this operator again, put here so it can share above code */
int WM_operator_repeat(bContext *C, wmOperator *op)
{
	return wm_operator_exec(C, op, true, true);
}
/* true if WM_operator_repeat can run
 * simple check for now but may become more involved.
 * To be sure the operator can run call WM_operator_poll(C, op->type) also, since this call
 * checks if WM_operator_repeat() can run at all, not that it WILL run at any time. */
bool WM_operator_repeat_check(const bContext *UNUSED(C), wmOperator *op)
{
	if (op->type->exec != NULL) {
		return true;
	}
	else if (op->opm) {
		/* for macros, check all have exec() we can call */
		wmOperatorTypeMacro *otmacro;
		for (otmacro = op->opm->type->macro.first; otmacro; otmacro = otmacro->next) {
			wmOperatorType *otm = WM_operatortype_find(otmacro->idname, 0);
			if (otm && otm->exec == NULL) {
				return false;
			}
		}
		return true;
	}

	return false;
}

static wmOperator *wm_operator_create(wmWindowManager *wm, wmOperatorType *ot,
                                      PointerRNA *properties, ReportList *reports)
{
	/* XXX operatortype names are static still. for debug */
	wmOperator *op = MEM_callocN(sizeof(wmOperator), ot->idname);
	
	/* XXX adding new operator could be function, only happens here now */
	op->type = ot;
	BLI_strncpy(op->idname, ot->idname, OP_MAX_TYPENAME);
	
	/* initialize properties, either copy or create */
	op->ptr = MEM_callocN(sizeof(PointerRNA), "wmOperatorPtrRNA");
	if (properties && properties->data) {
		op->properties = IDP_CopyProperty(properties->data);
	}
	else {
		IDPropertyTemplate val = {0};
		op->properties = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");
	}
	RNA_pointer_create(&wm->id, ot->srna, op->properties, op->ptr);

	/* initialize error reports */
	if (reports) {
		op->reports = reports; /* must be initialized already */
	}
	else {
		op->reports = MEM_mallocN(sizeof(ReportList), "wmOperatorReportList");
		BKE_reports_init(op->reports, RPT_STORE | RPT_FREE);
	}
	
	/* recursive filling of operator macro list */
	if (ot->macro.first) {
		static wmOperator *motherop = NULL;
		wmOperatorTypeMacro *otmacro;
		int root = 0;
		
		/* ensure all ops are in execution order in 1 list */
		if (motherop == NULL) {
			motherop = op;
			root = 1;
		}

		
		/* if properties exist, it will contain everything needed */
		if (properties) {
			otmacro = ot->macro.first;

			RNA_STRUCT_BEGIN (properties, prop)
			{

				if (otmacro == NULL)
					break;

				/* skip invalid properties */
				if (strcmp(RNA_property_identifier(prop), otmacro->idname) == 0) {
					wmOperatorType *otm = WM_operatortype_find(otmacro->idname, 0);
					PointerRNA someptr = RNA_property_pointer_get(properties, prop);
					wmOperator *opm = wm_operator_create(wm, otm, &someptr, NULL);

					IDP_ReplaceGroupInGroup(opm->properties, otmacro->properties);

					BLI_addtail(&motherop->macro, opm);
					opm->opm = motherop; /* pointer to mom, for modal() */

					otmacro = otmacro->next;
				}
			}
			RNA_STRUCT_END;
		}
		else {
			for (otmacro = ot->macro.first; otmacro; otmacro = otmacro->next) {
				wmOperatorType *otm = WM_operatortype_find(otmacro->idname, 0);
				wmOperator *opm = wm_operator_create(wm, otm, otmacro->ptr, NULL);

				BLI_addtail(&motherop->macro, opm);
				opm->opm = motherop; /* pointer to mom, for modal() */
			}
		}
		
		if (root)
			motherop = NULL;
	}
	
	WM_operator_properties_sanitize(op->ptr, 0);

	return op;
}

static void wm_region_mouse_co(bContext *C, wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	if (ar) {
		/* compatibility convention */
		event->mval[0] = event->x - ar->winrct.xmin;
		event->mval[1] = event->y - ar->winrct.ymin;
	}
	else {
		/* these values are invalid (avoid odd behavior by relying on old mval values) */
		event->mval[0] = -1;
		event->mval[1] = -1;
	}
}

#if 1 /* may want to disable operator remembering previous state for testing */
bool WM_operator_last_properties_init(wmOperator *op)
{
	bool changed = false;

	if (op->type->last_properties) {
		IDPropertyTemplate val = {0};
		IDProperty *replaceprops = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");
		PropertyRNA *iterprop;

		if (G.debug & G_DEBUG_WM) {
			printf("%s: loading previous properties for '%s'\n", __func__, op->type->idname);
		}

		iterprop = RNA_struct_iterator_property(op->type->srna);

		RNA_PROP_BEGIN (op->ptr, itemptr, iterprop)
		{
			PropertyRNA *prop = itemptr.data;
			if ((RNA_property_flag(prop) & PROP_SKIP_SAVE) == 0) {
				if (!RNA_property_is_set(op->ptr, prop)) { /* don't override a setting already set */
					const char *identifier = RNA_property_identifier(prop);
					IDProperty *idp_src = IDP_GetPropertyFromGroup(op->type->last_properties, identifier);
					if (idp_src) {
						IDProperty *idp_dst = IDP_CopyProperty(idp_src);

						/* note - in the future this may need to be done recursively,
						 * but for now RNA doesn't access nested operators */
						idp_dst->flag |= IDP_FLAG_GHOST;

						/* add to temporary group instead of immediate replace,
						 * because we are iterating over this group */
						IDP_AddToGroup(replaceprops, idp_dst);
						changed = true;
					}
				}
			}
		}
		RNA_PROP_END;

		IDP_MergeGroup(op->properties, replaceprops, true);
		IDP_FreeProperty(replaceprops);
		MEM_freeN(replaceprops);
	}

	return changed;
}

bool WM_operator_last_properties_store(wmOperator *op)
{
	if (op->type->last_properties) {
		IDP_FreeProperty(op->type->last_properties);
		MEM_freeN(op->type->last_properties);
		op->type->last_properties = NULL;
	}

	if (op->properties) {
		if (G.debug & G_DEBUG_WM) {
			printf("%s: storing properties for '%s'\n", __func__, op->type->idname);
		}
		op->type->last_properties = IDP_CopyProperty(op->properties);
		return true;
	}
	else {
		return false;
	}
}

#else

bool WM_operator_last_properties_init(wmOperator *UNUSED(op))
{
	return false;
}

bool WM_operator_last_properties_store(wmOperator *UNUSED(op))
{
	return false;
}

#endif

static int wm_operator_invoke(bContext *C, wmOperatorType *ot, wmEvent *event,
                              PointerRNA *properties, ReportList *reports, const bool poll_only)
{
	int retval = OPERATOR_PASS_THROUGH;

	/* this is done because complicated setup is done to call this function that is better not duplicated */
	if (poll_only)
		return WM_operator_poll(C, ot);

	if (WM_operator_poll(C, ot)) {
		wmWindowManager *wm = CTX_wm_manager(C);
		wmOperator *op = wm_operator_create(wm, ot, properties, reports); /* if reports == NULL, they'll be initialized */
		const bool is_nested_call = (wm->op_undo_depth != 0);
		
		op->flag |= OP_IS_INVOKE;

		/* initialize setting from previous run */
		if (!is_nested_call) { /* not called by py script */
			WM_operator_last_properties_init(op);
		}

		if ((G.debug & G_DEBUG_HANDLERS) && ((event == NULL) || (event->type != MOUSEMOVE))) {
			printf("%s: handle evt %d win %d op %s\n",
			       __func__, event ? event->type : 0, CTX_wm_screen(C)->subwinactive, ot->idname);
		}
		
		if (op->type->invoke && event) {
			wm_region_mouse_co(C, event);

			if (op->type->flag & OPTYPE_UNDO)
				wm->op_undo_depth++;

			retval = op->type->invoke(C, op, event);
			OPERATOR_RETVAL_CHECK(retval);

			if (op->type->flag & OPTYPE_UNDO && CTX_wm_manager(C) == wm)
				wm->op_undo_depth--;
		}
		else if (op->type->exec) {
			if (op->type->flag & OPTYPE_UNDO)
				wm->op_undo_depth++;

			retval = op->type->exec(C, op);
			OPERATOR_RETVAL_CHECK(retval);

			if (op->type->flag & OPTYPE_UNDO && CTX_wm_manager(C) == wm)
				wm->op_undo_depth--;
		}
		else {
			/* debug, important to leave a while, should never happen */
			printf("%s: invalid operator call '%s'\n", __func__, ot->idname);
		}
		
		/* Note, if the report is given as an argument then assume the caller will deal with displaying them
		 * currently python only uses this */
		if (!(retval & OPERATOR_HANDLED) && (retval & (OPERATOR_FINISHED | OPERATOR_CANCELLED))) {
			/* only show the report if the report list was not given in the function */
			wm_operator_reports(C, op, retval, (reports != NULL));
		}

		if (retval & OPERATOR_HANDLED) {
			/* do nothing, wm_operator_exec() has been called somewhere */
		}
		else if (retval & OPERATOR_FINISHED) {
			if (!is_nested_call) { /* not called by py script */
				WM_operator_last_properties_store(op);
			}
			wm_operator_finished(C, op, 0);
		}
		else if (retval & OPERATOR_RUNNING_MODAL) {
			/* take ownership of reports (in case python provided own) */
			op->reports->flag |= RPT_FREE;

			/* grab cursor during blocking modal ops (X11)
			 * Also check for macro
			 */
			if (ot->flag & OPTYPE_BLOCKING || (op->opm && op->opm->type->flag & OPTYPE_BLOCKING)) {
				int bounds[4] = {-1, -1, -1, -1};
				bool wrap;

				if (op->opm) {
					wrap = (U.uiflag & USER_CONTINUOUS_MOUSE) &&
					       ((op->opm->flag & OP_GRAB_POINTER) || (op->opm->type->flag & OPTYPE_GRAB_POINTER));
				}
				else {
					wrap = (U.uiflag & USER_CONTINUOUS_MOUSE) &&
					       ((op->flag & OP_GRAB_POINTER) || (ot->flag & OPTYPE_GRAB_POINTER));
				}

				/* exception, cont. grab in header is annoying */
				if (wrap) {
					ARegion *ar = CTX_wm_region(C);
					if (ar && ar->regiontype == RGN_TYPE_HEADER) {
						wrap = false;
					}
				}

				if (wrap) {
					rcti *winrect = NULL;
					ARegion *ar = CTX_wm_region(C);
					ScrArea *sa = CTX_wm_area(C);

					if (ar && ar->regiontype == RGN_TYPE_WINDOW && event &&
					    BLI_rcti_isect_pt_v(&ar->winrct, &event->x))
					{
						winrect = &ar->winrct;
					}
					else if (sa) {
						winrect = &sa->totrct;
					}

					if (winrect) {
						bounds[0] = winrect->xmin;
						bounds[1] = winrect->ymax;
						bounds[2] = winrect->xmax;
						bounds[3] = winrect->ymin;
					}
				}

				WM_cursor_grab_enable(CTX_wm_window(C), wrap, false, bounds);
			}

			/* cancel UI handlers, typically tooltips that can hang around
			 * while dragging the view or worse, that stay there permanently
			 * after the modal operator has swallowed all events and passed
			 * none to the UI handler */
			wm_handler_ui_cancel(C);
		}
		else {
			WM_operator_free(op);
		}
	}

	return retval;
}

/* WM_operator_name_call is the main accessor function
 * this is for python to access since its done the operator lookup
 * 
 * invokes operator in context */
static int wm_operator_call_internal(bContext *C, wmOperatorType *ot, PointerRNA *properties, ReportList *reports,
                                     const short context, const bool poll_only)
{
	wmEvent *event;
	
	int retval;

	CTX_wm_operator_poll_msg_set(C, NULL);

	/* dummie test */
	if (ot && C) {
		wmWindow *window = CTX_wm_window(C);

		switch (context) {
			case WM_OP_INVOKE_DEFAULT:
			case WM_OP_INVOKE_REGION_WIN:
			case WM_OP_INVOKE_AREA:
			case WM_OP_INVOKE_SCREEN:
				/* window is needed for invoke, cancel operator */
				if (window == NULL) {
					if (poll_only) {
						CTX_wm_operator_poll_msg_set(C, "Missing 'window' in context");
					}
					return 0;
				}
				else {
					event = window->eventstate;
				}
				break;
			default:
				event = NULL;
				break;
		}

		switch (context) {
			
			case WM_OP_EXEC_REGION_WIN:
			case WM_OP_INVOKE_REGION_WIN: 
			case WM_OP_EXEC_REGION_CHANNELS:
			case WM_OP_INVOKE_REGION_CHANNELS:
			case WM_OP_EXEC_REGION_PREVIEW:
			case WM_OP_INVOKE_REGION_PREVIEW:
			{
				/* forces operator to go to the region window/channels/preview, for header menus
				 * but we stay in the same region if we are already in one 
				 */
				ARegion *ar = CTX_wm_region(C);
				ScrArea *area = CTX_wm_area(C);
				int type = RGN_TYPE_WINDOW;
				
				switch (context) {
					case WM_OP_EXEC_REGION_CHANNELS:
					case WM_OP_INVOKE_REGION_CHANNELS:
						type = RGN_TYPE_CHANNELS;
						break;
					
					case WM_OP_EXEC_REGION_PREVIEW:
					case WM_OP_INVOKE_REGION_PREVIEW:
						type = RGN_TYPE_PREVIEW;
						break;
					
					case WM_OP_EXEC_REGION_WIN:
					case WM_OP_INVOKE_REGION_WIN: 
					default:
						type = RGN_TYPE_WINDOW;
						break;
				}
				
				if (!(ar && ar->regiontype == type) && area) {
					ARegion *ar1;
					if (type == RGN_TYPE_WINDOW) {
						ar1 = BKE_area_find_region_active_win(area);
					}
					else {
						ar1 = BKE_area_find_region_type(area, type);
					}

					if (ar1)
						CTX_wm_region_set(C, ar1);
				}
				
				retval = wm_operator_invoke(C, ot, event, properties, reports, poll_only);
				
				/* set region back */
				CTX_wm_region_set(C, ar);
				
				return retval;
			}
			case WM_OP_EXEC_AREA:
			case WM_OP_INVOKE_AREA:
			{
				/* remove region from context */
				ARegion *ar = CTX_wm_region(C);

				CTX_wm_region_set(C, NULL);
				retval = wm_operator_invoke(C, ot, event, properties, reports, poll_only);
				CTX_wm_region_set(C, ar);

				return retval;
			}
			case WM_OP_EXEC_SCREEN:
			case WM_OP_INVOKE_SCREEN:
			{
				/* remove region + area from context */
				ARegion *ar = CTX_wm_region(C);
				ScrArea *area = CTX_wm_area(C);

				CTX_wm_region_set(C, NULL);
				CTX_wm_area_set(C, NULL);
				retval = wm_operator_invoke(C, ot, event, properties, reports, poll_only);
				CTX_wm_area_set(C, area);
				CTX_wm_region_set(C, ar);

				return retval;
			}
			case WM_OP_EXEC_DEFAULT:
			case WM_OP_INVOKE_DEFAULT:
				return wm_operator_invoke(C, ot, event, properties, reports, poll_only);
		}
	}
	
	return 0;
}


/* invokes operator in context */
int WM_operator_name_call_ptr(bContext *C, wmOperatorType *ot, short context, PointerRNA *properties)
{
	BLI_assert(ot == WM_operatortype_find(ot->idname, true));
	return wm_operator_call_internal(C, ot, properties, NULL, context, false);
}
int WM_operator_name_call(bContext *C, const char *opstring, short context, PointerRNA *properties)
{
	wmOperatorType *ot = WM_operatortype_find(opstring, 0);
	if (ot) {
		return WM_operator_name_call_ptr(C, ot, context, properties);
	}

	return 0;
}

/* Similar to WM_operator_name_call called with WM_OP_EXEC_DEFAULT context.
 * - wmOperatorType is used instead of operator name since python already has the operator type
 * - poll() must be called by python before this runs.
 * - reports can be passed to this function (so python can report them as exceptions)
 */
int WM_operator_call_py(bContext *C, wmOperatorType *ot, short context,
                        PointerRNA *properties, ReportList *reports, const bool is_undo)
{
	int retval = OPERATOR_CANCELLED;

#if 0
	wmOperator *op;
	op = wm_operator_create(wm, ot, properties, reports);

	if (op->type->exec) {
		if (is_undo && op->type->flag & OPTYPE_UNDO)
			wm->op_undo_depth++;

		retval = op->type->exec(C, op);
		OPERATOR_RETVAL_CHECK(retval);

		if (is_undo && op->type->flag & OPTYPE_UNDO && CTX_wm_manager(C) == wm)
			wm->op_undo_depth--;
	}
	else
		printf("error \"%s\" operator has no exec function, python cannot call it\n", op->type->name);
#endif

	/* not especially nice using undo depth here, its used so py never
	 * triggers undo or stores operators last used state.
	 *
	 * we could have some more obvious way of doing this like passing a flag.
	 */
	wmWindowManager *wm = CTX_wm_manager(C);
	if (!is_undo && wm) wm->op_undo_depth++;

	retval = wm_operator_call_internal(C, ot, properties, reports, context, false);
	
	if (!is_undo && wm && (wm == CTX_wm_manager(C))) wm->op_undo_depth--;

	return retval;
}


/* ********************* handlers *************** */

/* future extra customadata free? */
void wm_event_free_handler(wmEventHandler *handler)
{
	MEM_freeN(handler);
}

/* only set context when area/region is part of screen */
static void wm_handler_op_context(bContext *C, wmEventHandler *handler)
{
	bScreen *screen = CTX_wm_screen(C);
	
	if (screen && handler->op) {
		if (handler->op_area == NULL)
			CTX_wm_area_set(C, NULL);
		else {
			ScrArea *sa;
			
			for (sa = screen->areabase.first; sa; sa = sa->next)
				if (sa == handler->op_area)
					break;
			if (sa == NULL) {
				/* when changing screen layouts with running modal handlers (like render display), this
				 * is not an error to print */
				if (handler->op == NULL)
					printf("internal error: handler (%s) has invalid area\n", handler->op->type->idname);
			}
			else {
				ARegion *ar;
				CTX_wm_area_set(C, sa);
				for (ar = sa->regionbase.first; ar; ar = ar->next)
					if (ar == handler->op_region)
						break;
				/* XXX no warning print here, after full-area and back regions are remade */
				if (ar)
					CTX_wm_region_set(C, ar);
			}
		}
	}
}

/* called on exit or remove area, only here call cancel callback */
void WM_event_remove_handlers(bContext *C, ListBase *handlers)
{
	wmEventHandler *handler;
	wmWindowManager *wm = CTX_wm_manager(C);
	
	/* C is zero on freeing database, modal handlers then already were freed */
	while ((handler = BLI_pophead(handlers))) {
		if (handler->op) {
			if (handler->op->type->cancel) {
				ScrArea *area = CTX_wm_area(C);
				ARegion *region = CTX_wm_region(C);
				
				wm_handler_op_context(C, handler);

				if (handler->op->type->flag & OPTYPE_UNDO)
					wm->op_undo_depth++;

				handler->op->type->cancel(C, handler->op);

				if (handler->op->type->flag & OPTYPE_UNDO)
					wm->op_undo_depth--;

				CTX_wm_area_set(C, area);
				CTX_wm_region_set(C, region);
			}

			WM_cursor_grab_disable(CTX_wm_window(C), NULL);
			WM_operator_free(handler->op);
		}
		else if (handler->ui_remove) {
			ScrArea *area = CTX_wm_area(C);
			ARegion *region = CTX_wm_region(C);
			ARegion *menu = CTX_wm_menu(C);
			
			if (handler->ui_area) CTX_wm_area_set(C, handler->ui_area);
			if (handler->ui_region) CTX_wm_region_set(C, handler->ui_region);
			if (handler->ui_menu) CTX_wm_menu_set(C, handler->ui_menu);

			handler->ui_remove(C, handler->ui_userdata);

			CTX_wm_area_set(C, area);
			CTX_wm_region_set(C, region);
			CTX_wm_menu_set(C, menu);
		}

		wm_event_free_handler(handler);
	}
}

/* do userdef mappings */
int WM_userdef_event_map(int kmitype)
{
	switch (kmitype) {
		case SELECTMOUSE:
			return (U.flag & USER_LMOUSESELECT) ? LEFTMOUSE : RIGHTMOUSE;
		case ACTIONMOUSE:
			return (U.flag & USER_LMOUSESELECT) ? RIGHTMOUSE : LEFTMOUSE;
		case EVT_TWEAK_A:
			return (U.flag & USER_LMOUSESELECT) ? EVT_TWEAK_R : EVT_TWEAK_L;
		case EVT_TWEAK_S:
			return (U.flag & USER_LMOUSESELECT) ? EVT_TWEAK_L : EVT_TWEAK_R;
		case WHEELOUTMOUSE:
			return (U.uiflag & USER_WHEELZOOMDIR) ? WHEELUPMOUSE : WHEELDOWNMOUSE;
		case WHEELINMOUSE:
			return (U.uiflag & USER_WHEELZOOMDIR) ? WHEELDOWNMOUSE : WHEELUPMOUSE;
	}
	
	return kmitype;
}


static int wm_eventmatch(wmEvent *winevent, wmKeyMapItem *kmi)
{
	int kmitype = WM_userdef_event_map(kmi->type);

	if (kmi->flag & KMI_INACTIVE) return 0;

	/* the matching rules */
	if (kmitype == KM_TEXTINPUT)
		if (winevent->val == KM_PRESS) {  /* prevent double clicks */
			/* NOT using ISTEXTINPUT anymore because (at least on Windows) some key codes above 255
			 * could have printable ascii keys - BUG [#30479] */
			if (ISKEYBOARD(winevent->type) && (winevent->ascii || winevent->utf8_buf[0])) return 1; 
		}

	if (kmitype != KM_ANY)
		if (winevent->type != kmitype) return 0;
	
	if (kmi->val != KM_ANY)
		if (winevent->val != kmi->val) return 0;
	
	/* modifiers also check bits, so it allows modifier order */
	if (kmi->shift != KM_ANY)
		if (winevent->shift != kmi->shift && !(winevent->shift & kmi->shift)) return 0;
	if (kmi->ctrl != KM_ANY)
		if (winevent->ctrl != kmi->ctrl && !(winevent->ctrl & kmi->ctrl)) return 0;
	if (kmi->alt != KM_ANY)
		if (winevent->alt != kmi->alt && !(winevent->alt & kmi->alt)) return 0;
	if (kmi->oskey != KM_ANY)
		if (winevent->oskey != kmi->oskey && !(winevent->oskey & kmi->oskey)) return 0;
	
	/* only keymap entry with keymodifier is checked, means all keys without modifier get handled too. */
	/* that is currently needed to make overlapping events work (when you press A - G fast or so). */
	if (kmi->keymodifier)
		if (winevent->keymodifier != kmi->keymodifier) return 0;
	
	return 1;
}


/* operator exists */
static void wm_event_modalkeymap(const bContext *C, wmOperator *op, wmEvent *event, bool *dbl_click_disabled)
{
	/* support for modal keymap in macros */
	if (op->opm)
		op = op->opm;

	if (op->type->modalkeymap) {
		wmKeyMap *keymap = WM_keymap_active(CTX_wm_manager(C), op->type->modalkeymap);
		wmKeyMapItem *kmi;

		for (kmi = keymap->items.first; kmi; kmi = kmi->next) {
			if (wm_eventmatch(event, kmi)) {
					
				event->prevtype = event->type;
				event->prevval = event->val;
				event->type = EVT_MODAL_MAP;
				event->val = kmi->propvalue;
				
				break;
			}
		}
	}
	else {
		/* modal keymap checking returns handled events fine, but all hardcoded modal
		 * handling typically swallows all events (OPERATOR_RUNNING_MODAL).
		 * This bypass just disables support for double clicks in hardcoded modal handlers */
		if (event->val == KM_DBL_CLICK) {
			event->val = KM_PRESS;
			*dbl_click_disabled = true;
		}
	}
}

/* Check whether operator is allowed to run in case interface is locked,
 * If interface is unlocked, will always return truth.
 */
static bool wm_operator_check_locked_interface(bContext *C, wmOperatorType *ot)
{
	wmWindowManager *wm = CTX_wm_manager(C);

	if (wm->is_interface_locked) {
		if ((ot->flag & OPTYPE_LOCK_BYPASS) == 0) {
			return false;
		}
	}

	return true;
}

/* bad hacking event system... better restore event type for checking of KM_CLICK for example */
/* XXX modal maps could use different method (ton) */
static void wm_event_modalmap_end(wmEvent *event, bool dbl_click_disabled)
{
	if (event->type == EVT_MODAL_MAP) {
		event->type = event->prevtype;
		event->prevtype = 0;
		event->val = event->prevval;
		event->prevval = 0;
	}
	else if (dbl_click_disabled)
		event->val = KM_DBL_CLICK;

}

/* Warning: this function removes a modal handler, when finished */
static int wm_handler_operator_call(bContext *C, ListBase *handlers, wmEventHandler *handler,
                                    wmEvent *event, PointerRNA *properties)
{
	int retval = OPERATOR_PASS_THROUGH;
	
	/* derived, modal or blocking operator */
	if (handler->op) {
		wmOperator *op = handler->op;
		wmOperatorType *ot = op->type;

		if (!wm_operator_check_locked_interface(C, ot)) {
			/* Interface is locked and pperator is not allowed to run,
			 * nothing to do in this case.
			 */
		}
		else if (ot->modal) {
			/* we set context to where modal handler came from */
			wmWindowManager *wm = CTX_wm_manager(C);
			ScrArea *area = CTX_wm_area(C);
			ARegion *region = CTX_wm_region(C);
			bool dbl_click_disabled = false;

			wm_handler_op_context(C, handler);
			wm_region_mouse_co(C, event);
			wm_event_modalkeymap(C, op, event, &dbl_click_disabled);
			
			if (ot->flag & OPTYPE_UNDO)
				wm->op_undo_depth++;

			/* warning, after this call all context data and 'event' may be freed. see check below */
			retval = ot->modal(C, op, event);
			OPERATOR_RETVAL_CHECK(retval);
			
			/* when this is _not_ the case the modal modifier may have loaded
			 * a new blend file (demo mode does this), so we have to assume
			 * the event, operator etc have all been freed. - campbell */
			if (CTX_wm_manager(C) == wm) {

				wm_event_modalmap_end(event, dbl_click_disabled);

				if (ot->flag & OPTYPE_UNDO)
					wm->op_undo_depth--;

				if (retval & (OPERATOR_CANCELLED | OPERATOR_FINISHED))
					wm_operator_reports(C, op, retval, false);

				/* important to run 'wm_operator_finished' before NULLing the context members */
				if (retval & OPERATOR_FINISHED) {
					wm_operator_finished(C, op, 0);
					handler->op = NULL;
				}
				else if (retval & (OPERATOR_CANCELLED | OPERATOR_FINISHED)) {
					WM_operator_free(op);
					handler->op = NULL;
				}

				/* putting back screen context, reval can pass trough after modal failures! */
				if ((retval & OPERATOR_PASS_THROUGH) || wm_event_always_pass(event)) {
					CTX_wm_area_set(C, area);
					CTX_wm_region_set(C, region);
				}
				else {
					/* this special cases is for areas and regions that get removed */
					CTX_wm_area_set(C, NULL);
					CTX_wm_region_set(C, NULL);
				}

				/* remove modal handler, operator itself should have been canceled and freed */
				if (retval & (OPERATOR_CANCELLED | OPERATOR_FINISHED)) {
					WM_cursor_grab_disable(CTX_wm_window(C), NULL);

					BLI_remlink(handlers, handler);
					wm_event_free_handler(handler);

					/* prevent silly errors from operator users */
					//retval &= ~OPERATOR_PASS_THROUGH;
				}
			}
			
		}
		else {
			printf("%s: error '%s' missing modal\n", __func__, op->idname);
		}
	}
	else {
		wmOperatorType *ot = WM_operatortype_find(event->keymap_idname, 0);

		if (ot) {
			if (wm_operator_check_locked_interface(C, ot)) {
				retval = wm_operator_invoke(C, ot, event, properties, NULL, false);
			}
		}
	}
	/* Finished and pass through flag as handled */

	/* Finished and pass through flag as handled */
	if (retval == (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH))
		return WM_HANDLER_HANDLED;

	/* Modal unhandled, break */
	if (retval == (OPERATOR_PASS_THROUGH | OPERATOR_RUNNING_MODAL))
		return (WM_HANDLER_BREAK | WM_HANDLER_MODAL);

	if (retval & OPERATOR_PASS_THROUGH)
		return WM_HANDLER_CONTINUE;

	return WM_HANDLER_BREAK;
}

/* fileselect handlers are only in the window queue, so it's safe to switch screens or area types */
static int wm_handler_fileselect_do(bContext *C, ListBase *handlers, wmEventHandler *handler, int val)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	SpaceFile *sfile;
	int action = WM_HANDLER_CONTINUE;

	switch (val) {
		case EVT_FILESELECT_OPEN: 
		case EVT_FILESELECT_FULL_OPEN: 
		{
			ScrArea *sa;
				
			/* sa can be null when window A is active, but mouse is over window B */
			/* in this case, open file select in original window A */
			if (handler->op_area == NULL) {
				bScreen *screen = CTX_wm_screen(C);
				sa = (ScrArea *)screen->areabase.first;
			}
			else {
				sa = handler->op_area;
			}
					
			if (val == EVT_FILESELECT_OPEN) {
				ED_area_newspace(C, sa, SPACE_FILE);     /* 'sa' is modified in-place */
			}
			else {
				sa = ED_screen_full_newspace(C, sa, SPACE_FILE);    /* sets context */
			}

			/* note, getting the 'sa' back from the context causes a nasty bug where the newly created
			 * 'sa' != CTX_wm_area(C). removed the line below and set 'sa' in the 'if' above */
			/* sa = CTX_wm_area(C); */

			/* settings for filebrowser, sfile is not operator owner but sends events */
			sfile = (SpaceFile *)sa->spacedata.first;
			sfile->op = handler->op;

			ED_fileselect_set_params(sfile);
				
			action = WM_HANDLER_BREAK;
			break;
		}
			
		case EVT_FILESELECT_EXEC:
		case EVT_FILESELECT_CANCEL:
		case EVT_FILESELECT_EXTERNAL_CANCEL:
		{
			/* XXX validate area and region? */
			bScreen *screen = CTX_wm_screen(C);

			/* remlink now, for load file case before removing*/
			BLI_remlink(handlers, handler);
				
			if (val != EVT_FILESELECT_EXTERNAL_CANCEL) {
				if (screen != handler->filescreen) {
					ED_screen_full_prevspace(C, CTX_wm_area(C));
				}
				else {
					ED_area_prevspace(C, CTX_wm_area(C));
				}
			}
				
			wm_handler_op_context(C, handler);

			/* needed for uiPupMenuReports */

			if (val == EVT_FILESELECT_EXEC) {
				int retval;

				if (handler->op->type->flag & OPTYPE_UNDO)
					wm->op_undo_depth++;

				retval = handler->op->type->exec(C, handler->op);

				/* XXX check this carefully, CTX_wm_manager(C) == wm is a bit hackish */
				if (handler->op->type->flag & OPTYPE_UNDO && CTX_wm_manager(C) == wm)
					wm->op_undo_depth--;

				/* XXX check this carefully, CTX_wm_manager(C) == wm is a bit hackish */
				if (CTX_wm_manager(C) == wm && wm->op_undo_depth == 0)
					if (handler->op->type->flag & OPTYPE_UNDO)
						ED_undo_push_op(C, handler->op);

				if (handler->op->reports->list.first) {

					/* FIXME, temp setting window, this is really bad!
					 * only have because lib linking errors need to be seen by users :(
					 * it can be removed without breaking anything but then no linking errors - campbell */
					wmWindow *win_prev = CTX_wm_window(C);
					ScrArea *area_prev = CTX_wm_area(C);
					ARegion *ar_prev = CTX_wm_region(C);

					if (win_prev == NULL)
						CTX_wm_window_set(C, CTX_wm_manager(C)->windows.first);

					BKE_report_print_level_set(handler->op->reports, RPT_WARNING);
					uiPupMenuReports(C, handler->op->reports);

					/* XXX - copied from 'wm_operator_finished()' */
					/* add reports to the global list, otherwise they are not seen */
					BLI_movelisttolist(&CTX_wm_reports(C)->list, &handler->op->reports->list);

					CTX_wm_window_set(C, win_prev);
					CTX_wm_area_set(C, area_prev);
					CTX_wm_region_set(C, ar_prev);
				}

				/* for WM_operator_pystring only, custom report handling is done above */
				wm_operator_reports(C, handler->op, retval, true);

				if (retval & OPERATOR_FINISHED) {
					WM_operator_last_properties_store(handler->op);
				}

				WM_operator_free(handler->op);
			}
			else {
				if (handler->op->type->cancel) {
					if (handler->op->type->flag & OPTYPE_UNDO)
						wm->op_undo_depth++;

					handler->op->type->cancel(C, handler->op);
				
					if (handler->op->type->flag & OPTYPE_UNDO)
						wm->op_undo_depth--;
				}
				
				WM_operator_free(handler->op);
			}

			CTX_wm_area_set(C, NULL);

			wm_event_free_handler(handler);

			action = WM_HANDLER_BREAK;
			break;
		}
	}
	
	return action;
}

static int wm_handler_fileselect_call(bContext *C, ListBase *handlers, wmEventHandler *handler, wmEvent *event)
{
	int action = WM_HANDLER_CONTINUE;
	
	if (event->type != EVT_FILESELECT)
		return action;
	if (handler->op != (wmOperator *)event->customdata)
		return action;
	
	return wm_handler_fileselect_do(C, handlers, handler, event->val);
}

static bool handler_boundbox_test(wmEventHandler *handler, wmEvent *event)
{
	if (handler->bbwin) {
		if (handler->bblocal) {
			rcti rect = *handler->bblocal;
			BLI_rcti_translate(&rect, handler->bbwin->xmin, handler->bbwin->ymin);

			if (BLI_rcti_isect_pt_v(&rect, &event->x))
				return 1;
			else if (event->type == MOUSEMOVE && BLI_rcti_isect_pt_v(&rect, &event->prevx))
				return 1;
			else
				return 0;
		}
		else {
			if (BLI_rcti_isect_pt_v(handler->bbwin, &event->x))
				return 1;
			else if (event->type == MOUSEMOVE && BLI_rcti_isect_pt_v(handler->bbwin, &event->prevx))
				return 1;
			else
				return 0;
		}
	}
	return 1;
}

static int wm_action_not_handled(int action)
{
	return action == WM_HANDLER_CONTINUE || action == (WM_HANDLER_BREAK | WM_HANDLER_MODAL);
}

static int wm_handlers_do_intern(bContext *C, wmEvent *event, ListBase *handlers)
{
#ifndef NDEBUG
	const int do_debug_handler = (G.debug & G_DEBUG_HANDLERS) &&
	        /* comment this out to flood the console! (if you really want to test) */
	        !ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)
	        ;
#    define PRINT if (do_debug_handler) printf
#else
#  define PRINT(format, ...)
#endif

	wmWindowManager *wm = CTX_wm_manager(C);
	wmEventHandler *handler, *nexthandler;
	int action = WM_HANDLER_CONTINUE;
	int always_pass;

	if (handlers == NULL) {
		return action;
	}

	/* modal handlers can get removed in this loop, we keep the loop this way
	 *
	 * note: check 'handlers->first' because in rare cases the handlers can be cleared
	 * by the event thats called, for eg:
	 *
	 * Calling a python script which changes the area.type, see [#32232] */
	for (handler = handlers->first; handler && handlers->first; handler = nexthandler) {

		nexthandler = handler->next;
		
		/* during this loop, ui handlers for nested menus can tag multiple handlers free */
		if (handler->flag & WM_HANDLER_DO_FREE) {
			/* pass */
		}
		else if (handler_boundbox_test(handler, event)) { /* optional boundbox */
			/* in advance to avoid access to freed event on window close */
			always_pass = wm_event_always_pass(event);
		
			/* modal+blocking handler */
			if (handler->flag & WM_HANDLER_BLOCKING)
				action |= WM_HANDLER_BREAK;

			if (handler->keymap) {
				wmKeyMap *keymap = WM_keymap_active(wm, handler->keymap);
				wmKeyMapItem *kmi;

				PRINT("%s:   checking '%s' ...", __func__, keymap->idname);

				if (!keymap->poll || keymap->poll(C)) {

					PRINT("pass\n");

					for (kmi = keymap->items.first; kmi; kmi = kmi->next) {
						if (wm_eventmatch(event, kmi)) {

							PRINT("%s:     item matched '%s'\n", __func__, kmi->idname);

							/* weak, but allows interactive callback to not use rawkey */
							event->keymap_idname = kmi->idname;

							action |= wm_handler_operator_call(C, handlers, handler, event, kmi->ptr);
							if (action & WM_HANDLER_BREAK) {
								/* not always_pass here, it denotes removed handler */
								
								if (G.debug & (G_DEBUG_EVENTS | G_DEBUG_HANDLERS))
									printf("%s:       handled! '%s'\n", __func__, kmi->idname);

								break;
							}
							else {
								if (action & WM_HANDLER_HANDLED)
									if (G.debug & (G_DEBUG_EVENTS | G_DEBUG_HANDLERS))
										printf("%s:       handled - and pass on! '%s'\n", __func__, kmi->idname);
								
									PRINT("%s:       un-handled '%s'...", __func__, kmi->idname);
							}
						}
					}
				}
				else {
					PRINT("fail\n");
				}
			}
			else if (handler->ui_handle) {
				if (!wm->is_interface_locked) {
					action |= wm_handler_ui_call(C, handler, event, always_pass);
				}
			}
			else if (handler->type == WM_HANDLER_FILESELECT) {
				if (!wm->is_interface_locked) {
					/* screen context changes here */
					action |= wm_handler_fileselect_call(C, handlers, handler, event);
				}
			}
			else if (handler->dropboxes) {
				if (!wm->is_interface_locked && event->type == EVT_DROP) {
					wmDropBox *drop = handler->dropboxes->first;
					for (; drop; drop = drop->next) {
						/* other drop custom types allowed */
						if (event->custom == EVT_DATA_LISTBASE) {
							ListBase *lb = (ListBase *)event->customdata;
							wmDrag *drag;
							
							for (drag = lb->first; drag; drag = drag->next) {
								if (drop->poll(C, drag, event)) {
									
									drop->copy(drag, drop);
									
									/* free the drags before calling operator */
									BLI_freelistN(event->customdata);
									event->customdata = NULL;
									event->custom = 0;
									
									WM_operator_name_call(C, drop->ot->idname, drop->opcontext, drop->ptr);
									action |= WM_HANDLER_BREAK;
									
									/* XXX fileread case */
									if (CTX_wm_window(C) == NULL)
										return action;
									
									/* escape from drag loop, got freed */
									break;
								}
							}
						}
					}
				}
			}
			else {
				/* modal, swallows all */
				action |= wm_handler_operator_call(C, handlers, handler, event, NULL);
			}

			if (action & WM_HANDLER_BREAK) {
				if (always_pass)
					action &= ~WM_HANDLER_BREAK;
				else
					break;
			}
		}
		
		/* XXX fileread case, if the wm is freed then the handler's
		 * will have been too so the code below need not run. */
		if (CTX_wm_window(C) == NULL) {
			return action;
		}

		/* XXX code this for all modal ops, and ensure free only happens here */
		
		/* modal ui handler can be tagged to be freed */ 
		if (BLI_findindex(handlers, handler) != -1) { /* could be freed already by regular modal ops */
			if (handler->flag & WM_HANDLER_DO_FREE) {
				BLI_remlink(handlers, handler);
				wm_event_free_handler(handler);
			}
		}
	}

	if (action == (WM_HANDLER_BREAK | WM_HANDLER_MODAL))
		wm_cursor_arrow_move(CTX_wm_window(C), event);

#undef PRINT

	return action;
}

/* this calls handlers twice - to solve (double-)click events */
static int wm_handlers_do(bContext *C, wmEvent *event, ListBase *handlers)
{
	int action = wm_handlers_do_intern(C, event, handlers);
		
	/* fileread case */
	if (CTX_wm_window(C) == NULL)
		return action;

	if (!ELEM3(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE, EVENT_NONE) && !ISTIMER(event->type)) {

		/* test for CLICK events */
		if (wm_action_not_handled(action)) {
			wmWindow *win = CTX_wm_window(C);
			
			/* eventstate stores if previous event was a KM_PRESS, in case that 
			 * wasn't handled, the KM_RELEASE will become a KM_CLICK */
			
			if (win && event->val == KM_PRESS) {
				win->eventstate->check_click = true;
			}
			
			if (win && win->eventstate->prevtype == event->type) {
				
				if ((event->val == KM_RELEASE) &&
				    (win->eventstate->prevval == KM_PRESS) &&
				    (win->eventstate->check_click == true))
				{
					event->val = KM_CLICK;
					
					if (G.debug & (G_DEBUG_HANDLERS)) {
						printf("%s: handling CLICK\n", __func__);
					}

					action |= wm_handlers_do_intern(C, event, handlers);

					event->val = KM_RELEASE;
				}
				else if (event->val == KM_DBL_CLICK) {
					event->val = KM_PRESS;
					action |= wm_handlers_do_intern(C, event, handlers);
					
					/* revert value if not handled */
					if (wm_action_not_handled(action)) {
						event->val = KM_DBL_CLICK;
					}
				}
			}
		}
		else {
			wmWindow *win = CTX_wm_window(C);

			if (win)
				win->eventstate->check_click = 0;
		}
	}
	
	return action;
}

static int wm_event_inside_i(wmEvent *event, rcti *rect)
{
	if (wm_event_always_pass(event))
		return 1;
	if (BLI_rcti_isect_pt_v(rect, &event->x))
		return 1;
	if (event->type == MOUSEMOVE) {
		if (BLI_rcti_isect_pt_v(rect, &event->prevx)) {
			return 1;
		}
		return 0;
	}
	return 0;
}

static ScrArea *area_event_inside(bContext *C, const int xy[2])
{
	bScreen *screen = CTX_wm_screen(C);
	ScrArea *sa;
	
	if (screen)
		for (sa = screen->areabase.first; sa; sa = sa->next)
			if (BLI_rcti_isect_pt_v(&sa->totrct, xy))
				return sa;
	return NULL;
}

static ARegion *region_event_inside(bContext *C, const int xy[2])
{
	bScreen *screen = CTX_wm_screen(C);
	ScrArea *area = CTX_wm_area(C);
	ARegion *ar;
	
	if (screen && area)
		for (ar = area->regionbase.first; ar; ar = ar->next)
			if (BLI_rcti_isect_pt_v(&ar->winrct, xy))
				return ar;
	return NULL;
}

static void wm_paintcursor_tag(bContext *C, wmPaintCursor *pc, ARegion *ar)
{
	if (ar) {
		for (; pc; pc = pc->next) {
			if (pc->poll == NULL || pc->poll(C)) {
				wmWindow *win = CTX_wm_window(C);
				WM_paint_cursor_tag_redraw(win, ar);
			}
		}
	}
}

/* called on mousemove, check updates for paintcursors */
/* context was set on active area and region */
static void wm_paintcursor_test(bContext *C, wmEvent *event)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	
	if (wm->paintcursors.first) {
		ARegion *ar = CTX_wm_region(C);
		
		if (ar)
			wm_paintcursor_tag(C, wm->paintcursors.first, ar);
		
		/* if previous position was not in current region, we have to set a temp new context */
		if (ar == NULL || !BLI_rcti_isect_pt_v(&ar->winrct, &event->prevx)) {
			ScrArea *sa = CTX_wm_area(C);
			
			CTX_wm_area_set(C, area_event_inside(C, &event->prevx));
			CTX_wm_region_set(C, region_event_inside(C, &event->prevx));

			wm_paintcursor_tag(C, wm->paintcursors.first, CTX_wm_region(C));
			
			CTX_wm_area_set(C, sa);
			CTX_wm_region_set(C, ar);
		}
	}
}

static void wm_event_drag_test(wmWindowManager *wm, wmWindow *win, wmEvent *event)
{
	if (BLI_listbase_is_empty(&wm->drags)) {
		return;
	}
	
	if (event->type == MOUSEMOVE || ISKEYMODIFIER(event->type))
		win->screen->do_draw_drag = true;
	else if (event->type == ESCKEY) {
		BLI_freelistN(&wm->drags);
		win->screen->do_draw_drag = true;
	}
	else if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
		event->type = EVT_DROP;
		
		/* create customdata, first free existing */
		if (event->customdata) {
			if (event->customdatafree)
				MEM_freeN(event->customdata);
		}
		
		event->custom = EVT_DATA_LISTBASE;
		event->customdata = &wm->drags;
		event->customdatafree = 1;
		
		/* clear drop icon */
		win->screen->do_draw_drag = true;
		
		/* restore cursor (disabled, see wm_dragdrop.c) */
		// WM_cursor_modal_restore(win);
	}
	
	/* overlap fails otherwise */
	if (win->screen->do_draw_drag)
		if (win->drawmethod == USER_DRAW_OVERLAP)
			win->screen->do_draw = true;
	
}

/* called in main loop */
/* goes over entire hierarchy:  events -> window -> screen -> area -> region */
void wm_event_do_handlers(bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win;

	/* update key configuration before handling events */
	WM_keyconfig_update(wm);

	for (win = wm->windows.first; win; win = win->next) {
		wmEvent *event;
		
		if (win->screen == NULL)
			wm_event_free_all(win);
		else {
			Scene *scene = win->screen->scene;
			
			if (scene) {
				int is_playing_sound = sound_scene_playing(win->screen->scene);
				
				if (is_playing_sound != -1) {
					bool is_playing_screen;
					CTX_wm_window_set(C, win);
					CTX_wm_screen_set(C, win->screen);
					CTX_data_scene_set(C, scene);
					
					is_playing_screen = (ED_screen_animation_playing(wm) != NULL);

					if (((is_playing_sound == 1) && (is_playing_screen == 0)) ||
					    ((is_playing_sound == 0) && (is_playing_screen == 1)))
					{
						ED_screen_animation_play(C, -1, 1);
					}
					
					if (is_playing_sound == 0) {
						const float time = sound_sync_scene(scene);
						if (finite(time)) {
							int ncfra = time * (float)FPS + 0.5f;
							if (ncfra != scene->r.cfra) {
								scene->r.cfra = ncfra;
								ED_update_for_newframe(CTX_data_main(C), scene, 1);
								WM_event_add_notifier(C, NC_WINDOW, NULL);
							}
						}
					}
					
					CTX_data_scene_set(C, NULL);
					CTX_wm_screen_set(C, NULL);
					CTX_wm_window_set(C, NULL);
				}
			}
		}
		
		while ( (event = win->queue.first) ) {
			int action = WM_HANDLER_CONTINUE;

#ifndef NDEBUG
			if (G.debug & (G_DEBUG_HANDLERS | G_DEBUG_EVENTS) && !ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
				printf("\n%s: Handling event\n", __func__);
				WM_event_print(event);
			}
#endif
			
			CTX_wm_window_set(C, win);
			
			/* we let modal handlers get active area/region, also wm_paintcursor_test needs it */
			CTX_wm_area_set(C, area_event_inside(C, &event->x));
			CTX_wm_region_set(C, region_event_inside(C, &event->x));
			
			/* MVC demands to not draw in event handlers... but we need to leave it for ogl selecting etc */
			wm_window_make_drawable(wm, win);
			
			wm_region_mouse_co(C, event);

			/* first we do priority handlers, modal + some limited keymaps */
			action |= wm_handlers_do(C, event, &win->modalhandlers);
			
			/* fileread case */
			if (CTX_wm_window(C) == NULL)
				return;
			
			/* check dragging, creates new event or frees, adds draw tag */
			wm_event_drag_test(wm, win, event);
			
			/* builtin tweak, if action is break it removes tweak */
			wm_tweakevent_test(C, event, action);

			if ((action & WM_HANDLER_BREAK) == 0) {
				ScrArea *sa;
				ARegion *ar;
				int doit = 0;
	
				/* Note: setting subwin active should be done here, after modal handlers have been done */
				if (event->type == MOUSEMOVE) {
					/* state variables in screen, cursors. Also used in wm_draw.c, fails for modal handlers though */
					ED_screen_set_subwinactive(C, event);
					/* for regions having custom cursors */
					wm_paintcursor_test(C, event);
				}
				else if (event->type == NDOF_MOTION) {
					win->addmousemove = true;
				}

				for (sa = win->screen->areabase.first; sa; sa = sa->next) {
					if (wm_event_inside_i(event, &sa->totrct)) {
						CTX_wm_area_set(C, sa);

						if ((action & WM_HANDLER_BREAK) == 0) {
							for (ar = sa->regionbase.first; ar; ar = ar->next) {
								if (wm_event_inside_i(event, &ar->winrct)) {
									CTX_wm_region_set(C, ar);
									
									/* call even on non mouse events, since the */
									wm_region_mouse_co(C, event);

									if (!BLI_listbase_is_empty(&wm->drags)) {
										/* does polls for drop regions and checks uibuts */
										/* need to be here to make sure region context is true */
										if (ELEM(event->type, MOUSEMOVE, EVT_DROP) || ISKEYMODIFIER(event->type)) {
											wm_drags_check_ops(C, event);
										}
									}
									
									action |= wm_handlers_do(C, event, &ar->handlers);

									/* fileread case (python), [#29489] */
									if (CTX_wm_window(C) == NULL)
										return;

									doit |= (BLI_rcti_isect_pt_v(&ar->winrct, &event->x));
									
									if (action & WM_HANDLER_BREAK)
										break;
								}
							}
						}

						CTX_wm_region_set(C, NULL);

						if ((action & WM_HANDLER_BREAK) == 0) {
							wm_region_mouse_co(C, event); /* only invalidates event->mval in this case */
							action |= wm_handlers_do(C, event, &sa->handlers);
						}
						CTX_wm_area_set(C, NULL);

						/* NOTE: do not escape on WM_HANDLER_BREAK, mousemove needs handled for previous area */
					}
				}
				
				if ((action & WM_HANDLER_BREAK) == 0) {
					/* also some non-modal handlers need active area/region */
					CTX_wm_area_set(C, area_event_inside(C, &event->x));
					CTX_wm_region_set(C, region_event_inside(C, &event->x));

					wm_region_mouse_co(C, event);

					action |= wm_handlers_do(C, event, &win->handlers);

					/* fileread case */
					if (CTX_wm_window(C) == NULL)
						return;
				}

				/* XXX hrmf, this gives reliable previous mouse coord for area change, feels bad? 
				 * doing it on ghost queue gives errors when mousemoves go over area borders */
				if (doit && win->screen && win->screen->subwinactive != win->screen->mainwin) {
					win->eventstate->prevx = event->x;
					win->eventstate->prevy = event->y;
					//printf("win->eventstate->prev = %d %d\n", event->x, event->y);
				}
				else {
					//printf("not setting prev to %d %d\n", event->x, event->y);
				}
			}
			
			/* unlink and free here, blender-quit then frees all */
			BLI_remlink(&win->queue, event);
			wm_event_free(event);
			
		}
		
		/* only add mousemove when queue was read entirely */
		if (win->addmousemove && win->eventstate) {
			wmEvent tevent = *(win->eventstate);
			// printf("adding MOUSEMOVE %d %d\n", tevent.x, tevent.y);
			tevent.type = MOUSEMOVE;
			tevent.prevx = tevent.x;
			tevent.prevy = tevent.y;
			wm_event_add(win, &tevent);
			win->addmousemove = 0;
		}
		
		CTX_wm_window_set(C, NULL);
	}

	/* update key configuration after handling events */
	WM_keyconfig_update(wm);

	if (G.debug) {
		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			printf("GL error: %s\n", gluErrorString(error));
		}
	}
}

/* ********** filesector handling ************ */

void WM_event_fileselect_event(wmWindowManager *wm, void *ophandle, int eventval)
{
	/* add to all windows! */
	wmWindow *win;
	
	for (win = wm->windows.first; win; win = win->next) {
		wmEvent event = *win->eventstate;
		
		event.type = EVT_FILESELECT;
		event.val = eventval;
		event.customdata = ophandle;     // only as void pointer type check

		wm_event_add(win, &event);
	}
}

/* operator is supposed to have a filled "path" property */
/* optional property: filetype (XXX enum?) */

/* Idea is to keep a handler alive on window queue, owning the operator.
 * The filewindow can send event to make it execute, thus ensuring
 * executing happens outside of lower level queues, with UI refreshed.
 * Should also allow multiwin solutions */

void WM_event_add_fileselect(bContext *C, wmOperator *op)
{
	wmEventHandler *handler, *handlernext;
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	int full = 1;    // XXX preset?

	/* only allow 1 file selector open per window */
	for (handler = win->modalhandlers.first; handler; handler = handlernext) {
		handlernext = handler->next;
		
		if (handler->type == WM_HANDLER_FILESELECT) {
			bScreen *screen = CTX_wm_screen(C);
			ScrArea *sa;

			/* find the area with the file selector for this handler */
			for (sa = screen->areabase.first; sa; sa = sa->next) {
				if (sa->spacetype == SPACE_FILE) {
					SpaceFile *sfile = sa->spacedata.first;

					if (sfile->op == handler->op) {
						CTX_wm_area_set(C, sa);
						wm_handler_fileselect_do(C, &win->modalhandlers, handler, EVT_FILESELECT_CANCEL);
						break;
					}
				}
			}

			/* if not found we stop the handler without changing the screen */
			if (!sa)
				wm_handler_fileselect_do(C, &win->modalhandlers, handler, EVT_FILESELECT_EXTERNAL_CANCEL);
		}
	}
	
	handler = MEM_callocN(sizeof(wmEventHandler), "fileselect handler");
	
	handler->type = WM_HANDLER_FILESELECT;
	handler->op = op;
	handler->op_area = CTX_wm_area(C);
	handler->op_region = CTX_wm_region(C);
	handler->filescreen = CTX_wm_screen(C);
	
	BLI_addhead(&win->modalhandlers, handler);
	
	/* check props once before invoking if check is available
	 * ensures initial properties are valid */
	if (op->type->check) {
		op->type->check(C, op); /* ignore return value */
	}

	WM_event_fileselect_event(wm, op, full ? EVT_FILESELECT_FULL_OPEN : EVT_FILESELECT_OPEN);
}

#if 0
/* lets not expose struct outside wm? */
static void WM_event_set_handler_flag(wmEventHandler *handler, int flag)
{
	handler->flag = flag;
}
#endif

wmEventHandler *WM_event_add_modal_handler(bContext *C, wmOperator *op)
{
	wmEventHandler *handler = MEM_callocN(sizeof(wmEventHandler), "event modal handler");
	wmWindow *win = CTX_wm_window(C);
	
	/* operator was part of macro */
	if (op->opm) {
		/* give the mother macro to the handler */
		handler->op = op->opm;
		/* mother macro opm becomes the macro element */
		handler->op->opm = op;
	}
	else
		handler->op = op;
	
	handler->op_area = CTX_wm_area(C);       /* means frozen screen context for modal handlers! */
	handler->op_region = CTX_wm_region(C);
	
	BLI_addhead(&win->modalhandlers, handler);

	return handler;
}

wmEventHandler *WM_event_add_keymap_handler(ListBase *handlers, wmKeyMap *keymap)
{
	wmEventHandler *handler;

	if (!keymap) {
		printf("%s: called with NULL keymap\n", __func__);
		return NULL;
	}

	/* only allow same keymap once */
	for (handler = handlers->first; handler; handler = handler->next)
		if (handler->keymap == keymap)
			return handler;
	
	handler = MEM_callocN(sizeof(wmEventHandler), "event keymap handler");
	BLI_addtail(handlers, handler);
	handler->keymap = keymap;

	return handler;
}

/* priorities not implemented yet, for time being just insert in begin of list */
wmEventHandler *WM_event_add_keymap_handler_priority(ListBase *handlers, wmKeyMap *keymap, int UNUSED(priority))
{
	wmEventHandler *handler;
	
	WM_event_remove_keymap_handler(handlers, keymap);
	
	handler = MEM_callocN(sizeof(wmEventHandler), "event keymap handler");
	BLI_addhead(handlers, handler);
	handler->keymap = keymap;
	
	return handler;
}

wmEventHandler *WM_event_add_keymap_handler_bb(ListBase *handlers, wmKeyMap *keymap, const rcti *bblocal, const rcti *bbwin)
{
	wmEventHandler *handler = WM_event_add_keymap_handler(handlers, keymap);
	
	if (handler) {
		handler->bblocal = bblocal;
		handler->bbwin = bbwin;
	}
	return handler;
}

void WM_event_remove_keymap_handler(ListBase *handlers, wmKeyMap *keymap)
{
	wmEventHandler *handler;
	
	for (handler = handlers->first; handler; handler = handler->next) {
		if (handler->keymap == keymap) {
			BLI_remlink(handlers, handler);
			wm_event_free_handler(handler);
			break;
		}
	}
}

wmEventHandler *WM_event_add_ui_handler(
        const bContext *C, ListBase *handlers,
        wmUIHandlerFunc ui_handle, wmUIHandlerRemoveFunc ui_remove,
        void *userdata)
{
	wmEventHandler *handler = MEM_callocN(sizeof(wmEventHandler), "event ui handler");
	handler->ui_handle = ui_handle;
	handler->ui_remove = ui_remove;
	handler->ui_userdata = userdata;
	if (C) {
		handler->ui_area    = CTX_wm_area(C);
		handler->ui_region  = CTX_wm_region(C);
		handler->ui_menu    = CTX_wm_menu(C);
	}
	else {
		handler->ui_area    = NULL;
		handler->ui_region  = NULL;
		handler->ui_menu    = NULL;
	}

	
	BLI_addhead(handlers, handler);
	
	return handler;
}

/* set "postpone" for win->modalhandlers, this is in a running for () loop in wm_handlers_do() */
void WM_event_remove_ui_handler(
        ListBase *handlers,
        wmUIHandlerFunc ui_handle, wmUIHandlerRemoveFunc ui_remove,
        void *userdata, const bool postpone)
{
	wmEventHandler *handler;
	
	for (handler = handlers->first; handler; handler = handler->next) {
		if ((handler->ui_handle == ui_handle) &&
		    (handler->ui_remove == ui_remove) &&
		    (handler->ui_userdata == userdata))
		{
			/* handlers will be freed in wm_handlers_do() */
			if (postpone) {
				handler->flag |= WM_HANDLER_DO_FREE;
			}
			else {
				BLI_remlink(handlers, handler);
				wm_event_free_handler(handler);
			}
			break;
		}
	}
}

void WM_event_free_ui_handler_all(
        bContext *C, ListBase *handlers,
        wmUIHandlerFunc ui_handle, wmUIHandlerRemoveFunc ui_remove)
{
	wmEventHandler *handler, *handler_next;

	for (handler = handlers->first; handler; handler = handler_next) {
		handler_next = handler->next;
		if ((handler->ui_handle == ui_handle) &&
		    (handler->ui_remove == ui_remove))
		{
			ui_remove(C, handler->ui_userdata);
			BLI_remlink(handlers, handler);
			wm_event_free_handler(handler);
		}
	}
}

wmEventHandler *WM_event_add_dropbox_handler(ListBase *handlers, ListBase *dropboxes)
{
	wmEventHandler *handler;

	/* only allow same dropbox once */
	for (handler = handlers->first; handler; handler = handler->next)
		if (handler->dropboxes == dropboxes)
			return handler;
	
	handler = MEM_callocN(sizeof(wmEventHandler), "dropbox handler");
	
	/* dropbox stored static, no free or copy */
	handler->dropboxes = dropboxes;
	BLI_addhead(handlers, handler);
	
	return handler;
}

/* XXX solution works, still better check the real cause (ton) */
void WM_event_remove_area_handler(ListBase *handlers, void *area)
{
	wmEventHandler *handler, *nexthandler;

	for (handler = handlers->first; handler; handler = nexthandler) {
		nexthandler = handler->next;
		if (handler->type != WM_HANDLER_FILESELECT) {
			if (handler->ui_area == area) {
				BLI_remlink(handlers, handler);
				wm_event_free_handler(handler);
			}
		}
	}
}

#if 0
static void WM_event_remove_handler(ListBase *handlers, wmEventHandler *handler)
{
	BLI_remlink(handlers, handler);
	wm_event_free_handler(handler);
}
#endif

void WM_event_add_mousemove(bContext *C)
{
	wmWindow *window = CTX_wm_window(C);
	
	window->addmousemove = 1;
}


/* for modal callbacks, check configuration for how to interpret exit with tweaks  */
bool WM_modal_tweak_exit(const wmEvent *event, int tweak_event)
{
	/* if the release-confirm userpref setting is enabled, 
	 * tweak events can be canceled when mouse is released
	 */
	if (U.flag & USER_RELEASECONFIRM) {
		/* option on, so can exit with km-release */
		if (event->val == KM_RELEASE) {
			switch (tweak_event) {
				case EVT_TWEAK_L:
				case EVT_TWEAK_M:
				case EVT_TWEAK_R:
					return 1;
			}
		}
		else {
			/* if the initial event wasn't a tweak event then
			 * ignore USER_RELEASECONFIRM setting: see [#26756] */
			if (ELEM3(tweak_event, EVT_TWEAK_L, EVT_TWEAK_M, EVT_TWEAK_R) == 0) {
				return 1;
			}
		}
	}
	else {
		/* this is fine as long as not doing km-release, otherwise
		 * some items (i.e. markers) being tweaked may end up getting
		 * dropped all over
		 */
		if (event->val != KM_RELEASE)
			return 1;
	}
	
	return 0;
}

/* ********************* ghost stuff *************** */

static int convert_key(GHOST_TKey key) 
{
	if (key >= GHOST_kKeyA && key <= GHOST_kKeyZ) {
		return (AKEY + ((int) key - GHOST_kKeyA));
	}
	else if (key >= GHOST_kKey0 && key <= GHOST_kKey9) {
		return (ZEROKEY + ((int) key - GHOST_kKey0));
	}
	else if (key >= GHOST_kKeyNumpad0 && key <= GHOST_kKeyNumpad9) {
		return (PAD0 + ((int) key - GHOST_kKeyNumpad0));
	}
	else if (key >= GHOST_kKeyF1 && key <= GHOST_kKeyF19) {
		return (F1KEY + ((int) key - GHOST_kKeyF1));
	}
	else {
		switch (key) {
			case GHOST_kKeyBackSpace:       return BACKSPACEKEY;
			case GHOST_kKeyTab:             return TABKEY;
			case GHOST_kKeyLinefeed:        return LINEFEEDKEY;
			case GHOST_kKeyClear:           return 0;
			case GHOST_kKeyEnter:           return RETKEY;

			case GHOST_kKeyEsc:             return ESCKEY;
			case GHOST_kKeySpace:           return SPACEKEY;
			case GHOST_kKeyQuote:           return QUOTEKEY;
			case GHOST_kKeyComma:           return COMMAKEY;
			case GHOST_kKeyMinus:           return MINUSKEY;
			case GHOST_kKeyPeriod:          return PERIODKEY;
			case GHOST_kKeySlash:           return SLASHKEY;

			case GHOST_kKeySemicolon:       return SEMICOLONKEY;
			case GHOST_kKeyEqual:           return EQUALKEY;

			case GHOST_kKeyLeftBracket:     return LEFTBRACKETKEY;
			case GHOST_kKeyRightBracket:    return RIGHTBRACKETKEY;
			case GHOST_kKeyBackslash:       return BACKSLASHKEY;
			case GHOST_kKeyAccentGrave:     return ACCENTGRAVEKEY;

			case GHOST_kKeyLeftShift:       return LEFTSHIFTKEY;
			case GHOST_kKeyRightShift:      return RIGHTSHIFTKEY;
			case GHOST_kKeyLeftControl:     return LEFTCTRLKEY;
			case GHOST_kKeyRightControl:    return RIGHTCTRLKEY;
			case GHOST_kKeyOS:              return OSKEY;
			case GHOST_kKeyLeftAlt:         return LEFTALTKEY;
			case GHOST_kKeyRightAlt:        return RIGHTALTKEY;

			case GHOST_kKeyCapsLock:        return CAPSLOCKKEY;
			case GHOST_kKeyNumLock:         return 0;
			case GHOST_kKeyScrollLock:      return 0;

			case GHOST_kKeyLeftArrow:       return LEFTARROWKEY;
			case GHOST_kKeyRightArrow:      return RIGHTARROWKEY;
			case GHOST_kKeyUpArrow:         return UPARROWKEY;
			case GHOST_kKeyDownArrow:       return DOWNARROWKEY;

			case GHOST_kKeyPrintScreen:     return 0;
			case GHOST_kKeyPause:           return PAUSEKEY;

			case GHOST_kKeyInsert:          return INSERTKEY;
			case GHOST_kKeyDelete:          return DELKEY;
			case GHOST_kKeyHome:            return HOMEKEY;
			case GHOST_kKeyEnd:             return ENDKEY;
			case GHOST_kKeyUpPage:          return PAGEUPKEY;
			case GHOST_kKeyDownPage:        return PAGEDOWNKEY;

			case GHOST_kKeyNumpadPeriod:    return PADPERIOD;
			case GHOST_kKeyNumpadEnter:     return PADENTER;
			case GHOST_kKeyNumpadPlus:      return PADPLUSKEY;
			case GHOST_kKeyNumpadMinus:     return PADMINUS;
			case GHOST_kKeyNumpadAsterisk:  return PADASTERKEY;
			case GHOST_kKeyNumpadSlash:     return PADSLASHKEY;

			case GHOST_kKeyGrLess:          return GRLESSKEY;

			case GHOST_kKeyMediaPlay:       return MEDIAPLAY;
			case GHOST_kKeyMediaStop:       return MEDIASTOP;
			case GHOST_kKeyMediaFirst:      return MEDIAFIRST;
			case GHOST_kKeyMediaLast:       return MEDIALAST;
			
			default:
				return UNKNOWNKEY;  /* GHOST_kKeyUnknown */
		}
	}
}

static void wm_eventemulation(wmEvent *event)
{
	/* Store last mmb event value to make emulation work when modifier keys are released first. */
	static int mmb_emulated = 0; /* this should be in a data structure somwhere */
	
	/* middlemouse emulation */
	if (U.flag & USER_TWOBUTTONMOUSE) {
		if (event->type == LEFTMOUSE) {
			
			if (event->val == KM_PRESS && event->alt) {
				event->type = MIDDLEMOUSE;
				event->alt = 0;
				mmb_emulated = 1;
			}
			else if (event->val == KM_RELEASE) {
				/* only send middle-mouse release if emulated */
				if (mmb_emulated) {
					event->type = MIDDLEMOUSE;
					event->alt = 0;
				}
				mmb_emulated = 0;
			}
		}
		
	}
	
#ifdef __APPLE__
	
	/* rightmouse emulation */
	if (U.flag & USER_TWOBUTTONMOUSE) {
		if (event->type == LEFTMOUSE) {
			
			if (event->val == KM_PRESS && event->oskey) {
				event->type = RIGHTMOUSE;
				event->oskey = 0;
				mmb_emulated = 1;
			}
			else if (event->val == KM_RELEASE) {
				if (mmb_emulated) {
					event->oskey = RIGHTMOUSE;
					event->alt = 0;
				}
				mmb_emulated = 0;
			}
		}
		
	}
#endif
	
	/* numpad emulation */
	if (U.flag & USER_NONUMPAD) {
		switch (event->type) {
			case ZEROKEY: event->type = PAD0; break;
			case ONEKEY: event->type = PAD1; break;
			case TWOKEY: event->type = PAD2; break;
			case THREEKEY: event->type = PAD3; break;
			case FOURKEY: event->type = PAD4; break;
			case FIVEKEY: event->type = PAD5; break;
			case SIXKEY: event->type = PAD6; break;
			case SEVENKEY: event->type = PAD7; break;
			case EIGHTKEY: event->type = PAD8; break;
			case NINEKEY: event->type = PAD9; break;
			case MINUSKEY: event->type = PADMINUS; break;
			case EQUALKEY: event->type = PADPLUSKEY; break;
			case BACKSLASHKEY: event->type = PADSLASHKEY; break;
		}
	}
}

/* adds customdata to event */
static void update_tablet_data(wmWindow *win, wmEvent *event)
{
	const GHOST_TabletData *td = GHOST_GetTabletData(win->ghostwin);
	
	/* if there's tablet data from an active tablet device then add it */
	if ((td != NULL) && td->Active != GHOST_kTabletModeNone) {
		struct wmTabletData *wmtab = MEM_mallocN(sizeof(wmTabletData), "customdata tablet");
		
		wmtab->Active = (int)td->Active;
		wmtab->Pressure = td->Pressure;
		wmtab->Xtilt = td->Xtilt;
		wmtab->Ytilt = td->Ytilt;
		
		event->tablet_data = wmtab;
		// printf("%s: using tablet %.5f\n", __func__, wmtab->Pressure);
	}
	else {
		event->tablet_data = NULL;
		// printf("%s: not using tablet\n", __func__);
	}
}

/* adds customdata to event */
static void attach_ndof_data(wmEvent *event, const GHOST_TEventNDOFMotionData *ghost)
{
	wmNDOFMotionData *data = MEM_mallocN(sizeof(wmNDOFMotionData), "customdata NDOF");

	const float ts = U.ndof_sensitivity;
	const float rs = U.ndof_orbit_sensitivity;

	mul_v3_v3fl(data->tvec, &ghost->tx, ts);
	mul_v3_v3fl(data->rvec, &ghost->rx, rs);

	if (U.ndof_flag & NDOF_PAN_YZ_SWAP_AXIS) {
		float t;
		t =  data->tvec[1];
		data->tvec[1] = -data->tvec[2];
		data->tvec[2] = t;
	}

	data->dt = ghost->dt;

	data->progress = (wmProgress) ghost->progress;

	event->custom = EVT_DATA_NDOF_MOTION;
	event->customdata = data;
	event->customdatafree = 1;
}

/* imperfect but probably usable... draw/enable drags to other windows */
static wmWindow *wm_event_cursor_other_windows(wmWindowManager *wm, wmWindow *win, wmEvent *event)
{
	int mx = event->x, my = event->y;
	
	if (wm->windows.first == wm->windows.last)
		return NULL;
	
	/* in order to use window size and mouse position (pixels), we have to use a WM function */
	
	/* check if outside, include top window bar... */
	if (mx < 0 || my < 0 || mx > WM_window_pixels_x(win) || my > WM_window_pixels_y(win) + 30) {
		wmWindow *owin;
		wmEventHandler *handler;
		
		/* let's skip windows having modal handlers now */
		/* potential XXX ugly... I wouldn't have added a modalhandlers list (introduced in rev 23331, ton) */
		for (handler = win->modalhandlers.first; handler; handler = handler->next)
			if (handler->ui_handle || handler->op)
				return NULL;
		
		/* to desktop space */
		mx += (int) (U.pixelsize * win->posx);
		my += (int) (U.pixelsize * win->posy);
		
		/* check other windows to see if it has mouse inside */
		for (owin = wm->windows.first; owin; owin = owin->next) {
			
			if (owin != win) {
				int posx = (int) (U.pixelsize * owin->posx);
				int posy = (int) (U.pixelsize * owin->posy);
				
				if (mx - posx >= 0 && owin->posy >= 0 &&
				    mx - posx <= WM_window_pixels_x(owin) && my - posy <= WM_window_pixels_y(owin))
				{
					event->x = mx - (int)(U.pixelsize * owin->posx);
					event->y = my - (int)(U.pixelsize * owin->posy);
					
					return owin;
				}
			}
		}
	}
	return NULL;
}

static bool wm_event_is_double_click(wmEvent *event, wmEvent *event_state)
{
	if ((event->type == event_state->prevtype) &&
	    (event_state->prevval == KM_RELEASE) &&
	    (event->val == KM_PRESS))
	{
		if ((ISMOUSE(event->type) == false) || ((ABS(event->x - event_state->prevclickx)) <= 2 &&
		                                        (ABS(event->y - event_state->prevclicky)) <= 2))
		{
			if ((PIL_check_seconds_timer() - event_state->prevclicktime) * 1000 < U.dbl_click_time) {
				return true;
			}
		}
	}

	return false;
}

static void wm_event_add_mousemove(wmWindow *win, const wmEvent *event)
{
	wmEvent *event_last = win->queue.last;

	/* some painting operators want accurate mouse events, they can
	 * handle in between mouse move moves, others can happily ignore
	 * them for better performance */
	if (event_last && event_last->type == MOUSEMOVE)
		event_last->type = INBETWEEN_MOUSEMOVE;

	wm_event_add(win, event);

	{
		wmEvent *event_new = win->queue.last;
		if (event_last == NULL) {
			event_last = win->eventstate;
		}

		copy_v2_v2_int(&event_new->prevx, &event_last->x);
	}
}

/* windows store own event queues, no bContext here */
/* time is in 1000s of seconds, from ghost */
void wm_event_add_ghostevent(wmWindowManager *wm, wmWindow *win, int type, int UNUSED(time), void *customdata)
{
	wmWindow *owin;
	wmEvent event, *evt = win->eventstate;

	/* initialize and copy state (only mouse x y and modifiers) */
	event = *evt;

	switch (type) {
		/* mouse move, also to inactive window (X11 does this) */
		case GHOST_kEventCursorMove:
		{
			GHOST_TEventCursorData *cd = customdata;

			copy_v2_v2_int(&event.x, &cd->x);
			event.type = MOUSEMOVE;
			wm_event_add_mousemove(win, &event);
			copy_v2_v2_int(&evt->x, &event.x);
			
			/* also add to other window if event is there, this makes overdraws disappear nicely */
			/* it remaps mousecoord to other window in event */
			owin = wm_event_cursor_other_windows(wm, win, &event);
			if (owin) {
				wmEvent oevent, *oevt = owin->eventstate;

				oevent = *oevt;

				copy_v2_v2_int(&oevent.x, &event.x);
				oevent.type = MOUSEMOVE;
				wm_event_add_mousemove(owin, &oevent);
				copy_v2_v2_int(&oevt->x, &oevent.x);
			}
				
			break;
		}
		case GHOST_kEventTrackpad:
		{
			GHOST_TEventTrackpadData *pd = customdata;
			switch (pd->subtype) {
				case GHOST_kTrackpadEventMagnify:
					event.type = MOUSEZOOM;
					pd->deltaX = -pd->deltaX;
					pd->deltaY = -pd->deltaY;
					break;
				case GHOST_kTrackpadEventRotate:
					event.type = MOUSEROTATE;
					break;
				case GHOST_kTrackpadEventScroll:
				default:
					event.type = MOUSEPAN;
					break;
			}

			event.x = evt->x = pd->x;
			event.y = evt->y = pd->y;
			event.val = 0;
			
			/* Use prevx/prevy so we can calculate the delta later */
			event.prevx = event.x - pd->deltaX;
			event.prevy = event.y - (-pd->deltaY);
			
			wm_event_add(win, &event);
			break;
		}
		/* mouse button */
		case GHOST_kEventButtonDown:
		case GHOST_kEventButtonUp:
		{
			GHOST_TEventButtonData *bd = customdata;
			
			/* get value and type from ghost */
			event.val = (type == GHOST_kEventButtonDown) ? KM_PRESS : KM_RELEASE;
			
			if (bd->button == GHOST_kButtonMaskLeft)
				event.type = LEFTMOUSE;
			else if (bd->button == GHOST_kButtonMaskRight)
				event.type = RIGHTMOUSE;
			else if (bd->button == GHOST_kButtonMaskButton4)
				event.type = BUTTON4MOUSE;
			else if (bd->button == GHOST_kButtonMaskButton5)
				event.type = BUTTON5MOUSE;
			else if (bd->button == GHOST_kButtonMaskButton6)
				event.type = BUTTON6MOUSE;
			else if (bd->button == GHOST_kButtonMaskButton7)
				event.type = BUTTON7MOUSE;
			else
				event.type = MIDDLEMOUSE;
			
			wm_eventemulation(&event);
			
			/* copy previous state to prev event state (two old!) */
			evt->prevval = evt->val;
			evt->prevtype = evt->type;

			/* copy to event state */
			evt->val = event.val;
			evt->type = event.type;

			if (win->active == 0) {
				int cx, cy;
				
				/* entering window, update mouse pos. (ghost sends win-activate *after* the mouseclick in window!) */
				wm_get_cursor_position(win, &cx, &cy);

				event.x = evt->x = cx;
				event.y = evt->y = cy;
			}
			
			/* double click test */
			if (wm_event_is_double_click(&event, evt)) {
				if (G.debug & (G_DEBUG_HANDLERS | G_DEBUG_EVENTS))
					printf("%s Send double click\n", __func__);
				event.val = KM_DBL_CLICK;
			}
			if (event.val == KM_PRESS) {
				evt->prevclicktime = PIL_check_seconds_timer();
				evt->prevclickx = event.x;
				evt->prevclicky = event.y;
			}
			
			/* add to other window if event is there (not to both!) */
			owin = wm_event_cursor_other_windows(wm, win, &event);
			if (owin) {
				wmEvent oevent = *(owin->eventstate);
				
				oevent.x = event.x;
				oevent.y = event.y;
				oevent.type = event.type;
				oevent.val = event.val;
				
				wm_event_add(owin, &oevent);
			}
			else {
				wm_event_add(win, &event);
			}
			
			break;
		}
		/* keyboard */
		case GHOST_kEventKeyDown:
		case GHOST_kEventKeyUp:
		{
			GHOST_TEventKeyData *kd = customdata;
			event.type = convert_key(kd->key);
			event.ascii = kd->ascii;
			memcpy(event.utf8_buf, kd->utf8_buf, sizeof(event.utf8_buf)); /* might be not null terminated*/
			event.val = (type == GHOST_kEventKeyDown) ? KM_PRESS : KM_RELEASE;
			
			wm_eventemulation(&event);
			
			/* copy previous state to prev event state (two old!) */
			evt->prevval = evt->val;
			evt->prevtype = evt->type;

			/* copy to event state */
			evt->val = event.val;
			evt->type = event.type;
			
			/* exclude arrow keys, esc, etc from text input */
			if (type == GHOST_kEventKeyUp) {
				event.ascii = '\0';

				/* ghost should do this already for key up */
				if (event.utf8_buf[0]) {
					printf("%s: ghost on your platform is misbehaving, utf8 events on key up!\n", __func__);
				}
				event.utf8_buf[0] = '\0';
			}
			else {
				if (event.ascii < 32 && event.ascii > 0)
					event.ascii = '\0';
				if (event.utf8_buf[0] < 32 && event.utf8_buf[0] > 0)
					event.utf8_buf[0] = '\0';
			}

			if (event.utf8_buf[0]) {
				if (BLI_str_utf8_size(event.utf8_buf) == -1) {
					printf("%s: ghost detected an invalid unicode character '%d'!\n",
					       __func__, (int)(unsigned char)event.utf8_buf[0]);
					event.utf8_buf[0] = '\0';
				}
			}

			/* modifiers assign to eventstate, so next event gets the modifer (makes modifier key events work) */
			/* assigning both first and second is strange - campbell */
			switch (event.type) {
				case LEFTSHIFTKEY: case RIGHTSHIFTKEY:
					evt->shift = (event.val == KM_PRESS) ?
					            ((evt->ctrl || evt->alt || evt->oskey) ? (KM_MOD_FIRST | KM_MOD_SECOND) : KM_MOD_FIRST) :
					            false;
					break;
				case LEFTCTRLKEY: case RIGHTCTRLKEY:
					evt->ctrl = (event.val == KM_PRESS) ?
					            ((evt->shift || evt->alt || evt->oskey) ? (KM_MOD_FIRST | KM_MOD_SECOND) : KM_MOD_FIRST) :
					            false;
					break;
				case LEFTALTKEY: case RIGHTALTKEY:
					evt->alt = (event.val == KM_PRESS) ?
					            ((evt->ctrl || evt->shift || evt->oskey) ? (KM_MOD_FIRST | KM_MOD_SECOND) : KM_MOD_FIRST) :
					            false;
					break;
				case OSKEY:
					evt->oskey = (event.val == KM_PRESS) ?
					            ((evt->ctrl || evt->alt || evt->shift) ? (KM_MOD_FIRST | KM_MOD_SECOND) : KM_MOD_FIRST) :
					            false;
					break;
				default:
					if (event.val == KM_PRESS && event.keymodifier == 0)
						evt->keymodifier = event.type;  /* only set in eventstate, for next event */
					else if (event.val == KM_RELEASE && event.keymodifier == event.type)
						event.keymodifier = evt->keymodifier = 0;
					break;
			}

			/* double click test */
			/* if previous event was same type, and previous was release, and now it presses... */
			if (wm_event_is_double_click(&event, evt)) {
				if (G.debug & (G_DEBUG_HANDLERS | G_DEBUG_EVENTS))
					printf("%s Send double click\n", __func__);
				evt->val = event.val = KM_DBL_CLICK;
			}
			
			/* this case happens on holding a key pressed, it should not generate
			 * press events events with the same key as modifier */
			if (event.keymodifier == event.type)
				event.keymodifier = 0;
						
			/* this case happened with an external numpad, it's not really clear
			 * why, but it's also impossible to map a key modifier to an unknown
			 * key, so it shouldn't harm */
			if (event.keymodifier == UNKNOWNKEY)
				event.keymodifier = 0;
			
			/* if test_break set, it catches this. Do not set with modifier presses. XXX Keep global for now? */
			if ((event.type == ESCKEY && event.val == KM_PRESS) &&
			    /* check other modifiers because ms-windows uses these to bring up the task manager */
			    (event.shift == 0 && event.ctrl == 0 && event.alt == 0))
			{
				G.is_break = true;
			}
			
			/* double click test - only for press */
			if (event.val == KM_PRESS) {
				evt->prevclicktime = PIL_check_seconds_timer();
				evt->prevclickx = event.x;
				evt->prevclicky = event.y;
			}
			
			wm_event_add(win, &event);
			
			break;
		}
			
		case GHOST_kEventWheel:
		{
			GHOST_TEventWheelData *wheelData = customdata;
			
			if (wheelData->z > 0)
				event.type = WHEELUPMOUSE;
			else
				event.type = WHEELDOWNMOUSE;
			
			event.val = KM_PRESS;
			wm_event_add(win, &event);
			
			break;
		}
		case GHOST_kEventTimer:
		{
			event.type = TIMER;
			event.custom = EVT_DATA_TIMER;
			event.customdata = customdata;
			event.val = 0;
			event.keymodifier = 0;
			wm_event_add(win, &event);

			break;
		}

		case GHOST_kEventNDOFMotion:
		{
			event.type = NDOF_MOTION;
			event.val = 0;
			attach_ndof_data(&event, customdata);
			wm_event_add(win, &event);

			if (G.debug & (G_DEBUG_HANDLERS | G_DEBUG_EVENTS))
				printf("%s sending NDOF_MOTION, prev = %d %d\n", __func__, event.x, event.y);

			break;
		}

		case GHOST_kEventNDOFButton:
		{
			GHOST_TEventNDOFButtonData *e = customdata;

			event.type = NDOF_BUTTON_NONE + e->button;

			switch (e->action) {
				case GHOST_kPress:
					event.val = KM_PRESS;
					break;
				case GHOST_kRelease:
					event.val = KM_RELEASE;
					break;
			}

			event.custom = 0;
			event.customdata = NULL;

			wm_event_add(win, &event);

			break;
		}

		case GHOST_kEventUnknown:
		case GHOST_kNumEventTypes:
			break;

		case GHOST_kEventWindowDeactivate:
		{
			event.type = WINDEACTIVATE;
			wm_event_add(win, &event);

			break;
		}

	}

#if 0
	WM_event_print(&event);
#endif
}

void WM_set_locked_interface(wmWindowManager *wm, bool lock)
{
	/* This will prevent events from being handled while interface is locked
	 *
	 * Use a "local" flag for now, because currently no other areas could
	 * benefit of locked interface anyway (aka using G.is_interface_locked
	 * wouldn't be useful anywhere outside of window manager, so let's not
	 * pollute global context with such an information for now).
	 */
	wm->is_interface_locked = lock ? 1 : 0;

	/* This will prevent drawing regions which uses non-threadsafe data.
	 * Currently it'll be just a 3D viewport.
	 *
	 * TODO(sergey): Make it different locked states, so different jobs
	 *               could lock different areas of blender and allow
	 *               interaction with others?
	 */
	BKE_spacedata_draw_locks(lock);
}


/* -------------------------------------------------------------------- */
/* NDOF */

/** \name NDOF Utility Functions
 * \{ */


void WM_event_ndof_pan_get(const wmNDOFMotionData *ndof, float r_pan[3], const bool use_zoom)
{
	int z_flag = use_zoom ? NDOF_ZOOM_INVERT : NDOF_PANZ_INVERT_AXIS;
	r_pan[0] = ndof->tvec[0] * ((U.ndof_flag & NDOF_PANX_INVERT_AXIS) ? -1.0f : 1.0f);
	r_pan[1] = ndof->tvec[1] * ((U.ndof_flag & NDOF_PANY_INVERT_AXIS) ? -1.0f : 1.0f);
	r_pan[2] = ndof->tvec[2] * ((U.ndof_flag & z_flag)                ? -1.0f : 1.0f);
}

void WM_event_ndof_rotate_get(const wmNDOFMotionData *ndof, float r_rot[3])
{
	r_rot[0] = ndof->rvec[0] * ((U.ndof_flag & NDOF_ROTX_INVERT_AXIS) ? -1.0f : 1.0f);
	r_rot[1] = ndof->rvec[1] * ((U.ndof_flag & NDOF_ROTY_INVERT_AXIS) ? -1.0f : 1.0f);
	r_rot[2] = ndof->rvec[2] * ((U.ndof_flag & NDOF_ROTZ_INVERT_AXIS) ? -1.0f : 1.0f);
}

float WM_event_ndof_to_axis_angle(const struct wmNDOFMotionData *ndof, float axis[3])
{
	float angle;
	angle = normalize_v3_v3(axis, ndof->rvec);

	axis[0] = axis[0] * ((U.ndof_flag & NDOF_ROTX_INVERT_AXIS) ? -1.0f : 1.0f);
	axis[1] = axis[1] * ((U.ndof_flag & NDOF_ROTY_INVERT_AXIS) ? -1.0f : 1.0f);
	axis[2] = axis[2] * ((U.ndof_flag & NDOF_ROTZ_INVERT_AXIS) ? -1.0f : 1.0f);

	return ndof->dt * angle;
}

void WM_event_ndof_to_quat(const struct wmNDOFMotionData *ndof, float q[4])
{
	float axis[3];
	float angle;

	angle = WM_event_ndof_to_axis_angle(ndof, axis);
	axis_angle_to_quat(q, axis, angle);
}

/* if this is a tablet event, return tablet pressure and set *pen_flip
 * to 1 if the eraser tool is being used, 0 otherwise */
float WM_event_tablet_data(const wmEvent *event, int *pen_flip, float tilt[2])
{
	int erasor = 0;
	float pressure = 1;

	if (tilt)
		zero_v2(tilt);

	if (event->tablet_data) {
		wmTabletData *wmtab = event->tablet_data;

		erasor = (wmtab->Active == EVT_TABLET_ERASER);
		if (wmtab->Active != EVT_TABLET_NONE) {
			pressure = wmtab->Pressure;
			if (tilt) {
				tilt[0] = wmtab->Xtilt;
				tilt[1] = wmtab->Ytilt;
			}
		}
	}

	if (pen_flip)
		(*pen_flip) = erasor;

	return pressure;
}

bool WM_event_is_tablet(const struct wmEvent *event)
{
	return (event->tablet_data) ? true : false;
}


/** \} */
