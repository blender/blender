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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_select.c
 *  \ingroup spview3d
 */


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <assert.h>

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_tracking_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_lasso.h"
#include "BLI_rect.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

/* vertex box select */
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "BKE_global.h"

#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_editmesh.h"
#include "BKE_tracking.h"
#include "BKE_utildefines.h"


#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_particle.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_mball.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "view3d_intern.h"  /* own include */

float ED_view3d_select_dist_px(void)
{
	return 75.0f * U.pixelsize;
}

/* TODO: should return whether there is valid context to continue */
void view3d_set_viewcontext(bContext *C, ViewContext *vc)
{
	memset(vc, 0, sizeof(ViewContext));
	vc->ar = CTX_wm_region(C);
	vc->scene = CTX_data_scene(C);
	vc->v3d = CTX_wm_view3d(C);
	vc->rv3d = CTX_wm_region_view3d(C);
	vc->obact = CTX_data_active_object(C);
	vc->obedit = CTX_data_edit_object(C);
}

/*
 * ob == NULL if you want global matrices
 * */
void view3d_get_transformation(const ARegion *ar, RegionView3D *rv3d, Object *ob, bglMats *mats)
{
	float cpy[4][4];
	int i, j;

	if (ob) {
		mul_m4_m4m4(cpy, rv3d->viewmat, ob->obmat);
	}
	else {
		copy_m4_m4(cpy, rv3d->viewmat);
	}

	for (i = 0; i < 4; ++i) {
		for (j = 0; j < 4; ++j) {
			mats->projection[i * 4 + j] = rv3d->winmat[i][j];
			mats->modelview[i * 4 + j] = cpy[i][j];
		}
	}

	mats->viewport[0] = ar->winrct.xmin;
	mats->viewport[1] = ar->winrct.ymin;
	mats->viewport[2] = ar->winx;
	mats->viewport[3] = ar->winy;
}

/* ********************** view3d_select: selection manipulations ********************* */

/* local prototypes */

static void edbm_backbuf_check_and_select_verts(BMEditMesh *em, const bool select)
{
	BMVert *eve;
	BMIter iter;
	unsigned int index = bm_wireoffs;

	BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
		if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
			if (EDBM_backbuf_check(index)) {
				BM_vert_select_set(em->bm, eve, select);
			}
		}
		index++;
	}
}

static void edbm_backbuf_check_and_select_edges(BMEditMesh *em, const bool select)
{
	BMEdge *eed;
	BMIter iter;
	unsigned int index = bm_solidoffs;

	BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
		if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
			if (EDBM_backbuf_check(index)) {
				BM_edge_select_set(em->bm, eed, select);
			}
		}
		index++;
	}
}

static void edbm_backbuf_check_and_select_faces(BMEditMesh *em, const bool select)
{
	BMFace *efa;
	BMIter iter;
	unsigned int index = 1;

	BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
		if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
			if (EDBM_backbuf_check(index)) {
				BM_face_select_set(em->bm, efa, select);
			}
		}
		index++;
	}
}


/* object mode, edbm_ prefix is confusing here, rename? */
static void edbm_backbuf_check_and_select_verts_obmode(Mesh *me, const bool select)
{
	MVert *mv = me->mvert;
	unsigned int index;

	if (mv) {
		for (index = 1; index <= me->totvert; index++, mv++) {
			if (EDBM_backbuf_check(index)) {
				if (!(mv->flag & ME_HIDE)) {
					mv->flag = select ? (mv->flag | SELECT) : (mv->flag & ~SELECT);
				}
			}
		}
	}
}

/* object mode, edbm_ prefix is confusing here, rename? */
static void edbm_backbuf_check_and_select_tfaces(Mesh *me, const bool select)
{
	MPoly *mpoly = me->mpoly;
	unsigned int index;

	if (mpoly) {
		for (index = 1; index <= me->totpoly; index++, mpoly++) {
			if (EDBM_backbuf_check(index)) {
				mpoly->flag = select ? (mpoly->flag | ME_FACE_SEL) : (mpoly->flag & ~ME_FACE_SEL);
			}
		}
	}
}

/* *********************** GESTURE AND LASSO ******************* */

typedef struct LassoSelectUserData {
	ViewContext *vc;
	const rcti *rect;
	const rctf *rect_fl;
	rctf       _rect_fl;
	const int (*mcords)[2];
	int moves;
	bool select;

	/* runtime */
	int pass;
	bool is_done;
	bool is_changed;
} LassoSelectUserData;

static void view3d_userdata_lassoselect_init(LassoSelectUserData *r_data,
                                             ViewContext *vc, const rcti *rect, const int (*mcords)[2],
                                             const int moves, const bool select)
{
	r_data->vc = vc;

	r_data->rect = rect;
	r_data->rect_fl = &r_data->_rect_fl;
	BLI_rctf_rcti_copy(&r_data->_rect_fl, rect);

	r_data->mcords = mcords;
	r_data->moves = moves;
	r_data->select = select;

	/* runtime */
	r_data->pass = 0;
	r_data->is_done = false;
	r_data->is_changed = false;
}

static int view3d_selectable_data(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if (!ED_operator_region_view3d_active(C))
		return 0;

	if (ob) {
		if (ob->mode & OB_MODE_EDIT) {
			if (ob->type == OB_FONT) {
				return 0;
			}
		}
		else {
			if ((ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT)) &&
			    !BKE_paint_select_elem_test(ob))
			{
				return 0;
			}
		}
	}

	return 1;
}


/* helper also for borderselect */
static bool edge_fully_inside_rect(const rctf *rect, const float v1[2], const float v2[2])
{
	return BLI_rctf_isect_pt_v(rect, v1) && BLI_rctf_isect_pt_v(rect, v2);
}

static bool edge_inside_rect(const rctf *rect, const float v1[2], const float v2[2])
{
	int d1, d2, d3, d4;
	
	/* check points in rect */
	if (edge_fully_inside_rect(rect, v1, v2)) return 1;
	
	/* check points completely out rect */
	if (v1[0] < rect->xmin && v2[0] < rect->xmin) return 0;
	if (v1[0] > rect->xmax && v2[0] > rect->xmax) return 0;
	if (v1[1] < rect->ymin && v2[1] < rect->ymin) return 0;
	if (v1[1] > rect->ymax && v2[1] > rect->ymax) return 0;
	
	/* simple check lines intersecting. */
	d1 = (v1[1] - v2[1]) * (v1[0] - rect->xmin) + (v2[0] - v1[0]) * (v1[1] - rect->ymin);
	d2 = (v1[1] - v2[1]) * (v1[0] - rect->xmin) + (v2[0] - v1[0]) * (v1[1] - rect->ymax);
	d3 = (v1[1] - v2[1]) * (v1[0] - rect->xmax) + (v2[0] - v1[0]) * (v1[1] - rect->ymax);
	d4 = (v1[1] - v2[1]) * (v1[0] - rect->xmax) + (v2[0] - v1[0]) * (v1[1] - rect->ymin);
	
	if (d1 < 0 && d2 < 0 && d3 < 0 && d4 < 0) return 0;
	if (d1 > 0 && d2 > 0 && d3 > 0 && d4 > 0) return 0;
	
	return 1;
}

static void do_lasso_select_pose__doSelectBone(void *userData, struct bPoseChannel *pchan, const float screen_co_a[2], const float screen_co_b[2])
{
	LassoSelectUserData *data = userData;
	bArmature *arm = data->vc->obact->data;

	if (PBONE_SELECTABLE(arm, pchan->bone)) {
		bool is_point_done = false;
		int points_proj_tot = 0;

		const int x0 = screen_co_a[0];
		const int y0 = screen_co_a[1];
		const int x1 = screen_co_b[0];
		const int y1 = screen_co_b[1];

		/* project head location to screenspace */
		if (x0 != IS_CLIPPED) {
			points_proj_tot++;
			if (BLI_rcti_isect_pt(data->rect, x0, y0) &&
			    BLI_lasso_is_point_inside(data->mcords, data->moves, x0, y0, INT_MAX))
			{
				is_point_done = true;
			}
		}

		/* project tail location to screenspace */
		if (x1 != IS_CLIPPED) {
			points_proj_tot++;
			if (BLI_rcti_isect_pt(data->rect, x1, y1) &&
			    BLI_lasso_is_point_inside(data->mcords, data->moves, x1, y1, INT_MAX))
			{
				is_point_done = true;
			}
		}

		/* if one of points selected, we skip the bone itself */
		if ((is_point_done == true) ||
		    ((is_point_done == false) && (points_proj_tot == 2) &&
		     BLI_lasso_is_edge_inside(data->mcords, data->moves, x0, y0, x1, y1, INT_MAX)))
		{
			if (data->select) pchan->bone->flag |=  BONE_SELECTED;
			else              pchan->bone->flag &= ~BONE_SELECTED;
			data->is_changed = true;
		}
		data->is_changed |= is_point_done;
	}
}
static void do_lasso_select_pose(ViewContext *vc, Object *ob, const int mcords[][2], short moves, const bool select)
{
	ViewContext vc_tmp;
	LassoSelectUserData data;
	rcti rect;
	
	if ((ob->type != OB_ARMATURE) || (ob->pose == NULL)) {
		return;
	}

	vc_tmp = *vc;
	vc_tmp.obact = ob;

	BLI_lasso_boundbox(&rect, mcords, moves);

	view3d_userdata_lassoselect_init(&data, vc, &rect, mcords, moves, select);

	ED_view3d_init_mats_rv3d(vc->obact, vc->rv3d);

	pose_foreachScreenBone(&vc_tmp, do_lasso_select_pose__doSelectBone, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

	if (data.is_changed) {
		bArmature *arm = ob->data;
		if (arm->flag & ARM_HAS_VIZ_DEPS) {
			/* mask modifier ('armature' mode), etc. */
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		}
	}
}

static void object_deselect_all_visible(Scene *scene, View3D *v3d)
{
	Base *base;

	for (base = scene->base.first; base; base = base->next) {
		if (BASE_SELECTABLE(v3d, base)) {
			ED_base_object_select(base, BA_DESELECT);
		}
	}
}

static void do_lasso_select_objects(ViewContext *vc, const int mcords[][2], const short moves,
                                    const bool extend, const bool select)
{
	Base *base;
	
	if (extend == false && select)
		object_deselect_all_visible(vc->scene, vc->v3d);

	for (base = vc->scene->base.first; base; base = base->next) {
		if (BASE_SELECTABLE(vc->v3d, base)) { /* use this to avoid un-needed lasso lookups */
			ED_view3d_project_base(vc->ar, base);
			if (BLI_lasso_is_point_inside(mcords, moves, base->sx, base->sy, IS_CLIPPED)) {
				
				ED_base_object_select(base, select ? BA_SELECT : BA_DESELECT);
				base->object->flag = base->flag;
			}
			if (vc->obact == base->object && (base->object->mode & OB_MODE_POSE)) {
				do_lasso_select_pose(vc, base->object, mcords, moves, select);
			}
		}
	}
}

static void do_lasso_select_mesh__doSelectVert(void *userData, BMVert *eve, const float screen_co[2], int UNUSED(index))
{
	LassoSelectUserData *data = userData;

	if (BLI_rctf_isect_pt_v(data->rect_fl, screen_co) &&
	    BLI_lasso_is_point_inside(data->mcords, data->moves, screen_co[0], screen_co[1], IS_CLIPPED))
	{
		BM_vert_select_set(data->vc->em->bm, eve, data->select);
	}
}
static void do_lasso_select_mesh__doSelectEdge(void *userData, BMEdge *eed, const float screen_co_a[2], const float screen_co_b[2], int index)
{
	LassoSelectUserData *data = userData;

	if (EDBM_backbuf_check(bm_solidoffs + index)) {
		const int x0 = screen_co_a[0];
		const int y0 = screen_co_a[1];
		const int x1 = screen_co_b[0];
		const int y1 = screen_co_b[1];

		if (data->pass == 0) {
			if (edge_fully_inside_rect(data->rect_fl, screen_co_a, screen_co_b)  &&
			    BLI_lasso_is_point_inside(data->mcords, data->moves, x0, y0, IS_CLIPPED) &&
			    BLI_lasso_is_point_inside(data->mcords, data->moves, x1, y1, IS_CLIPPED))
			{
				BM_edge_select_set(data->vc->em->bm, eed, data->select);
				data->is_done = true;
			}
		}
		else {
			if (BLI_lasso_is_edge_inside(data->mcords, data->moves, x0, y0, x1, y1, IS_CLIPPED)) {
				BM_edge_select_set(data->vc->em->bm, eed, data->select);
			}
		}
	}
}
static void do_lasso_select_mesh__doSelectFace(void *userData, BMFace *efa, const float screen_co[2], int UNUSED(index))
{
	LassoSelectUserData *data = userData;

	if (BLI_rctf_isect_pt_v(data->rect_fl, screen_co) &&
	    BLI_lasso_is_point_inside(data->mcords, data->moves, screen_co[0], screen_co[1], IS_CLIPPED))
	{
		BM_face_select_set(data->vc->em->bm, efa, data->select);
	}
}

static void do_lasso_select_mesh(ViewContext *vc, const int mcords[][2], short moves, bool extend, bool select)
{
	LassoSelectUserData data;
	ToolSettings *ts = vc->scene->toolsettings;
	rcti rect;
	int bbsel;
	
	/* set editmesh */
	vc->em = BKE_editmesh_from_object(vc->obedit);

	BLI_lasso_boundbox(&rect, mcords, moves);

	view3d_userdata_lassoselect_init(&data, vc, &rect, mcords, moves, select);

	if (extend == false && select)
		EDBM_flag_disable_all(vc->em, BM_ELEM_SELECT);

	/* for non zbuf projections, don't change the GL state */
	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

	glLoadMatrixf(vc->rv3d->viewmat);
	bbsel = EDBM_backbuf_border_mask_init(vc, mcords, moves, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
	
	if (ts->selectmode & SCE_SELECT_VERTEX) {
		if (bbsel) {
			edbm_backbuf_check_and_select_verts(vc->em, select);
		}
		else {
			mesh_foreachScreenVert(vc, do_lasso_select_mesh__doSelectVert, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
		}
	}
	if (ts->selectmode & SCE_SELECT_EDGE) {
		/* Does both bbsel and non-bbsel versions (need screen cos for both) */
		data.pass = 0;
		mesh_foreachScreenEdge(vc, do_lasso_select_mesh__doSelectEdge, &data, V3D_PROJ_TEST_CLIP_NEAR);

		if (data.is_done == false) {
			data.pass = 1;
			mesh_foreachScreenEdge(vc, do_lasso_select_mesh__doSelectEdge, &data, V3D_PROJ_TEST_CLIP_NEAR);
		}
	}
	
	if (ts->selectmode & SCE_SELECT_FACE) {
		if (bbsel) {
			edbm_backbuf_check_and_select_faces(vc->em, select);
		}
		else {
			mesh_foreachScreenFace(vc, do_lasso_select_mesh__doSelectFace, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
		}
	}
	
	EDBM_backbuf_free();
	EDBM_selectmode_flush(vc->em);
}

static void do_lasso_select_curve__doSelect(void *userData, Nurb *UNUSED(nu), BPoint *bp, BezTriple *bezt, int beztindex, const float screen_co[2])
{
	LassoSelectUserData *data = userData;
	Object *obedit = data->vc->obedit;
	Curve *cu = (Curve *)obedit->data;

	if (BLI_lasso_is_point_inside(data->mcords, data->moves, screen_co[0], screen_co[1], IS_CLIPPED)) {
		if (bp) {
			bp->f1 = data->select ? (bp->f1 | SELECT) : (bp->f1 & ~SELECT);
		}
		else {
			if (cu->drawflag & CU_HIDE_HANDLES) {
				/* can only be (beztindex == 0) here since handles are hidden */
				bezt->f1 = bezt->f2 = bezt->f3 = data->select ? (bezt->f2 | SELECT) : (bezt->f2 & ~SELECT);
			}
			else {
				if (beztindex == 0) {
					bezt->f1 = data->select ? (bezt->f1 | SELECT) : (bezt->f1 & ~SELECT);
				}
				else if (beztindex == 1) {
					bezt->f2 = data->select ? (bezt->f2 | SELECT) : (bezt->f2 & ~SELECT);
				}
				else {
					bezt->f3 = data->select ? (bezt->f3 | SELECT) : (bezt->f3 & ~SELECT);
				}
			}
		}
	}
}

static void do_lasso_select_curve(ViewContext *vc, const int mcords[][2], short moves, bool extend, bool select)
{
	LassoSelectUserData data;
	rcti rect;

	BLI_lasso_boundbox(&rect, mcords, moves);

	view3d_userdata_lassoselect_init(&data, vc, &rect, mcords, moves, select);

	if (extend == false && select) {
		Curve *curve = (Curve *) vc->obedit->data;
		ED_curve_deselect_all(curve->editnurb);
	}

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
	nurbs_foreachScreenVert(vc, do_lasso_select_curve__doSelect, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
	BKE_curve_nurb_vert_active_validate(vc->obedit->data);
}

static void do_lasso_select_lattice__doSelect(void *userData, BPoint *bp, const float screen_co[2])
{
	LassoSelectUserData *data = userData;

	if (BLI_rctf_isect_pt_v(data->rect_fl, screen_co) &&
	    BLI_lasso_is_point_inside(data->mcords, data->moves, screen_co[0], screen_co[1], IS_CLIPPED))
	{
		bp->f1 = data->select ? (bp->f1 | SELECT) : (bp->f1 & ~SELECT);
	}
}
static void do_lasso_select_lattice(ViewContext *vc, const int mcords[][2], short moves, bool extend, bool select)
{
	LassoSelectUserData data;
	rcti rect;

	BLI_lasso_boundbox(&rect, mcords, moves);

	view3d_userdata_lassoselect_init(&data, vc, &rect, mcords, moves, select);

	if (extend == false && select)
		ED_setflagsLatt(vc->obedit, 0);

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
	lattice_foreachScreenVert(vc, do_lasso_select_lattice__doSelect, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
}

static void do_lasso_select_armature__doSelectBone(void *userData, struct EditBone *ebone, const float screen_co_a[2], const float screen_co_b[2])
{
	LassoSelectUserData *data = userData;
	bArmature *arm = data->vc->obedit->data;

	if (data->select ? EBONE_SELECTABLE(arm, ebone) : EBONE_VISIBLE(arm, ebone)) {
		bool is_point_done = false;
		int points_proj_tot = 0;

		const int x0 = screen_co_a[0];
		const int y0 = screen_co_a[1];
		const int x1 = screen_co_b[0];
		const int y1 = screen_co_b[1];

		/* project head location to screenspace */
		if (x0 != IS_CLIPPED) {
			points_proj_tot++;
			if (BLI_rcti_isect_pt(data->rect, x0, y0) &&
			    BLI_lasso_is_point_inside(data->mcords, data->moves, x0, y0, INT_MAX))
			{
				is_point_done = true;
				if (data->select) ebone->flag |=  BONE_ROOTSEL;
				else              ebone->flag &= ~BONE_ROOTSEL;
			}
		}

		/* project tail location to screenspace */
		if (x1 != IS_CLIPPED) {
			points_proj_tot++;
			if (BLI_rcti_isect_pt(data->rect, x1, y1) &&
			    BLI_lasso_is_point_inside(data->mcords, data->moves, x1, y1, INT_MAX))
			{
				is_point_done = true;
				if (data->select) ebone->flag |=  BONE_TIPSEL;
				else              ebone->flag &= ~BONE_TIPSEL;
			}
		}

		/* if one of points selected, we skip the bone itself */
		if ((is_point_done == false) && (points_proj_tot == 2) &&
		    BLI_lasso_is_edge_inside(data->mcords, data->moves, x0, y0, x1, y1, INT_MAX))
		{
			if (data->select) ebone->flag |=  (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
			else              ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
			data->is_changed = true;
		}

		data->is_changed |= is_point_done;
	}
}

static void do_lasso_select_armature(ViewContext *vc, const int mcords[][2], short moves, bool extend, bool select)
{
	LassoSelectUserData data;
	rcti rect;

	BLI_lasso_boundbox(&rect, mcords, moves);

	view3d_userdata_lassoselect_init(&data, vc, &rect, mcords, moves, select);

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

	if (extend == false && select)
		ED_armature_deselect_all_visible(vc->obedit);

	armature_foreachScreenBone(vc, do_lasso_select_armature__doSelectBone, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

	if (data.is_changed) {
		bArmature *arm = vc->obedit->data;
		ED_armature_sync_selection(arm->edbo);
		ED_armature_validate_active(arm);
		WM_main_add_notifier(NC_OBJECT | ND_BONE_SELECT, vc->obedit);
	}
}

static void do_lasso_select_mball__doSelectElem(void *userData, struct MetaElem *ml, const float screen_co[2])
{
	LassoSelectUserData *data = userData;

	if (BLI_rctf_isect_pt_v(data->rect_fl, screen_co) &&
	    BLI_lasso_is_point_inside(data->mcords, data->moves, screen_co[0], screen_co[1], INT_MAX))
	{
		if (data->select) ml->flag |=  SELECT;
		else              ml->flag &= ~SELECT;
		data->is_changed = true;
	}
}
static void do_lasso_select_meta(ViewContext *vc, const int mcords[][2], short moves, bool extend, bool select)
{
	LassoSelectUserData data;
	rcti rect;

	MetaBall *mb = (MetaBall *)vc->obedit->data;

	if (extend == false && select)
		BKE_mball_deselect_all(mb);

	BLI_lasso_boundbox(&rect, mcords, moves);

	view3d_userdata_lassoselect_init(&data, vc, &rect, mcords, moves, select);

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

	mball_foreachScreenElem(vc, do_lasso_select_mball__doSelectElem, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
}

static void do_lasso_select_meshobject__doSelectVert(void *userData, MVert *mv, const float screen_co[2], int UNUSED(index))
{
	LassoSelectUserData *data = userData;

	if (BLI_rctf_isect_pt_v(data->rect_fl, screen_co) &&
	    BLI_lasso_is_point_inside(data->mcords, data->moves, screen_co[0], screen_co[1], IS_CLIPPED))
	{
		BKE_BIT_TEST_SET(mv->flag, data->select, SELECT);
	}
}
static void do_lasso_select_paintvert(ViewContext *vc, const int mcords[][2], short moves, bool extend, bool select)
{
	const int use_zbuf = (vc->v3d->flag & V3D_ZBUF_SELECT);
	Object *ob = vc->obact;
	Mesh *me = ob->data;
	rcti rect;

	if (me == NULL || me->totvert == 0)
		return;

	if (extend == false && select)
		paintvert_deselect_all_visible(ob, SEL_DESELECT, false);  /* flush selection at the end */

	BLI_lasso_boundbox(&rect, mcords, moves);

	if (use_zbuf) {
		bm_vertoffs = me->totvert + 1; /* max index array */

		EDBM_backbuf_border_mask_init(vc, mcords, moves, rect.xmin, rect.ymin, rect.xmax, rect.ymax);

		edbm_backbuf_check_and_select_verts_obmode(me, select);

		EDBM_backbuf_free();
	}
	else {
		LassoSelectUserData data;

		view3d_userdata_lassoselect_init(&data, vc, &rect, mcords, moves, select);

		ED_view3d_init_mats_rv3d(vc->obact, vc->rv3d);

		meshobject_foreachScreenVert(vc, do_lasso_select_meshobject__doSelectVert, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

	}

	if (select == false) {
		BKE_mesh_mselect_validate(me);
	}
	paintvert_flush_flags(ob);
}
static void do_lasso_select_paintface(ViewContext *vc, const int mcords[][2], short moves, bool extend, bool select)
{
	Object *ob = vc->obact;
	Mesh *me = ob->data;
	rcti rect;

	if (me == NULL || me->totpoly == 0)
		return;

	if (extend == false && select)
		paintface_deselect_all_visible(ob, SEL_DESELECT, false);  /* flush selection at the end */

	bm_vertoffs = me->totpoly + 1; /* max index array */

	BLI_lasso_boundbox(&rect, mcords, moves);
	EDBM_backbuf_border_mask_init(vc, mcords, moves, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
	
	edbm_backbuf_check_and_select_tfaces(me, select);

	EDBM_backbuf_free();

	paintface_flush_flags(ob);
}

#if 0
static void do_lasso_select_node(int mcords[][2], short moves, const bool select)
{
	SpaceNode *snode = sa->spacedata.first;
	
	bNode *node;
	rcti rect;
	int node_cent[2];
	float node_centf[2];
	
	BLI_lasso_boundbox(&rect, mcords, moves);
	
	/* store selection in temp test flag */
	for (node = snode->edittree->nodes.first; node; node = node->next) {
		node_centf[0] = BLI_RCT_CENTER_X(&node->totr);
		node_centf[1] = BLI_RCT_CENTER_Y(&node->totr);
		
		ipoco_to_areaco_noclip(G.v2d, node_centf, node_cent);
		if (BLI_rcti_isect_pt_v(&rect, node_cent) && BLI_lasso_is_point_inside(mcords, moves, node_cent[0], node_cent[1])) {
			if (select) {
				node->flag |= SELECT;
			}
			else {
				node->flag &= ~SELECT;
			}
		}
	}
	BIF_undo_push("Lasso select nodes");
}
#endif

static void view3d_lasso_select(bContext *C, ViewContext *vc,
                                const int mcords[][2], short moves,
                                bool extend, bool select)
{
	Object *ob = CTX_data_active_object(C);

	if (vc->obedit == NULL) { /* Object Mode */
		if (BKE_paint_select_face_test(ob))
			do_lasso_select_paintface(vc, mcords, moves, extend, select);
		else if (BKE_paint_select_vert_test(ob))
			do_lasso_select_paintvert(vc, mcords, moves, extend, select);
		else if (ob && (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT))) {
			/* pass */
		}
		else if (ob && (ob->mode & OB_MODE_PARTICLE_EDIT))
			PE_lasso_select(C, mcords, moves, extend, select);
		else {
			do_lasso_select_objects(vc, mcords, moves, extend, select);
			WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, vc->scene);
		}
	}
	else { /* Edit Mode */
		switch (vc->obedit->type) {
			case OB_MESH:
				do_lasso_select_mesh(vc, mcords, moves, extend, select);
				break;
			case OB_CURVE:
			case OB_SURF:
				do_lasso_select_curve(vc, mcords, moves, extend, select);
				break;
			case OB_LATTICE:
				do_lasso_select_lattice(vc, mcords, moves, extend, select);
				break;
			case OB_ARMATURE:
				do_lasso_select_armature(vc, mcords, moves, extend, select);
				break;
			case OB_MBALL:
				do_lasso_select_meta(vc, mcords, moves, extend, select);
				break;
			default:
				assert(!"lasso select on incorrect object type");
				break;
		}

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
	}
}


/* lasso operator gives properties, but since old code works
 * with short array we convert */
static int view3d_lasso_select_exec(bContext *C, wmOperator *op)
{
	ViewContext vc;
	int mcords_tot;
	const int (*mcords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcords_tot);
	
	if (mcords) {
		bool extend, select;
		view3d_operator_needs_opengl(C);
		
		/* setup view context for argument to callbacks */
		view3d_set_viewcontext(C, &vc);
		
		extend = RNA_boolean_get(op->ptr, "extend");
		select = !RNA_boolean_get(op->ptr, "deselect");
		view3d_lasso_select(C, &vc, mcords, mcords_tot, extend, select);
		
		MEM_freeN((void *)mcords);

		return OPERATOR_FINISHED;
	}
	return OPERATOR_PASS_THROUGH;
}

void VIEW3D_OT_select_lasso(wmOperatorType *ot)
{
	ot->name = "Lasso Select";
	ot->description = "Select items using lasso selection";
	ot->idname = "VIEW3D_OT_select_lasso";
	
	ot->invoke = WM_gesture_lasso_invoke;
	ot->modal = WM_gesture_lasso_modal;
	ot->exec = view3d_lasso_select_exec;
	ot->poll = view3d_selectable_data;
	ot->cancel = WM_gesture_lasso_cancel;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	RNA_def_collection_runtime(ot->srna, "path", &RNA_OperatorMousePath, "Path", "");
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Deselect rather than select items");
	RNA_def_boolean(ot->srna, "extend", 1, "Extend", "Extend selection instead of deselecting everything first");
}


/* ************************************************* */

#if 0
/* smart function to sample a rect spiralling outside, nice for backbuf selection */
static unsigned int samplerect(unsigned int *buf, int size, unsigned int dontdo)
{
	Base *base;
	unsigned int *bufmin, *bufmax;
	int a, b, rc, tel, len, dirvec[4][2], maxob;
	unsigned int retval = 0;
	
	base = LASTBASE;
	if (base == 0) return 0;
	maxob = base->selcol;

	len = (size - 1) / 2;
	rc = 0;

	dirvec[0][0] = 1;
	dirvec[0][1] = 0;
	dirvec[1][0] = 0;
	dirvec[1][1] = -size;
	dirvec[2][0] = -1;
	dirvec[2][1] = 0;
	dirvec[3][0] = 0;
	dirvec[3][1] = size;

	bufmin = buf;
	bufmax = buf + size * size;
	buf += len * size + len;

	for (tel = 1; tel <= size; tel++) {

		for (a = 0; a < 2; a++) {
			for (b = 0; b < tel; b++) {

				if (*buf && *buf <= maxob && *buf != dontdo) return *buf;
				if (*buf == dontdo) retval = dontdo;  /* if only color dontdo is available, still return dontdo */
				
				buf += (dirvec[rc][0] + dirvec[rc][1]);

				if (buf < bufmin || buf >= bufmax) return retval;
			}
			rc++;
			rc &= 3;
		}
	}
	return retval;
}
#endif

/* ************************** mouse select ************************* */


/* The max number of menu items in an object select menu */
typedef struct SelMenuItemF {
	char idname[MAX_ID_NAME - 2];
	int icon;
} SelMenuItemF;

#define SEL_MENU_SIZE   22
static SelMenuItemF object_mouse_select_menu_data[SEL_MENU_SIZE];

/* special (crappy) operator only for menu select */
static EnumPropertyItem *object_select_menu_enum_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	EnumPropertyItem *item = NULL, item_tmp = {0};
	int totitem = 0;
	int i = 0;

	/* don't need context but avoid docgen using this */
	if (C == NULL || object_mouse_select_menu_data[i].idname[0] == '\0') {
		return DummyRNA_NULL_items;
	}

	for (; i < SEL_MENU_SIZE && object_mouse_select_menu_data[i].idname[0] != '\0'; i++) {
		item_tmp.name = object_mouse_select_menu_data[i].idname;
		item_tmp.identifier = object_mouse_select_menu_data[i].idname;
		item_tmp.value = i;
		item_tmp.icon = object_mouse_select_menu_data[i].icon;
		RNA_enum_item_add(&item, &totitem, &item_tmp);
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static int object_select_menu_exec(bContext *C, wmOperator *op)
{
	const int name_index = RNA_enum_get(op->ptr, "name");
	const bool toggle = RNA_boolean_get(op->ptr, "toggle");
	bool changed = false;
	const char *name = object_mouse_select_menu_data[name_index].idname;

	if (!toggle) {
		CTX_DATA_BEGIN (C, Base *, base, selectable_bases)
		{
			if (base->flag & SELECT) {
				ED_base_object_select(base, BA_DESELECT);
				changed = true;
			}
		}
		CTX_DATA_END;
	}

	CTX_DATA_BEGIN (C, Base *, base, selectable_bases)
	{
		/* this is a bit dodjy, there should only be ONE object with this name, but library objects can mess this up */
		if (STREQ(name, base->object->id.name + 2)) {
			ED_base_object_activate(C, base);
			ED_base_object_select(base, BA_SELECT);
			changed = true;
		}
	}
	CTX_DATA_END;

	/* weak but ensures we activate menu again before using the enum */
	memset(object_mouse_select_menu_data, 0, sizeof(object_mouse_select_menu_data));

	/* undo? */
	if (changed) {
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, CTX_data_scene(C));
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void VIEW3D_OT_select_menu(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Select Menu";
	ot->description = "Menu object selection";
	ot->idname = "VIEW3D_OT_select_menu";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = object_select_menu_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* keyingset to use (dynamic enum) */
	prop = RNA_def_enum(ot->srna, "name", DummyRNA_NULL_items, 0, "Object Name", "");
	RNA_def_enum_funcs(prop, object_select_menu_enum_itemf);
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_ENUM_NO_TRANSLATE);
	ot->prop = prop;

	RNA_def_boolean(ot->srna, "toggle", 0, "Toggle", "Toggle selection instead of deselecting everything first");
}

static void deselectall_except(Scene *scene, Base *b)   /* deselect all except b */
{
	Base *base;
	
	for (base = FIRSTBASE; base; base = base->next) {
		if (base->flag & SELECT) {
			if (b != base) {
				ED_base_object_select(base, BA_DESELECT);
			}
		}
	}
}

static Base *object_mouse_select_menu(bContext *C, ViewContext *vc, unsigned int *buffer, int hits, const int mval[2], short toggle)
{
	short baseCount = 0;
	bool ok;
	LinkNode *linklist = NULL;
	
	CTX_DATA_BEGIN (C, Base *, base, selectable_bases)
	{
		ok = false;

		/* two selection methods, the CTRL select uses max dist of 15 */
		if (buffer) {
			int a;
			for (a = 0; a < hits; a++) {
				/* index was converted */
				if (base->selcol == buffer[(4 * a) + 3])
					ok = true;
			}
		}
		else {
			int temp, dist = 15;
			ED_view3d_project_base(vc->ar, base);
			
			temp = abs(base->sx - mval[0]) + abs(base->sy - mval[1]);
			if (temp < dist)
				ok = true;
		}

		if (ok) {
			baseCount++;
			BLI_linklist_prepend(&linklist, base);

			if (baseCount == SEL_MENU_SIZE)
				break;
		}
	}
	CTX_DATA_END;

	if (baseCount == 0) {
		return NULL;
	}
	if (baseCount == 1) {
		Base *base = (Base *)linklist->link;
		BLI_linklist_free(linklist, NULL);
		return base;
	}
	else {
		/* UI, full in static array values that we later use in an enum function */
		LinkNode *node;
		int i;

		memset(object_mouse_select_menu_data, 0, sizeof(object_mouse_select_menu_data));

		for (node = linklist, i = 0; node; node = node->next, i++) {
			Base *base = node->link;
			Object *ob = base->object;
			const char *name = ob->id.name + 2;

			BLI_strncpy(object_mouse_select_menu_data[i].idname, name, MAX_ID_NAME - 2);
			object_mouse_select_menu_data[i].icon = uiIconFromID(&ob->id);
		}

		{
			wmOperatorType *ot = WM_operatortype_find("VIEW3D_OT_select_menu", false);
			PointerRNA ptr;

			WM_operator_properties_create_ptr(&ptr, ot);
			RNA_boolean_set(&ptr, "toggle", toggle);
			WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &ptr);
			WM_operator_properties_free(&ptr);
		}

		BLI_linklist_free(linklist, NULL);
		return NULL;
	}
}

static bool selectbuffer_has_bones(const unsigned int *buffer, const unsigned int hits)
{
	unsigned int i;
	for (i = 0; i < hits; i++) {
		if (buffer[(4 * i) + 3] & 0xFFFF0000) {
			return true;
		}
	}
	return false;
}

/* utility function for mixed_bones_object_selectbuffer */
static short selectbuffer_ret_hits_15(unsigned int *UNUSED(buffer), const short hits15)
{
	return hits15;
}

static short selectbuffer_ret_hits_9(unsigned int *buffer, const short hits15, const short hits9)
{
	const int offs = 4 * hits15;
	memcpy(buffer, buffer + offs, 4 * hits9 * sizeof (unsigned int));
	return hits9;
}

static short selectbuffer_ret_hits_5(unsigned int *buffer, const short hits15, const short hits9, const short hits5)
{
	const int offs = 4 * hits15 + 4 * hits9;
	memcpy(buffer, buffer + offs, 4 * hits5  * sizeof (unsigned int));
	return hits5;
}

/* we want a select buffer with bones, if there are... */
/* so check three selection levels and compare */
static short mixed_bones_object_selectbuffer(ViewContext *vc, unsigned int *buffer, const int mval[2], bool *p_do_nearest, bool enumerate)
{
	rcti rect;
	int offs;
	short hits15, hits9 = 0, hits5 = 0;
	bool has_bones15 = false, has_bones9 = false, has_bones5 = false;
	static int last_mval[2] = {-100, -100};
	bool do_nearest = false;
	View3D *v3d = vc->v3d;

	/* define if we use solid nearest select or not */
	if (v3d->drawtype > OB_WIRE) {
		do_nearest = true;
		if (len_manhattan_v2v2_int(mval, last_mval) < 3) {
			do_nearest = false;
		}
	}
	copy_v2_v2_int(last_mval, mval);

	if (p_do_nearest)
		*p_do_nearest = do_nearest;

	do_nearest = do_nearest && !enumerate;

	BLI_rcti_init(&rect, mval[0] - 14, mval[0] + 14, mval[1] - 14, mval[1] + 14);
	hits15 = view3d_opengl_select(vc, buffer, MAXPICKBUF, &rect, do_nearest);
	if (hits15 == 1) {
		return selectbuffer_ret_hits_15(buffer, hits15);
	}
	else if (hits15 > 0) {
		has_bones15 = selectbuffer_has_bones(buffer, hits15);

		offs = 4 * hits15;
		BLI_rcti_init(&rect, mval[0] - 9, mval[0] + 9, mval[1] - 9, mval[1] + 9);
		hits9 = view3d_opengl_select(vc, buffer + offs, MAXPICKBUF - offs, &rect, do_nearest);
		if (hits9 == 1) {
			return selectbuffer_ret_hits_9(buffer, hits15, hits9);
		}
		else if (hits9 > 0) {
			has_bones9 = selectbuffer_has_bones(buffer + offs, hits9);

			offs += 4 * hits9;
			BLI_rcti_init(&rect, mval[0] - 5, mval[0] + 5, mval[1] - 5, mval[1] + 5);
			hits5 = view3d_opengl_select(vc, buffer + offs, MAXPICKBUF - offs, &rect, do_nearest);
			if (hits5 == 1) {
				return selectbuffer_ret_hits_5(buffer, hits15, hits9, hits5);
			}
			else if (hits5 > 0) {
				has_bones5 = selectbuffer_has_bones(buffer + offs, hits5);
			}
		}

		if      (has_bones5)  return selectbuffer_ret_hits_5(buffer,  hits15, hits9, hits5);
		else if (has_bones9)  return selectbuffer_ret_hits_9(buffer,  hits15, hits9);
		else if (has_bones15) return selectbuffer_ret_hits_15(buffer, hits15);
		
		if      (hits5 > 0) return selectbuffer_ret_hits_5(buffer,  hits15, hits9, hits5);
		else if (hits9 > 0) return selectbuffer_ret_hits_9(buffer,  hits15, hits9);
		else                return selectbuffer_ret_hits_15(buffer, hits15);
	}
	
	return 0;
}

/* returns basact */
static Base *mouse_select_eval_buffer(ViewContext *vc, unsigned int *buffer, int hits,
                                      Base *startbase, bool has_bones, bool do_nearest)
{
	Scene *scene = vc->scene;
	View3D *v3d = vc->v3d;
	Base *base, *basact = NULL;
	int a;
	
	if (do_nearest) {
		unsigned int min = 0xFFFFFFFF;
		int selcol = 0, notcol = 0;
		
		
		if (has_bones) {
			/* we skip non-bone hits */
			for (a = 0; a < hits; a++) {
				if (min > buffer[4 * a + 1] && (buffer[4 * a + 3] & 0xFFFF0000) ) {
					min = buffer[4 * a + 1];
					selcol = buffer[4 * a + 3] & 0xFFFF;
				}
			}
		}
		else {
			/* only exclude active object when it is selected... */
			if (BASACT && (BASACT->flag & SELECT) && hits > 1) notcol = BASACT->selcol;
			
			for (a = 0; a < hits; a++) {
				if (min > buffer[4 * a + 1] && notcol != (buffer[4 * a + 3] & 0xFFFF)) {
					min = buffer[4 * a + 1];
					selcol = buffer[4 * a + 3] & 0xFFFF;
				}
			}
		}
		
		base = FIRSTBASE;
		while (base) {
			if (BASE_SELECTABLE(v3d, base)) {
				if (base->selcol == selcol) break;
			}
			base = base->next;
		}
		if (base) basact = base;
	}
	else {
		
		base = startbase;
		while (base) {
			/* skip objects with select restriction, to prevent prematurely ending this loop
			 * with an un-selectable choice */
			if (base->object->restrictflag & OB_RESTRICT_SELECT) {
				base = base->next;
				if (base == NULL) base = FIRSTBASE;
				if (base == startbase) break;
			}
			
			if (BASE_SELECTABLE(v3d, base)) {
				for (a = 0; a < hits; a++) {
					if (has_bones) {
						/* skip non-bone objects */
						if ((buffer[4 * a + 3] & 0xFFFF0000)) {
							if (base->selcol == (buffer[(4 * a) + 3] & 0xFFFF))
								basact = base;
						}
					}
					else {
						if (base->selcol == (buffer[(4 * a) + 3] & 0xFFFF))
							basact = base;
					}
				}
			}
			
			if (basact) break;
			
			base = base->next;
			if (base == NULL) base = FIRSTBASE;
			if (base == startbase) break;
		}
	}
	
	return basact;
}

/* mval comes from event->mval, only use within region handlers */
Base *ED_view3d_give_base_under_cursor(bContext *C, const int mval[2])
{
	ViewContext vc;
	Base *basact = NULL;
	unsigned int buffer[4 * MAXPICKBUF];
	int hits;
	bool do_nearest;
	
	/* setup view context for argument to callbacks */
	view3d_operator_needs_opengl(C);
	view3d_set_viewcontext(C, &vc);
	
	hits = mixed_bones_object_selectbuffer(&vc, buffer, mval, &do_nearest, false);
	
	if (hits > 0) {
		const bool has_bones = selectbuffer_has_bones(buffer, hits);
		basact = mouse_select_eval_buffer(&vc, buffer, hits, vc.scene->base.first, has_bones, do_nearest);
	}
	
	return basact;
}

static void deselect_all_tracks(MovieTracking *tracking)
{
	MovieTrackingObject *object;

	object = tracking->objects.first;
	while (object) {
		ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, object);
		MovieTrackingTrack *track = tracksbase->first;

		while (track) {
			BKE_tracking_track_deselect(track, TRACK_AREA_ALL);

			track = track->next;
		}

		object = object->next;
	}
}

/* mval is region coords */
static bool mouse_select(bContext *C, const int mval[2],
                         bool extend, bool deselect, bool toggle, bool obcenter, bool enumerate, bool object)
{
	ViewContext vc;
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = CTX_wm_view3d(C);
	Scene *scene = CTX_data_scene(C);
	Base *base, *startbase = NULL, *basact = NULL, *oldbasact = NULL;
	bool is_obedit;
	float dist = ED_view3d_select_dist_px() * 1.3333f;
	bool retval = false;
	short hits;
	const float mval_fl[2] = {(float)mval[0], (float)mval[1]};

	
	/* setup view context for argument to callbacks */
	view3d_set_viewcontext(C, &vc);

	is_obedit = (vc.obedit != NULL);
	if (object) {
		/* signal for view3d_opengl_select to skip editmode objects */
		vc.obedit = NULL;
	}
	
	/* always start list from basact in wire mode */
	startbase =  FIRSTBASE;
	if (BASACT && BASACT->next) startbase = BASACT->next;
	
	/* This block uses the control key to make the object selected by its center point rather than its contents */
	/* in editmode do not activate */
	if (obcenter) {
		
		/* note; shift+alt goes to group-flush-selecting */
		if (enumerate) {
			basact = object_mouse_select_menu(C, &vc, NULL, 0, mval, toggle);
		}
		else {
			base = startbase;
			while (base) {
				if (BASE_SELECTABLE(v3d, base)) {
					float screen_co[2];
					if (ED_view3d_project_float_global(ar, base->object->obmat[3], screen_co,
					                                   V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_WIN | V3D_PROJ_TEST_CLIP_NEAR) == V3D_PROJ_RET_OK)
					{
						float dist_temp = len_manhattan_v2v2(mval_fl, screen_co);
						if (base == BASACT) dist_temp += 10.0f;
						if (dist_temp < dist) {
							dist = dist_temp;
							basact = base;
						}
					}
				}
				base = base->next;
				
				if (base == NULL) base = FIRSTBASE;
				if (base == startbase) break;
			}
		}
	}
	else {
		unsigned int buffer[4 * MAXPICKBUF];
		bool do_nearest;

		/* if objects have posemode set, the bones are in the same selection buffer */
		
		hits = mixed_bones_object_selectbuffer(&vc, buffer, mval, &do_nearest, enumerate);
		
		if (hits > 0) {
			/* note: bundles are handling in the same way as bones */
			const bool has_bones = selectbuffer_has_bones(buffer, hits);

			/* note; shift+alt goes to group-flush-selecting */
			if (has_bones == 0 && enumerate) {
				basact = object_mouse_select_menu(C, &vc, buffer, hits, mval, toggle);
			}
			else {
				basact = mouse_select_eval_buffer(&vc, buffer, hits, startbase, has_bones, do_nearest);
			}
			
			if (has_bones && basact) {
				if (basact->object->type == OB_CAMERA) {
					if (BASACT == basact) {
						int i, hitresult;
						bool changed = false;

						for (i = 0; i < hits; i++) {
							hitresult = buffer[3 + (i * 4)];

							/* if there's bundles in buffer select bundles first,
							 * so non-camera elements should be ignored in buffer */
							if (basact->selcol != (hitresult & 0xFFFF)) {
								continue;
							}

							/* index of bundle is 1<<16-based. if there's no "bone" index
							 * in height word, this buffer value belongs to camera. not to bundle */
							if (buffer[4 * i + 3] & 0xFFFF0000) {
								MovieClip *clip = BKE_object_movieclip_get(scene, basact->object, false);
								MovieTracking *tracking = &clip->tracking;
								ListBase *tracksbase;
								MovieTrackingTrack *track;

								track = BKE_tracking_track_get_indexed(&clip->tracking, hitresult >> 16, &tracksbase);

								if (TRACK_SELECTED(track) && extend) {
									changed = false;
									BKE_tracking_track_deselect(track, TRACK_AREA_ALL);
								}
								else {
									int oldsel = TRACK_SELECTED(track) ? 1 : 0;
									if (!extend)
										deselect_all_tracks(tracking);

									BKE_tracking_track_select(tracksbase, track, TRACK_AREA_ALL, extend);

									if (oldsel != (TRACK_SELECTED(track) ? 1 : 0))
										changed = true;
								}

								basact->flag |= SELECT;
								basact->object->flag = basact->flag;

								retval = true;

								WM_event_add_notifier(C, NC_MOVIECLIP | ND_SELECT, track);
								WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);

								break;
							}
						}

						if (!changed) {
							/* fallback to regular object selection if no new bundles were selected,
							 * allows to select object parented to reconstruction object */
							basact = mouse_select_eval_buffer(&vc, buffer, hits, startbase, 0, do_nearest);
						}
					}
				}
				else if (ED_do_pose_selectbuffer(scene, basact, buffer, hits, extend, deselect, toggle, do_nearest)) {
					/* then bone is found */
				
					/* we make the armature selected: 
					 * not-selected active object in posemode won't work well for tools */
					basact->flag |= SELECT;
					basact->object->flag = basact->flag;
					
					retval = true;
					WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, basact->object);
					WM_event_add_notifier(C, NC_OBJECT | ND_BONE_ACTIVE, basact->object);
					
					/* in weightpaint, we use selected bone to select vertexgroup, so no switch to new active object */
					if (BASACT && (BASACT->object->mode & OB_MODE_WEIGHT_PAINT)) {
						/* prevent activating */
						basact = NULL;
					}

				}
				/* prevent bone selecting to pass on to object selecting */
				if (basact == BASACT)
					basact = NULL;
			}
		}
	}
	
	/* so, do we have something selected? */
	if (basact) {
		retval = true;
		
		if (vc.obedit) {
			/* only do select */
			deselectall_except(scene, basact);
			ED_base_object_select(basact, BA_SELECT);
		}
		/* also prevent making it active on mouse selection */
		else if (BASE_SELECTABLE(v3d, basact)) {

			oldbasact = BASACT;
			
			if (extend) {
				ED_base_object_select(basact, BA_SELECT);
			}
			else if (deselect) {
				ED_base_object_select(basact, BA_DESELECT);
			}
			else if (toggle) {
				if (basact->flag & SELECT) {
					if (basact == oldbasact) {
						ED_base_object_select(basact, BA_DESELECT);
					}
				}
				else {
					ED_base_object_select(basact, BA_SELECT);
				}
			}
			else {
				deselectall_except(scene, basact);
				ED_base_object_select(basact, BA_SELECT);
			}

			if ((oldbasact != basact) && (is_obedit == false)) {
				ED_base_object_activate(C, basact); /* adds notifier */
			}
		}

		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	}

	return retval;
}

/* ********************  border and circle ************************************** */

typedef struct BoxSelectUserData {
	ViewContext *vc;
	const rcti *rect;
	const rctf *rect_fl;
	rctf       _rect_fl;
	bool select;

	/* runtime */
	int pass;
	bool is_done;
	bool is_changed;
} BoxSelectUserData;

static void view3d_userdata_boxselect_init(BoxSelectUserData *r_data,
                                           ViewContext *vc, const rcti *rect, const bool select)
{
	r_data->vc = vc;

	r_data->rect = rect;
	r_data->rect_fl = &r_data->_rect_fl;
	BLI_rctf_rcti_copy(&r_data->_rect_fl, rect);

	r_data->select = select;

	/* runtime */
	r_data->pass = 0;
	r_data->is_done = false;
	r_data->is_changed = false;
}

bool edge_inside_circle(const float cent[2], float radius, const float screen_co_a[2], const float screen_co_b[2])
{
	const float radius_squared = radius * radius;
	return (dist_squared_to_line_segment_v2(cent, screen_co_a, screen_co_b) < radius_squared);
}

static void do_paintvert_box_select__doSelectVert(void *userData, MVert *mv, const float screen_co[2], int UNUSED(index))
{
	BoxSelectUserData *data = userData;

	if (BLI_rctf_isect_pt_v(data->rect_fl, screen_co)) {
		BKE_BIT_TEST_SET(mv->flag, data->select, SELECT);
	}
}
static int do_paintvert_box_select(ViewContext *vc, rcti *rect, bool select, bool extend)
{
	const int use_zbuf = (vc->v3d->flag & V3D_ZBUF_SELECT);
	Mesh *me;
	MVert *mvert;
	struct ImBuf *ibuf;
	unsigned int *rt;
	int a, index;
	char *selar;
	int sx = BLI_rcti_size_x(rect) + 1;
	int sy = BLI_rcti_size_y(rect) + 1;

	me = vc->obact->data;

	if (me == NULL || me->totvert == 0 || sx * sy <= 0)
		return OPERATOR_CANCELLED;


	if (extend == false && select)
		paintvert_deselect_all_visible(vc->obact, SEL_DESELECT, false);

	if (use_zbuf) {
		selar = MEM_callocN(me->totvert + 1, "selar");
		view3d_validate_backbuf(vc);

		ibuf = IMB_allocImBuf(sx, sy, 32, IB_rect);
		rt = ibuf->rect;
		glReadPixels(rect->xmin + vc->ar->winrct.xmin,  rect->ymin + vc->ar->winrct.ymin, sx, sy, GL_RGBA, GL_UNSIGNED_BYTE,  ibuf->rect);
		if (ENDIAN_ORDER == B_ENDIAN) IMB_convert_rgba_to_abgr(ibuf);

		a = sx * sy;
		while (a--) {
			if (*rt) {
				index = WM_framebuffer_to_index(*rt);
				if (index <= me->totvert) selar[index] = 1;
			}
			rt++;
		}

		mvert = me->mvert;
		for (a = 1; a <= me->totvert; a++, mvert++) {
			if (selar[a]) {
				if ((mvert->flag & ME_HIDE) == 0) {
					if (select) mvert->flag |=  SELECT;
					else        mvert->flag &= ~SELECT;
				}
			}
		}

		IMB_freeImBuf(ibuf);
		MEM_freeN(selar);

#ifdef __APPLE__
		glReadBuffer(GL_BACK);
#endif
	}
	else {
		BoxSelectUserData data;

		view3d_userdata_boxselect_init(&data, vc, rect, select);

		ED_view3d_init_mats_rv3d(vc->obact, vc->rv3d);

		meshobject_foreachScreenVert(vc, do_paintvert_box_select__doSelectVert, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
	}

	if (select == false) {
		BKE_mesh_mselect_validate(me);
	}
	paintvert_flush_flags(vc->obact);

	return OPERATOR_FINISHED;
}

static void do_nurbs_box_select__doSelect(void *userData, Nurb *UNUSED(nu), BPoint *bp, BezTriple *bezt, int beztindex, const float screen_co[2])
{
	BoxSelectUserData *data = userData;
	Object *obedit = data->vc->obedit;
	Curve *cu = (Curve *)obedit->data;

	if (BLI_rctf_isect_pt_v(data->rect_fl, screen_co)) {
		if (bp) {
			bp->f1 = data->select ? (bp->f1 | SELECT) : (bp->f1 & ~SELECT);
		}
		else {
			if (cu->drawflag & CU_HIDE_HANDLES) {
				/* can only be (beztindex == 0) here since handles are hidden */
				bezt->f1 = bezt->f2 = bezt->f3 = data->select ? (bezt->f2 | SELECT) : (bezt->f2 & ~SELECT);
			}
			else {
				if (beztindex == 0) {
					bezt->f1 = data->select ? (bezt->f1 | SELECT) : (bezt->f1 & ~SELECT);
				}
				else if (beztindex == 1) {
					bezt->f2 = data->select ? (bezt->f2 | SELECT) : (bezt->f2 & ~SELECT);
				}
				else {
					bezt->f3 = data->select ? (bezt->f3 | SELECT) : (bezt->f3 & ~SELECT);
				}
			}
		}
	}
}
static int do_nurbs_box_select(ViewContext *vc, rcti *rect, bool select, bool extend)
{
	BoxSelectUserData data;
	
	view3d_userdata_boxselect_init(&data, vc, rect, select);

	if (extend == false && select) {
		Curve *curve = (Curve *) vc->obedit->data;
		ED_curve_deselect_all(curve->editnurb);
	}

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
	nurbs_foreachScreenVert(vc, do_nurbs_box_select__doSelect, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
	BKE_curve_nurb_vert_active_validate(vc->obedit->data);

	return OPERATOR_FINISHED;
}

static void do_lattice_box_select__doSelect(void *userData, BPoint *bp, const float screen_co[2])
{
	BoxSelectUserData *data = userData;

	if (BLI_rctf_isect_pt_v(data->rect_fl, screen_co)) {
		bp->f1 = data->select ? (bp->f1 | SELECT) : (bp->f1 & ~SELECT);
	}
}
static int do_lattice_box_select(ViewContext *vc, rcti *rect, bool select, bool extend)
{
	BoxSelectUserData data;

	view3d_userdata_boxselect_init(&data, vc, rect, select);

	if (extend == false && select)
		ED_setflagsLatt(vc->obedit, 0);

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
	lattice_foreachScreenVert(vc, do_lattice_box_select__doSelect, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
	
	return OPERATOR_FINISHED;
}

static void do_mesh_box_select__doSelectVert(void *userData, BMVert *eve, const float screen_co[2], int UNUSED(index))
{
	BoxSelectUserData *data = userData;

	if (BLI_rctf_isect_pt_v(data->rect_fl, screen_co)) {
		BM_vert_select_set(data->vc->em->bm, eve, data->select);
	}
}
static void do_mesh_box_select__doSelectEdge(void *userData, BMEdge *eed, const float screen_co_a[2], const float screen_co_b[2], int index)
{
	BoxSelectUserData *data = userData;

	if (EDBM_backbuf_check(bm_solidoffs + index)) {
		if (data->pass == 0) {
			if (edge_fully_inside_rect(data->rect_fl, screen_co_a, screen_co_b)) {
				BM_edge_select_set(data->vc->em->bm, eed, data->select);
				data->is_done = true;
			}
		}
		else {
			if (edge_inside_rect(data->rect_fl, screen_co_a, screen_co_b)) {
				BM_edge_select_set(data->vc->em->bm, eed, data->select);
			}
		}
	}
}
static void do_mesh_box_select__doSelectFace(void *userData, BMFace *efa, const float screen_co[2], int UNUSED(index))
{
	BoxSelectUserData *data = userData;

	if (BLI_rctf_isect_pt_v(data->rect_fl, screen_co)) {
		BM_face_select_set(data->vc->em->bm, efa, data->select);
	}
}
static int do_mesh_box_select(ViewContext *vc, rcti *rect, bool select, bool extend)
{
	BoxSelectUserData data;
	ToolSettings *ts = vc->scene->toolsettings;
	int bbsel;
	
	view3d_userdata_boxselect_init(&data, vc, rect, select);

	if (extend == false && select)
		EDBM_flag_disable_all(vc->em, BM_ELEM_SELECT);

	/* for non zbuf projections, don't change the GL state */
	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

	glLoadMatrixf(vc->rv3d->viewmat);
	bbsel = EDBM_backbuf_border_init(vc, rect->xmin, rect->ymin, rect->xmax, rect->ymax);

	if (ts->selectmode & SCE_SELECT_VERTEX) {
		if (bbsel) {
			edbm_backbuf_check_and_select_verts(vc->em, select);
		}
		else {
			mesh_foreachScreenVert(vc, do_mesh_box_select__doSelectVert, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
		}
	}
	if (ts->selectmode & SCE_SELECT_EDGE) {
		/* Does both bbsel and non-bbsel versions (need screen cos for both) */

		data.pass = 0;
		mesh_foreachScreenEdge(vc, do_mesh_box_select__doSelectEdge, &data, V3D_PROJ_TEST_CLIP_NEAR);

		if (data.is_done == 0) {
			data.pass = 1;
			mesh_foreachScreenEdge(vc, do_mesh_box_select__doSelectEdge, &data, V3D_PROJ_TEST_CLIP_NEAR);
		}
	}
	
	if (ts->selectmode & SCE_SELECT_FACE) {
		if (bbsel) {
			edbm_backbuf_check_and_select_faces(vc->em, select);
		}
		else {
			mesh_foreachScreenFace(vc, do_mesh_box_select__doSelectFace, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
		}
	}
	
	EDBM_backbuf_free();
		
	EDBM_selectmode_flush(vc->em);
	
	return OPERATOR_FINISHED;
}

static int do_meta_box_select(ViewContext *vc, rcti *rect, bool select, bool extend)
{
	MetaBall *mb = (MetaBall *)vc->obedit->data;
	MetaElem *ml;
	int a;

	unsigned int buffer[4 * MAXPICKBUF];
	short hits;

	hits = view3d_opengl_select(vc, buffer, MAXPICKBUF, rect, false);

	if (extend == false && select)
		BKE_mball_deselect_all(mb);
	
	for (ml = mb->editelems->first; ml; ml = ml->next) {
		for (a = 0; a < hits; a++) {
			if (ml->selcol1 == buffer[(4 * a) + 3]) {
				ml->flag |= MB_SCALE_RAD;
				if (select) ml->flag |= SELECT;
				else ml->flag &= ~SELECT;
				break;
			}
			if (ml->selcol2 == buffer[(4 * a) + 3]) {
				ml->flag &= ~MB_SCALE_RAD;
				if (select) ml->flag |= SELECT;
				else ml->flag &= ~SELECT;
				break;
			}
		}
	}

	return OPERATOR_FINISHED;
}

static int do_armature_box_select(ViewContext *vc, rcti *rect, bool select, bool extend)
{
	bArmature *arm = vc->obedit->data;
	EditBone *ebone;
	int a;

	unsigned int buffer[4 * MAXPICKBUF];
	short hits;

	hits = view3d_opengl_select(vc, buffer, MAXPICKBUF, rect, false);
	
	/* clear flag we use to detect point was affected */
	for (ebone = arm->edbo->first; ebone; ebone = ebone->next)
		ebone->flag &= ~BONE_DONE;
	
	if (extend == false && select)
		ED_armature_deselect_all_visible(vc->obedit);

	/* first we only check points inside the border */
	for (a = 0; a < hits; a++) {
		int index = buffer[(4 * a) + 3];
		if (index != -1) {
			ebone = BLI_findlink(arm->edbo, index & ~(BONESEL_ANY));
			if ((select == false) || ((ebone->flag & BONE_UNSELECTABLE) == 0)) {
				if (index & BONESEL_TIP) {
					ebone->flag |= BONE_DONE;
					if (select) ebone->flag |= BONE_TIPSEL;
					else ebone->flag &= ~BONE_TIPSEL;
				}
				
				if (index & BONESEL_ROOT) {
					ebone->flag |= BONE_DONE;
					if (select) ebone->flag |= BONE_ROOTSEL;
					else ebone->flag &= ~BONE_ROOTSEL;
				}
			}
		}
	}
	
	/* now we have to flush tag from parents... */
	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
			if (ebone->parent->flag & BONE_DONE)
				ebone->flag |= BONE_DONE;
		}
	}
	
	/* only select/deselect entire bones when no points where in the rect */
	for (a = 0; a < hits; a++) {
		int index = buffer[(4 * a) + 3];
		if (index != -1) {
			ebone = BLI_findlink(arm->edbo, index & ~(BONESEL_ANY));
			if (index & BONESEL_BONE) {
				if ((select == false) || ((ebone->flag & BONE_UNSELECTABLE) == 0)) {
					if (!(ebone->flag & BONE_DONE)) {
						if (select)
							ebone->flag |= (BONE_ROOTSEL | BONE_TIPSEL | BONE_SELECTED);
						else
							ebone->flag &= ~(BONE_ROOTSEL | BONE_TIPSEL | BONE_SELECTED);
					}
				}
			}
		}
	}
	
	ED_armature_sync_selection(arm->edbo);
	
	return hits > 0 ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static int do_object_pose_box_select(bContext *C, ViewContext *vc, rcti *rect, bool select, bool extend)
{
	Bone *bone;
	Object *ob = vc->obact;
	unsigned int *vbuffer = NULL; /* selection buffer	*/
	unsigned int *col;          /* color in buffer	*/
	int bone_only;
	int bone_selected = 0;
	int totobj = MAXPICKBUF; /* XXX solve later */
	short hits;
	
	if ((ob) && (ob->mode & OB_MODE_POSE))
		bone_only = 1;
	else
		bone_only = 0;
	
	if (extend == false && select) {
		if (bone_only) {
			CTX_DATA_BEGIN (C, bPoseChannel *, pchan, visible_pose_bones)
			{
				if ((select == false) || ((pchan->bone->flag & BONE_UNSELECTABLE) == 0)) {
					pchan->bone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
				}
			}
			CTX_DATA_END;
		}
		else {
			object_deselect_all_visible(vc->scene, vc->v3d);
		}
	}

	/* selection buffer now has bones potentially too, so we add MAXPICKBUF */
	vbuffer = MEM_mallocN(4 * (totobj + MAXPICKBUF) * sizeof(unsigned int), "selection buffer");
	hits = view3d_opengl_select(vc, vbuffer, 4 * (totobj + MAXPICKBUF), rect, false);
	/*
	 * LOGIC NOTES (theeth):
	 * The buffer and ListBase have the same relative order, which makes the selection
	 * very simple. Loop through both data sets at the same time, if the color
	 * is the same as the object, we have a hit and can move to the next color
	 * and object pair, if not, just move to the next object,
	 * keeping the same color until we have a hit.
	 * 
	 * The buffer order is defined by OGL standard, hopefully no stupid GFX card
	 * does it incorrectly.
	 */

	if (hits > 0) { /* no need to loop if there's no hit */
		Base *base;
		col = vbuffer + 3;
		
		for (base = vc->scene->base.first; base && hits; base = base->next) {
			if (BASE_SELECTABLE(vc->v3d, base)) {
				while (base->selcol == (*col & 0xFFFF)) {   /* we got an object */
					if (*col & 0xFFFF0000) {                    /* we got a bone */
						bone = get_indexed_bone(base->object, *col & ~(BONESEL_ANY));
						if (bone) {
							if (select) {
								if ((bone->flag & BONE_UNSELECTABLE) == 0) {
									bone->flag |= BONE_SELECTED;
									bone_selected = 1;
								}
							}
							else {
								bArmature *arm = base->object->data;
								bone->flag &= ~BONE_SELECTED;
								if (arm->act_bone == bone)
									arm->act_bone = NULL;
							}
						}
					}
					else if (!bone_only) {
						ED_base_object_select(base, select ? BA_SELECT : BA_DESELECT);
					}
					
					col += 4; /* next color */
					hits--;
					if (hits == 0) break;
				}
			}
			
			if (bone_selected) {
				if (base->object && (base->object->type == OB_ARMATURE)) {
					bArmature *arm = base->object->data;
					
					WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, base->object);
					
					if (arm && (arm->flag & ARM_HAS_VIZ_DEPS)) {
						/* mask modifier ('armature' mode), etc. */
						DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
					}
				}
			}
		}
		
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, vc->scene);
	}
	MEM_freeN(vbuffer);

	return hits > 0 ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static int view3d_borderselect_exec(bContext *C, wmOperator *op)
{
	ViewContext vc;
	rcti rect;
	bool extend;
	bool select;

	int ret = OPERATOR_CANCELLED;

	view3d_operator_needs_opengl(C);

	/* setup view context for argument to callbacks */
	view3d_set_viewcontext(C, &vc);
	
	select = (RNA_int_get(op->ptr, "gesture_mode") == GESTURE_MODAL_SELECT);
	WM_operator_properties_border_to_rcti(op, &rect);
	extend = RNA_boolean_get(op->ptr, "extend");

	if (vc.obedit) {
		switch (vc.obedit->type) {
			case OB_MESH:
				vc.em = BKE_editmesh_from_object(vc.obedit);
				ret = do_mesh_box_select(&vc, &rect, select, extend);
//			if (EM_texFaceCheck())
				if (ret & OPERATOR_FINISHED) {
					WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit->data);
				}
				break;
			case OB_CURVE:
			case OB_SURF:
				ret = do_nurbs_box_select(&vc, &rect, select, extend);
				if (ret & OPERATOR_FINISHED) {
					WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit->data);
				}
				break;
			case OB_MBALL:
				ret = do_meta_box_select(&vc, &rect, select, extend);
				if (ret & OPERATOR_FINISHED) {
					WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit->data);
				}
				break;
			case OB_ARMATURE:
				ret = do_armature_box_select(&vc, &rect, select, extend);
				if (ret & OPERATOR_FINISHED) {
					WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, vc.obedit);
				}
				break;
			case OB_LATTICE:
				ret = do_lattice_box_select(&vc, &rect, select, extend);
				if (ret & OPERATOR_FINISHED) {
					WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit->data);
				}
				break;
			default:
				assert(!"border select on incorrect object type");
				break;
		}
	}
	else {  /* no editmode, unified for bones and objects */
		if (vc.obact && vc.obact->mode & OB_MODE_SCULPT) {
			ret = ED_sculpt_mask_box_select(C, &vc, &rect, select, extend);
		}
		else if (vc.obact && BKE_paint_select_face_test(vc.obact)) {
			ret = do_paintface_box_select(&vc, &rect, select, extend);
		}
		else if (vc.obact && BKE_paint_select_vert_test(vc.obact)) {
			ret = do_paintvert_box_select(&vc, &rect, select, extend);
		}
		else if (vc.obact && vc.obact->mode & OB_MODE_PARTICLE_EDIT) {
			ret = PE_border_select(C, &rect, select, extend);
		}
		else { /* object mode with none active */
			ret = do_object_pose_box_select(C, &vc, &rect, select, extend);
		}
	}

	return ret;
} 


/* *****************Selection Operators******************* */

/* ****** Border Select ****** */
void VIEW3D_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Border Select";
	ot->description = "Select items using border selection";
	ot->idname = "VIEW3D_OT_select_border";
	
	/* api callbacks */
	ot->invoke = WM_border_select_invoke;
	ot->exec = view3d_borderselect_exec;
	ot->modal = WM_border_select_modal;
	ot->poll = view3d_selectable_data;
	ot->cancel = WM_border_select_cancel;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	/* rna */
	WM_operator_properties_gesture_border(ot, true);
}


/* mouse selection in weight paint */
/* gets called via generic mouse select operator */
static bool mouse_weight_paint_vertex_select(bContext *C, const int mval[2], bool extend, bool deselect, bool toggle, Object *obact)
{
	View3D *v3d = CTX_wm_view3d(C);
	const int use_zbuf = (v3d->flag & V3D_ZBUF_SELECT);

	Mesh *me = obact->data; /* already checked for NULL */
	unsigned int index = 0;
	MVert *mv;

	if (ED_mesh_pick_vert(C, obact, mval, &index, ED_MESH_PICK_DEFAULT_VERT_SIZE, use_zbuf)) {
		mv = &me->mvert[index];
		if (extend) {
			mv->flag |= SELECT;
		}
		else if (deselect) {
			mv->flag &= ~SELECT;
		}
		else if (toggle) {
			mv->flag ^= SELECT;
		}
		else {
			paintvert_deselect_all_visible(obact, SEL_DESELECT, false);
			mv->flag |= SELECT;
		}

		/* update mselect */
		if (mv->flag & SELECT) {
			BKE_mesh_mselect_active_set(me, index, ME_VSEL);
		}
		else {
			BKE_mesh_mselect_validate(me);
		}

		paintvert_flush_flags(obact);
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obact->data);
		return true;
	}
	return false;
}

/* ****** Mouse Select ****** */


static int view3d_select_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Object *obact = CTX_data_active_object(C);
	bool extend = RNA_boolean_get(op->ptr, "extend");
	bool deselect = RNA_boolean_get(op->ptr, "deselect");
	bool toggle = RNA_boolean_get(op->ptr, "toggle");
	bool center = RNA_boolean_get(op->ptr, "center");
	bool enumerate = RNA_boolean_get(op->ptr, "enumerate");
	/* only force object select for editmode to support vertex parenting,
	 * or paint-select to allow pose bone select with vert/face select */
	bool object = (RNA_boolean_get(op->ptr, "object") &&
	               (obedit ||
	                BKE_paint_select_elem_test(obact) ||
	                /* so its possible to select bones in weightpaint mode (LMB select) */
	                (obact && (obact->mode & OB_MODE_WEIGHT_PAINT) && BKE_object_pose_armature_get(obact))));

	bool retval = false;
	int location[2];

	RNA_int_get_array(op->ptr, "location", location);

	view3d_operator_needs_opengl(C);

	if (object) {
		obedit = NULL;
		obact = NULL;

		/* ack, this is incorrect but to do this correctly we would need an
		 * alternative editmode/objectmode keymap, this copies the functionality
		 * from 2.4x where Ctrl+Select in editmode does object select only */
		center = false;
	}

	if (obedit && object == false) {
		if (obedit->type == OB_MESH)
			retval = EDBM_select_pick(C, location, extend, deselect, toggle);
		else if (obedit->type == OB_ARMATURE)
			retval = mouse_armature(C, location, extend, deselect, toggle);
		else if (obedit->type == OB_LATTICE)
			retval = mouse_lattice(C, location, extend, deselect, toggle);
		else if (ELEM(obedit->type, OB_CURVE, OB_SURF))
			retval = mouse_nurb(C, location, extend, deselect, toggle);
		else if (obedit->type == OB_MBALL)
			retval = mouse_mball(C, location, extend, deselect, toggle);
		else if (obedit->type == OB_FONT)
			retval = mouse_font(C, location, extend, deselect, toggle);
			
	}
	else if (obact && obact->mode & OB_MODE_PARTICLE_EDIT)
		return PE_mouse_particles(C, location, extend, deselect, toggle);
	else if (obact && BKE_paint_select_face_test(obact))
		retval = paintface_mouse_select(C, obact, location, extend, deselect, toggle);
	else if (BKE_paint_select_vert_test(obact))
		retval = mouse_weight_paint_vertex_select(C, location, extend, deselect, toggle, obact);
	else
		retval = mouse_select(C, location, extend, deselect, toggle, center, enumerate, object);

	/* passthrough allows tweaks
	 * FINISHED to signal one operator worked
	 * */
	if (retval)
		return OPERATOR_PASS_THROUGH | OPERATOR_FINISHED;
	else
		return OPERATOR_PASS_THROUGH;  /* nothing selected, just passthrough */
}

static int view3d_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	RNA_int_set_array(op->ptr, "location", event->mval);

	return view3d_select_exec(C, op);
}

void VIEW3D_OT_select(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Activate/Select";
	ot->description = "Activate/select item(s)";
	ot->idname = "VIEW3D_OT_select";
	
	/* api callbacks */
	ot->invoke = view3d_select_invoke;
	ot->exec = view3d_select_exec;
	ot->poll = ED_operator_view3d_active;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	/* properties */
	WM_operator_properties_mouse_select(ot);

	RNA_def_boolean(ot->srna, "center", 0, "Center", "Use the object center when selecting, in editmode used to extend object selection");
	RNA_def_boolean(ot->srna, "enumerate", 0, "Enumerate", "List objects under the mouse (object mode only)");
	RNA_def_boolean(ot->srna, "object", 0, "Object", "Use object selection (editmode only)");

	prop = RNA_def_int_vector(ot->srna, "location", 2, NULL, INT_MIN, INT_MAX, "Location", "Mouse location", INT_MIN, INT_MAX);
	RNA_def_property_flag(prop, PROP_HIDDEN);
}


/* -------------------- circle select --------------------------------------------- */

typedef struct CircleSelectUserData {
	ViewContext *vc;
	bool select;
	int   mval[2];
	float mval_fl[2];
	float radius;
	float radius_squared;

	/* runtime */
	bool is_changed;
} CircleSelectUserData;

static void view3d_userdata_circleselect_init(CircleSelectUserData *r_data,
                                              ViewContext *vc, const bool select, const int mval[2], const float rad)
{
	r_data->vc = vc;
	r_data->select = select;
	copy_v2_v2_int(r_data->mval, mval);
	r_data->mval_fl[0] = mval[0];
	r_data->mval_fl[1] = mval[1];

	r_data->radius = rad;
	r_data->radius_squared = rad * rad;

	/* runtime */
	r_data->is_changed = false;
}

static void mesh_circle_doSelectVert(void *userData, BMVert *eve, const float screen_co[2], int UNUSED(index))
{
	CircleSelectUserData *data = userData;

	if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
		BM_vert_select_set(data->vc->em->bm, eve, data->select);
	}
}
static void mesh_circle_doSelectEdge(void *userData, BMEdge *eed, const float screen_co_a[2], const float screen_co_b[2], int UNUSED(index))
{
	CircleSelectUserData *data = userData;

	if (edge_inside_circle(data->mval_fl, data->radius, screen_co_a, screen_co_b)) {
		BM_edge_select_set(data->vc->em->bm, eed, data->select);
	}
}
static void mesh_circle_doSelectFace(void *userData, BMFace *efa, const float screen_co[2], int UNUSED(index))
{
	CircleSelectUserData *data = userData;

	if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
		BM_face_select_set(data->vc->em->bm, efa, data->select);
	}
}

static void mesh_circle_select(ViewContext *vc, const bool select, const int mval[2], float rad)
{
	ToolSettings *ts = vc->scene->toolsettings;
	int bbsel;
	CircleSelectUserData data;
	
	bbsel = EDBM_backbuf_circle_init(vc, mval[0], mval[1], (short)(rad + 1.0f));
	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */

	vc->em = BKE_editmesh_from_object(vc->obedit);

	view3d_userdata_circleselect_init(&data, vc, select, mval, rad);

	if (ts->selectmode & SCE_SELECT_VERTEX) {
		if (bbsel) {
			edbm_backbuf_check_and_select_verts(vc->em, select);
		}
		else {
			mesh_foreachScreenVert(vc, mesh_circle_doSelectVert, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
		}
	}

	if (ts->selectmode & SCE_SELECT_EDGE) {
		if (bbsel) {
			edbm_backbuf_check_and_select_edges(vc->em, select);
		}
		else {
			mesh_foreachScreenEdge(vc, mesh_circle_doSelectEdge, &data, V3D_PROJ_TEST_CLIP_NEAR);
		}
	}
	
	if (ts->selectmode & SCE_SELECT_FACE) {
		if (bbsel) {
			edbm_backbuf_check_and_select_faces(vc->em, select);
		}
		else {
			mesh_foreachScreenFace(vc, mesh_circle_doSelectFace, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
		}
	}

	EDBM_backbuf_free();
	EDBM_selectmode_flush(vc->em);
}

static void paint_facesel_circle_select(ViewContext *vc, const bool select, const int mval[2], float rad)
{
	Object *ob = vc->obact;
	Mesh *me = ob->data;
	bool bbsel;

	bm_vertoffs = me->totpoly + 1; /* max index array */

	bbsel = EDBM_backbuf_circle_init(vc, mval[0], mval[1], (short)(rad + 1.0f));
	if (bbsel) {
		edbm_backbuf_check_and_select_tfaces(me, select);
		EDBM_backbuf_free();
		paintface_flush_flags(ob);
	}
}

static void paint_vertsel_circle_select_doSelectVert(void *userData, MVert *mv, const float screen_co[2], int UNUSED(index))
{
	CircleSelectUserData *data = userData;

	if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
		BKE_BIT_TEST_SET(mv->flag, data->select, SELECT);
	}
}
static void paint_vertsel_circle_select(ViewContext *vc, const bool select, const int mval[2], float rad)
{
	const int use_zbuf = (vc->v3d->flag & V3D_ZBUF_SELECT);
	Object *ob = vc->obact;
	Mesh *me = ob->data;
	bool bbsel;
	/* CircleSelectUserData data = {NULL}; */ /* UNUSED */

	if (use_zbuf) {
		bm_vertoffs = me->totvert + 1; /* max index array */

		bbsel = EDBM_backbuf_circle_init(vc, mval[0], mval[1], (short)(rad + 1.0f));
		if (bbsel) {
			edbm_backbuf_check_and_select_verts_obmode(me, select);
			EDBM_backbuf_free();
		}
	}
	else {
		CircleSelectUserData data;

		ED_view3d_init_mats_rv3d(vc->obact, vc->rv3d); /* for foreach's screen/vert projection */

		view3d_userdata_circleselect_init(&data, vc, select, mval, rad);
		meshobject_foreachScreenVert(vc, paint_vertsel_circle_select_doSelectVert, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
	}

	if (select != LEFTMOUSE) {
		BKE_mesh_mselect_validate(me);
	}
	paintvert_flush_flags(ob);
}


static void nurbscurve_circle_doSelect(void *userData, Nurb *UNUSED(nu), BPoint *bp, BezTriple *bezt, int beztindex, const float screen_co[2])
{
	CircleSelectUserData *data = userData;
	Object *obedit = data->vc->obedit;
	Curve *cu = (Curve *)obedit->data;

	if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
		if (bp) {
			bp->f1 = data->select ? (bp->f1 | SELECT) : (bp->f1 & ~SELECT);
		}
		else {
			if (cu->drawflag & CU_HIDE_HANDLES) {
				/* can only be (beztindex == 0) here since handles are hidden */
				bezt->f1 = bezt->f2 = bezt->f3 = data->select ? (bezt->f2 | SELECT) : (bezt->f2 & ~SELECT);
			}
			else {
				if (beztindex == 0) {
					bezt->f1 = data->select ? (bezt->f1 | SELECT) : (bezt->f1 & ~SELECT);
				}
				else if (beztindex == 1) {
					bezt->f2 = data->select ? (bezt->f2 | SELECT) : (bezt->f2 & ~SELECT);
				}
				else {
					bezt->f3 = data->select ? (bezt->f3 | SELECT) : (bezt->f3 & ~SELECT);
				}
			}
		}
	}
}
static void nurbscurve_circle_select(ViewContext *vc, const bool select, const int mval[2], float rad)
{
	CircleSelectUserData data;

	view3d_userdata_circleselect_init(&data, vc, select, mval, rad);

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
	nurbs_foreachScreenVert(vc, nurbscurve_circle_doSelect, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
	BKE_curve_nurb_vert_active_validate(vc->obedit->data);
}


static void latticecurve_circle_doSelect(void *userData, BPoint *bp, const float screen_co[2])
{
	CircleSelectUserData *data = userData;

	if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
		bp->f1 = data->select ? (bp->f1 | SELECT) : (bp->f1 & ~SELECT);
	}
}
static void lattice_circle_select(ViewContext *vc, const bool select, const int mval[2], float rad)
{
	CircleSelectUserData data;

	view3d_userdata_circleselect_init(&data, vc, select, mval, rad);

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
	lattice_foreachScreenVert(vc, latticecurve_circle_doSelect, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
}


/* NOTE: pose-bone case is copied from editbone case... */
static short pchan_circle_doSelectJoint(void *userData, bPoseChannel *pchan, const float screen_co[2])
{
	CircleSelectUserData *data = userData;

	if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
		if (data->select)
			pchan->bone->flag |= BONE_SELECTED;
		else
			pchan->bone->flag &= ~BONE_SELECTED;
		return 1;
	}
	return 0;
}
static void do_circle_select_pose__doSelectBone(void *userData, struct bPoseChannel *pchan, const float screen_co_a[2], const float screen_co_b[2])
{
	CircleSelectUserData *data = userData;
	bArmature *arm = data->vc->obact->data;

	if (PBONE_SELECTABLE(arm, pchan->bone)) {
		bool is_point_done = false;
		int points_proj_tot = 0;

		/* project head location to screenspace */
		if (screen_co_a[0] != IS_CLIPPED) {
			points_proj_tot++;
			if (pchan_circle_doSelectJoint(data, pchan, screen_co_a)) {
				is_point_done = true;
			}
		}

		/* project tail location to screenspace */
		if (screen_co_b[0] != IS_CLIPPED) {
			points_proj_tot++;
			if (pchan_circle_doSelectJoint(data, pchan, screen_co_b)) {
				is_point_done = true;
			}
		}

		/* check if the head and/or tail is in the circle
		 * - the call to check also does the selection already
		 */

		/* only if the endpoints didn't get selected, deal with the middle of the bone too
		 * It works nicer to only do this if the head or tail are not in the circle,
		 * otherwise there is no way to circle select joints alone */
		if ((is_point_done == false) && (points_proj_tot == 2) &&
		    edge_inside_circle(data->mval_fl, data->radius, screen_co_a, screen_co_b))
		{
			if (data->select) pchan->bone->flag |= BONE_SELECTED;
			else              pchan->bone->flag &= ~BONE_SELECTED;
			data->is_changed = true;
		}

		data->is_changed |= is_point_done;
	}
}
static void pose_circle_select(ViewContext *vc, const bool select, const int mval[2], float rad)
{
	CircleSelectUserData data;
	
	view3d_userdata_circleselect_init(&data, vc, select, mval, rad);

	ED_view3d_init_mats_rv3d(vc->obact, vc->rv3d); /* for foreach's screen/vert projection */
	
	pose_foreachScreenBone(vc, do_circle_select_pose__doSelectBone, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

	if (data.is_changed) {
		bArmature *arm = vc->obact->data;

		WM_main_add_notifier(NC_OBJECT | ND_BONE_SELECT, vc->obact);

		if (arm->flag & ARM_HAS_VIZ_DEPS) {
			/* mask modifier ('armature' mode), etc. */
			DAG_id_tag_update(&vc->obact->id, OB_RECALC_DATA);
		}
	}
}

static short armature_circle_doSelectJoint(void *userData, EditBone *ebone, const float screen_co[2], short head)
{
	CircleSelectUserData *data = userData;

	if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
		if (head) {
			if (data->select)
				ebone->flag |= BONE_ROOTSEL;
			else 
				ebone->flag &= ~BONE_ROOTSEL;
		}
		else {
			if (data->select)
				ebone->flag |= BONE_TIPSEL;
			else 
				ebone->flag &= ~BONE_TIPSEL;
		}
		return 1;
	}
	return 0;
}
static void do_circle_select_armature__doSelectBone(void *userData, struct EditBone *ebone, const float screen_co_a[2], const float screen_co_b[2])
{
	CircleSelectUserData *data = userData;
	bArmature *arm = data->vc->obedit->data;

	if (data->select ? EBONE_SELECTABLE(arm, ebone) : EBONE_VISIBLE(arm, ebone)) {
		bool is_point_done = false;
		int points_proj_tot = 0;

		/* project head location to screenspace */
		if (screen_co_a[0] != IS_CLIPPED) {
			points_proj_tot++;
			if (armature_circle_doSelectJoint(data, ebone, screen_co_a, true)) {
				is_point_done = true;
			}
		}

		/* project tail location to screenspace */
		if (screen_co_b[0] != IS_CLIPPED) {
			points_proj_tot++;
			if (armature_circle_doSelectJoint(data, ebone, screen_co_b, false)) {
				is_point_done = true;
			}
		}

		/* check if the head and/or tail is in the circle
		 * - the call to check also does the selection already
		 */

		/* only if the endpoints didn't get selected, deal with the middle of the bone too
		 * It works nicer to only do this if the head or tail are not in the circle,
		 * otherwise there is no way to circle select joints alone */
		if ((is_point_done == false) && (points_proj_tot == 2) &&
		    edge_inside_circle(data->mval_fl, data->radius, screen_co_a, screen_co_b))
		{
			if (data->select) ebone->flag |=  (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
			else              ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
			data->is_changed = true;
		}

		data->is_changed |= is_point_done;
	}
}
static void armature_circle_select(ViewContext *vc, const bool select, const int mval[2], float rad)
{
	CircleSelectUserData data;
	bArmature *arm = vc->obedit->data;

	view3d_userdata_circleselect_init(&data, vc, select, mval, rad);

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

	armature_foreachScreenBone(vc, do_circle_select_armature__doSelectBone, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

	if (data.is_changed) {
		ED_armature_sync_selection(arm->edbo);
		ED_armature_validate_active(arm);
		WM_main_add_notifier(NC_OBJECT | ND_BONE_SELECT, vc->obedit);
	}
}

static void do_circle_select_mball__doSelectElem(void *userData, struct MetaElem *ml, const float screen_co[2])
{
	CircleSelectUserData *data = userData;

	if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
		if (data->select) ml->flag |=  SELECT;
		else              ml->flag &= ~SELECT;
		data->is_changed = true;
	}
}
static void mball_circle_select(ViewContext *vc, const bool select, const int mval[2], float rad)
{
	CircleSelectUserData data;

	view3d_userdata_circleselect_init(&data, vc, select, mval, rad);

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

	mball_foreachScreenElem(vc, do_circle_select_mball__doSelectElem, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
}

/** Callbacks for circle selection in Editmode */

static void obedit_circle_select(ViewContext *vc, const bool select, const int mval[2], float rad)
{
	switch (vc->obedit->type) {
		case OB_MESH:
			mesh_circle_select(vc, select, mval, rad);
			break;
		case OB_CURVE:
		case OB_SURF:
			nurbscurve_circle_select(vc, select, mval, rad);
			break;
		case OB_LATTICE:
			lattice_circle_select(vc, select, mval, rad);
			break;
		case OB_ARMATURE:
			armature_circle_select(vc, select, mval, rad);
			break;
		case OB_MBALL:
			mball_circle_select(vc, select, mval, rad);
			break;
		default:
			return;
	}
}

static bool object_circle_select(ViewContext *vc, const bool select, const int mval[2], float rad)
{
	Scene *scene = vc->scene;
	const float radius_squared = rad * rad;
	const float mval_fl[2] = {mval[0], mval[1]};
	bool changed = false;
	const int select_flag = select ? SELECT : 0;


	Base *base;
	for (base = FIRSTBASE; base; base = base->next) {
		if (BASE_SELECTABLE(vc->v3d, base) && ((base->flag & SELECT) != select_flag)) {
			float screen_co[2];
			if (ED_view3d_project_float_global(vc->ar, base->object->obmat[3], screen_co,
			                                   V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_WIN | V3D_PROJ_TEST_CLIP_NEAR) == V3D_PROJ_RET_OK)
			{
				if (len_squared_v2v2(mval_fl, screen_co) <= radius_squared) {
					ED_base_object_select(base, select ? BA_SELECT : BA_DESELECT);
					changed = true;
				}
			}
		}
	}

	return changed;
}

/* not a real operator, only for circle test */
static int view3d_circle_select_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *obact = CTX_data_active_object(C);
	const int radius = RNA_int_get(op->ptr, "radius");
	const int gesture_mode = RNA_int_get(op->ptr, "gesture_mode");
	const bool select = (gesture_mode == GESTURE_MODAL_SELECT);
	const int mval[2] = {RNA_int_get(op->ptr, "x"),
	                     RNA_int_get(op->ptr, "y")};

	if (CTX_data_edit_object(C) || BKE_paint_select_elem_test(obact) ||
	    (obact && (obact->mode & (OB_MODE_PARTICLE_EDIT | OB_MODE_POSE))) )
	{
		ViewContext vc;
		
		view3d_operator_needs_opengl(C);
		
		view3d_set_viewcontext(C, &vc);

		if (CTX_data_edit_object(C)) {
			obedit_circle_select(&vc, select, mval, (float)radius);
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obact->data);
		}
		else if (BKE_paint_select_face_test(obact)) {
			paint_facesel_circle_select(&vc, select, mval, (float)radius);
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obact->data);
		}
		else if (BKE_paint_select_vert_test(obact)) {
			paint_vertsel_circle_select(&vc, select, mval, (float)radius);
			WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obact->data);
		}
		else if (obact->mode & OB_MODE_POSE)
			pose_circle_select(&vc, select, mval, (float)radius);
		else
			return PE_circle_select(C, select, mval, (float)radius);
	}
	else if (obact && obact->mode & OB_MODE_SCULPT) {
		return OPERATOR_CANCELLED;
	}
	else {
		ViewContext vc;
		view3d_set_viewcontext(C, &vc);

		if (object_circle_select(&vc, select, mval, (float)radius)) {
			WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		}
	}
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_select_circle(wmOperatorType *ot)
{
	ot->name = "Circle Select";
	ot->description = "Select items using circle selection";
	ot->idname = "VIEW3D_OT_select_circle";
	
	ot->invoke = WM_gesture_circle_invoke;
	ot->modal = WM_gesture_circle_modal;
	ot->exec = view3d_circle_select_exec;
	ot->poll = view3d_selectable_data;
	ot->cancel = WM_gesture_circle_cancel;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "radius", 1, 1, INT_MAX, "Radius", "", 1, INT_MAX);
	RNA_def_int(ot->srna, "gesture_mode", 0, INT_MIN, INT_MAX, "Event Type", "", INT_MIN, INT_MAX);
}
