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
 * The Original Code is Copyright (C) 2004 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_extrude_spin.c
 *  \ingroup edmesh
 */

#include "DNA_object_types.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_editmesh.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "mesh_intern.h"  /* own include */


/* -------------------------------------------------------------------- */
/** \name Spin Operator
 * \{ */

static int edbm_spin_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMOperator spinop;
	float cent[3], axis[3];
	float d[3] = {0.0f, 0.0f, 0.0f};

	RNA_float_get_array(op->ptr, "center", cent);
	RNA_float_get_array(op->ptr, "axis", axis);
	const int steps = RNA_int_get(op->ptr, "steps");
	const float angle = RNA_float_get(op->ptr, "angle");
	const bool use_normal_flip = RNA_boolean_get(op->ptr, "use_normal_flip") ^ (angle < 0.0f);
	const bool dupli = RNA_boolean_get(op->ptr, "dupli");
	const bool use_auto_merge = (
	        RNA_boolean_get(op->ptr, "use_auto_merge") &&
	        (dupli == false) &&
	        (steps >= 3) &&
	        fabsf((fabsf(angle) - (M_PI * 2))) <= 1e-6f);

	if (is_zero_v3(axis)) {
		BKE_report(op->reports, RPT_ERROR, "Invalid/unset axis");
		return OPERATOR_CANCELLED;
	}

	/* keep the values in worldspace since we're passing the obmat */
	if (!EDBM_op_init(
	            em, &spinop, op,
	            "spin geom=%hvef cent=%v axis=%v dvec=%v steps=%i angle=%f space=%m4 "
	            "use_normal_flip=%b use_duplicate=%b use_merge=%b",
	            BM_ELEM_SELECT, cent, axis, d, steps, -angle, obedit->obmat,
	            use_normal_flip, dupli, use_auto_merge))
	{
		return OPERATOR_CANCELLED;
	}
	BMO_op_exec(bm, &spinop);
	if (use_auto_merge == false) {
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
		BMO_slot_buffer_hflag_enable(bm, spinop.slots_out, "geom_last.out", BM_ALL_NOLOOP, BM_ELEM_SELECT, true);
	}
	if (!EDBM_op_finish(em, &spinop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

/* get center and axis, in global coords */
static int edbm_spin_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);

	PropertyRNA *prop;
	prop = RNA_struct_find_property(op->ptr, "center");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_float_set_array(op->ptr, prop, ED_view3d_cursor3d_get(scene, v3d));
	}
	if (rv3d) {
		prop = RNA_struct_find_property(op->ptr, "axis");
		if (!RNA_property_is_set(op->ptr, prop)) {
			RNA_property_float_set_array(op->ptr, prop, rv3d->viewinv[2]);
		}
	}

	return edbm_spin_exec(C, op);
}

static bool edbm_spin_poll_property(const bContext *UNUSED(C), wmOperator *op, const PropertyRNA *prop)
{
	const char *prop_id = RNA_property_identifier(prop);
	const bool dupli = RNA_boolean_get(op->ptr, "dupli");

	if (dupli) {
		if (STREQ(prop_id, "use_auto_merge") ||
		    STREQ(prop_id, "use_normal_flip"))
		{
			return false;
		}
	}
	return true;
}

void MESH_OT_spin(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Spin";
	ot->description = "Extrude selected vertices in a circle around the cursor in indicated viewport";
	ot->idname = "MESH_OT_spin";

	/* api callbacks */
	ot->invoke = edbm_spin_invoke;
	ot->exec = edbm_spin_exec;
	ot->poll = ED_operator_editmesh;
	ot->poll_property = edbm_spin_poll_property;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_int(ot->srna, "steps", 9, 0, 1000000, "Steps", "Steps", 0, 1000);
	RNA_def_boolean(ot->srna, "dupli", 0, "Dupli", "Make Duplicates");
	prop = RNA_def_float(ot->srna, "angle", DEG2RADF(90.0f), -1e12f, 1e12f, "Angle", "Rotation for each step",
	                     DEG2RADF(-360.0f), DEG2RADF(360.0f));
	RNA_def_property_subtype(prop, PROP_ANGLE);
	RNA_def_boolean(ot->srna, "use_auto_merge", true, "Auto Merge", "Merge first/last when the angle is a full revolution");
	RNA_def_boolean(ot->srna, "use_normal_flip", 0, "Flip Normals", "");

	RNA_def_float_vector(ot->srna, "center", 3, NULL, -1e12f, 1e12f,
	                     "Center", "Center in global view space", -1e4f, 1e4f);
	RNA_def_float_vector(ot->srna, "axis", 3, NULL, -1.0f, 1.0f, "Axis", "Axis in global view space", -1.0f, 1.0f);

}

/** \} */
