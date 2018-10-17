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
 * The Original Code is Copyright (C) 2004 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/undo/ed_undo.c
 *  \ingroup edundo
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_callbacks.h"
#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "BKE_blender_undo.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_screen.h"
#include "BKE_undo_system.h"

#include "BLO_runtime.h"

#include "ED_gpencil.h"
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_undo.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

/** We only need this locally. */
static CLG_LogRef LOG = {"ed.undo"};

/* -------------------------------------------------------------------- */
/** \name Generic Undo System Access
 *
 * Non-operator undo editor functions.
 * \{ */

void ED_undo_push(bContext *C, const char *str)
{
	CLOG_INFO(&LOG, 1, "name='%s'", str);

	const int steps = U.undosteps;

	if (steps <= 0) {
		return;
	}

	wmWindowManager *wm = CTX_wm_manager(C);

	/* Only apply limit if this is the last undo step. */
	if (wm->undo_stack->step_active && (wm->undo_stack->step_active->next == NULL)) {
		BKE_undosys_stack_limit_steps_and_memory(wm->undo_stack, steps - 1, 0);
	}

	BKE_undosys_step_push(wm->undo_stack, C, str);

	if (U.undomemory != 0) {
		const size_t memory_limit = (size_t)U.undomemory * 1024 * 1024;
		BKE_undosys_stack_limit_steps_and_memory(wm->undo_stack, 0, memory_limit);
	}

	WM_file_tag_modified();
}

/* note: also check undo_history_exec() in bottom if you change notifiers */
static int ed_undo_step(bContext *C, int step, const char *undoname, ReportList *reports)
{
	CLOG_INFO(&LOG, 1, "name='%s', step=%d", undoname, step);
	wmWindowManager *wm = CTX_wm_manager(C);
	Scene *scene = CTX_data_scene(C);

	/* undo during jobs are running can easily lead to freeing data using by jobs,
	 * or they can just lead to freezing job in some other cases */
	WM_jobs_kill_all(wm);

	if (G.debug & G_DEBUG_IO) {
		Main *bmain = CTX_data_main(C);
		if (bmain->lock != NULL) {
			BKE_report(reports, RPT_INFO, "Checking sanity of current .blend file *BEFORE* undo step.");
			BLO_main_validate_libraries(bmain, reports);
		}
	}

	/* TODO(campbell): undo_system: use undo system */
	/* grease pencil can be can be used in plenty of spaces, so check it first */
	if (ED_gpencil_session_active()) {
		return ED_undo_gpencil_step(C, step, undoname);
	}

	UndoStep *step_data_from_name = NULL;
	int step_for_callback = step;
	if (undoname != NULL) {
		step_data_from_name = BKE_undosys_step_find_by_name(wm->undo_stack, undoname);
		if (step_data_from_name == NULL) {
			return OPERATOR_CANCELLED;
		}

		/* TODO(campbell), could use simple optimization. */
		/* Pointers match on redo. */
		step_for_callback = (
		        BLI_findindex(&wm->undo_stack->steps, step_data_from_name) <
		        BLI_findindex(&wm->undo_stack->steps, wm->undo_stack->step_active)) ? 1 : -1;
	}

	/* App-Handlers (pre). */
	{
		/* Note: ignore grease pencil for now. */
		Main *bmain = CTX_data_main(C);
		wm->op_undo_depth++;
		BLI_callback_exec(bmain, &scene->id, (step_for_callback > 0) ? BLI_CB_EVT_UNDO_PRE : BLI_CB_EVT_REDO_PRE);
		wm->op_undo_depth--;
	}


	/* Undo System */
	{
		if (undoname) {
			BKE_undosys_step_undo_with_data(wm->undo_stack, C, step_data_from_name);
		}
		else {
			BKE_undosys_step_undo_compat_only(wm->undo_stack, C, step);
		}
	}

	/* App-Handlers (post). */
	{
		Main *bmain = CTX_data_main(C);
		scene = CTX_data_scene(C);
		wm->op_undo_depth++;
		BLI_callback_exec(bmain, &scene->id, step_for_callback > 0 ? BLI_CB_EVT_UNDO_PRE : BLI_CB_EVT_REDO_PRE);
		wm->op_undo_depth--;
	}

	if (G.debug & G_DEBUG_IO) {
		Main *bmain = CTX_data_main(C);
		if (bmain->lock != NULL) {
			BKE_report(reports, RPT_INFO, "Checking sanity of current .blend file *AFTER* undo step.");
			BLO_main_validate_libraries(bmain, reports);
		}
	}

	WM_event_add_notifier(C, NC_WINDOW, NULL);
	WM_event_add_notifier(C, NC_WM | ND_UNDO, NULL);

	return OPERATOR_FINISHED;
}

void ED_undo_grouped_push(bContext *C, const char *str)
{
	/* do nothing if previous undo task is the same as this one (or from the same undo group) */
	wmWindowManager *wm = CTX_wm_manager(C);
	const UndoStep *us = wm->undo_stack->step_active;
	if (us && STREQ(str, us->name)) {
		BKE_undosys_stack_clear_active(wm->undo_stack);
	}

	/* push as usual */
	ED_undo_push(C, str);
}

void ED_undo_pop(bContext *C)
{
	ed_undo_step(C, 1, NULL, NULL);
}
void ED_undo_redo(bContext *C)
{
	ed_undo_step(C, -1, NULL, NULL);
}

void ED_undo_push_op(bContext *C, wmOperator *op)
{
	/* in future, get undo string info? */
	ED_undo_push(C, op->type->name);
}

void ED_undo_grouped_push_op(bContext *C, wmOperator *op)
{
	if (op->type->undo_group[0] != '\0') {
		ED_undo_grouped_push(C, op->type->undo_group);
	}
	else {
		ED_undo_grouped_push(C, op->type->name);
	}
}

void ED_undo_pop_op(bContext *C, wmOperator *op)
{
	/* search back a couple of undo's, in case something else added pushes */
	ed_undo_step(C, 0, op->type->name, op->reports);
}

/* name optionally, function used to check for operator redo panel */
bool ED_undo_is_valid(const bContext *C, const char *undoname)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	return BKE_undosys_stack_has_undo(wm->undo_stack, undoname);
}

/**
 * Ideally we wont access the stack directly,
 * this is needed for modes which handle undo themselves (bypassing #ED_undo_push).
 *
 * Using global isn't great, this just avoids doing inline,
 * causing 'BKE_global.h' & 'BKE_main.h' includes.
 */
UndoStack *ED_undo_stack_get(void)
{
	wmWindowManager *wm = G_MAIN->wm.first;
	return wm->undo_stack;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Undo, Undo Push & Redo Operators
 * \{ */

static int ed_undo_exec(bContext *C, wmOperator *op)
{
	/* "last operator" should disappear, later we can tie this with undo stack nicer */
	WM_operator_stack_clear(CTX_wm_manager(C));
	int ret = ed_undo_step(C, 1, NULL, op->reports);
	if (ret & OPERATOR_FINISHED) {
		/* Keep button under the cursor active. */
		WM_event_add_mousemove(C);
	}
	return ret;
}

static int ed_undo_push_exec(bContext *C, wmOperator *op)
{
	char str[BKE_UNDO_STR_MAX];
	RNA_string_get(op->ptr, "message", str);
	ED_undo_push(C, str);
	return OPERATOR_FINISHED;
}

static int ed_redo_exec(bContext *C, wmOperator *op)
{
	int ret = ed_undo_step(C, -1, NULL, op->reports);
	if (ret & OPERATOR_FINISHED) {
		/* Keep button under the cursor active. */
		WM_event_add_mousemove(C);
	}
	return ret;
}

static int ed_undo_redo_exec(bContext *C, wmOperator *UNUSED(op))
{
	wmOperator *last_op = WM_operator_last_redo(C);
	int ret = ED_undo_operator_repeat(C, last_op);
	ret = ret ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
	if (ret & OPERATOR_FINISHED) {
		/* Keep button under the cursor active. */
		WM_event_add_mousemove(C);
	}
	return ret;
}

static bool ed_undo_redo_poll(bContext *C)
{
	wmOperator *last_op = WM_operator_last_redo(C);
	return last_op && ED_operator_screenactive(C) &&
		WM_operator_check_ui_enabled(C, last_op->type->name);
}

void ED_OT_undo(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Undo";
	ot->description = "Undo previous action";
	ot->idname = "ED_OT_undo";

	/* api callbacks */
	ot->exec = ed_undo_exec;
	ot->poll = ED_operator_screenactive;
}

void ED_OT_undo_push(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Undo Push";
	ot->description = "Add an undo state (internal use only)";
	ot->idname = "ED_OT_undo_push";

	/* api callbacks */
	ot->exec = ed_undo_push_exec;

	ot->flag = OPTYPE_INTERNAL;

	RNA_def_string(ot->srna, "message", "Add an undo step *function may be moved*", BKE_UNDO_STR_MAX, "Undo Message", "");
}

void ED_OT_redo(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Redo";
	ot->description = "Redo previous action";
	ot->idname = "ED_OT_redo";

	/* api callbacks */
	ot->exec = ed_redo_exec;
	ot->poll = ED_operator_screenactive;
}

void ED_OT_undo_redo(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Undo and Redo";
	ot->description = "Undo and redo previous action";
	ot->idname = "ED_OT_undo_redo";

	/* api callbacks */
	ot->exec = ed_undo_redo_exec;
	ot->poll = ed_undo_redo_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Repeat
 * \{ */

/* ui callbacks should call this rather than calling WM_operator_repeat() themselves */
int ED_undo_operator_repeat(bContext *C, struct wmOperator *op)
{
	int ret = 0;

	if (op) {
		CLOG_INFO(&LOG, 1, "idname='%s'", op->type->idname);
		wmWindowManager *wm = CTX_wm_manager(C);
		struct Scene *scene = CTX_data_scene(C);

		/* keep in sync with logic in view3d_panel_operator_redo() */
		ARegion *ar = CTX_wm_region(C);
		ARegion *ar1 = BKE_area_find_region_active_win(CTX_wm_area(C));

		if (ar1)
			CTX_wm_region_set(C, ar1);

		if ((WM_operator_repeat_check(C, op)) &&
		    (WM_operator_poll(C, op->type)) &&
		     /* note, undo/redo cant run if there are jobs active,
		      * check for screen jobs only so jobs like material/texture/world preview
		      * (which copy their data), wont stop redo, see [#29579]],
		      *
		      * note, - WM_operator_check_ui_enabled() jobs test _must_ stay in sync with this */
		    (WM_jobs_test(wm, scene, WM_JOB_TYPE_ANY) == 0))
		{
			int retval;

			ED_viewport_render_kill_jobs(wm, CTX_data_main(C), true);

			if (G.debug & G_DEBUG)
				printf("redo_cb: operator redo %s\n", op->type->name);

			WM_operator_free_all_after(wm, op);

			ED_undo_pop_op(C, op);

			if (op->type->check) {
				if (op->type->check(C, op)) {
					/* check for popup and re-layout buttons */
					ARegion *ar_menu = CTX_wm_menu(C);
					if (ar_menu) {
						ED_region_tag_refresh_ui(ar_menu);
					}
				}
			}

			retval = WM_operator_repeat(C, op);
			if ((retval & OPERATOR_FINISHED) == 0) {
				if (G.debug & G_DEBUG)
					printf("redo_cb: operator redo failed: %s, return %d\n", op->type->name, retval);
				ED_undo_redo(C);
			}
			else {
				ret = 1;
			}
		}
		else {
			if (G.debug & G_DEBUG) {
				printf("redo_cb: WM_operator_repeat_check returned false %s\n", op->type->name);
			}
		}

		/* set region back */
		CTX_wm_region_set(C, ar);
	}
	else {
		CLOG_WARN(&LOG, "called with NULL 'op'");
	}

	return ret;
}


void ED_undo_operator_repeat_cb(bContext *C, void *arg_op, void *UNUSED(arg_unused))
{
	ED_undo_operator_repeat(C, (wmOperator *)arg_op);
}

void ED_undo_operator_repeat_cb_evt(bContext *C, void *arg_op, int UNUSED(arg_event))
{
	ED_undo_operator_repeat(C, (wmOperator *)arg_op);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Undo History Operator
 * \{ */

/* create enum based on undo items */
static const EnumPropertyItem *rna_undo_itemf(bContext *C, int *totitem)
{
	EnumPropertyItem item_tmp = {0}, *item = NULL;
	int i = 0;

	wmWindowManager *wm = CTX_wm_manager(C);
	if (wm->undo_stack == NULL) {
		return NULL;
	}

	for (UndoStep *us = wm->undo_stack->steps.first; us; us = us->next, i++) {
		if (us->skip == false) {
			item_tmp.identifier = us->name;
			item_tmp.name = IFACE_(us->name);
			if (us == wm->undo_stack->step_active) {
				item_tmp.icon = ICON_RESTRICT_VIEW_OFF;
			}
			else {
				item_tmp.icon = ICON_NONE;
			}
			item_tmp.value = i;
			RNA_enum_item_add(&item, totitem, &item_tmp);
		}
	}
	RNA_enum_item_end(&item, totitem);

	return item;
}


static int undo_history_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	int totitem = 0;

	{
		const EnumPropertyItem *item = rna_undo_itemf(C, &totitem);

		if (totitem > 0) {
			uiPopupMenu *pup = UI_popup_menu_begin(C, RNA_struct_ui_name(op->type->srna), ICON_NONE);
			uiLayout *layout = UI_popup_menu_layout(pup);
			uiLayout *split = uiLayoutSplit(layout, 0.0f, false);
			uiLayout *column = NULL;
			const int col_size = 20 + totitem / 12;
			int i, c;
			bool add_col = true;

			for (c = 0, i = totitem; i--;) {
				if (add_col && !(c % col_size)) {
					column = uiLayoutColumn(split, false);
					add_col = false;
				}
				if (item[i].identifier) {
					uiItemIntO(column, item[i].name, item[i].icon, op->type->idname, "item", item[i].value);
					++c;
					add_col = true;
				}
			}

			MEM_freeN((void *)item);

			UI_popup_menu_end(C, pup);
		}

	}
	return OPERATOR_CANCELLED;
}

/* note: also check ed_undo_step() in top if you change notifiers */
static int undo_history_exec(bContext *C, wmOperator *op)
{
	PropertyRNA *prop = RNA_struct_find_property(op->ptr, "item");
	if (RNA_property_is_set(op->ptr, prop)) {
		int item = RNA_property_int_get(op->ptr, prop);
		wmWindowManager *wm = CTX_wm_manager(C);
		BKE_undosys_step_undo_from_index(wm->undo_stack, C, item);
		WM_event_add_notifier(C, NC_WINDOW, NULL);
		return OPERATOR_FINISHED;
	}
	return OPERATOR_CANCELLED;
}

void ED_OT_undo_history(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Undo History";
	ot->description = "Redo specific action in history";
	ot->idname = "ED_OT_undo_history";

	/* api callbacks */
	ot->invoke = undo_history_invoke;
	ot->exec = undo_history_exec;
	ot->poll = ED_operator_screenactive;

	RNA_def_int(ot->srna, "item", 0, 0, INT_MAX, "Item", "", 0, INT_MAX);

}

/** \} */
