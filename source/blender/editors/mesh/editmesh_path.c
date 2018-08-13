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

/** \file blender/editors/mesh/editmesh_path.c
 *  \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_windowmanager_types.h"

#ifdef WITH_FREESTYLE
#  include "DNA_meshdata_types.h"
#endif

#include "BLI_math.h"
#include "BLI_linklist.h"

#include "BKE_layer.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_report.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DEG_depsgraph.h"

#include "mesh_intern.h"  /* own include */

/* -------------------------------------------------------------------- */
/** \name Path Select Struct & Properties
 * \{ */

struct PathSelectParams {
	bool track_active;  /* ensure the active element is the last selected item (handy for picking) */
	bool use_topology_distance;
	bool use_face_step;
	bool use_fill;
	char edge_mode;
	struct CheckerIntervalParams interval_params;
};

static void path_select_properties(wmOperatorType *ot)
{
	RNA_def_boolean(
	        ot->srna, "use_face_step", false, "Face Stepping",
	        "Traverse connected faces (includes diagonals and edge-rings)");
	RNA_def_boolean(
	        ot->srna, "use_topology_distance", false, "Topology Distance",
	        "Find the minimum number of steps, ignoring spatial distance");
	RNA_def_boolean(
	        ot->srna, "use_fill", false, "Fill Region",
	        "Select all paths between the source/destination elements");
	WM_operator_properties_checker_interval(ot, true);
}

static void path_select_params_from_op(wmOperator *op, struct PathSelectParams *op_params)
{
	op_params->edge_mode = EDGE_MODE_SELECT;
	op_params->track_active = false;
	op_params->use_face_step = RNA_boolean_get(op->ptr, "use_face_step");
	op_params->use_fill = RNA_boolean_get(op->ptr, "use_fill");
	op_params->use_topology_distance = RNA_boolean_get(op->ptr, "use_topology_distance");
	WM_operator_properties_checker_interval_from_op(op, &op_params->interval_params);
}

struct UserData {
	BMesh *bm;
	Mesh  *me;
	const struct PathSelectParams *op_params;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vert Path
 * \{ */

/* callbacks */
static bool verttag_filter_cb(BMVert *v, void *UNUSED(user_data_v))
{
	return !BM_elem_flag_test(v, BM_ELEM_HIDDEN);
}
static bool verttag_test_cb(BMVert *v, void *UNUSED(user_data_v))
{
	return BM_elem_flag_test_bool(v, BM_ELEM_SELECT);
}
static void verttag_set_cb(BMVert *v, bool val, void *user_data_v)
{
	struct UserData *user_data = user_data_v;
	BM_vert_select_set(user_data->bm, v, val);
}

static void mouse_mesh_shortest_path_vert(
        Scene *UNUSED(scene), Object *obedit, const struct PathSelectParams *op_params,
        BMVert *v_act, BMVert *v_dst)
{
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;

	struct UserData user_data = {bm, obedit->data, op_params};
	LinkNode *path = NULL;
	bool is_path_ordered = false;

	if (v_act && (v_act != v_dst)) {
		if (op_params->use_fill) {
			path = BM_mesh_calc_path_region_vert(
			        bm, (BMElem *)v_act, (BMElem *)v_dst,
			        verttag_filter_cb, &user_data);
		}
		else {
			is_path_ordered = true;
			path = BM_mesh_calc_path_vert(
			        bm, v_act, v_dst,
			        &(const struct BMCalcPathParams) {
			            .use_topology_distance = op_params->use_topology_distance,
			            .use_step_face = op_params->use_face_step,
			        },
			        verttag_filter_cb, &user_data);
		}

		if (path) {
			if (op_params->track_active) {
				BM_select_history_remove(bm, v_act);
			}
		}
	}

	BMVert *v_dst_last = v_dst;

	if (path) {
		/* toggle the flag */
		bool all_set = true;
		LinkNode *node;

		node = path;
		do {
			if (!verttag_test_cb((BMVert *)node->link, &user_data)) {
				all_set = false;
				break;
			}
		} while ((node = node->next));

		/* We need to start as if just *after* a 'skip' block... */
		int depth = op_params->interval_params.skip;
		node = path;
		do {
			if ((is_path_ordered == false) ||
			    WM_operator_properties_checker_interval_test(&op_params->interval_params, depth))
			{
				verttag_set_cb((BMVert *)node->link, !all_set, &user_data);
				if (is_path_ordered) {
					v_dst_last = node->link;
				}
			}
		} while ((void)depth++, (node = node->next));

		BLI_linklist_free(path, NULL);
	}
	else {
		const bool is_act = !verttag_test_cb(v_dst, &user_data);
		verttag_set_cb(v_dst, is_act, &user_data); /* switch the face option */
	}

	EDBM_selectmode_flush(em);

	if (op_params->track_active) {
		/* even if this is selected it may not be in the selection list */
		if (BM_elem_flag_test(v_dst_last, BM_ELEM_SELECT) == 0) {
			BM_select_history_remove(bm, v_dst_last);
		}
		else {
			BM_select_history_store(bm, v_dst_last);
		}
	}

	EDBM_update_generic(em, false, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge Path
 * \{ */

/* callbacks */
static bool edgetag_filter_cb(BMEdge *e, void *UNUSED(user_data_v))
{
	return !BM_elem_flag_test(e, BM_ELEM_HIDDEN);
}
static bool edgetag_test_cb(BMEdge *e, void *user_data_v)
{
	struct UserData *user_data = user_data_v;
	const char edge_mode = user_data->op_params->edge_mode;
	BMesh *bm = user_data->bm;

	switch (edge_mode) {
		case EDGE_MODE_SELECT:
			return BM_elem_flag_test(e, BM_ELEM_SELECT) ? true : false;
		case EDGE_MODE_TAG_SEAM:
			return BM_elem_flag_test(e, BM_ELEM_SEAM) ? true : false;
		case EDGE_MODE_TAG_SHARP:
			return BM_elem_flag_test(e, BM_ELEM_SMOOTH) ? false : true;
		case EDGE_MODE_TAG_CREASE:
			return BM_elem_float_data_get(&bm->edata, e, CD_CREASE) ? true : false;
		case EDGE_MODE_TAG_BEVEL:
			return BM_elem_float_data_get(&bm->edata, e, CD_BWEIGHT) ? true : false;
#ifdef WITH_FREESTYLE
		case EDGE_MODE_TAG_FREESTYLE:
		{
			FreestyleEdge *fed = CustomData_bmesh_get(&bm->edata, e->head.data, CD_FREESTYLE_EDGE);
			return (!fed) ? false : (fed->flag & FREESTYLE_EDGE_MARK) ? true : false;
		}
#endif
	}
	return 0;
}
static void edgetag_set_cb(BMEdge *e, bool val, void *user_data_v)
{
	struct UserData *user_data = user_data_v;
	const char edge_mode = user_data->op_params->edge_mode;
	BMesh *bm = user_data->bm;

	switch (edge_mode) {
		case EDGE_MODE_SELECT:
			BM_edge_select_set(bm, e, val);
			break;
		case EDGE_MODE_TAG_SEAM:
			BM_elem_flag_set(e, BM_ELEM_SEAM, val);
			break;
		case EDGE_MODE_TAG_SHARP:
			BM_elem_flag_set(e, BM_ELEM_SMOOTH, !val);
			break;
		case EDGE_MODE_TAG_CREASE:
			BM_elem_float_data_set(&bm->edata, e, CD_CREASE, (val) ? 1.0f : 0.0f);
			break;
		case EDGE_MODE_TAG_BEVEL:
			BM_elem_float_data_set(&bm->edata, e, CD_BWEIGHT, (val) ? 1.0f : 0.0f);
			break;
#ifdef WITH_FREESTYLE
		case EDGE_MODE_TAG_FREESTYLE:
		{
			FreestyleEdge *fed;
			fed = CustomData_bmesh_get(&bm->edata, e->head.data, CD_FREESTYLE_EDGE);
			if (!val)
				fed->flag &= ~FREESTYLE_EDGE_MARK;
			else
				fed->flag |= FREESTYLE_EDGE_MARK;
			break;
		}
#endif
	}
}

static void edgetag_ensure_cd_flag(Mesh *me, const char edge_mode)
{
	BMesh *bm = me->edit_btmesh->bm;

	switch (edge_mode) {
		case EDGE_MODE_TAG_CREASE:
			BM_mesh_cd_flag_ensure(bm, me, ME_CDFLAG_EDGE_CREASE);
			break;
		case EDGE_MODE_TAG_BEVEL:
			BM_mesh_cd_flag_ensure(bm, me, ME_CDFLAG_EDGE_BWEIGHT);
			break;
#ifdef WITH_FREESTYLE
		case EDGE_MODE_TAG_FREESTYLE:
			if (!CustomData_has_layer(&bm->edata, CD_FREESTYLE_EDGE)) {
				BM_data_layer_add(bm, &bm->edata, CD_FREESTYLE_EDGE);
			}
			break;
#endif
		default:
			break;
	}
}

/* mesh shortest path select, uses prev-selected edge */

/* since you want to create paths with multiple selects, it doesn't have extend option */
static void mouse_mesh_shortest_path_edge(
        Scene *scene, Object *obedit, const struct PathSelectParams *op_params,
        BMEdge *e_act, BMEdge *e_dst)
{
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;

	struct UserData user_data = {bm, obedit->data, op_params};
	LinkNode *path = NULL;
	Mesh *me = obedit->data;
	bool is_path_ordered = false;

	edgetag_ensure_cd_flag(obedit->data, op_params->edge_mode);

	if (e_act && (e_act != e_dst)) {
		if (op_params->use_fill) {
			path = BM_mesh_calc_path_region_edge(
			        bm, (BMElem *)e_act, (BMElem *)e_dst,
			        edgetag_filter_cb, &user_data);
		}
		else {
			is_path_ordered = true;
			path = BM_mesh_calc_path_edge(
			        bm, e_act, e_dst,
			        &(const struct BMCalcPathParams) {
			            .use_topology_distance = op_params->use_topology_distance,
			            .use_step_face = op_params->use_face_step,
			        },
			        edgetag_filter_cb, &user_data);
		}

		if (path) {
			if (op_params->track_active) {
				BM_select_history_remove(bm, e_act);
			}
		}
	}

	BMEdge *e_dst_last = e_dst;

	if (path) {
		/* toggle the flag */
		bool all_set = true;
		LinkNode *node;

		node = path;
		do {
			if (!edgetag_test_cb((BMEdge *)node->link, &user_data)) {
				all_set = false;
				break;
			}
		} while ((node = node->next));

		/* We need to start as if just *after* a 'skip' block... */
		int depth = op_params->interval_params.skip;
		node = path;
		do {
			if ((is_path_ordered == false) ||
			    WM_operator_properties_checker_interval_test(&op_params->interval_params, depth))
			{
				edgetag_set_cb((BMEdge *)node->link, !all_set, &user_data);
				if (is_path_ordered) {
					e_dst_last = node->link;
				}
			}
		} while ((void)depth++, (node = node->next));

		BLI_linklist_free(path, NULL);
	}
	else {
		const bool is_act = !edgetag_test_cb(e_dst, &user_data);
		edgetag_ensure_cd_flag(obedit->data, op_params->edge_mode);
		edgetag_set_cb(e_dst, is_act, &user_data); /* switch the edge option */
	}

	if (op_params->edge_mode != EDGE_MODE_SELECT) {
		if (op_params->track_active) {
			/* simple rules - last edge is _always_ active and selected */
			if (e_act)
				BM_edge_select_set(bm, e_act, false);
			BM_edge_select_set(bm, e_dst_last, true);
			BM_select_history_store(bm, e_dst_last);
		}
	}

	EDBM_selectmode_flush(em);

	if (op_params->track_active) {
		/* even if this is selected it may not be in the selection list */
		if (op_params->edge_mode == EDGE_MODE_SELECT) {
			if (edgetag_test_cb(e_dst_last, &user_data) == 0)
				BM_select_history_remove(bm, e_dst_last);
			else
				BM_select_history_store(bm, e_dst_last);
		}
	}

	/* force drawmode for mesh */
	switch (op_params->edge_mode) {

		case EDGE_MODE_TAG_SEAM:
			me->drawflag |= ME_DRAWSEAMS;
			ED_uvedit_live_unwrap(scene, obedit);
			break;
		case EDGE_MODE_TAG_SHARP:
			me->drawflag |= ME_DRAWSHARP;
			break;
		case EDGE_MODE_TAG_CREASE:
			me->drawflag |= ME_DRAWCREASES;
			break;
		case EDGE_MODE_TAG_BEVEL:
			me->drawflag |= ME_DRAWBWEIGHTS;
			break;
#ifdef WITH_FREESTYLE
		case EDGE_MODE_TAG_FREESTYLE:
			me->drawflag |= ME_DRAW_FREESTYLE_EDGE;
			break;
#endif
	}

	EDBM_update_generic(em, false, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Path
 * \{ */

/* callbacks */
static bool facetag_filter_cb(BMFace *f, void *UNUSED(user_data_v))
{
	return !BM_elem_flag_test(f, BM_ELEM_HIDDEN);
}
//static bool facetag_test_cb(Scene *UNUSED(scene), BMesh *UNUSED(bm), BMFace *f)
static bool facetag_test_cb(BMFace *f, void *UNUSED(user_data_v))
{
	return BM_elem_flag_test_bool(f, BM_ELEM_SELECT);
}
//static void facetag_set_cb(BMesh *bm, Scene *UNUSED(scene), BMFace *f, const bool val)
static void facetag_set_cb(BMFace *f, bool val, void *user_data_v)
{
	struct UserData *user_data = user_data_v;
	BM_face_select_set(user_data->bm, f, val);
}

static void mouse_mesh_shortest_path_face(
        Scene *UNUSED(scene), Object *obedit, const struct PathSelectParams *op_params,
        BMFace *f_act, BMFace *f_dst)
{
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;

	struct UserData user_data = {bm, obedit->data, op_params};
	LinkNode *path = NULL;
	bool is_path_ordered = false;

	if (f_act) {
		if (op_params->use_fill) {
			path = BM_mesh_calc_path_region_face(
			        bm, (BMElem *)f_act, (BMElem *)f_dst,
			        facetag_filter_cb, &user_data);
		}
		else {
			is_path_ordered = true;
			path = BM_mesh_calc_path_face(
			        bm, f_act, f_dst,
			        &(const struct BMCalcPathParams) {
			            .use_topology_distance = op_params->use_topology_distance,
			            .use_step_face = op_params->use_face_step,
			        },
			        facetag_filter_cb, &user_data);
		}

		if (f_act != f_dst) {
			if (path) {
				if (op_params->track_active) {
					BM_select_history_remove(bm, f_act);
				}
			}
		}
	}

	BMFace *f_dst_last = f_dst;

	if (path) {
		/* toggle the flag */
		bool all_set = true;
		LinkNode *node;

		node = path;
		do {
			if (!facetag_test_cb((BMFace *)node->link, &user_data)) {
				all_set = false;
				break;
			}
		} while ((node = node->next));

		/* We need to start as if just *after* a 'skip' block... */
		int depth = op_params->interval_params.skip;
		node = path;
		do {
			if ((is_path_ordered == false) ||
			    WM_operator_properties_checker_interval_test(&op_params->interval_params, depth))
			{
				facetag_set_cb((BMFace *)node->link, !all_set, &user_data);
				if (is_path_ordered) {
					f_dst_last = node->link;
				}
			}
		} while ((void)depth++, (node = node->next));

		BLI_linklist_free(path, NULL);
	}
	else {
		const bool is_act = !facetag_test_cb(f_dst, &user_data);
		facetag_set_cb(f_dst, is_act, &user_data); /* switch the face option */
	}

	EDBM_selectmode_flush(em);

	if (op_params->track_active) {
		/* even if this is selected it may not be in the selection list */
		if (facetag_test_cb(f_dst_last, &user_data) == 0) {
			BM_select_history_remove(bm, f_dst_last);
		}
		else {
			BM_select_history_store(bm, f_dst_last);
		}
		BM_mesh_active_face_set(bm, f_dst_last);
	}

	EDBM_update_generic(em, false, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Operator for vert/edge/face tag
 * \{ */

static bool edbm_shortest_path_pick_ex(
        Scene *scene, Object *obedit, const struct PathSelectParams *op_params,
        BMElem *ele_src, BMElem *ele_dst)
{

	if (ELEM(NULL, ele_src, ele_dst) && (ele_src->head.htype != ele_dst->head.htype)) {
		/* pass */
	}
	else if (ele_src->head.htype == BM_VERT) {
		mouse_mesh_shortest_path_vert(scene, obedit, op_params, (BMVert *)ele_src, (BMVert *)ele_dst);
		return true;
	}
	else if (ele_src->head.htype == BM_EDGE) {
		mouse_mesh_shortest_path_edge(scene, obedit, op_params, (BMEdge *)ele_src, (BMEdge *)ele_dst);
		return true;
	}
	else if (ele_src->head.htype == BM_FACE) {
		mouse_mesh_shortest_path_face(scene, obedit, op_params, (BMFace *)ele_src, (BMFace *)ele_dst);
		return true;
	}

	return false;
}

static int edbm_shortest_path_pick_exec(bContext *C, wmOperator *op);

static BMElem *edbm_elem_find_nearest(ViewContext *vc, const char htype)
{
	BMEditMesh *em = vc->em;
	float dist = ED_view3d_select_dist_px();

	if ((em->selectmode & SCE_SELECT_VERTEX) && (htype == BM_VERT)) {
		return (BMElem *)EDBM_vert_find_nearest(vc, &dist);
	}
	else if ((em->selectmode & SCE_SELECT_EDGE) && (htype == BM_EDGE)) {
		return (BMElem *)EDBM_edge_find_nearest(vc, &dist);
	}
	else if ((em->selectmode & SCE_SELECT_FACE) && (htype == BM_FACE)) {
		return (BMElem *)EDBM_face_find_nearest(vc, &dist);
	}

	return NULL;
}

static BMElem *edbm_elem_active_elem_or_face_get(BMesh *bm)
{
	BMElem *ele = BM_mesh_active_elem_get(bm);

	if ((ele == NULL) && bm->act_face && BM_elem_flag_test(bm->act_face, BM_ELEM_SELECT)) {
		ele = (BMElem *)bm->act_face;
	}

	return ele;
}

static int edbm_shortest_path_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	if (RNA_struct_property_is_set(op->ptr, "index")) {
		return edbm_shortest_path_pick_exec(C, op);
	}

	ViewContext vc;
	BMEditMesh *em;
	bool track_active = true;

	em_setup_viewcontext(C, &vc);
	copy_v2_v2_int(vc.mval, event->mval);
	em = vc.em;

	view3d_operator_needs_opengl(C);

	BMElem *ele_src, *ele_dst;
	if (!(ele_src = edbm_elem_active_elem_or_face_get(em->bm)) ||
	    !(ele_dst = edbm_elem_find_nearest(&vc, ele_src->head.htype)))
	{
		/* special case, toggle edge tags even when we don't have a path */
		if (((em->selectmode & SCE_SELECT_EDGE) &&
		     (vc.scene->toolsettings->edge_mode != EDGE_MODE_SELECT)) &&
		    /* check if we only have a destination edge */
		    ((ele_src == NULL) &&
		     (ele_dst = edbm_elem_find_nearest(&vc, BM_EDGE))))
		{
			ele_src = ele_dst;
			track_active = false;
		}
		else {
			return OPERATOR_PASS_THROUGH;
		}
	}

	struct PathSelectParams op_params;

	path_select_params_from_op(op, &op_params);
	op_params.track_active = track_active;
	op_params.edge_mode = vc.scene->toolsettings->edge_mode;

	if (!edbm_shortest_path_pick_ex(vc.scene, vc.obedit, &op_params, ele_src, ele_dst)) {
		return OPERATOR_PASS_THROUGH;
	}

	/* to support redo */
	BM_mesh_elem_index_ensure(em->bm, ele_dst->head.htype);
	int index = EDBM_elem_to_index_any(em, ele_dst);

	RNA_int_set(op->ptr, "index", index);

	return OPERATOR_FINISHED;
}

static int edbm_shortest_path_pick_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;

	const int index = RNA_int_get(op->ptr, "index");
	if (index < 0 || index >= (bm->totvert + bm->totedge + bm->totface)) {
		return OPERATOR_CANCELLED;
	}

	BMElem *ele_src, *ele_dst;
	if (!(ele_src = edbm_elem_active_elem_or_face_get(em->bm)) ||
	    !(ele_dst = EDBM_elem_from_index_any(em, index)))
	{
		return OPERATOR_CANCELLED;
	}

	struct PathSelectParams op_params;
	path_select_params_from_op(op, &op_params);
	op_params.track_active = true;
	op_params.edge_mode = scene->toolsettings->edge_mode;

	if (!edbm_shortest_path_pick_ex(scene, obedit, &op_params, ele_src, ele_dst)) {
		return OPERATOR_CANCELLED;
	}

	return OPERATOR_FINISHED;
}

void MESH_OT_shortest_path_pick(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Pick Shortest Path";
	ot->idname = "MESH_OT_shortest_path_pick";
	ot->description = "Select shortest path between two selections";

	/* api callbacks */
	ot->invoke = edbm_shortest_path_pick_invoke;
	ot->exec = edbm_shortest_path_pick_exec;
	ot->poll = ED_operator_editmesh_region_view3d;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	path_select_properties(ot);

	/* use for redo */
	prop = RNA_def_int(ot->srna, "index", -1, -1, INT_MAX, "", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Path Between Existing Selection
 * \{ */

static int edbm_shortest_path_select_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	bool found_valid_elements = false;

	ViewLayer *view_layer = CTX_data_view_layer(C);
	uint objects_len = 0;
	Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(view_layer, &objects_len);
	for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
		Object *obedit = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		BMesh *bm = em->bm;
		BMIter iter;
		BMEditSelection *ese_src, *ese_dst;
		BMElem *ele_src = NULL, *ele_dst = NULL, *ele;

		if ((em->bm->totvertsel == 0) &&
		    (em->bm->totedgesel == 0) &&
		    (em->bm->totfacesel == 0))
		{
			continue;
		}

		/* first try to find vertices in edit selection */
		ese_src = bm->selected.last;
		if (ese_src && (ese_dst = ese_src->prev) && (ese_src->htype  == ese_dst->htype)) {
			ele_src = ese_src->ele;
			ele_dst = ese_dst->ele;
		}
		else {
			/* if selection history isn't available, find two selected elements */
			ele_src = ele_dst = NULL;
			if ((em->selectmode & SCE_SELECT_VERTEX) && (bm->totvertsel >= 2)) {
				BM_ITER_MESH (ele, &iter, bm, BM_VERTS_OF_MESH) {
					if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
						if      (ele_src == NULL) ele_src = ele;
						else if (ele_dst == NULL) ele_dst = ele;
						else                      break;
					}
				}
			}

			if ((ele_dst == NULL) && (em->selectmode & SCE_SELECT_EDGE) && (bm->totedgesel >= 2)) {
				ele_src = NULL;
				BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
					if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
						if      (ele_src == NULL) ele_src = ele;
						else if (ele_dst == NULL) ele_dst = ele;
						else                      break;
					}
				}
			}

			if ((ele_dst == NULL) && (em->selectmode & SCE_SELECT_FACE) && (bm->totfacesel >= 2)) {
				ele_src = NULL;
				BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
					if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
						if      (ele_src == NULL) ele_src = ele;
						else if (ele_dst == NULL) ele_dst = ele;
						else                      break;
					}
				}
			}
		}

		if (ele_src && ele_dst) {
			struct PathSelectParams op_params;
			path_select_params_from_op(op, &op_params);

			edbm_shortest_path_pick_ex(scene, obedit, &op_params, ele_src, ele_dst);

			found_valid_elements = true;
		}
	}
	MEM_freeN(objects);

	if (!found_valid_elements) {
		BKE_report(op->reports,
		           RPT_WARNING,
		           "Path selection requires two matching elements to be selected");
		return OPERATOR_CANCELLED;
	}

	return OPERATOR_FINISHED;
}

void MESH_OT_shortest_path_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Shortest Path";
	ot->idname = "MESH_OT_shortest_path_select";
	ot->description = "Selected shortest path between two vertices/edges/faces";

	/* api callbacks */
	ot->exec = edbm_shortest_path_select_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	path_select_properties(ot);
}

/** \} */
