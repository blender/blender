/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * cursor/gestures/selecteren
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"
#include "PIL_time.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_camera_types.h"
#include "DNA_group_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"
#include "DNA_ipo_types.h" /* for fly mode recording */
#include "DNA_node_types.h" /* for fly mode recording */

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_rand.h" /* random object selection */

#include "BKE_armature.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_object.h" /* fly mode where_is_object to get camera location */
#include "BKE_particle.h"
#include "BKE_utildefines.h"
#include "BKE_customdata.h"

#include "BIF_drawimage.h"
#include "BIF_butspace.h"
#include "BIF_editaction.h"
#include "BIF_editarmature.h"
#include "BIF_editparticle.h"
#include "BIF_editgroup.h"
#include "BIF_editmesh.h"
#include "BIF_editoops.h"
#include "BIF_editsima.h"
#include "BIF_editview.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_previewrender.h" /* use only so fly mode can preview when its done */
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"

#include "BDR_editobject.h"	/* For headerprint */
#include "BDR_sculptmode.h"
#include "BDR_vpaint.h"
#include "BDR_editface.h"
#include "BDR_drawobject.h"
#include "BDR_editcurve.h"

#include "BSE_edit.h"
#include "BSE_view.h"		/* give_cursor() */
#include "BSE_editipo.h"
#include "BSE_drawipo.h"
#include "BSE_drawview.h"

#include "editmesh.h"	/* borderselect uses it... */
#include "blendef.h"
#include "mydevice.h"

#include "BIF_transform.h"
#include "BIF_toets.h"                      /* persptoetsen                 */

extern ListBase editNurb; /* originally from exports.h, memory from editcurve.c*/
/* editmball.c */
extern ListBase editelems;

/* fly mode uses this */
extern void setcameratoview3d(void);

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
				EM_select_face_fgon(efa, select);
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
}

/* simple API for object selection, rather than just using the flag
 * this takes into account the 'restrict selection in 3d view' flag.
 * deselect works always, the restriction just prevents selection */
void select_base_v3d(Base *base, short mode)
{
	if (base) {
		if (mode==BA_SELECT) {
			if (!(base->object->restrictflag & OB_RESTRICT_SELECT))
				if (mode==BA_SELECT) base->flag |= SELECT;
		}
		else if (mode==BA_DESELECT) {
			base->flag &= ~SELECT;
		}
	}
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
static void do_lasso_select_pose(Object *ob, short mcords[][2], short moves, short select)
{
	bPoseChannel *pchan;
	float vec[3];
	short sco1[2], sco2[2];
	
	if(ob->type!=OB_ARMATURE || ob->pose==NULL) return;
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		VECCOPY(vec, pchan->pose_head);
		Mat4MulVecfl(ob->obmat, vec);
		project_short(vec, sco1);
		VECCOPY(vec, pchan->pose_tail);
		Mat4MulVecfl(ob->obmat, vec);
		project_short(vec, sco2);
		
		if(lasso_inside_edge(mcords, moves, sco1[0], sco1[1], sco2[0], sco2[1])) {
			if(select) pchan->bone->flag |= BONE_SELECTED;
			else pchan->bone->flag &= ~(BONE_ACTIVE|BONE_SELECTED);
		}
	}
}


static void do_lasso_select_objects(short mcords[][2], short moves, short select)
{
	Base *base;
	
	for(base= G.scene->base.first; base; base= base->next) {
		if(base->lay & G.vd->lay) {
			project_short(base->object->obmat[3], &base->sx);
			if(lasso_inside(mcords, moves, base->sx, base->sy)) {
				
				if(select) select_base_v3d(base, BA_SELECT);
				else select_base_v3d(base, BA_DESELECT);
				base->object->flag= base->flag;
			}
			if(base->object->flag & OB_POSEMODE) {
				do_lasso_select_pose(base->object, mcords, moves, select);
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
	struct { rcti *rect; short (*mcords)[2], moves, select, pass, done; } *data = userData;

	if (BLI_in_rcti(data->rect, x, y) && lasso_inside(data->mcords, data->moves, x, y)) {
		eve->f = data->select?(eve->f|1):(eve->f&~1);
	}
}
static void do_lasso_select_mesh__doSelectEdge(void *userData, EditEdge *eed, int x0, int y0, int x1, int y1, int index)
{
	struct { rcti *rect; short (*mcords)[2], moves, select, pass, done; } *data = userData;

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
	struct { rcti *rect; short (*mcords)[2], moves, select, pass, done; } *data = userData;

	if (BLI_in_rcti(data->rect, x, y) && lasso_inside(data->mcords, data->moves, x, y)) {
		EM_select_face_fgon(efa, data->select);
	}
}

static void do_lasso_select_mesh(short mcords[][2], short moves, short select)
{
	struct { rcti *rect; short (*mcords)[2], moves, select, pass, done; } data;
	EditMesh *em = G.editMesh;
	rcti rect;
	int bbsel;
	
	lasso_select_boundbox(&rect, mcords, moves);
	
	data.rect = &rect;
	data.mcords = mcords;
	data.moves = moves;
	data.select = select;
	data.done = 0;
	data.pass = 0;

	bbsel= EM_mask_init_backbuf_border(mcords, moves, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
	
	if(G.scene->selectmode & SCE_SELECT_VERTEX) {
		if (bbsel) {
			EM_backbuf_checkAndSelectVerts(em, select);
		} else {
			mesh_foreachScreenVert(do_lasso_select_mesh__doSelectVert, &data, 1);
		}
	}
	if(G.scene->selectmode & SCE_SELECT_EDGE) {
			/* Does both bbsel and non-bbsel versions (need screen cos for both) */

		data.pass = 0;
		mesh_foreachScreenEdge(do_lasso_select_mesh__doSelectEdge, &data, 0);

		if (data.done==0) {
			data.pass = 1;
			mesh_foreachScreenEdge(do_lasso_select_mesh__doSelectEdge, &data, 0);
		}
	}
	
	if(G.scene->selectmode & SCE_SELECT_FACE) {
		if (bbsel) {
			EM_backbuf_checkAndSelectFaces(em, select);
		} else {
			mesh_foreachScreenFace(do_lasso_select_mesh__doSelectFace, &data);
		}
	}
	
	EM_free_backbuf();
	EM_selectmode_flush();	
}

/* this is an exception in that its the only lasso that dosnt use the 3d view (uses space image view) */
static void do_lasso_select_mesh_uv(short mcords[][2], short moves, short select)
{
	EditMesh *em = G.editMesh;
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
		if (select) EM_select_flush();
		else		EM_deselect_flush();
	}
}

static void do_lasso_select_curve__doSelect(void *userData, Nurb *nu, BPoint *bp, BezTriple *bezt, int beztindex, int x, int y)
{
	struct { short (*mcords)[2]; short moves; short select; } *data = userData;

	if (lasso_inside(data->mcords, data->moves, x, y)) {
		if (bp) {
			bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
		} else {
			if (G.f & G_HIDDENHANDLES) {
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
static void do_lasso_select_curve(short mcords[][2], short moves, short select)
{
	struct { short (*mcords)[2]; short moves; short select; } data;

	data.mcords = mcords;
	data.moves = moves;
	data.select = select;

	nurbs_foreachScreenVert(do_lasso_select_curve__doSelect, &data);
}

static void do_lasso_select_lattice__doSelect(void *userData, BPoint *bp, int x, int y)
{
	struct { short (*mcords)[2]; short moves; short select; } *data = userData;

	if (lasso_inside(data->mcords, data->moves, x, y)) {
		bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
	}
}
static void do_lasso_select_lattice(short mcords[][2], short moves, short select)
{
	struct { short (*mcords)[2]; short moves; short select; } data;

	data.mcords = mcords;
	data.moves = moves;
	data.select = select;

	lattice_foreachScreenVert(do_lasso_select_lattice__doSelect, &data);
}

static void do_lasso_select_armature(short mcords[][2], short moves, short select)
{
	EditBone *ebone;
	float vec[3];
	short sco1[2], sco2[2], didpoint;
	
	for (ebone=G.edbo.first; ebone; ebone=ebone->next) {

		VECCOPY(vec, ebone->head);
		Mat4MulVecfl(G.obedit->obmat, vec);
		project_short(vec, sco1);
		VECCOPY(vec, ebone->tail);
		Mat4MulVecfl(G.obedit->obmat, vec);
		project_short(vec, sco2);
		
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
	countall();	/* abused for flushing selection */
}

static void do_lasso_select_facemode(short mcords[][2], short moves, short select)
{
	Mesh *me;
	rcti rect;
	
	me= get_mesh(OBACT);
	if(me==NULL || me->mtface==NULL) return;
	if(me->totface==0) return;
	
	em_vertoffs= me->totface+1;	/* max index array */
	
	lasso_select_boundbox(&rect, mcords, moves);
	EM_mask_init_backbuf_border(mcords, moves, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
	
	EM_backbuf_checkAndSelectTFaces(me, select);
	
	EM_free_backbuf();
	
	object_tface_flags_changed(OBACT, 0);
}

static void do_lasso_select_node(short mcords[][2], short moves, short select)
{
	SpaceNode *snode = curarea->spacedata.first;
	
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
	allqueue(REDRAWNODE, 1);
	BIF_undo_push("Lasso select nodes");
}

static void do_lasso_select(short mcords[][2], short moves, short select)
{
	if(curarea->spacetype==SPACE_NODE) {
		do_lasso_select_node(mcords, moves, select);
	} else {
		if(G.obedit==NULL) {
			if(FACESEL_PAINT_TEST)
				do_lasso_select_facemode(mcords, moves, select);
			else if(G.f & (G_VERTEXPAINT|G_TEXTUREPAINT|G_WEIGHTPAINT))
				;
			else if(G.f & G_PARTICLEEDIT)
				PE_do_lasso_select(mcords, moves, select);
			else  
				do_lasso_select_objects(mcords, moves, select);
		}
		else if(G.obedit->type==OB_MESH) {
			if(curarea->spacetype==SPACE_VIEW3D) {
				do_lasso_select_mesh(mcords, moves, select);
			} else if (EM_texFaceCheck()){
				do_lasso_select_mesh_uv(mcords, moves, select);
			}
		} else if(G.obedit->type==OB_CURVE || G.obedit->type==OB_SURF) 
			do_lasso_select_curve(mcords, moves, select);
		else if(G.obedit->type==OB_LATTICE) 
			do_lasso_select_lattice(mcords, moves, select);
		else if(G.obedit->type==OB_ARMATURE)
			do_lasso_select_armature(mcords, moves, select);
	}
	BIF_undo_push("Lasso select");
	
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
	
	allqueue(REDRAWVIEW3D, 0);
	countall();
}

/* un-draws and draws again */
static void draw_lasso_select(short mcords[][2], short moves, short end)
{
	int a;
	
	setlinestyle(2);
	/* clear draw */
	if(moves>1) {
		for(a=1; a<=moves-1; a++) {
			sdrawXORline(mcords[a-1][0], mcords[a-1][1], mcords[a][0], mcords[a][1]);
		}
		sdrawXORline(mcords[moves-1][0], mcords[moves-1][1], mcords[0][0], mcords[0][1]);
	}
	if(!end) {
		/* new draw */
		for(a=1; a<=moves; a++) {
			sdrawXORline(mcords[a-1][0], mcords[a-1][1], mcords[a][0], mcords[a][1]);
		}
		sdrawXORline(mcords[moves][0], mcords[moves][1], mcords[0][0], mcords[0][1]);
	}
	setlinestyle(0);
}


static char interpret_move(short mcord[][2], int count)
{
	float x1, x2, y1, y2, d1, d2, inp, sq, mouse[MOVES_GESTURE][2];
	int i, j, dir = 0;
	
	if (count <= 10) return ('g');

	/* from short to float (drawing is with shorts) */
	for(j=0; j<count; j++) {
		mouse[j][0]= mcord[j][0];
		mouse[j][1]= mcord[j][1];
	}
	
	/* new method:
	 * 
	 * starting from end points, calculate center with maximum distance
	 * dependant at the angle s / g / r is defined
	 */
	

	/* filter */
	
	for( j = 3 ; j > 0; j--){
		x1 = mouse[1][0];
		y1 = mouse[1][1];
		for (i = 2; i < count; i++){
			x2 = mouse[i-1][0];
			y2 = mouse[i-1][1];
			mouse[i-1][0] = ((x1 + mouse[i][0]) /4.0) + (x2 / 2.0);
			mouse[i-1][1] = ((y1 + mouse[i][1]) /4.0) + (y2 / 2.0);
			x1 = x2;
			y1 = y2;
		}
	}

	/* make overview of directions */
	for (i = 0; i <= count - 2; i++){
		x1 = mouse[i][0] - mouse[i + 1][0];
		y1 = mouse[i][1] - mouse[i + 1][1];

		if (x1 < -0.5){
			if (y1 < -0.5) dir |= 32;
			else if (y1 > 0.5) dir |= 128;
			else dir |= 64;
		} else if (x1 > 0.5){
			if (y1 < -0.5) dir |= 8;
			else if (y1 > 0.5) dir |= 2;
			else dir |= 4;
		} else{
			if (y1 < -0.5) dir |= 16;
			else if (y1 > 0.5) dir |= 1;
			else dir |= 0;
		}
	}
	
	/* move all crosses to the right */
	for (i = 7; i>=0 ; i--){
		if (dir & 128) dir = (dir << 1) + 1;
		else break;
	}
	dir &= 255;
	for (i = 7; i>=0 ; i--){
		if ((dir & 1) == 0) dir >>= 1;
		else break;
	}
	
	/* in theory: 1 direction: straight line
     * multiple sequential directions: circle
     * non-sequential, and 1 bit set in upper 4 bits: size
     */
	switch(dir){
	case 1:
		return ('g');
		break;
	case 3:
	case 7:
		x1 = mouse[0][0] - mouse[count >> 1][0];
		y1 = mouse[0][1] - mouse[count >> 1][1];
		x2 = mouse[count >> 1][0] - mouse[count - 1][0];
		y2 = mouse[count >> 1][1] - mouse[count - 1][1];
		d1 = (x1 * x1) + (y1 * y1);
		d2 = (x2 * x2) + (y2 * y2);
		sq = sqrt(d1);
		x1 /= sq; 
		y1 /= sq;
		sq = sqrt(d2);
		x2 /= sq; 
		y2 /= sq;
		inp = (x1 * x2) + (y1 * y2);
		/*printf("%f\n", inp);*/
		if (inp > 0.9) return ('g');
		else return ('r');
		break;
	case 15:
	case 31:
	case 63:
	case 127:
	case 255:
		return ('r');
		break;
	default:
		/* for size at least one of the higher bits has to be set */
		if (dir < 16) return ('r');
		else return ('s');
	}

	return (0);
}


/* return 1 to denote gesture did something, also does lasso */
int gesture(void)
{
	unsigned short event=0;
	int i= 1, end= 0, a;
	short mcords[MOVES_LASSO][2]; /* the larger size */
	short mval[2], val, timer=0, mousebut, lasso=0, maxmoves;
	
	if (U.flag & USER_LMOUSESELECT) mousebut = R_MOUSE;
	else mousebut = L_MOUSE;
	
	/* check for lasso */
	if(G.qual & LR_CTRLKEY) {
		if(curarea->spacetype==SPACE_VIEW3D) {
			if(G.obedit==NULL) {
				if(G.f & (G_VERTEXPAINT|G_TEXTUREPAINT|G_WEIGHTPAINT)) return 0;
			}
			lasso= 1;
		} else if (curarea->spacetype==SPACE_IMAGE) {
			if(G.obedit) {
				lasso= 1;
			}
		} else if (curarea->spacetype==SPACE_NODE) {
			lasso= 1;
		}
	}
	
	glDrawBuffer(GL_FRONT);
	persp(PERSP_WIN);	/*  ortho at pixel level */
	
	getmouseco_areawin(mval);
	
	mcords[0][0] = mval[0];
	mcords[0][1] = mval[1];
	
	if(lasso) maxmoves= MOVES_LASSO;
	else maxmoves= MOVES_GESTURE;
	
	while(get_mbut() & mousebut) {
		
		if(qtest()) event= extern_qread(&val);
		else if(i==1) {
			/* not drawing yet... check for toolbox */
			PIL_sleep_ms(10);
			timer++;
			if(timer>=10*U.tb_leftmouse) {
				glDrawBuffer(GL_BACK); /* !! */
				toolbox_n();
				return 1;
			}
		}
		
		switch (event) {
		case MOUSEY:
			getmouseco_areawin(mval);
			if( abs(mval[0]-mcords[i-1][0])>3 || abs(mval[1]-mcords[i-1][1])>3 ) {
				mcords[i][0] = mval[0];
				mcords[i][1] = mval[1];
				
				if(i) {
					if(lasso) draw_lasso_select(mcords, i, 0);
					else sdrawXORline(mcords[i-1][0], mcords[i-1][1], mcords[i][0], mcords[i][1]);
					bglFlush();
				}
				i++;
			}
			break;
		case MOUSEX:
			break;
		case LEFTMOUSE:
			break;
		default:
			if(event) end= 1;	/* blender returns 0 */
			break;
		}
		if (i == maxmoves || end == 1) break;
	}
	
	/* clear */
	if(lasso) draw_lasso_select(mcords, i, 1);
	else for(a=1; a<i; a++) {
		sdrawXORline(mcords[a-1][0], mcords[a-1][1], mcords[a][0], mcords[a][1]);
	}
	
	persp(PERSP_VIEW);
	glDrawBuffer(GL_BACK);
	
	if (i > 2) {
		if(lasso) do_lasso_select(mcords, i, (G.qual & LR_SHIFTKEY)==0);
		else {
			i = interpret_move(mcords, i);
			
			if(i) {
				if(curarea->spacetype==SPACE_IPO) transform_ipo(i);
				else if(curarea->spacetype==SPACE_OOPS) transform_oops('g', 0);
				else {
					int context;

					if(curarea->spacetype==SPACE_IMAGE) context= CTX_NONE;
					else context= CTX_NONE;

					if(i=='g') {
						initTransform(TFM_TRANSLATION, context);
						Transform();
					}
					else if(i=='r') {
						initTransform(TFM_ROTATION, context);
						Transform();
					}
					else {
						initTransform(TFM_RESIZE, context);
						Transform();
					}
				}
			}
		}
		return 1;
	}
	return 0;
}

void mouse_cursor(void)
{
	float dx, dy, fz, *fp = NULL, dvec[3], oldcurs[3];
	short mval[2], mx, my, lr_click=0;
	
	if(gesture()) return;
	
	getmouseco_areawin(mval);

	mx= mval[0];
	my= mval[1];
	
	fp= give_cursor();
	
	if(G.obedit && ((G.qual & LR_CTRLKEY) || get_mbut()&R_MOUSE )) lr_click= 1;
	VECCOPY(oldcurs, fp);
	
	project_short_noclip(fp, mval);

	initgrabz(fp[0], fp[1], fp[2]);
	
	if(mval[0]!=IS_CLIPPED) {
		
		window_to_3d(dvec, mval[0]-mx, mval[1]-my);
		VecSubf(fp, fp, dvec);
		
	}
	else {

		dx= ((float)(mx-(curarea->winx/2)))*G.vd->zfac/(curarea->winx/2);
		dy= ((float)(my-(curarea->winy/2)))*G.vd->zfac/(curarea->winy/2);
		
		fz= G.vd->persmat[0][3]*fp[0]+ G.vd->persmat[1][3]*fp[1]+ G.vd->persmat[2][3]*fp[2]+ G.vd->persmat[3][3];
		fz= fz/G.vd->zfac;
		
		fp[0]= (G.vd->persinv[0][0]*dx + G.vd->persinv[1][0]*dy+ G.vd->persinv[2][0]*fz)-G.vd->ofs[0];
		fp[1]= (G.vd->persinv[0][1]*dx + G.vd->persinv[1][1]*dy+ G.vd->persinv[2][1]*fz)-G.vd->ofs[1];
		fp[2]= (G.vd->persinv[0][2]*dx + G.vd->persinv[1][2]*dy+ G.vd->persinv[2][2]*fz)-G.vd->ofs[2];
	}
	
	allqueue(REDRAWVIEW3D, 1);
	
	if(lr_click) {
		if(G.obedit->type==OB_MESH) add_click_mesh();
		else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) addvert_Nurb(0);
		else if (G.obedit->type==OB_ARMATURE) addvert_armature();
		VECCOPY(fp, oldcurs);
	}
	
}

void deselectall(void)	/* is toggle */
{
	Base *base;
	int a=0, ok=0; 

	base= FIRSTBASE;
	while(base) {
		/* is there a visible selected object */
		if(base->lay & G.vd->lay &&
		  (base->object->restrictflag & OB_RESTRICT_VIEW)==0 &&
		  (base->object->restrictflag & OB_RESTRICT_SELECT)==0
		) {
			if (base->flag & SELECT) {
				ok= a= 1;
				break;
			} else {
				ok=1;
			}
		}
		base= base->next;
	}
	
	if (!ok) return;
	
	base= FIRSTBASE;
	while(base) {
		if(base->lay & G.vd->lay &&
		  (base->object->restrictflag & OB_RESTRICT_VIEW)==0 &&
		  (base->object->restrictflag & OB_RESTRICT_SELECT)==0
		) {
			if(a) 
				select_base_v3d(base, BA_DESELECT);
			else 
				select_base_v3d(base, BA_SELECT);
			base->object->flag= base->flag;
		}
		base= base->next;
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWDATASELECT, 0);
	allqueue(REDRAWNLA, 0);
	
	countall();
	BIF_undo_push("(De)select all");
}

/* inverts object selection */
void selectswap(void)
{
	Base *base;

	for(base= FIRSTBASE; base; base= base->next) {
		if(base->lay & G.vd->lay &&
		  (base->object->restrictflag & OB_RESTRICT_VIEW)==0
		) {
			if TESTBASE(base)
				select_base_v3d(base, BA_DESELECT);
			else
				select_base_v3d(base, BA_SELECT);
			base->object->flag= base->flag;
		}
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWDATASELECT, 0);
	allqueue(REDRAWNLA, 0);
	
	countall();
	BIF_undo_push("Select Inverse");
}

/* inverts object selection */
void selectrandom(void)
{
	Base *base;
	static short randfac = 50;
	if(button(&randfac,0, 100,"Percentage:")==0) return;
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(base->lay & G.vd->lay &&
		  (base->object->restrictflag & OB_RESTRICT_VIEW)==0
		) {
			if (!TESTBASE(base) && ( (BLI_frand() * 100) < randfac)) {
				select_base_v3d(base, BA_SELECT);
				base->object->flag= base->flag;
			}
		}
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWDATASELECT, 0);
	allqueue(REDRAWNLA, 0);
	
	countall();
	BIF_undo_push("Select Random");
}

/* selects all objects of a particular type, on currently visible layers */
void selectall_type(short obtype) 
{
	Base *base;
	
	base= FIRSTBASE;
	while(base) {
		if((base->lay & G.vd->lay) &&
		  (base->object->type == obtype) &&
		  (base->object->restrictflag & OB_RESTRICT_VIEW)==0
		) {
			select_base_v3d(base, BA_SELECT);
			base->object->flag= base->flag;
		}
		base= base->next;
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWDATASELECT, 0);
	allqueue(REDRAWNLA, 0);
	
	countall();
	BIF_undo_push("Select all per type");
}
/* selects all objects on a particular layer */
void selectall_layer(unsigned int layernum) 
{
	Base *base;
	
	base= FIRSTBASE;
	while(base) {
		if(base->lay == (1<< (layernum -1)) &&
		  (base->object->restrictflag & OB_RESTRICT_VIEW)==0
		) {
			select_base_v3d(base, BA_SELECT);
			base->object->flag= base->flag;
		}
		base= base->next;
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWDATASELECT, 0);
	allqueue(REDRAWNLA, 0);
	
	countall();
	BIF_undo_push("Select all per layer");
}

static void deselectall_except(Base *b)   /* deselect all except b */
{
	Base *base;

	base= FIRSTBASE;
	while(base) {
		if (base->flag & SELECT) {
			if(b!=base) {
				select_base_v3d(base, BA_DESELECT);
				base->object->flag= base->flag;
			}
		}
		base= base->next;
	}
}

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

void set_active_base(Base *base)
{
	Base *tbase;
	
	/* activating a non-mesh, should end a couple of modes... */
	if(base && base->object->type!=OB_MESH)
		exit_paint_modes();
	
	/* sets scene->basact */
	BASACT= base;
	
	if(base) {
		
		/* signals to buttons */
		redraw_test_buttons(base->object);
		
		/* signal to ipo */
		allqueue(REDRAWIPO, base->object->ipowin);
		
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWNODE, 0);
		
		/* signal to action */
		select_actionchannel_by_name(base->object->action, "Object", 1);
		
		/* disable temporal locks */
		for(tbase=FIRSTBASE; tbase; tbase= tbase->next) {
			if(base!=tbase && (tbase->object->shapeflag & OB_SHAPE_TEMPLOCK)) {
				tbase->object->shapeflag &= ~OB_SHAPE_TEMPLOCK;
				DAG_object_flush_update(G.scene, tbase->object, OB_RECALC_DATA);
			}
		}
	}
}

void set_active_object(Object *ob)
{
	Base *base;
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(base->object==ob) {
			set_active_base(base);
			return;
		}
	}
}

static void select_all_from_groups(Base *basact)
{
	Group *group;
	GroupObject *go;
	int deselect= basact->flag & SELECT;
	
	for(group= G.main->group.first; group; group= group->id.next) {
		if(object_in_group(basact->object, group)) {
			for(go= group->gobject.first; go; go= go->next) {
				if(deselect) go->ob->flag &= ~SELECT;
				else {
					if ((go->ob->restrictflag & OB_RESTRICT_SELECT)==0 &&
						(go->ob->restrictflag & OB_RESTRICT_VIEW)==0)
						go->ob->flag |= SELECT;
				}
			}
		}
	}
	/* sync bases */
	for(basact= G.scene->base.first; basact; basact= basact->next) {
		if(basact->object->flag & SELECT)
			select_base_v3d(basact, BA_SELECT);
		else
			select_base_v3d(basact, BA_DESELECT);
	}
}

/* The max number of menu items in an object select menu */
#define SEL_MENU_SIZE 22

static Base *mouse_select_menu(unsigned int *buffer, int hits, short *mval)
{
	Base *baseList[SEL_MENU_SIZE]={NULL}; /*baseList is used to store all possible bases to bring up a menu */
	Base *base;
	short baseCount = 0;
	char menuText[20 + SEL_MENU_SIZE*32] = "Select Object%t";	/* max ob name = 22 */
	char str[32];
	
	for(base=FIRSTBASE; base; base= base->next) {
		if BASE_SELECTABLE(base) {
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
				
				project_short(base->object->obmat[3], &base->sx);
				
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
		baseCount = pupmenu(menuText);
		
		if (baseCount != -1) { /* If nothing is selected then dont do anything */
			return baseList[baseCount-1];
		}
		else return NULL;
	}
}

/* we want a select buffer with bones, if there are... */
/* so check three selection levels and compare */
static short mixed_bones_object_selectbuffer(unsigned int *buffer, short *mval)
{
	int offs;
	short a, hits15, hits9=0, hits5=0;
	short has_bones15=0, has_bones9=0, has_bones5=0;
	
	hits15= view3d_opengl_select(buffer, MAXPICKBUF, mval[0]-14, mval[1]-14, mval[0]+14, mval[1]+14);
	if(hits15>0) {
		for(a=0; a<hits15; a++) if(buffer[4*a+3] & 0xFFFF0000) has_bones15= 1;
		
		offs= 4*hits15;
		hits9= view3d_opengl_select(buffer+offs, MAXPICKBUF-offs, mval[0]-9, mval[1]-9, mval[0]+9, mval[1]+9);
		if(hits9>0) {
			for(a=0; a<hits9; a++) if(buffer[offs+4*a+3] & 0xFFFF0000) has_bones9= 1;
			
			offs+= 4*hits9;
			hits5= view3d_opengl_select(buffer+offs, MAXPICKBUF-offs, mval[0]-5, mval[1]-5, mval[0]+5, mval[1]+5);
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

void mouse_select(void)
{
	Base *base, *startbase=NULL, *basact=NULL, *oldbasact=NULL;
	unsigned int buffer[4*MAXPICKBUF];
	int temp, a, dist=100;
	short hits, mval[2];

	/* always start list from basact in wire mode */
	startbase=  FIRSTBASE;
	if(BASACT && BASACT->next) startbase= BASACT->next;

	getmouseco_areawin(mval);
	
	/* This block uses the control key to make the object selected by its center point rather then its contents */
	if(G.obedit==0 && (G.qual & LR_CTRLKEY)) {
		
		/* note; shift+alt goes to group-flush-selecting */
		if(G.qual == (LR_ALTKEY|LR_CTRLKEY)) 
			basact= mouse_select_menu(NULL, 0, mval);
		else {
			base= startbase;
			while(base) {
				if BASE_SELECTABLE(base) {
					project_short(base->object->obmat[3], &base->sx);
					
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
		
		hits= mixed_bones_object_selectbuffer(buffer, mval);
		
		if(hits>0) {
			int has_bones= 0;
			
			for(a=0; a<hits; a++) if(buffer[4*a+3] & 0xFFFF0000) has_bones= 1;

			/* note; shift+alt goes to group-flush-selecting */
			if(has_bones==0 && (G.qual == LR_ALTKEY)) 
				basact= mouse_select_menu(buffer, hits, mval);
			else {
				static short lastmval[2]={-100, -100};
				int donearest= 0;
				
				/* define if we use solid nearest select or not */
				if(G.vd->drawtype>OB_WIRE) {
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
						if(base->lay & G.vd->lay) {
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
					
						if(base->lay & G.vd->lay) {
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
				if( do_pose_selectbuffer(basact, buffer, hits) ) {	/* then bone is found */
				
					/* we make the armature selected: 
					   not-selected active object in posemode won't work well for tools */
					basact->flag|= SELECT;
					basact->object->flag= basact->flag;
					
					/* in weightpaint, we use selected bone to select vertexgroup, so no switch to new active object */
					if(G.f & G_WEIGHTPAINT) {
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
		
		if(G.obedit) {
			/* only do select */
			deselectall_except(basact);
			select_base_v3d(basact, BA_SELECT);
		}
		/* also prevent making it active on mouse selection */
		else if BASE_SELECTABLE(basact) {

			oldbasact= BASACT;
			BASACT= basact;
			
			if((G.qual & LR_SHIFTKEY)==0) {
				deselectall_except(basact);
				select_base_v3d(basact, BA_SELECT);
			}
			else if(G.qual==(LR_SHIFTKEY|LR_ALTKEY)) {
				select_all_from_groups(basact);
			}
			else {
				if(basact->flag & SELECT) {
					if(basact==oldbasact)
						select_base_v3d(basact, BA_DESELECT);
				}
				else select_base_v3d(basact, BA_SELECT);
			}

			/* copy */
			basact->object->flag= basact->flag;
			
			if(oldbasact != basact) {
				set_active_base(basact);
			}

			/* for visual speed, only in wire mode */
			if(G.vd->drawtype==OB_WIRE) {
				/* however, not for posemodes */
				if(basact->object->flag & OB_POSEMODE);
				else if(oldbasact && (oldbasact->object->flag & OB_POSEMODE));
				else {
					if(oldbasact && oldbasact != basact && (oldbasact->lay & G.vd->lay)) 
						draw_object_ext(oldbasact);
					draw_object_ext(basact);
				}
			}
			
			allqueue(REDRAWBUTSLOGIC, 0);
			allqueue(REDRAWDATASELECT, 0);
			allqueue(REDRAWBUTSOBJECT, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			allqueue(REDRAWTIME, 0);
			allqueue(REDRAWHEADERS, 0);	/* To force display update for the posebutton */
		}
		/* also because multiple 3d windows can be open */
		allqueue(REDRAWVIEW3D, 0);
		
	}

	countall();

	rightmouse_transform();	/* does undo push! */
}

/* ------------------------------------------------------------------------- */

static int edge_inside_circle(short centx, short centy, short rad, short x1, short y1, short x2, short y2)
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
	struct { rcti *rect; int select; } *data = userData;

	if (BLI_in_rcti(data->rect, x, y)) {
		if (bp) {
			bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
		} else {
			if (G.f & G_HIDDENHANDLES) {
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
static void do_nurbs_box_select(rcti *rect, int select)
{
	struct { rcti *rect; int select; } data;

	data.rect = rect;
	data.select = select;

	nurbs_foreachScreenVert(do_nurbs_box_select__doSelect, &data);
}

static void do_lattice_box_select__doSelect(void *userData, BPoint *bp, int x, int y)
{
	struct { rcti *rect; int select; } *data = userData;

	if (BLI_in_rcti(data->rect, x, y)) {
		bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
	}
}
static void do_lattice_box_select(rcti *rect, int select)
{
	struct { rcti *rect; int select, pass, done; } data;

	data.rect = rect;
	data.select = select;

	lattice_foreachScreenVert(do_lattice_box_select__doSelect, &data);
}

static void do_mesh_box_select__doSelectVert(void *userData, EditVert *eve, int x, int y, int index)
{
	struct { rcti *rect; short select, pass, done; } *data = userData;

	if (BLI_in_rcti(data->rect, x, y)) {
		eve->f = data->select?(eve->f|1):(eve->f&~1);
	}
}
static void do_mesh_box_select__doSelectEdge(void *userData, EditEdge *eed, int x0, int y0, int x1, int y1, int index)
{
	struct { rcti *rect; short select, pass, done; } *data = userData;

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
	struct { rcti *rect; short select, pass, done; } *data = userData;

	if (BLI_in_rcti(data->rect, x, y)) {
		EM_select_face_fgon(efa, data->select);
	}
}
static void do_mesh_box_select(rcti *rect, int select)
{
	struct { rcti *rect; short select, pass, done; } data;
	EditMesh *em = G.editMesh;
	int bbsel;
	
	data.rect = rect;
	data.select = select;
	data.pass = 0;
	data.done = 0;

	bbsel= EM_init_backbuf_border(rect->xmin, rect->ymin, rect->xmax, rect->ymax);

	if(G.scene->selectmode & SCE_SELECT_VERTEX) {
		if (bbsel) {
			EM_backbuf_checkAndSelectVerts(em, select);
		} else {
			mesh_foreachScreenVert(do_mesh_box_select__doSelectVert, &data, 1);
		}
	}
	if(G.scene->selectmode & SCE_SELECT_EDGE) {
			/* Does both bbsel and non-bbsel versions (need screen cos for both) */

		data.pass = 0;
		mesh_foreachScreenEdge(do_mesh_box_select__doSelectEdge, &data, 0);

		if (data.done==0) {
			data.pass = 1;
			mesh_foreachScreenEdge(do_mesh_box_select__doSelectEdge, &data, 0);
		}
	}
	
	if(G.scene->selectmode & SCE_SELECT_FACE) {
		if(bbsel) {
			EM_backbuf_checkAndSelectFaces(em, select);
		} else {
			mesh_foreachScreenFace(do_mesh_box_select__doSelectFace, &data);
		}
	}
	
	EM_free_backbuf();
		
	EM_selectmode_flush();
}

/**
 * Does the 'borderselect' command. (Select verts based on selecting with a 
 * border: key 'b'). All selecting seems to be done in the get_border part.
 */
void borderselect(void)
{
	rcti rect;
	Base *base;
	MetaElem *ml;
	unsigned int buffer[4*MAXPICKBUF];
	int a, index;
	short hits, val;

	if(G.obedit==NULL && (FACESEL_PAINT_TEST)) {
		face_borderselect();
		return;
	}
	else if(G.obedit==NULL && (G.f & G_PARTICLEEDIT)) {
		PE_borderselect();
		return;
	}

	a = 0;
#ifdef __APPLE__
	a = is_a_really_crappy_intel_card();
#endif
	if (!a) setlinestyle(2);
	val= get_border(&rect, 3);
	if (!a) setlinestyle(0);
	
	if(val==0) {
		if (EM_texFaceCheck())
			allqueue(REDRAWIMAGE, 0);
		return;
	}
	
	if(G.obedit) {
		if(G.obedit->type==OB_MESH) {
			do_mesh_box_select(&rect, (val==LEFTMOUSE));
			allqueue(REDRAWVIEW3D, 0);
			if (EM_texFaceCheck())
				allqueue(REDRAWIMAGE, 0);
			
		}
		else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {
			do_nurbs_box_select(&rect, val==LEFTMOUSE);
			allqueue(REDRAWVIEW3D, 0);
		}
		else if(G.obedit->type==OB_MBALL) {
			hits= view3d_opengl_select(buffer, MAXPICKBUF, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
			
			ml= editelems.first;
			
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
			allqueue(REDRAWVIEW3D, 0);
		}
		else if(G.obedit->type==OB_ARMATURE) {
			EditBone *ebone;
			
			/* clear flag we use to detect point was affected */
			for(ebone= G.edbo.first; ebone; ebone= ebone->next)
				ebone->flag &= ~BONE_DONE;
			
			hits= view3d_opengl_select(buffer, MAXPICKBUF, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
			
			/* first we only check points inside the border */
			for (a=0; a<hits; a++){
				index = buffer[(4*a)+3];
				if (index!=-1) {
					ebone = BLI_findlink(&G.edbo, index & ~(BONESEL_ANY));
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
			for(ebone= G.edbo.first; ebone; ebone= ebone->next) {
				if(ebone->parent && (ebone->flag & BONE_CONNECTED)) {
					if(ebone->parent->flag & BONE_DONE)
						ebone->flag |= BONE_DONE;
				}
			}
			
			/* only select/deselect entire bones when no points where in the rect */
			for (a=0; a<hits; a++){
				index = buffer[(4*a)+3];
				if (index!=-1) {
					ebone = BLI_findlink(&G.edbo, index & ~(BONESEL_ANY));
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
			
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWBUTSOBJECT, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
		else if(G.obedit->type==OB_LATTICE) {
			do_lattice_box_select(&rect, val==LEFTMOUSE);
			allqueue(REDRAWVIEW3D, 0);
		}
	}
	else {	/* no editmode, unified for bones and objects */
		Bone *bone;
		unsigned int *vbuffer=NULL; /* selection buffer	*/
		unsigned int *col;			/* color in buffer	*/
		short selecting = 0;
		Object *ob= OBACT;
		int bone_only;
		
		if((ob) && (ob->flag & OB_POSEMODE))
			bone_only= 1;
		else
			bone_only= 0;
		
		if (val==LEFTMOUSE)
			selecting = 1;
		
		/* selection buffer now has bones potentially too, so we add MAXPICKBUF */
		vbuffer = MEM_mallocN(4 * (G.totobj+MAXPICKBUF) * sizeof(unsigned int), "selection buffer");
		hits= view3d_opengl_select(vbuffer, 4*(G.totobj+MAXPICKBUF), rect.xmin, rect.ymin, rect.xmax, rect.ymax);
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
				if(base->lay & G.vd->lay) {
					while (base->selcol == (*col & 0xFFFF)) {	/* we got an object */
						
						if(*col & 0xFFFF0000) {					/* we got a bone */
							bone = get_indexed_bone(base->object, *col & ~(BONESEL_ANY));
							if(bone) {
								if(selecting) {
									bone->flag |= BONE_SELECTED;
									select_actionchannel_by_name(base->object->action, bone->name, 1);
								}
								else {
									bone->flag &= ~(BONE_ACTIVE|BONE_SELECTED);
									select_actionchannel_by_name(base->object->action, bone->name, 0);
								}
							}
						}
						else if(!bone_only) {
							if (selecting)
								select_base_v3d(base, BA_SELECT);
							else
								select_base_v3d(base, BA_DESELECT);

							base->object->flag= base->flag;
						}

						col+=4;	/* next color */
						hits--;
						if(hits==0) break;
					}
				}
				
				base= next;
			}
		}
		/* frontbuffer flush */
		bglFlush();

		MEM_freeN(vbuffer);
		
		allqueue(REDRAWDATASELECT, 0);
		allqueue(REDRAWBUTSLOGIC, 0);
		allqueue(REDRAWNLA, 0);
	}

	countall();
	
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWINFO, 0);

	BIF_undo_push("Border select");
	
} /* end of borderselect() */

/* ------------------------------------------------------------------------- */

/** The following functions are quick & dirty callback functions called
  * on the Circle select function (press B twice in Editmode)
  * They were torn out of the circle_select to make the latter more reusable
  * The callback version of circle_select (called circle_selectCB) was moved
  * to edit.c because of it's (wanted) generality.

	XXX These callback functions are still dirty, because they call globals... 
  */

static void mesh_selectionCB__doSelectVert(void *userData, EditVert *eve, int x, int y, int index)
{
	struct { short select, mval[2]; float radius; } *data = userData;
	int mx = x - data->mval[0], my = y - data->mval[1];
	float r = sqrt(mx*mx + my*my);

	if (r<=data->radius) {
		eve->f = data->select?(eve->f|1):(eve->f&~1);
	}
}
static void mesh_selectionCB__doSelectEdge(void *userData, EditEdge *eed, int x0, int y0, int x1, int y1, int index)
{
	struct { short select, mval[2]; float radius; } *data = userData;

	if (edge_inside_circle(data->mval[0], data->mval[1], (short) data->radius, x0, y0, x1, y1)) {
		EM_select_edge(eed, data->select);
	}
}
static void mesh_selectionCB__doSelectFace(void *userData, EditFace *efa, int x, int y, int index)
{
	struct { short select, mval[2]; float radius; } *data = userData;
	int mx = x - data->mval[0], my = y - data->mval[1];
	float r = sqrt(mx*mx + my*my);

	if (r<=data->radius) {
		EM_select_face_fgon(efa, data->select);
	}
}
static void mesh_selectionCB(int selecting, Object *editobj, short *mval, float rad)
{
	struct { short select, mval[2]; float radius; } data;
	EditMesh *em = G.editMesh;
	int bbsel;

	if(!G.obedit && (FACESEL_PAINT_TEST)) {
		Mesh *me = get_mesh(OBACT);

		if (me) {
			em_vertoffs= me->totface+1;	/* max index array */

			bbsel= EM_init_backbuf_circle(mval[0], mval[1], (short)(rad+1.0));
			EM_backbuf_checkAndSelectTFaces(me, selecting==LEFTMOUSE);
			EM_free_backbuf();

			object_tface_flags_changed(OBACT, 0);
		}

		return;
	}

	bbsel= EM_init_backbuf_circle(mval[0], mval[1], (short)(rad+1.0));
	
	data.select = (selecting==LEFTMOUSE);
	data.mval[0] = mval[0];
	data.mval[1] = mval[1];
	data.radius = rad;

	if(G.scene->selectmode & SCE_SELECT_VERTEX) {
		if(bbsel) {
			EM_backbuf_checkAndSelectVerts(em, selecting==LEFTMOUSE);
		} else {
			mesh_foreachScreenVert(mesh_selectionCB__doSelectVert, &data, 1);
		}
	}

	if(G.scene->selectmode & SCE_SELECT_EDGE) {
		if (bbsel) {
			EM_backbuf_checkAndSelectEdges(em, selecting==LEFTMOUSE);
		} else {
			mesh_foreachScreenEdge(mesh_selectionCB__doSelectEdge, &data, 0);
		}
	}
	
	if(G.scene->selectmode & SCE_SELECT_FACE) {
		if(bbsel) {
			EM_backbuf_checkAndSelectFaces(em, selecting==LEFTMOUSE);
		} else {
			mesh_foreachScreenFace(mesh_selectionCB__doSelectFace, &data);
		}
	}

	EM_free_backbuf();
	EM_selectmode_flush();
}


static void nurbscurve_selectionCB__doSelect(void *userData, Nurb *nu, BPoint *bp, BezTriple *bezt, int beztindex, int x, int y)
{
	struct { short select, mval[2]; float radius; } *data = userData;
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
static void nurbscurve_selectionCB(int selecting, Object *editobj, short *mval, float rad)
{
	struct { short select, mval[2]; float radius; } data;

	data.select = (selecting==LEFTMOUSE);
	data.mval[0] = mval[0];
	data.mval[1] = mval[1];
	data.radius = rad;

	nurbs_foreachScreenVert(nurbscurve_selectionCB__doSelect, &data);
}


static void latticecurve_selectionCB__doSelect(void *userData, BPoint *bp, int x, int y)
{
	struct { short select, mval[2]; float radius; } *data = userData;
	int mx = x - data->mval[0], my = y - data->mval[1];
	float r = sqrt(mx*mx + my*my);

	if (r<=data->radius) {
		bp->f1 = data->select?(bp->f1|SELECT):(bp->f1&~SELECT);
	}
}
static void lattice_selectionCB(int selecting, Object *editobj, short *mval, float rad)
{
	struct { short select, mval[2]; float radius; } data;

	data.select = (selecting==LEFTMOUSE);
	data.mval[0] = mval[0];
	data.mval[1] = mval[1];
	data.radius = rad;

	lattice_foreachScreenVert(latticecurve_selectionCB__doSelect, &data);
}

/** Callbacks for selection in Editmode */

void obedit_selectionCB(short selecting, Object *editobj, short *mval, float rad) 
{
	switch(editobj->type) {		
	case OB_MESH:
		mesh_selectionCB(selecting, editobj, mval, rad);
		break;
	case OB_CURVE:
	case OB_SURF:
		nurbscurve_selectionCB(selecting, editobj, mval, rad);
		break;
	case OB_LATTICE:
		lattice_selectionCB(selecting, editobj, mval, rad);
		break;
	default:
		return;
	}

	draw_sel_circle(0, 0, 0, 0, 0);	/* signal */
	force_draw(0);
}

void set_render_border(void)
{
	rcti rect;
	short val;
	
	val= get_border(&rect, 3);
	if(val) {
		rctf vb;

		calc_viewborder(G.vd, &vb);

		G.scene->r.border.xmin= ((float)rect.xmin-vb.xmin)/(vb.xmax-vb.xmin);
		G.scene->r.border.ymin= ((float)rect.ymin-vb.ymin)/(vb.ymax-vb.ymin);
		G.scene->r.border.xmax= ((float)rect.xmax-vb.xmin)/(vb.xmax-vb.xmin);
		G.scene->r.border.ymax= ((float)rect.ymax-vb.ymin)/(vb.ymax-vb.ymin);
				
		CLAMP(G.scene->r.border.xmin, 0.0, 1.0);
		CLAMP(G.scene->r.border.ymin, 0.0, 1.0);
		CLAMP(G.scene->r.border.xmax, 0.0, 1.0);
		CLAMP(G.scene->r.border.ymax, 0.0, 1.0);
	
		allqueue(REDRAWVIEWCAM, 1);
		
		/* drawing a border surrounding the entire camera view switches off border rendering
		 * or the border covers no pixels */
		if ((G.scene->r.border.xmin <= 0.0 && G.scene->r.border.xmax >= 1.0 &&
			G.scene->r.border.ymin <= 0.0 && G.scene->r.border.ymax >= 1.0) ||
		   (G.scene->r.border.xmin == G.scene->r.border.xmax ||
			G.scene->r.border.ymin == G.scene->r.border.ymax ))
		{
			G.scene->r.mode &= ~R_BORDER;
		} else {
			G.scene->r.mode |= R_BORDER;
		}
		
		allqueue(REDRAWBUTSSCENE, 1);
	}
}

void view3d_border_zoom(void)
{
	View3D *v3d = G.vd;
	
	/* Zooms in on a border drawn by the user */
	rcti rect;
	short val;
	float dvec[3], vb[2], xscale, yscale, scale;
	
	
	/* SMOOTHVIEW */
	float new_dist;
	float new_ofs[3];
	
	/* ZBuffer depth vars */
	bglMats mats;
	float depth, depth_close= MAXFLOAT;
	int had_depth = 0;
	double cent[2],  p[3];
	int xs, ys;
	
	/* Get the border input */
	val = get_border(&rect, 3);
	if(!val) return;
	
	/* Get Z Depths, needed for perspective, nice for ortho */
	bgl_get_mats(&mats);
	draw_depth(curarea, (void *)v3d);
	
	/* force updating */
	if (v3d->depths) {
		had_depth = 1;
		v3d->depths->damaged = 1;
	}
	
	view3d_update_depths(v3d);
	
	/* Constrain rect to depth bounds */
	if (rect.xmin < 0) rect.xmin = 0;
	if (rect.ymin < 0) rect.ymin = 0;
	if (rect.xmax >= v3d->depths->w) rect.xmax = v3d->depths->w-1;
	if (rect.ymax >= v3d->depths->h) rect.ymax = v3d->depths->h-1;		
	
	/* Find the closest Z pixel */
	for (xs=rect.xmin; xs < rect.xmax; xs++) {
		for (ys=rect.ymin; ys < rect.ymax; ys++) {
			depth= v3d->depths->depths[ys*v3d->depths->w+xs];
			if(depth < v3d->depths->depth_range[1] && depth > v3d->depths->depth_range[0]) {
				if (depth_close > depth) {
					depth_close = depth;
				}
			}
		}
	}
	
	if (had_depth==0) {
		MEM_freeN(v3d->depths->depths);
		v3d->depths->depths = NULL;
	}
	v3d->depths->damaged = 1;
	
	cent[0] = (((double)rect.xmin)+((double)rect.xmax)) / 2;
	cent[1] = (((double)rect.ymin)+((double)rect.ymax)) / 2;
	
	if (v3d->persp==V3D_PERSP) {
		double p_corner[3];

		/* no depths to use, we cant do anything! */
		if (depth_close==MAXFLOAT) 
			return;
		
		/* convert border to 3d coordinates */
		if ((	!gluUnProject(cent[0], cent[1], depth_close, mats.modelview, mats.projection, mats.viewport, &p[0], &p[1], &p[2])) || 
			(	!gluUnProject((double)rect.xmin, (double)rect.ymin, depth_close, mats.modelview, mats.projection, mats.viewport, &p_corner[0], &p_corner[1], &p_corner[2])))
			return;
		
		dvec[0] = p[0]-p_corner[0];
		dvec[1] = p[1]-p_corner[1];
		dvec[2] = p[2]-p_corner[2];
		
		new_dist = VecLength(dvec);
		if(new_dist <= v3d->near*1.5) new_dist= v3d->near*1.5; 
		
		new_ofs[0] = -p[0];
		new_ofs[1] = -p[1];
		new_ofs[2] = -p[2];
		
	} else { /* othographic */
		/* find the current window width and height */
		vb[0] = v3d->area->winx;
		vb[1] = v3d->area->winy;
		
		new_dist = v3d->dist;
		
		/* convert the drawn rectangle into 3d space */
		if (depth_close!=MAXFLOAT && gluUnProject(cent[0], cent[1], depth_close, mats.modelview, mats.projection, mats.viewport, &p[0], &p[1], &p[2])) {
			new_ofs[0] = -p[0];
			new_ofs[1] = -p[1];
			new_ofs[2] = -p[2];
		} else {
			/* We cant use the depth, fallback to the old way that dosnt set the center depth */
			new_ofs[0] = v3d->ofs[0];
			new_ofs[1] = v3d->ofs[1];
			new_ofs[2] = v3d->ofs[2];
			
			initgrabz(-new_ofs[0], -new_ofs[1], -new_ofs[2]);
			
			window_to_3d(dvec, (rect.xmin+rect.xmax-vb[0])/2, (rect.ymin+rect.ymax-vb[1])/2);
			/* center the view to the center of the rectangle */
			VecSubf(new_ofs, new_ofs, dvec);
		}
		
		/* work out the ratios, so that everything selected fits when we zoom */
		xscale = ((rect.xmax-rect.xmin)/vb[0]);
		yscale = ((rect.ymax-rect.ymin)/vb[1]);
		scale = (xscale >= yscale)?xscale:yscale;
		
		/* zoom in as required, or as far as we can go */
		new_dist = ((new_dist*scale) >= 0.001*v3d->grid)? new_dist*scale:0.001*v3d->grid;
	}
	
	smooth_view(v3d, new_ofs, NULL, &new_dist, NULL);
}

void fly(void)
{
	/*
	fly mode - Shift+F
	a fly loop where the user can move move the view as if they are flying
	*/
	float speed=0.0, /* the speed the view is moving per redraw */
	mat[3][3], /* 3x3 copy of the view matrix so we can move allong the view axis */
	dvec[3]={0,0,0}, /* this is the direction thast added to the view offset per redraw */
	dvec_old[3]={0,0,0}, /* old for some lag */
	dvec_tmp[3]={0,0,0}, /* old for some lag */
	dvec_lag=0.0, /* old for some lag */
	
	/* Camera Uprighting variables */
	roll, /* similar to the angle between the camera's up and the Z-up, but its very rough so just roll*/
	upvec[3]={0,0,0}, /* stores the view's up vector */
	
	dist_backup, /* backup the views distance since we use a zero dist for fly mode */
	rot_backup[4], /* backup the views quat incase the user cancels flying in non camera mode */
	ofs_backup[3], /* backup the views offset incase the user cancels flying in non camera mode */
	moffset[2], /* mouse offset from the views center */
	tmp_quat[4], /* used for rotating the view */
	winxf, winyf, /* scale the mouse movement by this value - scales mouse movement to the view size */
	time_redraw, time_redraw_clamped, time_wheel; /*time how fast it takes for us to redraw, this is so simple scenes dont fly too fast */
	
	/* time_lastwheel is used to accelerate when using the mousewheel a lot */
	double time_current, time_lastdraw, time_currwheel, time_lastwheel;
	
	short val, /* used for toets to see if a buttons pressed */
	cent_orig[2], /* view center */
	cent[2], /* view center modified */
	mval[2], /* mouse location */
	action=0, /* while zero stay in fly mode and wait for action, also used to see if we accepted or canceled 1:ok 2:Cancel */
	xmargin, ymargin; /* x and y margin are define the safe area where the mouses movement wont rotate the view */
	unsigned short
	toets; /* for reading the event */
	unsigned char
	apply_rotation= 1, /* if the user presses shift they can look about without movinf the direction there looking*/
	axis= 2, /* Axis index to move allong by default Z to move allong the view */
	persp_backup, /* remember if were ortho or not, only used for restoring the view if it was a ortho view */
	pan_view=0; /* if true, pan the view instead of rotating */
	
	/* relative view axis locking - xlock, zlock
	0; disabled
	1; enabled but not checking because mouse hasnt moved outside the margin since locking was checked an not needed
	   when the mouse moves, locking is set to 2 so checks are done.
	2; mouse moved and checking needed, if no view altering is donem its changed back to 1 */
	short xlock=0, zlock=0;
	float xlock_momentum=0.0f, zlock_momentum=0.0f; /* nicer dynamics */
	
	/* for recording */
	int playing_anim = has_screenhandler(G.curscreen, SCREEN_HANDLER_ANIM);
	int cfra = -1; /*so the first frame always has a key added */
	char *actname="";
	
	if(curarea->spacetype!=SPACE_VIEW3D) return;
		
	if(G.vd->persp==V3D_CAMOB && G.vd->camera->id.lib) {
		error("Cannot fly a camera from an external library");
		return;
	}
	
	if(G.vd->ob_centre) {
		error("Cannot fly when the view is locked to an object");
		return;
	}
	
	
	/* detect weather to start with Z locking */
	upvec[0]=1; upvec[1]=0; upvec[2]=0;
	Mat3CpyMat4(mat, G.vd->viewinv);
	Mat3MulVecfl(mat, upvec);
	if (fabs(upvec[2]) < 0.1)
		zlock = 1;
	upvec[0]=0; upvec[1]=0; upvec[2]=0;
	
	persp_backup= G.vd->persp;
	dist_backup= G.vd->dist;
	if (G.vd->persp==V3D_CAMOB) {
		if(G.vd->camera->constraints.first) {
			error("Cannot fly an object with constraints");
			return;
		}
		
		/* store the origoinal camera loc and rot */
		VECCOPY(ofs_backup, G.vd->camera->loc);
		VECCOPY(rot_backup, G.vd->camera->rot);
		
		where_is_object(G.vd->camera);
		VECCOPY(G.vd->ofs, G.vd->camera->obmat[3]);
		VecMulf(G.vd->ofs, -1.0f); /*flip the vector*/
		
		G.vd->dist=0.0;
		G.vd->viewbut=0;
		
		/* used for recording */
		if(G.vd->camera->ipoflag & OB_ACTION_OB)
			actname= "Object";
		
	} else {
		/* perspective or ortho */
		if (G.vd->persp==V3D_ORTHO)
			G.vd->persp= V3D_PERSP; /*if ortho projection, make perspective */
		QUATCOPY(rot_backup, G.vd->viewquat);
		VECCOPY(ofs_backup, G.vd->ofs);
		G.vd->dist= 0.0;
		
		upvec[2]=dist_backup; /*x and y are 0*/
		Mat3MulVecfl(mat, upvec);
		VecSubf(G.vd->ofs, G.vd->ofs, upvec);
		/*Done with correcting for the dist*/
	}
	
	/* the dist defines a vector that is infront of the offset
	to rotate the view about.
	this is no good for fly mode because we
	want to rotate about the viewers center.
	but to correct the dist removal we must
	alter offset so the view dosent jump. */
	
	xmargin= (short)((float)(curarea->winx)/20.0);
	ymargin= (short)((float)(curarea->winy)/20.0);
	
	cent_orig[0]= curarea->winrct.xmin+(curarea->winx)/2;
	cent_orig[1]= curarea->winrct.ymin+(curarea->winy)/2;
	
	warp_pointer(cent_orig[0], cent_orig[1]);

	/* we have to rely on events to give proper mousecoords after a warp_pointer */
	mval[0]= cent[0]=  (curarea->winx)/2;
	mval[1]= cent[1]=  (curarea->winy)/2;
	/* window size minus margin - use this to get the mouse range for rotation */
	winxf= (float)(curarea->winx)-(xmargin*2);
	winyf= (float)(curarea->winy)-(ymargin*2); 
	
	
	time_lastdraw= time_lastwheel= PIL_check_seconds_timer();
	
	G.vd->flag2 |= V3D_FLYMODE; /* so we draw the corner margins */
	scrarea_do_windraw(curarea);
	screen_swapbuffers();
	
	while(action==0) { /* keep flying, no acton taken */
		while(qtest()) {
			toets= extern_qread(&val);
			
			if(val) {
				if(toets==MOUSEY) getmouseco_areawin(mval);
				else if(toets==ESCKEY || toets==RIGHTMOUSE) {
					action= 2; /* Canceled */
					break;
				} else if(toets==SPACEKEY || toets==LEFTMOUSE) {
					action= 1; /* Accepted */
					break;
				} else if(toets==PADPLUSKEY || toets==EQUALKEY || toets==WHEELUPMOUSE) {
					time_currwheel= PIL_check_seconds_timer();
					time_wheel = (float)(time_currwheel - time_lastwheel);
					time_lastwheel = time_currwheel;
					/*printf("Wheel %f\n", time_wheel);*/
					/*Mouse wheel delays range from 0.5==slow to 0.01==fast*/ 
					time_wheel = 1+ (10 - (20*MIN2(time_wheel, 0.5))); /* 0-0.5 -> 0-5.0 */
					
					if (speed<0) speed=0;
					else {
						if (G.qual & LR_SHIFTKEY)
							speed+= G.vd->grid*time_wheel*0.1;
						else
							speed+= G.vd->grid*time_wheel;
					}
					
				} else if(toets==PADMINUS || toets==MINUSKEY || toets==WHEELDOWNMOUSE) {
					time_currwheel= PIL_check_seconds_timer();
					time_wheel = (float)(time_currwheel - time_lastwheel);
					time_lastwheel = time_currwheel;
					time_wheel = 1+ (10 - (20*MIN2(time_wheel, 0.5))); /* 0-0.5 -> 0-5.0 */
					
					if (speed>0) speed=0;
					else {
						if (G.qual & LR_SHIFTKEY)
							speed-= G.vd->grid*time_wheel*0.1;
						else
							speed-= G.vd->grid*time_wheel;
					}
				
				} else if (toets==MIDDLEMOUSE) {
					/* make it so the camera direction dosent follow the view
					good for flying backwards! - Only when MMB is held */
					
					/*apply_rotation=0;*/
					pan_view= 1;
					
				/* impliment WASD keys */
				} else if(toets==WKEY) {
					if (speed<0) speed=-speed; /* flip speed rather then stopping, game like motion */
					else speed+= G.vd->grid; /* increse like mousewheel if were alredy moving in that difection*/
					axis= 2;
				} else if(toets==SKEY) { /*SAME as above but flipped */
					if (speed>0) speed=-speed;
					else speed-= G.vd->grid;
					axis= 2;
				
				} else if(toets==AKEY) {
					if (speed<0) speed=-speed;
					axis= 0;
				} else if(toets==DKEY) {
					if (speed>0) speed=-speed;
					axis= 0;
				} else if(toets==FKEY) {
					if (speed<0) speed=-speed;
					axis= 1;
				} else if(toets==RKEY) {
					if (speed>0) speed=-speed;
					axis= 1;
				
				/* axis locking */
				} else if(toets==XKEY) {
					if (xlock) xlock=0;
					else {
						xlock = 2;
						xlock_momentum = 0.0;
					}
				} else if(toets==ZKEY) {
					if (zlock) zlock=0;
					else {
						zlock = 2;
						zlock_momentum = 0.0;
					}
				}
				
			} else {
				/* mouse buttons lifted */
				if (toets==MIDDLEMOUSE && pan_view) {
					/*apply_rotation=1;*/
					warp_pointer(cent_orig[0], cent_orig[1]);
					pan_view= 0;
				}
			}
		}
		if(action != 0) break;
		
		moffset[0]= mval[0]-cent[0];
		moffset[1]= mval[1]-cent[1];
		
		/* enforce a view margin */
		if (moffset[0]>xmargin)		moffset[0]-=xmargin;
		else if (moffset[0]<-xmargin)moffset[0]+=xmargin;
		else					moffset[0]=0;

		if (moffset[1]>ymargin)		moffset[1]-=ymargin;
		else if (moffset[1]<-ymargin)moffset[1]+=ymargin;
		else					moffset[1]=0;
		
		/* scale the mouse offset so the distance the mouse moves isnt linear */
		if (moffset[0]) {
			moffset[0]= moffset[0]/winxf;
			moffset[0]= moffset[0]*fabs(moffset[0]);
		}
		
		if (moffset[1]) {
			moffset[1]= moffset[1]/winyf;
			moffset[1]= moffset[1]*fabs(moffset[1]);
		}
		
		/* Should we redraw? */
		if(speed!=0.0 || moffset[0] || moffset[1] || zlock || xlock || dvec[0] || dvec[1] || dvec[2] ) {
			
			time_current= PIL_check_seconds_timer();
			time_redraw= (float)(time_current-time_lastdraw);
			time_redraw_clamped= MIN2(0.05, time_redraw); /* clamt the redraw time to avoid jitter in roll correction */
			time_lastdraw= time_current;
			/*fprintf(stderr, "%f\n", time_redraw);*/ /* 0.002 is a small redraw 0.02 is larger */
			
			/* Scale the time to use shift to scale the speed down- just like
			shift slows many other areas of blender down */
			if (G.qual & LR_SHIFTKEY)
				speed= speed * (1-time_redraw_clamped);
			
			Mat3CpyMat4(mat, G.vd->viewinv);
			
			if (pan_view) {
				/* pan only */
				dvec_tmp[0]= -moffset[0];
				dvec_tmp[1]= -moffset[1];
				dvec_tmp[2]= 0;
				
				if (G.qual & LR_SHIFTKEY) {
					dvec_tmp[0] *= 0.1;
					dvec_tmp[1] *= 0.1;
				}
				
				Mat3MulVecfl(mat, dvec_tmp);
				VecMulf(dvec_tmp, time_redraw*200.0 * G.vd->grid);
				
			} else {
				/* rotate about the X axis- look up/down */
				if (moffset[1]) {
					upvec[0]=1;
					upvec[1]=0;
					upvec[2]=0;
					Mat3MulVecfl(mat, upvec);
					VecRotToQuat( upvec, (float)moffset[1]*-time_redraw*20, tmp_quat); /* Rotate about the relative up vec */
					QuatMul(G.vd->viewquat, G.vd->viewquat, tmp_quat);
					
					if (xlock) xlock = 2; /*check for rotation*/
					if (zlock) zlock = 2;
					xlock_momentum= 0.0;
				}
				
				/* rotate about the Y axis- look left/right */
				if (moffset[0]) {
					
					if (zlock) {
						upvec[0]=0;
						upvec[1]=0;
						upvec[2]=1;
					} else {
						upvec[0]=0;
						upvec[1]=1;
						upvec[2]=0;
						Mat3MulVecfl(mat, upvec);
					}
						
					VecRotToQuat( upvec, (float)moffset[0]*time_redraw*20, tmp_quat); /* Rotate about the relative up vec */
					QuatMul(G.vd->viewquat, G.vd->viewquat, tmp_quat);
					
					if (xlock) xlock = 2;/*check for rotation*/
					if (zlock) zlock = 2;
				}
				
				if (zlock==2) {
					upvec[0]=1;
					upvec[1]=0;
					upvec[2]=0;
					Mat3MulVecfl(mat, upvec);

					/*make sure we have some z rolling*/
					if (fabs(upvec[2]) > 0.00001) {
						roll= upvec[2]*5;
						upvec[0]=0; /*rotate the view about this axis*/
						upvec[1]=0;
						upvec[2]=1;
						
						Mat3MulVecfl(mat, upvec);
						VecRotToQuat( upvec, roll*time_redraw_clamped*zlock_momentum*0.1, tmp_quat); /* Rotate about the relative up vec */
						QuatMul(G.vd->viewquat, G.vd->viewquat, tmp_quat);
						
						zlock_momentum += 0.05;
					} else {
						zlock=1; /* dont check until the view rotates again */
						zlock_momentum= 0.0;
					}
				}
				
				if (xlock==2 && moffset[1]==0) { /*only apply xcorrect when mouse isnt applying x rot*/
					upvec[0]=0;
					upvec[1]=0;
					upvec[2]=1;
					Mat3MulVecfl(mat, upvec);
					/*make sure we have some z rolling*/
					if (fabs(upvec[2]) > 0.00001) {
						roll= upvec[2]*-5;
						
						upvec[0]=1; /*rotate the view about this axis*/
						upvec[1]=0;
						upvec[2]=0;
						
						Mat3MulVecfl(mat, upvec);
						
						VecRotToQuat( upvec, roll*time_redraw_clamped*xlock_momentum*0.1, tmp_quat); /* Rotate about the relative up vec */
						QuatMul(G.vd->viewquat, G.vd->viewquat, tmp_quat);
						
						xlock_momentum += 0.05;
					} else {
						xlock=1; /* see above */
						xlock_momentum= 0.0;
					}
				}


				if (apply_rotation) {
					/* Normal operation */
					/* define dvec, view direction vector */
					dvec_tmp[0]= dvec_tmp[1]= dvec_tmp[2]= 0;
					/* move along the current axis */
					dvec_tmp[axis]= 1.0f;
					
					Mat3MulVecfl(mat, dvec_tmp);
					
					VecMulf(dvec_tmp, speed*time_redraw*0.25);
				}
			}
			
			/* impose a directional lag */
			dvec_lag = 1.0/(1+(time_redraw*5));
			dvec[0] = dvec_tmp[0]*(1-dvec_lag) + dvec_old[0]*dvec_lag;
			dvec[1] = dvec_tmp[1]*(1-dvec_lag) + dvec_old[1]*dvec_lag;
			dvec[2] = dvec_tmp[2]*(1-dvec_lag) + dvec_old[2]*dvec_lag;
			
			
			if (G.vd->persp==V3D_CAMOB) {
				if (G.vd->camera->protectflag & OB_LOCK_LOCX)
					dvec[0] = 0.0;
				if (G.vd->camera->protectflag & OB_LOCK_LOCY)
					dvec[1] = 0.0;
				if (G.vd->camera->protectflag & OB_LOCK_LOCZ)
					dvec[2] = 0.0;
			}
			
			VecAddf(G.vd->ofs, G.vd->ofs, dvec);
			if (zlock && xlock)
				headerprint("FlyKeys  Speed:(+/- | Wheel),  Upright Axis:X  on/Z on,   Slow:Shift,  Direction:WASDRF,  Ok:LMB,  Pan:MMB,  Cancel:RMB");
			else if (zlock) 
				headerprint("FlyKeys  Speed:(+/- | Wheel),  Upright Axis:X off/Z on,   Slow:Shift,  Direction:WASDRF,  Ok:LMB,  Pan:MMB,  Cancel:RMB");
			else if (xlock)
				headerprint("FlyKeys  Speed:(+/- | Wheel),  Upright Axis:X  on/Z off,  Slow:Shift,  Direction:WASDRF,  Ok:LMB,  Pan:MMB,  Cancel:RMB");
			else
				headerprint("FlyKeys  Speed:(+/- | Wheel),  Upright Axis:X off/Z off,  Slow:Shift,  Direction:WASDRF,  Ok:LMB,  Pan:MMB,  Cancel:RMB");
			
			do_screenhandlers(G.curscreen); /* advance the next frame */
			
			/* we are in camera view so apply the view ofs and quat to the view matrix and set the camera to the view */
			if (G.vd->persp==V3D_CAMOB) {
				G.vd->persp= V3D_PERSP; /*set this so setviewmatrixview3d uses the ofs and quat instead of the camera */
				setviewmatrixview3d();
				setcameratoview3d();
				G.vd->persp= V3D_CAMOB;
				
				/* record the motion */
				if (IS_AUTOKEY_MODE(NORMAL) && (!playing_anim || cfra != G.scene->r.cfra)) {
					cfra = G.scene->r.cfra;
					
					if (xlock || zlock || moffset[0] || moffset[1]) {
						insertkey(&G.vd->camera->id, ID_OB, actname, NULL, OB_ROT_X, 0);
						insertkey(&G.vd->camera->id, ID_OB, actname, NULL, OB_ROT_Y, 0);
						insertkey(&G.vd->camera->id, ID_OB, actname, NULL, OB_ROT_Z, 0);
					}
					if (speed) {
						insertkey(&G.vd->camera->id, ID_OB, actname, NULL, OB_LOC_X, 0);
						insertkey(&G.vd->camera->id, ID_OB, actname, NULL, OB_LOC_Y, 0);
						insertkey(&G.vd->camera->id, ID_OB, actname, NULL, OB_LOC_Z, 0);
					}
				}
			}
			scrarea_do_windraw(curarea);
			screen_swapbuffers();
		} else 
			/*were not redrawing but we need to update the time else the view will jump */
			time_lastdraw= PIL_check_seconds_timer();
		/* end drawing */
		VECCOPY(dvec_old, dvec);
	}
	
	G.vd->dist= dist_backup;
	
	/* Revert to original view? */ 
	if (action == 2) { /* action == 2 means the user pressed Esc of RMB, and not to apply view to camera */
		if (persp_backup==V3D_CAMOB) { /* a camera view */
			G.vd->viewbut=1;
			VECCOPY(G.vd->camera->loc, ofs_backup);
			VECCOPY(G.vd->camera->rot, rot_backup);
			DAG_object_flush_update(G.scene, G.vd->camera, OB_RECALC_OB);
		} else {
			/* Non Camera we need to reset the view back to the original location bacause the user canceled*/
			QUATCOPY(G.vd->viewquat, rot_backup);
			VECCOPY(G.vd->ofs, ofs_backup);
			G.vd->persp= persp_backup;
		}
	}
	else if (persp_backup==V3D_CAMOB) {	/* camera */
		float mat3[3][3];
		Mat3CpyMat4(mat3, G.vd->camera->obmat);
		Mat3ToCompatibleEul(mat3, G.vd->camera->rot, rot_backup);
		
		DAG_object_flush_update(G.scene, G.vd->camera, OB_RECALC_OB);
		
		if (IS_AUTOKEY_MODE(NORMAL)) {
			allqueue(REDRAWIPO, 0);
			allspace(REMAKEIPO, 0);
			allqueue(REDRAWNLA, 0);
			allqueue(REDRAWTIME, 0);
		}
	}
	else { /* not camera */
		/* Apply the fly mode view */
		/*restore the dist*/
		upvec[0]= upvec[1]= 0;
		upvec[2]=dist_backup; /*x and y are 0*/
		Mat3CpyMat4(mat, G.vd->viewinv);
		Mat3MulVecfl(mat, upvec);
		VecAddf(G.vd->ofs, G.vd->ofs, upvec);
		/*Done with correcting for the dist */
	}
	
	G.vd->flag2 &= ~V3D_FLYMODE;
	allqueue(REDRAWVIEW3D, 0);
	BIF_view3d_previewrender_signal(curarea, PR_DBASE|PR_DISPRECT); /* not working at the moment not sure why */
}

void view3d_edit_clipping(View3D *v3d)
{
	
	if(v3d->flag & V3D_CLIPPING) {
		v3d->flag &= ~V3D_CLIPPING;
		scrarea_queue_winredraw(curarea);
		if(v3d->clipbb) MEM_freeN(v3d->clipbb);
		v3d->clipbb= NULL;
	}
	else {
		rcti rect;
		double mvmatrix[16];
		double projmatrix[16];
		double xs, ys, p[3];
		GLint viewport[4];
		short val;
		
		/* get border in window coords */
		setlinestyle(2);
		val= get_border(&rect, 3);
		setlinestyle(0);
		if(val==0) return;
		
		v3d->flag |= V3D_CLIPPING;
		v3d->clipbb= MEM_callocN(sizeof(BoundBox), "clipbb");
		
		/* convert border to 3d coordinates */
		
		/* Get the matrices needed for gluUnProject */
		glGetIntegerv(GL_VIEWPORT, viewport);
		glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix);
		glGetDoublev(GL_PROJECTION_MATRIX, projmatrix);

		/* near zero floating point values can give issues with gluUnProject
		   in side view on some implementations */
		if(fabs(mvmatrix[0]) < 1e-6) mvmatrix[0]= 0.0;
		if(fabs(mvmatrix[5]) < 1e-6) mvmatrix[5]= 0.0;
		
		/* Set up viewport so that gluUnProject will give correct values */
		viewport[0] = 0;
		viewport[1] = 0;
		
		/* four clipping planes and bounding volume */
		/* first do the bounding volume */
		for(val=0; val<4; val++) {
			
			xs= (val==0||val==3)?rect.xmin:rect.xmax;
			ys= (val==0||val==1)?rect.ymin:rect.ymax;
			
			gluUnProject(xs, ys, 0.0, mvmatrix, projmatrix, viewport, &p[0], &p[1], &p[2]);
			VECCOPY(v3d->clipbb->vec[val], p);
			
			gluUnProject(xs, ys, 1.0, mvmatrix, projmatrix, viewport, &p[0], &p[1], &p[2]);
			VECCOPY(v3d->clipbb->vec[4+val], p);
		}
		
		/* then plane equations */
		for(val=0; val<4; val++) {
			
			CalcNormFloat(v3d->clipbb->vec[val], v3d->clipbb->vec[val==3?0:val+1], v3d->clipbb->vec[val+4],
						  v3d->clip[val]); 
			
			v3d->clip[val][3]= - v3d->clip[val][0]*v3d->clipbb->vec[val][0] 
							   - v3d->clip[val][1]*v3d->clipbb->vec[val][1] 
							   - v3d->clip[val][2]*v3d->clipbb->vec[val][2];
		}
	}
}

