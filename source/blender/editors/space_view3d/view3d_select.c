/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_paint.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "RE_pipeline.h"	// make_stars

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_particle.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_retopo.h"
#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"
#include "ED_mball.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "PIL_time.h" /* smoothview */

#include "view3d_intern.h"	// own include


void view3d_set_viewcontext(bContext *C, ViewContext *vc)
{
	memset(vc, 0, sizeof(ViewContext));
	vc->ar= CTX_wm_region(C);
	vc->scene= CTX_data_scene(C);
	vc->v3d= CTX_wm_view3d(C);
	vc->rv3d= vc->ar->regiondata;
	vc->obact= CTX_data_active_object(C);
	vc->obedit= CTX_data_edit_object(C); 
}

void view3d_get_view_aligned_coordinate(ViewContext *vc, float *fp, short mval[2])
{
	float dvec[3];
	short mx, my;
	
	mx= mval[0];
	my= mval[1];
	
	project_short_noclip(vc->ar, fp, mval);
	
	initgrabz(vc->rv3d, fp[0], fp[1], fp[2]);
	
	if(mval[0]!=IS_CLIPPED) {
		window_to_3d_delta(vc->ar, dvec, mval[0]-mx, mval[1]-my);
		VecSubf(fp, fp, dvec);
	}
}

void view3d_get_transformation(ViewContext *vc, Object *ob, bglMats *mats)
{
	float cpy[4][4];
	int i, j;

	Mat4MulMat4(cpy, ob->obmat, vc->rv3d->viewmat);

	for(i = 0; i < 4; ++i) {
		for(j = 0; j < 4; ++j) {
			mats->projection[i*4+j] = vc->rv3d->winmat[i][j];
			mats->modelview[i*4+j] = cpy[i][j];
		}
	}

	mats->viewport[0] = vc->ar->winrct.xmin;
	mats->viewport[1] = vc->ar->winrct.ymin;
	mats->viewport[2] = vc->ar->winx;
	mats->viewport[3] = vc->ar->winy;	
}

/* ********************** view3d_select: selection manipulations ********************* */

/* XXX to solve *************** */
static void BIF_undo_push() {}
/* XXX end ********************* */

/* local prototypes */

void EM_backbuf_checkAndSelectVerts(EditMesh *em, int select)
{
	EditVert *eve;
	int index= em_wireoffs;

	for(eve= em->verts.first; eve; eve= eve->next, index++) {
		if(eve->h==0) {
			if(EM_check_backbuf(index)) {
				eve->f = select?(eve->f|1):(eve->f&~1);
			}
		}
	}
}

void EM_backbuf_checkAndSelectEdges(EditMesh *em, int select)
{
	EditEdge *eed;
	int index= em_solidoffs;

	for(eed= em->edges.first; eed; eed= eed->next, index++) {
		if(eed->h==0) {
			if(EM_check_backbuf(index)) {
				EM_select_edge(eed, select);
			}
		}
	}
}

void EM_backbuf_checkAndSelectFaces(EditMesh *em, int select)
{
	EditFace *efa;
	int index= 1;

	for(efa= em->faces.first; efa; efa= efa->next, index++) {
		if(efa->h==0) {
			if(EM_check_backbuf(index)) {
				EM_select_face_fgon(em, efa, select);
			}
		}
	}
}

void EM_backbuf_checkAndSelectTFaces(Mesh *me, int select)
{
	MFace *mface = me->mface;
	int a;

	if (mface) {
		for(a=1; a<=me->totface; a++, mface++) {
			if(EM_check_backbuf(a)) {
				mface->flag = select?(mface->flag|ME_FACE_SEL):(mface->flag&~ME_FACE_SEL);
			}
		}
	}
}

void arrows_move_cursor(unsigned short event)
{
#if 0
	short mval[2];

	getmouseco_sc(mval);

	if(event==UPARROWKEY) {
		warp_pointer(mval[0], mval[1]+1);
	} else if(event==DOWNARROWKEY) {
		warp_pointer(mval[0], mval[1]-1);
	} else if(event==LEFTARROWKEY) {
		warp_pointer(mval[0]-1, mval[1]);
	} else if(event==RIGHTARROWKEY) {
		warp_pointer(mval[0]+1, mval[1]);
	}
#endif
}


/* *********************** GESTURE AND LASSO ******************* */

/* helper also for borderselect */
static int edge_fully_inside_rect(rcti *rect, short x1, short y1, short x2, short y2)
{
	return BLI_in_rcti(rect, x1, y1) && BLI_in_rcti(rect, x2, y2);
}

static int edge_inside_rect(rcti *rect, short x1, short y1, short x2, short y2)
{
	int d1, d2, d3, d4;
	
	/* check points in rect */
	if(edge_fully_inside_rect(rect, x1, y1, x2, y2)) return 1;
	
	/* check points completely out rect */
	if(x1<rect->xmin && x2<rect->xmin) return 0;
	if(x1>rect->xmax && x2>rect->xmax) return 0;
	if(y1<rect->ymin && y2<rect->ymin) return 0;
	if(y1>rect->ymax && y2>rect->ymax) return 0;
	
	/* simple check lines intersecting. */
	d1= (y1-y2)*(x1- rect->xmin ) + (x2-x1)*(y1- rect->ymin );
	d2= (y1-y2)*(x1- rect->xmin ) + (x2-x1)*(y1- rect->ymax );
	d3= (y1-y2)*(x1- rect->xmax ) + (x2-x1)*(y1- rect->ymax );
	d4= (y1-y2)*(x1- rect->xmax ) + (x2-x1)*(y1- rect->ymin );
	
	if(d1<0 && d2<0 && d3<0 && d4<0) return 0;
	if(d1>0 && d2>0 && d3>0 && d4>0) return 0;
	
	return 1;
}


#define MOVES_GESTURE 50
#define MOVES_LASSO 500

int lasso_inside(short mcords[][2], short moves, short sx, short sy)
{
	/* we do the angle rule, define that all added angles should be about zero or 2*PI */
	float angletot=0.0, len, dot, ang, cross, fp1[2], fp2[2];
	int a;
	short *p1, *p2;
	
	if(sx==IS_CLIPPED)
		return 0;
	
	p1= mcords[moves-1];
	p2= mcords[0];
	
	/* first vector */
	fp1[0]= (float)(p1[0]-sx);
	fp1[1]= (float)(p1[1]-sy);
	len= sqrt(fp1[0]*fp1[0] + fp1[1]*fp1[1]);
	fp1[0]/= len;
	fp1[1]/= len;
	
	for(a=0; a<moves; a++) {
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
		
		if(cross<0.0) angletot-= ang;
		else angletot+= ang;
		
		/* circulate */
		fp1[0]= fp2[0]; fp1[1]= fp2[1];
		p1= p2;
		p2= mcords[a+1];
	}
	
	if( fabs(angletot) > 4.0 ) return 1;
	return 0;
}

/* edge version for lasso select. we assume boundbox check was done */
int lasso_inside_edge(short mcords[][2], short moves, int x0, int y0, int x1, int y1)
{
	short v1[2], v2[2];
	int a;

	if(x0==IS_CLIPPED || x1==IS_CLIPPED)
		return 0;
	
	v1[0] = x0, v1[1] = y0;
	v2[0] = x1, v2[1] = y1;

	/* check points in lasso */
	if(lasso_inside(mcords, moves, v1[0], v1[1])) return 1;
	if(lasso_inside(mcords, moves, v2[0], v2[1])) return 1;
	
	/* no points in lasso, so we have to intersect with lasso edge */
	
	if( IsectLL2Ds(mcords[0], mcords[moves-1], v1, v2) > 0) return 1;
	for(a=0; a<moves-1; a++) {
		if( IsectLL2Ds(mcords[a], mcords[a+1], v1, v2) > 0) return 1;
	}
	
	return 0;
}


/* warning; lasso select with backbuffer-check draws in backbuf with persp(PERSP_WIN) 
   and returns with persp(PERSP_VIEW). After lasso select backbuf is not OK
*/
static void do_lasso_select_pose(ViewContext *vc, short mcords[][2], short moves, short select)
{
	Object *ob= vc->obact;
	bPoseChannel *pchan;
	float vec[3];
	short sco1[2], sco2[2];
	
	if(ob->type!=OB_ARMATURE || ob->pose==NULL) return;
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		VECCOPY(vec, pchan->pose_head);
		Mat4MulVecfl(ob->obmat, vec);
		project_short(vc->ar, vec, sco1);
		VECCOPY(vec, pchan->pose_tail);
		Mat4MulVecfl(ob->obmat, vec);
		project_short(vc->ar, vec, sco2);
		
		if(lasso_inside_edge(mcords, moves, sco1[0], sco1[1], sco2[0], sco2[1])) {
			if(select) pchan->bone->flag |= BONE_SELECTED;
			else pchan->bone->flag &= ~(BONE_ACTIVE|BONE_SELECTED);
		}
	}
}


static void do_lasso_select_objects(ViewContext *vc, short mcords[][2], short moves, short select)
{
	Base *base;
	
	for(base= vc->scene->base.first; base; base= base->next) {
		if(base->lay & vc->v3d->lay) {
			project_short(vc->ar, base->object->obmat[3], &base->sx);
			if(lasso_inside(mcords, moves, base->sx, base->sy)) {
				
				if(select) ED_base_object_select(base, BA_SELECT);
				else ED_base_object_select(base, BA_DESELECT);
				base->object->flag= base->flag;
			}
			if(base->object->mode & OB_MODE_POSE) {
				do_lasso_select_pose(vc, mcords, moves, select);
			}
		}
	}
}

void lasso_select_boundbox(rcti *rect, short mcords[][2], short moves)
{
	short a;
	
	rect->xmin= rect->xmax= mcords[0][0];
	rect->ymin= rect->ymax= mcords[0][1];
	
	for(a=1; a<moves; a++) {
		if(mcords[a][0]<rect->xmin) rect->xmin= mcords[a][0];
		else if(mcords[a][0]>rect->xmax) rect->xmax= mcords[a][0];
		if(mcords[a][1]<rect->ymin) rect->ymin= mcords[a][1];
		else if(mcords[a][1]>rect->ymax) rect->ymax= mcords[a][1];
	}
}

static void do_lasso_select_mesh__doSelectVert(void *userData, EditVert *eve, int x, int y, int index)
{
	struct { ViewContext vc; rcti *rect; short (*mcords)[2], moves, select, pass, done; } *data = userData;

	if (BLI_in_rcti(data->rect, x, y) && lasso_inside(data->mcords, data->moves, x, y)) {
		eve->f = data->select?(eve->f|1):(eve->f&~1);
	}
}
static void do_lasso_select_mesh__doSelectEdge(void *userData, EditEdge *eed, int x0, int y0, int x1, int y1, int index)
{
	struct { ViewContext vc; rcti *rect; short (*mcords)[2], moves, select, pass, done; } *data = userData;

	if (EM_check_backbuf(em_solidoffs+index)) {
		if (data->pass==0) {
			if (	edge_fully_inside_rect(data->rect, x0, y0, x1, y1)  &&
					lasso_inside(data->mcords, data->moves, x0, y0) &&
					lasso_inside(data->mcords, data->moves, x1, y1)) {
				EM_select_edge(eed, data->select);
				data->done = 1;
			}
		} else {
			if (lasso_inside_edge(data->mcords, data->moves, x0, y0, x1, y1)) {
				EM_select_edge(eed, data->select);
			}
		}
	}
}
static void do_lasso_select_mesh__doSelectFace(void *userData, EditFace *efa, int x, int y, int index)
{
	struct { ViewContext vc; rcti *rect; short (*mcords)[2], moves, select, pass, done; } *data = userData;

	if (BLI_in_rcti(data->rect, x, y) && lasso_inside(data->mcords, data->moves, x, y)) {
		EM_select_face_fgon(data->vc.em, efa, data->select);
	}
}

static void do_lasso_select_mesh(ViewContext *vc, short mcords[][2], short moves, short select)
{
	struct { ViewContext vc; rcti *rect; short (*mcords)[2], moves, select, pass, done; } data;
	ToolSettings *ts= vc->scene->toolsettings;
	rcti rect;
	int bbsel;
	
	lasso_select_boundbox(&rect, mcords, moves);
	
	/* set editmesh */
	vc->em= ((Mesh *)vc->obedit->data)->edit_mesh;

	data.vc= *vc;
	data.rect = &rect;
	data.mcords = mcords;
	data.moves = moves;
	data.select = select;
	data.done = 0;
	data.pass = 0;

	bbsel= EM_mask_init_backbuf_border(vc, mcords, moves, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
	
	if(ts->selectmode & SCE_SELECT_VERTEX) {
		if (bbsel) {
			EM_backbuf_checkAndSelectVerts(vc->em, select);
		} else {
			mesh_foreachScreenVert(vc, do_lasso_select_mesh__doSelectVert, &data, 1);
		}
	}
	if(ts->selectmode & SCE_SELECT_EDGE) {
			/* Does both bbsel and non-bbsel versions (need screen cos for both) */

		data.pass = 0;
		mesh_foreachScreenEdge(vc, do_lasso_select_mesh__doSelectEdge, &data, 0);

		if (data.done==0) {
			data.pass = 1;
			mesh_foreachScreenEdge(vc, do_lasso_select_mesh__doSelectEdge, &data, 0);
		}
	}
	
	if(ts->selectmode & SCE_SELECT_FACE) {
		if (bbsel) {
			EM_backbuf_checkAndSelectFaces(vc->em, select);
		} else {
			mesh_foreachScreenFace(vc, do_lasso_select_mesh__doSelectFace, &data);
		}
	}
	
	EM_free_backbuf();
	EM_selectmode_flush(vc->em);	
}

#if 0
/* this is an exception in that its the only lasso that dosnt use the 3d view (uses space image view) */
static void do_lasso_select_mesh_uv(short mcords[][2], short moves, short select)
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
		
	} else { /* Vert Sel*/
		for (efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if (simaFaceDraw_Check(efa, tf)) {		
				nverts= efa->v4? 4: 3;
				for(i=0; i<nverts; i++) {
					if ((select) != (simaUVSel_Check(efa, tf, i))) {
						uvco_to_areaco_noclip(tf->uv[i], screenUV);
						if (BLI_in_rcti(&rect, screenUV[0], screenUV[1]) && lasso_inside(mcords, moves, screenUV[0], screenUV[1])) {
							if (select) {
								simaUVSel_Set(efa, tf, i);
							} else {
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

static void do_lasso_select_curve__doSelect(void *userData, Nurb *nu, BPoint *bp, BezTriple *bezt, int beztindex, int x, int y)
{
	struct { ViewContext vc; short (*mcords)[2]; short moves; short select; } *data = userData;
	
	if (lasso_inside(data->mcords, data->moves, x, y)) {
		if (bp) {
			bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
		} else {
			Curve *cu= data->vc.obedit->data;
			
			if (cu->drawflag & CU_HIDE_HANDLES) {
				/* can only be beztindex==0 here since handles are hidden */
				bezt->f1 = bezt->f2 = bezt->f3 = data->select?(bezt->f2|SELECT):(bezt->f2&~SELECT);
			} else {
				if (beztindex==0) {
					bezt->f1 = data->select?(bezt->f1|SELECT):(bezt->f1&~SELECT);
				} else if (beztindex==1) {
					bezt->f2 = data->select?(bezt->f2|SELECT):(bezt->f2&~SELECT);
				} else {
					bezt->f3 = data->select?(bezt->f3|SELECT):(bezt->f3&~SELECT);
				}
			}
		}
	}
}

static void do_lasso_select_curve(ViewContext *vc, short mcords[][2], short moves, short select)
{
	struct { ViewContext vc; short (*mcords)[2]; short moves; short select; } data;

	/* set vc->editnurb */
	data.vc = *vc;
	data.mcords = mcords;
	data.moves = moves;
	data.select = select;

	nurbs_foreachScreenVert(vc, do_lasso_select_curve__doSelect, &data);
}

static void do_lasso_select_lattice__doSelect(void *userData, BPoint *bp, int x, int y)
{
	struct { short (*mcords)[2]; short moves; short select; } *data = userData;

	if (lasso_inside(data->mcords, data->moves, x, y)) {
		bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
	}
}
static void do_lasso_select_lattice(ViewContext *vc, short mcords[][2], short moves, short select)
{
	struct { short (*mcords)[2]; short moves; short select; } data;

	/* set editdata in vc */
	data.mcords = mcords;
	data.moves = moves;
	data.select = select;

	lattice_foreachScreenVert(vc, do_lasso_select_lattice__doSelect, &data);
}

static void do_lasso_select_armature(ViewContext *vc, short mcords[][2], short moves, short select)
{
	bArmature *arm= vc->obedit->data;
	EditBone *ebone;
	float vec[3];
	short sco1[2], sco2[2], didpoint;
	
	/* set editdata in vc */
	
	for (ebone= arm->edbo->first; ebone; ebone=ebone->next) {

		VECCOPY(vec, ebone->head);
		Mat4MulVecfl(vc->obedit->obmat, vec);
		project_short(vc->ar, vec, sco1);
		VECCOPY(vec, ebone->tail);
		Mat4MulVecfl(vc->obedit->obmat, vec);
		project_short(vc->ar, vec, sco2);
		
		didpoint= 0;
		if(lasso_inside(mcords, moves, sco1[0], sco1[1])) {
			if(select) ebone->flag |= BONE_ROOTSEL;
			else ebone->flag &= ~BONE_ROOTSEL;
			didpoint= 1;
		}
		if(lasso_inside(mcords, moves, sco2[0], sco2[1])) {
		   if(select) ebone->flag |= BONE_TIPSEL;
		   else ebone->flag &= ~BONE_TIPSEL;
		   didpoint= 1;
		}
		/* if one of points selected, we skip the bone itself */
		if(didpoint==0 && lasso_inside_edge(mcords, moves, sco1[0], sco1[1], sco2[0], sco2[1])) {
			if(select) ebone->flag |= BONE_TIPSEL|BONE_ROOTSEL|BONE_SELECTED;
			else ebone->flag &= ~(BONE_ACTIVE|BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
		}
	}
}

static void do_lasso_select_facemode(ViewContext *vc, short mcords[][2], short moves, short select)
{
	Object *ob= vc->obact;
	Mesh *me= ob?ob->data:NULL;
	rcti rect;
	
	if(me==NULL || me->mtface==NULL) return;
	if(me->totface==0) return;
	
	em_vertoffs= me->totface+1;	/* max index array */
	
	lasso_select_boundbox(&rect, mcords, moves);
	EM_mask_init_backbuf_border(vc, mcords, moves, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
	
	EM_backbuf_checkAndSelectTFaces(me, select);
	
	EM_free_backbuf();
	
// XXX	object_tface_flags_changed(ob, 0);
}

#if 0
static void do_lasso_select_node(short mcords[][2], short moves, short select)
{
	SpaceNode *snode = sa->spacedata.first;
	
	bNode *node;
	rcti rect;
	short node_cent[2];
	float node_centf[2];
	
	lasso_select_boundbox(&rect, mcords, moves);
	
	/* store selection in temp test flag */
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		
		node_centf[0] = (node->totr.xmin+node->totr.xmax)/2;
		node_centf[1] = (node->totr.ymin+node->totr.ymax)/2;
		
		ipoco_to_areaco_noclip(G.v2d, node_centf, node_cent);
		if (BLI_in_rcti(&rect, node_cent[0], node_cent[1]) && lasso_inside(mcords, moves, node_cent[0], node_cent[1])) {
			if (select) {
				node->flag |= SELECT;
			} else {
				node->flag &= ~SELECT;
			}
		}
	}
	BIF_undo_push("Lasso select nodes");
}
#endif

void view3d_lasso_select(bContext *C, ViewContext *vc, short mcords[][2], short moves, short select)
{
	Object *ob = CTX_data_active_object(C);

	if(vc->obedit==NULL) {
		if(paint_facesel_test(ob))
			do_lasso_select_facemode(vc, mcords, moves, select);
		else if(ob && ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT))
			;
		else if(ob && ob->mode & OB_MODE_PARTICLE_EDIT)
			PE_lasso_select(C, mcords, moves, select);
		else  
			do_lasso_select_objects(vc, mcords, moves, select);
	}
	else if(vc->obedit->type==OB_MESH) {
		do_lasso_select_mesh(vc, mcords, moves, select);
	} else if(vc->obedit->type==OB_CURVE || vc->obedit->type==OB_SURF) 
		do_lasso_select_curve(vc, mcords, moves, select);
	else if(vc->obedit->type==OB_LATTICE) 
		do_lasso_select_lattice(vc, mcords, moves, select);
	else if(vc->obedit->type==OB_ARMATURE)
		do_lasso_select_armature(vc, mcords, moves, select);

	BIF_undo_push("Lasso select");
	
}


/* lasso operator gives properties, but since old code works
   with short array we convert */
static int view3d_lasso_select_exec(bContext *C, wmOperator *op)
{
	ViewContext vc;
	int select, i= 0;
	short mcords[1024][2];

	RNA_BEGIN(op->ptr, itemptr, "path") {
		float loc[2];
		
		RNA_float_get_array(&itemptr, "loc", loc);
		mcords[i][0]= (short)loc[0];
		mcords[i][1]= (short)loc[1];
		i++;
		if(i>=1024) break;
	}
	RNA_END;
	
	if(i>1) {
		view3d_operator_needs_opengl(C);
		
		/* setup view context for argument to callbacks */
		view3d_set_viewcontext(C, &vc);
		
		select= !RNA_boolean_get(op->ptr, "deselect");
		view3d_lasso_select(C, &vc, mcords, i, select);
		
		return OPERATOR_FINISHED;
	}
	return OPERATOR_PASS_THROUGH;
}

void VIEW3D_OT_select_lasso(wmOperatorType *ot)
{
	ot->name= "Lasso Select";
	ot->description= "Select items using lasso selection.";
	ot->idname= "VIEW3D_OT_select_lasso";
	
	ot->invoke= WM_gesture_lasso_invoke;
	ot->modal= WM_gesture_lasso_modal;
	ot->exec= view3d_lasso_select_exec;
	ot->poll= WM_operator_winactive;
	
	/* flags */
	ot->flag= OPTYPE_UNDO;
	
	RNA_def_collection_runtime(ot->srna, "path", &RNA_OperatorMousePath, "Path", "");
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Deselect rather than select items.");
}


/* ************************************************* */

#if 0
/* smart function to sample a rect spiralling outside, nice for backbuf selection */
static unsigned int samplerect(unsigned int *buf, int size, unsigned int dontdo)
{
	Base *base;
	unsigned int *bufmin,*bufmax;
	int a,b,rc,tel,aantal,dirvec[4][2],maxob;
	unsigned int retval=0;
	
	base= LASTBASE;
	if(base==0) return 0;
	maxob= base->selcol;

	aantal= (size-1)/2;
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
	buf+= aantal*size+ aantal;

	for(tel=1;tel<=size;tel++) {

		for(a=0;a<2;a++) {
			for(b=0;b<tel;b++) {

				if(*buf && *buf<=maxob && *buf!=dontdo) return *buf;
				if( *buf==dontdo ) retval= dontdo;	/* if only color dontdo is available, still return dontdo */
				
				buf+= (dirvec[rc][0]+dirvec[rc][1]);

				if(buf<bufmin || buf>=bufmax) return retval;
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
#define SEL_MENU_SIZE	22

static void deselectall_except(Scene *scene, Base *b)   /* deselect all except b */
{
	Base *base;
	
	for(base= FIRSTBASE; base; base= base->next) {
		if (base->flag & SELECT) {
			if(b!=base) {
				ED_base_object_select(base, BA_DESELECT);
			}
		}
	}
}

static Base *mouse_select_menu(ViewContext *vc, unsigned int *buffer, int hits, short *mval)
{
	Scene *scene= vc->scene;
	View3D *v3d= vc->v3d;
	Base *baseList[SEL_MENU_SIZE]={NULL}; /*baseList is used to store all possible bases to bring up a menu */
	Base *base;
	short baseCount = 0;
	char menuText[20 + SEL_MENU_SIZE*32] = "Select Object%t";	/* max ob name = 22 */
	char str[32];
	
	for(base=FIRSTBASE; base; base= base->next) {
		if (BASE_SELECTABLE(v3d, base)) {
			baseList[baseCount] = NULL;
			
			/* two selection methods, the CTRL select uses max dist of 15 */
			if(buffer) {
				int a;
				for(a=0; a<hits; a++) {
					/* index was converted */
					if(base->selcol==buffer[ (4 * a) + 3 ]) baseList[baseCount] = base;
				}
			}
			else {
				int temp, dist=15;
				
				project_short(vc->ar, base->object->obmat[3], &base->sx);
				
				temp= abs(base->sx -mval[0]) + abs(base->sy -mval[1]);
				if(temp<dist ) baseList[baseCount] = base;
			}
			
			if(baseList[baseCount]) {
				if (baseCount < SEL_MENU_SIZE) {
					baseList[baseCount] = base;
					sprintf(str, "|%s %%x%d", base->object->id.name+2, baseCount+1);	/* max ob name == 22 */
							strcat(menuText, str);
							baseCount++;
				}
			}
		}
	}

	if(baseCount<=1) return baseList[0];
	else {
		baseCount = -1; // XXX = pupmenu(menuText);
		
		if (baseCount != -1) { /* If nothing is selected then dont do anything */
			return baseList[baseCount-1];
		}
		else return NULL;
	}
}

/* we want a select buffer with bones, if there are... */
/* so check three selection levels and compare */
static short mixed_bones_object_selectbuffer(ViewContext *vc, unsigned int *buffer, short *mval)
{
	rcti rect;
	int offs;
	short a, hits15, hits9=0, hits5=0;
	short has_bones15=0, has_bones9=0, has_bones5=0;
	
	BLI_init_rcti(&rect, mval[0]-14, mval[0]+14, mval[1]-14, mval[1]+14);
	hits15= view3d_opengl_select(vc, buffer, MAXPICKBUF, &rect);
	if(hits15>0) {
		for(a=0; a<hits15; a++) if(buffer[4*a+3] & 0xFFFF0000) has_bones15= 1;
		
		offs= 4*hits15;
		BLI_init_rcti(&rect, mval[0]-9, mval[0]+9, mval[1]-9, mval[1]+9);
		hits9= view3d_opengl_select(vc, buffer+offs, MAXPICKBUF-offs, &rect);
		if(hits9>0) {
			for(a=0; a<hits9; a++) if(buffer[offs+4*a+3] & 0xFFFF0000) has_bones9= 1;
			
			offs+= 4*hits9;
			BLI_init_rcti(&rect, mval[0]-5, mval[0]+5, mval[1]-5, mval[1]+5);
			hits5= view3d_opengl_select(vc, buffer+offs, MAXPICKBUF-offs, &rect);
			if(hits5>0) {
				for(a=0; a<hits5; a++) if(buffer[offs+4*a+3] & 0xFFFF0000) has_bones5= 1;
			}
		}
		
		if(has_bones5) {
			offs= 4*hits15 + 4*hits9;
			memcpy(buffer, buffer+offs, 4*offs);
			return hits5;
		}
		if(has_bones9) {
			offs= 4*hits15;
			memcpy(buffer, buffer+offs, 4*offs);
			return hits9;
		}
		if(has_bones15) {
			return hits15;
		}
		
		if(hits5>0) {
			offs= 4*hits15 + 4*hits9;
			memcpy(buffer, buffer+offs, 4*offs);
			return hits5;
		}
		if(hits9>0) {
			offs= 4*hits15;
			memcpy(buffer, buffer+offs, 4*offs);
			return hits9;
		}
		return hits15;
	}
	
	return 0;
}


/* mval is region coords */
static void mouse_select(bContext *C, short *mval, short extend, short obcenter)
{
	ViewContext vc;
	ARegion *ar= CTX_wm_region(C);
	View3D *v3d= CTX_wm_view3d(C);
	Scene *scene= CTX_data_scene(C);
	Base *base, *startbase=NULL, *basact=NULL, *oldbasact=NULL;
	unsigned int buffer[4*MAXPICKBUF];
	int temp, a, dist=100;
	short hits;
	
	/* setup view context for argument to callbacks */
	view3d_set_viewcontext(C, &vc);
	
	/* always start list from basact in wire mode */
	startbase=  FIRSTBASE;
	if(BASACT && BASACT->next) startbase= BASACT->next;
	
	/* This block uses the control key to make the object selected by its center point rather then its contents */
	/* XXX later on, in editmode do not activate */
	if(vc.obedit==NULL && obcenter) {
		
		/* note; shift+alt goes to group-flush-selecting */
		/* XXX solve */
		if(0) 
			basact= mouse_select_menu(&vc, NULL, 0, mval);
		else {
			base= startbase;
			while(base) {
				if (BASE_SELECTABLE(v3d, base)) {
					project_short(ar, base->object->obmat[3], &base->sx);
					
					temp= abs(base->sx -mval[0]) + abs(base->sy -mval[1]);
					if(base==BASACT) temp+=10;
					if(temp<dist ) {
						
						dist= temp;
						basact= base;
					}
				}
				base= base->next;
				
				if(base==0) base= FIRSTBASE;
				if(base==startbase) break;
			}
		}
	}
	else {
		/* if objects have posemode set, the bones are in the same selection buffer */
		
		hits= mixed_bones_object_selectbuffer(&vc, buffer, mval);
		
		if(hits>0) {
			int has_bones= 0;
			
			for(a=0; a<hits; a++) if(buffer[4*a+3] & 0xFFFF0000) has_bones= 1;

			/* note; shift+alt goes to group-flush-selecting */
			if(has_bones==0 && 0) 
				basact= mouse_select_menu(&vc, buffer, hits, mval);
			else {
				static short lastmval[2]={-100, -100};
				int donearest= 0;
				
				/* define if we use solid nearest select or not */
				if(v3d->drawtype>OB_WIRE) {
					donearest= 1;
					if( ABS(mval[0]-lastmval[0])<3 && ABS(mval[1]-lastmval[1])<3) {
						if(!has_bones)	/* hrms, if theres bones we always do nearest */
							donearest= 0;
					}
				}
				lastmval[0]= mval[0]; lastmval[1]= mval[1];
				
				if(donearest) {
					unsigned int min= 0xFFFFFFFF;
					int selcol= 0, notcol=0;
					

					if(has_bones) {
						/* we skip non-bone hits */
						for(a=0; a<hits; a++) {
							if( min > buffer[4*a+1] && (buffer[4*a+3] & 0xFFFF0000) ) {
								min= buffer[4*a+1];
								selcol= buffer[4*a+3] & 0xFFFF;
							}
						}
					}
					else {
						/* only exclude active object when it is selected... */
						if(BASACT && (BASACT->flag & SELECT) && hits>1) notcol= BASACT->selcol;	
					
						for(a=0; a<hits; a++) {
							if( min > buffer[4*a+1] && notcol!=(buffer[4*a+3] & 0xFFFF)) {
								min= buffer[4*a+1];
								selcol= buffer[4*a+3] & 0xFFFF;
							}
						}
					}

					base= FIRSTBASE;
					while(base) {
						if(base->lay & v3d->lay) {
							if(base->selcol==selcol) break;
						}
						base= base->next;
					}
					if(base) basact= base;
				}
				else {
					
					base= startbase;
					while(base) {
						/* skip objects with select restriction, to prevent prematurely ending this loop
						 * with an un-selectable choice */
						if (base->object->restrictflag & OB_RESTRICT_SELECT) {
							base=base->next;
							if(base==NULL) base= FIRSTBASE;
							if(base==startbase) break;
						}
					
						if(base->lay & v3d->lay) {
							for(a=0; a<hits; a++) {
								if(has_bones) {
									/* skip non-bone objects */
									if((buffer[4*a+3] & 0xFFFF0000)) {
										if(base->selcol== (buffer[(4*a)+3] & 0xFFFF))
											basact= base;
									}
								}
								else {
									if(base->selcol== (buffer[(4*a)+3] & 0xFFFF))
										basact= base;
								}
							}
						}
						
						if(basact) break;
						
						base= base->next;
						if(base==NULL) base= FIRSTBASE;
						if(base==startbase) break;
					}
				}
			}
			
			if(has_bones && basact) {
				if(ED_do_pose_selectbuffer(scene, basact, buffer, hits, extend) ) {	/* then bone is found */
				
					/* we make the armature selected: 
					   not-selected active object in posemode won't work well for tools */
					basact->flag|= SELECT;
					basact->object->flag= basact->flag;
					
					WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, basact->object);
					WM_event_add_notifier(C, NC_OBJECT|ND_BONE_ACTIVE, basact->object);
					
					/* in weightpaint, we use selected bone to select vertexgroup, so no switch to new active object */
					if(basact->object->mode & OB_MODE_WEIGHT_PAINT) {
						/* prevent activating */
						basact= NULL;
					}

				}
				/* prevent bone selecting to pass on to object selecting */
				if(basact==BASACT)
					basact= NULL;
			}
		}
	}
	
	/* so, do we have something selected? */
	if(basact) {
		
		if(vc.obedit) {
			/* only do select */
			deselectall_except(scene, basact);
			ED_base_object_select(basact, BA_SELECT);
		}
		/* also prevent making it active on mouse selection */
		else if (BASE_SELECTABLE(v3d, basact)) {

			oldbasact= BASACT;
			
			if(!extend) {
				deselectall_except(scene, basact);
				ED_base_object_select(basact, BA_SELECT);
			}
			else if(0) {
				// XXX select_all_from_groups(basact);
			}
			else {
				if(basact->flag & SELECT) {
					if(basact==oldbasact)
						ED_base_object_select(basact, BA_DESELECT);
				}
				else ED_base_object_select(basact, BA_SELECT);
			}

			if(oldbasact != basact) {
				ED_base_object_activate(C, basact); /* adds notifier */
			}

			WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, scene);
		}
	}
}

/* ********************  border and circle ************************************** */


int edge_inside_circle(short centx, short centy, short rad, short x1, short y1, short x2, short y2)
{
	int radsq= rad*rad;
	float v1[2], v2[2], v3[2];
	
	/* check points in circle itself */
	if( (x1-centx)*(x1-centx) + (y1-centy)*(y1-centy) <= radsq ) return 1;
	if( (x2-centx)*(x2-centx) + (y2-centy)*(y2-centy) <= radsq ) return 1;
	
	/* pointdistline */
	v3[0]= centx;
	v3[1]= centy;
	v1[0]= x1;
	v1[1]= y1;
	v2[0]= x2;
	v2[1]= y2;
	
	if( PdistVL2Dfl(v3, v1, v2) < (float)rad ) return 1;
	
	return 0;
}

static void do_nurbs_box_select__doSelect(void *userData, Nurb *nu, BPoint *bp, BezTriple *bezt, int beztindex, int x, int y)
{
	struct { ViewContext vc; rcti *rect; int select; } *data = userData;

	if (BLI_in_rcti(data->rect, x, y)) {
		if (bp) {
			bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
		} else {
			Curve *cu= data->vc.obedit->data;
			
			if (cu->drawflag & CU_HIDE_HANDLES) {
				/* can only be beztindex==0 here since handles are hidden */
				bezt->f1 = bezt->f2 = bezt->f3 = data->select?(bezt->f2|SELECT):(bezt->f2&~SELECT);
			} else {
				if (beztindex==0) {
					bezt->f1 = data->select?(bezt->f1|SELECT):(bezt->f1&~SELECT);
				} else if (beztindex==1) {
					bezt->f2 = data->select?(bezt->f2|SELECT):(bezt->f2&~SELECT);
				} else {
					bezt->f3 = data->select?(bezt->f3|SELECT):(bezt->f3&~SELECT);
				}
			}
		}
	}
}
static void do_nurbs_box_select(ViewContext *vc, rcti *rect, int select)
{
	struct { ViewContext vc; rcti *rect; int select; } data;
	
	data.vc = *vc;
	data.rect = rect;
	data.select = select;

	nurbs_foreachScreenVert(vc, do_nurbs_box_select__doSelect, &data);
}

static void do_lattice_box_select__doSelect(void *userData, BPoint *bp, int x, int y)
{
	struct { ViewContext vc; rcti *rect; int select; } *data = userData;

	if (BLI_in_rcti(data->rect, x, y)) {
		bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
	}
}
static void do_lattice_box_select(ViewContext *vc, rcti *rect, int select)
{
	struct { ViewContext vc; rcti *rect; int select, pass, done; } data;

	data.vc= *vc;
	data.rect = rect;
	data.select = select;

	lattice_foreachScreenVert(vc, do_lattice_box_select__doSelect, &data);
}

static void do_mesh_box_select__doSelectVert(void *userData, EditVert *eve, int x, int y, int index)
{
	struct { ViewContext vc; rcti *rect; short select, pass, done; } *data = userData;

	if (BLI_in_rcti(data->rect, x, y)) {
		eve->f = data->select?(eve->f|1):(eve->f&~1);
	}
}
static void do_mesh_box_select__doSelectEdge(void *userData, EditEdge *eed, int x0, int y0, int x1, int y1, int index)
{
	struct { ViewContext vc; rcti *rect; short select, pass, done; } *data = userData;

	if(EM_check_backbuf(em_solidoffs+index)) {
		if (data->pass==0) {
			if (edge_fully_inside_rect(data->rect, x0, y0, x1, y1)) {
				EM_select_edge(eed, data->select);
				data->done = 1;
			}
		} else {
			if (edge_inside_rect(data->rect, x0, y0, x1, y1)) {
				EM_select_edge(eed, data->select);
			}
		}
	}
}
static void do_mesh_box_select__doSelectFace(void *userData, EditFace *efa, int x, int y, int index)
{
	struct { ViewContext vc; rcti *rect; short select, pass, done; } *data = userData;

	if (BLI_in_rcti(data->rect, x, y)) {
		EM_select_face_fgon(data->vc.em, efa, data->select);
	}
}
static void do_mesh_box_select(ViewContext *vc, rcti *rect, int select)
{
	struct { ViewContext vc; rcti *rect; short select, pass, done; } data;
	ToolSettings *ts= vc->scene->toolsettings;
	int bbsel;
	
	data.vc= *vc;
	data.rect = rect;
	data.select = select;
	data.pass = 0;
	data.done = 0;

	bbsel= EM_init_backbuf_border(vc, rect->xmin, rect->ymin, rect->xmax, rect->ymax);

	if(ts->selectmode & SCE_SELECT_VERTEX) {
		if (bbsel) {
			EM_backbuf_checkAndSelectVerts(vc->em, select);
		} else {
			mesh_foreachScreenVert(vc, do_mesh_box_select__doSelectVert, &data, 1);
		}
	}
	if(ts->selectmode & SCE_SELECT_EDGE) {
			/* Does both bbsel and non-bbsel versions (need screen cos for both) */

		data.pass = 0;
		mesh_foreachScreenEdge(vc, do_mesh_box_select__doSelectEdge, &data, 0);

		if (data.done==0) {
			data.pass = 1;
			mesh_foreachScreenEdge(vc, do_mesh_box_select__doSelectEdge, &data, 0);
		}
	}
	
	if(ts->selectmode & SCE_SELECT_FACE) {
		if(bbsel) {
			EM_backbuf_checkAndSelectFaces(vc->em, select);
		} else {
			mesh_foreachScreenFace(vc, do_mesh_box_select__doSelectFace, &data);
		}
	}
	
	EM_free_backbuf();
		
	EM_selectmode_flush(vc->em);
}

static int view3d_borderselect_exec(bContext *C, wmOperator *op)
{
	ViewContext vc;
	Scene *scene= CTX_data_scene(C);
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	Object *obedit= CTX_data_edit_object(C);
	Object *obact= CTX_data_active_object(C);
	rcti rect;
	Base *base;
	MetaElem *ml;
	unsigned int buffer[4*MAXPICKBUF];
	int a, index;
	short hits, val;

	view3d_operator_needs_opengl(C);
	
	/* setup view context for argument to callbacks */
	view3d_set_viewcontext(C, &vc);
	
	val= RNA_int_get(op->ptr, "event_type");
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");
	
	if(obedit==NULL && (paint_facesel_test(OBACT))) {
// XXX		face_borderselect();
		return OPERATOR_FINISHED;
	}
	else if(obedit==NULL && (obact && obact->mode & OB_MODE_PARTICLE_EDIT)) {
		return PE_border_select(C, &rect, (val==LEFTMOUSE));
	}
	
	if(obedit) {
		if(obedit->type==OB_MESH) {
			Mesh *me= obedit->data;
			vc.em= me->edit_mesh;
			do_mesh_box_select(&vc, &rect, (val==LEFTMOUSE));
//			if (EM_texFaceCheck())
			WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
			
		}
		else if(ELEM(obedit->type, OB_CURVE, OB_SURF)) {
			do_nurbs_box_select(&vc, &rect, val==LEFTMOUSE);
		}
		else if(obedit->type==OB_MBALL) {
			MetaBall *mb = (MetaBall*)obedit->data;
			hits= view3d_opengl_select(&vc, buffer, MAXPICKBUF, &rect);
			
			ml= mb->editelems->first;
			
			while(ml) {
				for(a=0; a<hits; a++) {
					if(ml->selcol1==buffer[ (4 * a) + 3 ]) {
						ml->flag |= MB_SCALE_RAD;
						if(val==LEFTMOUSE) ml->flag |= SELECT;
						else ml->flag &= ~SELECT;
						break;
					}
					if(ml->selcol2==buffer[ (4 * a) + 3 ]) {
						ml->flag &= ~MB_SCALE_RAD;
						if(val==LEFTMOUSE) ml->flag |= SELECT;
						else ml->flag &= ~SELECT;
						break;
					}
				}
				ml= ml->next;
			}
		}
		else if(obedit->type==OB_ARMATURE) {
			bArmature *arm= obedit->data;
			EditBone *ebone;
			
			/* clear flag we use to detect point was affected */
			for(ebone= arm->edbo->first; ebone; ebone= ebone->next)
				ebone->flag &= ~BONE_DONE;
			
			hits= view3d_opengl_select(&vc, buffer, MAXPICKBUF, &rect);
			
			/* first we only check points inside the border */
			for (a=0; a<hits; a++){
				index = buffer[(4*a)+3];
				if (index!=-1) {
					ebone = BLI_findlink(arm->edbo, index & ~(BONESEL_ANY));
					if (index & BONESEL_TIP) {
						ebone->flag |= BONE_DONE;
						if (val==LEFTMOUSE) ebone->flag |= BONE_TIPSEL;
						else ebone->flag &= ~BONE_TIPSEL;
					}
					
					if (index & BONESEL_ROOT) {
						ebone->flag |= BONE_DONE;
						if (val==LEFTMOUSE) ebone->flag |= BONE_ROOTSEL;
						else ebone->flag &= ~BONE_ROOTSEL;
					}
				}
			}
			
			/* now we have to flush tag from parents... */
			for(ebone= arm->edbo->first; ebone; ebone= ebone->next) {
				if(ebone->parent && (ebone->flag & BONE_CONNECTED)) {
					if(ebone->parent->flag & BONE_DONE)
						ebone->flag |= BONE_DONE;
				}
			}
			
			/* only select/deselect entire bones when no points where in the rect */
			for (a=0; a<hits; a++){
				index = buffer[(4*a)+3];
				if (index!=-1) {
					ebone = BLI_findlink(arm->edbo, index & ~(BONESEL_ANY));
					if (index & BONESEL_BONE) {
						if(!(ebone->flag & BONE_DONE)) {
							if (val==LEFTMOUSE)
								ebone->flag |= (BONE_ROOTSEL|BONE_TIPSEL|BONE_SELECTED);
							else
								ebone->flag &= ~(BONE_ROOTSEL|BONE_TIPSEL|BONE_SELECTED);
						}
					}
				}
			}
			
			ED_armature_sync_selection(arm->edbo);
		}
		else if(obedit->type==OB_LATTICE) {
			do_lattice_box_select(&vc, &rect, val==LEFTMOUSE);
		}
	}
	else {	/* no editmode, unified for bones and objects */
		Bone *bone;
		Object *ob= OBACT;
		unsigned int *vbuffer=NULL; /* selection buffer	*/
		unsigned int *col;			/* color in buffer	*/
		short selecting = 0;
		int bone_only;
		int totobj= MAXPICKBUF;	// XXX solve later
		
		if((ob) && (ob->mode & OB_MODE_POSE))
			bone_only= 1;
		else
			bone_only= 0;
		
		if (val==LEFTMOUSE)
			selecting = 1;
		
		/* selection buffer now has bones potentially too, so we add MAXPICKBUF */
		vbuffer = MEM_mallocN(4 * (totobj+MAXPICKBUF) * sizeof(unsigned int), "selection buffer");
		hits= view3d_opengl_select(&vc, vbuffer, 4*(totobj+MAXPICKBUF), &rect);
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
			base= FIRSTBASE;
			col = vbuffer + 3;
			
			while(base && hits) {
				Base *next = base->next;
				if(base->lay & v3d->lay) {
					while (base->selcol == (*col & 0xFFFF)) {	/* we got an object */
						
						if(*col & 0xFFFF0000) {					/* we got a bone */
							bone = get_indexed_bone(base->object, *col & ~(BONESEL_ANY));
							if(bone) {
								if(selecting) {
									bone->flag |= BONE_SELECTED;
// XXX									select_actionchannel_by_name(base->object->action, bone->name, 1);
								}
								else {
									bone->flag &= ~(BONE_ACTIVE|BONE_SELECTED);
// XXX									select_actionchannel_by_name(base->object->action, bone->name, 0);
								}
							}
						}
						else if(!bone_only) {
							if (selecting)
								ED_base_object_select(base, BA_SELECT);
							else
								ED_base_object_select(base, BA_DESELECT);
						}

						col+=4;	/* next color */
						hits--;
						if(hits==0) break;
					}
				}
				
				base= next;
			}

			WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, scene);

		}
		MEM_freeN(vbuffer);
	}
	return OPERATOR_FINISHED;
} 


/* *****************Selection Operators******************* */

/* ****** Border Select ****** */
void VIEW3D_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->description= "Select items using border selection.";
	ot->idname= "VIEW3D_OT_select_border";
	
	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= view3d_borderselect_exec;
	ot->modal= WM_border_select_modal;
	
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_UNDO;
	
	/* rna */
	RNA_def_int(ot->srna, "event_type", 0, INT_MIN, INT_MAX, "Event Type", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmin", 0, INT_MIN, INT_MAX, "X Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmax", 0, INT_MIN, INT_MAX, "X Max", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymin", 0, INT_MIN, INT_MAX, "Y Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymax", 0, INT_MIN, INT_MAX, "Y Max", "", INT_MIN, INT_MAX);

	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend selection instead of deselecting everyting first.");
}

/* ****** Mouse Select ****** */


static int view3d_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *obedit= CTX_data_edit_object(C);
	Object *obact= CTX_data_active_object(C);
	short extend= RNA_boolean_get(op->ptr, "extend");

	view3d_operator_needs_opengl(C);
	
	if(obedit) {
		if(obedit->type==OB_MESH)
			mouse_mesh(C, event->mval, extend);
		else if(obedit->type==OB_ARMATURE)
			mouse_armature(C, event->mval, extend);
		else if(obedit->type==OB_LATTICE)
			mouse_lattice(C, event->mval, extend);
		else if(ELEM(obedit->type, OB_CURVE, OB_SURF))
			mouse_nurb(C, event->mval, extend);
		else if(obedit->type==OB_MBALL)
			mouse_mball(C, event->mval, extend);
			
	}
	else if(obact && obact->mode & OB_MODE_PARTICLE_EDIT)
		PE_mouse_particles(C, event->mval, extend);
	else 
		mouse_select(C, event->mval, extend, 0);

	/* allowing tweaks */
	return OPERATOR_PASS_THROUGH|OPERATOR_FINISHED;
}

void VIEW3D_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Activate/Select";
	ot->description= "Activate/select item(s).";
	ot->idname= "VIEW3D_OT_select";
	
	/* api callbacks */
	ot->invoke= view3d_select_invoke;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend selection instead of deselecting everyting first.");
}


/* -------------------- circle select --------------------------------------------- */

static void mesh_circle_doSelectVert(void *userData, EditVert *eve, int x, int y, int index)
{
	struct {ViewContext *vc; short select, mval[2]; float radius; } *data = userData;
	int mx = x - data->mval[0], my = y - data->mval[1];
	float r = sqrt(mx*mx + my*my);

	if (r<=data->radius) {
		eve->f = data->select?(eve->f|1):(eve->f&~1);
	}
}
static void mesh_circle_doSelectEdge(void *userData, EditEdge *eed, int x0, int y0, int x1, int y1, int index)
{
	struct {ViewContext *vc; short select, mval[2]; float radius; } *data = userData;

	if (edge_inside_circle(data->mval[0], data->mval[1], (short) data->radius, x0, y0, x1, y1)) {
		EM_select_edge(eed, data->select);
	}
}
static void mesh_circle_doSelectFace(void *userData, EditFace *efa, int x, int y, int index)
{
	struct {ViewContext *vc; short select, mval[2]; float radius; } *data = userData;
	int mx = x - data->mval[0], my = y - data->mval[1];
	float r = sqrt(mx*mx + my*my);
	
	if (r<=data->radius) {
		EM_select_face_fgon(data->vc->em, efa, data->select);
	}
}

static void mesh_circle_select(ViewContext *vc, int selecting, short *mval, float rad)
{
	ToolSettings *ts= vc->scene->toolsettings;
	int bbsel;
	Object *ob= vc->obact;
	
	if(vc->obedit==NULL && paint_facesel_test(ob)) {
		Mesh *me = ob?ob->data:NULL;

		if (me) {
			em_vertoffs= me->totface+1;	/* max index array */

			bbsel= EM_init_backbuf_circle(vc, mval[0], mval[1], (short)(rad+1.0));
			EM_backbuf_checkAndSelectTFaces(me, selecting==LEFTMOUSE);
			EM_free_backbuf();

// XXX			object_tface_flags_changed(OBACT, 0);
		}
	}
	else {
		struct {ViewContext *vc; short select, mval[2]; float radius; } data;
		
		bbsel= EM_init_backbuf_circle(vc, mval[0], mval[1], (short)(rad+1.0));
		vc->em= ((Mesh *)vc->obedit->data)->edit_mesh;

		data.select = selecting;
		data.mval[0] = mval[0];
		data.mval[1] = mval[1];
		data.radius = rad;

		if(ts->selectmode & SCE_SELECT_VERTEX) {
			if(bbsel) {
				EM_backbuf_checkAndSelectVerts(vc->em, selecting==LEFTMOUSE);
			} else {
				mesh_foreachScreenVert(vc, mesh_circle_doSelectVert, &data, 1);
			}
		}

		if(ts->selectmode & SCE_SELECT_EDGE) {
			if (bbsel) {
				EM_backbuf_checkAndSelectEdges(vc->em, selecting==LEFTMOUSE);
			} else {
				mesh_foreachScreenEdge(vc, mesh_circle_doSelectEdge, &data, 0);
			}
		}
		
		if(ts->selectmode & SCE_SELECT_FACE) {
			if(bbsel) {
				EM_backbuf_checkAndSelectFaces(vc->em, selecting==LEFTMOUSE);
			} else {
				mesh_foreachScreenFace(vc, mesh_circle_doSelectFace, &data);
			}
		}

		EM_free_backbuf();
		EM_selectmode_flush(vc->em);
	}
}


static void nurbscurve_circle_doSelect(void *userData, Nurb *nu, BPoint *bp, BezTriple *bezt, int beztindex, int x, int y)
{
	struct {ViewContext *vc; short select, mval[2]; float radius; } *data = userData;
	int mx = x - data->mval[0], my = y - data->mval[1];
	float r = sqrt(mx*mx + my*my);

	if (r<=data->radius) {
		if (bp) {
			bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
		} else {
			if (beztindex==0) {
				bezt->f1 = data->select?(bezt->f1|SELECT):(bezt->f1&~SELECT);
			} else if (beztindex==1) {
				bezt->f2 = data->select?(bezt->f2|SELECT):(bezt->f2&~SELECT);
			} else {
				bezt->f3 = data->select?(bezt->f3|SELECT):(bezt->f3&~SELECT);
			}
		}
	}
}
static void nurbscurve_circle_select(ViewContext *vc, int selecting, short *mval, float rad)
{
	struct {ViewContext *vc; short select, mval[2]; float radius; } data;

	/* set vc-> edit data */
	
	data.select = selecting;
	data.mval[0] = mval[0];
	data.mval[1] = mval[1];
	data.radius = rad;

	nurbs_foreachScreenVert(vc, nurbscurve_circle_doSelect, &data);
}


static void latticecurve_circle_doSelect(void *userData, BPoint *bp, int x, int y)
{
	struct {ViewContext *vc; short select, mval[2]; float radius; } *data = userData;
	int mx = x - data->mval[0], my = y - data->mval[1];
	float r = sqrt(mx*mx + my*my);

	if (r<=data->radius) {
		bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
	}
}
static void lattice_circle_select(ViewContext *vc, int selecting, short *mval, float rad)
{
	struct {ViewContext *vc; short select, mval[2]; float radius; } data;

	/* set vc-> edit data */
	
	data.select = selecting;
	data.mval[0] = mval[0];
	data.mval[1] = mval[1];
	data.radius = rad;

	lattice_foreachScreenVert(vc, latticecurve_circle_doSelect, &data);
}

/** Callbacks for circle selection in Editmode */

static void obedit_circle_select(ViewContext *vc, short selecting, short *mval, float rad) 
{
	switch(vc->obedit->type) {		
	case OB_MESH:
		mesh_circle_select(vc, selecting, mval, rad);
		break;
	case OB_CURVE:
	case OB_SURF:
		nurbscurve_circle_select(vc, selecting, mval, rad);
		break;
	case OB_LATTICE:
		lattice_circle_select(vc, selecting, mval, rad);
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
	
	if(CTX_data_edit_object(C) || (obact && obact->mode & OB_MODE_PARTICLE_EDIT)) {
		ViewContext vc;
		short mval[2], selecting;
		
		view3d_operator_needs_opengl(C);
		
		view3d_set_viewcontext(C, &vc);
		mval[0]= x;
		mval[1]= y;
		selecting= LEFTMOUSE==RNA_int_get(op->ptr, "event_type"); // XXX solve

		if(CTX_data_edit_object(C)) {
			obedit_circle_select(&vc, selecting, mval, (float)radius);
			WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obact->data);
		}
		else
			return PE_circle_select(C, selecting, mval, (float)radius);
	}
	else {
		Base *base;
		
		for(base= FIRSTBASE; base; base= base->next) {
			if(base->lay & v3d->lay) {
				project_short(ar, base->object->obmat[3], &base->sx);
				if(base->sx!=IS_CLIPPED) {
					int dx= base->sx-x;
					int dy= base->sy-y;
					if( dx*dx + dy*dy < radius*radius)
						ED_base_object_select(base, BA_SELECT);
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
	ot->description= "Select items using circle selection.";
	ot->idname= "VIEW3D_OT_select_circle";
	
	ot->invoke= WM_gesture_circle_invoke;
	ot->modal= WM_gesture_circle_modal;
	ot->exec= view3d_circle_select_exec;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "radius", 0, INT_MIN, INT_MAX, "Radius", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "event_type", 0, INT_MIN, INT_MAX, "Event Type", "", INT_MIN, INT_MAX);
}
