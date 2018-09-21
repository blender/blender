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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_select_similar.c
 *  \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_kdtree.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_mesh.h"
#include "ED_screen.h"

#include "mesh_intern.h"  /* own include */

/* -------------------------------------------------------------------- */
/** \name Select Similar (Vert/Edge/Face) Operator - common
 * \{ */

static const EnumPropertyItem prop_similar_compare_types[] = {
	{SIM_CMP_EQ, "EQUAL", 0, "Equal", ""},
	{SIM_CMP_GT, "GREATER", 0, "Greater", ""},
	{SIM_CMP_LT, "LESS", 0, "Less", ""},

	{0, NULL, 0, NULL, NULL}
};

static const EnumPropertyItem prop_similar_types[] = {
	{SIMVERT_NORMAL, "NORMAL", 0, "Normal", ""},
	{SIMVERT_FACE, "FACE", 0, "Amount of Adjacent Faces", ""},
	{SIMVERT_VGROUP, "VGROUP", 0, "Vertex Groups", ""},
	{SIMVERT_EDGE, "EDGE", 0, "Amount of connecting edges", ""},

	{SIMEDGE_LENGTH, "LENGTH", 0, "Length", ""},
	{SIMEDGE_DIR, "DIR", 0, "Direction", ""},
	{SIMEDGE_FACE, "FACE", 0, "Amount of Faces Around an Edge", ""},
	{SIMEDGE_FACE_ANGLE, "FACE_ANGLE", 0, "Face Angles", ""},
	{SIMEDGE_CREASE, "CREASE", 0, "Crease", ""},
	{SIMEDGE_BEVEL, "BEVEL", 0, "Bevel", ""},
	{SIMEDGE_SEAM, "SEAM", 0, "Seam", ""},
	{SIMEDGE_SHARP, "SHARP", 0, "Sharpness", ""},
#ifdef WITH_FREESTYLE
	{SIMEDGE_FREESTYLE, "FREESTYLE_EDGE", 0, "Freestyle Edge Marks", ""},
#endif

	{SIMFACE_MATERIAL, "MATERIAL", 0, "Material", ""},
	{SIMFACE_AREA, "AREA", 0, "Area", ""},
	{SIMFACE_SIDES, "SIDES", 0, "Polygon Sides", ""},
	{SIMFACE_PERIMETER, "PERIMETER", 0, "Perimeter", ""},
	{SIMFACE_NORMAL, "NORMAL", 0, "Normal", ""},
	{SIMFACE_COPLANAR, "COPLANAR", 0, "Co-planar", ""},
	{SIMFACE_SMOOTH, "SMOOTH", 0, "Flat/Smooth", ""},
	{SIMFACE_FACEMAP, "FACE_MAP", 0, "Face-Map", ""},
#ifdef WITH_FREESTYLE
	{SIMFACE_FREESTYLE, "FREESTYLE_FACE", 0, "Freestyle Face Marks", ""},
#endif

	{0, NULL, 0, NULL, NULL}
};

static int UNUSED_FUNCTION(bm_sel_similar_cmp_fl)(const float delta, const float thresh, const int compare)
{
	switch (compare) {
		case SIM_CMP_EQ:
			return (fabsf(delta) <= thresh);
		case SIM_CMP_GT:
			return ((delta + thresh) >= 0.0f);
		case SIM_CMP_LT:
			return ((delta - thresh) <= 0.0f);
		default:
			BLI_assert(0);
			return 0;
	}
}

static int bm_sel_similar_cmp_i(const int delta, const int compare)
{
	switch (compare) {
		case SIM_CMP_EQ:
			return (delta == 0);
		case SIM_CMP_GT:
			return (delta > 0);
		case SIM_CMP_LT:
			return (delta < 0);
		default:
			BLI_assert(0);
			return 0;
	}
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Similar Face
 * \{ */

static int similar_face_select_exec(bContext *C, wmOperator *op)
{
	/* TODO (dfelinto) port the face modes to multi-object. */
	BKE_report(op->reports, RPT_ERROR, "Select similar not supported for faces at the moment");
	return OPERATOR_CANCELLED;

	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMOperator bmop;

	/* get the type from RNA */
	const int type = RNA_enum_get(op->ptr, "type");
	const float thresh = RNA_float_get(op->ptr, "threshold");
	const int compare = RNA_enum_get(op->ptr, "compare");

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_op_init(em, &bmop, op,
	             "similar_faces faces=%hf type=%i thresh=%f compare=%i",
	             BM_ELEM_SELECT, type, thresh, compare);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	/* select the output */
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);

	/* finish the operator */
	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, false, false);

	return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Similar Edge
 * \{ */


/**
 * Note: This is not normal, but the edge direction itself and always in
 * a positive quadrant (tries z, y then x).
 * Therefore we need to use the entire object transformation matrix.
 */
static void edge_pos_direction_worldspace_get(Object *ob, BMEdge *edge, float *r_dir)
{
	float v1[3], v2[3];
	copy_v3_v3(v1, edge->v1->co);
	copy_v3_v3(v2, edge->v2->co);

	mul_m4_v3(ob->obmat, v1);
	mul_m4_v3(ob->obmat, v2);

	sub_v3_v3v3(r_dir, v1, v2);
	normalize_v3(r_dir);

	/* Make sure we have a consistent direction that can be checked regardless of
	 * the verts order of the edges. This spares us from storing dir and -dir in the tree. */
	if (fabs(r_dir[2]) < FLT_EPSILON) {
		if (fabs(r_dir[1]) < FLT_EPSILON) {
			if (r_dir[0] < 0.0f) {
				mul_v3_fl(r_dir, -1.0f);
			}
		}
		else if (r_dir[1] < 0.0f) {
			mul_v3_fl(r_dir, -1.0f);
		}
	}
	else if (r_dir[2] < 0.0f) {
		mul_v3_fl(r_dir, -1.0f);
	}
}

/* wrap the above function but do selection flushing edge to face */
static int similar_edge_select_exec(bContext *C, wmOperator *op)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);

	/* get the type from RNA */
	const int type = RNA_enum_get(op->ptr, "type");
	const float thresh = RNA_float_get(op->ptr, "threshold");
	const float thresh_radians = thresh * (float)M_PI + FLT_EPSILON;
	const int compare = RNA_enum_get(op->ptr, "compare");

	if (ELEM(type,
	         SIMEDGE_LENGTH,
	         SIMEDGE_FACE_ANGLE,
	         SIMEDGE_CREASE,
	         SIMEDGE_BEVEL,
	         SIMEDGE_SEAM,
#ifdef WITH_FREESTYLE
	         SIMEDGE_FREESTYLE,
#endif
	         SIMEDGE_SHARP))
	{
		/* TODO (dfelinto) port the edge modes to multi-object. */
		BKE_report(op->reports, RPT_ERROR, "Select similar edge mode not supported at the moment");
		return OPERATOR_CANCELLED;
	}

	int tot_edges_selected_all = 0;
	uint objects_len = 0;
	Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(view_layer, &objects_len);

	for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
		Object *ob = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(ob);
		tot_edges_selected_all += em->bm->totedgesel;
	}

	if (tot_edges_selected_all == 0) {
		BKE_report(op->reports, RPT_ERROR, "No edge selected");
		MEM_freeN(objects);
		return OPERATOR_CANCELLED;
	}

	KDTree *tree = NULL;
	GSet *gset = NULL;

	switch (type) {
		case SIMEDGE_DIR:
			tree = BLI_kdtree_new(tot_edges_selected_all);
			break;
		case SIMEDGE_FACE:
			gset = BLI_gset_ptr_new("Select similar edge: face");
			break;
	}

	int tree_index = 0;
	for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
		Object *ob = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(ob);
		BMesh *bm = em->bm;

		if (bm->totedgesel == 0) {
			continue;
		}

		BMEdge *edge; /* Mesh edge. */
		BMIter iter; /* Selected edges iterator. */

		BM_ITER_MESH (edge, &iter, bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(edge, BM_ELEM_SELECT)) {
				switch (type) {
					case SIMEDGE_FACE:
						BLI_gset_add(gset, POINTER_FROM_INT(BM_edge_face_count(edge)));
						break;
					case SIMEDGE_DIR:
					{
						float dir[3];
						edge_pos_direction_worldspace_get(ob, edge, dir);
						BLI_kdtree_insert(tree, tree_index++, dir);
						break;
					}
				}
			}
		}
	}

	if (tree != NULL) {
		BLI_kdtree_balance(tree);
	}

	for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
		Object *ob = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(ob);
		BMesh *bm = em->bm;
		bool changed = false;

		BMEdge *edge; /* Mesh edge. */
		BMIter iter; /* Selected edges iterator. */

		BM_ITER_MESH (edge, &iter, bm, BM_EDGES_OF_MESH) {
			if (!BM_elem_flag_test(edge, BM_ELEM_SELECT)) {
				switch (type) {
					case SIMEDGE_FACE:
					{
						const int num_faces = BM_edge_face_count(edge);
						GSetIterator gs_iter;
						GSET_ITER(gs_iter, gset) {
							const int num_faces_iter = POINTER_AS_INT(BLI_gsetIterator_getKey(&gs_iter));
							const int delta_i = num_faces - num_faces_iter;
							if (bm_sel_similar_cmp_i(delta_i, compare)) {
								BM_edge_select_set(bm, edge, true);
								changed = true;
								break;
							}
						}
						break;
					}
					case SIMEDGE_DIR:
					{
						float dir[3];
						edge_pos_direction_worldspace_get(ob, edge, dir);

						/* We are treating the direction as coordinates, the "nearest" one will
						 * also be the one closest to the intended direction. */
						KDTreeNearest nearest;
						if (BLI_kdtree_find_nearest(tree, dir, &nearest) != -1) {
							if (angle_normalized_v3v3(dir, nearest.co) <= thresh_radians) {
								BM_edge_select_set(bm, edge, true);
								changed = true;
							}
						}
						break;
					}
				}
			}
		}

		if (changed) {
			EDBM_selectmode_flush(em);
			EDBM_update_generic(em, false, false);
		}
	}

	MEM_freeN(objects);
	BLI_kdtree_free(tree);
	if (gset != NULL) {
		BLI_gset_free(gset, NULL);
	}

	return OPERATOR_FINISHED;
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Similar Vert
 * \{ */

static int similar_vert_select_exec(bContext *C, wmOperator *op)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);

	/* get the type from RNA */
	const int type = RNA_enum_get(op->ptr, "type");
	const float thresh = RNA_float_get(op->ptr, "threshold");
	const float thresh_radians = thresh * (float)M_PI + FLT_EPSILON;
	const int compare = RNA_enum_get(op->ptr, "compare");

	if (type == SIMVERT_VGROUP) {
		BKE_report(op->reports, RPT_ERROR, "Select similar vertex groups not supported at the moment.");
		return OPERATOR_CANCELLED;
	}

	int tot_verts_selected_all = 0;
	uint objects_len = 0;
	Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(view_layer, &objects_len);

	for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
		Object *ob = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(ob);
		tot_verts_selected_all += em->bm->totvertsel;
	}

	if (tot_verts_selected_all == 0) {
		BKE_report(op->reports, RPT_ERROR, "No vertex selected");
		MEM_freeN(objects);
		return OPERATOR_CANCELLED;
	}

	KDTree *tree = NULL;
	GSet *gset = NULL;

	switch (type) {
		case SIMVERT_NORMAL:
			tree = BLI_kdtree_new(tot_verts_selected_all);
			break;
		case SIMVERT_EDGE:
		case SIMVERT_FACE:
			gset = BLI_gset_ptr_new("Select similar vertex: edge/face");
			break;
	}

	int normal_tree_index = 0;
	for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
		Object *ob = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(ob);
		BMesh *bm = em->bm;
		invert_m4_m4(ob->imat, ob->obmat);

		if (bm->totvertsel == 0) {
			continue;
		}

		BMVert *vert; /* Mesh vertex. */
		BMIter iter; /* Selected verts iterator. */

		BM_ITER_MESH (vert, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(vert, BM_ELEM_SELECT)) {
				switch (type) {
					case SIMVERT_FACE:
						BLI_gset_add(gset, POINTER_FROM_INT(BM_vert_face_count(vert)));
						break;
					case SIMVERT_EDGE:
						BLI_gset_add(gset, POINTER_FROM_INT(BM_vert_edge_count(vert)));
						break;
					case SIMVERT_NORMAL:
					{
						float normal[3];
						copy_v3_v3(normal, vert->no);
						mul_transposed_mat3_m4_v3(ob->imat, normal);
						normalize_v3(normal);

						BLI_kdtree_insert(tree, normal_tree_index++, normal);
						break;
					}
				}
			}
		}
	}

	/* Remove duplicated entries. */
	if (tree != NULL) {
		BLI_kdtree_balance(tree);
	}

	/* Run .the BM operators. */
	for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
		Object *ob = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(ob);
		BMesh *bm = em->bm;
		bool changed = false;

		BMVert *vert; /* Mesh vertex. */
		BMIter iter; /* Selected verts iterator. */

		BM_ITER_MESH (vert, &iter, bm, BM_VERTS_OF_MESH) {
			if (!BM_elem_flag_test(vert, BM_ELEM_SELECT)) {
				switch (type) {
					case SIMVERT_EDGE:
					{
						const int num_edges = BM_vert_edge_count(vert);
						GSetIterator gs_iter;
						GSET_ITER(gs_iter, gset) {
							const int num_edges_iter = POINTER_AS_INT(BLI_gsetIterator_getKey(&gs_iter));
							const int delta_i = num_edges - num_edges_iter;
							if (bm_sel_similar_cmp_i(delta_i, compare)) {
								BM_vert_select_set(bm, vert, true);
								changed = true;
								break;
							}
						}
						break;
					}
					case SIMVERT_FACE:
					{
						const int num_faces = BM_vert_face_count(vert);
						GSetIterator gs_iter;
						GSET_ITER(gs_iter, gset) {
							const int num_faces_iter = POINTER_AS_INT(BLI_gsetIterator_getKey(&gs_iter));
							const int delta_i = num_faces - num_faces_iter;
							if (bm_sel_similar_cmp_i(delta_i, compare)) {
								BM_vert_select_set(bm, vert, true);
								changed = true;
								break;
							}
						}
						break;
					}
					case SIMVERT_NORMAL:
					{
						float normal[3];
						copy_v3_v3(normal, vert->no);
						mul_transposed_mat3_m4_v3(ob->imat, normal);
						normalize_v3(normal);

						/* We are treating the normals as coordinates, the "nearest" one will
						 * also be the one closest to the angle. */
						KDTreeNearest nearest;
						if (BLI_kdtree_find_nearest(tree, normal, &nearest) != -1) {
							if (angle_normalized_v3v3(normal, nearest.co) <= thresh_radians) {
								BM_vert_select_set(bm, vert, true);
								changed = true;
							}
						}
						break;
					}
				}
			}
		}

		if (changed) {
			EDBM_selectmode_flush(em);
			EDBM_update_generic(em, false, false);
		}
	}

	MEM_freeN(objects);
	BLI_kdtree_free(tree);
	if (gset != NULL) {
		BLI_gset_free(gset, NULL);
	}

	return OPERATOR_FINISHED;
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Similar Operator
 * \{ */

static int edbm_select_similar_exec(bContext *C, wmOperator *op)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	PropertyRNA *prop = RNA_struct_find_property(op->ptr, "threshold");

	const int type = RNA_enum_get(op->ptr, "type");

	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_float_set(op->ptr, prop, ts->select_thresh);
	}
	else {
		ts->select_thresh = RNA_property_float_get(op->ptr, prop);
	}

	if      (type < 100) return similar_vert_select_exec(C, op);
	else if (type < 200) return similar_edge_select_exec(C, op);
	else                 return similar_face_select_exec(C, op);
}

static const EnumPropertyItem *select_similar_type_itemf(
        bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop),
        bool *r_free)
{
	Object *obedit;

	if (!C) /* needed for docs and i18n tools */
		return prop_similar_types;

	obedit = CTX_data_edit_object(C);

	if (obedit && obedit->type == OB_MESH) {
		EnumPropertyItem *item = NULL;
		int a, totitem = 0;
		BMEditMesh *em = BKE_editmesh_from_object(obedit);

		if (em->selectmode & SCE_SELECT_VERTEX) {
			for (a = SIMVERT_NORMAL; a < SIMEDGE_LENGTH; a++) {
				RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
			}
		}
		else if (em->selectmode & SCE_SELECT_EDGE) {
			for (a = SIMEDGE_LENGTH; a < SIMFACE_MATERIAL; a++) {
				RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
			}
		}
		else if (em->selectmode & SCE_SELECT_FACE) {
#ifdef WITH_FREESTYLE
			const int a_end = SIMFACE_FREESTYLE;
#else
			const int a_end = SIMFACE_FACEMAP;
#endif
			for (a = SIMFACE_MATERIAL; a <= a_end; a++) {
				RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
			}
		}
		RNA_enum_item_end(&item, &totitem);

		*r_free = true;

		return item;
	}

	return prop_similar_types;
}

void MESH_OT_select_similar(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Select Similar";
	ot->idname = "MESH_OT_select_similar";
	ot->description = "Select similar vertices, edges or faces by property types";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = edbm_select_similar_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = ot->prop = RNA_def_enum(ot->srna, "type", prop_similar_types, SIMVERT_NORMAL, "Type", "");
	RNA_def_enum_funcs(prop, select_similar_type_itemf);

	RNA_def_enum(ot->srna, "compare", prop_similar_compare_types, SIM_CMP_EQ, "Compare", "");

	RNA_def_float(ot->srna, "threshold", 0.0f, 0.0f, 1.0f, "Threshold", "", 0.0f, 1.0f);
}

/** \} */
