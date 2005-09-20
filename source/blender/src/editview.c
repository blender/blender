/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
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
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "BKE_armature.h"
#include "BKE_global.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_utildefines.h"

#include "BIF_butspace.h"
#include "BIF_editaction.h"
#include "BIF_editarmature.h"
#include "BIF_editgroup.h"
#include "BIF_editmesh.h"
#include "BIF_editoops.h"
#include "BIF_editsima.h"
#include "BIF_editview.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"

#include "BDR_editobject.h"	/* For headerprint */
#include "BDR_vpaint.h"
#include "BDR_editface.h"
#include "BDR_drawobject.h"
#include "BDR_editcurve.h"

#include "BSE_edit.h"
#include "BSE_view.h"		/* give_cursor() */
#include "BSE_editipo.h"
#include "BSE_drawview.h"

#include "editmesh.h"	// borderselect uses it...
#include "blendef.h"
#include "mydevice.h"

#include "BIF_transform.h"

extern ListBase editNurb; /* originally from exports.h, memory from editcurve.c*/
/* editmball.c */
extern ListBase editelems;

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
	TFace *tface = me->tface;
	int a;

	if (tface) {
		for(a=1; a<=me->totface; a++, tface++) {
			if(EM_check_backbuf(a)) {
				tface->flag = select?(tface->flag|TF_SELECT):(tface->flag&~TF_SELECT);
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

/* *********************** GESTURE AND LASSO ******************* */

/* helper also for borderselect */
static int edge_fully_inside_rect(rcti *rect, short x1, short y1, short x2, short y2)
{
	return BLI_in_rcti(rect, x1, y1) && BLI_in_rcti(rect, x2, y2);
}

static int edge_inside_rect(rcti *rect, short x1, short y1, short x2, short y2)
{
	int d1, d2, d3, d4;
	
	// check points in rect
	if(edge_fully_inside_rect(rect, x1, y1, x2, y2)) return 1;
	
	// check points completely out rect
	if(x1<rect->xmin && x2<rect->xmin) return 0;
	if(x1>rect->xmax && x2>rect->xmax) return 0;
	if(y1<rect->ymin && y2<rect->ymin) return 0;
	if(y1>rect->ymax && y2>rect->ymax) return 0;
	
	// simple check lines intersecting. 
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

static int lasso_inside(short mcords[][2], short moves, short sx, short sy)
{
	/* we do the angle rule, define that all added angles should be about zero or 2*PI */
	float angletot=0.0, len, dot, ang, cross, fp1[2], fp2[2];
	int a;
	short *p1, *p2;
	
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
static int lasso_inside_edge(short mcords[][2], short moves, int x0, int y0, int x1, int y1)
{
	short v1[2], v2[2];
	int a;

	v1[0] = x0, v1[1] = y0;
	v2[0] = x1, v2[1] = y1;

	// check points in lasso
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
				
				if(select) base->flag |= SELECT;
				else base->flag &= ~SELECT;
				base->object->flag= base->flag;
			}
			if(base->object->flag & OB_POSEMODE) {
				do_lasso_select_pose(base->object, mcords, moves, select);
			}
		}
	}
}

static void lasso_select_boundbox(rcti *rect, short mcords[][2], short moves)
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

static void do_lasso_select_curve__doSelect(void *userData, Nurb *nu, BPoint *bp, BezTriple *bezt, int beztindex, int x, int y)
{
	struct { short (*mcords)[2]; short moves; short select; } *data = userData;

	if (lasso_inside(data->mcords, data->moves, x, y)) {
		if (bp) {
			bp->f1 = data->select?(bp->f1|1):(bp->f1&~1);
		} else {
			if (beztindex==0) {
				bezt->f1 = data->select?(bezt->f1|1):(bezt->f1&~1);
			} else if (beztindex==1) {
				bezt->f2 = data->select?(bezt->f2|1):(bezt->f2&~1);
			} else {
				bezt->f3 = data->select?(bezt->f3|1):(bezt->f3&~1);
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
		bp->f1 = data->select?(bp->f1|1):(bp->f1&~1);
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
	countall();	// abused for flushing selection
}

static void do_lasso_select_facemode(short mcords[][2], short moves, short select)
{
	Mesh *me;
	rcti rect;
	
	me= get_mesh(OBACT);
	if(me==NULL || me->tface==NULL) return;
	if(me->totface==0) return;
	
	em_vertoffs= me->totface+1;	// max index array
	
	lasso_select_boundbox(&rect, mcords, moves);
	EM_mask_init_backbuf_border(mcords, moves, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
	
	EM_backbuf_checkAndSelectTFaces(me, select);
	
	EM_free_backbuf();
	
	object_tface_flags_changed(OBACT, 0);
}

static void do_lasso_select(short mcords[][2], short moves, short select)
{
	if(G.obedit==NULL) {
		if(G.f & G_FACESELECT)
			do_lasso_select_facemode(mcords, moves, select);
		else if(G.f & (G_VERTEXPAINT|G_TEXTUREPAINT|G_WEIGHTPAINT))
			;
		else  
			do_lasso_select_objects(mcords, moves, select);
	}
	else if(G.obedit->type==OB_MESH) 
		do_lasso_select_mesh(mcords, moves, select);
	else if(G.obedit->type==OB_CURVE || G.obedit->type==OB_SURF) 
		do_lasso_select_curve(mcords, moves, select);
	else if(G.obedit->type==OB_LATTICE) 
		do_lasso_select_lattice(mcords, moves, select);
	else if(G.obedit->type==OB_ARMATURE)
		do_lasso_select_armature(mcords, moves, select);
	
	BIF_undo_push("Lasso select");

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
	 * starting from end points, calculate centre with maximum distance
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
	short mcords[MOVES_LASSO][2]; // the larger size
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
					glFlush();
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
	extern float zfac;	/* view.c */
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

		dx= ((float)(mx-(curarea->winx/2)))*zfac/(curarea->winx/2);
		dy= ((float)(my-(curarea->winy/2)))*zfac/(curarea->winy/2);
		
		fz= G.vd->persmat[0][3]*fp[0]+ G.vd->persmat[1][3]*fp[1]+ G.vd->persmat[2][3]*fp[2]+ G.vd->persmat[3][3];
		fz= fz/zfac;
		
		fp[0]= (G.vd->persinv[0][0]*dx + G.vd->persinv[1][0]*dy+ G.vd->persinv[2][0]*fz)-G.vd->ofs[0];
		fp[1]= (G.vd->persinv[0][1]*dx + G.vd->persinv[1][1]*dy+ G.vd->persinv[2][1]*fz)-G.vd->ofs[1];
		fp[2]= (G.vd->persinv[0][2]*dx + G.vd->persinv[1][2]*dy+ G.vd->persinv[2][2]*fz)-G.vd->ofs[2];
	}
	
	allqueue(REDRAWVIEW3D, 1);
	
	if(lr_click) {
		if(G.obedit->type==OB_MESH) addvert_mesh();
		else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) addvert_Nurb(0);
		else if (G.obedit->type==OB_ARMATURE) addvert_armature();
		VECCOPY(fp, oldcurs);
	}
	
}

void deselectall(void)	/* is toggle */
{
	Base *base;
	int a=0;

	base= FIRSTBASE;
	while(base) {
		if TESTBASE(base) {
			a= 1;
			break;
		}
		base= base->next;
	}
	
	base= FIRSTBASE;
	while(base) {
		if(base->lay & G.vd->lay) {
			if(a) base->flag &= ~SELECT;
			else base->flag |= SELECT;
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

/* selects all objects of a particular type, on currently visible layers */
void selectall_type(short obtype) 
{
	Base *base;
	
	base= FIRSTBASE;
	while(base) {
		if((base->lay & G.vd->lay) && (base->object->type == obtype)) {
			base->flag |= SELECT;
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
		if (base->lay == (1<< (layernum -1))) {
			base->flag |= SELECT;
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
				base->flag &= ~SELECT;
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

#define SELECTSIZE	51

void set_active_base(Base *base)
{
	
	BASACT= base;
	
	if(base) {
		/* signals to buttons */
		redraw_test_buttons(base->object);

		set_active_group();
		
		/* signal to ipo */

		if (base) {
			allqueue(REDRAWIPO, base->object->ipowin);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
		}
	}
}

void set_active_object(Object *ob)
{
	Base *base;
	
	base= FIRSTBASE;
	while(base) {
		if(base->object==ob) {
			set_active_base(base);
			return;
		}
		base= base->next;
	}
}

/* The max number of menu items in an object select menu */
#define SEL_MENU_SIZE 22

static Base *mouse_select_menu(unsigned int *buffer, int hits, short *mval)
{
	Base *baseList[SEL_MENU_SIZE]={NULL}; /*baseList is used to store all possible bases to bring up a menu */
	Base *base;
	short baseCount = 0;
	char menuText[20 + SEL_MENU_SIZE*32] = "Select Object%t";	// max ob name = 22
	char str[32];
	
	for(base=FIRSTBASE; base; base= base->next) {
		if(base->lay & G.vd->lay) {
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
					sprintf(str, "|%s %%x%d", base->object->id.name+2, baseCount+1);	// max ob name == 22
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
	if(hits15) {
		for(a=0; a<hits15; a++) if(buffer[4*a+3] & 0xFFFF0000) has_bones15= 1;
		
		offs= 4*hits15;
		hits9= view3d_opengl_select(buffer+offs, MAXPICKBUF-offs, mval[0]-9, mval[1]-9, mval[0]+9, mval[1]+9);
		if(hits9) {
			for(a=0; a<hits9; a++) if(buffer[offs+4*a+3] & 0xFFFF0000) has_bones9= 1;
			
			offs+= 4*hits9;
			hits5= view3d_opengl_select(buffer+offs, MAXPICKBUF-offs, mval[0]-5, mval[1]-5, mval[0]+5, mval[1]+5);
			if(hits5) {
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
		
		if(hits5) {
			offs= 4*hits15 + 4*hits9;
			memcpy(buffer, buffer+offs, 4*offs);
			return hits5;
		}
		if(hits9) {
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
	unsigned int buffer[MAXPICKBUF];
	int temp, a, dist=100;
	short hits, mval[2];

	/* always start list from basact in wire mode */
	startbase=  FIRSTBASE;
	if(BASACT && BASACT->next) startbase= BASACT->next;

	getmouseco_areawin(mval);
	
	/* This block uses the control key to make the object selected by its centre point rather then its contents */
	if(G.obedit==0 && (G.qual & LR_CTRLKEY)) {
		
		if(G.qual & LR_ALTKEY) basact= mouse_select_menu(NULL, 0, mval);
		else {
			base= startbase;
			while(base) {
				
				if(base->lay & G.vd->lay) {
					
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

			if(has_bones==0 && (G.qual & LR_ALTKEY)) 
				basact= mouse_select_menu(buffer, hits, mval);
			else {
				static short lastmval[2]={-100, -100};
				int donearest= 0;
				
				/* define if we use solid nearest select or not */
				if(G.vd->drawtype>OB_WIRE) {
					donearest= 1;
					if( ABS(mval[0]-lastmval[0])<3 && ABS(mval[1]-lastmval[1])<3) {
						if(!has_bones)	// hrms, if theres bones we always do nearest
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
				if( do_pose_selectbuffer(basact, buffer, hits) ) {	// then bone is found
				
					/* in weightpaint, we use selected bone to select vertexgroup, so no switch to new active object */
					if(G.f & G_WEIGHTPAINT) {
						/* we make the armature selected */
						basact->flag|= SELECT;
						basact->object->flag= basact->flag;
						/* prevent activating */
						basact= NULL;
					}
				}
			}
		}
	}
	
	/* so, do we have something selected? */
	if(basact) {
		
		if(G.obedit) {
			/* only do select */
			deselectall_except(basact);
			basact->flag |= SELECT;
		}
		else {
			oldbasact= BASACT;
			BASACT= basact;
			
			if((G.qual & LR_SHIFTKEY)==0) {
				deselectall_except(basact);
				basact->flag |= SELECT;
			}
			else {
				if(basact->flag & SELECT) {
					if(basact==oldbasact)
						basact->flag &= ~SELECT;
				}
				else basact->flag |= SELECT;
			}

			// copy
			basact->object->flag= basact->flag;
			
			if(oldbasact != basact) {
				set_active_base(basact);
			}

			// for visual speed, only in wire mode
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
			
			/* selecting a non-mesh, should end a couple of modes... */
			if(basact->object->type!=OB_MESH) {
				if(G.f & G_WEIGHTPAINT) {
					set_wpaint();	/* toggle */
				}
				if(G.f & G_VERTEXPAINT) {
					set_vpaint();	/* toggle */
				}
				if(G.f & G_FACESELECT) {
					set_faceselect();	/* toggle */
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

	rightmouse_transform();	// does undo push!
}

/* ------------------------------------------------------------------------- */

static int edge_inside_circle(short centx, short centy, short rad, short x1, short y1, short x2, short y2)
{
	int radsq= rad*rad;
	float v1[2], v2[2], v3[2];
	
	// check points in circle itself
	if( (x1-centx)*(x1-centx) + (y1-centy)*(y1-centy) <= radsq ) return 1;
	if( (x2-centx)*(x2-centx) + (y2-centy)*(y2-centy) <= radsq ) return 1;
	
	// pointdistline
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
			bp->f1 = data->select?(bp->f1|1):(bp->f1&~1);
		} else {
			if (beztindex==0) {
				bezt->f1 = data->select?(bezt->f1|1):(bezt->f1&~1);
			} else if (beztindex==1) {
				bezt->f2 = data->select?(bezt->f2|1):(bezt->f2&~1);
			} else {
				bezt->f3 = data->select?(bezt->f3|1):(bezt->f3&~1);
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
		bp->f1 = data->select?(bp->f1|1):(bp->f1&~1);
	}
}
static void do_lattice_box_select(rcti *rect, int select)
{
	struct { rcti *rect; short select, pass, done; } data;

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
	unsigned int buffer[MAXPICKBUF];
	int a, index;
	short hits, val;

	if(G.obedit==NULL && (G.f & G_FACESELECT)) {
		face_borderselect();
		return;
	}
	
	setlinestyle(2);
	val= get_border(&rect, 3);
	setlinestyle(0);
	
	if(val==0)
		return;
	
	if(G.obedit) {
		if(G.obedit->type==OB_MESH) {
			do_mesh_box_select(&rect, (val==LEFTMOUSE));
			allqueue(REDRAWVIEW3D, 0);
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
	else {	// no editmode, unified for bones and objects
		Bone *bone;
		unsigned int *vbuffer=NULL; /* selection buffer	*/
		unsigned int *col;			/* color in buffer	*/
		short selecting = 0;

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

		if (hits) { /* no need to loop if there's no hit */
			base= FIRSTBASE;
			col = vbuffer + 3;
			while(base && hits) {
				Base *next = base->next;
				if(base->lay & G.vd->lay) {
					while (base->selcol == (*col & 0xFFFF)) {	// we got an object
						
						if(*col & 0xFFFF0000) {					// we got a bone
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
						else {
							if (selecting)
								base->flag |= SELECT;
							else
								base->flag &= ~SELECT;

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
		glFlush();

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

	if(!G.obedit && (G.f&G_FACESELECT)) {
		Mesh *me = get_mesh(OBACT);

		if (me) {
			em_vertoffs= me->totface+1;	// max index array

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
			bp->f1 = data->select?(bp->f1|1):(bp->f1&~1);
		} else {
			if (beztindex==0) {
				bezt->f1 = data->select?(bezt->f1|1):(bezt->f1&~1);
			} else if (beztindex==1) {
				bezt->f2 = data->select?(bezt->f2|1):(bezt->f2&~1);
			} else {
				bezt->f3 = data->select?(bezt->f3|1):(bezt->f3&~1);
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
		bp->f1 = data->select?(bp->f1|1):(bp->f1&~1);
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

	if(G.vd->persp!=2) return;
	
	val= get_border(&rect, 2);
	if(val) {
		rcti vb;

		calc_viewborder(G.vd, &vb);

		G.scene->r.border.xmin= (float) (rect.xmin-vb.xmin)/(vb.xmax-vb.xmin);
		G.scene->r.border.ymin= (float) (rect.ymin-vb.ymin)/(vb.ymax-vb.ymin);
		G.scene->r.border.xmax= (float) (rect.xmax-vb.xmin)/(vb.xmax-vb.xmin);
		G.scene->r.border.ymax= (float) (rect.ymax-vb.ymin)/(vb.ymax-vb.ymin);
		
		CLAMP(G.scene->r.border.xmin, 0.0, 1.0);
		CLAMP(G.scene->r.border.ymin, 0.0, 1.0);
		CLAMP(G.scene->r.border.xmax, 0.0, 1.0);
		CLAMP(G.scene->r.border.ymax, 0.0, 1.0);
		
		allqueue(REDRAWVIEWCAM, 1);
		// if it was not set, we do this
		G.scene->r.mode |= R_BORDER;
		allqueue(REDRAWBUTSSCENE, 1);
	}
}



void fly(void)
{
	float speed=0.0, speedo=1.0, zspeed=0.0, dvec[3], *quat, mat[3][3];
	float oldvec[3], oldrot[3];
	int loop=1;
	unsigned short toets;
	short val, cent[2];
	short mval[2];
	
	if(curarea->spacetype!=SPACE_VIEW3D) return;
	if(G.vd->camera == 0) return;
	if(G.vd->persp<2) return;
	
	VECCOPY(oldvec, G.vd->camera->loc);
	VECCOPY(oldrot, G.vd->camera->rot);
	
	cent[0]= curarea->winrct.xmin+(curarea->winx)/2;
	cent[1]= curarea->winrct.ymin+(curarea->winy)/2;
	
	warp_pointer(cent[0], cent[1]);
	
	/* we have to rely on events to give proper mousecoords after a warp_pointer */
	mval[0]= cent[0]=  (curarea->winx)/2;
	mval[1]= cent[1]=  (curarea->winy)/2;
	
	headerprint("Fly");
	
	while(loop) {
		

		while(qtest()) {
			
			toets= extern_qread(&val);
			
			if(val) {
				if(toets==MOUSEY) getmouseco_areawin(mval);
				else if(toets==ESCKEY) {
					VECCOPY(G.vd->camera->loc, oldvec);
					VECCOPY(G.vd->camera->rot, oldrot);
					loop= 0;
					break;
				}
				else if(toets==SPACEKEY) {
					loop= 0;
					BIF_undo_push("Fly camera");
					break;
				}
				else if(toets==LEFTMOUSE) {
					speed+= G.vd->grid/75.0;
					if(get_mbut()&M_MOUSE) speed= 0.0;
				}
				else if(toets==MIDDLEMOUSE) {
					speed-= G.vd->grid/75.0;
					if(get_mbut()&L_MOUSE) speed= 0.0;
				}
			}
		}
		if(loop==0) break;
		
		/* define dvec */
		val= mval[0]-cent[0];
		if(val>20) val-= 20; else if(val< -20) val+= 20; else val= 0;
		dvec[0]= 0.000001*val*val;
		if(val>0) dvec[0]= -dvec[0];
		
		val= mval[1]-cent[1];
		if(val>20) val-= 20; else if(val< -20) val+= 20; else val= 0;
		dvec[1]= 0.000001*val*val;
		if(val>0) dvec[1]= -dvec[1];
		
		dvec[2]= 1.0;
		
		zspeed= 0.0;
		if(get_qual()&LR_CTRLKEY) zspeed= -G.vd->grid/25.0;
		if(get_qual()&LR_ALTKEY) zspeed= G.vd->grid/25.0;
		
		if(speedo!=0.0 || zspeed!=0.0 || dvec[0]!=0.0 || dvec[1]!=0.0) {
		
			Normalise(dvec);
			
			Mat3CpyMat4(mat, G.vd->viewinv);
			Mat3MulVecfl(mat, dvec);
			quat= vectoquat(dvec, 5, 1);	/* track and upflag, not from the base: camera view calculation does not use that */
			
			QuatToEul(quat, G.vd->camera->rot);
			
			compatible_eul(G.vd->camera->rot, oldrot);
			
			VecMulf(dvec, speed);
			G.vd->camera->loc[0]-= dvec[0];
			G.vd->camera->loc[1]-= dvec[1];
			G.vd->camera->loc[2]-= (dvec[2]-zspeed);
			
			scrarea_do_windraw(curarea);
			screen_swapbuffers();
		}
		speedo= speed;
	}
	
	allqueue(REDRAWVIEW3D, 0);
	scrarea_queue_headredraw(curarea);
	
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
		v3d->clipbb= MEM_mallocN(sizeof(BoundBox), "clipbb");
		
		/* convert border to 3d coordinates */
		
		/* Get the matrices needed for gluUnProject */
		glGetIntegerv(GL_VIEWPORT, viewport);
		glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix);
		glGetDoublev(GL_PROJECTION_MATRIX, projmatrix);
		
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

