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

#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"

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
bool ED_object_mode_compat_set(bContext *C, Object *ob, eObjectMode mode, ReportList *reports)
{
	bool ok;
	if (!ELEM(ob->mode, mode, OB_MODE_OBJECT)) {
		const char *opstring = object_mode_op_string(ob->mode);
		WM_operator_name_call(C, opstring, WM_OP_EXEC_REGION_WIN, NULL);
		ok = ELEM(ob->mode, mode, OB_MODE_OBJECT);
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
#if 0
	wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_mode_set", false);
	PointerRNA ptr;

	WM_operator_properties_create_ptr(&ptr, ot);
	RNA_enum_set(&ptr, "mode", mode);
	RNA_boolean_set(&ptr, "toggle", false);
	WM_operator_name_call_ptr(C, ot, WM_OP_EXEC_DEFAULT, &ptr);
	WM_operator_properties_free(&ptr);
#else
	Scene *scene = CTX_data_scene(C);
	Object *ob = OBACT;
	if (ob == NULL) {
		return;
	}
	if (ob->mode == mode) {
		/* pass */
	}
	else if (mode != OB_MODE_OBJECT) {
		if (ob && (ob->mode & mode) == 0) {
			/* needed so we don't do undo pushes. */
			wmWindowManager *wm = CTX_wm_manager(C);
			wm->op_undo_depth++;
			ED_object_mode_toggle(C, mode);
			wm->op_undo_depth--;
		}
	}
	else {
		/* needed so we don't do undo pushes. */
		wmWindowManager *wm = CTX_wm_manager(C);
		wm->op_undo_depth++;
		ED_object_mode_toggle(C, ob->mode);
		wm->op_undo_depth--;

	}
#endif
}
