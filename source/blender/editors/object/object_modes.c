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
 * Contributor(s): Blender Foundation, 2002-2008 full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_modes.c
 *  \ingroup edobj
 *
 * General utils to handle mode switching,
 * actual mode switching logic is per-object type.
 */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_workspace_types.h"

#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "DEG_depsgraph.h"

#include "ED_screen.h"

#include "ED_object.h"  /* own include */

/* -------------------------------------------------------------------- */
/** \name High Level Mode Operations
 *
 * \{ */

static const char *object_mode_op_string(eObjectMode mode)
{
	if (mode & OB_MODE_EDIT)
		return "OBJECT_OT_editmode_toggle";
	if (mode == OB_MODE_SCULPT)
		return "SCULPT_OT_sculptmode_toggle";
	if (mode == OB_MODE_VERTEX_PAINT)
		return "PAINT_OT_vertex_paint_toggle";
	if (mode == OB_MODE_WEIGHT_PAINT)
		return "PAINT_OT_weight_paint_toggle";
	if (mode == OB_MODE_TEXTURE_PAINT)
		return "PAINT_OT_texture_paint_toggle";
	if (mode == OB_MODE_PARTICLE_EDIT)
		return "PARTICLE_OT_particle_edit_toggle";
	if (mode == OB_MODE_POSE)
		return "OBJECT_OT_posemode_toggle";
	if (mode == OB_MODE_GPENCIL)
		return "GPENCIL_OT_editmode_toggle";
	return NULL;
}

/**
 * Checks the mode to be set is compatible with the object
 * should be made into a generic function
 */
bool ED_object_mode_compat_test(const Object *ob, eObjectMode mode)
{
	if (ob) {
		if (mode == OB_MODE_OBJECT)
			return true;
		else if (mode == OB_MODE_GPENCIL)
			return true; /* XXX: assume this is the case for now... */

		switch (ob->type) {
			case OB_MESH:
				if (mode & (OB_MODE_EDIT | OB_MODE_SCULPT | OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT |
				            OB_MODE_TEXTURE_PAINT | OB_MODE_PARTICLE_EDIT))
				{
					return true;
				}
				break;
			case OB_CURVE:
			case OB_SURF:
			case OB_FONT:
			case OB_MBALL:
				if (mode & (OB_MODE_EDIT))
					return true;
				break;
			case OB_LATTICE:
				if (mode & (OB_MODE_EDIT | OB_MODE_WEIGHT_PAINT))
					return true;
				break;
			case OB_ARMATURE:
				if (mode & (OB_MODE_EDIT | OB_MODE_POSE))
					return true;
				break;
		}
	}

	return false;
}

/**
 * Sets the mode to a compatible state (use before entering the mode).
 *
 * This is so each mode's exec function can call
 */
bool ED_object_mode_compat_set(bContext *C, WorkSpace *workspace, eObjectMode mode, ReportList *reports)
{
	bool ok;
	if (!ELEM(workspace->object_mode, mode, OB_MODE_OBJECT)) {
		const char *opstring = object_mode_op_string(workspace->object_mode);

		WM_operator_name_call(C, opstring, WM_OP_EXEC_REGION_WIN, NULL);
		ok = ELEM(workspace->object_mode, mode, OB_MODE_OBJECT);
		if (!ok) {
			wmOperatorType *ot = WM_operatortype_find(opstring, false);
			BKE_reportf(reports, RPT_ERROR, "Unable to execute '%s', error changing modes", ot->name);
		}
	}
	else {
		ok = true;
	}

	return ok;
}

void ED_object_mode_toggle(bContext *C, eObjectMode mode)
{
	if (mode != OB_MODE_OBJECT) {
		const char *opstring = object_mode_op_string(mode);

		if (opstring) {
			WM_operator_name_call(C, opstring, WM_OP_EXEC_REGION_WIN, NULL);
		}
	}
}


/* Wrapper for operator  */
void ED_object_mode_set(bContext *C, eObjectMode mode)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wm->op_undo_depth++;
	/* needed so we don't do undo pushes. */
	ED_object_mode_generic_enter(C, mode);
	wm->op_undo_depth--;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Mode Enter/Exit
 *
 * Supports exiting a mode without it being in the current context.
 * This could be done for entering modes too if it's needed.
 *
 * \{ */

bool ED_object_mode_generic_enter(
        struct bContext *C, eObjectMode object_mode)
{
	WorkSpace *workspace = CTX_wm_workspace(C);
	if (workspace->object_mode == object_mode) {
		return true;
	}
	wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_mode_set", false);
	PointerRNA ptr;
	WM_operator_properties_create_ptr(&ptr, ot);
	RNA_enum_set(&ptr, "mode", object_mode);
	WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &ptr);
	WM_operator_properties_free(&ptr);
	return (workspace->object_mode == object_mode);
}

/**
 * Use for changing works-paces or changing active object.
 * Caller can check #OB_MODE_ALL_MODE_DATA to test if this needs to be run.
 */
static bool ed_object_mode_generic_exit_ex(
        const struct EvaluationContext *eval_ctx,
        struct WorkSpace *workspace, struct Scene *scene, struct Object *ob,
        bool only_test)
{
	if (eval_ctx->object_mode & OB_MODE_EDIT) {
		if (BKE_object_is_in_editmode(ob)) {
			if (only_test) {
				return true;
			}
			ED_object_editmode_exit_ex(NULL, workspace, scene, ob, EM_FREEDATA);
		}
	}
	else if (eval_ctx->object_mode & OB_MODE_VERTEX_PAINT) {
		if (ob->sculpt && (ob->sculpt->mode_type == OB_MODE_VERTEX_PAINT)) {
			if (only_test) {
				return true;
			}
			ED_object_vpaintmode_exit_ex(workspace, ob);
		}
	}
	else if (eval_ctx->object_mode & OB_MODE_WEIGHT_PAINT) {
		if (ob->sculpt && (ob->sculpt->mode_type == OB_MODE_WEIGHT_PAINT)) {
			if (only_test) {
				return true;
			}
			ED_object_wpaintmode_exit_ex(workspace, ob);
		}
	}
	else if (eval_ctx->object_mode & OB_MODE_SCULPT) {
		if (ob->sculpt && (ob->sculpt->mode_type == OB_MODE_SCULPT)) {
			if (only_test) {
				return true;
			}
			ED_object_sculptmode_exit_ex(eval_ctx, workspace, scene, ob);
		}
	}
	else {
		if (only_test) {
			return false;
		}
		BLI_assert((eval_ctx->object_mode & OB_MODE_ALL_MODE_DATA) == 0);
	}

	return false;
}

void ED_object_mode_generic_exit(
        const struct EvaluationContext *eval_ctx,
        struct WorkSpace *workspace, struct Scene *scene, struct Object *ob)
{
	ed_object_mode_generic_exit_ex(eval_ctx, workspace, scene, ob, false);
}

bool ED_object_mode_generic_has_data(
        const struct EvaluationContext *eval_ctx,
        struct Object *ob)
{
	return ed_object_mode_generic_exit_ex(eval_ctx, NULL, NULL, ob, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mode Syncing Utils
 *
 * \{ */

/**
 * A version of #ED_object_mode_generic_enter that checks if the object
 * has an active mode mode in another window we need to use another window first.
 */
bool ED_object_mode_generic_enter_or_other_window(
        struct bContext *C, const wmWindow *win_compare, eObjectMode object_mode)
{
	WorkSpace *workspace = CTX_wm_workspace(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	Base *basact = view_layer->basact;
	if (basact == NULL) {
		workspace->object_mode = OB_MODE_OBJECT;
		return (workspace->object_mode == object_mode);
	}

	wmWindowManager *wm = CTX_wm_manager(C);
	eObjectMode object_mode_set = OB_MODE_OBJECT;
	bool use_object_mode = ED_workspace_object_mode_in_other_window(wm, win_compare, basact->object, &object_mode_set);

	if (use_object_mode) {
		workspace->object_mode = object_mode_set;
		return (workspace->object_mode == object_mode);
	}
	else {
		workspace->object_mode = OB_MODE_OBJECT;
		return ED_object_mode_generic_enter(C, object_mode);
	}
}

void ED_object_mode_generic_exit_or_other_window(
        const struct EvaluationContext *eval_ctx, wmWindowManager *wm,
        struct WorkSpace *workspace, struct Scene *scene, struct Object *ob)
{
	if (ob == NULL) {
		return;
	}
	bool is_active = ED_workspace_object_mode_in_other_window(wm, NULL, ob, NULL);
	if (is_active == false) {
		ED_object_mode_generic_exit(eval_ctx, workspace, scene, ob);
	}
}

/**
 * Use to find if we need to create the mode-data.
 *
 * When the mode 'exists' it means we have a windowing showing an object with this mode.
 * So it's data is already created.
 * Used to check if we need to perform mode switching.
 */
bool ED_object_mode_generic_exists(
        wmWindowManager *wm, struct Object *ob,
        eObjectMode object_mode)
{
	if (ob == NULL) {
		return false;
	}
	eObjectMode object_mode_test;
	if (ED_workspace_object_mode_in_other_window(wm, NULL, ob, &object_mode_test)) {
		if (object_mode == object_mode_test) {
			return true;
		}
	}
	return false;
}

/** \} */
