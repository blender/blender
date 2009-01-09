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
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "RE_pipeline.h"	// make_stars

#include "BIF_gl.h"
#include "BIF_retopo.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "PIL_time.h" /* smoothview */

#include "view3d_intern.h"	// own include

/* ********************** view3d_edit: view manipulations ********************* */

/* ************************** init for view ops **********************************/

typedef struct ViewOpsData {
	ARegion *ar;
	View3D *v3d;

	float oldquat[4];
	float trackvec[3];
	float ofs[3], obofs[3];
	float reverse, dist0;

	int origx, origy, oldx, oldy;
	int origkey;

	void *vh; // XXX temp
} ViewOpsData;

#define TRACKBALLSIZE  (1.1)

static void calctrackballvec(rcti *rect, int mx, int my, float *vec)
{
	float x, y, radius, d, z, t;

	radius= TRACKBALLSIZE;

	/* normalize x and y */
	x= (rect->xmax + rect->xmin)/2 - mx;
	x/= (float)((rect->xmax - rect->xmin)/4);
	y= (rect->ymax + rect->ymin)/2 - my;
	y/= (float)((rect->ymax - rect->ymin)/2);

	d = sqrt(x*x + y*y);
	if (d < radius*M_SQRT1_2)  	/* Inside sphere */
		z = sqrt(radius*radius - d*d);
	else
	{ 			/* On hyperbola */
		t = radius / M_SQRT2;
		z = t*t / d;
	}

	vec[0]= x;
	vec[1]= y;
	vec[2]= -z;		/* yah yah! */
}


static void viewops_data(bContext *C, wmOperator *op, wmEvent *event)
{
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	ViewOpsData *vod= MEM_callocN(sizeof(ViewOpsData), "viewops data");

	/* store data */
	op->customdata= vod;
	vod->ar= CTX_wm_region(C);
	vod->v3d= v3d;
	vod->dist0= v3d->dist;
	QUATCOPY(vod->oldquat, v3d->viewquat);
	vod->origx= vod->oldx= event->x;
	vod->origy= vod->oldy= event->y;
	vod->origkey= event->type;

	calctrackballvec(&vod->ar->winrct, event->x, event->y, vod->trackvec);

	initgrabz(v3d, -v3d->ofs[0], -v3d->ofs[1], -v3d->ofs[2]);

	vod->reverse= 1.0f;
	if (v3d->persmat[2][1] < 0.0f)
		vod->reverse= -1.0f;

}

/* ************************** viewrotate **********************************/

static const float thres = 0.93f; //cos(20 deg);

#define COS45 0.70710678118654746
#define SIN45 COS45

static float snapquats[39][6] = {
	/*{q0, q1, q3, q4, view, oposite_direction}*/
{COS45, -SIN45, 0.0, 0.0, 1, 0},  //front
{0.0, 0.0, -SIN45, -SIN45, 1, 1}, //back
{1.0, 0.0, 0.0, 0.0, 7, 0},       //top
{0.0, -1.0, 0.0, 0.0, 7, 1},      //bottom
{0.5, -0.5, -0.5, -0.5, 3, 0},    //left
{0.5, -0.5, 0.5, 0.5, 3, 1},      //right

	/* some more 45 deg snaps */
{0.65328145027160645, -0.65328145027160645, 0.27059805393218994, 0.27059805393218994, 0, 0},
{0.92387950420379639, 0.0, 0.0, 0.38268342614173889, 0, 0},
{0.0, -0.92387950420379639, 0.38268342614173889, 0.0, 0, 0},
{0.35355335474014282, -0.85355335474014282, 0.35355338454246521, 0.14644660055637360, 0, 0},
{0.85355335474014282, -0.35355335474014282, 0.14644660055637360, 0.35355338454246521, 0, 0},
{0.49999994039535522, -0.49999994039535522, 0.49999997019767761, 0.49999997019767761, 0, 0},
{0.27059802412986755, -0.65328145027160645, 0.65328145027160645, 0.27059802412986755, 0, 0},
{0.65328145027160645, -0.27059802412986755, 0.27059802412986755, 0.65328145027160645, 0, 0},
{0.27059799432754517, -0.27059799432754517, 0.65328139066696167, 0.65328139066696167, 0, 0},
{0.38268336653709412, 0.0, 0.0, 0.92387944459915161, 0, 0},
{0.0, -0.38268336653709412, 0.92387944459915161, 0.0, 0, 0},
{0.14644658565521240, -0.35355335474014282, 0.85355335474014282, 0.35355335474014282, 0, 0},
{0.35355335474014282, -0.14644658565521240, 0.35355335474014282, 0.85355335474014282, 0, 0},
{0.0, 0.0, 0.92387944459915161, 0.38268336653709412, 0, 0},
{-0.0, 0.0, 0.38268336653709412, 0.92387944459915161, 0, 0},
{-0.27059802412986755, 0.27059802412986755, 0.65328133106231689, 0.65328133106231689, 0, 0},
{-0.38268339633941650, 0.0, 0.0, 0.92387938499450684, 0, 0},
{0.0, 0.38268339633941650, 0.92387938499450684, 0.0, 0, 0},
{-0.14644658565521240, 0.35355338454246521, 0.85355329513549805, 0.35355332493782043, 0, 0},
{-0.35355338454246521, 0.14644658565521240, 0.35355332493782043, 0.85355329513549805, 0, 0},
{-0.49999991059303284, 0.49999991059303284, 0.49999985098838806, 0.49999985098838806, 0, 0},
{-0.27059799432754517, 0.65328145027160645, 0.65328139066696167, 0.27059799432754517, 0, 0},
{-0.65328145027160645, 0.27059799432754517, 0.27059799432754517, 0.65328139066696167, 0, 0},
{-0.65328133106231689, 0.65328133106231689, 0.27059793472290039, 0.27059793472290039, 0, 0},
{-0.92387932538986206, 0.0, 0.0, 0.38268333673477173, 0, 0},
{0.0, 0.92387932538986206, 0.38268333673477173, 0.0, 0, 0},
{-0.35355329513549805, 0.85355329513549805, 0.35355329513549805, 0.14644657075405121, 0, 0},
{-0.85355329513549805, 0.35355329513549805, 0.14644657075405121, 0.35355329513549805, 0, 0},
{-0.38268330693244934, 0.92387938499450684, 0.0, 0.0, 0, 0},
{-0.92387938499450684, 0.38268330693244934, 0.0, 0.0, 0, 0},
{-COS45, 0.0, 0.0, SIN45, 0, 0},
{COS45, 0.0, 0.0, SIN45, 0, 0},
{0.0, 0.0, 0.0, 1.0, 0, 0}
};


static void viewrotate_apply(ViewOpsData *vod, int x, int y, int ctrl)
{
	View3D *v3d= vod->v3d;
	int use_sel= 0;	/* XXX */

	v3d->view= 0; /* need to reset everytime because of view snapping */

	if (U.flag & USER_TRACKBALL) {
		float phi, si, q1[4], dvec[3], newvec[3];

		calctrackballvec(&vod->ar->winrct, x, y, newvec);

		VecSubf(dvec, newvec, vod->trackvec);

		si= sqrt(dvec[0]*dvec[0]+ dvec[1]*dvec[1]+ dvec[2]*dvec[2]);
		si/= (2.0*TRACKBALLSIZE);

		Crossf(q1+1, vod->trackvec, newvec);
		Normalize(q1+1);

		/* Allow for rotation beyond the interval
			* [-pi, pi] */
		while (si > 1.0)
			si -= 2.0;

		/* This relation is used instead of
			* phi = asin(si) so that the angle
			* of rotation is linearly proportional
			* to the distance that the mouse is
			* dragged. */
		phi = si * M_PI / 2.0;

		si= sin(phi);
		q1[0]= cos(phi);
		q1[1]*= si;
		q1[2]*= si;
		q1[3]*= si;
		QuatMul(v3d->viewquat, q1, vod->oldquat);

		if (use_sel) {
			/* compute the post multiplication quat, to rotate the offset correctly */
			QUATCOPY(q1, vod->oldquat);
			QuatConj(q1);
			QuatMul(q1, q1, v3d->viewquat);

			QuatConj(q1); /* conj == inv for unit quat */
			VECCOPY(v3d->ofs, vod->ofs);
			VecSubf(v3d->ofs, v3d->ofs, vod->obofs);
			QuatMulVecf(q1, v3d->ofs);
			VecAddf(v3d->ofs, v3d->ofs, vod->obofs);
		}
	}
	else {
		/* New turntable view code by John Aughey */
		float si, phi, q1[4];
		float m[3][3];
		float m_inv[3][3];
		float xvec[3] = {1,0,0};
		/* Sensitivity will control how fast the viewport rotates.  0.0035 was
			obtained experimentally by looking at viewport rotation sensitivities
			on other modeling programs. */
		/* Perhaps this should be a configurable user parameter. */
		const float sensitivity = 0.0035;

		/* Get the 3x3 matrix and its inverse from the quaternion */
		QuatToMat3(v3d->viewquat, m);
		Mat3Inv(m_inv,m);

		/* Determine the direction of the x vector (for rotating up and down) */
		/* This can likely be compuated directly from the quaternion. */
		Mat3MulVecfl(m_inv,xvec);

		/* Perform the up/down rotation */
		phi = sensitivity * -(y - vod->oldy);
		si = sin(phi);
		q1[0] = cos(phi);
		q1[1] = si * xvec[0];
		q1[2] = si * xvec[1];
		q1[3] = si * xvec[2];
		QuatMul(v3d->viewquat, v3d->viewquat, q1);

		if (use_sel) {
			QuatConj(q1); /* conj == inv for unit quat */
			VecSubf(v3d->ofs, v3d->ofs, vod->obofs);
			QuatMulVecf(q1, v3d->ofs);
			VecAddf(v3d->ofs, v3d->ofs, vod->obofs);
		}

		/* Perform the orbital rotation */
		phi = sensitivity * vod->reverse * (x - vod->oldx);
		q1[0] = cos(phi);
		q1[1] = q1[2] = 0.0;
		q1[3] = sin(phi);
		QuatMul(v3d->viewquat, v3d->viewquat, q1);

		if (use_sel) {
			QuatConj(q1);
			VecSubf(v3d->ofs, v3d->ofs, vod->obofs);
			QuatMulVecf(q1, v3d->ofs);
			VecAddf(v3d->ofs, v3d->ofs, vod->obofs);
		}
	}

	/* check for view snap */
	if (ctrl){
		int i;
		float viewmat[3][3];


		QuatToMat3(v3d->viewquat, viewmat);

		for (i = 0 ; i < 39; i++){
			float snapmat[3][3];
			float view = (int)snapquats[i][4];
			float oposite_dir = (int)snapquats[i][5];

			QuatToMat3(snapquats[i], snapmat);

			if ((Inpf(snapmat[0], viewmat[0]) > thres) &&
				(Inpf(snapmat[1], viewmat[1]) > thres) &&
				(Inpf(snapmat[2], viewmat[2]) > thres)){

				QUATCOPY(v3d->viewquat, snapquats[i]);

				v3d->view = view;
				if (view){
					if (oposite_dir){
						v3d->flag2 |= V3D_OPP_DIRECTION_NAME;
					}else{
						v3d->flag2 &= ~V3D_OPP_DIRECTION_NAME;
					}
				}

				break;
			}
		}
	}
	vod->oldx= x;
	vod->oldy= y;

	ED_region_tag_redraw(vod->ar);
}

static int viewrotate_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	ViewOpsData *vod= op->customdata;

	/* execute the events */
	switch(event->type) {
		case MOUSEMOVE:
			viewrotate_apply(vod, event->x, event->y, event->ctrl);
			break;

		default:
			if(event->type==vod->origkey && event->val==0) {

				if(vod->vh) {
					ED_region_draw_cb_exit(CTX_wm_region(C)->type, vod->vh);
					ED_region_tag_redraw(CTX_wm_region(C));
				}
				MEM_freeN(vod);
				op->customdata= NULL;

				return OPERATOR_FINISHED;
			}
	}

	return OPERATOR_RUNNING_MODAL;
}

static void vh_draw(const bContext *C, ARegion *ar)
{
	glColor3ub(100, 200, 100);
	glRectf(-0.2,  -0.2,  0.2,  0.2); 
}

static int viewrotate_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ViewOpsData *vod;

	/* makes op->customdata */
	viewops_data(C, op, event);
	vod= op->customdata;

	vod->vh= ED_region_draw_cb_activate(CTX_wm_region(C)->type, vh_draw, REGION_DRAW_POST);
	
	/* switch from camera view when: */
	if(vod->v3d->persp != V3D_PERSP) {

		if (U.uiflag & USER_AUTOPERSP)
			vod->v3d->persp= V3D_PERSP;
		else if(vod->v3d->persp==V3D_CAMOB)
			vod->v3d->persp= V3D_PERSP;
		ED_region_tag_redraw(vod->ar);
	}

	/* add temp handler */
	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);

	return OPERATOR_RUNNING_MODAL;
}


void VIEW3D_OT_viewrotate(wmOperatorType *ot)
{

	/* identifiers */
	ot->name= "Rotate view";
	ot->idname= "VIEW3D_OT_viewrotate";

	/* api callbacks */
	ot->invoke= viewrotate_invoke;
	ot->modal= viewrotate_modal;
	ot->poll= ED_operator_view3d_active;
}

/* ************************ viewmove ******************************** */

static void viewmove_apply(ViewOpsData *vod, int x, int y)
{
	if(vod->v3d->persp==V3D_CAMOB) {
		float max= (float)MAX2(vod->ar->winx, vod->ar->winy);

		vod->v3d->camdx += (vod->oldx - x)/(max);
		vod->v3d->camdy += (vod->oldy - y)/(max);
		CLAMP(vod->v3d->camdx, -1.0f, 1.0f);
		CLAMP(vod->v3d->camdy, -1.0f, 1.0f);
// XXX		preview3d_event= 0;
	}
	else {
		float dvec[3];

		window_to_3d(vod->ar, vod->v3d, dvec, x-vod->oldx, y-vod->oldy);
		VecAddf(vod->v3d->ofs, vod->v3d->ofs, dvec);
	}

	vod->oldx= x;
	vod->oldy= y;

	ED_region_tag_redraw(vod->ar);
}


static int viewmove_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	ViewOpsData *vod= op->customdata;

	/* execute the events */
	switch(event->type) {
		case MOUSEMOVE:
			viewmove_apply(vod, event->x, event->y);
			break;

		default:
			if(event->type==vod->origkey && event->val==0) {

				MEM_freeN(vod);
				op->customdata= NULL;

				return OPERATOR_FINISHED;
			}
	}

	return OPERATOR_RUNNING_MODAL;
}

static int viewmove_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	/* makes op->customdata */
	viewops_data(C, op, event);

	/* add temp handler */
	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);

	return OPERATOR_RUNNING_MODAL;
}


void VIEW3D_OT_viewmove(wmOperatorType *ot)
{

	/* identifiers */
	ot->name= "Rotate view";
	ot->idname= "VIEW3D_OT_viewmove";

	/* api callbacks */
	ot->invoke= viewmove_invoke;
	ot->modal= viewmove_modal;
	ot->poll= ED_operator_view3d_active;
}

/* ************************ viewzoom ******************************** */

static void view_zoom_mouseloc(ARegion *ar, View3D *v3d, float dfac, int mx, int my)
{
	if(U.uiflag & USER_ZOOM_TO_MOUSEPOS) {
		float dvec[3];
		float tvec[3];
		float tpos[3];
		float new_dist;
		short vb[2], mouseloc[2];

		mouseloc[0]= mx - ar->winrct.xmin;
		mouseloc[1]= my - ar->winrct.ymin;

		/* find the current window width and height */
		vb[0] = ar->winx;
		vb[1] = ar->winy;

		tpos[0] = -v3d->ofs[0];
		tpos[1] = -v3d->ofs[1];
		tpos[2] = -v3d->ofs[2];

		/* Project cursor position into 3D space */
		initgrabz(v3d, tpos[0], tpos[1], tpos[2]);
		window_to_3d(ar, v3d, dvec, mouseloc[0]-vb[0]/2, mouseloc[1]-vb[1]/2);

		/* Calculate view target position for dolly */
		tvec[0] = -(tpos[0] + dvec[0]);
		tvec[1] = -(tpos[1] + dvec[1]);
		tvec[2] = -(tpos[2] + dvec[2]);

		/* Offset to target position and dolly */
		new_dist = v3d->dist * dfac;

		VECCOPY(v3d->ofs, tvec);
		v3d->dist = new_dist;

		/* Calculate final offset */
		dvec[0] = tvec[0] + dvec[0] * dfac;
		dvec[1] = tvec[1] + dvec[1] * dfac;
		dvec[2] = tvec[2] + dvec[2] * dfac;

		VECCOPY(v3d->ofs, dvec);
	} else {
		v3d->dist *= dfac;
	}
}


static void viewzoom_apply(ViewOpsData *vod, int x, int y)
{
	float zfac=1.0;

	if(U.viewzoom==USER_ZOOM_CONT) {
		// oldstyle zoom
		zfac = 1.0+(float)(vod->origx - x + vod->origy - y)/1000.0;
	}
	else if(U.viewzoom==USER_ZOOM_SCALE) {
		int ctr[2], len1, len2;
		// method which zooms based on how far you move the mouse

		ctr[0] = (vod->ar->winrct.xmax + vod->ar->winrct.xmin)/2;
		ctr[1] = (vod->ar->winrct.ymax + vod->ar->winrct.ymin)/2;

		len1 = (int)sqrt((ctr[0] - x)*(ctr[0] - x) + (ctr[1] - y)*(ctr[1] - y)) + 5;
		len2 = (int)sqrt((ctr[0] - vod->origx)*(ctr[0] - vod->origx) + (ctr[1] - vod->origy)*(ctr[1] - vod->origy)) + 5;

		zfac = vod->dist0 * ((float)len2/len1) / vod->v3d->dist;
	}
	else {	/* USER_ZOOM_DOLLY */
		float len1 = (vod->ar->winrct.ymax - y) + 5;
		float len2 = (vod->ar->winrct.ymax - vod->origy) + 5;
		zfac = vod->dist0 * (2.0*((len2/len1)-1.0) + 1.0) / vod->v3d->dist;
	}

	if(zfac != 1.0 && zfac*vod->v3d->dist > 0.001*vod->v3d->grid &&
				zfac*vod->v3d->dist < 10.0*vod->v3d->far)
		view_zoom_mouseloc(vod->ar, vod->v3d, zfac, vod->oldx, vod->oldy);


	if ((U.uiflag & USER_ORBIT_ZBUF) && (U.viewzoom==USER_ZOOM_CONT) && (vod->v3d->persp==V3D_PERSP)) {
		float upvec[3], mat[3][3];

		/* Secret apricot feature, translate the view when in continues mode */
		upvec[0] = upvec[1] = 0.0f;
		upvec[2] = (vod->dist0 - vod->v3d->dist) * vod->v3d->grid;
		vod->v3d->dist = vod->dist0;
		Mat3CpyMat4(mat, vod->v3d->viewinv);
		Mat3MulVecfl(mat, upvec);
		VecAddf(vod->v3d->ofs, vod->v3d->ofs, upvec);
	} else {
		/* these limits are in toets.c too */
		if(vod->v3d->dist<0.001*vod->v3d->grid) vod->v3d->dist= 0.001*vod->v3d->grid;
		if(vod->v3d->dist>10.0*vod->v3d->far) vod->v3d->dist=10.0*vod->v3d->far;
	}

// XXX	if(vod->v3d->persp==V3D_ORTHO || vod->v3d->persp==V3D_CAMOB) preview3d_event= 0;

	ED_region_tag_redraw(vod->ar);
}


static int viewzoom_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	ViewOpsData *vod= op->customdata;

	/* execute the events */
	switch(event->type) {
		case MOUSEMOVE:
			viewzoom_apply(vod, event->x, event->y);
			break;

		default:
			if(event->type==vod->origkey && event->val==0) {

				MEM_freeN(vod);
				op->customdata= NULL;

				return OPERATOR_FINISHED;
			}
	}

	return OPERATOR_RUNNING_MODAL;
}

static int viewzoom_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	int delta= RNA_int_get(op->ptr, "delta");

	if(delta < 0) {
		/* this min and max is also in viewmove() */
		if(v3d->persp==V3D_CAMOB) {
			v3d->camzoom-= 10;
			if(v3d->camzoom<-30) v3d->camzoom= -30;
		}
		else if(v3d->dist<10.0*v3d->far) v3d->dist*=1.2f;
	}
	else {
		if(v3d->persp==V3D_CAMOB) {
			v3d->camzoom+= 10;
			if(v3d->camzoom>300) v3d->camzoom= 300;
		}
		else if(v3d->dist> 0.001*v3d->grid) v3d->dist*=.83333f;
	}

	ED_region_tag_redraw(CTX_wm_region(C));

	return OPERATOR_FINISHED;
}

static int viewzoom_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	int delta= RNA_int_get(op->ptr, "delta");

	if(delta) {
		viewzoom_exec(C, op);
	}
	else {
		/* makes op->customdata */
		viewops_data(C, op, event);

		/* add temp handler */
		WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);

		return OPERATOR_RUNNING_MODAL;
	}
	return OPERATOR_FINISHED;
}


void VIEW3D_OT_viewzoom(wmOperatorType *ot)
{

	/* identifiers */
	ot->name= "Rotate view";
	ot->idname= "VIEW3D_OT_viewzoom";

	/* api callbacks */
	ot->invoke= viewzoom_invoke;
	ot->exec= viewzoom_exec;
	ot->modal= viewzoom_modal;
	ot->poll= ED_operator_view3d_active;

	RNA_def_property(ot->srna, "delta", PROP_INT, PROP_NONE);
}

static int viewhome_exec(bContext *C, wmOperator *op) /* was view3d_home() in 2.4x */
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	View3D *v3d= sa->spacedata.first;
	Scene *scene= CTX_data_scene(C);
	Base *base;

	int center= RNA_boolean_get(op->ptr, "center");

	float size, min[3], max[3], afm[3];
	int ok= 1, onedone=0;

	if(center) {
		min[0]= min[1]= min[2]= 0.0f;
		max[0]= max[1]= max[2]= 0.0f;
	}
	else {
		INIT_MINMAX(min, max);
	}

	for(base= scene->base.first; base; base= base->next) {
		if(base->lay & v3d->lay) {
			onedone= 1;
			minmax_object(base->object, min, max);
		}
	}
	if(!onedone) return OPERATOR_FINISHED; /* TODO - should this be cancel? */

	afm[0]= (max[0]-min[0]);
	afm[1]= (max[1]-min[1]);
	afm[2]= (max[2]-min[2]);
	size= 0.7f*MAX3(afm[0], afm[1], afm[2]);
	if(size==0.0) ok= 0;

	if(ok) {
		float new_dist;
		float new_ofs[3];

		new_dist = size;
		new_ofs[0]= -(min[0]+max[0])/2.0f;
		new_ofs[1]= -(min[1]+max[1])/2.0f;
		new_ofs[2]= -(min[2]+max[2])/2.0f;

		// correction for window aspect ratio
		if(ar->winy>2 && ar->winx>2) {
			size= (float)ar->winx/(float)ar->winy;
			if(size<1.0) size= 1.0f/size;
			new_dist*= size;
		}

		if (v3d->persp==V3D_CAMOB) {
			v3d->persp= V3D_PERSP;
			smooth_view(C, NULL, v3d->camera, new_ofs, NULL, &new_dist, NULL); 
		}
	}
// XXX	BIF_view3d_previewrender_signal(curarea, PR_DBASE|PR_DISPRECT);

	return OPERATOR_FINISHED;
}

void VIEW3D_OT_viewhome(wmOperatorType *ot)
{

	/* identifiers */
	ot->name= "View home";
	ot->idname= "VIEW3D_OT_viewhome";

	/* api callbacks */
	ot->exec= viewhome_exec;
	ot->poll= ED_operator_view3d_active;

	RNA_def_property(ot->srna, "center", PROP_BOOLEAN, PROP_NONE);
}

static int viewcenter_exec(bContext *C, wmOperator *op) /* like a localview without local!, was centerview() in 2.4x */
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	View3D *v3d= sa->spacedata.first;
	Scene *scene= CTX_data_scene(C);
	Object *ob= OBACT;
	Object *obedit= CTX_data_edit_object(C);
	float size, min[3], max[3], afm[3];
	int ok=0;

	/* SMOOTHVIEW */
	float new_ofs[3];
	float new_dist;

	INIT_MINMAX(min, max);

	if (G.f & G_WEIGHTPAINT) {
		/* hardcoded exception, we look for the one selected armature */
		/* this is weak code this way, we should make a generic active/selection callback interface once... */
		Base *base;
		for(base=scene->base.first; base; base= base->next) {
			if(TESTBASELIB(v3d, base)) {
				if(base->object->type==OB_ARMATURE)
					if(base->object->flag & OB_POSEMODE)
						break;
			}
		}
		if(base)
			ob= base->object;
	}


	if(obedit) {
// XXX		ok = minmax_verts(min, max);	/* only selected */
	}
	else if(ob && (ob->flag & OB_POSEMODE)) {
		if(ob->pose) {
			bArmature *arm= ob->data;
			bPoseChannel *pchan;
			float vec[3];

			for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
				if(pchan->bone->flag & BONE_SELECTED) {
					if(pchan->bone->layer & arm->layer) {
						ok= 1;
						VECCOPY(vec, pchan->pose_head);
						Mat4MulVecfl(ob->obmat, vec);
						DO_MINMAX(vec, min, max);
						VECCOPY(vec, pchan->pose_tail);
						Mat4MulVecfl(ob->obmat, vec);
						DO_MINMAX(vec, min, max);
					}
				}
			}
		}
	}
	else if (FACESEL_PAINT_TEST) {
// XXX		ok= minmax_tface(min, max);
	}
	else if (G.f & G_PARTICLEEDIT) {
// XXX		ok= PE_minmax(min, max);
	}
	else {
		Base *base= FIRSTBASE;
		while(base) {
			if(TESTBASE(v3d, base))  {
				minmax_object(base->object, min, max);
				/* account for duplis */
				minmax_object_duplis(scene, base->object, min, max);

				ok= 1;
			}
			base= base->next;
		}
	}

	if(ok==0) return OPERATOR_FINISHED;

	afm[0]= (max[0]-min[0]);
	afm[1]= (max[1]-min[1]);
	afm[2]= (max[2]-min[2]);
	size= 0.7f*MAX3(afm[0], afm[1], afm[2]);

	if(size <= v3d->near*1.5f) size= v3d->near*1.5f;

	new_ofs[0]= -(min[0]+max[0])/2.0f;
	new_ofs[1]= -(min[1]+max[1])/2.0f;
	new_ofs[2]= -(min[2]+max[2])/2.0f;

	new_dist = size;

	/* correction for window aspect ratio */
	if(ar->winy>2 && ar->winx>2) {
		size= (float)ar->winx/(float)ar->winy;
		if(size<1.0f) size= 1.0f/size;
		new_dist*= size;
	}

	v3d->cursor[0]= -new_ofs[0];
	v3d->cursor[1]= -new_ofs[1];
	v3d->cursor[2]= -new_ofs[2];

	if (v3d->persp==V3D_CAMOB) {
		v3d->persp= V3D_PERSP;
		smooth_view(C, v3d->camera, NULL, new_ofs, NULL, &new_dist, NULL);
	} 
	else {
		smooth_view(C, NULL, NULL, new_ofs, NULL, &new_dist, NULL);
	}

// XXX	BIF_view3d_previewrender_signal(curarea, PR_DBASE|PR_DISPRECT);

	return OPERATOR_FINISHED;
}
void VIEW3D_OT_viewcenter(wmOperatorType *ot)
{

	/* identifiers */
	ot->name= "View center";
	ot->idname= "VIEW3D_OT_viewcenter";

	/* api callbacks */
	ot->exec= viewcenter_exec;
	ot->poll= ED_operator_view3d_active;
}

/* ********************* Set render border operator ****************** */

static int render_border_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	View3D *v3d= sa->spacedata.first;
	Scene *scene= CTX_data_scene(C);
	
	rcti rect;
	rctf vb;
	
	/* get border select values using rna */
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");
	
	/* calculate range */
	calc_viewborder(scene, ar, v3d, &vb);

	scene->r.border.xmin= ((float)rect.xmin-vb.xmin)/(vb.xmax-vb.xmin);
	scene->r.border.ymin= ((float)rect.ymin-vb.ymin)/(vb.ymax-vb.ymin);
	scene->r.border.xmax= ((float)rect.xmax-vb.xmin)/(vb.xmax-vb.xmin);
	scene->r.border.ymax= ((float)rect.ymax-vb.ymin)/(vb.ymax-vb.ymin);
	
	/* actually set border */	
	CLAMP(scene->r.border.xmin, 0.0, 1.0);
	CLAMP(scene->r.border.ymin, 0.0, 1.0);
	CLAMP(scene->r.border.xmax, 0.0, 1.0);
	CLAMP(scene->r.border.ymax, 0.0, 1.0);
		
	/* drawing a border surrounding the entire camera view switches off border rendering
	 * or the border covers no pixels */
	if ((scene->r.border.xmin <= 0.0 && scene->r.border.xmax >= 1.0 &&
		scene->r.border.ymin <= 0.0 && scene->r.border.ymax >= 1.0) ||
	   (scene->r.border.xmin == scene->r.border.xmax ||
		scene->r.border.ymin == scene->r.border.ymax ))
	{
		scene->r.mode &= ~R_BORDER;
	} else {
		scene->r.mode |= R_BORDER;
	}
	
	return OPERATOR_FINISHED;

}

static int view3d_render_border_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	
	/* if not in camera view do not exec the operator*/
	if (v3d->persp == V3D_CAMOB) return WM_border_select_invoke(C, op, event);	
	else return OPERATOR_PASS_THROUGH;
}

void VIEW3D_OT_render_border(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Set Render Border";
	ot->idname= "VIEW3D_OT_render_border";

	/* api callbacks */
	ot->invoke= view3d_render_border_invoke;
	ot->exec= render_border_exec;
	ot->modal= WM_border_select_modal;
	
	ot->poll= ED_operator_view3d_active;
	
	/* rna */
	RNA_def_property(ot->srna, "xmin", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "xmax", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "ymin", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "ymax", PROP_INT, PROP_NONE);

}
/* ********************* Changing view operator ****************** */

static EnumPropertyItem prop_view_items[] = {
	{V3D_VIEW_FRONT, "FRONT", "Front", "View From the Front"},
	{V3D_VIEW_BACK, "BACK", "Back", "View From the Back"},
	{V3D_VIEW_LEFT, "LEFT", "Left", "View From the Left"},
	{V3D_VIEW_RIGHT, "RIGHT", "Right", "View From the Right"},
	{V3D_VIEW_TOP, "TOP", "Top", "View From the Top"},
	{V3D_VIEW_BOTTOM, "BOTTOM", "Bottom", "View From the Bottom"},
	{V3D_VIEW_PERSPORTHO, "PERSPORTHO", "Persp-Ortho", "Switch between Perspecive and Orthographic View"},
	{V3D_VIEW_CAMERA, "CAMERA", "Camera", "View From the active amera"},
	{V3D_VIEW_STEPLEFT, "STEPLEFT", "Step Left", "Step the view around to the Left"},
	{V3D_VIEW_STEPRIGHT, "STEPRIGHT", "Step Right", "Step the view around to the Right"},
	{V3D_VIEW_STEPUP, "STEPUP", "Step Up", "Step the view Up"},
	{V3D_VIEW_STEPDOWN, "STEPDOWN", "Step Down", "Step the view Down"},
	{V3D_VIEW_PANLEFT, "PANLEFT", "Pan Left", "Pan the view to the Left"},
	{V3D_VIEW_PANRIGHT, "PANRIGHT", "Pan Right", "Pan the view to the Right"},
	{V3D_VIEW_PANUP, "PANUP", "Pan Up", "Pan the view Up"},
	{V3D_VIEW_PANDOWN, "PANDOWN", "Pan Down", "Pan the view Down"},
	{0, NULL, NULL, NULL}};

static void axis_set_view(bContext *C, View3D *v3d, float q1, float q2, float q3, float q4, short view, int perspo)
{
	float new_quat[4];
	new_quat[0]= q1; new_quat[1]= q2;
	new_quat[2]= q3; new_quat[3]= q4;
	v3d->view=0;

	v3d->view= view;
	
	if (v3d->persp==V3D_CAMOB && v3d->camera) {

		if (U.uiflag & USER_AUTOPERSP) v3d->persp= V3D_ORTHO;
		else if(v3d->persp==V3D_CAMOB) v3d->persp= perspo;

		smooth_view(C, v3d->camera, NULL, v3d->ofs, new_quat, NULL, NULL); 
	} 
	else {

		if (U.uiflag & USER_AUTOPERSP) v3d->persp= V3D_ORTHO;
		else if(v3d->persp==V3D_CAMOB) v3d->persp= perspo;

		smooth_view(C, NULL, NULL, NULL, new_quat, NULL, NULL);
	}

}


static int viewnumpad_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	View3D *v3d= sa->spacedata.first;
	Scene *scene= CTX_data_scene(C);
	float phi, si, q1[4], vec[3];
	static int perspo=V3D_PERSP;
	int viewnum;

	viewnum = RNA_enum_get(op->ptr, "viewnum");

	/* Use this to test if we started out with a camera */

	/* Indicate that this view is inverted,
	 * but only if it actually _was_ inverted (jobbe) */
	if (viewnum == V3D_VIEW_BOTTOM || viewnum == V3D_VIEW_BACK || viewnum == V3D_VIEW_LEFT)
		v3d->flag2 |= V3D_OPP_DIRECTION_NAME;
	else if (viewnum != V3D_VIEW_PERSPORTHO)
			v3d->flag2 &= ~V3D_OPP_DIRECTION_NAME;

	switch (viewnum) {
		case V3D_VIEW_BOTTOM :
			axis_set_view(C, v3d, 0.0, -1.0, 0.0, 0.0, 7, perspo);
			break;

		case V3D_VIEW_BACK:
			axis_set_view(C, v3d, 0.0, 0.0, (float)-cos(M_PI/4.0), (float)-cos(M_PI/4.0), 1, perspo);
			break;

		case V3D_VIEW_LEFT:
			axis_set_view(C, v3d, 0.5, -0.5, 0.5, 0.5, 3, perspo);
			break;

		case V3D_VIEW_TOP:
			axis_set_view(C, v3d, 1.0, 0.0, 0.0, 0.0, 7, perspo);
			break;

		case V3D_VIEW_FRONT:
			axis_set_view(C, v3d, (float)cos(M_PI/4.0), (float)-sin(M_PI/4.0), 0.0, 0.0, 1, perspo);
			break;

		case V3D_VIEW_RIGHT:
			axis_set_view(C, v3d, 0.5, -0.5, -0.5, -0.5, 3, perspo);
			break;

		case V3D_VIEW_PERSPORTHO:

			if(v3d->persp!=V3D_ORTHO) 
				v3d->persp=V3D_ORTHO;
			else v3d->persp=V3D_PERSP;

			ED_region_tag_redraw(ar);
			break;

		case V3D_VIEW_CAMERA:
			/* lastview -  */

			if(v3d->persp != V3D_CAMOB) {
				/* store settings of current view before allowing overwriting with camera view */
				QUATCOPY(v3d->lviewquat, v3d->viewquat);
				v3d->lview= v3d->view;
				v3d->lpersp= v3d->persp;
				
#if 0
				if(G.qual==LR_ALTKEY) {
					if(oldcamera && is_an_active_object(oldcamera)) {
						v3d->camera= oldcamera;
					}
					handle_view3d_lock();
				}
#endif
				
				if(BASACT) {
					/* check both G.vd as G.scene cameras */
					if((v3d->camera==NULL || scene->camera==NULL) && OBACT->type==OB_CAMERA) {
						v3d->camera= OBACT;
						/*handle_view3d_lock();*/
					}
				}
				
				if(v3d->camera==NULL) {
					v3d->camera= scene_find_camera(scene);
					/*handle_view3d_lock();*/
				}
				v3d->persp= V3D_CAMOB;
				smooth_view(C, NULL, v3d->camera, v3d->ofs, v3d->viewquat, &v3d->dist, &v3d->lens);
				
			}
			else{
				/* return to settings of last view */
				/* does smooth_view too */
				axis_set_view(C, v3d, v3d->lviewquat[0], v3d->lviewquat[1], v3d->lviewquat[2], v3d->lviewquat[3], v3d->lview, v3d->lpersp);
			}
			break;

		case V3D_VIEW_STEPLEFT:
		case V3D_VIEW_STEPRIGHT:
		case V3D_VIEW_STEPUP:
		case V3D_VIEW_STEPDOWN:

			if(v3d->persp != V3D_CAMOB) {
				if(viewnum == V3D_VIEW_STEPLEFT || viewnum == V3D_VIEW_STEPRIGHT) {
					/* z-axis */
					phi= (float)(M_PI/360.0)*U.pad_rot_angle;
					if(viewnum == V3D_VIEW_STEPRIGHT) phi= -phi;
					si= (float)sin(phi);
					q1[0]= (float)cos(phi);
					q1[1]= q1[2]= 0.0;
					q1[3]= si;
					QuatMul(v3d->viewquat, v3d->viewquat, q1);
					v3d->view= 0;
				}
				if(viewnum == V3D_VIEW_STEPDOWN || viewnum == V3D_VIEW_STEPUP) {
					/* horizontal axis */
					VECCOPY(q1+1, v3d->viewinv[0]);

					Normalize(q1+1);
					phi= (float)(M_PI/360.0)*U.pad_rot_angle;
					if(viewnum == V3D_VIEW_STEPDOWN) phi= -phi;
					si= (float)sin(phi);
					q1[0]= (float)cos(phi);
					q1[1]*= si;
					q1[2]*= si;
					q1[3]*= si;
					QuatMul(v3d->viewquat, v3d->viewquat, q1);
					v3d->view= 0;
				}
				ED_region_tag_redraw(ar);
			}
			break;

		case V3D_VIEW_PANRIGHT:
		case V3D_VIEW_PANLEFT:
		case V3D_VIEW_PANUP:
		case V3D_VIEW_PANDOWN:

			initgrabz(v3d, 0.0, 0.0, 0.0);

			if(viewnum == V3D_VIEW_PANRIGHT) window_to_3d(ar, v3d, vec, -32, 0);
			else if(viewnum == V3D_VIEW_PANLEFT) window_to_3d(ar, v3d, vec, 32, 0);
			else if(viewnum == V3D_VIEW_PANUP) window_to_3d(ar, v3d, vec, 0, -25);
			else if(viewnum == V3D_VIEW_PANDOWN) window_to_3d(ar, v3d, vec, 0, 25);
			v3d->ofs[0]+= vec[0];
			v3d->ofs[1]+= vec[1];
			v3d->ofs[2]+= vec[2];

			ED_region_tag_redraw(ar);
			break;

		default :
			break;
	}

	if(v3d->persp != V3D_CAMOB) perspo= v3d->persp;

	return OPERATOR_FINISHED;
}


void VIEW3D_OT_viewnumpad(wmOperatorType *ot)
{

	PropertyRNA *prop;

	/* identifiers */
	ot->name= "View numpad";
	ot->idname= "VIEW3D_OT_viewnumpad";

	/* api callbacks */
	ot->exec= viewnumpad_exec;
	ot->poll= ED_operator_view3d_active;
	ot->flag= OPTYPE_REGISTER;

	prop = RNA_def_property(ot->srna, "viewnum", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_view_items);
}

/* ********************* set clipping operator ****************** */

static int view3d_clipping_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	rcti rect;
	double mvmatrix[16];
	double projmatrix[16];
	double xs, ys, p[3];
	GLint viewport[4];
	short val;

	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");

	v3d->flag |= V3D_CLIPPING;
	v3d->clipbb= MEM_callocN(sizeof(BoundBox), "clipbb");

	/* note; otherwise opengl won't work */
	view3d_operator_needs_opengl(C);

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
	return OPERATOR_FINISHED;
}

static int view3d_clipping_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;

	if(v3d->flag & V3D_CLIPPING) {
		v3d->flag &= ~V3D_CLIPPING;
		ED_area_tag_redraw(sa);
		if(v3d->clipbb) MEM_freeN(v3d->clipbb);
		v3d->clipbb= NULL;
		return OPERATOR_FINISHED;
	}
	else {
		return WM_border_select_invoke(C, op, event);
	}
}

/* toggles */
void VIEW3D_OT_clipping(wmOperatorType *ot)
{

	/* identifiers */
	ot->name= "Border Select";
	ot->idname= "VIEW3D_OT_clipping";

	/* api callbacks */
	ot->invoke= view3d_clipping_invoke;
	ot->exec= view3d_clipping_exec;
	ot->modal= WM_border_select_modal;

	ot->poll= ED_operator_view3d_active;

	/* rna */
	RNA_def_property(ot->srna, "xmin", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "xmax", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "ymin", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "ymax", PROP_INT, PROP_NONE);
}

/* ********************* draw type operator ****************** */

static int view3d_drawtype_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	int dt, dt_alt;

	dt  = RNA_int_get(op->ptr, "drawtype");
	dt_alt = RNA_int_get(op->ptr, "drawtype_alt");
	
	if (dt_alt != -1)
	{
		if (v3d->drawtype == dt)
		{
			v3d->drawtype = dt_alt;
		}
		else
		{
			v3d->drawtype = dt;
		}
	}
	else
	{
		v3d->drawtype = dt;
	}

	ED_area_tag_redraw(sa);
	
	return OPERATOR_FINISHED;
}

static int view3d_drawtype_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	return view3d_drawtype_exec(C, op);
}

/* toggles */
void VIEW3D_OT_drawtype(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Change draw type";
	ot->idname= "VIEW3D_OT_drawtype";

	/* api callbacks */
	ot->invoke= view3d_drawtype_invoke;
	ot->exec= view3d_drawtype_exec;

	ot->poll= ED_operator_view3d_active;

	/* rna */
	RNA_def_property(ot->srna, "drawtype", PROP_INT, PROP_NONE);
	prop = RNA_def_property(ot->srna, "drawtype_alt", PROP_INT, PROP_NONE);
	RNA_def_property_int_default(prop, -1);
}

/* ********************************************************* */

void view3d_border_zoom(Scene *scene, ARegion *ar, View3D *v3d)
{

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
	val = 0; // XXX get_border(&rect, 3);
	if(!val) return;

	/* Get Z Depths, needed for perspective, nice for ortho */
	bgl_get_mats(&mats);
	draw_depth(scene, ar, v3d, NULL);

	/* force updating */
	if (v3d->depths) {
		had_depth = 1;
		v3d->depths->damaged = 1;
	}

	view3d_update_depths(ar, v3d);

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
		if ((	!gluUnProject(cent[0], cent[1], depth_close, mats.modelview, mats.projection, (GLint *)mats.viewport, &p[0], &p[1], &p[2])) ||
			(	!gluUnProject((double)rect.xmin, (double)rect.ymin, depth_close, mats.modelview, mats.projection, (GLint *)mats.viewport, &p_corner[0], &p_corner[1], &p_corner[2])))
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
		vb[0] = ar->winx;
		vb[1] = ar->winy;

		new_dist = v3d->dist;

		/* convert the drawn rectangle into 3d space */
		if (depth_close!=MAXFLOAT && gluUnProject(cent[0], cent[1], depth_close, mats.modelview, mats.projection, (GLint *)mats.viewport, &p[0], &p[1], &p[2])) {
			new_ofs[0] = -p[0];
			new_ofs[1] = -p[1];
			new_ofs[2] = -p[2];
		} else {
			/* We cant use the depth, fallback to the old way that dosnt set the center depth */
			new_ofs[0] = v3d->ofs[0];
			new_ofs[1] = v3d->ofs[1];
			new_ofs[2] = v3d->ofs[2];

			initgrabz(v3d, -new_ofs[0], -new_ofs[1], -new_ofs[2]);

			window_to_3d(ar, v3d, dvec, (rect.xmin+rect.xmax-vb[0])/2, (rect.ymin+rect.ymax-vb[1])/2);
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

	smooth_view(NULL, NULL, NULL, new_ofs, NULL, &new_dist, NULL); // XXX
}

/* ***************** 3d cursor cursor op ******************* */

/* mx my in region coords */
static int set_3dcursor_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene= CTX_data_scene(C);
	ARegion *ar= CTX_wm_region(C);
	View3D *v3d= (View3D *)CTX_wm_space_data(C);
	float dx, dy, fz, *fp = NULL, dvec[3], oldcurs[3];
	short mx, my, mval[2];
//	short ctrl= 0; // XXX
	
	fp= give_cursor(scene, v3d);
	
//	if(obedit && ctrl) lr_click= 1;
	VECCOPY(oldcurs, fp);
	
	mx= event->x - ar->winrct.xmin;
	my= event->y - ar->winrct.ymin;
	project_short_noclip(ar, v3d, fp, mval);
	
	initgrabz(v3d, fp[0], fp[1], fp[2]);
	
	if(mval[0]!=IS_CLIPPED) {
		
		window_to_3d(ar, v3d, dvec, mval[0]-mx, mval[1]-my);
		VecSubf(fp, fp, dvec);
	}
	else {
		
		dx= ((float)(mx-(ar->winx/2)))*v3d->zfac/(ar->winx/2);
		dy= ((float)(my-(ar->winy/2)))*v3d->zfac/(ar->winy/2);
		
		fz= v3d->persmat[0][3]*fp[0]+ v3d->persmat[1][3]*fp[1]+ v3d->persmat[2][3]*fp[2]+ v3d->persmat[3][3];
		fz= fz/v3d->zfac;
		
		fp[0]= (v3d->persinv[0][0]*dx + v3d->persinv[1][0]*dy+ v3d->persinv[2][0]*fz)-v3d->ofs[0];
		fp[1]= (v3d->persinv[0][1]*dx + v3d->persinv[1][1]*dy+ v3d->persinv[2][1]*fz)-v3d->ofs[1];
		fp[2]= (v3d->persinv[0][2]*dx + v3d->persinv[1][2]*dy+ v3d->persinv[2][2]*fz)-v3d->ofs[2];
	}
	
//	if(lr_click) {
		// XXX		if(obedit->type==OB_MESH) add_click_mesh();
		//		else if ELEM(obedit->type, OB_CURVE, OB_SURF) addvert_Nurb(0);
		//		else if (obedit->type==OB_ARMATURE) addvert_armature();
//		VECCOPY(fp, oldcurs);
//	}
	// XXX notifier for scene */
	ED_region_tag_redraw(ar);
	
	/* prevent other mouse ops to fail */
	return OPERATOR_PASS_THROUGH;
}

void VIEW3D_OT_cursor3d(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Set 3D Cursor";
	ot->idname= "VIEW3D_OT_cursor3d";
	
	/* api callbacks */
	ot->invoke= set_3dcursor_invoke;
	
	ot->poll= ED_operator_view3d_active;
	
	/* rna later */

}


/* ************************* below the line! *********************** */


/* XXX todo Zooms in on a border drawn by the user */
int view_autodist(Scene *scene, ARegion *ar, View3D *v3d, short *mval, float mouse_worldloc[3] ) //, float *autodist )
{
	rcti rect;
	/* ZBuffer depth vars */
	bglMats mats;
	float depth, depth_close= MAXFLOAT;
	int had_depth = 0;
	double cent[2],  p[3];
	int xs, ys;

	// XXX		getmouseco_areawin(mval);

	// XXX	persp(PERSP_VIEW);

	rect.xmax = mval[0] + 4;
	rect.ymax = mval[1] + 4;

	rect.xmin = mval[0] - 4;
	rect.ymin = mval[1] - 4;

	/* Get Z Depths, needed for perspective, nice for ortho */
	bgl_get_mats(&mats);
	draw_depth(scene, ar, v3d, NULL);

	/* force updating */
	if (v3d->depths) {
		had_depth = 1;
		v3d->depths->damaged = 1;
	}

	view3d_update_depths(ar, v3d);

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

	if (depth_close==MAXFLOAT)
		return 0;

	if (had_depth==0) {
		MEM_freeN(v3d->depths->depths);
		v3d->depths->depths = NULL;
	}
	v3d->depths->damaged = 1;

	cent[0] = (double)mval[0];
	cent[1] = (double)mval[1];

	if (!gluUnProject(cent[0], cent[1], depth_close, mats.modelview, mats.projection, (GLint *)mats.viewport, &p[0], &p[1], &p[2]))
		return 0;

	mouse_worldloc[0] = (float)p[0];
	mouse_worldloc[1] = (float)p[1];
	mouse_worldloc[2] = (float)p[2];
	return 1;
}



/* ********************* NDOF ************************ */
/* note: this code is confusing and unclear... (ton) */
/* **************************************************** */

// ndof scaling will be moved to user setting.
// In the mean time this is just a place holder.

// Note: scaling in the plugin and ghostwinlay.c
// should be removed. With driver default setting,
// each axis returns approx. +-200 max deflection.

// The values I selected are based on the older
// polling i/f. With event i/f, the sensistivity
// can be increased for improved response from
// small deflections of the device input.


// lukep notes : i disagree on the range.
// the normal 3Dconnection driver give +/-400
// on defaut range in other applications
// and up to +/- 1000 if set to maximum
// because i remove the scaling by delta,
// which was a bad idea as it depend of the system
// speed and os, i changed the scaling values, but
// those are still not ok


float ndof_axis_scale[6] = {
	+0.01,	// Tx
	+0.01,	// Tz
	+0.01,	// Ty
	+0.0015,	// Rx
	+0.0015,	// Rz
	+0.0015	// Ry
};

void filterNDOFvalues(float *sbval)
{
	int i=0;
	float max  = 0.0;

	for (i =0; i<6;i++)
		if (fabs(sbval[i]) > max)
			max = fabs(sbval[i]);
	for (i =0; i<6;i++)
		if (fabs(sbval[i]) != max )
			sbval[i]=0.0;
}

// statics for controlling v3d->dist corrections.
// viewmoveNDOF zeros and adjusts v3d->ofs.
// viewmove restores based on dz_flag state.

int dz_flag = 0;
float m_dist;

void viewmoveNDOFfly(ARegion *ar, View3D *v3d, int mode)
{
    int i;
    float phi;
    float dval[7];
	// static fval[6] for low pass filter; device input vector is dval[6]
	static float fval[6];
    float tvec[3],rvec[3];
    float q1[4];
	float mat[3][3];
	float upvec[3];


    /*----------------------------------------------------
	 * sometimes this routine is called from headerbuttons
     * viewmove needs to refresh the screen
     */
// XXX	areawinset(ar->win);


	// fetch the current state of the ndof device
// XXX	getndof(dval);

	if (v3d->ndoffilter)
		filterNDOFvalues(fval);

	// Scale input values

//	if(dval[6] == 0) return; // guard against divide by zero

	for(i=0;i<6;i++) {

		// user scaling
		dval[i] = dval[i] * ndof_axis_scale[i];
	}


	// low pass filter with zero crossing reset

	for(i=0;i<6;i++) {
		if((dval[i] * fval[i]) >= 0)
			dval[i] = (fval[i] * 15 + dval[i]) / 16;
		else
			fval[i] = 0;
	}


	// force perspective mode. This is a hack and is
	// incomplete. It doesn't actually effect the view
	// until the first draw and doesn't update the menu
	// to reflect persp mode.

	v3d->persp = V3D_PERSP;


	// Correct the distance jump if v3d->dist != 0

	// This is due to a side effect of the original
	// mouse view rotation code. The rotation point is
	// set a distance in front of the viewport to
	// make rotating with the mouse look better.
	// The distance effect is written at a low level
	// in the view management instead of the mouse
	// view function. This means that all other view
	// movement devices must subtract this from their
	// view transformations.

	if(v3d->dist != 0.0) {
		dz_flag = 1;
		m_dist = v3d->dist;
		upvec[0] = upvec[1] = 0;
		upvec[2] = v3d->dist;
		Mat3CpyMat4(mat, v3d->viewinv);
		Mat3MulVecfl(mat, upvec);
		VecSubf(v3d->ofs, v3d->ofs, upvec);
		v3d->dist = 0.0;
	}


	// Apply rotation
	// Rotations feel relatively faster than translations only in fly mode, so
	// we have no choice but to fix that here (not in the plugins)
	rvec[0] = -0.5 * dval[3];
	rvec[1] = -0.5 * dval[4];
	rvec[2] = -0.5 * dval[5];

	// rotate device x and y by view z

	Mat3CpyMat4(mat, v3d->viewinv);
	mat[2][2] = 0.0f;
	Mat3MulVecfl(mat, rvec);

	// rotate the view

	phi = Normalize(rvec);
	if(phi != 0) {
		VecRotToQuat(rvec,phi,q1);
		QuatMul(v3d->viewquat, v3d->viewquat, q1);
	}


	// Apply translation

	tvec[0] = dval[0];
	tvec[1] = dval[1];
	tvec[2] = -dval[2];

	// the next three lines rotate the x and y translation coordinates
	// by the current z axis angle

	Mat3CpyMat4(mat, v3d->viewinv);
	mat[2][2] = 0.0f;
	Mat3MulVecfl(mat, tvec);

	// translate the view

	VecSubf(v3d->ofs, v3d->ofs, tvec);


	/*----------------------------------------------------
     * refresh the screen XXX
      */

	// update render preview window

// XXX	BIF_view3d_previewrender_signal(ar, PR_DBASE|PR_DISPRECT);
}

void viewmoveNDOF(Scene *scene, View3D *v3d, int mode)
{
    float fval[7];
    float dvec[3];
    float sbadjust = 1.0f;
    float len;
	short use_sel = 0;
	Object *ob = OBACT;
    float m[3][3];
    float m_inv[3][3];
    float xvec[3] = {1,0,0};
    float yvec[3] = {0,-1,0};
    float zvec[3] = {0,0,1};
	float phi, si;
    float q1[4];
    float obofs[3];
    float reverse;
    //float diff[4];
    float d, curareaX, curareaY;
    float mat[3][3];
    float upvec[3];

    /* Sensitivity will control how fast the view rotates.  The value was
     * obtained experimentally by tweaking until the author didn't get dizzy watching.
     * Perhaps this should be a configurable user parameter.
     */
    float psens = 0.005f * (float) U.ndof_pan;   /* pan sensitivity */
    float rsens = 0.005f * (float) U.ndof_rotate;  /* rotate sensitivity */
    float zsens = 0.3f;   /* zoom sensitivity */

    const float minZoom = -30.0f;
    const float maxZoom = 300.0f;

	//reset view type
	v3d->view = 0;
//printf("passing here \n");
//
	if (scene->obedit==NULL && ob && !(ob->flag & OB_POSEMODE)) {
		use_sel = 1;
	}

    if((dz_flag)||v3d->dist==0) {
		dz_flag = 0;
		v3d->dist = m_dist;
		upvec[0] = upvec[1] = 0;
		upvec[2] = v3d->dist;
		Mat3CpyMat4(mat, v3d->viewinv);
		Mat3MulVecfl(mat, upvec);
		VecAddf(v3d->ofs, v3d->ofs, upvec);
	}

    /*----------------------------------------------------
	 * sometimes this routine is called from headerbuttons
     * viewmove needs to refresh the screen
     */
// XXX	areawinset(curarea->win);

    /*----------------------------------------------------
     * record how much time has passed. clamp at 10 Hz
     * pretend the previous frame occured at the clamped time
     */
//    now = PIL_check_seconds_timer();
 //   frametime = (now - prevTime);
 //   if (frametime > 0.1f){        /* if more than 1/10s */
 //       frametime = 1.0f/60.0;      /* clamp at 1/60s so no jumps when starting to move */
//    }
//    prevTime = now;
 //   sbadjust *= 60 * frametime;             /* normalize ndof device adjustments to 100Hz for framerate independence */

    /* fetch the current state of the ndof device & enforce dominant mode if selected */
// XXX    getndof(fval);
	if (v3d->ndoffilter)
		filterNDOFvalues(fval);


    // put scaling back here, was previously in ghostwinlay
    fval[0] = fval[0] * (1.0f/600.0f);
    fval[1] = fval[1] * (1.0f/600.0f);
    fval[2] = fval[2] * (1.0f/1100.0f);
    fval[3] = fval[3] * 0.00005f;
    fval[4] =-fval[4] * 0.00005f;
    fval[5] = fval[5] * 0.00005f;
    fval[6] = fval[6] / 1000000.0f;

    // scale more if not in perspective mode
    if (v3d->persp == V3D_ORTHO) {
        fval[0] = fval[0] * 0.05f;
        fval[1] = fval[1] * 0.05f;
        fval[2] = fval[2] * 0.05f;
        fval[3] = fval[3] * 0.9f;
        fval[4] = fval[4] * 0.9f;
        fval[5] = fval[5] * 0.9f;
        zsens *= 8;
    }


    /* set object offset */
	if (ob) {
		obofs[0] = -ob->obmat[3][0];
		obofs[1] = -ob->obmat[3][1];
		obofs[2] = -ob->obmat[3][2];
	}
	else {
		VECCOPY(obofs, v3d->ofs);
	}

    /* calc an adjustment based on distance from camera
       disabled per patch 14402 */
     d = 1.0f;

/*    if (ob) {
        VecSubf(diff, obofs, v3d->ofs);
        d = VecLength(diff);
    }
*/

    reverse = (v3d->persmat[2][1] < 0.0f) ? -1.0f : 1.0f;

    /*----------------------------------------------------
     * ndof device pan
     */
    psens *= 1.0f + d;
    curareaX = sbadjust * psens * fval[0];
    curareaY = sbadjust * psens * fval[1];
    dvec[0] = curareaX * v3d->persinv[0][0] + curareaY * v3d->persinv[1][0];
    dvec[1] = curareaX * v3d->persinv[0][1] + curareaY * v3d->persinv[1][1];
    dvec[2] = curareaX * v3d->persinv[0][2] + curareaY * v3d->persinv[1][2];
    VecAddf(v3d->ofs, v3d->ofs, dvec);

    /*----------------------------------------------------
     * ndof device dolly
     */
    len = zsens * sbadjust * fval[2];

    if (v3d->persp==V3D_CAMOB) {
        if(v3d->persp==V3D_CAMOB) { /* This is stupid, please fix - TODO */
            v3d->camzoom+= 10.0f * -len;
        }
        if (v3d->camzoom < minZoom) v3d->camzoom = minZoom;
        else if (v3d->camzoom > maxZoom) v3d->camzoom = maxZoom;
    }
    else if ((v3d->dist> 0.001*v3d->grid) && (v3d->dist<10.0*v3d->far)) {
        v3d->dist*=(1.0 + len);
    }


    /*----------------------------------------------------
     * ndof device turntable
     * derived from the turntable code in viewmove
     */

    /* Get the 3x3 matrix and its inverse from the quaternion */
    QuatToMat3(v3d->viewquat, m);
    Mat3Inv(m_inv,m);

    /* Determine the direction of the x vector (for rotating up and down) */
    /* This can likely be compuated directly from the quaternion. */
    Mat3MulVecfl(m_inv,xvec);
    Mat3MulVecfl(m_inv,yvec);
    Mat3MulVecfl(m_inv,zvec);

    /* Perform the up/down rotation */
    phi = sbadjust * rsens * /*0.5f * */ fval[3]; /* spin vertically half as fast as horizontally */
    si = sin(phi);
    q1[0] = cos(phi);
    q1[1] = si * xvec[0];
    q1[2] = si * xvec[1];
    q1[3] = si * xvec[2];
    QuatMul(v3d->viewquat, v3d->viewquat, q1);

    if (use_sel) {
        QuatConj(q1); /* conj == inv for unit quat */
        VecSubf(v3d->ofs, v3d->ofs, obofs);
        QuatMulVecf(q1, v3d->ofs);
        VecAddf(v3d->ofs, v3d->ofs, obofs);
    }

    /* Perform the orbital rotation */
    /* Perform the orbital rotation
       If the seen Up axis is parallel to the zoom axis, rotation should be
       achieved with a pure Roll motion (no Spin) on the device. When you start
       to tilt, moving from Top to Side view, Spinning will increasingly become
       more relevant while the Roll component will decrease. When a full
       Side view is reached, rotations around the world's Up axis are achieved
       with a pure Spin-only motion.  In other words the control of the spinning
       around the world's Up axis should move from the device's Spin axis to the
       device's Roll axis depending on the orientation of the world's Up axis
       relative to the screen. */
    //phi = sbadjust * rsens * reverse * fval[4];  /* spin the knob, y axis */
    phi = sbadjust * rsens * (yvec[2] * fval[4] + zvec[2] * fval[5]);
    q1[0] = cos(phi);
    q1[1] = q1[2] = 0.0;
    q1[3] = sin(phi);
    QuatMul(v3d->viewquat, v3d->viewquat, q1);

    if (use_sel) {
        QuatConj(q1);
        VecSubf(v3d->ofs, v3d->ofs, obofs);
        QuatMulVecf(q1, v3d->ofs);
        VecAddf(v3d->ofs, v3d->ofs, obofs);
    }

    /*----------------------------------------------------
     * refresh the screen
     */
// XXX    scrarea_do_windraw(curarea);
}




