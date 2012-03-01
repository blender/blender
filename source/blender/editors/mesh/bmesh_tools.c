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

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "BKE_material.h"
#include "BKE_context.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_texture.h"
#include "BKE_main.h"
#include "BKE_tessmesh.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_view3d.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_object.h"

#include "RE_render_ext.h"

#include "mesh_intern.h"


static void add_normal_aligned(float nor[3], const float add[3])
{
	if (dot_v3v3(nor, add) < -0.9999f)
		sub_v3_v3(nor, add);
	else
		sub_v3_v3(nor, add);
}


static int subdivide_exec(bContext *C, wmOperator *op)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	int cuts = RNA_int_get(op->ptr,"number_cuts");
	float smooth = 0.292f * RNA_float_get(op->ptr, "smoothness");
	float fractal = RNA_float_get(op->ptr, "fractal")/2.5;
	int flag = 0;

	if (smooth != 0.0f)
		flag |= B_SMOOTH;
	if (fractal != 0.0f)
		flag |= B_FRACTAL;
	
	if (RNA_boolean_get(op->ptr, "quadtri") && 
	    RNA_enum_get(op->ptr, "quadcorner") == SUBD_STRAIGHT_CUT)
	{
		RNA_enum_set(op->ptr, "quadcorner", SUBD_INNERVERT);
	}
	
	BM_mesh_esubdivideflag(obedit, em->bm, BM_ELEM_SELECT,
	                       smooth, fractal,
	                       ts->editbutflag|flag,
	                       cuts, 0, RNA_enum_get(op->ptr, "quadcorner"),
	                       RNA_boolean_get(op->ptr, "quadtri"),
	                       TRUE, RNA_int_get(op->ptr, "seed"));

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

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
	/* identifiers */
	ot->name = "Subdivide";
	ot->description = "Subdivide selected edges";
	ot->idname = "MESH_OT_subdivide";

	/* api callbacks */
	ot->exec = subdivide_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_int(ot->srna, "number_cuts", 1, 1, INT_MAX, "Number of Cuts", "", 1, 10);
	/* BMESH_TODO, this currently does nothing, just add to stop UI from erroring out! */
	RNA_def_float(ot->srna, "smoothness", 0.0f, 0.0f, FLT_MAX, "Smoothness", "Smoothness factor (BMESH TODO)", 0.0f, 1.0f);

	RNA_def_boolean(ot->srna, "quadtri", 0, "Quad/Tri Mode", "Tries to prevent ngons");
	RNA_def_enum(ot->srna, "quadcorner", prop_mesh_cornervert_types, SUBD_STRAIGHT_CUT,
	             "Quad Corner Type", "How to subdivide quad corners (anything other then Straight Cut will prevent ngons)");

	RNA_def_float(ot->srna, "fractal", 0.0f, 0.0f, FLT_MAX, "Fractal", "Fractal randomness factor", 0.0f, 1000.0f);
	RNA_def_int(ot->srna, "seed", 0, 0, 10000, "Random Seed", "Seed for the random number generator", 0, 50);
}


void EMBM_project_snap_verts(bContext *C, ARegion *ar, Object *obedit, BMEditMesh *em)
{
	BMIter iter;
	BMVert *eve;

	BM_ITER(eve, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
			float mval[2], vec[3], no_dummy[3];
			int dist_dummy;
			mul_v3_m4v3(vec, obedit->obmat, eve->co);
			project_float_noclip(ar, vec, mval);
			if (snapObjectsContext(C, mval, &dist_dummy, vec, no_dummy, SNAP_NOT_OBEDIT)) {
				mul_v3_m4v3(eve->co, obedit->imat, vec);
			}
		}
	}
}


/* individual face extrude */
/* will use vertex normals for extrusion directions, so *nor is unaffected */
static short EDBM_Extrude_face_indiv(BMEditMesh *em, wmOperator *op, const char hflag, float *UNUSED(nor))
{
	BMOIter siter;
	BMIter liter;
	BMFace *f;
	BMLoop *l;
	BMOperator bmop;

	EDBM_InitOpf(em, &bmop, op, "extrude_face_indiv faces=%hf", hflag);

	/* deselect original verts */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	BMO_op_exec(em->bm, &bmop);
	
	BMO_ITER(f, &siter, em->bm, &bmop, "faceout", BM_FACE) {
		BM_elem_select_set(em->bm, f, TRUE);

		/* set face vertex normals to face normal */
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, f) {
			copy_v3_v3(l->v->no, f->no);
		}
	}

	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return 0;
	}

	return 's'; // s is shrink/fatten
}

/* extrudes individual edges */
static short EDBM_Extrude_edges_indiv(BMEditMesh *em, wmOperator *op, const char hflag, float *UNUSED(nor))
{
	BMOperator bmop;

	EDBM_InitOpf(em, &bmop, op, "extrude_edge_only edges=%he", hflag);

	/* deselect original verts */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	BMO_op_exec(em->bm, &bmop);
	BMO_slot_buffer_hflag_enable(em->bm, &bmop, "geomout", BM_ELEM_SELECT, BM_VERT|BM_EDGE, TRUE);

	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return 0;
	}

	return 'n'; // n is normal grab
}

/* extrudes individual vertices */
static short EDBM_Extrude_verts_indiv(BMEditMesh *em, wmOperator *op, const char hflag, float *UNUSED(nor))
{
	BMOperator bmop;

	EDBM_InitOpf(em, &bmop, op, "extrude_vert_indiv verts=%hv", hflag);

	/* deselect original verts */
	BMO_slot_buffer_hflag_disable(em->bm, &bmop, "verts", BM_ELEM_SELECT, BM_VERT, TRUE);

	BMO_op_exec(em->bm, &bmop);
	BMO_slot_buffer_hflag_enable(em->bm, &bmop, "vertout", BM_ELEM_SELECT, BM_VERT, TRUE);

	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return 0;
	}

	return 'g'; // g is grab
}

static short EDBM_Extrude_edge(Object *obedit, BMEditMesh *em, const char hflag, float nor[3])
{
	BMesh *bm = em->bm;
	BMIter iter;
	BMOIter siter;
	BMOperator extop;
	BMEdge *edge;
	BMFace *f;
	ModifierData *md;
	BMElem *ele;
	
	BMO_op_init(bm, &extop, "extrude_face_region");
	BMO_slot_buffer_from_hflag(bm, &extop, "edgefacein", hflag, BM_VERT|BM_EDGE|BM_FACE);

	/* If a mirror modifier with clipping is on, we need to adjust some 
	 * of the cases above to handle edges on the line of symmetry.
	 */
	md = obedit->modifiers.first;
	for (; md; md = md->next) {
		if ((md->type == eModifierType_Mirror) && (md->mode & eModifierMode_Realtime)) {
			MirrorModifierData *mmd = (MirrorModifierData *) md;
		
			if (mmd->flag & MOD_MIR_CLIPPING) {
				float mtx[4][4];
				if (mmd->mirror_ob) {
					float imtx[4][4];
					invert_m4_m4(imtx, mmd->mirror_ob->obmat);
					mult_m4_m4m4(mtx, imtx, obedit->obmat);
				}

				for (edge = BM_iter_new(&iter, bm, BM_EDGES_OF_MESH, NULL);
				     edge;
				     edge = BM_iter_step(&iter))
				{
					if (BM_elem_flag_test(edge, hflag) &&
					    BM_edge_is_boundary(edge) &&
					    BM_elem_flag_test(edge->l->f, hflag))
					{
						float co1[3], co2[3];

						copy_v3_v3(co1, edge->v1->co);
						copy_v3_v3(co2, edge->v2->co);

						if (mmd->mirror_ob) {
							mul_v3_m4v3(co1, mtx, co1);
							mul_v3_m4v3(co2, mtx, co2);
						}

						if (mmd->flag & MOD_MIR_AXIS_X) {
							if ((fabsf(co1[0]) < mmd->tolerance) &&
								(fabsf(co2[0]) < mmd->tolerance))
							{
								BMO_slot_map_ptr_insert(bm, &extop, "exclude", edge, NULL);
							}
						}
						if (mmd->flag & MOD_MIR_AXIS_Y) {
							if ((fabsf(co1[1]) < mmd->tolerance) &&
								(fabsf(co2[1]) < mmd->tolerance))
							{
								BMO_slot_map_ptr_insert(bm, &extop, "exclude", edge, NULL);
							}
						}
						if (mmd->flag & MOD_MIR_AXIS_Z) {
							if ((fabsf(co1[2]) < mmd->tolerance) &&
								(fabsf(co2[2]) < mmd->tolerance))
							{
								BMO_slot_map_ptr_insert(bm, &extop, "exclude", edge, NULL);
							}
						}
					}
				}
			}
		}
	}

	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	BMO_op_exec(bm, &extop);

	nor[0] = nor[1] = nor[2] = 0.0f;
	
	BMO_ITER(ele, &siter, bm, &extop, "geomout", BM_ALL) {
		BM_elem_select_set(bm, ele, TRUE);

		if (ele->head.htype == BM_FACE) {
			f = (BMFace *)ele;
			add_normal_aligned(nor, f->no);
		};
	}

	normalize_v3(nor);

	BMO_op_finish(bm, &extop);

	if (nor[0] == 0.0f && nor[1] == 0.0f && nor[2] == 0.0f) return 'g'; // grab
	return 'n'; // normal constraint 

}
static short EDBM_Extrude_vert(Object *obedit, BMEditMesh *em, const char hflag, float *nor)
{
	BMIter iter;
	BMEdge *eed;
		
	/* ensure vert flags are consistent for edge selections */
	eed = BM_iter_new(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
	for ( ; eed; eed = BM_iter_step(&iter)) {
		if (BM_elem_flag_test(eed, hflag)) {
			if (hflag & BM_ELEM_SELECT) {
				BM_elem_select_set(em->bm, eed->v1, TRUE);
				BM_elem_select_set(em->bm, eed->v2, TRUE);
			}

			BM_elem_flag_enable(eed->v1, hflag & ~BM_ELEM_SELECT);
			BM_elem_flag_enable(eed->v2, hflag & ~BM_ELEM_SELECT);
		}
		else {
			if (BM_elem_flag_test(eed->v1, hflag) && BM_elem_flag_test(eed->v2, hflag)) {
				if (hflag & BM_ELEM_SELECT) {
					BM_elem_select_set(em->bm, eed, TRUE);
				}

				BM_elem_flag_enable(eed, hflag & ~BM_ELEM_SELECT);
			}
		}
	}

	return EDBM_Extrude_edge(obedit, em, hflag, nor);
}

static int extrude_repeat_mesh(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
		
	int steps = RNA_int_get(op->ptr,"steps");
	
	float offs = RNA_float_get(op->ptr,"offset");
	float dvec[3], tmat[3][3], bmat[3][3], nor[3] = {0.0, 0.0, 0.0};
	short a;

	/* dvec */
	normalize_v3_v3(dvec, rv3d->persinv[2]);
	mul_v3_fl(dvec, offs);

	/* base correction */
	copy_m3_m4(bmat, obedit->obmat);
	invert_m3_m3(tmat, bmat);
	mul_m3_v3(tmat, dvec);

	for (a = 0; a < steps; a++) {
		EDBM_Extrude_edge(obedit, em, BM_ELEM_SELECT, nor);
		//BMO_op_callf(em->bm, "extrude_face_region edgefacein=%hef", BM_ELEM_SELECT);
		BMO_op_callf(em->bm, "translate vec=%v verts=%hv", (float *)dvec, BM_ELEM_SELECT);
		//extrudeflag(obedit, em, SELECT, nor);
		//translateflag(em, SELECT, dvec);
	}
	
	EDBM_RecalcNormals(em);

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_extrude_repeat(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Extrude Repeat Mesh";
	ot->description = "Extrude selected vertices, edges or faces repeatedly";
	ot->idname = "MESH_OT_extrude_repeat";
	
	/* api callbacks */
	ot->exec = extrude_repeat_mesh;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_float(ot->srna, "offset", 2.0f, 0.0f, 100.0f, "Offset", "", 0.0f, FLT_MAX);
	RNA_def_int(ot->srna, "steps", 10, 0, 180, "Steps", "", 0, INT_MAX);
}

/* generic extern called extruder */
static int EDBM_Extrude_Mesh(Scene *scene, Object *obedit, BMEditMesh *em, wmOperator *op, float *norin)
{
	short nr, transmode = 0;
	float stacknor[3] = {0.0f, 0.0f, 0.0f};
	float *nor = norin ? norin : stacknor;

	nor[0] = nor[1] = nor[2] = 0.0f;

	if (em->selectmode & SCE_SELECT_VERTEX) {
		if (em->bm->totvertsel == 0) nr = 0;
		else if (em->bm->totvertsel == 1) nr = 4;
		else if (em->bm->totedgesel == 0) nr = 4;
		else if (em->bm->totfacesel == 0)
			nr = 3; // pupmenu("Extrude %t|Only Edges%x3|Only Vertices%x4");
		else if (em->bm->totfacesel == 1)
			nr = 1; // pupmenu("Extrude %t|Region %x1|Only Edges%x3|Only Vertices%x4");
		else 
			nr = 1; // pupmenu("Extrude %t|Region %x1||Individual Faces %x2|Only Edges%x3|Only Vertices%x4");
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		if (em->bm->totedgesel == 0) nr = 0;
		
		nr = 1;
		/* else if (em->totedgesel == 1) nr = 3;
		else if (em->totfacesel == 0) nr = 3;
		else if (em->totfacesel == 1)
			nr = 1; // pupmenu("Extrude %t|Region %x1|Only Edges%x3");
		else
			nr = 1; // pupmenu("Extrude %t|Region %x1||Individual Faces %x2|Only Edges%x3");
		*/
	}
	else {
		if (em->bm->totfacesel == 0) nr = 0;
		else if (em->bm->totfacesel == 1) nr = 1;
		else
			nr = 1; // pupmenu("Extrude %t|Region %x1||Individual Faces %x2");
	}

	if (nr < 1) return 'g';

	if (nr == 1 && em->selectmode & SCE_SELECT_VERTEX)
		transmode = EDBM_Extrude_vert(obedit, em, BM_ELEM_SELECT, nor);
	else if (nr == 1) transmode = EDBM_Extrude_edge(obedit, em, BM_ELEM_SELECT, nor);
	else if (nr == 4) transmode = EDBM_Extrude_verts_indiv(em, op, BM_ELEM_SELECT, nor);
	else if (nr == 3) transmode = EDBM_Extrude_edges_indiv(em, op, BM_ELEM_SELECT, nor);
	else transmode = EDBM_Extrude_face_indiv(em, op, BM_ELEM_SELECT, nor);
	
	if (transmode == 0) {
		BKE_report(op->reports, RPT_ERROR, "Not a valid selection for extrude");
	}
	else {
		
			/* We need to force immediate calculation here because 
			* transform may use derived objects (which are now stale).
			*
			* This shouldn't be necessary, derived queries should be
			* automatically building this data if invalid. Or something.
			*/
//		DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
		object_handle_update(scene, obedit);

		/* individual faces? */
//		BIF_TransformSetUndo("Extrude");
		if (nr == 2) {
//			initTransform(TFM_SHRINKFATTEN, CTX_NO_PET|CTX_NO_MIRROR);
//			Transform();
		}
		else {
//			initTransform(TFM_TRANSLATION, CTX_NO_PET|CTX_NO_MIRROR);
			if (transmode == 'n') {
				mul_m4_v3(obedit->obmat, nor);
				sub_v3_v3v3(nor, nor, obedit->obmat[3]);
//				BIF_setSingleAxisConstraint(nor, "along normal");
			}
//			Transform();
		}
	}
	
	return transmode;
}

/* extrude without transform */
static int mesh_extrude_region_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	
	EDBM_Extrude_Mesh(scene, obedit, em, op, NULL);

	/* This normally happens when pushing undo but modal operators
	 * like this one don't push undo data until after modal mode is
	 * done.*/
	EDBM_RecalcNormals(em);
	BMEdit_RecalcTesselation(em);

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_extrude_region(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Extrude Region";
	ot->idname = "MESH_OT_extrude_region";
	
	/* api callbacks */
	//ot->invoke = mesh_extrude_region_invoke;
	ot->exec = mesh_extrude_region_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
}

static int mesh_extrude_verts_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	float nor[3];

	EDBM_Extrude_verts_indiv(em, op, BM_ELEM_SELECT, nor);
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_extrude_verts_indiv(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Extrude Only Vertices";
	ot->idname = "MESH_OT_extrude_verts_indiv";
	
	/* api callbacks */
	ot->exec = mesh_extrude_verts_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* to give to transform */
	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
}

static int mesh_extrude_edges_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	float nor[3];

	EDBM_Extrude_edges_indiv(em, op, BM_ELEM_SELECT, nor);
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_extrude_edges_indiv(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Extrude Only Edges";
	ot->idname = "MESH_OT_extrude_edges_indiv";
	
	/* api callbacks */
	ot->exec = mesh_extrude_edges_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* to give to transform */
	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
}

static int mesh_extrude_faces_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	float nor[3];

	EDBM_Extrude_face_indiv(em, op, BM_ELEM_SELECT, nor);
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_extrude_faces_indiv(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Extrude Individual Faces";
	ot->idname = "MESH_OT_extrude_faces_indiv";
	
	/* api callbacks */
	ot->exec = mesh_extrude_faces_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
}

/* ******************** (de)select all operator **************** */

void EDBM_toggle_select_all(BMEditMesh *em) /* exported for UV */
{
	if (em->bm->totvertsel || em->bm->totedgesel || em->bm->totfacesel)
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	else 
		EDBM_flag_enable_all(em, BM_ELEM_SELECT);
}

static int mesh_select_all_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	int action = RNA_enum_get(op->ptr, "action");
	
	switch (action) {
	case SEL_TOGGLE:
		EDBM_toggle_select_all(em);
		break;
	case SEL_SELECT:
		EDBM_flag_enable_all(em, BM_ELEM_SELECT);
		break;
	case SEL_DESELECT:
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
		break;
	case SEL_INVERT:
		EDBM_select_swap(em);
		break;
	}
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select/Deselect All";
	ot->idname = "MESH_OT_select_all";
	ot->description = "(De)select all vertices, edges or faces";
	
	/* api callbacks */
	ot->exec = mesh_select_all_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}

static int mesh_faces_select_interior_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;

	if (EDBM_select_interior_faces(em)) {
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}

}

void MESH_OT_select_interior_faces(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Interior Faces";
	ot->idname = "MESH_OT_select_interior_faces";
	ot->description = "Select faces where all edges have more than 2 face users";

	/* api callbacks */
	ot->exec = mesh_faces_select_interior_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* *************** add-click-mesh (extrude) operator ************** */
/* in trunk see: 'editmesh_add.c' */
static int dupli_extrude_cursor(bContext *C, wmOperator *op, wmEvent *event)
{
	ViewContext vc;
	BMVert *v1;
	BMIter iter;
	float min[3], max[3];
	int done = 0;
	short use_proj;
	
	em_setup_viewcontext(C, &vc);
	
	use_proj = (vc.scene->toolsettings->snap_flag & SCE_SNAP) &&	(vc.scene->toolsettings->snap_mode == SCE_SNAP_MODE_FACE);

	INIT_MINMAX(min, max);
	
	BM_ITER(v1, &iter, vc.em->bm, BM_VERTS_OF_MESH, NULL) {
		if (BM_elem_flag_test(v1, BM_ELEM_SELECT)) {
			DO_MINMAX(v1->co, min, max);
			done = 1;
		}
	}

	/* call extrude? */
	if (done) {
		const short rot_src = RNA_boolean_get(op->ptr, "rotate_source");
		BMEdge *eed;
		float vec[3], cent[3], mat[3][3];
		float nor[3] = {0.0, 0.0, 0.0};

		/* 2D normal calc */
		float mval_f[2];

		mval_f[0] = (float)event->mval[0];
		mval_f[1] = (float)event->mval[1];

		/* check for edges that are half selected, use for rotation */
		done = 0;
		BM_ITER(eed, &iter, vc.em->bm, BM_EDGES_OF_MESH, NULL) {
			if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
				float co1[3], co2[3];
				mul_v3_m4v3(co1, vc.obedit->obmat, eed->v1->co);
				mul_v3_m4v3(co2, vc.obedit->obmat, eed->v2->co);
				project_float_noclip(vc.ar, co1, co1);
				project_float_noclip(vc.ar, co2, co2);

				/* 2D rotate by 90d while adding.
				 *  (x, y) = (y, -x)
				 *
				 * accumulate the screenspace normal in 2D,
				 * with screenspace edge length weighting the result. */
				if (line_point_side_v2(co1, co2, mval_f) >= 0.0f) {
					nor[0] +=  (co1[1] - co2[1]);
					nor[1] += -(co1[0] - co2[0]);
				}
				else {
					nor[0] +=  (co2[1] - co1[1]);
					nor[1] += -(co2[0] - co1[0]);
				}
			}
			done = 1;
		}

		if (done) {
			float view_vec[3], cross[3];

			/* convert the 2D nomal into 3D */
			mul_mat3_m4_v3(vc.rv3d->viewinv, nor); /* worldspace */
			mul_mat3_m4_v3(vc.obedit->imat, nor); /* local space */

			/* correct the normal to be aligned on the view plane */
			copy_v3_v3(view_vec, vc.rv3d->viewinv[2]);
			mul_mat3_m4_v3(vc.obedit->imat, view_vec);
			cross_v3_v3v3(cross, nor, view_vec);
			cross_v3_v3v3(nor, view_vec, cross);
			normalize_v3(nor);
		}
		
		/* center */
		mid_v3_v3v3(cent, min, max);
		copy_v3_v3(min, cent);

		mul_m4_v3(vc.obedit->obmat, min);	// view space
		view3d_get_view_aligned_coordinate(&vc, min, event->mval, TRUE);
		mul_m4_v3(vc.obedit->imat, min); // back in object space

		sub_v3_v3(min, cent);
		
		/* calculate rotation */
		unit_m3(mat);
		if (done) {
			float dot;

			copy_v3_v3(vec, min);
			normalize_v3(vec);
			dot = dot_v3v3(vec, nor);

			if (fabsf(dot) < 0.999f) {
				float cross[3], si, q1[4];

				cross_v3_v3v3(cross, nor, vec);
				normalize_v3(cross);
				dot = 0.5f * saacos(dot);

				/* halve the rotation if its applied twice */
				if (rot_src) dot *= 0.5f;

				si = sinf(dot);
				q1[0] = cosf(dot);
				q1[1] = cross[0] * si;
				q1[2] = cross[1] * si;
				q1[3] = cross[2] * si;
				normalize_qt(q1);
				quat_to_mat3(mat, q1);
			}
		}
		
		if (rot_src) {
			EDBM_CallOpf(vc.em, op, "rotate verts=%hv cent=%v mat=%m3",
				BM_ELEM_SELECT, cent, mat);

			/* also project the source, for retopo workflow */
			if (use_proj)
				EMBM_project_snap_verts(C, vc.ar, vc.obedit, vc.em);
		}

		EDBM_Extrude_edge(vc.obedit, vc.em, BM_ELEM_SELECT, nor);
		EDBM_CallOpf(vc.em, op, "rotate verts=%hv cent=%v mat=%m3",
			BM_ELEM_SELECT, cent, mat);
		EDBM_CallOpf(vc.em, op, "translate verts=%hv vec=%v",
			BM_ELEM_SELECT, min);
	}
	else {
		float *curs = give_cursor(vc.scene, vc.v3d);
		BMOperator bmop;
		BMOIter oiter;
		
		copy_v3_v3(min, curs);
		view3d_get_view_aligned_coordinate(&vc, min, event->mval, 0);

		invert_m4_m4(vc.obedit->imat, vc.obedit->obmat);
		mul_m4_v3(vc.obedit->imat, min); // back in object space
		
		EDBM_InitOpf(vc.em, &bmop, op, "makevert co=%v", min);
		BMO_op_exec(vc.em->bm, &bmop);

		BMO_ITER(v1, &oiter, vc.em->bm, &bmop, "newvertout", BM_VERT) {
			BM_elem_select_set(vc.em->bm, v1, TRUE);
		}

		if (!EDBM_FinishOp(vc.em, &bmop, op, TRUE)) {
			return OPERATOR_CANCELLED;
		}
	}

	if (use_proj)
		EMBM_project_snap_verts(C, vc.ar, vc.obedit, vc.em);

	/* This normally happens when pushing undo but modal operators
	 * like this one don't push undo data until after modal mode is
	 * done. */
	EDBM_RecalcNormals(vc.em);
	BMEdit_RecalcTesselation(vc.em);

	WM_event_add_notifier(C, NC_GEOM|ND_DATA, vc.obedit->data);
	DAG_id_tag_update(vc.obedit->data, OB_RECALC_DATA);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_dupli_extrude_cursor(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate or Extrude at 3D Cursor";
	ot->idname = "MESH_OT_dupli_extrude_cursor";
	
	/* api callbacks */
	ot->invoke = dupli_extrude_cursor;
	ot->description = "Duplicate and extrude selected vertices, edges or faces towards the mouse cursor";
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "rotate_source", 1, "Rotate Source", "Rotate initial selection giving better shape");
}

static int delete_mesh(bContext *C, Object *obedit, wmOperator *op, int event, Scene *UNUSED(scene))
{
	BMEditMesh *bem = ((Mesh *)obedit->data)->edit_btmesh;
	
	if (event < 1) return OPERATOR_CANCELLED;

	if (event == 10) {
		//"Erase Vertices";

		if (!EDBM_CallOpf(bem, op, "del geom=%hv context=%i", BM_ELEM_SELECT, DEL_VERTS))
			return OPERATOR_CANCELLED;
	} 
	else if (event == 11) {
		//"Edge Loop"
		if (!EDBM_CallOpf(bem, op, "dissolve_edge_loop edges=%he", BM_ELEM_SELECT))
			return OPERATOR_CANCELLED;
	}
	else if (event == 7) {
		int use_verts = RNA_boolean_get(op->ptr, "use_verts");
		//"Dissolve"
		if (bem->selectmode & SCE_SELECT_FACE) {
			if (!EDBM_CallOpf(bem, op, "dissolve_faces faces=%hf use_verts=%b", BM_ELEM_SELECT, use_verts))
				return OPERATOR_CANCELLED;
		}
		else if (bem->selectmode & SCE_SELECT_EDGE) {
			if (!EDBM_CallOpf(bem, op, "dissolve_edges edges=%he use_verts=%b", BM_ELEM_SELECT, use_verts))
				return OPERATOR_CANCELLED;
		}
		else if (bem->selectmode & SCE_SELECT_VERTEX) {
			if (!EDBM_CallOpf(bem, op, "dissolve_verts verts=%hv", BM_ELEM_SELECT))
				return OPERATOR_CANCELLED;
		}
	}
	else if (event == 4) {
		//Edges and Faces
		if (!EDBM_CallOpf(bem, op, "del geom=%hef context=%i", BM_ELEM_SELECT, DEL_EDGESFACES))
			return OPERATOR_CANCELLED;
	} 
	else if (event == 1) {
		//"Erase Edges"
		if (!EDBM_CallOpf(bem, op, "del geom=%he context=%i", BM_ELEM_SELECT, DEL_EDGES))
			return OPERATOR_CANCELLED;
	}
	else if (event == 2) {
		//"Erase Faces";
		if (!EDBM_CallOpf(bem, op, "del geom=%hf context=%i", BM_ELEM_SELECT, DEL_FACES))
			return OPERATOR_CANCELLED;
	}
	else if (event == 5) {
		//"Erase Only Faces";
		if (!EDBM_CallOpf(bem, op, "del geom=%hf context=%i",
		                  BM_ELEM_SELECT, DEL_ONLYFACES))
			return OPERATOR_CANCELLED;
	}
	
	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

/* Note, these values must match delete_mesh() event values */
static EnumPropertyItem prop_mesh_delete_types[] = {
	{7, "DISSOLVE",         0, "Dissolve", ""},
	{12, "COLLAPSE", 0, "Collapse", ""},
	{10,"VERT",		0, "Vertices", ""},
	{1, "EDGE",		0, "Edges", ""},
	{2, "FACE",		0, "Faces", ""},
	{11, "EDGE_LOOP", 0, "Edge Loop", ""},
	{4, "EDGE_FACE", 0, "Edges & Faces", ""},
	{5, "ONLY_FACE", 0, "Only Faces", ""},
	{0, NULL, 0, NULL, NULL}
};

static int delete_mesh_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	Scene *scene = CTX_data_scene(C);
	int type = RNA_enum_get(op->ptr, "type");
	
	if (type != 12) {
		if (delete_mesh(C, obedit, op, type, scene) == OPERATOR_CANCELLED)
			return OPERATOR_CANCELLED;
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	}
	else {
		if (!EDBM_CallOpf(em, op, "collapse edges=%he", BM_ELEM_SELECT))
			return OPERATOR_CANCELLED;
		DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	}

	WM_event_add_notifier(C, NC_GEOM|ND_DATA|ND_SELECT, obedit);
	
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
	ot->exec = delete_mesh_exec;
	
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* props */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_mesh_delete_types, 10, "Type", "Method used for deleting mesh data");

	/* TODO, move dissolve into its own operator so this doesnt confuse non-dissolve options */
	RNA_def_boolean(ot->srna, "use_verts", 0, "Dissolve Verts",
	                "When dissolving faces/edges, also dissolve remaining vertices");
}


static int addedgeface_mesh_exec(bContext *C, wmOperator *op)
{
	BMOperator bmop;
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	
	if (!EDBM_InitOpf(em, &bmop, op, "contextual_create geom=%hfev", BM_ELEM_SELECT))
		return OPERATOR_CANCELLED;
	
	BMO_op_exec(em->bm, &bmop);
	BMO_slot_buffer_hflag_enable(em->bm, &bmop, "faceout", BM_ELEM_SELECT, BM_FACE, TRUE);

	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit);
	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_edge_face_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Edge/Face";
	ot->description = "Add an edge or face to selected";
	ot->idname = "MESH_OT_edge_face_add";
	
	/* api callbacks */
	ot->exec = addedgeface_mesh_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ************************* SEAMS AND EDGES **************** */

static int editbmesh_mark_seam(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Mesh *me = ((Mesh *)obedit->data);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMesh *bm = em->bm;
	BMEdge *eed;
	BMIter iter;
	int clear = RNA_boolean_get(op->ptr, "clear");
	
	/* auto-enable seams drawing */
	if (clear == 0) {
		me->drawflag |= ME_DRAWSEAMS;
	}

	if (clear) {
		BM_ITER(eed, &iter, bm, BM_EDGES_OF_MESH, NULL) {
			if (!BM_elem_flag_test(eed, BM_ELEM_SELECT) || BM_elem_flag_test(eed, BM_ELEM_HIDDEN))
				continue;
			
			BM_elem_flag_disable(eed, BM_ELEM_SEAM);
		}
	}
	else {
		BM_ITER(eed, &iter, bm, BM_EDGES_OF_MESH, NULL) {
			if (!BM_elem_flag_test(eed, BM_ELEM_SELECT) || BM_elem_flag_test(eed, BM_ELEM_HIDDEN))
				continue;
			BM_elem_flag_enable(eed, BM_ELEM_SEAM);
		}
	}

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_mark_seam(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Mark Seam";
	ot->idname = "MESH_OT_mark_seam";
	ot->description = "(un)mark selected edges as a seam";
	
	/* api callbacks */
	ot->exec = editbmesh_mark_seam;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "clear", 0, "Clear", "");
}

static int editbmesh_mark_sharp(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Mesh *me = ((Mesh *)obedit->data);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMesh *bm = em->bm;
	BMEdge *eed;
	BMIter iter;
	int clear = RNA_boolean_get(op->ptr, "clear");

	/* auto-enable sharp edge drawing */
	if (clear == 0) {
		me->drawflag |= ME_DRAWSHARP;
	}

	if (!clear) {
		BM_ITER(eed, &iter, bm, BM_EDGES_OF_MESH, NULL) {
			if (!BM_elem_flag_test(eed, BM_ELEM_SELECT) || BM_elem_flag_test(eed, BM_ELEM_HIDDEN))
				continue;
			
			BM_elem_flag_disable(eed, BM_ELEM_SMOOTH);
		}
	}
	else {
		BM_ITER(eed, &iter, bm, BM_EDGES_OF_MESH, NULL) {
			if (!BM_elem_flag_test(eed, BM_ELEM_SELECT) || BM_elem_flag_test(eed, BM_ELEM_HIDDEN))
				continue;
			
			BM_elem_flag_enable(eed, BM_ELEM_SMOOTH);
		}
	}


	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_mark_sharp(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Mark Sharp";
	ot->idname = "MESH_OT_mark_sharp";
	ot->description = "(un)mark selected edges as sharp";
	
	/* api callbacks */
	ot->exec = editbmesh_mark_sharp;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "clear", 0, "Clear", "");
}


static int editbmesh_vert_connect(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMesh *bm = em->bm;
	BMOperator bmop;
	int len = 0;
	
	if (!EDBM_InitOpf(em, &bmop, op, "connectverts verts=%hv", BM_ELEM_SELECT)) {
		return OPERATOR_CANCELLED;
	}
	BMO_op_exec(bm, &bmop);
	len = BMO_slot_get(&bmop, "edgeout")->len;
	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}
	
	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return len ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void MESH_OT_vert_connect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Vertex Connect";
	ot->idname = "MESH_OT_vert_connect";
	
	/* api callbacks */
	ot->exec = editbmesh_vert_connect;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int editbmesh_edge_split(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMesh *bm = em->bm;
	BMOperator bmop;
	int len = 0;
	
	if (!EDBM_InitOpf(em, &bmop, op, "edgesplit edges=%he numcuts=%i",
	                  BM_ELEM_SELECT, RNA_int_get(op->ptr,"number_cuts")))
	{
		return OPERATOR_CANCELLED;
	}
	BMO_op_exec(bm, &bmop);
	len = BMO_slot_get(&bmop, "outsplit")->len;
	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}
	
	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return len ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void MESH_OT_edge_split(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Edge Split";
	ot->idname = "MESH_OT_edge_split";
	
	/* api callbacks */
	ot->exec = editbmesh_edge_split;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_int(ot->srna, "number_cuts", 1, 1, 10, "Number of Cuts", "", 1, INT_MAX);
}

/****************** add duplicate operator ***************/

static int mesh_duplicate_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)ob->data)->edit_btmesh;
	BMOperator bmop;

	EDBM_InitOpf(em, &bmop, op, "dupe geom=%hvef", BM_ELEM_SELECT);
	
	BMO_op_exec(em->bm, &bmop);
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	BMO_slot_buffer_hflag_enable(em->bm, &bmop, "newout", BM_ELEM_SELECT, BM_ALL, TRUE);

	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	DAG_id_tag_update(ob->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);
	
	return OPERATOR_FINISHED;
}

static int mesh_duplicate_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	WM_cursor_wait(1);
	mesh_duplicate_exec(C, op);
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
	ot->invoke = mesh_duplicate_invoke;
	ot->exec = mesh_duplicate_exec;
	
	ot->poll = ED_operator_editmesh;
	
	/* to give to transform */
	RNA_def_int(ot->srna, "mode", TFM_TRANSLATION, 0, INT_MAX, "Mode", "", 0, INT_MAX);
}

static int flip_normals(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = (((Mesh *)obedit->data))->edit_btmesh;
	
	if (!EDBM_CallOpf(em, op, "reversefaces faces=%hf", BM_ELEM_SELECT))
		return OPERATOR_CANCELLED;
	
	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_flip_normals(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Flip Normals";
	ot->description = "Flip the direction of selected faces' normals (and of their vertices)";
	ot->idname = "MESH_OT_flip_normals";
	
	/* api callbacks */
	ot->exec = flip_normals;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static const EnumPropertyItem direction_items[] = {
	{DIRECTION_CW, "CW", 0, "Clockwise", ""},
	{DIRECTION_CCW, "CCW", 0, "Counter Clockwise", ""},
	{0, NULL, 0, NULL, NULL}};

/* only accepts 1 selected edge, or 2 selected faces */
static int edge_rotate_selected(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMOperator bmop;
	BMEdge *eed;
	BMIter iter;
	const int do_ccw = RNA_enum_get(op->ptr, "direction") == 1;
	int do_deselect = FALSE; /* do we deselect */
	
	if (!(em->bm->totfacesel == 2 || em->bm->totedgesel == 1)) {
		BKE_report(op->reports, RPT_ERROR, "Select one edge or two adjacent faces");
		return OPERATOR_CANCELLED;
	}

	/* first see if we have two adjacent faces */
	BM_ITER(eed, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		if (BM_edge_face_count(eed) == 2) {
			if ((BM_elem_flag_test(eed->l->f, BM_ELEM_SELECT) && BM_elem_flag_test(eed->l->radial_next->f, BM_ELEM_SELECT))
				 && !(BM_elem_flag_test(eed->l->f, BM_ELEM_HIDDEN) || BM_elem_flag_test(eed->l->radial_next->f, BM_ELEM_HIDDEN)))
			{
				break;
			}
		}
	}
	
	/* ok, we don't have two adjacent faces, but we do have two selected ones.
	 * that's an error condition.*/
	if (!eed && em->bm->totfacesel == 2) {
		BKE_report(op->reports, RPT_ERROR, "Select one edge or two adjacent faces");
		return OPERATOR_CANCELLED;
	}

	if (!eed) {
		BM_ITER(eed, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
			if (BM_elem_flag_test(eed, BM_ELEM_SELECT) && !BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
				/* de-select the edge before */
				do_deselect = TRUE;
				break;
			}
		}
	}

	/* this should never happen */
	if (!eed)
		return OPERATOR_CANCELLED;
	
	EDBM_InitOpf(em, &bmop, op, "edgerotate edges=%e ccw=%b", eed, do_ccw);

	/* avoid adding to the selection if we start off with only a selected edge,
	 * we could also just deselect the single edge easily but use the BMO api
	 * since it seems this is more 'correct' */
	if (do_deselect) BMO_slot_buffer_hflag_disable(em->bm, &bmop, "edges", BM_ELEM_SELECT, BM_EDGE, TRUE);

	BMO_op_exec(em->bm, &bmop);
	BMO_slot_buffer_hflag_enable(em->bm, &bmop, "edgeout", BM_ELEM_SELECT, BM_EDGE, TRUE);

	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_edge_rotate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Rotate Selected Edge";
	ot->description = "Rotate selected edge or adjoining faces";
	ot->idname = "MESH_OT_edge_rotate";

	/* api callbacks */
	ot->exec = edge_rotate_selected;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* props */
	RNA_def_enum(ot->srna, "direction", direction_items, DIRECTION_CW, "Direction", "Direction to rotate edge around");
}

/* swap is 0 or 1, if 1 it hides not selected */
void EDBM_hide_mesh(BMEditMesh *em, int swap)
{
	BMIter iter;
	BMElem *ele;
	int itermode;

	if (em == NULL) return;
	
	if (em->selectmode & SCE_SELECT_VERTEX)
		itermode = BM_VERTS_OF_MESH;
	else if (em->selectmode & SCE_SELECT_EDGE)
		itermode = BM_EDGES_OF_MESH;
	else
		itermode = BM_FACES_OF_MESH;

	BM_ITER(ele, &iter, em->bm, itermode, NULL) {
		if (BM_elem_flag_test(ele, BM_ELEM_SELECT) ^ swap)
			BM_elem_hide_set(em->bm, ele, TRUE);
	}

	EDBM_selectmode_flush(em);

	/* original hide flushing comment (OUTDATED):
	 * hide happens on least dominant select mode, and flushes up, not down! (helps preventing errors in subsurf) */
	/*  - vertex hidden, always means edge is hidden too
		- edge hidden, always means face is hidden too
		- face hidden, only set face hide
		- then only flush back down what's absolute hidden
	*/

}

static int hide_mesh_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = (((Mesh *)obedit->data))->edit_btmesh;
	
	EDBM_hide_mesh(em, RNA_boolean_get(op->ptr, "unselected"));
		
	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hide Selection";
	ot->idname = "MESH_OT_hide";
	
	/* api callbacks */
	ot->exec = hide_mesh_exec;
	ot->poll = ED_operator_editmesh;
	 ot->description = "Hide (un)selected vertices, edges or faces";

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected");
}


void EDBM_reveal_mesh(BMEditMesh *em)
{
	const char iter_types[3] = {BM_VERTS_OF_MESH,
	                            BM_EDGES_OF_MESH,
	                            BM_FACES_OF_MESH};

	int sels[3] = {(em->selectmode & SCE_SELECT_VERTEX),
	               (em->selectmode & SCE_SELECT_EDGE),
	               (em->selectmode & SCE_SELECT_FACE),
	              };

	BMIter iter;
    BMElem *ele;
	int i;

	/* Use tag flag to remember what was hidden before all is revealed.
	 * BM_ELEM_HIDDEN --> BM_ELEM_TAG */
	for (i = 0; i < 3; i++) {
		BM_ITER(ele, &iter, em->bm, iter_types[i], NULL) {
			BM_elem_flag_set(ele, BM_ELEM_TAG, BM_elem_flag_test(ele, BM_ELEM_HIDDEN));
		}
	}

	/* Reveal everything */
	EDBM_flag_disable_all(em, BM_ELEM_HIDDEN);

	/* Select relevant just-revealed elements */
	for (i = 0; i < 3; i++) {
		if (!sels[i]) {
			continue;
		}

		BM_ITER(ele, &iter, em->bm, iter_types[i], NULL) {
			if (BM_elem_flag_test(ele, BM_ELEM_TAG)) {
				BM_elem_select_set(em->bm, ele, TRUE);
			}
		}
	}

	EDBM_selectmode_flush(em);
}

static int reveal_mesh_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = (((Mesh *)obedit->data))->edit_btmesh;
	
	EDBM_reveal_mesh(em);

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_reveal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reveal Hidden";
	ot->idname = "MESH_OT_reveal";
	ot->description = "Reveal all hidden vertices, edges and faces";
	
	/* api callbacks */
	ot->exec = reveal_mesh_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int normals_make_consistent_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	
	/* doflip has to do with bmesh_rationalize_normals, it's an internal
	 * thing */
	if (!EDBM_CallOpf(em, op, "righthandfaces faces=%hf do_flip=%b", BM_ELEM_SELECT, TRUE))
		return OPERATOR_CANCELLED;

	if (RNA_boolean_get(op->ptr, "inside"))
		EDBM_CallOpf(em, op, "reversefaces faces=%hf", BM_ELEM_SELECT);

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_normals_make_consistent(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Normals Consistent";
	ot->description = "Make face and vertex normals point either outside or inside the mesh";
	ot->idname = "MESH_OT_normals_make_consistent";
	
	/* api callbacks */
	ot->exec = normals_make_consistent_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "inside", 0, "Inside", "");
}



static int do_smooth_vertex(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	ModifierData *md;
	int mirrx = FALSE, mirry = FALSE, mirrz = FALSE;
	int i, repeat;
	float clipdist = 0.0f;

	/* mirror before smooth */
	if (((Mesh *)obedit->data)->editflag & ME_EDIT_MIRROR_X) {
		EDBM_CacheMirrorVerts(em, TRUE);
	}

	/* if there is a mirror modifier with clipping, flag the verts that
	 * are within tolerance of the plane(s) of reflection 
	 */
	for (md = obedit->modifiers.first; md; md = md->next) {
		if (md->type == eModifierType_Mirror && (md->mode & eModifierMode_Realtime)) {
			MirrorModifierData *mmd = (MirrorModifierData *)md;
		
			if (mmd->flag & MOD_MIR_CLIPPING) {
				if (mmd->flag & MOD_MIR_AXIS_X)
					mirrx = TRUE;
				if (mmd->flag & MOD_MIR_AXIS_Y)
					mirry = TRUE;
				if (mmd->flag & MOD_MIR_AXIS_Z)
					mirrz = TRUE;

				clipdist = mmd->tolerance;
			}
		}
	}

	repeat = RNA_int_get(op->ptr,"repeat");
	if (!repeat)
		repeat = 1;
	
	for (i = 0; i < repeat; i++) {
		if (!EDBM_CallOpf(em, op,
		                  "vertexsmooth verts=%hv mirror_clip_x=%b mirror_clip_y=%b mirror_clip_z=%b clipdist=%f",
		                  BM_ELEM_SELECT, mirrx, mirry, mirrz, clipdist))
		{
			return OPERATOR_CANCELLED;
		}
	}

	/* apply mirror */
	if (((Mesh *)obedit->data)->editflag & ME_EDIT_MIRROR_X) {
		EDBM_ApplyMirrorCache(em, BM_ELEM_SELECT, 0);
		EDBM_EndMirrorCache(em);
	}

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}	
	
void MESH_OT_vertices_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Smooth Vertex";
	ot->description = "Flatten angles of selected vertices";
	ot->idname = "MESH_OT_vertices_smooth";
	
	/* api callbacks */
	ot->exec = do_smooth_vertex;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_int(ot->srna, "repeat", 1, 1, 100, "Number of times to smooth the mesh", "", 1, INT_MAX);
}


static int bm_test_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = CTX_wm_view3d(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMBVHTree *tree = BMBVH_NewBVH(em, 0, NULL, NULL);
	BMIter iter;
	BMEdge *e;

	/* hide all back edges */
	BM_ITER(e, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		if (!BM_elem_flag_test(e, BM_ELEM_SELECT))
			continue;

		if (!BMBVH_EdgeVisible(tree, e, ar, v3d, obedit))
			BM_elem_select_set(em->bm, e, FALSE);
	}

	BMBVH_FreeBVH(tree);
	
#if 0 //uv island walker test
	BMIter iter, liter;
	BMFace *f;
	BMLoop *l, *l2;
	MLoopUV *luv;
	BMWalker walker;
	int i = 0;

	BM_ITER(f, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, f) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		}
	}

	BMW_init(&walker, em->bm, BMW_UVISLAND, BMW_NIL_LAY);

	BM_ITER(f, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, f) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			if (luv->flag & MLOOPUV_VERTSEL) {
				l2 = BMW_begin(&walker, l);
				for (; l2; l2 = BMW_step(&walker)) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l2->head.data, CD_MLOOPUV);
					luv->flag |= MLOOPUV_VERTSEL;
				}				
			}
		}
	}

	BMW_end(&walker);
#endif
	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}	
	
void MESH_OT_bm_test(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "BMesh Test Operator";
	ot->idname = "MESH_OT_bm_test";
	
	/* api callbacks */
	ot->exec = bm_test_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	//RNA_def_int(ot->srna, "repeat", 1, 1, 100, "Number of times to smooth the mesh", "", 1, INT_MAX);
}

/********************** Smooth/Solid Operators *************************/

static void mesh_set_smooth_faces(BMEditMesh *em, short smooth)
{
	BMIter iter;
	BMFace *efa;

	if (em == NULL) return;
	
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
			BM_elem_flag_set(efa, BM_ELEM_SMOOTH, smooth);
		}
	}
}

static int mesh_faces_shade_smooth_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;

	mesh_set_smooth_faces(em, 1);

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_faces_shade_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Shade Smooth";
	 ot->description = "Display faces smooth (using vertex normals)";
	ot->idname = "MESH_OT_faces_shade_smooth";

	/* api callbacks */
	ot->exec = mesh_faces_shade_smooth_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int mesh_faces_shade_flat_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;

	mesh_set_smooth_faces(em, 0);

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_faces_shade_flat(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Shade Flat";
	ot->description = "Display faces flat";
	ot->idname = "MESH_OT_faces_shade_flat";

	/* api callbacks */
	ot->exec = mesh_faces_shade_flat_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}


/********************** UV/Color Operators *************************/


static const EnumPropertyItem axis_items[] = {
	{OPUVC_AXIS_X, "X", 0, "X", ""},
	{OPUVC_AXIS_Y, "Y", 0, "Y", ""},
	{0, NULL, 0, NULL, NULL}};

static int mesh_rotate_uvs(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)ob->data)->edit_btmesh;
	BMOperator bmop;

	/* get the direction from RNA */
	int dir = RNA_enum_get(op->ptr, "direction");

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_InitOpf(em, &bmop, op, "face_rotateuvs faces=%hf dir=%i", BM_ELEM_SELECT, dir);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* finish the operator */
	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	/* dependencies graph and notification stuff */
	DAG_id_tag_update(ob->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);

	/* we succeeded */
	return OPERATOR_FINISHED;
}

static int mesh_reverse_uvs(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)ob->data)->edit_btmesh;
	BMOperator bmop;

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_InitOpf(em, &bmop, op, "face_reverseuvs faces=%hf", BM_ELEM_SELECT);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* finish the operator */
	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	/* dependencies graph and notification stuff */
	DAG_id_tag_update(ob->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);

	/* we succeeded */
	return OPERATOR_FINISHED;
}

static int mesh_rotate_colors(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)ob->data)->edit_btmesh;
	BMOperator bmop;

	/* get the direction from RNA */
	int dir = RNA_enum_get(op->ptr, "direction");

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_InitOpf(em, &bmop, op, "face_rotatecolors faces=%hf dir=%i", BM_ELEM_SELECT, dir);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* finish the operator */
	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	/* dependencies graph and notification stuff */
	DAG_id_tag_update(ob->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);
/*	DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_GEOM_SELECT, ob);
*/
	/* we succeeded */
	return OPERATOR_FINISHED;
}


static int mesh_reverse_colors(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)ob->data)->edit_btmesh;
	BMOperator bmop;

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_InitOpf(em, &bmop, op, "face_reversecolors faces=%hf", BM_ELEM_SELECT);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* finish the operator */
	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	DAG_id_tag_update(ob->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);

	/* we succeeded */
	return OPERATOR_FINISHED;
}

void MESH_OT_uvs_rotate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Rotate UVs";
	ot->idname = "MESH_OT_uvs_rotate";

	/* api callbacks */
	ot->exec = mesh_rotate_uvs;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* props */
	RNA_def_enum(ot->srna, "direction", direction_items, DIRECTION_CW, "Direction", "Direction to rotate UVs around");
}

//void MESH_OT_uvs_mirror(wmOperatorType *ot)
void MESH_OT_uvs_reverse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reverse UVs";
	ot->idname = "MESH_OT_uvs_reverse";

	/* api callbacks */
	ot->exec = mesh_reverse_uvs;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* props */
	//RNA_def_enum(ot->srna, "axis", axis_items, DIRECTION_CW, "Axis", "Axis to mirror UVs around");
}

void MESH_OT_colors_rotate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Rotate Colors";
	ot->idname = "MESH_OT_colors_rotate";

	/* api callbacks */
	ot->exec = mesh_rotate_colors;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* props */
	RNA_def_enum(ot->srna, "direction", direction_items, DIRECTION_CCW, "Direction", "Direction to rotate edge around");
}

void MESH_OT_colors_reverse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reverse Colors";
	ot->idname = "MESH_OT_colors_reverse";

	/* api callbacks */
	ot->exec = mesh_reverse_colors;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* props */
	//RNA_def_enum(ot->srna, "axis", axis_items, DIRECTION_CW, "Axis", "Axis to mirror colors around");
}


static int merge_firstlast(BMEditMesh *em, int first, int uvmerge, wmOperator *wmop)
{
	BMVert *mergevert;
	BMEditSelection *ese;

	/* do sanity check in mergemenu in edit.c ?*/
	if (first == 0) {
		ese = em->bm->selected.last;
		mergevert = (BMVert *)ese->ele;
	}
	else {
		ese = em->bm->selected.first;
		mergevert = (BMVert *)ese->ele;
	}

	if (!BM_elem_flag_test(mergevert, BM_ELEM_SELECT))
		return OPERATOR_CANCELLED;
	
	if (uvmerge) {
		if (!EDBM_CallOpf(em, wmop, "pointmerge_facedata verts=%hv snapv=%e", BM_ELEM_SELECT, mergevert))
			return OPERATOR_CANCELLED;
	}

	if (!EDBM_CallOpf(em, wmop, "pointmerge verts=%hv mergeco=%v", BM_ELEM_SELECT, mergevert->co))
		return OPERATOR_CANCELLED;

	return OPERATOR_FINISHED;
}

static int merge_target(BMEditMesh *em, Scene *scene, View3D *v3d, Object *ob, 
                        int target, int uvmerge, wmOperator *wmop)
{
	BMIter iter;
	BMVert *v;
	float *vco = NULL, co[3], cent[3] = {0.0f, 0.0f, 0.0f};

	if (target) {
		vco = give_cursor(scene, v3d);
		copy_v3_v3(co, vco);
		mul_m4_v3(ob->imat, co);
	}
	else {
		float fac;
		int i = 0;
		BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			if (!BM_elem_flag_test(v, BM_ELEM_SELECT))
				continue;
			add_v3_v3(cent, v->co);
			i++;
		}
		
		if (!i)
			return OPERATOR_CANCELLED;

		fac = 1.0f / (float)i;
		mul_v3_fl(cent, fac);
		copy_v3_v3(co, cent);
		vco = co;
	}

	if (!vco)
		return OPERATOR_CANCELLED;
	
	if (uvmerge) {
		if (!EDBM_CallOpf(em, wmop, "vert_average_facedata verts=%hv", BM_ELEM_SELECT))
			return OPERATOR_CANCELLED;
	}

	if (!EDBM_CallOpf(em, wmop, "pointmerge verts=%hv mergeco=%v", BM_ELEM_SELECT, co))
		return OPERATOR_CANCELLED;

	return OPERATOR_FINISHED;
}

static int merge_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	int status = 0, uvs = RNA_boolean_get(op->ptr, "uvs");

	switch(RNA_enum_get(op->ptr, "type")) {
		case 3:
			status = merge_target(em, scene, v3d, obedit, 0, uvs, op);
			break;
		case 4:
			status = merge_target(em, scene, v3d, obedit, 1, uvs, op);
			break;
		case 1:
			status = merge_firstlast(em, 0, uvs, op);
			break;
		case 6:
			status = merge_firstlast(em, 1, uvs, op);
			break;
		case 5:
			status = 1;
			if (!EDBM_CallOpf(em, op, "collapse edges=%he", BM_ELEM_SELECT))
				status = 0;
			break;
	}

	if (!status)
		return OPERATOR_CANCELLED;

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

static EnumPropertyItem merge_type_items[] = {
	{6, "FIRST", 0, "At First", ""},
	{1, "LAST", 0, "At Last", ""},
	{3, "CENTER", 0, "At Center", ""},
	{4, "CURSOR", 0, "At Cursor", ""},
	{5, "COLLAPSE", 0, "Collapse", ""},
	{0, NULL, 0, NULL, NULL}};

static EnumPropertyItem *merge_type_itemf(bContext *C, PointerRNA *UNUSED(ptr),  PropertyRNA *UNUSED(prop), int *free)
{	
	Object *obedit;
	EnumPropertyItem *item = NULL;
	int totitem = 0;
	
	if (!C) /* needed for docs */
		return merge_type_items;
	
	obedit = CTX_data_edit_object(C);
	if (obedit && obedit->type == OB_MESH) {
		BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;

		if (em->selectmode & SCE_SELECT_VERTEX) {
			if (em->bm->selected.first && em->bm->selected.last &&
			    ((BMEditSelection *)em->bm->selected.first)->htype == BM_VERT &&
			    ((BMEditSelection *)em->bm->selected.last)->htype == BM_VERT)
			{
				RNA_enum_items_add_value(&item, &totitem, merge_type_items, 6);
				RNA_enum_items_add_value(&item, &totitem, merge_type_items, 1);
			}
			else if (em->bm->selected.first && ((BMEditSelection *)em->bm->selected.first)->htype == BM_VERT) {
				RNA_enum_items_add_value(&item, &totitem, merge_type_items, 1);
			}
			else if (em->bm->selected.last && ((BMEditSelection *)em->bm->selected.last)->htype == BM_VERT) {
				RNA_enum_items_add_value(&item, &totitem, merge_type_items, 6);
			}
		}

		RNA_enum_items_add_value(&item, &totitem, merge_type_items, 3);
		RNA_enum_items_add_value(&item, &totitem, merge_type_items, 4);
		RNA_enum_items_add_value(&item, &totitem, merge_type_items, 5);
		RNA_enum_item_end(&item, &totitem);

		*free = 1;

		return item;
	}
	
	return NULL;
}

void MESH_OT_merge(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Merge";
	ot->idname = "MESH_OT_merge";

	/* api callbacks */
	ot->exec = merge_exec;
	ot->invoke = WM_menu_invoke;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", merge_type_items, 3, "Type", "Merge method to use");
	RNA_def_enum_funcs(ot->prop, merge_type_itemf);
	RNA_def_boolean(ot->srna, "uvs", 1, "UVs", "Move UVs according to merge");
}


static int removedoublesflag_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMOperator bmop;
	int count;

	EDBM_InitOpf(em, &bmop, op, "finddoubles verts=%hv dist=%f", BM_ELEM_SELECT, RNA_float_get(op->ptr, "mergedist"));
	BMO_op_exec(em->bm, &bmop);

	count = BMO_slot_map_count(em->bm, &bmop, "targetmapout");

	if (!EDBM_CallOpf(em, op, "weldverts targetmap=%s", &bmop, "targetmapout")) {
		BMO_op_finish(em->bm, &bmop);
		return OPERATOR_CANCELLED;
	}

	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}
	
	BKE_reportf(op->reports, RPT_INFO, "Removed %d vert%s", count, (count==1)?"ex":"ices");


	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_remove_doubles(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Doubles";
	ot->idname = "MESH_OT_remove_doubles";

	/* api callbacks */
	ot->exec = removedoublesflag_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_float(ot->srna, "mergedist", 0.0001f, 0.000001f, 50.0f, 
		"Merge Distance", 
		"Minimum distance between elements to merge", 0.00001, 10.0);
}

/************************ Vertex Path Operator *************************/

typedef struct PathNode {
	int u;
	int visited;
	ListBase edges;
} PathNode;

typedef struct PathEdge {
	struct PathEdge *next, *prev;
	int v;
	float w;
} PathEdge;



static int select_vertex_path_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)ob->data)->edit_btmesh;
	BMOperator bmop;
	BMEditSelection *sv, *ev;

	/* get the type from RNA */
	int type = RNA_enum_get(op->ptr, "type");

	sv = em->bm->selected.last;
	if (sv != NULL)
		ev = sv->prev;
	else return OPERATOR_CANCELLED;
	if (ev == NULL)
		return OPERATOR_CANCELLED;

	if ((sv->htype != BM_VERT) || (ev->htype != BM_VERT))
		return OPERATOR_CANCELLED;

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_InitOpf(em, &bmop, op, "vertexshortestpath startv=%e endv=%e type=%i", sv->ele, ev->ele, type);

	/* execute the operator */
	BMO_op_exec(em->bm, &bmop);

	/* DO NOT clear the existing selection */
	/* EDBM_flag_disable_all(em, BM_ELEM_SELECT); */

	/* select the output */
	BMO_slot_buffer_hflag_enable(em->bm, &bmop, "vertout", BM_ELEM_SELECT, BM_ALL, TRUE);

	/* finish the operator */
	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_selectmode_flush(em);

	/* dependencies graph and notification stuff */
/*	DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_GEOM_SELECT, ob);
*/
	DAG_id_tag_update(ob->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);


	/* we succeeded */
	return OPERATOR_FINISHED;
}

void MESH_OT_select_vertex_path(wmOperatorType *ot)
{
	static const EnumPropertyItem type_items[] = {
		{VPATH_SELECT_EDGE_LENGTH, "EDGE_LENGTH", 0, "Edge Length", NULL},
		{VPATH_SELECT_TOPOLOGICAL, "TOPOLOGICAL", 0, "Topological", NULL},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Select Vertex Path";
	ot->idname = "MESH_OT_select_vertex_path";

	/* api callbacks */
	ot->exec = select_vertex_path_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "type", type_items, VPATH_SELECT_EDGE_LENGTH, "Type", "Method to compute distance");
}
/********************** Rip Operator *************************/

/* helper to find edge for edge_rip */
static float mesh_rip_edgedist(ARegion *ar, float mat[][4], float *co1, float *co2, const int mval[2])
{
	float vec1[3], vec2[3], mvalf[2];

	ED_view3d_project_float(ar, co1, vec1, mat);
	ED_view3d_project_float(ar, co2, vec2, mat);
	mvalf[0] = (float)mval[0];
	mvalf[1] = (float)mval[1];

	return dist_to_line_segment_v2(mvalf, vec1, vec2);
}

/* based on mouse cursor position, it defines how is being ripped */
static int mesh_rip_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *obedit = CTX_data_edit_object(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMesh *bm = em->bm;
	BMOperator bmop;
	BMBVHTree *bvhtree;
	BMOIter siter;
	BMIter iter, eiter, liter;
	BMLoop *l;
	BMEdge *e, *e2, *closest = NULL;
	BMVert *v, *ripvert = NULL;
	int side = 0, i, singlesel = 0;
	float projectMat[4][4], fmval[3] = {event->mval[0], event->mval[1]};
	float dist = FLT_MAX, d;

	ED_view3d_ob_project_mat_get(rv3d, obedit, projectMat);

	/* BM_ELEM_SELECT --> BM_ELEM_TAG */
	BM_ITER(e, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		BM_elem_flag_set(e, BM_ELEM_TAG, BM_elem_flag_test(e, BM_ELEM_SELECT));
	}

	/* handle case of one vert selected.  identify
	 * closest edge around that vert to mouse cursor,
	 * then rip two adjacent edges in the vert fan. */
	if (bm->totvertsel == 1 && bm->totedgesel == 0 && bm->totfacesel == 0) {
		singlesel = 1;

		/* find selected vert */
		BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
			if (BM_elem_flag_test(v, BM_ELEM_SELECT))
				break;
		}

		/* this should be impossible, but sanity checks are a good thing */
		if (!v)
			return OPERATOR_CANCELLED;

		if (!v->e || !v->e->l) {
			BKE_report(op->reports, RPT_ERROR, "Selected vertex has no faces");
			return OPERATOR_CANCELLED;
		}

		/* find closest edge to mouse cursor */
		e2 = NULL;
		BM_ITER(e, &iter, bm, BM_EDGES_OF_VERT, v) {
			d = mesh_rip_edgedist(ar, projectMat, e->v1->co, e->v2->co, event->mval);
			if (d < dist) {
				dist = d;
				e2 = e;
			}
		}

		if (!e2)
			return OPERATOR_CANCELLED;

		/* rip two adjacent edges */
		if (BM_edge_face_count(e2) == 1 || BM_vert_face_count(v) == 2) {
			l = e2->l;
			ripvert = BM_vert_rip(bm, l->f, v);

			BLI_assert(ripvert);
			if (!ripvert) {
				return OPERATOR_CANCELLED;
			}
		}
		else if (BM_edge_face_count(e2) == 2) {
			l = e2->l;
			e = BM_face_other_loop(e2, l->f, v)->e;
			BM_elem_flag_enable(e, BM_ELEM_TAG);
			BM_elem_select_set(bm, e, TRUE);
			
			l = e2->l->radial_next;
			e = BM_face_other_loop(e2, l->f, v)->e;
			BM_elem_flag_enable(e, BM_ELEM_TAG);
			BM_elem_select_set(bm, e, TRUE);
		}

		dist = FLT_MAX;
	}
	else {
		/* expand edge selection */
		BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
			e2 = NULL;
			i = 0;
			BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
				if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
					e2 = e;
					i++;
				}
			}
			
			if (i == 1 && e2->l) {
				l = BM_face_other_loop(e2, e2->l->f, v);
				l = l->radial_next;
				l = BM_face_other_loop(l->e, l->f, v);

				if (l) {
					BM_elem_select_set(bm, l->e, TRUE);
				}
			}
		}
	}

	if (!EDBM_InitOpf(em, &bmop, op, "edgesplit edges=%he", BM_ELEM_SELECT)) {
		return OPERATOR_CANCELLED;
	}
	
	BMO_op_exec(bm, &bmop);

	/* build bvh tree for edge visibility tests */
	bvhtree = BMBVH_NewBVH(em, 0, NULL, NULL);

	for (i = 0; i < 2; i++) {
		BMO_ITER(e, &siter, bm, &bmop, i ? "edgeout2":"edgeout1", BM_EDGE) {
			float cent[3] = {0, 0, 0}, mid[3], vec[3];

			if (!BMBVH_EdgeVisible(bvhtree, e, ar, v3d, obedit) || !e->l)
				continue;

			/* method for calculating distance:
			 *
			 * for each edge: calculate face center, then made a vector
			 * from edge midpoint to face center.  offset edge midpoint
			 * by a small amount along this vector. */
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, e->l->f) {
				add_v3_v3(cent, l->v->co);
			}
			mul_v3_fl(cent, 1.0f/(float)e->l->f->len);

			mid_v3_v3v3(mid, e->v1->co, e->v2->co);
			sub_v3_v3v3(vec, cent, mid);
			normalize_v3(vec);
			mul_v3_fl(vec, 0.01f);
			add_v3_v3v3(mid, mid, vec);

			/* yay we have our comparison point, now project it */
			ED_view3d_project_float(ar, mid, mid, projectMat);

			d = len_squared_v2v2(fmval, mid);

			if (d < dist) {
				side = i;
				closest = e;
				dist = d;
			}
		}
	}
	
	BMBVH_FreeBVH(bvhtree);

	EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	BMO_slot_buffer_hflag_enable(bm, &bmop, side?"edgeout2":"edgeout1", BM_ELEM_SELECT, BM_EDGE, TRUE);

	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		BM_elem_flag_set(e, BM_ELEM_TAG, BM_elem_flag_test(e, BM_ELEM_SELECT));
	}

	/* constrict edge selection again */
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		e2 = NULL;
		i = 0;
		BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
			if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
				e2 = e;
				i++;
			}
		}
		
		if (i == 1) {
			if (singlesel)
				BM_elem_select_set(bm, v, FALSE);
			else
				BM_elem_select_set(bm, e2, FALSE);
		}
	}

	if (ripvert) {
		BM_elem_select_set(bm, ripvert, TRUE);
	}

	EDBM_selectmode_flush(em);

	BLI_assert(singlesel ? (bm->totvertsel > 0) : (bm->totedgesel > 0));

	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	if (bm->totvertsel == 0) {
		return OPERATOR_CANCELLED;
	}
	
	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_rip(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Rip";
	ot->idname = "MESH_OT_rip";

	/* api callbacks */
	ot->invoke = mesh_rip_invoke;
	ot->poll = EM_view3d_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* to give to transform */
	Transform_Properties(ot, P_PROPORTIONAL);
	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
}

/************************ Shape Operators *************************/

/* BMESH_TODO this should be properly encapsulated in a bmop.  but later.*/
static void shape_propagate(Object *obedit, BMEditMesh *em, wmOperator *op)
{
	BMIter iter;
	BMVert *eve = NULL;
	float *co;
	int i, totshape = CustomData_number_of_layers(&em->bm->vdata, CD_SHAPEKEY);

	if (!CustomData_has_layer(&em->bm->vdata, CD_SHAPEKEY)) {
		BKE_report(op->reports, RPT_ERROR, "Mesh does not have shape keys");
		return;
	}
	
	BM_ITER(eve, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
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
			base->object->recalc = OB_RECALC_DATA;
		}
	}
#endif

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
}


static int shape_propagate_to_all_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Mesh *me = obedit->data;
	BMEditMesh *em = me->edit_btmesh;

	shape_propagate(obedit, em, op);

	DAG_id_tag_update(&me->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, me);

	return OPERATOR_FINISHED;
}


void MESH_OT_shape_propagate_to_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Shape Propagate";
	ot->description = "Apply selected vertex locations to all other shape keys";
	ot->idname = "MESH_OT_shape_propagate_to_all";

	/* api callbacks */
	ot->exec = shape_propagate_to_all_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* BMESH_TODO this should be properly encapsulated in a bmop.  but later.*/
static int blend_from_shape_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Mesh *me = obedit->data;
	BMEditMesh *em = me->edit_btmesh;
	BMVert *eve;
	BMIter iter;
	float co[3], *sco;
	float blend = RNA_float_get(op->ptr, "blend");
	int shape = RNA_enum_get(op->ptr, "shape");
	int add = RNA_boolean_get(op->ptr, "add");
	int totshape;

	/* sanity check */
	totshape = CustomData_number_of_layers(&em->bm->vdata, CD_SHAPEKEY);
	if (totshape == 0 || shape < 0 || shape >= totshape)
		return OPERATOR_CANCELLED;

	BM_ITER(eve, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		if (!BM_elem_flag_test(eve, BM_ELEM_SELECT) || BM_elem_flag_test(eve, BM_ELEM_HIDDEN))
			continue;

		sco = CustomData_bmesh_get_n(&em->bm->vdata, eve->head.data, CD_SHAPEKEY, shape);
		copy_v3_v3(co, sco);


		if (add) {
			mul_v3_fl(co, blend);
			add_v3_v3v3(eve->co, eve->co, co);
		}
		else
			interp_v3_v3v3(eve->co, eve->co, co, blend);
		
		copy_v3_v3(sco, co);
	}

	DAG_id_tag_update(&me->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, me);

	return OPERATOR_FINISHED;
}

static EnumPropertyItem *shape_itemf(bContext *C, PointerRNA *UNUSED(ptr),  PropertyRNA *UNUSED(prop), int *free)
{	
	Object *obedit = CTX_data_edit_object(C);
	Mesh *me = (obedit) ? obedit->data : NULL;
	BMEditMesh *em = (me) ? me->edit_btmesh : NULL;
	EnumPropertyItem *item = NULL;
	int totitem = 0;

	if (obedit && obedit->type == OB_MESH && CustomData_has_layer(&em->bm->vdata, CD_SHAPEKEY)) {
		EnumPropertyItem tmp = {0, "", 0, "", ""};
		int a;

		for (a = 0; a < em->bm->vdata.totlayer; a++) {
			if (em->bm->vdata.layers[a].type != CD_SHAPEKEY)
				continue;

			tmp.value = totitem;
			tmp.identifier = em->bm->vdata.layers[a].name;
			tmp.name = em->bm->vdata.layers[a].name;
			RNA_enum_item_add(&item, &totitem, &tmp);

			totitem++;
		}
	}

	RNA_enum_item_end(&item, &totitem);
	*free = 1;

	return item;
}

void MESH_OT_blend_from_shape(wmOperatorType *ot)
{
	PropertyRNA *prop;
	static EnumPropertyItem shape_items[] = {{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Blend From Shape";
	ot->description = "Blend in shape from a shape key";
	ot->idname = "MESH_OT_blend_from_shape";

	/* api callbacks */
	ot->exec = blend_from_shape_exec;
	ot->invoke = WM_operator_props_popup;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_enum(ot->srna, "shape", shape_items, 0, "Shape", "Shape key to use for blending");
	RNA_def_enum_funcs(prop, shape_itemf);
	RNA_def_float(ot->srna, "blend", 1.0f, -FLT_MAX, FLT_MAX, "Blend", "Blending factor", -2.0f, 2.0f);
	RNA_def_boolean(ot->srna, "add", 1, "Add", "Add rather than blend between shapes");
}

/* BMESH_TODO - some way to select on an arbitrary axis */
static int select_axis_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMEditSelection *ese = em->bm->selected.last;
	int axis = RNA_enum_get(op->ptr, "axis");
	int mode = RNA_enum_get(op->ptr, "mode"); /* -1 == aligned, 0 == neg, 1 == pos */

	if (ese == NULL || ese->htype != BM_VERT) {
		BKE_report(op->reports, RPT_WARNING, "This operator requires an active vertex (last selected)");
		return OPERATOR_CANCELLED;
	}
	else {
		BMVert *ev, *act_vert = (BMVert *)ese->ele;
		BMIter iter;
		float value = act_vert->co[axis];
		float limit =  CTX_data_tool_settings(C)->doublimit; // XXX

		if (mode == 0)
			value -= limit;
		else if (mode == 1)
			value += limit;

		BM_ITER(ev, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			if (!BM_elem_flag_test(ev, BM_ELEM_HIDDEN)) {
				switch(mode) {
				case -1: /* aligned */
					if (fabs(ev->co[axis] - value) < limit)
						BM_elem_select_set(em->bm, ev, TRUE);
					break;
				case 0: /* neg */
					if (ev->co[axis] > value)
						BM_elem_select_set(em->bm, ev, TRUE);
					break;
				case 1: /* pos */
					if (ev->co[axis] < value)
						BM_elem_select_set(em->bm, ev, TRUE);
					break;
				}
			}
		}
	}

	EDBM_selectmode_flush(em);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_axis(wmOperatorType *ot)
{
	static EnumPropertyItem axis_mode_items[] = {
		{0,  "POSITIVE", 0, "Positive Axis", ""},
		{1,  "NEGATIVE", 0, "Negative Axis", ""},
		{-1, "ALIGNED",  0, "Aligned Axis", ""},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem axis_items_xyz[] = {
		{0, "X_AXIS", 0, "X Axis", ""},
		{1, "Y_AXIS", 0, "Y Axis", ""},
		{2, "Z_AXIS", 0, "Z Axis", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Select Axis";
	ot->description = "Select all data in the mesh on a single axis";
	ot->idname = "MESH_OT_select_axis";

	/* api callbacks */
	ot->exec = select_axis_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_enum(ot->srna, "mode", axis_mode_items, 0, "Axis Mode", "Axis side to use when selecting");
	RNA_def_enum(ot->srna, "axis", axis_items_xyz, 0, "Axis", "Select the axis to compare each vertex on");
}

static int solidify_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Mesh *me = obedit->data;
	BMEditMesh *em = me->edit_btmesh;
	BMesh *bm = em->bm;
	BMOperator bmop;

	float thickness = RNA_float_get(op->ptr, "thickness");

	if (!EDBM_InitOpf(em, &bmop, op, "solidify geom=%hf thickness=%f", BM_ELEM_SELECT, thickness)) {
		return OPERATOR_CANCELLED;
	}

	/* deselect only the faces in the region to be solidified (leave wire
	   edges and loose verts selected, as there will be no corresponding
	   geometry selected below) */
	BMO_slot_buffer_hflag_disable(bm, &bmop, "geom", BM_ELEM_SELECT, BM_FACE, TRUE);

	/* run the solidify operator */
	BMO_op_exec(bm, &bmop);

	/* select the newly generated faces */
	BMO_slot_buffer_hflag_enable(bm, &bmop, "geomout", BM_ELEM_SELECT, BM_FACE, TRUE);

	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

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
	ot->exec = solidify_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	prop = RNA_def_float(ot->srna, "thickness", 0.01f, -FLT_MAX, FLT_MAX, "thickness", "", -10.0f, 10.0f);
	RNA_def_property_ui_range(prop, -10, 10, 0.1, 4);
}

#define TRAIL_POLYLINE 1 /* For future use, They don't do anything yet */
#define TRAIL_FREEHAND 2
#define TRAIL_MIXED    3 /* (1|2) */
#define TRAIL_AUTO     4 
#define	TRAIL_MIDPOINTS 8

typedef struct CutCurve {
	float  x;
	float  y;
} CutCurve;

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

#define KNIFE_EXACT		1
#define KNIFE_MIDPOINT	2
#define KNIFE_MULTICUT	3

static EnumPropertyItem knife_items[] = {
	{KNIFE_EXACT, "EXACT", 0, "Exact", ""},
	{KNIFE_MIDPOINT, "MIDPOINTS", 0, "Midpoints", ""},
	{KNIFE_MULTICUT, "MULTICUT", 0, "Multicut", ""},
	{0, NULL, 0, NULL, NULL}
};

/* bm_edge_seg_isect() Determines if and where a mouse trail intersects an BMEdge */

static float bm_edge_seg_isect(BMEdge *e, CutCurve *c, int len, char mode,
                               struct GHash *gh, int *isected)
{
#define MAXSLOPE 100000
	float  x11, y11, x12 = 0, y12 = 0, x2max, x2min, y2max;
	float  y2min, dist, lastdist = 0, xdiff2, xdiff1;
	float  m1, b1, m2, b2, x21, x22, y21, y22, xi;
	float  yi, x1min, x1max, y1max, y1min, perc = 0;
	float  *scr;
	float  threshold = 0.0;
	int  i;
	
	//threshold = 0.000001; /* tolerance for vertex intersection */
	// XXX	threshold = scene->toolsettings->select_thresh / 100;
	
	/* Get screen coords of verts */
	scr = BLI_ghash_lookup(gh, e->v1);
	x21 = scr[0];
	y21 = scr[1];
	
	scr = BLI_ghash_lookup(gh, e->v2);
	x22 = scr[0];
	y22 = scr[1];
	
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

	/* check for *exact* vertex intersection first */
	if (mode != KNIFE_MULTICUT) {
		for (i = 0; i < len; i++) {
			if (i > 0) {
				x11 = x12;
				y11 = y12;
			}
			else {
				x11 = c[i].x;
				y11 = c[i].y;
			}
			x12 = c[i].x;
			y12 = c[i].y;
			
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
	
	/* now check for edge interesect (may produce vertex intersection as well)*/
	for (i = 0; i < len; i++) {
		if (i > 0) {
			x11 = x12;
			y11 = y12;
		}
		else {
			x11 = c[i].x;
			y11 = c[i].y;
		}
		x12 = c[i].x;
		y12 = c[i].y;
		
		/* Perp. Distance from point to line */
		if (m2 != MAXSLOPE) dist = (y12 - m2 * x12 - b2);/* /sqrt(m2 * m2 + 1); Only looking for */
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
			x2max = MAX2(x21, x22) + 0.001; /* prevent missed edges   */
			x2min = MIN2(x21, x22) - 0.001; /* due to round off error */
			y2max = MAX2(y21, y22) + 0.001;
			y2min = MIN2(y21, y22) - 0.001;
			
			/* Found an intersect,  calc intersect point */
			if (m1 == m2) { /* co-incident lines */
				/* cut at 50% of overlap area */
				x1max = MAX2(x11, x12);
				x1min = MIN2(x11, x12);
				xi = (MIN2(x2max, x1max) + MAX2(x2min, x1min)) / 2.0;
				
				y1max = MAX2(y11, y12);
				y1min = MIN2(y11, y12);
				yi = (MIN2(y2max, y1max) + MAX2(y2min, y1min)) / 2.0;
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
				else perc = (yi - y21) / (y22 - y21); /* lower slope more accurate */
				//isect = 32768.0 * (perc + 0.0000153); /* Percentage in 1 / 32768ths */
				
				break;
			}
		}	
		lastdist = dist;
	}
	return(perc);
} 

#define MAX_CUTS 2048

static int knife_cut_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = (((Mesh *)obedit->data))->edit_btmesh;
	BMesh *bm = em->bm;
	ARegion *ar = CTX_wm_region(C);
	BMVert *bv;
	BMIter iter;
	BMEdge *be;
	BMOperator bmop;
	CutCurve curve[MAX_CUTS];
	struct GHash *gh;
	float isect = 0.0f;
	float  *scr, co[4];
	int len = 0, isected;
	short numcuts = 1, mode = RNA_int_get(op->ptr, "type");
	
	/* edit-object needed for matrix, and ar->regiondata for projections to work */
	if (ELEM3(NULL, obedit, ar, ar->regiondata))
		return OPERATOR_CANCELLED;
	
	if (bm->totvertsel < 2) {
		//error("No edges are selected to operate on");
		return OPERATOR_CANCELLED;
	}

	/* get the cut curve */
	RNA_BEGIN(op->ptr, itemptr, "path") {
		
		RNA_float_get_array(&itemptr, "loc", (float *)&curve[len]);
		len++;
		if (len >= MAX_CUTS) break;
	}
	RNA_END;
	
	if (len < 2) {
		return OPERATOR_CANCELLED;
	}

	/* the floating point coordinates of verts in screen space will be stored in a hash table according to the vertices pointer */
	gh = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "knife cut exec");
	for (bv = BM_iter_new(&iter, bm, BM_VERTS_OF_MESH, NULL); bv; bv = BM_iter_step(&iter)) {
		scr = MEM_mallocN(sizeof(float) * 2, "Vertex Screen Coordinates");
		copy_v3_v3(co, bv->co);
		co[3] = 1.0f;
		mul_m4_v4(obedit->obmat, co);
		project_float(ar, co, scr);
		BLI_ghash_insert(gh, bv, scr);
	}

	if (!EDBM_InitOpf(em, &bmop, op, "esubd")) {
		return OPERATOR_CANCELLED;
	}

	/* store percentage of edge cut for KNIFE_EXACT here.*/
	for (be = BM_iter_new(&iter, bm, BM_EDGES_OF_MESH, NULL); be; be = BM_iter_step(&iter)) {
		if (BM_elem_flag_test(be, BM_ELEM_SELECT)) {
			isect = bm_edge_seg_isect(be, curve, len, mode, gh, &isected);
			
			if (isect != 0.0f) {
				if (mode != KNIFE_MULTICUT && mode != KNIFE_MIDPOINT) {
					BMO_slot_map_float_insert(bm, &bmop,
					                    "edgepercents",
					                    be, isect);

				}
				BMO_elem_flag_enable(bm, be, 1);
			}
			else {
				BMO_elem_flag_disable(bm, be, 1);
			}
		}
		else {
			BMO_elem_flag_disable(bm, be, 1);
		}
	}
	
	BMO_slot_buffer_from_flag(bm, &bmop, "edges", 1, BM_EDGE);

	if (mode == KNIFE_MIDPOINT) numcuts = 1;
	BMO_slot_int_set(&bmop, "numcuts", numcuts);

	BMO_slot_int_set(&bmop, "flag", B_KNIFE);
	BMO_slot_int_set(&bmop, "quadcornertype", SUBD_STRAIGHT_CUT);
	BMO_slot_bool_set(&bmop, "singleedge", FALSE);
	BMO_slot_bool_set(&bmop, "gridfill", FALSE);

	BMO_slot_float_set(&bmop, "radius", 0);
	
	BMO_op_exec(bm, &bmop);
	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}
	
	BLI_ghash_free(gh, NULL, (GHashValFreeFP)MEM_freeN);

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_knife_cut(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	ot->name = "Knife Cut";
	ot->description = "Cut selected edges and faces into parts";
	ot->idname = "MESH_OT_knife_cut";
	
	ot->invoke = WM_gesture_lines_invoke;
	ot->modal = WM_gesture_lines_modal;
	ot->exec = knife_cut_exec;
	
	ot->poll = EM_view3d_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", knife_items, KNIFE_EXACT, "Type", "");
	prop = RNA_def_property(ot->srna, "path", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_runtime(prop, &RNA_OperatorMousePath);
	
	/* internal */
	RNA_def_int(ot->srna, "cursor", BC_KNIFECURSOR, 0, INT_MAX, "Cursor", "", 0, INT_MAX);
}

static int mesh_separate_selected(Main *bmain, Scene *scene, Base *editbase, wmOperator *wmop)
{
	Base *basenew;
	BMIter iter;
	BMVert *v;
	BMEdge *e;
	Object *obedit = editbase->object;
	Mesh *me = obedit->data;
	BMEditMesh *em = me->edit_btmesh;
	BMesh *bm_new;
	
	if (!em)
		return OPERATOR_CANCELLED;
		
	bm_new = BM_mesh_create(obedit, &bm_mesh_allocsize_default);
	CustomData_copy(&em->bm->vdata, &bm_new->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&em->bm->edata, &bm_new->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&em->bm->ldata, &bm_new->ldata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&em->bm->pdata, &bm_new->pdata, CD_MASK_BMESH, CD_CALLOC, 0);

	CustomData_bmesh_init_pool(&bm_new->vdata, bm_mesh_allocsize_default.totvert, BM_VERT);
	CustomData_bmesh_init_pool(&bm_new->edata, bm_mesh_allocsize_default.totedge, BM_EDGE);
	CustomData_bmesh_init_pool(&bm_new->ldata, bm_mesh_allocsize_default.totloop, BM_LOOP);
	CustomData_bmesh_init_pool(&bm_new->pdata, bm_mesh_allocsize_default.totface, BM_FACE);
		
	basenew = ED_object_add_duplicate(bmain, scene, editbase, USER_DUP_MESH);	/* 0 = fully linked */
	assign_matarar(basenew->object, give_matarar(obedit), *give_totcolp(obedit)); /* new in 2.5 */

	ED_base_object_select(basenew, BA_DESELECT);
	
	EDBM_CallOpf(em, wmop, "dupe geom=%hvef dest=%p", BM_ELEM_SELECT, bm_new);
	EDBM_CallOpf(em, wmop, "del geom=%hvef context=%i", BM_ELEM_SELECT, DEL_FACES);

	/* clean up any loose edges */
	BM_ITER(e, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		if (BM_elem_flag_test(e, BM_ELEM_HIDDEN))
			continue;

		if (BM_edge_face_count(e) != 0) {
			BM_elem_select_set(em->bm, e, FALSE);
		}
	}
	EDBM_CallOpf(em, wmop, "del geom=%hvef context=%i", BM_ELEM_SELECT, DEL_EDGES);

	/* clean up any loose verts */
	BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		if (BM_elem_flag_test(v, BM_ELEM_HIDDEN))
			continue;

		if (BM_vert_edge_count(v) != 0) {
			BM_elem_select_set(em->bm, v, FALSE);
		}
	}

	EDBM_CallOpf(em, wmop, "del geom=%hvef context=%i", BM_ELEM_SELECT, DEL_VERTS);

	BM_mesh_normals_update(bm_new, TRUE);
	BMO_op_callf(bm_new, "bmesh_to_mesh mesh=%p object=%p notesselation=%b",
	             basenew->object->data, basenew->object, TRUE);
		
	BM_mesh_free(bm_new);
	((Mesh *)basenew->object->data)->edit_btmesh = NULL;
	
	return 1;
}

//BMESH_TODO
static int mesh_separate_material(Main *UNUSED(bmain), Scene *UNUSED(scene), Base *UNUSED(editbase), wmOperator *UNUSED(wmop))
{
	return 0;
}

static int mesh_separate_loose(Main *bmain, Scene *scene, Base *editbase, wmOperator *wmop)
{
	int i;
	BMVert *v;
	BMEdge *e;
	BMVert *v_seed;
	BMWalker walker;
	BMIter iter;
	int result = 0;
	Object *obedit = editbase->object;
	Mesh *me = obedit->data;
	BMEditMesh *em = me->edit_btmesh;
	BMesh *bm = em->bm;
	int max_iter = bm->totvert;

	/* Clear all selected vertices */
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		BM_elem_select_set(bm, v, FALSE);
	}

	/* Flush the selection to clear edge/face selections to match
	 * selected vertices */
	EDBM_selectmode_flush_ex(em, SCE_SELECT_VERTEX);

	/* A "while (true)" loop should work here as each iteration should
	 * select and remove at least one vertex and when all vertices
	 * are selected the loop will break out. But guard against bad
	 * behavior by limiting iterations to the number of vertices in the
	 * original mesh.*/
	for (i = 0; i < max_iter; i++) {
		/* Get a seed vertex to start the walk */
		v_seed = NULL;
		BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
			v_seed = v;
			break;
		}

		/* No vertices available, can't do anything */
		if (v_seed == NULL) {
			break;
		}

		/* Select the seed explicitly, in case it has no edges */
		BM_elem_select_set(bm, v_seed, TRUE);

		/* Walk from the single vertex, selecting everything connected
		 * to it */
		BMW_init(&walker, bm, BMW_SHELL,
		         BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
		         BMW_NIL_LAY);

		e = BMW_begin(&walker, v_seed);
		for (; e; e = BMW_step(&walker)) {
			BM_elem_select_set(bm, e->v1, TRUE);
			BM_elem_select_set(bm, e->v2, TRUE);
		}
		BMW_end(&walker);
				
		/* Flush the selection to get edge/face selections matching
		 * the vertex selection */
		EDBM_selectmode_flush_ex(em, SCE_SELECT_VERTEX);

		if (bm->totvert == bm->totvertsel) {
			/* Every vertex selected, nothing to separate, work is done */
			break;
		}

		/* Move selection into a separate object */
		result |= mesh_separate_selected(bmain, scene, editbase, wmop);
	}

	return result;
}

static int mesh_separate_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Base *base = CTX_data_active_base(C);
	int retval = 0, type = RNA_enum_get(op->ptr, "type");
	
	if (type == 0)
		retval = mesh_separate_selected(bmain, scene, base, op);
	else if (type == 1)
		retval = mesh_separate_material(bmain, scene, base, op);
	else if (type == 2)
		retval = mesh_separate_loose(bmain, scene, base, op);
	   
	if (retval) {
		DAG_id_tag_update(base->object->data, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_GEOM|ND_DATA, base->object->data);
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
	ot->exec = mesh_separate_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	ot->prop = RNA_def_enum(ot->srna, "type", prop_separate_types, 0, "Type", "");
}


static int fill_mesh_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMOperator bmop;
	
	if (!EDBM_InitOpf(em, &bmop, op, "triangle_fill edges=%he", BM_ELEM_SELECT)) {
		return OPERATOR_CANCELLED;
	}
	
	BMO_op_exec(em->bm, &bmop);
	
	/* select new geometry */
	BMO_slot_buffer_hflag_enable(em->bm, &bmop, "geomout", BM_ELEM_SELECT, BM_FACE|BM_EDGE, TRUE);
	
	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}
	
	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);
	
	return OPERATOR_FINISHED;

}

void MESH_OT_fill(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Fill";
	ot->idname = "MESH_OT_fill";

	/* api callbacks */
	ot->exec = fill_mesh_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int beautify_fill_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;

	if (!EDBM_CallOpf(em, op, "beautify_fill faces=%hf", BM_ELEM_SELECT))
		return OPERATOR_CANCELLED;

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_beautify_fill(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Beautify Fill";
	ot->idname = "MESH_OT_beautify_fill";

	/* api callbacks */
	ot->exec = beautify_fill_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** Quad/Tri Operators *************************/

static int quads_convert_to_tris_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;

	if (!EDBM_CallOpf(em, op, "triangulate faces=%hf", BM_ELEM_SELECT))
		return OPERATOR_CANCELLED;

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_quads_convert_to_tris(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Quads to Tris";
	ot->idname = "MESH_OT_quads_convert_to_tris";

	/* api callbacks */
	ot->exec = quads_convert_to_tris_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int tris_convert_to_quads_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	int dosharp, douvs, dovcols, domaterials;
	float limit = RNA_float_get(op->ptr, "limit");

	dosharp = RNA_boolean_get(op->ptr, "sharp");
	douvs = RNA_boolean_get(op->ptr, "uvs");
	dovcols = RNA_boolean_get(op->ptr, "vcols");
	domaterials = RNA_boolean_get(op->ptr, "materials");

	if (!EDBM_CallOpf(em, op,
	                  "join_triangles faces=%hf limit=%f cmp_sharp=%b cmp_uvs=%b cmp_vcols=%b cmp_materials=%b",
	                  BM_ELEM_SELECT, limit, dosharp, douvs, dovcols, domaterials))
	{
		return OPERATOR_CANCELLED;
	}

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_tris_convert_to_quads(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Tris to Quads";
	ot->idname = "MESH_OT_tris_convert_to_quads";

	/* api callbacks */
	ot->exec = tris_convert_to_quads_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	prop = RNA_def_float_rotation(ot->srna, "limit", 0, NULL, 0.0f, DEG2RADF(180.0f),
	                              "Max Angle", "Angle Limit", 0.0f, DEG2RADF(180.0f));
	RNA_def_property_float_default(prop, DEG2RADF(40.0f));

	RNA_def_boolean(ot->srna, "uvs", 0, "Compare UVs", "");
	RNA_def_boolean(ot->srna, "vcols", 0, "Compare VCols", "");
	RNA_def_boolean(ot->srna, "sharp", 0, "Compare Sharp", "");
	RNA_def_boolean(ot->srna, "materials", 0, "Compare Materials", "");
}

static int dissolve_limited_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	float angle_limit = RNA_float_get(op->ptr, "angle_limit");

	if (!EDBM_CallOpf(em, op,
	                  "dissolve_limit edges=%he verts=%hv angle_limit=%f",
	                  BM_ELEM_SELECT, BM_ELEM_SELECT, angle_limit))
	{
		return OPERATOR_CANCELLED;
	}

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

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
	ot->exec = dissolve_limited_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	prop = RNA_def_float_rotation(ot->srna, "angle_limit", 0, NULL, 0.0f, DEG2RADF(180.0f),
	                              "Max Angle", "Angle Limit in Degrees", 0.0f, DEG2RADF(180.0f));
	RNA_def_property_float_default(prop, DEG2RADF(15.0f));
}

static int split_mesh_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)ob->data)->edit_btmesh;
	BMOperator bmop;

	EDBM_InitOpf(em, &bmop, op, "split geom=%hvef use_only_faces=%b", BM_ELEM_SELECT, FALSE);
	BMO_op_exec(em->bm, &bmop);
	BM_mesh_elem_flag_disable_all(em->bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT);
	BMO_slot_buffer_hflag_enable(em->bm, &bmop, "geomout", BM_ELEM_SELECT, BM_ALL, TRUE);
	if (!EDBM_FinishOp(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	/* Geometry has changed, need to recalc normals and looptris */
	BMEdit_RecalcTesselation(em);
	EDBM_RecalcNormals(em);

	DAG_id_tag_update(ob->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_split(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Split";
	ot->idname = "MESH_OT_split";

	/* api callbacks */
	ot->exec = split_mesh_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}


static int spin_mesh_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMesh *bm = em->bm;
	BMOperator spinop;
	float cent[3], axis[3], imat[3][3];
	float d[3] = {0.0f, 0.0f, 0.0f};
	int steps, dupli;
	float degr;
    
	RNA_float_get_array(op->ptr, "center", cent);
	RNA_float_get_array(op->ptr, "axis", axis);
	steps = RNA_int_get(op->ptr, "steps");
	degr = RNA_float_get(op->ptr, "degrees");
	if (ts->editbutflag & B_CLOCKWISE) degr = -degr;
	dupli = RNA_boolean_get(op->ptr, "dupli");
    
	/* undo object transformation */
	copy_m3_m4(imat, obedit->imat);
	sub_v3_v3(cent, obedit->obmat[3]);
	mul_m3_v3(imat, cent);
	mul_m3_v3(imat, axis);

	if (!EDBM_InitOpf(em, &spinop, op,
	                  "spin geom=%hvef cent=%v axis=%v dvec=%v steps=%i ang=%f do_dupli=%b",
	                  BM_ELEM_SELECT, cent, axis, d, steps, degr, dupli))
	{
		return OPERATOR_CANCELLED;
	}
	BMO_op_exec(bm, &spinop);
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	BMO_slot_buffer_hflag_enable(bm, &spinop, "lastout", BM_ELEM_SELECT, BM_ALL, TRUE);
	if (!EDBM_FinishOp(em, &spinop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

/* get center and axis, in global coords */
static int spin_mesh_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);

	RNA_float_set_array(op->ptr, "center", give_cursor(scene, v3d));
	RNA_float_set_array(op->ptr, "axis", rv3d->viewinv[2]);

	return spin_mesh_exec(C, op);
}

void MESH_OT_spin(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Spin";
	ot->idname = "MESH_OT_spin";

	/* api callbacks */
	ot->invoke = spin_mesh_invoke;
	ot->exec = spin_mesh_exec;
	ot->poll = EM_view3d_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* props */
	RNA_def_int(ot->srna, "steps", 9, 0, INT_MAX, "Steps", "Steps", 0, INT_MAX);
	RNA_def_boolean(ot->srna, "dupli", 0, "Dupli", "Make Duplicates");
	RNA_def_float(ot->srna, "degrees", 90.0f, -FLT_MAX, FLT_MAX, "Degrees", "Degrees", -360.0f, 360.0f);

	RNA_def_float_vector(ot->srna, "center", 3, NULL, -FLT_MAX, FLT_MAX, "Center", "Center in global view space", -FLT_MAX, FLT_MAX);
	RNA_def_float_vector(ot->srna, "axis", 3, NULL, -1.0f, 1.0f, "Axis", "Axis in global view space", -FLT_MAX, FLT_MAX);

}

static int screw_mesh_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMesh *bm = em->bm;
	BMEdge *eed;
	BMVert *eve, *v1, *v2;
	BMIter iter, eiter;
	BMOperator spinop;
	float dvec[3], nor[3], cent[3], axis[3];
	float imat[3][3];
	int steps, turns;
	int valence;


	turns = RNA_int_get(op->ptr, "turns");
	steps = RNA_int_get(op->ptr, "steps");
	RNA_float_get_array(op->ptr, "center", cent);
	RNA_float_get_array(op->ptr, "axis", axis);

	/* undo object transformation */
	copy_m3_m4(imat, obedit->imat);
	sub_v3_v3(cent, obedit->obmat[3]);
	mul_m3_v3(imat, cent);
	mul_m3_v3(imat, axis);


	/* find two vertices with valence count == 1, more or less is wrong */
	v1 = NULL;
	v2 = NULL;
	for (eve = BM_iter_new(&iter, em->bm, BM_VERTS_OF_MESH, NULL);
	    eve; eve = BM_iter_step(&iter)) {

		valence = 0;

		for (eed = BM_iter_new(&eiter, em->bm, BM_EDGES_OF_VERT, eve);
		    eed; eed = BM_iter_step(&eiter)) {

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
		BKE_report(op->reports, RPT_ERROR, "You have to select a string of connected vertices too");
		return OPERATOR_CANCELLED;
	}

	/* calculate dvec */
	sub_v3_v3v3(dvec, v1->co, v2->co);
	mul_v3_fl(dvec, 1.0f / steps);

	if (dot_v3v3(nor, dvec) > 0.000f)
		negate_v3(dvec);

	if (!EDBM_InitOpf(em, &spinop, op,
	                  "spin geom=%hvef cent=%v axis=%v dvec=%v steps=%i ang=%f do_dupli=%b",
	                  BM_ELEM_SELECT, cent, axis, dvec, turns * steps, 360.0f * turns, FALSE))
	{
		return OPERATOR_CANCELLED;
	}
	BMO_op_exec(bm, &spinop);
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	BMO_slot_buffer_hflag_enable(bm, &spinop, "lastout", BM_ELEM_SELECT, BM_ALL, TRUE);
	if (!EDBM_FinishOp(em, &spinop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

/* get center and axis, in global coords */
static int screw_mesh_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);

	RNA_float_set_array(op->ptr, "center", give_cursor(scene, v3d));
	RNA_float_set_array(op->ptr, "axis", rv3d->viewinv[1]);

	return screw_mesh_exec(C, op);
}

void MESH_OT_screw(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Screw";
	ot->idname = "MESH_OT_screw";

	/* api callbacks */
	ot->invoke = screw_mesh_invoke;
	ot->exec = screw_mesh_exec;
	ot->poll = EM_view3d_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* props */
	RNA_def_int(ot->srna, "steps", 9, 0, INT_MAX, "Steps", "Steps", 0, 256);
	RNA_def_int(ot->srna, "turns", 1, 0, INT_MAX, "Turns", "Turns", 0, 256);

	RNA_def_float_vector(ot->srna, "center", 3, NULL, -FLT_MAX, FLT_MAX,
	                     "Center", "Center in global view space", -FLT_MAX, FLT_MAX);
	RNA_def_float_vector(ot->srna, "axis", 3, NULL, -1.0f, 1.0f,
	                     "Axis", "Axis in global view space", -FLT_MAX, FLT_MAX);
}

static int select_by_number_vertices_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMIter iter;
	int numverts = RNA_int_get(op->ptr, "number");
	int type = RNA_enum_get(op->ptr, "type");

	for (efa = BM_iter_new(&iter, em->bm, BM_FACES_OF_MESH, NULL);
	    efa; efa = BM_iter_step(&iter)) {

		int select = 0;

		if (type == 0 && efa->len < numverts) {
			select = 1;
		}else if (type == 1 && efa->len == numverts) {
			select = 1;
		}else if (type == 2 && efa->len > numverts) {
			select = 1;
		}

		if (select) {
			BM_elem_select_set(em->bm, efa, TRUE);
		}
	}

	EDBM_selectmode_flush(em);

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_by_number_vertices(wmOperatorType *ot)
{
	static const EnumPropertyItem type_items[] = {
	    {0, "LESS", 0, "Less Than", ""},
	    {1, "EQUAL", 0, "Equal To", ""},
	    {2, "GREATER", 0, "Greater Than", ""},
	    {0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Select by Number of Vertices";
	ot->description = "Select vertices or faces by vertex count";
	ot->idname = "MESH_OT_select_by_number_vertices";
	
	/* api callbacks */
	ot->exec = select_by_number_vertices_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_int(ot->srna, "number", 4, 3, INT_MAX, "Number of Vertices", "", 3, INT_MAX);
	RNA_def_enum(ot->srna, "type", type_items, 1, "Type", "Type of comparison to make");
}

static int select_loose_verts_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	BMVert *eve;
	BMEdge *eed;
	BMIter iter;

	for (eve = BM_iter_new(&iter, em->bm, BM_VERTS_OF_MESH, NULL);
	    eve; eve = BM_iter_step(&iter)) {

		if (!eve->e) {
			BM_elem_select_set(em->bm, eve, TRUE);
		}
	}

	for (eed = BM_iter_new(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
	    eed; eed = BM_iter_step(&iter)) {

		if (!eed->l) {
			BM_elem_select_set(em->bm, eed, TRUE);
		}
	}

	EDBM_selectmode_flush(em);

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_loose_verts(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Loose Vertices/Edges";
	ot->description = "Select vertices with no edges nor faces, and edges with no faces";
	ot->idname = "MESH_OT_select_loose_verts";

	/* api callbacks */
	ot->exec = select_loose_verts_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int select_mirror_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	int extend = RNA_boolean_get(op->ptr, "extend");

	EDBM_select_mirrored(obedit, em, extend);
	EDBM_selectmode_flush(em);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_mirror(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Mirror";
	ot->description = "Select mesh items at mirrored locations";
	ot->idname = "MESH_OT_select_mirror";

	/* api callbacks */
	ot->exec = select_mirror_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* props */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the existing selection");
}

#if 0 /* UNUSED */
/********* qsort routines.  not sure how to make these
           work, since we aren't using linked lists for
           geometry anymore.  might need a sortof "swap"
           function for bmesh elements.           *********/

typedef struct xvertsort {
	float x;
	BMVert *v1;
} xvertsort;


static int vergxco(const void *v1, const void *v2)
{
	const xvertsort *x1 = v1, *x2 = v2;

	if (x1->x > x2->x)       return  1;
	else if (x1->x < x2->x)  return -1;
	return 0;
}

struct facesort {
	uintptr_t x;
	struct EditFace *efa;
};

static int vergface(const void *v1, const void *v2)
{
	const struct facesort *x1 = v1, *x2 = v2;

	if (x1->x > x2->x)       return  1;
	else if (x1->x < x2->x)  return -1;
	return 0;
}
#endif

// XXX is this needed?
/* called from buttons */
#if 0 /* UNUSED */
static void xsortvert_flag__doSetX(void *userData, EditVert *UNUSED(eve), int x, int UNUSED(y), int index)
{
	xvertsort *sortblock = userData;

	sortblock[index].x = x;
}
#endif

/* all verts with (flag & 'flag') are sorted */
static void xsortvert_flag(bContext *UNUSED(C), int UNUSED(flag))
{
	/* BMESH_TODO */
#if 0 //hrm, geometry isn't in linked lists anymore. . .
	ViewContext vc;
	BMEditMesh *em;
	BMVert *eve;
	BMIter iter;
	xvertsort *sortblock;
	ListBase tbase;
	int i, amount;

	em_setup_viewcontext(C, &vc);
	em = vc.em;

	amount = em->bm->totvert;
	sortblock = MEM_callocN(sizeof(xvertsort) * amount,"xsort");
	BM_ITER(eve, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		if (BM_elem_flag_test(eve, BM_ELEM_SELECT))
			sortblock[i].v1 = eve;
	}
	
	ED_view3d_init_mats_rv3d(vc.obedit, vc.rv3d);
	mesh_foreachScreenVert(&vc, xsortvert_flag__doSetX, sortblock, V3D_CLIP_TEST_OFF);

	qsort(sortblock, amount, sizeof(xvertsort), vergxco);

		/* make temporal listbase */
	tbase.first = tbase.last = 0;
	for (i = 0; i < amount; i++) {
		eve = sortblock[i].v1;

		if (eve) {
			BLI_remlink(&vc.em->verts, eve);
			BLI_addtail(&tbase, eve);
		}
	}

	BLI_movelisttolist(&vc.em->verts, &tbase);

	MEM_freeN(sortblock);
#endif

}

static int mesh_vertices_sort_exec(bContext *C, wmOperator *UNUSED(op))
{
	xsortvert_flag(C, SELECT);
	return OPERATOR_FINISHED;
}

void MESH_OT_vertices_sort(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Vertex Sort";
	ot->description = "Sort vertex order";
	ot->idname = "MESH_OT_vertices_sort";

	/* api callbacks */
	ot->exec = mesh_vertices_sort_exec;

	ot->poll = EM_view3d_poll; /* uses view relative X axis to sort verts */

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ********************** SORT FACES ******************* */

static void permutate(void *list, int num, int size, int *index)
{
	void *buf;
	int len;
	int i;

	len = num * size;

	buf = MEM_mallocN(len, "permutate");
	memcpy(buf, list, len);
	
	for (i = 0; i < num; i++) {
		memcpy((char *)list + (i * size), (char *)buf + (index[i] * size), size);
	}
	MEM_freeN(buf);
}

/* sort faces on view axis */
static float *face_sort_floats;
static int float_sort(const void *v1, const void *v2)
{
	float x1, x2;
	
	x1 = face_sort_floats[((int *) v1)[0]];
	x2 = face_sort_floats[((int *) v2)[0]];
	
	if (x1 > x2)       return  1;
	else if (x1 < x2)  return -1;
	return 0;
}

static int sort_faces_exec(bContext *C, wmOperator *op)
{
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);
	View3D *v3d = CTX_wm_view3d(C);
	Object *ob = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	Mesh *me;
	CustomDataLayer *layer;
	int i, j, *index;
	int event;
	float reverse = 1;
	// XXX int ctrl = 0;
	
	if (!v3d) return OPERATOR_CANCELLED;

	/* This operator work in Object Mode, not in edit mode.
	 * After talk with Campbell we agree that there is no point to port this to EditMesh right now.
	 * so for now, we just exit_editmode and enter_editmode at the end of this function.
	 */
	ED_object_exit_editmode(C, EM_FREEDATA);

	me = ob->data;
	if (me->totpoly == 0) {
		ED_object_enter_editmode(C, 0);
		return OPERATOR_FINISHED;
	}

	event = RNA_enum_get(op->ptr, "type");

	// XXX
	//if (ctrl)
	//	reverse = -1;
	
	/* create index list */
	index = (int *)MEM_mallocN(sizeof(int) * me->totpoly, "sort faces");
	for (i = 0; i < me->totpoly; i++) {
		index[i] = i;
	}
	
	face_sort_floats = (float *) MEM_mallocN(sizeof(float) * me->totpoly, "sort faces float");

	/* sort index list instead of faces itself 
	 * and apply this permutation to all face layers
	 */
	if (event == 5) {
		/* Random */
		for (i = 0; i < me->totpoly; i++) {
			face_sort_floats[i] = BLI_frand();
		}
		qsort(index, me->totpoly, sizeof(int), float_sort);
	}
	else {
		MPoly *mp;
		MLoop *ml;
		MVert *mv;
		float vec[3];
		float mat[4][4];
		float cur[3];
		
		if (event == 1)
			mult_m4_m4m4(mat, rv3d->viewmat, OBACT->obmat); /* apply the view matrix to the object matrix */
		else if (event == 2) { /* sort from cursor */
			if (v3d && v3d->localvd) {
				copy_v3_v3(cur, v3d->cursor);
			}
			else {
				copy_v3_v3(cur, scene->cursor);
			}
			invert_m4_m4(mat, OBACT->obmat);
			mul_m4_v3(mat, cur);
		}
		
		mp = me->mpoly;

		for (i = 0; i < me->totpoly; i++, mp++) {
			if (event == 3) {
				face_sort_floats[i] = ((float)mp->mat_nr)*reverse;
			}
			else if (event == 4) {
				/* selected first */
				if (mp->flag & ME_FACE_SEL)
					face_sort_floats[i] = 0.0;
				else
					face_sort_floats[i] = reverse;
			}
			else {
				/* find the face's center */
				ml = me->mloop + mp->loopstart;
				zero_v3(vec);
				for (j = 0; j < mp->totloop; j++, ml++) {
					mv = me->mvert + ml->v;
					add_v3_v3(vec, mv->co);
				}
				mul_v3_fl(vec, 1.0f / (float)mp->totloop);
				
				if (event == 1) { /* sort on view axis */
					mul_m4_v3(mat, vec);
					face_sort_floats[i] = vec[2] * reverse;
				}
				else if (event == 2) { /* distance from cursor */
					face_sort_floats[i] = len_v3v3(cur, vec) * reverse; /* back to front */
				}
			}
		}
		qsort(index, me->totpoly, sizeof(int), float_sort);
	}
	
	MEM_freeN(face_sort_floats);
	for (i = 0; i < me->pdata.totlayer; i++) {
		layer = &me->pdata.layers[i];
		permutate(layer->data, me->totpoly, CustomData_sizeof(layer->type), index);
	}

	MEM_freeN(index);
	DAG_id_tag_update(ob->data, 0);

	/* Return to editmode. */
	ED_object_enter_editmode(C, 0);

	return OPERATOR_FINISHED;
}

void MESH_OT_sort_faces(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[] = {
		{ 1, "VIEW_AXIS", 0, "View Axis", "" },
		{ 2, "CURSOR_DISTANCE", 0, "Cursor Distance", "" },
		{ 3, "MATERIAL", 0, "Material", "" },
		{ 4, "SELECTED", 0, "Selected", "" },
		{ 5, "RANDOMIZE", 0, "Randomize", "" },
		{ 0, NULL, 0, NULL, NULL }};

	/* identifiers */
	ot->name = "Sort Faces"; // XXX (Ctrl to reverse)%t|
	ot->description = "The faces of the active Mesh Object are sorted, based on the current view";
	ot->idname = "MESH_OT_sort_faces";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = sort_faces_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "");
}

#if 0
/* called from buttons */
static void hashvert_flag(EditMesh *em, int flag)
{
	/* switch vertex order using hash table */
	EditVert *eve;
	struct xvertsort *sortblock, *sb, onth, *newsort;
	ListBase tbase;
	int amount, a, b;

	/* count */
	eve = em->verts.first;
	amount = 0;
	while (eve) {
		if (eve->f & flag) amount++;
		eve = eve->next;
	}
	if (amount == 0) return;

	/* allocate memory */
	sb = sortblock = (struct xvertsort *)MEM_mallocN(sizeof(struct xvertsort)*amount,"sortremovedoub");
	eve = em->verts.first;
	while (eve) {
		if (eve->f & flag) {
			sb->v1 = eve;
			sb++;
		}
		eve = eve->next;
	}

	BLI_srand(1);

	sb = sortblock;
	for (a = 0; a < amount; a++, sb++) {
		b = (int)(amount * BLI_drand());
		if (b >= 0 && b < amount) {
			newsort = sortblock + b;
			onth = *sb;
			*sb = *newsort;
			*newsort = onth;
		}
	}

	/* make temporal listbase */
	tbase.first = tbase.last = 0;
	sb = sortblock;
	while (amount--) {
		eve = sb->v1;
		BLI_remlink(&em->verts, eve);
		BLI_addtail(&tbase, eve);
		sb++;
	}

	BLI_movelisttolist(&em->verts, &tbase);

	MEM_freeN(sortblock);

}
#endif

static int mesh_vertices_randomize_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
#if 1 /* BMESH TODO */
	(void)em;
#else
	hashvert_flag(em, SELECT);
#endif
	return OPERATOR_FINISHED;
}

void MESH_OT_vertices_randomize(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Vertex Randomize";
	ot->description = "Randomize vertex order";
	ot->idname = "MESH_OT_vertices_randomize";

	/* api callbacks */
	ot->exec = mesh_vertices_randomize_exec;

	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/******end of qsort stuff ****/


static int mesh_noise_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = (((Mesh *)obedit->data))->edit_btmesh;
	Material *ma;
	Tex *tex;
	BMVert *eve;
	BMIter iter;
	float fac = RNA_float_get(op->ptr, "factor");

	if (em == NULL) return OPERATOR_FINISHED;

	ma = give_current_material(obedit, obedit->actcol);
	if (ma == 0 || ma->mtex[0] == 0 || ma->mtex[0]->tex == 0) {
		BKE_report(op->reports, RPT_WARNING, "Mesh has no material or texture assigned");
		return OPERATOR_FINISHED;
	}
	tex = give_current_material_texture(ma);

	if (tex->type == TEX_STUCCI) {
		float b2, vec[3];
		float ofs = tex->turbul / 200.0;
		BM_ITER(eve, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			if (BM_elem_flag_test(eve, BM_ELEM_SELECT) && !BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
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
		BM_ITER(eve, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			if (BM_elem_flag_test(eve, BM_ELEM_SELECT) && !BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
				float tin, dum;
				externtex(ma->mtex[0], eve->co, &tin, &dum, &dum, &dum, &dum, 0);
				eve->co[2] += fac * tin;
			}
		}
	}

	EDBM_RecalcNormals(em);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_noise(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Noise";
	ot->description = "Use vertex coordinate as texture coordinate";
	ot->idname = "MESH_OT_noise";

	/* api callbacks */
	ot->exec = mesh_noise_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_float(ot->srna, "factor", 0.1f, -FLT_MAX, FLT_MAX, "Factor", "", 0.0f, 1.0f);
}

/* bevel! yay!!*/
static int mesh_bevel_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = (((Mesh *)obedit->data))->edit_btmesh;
	BMIter iter;
	BMEdge *eed;
	BMOperator bmop;
	float factor = RNA_float_get(op->ptr, "percent"), fac = factor /*, dfac */ /* UNUSED */, df, s;
	int i, recursion = RNA_int_get(op->ptr, "recursion");
	const int use_even = RNA_boolean_get(op->ptr, "use_even");
	const int use_dist = RNA_boolean_get(op->ptr, "use_dist");
	float *w = NULL, ftot;
	int li;
	
	BM_data_layer_add(em->bm, &em->bm->edata, CD_PROP_FLT);
	li = CustomData_number_of_layers(&em->bm->edata, CD_PROP_FLT) - 1;
	
	BM_ITER(eed, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		float d = len_v3v3(eed->v1->co, eed->v2->co);
		float *dv = CustomData_bmesh_get_n(&em->bm->edata, eed->head.data, CD_PROP_FLT, li);
		
		*dv = d;
	}
	
	if (em == NULL) {
		return OPERATOR_CANCELLED;
	}
	
	w = MEM_mallocN(sizeof(float) * recursion, "bevel weights");

	/* ugh, stupid math depends somewhat on angles!*/
	/* dfac = 1.0/(float)(recursion + 1); */ /* UNUSED */
	df = 1.0;
	for (i = 0, ftot = 0.0f; i < recursion; i++) {
		s = powf(df, 1.25f);

		w[i] = s;
		ftot += s;

		df *= 2.0;
	}

	mul_vn_fl(w, recursion, 1.0f / (float)ftot);

	fac = factor;
	for (i = 0; i < recursion; i++) {
		fac = w[recursion - i - 1] * factor;

		if (!EDBM_InitOpf(em, &bmop, op,
		                  "bevel geom=%hev percent=%f lengthlayer=%i use_lengths=%b use_even=%b use_dist=%b",
		                  BM_ELEM_SELECT, fac, li, TRUE, use_even, use_dist))
		{
			return OPERATOR_CANCELLED;
		}
		
		BMO_op_exec(em->bm, &bmop);
		if (!EDBM_FinishOp(em, &bmop, op, TRUE))
			return OPERATOR_CANCELLED;
	}
	
	BM_data_layer_free_n(em->bm, &em->bm->edata, CD_PROP_FLT, li);
	
	MEM_freeN(w);

	EDBM_RecalcNormals(em);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_bevel(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Bevel";
	ot->description = "Edge/Vertex Bevel";
	ot->idname = "MESH_OT_bevel";

	/* api callbacks */
	ot->exec = mesh_bevel_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_float(ot->srna, "percent", 0.5f, -FLT_MAX, FLT_MAX, "Percentage", "", 0.0f, 1.0f);
	RNA_def_int(ot->srna, "recursion", 1, 1, 50, "Recursion Level", "Recursion Level", 1, 8);

	RNA_def_boolean(ot->srna, "use_even", FALSE, "Even",     "Calculate evenly spaced bevel");
	RNA_def_boolean(ot->srna, "use_dist", FALSE, "Distance", "Interpret the percent in blender units");

}

static int bridge_edge_loops(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh *)obedit->data)->edit_btmesh;
	
	if (!EDBM_CallOpf(em, op, "bridge_loops edges=%he", BM_ELEM_SELECT))
		return OPERATOR_CANCELLED;
	
	DAG_id_tag_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_bridge_edge_loops(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Bridge edge loops";
	ot->description = "Make faces between two edge loops";
	ot->idname = "MESH_OT_bridge_edge_loops";
	
	/* api callbacks */
	ot->exec = bridge_edge_loops;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "inside", 0, "Inside", "");
}
