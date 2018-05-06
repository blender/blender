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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/manipulators/intern/wm_manipulator_group.c
 *  \ingroup wm
 *
 * \name Manipulator-Group
 *
 * Manipulator-groups store and manage groups of manipulators. They can be
 * attached to modal handlers and have own keymaps.
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_workspace.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_event_system.h"

#include "ED_screen.h"
#include "ED_undo.h"

/* own includes */
#include "wm_manipulator_wmapi.h"
#include "wm_manipulator_intern.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

/* Allow manipulator part's to be single click only,
 * dragging falls back to activating their 'drag_part' action. */
#define USE_DRAG_DETECT

/* -------------------------------------------------------------------- */
/** \name wmManipulatorGroup
 *
 * \{ */

/**
 * Create a new manipulator-group from \a wgt.
 */
wmManipulatorGroup *wm_manipulatorgroup_new_from_type(
        wmManipulatorMap *mmap, wmManipulatorGroupType *wgt)
{
	wmManipulatorGroup *mgroup = MEM_callocN(sizeof(*mgroup), "manipulator-group");
	mgroup->type = wgt;

	/* keep back-link */
	mgroup->parent_mmap = mmap;

	BLI_addtail(&mmap->groups, mgroup);

	return mgroup;
}

void wm_manipulatorgroup_free(bContext *C, wmManipulatorGroup *mgroup)
{
	wmManipulatorMap *mmap = mgroup->parent_mmap;

	/* Similar to WM_manipulator_unlink, but only to keep mmap state correct,
	 * we don't want to run callbacks. */
	if (mmap->mmap_context.highlight && mmap->mmap_context.highlight->parent_mgroup == mgroup) {
		wm_manipulatormap_highlight_set(mmap, C, NULL, 0);
	}
	if (mmap->mmap_context.modal && mmap->mmap_context.modal->parent_mgroup == mgroup) {
		wm_manipulatormap_modal_set(mmap, C, mmap->mmap_context.modal, NULL, false);
	}

	for (wmManipulator *mpr = mgroup->manipulators.first, *mpr_next; mpr; mpr = mpr_next) {
		mpr_next = mpr->next;
		if (mmap->mmap_context.select.len) {
			WM_manipulator_select_unlink(mmap, mpr);
		}
		WM_manipulator_free(mpr);
	}
	BLI_listbase_clear(&mgroup->manipulators);

#ifdef WITH_PYTHON
	if (mgroup->py_instance) {
		/* do this first in case there are any __del__ functions or
		 * similar that use properties */
		BPY_DECREF_RNA_INVALIDATE(mgroup->py_instance);
	}
#endif

	if (mgroup->reports && (mgroup->reports->flag & RPT_FREE)) {
		BKE_reports_clear(mgroup->reports);
		MEM_freeN(mgroup->reports);
	}

	if (mgroup->customdata_free) {
		mgroup->customdata_free(mgroup->customdata);
	}
	else {
		MEM_SAFE_FREE(mgroup->customdata);
	}

	BLI_remlink(&mmap->groups, mgroup);

	MEM_freeN(mgroup);
}

/**
 * Add \a manipulator to \a mgroup and make sure its name is unique within the group.
 */
void wm_manipulatorgroup_manipulator_register(wmManipulatorGroup *mgroup, wmManipulator *mpr)
{
	BLI_assert(BLI_findindex(&mgroup->manipulators, mpr) == -1);
	BLI_addtail(&mgroup->manipulators, mpr);
	mpr->parent_mgroup = mgroup;
}

wmManipulator *wm_manipulatorgroup_find_intersected_manipulator(
        const wmManipulatorGroup *mgroup, bContext *C, const wmEvent *event,
        int *r_part)
{
	for (wmManipulator *mpr = mgroup->manipulators.first; mpr; mpr = mpr->next) {
		if (mpr->type->test_select && (mpr->flag & WM_MANIPULATOR_HIDDEN) == 0) {
			if ((*r_part = mpr->type->test_select(C, mpr, event)) != -1) {
				return mpr;
			}
		}
	}

	return NULL;
}

/**
 * Adds all manipulators of \a mgroup that can be selected to the head of \a listbase. Added items need freeing!
 */
void wm_manipulatorgroup_intersectable_manipulators_to_list(const wmManipulatorGroup *mgroup, ListBase *listbase)
{
	for (wmManipulator *mpr = mgroup->manipulators.first; mpr; mpr = mpr->next) {
		if ((mpr->flag & WM_MANIPULATOR_HIDDEN) == 0) {
			if (((mgroup->type->flag & WM_MANIPULATORGROUPTYPE_3D) && mpr->type->draw_select) ||
			    ((mgroup->type->flag & WM_MANIPULATORGROUPTYPE_3D) == 0 && mpr->type->test_select))
			{
				BLI_addhead(listbase, BLI_genericNodeN(mpr));
			}
		}
	}
}

void wm_manipulatorgroup_ensure_initialized(wmManipulatorGroup *mgroup, const bContext *C)
{
	/* prepare for first draw */
	if (UNLIKELY((mgroup->init_flag & WM_MANIPULATORGROUP_INIT_SETUP) == 0)) {
		mgroup->type->setup(C, mgroup);

		/* Not ideal, initialize keymap here, needed for RNA runtime generated manipulators. */
		wmManipulatorGroupType *wgt = mgroup->type;
		if (wgt->keymap == NULL) {
			wmWindowManager *wm = CTX_wm_manager(C);
			wm_manipulatorgrouptype_setup_keymap(wgt, wm->defaultconf);
			BLI_assert(wgt->keymap != NULL);
		}
		mgroup->init_flag |= WM_MANIPULATORGROUP_INIT_SETUP;
	}

	/* refresh may be called multiple times, this just ensures its called at least once before we draw. */
	if (UNLIKELY((mgroup->init_flag & WM_MANIPULATORGROUP_INIT_REFRESH) == 0)) {
		if (mgroup->type->refresh) {
			mgroup->type->refresh(C, mgroup);
		}
		mgroup->init_flag |= WM_MANIPULATORGROUP_INIT_REFRESH;
	}
}

bool WM_manipulator_group_type_poll(const bContext *C, const struct wmManipulatorGroupType *wgt)
{
	/* If we're tagged, only use compatible. */
	if (wgt->owner_id[0] != '\0') {
		const WorkSpace *workspace = CTX_wm_workspace(C);
		if (BKE_workspace_owner_id_check(workspace, wgt->owner_id) == false) {
			return false;
		}
	}
	/* Check for poll function, if manipulator-group belongs to an operator, also check if the operator is running. */
	return (!wgt->poll || wgt->poll(C, (wmManipulatorGroupType *)wgt));
}

bool wm_manipulatorgroup_is_visible_in_drawstep(
        const wmManipulatorGroup *mgroup, const eWM_ManipulatorMapDrawStep drawstep)
{
	switch (drawstep) {
		case WM_MANIPULATORMAP_DRAWSTEP_2D:
			return (mgroup->type->flag & WM_MANIPULATORGROUPTYPE_3D) == 0;
		case WM_MANIPULATORMAP_DRAWSTEP_3D:
			return (mgroup->type->flag & WM_MANIPULATORGROUPTYPE_3D);
		default:
			BLI_assert(0);
			return false;
	}
}

bool wm_manipulatorgroup_is_any_selected(const wmManipulatorGroup *mgroup)
{
	if (mgroup->type->flag & WM_MANIPULATORGROUPTYPE_SELECT) {
		for (const wmManipulator *mpr = mgroup->manipulators.first; mpr; mpr = mpr->next) {
			if (mpr->state & WM_MANIPULATOR_STATE_SELECT) {
				return true;
			}
		}
	}
	return false;
}

/** \} */

/** \name Manipulator operators
 *
 * Basic operators for manipulator interaction with user configurable keymaps.
 *
 * \{ */

static int manipulator_select_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	ARegion *ar = CTX_wm_region(C);
	wmManipulatorMap *mmap = ar->manipulator_map;
	wmManipulatorMapSelectState *msel = &mmap->mmap_context.select;
	wmManipulator *highlight = mmap->mmap_context.highlight;

	bool extend = RNA_boolean_get(op->ptr, "extend");
	bool deselect = RNA_boolean_get(op->ptr, "deselect");
	bool toggle = RNA_boolean_get(op->ptr, "toggle");

	/* deselect all first */
	if (extend == false && deselect == false && toggle == false) {
		wm_manipulatormap_deselect_all(mmap);
		BLI_assert(msel->items == NULL && msel->len == 0);
		UNUSED_VARS_NDEBUG(msel);
	}

	if (highlight) {
		const bool is_selected = (highlight->state & WM_MANIPULATOR_STATE_SELECT);
		bool redraw = false;

		if (toggle) {
			/* toggle: deselect if already selected, else select */
			deselect = is_selected;
		}

		if (deselect) {
			if (is_selected && WM_manipulator_select_set(mmap, highlight, false)) {
				redraw = true;
			}
		}
		else if (wm_manipulator_select_and_highlight(C, mmap, highlight)) {
			redraw = true;
		}

		if (redraw) {
			ED_region_tag_redraw(ar);
		}

		return OPERATOR_FINISHED;
	}
	else {
		BLI_assert(0);
		return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
	}

	return OPERATOR_PASS_THROUGH;
}

void MANIPULATORGROUP_OT_manipulator_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Manipulator Select";
	ot->description = "Select the currently highlighted manipulator";
	ot->idname = "MANIPULATORGROUP_OT_manipulator_select";

	/* api callbacks */
	ot->invoke = manipulator_select_invoke;

	ot->flag = OPTYPE_UNDO;

	WM_operator_properties_mouse_select(ot);
}

typedef struct ManipulatorTweakData {
	wmManipulatorMap *mmap;
	wmManipulatorGroup *mgroup;
	wmManipulator *mpr_modal;

	int init_event; /* initial event type */
	int flag;       /* tweak flags */

#ifdef USE_DRAG_DETECT
	/* True until the mouse is moved (only use when the operator has no modal).
	 * this allows some manipulators to be click-only. */
	enum {
		/* Don't detect dragging. */
		DRAG_NOP = 0,
		/* Detect dragging (wait until a drag or click is detected). */
		DRAG_DETECT,
		/* Drag has started, idle until there is no active modal operator.
		 * This is needed because finishing the modal operator also exits
		 * the modal manipulator state (un-grabbs the cursor).
		 * Ideally this workaround could be removed later. */
		DRAG_IDLE,
	} drag_state;
#endif

} ManipulatorTweakData;

static bool manipulator_tweak_start(
        bContext *C, wmManipulatorMap *mmap, wmManipulator *mpr, const wmEvent *event)
{
	/* activate highlighted manipulator */
	wm_manipulatormap_modal_set(mmap, C, mpr, event, true);

	return (mpr->state & WM_MANIPULATOR_STATE_MODAL);
}

static bool manipulator_tweak_start_and_finish(
        bContext *C, wmManipulatorMap *mmap, wmManipulator *mpr, const wmEvent *event, bool *r_is_modal)
{
	wmManipulatorOpElem *mpop = WM_manipulator_operator_get(mpr, mpr->highlight_part);
	if (r_is_modal) {
		*r_is_modal = false;
	}
	if (mpop && mpop->type) {

		/* Undo/Redo */
		if (mpop->is_redo) {
			wmWindowManager *wm = CTX_wm_manager(C);
			wmOperator *op = WM_operator_last_redo(C);

			/* We may want to enable this, for now the manipulator can manage it's own properties. */
#if 0
			IDP_MergeGroup(mpop->ptr.data, op->properties, false);
#endif

			WM_operator_free_all_after(wm, op);
			ED_undo_pop_op(C, op);
		}
		
		/* XXX temporary workaround for modal manipulator operator
		 * conflicting with modal operator attached to manipulator */
		if (mpop->type->modal) {
			/* activate highlighted manipulator */
			wm_manipulatormap_modal_set(mmap, C, mpr, event, true);
			if (r_is_modal) {
				*r_is_modal = true;
			}
		}
		else {
			/* Allow for 'button' manipulators, single click to run an action. */
			WM_operator_name_call_ptr(C, mpop->type, WM_OP_INVOKE_DEFAULT, &mpop->ptr);
		}
		return true;
	}
	else {
		return false;
	}
}

static void manipulator_tweak_finish(bContext *C, wmOperator *op, const bool cancel, bool clear_modal)
{
	ManipulatorTweakData *mtweak = op->customdata;
	if (mtweak->mpr_modal->type->exit) {
		mtweak->mpr_modal->type->exit(C, mtweak->mpr_modal, cancel);
	}
	if (clear_modal) {
		/* The manipulator may have been removed. */
		if ((BLI_findindex(&mtweak->mmap->groups, mtweak->mgroup) != -1) &&
		    (BLI_findindex(&mtweak->mgroup->manipulators, mtweak->mpr_modal) != -1))
		{
			wm_manipulatormap_modal_set(mtweak->mmap, C, mtweak->mpr_modal, NULL, false);
		}
	}
	MEM_freeN(mtweak);
}

static int manipulator_tweak_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	ManipulatorTweakData *mtweak = op->customdata;
	wmManipulator *mpr = mtweak->mpr_modal;
	int retval = OPERATOR_PASS_THROUGH;
	bool clear_modal = true;

	if (mpr == NULL) {
		BLI_assert(0);
		return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
	}

#ifdef USE_DRAG_DETECT
	wmManipulatorMap *mmap = mtweak->mmap;
	if (mtweak->drag_state == DRAG_DETECT) {
		if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
			if (len_manhattan_v2v2_int(&event->x, mmap->mmap_context.event_xy) > 2) {
				mtweak->drag_state = DRAG_IDLE;
				mpr->highlight_part = mpr->drag_part;
			}
		}
		else if (event->type == mtweak->init_event && event->val == KM_RELEASE) {
			mtweak->drag_state = DRAG_NOP;
			retval = OPERATOR_FINISHED;
		}

		if (mtweak->drag_state != DRAG_DETECT) {
			/* Follow logic in 'manipulator_tweak_invoke' */
			bool is_modal = false;
			if (manipulator_tweak_start_and_finish(C, mmap, mpr, event, &is_modal)) {
				if (is_modal) {
					clear_modal = false;
				}
			}
			else {
				if (!manipulator_tweak_start(C, mmap, mpr, event)) {
					retval = OPERATOR_FINISHED;
				}
			}
		}
	}
	if (mtweak->drag_state == DRAG_IDLE) {
		if (mmap->mmap_context.modal != NULL) {
			return OPERATOR_PASS_THROUGH;
		}
		else {
			manipulator_tweak_finish(C, op, false, false);
			return OPERATOR_FINISHED;
		}
	}
#endif  /* USE_DRAG_DETECT */

	if (retval == OPERATOR_FINISHED) {
		/* pass */
	}
	else if (event->type == mtweak->init_event && event->val == KM_RELEASE) {
		retval = OPERATOR_FINISHED;
	}
	else if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case TWEAK_MODAL_CANCEL:
				retval = OPERATOR_CANCELLED;
				break;
			case TWEAK_MODAL_CONFIRM:
				retval = OPERATOR_FINISHED;
				break;
			case TWEAK_MODAL_PRECISION_ON:
				mtweak->flag |= WM_MANIPULATOR_TWEAK_PRECISE;
				break;
			case TWEAK_MODAL_PRECISION_OFF:
				mtweak->flag &= ~WM_MANIPULATOR_TWEAK_PRECISE;
				break;

			case TWEAK_MODAL_SNAP_ON:
				mtweak->flag |= WM_MANIPULATOR_TWEAK_SNAP;
				break;
			case TWEAK_MODAL_SNAP_OFF:
				mtweak->flag &= ~WM_MANIPULATOR_TWEAK_SNAP;
				break;
		}
	}

	if (retval != OPERATOR_PASS_THROUGH) {
		manipulator_tweak_finish(C, op, retval != OPERATOR_FINISHED, clear_modal);
		return retval;
	}

	/* handle manipulator */
	wmManipulatorFnModal modal_fn = mpr->custom_modal ? mpr->custom_modal : mpr->type->modal;
	if (modal_fn) {
		int modal_retval = modal_fn(C, mpr, event, mtweak->flag);

		if ((modal_retval & OPERATOR_RUNNING_MODAL) == 0) {
			manipulator_tweak_finish(C, op, (modal_retval & OPERATOR_CANCELLED) != 0, true);
			return OPERATOR_FINISHED;
		}

		/* Ugly hack to send manipulator events */
		((wmEvent *)event)->type = EVT_MANIPULATOR_UPDATE;
	}

	/* always return PASS_THROUGH so modal handlers
	 * with manipulators attached can update */
	BLI_assert(retval == OPERATOR_PASS_THROUGH);
	return OPERATOR_PASS_THROUGH;
}

static int manipulator_tweak_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	wmManipulatorMap *mmap = ar->manipulator_map;
	wmManipulator *mpr = mmap->mmap_context.highlight;

	/* Needed for single click actions which don't enter modal state. */
	WM_tooltip_clear(C, CTX_wm_window(C));

	if (!mpr) {
		/* wm_handlers_do_intern shouldn't let this happen */
		BLI_assert(0);
		return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
	}

	bool use_drag_fallback = false;

#ifdef USE_DRAG_DETECT
	use_drag_fallback = !ELEM(mpr->drag_part, -1, mpr->highlight_part);
#endif

	if (use_drag_fallback == false) {
		if (manipulator_tweak_start_and_finish(C, mmap, mpr, event, NULL)) {
			return OPERATOR_FINISHED;
		}
	}

	bool use_drag_detect = false;
#ifdef USE_DRAG_DETECT
	if (use_drag_fallback) {
		wmManipulatorOpElem *mpop = WM_manipulator_operator_get(mpr, mpr->highlight_part);
		if (mpop && mpop->type) {
			if (mpop->type->modal == NULL) {
				use_drag_detect = true;
			}
		}
	}
#endif

	if (use_drag_detect == false) {
		if (!manipulator_tweak_start(C, mmap, mpr, event)) {
			/* failed to start */
			return OPERATOR_PASS_THROUGH;
		}
	}

	ManipulatorTweakData *mtweak = MEM_mallocN(sizeof(ManipulatorTweakData), __func__);

	mtweak->init_event = WM_userdef_event_type_from_keymap_type(event->type);
	mtweak->mpr_modal = mmap->mmap_context.highlight;
	mtweak->mgroup = mtweak->mpr_modal->parent_mgroup;
	mtweak->mmap = mmap;
	mtweak->flag = 0;

#ifdef USE_DRAG_DETECT
	mtweak->drag_state = use_drag_detect ? DRAG_DETECT : DRAG_NOP;
#endif

	op->customdata = mtweak;

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

void MANIPULATORGROUP_OT_manipulator_tweak(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Manipulator Tweak";
	ot->description = "Tweak the active manipulator";
	ot->idname = "MANIPULATORGROUP_OT_manipulator_tweak";

	/* api callbacks */
	ot->invoke = manipulator_tweak_invoke;
	ot->modal = manipulator_tweak_modal;

	/* TODO(campbell) This causes problems tweaking settings for operators,
	 * need to find a way to support this. */
#if 0
	ot->flag = OPTYPE_UNDO;
#endif
}

/** \} */ // Manipulator operators


static wmKeyMap *manipulatorgroup_tweak_modal_keymap(wmKeyConfig *keyconf, const char *mgroupname)
{
	wmKeyMap *keymap;
	char name[KMAP_MAX_NAME];

	static EnumPropertyItem modal_items[] = {
		{TWEAK_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
		{TWEAK_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
		{TWEAK_MODAL_PRECISION_ON, "PRECISION_ON", 0, "Enable Precision", ""},
		{TWEAK_MODAL_PRECISION_OFF, "PRECISION_OFF", 0, "Disable Precision", ""},
		{TWEAK_MODAL_SNAP_ON, "SNAP_ON", 0, "Enable Snap", ""},
		{TWEAK_MODAL_SNAP_OFF, "SNAP_OFF", 0, "Disable Snap", ""},
		{0, NULL, 0, NULL, NULL}
	};


	BLI_snprintf(name, sizeof(name), "%s Tweak Modal Map", mgroupname);
	keymap = WM_modalkeymap_get(keyconf, name);

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items)
		return NULL;

	keymap = WM_modalkeymap_add(keyconf, name, modal_items);


	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_CANCEL);

	WM_modalkeymap_add_item(keymap, RETKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, PADENTER, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_CONFIRM);

	WM_modalkeymap_add_item(keymap, RIGHTSHIFTKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_PRECISION_ON);
	WM_modalkeymap_add_item(keymap, RIGHTSHIFTKEY, KM_RELEASE, KM_ANY, 0, TWEAK_MODAL_PRECISION_OFF);
	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_PRECISION_ON);
	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_RELEASE, KM_ANY, 0, TWEAK_MODAL_PRECISION_OFF);

	WM_modalkeymap_add_item(keymap, RIGHTCTRLKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_SNAP_ON);
	WM_modalkeymap_add_item(keymap, RIGHTCTRLKEY, KM_RELEASE, KM_ANY, 0, TWEAK_MODAL_SNAP_OFF);
	WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_SNAP_ON);
	WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_RELEASE, KM_ANY, 0, TWEAK_MODAL_SNAP_OFF);

	WM_modalkeymap_assign(keymap, "MANIPULATORGROUP_OT_manipulator_tweak");

	return keymap;
}

/**
 * Common default keymap for manipulator groups
 */
wmKeyMap *WM_manipulatorgroup_keymap_common(
        const wmManipulatorGroupType *wgt, wmKeyConfig *config)
{
	/* Use area and region id since we might have multiple manipulators with the same name in different areas/regions */
	wmKeyMap *km = WM_keymap_find(config, wgt->name, wgt->mmap_params.spaceid, wgt->mmap_params.regionid);

	WM_keymap_add_item(km, "MANIPULATORGROUP_OT_manipulator_tweak", LEFTMOUSE, KM_PRESS, KM_ANY, 0);
	manipulatorgroup_tweak_modal_keymap(config, wgt->name);

	return km;
}

/**
 * Variation of #WM_manipulatorgroup_keymap_common but with keymap items for selection
 */
wmKeyMap *WM_manipulatorgroup_keymap_common_select(
        const wmManipulatorGroupType *wgt, wmKeyConfig *config)
{
	/* Use area and region id since we might have multiple manipulators with the same name in different areas/regions */
	wmKeyMap *km = WM_keymap_find(config, wgt->name, wgt->mmap_params.spaceid, wgt->mmap_params.regionid);

	WM_keymap_add_item(km, "MANIPULATORGROUP_OT_manipulator_tweak", ACTIONMOUSE, KM_PRESS, KM_ANY, 0);
	WM_keymap_add_item(km, "MANIPULATORGROUP_OT_manipulator_tweak", EVT_TWEAK_S, KM_ANY, 0, 0);
	manipulatorgroup_tweak_modal_keymap(config, wgt->name);

	wmKeyMapItem *kmi = WM_keymap_add_item(km, "MANIPULATORGROUP_OT_manipulator_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", false);
	kmi = WM_keymap_add_item(km, "MANIPULATORGROUP_OT_manipulator_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", true);

	return km;
}

/** \} */ /* wmManipulatorGroup */

/* -------------------------------------------------------------------- */
/** \name wmManipulatorGroupType
 *
 * \{ */

struct wmManipulatorGroupTypeRef *WM_manipulatormaptype_group_find_ptr(
        struct wmManipulatorMapType *mmap_type,
        const wmManipulatorGroupType *wgt)
{
	/* could use hash lookups as operator types do, for now simple search. */
	for (wmManipulatorGroupTypeRef *wgt_ref = mmap_type->grouptype_refs.first;
	     wgt_ref;
	     wgt_ref = wgt_ref->next)
	{
		if (wgt_ref->type == wgt) {
			return wgt_ref;
		}
	}
	return NULL;
}

struct wmManipulatorGroupTypeRef *WM_manipulatormaptype_group_find(
        struct wmManipulatorMapType *mmap_type,
        const char *idname)
{
	/* could use hash lookups as operator types do, for now simple search. */
	for (wmManipulatorGroupTypeRef *wgt_ref = mmap_type->grouptype_refs.first;
	     wgt_ref;
	     wgt_ref = wgt_ref->next)
	{
		if (STREQ(idname, wgt_ref->type->idname)) {
			return wgt_ref;
		}
	}
	return NULL;
}

/**
 * Use this for registering manipulators on startup. For runtime, use #WM_manipulatormaptype_group_link_runtime.
 */
wmManipulatorGroupTypeRef *WM_manipulatormaptype_group_link(
        wmManipulatorMapType *mmap_type, const char *idname)
{
	wmManipulatorGroupType *wgt = WM_manipulatorgrouptype_find(idname, false);
	BLI_assert(wgt != NULL);
	return WM_manipulatormaptype_group_link_ptr(mmap_type, wgt);
}

wmManipulatorGroupTypeRef *WM_manipulatormaptype_group_link_ptr(
        wmManipulatorMapType *mmap_type, wmManipulatorGroupType *wgt)
{
	wmManipulatorGroupTypeRef *wgt_ref = MEM_callocN(sizeof(wmManipulatorGroupTypeRef), "manipulator-group-ref");
	wgt_ref->type = wgt;
	BLI_addtail(&mmap_type->grouptype_refs, wgt_ref);
	return wgt_ref;
}

void WM_manipulatormaptype_group_init_runtime_keymap(
        const Main *bmain,
        wmManipulatorGroupType *wgt)
{
	/* init keymap - on startup there's an extra call to init keymaps for 'permanent' manipulator-groups */
	wm_manipulatorgrouptype_setup_keymap(wgt, ((wmWindowManager *)bmain->wm.first)->defaultconf);
}

void WM_manipulatormaptype_group_init_runtime(
        const Main *bmain, wmManipulatorMapType *mmap_type,
        wmManipulatorGroupType *wgt)
{
	/* now create a manipulator for all existing areas */
	for (bScreen *sc = bmain->screen.first; sc; sc = sc->id.next) {
		for (ScrArea *sa = sc->areabase.first; sa; sa = sa->next) {
			for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
				ListBase *lb = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;
				for (ARegion *ar = lb->first; ar; ar = ar->next) {
					wmManipulatorMap *mmap = ar->manipulator_map;
					if (mmap && mmap->type == mmap_type) {
						wm_manipulatorgroup_new_from_type(mmap, wgt);

						/* just add here, drawing will occur on next update */
						wm_manipulatormap_highlight_set(mmap, NULL, NULL, 0);
						ED_region_tag_redraw(ar);
					}
				}
			}
		}
	}
}


/**
 * Unlike #WM_manipulatormaptype_group_unlink this doesn't maintain correct state, simply free.
 */
void WM_manipulatormaptype_group_free(wmManipulatorGroupTypeRef *wgt_ref)
{
	MEM_freeN(wgt_ref);
}

void WM_manipulatormaptype_group_unlink(
        bContext *C, Main *bmain, wmManipulatorMapType *mmap_type,
        const wmManipulatorGroupType *wgt)
{
	/* Free instances. */
	for (bScreen *sc = bmain->screen.first; sc; sc = sc->id.next) {
		for (ScrArea *sa = sc->areabase.first; sa; sa = sa->next) {
			for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
				ListBase *lb = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;
				for (ARegion *ar = lb->first; ar; ar = ar->next) {
					wmManipulatorMap *mmap = ar->manipulator_map;
					if (mmap && mmap->type == mmap_type) {
						wmManipulatorGroup *mgroup, *mgroup_next;
						for (mgroup = mmap->groups.first; mgroup; mgroup = mgroup_next) {
							mgroup_next = mgroup->next;
							if (mgroup->type == wgt) {
								BLI_assert(mgroup->parent_mmap == mmap);
								wm_manipulatorgroup_free(C, mgroup);
								ED_region_tag_redraw(ar);
							}
						}
					}
				}
			}
		}
	}

	/* Free types. */
	wmManipulatorGroupTypeRef *wgt_ref = WM_manipulatormaptype_group_find_ptr(mmap_type, wgt);
	if (wgt_ref) {
		BLI_remlink(&mmap_type->grouptype_refs, wgt_ref);
		WM_manipulatormaptype_group_free(wgt_ref);
	}

	/* Note, we may want to keep this keymap for editing */
	WM_keymap_remove(wgt->keyconf, wgt->keymap);

	BLI_assert(WM_manipulatormaptype_group_find_ptr(mmap_type, wgt) == NULL);
}

void wm_manipulatorgrouptype_setup_keymap(
        wmManipulatorGroupType *wgt, wmKeyConfig *keyconf)
{
	/* Use flag since setup_keymap may return NULL,
	 * in that case we better not keep calling it. */
	if (wgt->type_update_flag & WM_MANIPULATORMAPTYPE_KEYMAP_INIT) {
		wgt->keymap = wgt->setup_keymap(wgt, keyconf);
		wgt->keyconf = keyconf;
		wgt->type_update_flag &= ~WM_MANIPULATORMAPTYPE_KEYMAP_INIT;
	}
}

/** \} */ /* wmManipulatorGroupType */

/* -------------------------------------------------------------------- */
/** \name High Level Add/Remove API
 *
 * For use directly from operators & RNA registration.
 *
 * \note In context of manipulator API these names are a bit misleading,
 * but for general use terms its OK.
 * `WM_manipulator_group_type_add` would be more correctly called:
 * `WM_manipulatormaptype_grouptype_reference_link`
 * but for general purpose API this is too detailed & annoying.
 *
 * \note We may want to return a value if there is nothing to remove.
 *
 * \{ */

void WM_manipulator_group_type_add_ptr_ex(
        wmManipulatorGroupType *wgt,
        wmManipulatorMapType *mmap_type)
{
	WM_manipulatormaptype_group_link_ptr(mmap_type, wgt);

	WM_manipulatorconfig_update_tag_init(mmap_type, wgt);
}
void WM_manipulator_group_type_add_ptr(
        wmManipulatorGroupType *wgt)
{
	wmManipulatorMapType *mmap_type = WM_manipulatormaptype_ensure(&wgt->mmap_params);
	WM_manipulator_group_type_add_ptr_ex(wgt, mmap_type);
}
void WM_manipulator_group_type_add(const char *idname)
{
	wmManipulatorGroupType *wgt = WM_manipulatorgrouptype_find(idname, false);
	BLI_assert(wgt != NULL);
	WM_manipulator_group_type_add_ptr(wgt);
}

void WM_manipulator_group_type_ensure_ptr_ex(
        wmManipulatorGroupType *wgt,
        wmManipulatorMapType *mmap_type)
{
	wmManipulatorGroupTypeRef *wgt_ref = WM_manipulatormaptype_group_find_ptr(mmap_type, wgt);
	if (wgt_ref == NULL) {
		WM_manipulator_group_type_add_ptr_ex(wgt, mmap_type);
	}
}
void WM_manipulator_group_type_ensure_ptr(
        wmManipulatorGroupType *wgt)
{
	wmManipulatorMapType *mmap_type = WM_manipulatormaptype_ensure(&wgt->mmap_params);
	WM_manipulator_group_type_ensure_ptr_ex(wgt, mmap_type);
}
void WM_manipulator_group_type_ensure(const char *idname)
{
	wmManipulatorGroupType *wgt = WM_manipulatorgrouptype_find(idname, false);
	BLI_assert(wgt != NULL);
	WM_manipulator_group_type_ensure_ptr(wgt);
}

void WM_manipulator_group_type_remove_ptr_ex(
        struct Main *bmain, wmManipulatorGroupType *wgt,
        wmManipulatorMapType *mmap_type)
{
	WM_manipulatormaptype_group_unlink(NULL, bmain, mmap_type, wgt);
	WM_manipulatorgrouptype_free_ptr(wgt);
}
void WM_manipulator_group_type_remove_ptr(
        struct Main *bmain, wmManipulatorGroupType *wgt)
{
	wmManipulatorMapType *mmap_type = WM_manipulatormaptype_ensure(&wgt->mmap_params);
	WM_manipulator_group_type_remove_ptr_ex(bmain, wgt, mmap_type);
}
void WM_manipulator_group_type_remove(struct Main *bmain, const char *idname)
{
	wmManipulatorGroupType *wgt = WM_manipulatorgrouptype_find(idname, false);
	BLI_assert(wgt != NULL);
	WM_manipulator_group_type_remove_ptr(bmain, wgt);
}

/* delayed versions */

void WM_manipulator_group_type_unlink_delayed_ptr_ex(
        wmManipulatorGroupType *wgt,
        wmManipulatorMapType *mmap_type)
{
	WM_manipulatorconfig_update_tag_remove(mmap_type, wgt);
}

void WM_manipulator_group_type_unlink_delayed_ptr(
        wmManipulatorGroupType *wgt)
{
	wmManipulatorMapType *mmap_type = WM_manipulatormaptype_ensure(&wgt->mmap_params);
	WM_manipulator_group_type_unlink_delayed_ptr_ex(wgt, mmap_type);
}

void WM_manipulator_group_type_unlink_delayed(const char *idname)
{
	wmManipulatorGroupType *wgt = WM_manipulatorgrouptype_find(idname, false);
	BLI_assert(wgt != NULL);
	WM_manipulator_group_type_unlink_delayed_ptr(wgt);
}

/** \} */
