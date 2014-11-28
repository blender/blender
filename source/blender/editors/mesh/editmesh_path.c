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

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_windowmanager_types.h"

#ifdef WITH_FREESTYLE
#  include "DNA_meshdata_types.h"
#endif

#include "BLI_math.h"
#include "BLI_linklist.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_report.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_types.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "mesh_intern.h"  /* own include */

struct UserData {
	BMesh *bm;
	Mesh  *me;
	Scene *scene;
};

/* -------------------------------------------------------------------- */
/* Vert Path */

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

static bool mouse_mesh_shortest_path_vert(ViewContext *vc)
{
	/* unlike edge/face versions, this uses a bmesh operator */

	BMEditMesh *em = vc->em;
	BMesh *bm = em->bm;
	BMVert *v_dst;
	float dist = ED_view3d_select_dist_px();
	const bool use_length = true;

	v_dst = EDBM_vert_find_nearest(vc, &dist, false, false);
	if (v_dst) {
		struct UserData user_data = {bm, vc->obedit->data, vc->scene};
		LinkNode *path = NULL;
		BMVert *v_act = BM_mesh_active_vert_get(bm);

		if (v_act && (v_act != v_dst)) {
			if ((path = BM_mesh_calc_path_vert(bm, v_act, v_dst, use_length,
			                                   verttag_filter_cb, &user_data)))
			{
				BM_select_history_remove(bm, v_act);
			}
		}

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

			node = path;
			do {
				verttag_set_cb((BMVert *)node->link, !all_set, &user_data);
			} while ((node = node->next));

			BLI_linklist_free(path, NULL);
		}
		else {
			const bool is_act = !verttag_test_cb(v_dst, &user_data);
			verttag_set_cb(v_dst, is_act, &user_data); /* switch the face option */
		}

		EDBM_selectmode_flush(em);

		/* even if this is selected it may not be in the selection list */
		if (BM_elem_flag_test(v_dst, BM_ELEM_SELECT) == 0)
			BM_select_history_remove(bm, v_dst);
		else
			BM_select_history_store(bm, v_dst);

		EDBM_update_generic(em, false, false);

		return true;
	}
	else {
		return false;
	}
}



/* -------------------------------------------------------------------- */
/* Edge Path */

/* callbacks */
static bool edgetag_filter_cb(BMEdge *e, void *UNUSED(user_data_v))
{
	return !BM_elem_flag_test(e, BM_ELEM_HIDDEN);
}
static bool edgetag_test_cb(BMEdge *e, void *user_data_v)
{
	struct UserData *user_data = user_data_v;
	Scene *scene = user_data->scene;
	BMesh *bm = user_data->bm;

	switch (scene->toolsettings->edge_mode) {
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
	Scene *scene = user_data->scene;
	BMesh *bm = user_data->bm;

	switch (scene->toolsettings->edge_mode) {
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

static void edgetag_ensure_cd_flag(Scene *scene, Mesh *me)
{
	BMesh *bm = me->edit_btmesh->bm;

	switch (scene->toolsettings->edge_mode) {
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
static bool mouse_mesh_shortest_path_edge(ViewContext *vc)
{
	BMEditMesh *em = vc->em;
	BMesh *bm = em->bm;
	BMEdge *e_dst;
	float dist = ED_view3d_select_dist_px();
	const bool use_length = true;

	e_dst = EDBM_edge_find_nearest(vc, &dist);
	if (e_dst) {
		const char edge_mode = vc->scene->toolsettings->edge_mode;
		struct UserData user_data = {bm, vc->obedit->data, vc->scene};
		LinkNode *path = NULL;
		Mesh *me = vc->obedit->data;
		BMEdge *e_act = BM_mesh_active_edge_get(bm);

		edgetag_ensure_cd_flag(vc->scene, em->ob->data);

		if (e_act && (e_act != e_dst)) {
			if ((path = BM_mesh_calc_path_edge(bm, e_act, e_dst, use_length,
			                                   edgetag_filter_cb, &user_data)))
			{
				BM_select_history_remove(bm, e_act);
			}
		}

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

			node = path;
			do {
				edgetag_set_cb((BMEdge *)node->link, !all_set, &user_data);
			} while ((node = node->next));

			BLI_linklist_free(path, NULL);
		}
		else {
			const bool is_act = !edgetag_test_cb(e_dst, &user_data);
			edgetag_ensure_cd_flag(vc->scene, vc->obedit->data);
			edgetag_set_cb(e_dst, is_act, &user_data); /* switch the edge option */
		}

		if (edge_mode != EDGE_MODE_SELECT) {
			/* simple rules - last edge is _always_ active and selected */
			if (e_act)
				BM_edge_select_set(bm, e_act, false);
			BM_edge_select_set(bm, e_dst, true);
			BM_select_history_store(bm, e_dst);
		}

		EDBM_selectmode_flush(em);

		/* even if this is selected it may not be in the selection list */
		if (edge_mode == EDGE_MODE_SELECT) {
			if (edgetag_test_cb(e_dst, &user_data) == 0)
				BM_select_history_remove(bm, e_dst);
			else
				BM_select_history_store(bm, e_dst);
		}

		/* force drawmode for mesh */
		switch (edge_mode) {

			case EDGE_MODE_TAG_SEAM:
				me->drawflag |= ME_DRAWSEAMS;
				ED_uvedit_live_unwrap(vc->scene, vc->obedit);
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

		return true;
	}
	else {
		return false;
	}
}



/* -------------------------------------------------------------------- */
/* Face Path */

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

static bool mouse_mesh_shortest_path_face(ViewContext *vc)
{
	BMEditMesh *em = vc->em;
	BMesh *bm = em->bm;
	BMFace *f_dst;
	float dist = ED_view3d_select_dist_px();
	const bool use_length = true;

	f_dst = EDBM_face_find_nearest(vc, &dist);
	if (f_dst) {
		struct UserData user_data = {bm, vc->obedit->data, vc->scene};
		LinkNode *path = NULL;
		BMFace *f_act = BM_mesh_active_face_get(bm, false, true);

		if (f_act) {
			if (f_act != f_dst) {
				if ((path = BM_mesh_calc_path_face(bm, f_act, f_dst, use_length,
				                                   facetag_filter_cb, &user_data)))
				{
					BM_select_history_remove(bm, f_act);
				}
			}
		}

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

			node = path;
			do {
				facetag_set_cb((BMFace *)node->link, !all_set, &user_data);
			} while ((node = node->next));

			BLI_linklist_free(path, NULL);
		}
		else {
			const bool is_act = !facetag_test_cb(f_dst, &user_data);
			facetag_set_cb(f_dst, is_act, &user_data); /* switch the face option */
		}

		EDBM_selectmode_flush(em);

		/* even if this is selected it may not be in the selection list */
		if (facetag_test_cb(f_dst, &user_data) == 0)
			BM_select_history_remove(bm, f_dst);
		else
			BM_select_history_store(bm, f_dst);

		BM_mesh_active_face_set(bm, f_dst);

		EDBM_update_generic(em, false, false);

		return true;
	}
	else {
		return false;
	}
}



/* -------------------------------------------------------------------- */
/* Main Operator for vert/edge/face tag */

static int edbm_shortest_path_pick_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	ViewContext vc;
	BMEditMesh *em;
	BMElem *ele;


	em_setup_viewcontext(C, &vc);
	copy_v2_v2_int(vc.mval, event->mval);
	em = vc.em;

	ele = BM_mesh_active_elem_get(em->bm);
	if (ele == NULL) {
		return OPERATOR_PASS_THROUGH;
	}

	view3d_operator_needs_opengl(C);

	if ((em->selectmode & SCE_SELECT_VERTEX) && (ele->head.htype == BM_VERT)) {
		if (mouse_mesh_shortest_path_vert(&vc)) {
			return OPERATOR_FINISHED;
		}
		else {
			return OPERATOR_PASS_THROUGH;
		}
	}
	else if ((em->selectmode & SCE_SELECT_EDGE) && (ele->head.htype == BM_EDGE)) {
		if (mouse_mesh_shortest_path_edge(&vc)) {
			return OPERATOR_FINISHED;
		}
		else {
			return OPERATOR_PASS_THROUGH;
		}
	}
	else if ((em->selectmode & SCE_SELECT_FACE) && (ele->head.htype == BM_FACE)) {
		if (mouse_mesh_shortest_path_face(&vc)) {
			return OPERATOR_FINISHED;
		}
		else {
			return OPERATOR_PASS_THROUGH;
		}
	}

	return OPERATOR_PASS_THROUGH;
}

void MESH_OT_shortest_path_pick(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Pick Shortest Path";
	ot->idname = "MESH_OT_shortest_path_pick";
	ot->description = "Select shortest path between two selections";

	/* api callbacks */
	ot->invoke = edbm_shortest_path_pick_invoke;
	ot->poll = ED_operator_editmesh_region_view3d;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}



/* -------------------------------------------------------------------- */
/* Select path between existing selection */

static int edbm_shortest_path_select_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMesh *bm = em->bm;
	BMIter iter;
	BMEditSelection *ese_src, *ese_dst;
	BMElem *ele_src = NULL, *ele_dst = NULL, *ele;

	const bool use_length = RNA_boolean_get(op->ptr, "use_length");

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
		LinkNode *path = NULL;
		switch (ele_src->head.htype) {
			case BM_VERT:
				path = BM_mesh_calc_path_vert(
				           bm, (BMVert *)ele_src, (BMVert *)ele_dst, use_length,
				           BM_elem_cb_check_hflag_disabled_simple(BMVert *, BM_ELEM_HIDDEN));
				break;
			case BM_EDGE:
				path = BM_mesh_calc_path_edge(
				           bm, (BMEdge *)ele_src, (BMEdge *)ele_dst, use_length,
				           BM_elem_cb_check_hflag_disabled_simple(BMEdge *, BM_ELEM_HIDDEN));
				break;
			case BM_FACE:
				path = BM_mesh_calc_path_face(
				           bm, (BMFace *)ele_src, (BMFace *)ele_dst, use_length,
				           BM_elem_cb_check_hflag_disabled_simple(BMFace *, BM_ELEM_HIDDEN));
				break;
		}

		if (path) {
			LinkNode *node = path;

			do {
				BM_elem_select_set(bm, node->link, true);
			} while ((node = node->next));

			BLI_linklist_free(path, NULL);
		}
		else {
			BKE_report(op->reports, RPT_WARNING, "Path can't be found");
			return OPERATOR_CANCELLED;
		}

		EDBM_selectmode_flush(em);
		EDBM_update_generic(em, false, false);

		return OPERATOR_FINISHED;
	}
	else {
		BKE_report(op->reports, RPT_WARNING, "Path selection requires two matching elements to be selected");
		return OPERATOR_CANCELLED;
	}
}

void MESH_OT_shortest_path_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Shortest Path";
	ot->idname = "MESH_OT_shortest_path_select";
	ot->description = "Selected vertex path between two vertices";

	/* api callbacks */
	ot->exec = edbm_shortest_path_select_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "use_length", true, "Length", "Use length when measuring distance");
}
