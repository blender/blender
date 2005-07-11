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
 */

#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"
#include "DNA_constraint_types.h"

#include "BIF_screen.h"
#include "BIF_resources.h"
#include "BIF_mywindow.h"
#include "BIF_gl.h"
#include "BIF_editarmature.h"
#include "BIF_editmesh.h"

#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_lattice.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BSE_view.h"

#include "BLI_arithb.h"

#include "blendef.h"

#include "mydevice.h"

#include "transform.h"

extern ListBase editNurb;
extern ListBase editelems;

extern TransInfo Trans;	/* From transform.c */

/* ************************** Functions *************************** */


void getViewVector(float coord[3], float vec[3]) {
	if (G.vd->persp)
	{
		float p1[4], p2[4];

		VECCOPY(p1, coord);
		p1[3] = 1.0f;
		VECCOPY(p2, p1);
		p2[3] = 1.0f;
		Mat4MulVec4fl(G.vd->viewmat, p2);

		p2[0] = 2.0f * p2[0];
		p2[1] = 2.0f * p2[1];
		p2[2] = 2.0f * p2[2];

		Mat4MulVec4fl(G.vd->viewinv, p2);

		VecSubf(vec, p1, p2);
	}
	else {
		VECCOPY(vec, G.vd->viewinv[2]);
	}
	Normalise(vec);
}

/* ************************** GENERICS **************************** */

/* called for objects updating while transform acts, once per redraw */
void recalcData(TransInfo *t)
{
	Base *base;
	
	if(G.obpose) {
		/* old optimize trick... this enforces to bypass the depgraph */
		if (!is_delay_deform()) 
			DAG_object_flush_update(G.scene, G.obpose, OB_RECALC_DATA);  /* sets recalc flags */
		else
			where_is_pose(G.obpose);
	}
	else if (G.obedit) {
		
		if (G.obedit->type == OB_MESH) {
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);  /* sets recalc flags */
			
			recalc_editnormals();
		}
		else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {
			Nurb *nu= editNurb.first;
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);  /* sets recalc flags */
			
			while(nu) {
				test2DNurb(nu);
				testhandlesNurb(nu); /* test for bezier too */
				nu= nu->next;
			}
		}
		else if(G.obedit->type==OB_ARMATURE){   /* no recalc flag, does pose */
			EditBone *ebo;
			
			/* Ensure all bones are correctly adjusted */
			for (ebo=G.edbo.first; ebo; ebo=ebo->next){
				
				if ((ebo->flag & BONE_IK_TOPARENT) && ebo->parent){
					/* If this bone has a parent tip that has been moved */
					if (ebo->parent->flag & BONE_TIPSEL){
						VECCOPY (ebo->head, ebo->parent->tail);
					}
					/* If this bone has a parent tip that has NOT been moved */
					else{
						VECCOPY (ebo->parent->tail, ebo->head);
					}
				}
			}
		}
		else if(G.obedit->type==OB_LATTICE) {
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);  /* sets recalc flags */
			
			if(editLatt->flag & LT_OUTSIDE) outside_lattice(editLatt);
		}
		else {
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);  /* sets recalc flags */
		}
	}
	else {
		
		base= FIRSTBASE;
		while(base) {
			/* this flag is from depgraph, was stored in nitialize phase, handled in drawview.c */
			if(base->flag & BA_HAS_RECALC_OB)
				base->object->recalc |= OB_RECALC_OB;
			if(base->flag & BA_HAS_RECALC_DATA)
				base->object->recalc |= OB_RECALC_DATA;
			
			/* thanks to ob->ctime usage, ipos are not called in where_is_object,
			   unless we edit ipokeys */
			if(base->flag & BA_DO_IPO) {
				if(base->object->ipo) {
					IpoCurve *icu;
					
					base->object->ctime= -1234567.0;
					
					icu= base->object->ipo->curve.first;
					while(icu) {
						calchandles_ipocurve(icu);
						icu= icu->next;
					}
				}				
			}
			
			base= base->next;
		} 
	}
	
	/* update shaded drawmode while transform */
	if(G.vd->drawtype == OB_SHADED) reshadeall_displist();
	
}

void initTransModeFlags(TransInfo *t, int mode) 
{
	t->mode = mode;
	t->num.flag = 0;

	/* REMOVING RESTRICTIONS FLAGS */
	t->flag &= ~T_ALL_RESTRICTIONS;
	
	switch (mode) {
	case TFM_RESIZE:
		t->flag |= T_NULL_ONE;
		t->num.flag |= NUM_NULL_ONE;
		t->num.flag |= NUM_AFFECT_ALL;
		if (!G.obedit) {
			t->flag |= T_NO_ZERO;
			t->num.flag |= NUM_NO_ZERO;
		}
		break;
	case TFM_TOSPHERE:
		t->num.flag |= NUM_NULL_ONE;
		t->num.flag |= NUM_NO_NEGATIVE;
		t->flag |= T_NO_CONSTRAINT;
		break;
	case TFM_SHEAR:
		t->flag |= T_NO_CONSTRAINT;
		break;
	case TFM_CREASE:
		t->flag |= T_NO_CONSTRAINT;
		break;
	}
}

void drawLine(float *center, float *dir, char axis, short options)
{
	extern void make_axis_color(char *col, char *col2, char axis);	// drawview.c
	float v1[3], v2[3], v3[3];
	char col[3], col2[3];
	
	//if(G.obedit) mymultmatrix(G.obedit->obmat);	// sets opengl viewing

	VecCopyf(v3, dir);
	VecMulf(v3, G.vd->far);
	
	VecSubf(v2, center, v3);
	VecAddf(v1, center, v3);

	if (options & DRAWLIGHT) {
		col[0] = col[1] = col[2] = 220;
	}
	else {
		BIF_GetThemeColor3ubv(TH_GRID, col);
	}
	make_axis_color(col, col2, axis);
	glColor3ubv(col2);

	setlinestyle(0);
	glBegin(GL_LINE_STRIP); 
		glVertex3fv(v1); 
		glVertex3fv(v2); 
	glEnd();
	
	myloadmatrix(G.vd->viewmat);
}

void initTrans (TransInfo *t)
{

	/* moving: is shown in drawobject() (transform color) */
	if(G.obedit || G.obpose) G.moving= G_TRANSFORM_EDIT;
	else G.moving= G_TRANSFORM_OBJ;

	t->data = NULL;
	t->ext = NULL;

	t->flag = 0;

	/* setting PET flag */
	if ((t->context & CTX_NO_PET) == 0 && (G.scene->proportional)) {
		t->flag |= T_PROP_EDIT;
		if(G.scene->proportional==2) t->flag |= T_PROP_CONNECTED;	// yes i know, has to become define
	}

	getmouseco_areawin(t->imval);
	t->con.imval[0] = t->imval[0];
	t->con.imval[1] = t->imval[1];

	t->transform		= NULL;

	t->total			=
		t->num.idx		=
		t->num.idx_max	=
		t->num.ctrl[0]	= 
		t->num.ctrl[1]	= 
		t->num.ctrl[2]	= 0;

	t->val = 0.0f;

	t->num.val[0]		= 
		t->num.val[1]	= 
		t->num.val[2]	= 0.0f;

	t->vec[0]			=
		t->vec[1]		=
		t->vec[2]		= 0.0f;
	
	Mat3One(t->mat);
	
	Mat4CpyMat4(t->viewmat, G.vd->viewmat);
	Mat4CpyMat4(t->viewinv, G.vd->viewinv);
	Mat4CpyMat4(t->persinv, G.vd->persinv);
}

/* Here I would suggest only TransInfo related issues, like free data & reset vars. Not redraws */
void postTrans (TransInfo *t) 
{
	G.moving = 0; // Set moving flag off (display as usual)

	stopConstraint(t);
	/* Not needed anymore but will keep there in case it will be
	t->con.drawExtra = NULL;
	t->con.applyVec	= NULL;
	t->con.applySize= NULL;
	t->con.applyRot	= NULL;
	t->con.mode		= 0;
	*/

	
	/* postTrans can be called when nothing is selected, so data is NULL already */
	if (t->data) {
		TransData *td;
		int a;

		/* since ipokeys are optional on objects, we mallocced them per trans-data */
		for(a=0, td= t->data; a<t->total; a++, td++) {
			if(td->tdi) MEM_freeN(td->tdi);
		}
		MEM_freeN(t->data);
	}

	if (t->ext) MEM_freeN(t->ext);
	
}

static void apply_grid3(float *val, int max_index, float fac1, float fac2, float fac3)
{
	/* fac1 is for 'nothing', fac2 for CTRL, fac3 for SHIFT */
	int invert = U.flag & USER_AUTOGRABGRID;
	int ctrl;
	int i;

	for (i=0; i<=max_index; i++) {

		if(invert) {
			if(G.qual & LR_CTRLKEY) ctrl= 0;
			else ctrl= 1;
		}
		else ctrl= (G.qual & LR_CTRLKEY);

		if(ctrl && (G.qual & LR_SHIFTKEY)) {
			if(fac3!= 0.0) {
				for (i=0; i<=max_index; i++) {
					val[i]= fac3*(float)floor(val[i]/fac3 +.5);
				}
			}
		}
		else if(ctrl) {
			if(fac2!= 0.0) {
				for (i=0; i<=max_index; i++) {
					val[i]= fac2*(float)floor(val[i]/fac2 +.5);
				}
			}
		}
		else {
			if(fac1!= 0.0) {
				for (i=0; i<=max_index; i++) {
					val[i]= fac1*(float)floor(val[i]/fac1 +.5);
				}
			}
		}
	}
}

void snapGrid(TransInfo *t, float *val) {
	apply_grid3(val, t->idx_max, t->snap[0], t->snap[1], t->snap[2]);
}

void applyTransObjects(TransInfo *t)
{
	TransData *td;
	
	for (td = t->data; td < t->data + t->total; td++) {
		VECCOPY(td->iloc, td->loc);
		if (td->ext->rot) {
			VECCOPY(td->ext->irot, td->ext->rot);
		}
		if (td->ext->size) {
			VECCOPY(td->ext->isize, td->ext->size);
		}
	}	
	recalcData(t);
} 

/* helper for below */
static void restore_ipokey(float *poin, float *old)
{
	if(poin) {
		poin[0]= old[0];
		poin[-3]= old[3];
		poin[3]= old[6];
	}
}

static void restoreElement(TransData *td) {
	/* TransData for crease has no loc */
	if (td->loc) {
		VECCOPY(td->loc, td->iloc);
	}
	if (td->val) {
		*td->val = td->ival;
	}
	if (td->ext) {
		if (td->ext->rot) {
			VECCOPY(td->ext->rot, td->ext->irot);
		}
		if (td->ext->size) {
			VECCOPY(td->ext->size, td->ext->isize);
		}
		if(td->flag & TD_USEQUAT) {
			if (td->ext->quat) {
				QUATCOPY(td->ext->quat, td->ext->iquat);
			}
		}
	}
	if(td->tdi) {
		TransDataIpokey *tdi= td->tdi;
		
		restore_ipokey(tdi->locx, tdi->oldloc);
		restore_ipokey(tdi->locy, tdi->oldloc+1);
		restore_ipokey(tdi->locz, tdi->oldloc+2);

		restore_ipokey(tdi->rotx, tdi->oldrot);
		restore_ipokey(tdi->roty, tdi->oldrot+1);
		restore_ipokey(tdi->rotz, tdi->oldrot+2);
		
		restore_ipokey(tdi->sizex, tdi->oldsize);
		restore_ipokey(tdi->sizey, tdi->oldsize+1);
		restore_ipokey(tdi->sizez, tdi->oldsize+2);
	}
}

void restoreTransObjects(TransInfo *t)
{
	TransData *td;
	
	for (td = t->data; td < t->data + t->total; td++) {
		restoreElement(td);
	}	
	recalcData(t);
} 

void calculateCenterCursor(TransInfo *t)
{
	float *cursor;

	cursor = give_cursor();
	VECCOPY(t->center, cursor);

	if(t->flag & (T_EDIT|T_POSE)) {
		Object *ob= G.obedit?G.obedit:G.obpose;
		float mat[3][3], imat[3][3];
		float vec[3];
		
		VecSubf(t->center, t->center, ob->obmat[3]);
		Mat3CpyMat4(mat, ob->obmat);
		Mat3Inv(imat, mat);
		Mat3MulVecfl(imat, t->center);
		
		VECCOPY(vec, t->center);
		Mat4MulVecfl(ob->obmat, vec);
		project_int(vec, t->center2d);
	}
	else {
		project_int(t->center, t->center2d);
	}
}

void calculateCenterMedian(TransInfo *t)
{
	float partial[3] = {0.0f, 0.0f, 0.0f};
	int i;
	for(i = 0; i < t->total; i++) {
		if (t->data[i].flag & TD_SELECTED) {
			VecAddf(partial, partial, t->data[i].center);
		}
		else {
			/* 
			   All the selected elements are at the head of the array 
			   which means we can stop when it finds unselected data
			*/
			break;
		}
	}
	VecMulf(partial, 1.0f / i);
	VECCOPY(t->center, partial);

	if (t->flag & (T_EDIT|T_POSE)) {
		Object *ob= G.obedit?G.obedit:G.obpose;
		float vec[3];
		
		VECCOPY(vec, t->center);
		Mat4MulVecfl(ob->obmat, vec);
		project_int(vec, t->center2d);
	}
	else {
		project_int(t->center, t->center2d);
	}
}

void calculateCenterBound(TransInfo *t)
{
	float max[3];
	float min[3];
	int i;
	for(i = 0; i < t->total; i++) {
		if (i) {
			if (t->data[i].flag & TD_SELECTED) {
				MinMax3(min, max, t->data[i].center);
			}
			else {
				/* 
				   All the selected elements are at the head of the array 
				   which means we can stop when it finds unselected data
				*/
				break;
			}
		}
		else {
			VECCOPY(max, t->data[i].center);
			VECCOPY(min, t->data[i].center);
		}
	}
	VecAddf(t->center, min, max);
	VecMulf(t->center, 0.5);

	if (t->flag & (T_EDIT|T_POSE)) {
		Object *ob= G.obedit?G.obedit:G.obpose;
		float vec[3];
		
		VECCOPY(vec, t->center);
		Mat4MulVecfl(ob->obmat, vec);
		project_int(vec, t->center2d);
	}
	else {
		project_int(t->center, t->center2d);
	}
}

void calculateCenter(TransInfo *t) 
{
	switch(G.vd->around) {
	case V3D_CENTRE:
		calculateCenterBound(t);
		break;
	case V3D_CENTROID:
		calculateCenterMedian(t);
		break;
	case V3D_CURSOR:
		calculateCenterCursor(t);
		break;
	case V3D_LOCAL:
		/* Individual element center uses median center for helpline and such */
		calculateCenterMedian(t);
		break;
	case V3D_ACTIVE:
		/* set median, and if if if... do object center */
		calculateCenterMedian(t);
		if((t->flag & (T_EDIT|T_POSE))==0) {
			Object *ob= OBACT;
			if(ob) {
				VECCOPY(t->center, ob->obmat[3]);
				project_int(t->center, t->center2d);
			}
		}
		
	}

	/* setting constraint center */
	VECCOPY(t->con.center, t->center);
	if(t->flag & (T_EDIT|T_POSE)) {
		Object *ob= G.obedit?G.obedit:G.obpose;
		Mat4MulVecfl(ob->obmat, t->con.center);
	}

	/* voor panning from cameraview */
	if(t->flag & T_OBJECT) {
		if( G.vd->camera==OBACT && G.vd->persp>1) {
			float axis[3];
			/* persinv is nasty, use viewinv instead, always right */
			VECCOPY(axis, G.vd->viewinv[2]);
			Normalise(axis);

			/* 6.0 = 6 grid units */
			axis[0]= t->center[0]- 6.0f*axis[0];
			axis[1]= t->center[1]- 6.0f*axis[1];
			axis[2]= t->center[2]- 6.0f*axis[2];
			
			project_int(axis, t->center2d);
			
			/* rotate only needs correct 2d center, grab needs initgrabz() value */
			if(t->mode==TFM_TRANSLATION) VECCOPY(t->center, axis);
		}
	}	
	initgrabz(t->center[0], t->center[1], t->center[2]);
}

void calculatePropRatio(TransInfo *t)
{
	TransData *td = t->data;
	int i;
	float dist;
	short connected = t->flag & T_PROP_CONNECTED;

	if (t->flag & T_PROP_EDIT) {
		for(i = 0 ; i < t->total; i++, td++) {
			if (td->flag & TD_SELECTED) {
				td->factor = 1.0f;
			}
			else if	((connected && 
						(td->flag & TD_NOTCONNECTED || td->dist > t->propsize))
				||
					(connected == 0 &&
						td->rdist > t->propsize)) {
				/* 
				   The elements are sorted according to their dist member in the array,
				   that means we can stop when it finds one element outside of the propsize.
				*/
				td->flag |= TD_NOACTION;
				td->factor = 0.0f;
				restoreElement(td);
			}
			else {
				/* Use rdist for falloff calculations, it is the real distance */
				td->flag &= ~TD_NOACTION;
				dist= (t->propsize-td->rdist)/t->propsize;
				switch(G.scene->prop_mode) {
				case PROP_SHARP:
					td->factor= dist*dist;
					break;
				case PROP_SMOOTH:
					td->factor= 3.0f*dist*dist - 2.0f*dist*dist*dist;
					break;
				case PROP_ROOT:
					td->factor = (float)sqrt(dist);
					break;
				case PROP_LIN:
					td->factor = dist;
					break;
				case PROP_CONST:
					td->factor = 1.0f;
					break;
				case PROP_SPHERE:
					td->factor = (float)sqrt(2*dist - dist * dist);
					break;
				default:
					td->factor = 1;
				}
			}
		}
		switch(G.scene->prop_mode) {
		case PROP_SHARP:
			strcpy(t->proptext, "(Sharp)");
			break;
		case PROP_SMOOTH:
			strcpy(t->proptext, "(Smooth)");
			break;
		case PROP_ROOT:
			strcpy(t->proptext, "(Root)");
			break;
		case PROP_LIN:
			strcpy(t->proptext, "(Linear)");
			break;
		case PROP_CONST:
			strcpy(t->proptext, "(Constant)");
			break;
		case PROP_SPHERE:
			strcpy(t->proptext, "(Sphere)");
			break;
		default:
			strcpy(t->proptext, "");
		}
	}
	else {
		for(i = 0 ; i < t->total; i++, td++) {
			td->factor = 1.0;
		}
		strcpy(t->proptext, "");
	}
}

TransInfo * BIF_GetTransInfo() {
	return &Trans;
}

