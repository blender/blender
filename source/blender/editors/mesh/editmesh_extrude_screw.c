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

/** \file blender/editors/mesh/editmesh_extrude_screw.c
 *  \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_report.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"



#include "mesh_intern.h"  /* own include */

/* -------------------------------------------------------------------- */
/** \name Screw Operator
 * \{ */

static int edbm_screw_exec(bContext *C, wmOperator *op)
{
	BMEdge *eed;
	BMVert *eve, *v1, *v2;
	BMIter iter, eiter;
	float dvec[3], nor[3], cent[3], axis[3], v1_co_global[3], v2_co_global[3];
	int steps, turns;
	int valence;
	uint objects_empty_len = 0;
	uint failed_axis_len = 0;
	uint failed_vertices_len = 0;

	turns = RNA_int_get(op->ptr, "turns");
	steps = RNA_int_get(op->ptr, "steps");
	RNA_float_get_array(op->ptr, "center", cent);
	RNA_float_get_array(op->ptr, "axis", axis);

	uint objects_len = 0;
	ViewLayer *view_layer = CTX_data_view_layer(C);
	Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(view_layer, CTX_wm_view3d(C), &objects_len);

	for (uint ob_index = 0; ob_index < objects_len; ob_index++)	{
		Object *obedit = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		BMesh *bm = em->bm;

		if (bm->totvertsel < 2) {
			if (bm->totvertsel == 0) {
				objects_empty_len++;
			}
			continue;
		}

		if (is_zero_v3(axis)) {
			failed_axis_len++;
			continue;
		}

		/* find two vertices with valence count == 1, more or less is wrong */
		v1 = NULL;
		v2 = NULL;

		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			valence = 0;
			BM_ITER_ELEM (eed, &eiter, eve, BM_EDGES_OF_VERT) {
				if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
					valence++;
				}
			}

			if (valence == 1) {
				if (v1 == NULL) {
					v1 = eve;
				}
				else if (v2 == NULL) {
					v2 = eve;
				}
				else {
					v1 = NULL;
					break;
				}
			}
		}

		if (v1 == NULL || v2 == NULL) {
			failed_vertices_len++;
			continue;
		}

		copy_v3_v3(nor, obedit->obmat[2]);

		/* calculate dvec */
		mul_v3_m4v3(v1_co_global, obedit->obmat, v1->co);
		mul_v3_m4v3(v2_co_global, obedit->obmat, v2->co);
		sub_v3_v3v3(dvec, v1_co_global, v2_co_global);
		mul_v3_fl(dvec, 1.0f / steps);

		if (dot_v3v3(nor, dvec) > 0.0f)
			negate_v3(dvec);

		BMOperator spinop;
		if (!EDBM_op_init(em, &spinop, op,
		                  "spin geom=%hvef cent=%v axis=%v dvec=%v steps=%i angle=%f space=%m4 use_duplicate=%b",
		                  BM_ELEM_SELECT, cent, axis, dvec, turns * steps, DEG2RADF(360.0f * turns), obedit->obmat, false))
		{
			continue;
		}

		BMO_op_exec(bm, &spinop);
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
		BMO_slot_buffer_hflag_enable(bm, spinop.slots_out, "geom_last.out", BM_ALL_NOLOOP, BM_ELEM_SELECT, true);

		if (!EDBM_op_finish(em, &spinop, op, true)) {
			continue;
		}

		EDBM_update_generic(em, true, true);
	}
	MEM_freeN(objects);

	if (failed_axis_len == objects_len - objects_empty_len) {
		BKE_report(op->reports, RPT_ERROR, "Invalid/unset axis");
	}
	else if (failed_vertices_len == objects_len - objects_empty_len) {
		BKE_report(op->reports, RPT_ERROR, "You have to select a string of connected vertices too");
	}

	return OPERATOR_FINISHED;
}

/* get center and axis, in global coords */
static int edbm_screw_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Scene *scene = CTX_data_scene(C);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);

	PropertyRNA *prop;
	prop = RNA_struct_find_property(op->ptr, "center");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_float_set_array(op->ptr, prop, scene->cursor.location);
	}
	if (rv3d) {
		prop = RNA_struct_find_property(op->ptr, "axis");
		if (!RNA_property_is_set(op->ptr, prop)) {
			RNA_property_float_set_array(op->ptr, prop, rv3d->viewinv[1]);
		}
	}

	return edbm_screw_exec(C, op);
}

void MESH_OT_screw(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Screw";
	ot->description = "Extrude selected vertices in screw-shaped rotation around the cursor in indicated viewport";
	ot->idname = "MESH_OT_screw";

	/* api callbacks */
	ot->invoke = edbm_screw_invoke;
	ot->exec = edbm_screw_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_int(ot->srna, "steps", 9, 1, 100000, "Steps", "Steps", 3, 256);
	RNA_def_int(ot->srna, "turns", 1, 1, 100000, "Turns", "Turns", 1, 256);

	RNA_def_float_vector(ot->srna, "center", 3, NULL, -1e12f, 1e12f,
	                     "Center", "Center in global view space", -1e4f, 1e4f);
	RNA_def_float_vector(ot->srna, "axis", 3, NULL, -1.0f, 1.0f,
	                     "Axis", "Axis in global view space", -1.0f, 1.0f);
}

/** \} */
