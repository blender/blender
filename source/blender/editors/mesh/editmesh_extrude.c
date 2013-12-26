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

/** \file blender/editors/mesh/editmesh_extrude.c
 *  \ingroup edmesh
 */

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_editmesh.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "mesh_intern.h"  /* own include */

/* allow accumulated normals to form a new direction but don't
 * accept direct opposite directions else they will cancel each other out */
static void add_normal_aligned(float nor[3], const float add[3])
{
	if (dot_v3v3(nor, add) < -0.9999f) {
		sub_v3_v3(nor, add);
	}
	else {
		add_v3_v3(nor, add);
	}
}

/* individual face extrude */
/* will use vertex normals for extrusion directions, so *nor is unaffected */
static short edbm_extrude_discrete_faces(BMEditMesh *em, wmOperator *op, const char hflag, float *UNUSED(nor))
{
	BMOIter siter;
	BMIter liter;
	BMFace *f;
	BMLoop *l;
	BMOperator bmop;

	EDBM_op_init(em, &bmop, op, "extrude_discrete_faces faces=%hf", hflag);

	/* deselect original verts */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	BMO_op_exec(em->bm, &bmop);
	
	BMO_ITER (f, &siter, bmop.slots_out, "faces.out", BM_FACE) {
		BM_face_select_set(em->bm, f, true);

		/* set face vertex normals to face normal */
		BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
			copy_v3_v3(l->v->no, f->no);
		}
	}

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return 0;
	}

	return 's';  /* s is shrink/fatten */
}

/* extrudes individual edges */
static short edbm_extrude_edges_indiv(BMEditMesh *em, wmOperator *op, const char hflag, float *UNUSED(nor))
{
	BMOperator bmop;

	EDBM_op_init(em, &bmop, op, "extrude_edge_only edges=%he", hflag);

	/* deselect original verts */
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	BMO_op_exec(em->bm, &bmop);
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "geom.out", BM_VERT | BM_EDGE, BM_ELEM_SELECT, true);

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return 0;
	}

	return 'n';  /* n is normal grab */
}

/* extrudes individual vertices */
static short edbm_extrude_verts_indiv(BMEditMesh *em, wmOperator *op, const char hflag, float *UNUSED(nor))
{
	BMOperator bmop;

	EDBM_op_init(em, &bmop, op, "extrude_vert_indiv verts=%hv", hflag);

	/* deselect original verts */
	BMO_slot_buffer_hflag_disable(em->bm, bmop.slots_in, "verts", BM_VERT, BM_ELEM_SELECT, true);

	BMO_op_exec(em->bm, &bmop);
	BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "verts.out", BM_VERT, BM_ELEM_SELECT, true);

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return 0;
	}

	return 'g';  /* g is grab */
}

static short edbm_extrude_edge(Object *obedit, BMEditMesh *em, const char hflag, float nor[3])
{
	BMesh *bm = em->bm;
	BMIter iter;
	BMOIter siter;
	BMOperator extop;
	BMEdge *edge;
	BMFace *f;
	ModifierData *md;
	BMElem *ele;
	BMOpSlot *slot_edges_exclude;
	
	BMO_op_init(bm, &extop, BMO_FLAG_DEFAULTS, "extrude_face_region");
	BMO_slot_buffer_from_enabled_hflag(bm, &extop, extop.slots_in, "geom", BM_VERT | BM_EDGE | BM_FACE, hflag);

	slot_edges_exclude = BMO_slot_get(extop.slots_in, "edges_exclude");

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
					mul_m4_m4m4(mtx, imtx, obedit->obmat);
				}

				BM_ITER_MESH (edge, &iter, bm, BM_EDGES_OF_MESH) {
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
								BMO_slot_map_empty_insert(&extop, slot_edges_exclude, edge);
							}
						}
						if (mmd->flag & MOD_MIR_AXIS_Y) {
							if ((fabsf(co1[1]) < mmd->tolerance) &&
							    (fabsf(co2[1]) < mmd->tolerance))
							{
								BMO_slot_map_empty_insert(&extop, slot_edges_exclude, edge);
							}
						}
						if (mmd->flag & MOD_MIR_AXIS_Z) {
							if ((fabsf(co1[2]) < mmd->tolerance) &&
							    (fabsf(co2[2]) < mmd->tolerance))
							{
								BMO_slot_map_empty_insert(&extop, slot_edges_exclude, edge);
							}
						}
					}
				}
			}
		}
	}

	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	BMO_op_exec(bm, &extop);

	zero_v3(nor);
	
	BMO_ITER (ele, &siter, extop.slots_out, "geom.out", BM_ALL) {
		BM_elem_select_set(bm, ele, true);

		if (ele->head.htype == BM_FACE) {
			f = (BMFace *)ele;
			add_normal_aligned(nor, f->no);
		}
	}

	normalize_v3(nor);

	BMO_op_finish(bm, &extop);

	/* grab / normal constraint */
	return is_zero_v3(nor) ? 'g' : 'n';
}

static short edbm_extrude_vert(Object *obedit, BMEditMesh *em, const char hflag, float nor[3])
{
	BMIter iter;
	BMEdge *eed;
		
	/* ensure vert flags are consistent for edge selections */
	BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(eed, hflag)) {
			if (hflag & BM_ELEM_SELECT) {
				BM_vert_select_set(em->bm, eed->v1, true);
				BM_vert_select_set(em->bm, eed->v2, true);
			}

			BM_elem_flag_enable(eed->v1, hflag & ~BM_ELEM_SELECT);
			BM_elem_flag_enable(eed->v2, hflag & ~BM_ELEM_SELECT);
		}
		else {
			if (BM_elem_flag_test(eed->v1, hflag) && BM_elem_flag_test(eed->v2, hflag)) {
				if (hflag & BM_ELEM_SELECT) {
					BM_edge_select_set(em->bm, eed, true);
				}

				BM_elem_flag_enable(eed, hflag & ~BM_ELEM_SELECT);
			}
		}
	}

	return edbm_extrude_edge(obedit, em, hflag, nor);
}

static int edbm_extrude_repeat_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
		
	const int steps = RNA_int_get(op->ptr, "steps");
	
	const float offs = RNA_float_get(op->ptr, "offset");
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
		edbm_extrude_edge(obedit, em, BM_ELEM_SELECT, nor);
		//BMO_op_callf(em->bm, BMO_FLAG_DEFAULTS, "extrude_face_region geom=%hef", BM_ELEM_SELECT);
		BMO_op_callf(em->bm, BMO_FLAG_DEFAULTS,
		             "translate vec=%v verts=%hv",
		             dvec, BM_ELEM_SELECT);
		//extrudeflag(obedit, em, SELECT, nor);
		//translateflag(em, SELECT, dvec);
	}
	
	EDBM_mesh_normals_update(em);

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_extrude_repeat(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Extrude Repeat Mesh";
	ot->description = "Extrude selected vertices, edges or faces repeatedly";
	ot->idname = "MESH_OT_extrude_repeat";
	
	/* api callbacks */
	ot->exec = edbm_extrude_repeat_exec;
	ot->poll = ED_operator_editmesh_view3d;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_float(ot->srna, "offset", 2.0f, 0.0f, FLT_MAX, "Offset", "", 0.0f, 100.0f);
	RNA_def_int(ot->srna, "steps", 10, 0, INT_MAX, "Steps", "", 0, 180);
}

/* generic extern called extruder */
static int edbm_extrude_mesh(Scene *scene, Object *obedit, BMEditMesh *em, wmOperator *op, float *norin)
{
	short nr, transmode = 0;
	float stacknor[3] = {0.0f, 0.0f, 0.0f};
	float *nor = norin ? norin : stacknor;

	zero_v3(nor);

	if (em->selectmode & SCE_SELECT_VERTEX) {
		if (em->bm->totvertsel == 0) nr = 0;
		else if (em->bm->totvertsel == 1) nr = 4;
		else if (em->bm->totedgesel == 0) nr = 4;
		else if (em->bm->totfacesel == 0)
			nr = 3;
		else if (em->bm->totfacesel == 1)
			nr = 1;
		else
			nr = 1;
	}
	else if (em->selectmode & SCE_SELECT_EDGE) {
		if (em->bm->totedgesel == 0) nr = 0;
		
		nr = 1;
	}
	else {
		if (em->bm->totfacesel == 0) nr = 0;
		else if (em->bm->totfacesel == 1) nr = 1;
		else
			nr = 1;
	}

	if (nr < 1) return 'g';

	if (nr == 1 && (em->selectmode & SCE_SELECT_VERTEX))
		transmode = edbm_extrude_vert(obedit, em, BM_ELEM_SELECT, nor);
	else if (nr == 1) transmode = edbm_extrude_edge(obedit, em, BM_ELEM_SELECT, nor);
	else if (nr == 4) transmode = edbm_extrude_verts_indiv(em, op, BM_ELEM_SELECT, nor);
	else if (nr == 3) transmode = edbm_extrude_edges_indiv(em, op, BM_ELEM_SELECT, nor);
	else transmode = edbm_extrude_discrete_faces(em, op, BM_ELEM_SELECT, nor);
	
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
		BKE_object_handle_update(G.main->eval_ctx, scene, obedit);

		/* individual faces? */
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
static int edbm_extrude_region_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	
	edbm_extrude_mesh(scene, obedit, em, op, NULL);

	/* This normally happens when pushing undo but modal operators
	 * like this one don't push undo data until after modal mode is
	 * done.*/
	EDBM_mesh_normals_update(em);

	EDBM_update_generic(em, true, true);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_extrude_region(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Extrude Region";
	ot->idname = "MESH_OT_extrude_region";
	ot->description = "Extrude region of faces";
	
	/* api callbacks */
	//ot->invoke = mesh_extrude_region_invoke;
	ot->exec = edbm_extrude_region_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	Transform_Properties(ot, P_NO_DEFAULTS | P_MIRROR_DUMMY);
}

static int edbm_extrude_verts_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	float nor[3];

	edbm_extrude_verts_indiv(em, op, BM_ELEM_SELECT, nor);
	
	EDBM_update_generic(em, true, true);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_extrude_verts_indiv(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Extrude Only Vertices";
	ot->idname = "MESH_OT_extrude_verts_indiv";
	ot->description = "Extrude individual vertices only";
	
	/* api callbacks */
	ot->exec = edbm_extrude_verts_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* to give to transform */
	Transform_Properties(ot, P_NO_DEFAULTS | P_MIRROR_DUMMY);
}

static int edbm_extrude_edges_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	float nor[3];

	edbm_extrude_edges_indiv(em, op, BM_ELEM_SELECT, nor);
	
	EDBM_update_generic(em, true, true);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_extrude_edges_indiv(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Extrude Only Edges";
	ot->idname = "MESH_OT_extrude_edges_indiv";
	ot->description = "Extrude individual edges only";
	
	/* api callbacks */
	ot->exec = edbm_extrude_edges_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* to give to transform */
	Transform_Properties(ot, P_NO_DEFAULTS | P_MIRROR_DUMMY);
}

static int edbm_extrude_faces_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	float nor[3];

	edbm_extrude_discrete_faces(em, op, BM_ELEM_SELECT, nor);
	
	EDBM_update_generic(em, true, true);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_extrude_faces_indiv(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Extrude Individual Faces";
	ot->idname = "MESH_OT_extrude_faces_indiv";
	ot->description = "Extrude individual faces only";
	
	/* api callbacks */
	ot->exec = edbm_extrude_faces_exec;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	Transform_Properties(ot, P_NO_DEFAULTS | P_MIRROR_DUMMY);
}

/* *************** add-click-mesh (extrude) operator ************** */
static int edbm_dupli_extrude_cursor_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ViewContext vc;
	BMVert *v1;
	BMIter iter;
	float min[3], max[3];
	bool done = false;
	bool use_proj;
	
	em_setup_viewcontext(C, &vc);

	ED_view3d_init_mats_rv3d(vc.obedit, vc.rv3d);


	use_proj = ((vc.scene->toolsettings->snap_flag & SCE_SNAP) &&
	            (vc.scene->toolsettings->snap_mode == SCE_SNAP_MODE_FACE));

	INIT_MINMAX(min, max);
	
	BM_ITER_MESH (v1, &iter, vc.em->bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(v1, BM_ELEM_SELECT)) {
			minmax_v3v3_v3(min, max, v1->co);
			done = true;
		}
	}

	/* call extrude? */
	if (done) {
		const bool rot_src = RNA_boolean_get(op->ptr, "rotate_source");
		BMEdge *eed;
		float vec[3], cent[3], mat[3][3];
		float nor[3] = {0.0, 0.0, 0.0};

		/* 2D normal calc */
		const float mval_f[2] = {(float)event->mval[0],
		                         (float)event->mval[1]};

		/* check for edges that are half selected, use for rotation */
		done = false;
		BM_ITER_MESH (eed, &iter, vc.em->bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
				float co1[2], co2[2];

				if ((ED_view3d_project_float_object(vc.ar, eed->v1->co, co1, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) &&
				    (ED_view3d_project_float_object(vc.ar, eed->v2->co, co2, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK))
				{
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
					done = true;
				}
			}
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

		mul_m4_v3(vc.obedit->obmat, min);  /* view space */
		ED_view3d_win_to_3d_int(vc.ar, min, event->mval, min);
		mul_m4_v3(vc.obedit->imat, min); // back in object space

		sub_v3_v3(min, cent);
		
		/* calculate rotation */
		unit_m3(mat);
		if (done) {
			float angle;

			normalize_v3_v3(vec, min);

			angle = angle_normalized_v3v3(vec, nor);

			if (angle != 0.0f) {
				float axis[3];

				cross_v3_v3v3(axis, nor, vec);

				/* halve the rotation if its applied twice */
				if (rot_src) {
					angle *= 0.5f;
				}

				axis_angle_to_mat3(mat, axis, angle);
			}
		}
		
		if (rot_src) {
			EDBM_op_callf(vc.em, op, "rotate verts=%hv cent=%v matrix=%m3",
			              BM_ELEM_SELECT, cent, mat);

			/* also project the source, for retopo workflow */
			if (use_proj)
				EMBM_project_snap_verts(C, vc.ar, vc.em);
		}

		edbm_extrude_edge(vc.obedit, vc.em, BM_ELEM_SELECT, nor);
		EDBM_op_callf(vc.em, op, "rotate verts=%hv cent=%v matrix=%m3",
		              BM_ELEM_SELECT, cent, mat);
		EDBM_op_callf(vc.em, op, "translate verts=%hv vec=%v",
		              BM_ELEM_SELECT, min);
	}
	else {
		const float *curs = ED_view3d_cursor3d_get(vc.scene, vc.v3d);
		BMOperator bmop;
		BMOIter oiter;
		
		copy_v3_v3(min, curs);
		ED_view3d_win_to_3d_int(vc.ar, min, event->mval, min);

		invert_m4_m4(vc.obedit->imat, vc.obedit->obmat);
		mul_m4_v3(vc.obedit->imat, min); // back in object space
		
		EDBM_op_init(vc.em, &bmop, op, "create_vert co=%v", min);
		BMO_op_exec(vc.em->bm, &bmop);

		BMO_ITER (v1, &oiter, bmop.slots_out, "vert.out", BM_VERT) {
			BM_vert_select_set(vc.em->bm, v1, true);
		}

		if (!EDBM_op_finish(vc.em, &bmop, op, true)) {
			return OPERATOR_CANCELLED;
		}
	}

	if (use_proj)
		EMBM_project_snap_verts(C, vc.ar, vc.em);

	/* This normally happens when pushing undo but modal operators
	 * like this one don't push undo data until after modal mode is
	 * done. */
	EDBM_mesh_normals_update(vc.em);

	EDBM_update_generic(vc.em, true, true);

	return OPERATOR_FINISHED;
}

void MESH_OT_dupli_extrude_cursor(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate or Extrude at 3D Cursor";
	ot->idname = "MESH_OT_dupli_extrude_cursor";
	ot->description = "Duplicate and extrude selected vertices, edges or faces towards the mouse cursor";
	
	/* api callbacks */
	ot->invoke = edbm_dupli_extrude_cursor_invoke;
	ot->poll = ED_operator_editmesh;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "rotate_source", 1, "Rotate Source", "Rotate initial selection giving better shape");
}


static int edbm_spin_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMOperator spinop;
	float cent[3], axis[3];
	float d[3] = {0.0f, 0.0f, 0.0f};
	int steps, dupli;
	float angle;

	RNA_float_get_array(op->ptr, "center", cent);
	RNA_float_get_array(op->ptr, "axis", axis);
	steps = RNA_int_get(op->ptr, "steps");
	angle = RNA_float_get(op->ptr, "angle");
	//if (ts->editbutflag & B_CLOCKWISE)
	angle = -angle;
	dupli = RNA_boolean_get(op->ptr, "dupli");

	/* keep the values in worldspace since we're passing the obmat */
	if (!EDBM_op_init(em, &spinop, op,
	                  "spin geom=%hvef cent=%v axis=%v dvec=%v steps=%i angle=%f space=%m4 use_duplicate=%b",
	                  BM_ELEM_SELECT, cent, axis, d, steps, angle, obedit->obmat, dupli))
	{
		return OPERATOR_CANCELLED;
	}
	BMO_op_exec(bm, &spinop);
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	BMO_slot_buffer_hflag_enable(bm, spinop.slots_out, "geom_last.out", BM_ALL_NOLOOP, BM_ELEM_SELECT, true);
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

	RNA_float_set_array(op->ptr, "center", ED_view3d_cursor3d_get(scene, v3d));
	RNA_float_set_array(op->ptr, "axis", rv3d->viewinv[2]);

	return edbm_spin_exec(C, op);
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
	ot->poll = EDBM_view3d_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_int(ot->srna, "steps", 9, 0, INT_MAX, "Steps", "Steps", 0, INT_MAX);
	RNA_def_boolean(ot->srna, "dupli", 0, "Dupli", "Make Duplicates");
	prop = RNA_def_float(ot->srna, "angle", DEG2RADF(90.0f), -FLT_MAX, FLT_MAX, "Angle", "Angle", DEG2RADF(-360.0f), DEG2RADF(360.0f));
	RNA_def_property_subtype(prop, PROP_ANGLE);

	RNA_def_float_vector(ot->srna, "center", 3, NULL, -FLT_MAX, FLT_MAX, "Center", "Center in global view space", -FLT_MAX, FLT_MAX);
	RNA_def_float_vector(ot->srna, "axis", 3, NULL, -FLT_MAX, FLT_MAX, "Axis", "Axis in global view space", -1.0f, 1.0f);

}

static int edbm_screw_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMEdge *eed;
	BMVert *eve, *v1, *v2;
	BMIter iter, eiter;
	BMOperator spinop;
	float dvec[3], nor[3], cent[3], axis[3], v1_co_global[3], v2_co_global[3];
	int steps, turns;
	int valence;


	turns = RNA_int_get(op->ptr, "turns");
	steps = RNA_int_get(op->ptr, "steps");
	RNA_float_get_array(op->ptr, "center", cent);
	RNA_float_get_array(op->ptr, "axis", axis);

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
		BKE_report(op->reports, RPT_ERROR, "You have to select a string of connected vertices too");
		return OPERATOR_CANCELLED;
	}

	copy_v3_v3(nor, obedit->obmat[2]);

	/* calculate dvec */
	mul_v3_m4v3(v1_co_global, obedit->obmat, v1->co);
	mul_v3_m4v3(v2_co_global, obedit->obmat, v2->co);
	sub_v3_v3v3(dvec, v1_co_global, v2_co_global);
	mul_v3_fl(dvec, 1.0f / steps);

	if (dot_v3v3(nor, dvec) > 0.0f)
		negate_v3(dvec);

	if (!EDBM_op_init(em, &spinop, op,
	                  "spin geom=%hvef cent=%v axis=%v dvec=%v steps=%i angle=%f space=%m4 use_duplicate=%b",
	                  BM_ELEM_SELECT, cent, axis, dvec, turns * steps, DEG2RADF(360.0f * turns), obedit->obmat, false))
	{
		return OPERATOR_CANCELLED;
	}
	BMO_op_exec(bm, &spinop);
	EDBM_flag_disable_all(em, BM_ELEM_SELECT);
	BMO_slot_buffer_hflag_enable(bm, spinop.slots_out, "geom_last.out", BM_ALL_NOLOOP, BM_ELEM_SELECT, true);
	if (!EDBM_op_finish(em, &spinop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

/* get center and axis, in global coords */
static int edbm_screw_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);

	RNA_float_set_array(op->ptr, "center", ED_view3d_cursor3d_get(scene, v3d));
	RNA_float_set_array(op->ptr, "axis", rv3d->viewinv[1]);

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
	ot->poll = EDBM_view3d_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_int(ot->srna, "steps", 9, 1, INT_MAX, "Steps", "Steps", 3, 256);
	RNA_def_int(ot->srna, "turns", 1, 1, INT_MAX, "Turns", "Turns", 1, 256);

	RNA_def_float_vector(ot->srna, "center", 3, NULL, -FLT_MAX, FLT_MAX,
	                     "Center", "Center in global view space", -FLT_MAX, FLT_MAX);
	RNA_def_float_vector(ot->srna, "axis", 3, NULL, -FLT_MAX, FLT_MAX,
	                     "Axis", "Axis in global view space", -1.0f, 1.0f);
}
