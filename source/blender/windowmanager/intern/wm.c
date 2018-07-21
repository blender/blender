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

/** \file blender/windowmanager/intern/wm.c
 *  \ingroup wm
 *
 * Internal functions for managing UI registrable types (operator, UI and menu types)
 *
 * Also Blenders main event loop (WM_main)
 */

#include <string.h>
#include <stddef.h>

#include "BLI_sys_types.h"

#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"

#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_report.h"
#include "BKE_global.h"
#include "BKE_workspace.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"
#include "wm_window.h"
#include "wm_event_system.h"
#include "wm_draw.h"
#include "wm.h"

#include "ED_screen.h"
#include "BKE_undo_system.h"

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

/* ****************************************************** */

#define MAX_OP_REGISTERED   32

void WM_operator_free(wmOperator *op)
{

#ifdef WITH_PYTHON
	if (op->py_instance) {
		/* do this first in case there are any __del__ functions or
		 * similar that use properties */
		BPY_DECREF_RNA_INVALIDATE(op->py_instance);
	}
#endif

	if (op->ptr) {
		op->properties = op->ptr->data;
		MEM_freeN(op->ptr);
	}

	if (op->properties) {
		IDP_FreeProperty(op->properties);
		MEM_freeN(op->properties);
	}

	if (op->reports && (op->reports->flag & RPT_FREE)) {
		BKE_reports_clear(op->reports);
		MEM_freeN(op->reports);
	}

	if (op->macro.first) {
		wmOperator *opm, *opmnext;
		for (opm = op->macro.first; opm; opm = opmnext) {
			opmnext = opm->next;
			WM_operator_free(opm);
		}
	}

	MEM_freeN(op);
}

void WM_operator_free_all_after(wmWindowManager *wm, struct wmOperator *op)
{
	op = op->next;
	while (op != NULL) {
		wmOperator *op_next = op->next;
		BLI_remlink(&wm->operators, op);
		WM_operator_free(op);
		op = op_next;
	}
}

/**
 * Use with extreme care!,
 * properties, customdata etc - must be compatible.
 *
 * \param op  Operator to assign the type to.
 * \param ot  OperatorType to assign.
 */
void WM_operator_type_set(wmOperator *op, wmOperatorType *ot)
{
	/* not supported for Python */
	BLI_assert(op->py_instance == NULL);

	op->type = ot;
	op->ptr->type = ot->srna;

	/* ensure compatible properties */
	if (op->properties) {
		PointerRNA ptr;

		WM_operator_properties_create_ptr(&ptr, ot);

		WM_operator_properties_default(&ptr, false);

		if (ptr.data) {
			IDP_SyncGroupTypes(op->properties, ptr.data, true);
		}

		WM_operator_properties_free(&ptr);
	}
}

static void wm_reports_free(wmWindowManager *wm)
{
	BKE_reports_clear(&wm->reports);
	WM_event_remove_timer(wm, NULL, wm->reports.reporttimer);
}

/* all operations get registered in the windowmanager here */
/* called on event handling by event_system.c */
void wm_operator_register(bContext *C, wmOperator *op)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	int tot = 0;

	BLI_addtail(&wm->operators, op);

	/* only count registered operators */
	while (op) {
		wmOperator *op_prev = op->prev;
		if (op->type->flag & OPTYPE_REGISTER) {
			tot += 1;
		}
		if (tot > MAX_OP_REGISTERED) {
			BLI_remlink(&wm->operators, op);
			WM_operator_free(op);
		}
		op = op_prev;
	}

	/* so the console is redrawn */
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO_REPORT, NULL);
	WM_event_add_notifier(C, NC_WM | ND_HISTORY, NULL);
}


void WM_operator_stack_clear(wmWindowManager *wm)
{
	wmOperator *op;

	while ((op = BLI_pophead(&wm->operators))) {
		WM_operator_free(op);
	}

	WM_main_add_notifier(NC_WM | ND_HISTORY, NULL);
}

/**
 * This function is needed in the case when an addon id disabled
 * while a modal operator it defined is running.
 */
void WM_operator_handlers_clear(wmWindowManager *wm, wmOperatorType *ot)
{
	wmWindow *win;
	for (win = wm->windows.first; win; win = win->next) {
		ListBase *lb[2] = {&win->handlers, &win->modalhandlers};
		wmEventHandler *handler;
		int i;

		for (i = 0; i < 2; i++) {
			for (handler = lb[i]->first; handler; handler = handler->next) {
				if (handler->op && handler->op->type == ot) {
					/* don't run op->cancel because it needs the context,
					 * assume whoever unregisters the operator will cleanup */
					handler->flag |= WM_HANDLER_DO_FREE;
					WM_operator_free(handler->op);
					handler->op = NULL;
				}
			}
		}
	}
}

/* ************ uiListType handling ************** */

static GHash *uilisttypes_hash = NULL;

uiListType *WM_uilisttype_find(const char *idname, bool quiet)
{
	uiListType *ult;

	if (idname[0]) {
		ult = BLI_ghash_lookup(uilisttypes_hash, idname);
		if (ult) {
			return ult;
		}
	}

	if (!quiet) {
		printf("search for unknown uilisttype %s\n", idname);
	}

	return NULL;
}

bool WM_uilisttype_add(uiListType *ult)
{
	BLI_ghash_insert(uilisttypes_hash, ult->idname, ult);
	return 1;
}

void WM_uilisttype_freelink(uiListType *ult)
{
	bool ok;

	ok = BLI_ghash_remove(uilisttypes_hash, ult->idname, NULL, MEM_freeN);

	BLI_assert(ok);
	(void)ok;
}

/* called on initialize WM_init() */
void WM_uilisttype_init(void)
{
	uilisttypes_hash = BLI_ghash_str_new_ex("uilisttypes_hash gh", 16);
}

void WM_uilisttype_free(void)
{
	GHashIterator gh_iter;

	GHASH_ITER (gh_iter, uilisttypes_hash) {
		uiListType *ult = BLI_ghashIterator_getValue(&gh_iter);
		if (ult->ext.free) {
			ult->ext.free(ult->ext.data);
		}
	}

	BLI_ghash_free(uilisttypes_hash, NULL, MEM_freeN);
	uilisttypes_hash = NULL;
}

/* ****************************************** */

void WM_keymap_init(bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);

	/* create standard key configs */
	if (!wm->defaultconf)
		wm->defaultconf = WM_keyconfig_new(wm, "Blender");
	if (!wm->addonconf)
		wm->addonconf = WM_keyconfig_new(wm, "Blender Addon");
	if (!wm->userconf)
		wm->userconf = WM_keyconfig_new(wm, "Blender User");

	/* initialize only after python init is done, for keymaps that
	 * use python operators */
	if (CTX_py_init_get(C) && (wm->initialized & WM_KEYMAP_IS_INITIALIZED) == 0) {
		/* create default key config, only initialize once,
		 * it's persistent across sessions */
		if (!(wm->defaultconf->flag & KEYCONF_INIT_DEFAULT)) {
			wm_window_keymap(wm->defaultconf);
			ED_spacetypes_keymap(wm->defaultconf);

			wm->defaultconf->flag |= KEYCONF_INIT_DEFAULT;
		}

		WM_keyconfig_update_tag(NULL, NULL);
		WM_keyconfig_update(wm);

		wm->initialized |= WM_KEYMAP_IS_INITIALIZED;
	}
}

void WM_check(bContext *C)
{
	Main *bmain = CTX_data_main(C);
	wmWindowManager *wm = CTX_wm_manager(C);

	/* wm context */
	if (wm == NULL) {
		wm = CTX_data_main(C)->wm.first;
		CTX_wm_manager_set(C, wm);
	}

	if (wm == NULL || BLI_listbase_is_empty(&wm->windows)) {
		return;
	}

	if (!G.background) {
		/* case: fileread */
		if ((wm->initialized & WM_WINDOW_IS_INITIALIZED) == 0) {
			WM_keymap_init(C);
			WM_autosave_init(wm);
		}

		/* case: no open windows at all, for old file reads */
		wm_window_ghostwindows_ensure(wm);
	}

	if (wm->message_bus == NULL) {
		wm->message_bus = WM_msgbus_create();
	}

	/* case: fileread */
	/* note: this runs in bg mode to set the screen context cb */
	if ((wm->initialized & WM_WINDOW_IS_INITIALIZED) == 0) {
		ED_screens_initialize(bmain, wm);
		wm->initialized |= WM_WINDOW_IS_INITIALIZED;
	}
}

void wm_clear_default_size(bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win;

	/* wm context */
	if (wm == NULL) {
		wm = CTX_data_main(C)->wm.first;
		CTX_wm_manager_set(C, wm);
	}

	if (wm == NULL || BLI_listbase_is_empty(&wm->windows)) {
		return;
	}

	for (win = wm->windows.first; win; win = win->next) {
		win->sizex = 0;
		win->sizey = 0;
		win->posx = 0;
		win->posy = 0;
	}

}

/* on startup, it adds all data, for matching */
void wm_add_default(Main *bmain, bContext *C)
{
	wmWindowManager *wm = BKE_libblock_alloc(bmain, ID_WM, "WinMan", 0);
	wmWindow *win;
	bScreen *screen = CTX_wm_screen(C); /* XXX from file read hrmf */
	WorkSpace *workspace;
	WorkSpaceLayout *layout = BKE_workspace_layout_find_global(bmain, screen, &workspace);

	CTX_wm_manager_set(C, wm);
	win = wm_window_new(C, NULL);
	win->scene = CTX_data_scene(C);
	STRNCPY(win->view_layer_name, CTX_data_view_layer(C)->name);
	BKE_workspace_active_set(win->workspace_hook, workspace);
	BKE_workspace_hook_layout_for_workspace_set(win->workspace_hook, workspace, layout);
	screen->winid = win->winid;

	wm->winactive = win;
	wm->file_saved = 1;
	wm_window_make_drawable(wm, win);
}


/* context is allowed to be NULL, do not free wm itself (library.c) */
void wm_close_and_free(bContext *C, wmWindowManager *wm)
{
	wmWindow *win;
	wmOperator *op;
	wmKeyConfig *keyconf;

	if (wm->autosavetimer)
		wm_autosave_timer_ended(wm);

	while ((win = BLI_pophead(&wm->windows))) {
		/* prevent draw clear to use screen */
		BKE_workspace_active_set(win->workspace_hook, NULL);
		wm_window_free(C, wm, win);
	}

	while ((op = BLI_pophead(&wm->operators))) {
		WM_operator_free(op);
	}

	while ((keyconf = BLI_pophead(&wm->keyconfigs))) {
		WM_keyconfig_free(keyconf);
	}

	BLI_freelistN(&wm->queue);

	if (wm->message_bus != NULL) {
		WM_msgbus_destroy(wm->message_bus);
	}

	BLI_freelistN(&wm->paintcursors);

	WM_drag_free_list(&wm->drags);

	wm_reports_free(wm);

	if (wm->undo_stack) {
		BKE_undosys_stack_destroy(wm->undo_stack);
		wm->undo_stack = NULL;
	}

	if (C && CTX_wm_manager(C) == wm) CTX_wm_manager_set(C, NULL);
}

void wm_close_and_free_all(bContext *C, ListBase *wmlist)
{
	wmWindowManager *wm;

	while ((wm = wmlist->first)) {
		wm_close_and_free(C, wm);
		BLI_remlink(wmlist, wm);
		BKE_libblock_free_data(&wm->id, true);
		MEM_freeN(wm);
	}
}

void WM_main(bContext *C)
{
	/* Single refresh before handling events.
	 * This ensures we don't run operators before the depsgraph has been evaluated. */
	wm_event_do_refresh_wm_and_depsgraph(C);

	while (1) {

		/* get events from ghost, handle window events, add to window queues */
		wm_window_process_events(C);

		/* per window, all events to the window, screen, area and region handlers */
		wm_event_do_handlers(C);

		/* events have left notes about changes, we handle and cache it */
		wm_event_do_notifiers(C);

		/* execute cached changes draw */
		wm_draw_update(C);
	}
}
