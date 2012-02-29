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
#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_linklist.h"
#include "BLI_utildefines.h"

/* vertex box select */
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "BKE_global.h"

#include "BKE_context.h"
#include "BKE_paint.h"
#include "BKE_armature.h"
#include "BKE_tessmesh.h"
#include "BKE_movieclip.h"
#include "BKE_object.h"
#include "BKE_tracking.h"


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
#include "ED_mball.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "view3d_intern.h"	// own include

// TODO: should return whether there is valid context to continue
void view3d_set_viewcontext(bContext *C, ViewContext *vc)
{
	memset(vc, 0, sizeof(ViewContext));
	vc->ar= CTX_wm_region(C);
	vc->scene= CTX_data_scene(C);
	vc->v3d= CTX_wm_view3d(C);
	vc->rv3d= CTX_wm_region_view3d(C);
	vc->obact= CTX_data_active_object(C);
	vc->obedit= CTX_data_edit_object(C); 
}

int view3d_get_view_aligned_coordinate(ViewContext *vc, float fp[3], const int mval[2], const short do_fallback)
{
	float dvec[3];
	int mval_cpy[2];

	mval_cpy[0]= mval[0];
	mval_cpy[1]= mval[1];

	project_int_noclip(vc->ar, fp, mval_cpy);

	initgrabz(vc->rv3d, fp[0], fp[1], fp[2]);

	if (mval_cpy[0]!=IS_CLIPPED) {
		float mval_f[2];
		VECSUB2D(mval_f, mval_cpy, mval);
		ED_view3d_win_to_delta(vc->ar, mval_f, dvec);
		sub_v3_v3(fp, dvec);

		return TRUE;
	}
	else {
		/* fallback to the view center */
		if (do_fallback) {
			negate_v3_v3(fp, vc->rv3d->ofs);
			return view3d_get_view_aligned_coordinate(vc, fp, mval, FALSE);
		}
		else {
			return FALSE;
		}
	}
}

/*
 * ob == NULL if you want global matrices
 * */
void view3d_get_transformation(const ARegion *ar, RegionView3D *rv3d, Object *ob, bglMats *mats)
{
	float cpy[4][4];
	int i, j;

	if (ob) {
		mult_m4_m4m4(cpy, rv3d->viewmat, ob->obmat);
	}
	else {
		copy_m4_m4(cpy, rv3d->viewmat);
	}

	for (i = 0; i < 4; ++i) {
		for (j = 0; j < 4; ++j) {
			mats->projection[i*4+j] = rv3d->winmat[i][j];
			mats->modelview[i*4+j] = cpy[i][j];
		}
	}

	mats->viewport[0] = ar->winrct.xmin;
	mats->viewport[1] = ar->winrct.ymin;
	mats->viewport[2] = ar->winx;
	mats->viewport[3] = ar->winy;	
}

/* ********************** view3d_select: selection manipulations ********************* */

/* local prototypes */

static void EDBM_backbuf_checkAndSelectVerts(BMEditMesh *em, int select)
{
	BMVert *eve;
	BMIter iter;
	int index= bm_wireoffs;

	eve = BM_iter_new(&iter, em->bm, BM_VERTS_OF_MESH, NULL);
	for ( ; eve; eve=BM_iter_step(&iter), index++) {
		if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
			if (EDBM_check_backbuf(index)) {
				BM_vert_select_set(em->bm, eve, select);
			}
		}
	}
}

static void EDBM_backbuf_checkAndSelectEdges(BMEditMesh *em, int select)
{
	BMEdge *eed;
	BMIter iter;
	int index= bm_solidoffs;

	eed = BM_iter_new(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
	for ( ; eed; eed=BM_iter_step(&iter), index++) {
		if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
			if (EDBM_check_backbuf(index)) {
				BM_edge_select_set(em->bm, eed, select);
			}
		}
	}
}

static void EDBM_backbuf_checkAndSelectFaces(BMEditMesh *em, int select)
{
	BMFace *efa;
	BMIter iter;
	int index= 1;

	efa = BM_iter_new(&iter, em->bm, BM_FACES_OF_MESH, NULL);
	for ( ; efa; efa=BM_iter_step(&iter), index++) {
		if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
			if (EDBM_check_backbuf(index)) {
				BM_face_select_set(em->bm, efa, select);
			}
		}
	}
}


/* object mode, EM_ prefix is confusing here, rename? */
static void EDBM_backbuf_checkAndSelectVerts_obmode(Mesh *me, int select)
{
	MVert *mv = me->mvert;
	int a;

	if (mv) {
		for (a=1; a<=me->totvert; a++, mv++) {
			if (EDBM_check_backbuf(a)) {
				if (!(mv->flag & ME_HIDE)) {
					mv->flag = select?(mv->flag|SELECT):(mv->flag&~SELECT);
				}
			}
		}
	}
}
/* object mode, EM_ prefix is confusing here, rename? */

static void EDBM_backbuf_checkAndSelectTFaces(Mesh *me, int select)
{
	MPoly *mpoly = me->mpoly;
	int a;

	if (mpoly) {
		for (a=1; a<=me->totpoly; a++, mpoly++) {
			if (EDBM_check_backbuf(a)) {
				mpoly->flag = select?(mpoly->flag|ME_FACE_SEL):(mpoly->flag&~ME_FACE_SEL);
			}
		}
	}
}

/* *********************** GESTURE AND LASSO ******************* */

typedef struct LassoSelectUserData {
	ViewContext *vc;
	rcti *rect;
	int (*mcords)[2], moves, select, pass, done;
} LassoSelectUserData;

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
			if (ob->mode & OB_MODE_SCULPT) {
				return 0;
			}
			if ((ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT)) &&
			    !paint_facesel_test(ob) && !paint_vertsel_test(ob))
			{
				return 0;
			}
		}
	}

	return 1;
}


/* helper also for borderselect */
static int edge_fully_inside_rect(rcti *rect, short x1, short y1, short x2, short y2)
{
	return BLI_in_rcti(rect, x1, y1) && BLI_in_rcti(rect, x2, y2);
}

static int edge_inside_rect(rcti *rect, short x1, short y1, short x2, short y2)
{
	int d1, d2, d3, d4;
	
	/* check points in rect */
	if (edge_fully_inside_rect(rect, x1, y1, x2, y2)) return 1;
	
	/* check points completely out rect */
	if (x1<rect->xmin && x2<rect->xmin) return 0;
	if (x1>rect->xmax && x2>rect->xmax) return 0;
	if (y1<rect->ymin && y2<rect->ymin) return 0;
	if (y1>rect->ymax && y2>rect->ymax) return 0;
	
	/* simple check lines intersecting. */
	d1= (y1-y2)*(x1- rect->xmin ) + (x2-x1)*(y1- rect->ymin );
	d2= (y1-y2)*(x1- rect->xmin ) + (x2-x1)*(y1- rect->ymax );
	d3= (y1-y2)*(x1- rect->xmax ) + (x2-x1)*(y1- rect->ymax );
	d4= (y1-y2)*(x1- rect->xmax ) + (x2-x1)*(y1- rect->ymin );
	
	if (d1<0 && d2<0 && d3<0 && d4<0) return 0;
	if (d1>0 && d2>0 && d3>0 && d4>0) return 0;
	
	return 1;
}


#define MOVES_GESTURE 50
#define MOVES_LASSO 500

int lasso_inside(int mcords[][2], short moves, int sx, int sy)
{
	/* we do the angle rule, define that all added angles should be about zero or 2*PI */
	float angletot=0.0, len, dot, ang, cross, fp1[2], fp2[2];
	int a;
	int *p1, *p2;
	
	if (sx==IS_CLIPPED)
		return 0;
	
	p1= mcords[moves-1];
	p2= mcords[0];
	
	/* first vector */
	fp1[0]= (float)(p1[0]-sx);
	fp1[1]= (float)(p1[1]-sy);
	len= sqrt(fp1[0]*fp1[0] + fp1[1]*fp1[1]);
	fp1[0]/= len;
	fp1[1]/= len;
	
	for (a=0; a<moves; a++) {
		/* second vector */
		fp2[0]= (float)(p2[0]-sx);
		fp2[1]= (float)(p2[1]-sy);
		len= sqrt(fp2[0]*fp2[0] + fp2[1]*fp2[1]);
		fp2[0]/= len;
		fp2[1]/= len;
		
		/* dot and angle and cross */
		dot= fp1[0]*fp2[0] + fp1[1]*fp2[1];
		ang= fabs(saacos(dot));

		cross= (float)((p1[1]-p2[1])*(p1[0]-sx) + (p2[0]-p1[0])*(p1[1]-sy));
		
		if (cross<0.0f) angletot-= ang;
		else angletot+= ang;
		
		/* circulate */
		fp1[0]= fp2[0]; fp1[1]= fp2[1];
		p1= p2;
		p2= mcords[a+1];
	}
	
	if ( fabs(angletot) > 4.0 ) return 1;
	return 0;
}

/* edge version for lasso select. we assume boundbox check was done */
int lasso_inside_edge(int mcords[][2], short moves, int x0, int y0, int x1, int y1)
{
	int v1[2], v2[2];
	int a;

	if (x0==IS_CLIPPED || x1==IS_CLIPPED)
		return 0;
	
	v1[0] = x0, v1[1] = y0;
	v2[0] = x1, v2[1] = y1;

	/* check points in lasso */
	if (lasso_inside(mcords, moves, v1[0], v1[1])) return 1;
	if (lasso_inside(mcords, moves, v2[0], v2[1])) return 1;
	
	/* no points in lasso, so we have to intersect with lasso edge */
	
	if ( isect_line_line_v2_int(mcords[0], mcords[moves-1], v1, v2) > 0) return 1;
	for (a=0; a<moves-1; a++) {
		if ( isect_line_line_v2_int(mcords[a], mcords[a+1], v1, v2) > 0) return 1;
	}
	
	return 0;
}


/* warning; lasso select with backbuffer-check draws in backbuf with persp(PERSP_WIN) 
   and returns with persp(PERSP_VIEW). After lasso select backbuf is not OK
*/
static void do_lasso_select_pose(ViewContext *vc, Object *ob, int mcords[][2], short moves, short select)
{
	bPoseChannel *pchan;
	float vec[3];
	int sco1[2], sco2[2];
	bArmature *arm= ob->data;
	
	if (ob->type!=OB_ARMATURE || ob->pose==NULL) return;

	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if (PBONE_VISIBLE(arm, pchan->bone) && (pchan->bone->flag & BONE_UNSELECTABLE)==0) {
			mul_v3_m4v3(vec, ob->obmat, pchan->pose_head);
			project_int(vc->ar, vec, sco1);
			mul_v3_m4v3(vec, ob->obmat, pchan->pose_tail);
			project_int(vc->ar, vec, sco2);
			
			if (lasso_inside_edge(mcords, moves, sco1[0], sco1[1], sco2[0], sco2[1])) {
				if (select) pchan->bone->flag |= BONE_SELECTED;
				else pchan->bone->flag &= ~BONE_SELECTED;
			}
		}
	}
}

static void object_deselect_all_visible(Scene *scene, View3D *v3d)
{
	Base *base;

	for (base= scene->base.first; base; base= base->next) {
		if (BASE_SELECTABLE(v3d, base)) {
			ED_base_object_select(base, BA_DESELECT);
		}
	}
}

static void do_lasso_select_objects(ViewContext *vc, int mcords[][2], short moves, short extend, short select)
{
	Base *base;
	
	if (extend == 0 && select)
		object_deselect_all_visible(vc->scene, vc->v3d);

	for (base= vc->scene->base.first; base; base= base->next) {
		if (BASE_SELECTABLE(vc->v3d, base)) { /* use this to avoid un-needed lasso lookups */
			project_short(vc->ar, base->object->obmat[3], &base->sx);
			if (lasso_inside(mcords, moves, base->sx, base->sy)) {
				
				if (select) ED_base_object_select(base, BA_SELECT);
				else ED_base_object_select(base, BA_DESELECT);
				base->object->flag= base->flag;
			}
			if (base->object->mode & OB_MODE_POSE) {
				do_lasso_select_pose(vc, base->object, mcords, moves, select);
			}
		}
	}
}

static void lasso_select_boundbox(rcti *rect, int mcords[][2], short moves)
{
	short a;
	
	rect->xmin= rect->xmax= mcords[0][0];
	rect->ymin= rect->ymax= mcords[0][1];
	
	for (a=1; a<moves; a++) {
		if (mcords[a][0]<rect->xmin) rect->xmin= mcords[a][0];
		else if (mcords[a][0]>rect->xmax) rect->xmax= mcords[a][0];
		if (mcords[a][1]<rect->ymin) rect->ymin= mcords[a][1];
		else if (mcords[a][1]>rect->ymax) rect->ymax= mcords[a][1];
	}
}

static void do_lasso_select_mesh__doSelectVert(void *userData, BMVert *eve, int x, int y, int UNUSED(index))
{
	LassoSelectUserData *data = userData;

	if (BLI_in_rcti(data->rect, x, y) && lasso_inside(data->mcords, data->moves, x, y)) {
		BM_elem_select_set(data->vc->em->bm, eve, data->select);
	}
}
static void do_lasso_select_mesh__doSelectEdge(void *userData, BMEdge *eed, int x0, int y0, int x1, int y1, int index)
{
	LassoSelectUserData *data = userData;

	if (EDBM_check_backbuf(bm_solidoffs+index)) {
		if (data->pass==0) {
			if (	edge_fully_inside_rect(data->rect, x0, y0, x1, y1)  &&
					lasso_inside(data->mcords, data->moves, x0, y0) &&
					lasso_inside(data->mcords, data->moves, x1, y1)) {
				BM_elem_select_set(data->vc->em->bm, eed, data->select);
				data->done = 1;
			}
		}
		else {
			if (lasso_inside_edge(data->mcords, data->moves, x0, y0, x1, y1)) {
				BM_elem_select_set(data->vc->em->bm, eed, data->select);
			}
		}
	}
}
static void do_lasso_select_mesh__doSelectFace(void *userData, BMFace *efa, int x, int y, int UNUSED(index))
{
	LassoSelectUserData *data = userData;

	if (BLI_in_rcti(data->rect, x, y) && lasso_inside(data->mcords, data->moves, x, y)) {
		BM_elem_select_set(data->vc->em->bm, efa, data->select);
	}
}

static void do_lasso_select_mesh(ViewContext *vc, int mcords[][2], short moves, short extend, short select)
{
	LassoSelectUserData data;
	ToolSettings *ts= vc->scene->toolsettings;
	rcti rect;
	int bbsel;
	
	lasso_select_boundbox(&rect, mcords, moves);
	
	/* set editmesh */
	vc->em= ((Mesh *)vc->obedit->data)->edit_btmesh;

	data.vc= vc;
	data.rect = &rect;
	data.mcords = mcords;
	data.moves = moves;
	data.select = select;
	data.done = 0;
	data.pass = 0;

	if (extend == 0 && select)
		EDBM_flag_disable_all(vc->em, BM_ELEM_SELECT);

	 /* for non zbuf projections, dont change the GL state */
	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

	glLoadMatrixf(vc->rv3d->viewmat);
	bbsel= EDBM_mask_init_backbuf_border(vc, mcords, moves, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
	
	if (ts->selectmode & SCE_SELECT_VERTEX) {
		if (bbsel) {
			EDBM_backbuf_checkAndSelectVerts(vc->em, select);
		}
		else {
			mesh_foreachScreenVert(vc, do_lasso_select_mesh__doSelectVert, &data, V3D_CLIP_TEST_RV3D_CLIPPING);
		}
	}
	if (ts->selectmode & SCE_SELECT_EDGE) {
		/* Does both bbsel and non-bbsel versions (need screen cos for both) */
		data.pass = 0;
		mesh_foreachScreenEdge(vc, do_lasso_select_mesh__doSelectEdge, &data, V3D_CLIP_TEST_OFF);

		if (data.done==0) {
			data.pass = 1;
			mesh_foreachScreenEdge(vc, do_lasso_select_mesh__doSelectEdge, &data, V3D_CLIP_TEST_OFF);
		}
	}
	
	if (ts->selectmode & SCE_SELECT_FACE) {
		if (bbsel) {
			EDBM_backbuf_checkAndSelectFaces(vc->em, select);
		}
		else {
			mesh_foreachScreenFace(vc, do_lasso_select_mesh__doSelectFace, &data);
		}
	}
	
	EDBM_free_backbuf();
	EDBM_selectmode_flush(vc->em);
}

/* BMESH_TODO */
#if 0
/* this is an exception in that its the only lasso that dosnt use the 3d view (uses space image view) */
static void do_lasso_select_mesh_uv(int mcords[][2], short moves, short select)
{
	EditFace *efa;
	MTFace *tf;
	int screenUV[2], nverts, i, ok = 1;
	rcti rect;
	
	lasso_select_boundbox(&rect, mcords, moves);
	
	if (draw_uvs_face_check()) { /* Face Center Sel */
		float cent[2];
		ok = 0;
		for (efa= em->faces.first; efa; efa= efa->next) {
			/* assume not touched */
			efa->tmp.l = 0;
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if ((select) != (simaFaceSel_Check(efa, tf))) {
				uv_center(tf->uv, cent, (void *)efa->v4);
				uvco_to_areaco_noclip(cent, screenUV);
				if (BLI_in_rcti(&rect, screenUV[0], screenUV[1]) && lasso_inside(mcords, moves, screenUV[0], screenUV[1])) {
					efa->tmp.l = ok = 1;
				}
			}
		}
		/* (de)selects all tagged faces and deals with sticky modes */
		if (ok)
			uvface_setsel__internal(select);
		
	}
	else { /* Vert Sel*/
		for (efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if (uvedit_face_visible(scene, ima, efa, tf)) {		
				nverts= efa->v4? 4: 3;
				for (i=0; i<nverts; i++) {
					if ((select) != (simaUVSel_Check(efa, tf, i))) {
						uvco_to_areaco_noclip(tf->uv[i], screenUV);
						if (BLI_in_rcti(&rect, screenUV[0], screenUV[1]) && lasso_inside(mcords, moves, screenUV[0], screenUV[1])) {
							if (select) {
								simaUVSel_Set(efa, tf, i);
							}
							else {
								simaUVSel_UnSet(efa, tf, i);
							}
						}
					}
				}
			}
		}
	}
	if (ok && G.sima->flag & SI_SYNC_UVSEL) {
		if (select) EM_select_flush(vc->em);
		else		EM_deselect_flush(vc->em);
	}
}
#endif

static void do_lasso_select_curve__doSelect(void *userData, Nurb *UNUSED(nu), BPoint *bp, BezTriple *bezt, int beztindex, int x, int y)
{
	LassoSelectUserData *data = userData;
	Object *obedit= data->vc->obedit;
	Curve *cu= (Curve*)obedit->data;

	if (lasso_inside(data->mcords, data->moves, x, y)) {
		if (bp) {
			bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
			if (bp == cu->lastsel && !(bp->f1 & 1)) cu->lastsel = NULL;
		}
		else {
			if (cu->drawflag & CU_HIDE_HANDLES) {
				/* can only be beztindex==0 here since handles are hidden */
				bezt->f1 = bezt->f2 = bezt->f3 = data->select?(bezt->f2|SELECT):(bezt->f2&~SELECT);
			}
			else {
				if (beztindex==0) {
					bezt->f1 = data->select?(bezt->f1|SELECT):(bezt->f1&~SELECT);
				}
				else if (beztindex==1) {
					bezt->f2 = data->select?(bezt->f2|SELECT):(bezt->f2&~SELECT);
				}
				else {
					bezt->f3 = data->select?(bezt->f3|SELECT):(bezt->f3&~SELECT);
				}
			}

			if (bezt == cu->lastsel && !(bezt->f2 & 1)) cu->lastsel = NULL;
		}
	}
}

static void do_lasso_select_curve(ViewContext *vc, int mcords[][2], short moves, short extend, short select)
{
	LassoSelectUserData data;

	/* set vc->editnurb */
	data.vc = vc;
	data.mcords = mcords;
	data.moves = moves;
	data.select = select;

	if (extend == 0 && select)
		CU_deselect_all(vc->obedit);

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
	nurbs_foreachScreenVert(vc, do_lasso_select_curve__doSelect, &data);
}

static void do_lasso_select_lattice__doSelect(void *userData, BPoint *bp, int x, int y)
{
	LassoSelectUserData *data = userData;

	if (lasso_inside(data->mcords, data->moves, x, y)) {
		bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
	}
}
static void do_lasso_select_lattice(ViewContext *vc, int mcords[][2], short moves, short extend, short select)
{
	LassoSelectUserData data;

	/* set editdata in vc */
	data.mcords = mcords;
	data.moves = moves;
	data.select = select;

	if (extend == 0 && select)
		ED_setflagsLatt(vc->obedit, 0);

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
	lattice_foreachScreenVert(vc, do_lasso_select_lattice__doSelect, &data);
}

static void do_lasso_select_armature(ViewContext *vc, int mcords[][2], short moves, short extend, short select)
{
	bArmature *arm= vc->obedit->data;
	EditBone *ebone;
	float vec[3];
	short sco1[2], sco2[2], didpoint;
	int change= FALSE;

	if (extend==0 && select)
		ED_armature_deselect_all_visible(vc->obedit);

	/* set editdata in vc */
	
	for (ebone= arm->edbo->first; ebone; ebone=ebone->next) {
		if (EBONE_VISIBLE(arm, ebone) && (ebone->flag & BONE_UNSELECTABLE)==0) {
			mul_v3_m4v3(vec, vc->obedit->obmat, ebone->head);
			project_short(vc->ar, vec, sco1);
			mul_v3_m4v3(vec, vc->obedit->obmat, ebone->tail);
			project_short(vc->ar, vec, sco2);
			
			didpoint= 0;
			if (lasso_inside(mcords, moves, sco1[0], sco1[1])) {
				if (select) ebone->flag |= BONE_ROOTSEL;
				else ebone->flag &= ~BONE_ROOTSEL;
				didpoint= 1;
				change= TRUE;
			}
			if (lasso_inside(mcords, moves, sco2[0], sco2[1])) {
				if (select) ebone->flag |= BONE_TIPSEL;
				else ebone->flag &= ~BONE_TIPSEL;
				didpoint= 1;
				change= TRUE;
			}
			/* if one of points selected, we skip the bone itself */
			if (didpoint==0 && lasso_inside_edge(mcords, moves, sco1[0], sco1[1], sco2[0], sco2[1])) {
				if (select) ebone->flag |= BONE_TIPSEL|BONE_ROOTSEL|BONE_SELECTED;
				else ebone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
				change= TRUE;
			}
		}
	}
	
	if (change) {
		ED_armature_sync_selection(arm->edbo);
		ED_armature_validate_active(arm);
		WM_main_add_notifier(NC_OBJECT|ND_BONE_SELECT, vc->obedit);
	}
}




static void do_lasso_select_meta(ViewContext *vc, int mcords[][2], short moves, short extend, short select)
{
	MetaBall *mb = (MetaBall*)vc->obedit->data;
	MetaElem *ml;
	float vec[3];
	short sco[2];

	if (extend == 0 && select) {
		for (ml= mb->editelems->first; ml; ml= ml->next) {
			ml->flag &= ~SELECT;
		}
	}

	for (ml= mb->editelems->first; ml; ml= ml->next) {
		
		mul_v3_m4v3(vec, vc->obedit->obmat, &ml->x);
		project_short(vc->ar, vec, sco);

		if (lasso_inside(mcords, moves, sco[0], sco[1])) {
			if (select)	ml->flag |= SELECT;
			else		ml->flag &= ~SELECT;
		}
	}
}

int do_paintvert_box_select(ViewContext *vc, rcti *rect, int select, int extend)
{
	Mesh *me;
	MVert *mvert;
	struct ImBuf *ibuf;
	unsigned int *rt;
	int a, index;
	char *selar;
	int sx= rect->xmax-rect->xmin+1;
	int sy= rect->ymax-rect->ymin+1;

	me= vc->obact->data;

	if (me==NULL || me->totvert==0 || sx*sy <= 0)
		return OPERATOR_CANCELLED;

	selar= MEM_callocN(me->totvert+1, "selar");

	if (extend == 0 && select)
		paintvert_deselect_all_visible(vc->obact, SEL_DESELECT, FALSE);

	view3d_validate_backbuf(vc);

	ibuf = IMB_allocImBuf(sx,sy,32,IB_rect);
	rt = ibuf->rect;
	glReadPixels(rect->xmin+vc->ar->winrct.xmin,  rect->ymin+vc->ar->winrct.ymin, sx, sy, GL_RGBA, GL_UNSIGNED_BYTE,  ibuf->rect);
	if (ENDIAN_ORDER==B_ENDIAN) IMB_convert_rgba_to_abgr(ibuf);

	a= sx*sy;
	while (a--) {
		if (*rt) {
			index= WM_framebuffer_to_index(*rt);
			if (index<=me->totvert) selar[index]= 1;
		}
		rt++;
	}

	mvert= me->mvert;
	for (a=1; a<=me->totvert; a++, mvert++) {
		if (selar[a]) {
			if (mvert->flag & ME_HIDE);
			else {
				if (select) mvert->flag |= SELECT;
				else mvert->flag &= ~SELECT;
			}
		}
	}

	IMB_freeImBuf(ibuf);
	MEM_freeN(selar);

#ifdef __APPLE__	
	glReadBuffer(GL_BACK);
#endif

	paintvert_flush_flags(vc->obact);

	return OPERATOR_FINISHED;
}

static void do_lasso_select_paintvert(ViewContext *vc, int mcords[][2], short moves, short extend, short select)
{
	Object *ob= vc->obact;
	Mesh *me= ob?ob->data:NULL;
	rcti rect;

	if (me==NULL || me->totvert==0)
		return;

	if (extend==0 && select)
		paintvert_deselect_all_visible(ob, SEL_DESELECT, FALSE); /* flush selection at the end */
	bm_vertoffs= me->totvert+1;	/* max index array */

	lasso_select_boundbox(&rect, mcords, moves);
	EDBM_mask_init_backbuf_border(vc, mcords, moves, rect.xmin, rect.ymin, rect.xmax, rect.ymax);

	EDBM_backbuf_checkAndSelectVerts_obmode(me, select);

	EDBM_free_backbuf();

	paintvert_flush_flags(ob);
}
static void do_lasso_select_paintface(ViewContext *vc, int mcords[][2], short moves, short extend, short select)
{
	Object *ob= vc->obact;
	Mesh *me= ob?ob->data:NULL;
	rcti rect;

	if (me==NULL || me->totpoly==0)
		return;

	if (extend==0 && select)
		paintface_deselect_all_visible(ob, SEL_DESELECT, FALSE); /* flush selection at the end */

	bm_vertoffs= me->totpoly+1;	/* max index array */

	lasso_select_boundbox(&rect, mcords, moves);
	EDBM_mask_init_backbuf_border(vc, mcords, moves, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
	
	EDBM_backbuf_checkAndSelectTFaces(me, select);

	EDBM_free_backbuf();

	paintface_flush_flags(ob);
}

#if 0
static void do_lasso_select_node(int mcords[][2], short moves, short select)
{
	SpaceNode *snode = sa->spacedata.first;
	
	bNode *node;
	rcti rect;
	short node_cent[2];
	float node_centf[2];
	
	lasso_select_boundbox(&rect, mcords, moves);
	
	/* store selection in temp test flag */
	for (node= snode->edittree->nodes.first; node; node= node->next) {
		
		node_centf[0] = (node->totr.xmin+node->totr.xmax)/2;
		node_centf[1] = (node->totr.ymin+node->totr.ymax)/2;
		
		ipoco_to_areaco_noclip(G.v2d, node_centf, node_cent);
		if (BLI_in_rcti(&rect, node_cent[0], node_cent[1]) && lasso_inside(mcords, moves, node_cent[0], node_cent[1])) {
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

static void view3d_lasso_select(bContext *C, ViewContext *vc, int mcords[][2], short moves, short extend, short select)
{
	Object *ob = CTX_data_active_object(C);

	if (vc->obedit==NULL) { /* Object Mode */
		if (paint_facesel_test(ob))
			do_lasso_select_paintface(vc, mcords, moves, extend, select);
		else if (paint_vertsel_test(ob))
			do_lasso_select_paintvert(vc, mcords, moves, extend, select);
		else if (ob && ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT))
			;
		else if (ob && ob->mode & OB_MODE_PARTICLE_EDIT)
			PE_lasso_select(C, mcords, moves, extend, select);
		else {
			do_lasso_select_objects(vc, mcords, moves, extend, select);
			WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, vc->scene);
		}
	}
	else { /* Edit Mode */
		switch(vc->obedit->type) {
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
		}

		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, vc->obedit->data);
	}
}


/* lasso operator gives properties, but since old code works
   with short array we convert */
static int view3d_lasso_select_exec(bContext *C, wmOperator *op)
{
	ViewContext vc;
	int i= 0;
	int mcords[1024][2];

	RNA_BEGIN(op->ptr, itemptr, "path") {
		float loc[2];
		
		RNA_float_get_array(&itemptr, "loc", loc);
		mcords[i][0]= (int)loc[0];
		mcords[i][1]= (int)loc[1];
		i++;
		if (i>=1024) break;
	}
	RNA_END;
	
	if (i>1) {
		short extend, select;
		view3d_operator_needs_opengl(C);
		
		/* setup view context for argument to callbacks */
		view3d_set_viewcontext(C, &vc);
		
		extend= RNA_boolean_get(op->ptr, "extend");
		select= !RNA_boolean_get(op->ptr, "deselect");
		view3d_lasso_select(C, &vc, mcords, i, extend, select);
		
		return OPERATOR_FINISHED;
	}
	return OPERATOR_PASS_THROUGH;
}

void VIEW3D_OT_select_lasso(wmOperatorType *ot)
{
	ot->name= "Lasso Select";
	ot->description= "Select items using lasso selection";
	ot->idname= "VIEW3D_OT_select_lasso";
	
	ot->invoke= WM_gesture_lasso_invoke;
	ot->modal= WM_gesture_lasso_modal;
	ot->exec= view3d_lasso_select_exec;
	ot->poll= view3d_selectable_data;
	ot->cancel= WM_gesture_lasso_cancel;
	
	/* flags */
	ot->flag= OPTYPE_UNDO;
	
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
	unsigned int *bufmin,*bufmax;
	int a,b,rc,tel,len,dirvec[4][2],maxob;
	unsigned int retval=0;
	
	base= LASTBASE;
	if (base==0) return 0;
	maxob= base->selcol;

	len= (size-1)/2;
	rc= 0;

	dirvec[0][0]= 1;
	dirvec[0][1]= 0;
	dirvec[1][0]= 0;
	dirvec[1][1]= -size;
	dirvec[2][0]= -1;
	dirvec[2][1]= 0;
	dirvec[3][0]= 0;
	dirvec[3][1]= size;

	bufmin= buf;
	bufmax= buf+ size*size;
	buf+= len*size+ len;

	for (tel=1;tel<=size;tel++) {

		for (a=0;a<2;a++) {
			for (b=0;b<tel;b++) {

				if (*buf && *buf<=maxob && *buf!=dontdo) return *buf;
				if ( *buf==dontdo ) retval= dontdo;	/* if only color dontdo is available, still return dontdo */
				
				buf+= (dirvec[rc][0]+dirvec[rc][1]);

				if (buf<bufmin || buf>=bufmax) return retval;
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
	char idname[MAX_ID_NAME-2];
	int icon;
} SelMenuItemF;

#define SEL_MENU_SIZE	22
static SelMenuItemF object_mouse_select_menu_data[SEL_MENU_SIZE];

/* special (crappy) operator only for menu select */
static EnumPropertyItem *object_select_menu_enum_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), int *free)
{
	EnumPropertyItem *item= NULL, item_tmp= {0};
	int totitem= 0;
	int i= 0;

	/* dont need context but avoid docgen using this */
	if (C == NULL || object_mouse_select_menu_data[i].idname[0] == '\0') {
		return DummyRNA_NULL_items;
	}

	for (; i < SEL_MENU_SIZE && object_mouse_select_menu_data[i].idname[0] != '\0'; i++) {
		item_tmp.name= object_mouse_select_menu_data[i].idname;
		item_tmp.identifier= object_mouse_select_menu_data[i].idname;
		item_tmp.value= i;
		item_tmp.icon= object_mouse_select_menu_data[i].icon;
		RNA_enum_item_add(&item, &totitem, &item_tmp);
	}

	RNA_enum_item_end(&item, &totitem);
	*free= 1;

	return item;
}

static int object_select_menu_exec(bContext *C, wmOperator *op)
{
	int name_index= RNA_enum_get(op->ptr, "name");
	short extend= RNA_boolean_get(op->ptr, "extend");
	short changed = 0;
	const char *name= object_mouse_select_menu_data[name_index].idname;

	if (!extend) {
		CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
			if (base->flag & SELECT) {
				ED_base_object_select(base, BA_DESELECT);
				changed= 1;
			}
		}
		CTX_DATA_END;
	}

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		/* this is a bit dodjy, there should only be ONE object with this name, but library objects can mess this up */
		if (strcmp(name, base->object->id.name+2)==0) {
			ED_base_object_activate(C, base);
			ED_base_object_select(base, BA_SELECT);
			changed= 1;
		}
	}
	CTX_DATA_END;

	/* weak but ensures we activate menu again before using the enum */
	memset(object_mouse_select_menu_data, 0, sizeof(object_mouse_select_menu_data));

	/* undo? */
	if (changed) {
		WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
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
	ot->name= "Select Menu";
	ot->description = "Menu object selection";
	ot->idname= "VIEW3D_OT_select_menu";

	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= object_select_menu_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* keyingset to use (dynamic enum) */
	prop= RNA_def_enum(ot->srna, "name", DummyRNA_NULL_items, 0, "Object Name", "");
	RNA_def_enum_funcs(prop, object_select_menu_enum_itemf);
	RNA_def_property_flag(prop, PROP_HIDDEN);
	ot->prop= prop;

	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend selection instead of deselecting everything first");
}

static void deselectall_except(Scene *scene, Base *b)   /* deselect all except b */
{
	Base *base;
	
	for (base= FIRSTBASE; base; base= base->next) {
		if (base->flag & SELECT) {
			if (b!=base) {
				ED_base_object_select(base, BA_DESELECT);
			}
		}
	}
}

static Base *object_mouse_select_menu(bContext *C, ViewContext *vc, unsigned int *buffer, int hits, const int mval[2], short extend)
{
	short baseCount = 0;
	short ok;
	LinkNode *linklist= NULL;
	
	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		ok= FALSE;

		/* two selection methods, the CTRL select uses max dist of 15 */
		if (buffer) {
			int a;
			for (a=0; a<hits; a++) {
				/* index was converted */
				if (base->selcol==buffer[ (4 * a) + 3 ])
					ok= TRUE;
			}
		}
		else {
			int temp, dist=15;

			project_short(vc->ar, base->object->obmat[3], &base->sx);
			
			temp= abs(base->sx -mval[0]) + abs(base->sy -mval[1]);
			if (temp < dist)
				ok= TRUE;
		}

		if (ok) {
			baseCount++;
			BLI_linklist_prepend(&linklist, base);

			if (baseCount==SEL_MENU_SIZE)
				break;
		}
	}
	CTX_DATA_END;

	if (baseCount==0) {
		return NULL;
	}
	if (baseCount == 1) {
		Base *base= (Base *)linklist->link;
		BLI_linklist_free(linklist, NULL);
		return base;
	}
	else {
		/* UI, full in static array values that we later use in an enum function */
		LinkNode *node;
		int i;

		memset(object_mouse_select_menu_data, 0, sizeof(object_mouse_select_menu_data));

		for (node = linklist, i = 0; node; node= node->next, i++) {
			Base *base=node->link;
			Object *ob= base->object;
			char *name= ob->id.name+2;

			BLI_strncpy(object_mouse_select_menu_data[i].idname, name, MAX_ID_NAME-2);
			object_mouse_select_menu_data[i].icon = uiIconFromID(&ob->id);
		}

		{
			PointerRNA ptr;

			WM_operator_properties_create(&ptr, "VIEW3D_OT_select_menu");
			RNA_boolean_set(&ptr, "extend", extend);
			WM_operator_name_call(C, "VIEW3D_OT_select_menu", WM_OP_INVOKE_DEFAULT, &ptr);
			WM_operator_properties_free(&ptr);
		}

		BLI_linklist_free(linklist, NULL);
		return NULL;
	}
}

/* we want a select buffer with bones, if there are... */
/* so check three selection levels and compare */
static short mixed_bones_object_selectbuffer(ViewContext *vc, unsigned int *buffer, const int mval[2])
{
	rcti rect;
	int offs;
	short a, hits15, hits9=0, hits5=0;
	short has_bones15=0, has_bones9=0, has_bones5=0;
	
	BLI_init_rcti(&rect, mval[0]-14, mval[0]+14, mval[1]-14, mval[1]+14);
	hits15= view3d_opengl_select(vc, buffer, MAXPICKBUF, &rect);
	if (hits15>0) {
		for (a=0; a<hits15; a++) if (buffer[4*a+3] & 0xFFFF0000) has_bones15= 1;
		
		offs= 4*hits15;
		BLI_init_rcti(&rect, mval[0]-9, mval[0]+9, mval[1]-9, mval[1]+9);
		hits9= view3d_opengl_select(vc, buffer+offs, MAXPICKBUF-offs, &rect);
		if (hits9>0) {
			for (a=0; a<hits9; a++) if (buffer[offs+4*a+3] & 0xFFFF0000) has_bones9= 1;
			
			offs+= 4*hits9;
			BLI_init_rcti(&rect, mval[0]-5, mval[0]+5, mval[1]-5, mval[1]+5);
			hits5= view3d_opengl_select(vc, buffer+offs, MAXPICKBUF-offs, &rect);
			if (hits5>0) {
				for (a=0; a<hits5; a++) if (buffer[offs+4*a+3] & 0xFFFF0000) has_bones5= 1;
			}
		}
		
		if (has_bones5) {
			offs= 4*hits15 + 4*hits9;
			memcpy(buffer, buffer+offs, 4*offs);
			return hits5;
		}
		if (has_bones9) {
			offs= 4*hits15;
			memcpy(buffer, buffer+offs, 4*offs);
			return hits9;
		}
		if (has_bones15) {
			return hits15;
		}
		
		if (hits5>0) {
			offs= 4*hits15 + 4*hits9;
			memcpy(buffer, buffer+offs, 4*offs);
			return hits5;
		}
		if (hits9>0) {
			offs= 4*hits15;
			memcpy(buffer, buffer+offs, 4*offs);
			return hits9;
		}
		return hits15;
	}
	
	return 0;
}

/* returns basact */
static Base *mouse_select_eval_buffer(ViewContext *vc, unsigned int *buffer, int hits, const int mval[2], Base *startbase, int has_bones)
{
	Scene *scene= vc->scene;
	View3D *v3d= vc->v3d;
	Base *base, *basact= NULL;
	static int lastmval[2]={-100, -100};
	int a, donearest= 0;
	
	/* define if we use solid nearest select or not */
	if (v3d->drawtype>OB_WIRE) {
		donearest= 1;
		if ( ABS(mval[0]-lastmval[0])<3 && ABS(mval[1]-lastmval[1])<3) {
			if (!has_bones)	/* hrms, if theres bones we always do nearest */
				donearest= 0;
		}
	}
	lastmval[0]= mval[0]; lastmval[1]= mval[1];
	
	if (donearest) {
		unsigned int min= 0xFFFFFFFF;
		int selcol= 0, notcol=0;
		
		
		if (has_bones) {
			/* we skip non-bone hits */
			for (a=0; a<hits; a++) {
				if ( min > buffer[4*a+1] && (buffer[4*a+3] & 0xFFFF0000) ) {
					min= buffer[4*a+1];
					selcol= buffer[4*a+3] & 0xFFFF;
				}
			}
		}
		else {
			/* only exclude active object when it is selected... */
			if (BASACT && (BASACT->flag & SELECT) && hits>1) notcol= BASACT->selcol;	
			
			for (a=0; a<hits; a++) {
				if ( min > buffer[4*a+1] && notcol!=(buffer[4*a+3] & 0xFFFF)) {
					min= buffer[4*a+1];
					selcol= buffer[4*a+3] & 0xFFFF;
				}
			}
		}
		
		base= FIRSTBASE;
		while (base) {
			if (BASE_SELECTABLE(v3d, base)) {
				if (base->selcol==selcol) break;
			}
			base= base->next;
		}
		if (base) basact= base;
	}
	else {
		
		base= startbase;
		while (base) {
			/* skip objects with select restriction, to prevent prematurely ending this loop
			* with an un-selectable choice */
			if (base->object->restrictflag & OB_RESTRICT_SELECT) {
				base=base->next;
				if (base==NULL) base= FIRSTBASE;
				if (base==startbase) break;
			}
			
			if (BASE_SELECTABLE(v3d, base)) {
				for (a=0; a<hits; a++) {
					if (has_bones) {
						/* skip non-bone objects */
						if ((buffer[4*a+3] & 0xFFFF0000)) {
							if (base->selcol== (buffer[(4*a)+3] & 0xFFFF))
								basact= base;
						}
					}
					else {
						if (base->selcol== (buffer[(4*a)+3] & 0xFFFF))
							basact= base;
					}
				}
			}
			
			if (basact) break;
			
			base= base->next;
			if (base==NULL) base= FIRSTBASE;
			if (base==startbase) break;
		}
	}
	
	return basact;
}

/* mval comes from event->mval, only use within region handlers */
Base *ED_view3d_give_base_under_cursor(bContext *C, const int mval[2])
{
	ViewContext vc;
	Base *basact= NULL;
	unsigned int buffer[4*MAXPICKBUF];
	int hits;
	
	/* setup view context for argument to callbacks */
	view3d_operator_needs_opengl(C);
	view3d_set_viewcontext(C, &vc);
	
	hits= mixed_bones_object_selectbuffer(&vc, buffer, mval);
	
	if (hits>0) {
		int a, has_bones= 0;
		
		for (a=0; a<hits; a++) if (buffer[4*a+3] & 0xFFFF0000) has_bones= 1;
		
		basact= mouse_select_eval_buffer(&vc, buffer, hits, mval, vc.scene->base.first, has_bones);
	}
	
	return basact;
}

static void deselect_all_tracks(MovieTracking *tracking)
{
	MovieTrackingObject *object;

	object= tracking->objects.first;
	while (object) {
		ListBase *tracksbase= BKE_tracking_object_tracks(tracking, object);
		MovieTrackingTrack *track= tracksbase->first;

		while (track) {
			BKE_tracking_deselect_track(track, TRACK_AREA_ALL);

			track= track->next;
		}

		object= object->next;
	}
}

/* mval is region coords */
static int mouse_select(bContext *C, const int mval[2], short extend, short obcenter, short enumerate)
{
	ViewContext vc;
	ARegion *ar= CTX_wm_region(C);
	View3D *v3d= CTX_wm_view3d(C);
	Scene *scene= CTX_data_scene(C);
	Base *base, *startbase=NULL, *basact=NULL, *oldbasact=NULL;
	int temp, a, dist=100;
	int retval = 0;
	short hits;
	
	/* setup view context for argument to callbacks */
	view3d_set_viewcontext(C, &vc);
	
	/* always start list from basact in wire mode */
	startbase=  FIRSTBASE;
	if (BASACT && BASACT->next) startbase= BASACT->next;
	
	/* This block uses the control key to make the object selected by its center point rather than its contents */
	/* in editmode do not activate */
	if (obcenter) {
		
		/* note; shift+alt goes to group-flush-selecting */
		if (enumerate) {
			basact= object_mouse_select_menu(C, &vc, NULL, 0, mval, extend);
		}
		else {
			base= startbase;
			while (base) {
				if (BASE_SELECTABLE(v3d, base)) {
					project_short(ar, base->object->obmat[3], &base->sx);
					
					temp= abs(base->sx -mval[0]) + abs(base->sy -mval[1]);
					if (base==BASACT) temp+=10;
					if (temp<dist ) {
						
						dist= temp;
						basact= base;
					}
				}
				base= base->next;
				
				if (base==NULL) base= FIRSTBASE;
				if (base==startbase) break;
			}
		}
	}
	else {
		unsigned int buffer[4*MAXPICKBUF];

		/* if objects have posemode set, the bones are in the same selection buffer */
		
		hits= mixed_bones_object_selectbuffer(&vc, buffer, mval);
		
		if (hits>0) {
			int has_bones= 0;
			
			/* note: bundles are handling in the same way as bones */
			for (a=0; a<hits; a++) if (buffer[4*a+3] & 0xFFFF0000) has_bones= 1;

			/* note; shift+alt goes to group-flush-selecting */
			if (has_bones==0 && enumerate) {
				basact= object_mouse_select_menu(C, &vc, buffer, hits, mval, extend);
			}
			else {
				basact= mouse_select_eval_buffer(&vc, buffer, hits, mval, startbase, has_bones);
			}
			
			if (has_bones && basact) {
				if (basact->object->type==OB_CAMERA) {
					if (BASACT==basact) {
						int i, hitresult;
						int changed= 0;

						for (i=0; i< hits; i++) {
							hitresult= buffer[3+(i*4)];

							/* if there's bundles in buffer select bundles first,
							   so non-camera elements should be ignored in buffer */
							if (basact->selcol != (hitresult & 0xFFFF)) {
								continue;
							}

							/* index of bundle is 1<<16-based. if there's no "bone" index
							   in hight word, this buffer value belongs to camera,. not to bundle */
							if (buffer[4*i+3] & 0xFFFF0000) {
								MovieClip *clip= object_get_movieclip(scene, basact->object, 0);
								MovieTracking *tracking= &clip->tracking;
								ListBase *tracksbase;
								MovieTrackingTrack *track;

								track= BKE_tracking_indexed_track(&clip->tracking, hitresult >> 16, &tracksbase);

								if (TRACK_SELECTED(track) && extend) {
									changed= 0;
									BKE_tracking_deselect_track(track, TRACK_AREA_ALL);
								}
								else {
									int oldsel= TRACK_SELECTED(track) ? 1 : 0;
									if (!extend)
										deselect_all_tracks(tracking);

									BKE_tracking_select_track(tracksbase, track, TRACK_AREA_ALL, extend);

									if (oldsel!=(TRACK_SELECTED(track) ? 1 : 0))
										changed= 1;
								}

								basact->flag|= SELECT;
								basact->object->flag= basact->flag;

								retval= 1;

								WM_event_add_notifier(C, NC_MOVIECLIP|ND_SELECT, track);
								WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, scene);

								break;
							}
						}

						if (!changed) {
							/* fallback to regular object selection if no new bundles were selected,
							   allows to select object parented to reconstruction object */
							basact= mouse_select_eval_buffer(&vc, buffer, hits, mval, startbase, 0);
						}
					}
				}
				else if (ED_do_pose_selectbuffer(scene, basact, buffer, hits, extend) ) {	/* then bone is found */
				
					/* we make the armature selected: 
					   not-selected active object in posemode won't work well for tools */
					basact->flag|= SELECT;
					basact->object->flag= basact->flag;
					
					retval = 1;
					WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, basact->object);
					WM_event_add_notifier(C, NC_OBJECT|ND_BONE_ACTIVE, basact->object);
					
					/* in weightpaint, we use selected bone to select vertexgroup, so no switch to new active object */
					if (BASACT && BASACT->object->mode & OB_MODE_WEIGHT_PAINT) {
						/* prevent activating */
						basact= NULL;
					}

				}
				/* prevent bone selecting to pass on to object selecting */
				if (basact==BASACT)
					basact= NULL;
			}
		}
	}
	
	/* so, do we have something selected? */
	if (basact) {
		retval = 1;
		
		if (vc.obedit) {
			/* only do select */
			deselectall_except(scene, basact);
			ED_base_object_select(basact, BA_SELECT);
		}
		/* also prevent making it active on mouse selection */
		else if (BASE_SELECTABLE(v3d, basact)) {

			oldbasact= BASACT;
			
			if (!extend) {
				deselectall_except(scene, basact);
				ED_base_object_select(basact, BA_SELECT);
			}
			else if (0) {
				// XXX select_all_from_groups(basact);
			}
			else {
				if (basact->flag & SELECT) {
					if (basact==oldbasact)
						ED_base_object_select(basact, BA_DESELECT);
				}
				else ED_base_object_select(basact, BA_SELECT);
			}

			if (oldbasact != basact) {
				ED_base_object_activate(C, basact); /* adds notifier */
			}
		}

		WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, scene);
	}

	return retval;
}

/* ********************  border and circle ************************************** */

typedef struct BoxSelectUserData {
	ViewContext *vc;
	rcti *rect;
	int select, pass, done;
} BoxSelectUserData;

int edge_inside_circle(short centx, short centy, short rad, short x1, short y1, short x2, short y2)
{
	int radsq= rad*rad;
	float v1[2], v2[2], v3[2];
	
	/* check points in circle itself */
	if ( (x1-centx)*(x1-centx) + (y1-centy)*(y1-centy) <= radsq ) return 1;
	if ( (x2-centx)*(x2-centx) + (y2-centy)*(y2-centy) <= radsq ) return 1;
	
	/* pointdistline */
	v3[0]= centx;
	v3[1]= centy;
	v1[0]= x1;
	v1[1]= y1;
	v2[0]= x2;
	v2[1]= y2;
	
	if ( dist_to_line_segment_v2(v3, v1, v2) < (float)rad ) return 1;
	
	return 0;
}

static void do_nurbs_box_select__doSelect(void *userData, Nurb *UNUSED(nu), BPoint *bp, BezTriple *bezt, int beztindex, int x, int y)
{
	BoxSelectUserData *data = userData;
	Object *obedit= data->vc->obedit;
	Curve *cu= (Curve*)obedit->data;

	if (BLI_in_rcti(data->rect, x, y)) {
		if (bp) {
			bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
			if (bp == cu->lastsel && !(bp->f1 & 1)) cu->lastsel = NULL;
		}
		else {
			if (cu->drawflag & CU_HIDE_HANDLES) {
				/* can only be beztindex==0 here since handles are hidden */
				bezt->f1 = bezt->f2 = bezt->f3 = data->select?(bezt->f2|SELECT):(bezt->f2&~SELECT);
			}
			else {
				if (beztindex==0) {
					bezt->f1 = data->select?(bezt->f1|SELECT):(bezt->f1&~SELECT);
				}
				else if (beztindex==1) {
					bezt->f2 = data->select?(bezt->f2|SELECT):(bezt->f2&~SELECT);
				}
				else {
					bezt->f3 = data->select?(bezt->f3|SELECT):(bezt->f3&~SELECT);
				}
			}

			if (bezt == cu->lastsel && !(bezt->f2 & 1)) cu->lastsel = NULL;
		}
	}
}
static int do_nurbs_box_select(ViewContext *vc, rcti *rect, int select, int extend)
{
	BoxSelectUserData data;
	
	data.vc = vc;
	data.rect = rect;
	data.select = select;

	if (extend == 0 && select)
		CU_deselect_all(vc->obedit);

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
	nurbs_foreachScreenVert(vc, do_nurbs_box_select__doSelect, &data);

	return OPERATOR_FINISHED;
}

static void do_lattice_box_select__doSelect(void *userData, BPoint *bp, int x, int y)
{
	BoxSelectUserData *data = userData;

	if (BLI_in_rcti(data->rect, x, y)) {
		bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
	}
}
static int do_lattice_box_select(ViewContext *vc, rcti *rect, int select, int extend)
{
	BoxSelectUserData data;

	data.vc= vc;
	data.rect = rect;
	data.select = select;

	if (extend == 0 && select)
		ED_setflagsLatt(vc->obedit, 0);

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
	lattice_foreachScreenVert(vc, do_lattice_box_select__doSelect, &data);
	
	return OPERATOR_FINISHED;
}

static void do_mesh_box_select__doSelectVert(void *userData, BMVert *eve, int x, int y, int UNUSED(index))
{
	BoxSelectUserData *data = userData;

	if (BLI_in_rcti(data->rect, x, y)) {
		BM_elem_select_set(data->vc->em->bm, eve, data->select);
	}
}
static void do_mesh_box_select__doSelectEdge(void *userData, BMEdge *eed, int x0, int y0, int x1, int y1, int index)
{
	BoxSelectUserData *data = userData;

	if (EDBM_check_backbuf(bm_solidoffs+index)) {
		if (data->pass==0) {
			if (edge_fully_inside_rect(data->rect, x0, y0, x1, y1)) {
				BM_elem_select_set(data->vc->em->bm, eed, data->select);
				data->done = 1;
			}
		}
		else {
			if (edge_inside_rect(data->rect, x0, y0, x1, y1)) {
				BM_elem_select_set(data->vc->em->bm, eed, data->select);
			}
		}
	}
}
static void do_mesh_box_select__doSelectFace(void *userData, BMFace *efa, int x, int y, int UNUSED(index))
{
	BoxSelectUserData *data = userData;

	if (BLI_in_rcti(data->rect, x, y)) {
		BM_elem_select_set(data->vc->em->bm, efa, data->select);
	}
}
static int do_mesh_box_select(ViewContext *vc, rcti *rect, int select, int extend)
{
	BoxSelectUserData data;
	ToolSettings *ts= vc->scene->toolsettings;
	int bbsel;
	
	data.vc= vc;
	data.rect = rect;
	data.select = select;
	data.pass = 0;
	data.done = 0;

	if (extend == 0 && select)
		EDBM_flag_disable_all(vc->em, BM_ELEM_SELECT);

	/* for non zbuf projections, dont change the GL state */
	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

	glLoadMatrixf(vc->rv3d->viewmat);
	bbsel= EDBM_init_backbuf_border(vc, rect->xmin, rect->ymin, rect->xmax, rect->ymax);

	if (ts->selectmode & SCE_SELECT_VERTEX) {
		if (bbsel) {
			EDBM_backbuf_checkAndSelectVerts(vc->em, select);
		}
		else {
			mesh_foreachScreenVert(vc, do_mesh_box_select__doSelectVert, &data, V3D_CLIP_TEST_RV3D_CLIPPING);
		}
	}
	if (ts->selectmode & SCE_SELECT_EDGE) {
			/* Does both bbsel and non-bbsel versions (need screen cos for both) */

		data.pass = 0;
		mesh_foreachScreenEdge(vc, do_mesh_box_select__doSelectEdge, &data, V3D_CLIP_TEST_OFF);

		if (data.done==0) {
			data.pass = 1;
			mesh_foreachScreenEdge(vc, do_mesh_box_select__doSelectEdge, &data, V3D_CLIP_TEST_OFF);
		}
	}
	
	if (ts->selectmode & SCE_SELECT_FACE) {
		if (bbsel) {
			EDBM_backbuf_checkAndSelectFaces(vc->em, select);
		}
		else {
			mesh_foreachScreenFace(vc, do_mesh_box_select__doSelectFace, &data);
		}
	}
	
	EDBM_free_backbuf();
		
	EDBM_selectmode_flush(vc->em);
	
	return OPERATOR_FINISHED;
}

static int do_meta_box_select(ViewContext *vc, rcti *rect, int select, int extend)
{
	MetaBall *mb = (MetaBall*)vc->obedit->data;
	MetaElem *ml;
	int a;

	unsigned int buffer[4*MAXPICKBUF];
	short hits;

	hits= view3d_opengl_select(vc, buffer, MAXPICKBUF, rect);

	if (extend == 0 && select) {
		for (ml= mb->editelems->first; ml; ml= ml->next) {
			ml->flag &= ~SELECT;
		}
	}
	
	for (ml= mb->editelems->first; ml; ml= ml->next) {
		for (a=0; a<hits; a++) {
			if (ml->selcol1==buffer[ (4 * a) + 3 ]) {
				ml->flag |= MB_SCALE_RAD;
				if (select)	ml->flag |= SELECT;
				else		ml->flag &= ~SELECT;
				break;
			}
			if (ml->selcol2==buffer[ (4 * a) + 3 ]) {
				ml->flag &= ~MB_SCALE_RAD;
				if (select)	ml->flag |= SELECT;
				else		ml->flag &= ~SELECT;
				break;
			}
		}
	}

	return OPERATOR_FINISHED;
}

static int do_armature_box_select(ViewContext *vc, rcti *rect, short select, short extend)
{
	bArmature *arm= vc->obedit->data;
	EditBone *ebone;
	int a;

	unsigned int buffer[4*MAXPICKBUF];
	short hits;

	hits= view3d_opengl_select(vc, buffer, MAXPICKBUF, rect);
	
	/* clear flag we use to detect point was affected */
	for (ebone= arm->edbo->first; ebone; ebone= ebone->next)
		ebone->flag &= ~BONE_DONE;
	
	if (extend==0 && select)
		ED_armature_deselect_all_visible(vc->obedit);

	/* first we only check points inside the border */
	for (a=0; a<hits; a++) {
		int index = buffer[(4*a)+3];
		if (index!=-1) {
			ebone = BLI_findlink(arm->edbo, index & ~(BONESEL_ANY));
			if ((ebone->flag & BONE_UNSELECTABLE)==0) {
				if (index & BONESEL_TIP) {
					ebone->flag |= BONE_DONE;
					if (select)	ebone->flag |= BONE_TIPSEL;
					else		ebone->flag &= ~BONE_TIPSEL;
				}
				
				if (index & BONESEL_ROOT) {
					ebone->flag |= BONE_DONE;
					if (select)	ebone->flag |= BONE_ROOTSEL;
					else		ebone->flag &= ~BONE_ROOTSEL;
				}
			}
		}
	}
	
	/* now we have to flush tag from parents... */
	for (ebone= arm->edbo->first; ebone; ebone= ebone->next) {
		if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
			if (ebone->parent->flag & BONE_DONE)
				ebone->flag |= BONE_DONE;
		}
	}
	
	/* only select/deselect entire bones when no points where in the rect */
	for (a=0; a<hits; a++) {
		int index = buffer[(4*a)+3];
		if (index!=-1) {
			ebone = BLI_findlink(arm->edbo, index & ~(BONESEL_ANY));
			if (index & BONESEL_BONE) {
				if ((ebone->flag & BONE_UNSELECTABLE)==0) {
					if (!(ebone->flag & BONE_DONE)) {
						if (select)
							ebone->flag |= (BONE_ROOTSEL|BONE_TIPSEL|BONE_SELECTED);
						else
							ebone->flag &= ~(BONE_ROOTSEL|BONE_TIPSEL|BONE_SELECTED);
					}
				}
			}
		}
	}
	
	ED_armature_sync_selection(arm->edbo);
	
	return OPERATOR_CANCELLED;
}

static int do_object_pose_box_select(bContext *C, ViewContext *vc, rcti *rect, int select, int extend)
{
	Bone *bone;
	Object *ob= vc->obact;
	unsigned int *vbuffer=NULL; /* selection buffer	*/
	unsigned int *col;			/* color in buffer	*/
	int bone_only;
	int bone_selected=0;
	int totobj= MAXPICKBUF;	// XXX solve later
	short hits;
	
	if ((ob) && (ob->mode & OB_MODE_POSE))
		bone_only= 1;
	else
		bone_only= 0;
	
	if (extend == 0 && select) {
		if (bone_only) {
			CTX_DATA_BEGIN(C, bPoseChannel *, pchan, visible_pose_bones) {
				if ((pchan->bone->flag & BONE_UNSELECTABLE)==0) {
					pchan->bone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
				}
			}
			CTX_DATA_END;
		}
		else {
			object_deselect_all_visible(vc->scene, vc->v3d);
		}
	}

	/* selection buffer now has bones potentially too, so we add MAXPICKBUF */
	vbuffer = MEM_mallocN(4 * (totobj+MAXPICKBUF) * sizeof(unsigned int), "selection buffer");
	hits= view3d_opengl_select(vc, vbuffer, 4*(totobj+MAXPICKBUF), rect);
	/*
	LOGIC NOTES (theeth):
	The buffer and ListBase have the same relative order, which makes the selection
	very simple. Loop through both data sets at the same time, if the color
	is the same as the object, we have a hit and can move to the next color
	and object pair, if not, just move to the next object,
	keeping the same color until we have a hit.

	The buffer order is defined by OGL standard, hopefully no stupid GFX card
	does it incorrectly.
	*/

	if (hits>0) { /* no need to loop if there's no hit */
		Base *base;
		col = vbuffer + 3;
		
		for (base= vc->scene->base.first; base && hits; base= base->next) {
			if (BASE_SELECTABLE(vc->v3d, base)) {
				while (base->selcol == (*col & 0xFFFF)) {	/* we got an object */
					
					if (*col & 0xFFFF0000) {					/* we got a bone */
						bone = get_indexed_bone(base->object, *col & ~(BONESEL_ANY));
						if (bone) {
							if (select) {
								if ((bone->flag & BONE_UNSELECTABLE)==0) {
									bone->flag |= BONE_SELECTED;
									bone_selected=1;
// XXX									select_actionchannel_by_name(base->object->action, bone->name, 1);
								}
							}
							else {
								bArmature *arm= base->object->data;
								bone->flag &= ~BONE_SELECTED;
// XXX									select_actionchannel_by_name(base->object->action, bone->name, 0);
								if (arm->act_bone==bone)
									arm->act_bone= NULL;
								
							}
						}
					}
					else if (!bone_only) {
						if (select)
							ED_base_object_select(base, BA_SELECT);
						else
							ED_base_object_select(base, BA_DESELECT);
					}

					col+=4;	/* next color */
					hits--;
					if (hits==0) break;
				}
			}
			
			if (bone_selected) {
				WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, base->object);
			}
		}

		WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, vc->scene);

	}
	MEM_freeN(vbuffer);

	return hits > 0 ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static int view3d_borderselect_exec(bContext *C, wmOperator *op)
{
	ViewContext vc;
	rcti rect;
	short extend;
	short select;

	int ret= OPERATOR_CANCELLED;

	view3d_operator_needs_opengl(C);

	/* setup view context for argument to callbacks */
	view3d_set_viewcontext(C, &vc);
	
	select= (RNA_int_get(op->ptr, "gesture_mode")==GESTURE_MODAL_SELECT);
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");
	extend = RNA_boolean_get(op->ptr, "extend");

	if (vc.obedit) {
		switch(vc.obedit->type) {
		case OB_MESH:
			vc.em= ((Mesh *)vc.obedit->data)->edit_btmesh;
			ret= do_mesh_box_select(&vc, &rect, select, extend);
//			if (EM_texFaceCheck())
			if (ret & OPERATOR_FINISHED) {
				WM_event_add_notifier(C, NC_GEOM|ND_SELECT, vc.obedit->data);
			}
			break;
		case OB_CURVE:
		case OB_SURF:
			ret= do_nurbs_box_select(&vc, &rect, select, extend);
			if (ret & OPERATOR_FINISHED) {
				WM_event_add_notifier(C, NC_GEOM|ND_SELECT, vc.obedit->data);
			}
			break;
		case OB_MBALL:
			ret= do_meta_box_select(&vc, &rect, select, extend);
			if (ret & OPERATOR_FINISHED) {
				WM_event_add_notifier(C, NC_GEOM|ND_SELECT, vc.obedit->data);
			}
			break;
		case OB_ARMATURE:
			ret= do_armature_box_select(&vc, &rect, select, extend);
			if (ret & OPERATOR_FINISHED) {
				WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, vc.obedit);
			}
			break;
		case OB_LATTICE:
			ret= do_lattice_box_select(&vc, &rect, select, extend);		
			if (ret & OPERATOR_FINISHED) {
				WM_event_add_notifier(C, NC_GEOM|ND_SELECT, vc.obedit->data);
			}
			break;			
		default:
			assert(!"border select on incorrect object type");
		}
	}
	else {	/* no editmode, unified for bones and objects */
		if (vc.obact && vc.obact->mode & OB_MODE_SCULPT) {
			/* pass */
		}
		else if (vc.obact && paint_facesel_test(vc.obact)) {
			ret= do_paintface_box_select(&vc, &rect, select, extend);
		}
		else if (vc.obact && paint_vertsel_test(vc.obact)) {
			ret= do_paintvert_box_select(&vc, &rect, select, extend);
		}
		else if (vc.obact && vc.obact->mode & OB_MODE_PARTICLE_EDIT) {
			ret= PE_border_select(C, &rect, select, extend);
		}
		else { /* object mode with none active */
			ret= do_object_pose_box_select(C, &vc, &rect, select, extend);
		}
	}

	return ret;
} 


/* *****************Selection Operators******************* */

/* ****** Border Select ****** */
void VIEW3D_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->description= "Select items using border selection";
	ot->idname= "VIEW3D_OT_select_border";
	
	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= view3d_borderselect_exec;
	ot->modal= WM_border_select_modal;
	ot->poll= view3d_selectable_data;
	ot->cancel= WM_border_select_cancel;
	
	/* flags */
	ot->flag= OPTYPE_UNDO;
	
	/* rna */
	WM_operator_properties_gesture_border(ot, TRUE);
}

/* much like facesel_face_pick()*/
/* returns 0 if not found, otherwise 1 */
static int vertsel_vert_pick(struct bContext *C, Mesh *me, const int mval[2], unsigned int *index, int size)
{
	ViewContext vc;
	view3d_set_viewcontext(C, &vc);

	if (!me || me->totvert==0)
		return 0;

	if (size > 0) {
		/* sample rect to increase changes of selecting, so that when clicking
		   on an face in the backbuf, we can still select a vert */

		int dist;
		*index = view3d_sample_backbuf_rect(&vc, mval, size, 1, me->totvert+1, &dist,0,NULL, NULL);
	}
	else {
		/* sample only on the exact position */
		*index = view3d_sample_backbuf(&vc, mval[0], mval[1]);
	}

	if ((*index)<=0 || (*index)>(unsigned int)me->totvert)
		return 0;

	(*index)--;
	
	return 1;
}

/* mouse selection in weight paint */
/* gets called via generic mouse select operator */
static int mouse_weight_paint_vertex_select(bContext *C, const int mval[2], short extend, Object *obact)
{
	Mesh* me= obact->data; /* already checked for NULL */
	unsigned int index = 0;
	MVert *mv;

	if (vertsel_vert_pick(C, me, mval, &index, 50)) {
		mv = me->mvert+index;
		if (extend) {
			mv->flag ^= SELECT;
		}
		else {
			paintvert_deselect_all_visible(obact, SEL_DESELECT, FALSE);
			mv->flag |= SELECT;
		}
		paintvert_flush_flags(obact);
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obact->data);
		return 1;
	}
	return 0;
}

/* ****** Mouse Select ****** */


static int view3d_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *obedit= CTX_data_edit_object(C);
	Object *obact= CTX_data_active_object(C);
	short extend= RNA_boolean_get(op->ptr, "extend");
	short center= RNA_boolean_get(op->ptr, "center");
	short enumerate= RNA_boolean_get(op->ptr, "enumerate");
	short object= RNA_boolean_get(op->ptr, "object");
	int	retval = 0;

	view3d_operator_needs_opengl(C);

	if (object) {
		obedit= NULL;
		obact= NULL;

		/* ack, this is incorrect but to do this correctly we would need an
		 * alternative editmode/objectmode keymap, this copies the functionality
		 * from 2.4x where Ctrl+Select in editmode does object select only */
		center= FALSE;
	}

	if (obedit && object==FALSE) {
		if (obedit->type==OB_MESH)
			retval = mouse_mesh(C, event->mval, extend);
		else if (obedit->type==OB_ARMATURE)
			retval = mouse_armature(C, event->mval, extend);
		else if (obedit->type==OB_LATTICE)
			retval = mouse_lattice(C, event->mval, extend);
		else if (ELEM(obedit->type, OB_CURVE, OB_SURF))
			retval = mouse_nurb(C, event->mval, extend);
		else if (obedit->type==OB_MBALL)
			retval = mouse_mball(C, event->mval, extend);
			
	}
	else if (obact && obact->mode & OB_MODE_SCULPT)
		return OPERATOR_CANCELLED;
	else if (obact && obact->mode & OB_MODE_PARTICLE_EDIT)
		return PE_mouse_particles(C, event->mval, extend);
	else if (obact && paint_facesel_test(obact))
		retval = paintface_mouse_select(C, obact, event->mval, extend);
	else if (paint_vertsel_test(obact))
		retval = mouse_weight_paint_vertex_select(C, event->mval, extend, obact);
	else
		retval = mouse_select(C, event->mval, extend, center, enumerate);

	/* passthrough allows tweaks
	 * FINISHED to signal one operator worked
	 * */
	if (retval)
		return OPERATOR_PASS_THROUGH|OPERATOR_FINISHED;
	else
		return OPERATOR_PASS_THROUGH; /* nothing selected, just passthrough */
}

void VIEW3D_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Activate/Select";
	ot->description= "Activate/select item(s)";
	ot->idname= "VIEW3D_OT_select";
	
	/* api callbacks */
	ot->invoke= view3d_select_invoke;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend selection instead of deselecting everything first");
	RNA_def_boolean(ot->srna, "center", 0, "Center", "Use the object center when selecting, in editmode used to extend object selection");
	RNA_def_boolean(ot->srna, "enumerate", 0, "Enumerate", "List objects under the mouse (object mode only)");
	RNA_def_boolean(ot->srna, "object", 0, "Object", "Use object selection (editmode only)");
}


/* -------------------- circle select --------------------------------------------- */

typedef struct CircleSelectUserData {
	ViewContext *vc;
	short select;
	int mval[2];
	float radius;
} CircleSelectUserData;

static void mesh_circle_doSelectVert(void *userData, BMVert *eve, int x, int y, int UNUSED(index))
{
	CircleSelectUserData *data = userData;
	int mx = x - data->mval[0], my = y - data->mval[1];
	float r = sqrt(mx*mx + my*my);

	if (r<=data->radius) {
		BM_elem_select_set(data->vc->em->bm, eve, data->select);
	}
}
static void mesh_circle_doSelectEdge(void *userData, BMEdge *eed, int x0, int y0, int x1, int y1, int UNUSED(index))
{
	CircleSelectUserData *data = userData;

	if (edge_inside_circle(data->mval[0], data->mval[1], (short) data->radius, x0, y0, x1, y1)) {
		BM_elem_select_set(data->vc->em->bm, eed, data->select);
	}
}
static void mesh_circle_doSelectFace(void *userData, BMFace *efa, int x, int y, int UNUSED(index))
{
	CircleSelectUserData *data = userData;
	int mx = x - data->mval[0], my = y - data->mval[1];
	float r = sqrt(mx*mx + my*my);
	
	if (r<=data->radius) {
		BM_elem_select_set(data->vc->em->bm, efa, data->select);
	}
}

static void mesh_circle_select(ViewContext *vc, int select, const int mval[2], float rad)
{
	ToolSettings *ts= vc->scene->toolsettings;
	int bbsel;
	CircleSelectUserData data;
	
	bbsel= EDBM_init_backbuf_circle(vc, mval[0], mval[1], (short)(rad+1.0));
	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */

	vc->em= ((Mesh *)vc->obedit->data)->edit_btmesh;

	data.vc = vc;
	data.select = select;
	data.mval[0] = mval[0];
	data.mval[1] = mval[1];
	data.radius = rad;

	if (ts->selectmode & SCE_SELECT_VERTEX) {
		if (bbsel) {
			EDBM_backbuf_checkAndSelectVerts(vc->em, select==LEFTMOUSE);
		}
		else {
			mesh_foreachScreenVert(vc, mesh_circle_doSelectVert, &data, V3D_CLIP_TEST_RV3D_CLIPPING);
		}
	}

	if (ts->selectmode & SCE_SELECT_EDGE) {
		if (bbsel) {
			EDBM_backbuf_checkAndSelectEdges(vc->em, select==LEFTMOUSE);
		}
		else {
			mesh_foreachScreenEdge(vc, mesh_circle_doSelectEdge, &data, V3D_CLIP_TEST_OFF);
		}
	}
	
	if (ts->selectmode & SCE_SELECT_FACE) {
		if (bbsel) {
			EDBM_backbuf_checkAndSelectFaces(vc->em, select==LEFTMOUSE);
		}
		else {
			mesh_foreachScreenFace(vc, mesh_circle_doSelectFace, &data);
		}
	}

	EDBM_free_backbuf();
	EDBM_selectmode_flush(vc->em);
}

static void paint_facesel_circle_select(ViewContext *vc, int select, const int mval[2], float rad)
{
	Object *ob= vc->obact;
	Mesh *me = ob?ob->data:NULL;
	/* int bbsel; */ /* UNUSED */

	if (me) {
		bm_vertoffs= me->totpoly+1;	/* max index array */

		/* bbsel= */ /* UNUSED */ EDBM_init_backbuf_circle(vc, mval[0], mval[1], (short)(rad+1.0));
		EDBM_backbuf_checkAndSelectTFaces(me, select==LEFTMOUSE);
		EDBM_free_backbuf();
	}
}


static void paint_vertsel_circle_select(ViewContext *vc, int select, const int mval[2], float rad)
{
	Object *ob= vc->obact;
	Mesh *me = ob?ob->data:NULL;
	/* int bbsel; */ /* UNUSED */
	/* CircleSelectUserData data = {NULL}; */ /* UNUSED */
	if (me) {
		bm_vertoffs= me->totvert+1;	/* max index array */

		/* bbsel= */ /* UNUSED */ EDBM_init_backbuf_circle(vc, mval[0], mval[1], (short)(rad+1.0f));
		EDBM_backbuf_checkAndSelectVerts_obmode(me, select==LEFTMOUSE);
		EDBM_free_backbuf();

		paintvert_flush_flags(ob);
	}
}


static void nurbscurve_circle_doSelect(void *userData, Nurb *UNUSED(nu), BPoint *bp, BezTriple *bezt, int beztindex, int x, int y)
{
	CircleSelectUserData *data = userData;
	int mx = x - data->mval[0], my = y - data->mval[1];
	float r = sqrt(mx*mx + my*my);
	Object *obedit= data->vc->obedit;
	Curve *cu= (Curve*)obedit->data;

	if (r<=data->radius) {
		if (bp) {
			bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);

			if (bp == cu->lastsel && !(bp->f1 & 1)) cu->lastsel = NULL;
		}
		else {
			if (cu->drawflag & CU_HIDE_HANDLES) {
				/* can only be beztindex==0 here since handles are hidden */
				bezt->f1 = bezt->f2 = bezt->f3 = data->select?(bezt->f2|SELECT):(bezt->f2&~SELECT);
			}
			else {
				if (beztindex==0) {
					bezt->f1 = data->select?(bezt->f1|SELECT):(bezt->f1&~SELECT);
				}
				else if (beztindex==1) {
					bezt->f2 = data->select?(bezt->f2|SELECT):(bezt->f2&~SELECT);
				}
				else {
					bezt->f3 = data->select?(bezt->f3|SELECT):(bezt->f3&~SELECT);
				}
			}

			if (bezt == cu->lastsel && !(bezt->f2 & 1)) cu->lastsel = NULL;
		}
	}
}
static void nurbscurve_circle_select(ViewContext *vc, int select, const int mval[2], float rad)
{
	CircleSelectUserData data;

	/* set vc-> edit data */
	
	data.select = select;
	data.mval[0] = mval[0];
	data.mval[1] = mval[1];
	data.radius = rad;
	data.vc = vc;

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
	nurbs_foreachScreenVert(vc, nurbscurve_circle_doSelect, &data);
}


static void latticecurve_circle_doSelect(void *userData, BPoint *bp, int x, int y)
{
	CircleSelectUserData *data = userData;
	int mx = x - data->mval[0], my = y - data->mval[1];
	float r = sqrt(mx*mx + my*my);

	if (r<=data->radius) {
		bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
	}
}
static void lattice_circle_select(ViewContext *vc, int select, const int mval[2], float rad)
{
	CircleSelectUserData data;

	/* set vc-> edit data */
	
	data.select = select;
	data.mval[0] = mval[0];
	data.mval[1] = mval[1];
	data.radius = rad;

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
	lattice_foreachScreenVert(vc, latticecurve_circle_doSelect, &data);
}


// NOTE: pose-bone case is copied from editbone case...
static short pchan_circle_doSelectJoint(void *userData, bPoseChannel *pchan, int x, int y)
{
	CircleSelectUserData *data = userData;
	int mx = x - data->mval[0], my = y - data->mval[1];
	float r = sqrt(mx*mx + my*my);
	
	if (r <= data->radius) {
		if (data->select)
			pchan->bone->flag |= BONE_SELECTED;
		else
			pchan->bone->flag &= ~BONE_SELECTED;
		return 1;
	}
	return 0;
}
static void pose_circle_select(ViewContext *vc, int select, const int mval[2], float rad)
{
	CircleSelectUserData data;
	bArmature *arm = vc->obact->data;
	bPose *pose = vc->obact->pose;
	bPoseChannel *pchan;
	int change= FALSE;
	
	/* set vc->edit data */
	data.select = select;
	data.mval[0] = mval[0];
	data.mval[1] = mval[1];
	data.radius = rad;

	ED_view3d_init_mats_rv3d(vc->obact, vc->rv3d); /* for foreach's screen/vert projection */
	
	/* check each PoseChannel... */
	// TODO: could be optimised at some point
	for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
		short sco1[2], sco2[2], didpoint=0;
		float vec[3];
		
		/* skip invisible bones */
		if (PBONE_VISIBLE(arm, pchan->bone) == 0)
			continue;
		
		/* project head location to screenspace */
		mul_v3_m4v3(vec, vc->obact->obmat, pchan->pose_head);
		project_short(vc->ar, vec, sco1);
		
		/* project tail location to screenspace */
		mul_v3_m4v3(vec, vc->obact->obmat, pchan->pose_tail);
		project_short(vc->ar, vec, sco2);
		
		/* check if the head and/or tail is in the circle 
		 *	- the call to check also does the selection already
		 */
		if (pchan_circle_doSelectJoint(&data, pchan, sco1[0], sco1[1]))
			didpoint= 1;
		if (pchan_circle_doSelectJoint(&data, pchan, sco2[0], sco2[1]))
			didpoint= 1;
		
		change |= didpoint;
	}

	if (change) {
		WM_main_add_notifier(NC_OBJECT|ND_BONE_SELECT, vc->obact);
	}
}

static short armature_circle_doSelectJoint(void *userData, EditBone *ebone, int x, int y, short head)
{
	CircleSelectUserData *data = userData;
	int mx = x - data->mval[0], my = y - data->mval[1];
	float r = sqrt(mx*mx + my*my);
	
	if (r <= data->radius) {
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
static void armature_circle_select(ViewContext *vc, int select, const int mval[2], float rad)
{
	CircleSelectUserData data;
	bArmature *arm= vc->obedit->data;
	EditBone *ebone;
	int change= FALSE;
	
	/* set vc->edit data */
	data.select = select;
	data.mval[0] = mval[0];
	data.mval[1] = mval[1];
	data.radius = rad;

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
	
	/* check each EditBone... */
	// TODO: could be optimised at some point
	for (ebone= arm->edbo->first; ebone; ebone=ebone->next) {
		short sco1[2], sco2[2], didpoint=0;
		float vec[3];
		
		/* project head location to screenspace */
		mul_v3_m4v3(vec, vc->obedit->obmat, ebone->head);
		project_short(vc->ar, vec, sco1);
		
		/* project tail location to screenspace */
		mul_v3_m4v3(vec, vc->obedit->obmat, ebone->tail);
		project_short(vc->ar, vec, sco2);
		
		/* check if the head and/or tail is in the circle 
		 *	- the call to check also does the selection already
		 */
		if (armature_circle_doSelectJoint(&data, ebone, sco1[0], sco1[1], 1))
			didpoint= 1;
		if (armature_circle_doSelectJoint(&data, ebone, sco2[0], sco2[1], 0))
			didpoint= 1;
			
		/* only if the endpoints didn't get selected, deal with the middle of the bone too */
		// XXX should we just do this always?
		if ( (didpoint==0) && edge_inside_circle(mval[0], mval[1], rad, sco1[0], sco1[1], sco2[0], sco2[1]) ) {
			if (select) 
				ebone->flag |= BONE_TIPSEL|BONE_ROOTSEL|BONE_SELECTED;
			else 
				ebone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
			change= TRUE;
		}
		
		change |= didpoint;
	}

	if (change) {
		ED_armature_sync_selection(arm->edbo);
		ED_armature_validate_active(arm);
		WM_main_add_notifier(NC_OBJECT|ND_BONE_SELECT, vc->obedit);
	}
}

/** Callbacks for circle selection in Editmode */

static void obedit_circle_select(ViewContext *vc, short select, const int mval[2], float rad)
{
	switch(vc->obedit->type) {		
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
	default:
		return;
	}
}

/* not a real operator, only for circle test */
static int view3d_circle_select_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	Scene *scene= CTX_data_scene(C);
	Object *obact= CTX_data_active_object(C);
	View3D *v3d= sa->spacedata.first;
	int x= RNA_int_get(op->ptr, "x");
	int y= RNA_int_get(op->ptr, "y");
	int radius= RNA_int_get(op->ptr, "radius");
	int gesture_mode= RNA_int_get(op->ptr, "gesture_mode");
	int select;
	
	select= (gesture_mode==GESTURE_MODAL_SELECT);

	if ( CTX_data_edit_object(C) || paint_facesel_test(obact) || paint_vertsel_test(obact) ||
		(obact && (obact->mode & (OB_MODE_PARTICLE_EDIT|OB_MODE_POSE))) )
	{
		ViewContext vc;
		int mval[2];
		
		view3d_operator_needs_opengl(C);
		
		view3d_set_viewcontext(C, &vc);
		mval[0]= x;
		mval[1]= y;

		if (CTX_data_edit_object(C)) {
			obedit_circle_select(&vc, select, mval, (float)radius);
			WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obact->data);
		}
		else if (paint_facesel_test(obact)) {
			paint_facesel_circle_select(&vc, select, mval, (float)radius);
			WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obact->data);
		}
		else if (paint_vertsel_test(obact)) {
			paint_vertsel_circle_select(&vc, select, mval, (float)radius);
			WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obact->data);
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
		Base *base;
		select= select?BA_SELECT:BA_DESELECT;
		for (base= FIRSTBASE; base; base= base->next) {
			if (BASE_SELECTABLE(v3d, base)) {
				project_short(ar, base->object->obmat[3], &base->sx);
				if (base->sx!=IS_CLIPPED) {
					int dx= base->sx-x;
					int dy= base->sy-y;
					if ( dx*dx + dy*dy < radius*radius)
						ED_base_object_select(base, select);
				}
			}
		}
		
		WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
	}
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_select_circle(wmOperatorType *ot)
{
	ot->name= "Circle Select";
	ot->description= "Select items using circle selection";
	ot->idname= "VIEW3D_OT_select_circle";
	
	ot->invoke= WM_gesture_circle_invoke;
	ot->modal= WM_gesture_circle_modal;
	ot->exec= view3d_circle_select_exec;
	ot->poll= view3d_selectable_data;
	ot->cancel= WM_gesture_circle_cancel;
	
	/* flags */
	ot->flag= OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "radius", 0, INT_MIN, INT_MAX, "Radius", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "gesture_mode", 0, INT_MIN, INT_MAX, "Event Type", "", INT_MIN, INT_MAX);
}
