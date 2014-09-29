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

/** \file blender/editors/mesh/editmesh_tools.c
 *  \ingroup edmesh
 */

#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_noise.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_sort_utils.h"

#include "BKE_material.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_report.h"
#include "BKE_texture.h"
#include "BKE_main.h"
#include "BKE_editmesh.h"

#include "BLF_translation.h"

#include "RNA_define.h"
#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"

#include "RE_render_ext.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "mesh_intern.h"  /* own include */

#define USE_FACE_CREATE_SEL_EXTEND

static int edbm_subdivide_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const int cuts = RNA_int_get(op->ptr, "number_cuts");
	float smooth = 0.292f * RNA_float_get(op->ptr, "smoothness");
	const float fractal = RNA_float_get(op->ptr, "fractal") / 2.5f;
	const float along_normal = RNA_float_get(op->ptr, "fractal_along_normal");

	if (RNA_boolean_get(op->ptr, "quadtri") && 
	    RNA_enum_get(op->ptr, "quadcorner") == SUBD_STRAIGHT_CUT)
	{
		RNA_enum_set(op->ptr, "quadcorner", SUBD_INNERVERT);
	}
	
	BM_mesh_esubdivide(em->bm, BM_ELEM_SELECT,
	                   smooth, SUBD_FALLOFF_ROOT, false,
	                   fractal, along_normal,
	                   cuts,
	                   SUBDIV_SELECT_ORIG, RNA_enum_get(op->ptr, "quadcorner"),
	                   RNA_boolean_get(op->ptr, "quadtri"), true, false,
	                   RNA_int_get(op->ptr, "seed"));

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

/* Note, these values must match delete_mesh() event values */
static EnumPropertyItem prop_mesh_cornervert_types[] = {
	{SUBD_INNERVERT,     "INNERVERT", 0,      "Inner Vert", ""},
	{SUBD_PATH,          "PATH", 0,           "Path", ""},
	{SUBD_STRAIGHT_CUT,  "STRAIGHT_CUT", 0,   "Straight Cut", ""},
	{SUBD_FAN,           "FAN", 0,            "Fan", ""},
	{0, NULL, 0, NULL, NULL}
};

void MESH_OT_subdivide(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Subdivide";
	ot->description = "Subdivide selected edges";
	ot->idname = "MESH_OT_subdivide";

	/* api callbacks */
	ot->exec = edbm_subdivide_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_int(ot->srna, "number_cuts", 1, 1, INT_MAX, "Number of Cuts", "", 1, 10);
	/* avoid re-using last var because it can cause _very_ high poly meshes and annoy users (or worse crash) */
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	RNA_def_float(ot->srna, "smoothness", 0.0f, 0.0f, FLT_MAX, "Smoothness", "Smoothness factor", 0.0f, 1.0f);

	RNA_def_boolean(ot->srna, "quadtri", 0, "Quad/Tri Mode", "Tries to prevent ngons");
	RNA_def_enum(ot->srna, "quadcorner", prop_mesh_cornervert_types, SUBD_STRAIGHT_CUT,
	             "Quad Corner Type", "How to subdivide quad corners (anything other than Straight Cut will prevent ngons)");

	RNA_def_float(ot->srna, "fractal", 0.0f, 0.0f, FLT_MAX, "Fractal", "Fractal randomness factor", 0.0f, 1000.0f);
	RNA_def_float(ot->srna, "fractal_along_normal", 0.0f, 0.0f, 1.0f, "Along Normal", "Apply fractal displacement along normal only", 0.0f, 1.0f);
	RNA_def_int(ot->srna, "seed", 0, 0, 10000, "Random Seed", "Seed for the random number generator", 0, 50);
}

/* -------------------------------------------------------------------- */
/* Edge Ring Subdiv
 * (bridge code shares props)
 */

struct EdgeRingOpSubdProps {
	int interp_mode;
	int cuts;
	float smooth;

	int profile_shape;
	float profile_shape_factor;
};


static void mesh_operator_edgering_props(wmOperatorType *ot, const int cuts_default)
{
	/* Note, these values must match delete_mesh() event values */
	static EnumPropertyItem prop_subd_edgering_types[] = {
		{SUBD_RING_INTERP_LINEAR, "LINEAR", 0, "Linear", ""},
		{SUBD_RING_INTERP_PATH, "PATH", 0, "Blend Path", ""},
		{SUBD_RING_INTERP_SURF, "SURFACE", 0, "Blend Surface", ""},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;

	prop = RNA_def_int(ot->srna, "number_cuts", cuts_default, 0, INT_MAX, "Number of Cuts", "", 0, 64);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	RNA_def_enum(ot->srna, "interpolation", prop_subd_edgering_types, SUBD_RING_INTERP_PATH,
	             "Interpolation", "Interpolation method");

	RNA_def_float(ot->srna, "smoothness", 1.0f, 0.0f, FLT_MAX,
	              "Smoothness", "Smoothness factor", 0.0f, 2.0f);

	/* profile-shape */
	RNA_def_float(ot->srna, "profile_shape_factor", 0.0f, -FLT_MAX, FLT_MAX,
	              "Profile Factor", "", -2.0f, 2.0f);

	prop = RNA_def_property(ot->srna, "profile_shape", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, proportional_falloff_curve_only_items);
	RNA_def_property_enum_default(prop, PROP_SMOOTH);
	RNA_def_property_ui_text(prop, "Profile Shape", "Shape of the profile");
	RNA_def_property_translation_context(prop, BLF_I18NCONTEXT_ID_CURVE); /* Abusing id_curve :/ */
}

static void mesh_operator_edgering_props_get(wmOperator *op, struct EdgeRingOpSubdProps *op_props)
{
	op_props->interp_mode = RNA_enum_get(op->ptr, "interpolation");
	op_props->cuts = RNA_int_get(op->ptr, "number_cuts");
	op_props->smooth = RNA_float_get(op->ptr, "smoothness");

	op_props->profile_shape = RNA_enum_get(op->ptr, "profile_shape");
	op_props->profile_shape_factor = RNA_float_get(op->ptr, "profile_shape_factor");
}

static int edbm_subdivide_edge_ring_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	struct EdgeRingOpSubdProps op_props;

	mesh_operator_edgering_props_get(op, &op_props);

	if (!EDBM_op_callf(em, op,
	                   "subdivide_edgering edges=%he interp_mode=%i cuts=%i smooth=%f "
	                   "profile_shape=%i profile_shape_factor=%f",
	                   BM_ELEM_SELECT, op_props.interp_mode, op_props.cuts, op_props.smooth,
	                   op_props.profile_shape, op_props.profile_shape_factor))
	{
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_subdivide_edgering(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Subdivide Edge-Ring";
	ot->description = "";
	ot->idname = "MESH_OT_subdivide_edgering";

	/* api callbacks */
	ot->exec = edbm_subdivide_edge_ring_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	mesh_operator_edgering_props(ot, 10);
}


static int edbm_unsubdivide_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMOperator bmop;

	const int iterations = RNA_int_get(op->ptr, "iterations");

	EDBM_op_init(em, &bmop, op,
	             "unsubdivide verts=%hv iterations=%i", BM_ELEM_SELECT, iterations);

	BMO_op_exec(em->bm, &bmop);

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return 0;
	}

	if ((em->selectmode & SCE_SELECT_VERTEX) == 0) {
		EDBM_selectmode_flush_ex(em, SCE_SELECT_VERTEX);  /* need to flush vert->face first */
	}
	EDBM_selectmode_flush(em);

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_unsubdivide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Un-Subdivide";
	ot->description = "UnSubdivide selected edges & faces";
	ot->idname = "MESH_OT_unsubdivide";

	/* api callbacks */
	ot->exec = edbm_unsubdivide_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_int(ot->srna, "iterations", 2, 1, INT_MAX, "Iterations", "Number of times to unsubdivide", 1, 100);
}

void EMBM_project_snap_verts(bContext *C, ARegion *ar, BMEditMesh *em)
{
	Object *obedit = em->ob;
	BMIter iter;
	BMVert *eve;

	ED_view3d_init_mats_rv3d(obedit, ar->regiondata);

	BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
			float mval[2], co_proj[3], no_dummy[3];
			float dist_px_dummy;
			if (ED_view3d_project_float_object(ar, eve->co, mval, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
				if (snapObjectsContext(C, mval, &dist_px_dummy, co_proj, no_dummy, SNAP_NOT_OBEDIT)) {
					mul_v3_m4v3(eve->co, obedit->imat, co_proj);
				}
			}
		}
	}
}

static void edbm_report_delete_info(ReportList *reports, BMesh *bm, const int totelem[3])
{
	BKE_reportf(reports, RPT_INFO,
	            "Removed: %d vertices, %d edges, %d faces",
	            totelem[0] - bm->totvert, totelem[1] - bm->totedge, totelem[2] - bm->totface);
}

/* Note, these values must match delete_mesh() event values */
static EnumPropertyItem prop_mesh_delete_types[] = {
	{0, "VERT",      0, "Vertices", ""},
	{1,  "EDGE",      0, "Edges", ""},
	{2,  "FACE",      0, "Faces", ""},
	{3,  "EDGE_FACE", 0, "Only Edges & Faces", ""},
	{4,  "ONLY_FACE", 0, "Only Faces", ""},
	{0, NULL, 0, NULL, NULL}
};

static int edbm_delete_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const int type = RNA_enum_get(op->ptr, "type");

	if (type == 0) {
		if (!EDBM_op_callf(em, op, "delete geom=%hv context=%i", BM_ELEM_SELECT, DEL_VERTS)) /* Erase Vertices */
			return OPERATOR_CANCELLED;
	}
	else if (type == 1) {
		if (!EDBM_op_callf(em, op, "delete geom=%he context=%i", BM_ELEM_SELECT, DEL_EDGES)) /* Erase Edges */
			return OPERATOR_CANCELLED;
	}
	else if (type == 2) {
		if (!EDBM_op_callf(em, op, "delete geom=%hf context=%i", BM_ELEM_SELECT, DEL_FACES)) /* Erase Faces */
			return OPERATOR_CANCELLED;
	}
	else if (type == 3) {
		if (!EDBM_op_callf(em, op, "delete geom=%hef context=%i", BM_ELEM_SELECT, DEL_EDGESFACES)) /* Edges and Faces */
			return OPERATOR_CANCELLED;
	}
	else if (type == 4) {
		//"Erase Only Faces";
		if (!EDBM_op_callf(em, op, "delete geom=%hf context=%i",
		                   BM_ELEM_SELECT, DEL_ONLYFACES))
		{
			return OPERATOR_CANCELLED;
		}
	}

	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	EDBM_update_generic(em, true, true);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete";
	ot->description = "Delete selected vertices, edges or faces";
	ot->idname = "MESH_OT_delete";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = edbm_delete_exec;
	
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_mesh_delete_types, 0, "Type", "Method used for deleting mesh data");
}


static bool bm_face_is_loose(BMFace *f)
{
	BMLoop *l_iter, *l_first;

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		if (!BM_edge_is_boundary(l_iter->e)) {
			return false;
		}
	} while ((l_iter = l_iter->next) != l_first);

	return true;
}

static int edbm_delete_loose_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMIter iter;

	const bool use_verts = (RNA_boolean_get(op->ptr, "use_verts") && bm->totvertsel);
	const bool use_edges = (RNA_boolean_get(op->ptr, "use_edges") && bm->totedgesel);
	const bool use_faces = (RNA_boolean_get(op->ptr, "use_faces") && bm->totfacesel);

	const int totelem[3] = {bm->totvert, bm->totedge, bm->totface};


	BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

	if (use_faces) {
		BMFace *f;

		BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
			if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
				BM_elem_flag_set(f, BM_ELEM_TAG, bm_face_is_loose(f));
			}
		}

		BM_mesh_delete_hflag_context(bm, BM_ELEM_TAG, DEL_FACES);
	}

	if (use_edges) {
		BMEdge *e;

		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
				BM_elem_flag_set(e, BM_ELEM_TAG, BM_edge_is_wire(e));
			}
		}

		BM_mesh_delete_hflag_context(bm, BM_ELEM_TAG, DEL_EDGES);
	}

	if (use_verts) {
		BMVert *v;

		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
				BM_elem_flag_set(v, BM_ELEM_TAG, (v->e == NULL));
			}
		}

		BM_mesh_delete_hflag_context(bm, BM_ELEM_TAG, DEL_VERTS);
	}

	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	EDBM_update_generic(em, true, true);

	edbm_report_delete_info(op->reports, bm, totelem);

	return OPERATOR_FINISHED;
}


void MESH_OT_delete_loose(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Loose";
	ot->description = "Delete loose vertices, edges or faces";
	ot->idname = "MESH_OT_delete_loose";

	/* api callbacks */
	ot->exec = edbm_delete_loose_exec;

	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_boolean(ot->srna, "use_verts", true, "Vertices", "Remove loose vertices");
	RNA_def_boolean(ot->srna, "use_edges", true, "Edges", "Remove loose edges");
	RNA_def_boolean(ot->srna, "use_faces", false, "Faces", "Remove loose faces");
}


static int edbm_collapse_edge_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	if (!EDBM_op_callf(em, op, "collapse edges=%he", BM_ELEM_SELECT))
		return OPERATOR_CANCELLED;

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_edge_collapse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Edge Collapse";
	ot->description = "Collapse selected edges";
	ot->idname = "MESH_OT_edge_collapse";

	/* api callbacks */
	ot->exec = edbm_collapse_edge_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int edbm_add_edge_face__smooth_get(BMesh *bm)
{
	BMEdge *e;
	BMIter iter;

	unsigned int vote_on_smooth[2] = {0, 0};

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_SELECT) && e->l) {
			vote_on_smooth[BM_elem_flag_test_bool(e->l->f, BM_ELEM_SMOOTH)]++;
		}
	}

	return (vote_on_smooth[0] < vote_on_smooth[1]);
}

#ifdef USE_FACE_CREATE_SEL_EXTEND
/**
 * Function used to get a fixed number of edges linked to a vertex that passes a test function.
 * This is used so we can request all boundary edges connected to a vertex for eg.
 */
static int edbm_add_edge_face_exec__vert_edge_lookup(
        BMVert *v, BMEdge *e_used, BMEdge **e_arr, const int e_arr_len,
        bool (* func)(const BMEdge *))
{
	BMIter iter;
	BMEdge *e_iter;
	int i = 0;
	BM_ITER_ELEM (e_iter, &iter, v, BM_EDGES_OF_VERT) {
		if (BM_elem_flag_test(e_iter, BM_ELEM_HIDDEN) == false) {
			if ((e_used == NULL) || (e_used != e_iter)) {
				if (func(e_iter)) {
					e_arr[i++] = e_iter;
					if (i >= e_arr_len) {
						break;
					}
				}
			}
		}
	}
	return i;
}

static BMElem *edbm_add_edge_face_exec__tricky_extend_sel(BMesh *bm)
{
	BMIter iter;
	bool found = false;

	if (bm->totvertsel == 1 && bm->totedgesel == 0 && bm->totfacesel == 0) {
		/* first look for 2 boundary edges */
		BMVert *v;

		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
				found = true;
				break;
			}
		}

		if (found) {
			BMEdge *ed_pair[3];
			if (
			    ((edbm_add_edge_face_exec__vert_edge_lookup(v, NULL, ed_pair, 3, BM_edge_is_wire) == 2) &&
			     (BM_edge_share_face_check(ed_pair[0], ed_pair[1]) == false)) ||

			    ((edbm_add_edge_face_exec__vert_edge_lookup(v, NULL, ed_pair, 3, BM_edge_is_boundary) == 2) &&
			     (BM_edge_share_face_check(ed_pair[0], ed_pair[1]) == false))
			    )
			{
				BMEdge *e_other = BM_edge_exists(BM_edge_other_vert(ed_pair[0], v),
				                                 BM_edge_other_vert(ed_pair[1], v));
				BM_edge_select_set(bm, ed_pair[0], true);
				BM_edge_select_set(bm, ed_pair[1], true);
				if (e_other) {
					BM_edge_select_set(bm, e_other, true);
				}
				return (BMElem *)v;
			}
		}
	}
	else if (bm->totvertsel == 2 && bm->totedgesel == 1 && bm->totfacesel == 0) {
		/* first look for 2 boundary edges */
		BMEdge *e;

		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
				found = true;
				break;
			}
		}
		if (found) {
			BMEdge *ed_pair_v1[2];
			BMEdge *ed_pair_v2[2];
			if (
			    ((edbm_add_edge_face_exec__vert_edge_lookup(e->v1, e, ed_pair_v1, 2, BM_edge_is_wire) == 1) &&
			     (edbm_add_edge_face_exec__vert_edge_lookup(e->v2, e, ed_pair_v2, 2, BM_edge_is_wire) == 1) &&
			     (BM_edge_share_face_check(e, ed_pair_v1[0]) == false) &&
			     (BM_edge_share_face_check(e, ed_pair_v2[0]) == false)) ||

#if 1  /* better support mixed cases [#37203] */
			    ((edbm_add_edge_face_exec__vert_edge_lookup(e->v1, e, ed_pair_v1, 2, BM_edge_is_wire)     == 1) &&
			     (edbm_add_edge_face_exec__vert_edge_lookup(e->v2, e, ed_pair_v2, 2, BM_edge_is_boundary) == 1) &&
			     (BM_edge_share_face_check(e, ed_pair_v1[0]) == false) &&
			     (BM_edge_share_face_check(e, ed_pair_v2[0]) == false)) ||

			    ((edbm_add_edge_face_exec__vert_edge_lookup(e->v1, e, ed_pair_v1, 2, BM_edge_is_boundary) == 1) &&
			     (edbm_add_edge_face_exec__vert_edge_lookup(e->v2, e, ed_pair_v2, 2, BM_edge_is_wire)     == 1) &&
			     (BM_edge_share_face_check(e, ed_pair_v1[0]) == false) &&
			     (BM_edge_share_face_check(e, ed_pair_v2[0]) == false)) ||
#endif

			    ((edbm_add_edge_face_exec__vert_edge_lookup(e->v1, e, ed_pair_v1, 2, BM_edge_is_boundary) == 1) &&
			     (edbm_add_edge_face_exec__vert_edge_lookup(e->v2, e, ed_pair_v2, 2, BM_edge_is_boundary) == 1) &&
			     (BM_edge_share_face_check(e, ed_pair_v1[0]) == false) &&
			     (BM_edge_share_face_check(e, ed_pair_v2[0]) == false))
			    )
			{
				BMVert *v1_other = BM_edge_other_vert(ed_pair_v1[0], e->v1);
				BMVert *v2_other = BM_edge_other_vert(ed_pair_v2[0], e->v2);
				BMEdge *e_other = (v1_other != v2_other) ? BM_edge_exists(v1_other, v2_other) : NULL;
				BM_edge_select_set(bm, ed_pair_v1[0], true);
				BM_edge_select_set(bm, ed_pair_v2[0], true);
				if (e_other) {
					BM_edge_select_set(bm, e_other, true);
				}
				return (BMElem *)e;
			}
		}
	}

	return NULL;
}
static void edbm_add_edge_face_exec__tricky_finalize_sel(BMesh *bm, BMElem *ele_desel, BMFace *f)
{
	/* now we need to find the edge that isnt connected to this element */
	BM_select_history_clear(bm);

	if (ele_desel->head.htype == BM_VERT) {
		BMLoop *l = BM_face_vert_share_loop(f, (BMVert *)ele_desel);
		BLI_assert(f->len == 3);
		BM_face_select_set(bm, f, false);
		BM_vert_select_set(bm, (BMVert *)ele_desel, false);

		BM_edge_select_set(bm, l->next->e, true);
		BM_select_history_store(bm, l->next->e);
	}
	else {
		BMLoop *l = BM_face_edge_share_loop(f, (BMEdge *)ele_desel);
		BLI_assert(f->len == 4 || f->len == 3);
		BM_face_select_set(bm, f, false);
		BM_edge_select_set(bm, (BMEdge *)ele_desel, false);
		if (f->len == 4) {
			BM_edge_select_set(bm, l->next->next->e, true);
			BM_select_history_store(bm, l->next->next->e);
		}
		else {
			BM_vert_select_set(bm, l->next->next->v, true);
			BM_select_history_store(bm, l->next->next->v);
		}
	}
}
#endif  /* USE_FACE_CREATE_SEL_EXTEND */

static int edbm_add_edge_face_exec(bContext *C, wmOperator *op)
{
	BMOperator bmop;
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const short use_smooth = edbm_add_edge_face__smooth_get(em->bm);
	const int totedge_orig = em->bm->totedge;
	const int totface_orig = em->bm->totface;
	/* when this is used to dissolve we could avoid this, but checking isnt too slow */

#ifdef USE_FACE_CREATE_SEL_EXTEND
	BMElem *ele_desel;
	BMFace *ele_desel_face;

	/* be extra clever, figure out if a partial selection should be extended so we can create geometry
	 * with single vert or single edge selection */
	ele_desel = edbm_add_edge_face_exec__tricky_extend_sel(em->bm);
#endif

	if (!EDBM_op_init(em, &bmop, op,
	                  "contextual_create geom=%hfev mat_nr=%i use_smooth=%b",
	                  BM_ELEM_SELECT, em->mat_nr, use_smooth))
	{
		return OPERATOR_CANCELLED;
	}
	
	BMO_op_exec(em->bm, &bmop);

	/* cancel if nothing was done */
	if ((totedge_orig == em->bm->totedge) &&
	    (totface_orig == em->bm->totface))
	{
		EDBM_op_finish(em, &bmop, op, true);
		return OPERATOR_CANCELLED;
	}

#ifdef USE_FACE_CREATE_SEL_EXTEND
	/* normally we would want to leave the new geometry selected,
	 * but being able to press F many times to add geometry is too useful! */
	if (ele_desel &&
	    (BMO_slot_buffer_count(bmop.slots_out, "faces.out") == 1) &&
	    (ele_desel_face = BMO_slot_buffer_get_first(bmop.slots_out, "faces.out")))
	{
		edbm_add_edge_face_exec__tricky_finalize_sel(em->bm, ele_desel, ele_desel_face);
	}
	else
#endif
	{
		BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);
		BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "edges.out", BM_EDGE, BM_ELEM_SELECT, true);
	}

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_edge_face_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Edge/Face";
	ot->description = "Add an edge or face to selected";
	ot->idname = "MESH_OT_edge_face_add";
	
	/* api callbacks */
	ot->exec = edbm_add_edge_face_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************* SEAMS AND EDGES **************** */

static int edbm_mark_seam_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Mesh *me = ((Mesh *)obedit->data);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMEdge *eed;
	BMIter iter;
	const bool clear = RNA_boolean_get(op->ptr, "clear");
	
	/* auto-enable seams drawing */
	if (clear == 0) {
		me->drawflag |= ME_DRAWSEAMS;
	}

	if (clear) {
		BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
			if (!BM_elem_flag_test(eed, BM_ELEM_SELECT) || BM_elem_flag_test(eed, BM_ELEM_HIDDEN))
				continue;
			
			BM_elem_flag_disable(eed, BM_ELEM_SEAM);
		}
	}
	else {
		BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
			if (!BM_elem_flag_test(eed, BM_ELEM_SELECT) || BM_elem_flag_test(eed, BM_ELEM_HIDDEN))
				continue;
			BM_elem_flag_enable(eed, BM_ELEM_SEAM);
		}
	}

	ED_uvedit_live_unwrap(scene, obedit);
	EDBM_update_generic(em, true, false);

	return OPERATOR_FINISHED;
}

void MESH_OT_mark_seam(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Mark Seam";
	ot->idname = "MESH_OT_mark_seam";
	ot->description = "(Un)mark selected edges as a seam";
	
	/* api callbacks */
	ot->exec = edbm_mark_seam_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	prop = RNA_def_boolean(ot->srna, "clear", 0, "Clear", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int edbm_mark_sharp_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Mesh *me = ((Mesh *)obedit->data);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMEdge *eed;
	BMIter iter;
	const bool clear = RNA_boolean_get(op->ptr, "clear");
	const bool use_verts = RNA_boolean_get(op->ptr, "use_verts");

	/* auto-enable sharp edge drawing */
	if (clear == 0) {
		me->drawflag |= ME_DRAWSHARP;
	}

	BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
		if (use_verts) {
			if (!(BM_elem_flag_test(eed->v1, BM_ELEM_SELECT) || BM_elem_flag_test(eed->v2, BM_ELEM_SELECT))) {
				continue;
			}
		}
		else if (!BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
			continue;
		}

		BM_elem_flag_set(eed, BM_ELEM_SMOOTH, clear);
	}

	EDBM_update_generic(em, true, false);

	return OPERATOR_FINISHED;
}

void MESH_OT_mark_sharp(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Mark Sharp";
	ot->idname = "MESH_OT_mark_sharp";
	ot->description = "(Un)mark selected edges as sharp";
	
	/* api callbacks */
	ot->exec = edbm_mark_sharp_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	prop = RNA_def_boolean(ot->srna, "clear", false, "Clear", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "use_verts", false, "Vertices",
	                       "Consider vertices instead of edges to select which edges to (un)tag as sharp");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int edbm_vert_connect_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMOperator bmop;
	const bool is_pair = (bm->totvertsel == 2);
	int len;
	
	if (is_pair) {
		if (!EDBM_op_init(em, &bmop, op,
		                  "connect_vert_pair verts=%hv verts_exclude=%hv faces_exclude=%hf",
		                  BM_ELEM_SELECT, BM_ELEM_HIDDEN, BM_ELEM_HIDDEN))
		{
			return OPERATOR_CANCELLED;
		}
	}
	else {
		if (!EDBM_op_init(em, &bmop, op,
		                  "connect_verts verts=%hv faces_exclude=%hf check_degenerate=%b",
		                  BM_ELEM_SELECT, BM_ELEM_HIDDEN, true))
		{
			return OPERATOR_CANCELLED;
		}
	}

	BMO_op_exec(bm, &bmop);
	len = BMO_slot_get(bmop.slots_out, "edges.out")->len;

	if (len) {
		if (is_pair) {
			/* new verts have been added, we have to select the edges, not just flush */
			BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "edges.out", BM_EDGE, BM_ELEM_SELECT, true);
		}
	}

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}
	else {
		EDBM_selectmode_flush(em);  /* so newly created edges get the selection state from the vertex */

		EDBM_update_generic(em, true, true);

		return len ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
	}
}

void MESH_OT_vert_connect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Vertex Connect";
	ot->idname = "MESH_OT_vert_connect";
	ot->description = "Connect 2 vertices of a face by an edge, splitting the face in two";
	
	/* api callbacks */
	ot->exec = edbm_vert_connect_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static int edbm_vert_connect_nonplaner_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	const float angle_limit = RNA_float_get(op->ptr, "angle_limit");

	if (!EDBM_op_call_and_selectf(
	             em, op,
	             "faces.out", true,
	             "connect_verts_nonplanar faces=%hf angle_limit=%f",
	             BM_ELEM_SELECT, angle_limit))
	{
		return OPERATOR_CANCELLED;
	}


	EDBM_update_generic(em, true, true);
	return OPERATOR_FINISHED;
}

void MESH_OT_vert_connect_nonplanar(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Split Non-Planar Faces";
	ot->idname = "MESH_OT_vert_connect_nonplanar";
	ot->description = "Split non-planar faces that exceed the angle threshold";

	/* api callbacks */
	ot->exec = edbm_vert_connect_nonplaner_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	prop = RNA_def_float_rotation(ot->srna, "angle_limit", 0, NULL, 0.0f, DEG2RADF(180.0f),
	                              "Max Angle", "Angle limit", 0.0f, DEG2RADF(180.0f));
	RNA_def_property_float_default(prop, DEG2RADF(5.0f));
}


static int edbm_edge_split_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	if (!EDBM_op_call_and_selectf(
	        em, op,
	        "edges.out", false,
	        "split_edges edges=%he",
	        BM_ELEM_SELECT))
	{
		return OPERATOR_CANCELLED;
	}
	
	if (em->selectmode == SCE_SELECT_FACE) {
		EDBM_select_flush(em);
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_edge_split(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Edge Split";
	ot->idname = "MESH_OT_edge_split";
	ot->description = "Split selected edges so that each neighbor face gets its own copy";
	
	/* api callbacks */
	ot->exec = edbm_edge_split_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/****************** add duplicate operator ***************/

static int edbm_duplicate_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMesh *bm = em->bm;
	BMOperator bmop;

	EDBM_op_init(
	        em, &bmop, op,
	        "duplicate geom=%hvef use_select_history=%b",
	        BM_ELEM_SELECT, true);

	BMO_op_exec(bm, &bmop);

	/* de-select all would clear otherwise */
	BM_SELECT_HISTORY_BACKUP(bm);

	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	BMO_slot_buffer_hflag_enable(bm, bmop.slots_out, "geom.out", BM_ALL_NOLOOP, BM_ELEM_SELECT, true);

	/* rebuild editselection */
	BM_SELECT_HISTORY_RESTORE(bm);

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);
	
	return OPERATOR_FINISHED;
}

static int edbm_duplicate_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	WM_cursor_wait(1);
	edbm_duplicate_exec(C, op);
	WM_cursor_wait(0);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate";
	ot->description = "Duplicate selected vertices, edges or faces";
	ot->idname = "MESH_OT_duplicate";
	
	/* api callbacks */
	ot->invoke = edbm_duplicate_invoke;
	ot->exec = edbm_duplicate_exec;
	
	ot->poll = ED_operator_editmesh;
	
	/* to give to transform */
	RNA_def_int(ot->srna, "mode", TFM_TRANSLATION, 0, INT_MAX, "Mode", "", 0, INT_MAX);
}

static int edbm_flip_normals_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	
	if (!EDBM_op_callf(em, op, "reverse_faces faces=%hf", BM_ELEM_SELECT))
		return OPERATOR_CANCELLED;
	
	EDBM_update_generic(em, true, false);

	return OPERATOR_FINISHED;
}

void MESH_OT_flip_normals(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Flip Normals";
	ot->description = "Flip the direction of selected faces' normals (and of their vertices)";
	ot->idname = "MESH_OT_flip_normals";
	
	/* api callbacks */
	ot->exec = edbm_flip_normals_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* only accepts 1 selected edge, or 2 selected faces */
static int edbm_edge_rotate_selected_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMOperator bmop;
	BMEdge *eed;
	BMIter iter;
	const bool use_ccw = RNA_boolean_get(op->ptr, "use_ccw");
	int tot = 0;

	if (em->bm->totedgesel == 0) {
		BKE_report(op->reports, RPT_ERROR, "Select edges or face pairs for edge loops to rotate about");
		return OPERATOR_CANCELLED;
	}

	/* first see if we have two adjacent faces */
	BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
		BM_elem_flag_disable(eed, BM_ELEM_TAG);
		if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
			BMFace *fa, *fb;
			if (BM_edge_face_pair(eed, &fa, &fb)) {
				/* if both faces are selected we rotate between them,
				 * otherwise - rotate between 2 unselected - but not mixed */
				if (BM_elem_flag_test(fa, BM_ELEM_SELECT) == BM_elem_flag_test(fb, BM_ELEM_SELECT)) {
					BM_elem_flag_enable(eed, BM_ELEM_TAG);
					tot++;
				}
			}
		}
	}
	
	/* ok, we don't have two adjacent faces, but we do have two selected ones.
	 * that's an error condition.*/
	if (tot == 0) {
		BKE_report(op->reports, RPT_ERROR, "Could not find any selected edges that can be rotated");
		return OPERATOR_CANCELLED;
	}

	EDBM_op_init(em, &bmop, op, "rotate_edges edges=%he use_ccw=%b", BM_ELEM_TAG, use_ccw);

	/* avoids leaving old verts selected which can be a problem running multiple times,
	 * since this means the edges become selected around the face which then attempt to rotate */
	BMO_slot_buffer_hflag_disable(em->bm, bmop.slots_in, "edges", BM_EDGE, BM_ELEM_SELECT, true);

	BMO_op_exec(em->bm, &bmop);
	/* edges may rotate into hidden vertices, if this does _not_ run we get an ilogical state */
	BMO_slot_buffer_hflag_disable(em->bm, bmop.slots_out, "edges.out", BM_EDGE, BM_ELEM_HIDDEN, true);
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "edges.out", BM_EDGE, BM_ELEM_SELECT, true);
	EDBM_selectmode_flush(em);

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_edge_rotate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Rotate Selected Edge";
	ot->description = "Rotate selected edge or adjoining faces";
	ot->idname = "MESH_OT_edge_rotate";

	/* api callbacks */
	ot->exec = edbm_edge_rotate_selected_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_boolean(ot->srna, "use_ccw", false, "Counter Clockwise", "");
}


static int edbm_hide_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	
	EDBM_mesh_hide(em, RNA_boolean_get(op->ptr, "unselected"));

	EDBM_update_generic(em, true, false);

	return OPERATOR_FINISHED;
}

void MESH_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hide Selection";
	ot->idname = "MESH_OT_hide";
	ot->description = "Hide (un)selected vertices, edges or faces";
	
	/* api callbacks */
	ot->exec = edbm_hide_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected");
}

static int edbm_reveal_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	
	EDBM_mesh_reveal(em);

	EDBM_update_generic(em, true, false);

	return OPERATOR_FINISHED;
}

void MESH_OT_reveal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reveal Hidden";
	ot->idname = "MESH_OT_reveal";
	ot->description = "Reveal all hidden vertices, edges and faces";
	
	/* api callbacks */
	ot->exec = edbm_reveal_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int edbm_normals_make_consistent_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	
	/* doflip has to do with bmesh_rationalize_normals, it's an internal
	 * thing */
	if (!EDBM_op_callf(em, op, "recalc_face_normals faces=%hf", BM_ELEM_SELECT))
		return OPERATOR_CANCELLED;

	if (RNA_boolean_get(op->ptr, "inside"))
		EDBM_op_callf(em, op, "reverse_faces faces=%hf", BM_ELEM_SELECT);

	EDBM_update_generic(em, true, false);

	return OPERATOR_FINISHED;
}

void MESH_OT_normals_make_consistent(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Normals Consistent";
	ot->description = "Make face and vertex normals point either outside or inside the mesh";
	ot->idname = "MESH_OT_normals_make_consistent";
	
	/* api callbacks */
	ot->exec = edbm_normals_make_consistent_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "inside", 0, "Inside", "");
}



static int edbm_do_smooth_vertex_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Mesh *me = obedit->data;
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	ModifierData *md;
	bool mirrx = false, mirry = false, mirrz = false;
	int i, repeat;
	float clip_dist = 0.0f;
	const float fac = RNA_float_get(op->ptr, "factor");
	const bool use_topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;

	const bool xaxis = RNA_boolean_get(op->ptr, "xaxis");
	const bool yaxis = RNA_boolean_get(op->ptr, "yaxis");
	const bool zaxis = RNA_boolean_get(op->ptr, "zaxis");

	/* mirror before smooth */
	if (((Mesh *)obedit->data)->editflag & ME_EDIT_MIRROR_X) {
		EDBM_verts_mirror_cache_begin(em, 0, false, true, use_topology);
	}

	/* if there is a mirror modifier with clipping, flag the verts that
	 * are within tolerance of the plane(s) of reflection 
	 */
	for (md = obedit->modifiers.first; md; md = md->next) {
		if (md->type == eModifierType_Mirror && (md->mode & eModifierMode_Realtime)) {
			MirrorModifierData *mmd = (MirrorModifierData *)md;
		
			if (mmd->flag & MOD_MIR_CLIPPING) {
				if (mmd->flag & MOD_MIR_AXIS_X)
					mirrx = true;
				if (mmd->flag & MOD_MIR_AXIS_Y)
					mirry = true;
				if (mmd->flag & MOD_MIR_AXIS_Z)
					mirrz = true;

				clip_dist = mmd->tolerance;
			}
		}
	}

	repeat = RNA_int_get(op->ptr, "repeat");
	if (!repeat)
		repeat = 1;

	for (i = 0; i < repeat; i++) {
		if (!EDBM_op_callf(em, op,
		                   "smooth_vert verts=%hv factor=%f mirror_clip_x=%b mirror_clip_y=%b mirror_clip_z=%b "
		                   "clip_dist=%f use_axis_x=%b use_axis_y=%b use_axis_z=%b",
		                   BM_ELEM_SELECT, fac, mirrx, mirry, mirrz, clip_dist, xaxis, yaxis, zaxis))
		{
			return OPERATOR_CANCELLED;
		}
	}

	/* apply mirror */
	if (((Mesh *)obedit->data)->editflag & ME_EDIT_MIRROR_X) {
		EDBM_verts_mirror_apply(em, BM_ELEM_SELECT, 0);
		EDBM_verts_mirror_cache_end(em);
	}

	EDBM_update_generic(em, true, false);

	return OPERATOR_FINISHED;
}	
	
void MESH_OT_vertices_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Smooth Vertex";
	ot->description = "Flatten angles of selected vertices";
	ot->idname = "MESH_OT_vertices_smooth";
	
	/* api callbacks */
	ot->exec = edbm_do_smooth_vertex_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_float(ot->srna, "factor", 0.5f, 0.0f, 1.0f, "Smoothing", "Smoothing factor", 0.0f, 1.0f);
	RNA_def_int(ot->srna, "repeat", 1, 1, 1000, "Repeat", "Number of times to smooth the mesh", 1, 100);
	RNA_def_boolean(ot->srna, "xaxis", 1, "X-Axis", "Smooth along the X axis");
	RNA_def_boolean(ot->srna, "yaxis", 1, "Y-Axis", "Smooth along the Y axis");
	RNA_def_boolean(ot->srna, "zaxis", 1, "Z-Axis", "Smooth along the Z axis");
}

static int edbm_do_smooth_laplacian_vertex_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	Mesh *me = obedit->data;
	bool use_topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;
	bool usex = true, usey = true, usez = true, preserve_volume = true;
	int i, repeat;
	float lambda_factor;
	float lambda_border;
	BMIter fiter;
	BMFace *f;

	/* Check if select faces are triangles	*/
	BM_ITER_MESH (f, &fiter, em->bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
			if (f->len > 4) {
				BKE_report(op->reports, RPT_WARNING, "Selected faces must be triangles or quads");
				return OPERATOR_CANCELLED;
			}	
		}
	}

	/* mirror before smooth */
	if (((Mesh *)obedit->data)->editflag & ME_EDIT_MIRROR_X) {
		EDBM_verts_mirror_cache_begin(em, 0, false, true, use_topology);
	}

	repeat = RNA_int_get(op->ptr, "repeat");
	lambda_factor = RNA_float_get(op->ptr, "lambda_factor");
	lambda_border = RNA_float_get(op->ptr, "lambda_border");
	usex = RNA_boolean_get(op->ptr, "use_x");
	usey = RNA_boolean_get(op->ptr, "use_y");
	usez = RNA_boolean_get(op->ptr, "use_z");
	preserve_volume = RNA_boolean_get(op->ptr, "preserve_volume");
	if (!repeat)
		repeat = 1;
	
	for (i = 0; i < repeat; i++) {
		if (!EDBM_op_callf(em, op,
		                   "smooth_laplacian_vert verts=%hv lambda_factor=%f lambda_border=%f use_x=%b use_y=%b use_z=%b preserve_volume=%b",
		                   BM_ELEM_SELECT, lambda_factor, lambda_border, usex, usey, usez, preserve_volume))
		{
			return OPERATOR_CANCELLED;
		}
	}

	/* apply mirror */
	if (((Mesh *)obedit->data)->editflag & ME_EDIT_MIRROR_X) {
		EDBM_verts_mirror_apply(em, BM_ELEM_SELECT, 0);
		EDBM_verts_mirror_cache_end(em);
	}

	EDBM_update_generic(em, true, false);

	return OPERATOR_FINISHED;
}

void MESH_OT_vertices_smooth_laplacian(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Laplacian Smooth Vertex";
	ot->description = "Laplacian smooth of selected vertices";
	ot->idname = "MESH_OT_vertices_smooth_laplacian";
	
	/* api callbacks */
	ot->exec = edbm_do_smooth_laplacian_vertex_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_int(ot->srna, "repeat", 1, 1, 200,
	            "Number of iterations to smooth the mesh", "", 1, 200);
	RNA_def_float(ot->srna, "lambda_factor", 0.00005f, 0.0000001f, 1000.0f,
	              "Lambda factor", "", 0.0000001f, 1000.0f);
	RNA_def_float(ot->srna, "lambda_border", 0.00005f, 0.0000001f, 1000.0f,
	              "Lambda factor in border", "", 0.0000001f, 1000.0f);
	RNA_def_boolean(ot->srna, "use_x", 1, "Smooth X Axis", "Smooth object along X axis");
	RNA_def_boolean(ot->srna, "use_y", 1, "Smooth Y Axis", "Smooth object along Y axis");
	RNA_def_boolean(ot->srna, "use_z", 1, "Smooth Z Axis", "Smooth object along Z axis");
	RNA_def_boolean(ot->srna, "preserve_volume", 1, "Preserve Volume", "Apply volume preservation after smooth");
}

/********************** Smooth/Solid Operators *************************/

static void mesh_set_smooth_faces(BMEditMesh *em, short smooth)
{
	BMIter iter;
	BMFace *efa;

	if (em == NULL) return;
	
	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
			BM_elem_flag_set(efa, BM_ELEM_SMOOTH, smooth);
		}
	}
}

static int edbm_faces_shade_smooth_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	mesh_set_smooth_faces(em, 1);

	EDBM_update_generic(em, false, false);

	return OPERATOR_FINISHED;
}

void MESH_OT_faces_shade_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Shade Smooth";
	ot->description = "Display faces smooth (using vertex normals)";
	ot->idname = "MESH_OT_faces_shade_smooth";

	/* api callbacks */
	ot->exec = edbm_faces_shade_smooth_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int edbm_faces_shade_flat_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	mesh_set_smooth_faces(em, 0);

	EDBM_update_generic(em, false, false);

	return OPERATOR_FINISHED;
}

void MESH_OT_faces_shade_flat(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Shade Flat";
	ot->description = "Display faces flat";
	ot->idname = "MESH_OT_faces_shade_flat";

	/* api callbacks */
	ot->exec = edbm_faces_shade_flat_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/********************** UV/Color Operators *************************/

static int edbm_rotate_uvs_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMOperator bmop;

	/* get the direction from RNA */
	const bool use_ccw = RNA_boolean_get(op->ptr, "use_ccw");

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_op_init(em, &bmop, op, "rotate_uvs faces=%hf use_ccw=%b", BM_ELEM_SELECT, use_ccw);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* finish the operator */
	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, false, false);

	return OPERATOR_FINISHED;
}

static int edbm_reverse_uvs_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMOperator bmop;

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_op_init(em, &bmop, op, "reverse_uvs faces=%hf", BM_ELEM_SELECT);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* finish the operator */
	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, false, false);

	return OPERATOR_FINISHED;
}

static int edbm_rotate_colors_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMOperator bmop;

	/* get the direction from RNA */
	const bool use_ccw = RNA_boolean_get(op->ptr, "use_ccw");

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_op_init(em, &bmop, op, "rotate_colors faces=%hf use_ccw=%b", BM_ELEM_SELECT, use_ccw);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* finish the operator */
	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	/* dependencies graph and notification stuff */
	EDBM_update_generic(em, false, false);

	return OPERATOR_FINISHED;
}


static int edbm_reverse_colors_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMOperator bmop;

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_op_init(em, &bmop, op, "reverse_colors faces=%hf", BM_ELEM_SELECT);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* finish the operator */
	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, false, false);

	return OPERATOR_FINISHED;
}

void MESH_OT_uvs_rotate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Rotate UVs";
	ot->idname = "MESH_OT_uvs_rotate";
	ot->description = "Rotate UV coordinates inside faces";

	/* api callbacks */
	ot->exec = edbm_rotate_uvs_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_boolean(ot->srna, "use_ccw", false, "Counter Clockwise", "");
}

void MESH_OT_uvs_reverse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reverse UVs";
	ot->idname = "MESH_OT_uvs_reverse";
	ot->description = "Flip direction of UV coordinates inside faces";

	/* api callbacks */
	ot->exec = edbm_reverse_uvs_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	//RNA_def_enum(ot->srna, "axis", axis_items, DIRECTION_CW, "Axis", "Axis to mirror UVs around");
}

void MESH_OT_colors_rotate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Rotate Colors";
	ot->idname = "MESH_OT_colors_rotate";
	ot->description = "Rotate vertex colors inside faces";

	/* api callbacks */
	ot->exec = edbm_rotate_colors_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_boolean(ot->srna, "use_ccw", false, "Counter Clockwise", "");
}

void MESH_OT_colors_reverse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reverse Colors";
	ot->idname = "MESH_OT_colors_reverse";
	ot->description = "Flip direction of vertex colors inside faces";

	/* api callbacks */
	ot->exec = edbm_reverse_colors_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	//RNA_def_enum(ot->srna, "axis", axis_items, DIRECTION_CW, "Axis", "Axis to mirror colors around");
}


static bool merge_firstlast(BMEditMesh *em, const bool use_first, const bool use_uvmerge, wmOperator *wmop)
{
	BMVert *mergevert;
	BMEditSelection *ese;

	/* operator could be called directly from shortcut or python,
	 * so do extra check for data here
	 */

	/* do sanity check in mergemenu in edit.c ?*/
	if (use_first == false) {
		if (!em->bm->selected.last || ((BMEditSelection *)em->bm->selected.last)->htype != BM_VERT)
			return false;

		ese = em->bm->selected.last;
		mergevert = (BMVert *)ese->ele;
	}
	else {
		if (!em->bm->selected.first || ((BMEditSelection *)em->bm->selected.first)->htype != BM_VERT)
			return false;

		ese = em->bm->selected.first;
		mergevert = (BMVert *)ese->ele;
	}

	if (!BM_elem_flag_test(mergevert, BM_ELEM_SELECT))
		return false;
	
	if (use_uvmerge) {
		if (!EDBM_op_callf(em, wmop, "pointmerge_facedata verts=%hv vert_snap=%e", BM_ELEM_SELECT, mergevert))
			return false;
	}

	if (!EDBM_op_callf(em, wmop, "pointmerge verts=%hv merge_co=%v", BM_ELEM_SELECT, mergevert->co))
		return false;

	return true;
}

static bool merge_target(BMEditMesh *em, Scene *scene, View3D *v3d, Object *ob,
                         const bool use_cursor, const bool use_uvmerge, wmOperator *wmop)
{
	BMIter iter;
	BMVert *v;
	float co[3], cent[3] = {0.0f, 0.0f, 0.0f};
	const float *vco = NULL;

	if (use_cursor) {
		vco = ED_view3d_cursor3d_get(scene, v3d);
		copy_v3_v3(co, vco);
		mul_m4_v3(ob->imat, co);
	}
	else {
		float fac;
		int i = 0;
		BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
			if (!BM_elem_flag_test(v, BM_ELEM_SELECT))
				continue;
			add_v3_v3(cent, v->co);
			i++;
		}
		
		if (!i)
			return false;

		fac = 1.0f / (float)i;
		mul_v3_fl(cent, fac);
		copy_v3_v3(co, cent);
		vco = co;
	}

	if (!vco)
		return false;
	
	if (use_uvmerge) {
		if (!EDBM_op_callf(em, wmop, "average_vert_facedata verts=%hv", BM_ELEM_SELECT))
			return false;
	}

	if (!EDBM_op_callf(em, wmop, "pointmerge verts=%hv merge_co=%v", BM_ELEM_SELECT, co))
		return false;

	return true;
}

static int edbm_merge_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const int type = RNA_enum_get(op->ptr, "type");
	const bool uvs = RNA_boolean_get(op->ptr, "uvs");
	bool ok = false;

	switch (type) {
		case 3:
			ok = merge_target(em, scene, v3d, obedit, false, uvs, op);
			break;
		case 4:
			ok = merge_target(em, scene, v3d, obedit, true, uvs, op);
			break;
		case 1:
			ok = merge_firstlast(em, false, uvs, op);
			break;
		case 6:
			ok = merge_firstlast(em, true, uvs, op);
			break;
		case 5:
			ok = true;
			if (!EDBM_op_callf(em, op, "collapse edges=%he", BM_ELEM_SELECT))
				ok = false;
			break;
		default:
			BLI_assert(0);
			break;
	}

	if (!ok) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

static EnumPropertyItem merge_type_items[] = {
	{6, "FIRST", 0, "At First", ""},
	{1, "LAST", 0, "At Last", ""},
	{3, "CENTER", 0, "At Center", ""},
	{4, "CURSOR", 0, "At Cursor", ""},
	{5, "COLLAPSE", 0, "Collapse", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem *merge_type_itemf(bContext *C, PointerRNA *UNUSED(ptr),  PropertyRNA *UNUSED(prop), bool *r_free)
{	
	Object *obedit;
	EnumPropertyItem *item = NULL;
	int totitem = 0;
	
	if (!C) /* needed for docs */
		return merge_type_items;
	
	obedit = CTX_data_edit_object(C);
	if (obedit && obedit->type == OB_MESH) {
		BMEditMesh *em = BKE_editmesh_from_object(obedit);

		if (em->selectmode & SCE_SELECT_VERTEX) {
			if (em->bm->selected.first && em->bm->selected.last &&
			    ((BMEditSelection *)em->bm->selected.first)->htype == BM_VERT &&
			    ((BMEditSelection *)em->bm->selected.last)->htype == BM_VERT)
			{
				RNA_enum_items_add_value(&item, &totitem, merge_type_items, 6);
				RNA_enum_items_add_value(&item, &totitem, merge_type_items, 1);
			}
			else if (em->bm->selected.first && ((BMEditSelection *)em->bm->selected.first)->htype == BM_VERT) {
				RNA_enum_items_add_value(&item, &totitem, merge_type_items, 6);
			}
			else if (em->bm->selected.last && ((BMEditSelection *)em->bm->selected.last)->htype == BM_VERT) {
				RNA_enum_items_add_value(&item, &totitem, merge_type_items, 1);
			}
		}

		RNA_enum_items_add_value(&item, &totitem, merge_type_items, 3);
		RNA_enum_items_add_value(&item, &totitem, merge_type_items, 4);
		RNA_enum_items_add_value(&item, &totitem, merge_type_items, 5);
		RNA_enum_item_end(&item, &totitem);

		*r_free = true;

		return item;
	}
	
	return NULL;
}

void MESH_OT_merge(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Merge";
	ot->description = "Merge selected vertices";
	ot->idname = "MESH_OT_merge";

	/* api callbacks */
	ot->exec = edbm_merge_exec;
	ot->invoke = WM_menu_invoke;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", merge_type_items, 3, "Type", "Merge method to use");
	RNA_def_enum_funcs(ot->prop, merge_type_itemf);
	RNA_def_boolean(ot->srna, "uvs", 0, "UVs", "Move UVs according to merge");
}


static int edbm_remove_doubles_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMOperator bmop;
	const float threshold = RNA_float_get(op->ptr, "threshold");
	const bool use_unselected = RNA_boolean_get(op->ptr, "use_unselected");
	const int totvert_orig = em->bm->totvert;
	int count;
	char htype_select;

	/* avoid loosing selection state (select -> tags) */
	if      (em->selectmode & SCE_SELECT_VERTEX) htype_select = BM_VERT;
	else if (em->selectmode & SCE_SELECT_EDGE)   htype_select = BM_EDGE;
	else                                         htype_select = BM_FACE;

	/* store selection as tags */
	BM_mesh_elem_hflag_enable_test(em->bm, htype_select, BM_ELEM_TAG, true, true, BM_ELEM_SELECT);


	if (use_unselected) {
		EDBM_op_init(em, &bmop, op,
		             "automerge verts=%hv dist=%f",
		             BM_ELEM_SELECT, threshold);
		BMO_op_exec(em->bm, &bmop);

		if (!EDBM_op_finish(em, &bmop, op, true)) {
			return OPERATOR_CANCELLED;
		}
	}
	else {
		EDBM_op_init(em, &bmop, op,
		             "find_doubles verts=%hv dist=%f",
		             BM_ELEM_SELECT, threshold);
		BMO_op_exec(em->bm, &bmop);

		if (!EDBM_op_callf(em, op, "weld_verts targetmap=%S", &bmop, "targetmap.out")) {
			BMO_op_finish(em->bm, &bmop);
			return OPERATOR_CANCELLED;
		}

		if (!EDBM_op_finish(em, &bmop, op, true)) {
			return OPERATOR_CANCELLED;
		}
	}
	
	count = totvert_orig - em->bm->totvert;
	BKE_reportf(op->reports, RPT_INFO, "Removed %d vertices", count);

	/* restore selection from tags */
	BM_mesh_elem_hflag_enable_test(em->bm, htype_select, BM_ELEM_SELECT, true, true, BM_ELEM_TAG);
	EDBM_selectmode_flush(em);

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_remove_doubles(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Doubles";
	ot->description = "Remove duplicate vertices";
	ot->idname = "MESH_OT_remove_doubles";

	/* api callbacks */
	ot->exec = edbm_remove_doubles_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_float(ot->srna, "threshold", 0.0001f, 0.000001f, 50.0f,  "Merge Distance",
	              "Minimum distance between elements to merge", 0.00001, 10.0);
	RNA_def_boolean(ot->srna, "use_unselected", 0, "Unselected", "Merge selected to other unselected vertices");
}


/************************ Shape Operators *************************/

/* BMESH_TODO this should be properly encapsulated in a bmop.  but later.*/
static void shape_propagate(BMEditMesh *em, wmOperator *op)
{
	BMIter iter;
	BMVert *eve = NULL;
	float *co;
	int i, totshape = CustomData_number_of_layers(&em->bm->vdata, CD_SHAPEKEY);

	if (!CustomData_has_layer(&em->bm->vdata, CD_SHAPEKEY)) {
		BKE_report(op->reports, RPT_ERROR, "Mesh does not have shape keys");
		return;
	}
	
	BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
		if (!BM_elem_flag_test(eve, BM_ELEM_SELECT) || BM_elem_flag_test(eve, BM_ELEM_HIDDEN))
			continue;

		for (i = 0; i < totshape; i++) {
			co = CustomData_bmesh_get_n(&em->bm->vdata, eve->head.data, CD_SHAPEKEY, i);
			copy_v3_v3(co, eve->co);
		}
	}

#if 0
	//TAG Mesh Objects that share this data
	for (base = scene->base.first; base; base = base->next) {
		if (base->object && base->object->data == me) {
			DAG_id_tag_update(&base->object->id, OB_RECALC_DATA);
		}
	}
#endif
}


static int edbm_shape_propagate_to_all_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Mesh *me = obedit->data;
	BMEditMesh *em = me->edit_btmesh;

	shape_propagate(em, op);

	EDBM_update_generic(em, false, false);

	return OPERATOR_FINISHED;
}


void MESH_OT_shape_propagate_to_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Shape Propagate";
	ot->description = "Apply selected vertex locations to all other shape keys";
	ot->idname = "MESH_OT_shape_propagate_to_all";

	/* api callbacks */
	ot->exec = edbm_shape_propagate_to_all_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* BMESH_TODO this should be properly encapsulated in a bmop.  but later.*/
static int edbm_blend_from_shape_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Mesh *me = obedit->data;
	Key *key = me->key;
	KeyBlock *kb = NULL;
	BMEditMesh *em = me->edit_btmesh;
	BMVert *eve;
	BMIter iter;
	float co[3], *sco;
	int totshape;

	const float blend = RNA_float_get(op->ptr, "blend");
	const int shape = RNA_enum_get(op->ptr, "shape");
	const bool use_add = RNA_boolean_get(op->ptr, "add");

	/* sanity check */
	totshape = CustomData_number_of_layers(&em->bm->vdata, CD_SHAPEKEY);
	if (totshape == 0 || shape < 0 || shape >= totshape)
		return OPERATOR_CANCELLED;

	/* get shape key - needed for finding reference shape (for add mode only) */
	if (key) {
		kb = BLI_findlink(&key->block, shape);
	}
	
	/* perform blending on selected vertices*/
	BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
		if (!BM_elem_flag_test(eve, BM_ELEM_SELECT) || BM_elem_flag_test(eve, BM_ELEM_HIDDEN))
			continue;
		
		/* get coordinates of shapekey we're blending from */
		sco = CustomData_bmesh_get_n(&em->bm->vdata, eve->head.data, CD_SHAPEKEY, shape);
		copy_v3_v3(co, sco);
		
		if (use_add) {
			/* in add mode, we add relative shape key offset */
			if (kb) {
				const float *rco = CustomData_bmesh_get_n(&em->bm->vdata, eve->head.data, CD_SHAPEKEY, kb->relative);
				sub_v3_v3v3(co, co, rco);
			}
			
			madd_v3_v3fl(eve->co, co, blend);
		}
		else {
			/* in blend mode, we interpolate to the shape key */
			interp_v3_v3v3(eve->co, eve->co, co, blend);
		}
	}

	EDBM_update_generic(em, true, false);

	return OPERATOR_FINISHED;
}

static EnumPropertyItem *shape_itemf(bContext *C, PointerRNA *UNUSED(ptr),  PropertyRNA *UNUSED(prop), bool *r_free)
{	
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em;
	EnumPropertyItem *item = NULL;
	int totitem = 0;

	if ((obedit && obedit->type == OB_MESH) &&
	    (em = BKE_editmesh_from_object(obedit)) &&
	    CustomData_has_layer(&em->bm->vdata, CD_SHAPEKEY))
	{
		EnumPropertyItem tmp = {0, "", 0, "", ""};
		int a;

		for (a = 0; a < em->bm->vdata.totlayer; a++) {
			if (em->bm->vdata.layers[a].type != CD_SHAPEKEY)
				continue;

			tmp.value = totitem;
			tmp.identifier = em->bm->vdata.layers[a].name;
			tmp.name = em->bm->vdata.layers[a].name;
			/* RNA_enum_item_add sets totitem itself! */
			RNA_enum_item_add(&item, &totitem, &tmp);
		}
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static void edbm_blend_from_shape_ui(bContext *C, wmOperator *op)
{
	uiLayout *layout = op->layout;
	PointerRNA ptr;
	Object *obedit = CTX_data_edit_object(C);
	Mesh *me = obedit->data;
	PointerRNA ptr_key;

	RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
	RNA_id_pointer_create((ID *)me->key, &ptr_key);

	uiItemPointerR(layout, &ptr, "shape", &ptr_key, "key_blocks", "", ICON_SHAPEKEY_DATA);
	uiItemR(layout, &ptr, "blend", 0, NULL, ICON_NONE);
	uiItemR(layout, &ptr, "add", 0, NULL, ICON_NONE);
}

void MESH_OT_blend_from_shape(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Blend From Shape";
	ot->description = "Blend in shape from a shape key";
	ot->idname = "MESH_OT_blend_from_shape";

	/* api callbacks */
	ot->exec = edbm_blend_from_shape_exec;
//	ot->invoke = WM_operator_props_popup_call;  /* disable because search popup closes too easily */
	ot->ui = edbm_blend_from_shape_ui;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_enum(ot->srna, "shape", DummyRNA_NULL_items, 0, "Shape", "Shape key to use for blending");
	RNA_def_enum_funcs(prop, shape_itemf);
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
	RNA_def_float(ot->srna, "blend", 1.0f, -FLT_MAX, FLT_MAX, "Blend", "Blending factor", -2.0f, 2.0f);
	RNA_def_boolean(ot->srna, "add", 1, "Add", "Add rather than blend between shapes");
}

static int edbm_solidify_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Mesh *me = obedit->data;
	BMEditMesh *em = me->edit_btmesh;
	BMesh *bm = em->bm;
	BMOperator bmop;

	const float thickness = RNA_float_get(op->ptr, "thickness");

	if (!EDBM_op_init(em, &bmop, op, "solidify geom=%hf thickness=%f", BM_ELEM_SELECT, thickness)) {
		return OPERATOR_CANCELLED;
	}

	/* deselect only the faces in the region to be solidified (leave wire
	 * edges and loose verts selected, as there will be no corresponding
	 * geometry selected below) */
	BMO_slot_buffer_hflag_disable(bm, bmop.slots_in, "geom", BM_FACE, BM_ELEM_SELECT, true);

	/* run the solidify operator */
	BMO_op_exec(bm, &bmop);

	/* select the newly generated faces */
	BMO_slot_buffer_hflag_enable(bm, bmop.slots_out, "geom.out", BM_FACE, BM_ELEM_SELECT, true);

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}


void MESH_OT_solidify(wmOperatorType *ot)
{
	PropertyRNA *prop;
	/* identifiers */
	ot->name = "Solidify";
	ot->description = "Create a solid skin by extruding, compensating for sharp angles";
	ot->idname = "MESH_OT_solidify";

	/* api callbacks */
	ot->exec = edbm_solidify_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_float(ot->srna, "thickness", 0.01f, -FLT_MAX, FLT_MAX, "thickness", "", -10.0f, 10.0f);
	RNA_def_property_ui_range(prop, -10, 10, 0.1, 4);
}

/* ******************************************************************** */
/* Knife Subdivide Tool.  Subdivides edges intersected by a mouse trail
 * drawn by user.
 *
 * Currently mapped to KKey when in MeshEdit mode.
 * Usage:
 * - Hit Shift K, Select Centers or Exact
 * - Hold LMB down to draw path, hit RETKEY.
 * - ESC cancels as expected.
 *
 * Contributed by Robert Wenzlaff (Det. Thorn).
 *
 * 2.5 Revamp:
 *  - non modal (no menu before cutting)
 *  - exit on mouse release
 *  - polygon/segment drawing can become handled by WM cb later
 *
 * bmesh port version
 */

#define KNIFE_EXACT     1
#define KNIFE_MIDPOINT  2
#define KNIFE_MULTICUT  3

static EnumPropertyItem knife_items[] = {
	{KNIFE_EXACT, "EXACT", 0, "Exact", ""},
	{KNIFE_MIDPOINT, "MIDPOINTS", 0, "Midpoints", ""},
	{KNIFE_MULTICUT, "MULTICUT", 0, "Multicut", ""},
	{0, NULL, 0, NULL, NULL}
};

/* bm_edge_seg_isect() Determines if and where a mouse trail intersects an BMEdge */

static float bm_edge_seg_isect(const float sco_a[2], const float sco_b[2],
                               float (*mouse_path)[2], int len, char mode, int *isected)
{
#define MAXSLOPE 100000
	float x11, y11, x12 = 0, y12 = 0, x2max, x2min, y2max;
	float y2min, dist, lastdist = 0, xdiff2, xdiff1;
	float m1, b1, m2, b2, x21, x22, y21, y22, xi;
	float yi, x1min, x1max, y1max, y1min, perc = 0;
	float threshold = 0.0;
	int i;
	
	//threshold = 0.000001; /* tolerance for vertex intersection */
	// XXX threshold = scene->toolsettings->select_thresh / 100;
	
	/* Get screen coords of verts */
	x21 = sco_a[0];
	y21 = sco_a[1];
	
	x22 = sco_b[0];
	y22 = sco_b[1];
	
	xdiff2 = (x22 - x21);
	if (xdiff2) {
		m2 = (y22 - y21) / xdiff2;
		b2 = ((x22 * y21) - (x21 * y22)) / xdiff2;
	}
	else {
		m2 = MAXSLOPE;  /* Verticle slope  */
		b2 = x22;
	}

	*isected = 0;

	/* check for _exact_ vertex intersection first */
	if (mode != KNIFE_MULTICUT) {
		for (i = 0; i < len; i++) {
			if (i > 0) {
				x11 = x12;
				y11 = y12;
			}
			else {
				x11 = mouse_path[i][0];
				y11 = mouse_path[i][1];
			}
			x12 = mouse_path[i][0];
			y12 = mouse_path[i][1];
			
			/* test e->v1 */
			if ((x11 == x21 && y11 == y21) || (x12 == x21 && y12 == y21)) {
				perc = 0;
				*isected = 1;
				return perc;
			}
			/* test e->v2 */
			else if ((x11 == x22 && y11 == y22) || (x12 == x22 && y12 == y22)) {
				perc = 0;
				*isected = 2;
				return perc;
			}
		}
	}
	
	/* now check for edge intersect (may produce vertex intersection as well) */
	for (i = 0; i < len; i++) {
		if (i > 0) {
			x11 = x12;
			y11 = y12;
		}
		else {
			x11 = mouse_path[i][0];
			y11 = mouse_path[i][1];
		}
		x12 = mouse_path[i][0];
		y12 = mouse_path[i][1];
		
		/* Perp. Distance from point to line */
		if (m2 != MAXSLOPE) dist = (y12 - m2 * x12 - b2);  /* /sqrt(m2 * m2 + 1); Only looking for */
		/* change in sign.  Skip extra math */
		else dist = x22 - x12;
		
		if (i == 0) lastdist = dist;
		
		/* if dist changes sign, and intersect point in edge's Bound Box */
		if ((lastdist * dist) <= 0) {
			xdiff1 = (x12 - x11); /* Equation of line between last 2 points */
			if (xdiff1) {
				m1 = (y12 - y11) / xdiff1;
				b1 = ((x12 * y11) - (x11 * y12)) / xdiff1;
			}
			else {
				m1 = MAXSLOPE;
				b1 = x12;
			}
			x2max = max_ff(x21, x22) + 0.001f; /* prevent missed edges   */
			x2min = min_ff(x21, x22) - 0.001f; /* due to round off error */
			y2max = max_ff(y21, y22) + 0.001f;
			y2min = min_ff(y21, y22) - 0.001f;
			
			/* Found an intersect,  calc intersect point */
			if (m1 == m2) { /* co-incident lines */
				/* cut at 50% of overlap area */
				x1max = max_ff(x11, x12);
				x1min = min_ff(x11, x12);
				xi = (min_ff(x2max, x1max) + max_ff(x2min, x1min)) / 2.0f;
				
				y1max = max_ff(y11, y12);
				y1min = min_ff(y11, y12);
				yi = (min_ff(y2max, y1max) + max_ff(y2min, y1min)) / 2.0f;
			}
			else if (m2 == MAXSLOPE) {
				xi = x22;
				yi = m1 * x22 + b1;
			}
			else if (m1 == MAXSLOPE) {
				xi = x12;
				yi = m2 * x12 + b2;
			}
			else {
				xi = (b1 - b2) / (m2 - m1);
				yi = (b1 * m2 - m1 * b2) / (m2 - m1);
			}
			
			/* Intersect inside bounding box of edge?*/
			if ((xi >= x2min) && (xi <= x2max) && (yi <= y2max) && (yi >= y2min)) {
				/* test for vertex intersect that may be 'close enough'*/
				if (mode != KNIFE_MULTICUT) {
					if (xi <= (x21 + threshold) && xi >= (x21 - threshold)) {
						if (yi <= (y21 + threshold) && yi >= (y21 - threshold)) {
							*isected = 1;
							perc = 0;
							break;
						}
					}
					if (xi <= (x22 + threshold) && xi >= (x22 - threshold)) {
						if (yi <= (y22 + threshold) && yi >= (y22 - threshold)) {
							*isected = 2;
							perc = 0;
							break;
						}
					}
				}
				if ((m2 <= 1.0f) && (m2 >= -1.0f)) perc = (xi - x21) / (x22 - x21);
				else perc = (yi - y21) / (y22 - y21);  /* lower slope more accurate */
				//isect = 32768.0 * (perc + 0.0000153); /* Percentage in 1 / 32768ths */
				
				break;
			}
		}
		lastdist = dist;
	}
	return perc;
}

#define ELE_EDGE_CUT 1

static int edbm_knife_cut_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	ARegion *ar = CTX_wm_region(C);
	BMVert *bv;
	BMIter iter;
	BMEdge *be;
	BMOperator bmop;
	float isect = 0.0f;
	int len = 0, isected, i;
	short numcuts = 1;
	const short mode = RNA_int_get(op->ptr, "type");
	BMOpSlot *slot_edge_percents;

	/* allocd vars */
	float (*screen_vert_coords)[2], (*sco)[2], (*mouse_path)[2];
	
	/* edit-object needed for matrix, and ar->regiondata for projections to work */
	if (ELEM(NULL, obedit, ar, ar->regiondata))
		return OPERATOR_CANCELLED;
	
	if (bm->totvertsel < 2) {
		BKE_report(op->reports, RPT_ERROR, "No edges are selected to operate on");
		return OPERATOR_CANCELLED;
	}

	len = RNA_collection_length(op->ptr, "path");

	if (len < 2) {
		BKE_report(op->reports, RPT_ERROR, "Mouse path too short");
		return OPERATOR_CANCELLED;
	}

	mouse_path = MEM_mallocN(len * sizeof(*mouse_path), __func__);

	/* get the cut curve */
	RNA_BEGIN (op->ptr, itemptr, "path")
	{
		RNA_float_get_array(&itemptr, "loc", (float *)&mouse_path[len]);
	}
	RNA_END;

	/* for ED_view3d_project_float_object */
	ED_view3d_init_mats_rv3d(obedit, ar->regiondata);

	/* TODO, investigate using index lookup for screen_vert_coords() rather then a hash table */

	/* the floating point coordinates of verts in screen space will be stored in a hash table according to the vertices pointer */
	screen_vert_coords = sco = MEM_mallocN(bm->totvert * sizeof(float) * 2, __func__);

	BM_ITER_MESH_INDEX (bv, &iter, bm, BM_VERTS_OF_MESH, i) {
		if (ED_view3d_project_float_object(ar, bv->co, *sco, V3D_PROJ_TEST_CLIP_NEAR) != V3D_PROJ_RET_OK) {
			copy_v2_fl(*sco, FLT_MAX);  /* set error value */
		}
		BM_elem_index_set(bv, i); /* set_inline */
		sco++;

	}
	bm->elem_index_dirty &= ~BM_VERT; /* clear dirty flag */

	if (!EDBM_op_init(em, &bmop, op, "subdivide_edges")) {
		MEM_freeN(mouse_path);
		MEM_freeN(screen_vert_coords);
		return OPERATOR_CANCELLED;
	}

	/* store percentage of edge cut for KNIFE_EXACT here.*/
	slot_edge_percents = BMO_slot_get(bmop.slots_in, "edge_percents");
	BM_ITER_MESH (be, &iter, bm, BM_EDGES_OF_MESH) {
		bool is_cut = false;
		if (BM_elem_flag_test(be, BM_ELEM_SELECT)) {
			const float *sco_a = screen_vert_coords[BM_elem_index_get(be->v1)];
			const float *sco_b = screen_vert_coords[BM_elem_index_get(be->v2)];

			/* check for error value (vert cant be projected) */
			if ((sco_a[0] != FLT_MAX) && (sco_b[0] != FLT_MAX)) {
				isect = bm_edge_seg_isect(sco_a, sco_b, mouse_path, len, mode, &isected);

				if (isect != 0.0f) {
					if (mode != KNIFE_MULTICUT && mode != KNIFE_MIDPOINT) {
						BMO_slot_map_float_insert(&bmop, slot_edge_percents, be, isect);
					}
				}
			}
		}

		BMO_elem_flag_set(bm, be, ELE_EDGE_CUT, is_cut);
	}


	/* free all allocs */
	MEM_freeN(screen_vert_coords);
	MEM_freeN(mouse_path);


	BMO_slot_buffer_from_enabled_flag(bm, &bmop, bmop.slots_in, "edges", BM_EDGE, ELE_EDGE_CUT);

	if (mode == KNIFE_MIDPOINT) numcuts = 1;
	BMO_slot_int_set(bmop.slots_in, "cuts", numcuts);

	BMO_slot_int_set(bmop.slots_in, "quad_corner_type", SUBD_STRAIGHT_CUT);
	BMO_slot_bool_set(bmop.slots_in, "use_single_edge", false);
	BMO_slot_bool_set(bmop.slots_in, "use_grid_fill", false);

	BMO_slot_float_set(bmop.slots_in, "radius", 0);
	
	BMO_op_exec(bm, &bmop);
	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

#undef ELE_EDGE_CUT

void MESH_OT_knife_cut(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	ot->name = "Knife Cut";
	ot->description = "Cut selected edges and faces into parts";
	ot->idname = "MESH_OT_knife_cut";
	
	ot->invoke = WM_gesture_lines_invoke;
	ot->modal = WM_gesture_lines_modal;
	ot->exec = edbm_knife_cut_exec;
	
	ot->poll = EDBM_view3d_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", knife_items, KNIFE_EXACT, "Type", "");
	prop = RNA_def_property(ot->srna, "path", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_runtime(prop, &RNA_OperatorMousePath);
	
	/* internal */
	RNA_def_int(ot->srna, "cursor", BC_KNIFECURSOR, 0, INT_MAX, "Cursor", "", 0, INT_MAX);
}

static Base *mesh_separate_tagged(Main *bmain, Scene *scene, Base *base_old, BMesh *bm_old)
{
	Base *base_new;
	Object *obedit = base_old->object;
	BMesh *bm_new;

	bm_new = BM_mesh_create(&bm_mesh_allocsize_default);
	BM_mesh_elem_toolflags_ensure(bm_new);  /* needed for 'duplicate' bmo */

	CustomData_copy(&bm_old->vdata, &bm_new->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm_old->edata, &bm_new->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm_old->ldata, &bm_new->ldata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm_old->pdata, &bm_new->pdata, CD_MASK_BMESH, CD_CALLOC, 0);

	CustomData_bmesh_init_pool(&bm_new->vdata, bm_mesh_allocsize_default.totvert, BM_VERT);
	CustomData_bmesh_init_pool(&bm_new->edata, bm_mesh_allocsize_default.totedge, BM_EDGE);
	CustomData_bmesh_init_pool(&bm_new->ldata, bm_mesh_allocsize_default.totloop, BM_LOOP);
	CustomData_bmesh_init_pool(&bm_new->pdata, bm_mesh_allocsize_default.totface, BM_FACE);

	base_new = ED_object_add_duplicate(bmain, scene, base_old, USER_DUP_MESH);
	/* DAG_relations_tag_update(bmain); */ /* normally would call directly after but in this case delay recalc */
	assign_matarar(base_new->object, give_matarar(obedit), *give_totcolp(obedit)); /* new in 2.5 */

	ED_base_object_select(base_new, BA_SELECT);

	BMO_op_callf(bm_old, (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
	             "duplicate geom=%hvef dest=%p", BM_ELEM_TAG, bm_new);
	BMO_op_callf(bm_old, (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
	             "delete geom=%hvef context=%i", BM_ELEM_TAG, DEL_FACES);

	/* deselect loose data - this used to get deleted,
	 * we could de-select edges and verts only, but this turns out to be less complicated
	 * since de-selecting all skips selection flushing logic */
	BM_mesh_elem_hflag_disable_all(bm_old, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);

	BM_mesh_normals_update(bm_new);

	BM_mesh_bm_to_me(bm_new, base_new->object->data, false);

	BM_mesh_free(bm_new);
	((Mesh *)base_new->object->data)->edit_btmesh = NULL;
	
	return base_new;
}

static bool mesh_separate_selected(Main *bmain, Scene *scene, Base *base_old, BMesh *bm_old)
{
	/* we may have tags from previous operators */
	BM_mesh_elem_hflag_disable_all(bm_old, BM_FACE | BM_EDGE | BM_VERT, BM_ELEM_TAG, false);

	/* sel -> tag */
	BM_mesh_elem_hflag_enable_test(bm_old, BM_FACE | BM_EDGE | BM_VERT, BM_ELEM_TAG, true, false, BM_ELEM_SELECT);

	return (mesh_separate_tagged(bmain, scene, base_old, bm_old) != NULL);
}

/* flush a hflag to from verts to edges/faces */
static void bm_mesh_hflag_flush_vert(BMesh *bm, const char hflag)
{
	BMEdge *e;
	BMLoop *l_iter;
	BMLoop *l_first;
	BMFace *f;

	BMIter eiter;
	BMIter fiter;

	bool ok;

	BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e->v1, hflag) &&
		    BM_elem_flag_test(e->v2, hflag))
		{
			BM_elem_flag_enable(e, hflag);
		}
		else {
			BM_elem_flag_disable(e, hflag);
		}
	}
	BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
		ok = true;
		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			if (!BM_elem_flag_test(l_iter->v, hflag)) {
				ok = false;
				break;
			}
		} while ((l_iter = l_iter->next) != l_first);

		BM_elem_flag_set(f, hflag, ok);
	}
}

/**
 * Sets an object to a single material. from one of its slots.
 *
 * \note This could be used for split-by-material for non mesh types.
 * \note This could take material data from another object or args.
 */
static void mesh_separate_material_assign_mat_nr(Object *ob, const short mat_nr)
{
	ID *obdata = ob->data;

	Material ***matarar;
	const short *totcolp;

	totcolp = give_totcolp_id(obdata);
	matarar = give_matarar_id(obdata);

	if ((totcolp && matarar) == 0) {
		BLI_assert(0);
		return;
	}

	if (*totcolp) {
		Material *ma_ob;
		Material *ma_obdata;
		char matbit;

		if (mat_nr < ob->totcol) {
			ma_ob = ob->mat[mat_nr];
			matbit = ob->matbits[mat_nr];
		}
		else {
			ma_ob = NULL;
			matbit = 0;
		}

		if (mat_nr < *totcolp) {
			 ma_obdata = (*matarar)[mat_nr];
		}
		else {
			ma_obdata = NULL;
		}

		BKE_material_clear_id(obdata, true);
		BKE_material_resize_object(ob, 1, true);
		BKE_material_resize_id(obdata, 1, true);

		ob->mat[0] = ma_ob;
		ob->matbits[0] = matbit;
		(*matarar)[0] = ma_obdata;
	}
	else {
		BKE_material_clear_id(obdata, true);
		BKE_material_resize_object(ob, 0, true);
		BKE_material_resize_id(obdata, 0, true);
	}
}

static bool mesh_separate_material(Main *bmain, Scene *scene, Base *base_old, BMesh *bm_old)
{
	BMFace *f_cmp, *f;
	BMIter iter;
	bool result = false;

	while ((f_cmp = BM_iter_at_index(bm_old, BM_FACES_OF_MESH, NULL, 0))) {
		Base *base_new;
		const short mat_nr = f_cmp->mat_nr;
		int tot = 0;

		BM_mesh_elem_hflag_disable_all(bm_old, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

		BM_ITER_MESH (f, &iter, bm_old, BM_FACES_OF_MESH) {
			if (f->mat_nr == mat_nr) {
				BMLoop *l_iter;
				BMLoop *l_first;

				BM_elem_flag_enable(f, BM_ELEM_TAG);
				l_iter = l_first = BM_FACE_FIRST_LOOP(f);
				do {
					BM_elem_flag_enable(l_iter->v, BM_ELEM_TAG);
					BM_elem_flag_enable(l_iter->e, BM_ELEM_TAG);
				} while ((l_iter = l_iter->next) != l_first);

				tot++;
			}
		}

		/* leave the current object with some materials */
		if (tot == bm_old->totface) {
			mesh_separate_material_assign_mat_nr(base_old->object, mat_nr);

			/* since we're in editmode, must set faces here */
			BM_ITER_MESH (f, &iter, bm_old, BM_FACES_OF_MESH) {
				f->mat_nr = 0;
			}
			break;
		}

		/* Move selection into a separate object */
		base_new = mesh_separate_tagged(bmain, scene, base_old, bm_old);
		if (base_new) {
			mesh_separate_material_assign_mat_nr(base_new->object, mat_nr);
		}

		result |= (base_new != NULL);
	}

	return result;
}

static bool mesh_separate_loose(Main *bmain, Scene *scene, Base *base_old, BMesh *bm_old)
{
	int i;
	BMEdge *e;
	BMVert *v_seed;
	BMWalker walker;
	bool result = false;
	int max_iter = bm_old->totvert;

	/* Clear all selected vertices */
	BM_mesh_elem_hflag_disable_all(bm_old, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

	/* A "while (true)" loop should work here as each iteration should
	 * select and remove at least one vertex and when all vertices
	 * are selected the loop will break out. But guard against bad
	 * behavior by limiting iterations to the number of vertices in the
	 * original mesh.*/
	for (i = 0; i < max_iter; i++) {
		int tot = 0;
		/* Get a seed vertex to start the walk */
		v_seed = BM_iter_at_index(bm_old, BM_VERTS_OF_MESH, NULL, 0);

		/* No vertices available, can't do anything */
		if (v_seed == NULL) {
			break;
		}

		/* Select the seed explicitly, in case it has no edges */
		if (!BM_elem_flag_test(v_seed, BM_ELEM_TAG)) { BM_elem_flag_enable(v_seed, BM_ELEM_TAG); tot++; }

		/* Walk from the single vertex, selecting everything connected
		 * to it */
		BMW_init(&walker, bm_old, BMW_VERT_SHELL,
		         BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_FLAG_NOP,
		         BMW_NIL_LAY);

		for (e = BMW_begin(&walker, v_seed); e; e = BMW_step(&walker)) {
			if (!BM_elem_flag_test(e->v1, BM_ELEM_TAG)) { BM_elem_flag_enable(e->v1, BM_ELEM_TAG); tot++; }
			if (!BM_elem_flag_test(e->v2, BM_ELEM_TAG)) { BM_elem_flag_enable(e->v2, BM_ELEM_TAG); tot++; }
		}
		BMW_end(&walker);

		if (bm_old->totvert == tot) {
			/* Every vertex selected, nothing to separate, work is done */
			break;
		}

		/* Flush the selection to get edge/face selections matching
		 * the vertex selection */
		bm_mesh_hflag_flush_vert(bm_old, BM_ELEM_TAG);

		/* Move selection into a separate object */
		result |= (mesh_separate_tagged(bmain, scene, base_old, bm_old) != NULL);
	}

	return result;
}

static int edbm_separate_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	const int type = RNA_enum_get(op->ptr, "type");
	int retval = 0;
	
	if (ED_operator_editmesh(C)) {
		Base *base = CTX_data_active_base(C);
		BMEditMesh *em = BKE_editmesh_from_object(base->object);

		if (type == 0) {
			if ((em->bm->totvertsel == 0) &&
			    (em->bm->totedgesel == 0) &&
			    (em->bm->totfacesel == 0))
			{
				BKE_report(op->reports, RPT_ERROR, "Nothing selected");
				return OPERATOR_CANCELLED;
			}
		}

		/* editmode separate */
		if      (type == 0) retval = mesh_separate_selected(bmain, scene, base, em->bm);
		else if (type == 1) retval = mesh_separate_material(bmain, scene, base, em->bm);
		else if (type == 2) retval = mesh_separate_loose(bmain, scene, base, em->bm);
		else                BLI_assert(0);

		if (retval) {
			EDBM_update_generic(em, true, true);
		}
	}
	else {
		if (type == 0) {
			BKE_report(op->reports, RPT_ERROR, "Selection not supported in object mode");
			return OPERATOR_CANCELLED;
		}

		/* object mode separate */
		CTX_DATA_BEGIN(C, Base *, base_iter, selected_editable_bases)
		{
			Object *ob = base_iter->object;
			if (ob->type == OB_MESH) {
				Mesh *me = ob->data;
				if (me->id.lib == NULL) {
					BMesh *bm_old = NULL;
					int retval_iter = 0;

					bm_old = BM_mesh_create(&bm_mesh_allocsize_default);

					BM_mesh_bm_from_me(bm_old, me, false, false, 0);

					if      (type == 1) retval_iter = mesh_separate_material(bmain, scene, base_iter, bm_old);
					else if (type == 2) retval_iter = mesh_separate_loose(bmain, scene, base_iter, bm_old);
					else                BLI_assert(0);

					if (retval_iter) {
						BM_mesh_bm_to_me(bm_old, me, false);

						DAG_id_tag_update(&me->id, OB_RECALC_DATA);
						WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);
					}

					BM_mesh_free(bm_old);

					retval |= retval_iter;
				}
			}
		}
		CTX_DATA_END;
	}

	if (retval) {
		/* delay depsgraph recalc until all objects are duplicated */
		DAG_relations_tag_update(bmain);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, NULL);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

/* *************** Operator: separate parts *************/

static EnumPropertyItem prop_separate_types[] = {
	{0, "SELECTED", 0, "Selection", ""},
	{1, "MATERIAL", 0, "By Material", ""},
	{2, "LOOSE", 0, "By loose parts", ""},
	{0, NULL, 0, NULL, NULL}
};

void MESH_OT_separate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Separate";
	ot->description = "Separate selected geometry into a new mesh";
	ot->idname = "MESH_OT_separate";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = edbm_separate_exec;
	ot->poll = ED_operator_scene_editable; /* object and editmode */
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	ot->prop = RNA_def_enum(ot->srna, "type", prop_separate_types, 0, "Type", "");
}


static int edbm_fill_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const bool use_beauty = RNA_boolean_get(op->ptr, "use_beauty");
	BMOperator bmop;
	
	if (!EDBM_op_init(em, &bmop, op,
	                  "triangle_fill edges=%he use_beauty=%b",
	                  BM_ELEM_SELECT, use_beauty))
	{
		return OPERATOR_CANCELLED;
	}
	
	BMO_op_exec(em->bm, &bmop);
	
	/* select new geometry */
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "geom.out", BM_FACE | BM_EDGE, BM_ELEM_SELECT, true);
	
	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);
	
	return OPERATOR_FINISHED;

}

void MESH_OT_fill(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Fill";
	ot->idname = "MESH_OT_fill";
	ot->description = "Fill a selected edge loop with faces";

	/* api callbacks */
	ot->exec = edbm_fill_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "use_beauty", true, "Beauty", "Use best triangulation division");
}


/* -------------------------------------------------------------------- */
/* Grid Fill (and helper functions) */

static bool bm_edge_test_fill_grid_cb(BMEdge *e, void *UNUSED(bm_v))
{
	return BM_elem_flag_test_bool(e, BM_ELEM_TAG);
}

static float edbm_fill_grid_vert_tag_angle(BMVert *v)
{
	BMIter iter;
	BMEdge *e_iter;
	BMVert *v_pair[2];
	int i = 0;
	BM_ITER_ELEM (e_iter, &iter, v, BM_EDGES_OF_VERT) {
		if (BM_elem_flag_test(e_iter, BM_ELEM_TAG)) {
			v_pair[i++] = BM_edge_other_vert(e_iter, v);
		}
	}
	BLI_assert(i == 2);

	return fabsf((float)M_PI - angle_v3v3v3(v_pair[0]->co, v->co, v_pair[1]->co));
}

/**
 * non-essential utility function to select 2 open edge loops from a closed loop.
 */
static void edbm_fill_grid_prepare(BMesh *bm, int offset, int *r_span, bool span_calc)
{
	BMEdge *e;
	BMIter iter;
	int count;
	int span = *r_span;

	ListBase eloops = {NULL};
	struct BMEdgeLoopStore *el_store;
	// LinkData *el_store;

	/* select -> tag */
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		BM_elem_flag_set(e, BM_ELEM_TAG, BM_elem_flag_test(e, BM_ELEM_SELECT));
	}

	count = BM_mesh_edgeloops_find(bm, &eloops, bm_edge_test_fill_grid_cb, bm);
	el_store = eloops.first;

	if (count == 1 && BM_edgeloop_is_closed(el_store) && (BM_edgeloop_length_get(el_store) & 1) == 0) {
		/* be clever! detect 2 edge loops from one closed edge loop */
		const int verts_len = BM_edgeloop_length_get(el_store);
		ListBase *verts = BM_edgeloop_verts_get(el_store);
		BMVert *v_act = BM_mesh_active_vert_get(bm);
		LinkData *v_act_link;
		BMEdge **edges = MEM_mallocN(sizeof(*edges) * verts_len, __func__);
		int i;

		if (v_act && (v_act_link = BLI_findptr(verts, v_act, offsetof(LinkData, data)))) {
			/* pass */
		}
		else {
			/* find the vertex with the best angle (a corner vertex) */
			LinkData *v_link, *v_link_best = NULL;
			float angle_best = -1.0f;
			for (v_link = verts->first; v_link; v_link = v_link->next) {
				const float angle = edbm_fill_grid_vert_tag_angle(v_link->data);
				if ((angle > angle_best) || (v_link_best == NULL)) {
					angle_best = angle;
					v_link_best = v_link;
				}
			}

			v_act_link = v_link_best;
			v_act = v_act_link->data;
		}

		if (offset != 0) {
			v_act_link = BLI_findlink(verts, offset);
			v_act = v_act_link->data;
		}

		/* set this vertex first */
		BLI_listbase_rotate_first(verts, v_act_link);
		BM_edgeloop_edges_get(el_store, edges);


		if (span_calc) {
			/* calculate the span by finding the next corner in 'verts'
			 * we dont know what defines a corner exactly so find the 4 verts
			 * in the loop with the greatest angle.
			 * Tag them and use the first tagged vertex to calculate the span.
			 *
			 * note: we may have already checked 'edbm_fill_grid_vert_tag_angle()' on each
			 * vert, but advantage of de-duplicating is minimal. */
			struct SortPointerByFloat *ele_sort = MEM_mallocN(sizeof(*ele_sort) * verts_len, __func__);
			LinkData *v_link;
			for (v_link = verts->first, i = 0; v_link; v_link = v_link->next, i++) {
				BMVert *v = v_link->data;
				const float angle = edbm_fill_grid_vert_tag_angle(v);
				ele_sort[i].sort_value = angle;
				ele_sort[i].data = v;

				BM_elem_flag_disable(v, BM_ELEM_TAG);
			}

			qsort(ele_sort, verts_len, sizeof(*ele_sort), BLI_sortutil_cmp_float_reverse);

			for (i = 0; i < 4; i++) {
				BMVert *v = ele_sort[i].data;
				BM_elem_flag_enable(v, BM_ELEM_TAG);
			}

			/* now find the first... */
			for (v_link = verts->first, i = 0; i < verts_len / 2; v_link = v_link->next, i++) {
				BMVert *v = v_link->data;
				if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
					if (v != v_act) {
						span = i;
						break;
					}
				}
			}
			MEM_freeN(ele_sort);
		}
		/* end span calc */


		/* un-flag 'rails' */
		for (i = 0; i < span; i++) {
			BM_elem_flag_disable(edges[i], BM_ELEM_TAG);
			BM_elem_flag_disable(edges[(verts_len / 2) + i], BM_ELEM_TAG);
		}
		MEM_freeN(edges);
	}
	/* else let the bmesh-operator handle it */

	BM_mesh_edgeloops_free(&eloops);

	*r_span = span;
}

static int edbm_fill_grid_exec(bContext *C, wmOperator *op)
{
	BMOperator bmop;
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const short use_smooth = edbm_add_edge_face__smooth_get(em->bm);
	const int totedge_orig = em->bm->totedge;
	const int totface_orig = em->bm->totface;
	const bool use_interp_simple = RNA_boolean_get(op->ptr, "use_interp_simple");
	const bool use_prepare = true;


	if (use_prepare) {
		/* use when we have a single loop selected */
		PropertyRNA *prop_span = RNA_struct_find_property(op->ptr, "span");
		PropertyRNA *prop_offset = RNA_struct_find_property(op->ptr, "offset");
		bool calc_span;

		const int clamp = em->bm->totvertsel;
		int span;
		int offset;

		if (RNA_property_is_set(op->ptr, prop_span)) {
			span = RNA_property_int_get(op->ptr, prop_span);
			span = min_ii(span, (clamp / 2) - 1);
			calc_span = false;
		}
		else {
			span = clamp / 4;
			calc_span = true;
		}

		offset = RNA_property_int_get(op->ptr, prop_offset);
		offset = clamp ? mod_i(offset, clamp) : 0;

		/* in simple cases, move selection for tags, but also support more advanced cases */
		edbm_fill_grid_prepare(em->bm, offset, &span, calc_span);

		RNA_property_int_set(op->ptr, prop_span, span);
	}
	/* end tricky prepare code */


	if (!EDBM_op_init(em, &bmop, op,
	                  "grid_fill edges=%he mat_nr=%i use_smooth=%b use_interp_simple=%b",
	                  use_prepare ? BM_ELEM_TAG : BM_ELEM_SELECT,
	                  em->mat_nr, use_smooth, use_interp_simple))
	{
		return OPERATOR_CANCELLED;
	}

	BMO_op_exec(em->bm, &bmop);

	/* cancel if nothing was done */
	if ((totedge_orig == em->bm->totedge) &&
	    (totface_orig == em->bm->totface))
	{
		EDBM_op_finish(em, &bmop, op, true);
		return OPERATOR_CANCELLED;
	}

	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_fill_grid(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Grid Fill";
	ot->description = "Fill grid from two loops";
	ot->idname = "MESH_OT_fill_grid";

	/* api callbacks */
	ot->exec = edbm_fill_grid_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_int(ot->srna, "span", 1, 1, INT_MAX, "Span", "Number of sides (zero disables)", 1, 100);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_int(ot->srna, "offset", 0, INT_MIN, INT_MAX, "Offset", "Number of sides (zero disables)", -100, 100);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	RNA_def_boolean(ot->srna, "use_interp_simple", 0, "Simple Blending", "");
}

static int edbm_fill_holes_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const int sides = RNA_int_get(op->ptr, "sides");

	if (!EDBM_op_call_and_selectf(
	        em, op,
	        "faces.out", true,
	        "holes_fill edges=%he sides=%i",
	        BM_ELEM_SELECT, sides))
	{
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;

}

void MESH_OT_fill_holes(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Fill Holes";
	ot->idname = "MESH_OT_fill_holes";
	ot->description = "Fill in holes (boundary edge loops)";

	/* api callbacks */
	ot->exec = edbm_fill_holes_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_int(ot->srna, "sides", 4, 0, INT_MAX, "Sides", "Number of sides in hole required to fill (zero fills all holes)", 0, 100);
}

static int edbm_beautify_fill_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	const float angle_max = M_PI;
	const float angle_limit = RNA_float_get(op->ptr, "angle_limit");
	char hflag;

	if (angle_limit >= angle_max) {
		hflag = BM_ELEM_SELECT;
	}
	else {
		BMIter iter;
		BMEdge *e;

		BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
			BM_elem_flag_set(e, BM_ELEM_TAG,
			                 (BM_elem_flag_test(e, BM_ELEM_SELECT) &&
			                  BM_edge_calc_face_angle_ex(e, angle_max) < angle_limit));

		}

		hflag = BM_ELEM_TAG;
	}

	if (!EDBM_op_call_and_selectf(
	        em, op, "geom.out", true,
	        "beautify_fill faces=%hf edges=%he",
	        BM_ELEM_SELECT, hflag))
	{
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_beautify_fill(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Beautify Faces";
	ot->idname = "MESH_OT_beautify_fill";
	ot->description = "Rearrange some faces to try to get less degenerated geometry";

	/* api callbacks */
	ot->exec = edbm_beautify_fill_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	prop = RNA_def_float_rotation(ot->srna, "angle_limit", 0, NULL, 0.0f, DEG2RADF(180.0f),
	                              "Max Angle", "Angle limit", 0.0f, DEG2RADF(180.0f));
	RNA_def_property_float_default(prop, DEG2RADF(180.0f));
}


/********************** Poke Face **********************/

static int edbm_poke_face_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMOperator bmop;

	const float offset = RNA_float_get(op->ptr, "offset");
	const bool use_relative_offset = RNA_boolean_get(op->ptr, "use_relative_offset");
	const int center_mode = RNA_enum_get(op->ptr, "center_mode");

	EDBM_op_init(em, &bmop, op, "poke faces=%hf offset=%f use_relative_offset=%b center_mode=%i",
	             BM_ELEM_SELECT, offset, use_relative_offset, center_mode);
	BMO_op_exec(em->bm, &bmop);

	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "verts.out", BM_VERT, BM_ELEM_SELECT, true);
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_mesh_normals_update(em);

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;

}

void MESH_OT_poke(wmOperatorType *ot)
{

	static EnumPropertyItem poke_center_modes[] = {
		{BMOP_POKE_MEAN_WEIGHTED, "MEAN_WEIGHTED", 0, "Weighted Mean", "Weighted Mean Face Center"},
		{BMOP_POKE_MEAN, "MEAN", 0, "Mean", "Mean Face Center"},
		{BMOP_POKE_BOUNDS, "BOUNDS", 0, "Bounds", "Face Bounds Center"},
		{0, NULL, 0, NULL, NULL}};


	/* identifiers */
	ot->name = "Poke Faces";
	ot->idname = "MESH_OT_poke";
	ot->description = "Split a face into a fan";

	/* api callbacks */
	ot->exec = edbm_poke_face_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_float(ot->srna, "offset", 0.0f, -FLT_MAX, FLT_MAX, "Poke Offset", "Poke Offset", -1.0f, 1.0f);
	RNA_def_boolean(ot->srna, "use_relative_offset", false, "Offset Relative", "Scale the offset by surrounding geometry");
	RNA_def_enum(ot->srna, "center_mode", poke_center_modes, BMOP_POKE_MEAN_WEIGHTED, "Poke Center", "Poke Face Center Calculation");
}

/********************** Quad/Tri Operators *************************/

static int edbm_quads_convert_to_tris_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMOperator bmop;
	const int quad_method = RNA_enum_get(op->ptr, "quad_method");
	const int ngon_method = RNA_enum_get(op->ptr, "ngon_method");

	EDBM_op_init(em, &bmop, op, "triangulate faces=%hf quad_method=%i ngon_method=%i", BM_ELEM_SELECT, quad_method, ngon_method);
	BMO_op_exec(em->bm, &bmop);

	/* select the output */
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);
	EDBM_selectmode_flush(em);

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}


void MESH_OT_quads_convert_to_tris(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Triangulate Faces";
	ot->idname = "MESH_OT_quads_convert_to_tris";
	ot->description = "Triangulate selected faces";

	/* api callbacks */
	ot->exec = edbm_quads_convert_to_tris_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "quad_method", modifier_triangulate_quad_method_items, MOD_TRIANGULATE_QUAD_BEAUTY,
	             "Quad Method", "Method for splitting the quads into triangles");
	RNA_def_enum(ot->srna, "ngon_method", modifier_triangulate_ngon_method_items, MOD_TRIANGULATE_NGON_BEAUTY,
	             "Polygon Method", "Method for splitting the polygons into triangles");
}

static int edbm_tris_convert_to_quads_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	int dosharp, douvs, dovcols, domaterials;
	const float limit = RNA_float_get(op->ptr, "limit");

	dosharp = RNA_boolean_get(op->ptr, "sharp");
	douvs = RNA_boolean_get(op->ptr, "uvs");
	dovcols = RNA_boolean_get(op->ptr, "vcols");
	domaterials = RNA_boolean_get(op->ptr, "materials");

	if (!EDBM_op_call_and_selectf(
	        em, op,
	        "faces.out", true,
	        "join_triangles faces=%hf limit=%f cmp_sharp=%b cmp_uvs=%b cmp_vcols=%b cmp_materials=%b",
	        BM_ELEM_SELECT, limit, dosharp, douvs, dovcols, domaterials))
	{
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

static void join_triangle_props(wmOperatorType *ot)
{
	PropertyRNA *prop;

	prop = RNA_def_float_rotation(ot->srna, "limit", 0, NULL, 0.0f, DEG2RADF(180.0f),
	                              "Max Angle", "Angle Limit", 0.0f, DEG2RADF(180.0f));
	RNA_def_property_float_default(prop, DEG2RADF(40.0f));

	RNA_def_boolean(ot->srna, "uvs", 0, "Compare UVs", "");
	RNA_def_boolean(ot->srna, "vcols", 0, "Compare VCols", "");
	RNA_def_boolean(ot->srna, "sharp", 0, "Compare Sharp", "");
	RNA_def_boolean(ot->srna, "materials", 0, "Compare Materials", "");
}

void MESH_OT_tris_convert_to_quads(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Tris to Quads";
	ot->idname = "MESH_OT_tris_convert_to_quads";
	ot->description = "Join triangles into quads";

	/* api callbacks */
	ot->exec = edbm_tris_convert_to_quads_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	join_triangle_props(ot);
}

/* -------------------------------------------------------------------- */
/* Dissolve */

static void edbm_dissolve_prop__use_verts(wmOperatorType *ot)
{
	RNA_def_boolean(ot->srna, "use_verts", 0, "Dissolve Verts",
	                "Dissolve remaining vertices");
}
static void edbm_dissolve_prop__use_face_split(wmOperatorType *ot)
{
	RNA_def_boolean(ot->srna, "use_face_split", 0, "Face Split",
	                "Split off face corners to maintain surrounding geometry");
}
static void edbm_dissolve_prop__use_boundary_tear(wmOperatorType *ot)
{
	RNA_def_boolean(ot->srna, "use_boundary_tear", 0, "Tear Boundary",
	                "Split off face corners instead of merging faces");
}

static int edbm_dissolve_verts_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	const bool use_face_split = RNA_boolean_get(op->ptr, "use_face_split");
	const bool use_boundary_tear = RNA_boolean_get(op->ptr, "use_boundary_tear");

	if (!EDBM_op_callf(em, op,
	                   "dissolve_verts verts=%hv use_face_split=%b use_boundary_tear=%b",
	                   BM_ELEM_SELECT, use_face_split, use_boundary_tear))
	{
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_dissolve_verts(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Dissolve Vertices";
	ot->description = "Dissolve verts, merge edges and faces";
	ot->idname = "MESH_OT_dissolve_verts";

	/* api callbacks */
	ot->exec = edbm_dissolve_verts_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	edbm_dissolve_prop__use_face_split(ot);
	edbm_dissolve_prop__use_boundary_tear(ot);
}

static int edbm_dissolve_edges_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	const bool use_verts = RNA_boolean_get(op->ptr, "use_verts");
	const bool use_face_split = RNA_boolean_get(op->ptr, "use_face_split");

	if (!EDBM_op_callf(em, op,
	                   "dissolve_edges edges=%he use_verts=%b use_face_split=%b",
	                   BM_ELEM_SELECT, use_verts, use_face_split))
	{
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_dissolve_edges(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Dissolve Edges";
	ot->description = "Dissolve edges, merging faces";
	ot->idname = "MESH_OT_dissolve_edges";

	/* api callbacks */
	ot->exec = edbm_dissolve_edges_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	edbm_dissolve_prop__use_verts(ot);
	edbm_dissolve_prop__use_face_split(ot);
}

static int edbm_dissolve_faces_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	const bool use_verts = RNA_boolean_get(op->ptr, "use_verts");

	if (!EDBM_op_call_and_selectf(
	        em, op,
	        "region.out", true,
	        "dissolve_faces faces=%hf use_verts=%b",
	        BM_ELEM_SELECT, use_verts))
	{
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_dissolve_faces(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Dissolve Faces";
	ot->description = "Dissolve faces";
	ot->idname = "MESH_OT_dissolve_faces";

	/* api callbacks */
	ot->exec = edbm_dissolve_faces_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	edbm_dissolve_prop__use_verts(ot);
}


static int edbm_dissolve_mode_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	if (em->selectmode & SCE_SELECT_VERTEX) {
		return edbm_dissolve_verts_exec(C, op);
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		return edbm_dissolve_edges_exec(C, op);
	}
	else {
		return edbm_dissolve_faces_exec(C, op);
	}
}

void MESH_OT_dissolve_mode(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Dissolve Selection";
	ot->description = "Dissolve geometry based on the selection mode";
	ot->idname = "MESH_OT_dissolve_mode";

	/* api callbacks */
	ot->exec = edbm_dissolve_mode_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	edbm_dissolve_prop__use_verts(ot);
	edbm_dissolve_prop__use_face_split(ot);
	edbm_dissolve_prop__use_boundary_tear(ot);
}

static int edbm_dissolve_limited_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	const float angle_limit = RNA_float_get(op->ptr, "angle_limit");
	const bool use_dissolve_boundaries = RNA_boolean_get(op->ptr, "use_dissolve_boundaries");
	const int delimit = RNA_enum_get(op->ptr, "delimit");

	char dissolve_flag;

	if (em->selectmode == SCE_SELECT_FACE) {
		/* flush selection to tags and untag edges/verts with partially selected faces */
		BMIter iter;
		BMIter liter;

		BMElem *ele;
		BMFace *f;
		BMLoop *l;

		BM_ITER_MESH (ele, &iter, bm, BM_VERTS_OF_MESH) {
			BM_elem_flag_set(ele, BM_ELEM_TAG, BM_elem_flag_test(ele, BM_ELEM_SELECT));
		}
		BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
			BM_elem_flag_set(ele, BM_ELEM_TAG, BM_elem_flag_test(ele, BM_ELEM_SELECT));
		}

		BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(f, BM_ELEM_SELECT)) {
				BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
					BM_elem_flag_disable(l->v, BM_ELEM_TAG);
					BM_elem_flag_disable(l->e, BM_ELEM_TAG);
				}
			}
		}

		dissolve_flag = BM_ELEM_TAG;
	}
	else {
		dissolve_flag = BM_ELEM_SELECT;
	}

	EDBM_op_call_and_selectf(
	            em, op, "region.out", true,
	            "dissolve_limit edges=%he verts=%hv angle_limit=%f use_dissolve_boundaries=%b delimit=%i",
	            dissolve_flag, dissolve_flag, angle_limit, use_dissolve_boundaries, delimit);

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_dissolve_limited(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Limited Dissolve";
	ot->idname = "MESH_OT_dissolve_limited";
	ot->description = "Dissolve selected edges and verts, limited by the angle of surrounding geometry";

	/* api callbacks */
	ot->exec = edbm_dissolve_limited_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_float_rotation(ot->srna, "angle_limit", 0, NULL, 0.0f, DEG2RADF(180.0f),
	                              "Max Angle", "Angle limit", 0.0f, DEG2RADF(180.0f));
	RNA_def_property_float_default(prop, DEG2RADF(5.0f));
	RNA_def_boolean(ot->srna, "use_dissolve_boundaries", 0, "All Boundaries",
	                "Dissolve all vertices inbetween face boundaries");
	RNA_def_enum_flag(ot->srna, "delimit", mesh_delimit_mode_items, 0, "Delimit",
	                  "Delimit dissolve operation");
}

static int edbm_dissolve_degenerate_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const float thresh = RNA_float_get(op->ptr, "threshold");
	BMesh *bm = em->bm;
	const int totelem[3] = {bm->totvert, bm->totedge, bm->totface};

	if (!EDBM_op_callf(
	        em, op,
	        "dissolve_degenerate edges=%he dist=%f",
	        BM_ELEM_SELECT, thresh))
	{
		return OPERATOR_CANCELLED;
	}

	/* tricky to maintain correct selection here, so just flush up from verts */
	EDBM_select_flush(em);

	EDBM_update_generic(em, true, true);

	edbm_report_delete_info(op->reports, bm, totelem);

	return OPERATOR_FINISHED;
}

void MESH_OT_dissolve_degenerate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Degenerate Dissolve";
	ot->idname = "MESH_OT_dissolve_degenerate";
	ot->description = "Dissolve zero area faces and zero length edges";

	/* api callbacks */
	ot->exec = edbm_dissolve_degenerate_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_float(ot->srna, "threshold", 0.0001f, 0.000001f, 50.0f,  "Merge Distance",
	              "Minimum distance between elements to merge", 0.00001, 10.0);
}


/* internally uses dissolve */
static int edbm_delete_edgeloop_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	const bool use_face_split = RNA_boolean_get(op->ptr, "use_face_split");

	/* deal with selection */
	{
		BMEdge *e;
		BMIter iter;

		BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, false);

		BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(e, BM_ELEM_SELECT) && e->l) {
				BMLoop *l_iter = e->l;
				do {
					BM_elem_flag_enable(l_iter->f, BM_ELEM_TAG);
				} while ((l_iter = l_iter->radial_next) != e->l);
			}
		}
	}

	if (!EDBM_op_callf(em, op,
	                   "dissolve_edges edges=%he use_verts=%b use_face_split=%b",
	                   BM_ELEM_SELECT, true, use_face_split))
	{
		return OPERATOR_CANCELLED;
	}

	BM_mesh_elem_hflag_enable_test(em->bm, BM_FACE, BM_ELEM_SELECT, true, false, BM_ELEM_TAG);

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_delete_edgeloop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Edge Loop";
	ot->description = "Delete an edge loop by merging the faces on each side";
	ot->idname = "MESH_OT_delete_edgeloop";

	/* api callbacks */
	ot->exec = edbm_delete_edgeloop_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "use_face_split", true, "Face Split",
	                "Split off face corners to maintain surrounding geometry");
}

static int edbm_split_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMOperator bmop;

	EDBM_op_init(em, &bmop, op, "split geom=%hvef use_only_faces=%b", BM_ELEM_SELECT, false);
	BMO_op_exec(em->bm, &bmop);
	BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "geom.out", BM_ALL_NOLOOP, BM_ELEM_SELECT, true);
	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	/* Geometry has changed, need to recalc normals and looptris */
	EDBM_mesh_normals_update(em);

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_split(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Split";
	ot->idname = "MESH_OT_split";
	ot->description = "Split off selected geometry from connected unselected geometry";

	/* api callbacks */
	ot->exec = edbm_split_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************************************************************************
 * qsort routines.
 * Now unified, for vertices/edges/faces. */

enum {
	SRT_VIEW_ZAXIS = 1,  /* Use view Z (deep) axis. */
	SRT_VIEW_XAXIS,      /* Use view X (left to right) axis. */
	SRT_CURSOR_DISTANCE, /* Use distance from element to 3D cursor. */
	SRT_MATERIAL,        /* Face only: use mat number. */
	SRT_SELECTED,        /* Move selected elements in first, without modifying 
	                      * relative order of selected and unselected elements. */
	SRT_RANDOMIZE,       /* Randomize selected elements. */
	SRT_REVERSE,         /* Reverse current order of selected elements. */
};

typedef struct BMElemSort {
	float srt; /* Sort factor */
	int org_idx; /* Original index of this element _in its mempool_ */
} BMElemSort;

static int bmelemsort_comp(const void *v1, const void *v2)
{
	const BMElemSort *x1 = v1, *x2 = v2;

	return (x1->srt > x2->srt) - (x1->srt < x2->srt);
}

/* Reorders vertices/edges/faces using a given methods. Loops are not supported. */
static void sort_bmelem_flag(Scene *scene, Object *ob,
                             View3D *v3d, RegionView3D *rv3d,
                             const int types, const int flag, const int action,
                             const int reverse, const unsigned int seed)
{
	BMEditMesh *em = BKE_editmesh_from_object(ob);

	BMVert *ve;
	BMEdge *ed;
	BMFace *fa;
	BMIter iter;

	/* In all five elements below, 0 = vertices, 1 = edges, 2 = faces. */
	/* Just to mark protected elements. */
	char *pblock[3] = {NULL, NULL, NULL}, *pb;
	BMElemSort *sblock[3] = {NULL, NULL, NULL}, *sb;
	unsigned int *map[3] = {NULL, NULL, NULL}, *mp;
	int totelem[3] = {0, 0, 0};
	int affected[3] = {0, 0, 0};
	int i, j;

	if (!(types && flag && action))
		return;

	if (types & BM_VERT)
		totelem[0] = em->bm->totvert;
	if (types & BM_EDGE)
		totelem[1] = em->bm->totedge;
	if (types & BM_FACE)
		totelem[2] = em->bm->totface;

	if (ELEM(action, SRT_VIEW_ZAXIS, SRT_VIEW_XAXIS)) {
		float mat[4][4];
		float fact = reverse ? -1.0 : 1.0;
		int coidx = (action == SRT_VIEW_ZAXIS) ? 2 : 0;

		mul_m4_m4m4(mat, rv3d->viewmat, ob->obmat);  /* Apply the view matrix to the object matrix. */

		if (totelem[0]) {
			pb = pblock[0] = MEM_callocN(sizeof(char) * totelem[0], "sort_bmelem vert pblock");
			sb = sblock[0] = MEM_callocN(sizeof(BMElemSort) * totelem[0], "sort_bmelem vert sblock");

			BM_ITER_MESH_INDEX (ve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
				if (BM_elem_flag_test(ve, flag)) {
					float co[3];
					mul_v3_m4v3(co, mat, ve->co);

					pb[i] = false;
					sb[affected[0]].org_idx = i;
					sb[affected[0]++].srt = co[coidx] * fact;
				}
				else {
					pb[i] = true;
				}
			}
		}

		if (totelem[1]) {
			pb = pblock[1] = MEM_callocN(sizeof(char) * totelem[1], "sort_bmelem edge pblock");
			sb = sblock[1] = MEM_callocN(sizeof(BMElemSort) * totelem[1], "sort_bmelem edge sblock");

			BM_ITER_MESH_INDEX (ed, &iter, em->bm, BM_EDGES_OF_MESH, i) {
				if (BM_elem_flag_test(ed, flag)) {
					float co[3];
					mid_v3_v3v3(co, ed->v1->co, ed->v2->co);
					mul_m4_v3(mat, co);

					pb[i] = false;
					sb[affected[1]].org_idx = i;
					sb[affected[1]++].srt = co[coidx] * fact;
				}
				else {
					pb[i] = true;
				}
			}
		}

		if (totelem[2]) {
			pb = pblock[2] = MEM_callocN(sizeof(char) * totelem[2], "sort_bmelem face pblock");
			sb = sblock[2] = MEM_callocN(sizeof(BMElemSort) * totelem[2], "sort_bmelem face sblock");

			BM_ITER_MESH_INDEX (fa, &iter, em->bm, BM_FACES_OF_MESH, i) {
				if (BM_elem_flag_test(fa, flag)) {
					float co[3];
					BM_face_calc_center_mean(fa, co);
					mul_m4_v3(mat, co);

					pb[i] = false;
					sb[affected[2]].org_idx = i;
					sb[affected[2]++].srt = co[coidx] * fact;
				}
				else {
					pb[i] = true;
				}
			}
		}
	}

	else if (action == SRT_CURSOR_DISTANCE) {
		float cur[3];
		float mat[4][4];
		float fact = reverse ? -1.0 : 1.0;

		if (v3d && v3d->localvd)
			copy_v3_v3(cur, v3d->cursor);
		else
			copy_v3_v3(cur, scene->cursor);
		invert_m4_m4(mat, ob->obmat);
		mul_m4_v3(mat, cur);

		if (totelem[0]) {
			pb = pblock[0] = MEM_callocN(sizeof(char) * totelem[0], "sort_bmelem vert pblock");
			sb = sblock[0] = MEM_callocN(sizeof(BMElemSort) * totelem[0], "sort_bmelem vert sblock");

			BM_ITER_MESH_INDEX (ve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
				if (BM_elem_flag_test(ve, flag)) {
					pb[i] = false;
					sb[affected[0]].org_idx = i;
					sb[affected[0]++].srt = len_squared_v3v3(cur, ve->co) * fact;
				}
				else {
					pb[i] = true;
				}
			}
		}

		if (totelem[1]) {
			pb = pblock[1] = MEM_callocN(sizeof(char) * totelem[1], "sort_bmelem edge pblock");
			sb = sblock[1] = MEM_callocN(sizeof(BMElemSort) * totelem[1], "sort_bmelem edge sblock");

			BM_ITER_MESH_INDEX (ed, &iter, em->bm, BM_EDGES_OF_MESH, i) {
				if (BM_elem_flag_test(ed, flag)) {
					float co[3];
					mid_v3_v3v3(co, ed->v1->co, ed->v2->co);

					pb[i] = false;
					sb[affected[1]].org_idx = i;
					sb[affected[1]++].srt = len_squared_v3v3(cur, co) * fact;
				}
				else {
					pb[i] = true;
				}
			}
		}

		if (totelem[2]) {
			pb = pblock[2] = MEM_callocN(sizeof(char) * totelem[2], "sort_bmelem face pblock");
			sb = sblock[2] = MEM_callocN(sizeof(BMElemSort) * totelem[2], "sort_bmelem face sblock");

			BM_ITER_MESH_INDEX (fa, &iter, em->bm, BM_FACES_OF_MESH, i) {
				if (BM_elem_flag_test(fa, flag)) {
					float co[3];
					BM_face_calc_center_mean(fa, co);

					pb[i] = false;
					sb[affected[2]].org_idx = i;
					sb[affected[2]++].srt = len_squared_v3v3(cur, co) * fact;
				}
				else {
					pb[i] = true;
				}
			}
		}
	}

	/* Faces only! */
	else if (action == SRT_MATERIAL && totelem[2]) {
		pb = pblock[2] = MEM_callocN(sizeof(char) * totelem[2], "sort_bmelem face pblock");
		sb = sblock[2] = MEM_callocN(sizeof(BMElemSort) * totelem[2], "sort_bmelem face sblock");

		BM_ITER_MESH_INDEX (fa, &iter, em->bm, BM_FACES_OF_MESH, i) {
			if (BM_elem_flag_test(fa, flag)) {
				/* Reverse materials' order, not order of faces inside each mat! */
				/* Note: cannot use totcol, as mat_nr may sometimes be greater... */
				float srt = reverse ? (float)(MAXMAT - fa->mat_nr) : (float)fa->mat_nr;
				pb[i] = false;
				sb[affected[2]].org_idx = i;
				/* Multiplying with totface and adding i ensures us we keep current order for all faces of same mat. */
				sb[affected[2]++].srt = srt * ((float)totelem[2]) + ((float)i);
/*				printf("e: %d; srt: %f; final: %f\n", i, srt, srt * ((float)totface) + ((float)i));*/
			}
			else {
				pb[i] = true;
			}
		}
	}

	else if (action == SRT_SELECTED) {
		unsigned int *tbuf[3] = {NULL, NULL, NULL}, *tb;

		if (totelem[0]) {
			tb = tbuf[0] = MEM_callocN(sizeof(int) * totelem[0], "sort_bmelem vert tbuf");
			mp = map[0] = MEM_callocN(sizeof(int) * totelem[0], "sort_bmelem vert map");

			BM_ITER_MESH_INDEX (ve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
				if (BM_elem_flag_test(ve, flag)) {
					mp[affected[0]++] = i;
				}
				else {
					*tb = i;
					tb++;
				}
			}
		}

		if (totelem[1]) {
			tb = tbuf[1] = MEM_callocN(sizeof(int) * totelem[1], "sort_bmelem edge tbuf");
			mp = map[1] = MEM_callocN(sizeof(int) * totelem[1], "sort_bmelem edge map");

			BM_ITER_MESH_INDEX (ed, &iter, em->bm, BM_EDGES_OF_MESH, i) {
				if (BM_elem_flag_test(ed, flag)) {
					mp[affected[1]++] = i;
				}
				else {
					*tb = i;
					tb++;
				}
			}
		}

		if (totelem[2]) {
			tb = tbuf[2] = MEM_callocN(sizeof(int) * totelem[2], "sort_bmelem face tbuf");
			mp = map[2] = MEM_callocN(sizeof(int) * totelem[2], "sort_bmelem face map");

			BM_ITER_MESH_INDEX (fa, &iter, em->bm, BM_FACES_OF_MESH, i) {
				if (BM_elem_flag_test(fa, flag)) {
					mp[affected[2]++] = i;
				}
				else {
					*tb = i;
					tb++;
				}
			}
		}

		for (j = 3; j--; ) {
			int tot = totelem[j];
			int aff = affected[j];
			tb = tbuf[j];
			mp = map[j];
			if (!(tb && mp))
				continue;
			if (ELEM(aff, 0, tot)) {
				MEM_freeN(tb);
				MEM_freeN(mp);
				map[j] = NULL;
				continue;
			}
			if (reverse) {
				memcpy(tb + (tot - aff), mp, aff * sizeof(int));
			}
			else {
				memcpy(mp + aff, tb, (tot - aff) * sizeof(int));
				tb = mp;
				mp = map[j] = tbuf[j];
				tbuf[j] = tb;
			}

			/* Reverse mapping, we want an org2new one! */
			for (i = tot, tb = tbuf[j] + tot - 1; i--; tb--) {
				mp[*tb] = i;
			}
			MEM_freeN(tbuf[j]);
		}
	}

	else if (action == SRT_RANDOMIZE) {
		if (totelem[0]) {
			/* Re-init random generator for each element type, to get consistent random when
			 * enabling/disabling an element type. */
			RNG *rng = BLI_rng_new_srandom(seed);
			pb = pblock[0] = MEM_callocN(sizeof(char) * totelem[0], "sort_bmelem vert pblock");
			sb = sblock[0] = MEM_callocN(sizeof(BMElemSort) * totelem[0], "sort_bmelem vert sblock");

			BM_ITER_MESH_INDEX (ve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
				if (BM_elem_flag_test(ve, flag)) {
					pb[i] = false;
					sb[affected[0]].org_idx = i;
					sb[affected[0]++].srt = BLI_rng_get_float(rng);
				}
				else {
					pb[i] = true;
				}
			}

			BLI_rng_free(rng);
		}

		if (totelem[1]) {
			RNG *rng = BLI_rng_new_srandom(seed);
			pb = pblock[1] = MEM_callocN(sizeof(char) * totelem[1], "sort_bmelem edge pblock");
			sb = sblock[1] = MEM_callocN(sizeof(BMElemSort) * totelem[1], "sort_bmelem edge sblock");

			BM_ITER_MESH_INDEX (ed, &iter, em->bm, BM_EDGES_OF_MESH, i) {
				if (BM_elem_flag_test(ed, flag)) {
					pb[i] = false;
					sb[affected[1]].org_idx = i;
					sb[affected[1]++].srt = BLI_rng_get_float(rng);
				}
				else {
					pb[i] = true;
				}
			}

			BLI_rng_free(rng);
		}

		if (totelem[2]) {
			RNG *rng = BLI_rng_new_srandom(seed);
			pb = pblock[2] = MEM_callocN(sizeof(char) * totelem[2], "sort_bmelem face pblock");
			sb = sblock[2] = MEM_callocN(sizeof(BMElemSort) * totelem[2], "sort_bmelem face sblock");

			BM_ITER_MESH_INDEX (fa, &iter, em->bm, BM_FACES_OF_MESH, i) {
				if (BM_elem_flag_test(fa, flag)) {
					pb[i] = false;
					sb[affected[2]].org_idx = i;
					sb[affected[2]++].srt = BLI_rng_get_float(rng);
				}
				else {
					pb[i] = true;
				}
			}

			BLI_rng_free(rng);
		}
	}

	else if (action == SRT_REVERSE) {
		if (totelem[0]) {
			pb = pblock[0] = MEM_callocN(sizeof(char) * totelem[0], "sort_bmelem vert pblock");
			sb = sblock[0] = MEM_callocN(sizeof(BMElemSort) * totelem[0], "sort_bmelem vert sblock");

			BM_ITER_MESH_INDEX (ve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
				if (BM_elem_flag_test(ve, flag)) {
					pb[i] = false;
					sb[affected[0]].org_idx = i;
					sb[affected[0]++].srt = (float)-i;
				}
				else {
					pb[i] = true;
				}
			}
		}

		if (totelem[1]) {
			pb = pblock[1] = MEM_callocN(sizeof(char) * totelem[1], "sort_bmelem edge pblock");
			sb = sblock[1] = MEM_callocN(sizeof(BMElemSort) * totelem[1], "sort_bmelem edge sblock");

			BM_ITER_MESH_INDEX (ed, &iter, em->bm, BM_EDGES_OF_MESH, i) {
				if (BM_elem_flag_test(ed, flag)) {
					pb[i] = false;
					sb[affected[1]].org_idx = i;
					sb[affected[1]++].srt = (float)-i;
				}
				else {
					pb[i] = true;
				}
			}
		}

		if (totelem[2]) {
			pb = pblock[2] = MEM_callocN(sizeof(char) * totelem[2], "sort_bmelem face pblock");
			sb = sblock[2] = MEM_callocN(sizeof(BMElemSort) * totelem[2], "sort_bmelem face sblock");

			BM_ITER_MESH_INDEX (fa, &iter, em->bm, BM_FACES_OF_MESH, i) {
				if (BM_elem_flag_test(fa, flag)) {
					pb[i] = false;
					sb[affected[2]].org_idx = i;
					sb[affected[2]++].srt = (float)-i;
				}
				else {
					pb[i] = true;
				}
			}
		}
	}

/*	printf("%d vertices: %d to be affected...\n", totelem[0], affected[0]);*/
/*	printf("%d edges: %d to be affected...\n", totelem[1], affected[1]);*/
/*	printf("%d faces: %d to be affected...\n", totelem[2], affected[2]);*/
	if (affected[0] == 0 && affected[1] == 0 && affected[2] == 0) {
		for (j = 3; j--; ) {
			if (pblock[j])
				MEM_freeN(pblock[j]);
			if (sblock[j])
				MEM_freeN(sblock[j]);
			if (map[j])
				MEM_freeN(map[j]);
		}
		return;
	}

	/* Sort affected elements, and populate mapping arrays, if needed. */
	for (j = 3; j--; ) {
		pb = pblock[j];
		sb = sblock[j];
		if (pb && sb && !map[j]) {
			const char *p_blk;
			BMElemSort *s_blk;
			int tot = totelem[j];
			int aff = affected[j];

			qsort(sb, aff, sizeof(BMElemSort), bmelemsort_comp);

			mp = map[j] = MEM_mallocN(sizeof(int) * tot, "sort_bmelem map");
			p_blk = pb + tot - 1;
			s_blk = sb + aff - 1;
			for (i = tot; i--; p_blk--) {
				if (*p_blk) { /* Protected! */
					mp[i] = i;
				}
				else {
					mp[s_blk->org_idx] = i;
					s_blk--;
				}
			}
		}
		if (pb)
			MEM_freeN(pb);
		if (sb)
			MEM_freeN(sb);
	}

	BM_mesh_remap(em->bm, map[0], map[1], map[2]);
/*	DAG_id_tag_update(ob->data, 0);*/

	for (j = 3; j--; ) {
		if (map[j])
			MEM_freeN(map[j]);
	}
}

static int edbm_sort_elements_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_edit_object(C);

	/* may be NULL */
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);

	const int action = RNA_enum_get(op->ptr, "type");
	PropertyRNA *prop_elem_types = RNA_struct_find_property(op->ptr, "elements");
	const bool use_reverse = RNA_boolean_get(op->ptr, "reverse");
	unsigned int seed = RNA_int_get(op->ptr, "seed");
	int elem_types = 0;

	if (ELEM(action, SRT_VIEW_ZAXIS, SRT_VIEW_XAXIS)) {
		if (rv3d == NULL) {
			BKE_report(op->reports, RPT_ERROR, "View not found, cannot sort by view axis");
			return OPERATOR_CANCELLED;
		}
	}

	/* If no elem_types set, use current selection mode to set it! */
	if (RNA_property_is_set(op->ptr, prop_elem_types)) {
		elem_types = RNA_property_enum_get(op->ptr, prop_elem_types);
	}
	else {
		BMEditMesh *em = BKE_editmesh_from_object(ob);
		if (em->selectmode & SCE_SELECT_VERTEX)
			elem_types |= BM_VERT;
		if (em->selectmode & SCE_SELECT_EDGE)
			elem_types |= BM_EDGE;
		if (em->selectmode & SCE_SELECT_FACE)
			elem_types |= BM_FACE;
		RNA_enum_set(op->ptr, "elements", elem_types);
	}

	sort_bmelem_flag(scene, ob, v3d, rv3d,
	                 elem_types, BM_ELEM_SELECT, action, use_reverse, seed);
	return OPERATOR_FINISHED;
}

static bool edbm_sort_elements_draw_check_prop(PointerRNA *ptr, PropertyRNA *prop)
{
	const char *prop_id = RNA_property_identifier(prop);
	const int action = RNA_enum_get(ptr, "type");

	/* Only show seed for randomize action! */
	if (STREQ(prop_id, "seed")) {
		if (action == SRT_RANDOMIZE)
			return true;
		else
			return false;
	}

	/* Hide seed for reverse and randomize actions! */
	if (STREQ(prop_id, "reverse")) {
		if (ELEM(action, SRT_RANDOMIZE, SRT_REVERSE))
			return false;
		else
			return true;
	}

	return true;
}

static void edbm_sort_elements_ui(bContext *C, wmOperator *op)
{
	uiLayout *layout = op->layout;
	wmWindowManager *wm = CTX_wm_manager(C);
	PointerRNA ptr;

	RNA_pointer_create(&wm->id, op->type->srna, op->properties, &ptr);

	/* Main auto-draw call. */
	uiDefAutoButsRNA(layout, &ptr, edbm_sort_elements_draw_check_prop, '\0');
}

void MESH_OT_sort_elements(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[] = {
		{SRT_VIEW_ZAXIS, "VIEW_ZAXIS", 0, "View Z Axis",
		                 "Sort selected elements from farthest to nearest one in current view"},
		{SRT_VIEW_XAXIS, "VIEW_XAXIS", 0, "View X Axis",
		                 "Sort selected elements from left to right one in current view"},
		{SRT_CURSOR_DISTANCE, "CURSOR_DISTANCE", 0, "Cursor Distance",
		                      "Sort selected elements from nearest to farthest from 3D cursor"},
		{SRT_MATERIAL, "MATERIAL", 0, "Material",
		               "Sort selected elements from smallest to greatest material index (faces only!)"},
		{SRT_SELECTED, "SELECTED", 0, "Selected",
		               "Move all selected elements in first places, preserving their relative order "
		               "(WARNING: this will affect unselected elements' indices as well!)"},
		{SRT_RANDOMIZE, "RANDOMIZE", 0, "Randomize", "Randomize order of selected elements"},
		{SRT_REVERSE, "REVERSE", 0, "Reverse", "Reverse current order of selected elements"},
		{0, NULL, 0, NULL, NULL},
	};

	static EnumPropertyItem elem_items[] = {
		{BM_VERT, "VERT", 0, "Vertices", ""},
		{BM_EDGE, "EDGE", 0, "Edges", ""},
		{BM_FACE, "FACE", 0, "Faces", ""},
		{0, NULL, 0, NULL, NULL},
	};

	/* identifiers */
	ot->name = "Sort Mesh Elements";
	ot->description = "The order of selected vertices/edges/faces is modified, based on a given method";
	ot->idname = "MESH_OT_sort_elements";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = edbm_sort_elements_exec;
	ot->poll = ED_operator_editmesh;
	ot->ui = edbm_sort_elements_ui;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "Type of re-ordering operation to apply");
	RNA_def_enum_flag(ot->srna, "elements", elem_items, 0, "Elements",
	                  "Which elements to affect (vertices, edges and/or faces)");
	RNA_def_boolean(ot->srna, "reverse", false, "Reverse", "Reverse the sorting effect");
	RNA_def_int(ot->srna, "seed", 0, 0, INT_MAX, "Seed", "Seed for random-based operations", 0, 255);
}

/****** end of qsort stuff ****/

static int edbm_noise_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	Material *ma;
	Tex *tex;
	BMVert *eve;
	BMIter iter;
	const float fac = RNA_float_get(op->ptr, "factor");

	if (em == NULL) {
		return OPERATOR_FINISHED;
	}

	if ((ma  = give_current_material(obedit, obedit->actcol)) == NULL ||
	    (tex = give_current_material_texture(ma)) == NULL)
	{
		BKE_report(op->reports, RPT_WARNING, "Mesh has no material or texture assigned");
		return OPERATOR_FINISHED;
	}

	if (tex->type == TEX_STUCCI) {
		float b2, vec[3];
		float ofs = tex->turbul / 200.0f;
		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
				b2 = BLI_hnoise(tex->noisesize, eve->co[0], eve->co[1], eve->co[2]);
				if (tex->stype) ofs *= (b2 * b2);
				vec[0] = fac * (b2 - BLI_hnoise(tex->noisesize, eve->co[0] + ofs, eve->co[1], eve->co[2]));
				vec[1] = fac * (b2 - BLI_hnoise(tex->noisesize, eve->co[0], eve->co[1] + ofs, eve->co[2]));
				vec[2] = fac * (b2 - BLI_hnoise(tex->noisesize, eve->co[0], eve->co[1], eve->co[2] + ofs));
				
				add_v3_v3(eve->co, vec);
			}
		}
	}
	else {
		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
				float tin, dum;
				externtex(ma->mtex[0], eve->co, &tin, &dum, &dum, &dum, &dum, 0, NULL);
				eve->co[2] += fac * tin;
			}
		}
	}

	EDBM_mesh_normals_update(em);

	EDBM_update_generic(em, true, false);

	return OPERATOR_FINISHED;
}

void MESH_OT_noise(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Noise";
	ot->description = "Use vertex coordinate as texture coordinate";
	ot->idname = "MESH_OT_noise";

	/* api callbacks */
	ot->exec = edbm_noise_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_float(ot->srna, "factor", 0.1f, -FLT_MAX, FLT_MAX, "Factor", "", 0.0f, 1.0f);
}


static int edbm_bridge_tag_boundary_edges(BMesh *bm)
{
	/* tags boundary edges from a face selection */
	BMIter iter;
	BMFace *f;
	BMEdge *e;
	int totface_del = 0;

	BM_mesh_elem_hflag_disable_all(bm, BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
			if (BM_edge_is_wire(e) || BM_edge_is_boundary(e)) {
				BM_elem_flag_enable(e, BM_ELEM_TAG);
			}
			else {
				BMIter fiter;
				bool is_all_sel = true;
				/* check if its only used by selected faces */
				BM_ITER_ELEM (f, &fiter, e, BM_FACES_OF_EDGE) {
					if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
						/* tag face for removal*/
						if (!BM_elem_flag_test(f, BM_ELEM_TAG)) {
							BM_elem_flag_enable(f, BM_ELEM_TAG);
							totface_del++;
						}
					}
					else {
						is_all_sel = false;
					}
				}

				if (is_all_sel == false) {
					BM_elem_flag_enable(e, BM_ELEM_TAG);
				}
			}
		}
	}

	return totface_del;
}

static int edbm_bridge_edge_loops_exec(bContext *C, wmOperator *op)
{
	BMOperator bmop;
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const int type = RNA_enum_get(op->ptr, "type");
	const bool use_pairs = (type == 2);
	const bool use_cyclic = (type == 1);
	const bool use_merge = RNA_boolean_get(op->ptr, "use_merge");
	const float merge_factor = RNA_float_get(op->ptr, "merge_factor");
	const int twist_offset = RNA_int_get(op->ptr, "twist_offset");
	const bool use_faces = (em->bm->totfacesel != 0);
	char edge_hflag;

	int totface_del = 0;
	BMFace **totface_del_arr = NULL;

	if (use_faces) {
		BMIter iter;
		BMFace *f;
		int i;

		totface_del = edbm_bridge_tag_boundary_edges(em->bm);
		totface_del_arr = MEM_mallocN(sizeof(*totface_del_arr) * totface_del, __func__);

		i = 0;
		BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
			if (BM_elem_flag_test(f, BM_ELEM_TAG)) {
				totface_del_arr[i++] = f;
			}
		}
		edge_hflag = BM_ELEM_TAG;
	}
	else {
		edge_hflag = BM_ELEM_SELECT;
	}

	EDBM_op_init(em, &bmop, op,
	             "bridge_loops edges=%he use_pairs=%b use_cyclic=%b use_merge=%b merge_factor=%f twist_offset=%i",
	             edge_hflag, use_pairs, use_cyclic, use_merge, merge_factor, twist_offset);

	BMO_op_exec(em->bm, &bmop);

	if (!BMO_error_occurred(em->bm)) {
		/* when merge is used the edges are joined and remain selected */
		if (use_merge == false) {
			EDBM_flag_disable_all(em, BM_ELEM_SELECT);
			BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);
		}

		if (use_faces && totface_del) {
			int i;
			BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, false);
			for (i = 0; i < totface_del; i++) {
				BM_elem_flag_enable(totface_del_arr[i], BM_ELEM_TAG);
			}
			BMO_op_callf(em->bm, BMO_FLAG_DEFAULTS,
			             "delete geom=%hf context=%i",
			             BM_ELEM_TAG, DEL_FACES);
		}

		if (use_merge == false) {
			struct EdgeRingOpSubdProps op_props;
			mesh_operator_edgering_props_get(op, &op_props);

			if (op_props.cuts) {
				BMOperator bmop_subd;
				/* we only need face normals updated */
				EDBM_mesh_normals_update(em);

				BMO_op_initf(
				        em->bm, &bmop_subd, op->flag,
				        "subdivide_edgering edges=%S interp_mode=%i cuts=%i smooth=%f "
				        "profile_shape=%i profile_shape_factor=%f",
				        &bmop, "edges.out", op_props.interp_mode, op_props.cuts, op_props.smooth,
				        op_props.profile_shape, op_props.profile_shape_factor
				        );
				BMO_op_exec(em->bm, &bmop_subd);

				BMO_slot_buffer_hflag_enable(em->bm, bmop_subd.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);

				BMO_op_finish(em->bm, &bmop_subd);

			}
		}
	}

	if (totface_del_arr) {
		MEM_freeN(totface_del_arr);
	}

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		/* grr, need to return finished so the user can select different options */
		//return OPERATOR_CANCELLED;
		return OPERATOR_FINISHED;
	}
	else {
		EDBM_update_generic(em, true, true);
		return OPERATOR_FINISHED;
	}
}

void MESH_OT_bridge_edge_loops(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[] = {
		{0, "SINGLE", 0, "Open Loop", ""},
		{1, "CLOSED", 0, "Closed Loop", ""},
		{2, "PAIRS", 0, "Loop Pairs", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Bridge Edge Loops";
	ot->description = "Make faces between two or more edge loops";
	ot->idname = "MESH_OT_bridge_edge_loops";
	
	/* api callbacks */
	ot->exec = edbm_bridge_edge_loops_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna, "type", type_items, 0,
	                        "Connect Loops", "Method of bridging multiple loops");

	RNA_def_boolean(ot->srna, "use_merge", false, "Merge", "Merge rather than creating faces");
	RNA_def_float(ot->srna, "merge_factor", 0.5f, 0.0f, 1.0f, "Merge Factor", "", 0.0f, 1.0f);
	RNA_def_int(ot->srna, "twist_offset", 0, -1000, 1000, "Twist", "Twist offset for closed loops", -1000, 1000);

	mesh_operator_edgering_props(ot, 0);
}

static int edbm_wireframe_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMOperator bmop;
	const bool use_boundary        = RNA_boolean_get(op->ptr, "use_boundary");
	const bool use_even_offset     = RNA_boolean_get(op->ptr, "use_even_offset");
	const bool use_replace         = RNA_boolean_get(op->ptr, "use_replace");
	const bool use_relative_offset = RNA_boolean_get(op->ptr, "use_relative_offset");
	const bool use_crease          = RNA_boolean_get(op->ptr, "use_crease");
	const float crease_weight      = RNA_float_get(op->ptr,   "crease_weight");
	const float thickness          = RNA_float_get(op->ptr,   "thickness");
	const float offset             = RNA_float_get(op->ptr,   "offset");

	EDBM_op_init(em, &bmop, op,
	             "wireframe faces=%hf use_replace=%b use_boundary=%b use_even_offset=%b use_relative_offset=%b "
	             "use_crease=%b crease_weight=%f thickness=%f offset=%f",
	             BM_ELEM_SELECT, use_replace, use_boundary, use_even_offset, use_relative_offset,
	             use_crease, crease_weight, thickness, offset);

	BMO_op_exec(em->bm, &bmop);

	BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}
	else {
		EDBM_update_generic(em, true, true);
		return OPERATOR_FINISHED;
	}
}

void MESH_OT_wireframe(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Wire Frame";
	ot->idname = "MESH_OT_wireframe";
	ot->description = "Create a solid wire-frame from faces";

	/* api callbacks */
	ot->exec = edbm_wireframe_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "use_boundary",        true,  "Boundary",        "Inset face boundaries");
	RNA_def_boolean(ot->srna, "use_even_offset",     true,  "Offset Even",     "Scale the offset to give more even thickness");
	RNA_def_boolean(ot->srna, "use_relative_offset", false, "Offset Relative", "Scale the offset by surrounding geometry");
	RNA_def_boolean(ot->srna, "use_replace",         true,	"Replace",		   "Remove original faces");
	prop = RNA_def_float(ot->srna, "thickness", 0.01f, 0.0f, FLT_MAX, "Thickness", "", 0.0f, 10.0f);
	/* use 1 rather then 10 for max else dragging the button moves too far */
	RNA_def_property_ui_range(prop, 0.0, 1.0, 0.01, 4);
	RNA_def_float(ot->srna, "offset", 0.01f, 0.0f, FLT_MAX, "Offset", "", 0.0f, 10.0f);
	RNA_def_boolean(ot->srna, "use_crease",          false, "Crease",          "Crease hub edges for improved subsurf");
	prop = RNA_def_float(ot->srna, "crease_weight", 0.01f, 0.0f, FLT_MAX, "Crease weight", "", 0.0f, 1.0f);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 2);
}

#ifdef WITH_BULLET
static int edbm_convex_hull_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMOperator bmop;

	EDBM_op_init(em, &bmop, op, "convex_hull input=%hvef "
	             "use_existing_faces=%b",
	             BM_ELEM_SELECT,
	             RNA_boolean_get(op->ptr, "use_existing_faces"));
	BMO_op_exec(em->bm, &bmop);

	/* Hull fails if input is coplanar */
	if (BMO_error_occurred(em->bm)) {
		EDBM_op_finish(em, &bmop, op, true);
		return OPERATOR_CANCELLED;
	}

	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "geom.out", BM_FACE, BM_ELEM_SELECT, true);

	/* Delete unused vertices, edges, and faces */
	if (RNA_boolean_get(op->ptr, "delete_unused")) {
		if (!EDBM_op_callf(em, op, "delete geom=%S context=%i",
		                   &bmop, "geom_unused.out", DEL_ONLYTAGGED))
		{
			EDBM_op_finish(em, &bmop, op, true);
			return OPERATOR_CANCELLED;
		}
	}

	/* Delete hole edges/faces */
	if (RNA_boolean_get(op->ptr, "make_holes")) {
		if (!EDBM_op_callf(em, op, "delete geom=%S context=%i",
		                   &bmop, "geom_holes.out", DEL_ONLYTAGGED))
		{
			EDBM_op_finish(em, &bmop, op, true);
			return OPERATOR_CANCELLED;
		}
	}

	/* Merge adjacent triangles */
	if (RNA_boolean_get(op->ptr, "join_triangles")) {
		if (!EDBM_op_call_and_selectf(em, op,
		                              "faces.out", true,
		                              "join_triangles faces=%S limit=%f",
		                              &bmop, "geom.out",
		                              RNA_float_get(op->ptr, "limit")))
		{
			EDBM_op_finish(em, &bmop, op, true);
			return OPERATOR_CANCELLED;
		}
	}

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}
	else {
		EDBM_update_generic(em, true, true);
		EDBM_selectmode_flush(em);
		return OPERATOR_FINISHED;
	}
}

void MESH_OT_convex_hull(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Convex Hull";
	ot->description = "Enclose selected vertices in a convex polyhedron";
	ot->idname = "MESH_OT_convex_hull";

	/* api callbacks */
	ot->exec = edbm_convex_hull_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_boolean(ot->srna, "delete_unused", true,
	                "Delete Unused",
	                "Delete selected elements that are not used by the hull");

	RNA_def_boolean(ot->srna, "use_existing_faces", true,
	                "Use Existing Faces",
	                "Skip hull triangles that are covered by a pre-existing face");

	RNA_def_boolean(ot->srna, "make_holes", false,
	                "Make Holes",
	                "Delete selected faces that are used by the hull");

	RNA_def_boolean(ot->srna, "join_triangles", true,
	                "Join Triangles",
	                "Merge adjacent triangles into quads");

	join_triangle_props(ot);
}
#endif

static int mesh_symmetrize_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMOperator bmop;

	const float thresh = RNA_float_get(op->ptr, "threshold");

	EDBM_op_init(em, &bmop, op,
	             "symmetrize input=%hvef direction=%i dist=%f",
	             BM_ELEM_SELECT, RNA_enum_get(op->ptr, "direction"), thresh);
	BMO_op_exec(em->bm, &bmop);

	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "geom.out", BM_ALL_NOLOOP, BM_ELEM_SELECT, true);

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}
	else {
		EDBM_update_generic(em, true, true);
		EDBM_selectmode_flush(em);
		return OPERATOR_FINISHED;
	}
}

void MESH_OT_symmetrize(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Symmetrize";
	ot->description = "Enforce symmetry (both form and topological) across an axis";
	ot->idname = "MESH_OT_symmetrize";

	/* api callbacks */
	ot->exec = mesh_symmetrize_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna, "direction", symmetrize_direction_items,
	                        BMO_SYMMETRIZE_NEGATIVE_X,
	                        "Direction", "Which sides to copy from and to");
	RNA_def_float(ot->srna, "threshold", 0.0001, 0.0, 10.0, "Threshold", "", 0.00001, 0.1);
}

static int mesh_symmetry_snap_exec(bContext *C, wmOperator *op)
{
	const float eps = 0.00001f;
	const float eps_sq = eps * eps;

	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	int *index = MEM_mallocN(bm->totvert * sizeof(*index), __func__);
	const bool use_topology = false;

	const float thresh = RNA_float_get(op->ptr, "threshold");
	const float fac = RNA_float_get(op->ptr, "factor");
	const bool use_center = RNA_boolean_get(op->ptr, "use_center");

	/* stats */
	int totmirr = 0, totfail = 0, totfound = 0;

	/* axix */
	const int axis_dir = RNA_enum_get(op->ptr, "direction");
	int axis = axis_dir % 3;
	bool axis_sign = axis != axis_dir;

	/* vertex iter */
	BMIter iter;
	BMVert *v;
	int i;

	EDBM_verts_mirror_cache_begin_ex(em, axis, true, true, use_topology, thresh, index);

	BM_mesh_elem_table_ensure(bm, BM_VERT);

	BM_mesh_elem_hflag_disable_all(bm, BM_VERT, BM_ELEM_TAG, false);


	BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
		if ((BM_elem_flag_test(v, BM_ELEM_SELECT) != false) &&
		    (BM_elem_flag_test(v, BM_ELEM_TAG) == false))
		{
			int i_mirr = index[i];
			if (i_mirr != -1) {

				BMVert *v_mirr = BM_vert_at_index(bm, index[i]);

				if (v != v_mirr) {
					float co[3], co_mirr[3];

					if ((v->co[axis] > v_mirr->co[axis]) == axis_sign) {
						SWAP(BMVert *, v, v_mirr);
					}

					copy_v3_v3(co_mirr, v_mirr->co);
					co_mirr[axis] *= -1.0f;

					if (len_squared_v3v3(v->co, co_mirr) > eps_sq) {
						totmirr++;
					}

					interp_v3_v3v3(co, v->co, co_mirr, fac);

					copy_v3_v3(v->co, co);

					co[axis] *= -1.0f;
					copy_v3_v3(v_mirr->co, co);

					BM_elem_flag_enable(v, BM_ELEM_TAG);
					BM_elem_flag_enable(v_mirr, BM_ELEM_TAG);
					totfound++;
				}
				else {
					if (use_center) {

						if (fabsf(v->co[axis]) > eps) {
							totmirr++;
						}

						v->co[axis] = 0.0f;
					}
					BM_elem_flag_enable(v, BM_ELEM_TAG);
					totfound++;
				}
			}
			else {
				totfail++;
			}
		}
	}


	if (totfail) {
		BKE_reportf(op->reports, RPT_WARNING, "%d already symmetrical, %d pairs mirrored, %d failed",
		            totfound - totmirr, totmirr, totfail);
	}
	else {
		BKE_reportf(op->reports, RPT_INFO, "%d already symmetrical, %d pairs mirrored",
		            totfound - totmirr, totmirr);
	}

	/* no need to end cache, just free the array */
	MEM_freeN(index);

	return OPERATOR_FINISHED;
}

void MESH_OT_symmetry_snap(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Snap to Symmetry";
	ot->description = "Snap vertex pairs to their mirrored locations";
	ot->idname = "MESH_OT_symmetry_snap";

	/* api callbacks */
	ot->exec = mesh_symmetry_snap_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna, "direction", symmetrize_direction_items,
	                        BMO_SYMMETRIZE_NEGATIVE_X,
	                        "Direction", "Which sides to copy from and to");
	RNA_def_float(ot->srna, "threshold", 0.05, 0.0, 10.0, "Threshold", "", 0.0001, 1.0);
	RNA_def_float(ot->srna, "factor", 0.5f, 0.0, 1.0, "Factor", "", 0.0, 1.0);
	RNA_def_boolean(ot->srna, "use_center", true, "Center", "Snap mid verts to the axis center");
}

#ifdef WITH_FREESTYLE

static int edbm_mark_freestyle_edge_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Mesh *me = (Mesh *)obedit->data;
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMEdge *eed;
	BMIter iter;
	FreestyleEdge *fed;
	const bool clear = RNA_boolean_get(op->ptr, "clear");

	if (em == NULL)
		return OPERATOR_FINISHED;

	/* auto-enable Freestyle edge mark drawing */
	if (clear == 0) {
		me->drawflag |= ME_DRAW_FREESTYLE_EDGE;
	}

	if (!CustomData_has_layer(&em->bm->edata, CD_FREESTYLE_EDGE)) {
		BM_data_layer_add(em->bm, &em->bm->edata, CD_FREESTYLE_EDGE);
	}

	if (clear) {
		BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(eed, BM_ELEM_SELECT) && !BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
				fed = CustomData_bmesh_get(&em->bm->edata, eed->head.data, CD_FREESTYLE_EDGE);
				fed->flag &= ~FREESTYLE_EDGE_MARK;
			}
		}
	}
	else {
		BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(eed, BM_ELEM_SELECT) && !BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
				fed = CustomData_bmesh_get(&em->bm->edata, eed->head.data, CD_FREESTYLE_EDGE);
				fed->flag |= FREESTYLE_EDGE_MARK;
			}
		}
	}

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_mark_freestyle_edge(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Mark Freestyle Edge";
	ot->description = "(Un)mark selected edges as Freestyle feature edges";
	ot->idname = "MESH_OT_mark_freestyle_edge";

	/* api callbacks */
	ot->exec = edbm_mark_freestyle_edge_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_boolean(ot->srna, "clear", 0, "Clear", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int edbm_mark_freestyle_face_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Mesh *me = (Mesh *)obedit->data;
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMFace *efa;
	BMIter iter;
	FreestyleFace *ffa;
	const bool clear = RNA_boolean_get(op->ptr, "clear");

	if (em == NULL) return OPERATOR_FINISHED;

	/* auto-enable Freestyle face mark drawing */
	if (!clear) {
		me->drawflag |= ME_DRAW_FREESTYLE_FACE;
	}

	if (!CustomData_has_layer(&em->bm->pdata, CD_FREESTYLE_FACE)) {
		BM_data_layer_add(em->bm, &em->bm->pdata, CD_FREESTYLE_FACE);
	}

	if (clear) {
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			if (BM_elem_flag_test(efa, BM_ELEM_SELECT) && !BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
				ffa = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_FREESTYLE_FACE);
				ffa->flag &= ~FREESTYLE_FACE_MARK;
			}
		}
	}
	else {
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			if (BM_elem_flag_test(efa, BM_ELEM_SELECT) && !BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
				ffa = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_FREESTYLE_FACE);
				ffa->flag |= FREESTYLE_FACE_MARK;
			}
		}
	}

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_mark_freestyle_face(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Mark Freestyle Face";
	ot->description = "(Un)mark selected faces for exclusion from Freestyle feature edge detection";
	ot->idname = "MESH_OT_mark_freestyle_face";

	/* api callbacks */
	ot->exec = edbm_mark_freestyle_face_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_boolean(ot->srna, "clear", 0, "Clear", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

#endif
