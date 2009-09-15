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

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_anim.h"
#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "RE_pipeline.h"	// make_stars

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_armature.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_draw.h"

#include "PIL_time.h" /* smoothview */

#if GAMEBLENDER == 1
#include "SYS_System.h"
#endif

#include "view3d_intern.h"	// own include

/* use this call when executing an operator,
   event system doesn't set for each event the
   opengl drawing context */
void view3d_operator_needs_opengl(const bContext *C)
{
	ARegion *ar= CTX_wm_region(C);

	/* for debugging purpose, context should always be OK */
	if(ar->regiontype!=RGN_TYPE_WINDOW)
		printf("view3d_operator_needs_opengl error, wrong region\n");
	else {
		RegionView3D *rv3d= ar->regiondata;
		
		wmSubWindowSet(CTX_wm_window(C), ar->swinid);
		glMatrixMode(GL_PROJECTION);
		wmLoadMatrix(rv3d->winmat);
		glMatrixMode(GL_MODELVIEW);
		wmLoadMatrix(rv3d->viewmat);
	}
}

float *give_cursor(Scene *scene, View3D *v3d)
{
	if(v3d && v3d->localview) return v3d->cursor;
	else return scene->cursor;
}


/* Gets the lens and clipping values from a camera of lamp type object */
static void object_lens_clip_settings(Object *ob, float *lens, float *clipsta, float *clipend)
{	
	if (!ob) return;
	
	if(ob->type==OB_LAMP ) {
		Lamp *la = ob->data;
		if (lens) {
			float x1, fac;
			fac= cos( M_PI*la->spotsize/360.0);
			x1= saacos(fac);
			*lens= 16.0*fac/sin(x1);
		}
		if (clipsta)	*clipsta= la->clipsta;
		if (clipend)	*clipend= la->clipend;
	}
	else if(ob->type==OB_CAMERA) {
		Camera *cam= ob->data;
		if (lens)		*lens= cam->lens;
		if (clipsta)	*clipsta= cam->clipsta;
		if (clipend)	*clipend= cam->clipend;
	}
	else {
		if (lens)		*lens= 35.0f;
	}
}


/* Gets the view trasnformation from a camera
* currently dosnt take camzoom into account
* 
* The dist is not modified for this function, if NULL its assimed zero
* */
static void view_settings_from_ob(Object *ob, float *ofs, float *quat, float *dist, float *lens)
{	
	float bmat[4][4];
	float imat[4][4];
	float tmat[3][3];
	
	if (!ob) return;
	
	/* Offset */
	if (ofs) {
		VECCOPY(ofs, ob->obmat[3]);
		VecMulf(ofs, -1.0f); /*flip the vector*/
	}
	
	/* Quat */
	if (quat) {
		Mat4CpyMat4(bmat, ob->obmat);
		Mat4Ortho(bmat);
		Mat4Invert(imat, bmat);
		Mat3CpyMat4(tmat, imat);
		Mat3ToQuat(tmat, quat);
	}
	
	if (dist) {
		float vec[3];
		Mat3CpyMat4(tmat, ob->obmat);
		
		vec[0]= vec[1] = 0.0;
		vec[2]= -(*dist);
		Mat3MulVecfl(tmat, vec);
		VecSubf(ofs, ofs, vec);
	}
	
	/* Lens */
	if (lens)
		object_lens_clip_settings(ob, lens, NULL, NULL);
}


/* ****************** smooth view operator ****************** */

struct SmoothViewStore {
	float orig_dist, new_dist;
	float orig_lens, new_lens;
	float orig_quat[4], new_quat[4];
	float orig_ofs[3], new_ofs[3];
	
	int to_camera, orig_view;
	
	double time_allowed;
};

/* will start timer if appropriate */
/* the arguments are the desired situation */
void smooth_view(bContext *C, Object *oldcamera, Object *camera, float *ofs, float *quat, float *dist, float *lens)
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	struct SmoothViewStore sms;
	
	/* initialize sms */
	memset(&sms,0,sizeof(struct SmoothViewStore));
	VECCOPY(sms.new_ofs, rv3d->ofs);
	QUATCOPY(sms.new_quat, rv3d->viewquat);
	sms.new_dist= rv3d->dist;
	sms.new_lens= v3d->lens;
	sms.to_camera= 0;
	
	/* store the options we want to end with */
	if(ofs) VECCOPY(sms.new_ofs, ofs);
	if(quat) QUATCOPY(sms.new_quat, quat);
	if(dist) sms.new_dist= *dist;
	if(lens) sms.new_lens= *lens;
	
	if (camera) {
		view_settings_from_ob(camera, sms.new_ofs, sms.new_quat, &sms.new_dist, &sms.new_lens);
		sms.to_camera= 1; /* restore view3d values in end */
	}
	
	if (C && U.smooth_viewtx) {
		int changed = 0; /* zero means no difference */
		
		if (sms.new_dist != rv3d->dist)
			changed = 1;
		if (sms.new_lens != v3d->lens)
			changed = 1;
		
		if ((sms.new_ofs[0]!=rv3d->ofs[0]) ||
			(sms.new_ofs[1]!=rv3d->ofs[1]) ||
			(sms.new_ofs[2]!=rv3d->ofs[2]) )
			changed = 1;
		
		if ((sms.new_quat[0]!=rv3d->viewquat[0]) ||
			(sms.new_quat[1]!=rv3d->viewquat[1]) ||
			(sms.new_quat[2]!=rv3d->viewquat[2]) ||
			(sms.new_quat[3]!=rv3d->viewquat[3]) )
			changed = 1;
		
		/* The new view is different from the old one
			* so animate the view */
		if (changed) {
			
			sms.time_allowed= (double)U.smooth_viewtx / 1000.0;
			
			/* if this is view rotation only
				* we can decrease the time allowed by
				* the angle between quats 
				* this means small rotations wont lag */
			if (quat && !ofs && !dist) {
			 	float vec1[3], vec2[3];
				
			 	VECCOPY(vec1, sms.new_quat);
			 	VECCOPY(vec2, sms.orig_quat);
			 	Normalize(vec1);
			 	Normalize(vec2);
			 	/* scale the time allowed by the rotation */
			 	sms.time_allowed *= NormalizedVecAngle2(vec1, vec2)/(M_PI/2); 
			}
			
			/* original values */
			if (oldcamera) {
				sms.orig_dist= rv3d->dist; // below function does weird stuff with it...
				view_settings_from_ob(oldcamera, sms.orig_ofs, sms.orig_quat, &sms.orig_dist, &sms.orig_lens);
			}
			else {
				VECCOPY(sms.orig_ofs, rv3d->ofs);
				QUATCOPY(sms.orig_quat, rv3d->viewquat);
				sms.orig_dist= rv3d->dist;
				sms.orig_lens= v3d->lens;
			}
			/* grid draw as floor */
			sms.orig_view= rv3d->view;
			rv3d->view= 0;
			
			/* ensure it shows correct */
			if(sms.to_camera) rv3d->persp= V3D_PERSP;
			
			/* keep track of running timer! */
			if(rv3d->sms==NULL)
				rv3d->sms= MEM_mallocN(sizeof(struct SmoothViewStore), "smoothview v3d");
			*rv3d->sms= sms;
			if(rv3d->smooth_timer)
				WM_event_remove_window_timer(CTX_wm_window(C), rv3d->smooth_timer);
			/* TIMER1 is hardcoded in keymap */
			rv3d->smooth_timer= WM_event_add_window_timer(CTX_wm_window(C), TIMER1, 1.0/30.0);	/* max 30 frs/sec */
			
			return;
		}
	}
	
	/* if we get here nothing happens */
	if(sms.to_camera==0) {
		VECCOPY(rv3d->ofs, sms.new_ofs);
		QUATCOPY(rv3d->viewquat, sms.new_quat);
		rv3d->dist = sms.new_dist;
		v3d->lens = sms.new_lens;
	}
	ED_region_tag_redraw(CTX_wm_region(C));
}

/* only meant for timer usage */
static int view3d_smoothview_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	struct SmoothViewStore *sms= rv3d->sms;
	double step, step_inv;
	
	/* escape if not our timer */
	if(rv3d->smooth_timer==NULL || rv3d->smooth_timer!=event->customdata)
		return OPERATOR_PASS_THROUGH;
	
	step =  (rv3d->smooth_timer->duration)/sms->time_allowed;
	
	/* end timer */
	if(step >= 1.0f) {
		
		/* if we went to camera, store the original */
		if(sms->to_camera) {
			rv3d->persp= V3D_CAMOB;
			VECCOPY(rv3d->ofs, sms->orig_ofs);
			QUATCOPY(rv3d->viewquat, sms->orig_quat);
			rv3d->dist = sms->orig_dist;
			v3d->lens = sms->orig_lens;
		}
		else {
			VECCOPY(rv3d->ofs, sms->new_ofs);
			QUATCOPY(rv3d->viewquat, sms->new_quat);
			rv3d->dist = sms->new_dist;
			v3d->lens = sms->new_lens;
		}
		rv3d->view= sms->orig_view;
		
		MEM_freeN(rv3d->sms);
		rv3d->sms= NULL;
		
		WM_event_remove_window_timer(CTX_wm_window(C), rv3d->smooth_timer);
		rv3d->smooth_timer= NULL;
	}
	else {
		int i;
		
		/* ease in/out */
		if (step < 0.5)	step = (float)pow(step*2, 2)/2;
		else			step = (float)1-(pow(2*(1-step),2)/2);

		step_inv = 1.0-step;

		for (i=0; i<3; i++)
			rv3d->ofs[i] = sms->new_ofs[i]*step + sms->orig_ofs[i]*step_inv;

		QuatInterpol(rv3d->viewquat, sms->orig_quat, sms->new_quat, step);
		
		rv3d->dist = sms->new_dist*step + sms->orig_dist*step_inv;
		v3d->lens = sms->new_lens*step + sms->orig_lens*step_inv;
	}
	
	ED_region_tag_redraw(CTX_wm_region(C));
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_smoothview(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Smooth View";
	ot->idname= "VIEW3D_OT_smoothview";
	ot->description="The time to animate the change of view (in milliseconds)";
	
	/* api callbacks */
	ot->invoke= view3d_smoothview_invoke;
	
	ot->poll= ED_operator_view3d_active;
}

static int view3d_setcameratoview_exec(bContext *C, wmOperator *op)
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	Object *ob;
	float dvec[3];
	
	ob= v3d->camera;
	dvec[0]= rv3d->dist*rv3d->viewinv[2][0];
	dvec[1]= rv3d->dist*rv3d->viewinv[2][1];
	dvec[2]= rv3d->dist*rv3d->viewinv[2][2];
	
	VECCOPY(ob->loc, dvec);
	VecSubf(ob->loc, ob->loc, v3d->ofs);
	rv3d->viewquat[0]= -rv3d->viewquat[0];

	QuatToEul(rv3d->viewquat, ob->rot);
	rv3d->viewquat[0]= -rv3d->viewquat[0];
	
	ob->recalc= OB_RECALC_OB;
	
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;

}

void VIEW3D_OT_setcameratoview(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Align Camera To View";
	ot->description= "Set camera view to active view.";
	ot->idname= "VIEW3D_OT_camera_to_view";
	
	/* api callbacks */
	ot->exec= view3d_setcameratoview_exec;	
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ********************************** */

/* create intersection coordinates in view Z direction at mouse coordinates */
void viewline(ARegion *ar, View3D *v3d, short mval[2], float ray_start[3], float ray_end[3])
{
	RegionView3D *rv3d= ar->regiondata;
	float vec[4];
	
	if(rv3d->persp != V3D_ORTHO){
		vec[0]= 2.0f * mval[0] / ar->winx - 1;
		vec[1]= 2.0f * mval[1] / ar->winy - 1;
		vec[2]= -1.0f;
		vec[3]= 1.0f;
		
		Mat4MulVec4fl(rv3d->persinv, vec);
		VecMulf(vec, 1.0f / vec[3]);
		
		VECCOPY(ray_start, rv3d->viewinv[3]);
		VECSUB(vec, vec, ray_start);
		Normalize(vec);
		
		VECADDFAC(ray_start, rv3d->viewinv[3], vec, v3d->near);
		VECADDFAC(ray_end, rv3d->viewinv[3], vec, v3d->far);
	}
	else {
		vec[0] = 2.0f * mval[0] / ar->winx - 1;
		vec[1] = 2.0f * mval[1] / ar->winy - 1;
		vec[2] = 0.0f;
		vec[3] = 1.0f;
		
		Mat4MulVec4fl(rv3d->persinv, vec);
		
		VECADDFAC(ray_start, vec, rv3d->viewinv[2],  1000.0f);
		VECADDFAC(ray_end, vec, rv3d->viewinv[2], -1000.0f);
	}
}

/* create intersection ray in view Z direction at mouse coordinates */
void viewray(ARegion *ar, View3D *v3d, short mval[2], float ray_start[3], float ray_normal[3])
{
	float ray_end[3];
	
	viewline(ar, v3d, mval, ray_start, ray_end);
	VecSubf(ray_normal, ray_end, ray_start);
	Normalize(ray_normal);
}


void initgrabz(RegionView3D *rv3d, float x, float y, float z)
{
	if(rv3d==NULL) return;
	rv3d->zfac= rv3d->persmat[0][3]*x+ rv3d->persmat[1][3]*y+ rv3d->persmat[2][3]*z+ rv3d->persmat[3][3];
	
	/* if x,y,z is exactly the viewport offset, zfac is 0 and we don't want that 
		* (accounting for near zero values)
		* */
	if (rv3d->zfac < 1.e-6f && rv3d->zfac > -1.e-6f) rv3d->zfac = 1.0f;
	
	/* Negative zfac means x, y, z was behind the camera (in perspective).
		* This gives flipped directions, so revert back to ok default case.
	*/
	// NOTE: I've changed this to flip zfac to be positive again for now so that GPencil draws ok
	// 	-- Aligorith, 2009Aug31
	//if (rv3d->zfac < 0.0f) rv3d->zfac = 1.0f;
	if (rv3d->zfac < 0.0f) rv3d->zfac= -rv3d->zfac;
}

/* always call initgrabz */
void window_to_3d(ARegion *ar, float *vec, short mx, short my)
{
	RegionView3D *rv3d= ar->regiondata;
	
	float dx= ((float)(mx-(ar->winx/2)))*rv3d->zfac/(ar->winx/2);
	float dy= ((float)(my-(ar->winy/2)))*rv3d->zfac/(ar->winy/2);
	
	float fz= rv3d->persmat[0][3]*vec[0]+ rv3d->persmat[1][3]*vec[1]+ rv3d->persmat[2][3]*vec[2]+ rv3d->persmat[3][3];
	fz= fz/rv3d->zfac;
	
	vec[0]= (rv3d->persinv[0][0]*dx + rv3d->persinv[1][0]*dy+ rv3d->persinv[2][0]*fz)-rv3d->ofs[0];
	vec[1]= (rv3d->persinv[0][1]*dx + rv3d->persinv[1][1]*dy+ rv3d->persinv[2][1]*fz)-rv3d->ofs[1];
	vec[2]= (rv3d->persinv[0][2]*dx + rv3d->persinv[1][2]*dy+ rv3d->persinv[2][2]*fz)-rv3d->ofs[2];
	
}

/* always call initgrabz */
/* only to detect delta motion */
void window_to_3d_delta(ARegion *ar, float *vec, short mx, short my)
{
	RegionView3D *rv3d= ar->regiondata;
	float dx, dy;
	
	dx= 2.0f*mx*rv3d->zfac/ar->winx;
	dy= 2.0f*my*rv3d->zfac/ar->winy;
	
	vec[0]= (rv3d->persinv[0][0]*dx + rv3d->persinv[1][0]*dy);
	vec[1]= (rv3d->persinv[0][1]*dx + rv3d->persinv[1][1]*dy);
	vec[2]= (rv3d->persinv[0][2]*dx + rv3d->persinv[1][2]*dy);
}

float read_cached_depth(ViewContext *vc, int x, int y)
{
	ViewDepths *vd = vc->rv3d->depths;
		
	x -= vc->ar->winrct.xmin;
	y -= vc->ar->winrct.ymin;

	if(vd && vd->depths && x > 0 && y > 0 && x < vd->w && y < vd->h)
		return vd->depths[y * vd->w + x];
	else
		return 1;
}

void request_depth_update(RegionView3D *rv3d)
{
	if(rv3d->depths)
		rv3d->depths->damaged= 1;
}

void view3d_get_object_project_mat(RegionView3D *rv3d, Object *ob, float pmat[4][4])
{
	float vmat[4][4];
	
	Mat4MulMat4(vmat, ob->obmat, rv3d->viewmat);
	Mat4MulMat4(pmat, vmat, rv3d->winmat);
}

/* Uses window coordinates (x,y) and depth component z to find a point in
   modelspace */
void view3d_unproject(bglMats *mats, float out[3], const short x, const short y, const float z)
{
	double ux, uy, uz;

        gluUnProject(x,y,z, mats->modelview, mats->projection,
		     (GLint *)mats->viewport, &ux, &uy, &uz );
	out[0] = ux;
	out[1] = uy;
	out[2] = uz;
}

/* use above call to get projecting mat */
void view3d_project_float(ARegion *ar, float *vec, float *adr, float mat[4][4])
{
	float vec4[4];
	
	adr[0]= IS_CLIPPED;
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(mat, vec4);
	
	if( vec4[3]>FLT_EPSILON ) {
		adr[0] = (float)(ar->winx/2.0f)+(ar->winx/2.0f)*vec4[0]/vec4[3];	
		adr[1] = (float)(ar->winy/2.0f)+(ar->winy/2.0f)*vec4[1]/vec4[3];
	} else {
		adr[0] = adr[1] = 0.0f;
	}
}

int boundbox_clip(RegionView3D *rv3d, float obmat[][4], BoundBox *bb)
{
	/* return 1: draw */
	
	float mat[4][4];
	float vec[4], min, max;
	int a, flag= -1, fl;
	
	if(bb==NULL) return 1;
	if(bb->flag & OB_BB_DISABLED) return 1;
	
	Mat4MulMat4(mat, obmat, rv3d->persmat);
	
	for(a=0; a<8; a++) {
		VECCOPY(vec, bb->vec[a]);
		vec[3]= 1.0;
		Mat4MulVec4fl(mat, vec);
		max= vec[3];
		min= -vec[3];
		
		fl= 0;
		if(vec[0] < min) fl+= 1;
		if(vec[0] > max) fl+= 2;
		if(vec[1] < min) fl+= 4;
		if(vec[1] > max) fl+= 8;
		if(vec[2] < min) fl+= 16;
		if(vec[2] > max) fl+= 32;
		
		flag &= fl;
		if(flag==0) return 1;
	}
	
	return 0;
}

void project_short(ARegion *ar, float *vec, short *adr)	/* clips */
{
	RegionView3D *rv3d= ar->regiondata;
	float fx, fy, vec4[4];
	
	adr[0]= IS_CLIPPED;
	
	if(rv3d->rflag & RV3D_CLIPPING) {
		if(view3d_test_clipping(rv3d, vec))
			return;
	}
	
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	Mat4MulVec4fl(rv3d->persmat, vec4);
	
	if( vec4[3]>BL_NEAR_CLIP ) {	/* 0.001 is the NEAR clipping cutoff for picking */
		fx= (ar->winx/2)*(1 + vec4[0]/vec4[3]);
		
		if( fx>0 && fx<ar->winx) {
			
			fy= (ar->winy/2)*(1 + vec4[1]/vec4[3]);
			
			if(fy>0.0 && fy< (float)ar->winy) {
				adr[0]= (short)floor(fx); 
				adr[1]= (short)floor(fy);
			}
		}
	}
}

void project_int(ARegion *ar, float *vec, int *adr)
{
	RegionView3D *rv3d= ar->regiondata;
	float fx, fy, vec4[4];
	
	adr[0]= (int)2140000000.0f;
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(rv3d->persmat, vec4);
	
	if( vec4[3]>BL_NEAR_CLIP ) {	/* 0.001 is the NEAR clipping cutoff for picking */
		fx= (ar->winx/2)*(1 + vec4[0]/vec4[3]);
		
		if( fx>-2140000000.0f && fx<2140000000.0f) {
			fy= (ar->winy/2)*(1 + vec4[1]/vec4[3]);
			
			if(fy>-2140000000.0f && fy<2140000000.0f) {
				adr[0]= (int)floor(fx); 
				adr[1]= (int)floor(fy);
			}
		}
	}
}

void project_int_noclip(ARegion *ar, float *vec, int *adr)
{
	RegionView3D *rv3d= ar->regiondata;
	float fx, fy, vec4[4];
	
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(rv3d->persmat, vec4);
	
	if( fabs(vec4[3]) > BL_NEAR_CLIP ) {
		fx = (ar->winx/2)*(1 + vec4[0]/vec4[3]);
		fy = (ar->winy/2)*(1 + vec4[1]/vec4[3]);
		
		adr[0] = (int)floor(fx); 
		adr[1] = (int)floor(fy);
	}
	else
	{
		adr[0] = ar->winx / 2;
		adr[1] = ar->winy / 2;
	}
}

void project_short_noclip(ARegion *ar, float *vec, short *adr)
{
	RegionView3D *rv3d= ar->regiondata;
	float fx, fy, vec4[4];
	
	adr[0]= IS_CLIPPED;
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(rv3d->persmat, vec4);
	
	if( vec4[3]>BL_NEAR_CLIP ) {	/* 0.001 is the NEAR clipping cutoff for picking */
		fx= (ar->winx/2)*(1 + vec4[0]/vec4[3]);
		
		if( fx>-32700 && fx<32700) {
			
			fy= (ar->winy/2)*(1 + vec4[1]/vec4[3]);
			
			if(fy>-32700.0 && fy<32700.0) {
				adr[0]= (short)floor(fx); 
				adr[1]= (short)floor(fy);
			}
		}
	}
}

void project_float(ARegion *ar, float *vec, float *adr)
{
	RegionView3D *rv3d= ar->regiondata;
	float vec4[4];
	
	adr[0]= IS_CLIPPED;
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(rv3d->persmat, vec4);
	
	if( vec4[3]>BL_NEAR_CLIP ) {
		adr[0] = (float)(ar->winx/2.0)+(ar->winx/2.0)*vec4[0]/vec4[3];	
		adr[1] = (float)(ar->winy/2.0)+(ar->winy/2.0)*vec4[1]/vec4[3];
	}
}

void project_float_noclip(ARegion *ar, float *vec, float *adr)
{
	RegionView3D *rv3d= ar->regiondata;
	float vec4[4];
	
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(rv3d->persmat, vec4);
	
	if( fabs(vec4[3]) > BL_NEAR_CLIP ) {
		adr[0] = (float)(ar->winx/2.0)+(ar->winx/2.0)*vec4[0]/vec4[3];	
		adr[1] = (float)(ar->winy/2.0)+(ar->winy/2.0)*vec4[1]/vec4[3];
	}
	else
	{
		adr[0] = ar->winx / 2.0f;
		adr[1] = ar->winy / 2.0f;
	}
}

int get_view3d_ortho(View3D *v3d, RegionView3D *rv3d)
{
  Camera *cam;
  
  if(rv3d->persp==V3D_CAMOB) {
      if(v3d->camera && v3d->camera->type==OB_CAMERA) {
          cam= v3d->camera->data;

          if(cam && cam->type==CAM_ORTHO)
              return 1;
          else
              return 0;
      }
      else
          return 0;
  }
  
  if(rv3d->persp==V3D_ORTHO)
      return 1;

  return 0;
}

/* also exposed in previewrender.c */
int get_view3d_viewplane(View3D *v3d, RegionView3D *rv3d, int winxi, int winyi, rctf *viewplane, float *clipsta, float *clipend, float *pixsize)
{
	Camera *cam=NULL;
	float lens, fac, x1, y1, x2, y2;
	float winx= (float)winxi, winy= (float)winyi;
	int orth= 0;
	
	lens= v3d->lens;	
	
	*clipsta= v3d->near;
	*clipend= v3d->far;
	
	if(rv3d->persp==V3D_CAMOB) {
		if(v3d->camera) {
			if(v3d->camera->type==OB_LAMP ) {
				Lamp *la;
				
				la= v3d->camera->data;
				fac= cos( M_PI*la->spotsize/360.0);
				
				x1= saacos(fac);
				lens= 16.0*fac/sin(x1);
				
				*clipsta= la->clipsta;
				*clipend= la->clipend;
			}
			else if(v3d->camera->type==OB_CAMERA) {
				cam= v3d->camera->data;
				lens= cam->lens;
				*clipsta= cam->clipsta;
				*clipend= cam->clipend;
			}
		}
	}
	
	if(rv3d->persp==V3D_ORTHO) {
		if(winx>winy) x1= -rv3d->dist;
		else x1= -winx*rv3d->dist/winy;
		x2= -x1;
		
		if(winx>winy) y1= -winy*rv3d->dist/winx;
		else y1= -rv3d->dist;
		y2= -y1;
		
		*clipend *= 0.5;	// otherwise too extreme low zbuffer quality
		*clipsta= - *clipend;
		orth= 1;
	}
	else {
		/* fac for zoom, also used for camdx */
		if(rv3d->persp==V3D_CAMOB) {
			fac= (1.41421+( (float)rv3d->camzoom )/50.0);
			fac*= fac;
		}
		else fac= 2.0;
		
		/* viewplane size depends... */
		if(cam && cam->type==CAM_ORTHO) {
			/* ortho_scale == 1 means exact 1 to 1 mapping */
			float dfac= 2.0*cam->ortho_scale/fac;
			
			if(winx>winy) x1= -dfac;
			else x1= -winx*dfac/winy;
			x2= -x1;
			
			if(winx>winy) y1= -winy*dfac/winx;
			else y1= -dfac;
			y2= -y1;
			orth= 1;
		}
		else {
			float dfac;
			
			if(winx>winy) dfac= 64.0/(fac*winx*lens);
			else dfac= 64.0/(fac*winy*lens);
			
			x1= - *clipsta * winx*dfac;
			x2= -x1;
			y1= - *clipsta * winy*dfac;
			y2= -y1;
			orth= 0;
		}
		/* cam view offset */
		if(cam) {
			float dx= 0.5*fac*rv3d->camdx*(x2-x1);
			float dy= 0.5*fac*rv3d->camdy*(y2-y1);
			x1+= dx;
			x2+= dx;
			y1+= dy;
			y2+= dy;
		}
	}
	
	if(pixsize) {
		float viewfac;
		
		if(orth) {
			viewfac= (winx >= winy)? winx: winy;
			*pixsize= 1.0f/viewfac;
		}
		else {
			viewfac= (((winx >= winy)? winx: winy)*lens)/32.0;
			*pixsize= *clipsta/viewfac;
		}
	}
	
	viewplane->xmin= x1;
	viewplane->ymin= y1;
	viewplane->xmax= x2;
	viewplane->ymax= y2;
	
	return orth;
}

void setwinmatrixview3d(ARegion *ar, View3D *v3d, rctf *rect)		/* rect: for picking */
{
	RegionView3D *rv3d= ar->regiondata;
	rctf viewplane;
	float clipsta, clipend, x1, y1, x2, y2;
	int orth;
	
	orth= get_view3d_viewplane(v3d, rv3d, ar->winx, ar->winy, &viewplane, &clipsta, &clipend, NULL);
	//	printf("%d %d %f %f %f %f %f %f\n", winx, winy, viewplane.xmin, viewplane.ymin, viewplane.xmax, viewplane.ymax, clipsta, clipend);
	x1= viewplane.xmin;
	y1= viewplane.ymin;
	x2= viewplane.xmax;
	y2= viewplane.ymax;
	
	if(rect) {		/* picking */
		rect->xmin/= (float)ar->winx;
		rect->xmin= x1+rect->xmin*(x2-x1);
		rect->ymin/= (float)ar->winy;
		rect->ymin= y1+rect->ymin*(y2-y1);
		rect->xmax/= (float)ar->winx;
		rect->xmax= x1+rect->xmax*(x2-x1);
		rect->ymax/= (float)ar->winy;
		rect->ymax= y1+rect->ymax*(y2-y1);
		
		if(orth) wmOrtho(rect->xmin, rect->xmax, rect->ymin, rect->ymax, -clipend, clipend);
		else wmFrustum(rect->xmin, rect->xmax, rect->ymin, rect->ymax, clipsta, clipend);
		
	}
	else {
		if(orth) wmOrtho(x1, x2, y1, y2, clipsta, clipend);
		else wmFrustum(x1, x2, y1, y2, clipsta, clipend);
	}

	/* not sure what this was for? (ton) */
	glMatrixMode(GL_PROJECTION);
	wmGetMatrix(rv3d->winmat);
	glMatrixMode(GL_MODELVIEW);
}


static void obmat_to_viewmat(View3D *v3d, RegionView3D *rv3d, Object *ob, short smooth)
{
	float bmat[4][4];
	float tmat[3][3];
	
	rv3d->view= 0; /* dont show the grid */
	
	Mat4CpyMat4(bmat, ob->obmat);
	Mat4Ortho(bmat);
	Mat4Invert(rv3d->viewmat, bmat);
	
	/* view quat calculation, needed for add object */
	Mat3CpyMat4(tmat, rv3d->viewmat);
	if (smooth) {
		float new_quat[4];
		if (rv3d->persp==V3D_CAMOB && v3d->camera) {
			/* were from a camera view */
			
			float orig_ofs[3];
			float orig_dist= rv3d->dist;
			float orig_lens= v3d->lens;
			VECCOPY(orig_ofs, rv3d->ofs);
			
			/* Switch from camera view */
			Mat3ToQuat(tmat, new_quat);
			
			rv3d->persp=V3D_PERSP;
			rv3d->dist= 0.0;
			
			view_settings_from_ob(v3d->camera, rv3d->ofs, NULL, NULL, &v3d->lens);
			smooth_view(NULL, NULL, NULL, orig_ofs, new_quat, &orig_dist, &orig_lens); // XXX
			
			rv3d->persp=V3D_CAMOB; /* just to be polite, not needed */
			
		} else {
			Mat3ToQuat(tmat, new_quat);
			smooth_view(NULL, NULL, NULL, NULL, new_quat, NULL, NULL); // XXX
		}
	} else {
		Mat3ToQuat(tmat, rv3d->viewquat);
	}
}

#define QUATSET(a, b, c, d, e)	a[0]=b; a[1]=c; a[2]=d; a[3]=e; 

static void view3d_viewlock(RegionView3D *rv3d)
{
	switch(rv3d->view) {
	case V3D_VIEW_BOTTOM :
		QUATSET(rv3d->viewquat,0.0, -1.0, 0.0, 0.0);
		break;
		
	case V3D_VIEW_BACK:
		QUATSET(rv3d->viewquat,0.0, 0.0, (float)-cos(M_PI/4.0), (float)-cos(M_PI/4.0));
		break;
		
	case V3D_VIEW_LEFT:
		QUATSET(rv3d->viewquat,0.5, -0.5, 0.5, 0.5);
		break;
		
	case V3D_VIEW_TOP:
		QUATSET(rv3d->viewquat,1.0, 0.0, 0.0, 0.0);
		break;
		
	case V3D_VIEW_FRONT:
		QUATSET(rv3d->viewquat,(float)cos(M_PI/4.0), (float)-sin(M_PI/4.0), 0.0, 0.0);
		break;
		
	case V3D_VIEW_RIGHT:
		QUATSET(rv3d->viewquat, 0.5, -0.5, -0.5, -0.5);
		break;
	}
}

/* dont set windows active in in here, is used by renderwin too */
void setviewmatrixview3d(Scene *scene, View3D *v3d, RegionView3D *rv3d)
{
	if(rv3d->persp==V3D_CAMOB) {	    /* obs/camera */
		if(v3d->camera) {
			where_is_object(scene, v3d->camera);	
			obmat_to_viewmat(v3d, rv3d, v3d->camera, 0);
		}
		else {
			QuatToMat4(rv3d->viewquat, rv3d->viewmat);
			rv3d->viewmat[3][2]-= rv3d->dist;
		}
	}
	else {
		/* should be moved to better initialize later on XXX */
		if(rv3d->viewlock)
			view3d_viewlock(rv3d);
		
		QuatToMat4(rv3d->viewquat, rv3d->viewmat);
		if(rv3d->persp==V3D_PERSP) rv3d->viewmat[3][2]-= rv3d->dist;
		if(v3d->ob_centre) {
			Object *ob= v3d->ob_centre;
			float vec[3];
			
			VECCOPY(vec, ob->obmat[3]);
			if(ob->type==OB_ARMATURE && v3d->ob_centre_bone[0]) {
				bPoseChannel *pchan= get_pose_channel(ob->pose, v3d->ob_centre_bone);
				if(pchan) {
					VECCOPY(vec, pchan->pose_mat[3]);
					Mat4MulVecfl(ob->obmat, vec);
				}
			}
			i_translate(-vec[0], -vec[1], -vec[2], rv3d->viewmat);
		}
		else i_translate(rv3d->ofs[0], rv3d->ofs[1], rv3d->ofs[2], rv3d->viewmat);
	}
}

/* IGLuint-> GLuint*/
/* Warning: be sure to account for a negative return value
*   This is an error, "Too many objects in select buffer"
*   and no action should be taken (can crash blender) if this happens
*/
short view3d_opengl_select(ViewContext *vc, unsigned int *buffer, unsigned int bufsize, rcti *input)
{
	Scene *scene= vc->scene;
	View3D *v3d= vc->v3d;
	ARegion *ar= vc->ar;
	rctf rect;
	short code, hits;
	
	G.f |= G_PICKSEL;
	
	/* case not a border select */
	if(input->xmin==input->xmax) {
		rect.xmin= input->xmin-12;	// seems to be default value for bones only now
		rect.xmax= input->xmin+12;
		rect.ymin= input->ymin-12;
		rect.ymax= input->ymin+12;
	}
	else {
		rect.xmin= input->xmin;
		rect.xmax= input->xmax;
		rect.ymin= input->ymin;
		rect.ymax= input->ymax;
	}
	
	setwinmatrixview3d(ar, v3d, &rect);
	Mat4MulMat4(vc->rv3d->persmat, vc->rv3d->viewmat, vc->rv3d->winmat);
	
	if(v3d->drawtype > OB_WIRE) {
		v3d->zbuf= TRUE;
		glEnable(GL_DEPTH_TEST);
	}
	
	if(vc->rv3d->rflag & RV3D_CLIPPING)
		view3d_set_clipping(vc->rv3d);
	
	glSelectBuffer( bufsize, (GLuint *)buffer);
	glRenderMode(GL_SELECT);
	glInitNames();	/* these two calls whatfor? It doesnt work otherwise */
	glPushName(-1);
	code= 1;
	
	if(vc->obedit && vc->obedit->type==OB_MBALL) {
		draw_object(scene, ar, v3d, BASACT, DRAW_PICKING|DRAW_CONSTCOLOR);
	}
	else if((vc->obedit && vc->obedit->type==OB_ARMATURE)) {
		/* if not drawing sketch, draw bones */
		if(!BDR_drawSketchNames(vc)) {
			draw_object(scene, ar, v3d, BASACT, DRAW_PICKING|DRAW_CONSTCOLOR);
		}
	}
	else {
		Base *base;
		
		v3d->xray= TRUE;	// otherwise it postpones drawing
		for(base= scene->base.first; base; base= base->next) {
			if(base->lay & v3d->lay) {
				
				if (base->object->restrictflag & OB_RESTRICT_SELECT)
					base->selcol= 0;
				else {
					base->selcol= code;
					glLoadName(code);
					draw_object(scene, ar, v3d, base, DRAW_PICKING|DRAW_CONSTCOLOR);
					
					/* we draw group-duplicators for selection too */
					if((base->object->transflag & OB_DUPLI) && base->object->dup_group) {
						ListBase *lb;
						DupliObject *dob;
						Base tbase;
						
						tbase.flag= OB_FROMDUPLI;
						lb= object_duplilist(scene, base->object);
						
						for(dob= lb->first; dob; dob= dob->next) {
							tbase.object= dob->ob;
							Mat4CpyMat4(dob->ob->obmat, dob->mat);
							
							draw_object(scene, ar, v3d, &tbase, DRAW_PICKING|DRAW_CONSTCOLOR);
							
							Mat4CpyMat4(dob->ob->obmat, dob->omat);
						}
						free_object_duplilist(lb);
					}
					code++;
				}				
			}
		}
		v3d->xray= FALSE;	// restore
	}
	
	glPopName();	/* see above (pushname) */
	hits= glRenderMode(GL_RENDER);
	
	G.f &= ~G_PICKSEL;
	setwinmatrixview3d(ar, v3d, NULL);
	Mat4MulMat4(vc->rv3d->persmat, vc->rv3d->viewmat, vc->rv3d->winmat);
	
	if(v3d->drawtype > OB_WIRE) {
		v3d->zbuf= 0;
		glDisable(GL_DEPTH_TEST);
	}
// XXX	persp(PERSP_WIN);
	
	if(vc->rv3d->rflag & RV3D_CLIPPING)
		view3d_clr_clipping();
	
	if(hits<0) printf("Too many objects in select buffer\n");	// XXX make error message
	
	return hits;
}

/* ********************** local view operator ******************** */

static unsigned int free_localbit(void)
{
	unsigned int lay;
	ScrArea *sa;
	bScreen *sc;
	
	lay= 0;
	
	/* sometimes we loose a localview: when an area is closed */
	/* check all areas: which localviews are in use? */
	for(sc= G.main->screen.first; sc; sc= sc->id.next) {
		for(sa= sc->areabase.first; sa; sa= sa->next) {
			SpaceLink *sl= sa->spacedata.first;
			for(; sl; sl= sl->next) {
				if(sl->spacetype==SPACE_VIEW3D) {
					View3D *v3d= (View3D*) sl;
					lay |= v3d->lay;
				}
			}
		}
	}
	
	if( (lay & 0x01000000)==0) return 0x01000000;
	if( (lay & 0x02000000)==0) return 0x02000000;
	if( (lay & 0x04000000)==0) return 0x04000000;
	if( (lay & 0x08000000)==0) return 0x08000000;
	if( (lay & 0x10000000)==0) return 0x10000000;
	if( (lay & 0x20000000)==0) return 0x20000000;
	if( (lay & 0x40000000)==0) return 0x40000000;
	if( (lay & 0x80000000)==0) return 0x80000000;
	
	return 0;
}


static void initlocalview(Scene *scene, ScrArea *sa)
{
	View3D *v3d= sa->spacedata.first;
	Base *base;
	float size = 0.0, min[3], max[3], box[3];
	unsigned int locallay;
	int ok=0;

	if(v3d->localvd) return;

	INIT_MINMAX(min, max);

	locallay= free_localbit();

	if(locallay==0) {
		printf("Sorry, no more than 8 localviews\n");	// XXX error 
		ok= 0;
	}
	else {
		if(scene->obedit) {
			minmax_object(scene->obedit, min, max);
			
			ok= 1;
		
			BASACT->lay |= locallay;
			scene->obedit->lay= BASACT->lay;
		}
		else {
			for(base= FIRSTBASE; base; base= base->next) {
				if(TESTBASE(v3d, base))  {
					minmax_object(base->object, min, max);
					base->lay |= locallay;
					base->object->lay= base->lay;
					ok= 1;
				}
			}
		}
		
		box[0]= (max[0]-min[0]);
		box[1]= (max[1]-min[1]);
		box[2]= (max[2]-min[2]);
		size= MAX3(box[0], box[1], box[2]);
		if(size<=0.01) size= 0.01;
	}
	
	if(ok) {
		ARegion *ar;
		
		v3d->localvd= MEM_mallocN(sizeof(View3D), "localview");
		
		memcpy(v3d->localvd, v3d, sizeof(View3D));

		for(ar= sa->regionbase.first; ar; ar= ar->next) {
			if(ar->regiontype == RGN_TYPE_WINDOW) {
				RegionView3D *rv3d= ar->regiondata;

				rv3d->localvd= MEM_mallocN(sizeof(RegionView3D), "localview region");
				memcpy(rv3d->localvd, rv3d, sizeof(RegionView3D));
				
				rv3d->ofs[0]= -(min[0]+max[0])/2.0;
				rv3d->ofs[1]= -(min[1]+max[1])/2.0;
				rv3d->ofs[2]= -(min[2]+max[2])/2.0;

				rv3d->dist= size;
				/* perspective should be a bit farther away to look nice */
				if(rv3d->persp==V3D_ORTHO)
					rv3d->dist*= 0.7;

				// correction for window aspect ratio
				if(ar->winy>2 && ar->winx>2) {
					float asp= (float)ar->winx/(float)ar->winy;
					if(asp<1.0) asp= 1.0/asp;
					rv3d->dist*= asp;
				}
				
				if (rv3d->persp==V3D_CAMOB) rv3d->persp= V3D_PERSP;
				
				v3d->cursor[0]= -rv3d->ofs[0];
				v3d->cursor[1]= -rv3d->ofs[1];
				v3d->cursor[2]= -rv3d->ofs[2];
			}
		}
		if (v3d->near> 0.1) v3d->near= 0.1;
		
		v3d->lay= locallay;
	}
	else {
		/* clear flags */ 
		for(base= FIRSTBASE; base; base= base->next) {
			if( base->lay & locallay ) {
				base->lay-= locallay;
				if(base->lay==0) base->lay= v3d->layact;
				if(base->object != scene->obedit) base->flag |= SELECT;
				base->object->lay= base->lay;
			}
		}		
		v3d->localview= 0;
	}

}

static void restore_localviewdata(ScrArea *sa, int free)
{
	ARegion *ar;
	View3D *v3d= sa->spacedata.first;
	
	if(v3d->localvd==NULL) return;
	
	v3d->near= v3d->localvd->near;
	v3d->far= v3d->localvd->far;
	v3d->lay= v3d->localvd->lay;
	v3d->layact= v3d->localvd->layact;
	v3d->drawtype= v3d->localvd->drawtype;
	v3d->camera= v3d->localvd->camera;
	
	if(free) {
		MEM_freeN(v3d->localvd);
		v3d->localvd= NULL;
		v3d->localview= 0;
	}
	
	for(ar= sa->regionbase.first; ar; ar= ar->next) {
		if(ar->regiontype == RGN_TYPE_WINDOW) {
			RegionView3D *rv3d= ar->regiondata;
			
			if(rv3d->localvd) {
				rv3d->dist= rv3d->localvd->dist;
				VECCOPY(rv3d->ofs, rv3d->localvd->ofs);
				QUATCOPY(rv3d->viewquat, rv3d->localvd->viewquat);
				rv3d->view= rv3d->localvd->view;
				rv3d->persp= rv3d->localvd->persp;
				rv3d->camzoom= rv3d->localvd->camzoom;

				if(free) {
					MEM_freeN(rv3d->localvd);
					rv3d->localvd= NULL;
				}
			}
		}
	}
}

static void endlocalview(Scene *scene, ScrArea *sa)
{
	View3D *v3d= sa->spacedata.first;
	struct Base *base;
	unsigned int locallay;
	
	if(v3d->localvd) {
		
		locallay= v3d->lay & 0xFF000000;
		
		restore_localviewdata(sa, 1); // 1 = free

		/* for when in other window the layers have changed */
		if(v3d->scenelock) v3d->lay= scene->lay;
		
		for(base= FIRSTBASE; base; base= base->next) {
			if( base->lay & locallay ) {
				base->lay-= locallay;
				if(base->lay==0) base->lay= v3d->layact;
				if(base->object != scene->obedit) {
					base->flag |= SELECT;
					base->object->flag |= SELECT;
				}
				base->object->lay= base->lay;
			}
		}
	} 
}

static int localview_exec(bContext *C, wmOperator *unused)
{
	View3D *v3d= CTX_wm_view3d(C);
	
	if(v3d->localvd)
		endlocalview(CTX_data_scene(C), CTX_wm_area(C));
	else
		initlocalview(CTX_data_scene(C), CTX_wm_area(C));
	
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_localview(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Local View";
	ot->description= "Toggle display of selected object(s) seperately and centered in view.";
	ot->idname= "VIEW3D_OT_localview";
	
	/* api callbacks */
	ot->exec= localview_exec;
	
	ot->poll= ED_operator_view3d_active;
}

#if GAMEBLENDER == 1

static ListBase queue_back;
static void SaveState(bContext *C)
{
	wmWindow *win= CTX_wm_window(C);
	Object *obact = CTX_data_active_object(C);
	
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	GPU_state_init();

	if(obact && obact->mode & OB_MODE_TEXTURE_PAINT)
		GPU_paint_set_mipmap(1);
	
	queue_back= win->queue;
	
	win->queue.first= win->queue.last= NULL;
	
	//XXX waitcursor(1);
}

static void RestoreState(bContext *C)
{
	wmWindow *win= CTX_wm_window(C);
	Object *obact = CTX_data_active_object(C);
	
	if(obact && obact->mode & OB_MODE_TEXTURE_PAINT)
		GPU_paint_set_mipmap(0);

	//XXX curarea->win_swap = 0;
	//XXX curarea->head_swap=0;
	//XXX allqueue(REDRAWVIEW3D, 1);
	//XXX allqueue(REDRAWBUTSALL, 0);
	//XXX reset_slowparents();
	//XXX waitcursor(0);
	//XXX G.qual= 0;
	
	win->queue= queue_back;
	
	glPopAttrib();
}

/* was space_set_commmandline_options in 2.4x */
void game_set_commmandline_options(GameData *gm)
{
	SYS_SystemHandle syshandle;
	int test;

	if ( (syshandle = SYS_GetSystem()) ) {
		/* User defined settings */
		test= (U.gameflags & USER_DISABLE_SOUND);
		/* if user already disabled audio at the command-line, don't re-enable it */
		if (test)
			SYS_WriteCommandLineInt(syshandle, "noaudio", test);

		test= (U.gameflags & USER_DISABLE_MIPMAP);
		GPU_set_mipmap(!test);
		SYS_WriteCommandLineInt(syshandle, "nomipmap", test);

		/* File specific settings: */
		/* Only test the first one. These two are switched
		 * simultaneously. */
		test= (gm->flag & GAME_SHOW_FRAMERATE);
		SYS_WriteCommandLineInt(syshandle, "show_framerate", test);
		SYS_WriteCommandLineInt(syshandle, "show_profile", test);

		test = (gm->flag & GAME_SHOW_FRAMERATE);
		SYS_WriteCommandLineInt(syshandle, "show_properties", test);

		test= (gm->flag & GAME_SHOW_PHYSICS);
		SYS_WriteCommandLineInt(syshandle, "show_physics", test);

		test= (gm->flag & GAME_ENABLE_ALL_FRAMES);
		SYS_WriteCommandLineInt(syshandle, "fixedtime", test);

//		a= (G.fileflags & G_FILE_GAME_TO_IPO);
//		SYS_WriteCommandLineInt(syshandle, "game2ipo", a);

		test= (gm->flag & GAME_IGNORE_DEPRECATION_WARNINGS);
		SYS_WriteCommandLineInt(syshandle, "ignore_deprecation_warnings", test);

		test= (gm->matmode == GAME_MAT_MULTITEX);
		SYS_WriteCommandLineInt(syshandle, "blender_material", test);
		test= (gm->matmode == GAME_MAT_GLSL);
		SYS_WriteCommandLineInt(syshandle, "blender_glsl_material", test);
		test= (gm->flag & GAME_DISPLAY_LISTS);
		SYS_WriteCommandLineInt(syshandle, "displaylists", test);


	}
}

/* maybe we need this defined somewhere else */
extern void StartKetsjiShell(struct bContext *C, struct ARegion *ar, int always_use_expand_framing);

#endif // GAMEBLENDER == 1

int game_engine_poll(bContext *C)
{
	return CTX_data_mode_enum(C)==CTX_MODE_OBJECT ? 1:0;
}

static int game_engine_exec(bContext *C, wmOperator *unused)
{
#if GAMEBLENDER == 1
	Scene *startscene = CTX_data_scene(C);
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa, *prevsa= CTX_wm_area(C);
	ARegion *ar, *prevar= CTX_wm_region(C);

	sa= prevsa;
	if(sa->spacetype != SPACE_VIEW3D) {
		for(sa=sc->areabase.first; sa; sa= sa->next)
			if(sa->spacetype==SPACE_VIEW3D)
				break;
	}

	if(!sa)
		return OPERATOR_CANCELLED;
	
	for(ar=sa->regionbase.first; ar; ar=ar->next)
		if(ar->regiontype == RGN_TYPE_WINDOW)
			break;
	
	if(!ar)
		return OPERATOR_CANCELLED;
	
	// bad context switch ..
	CTX_wm_area_set(C, sa);
	CTX_wm_region_set(C, ar);

	view3d_operator_needs_opengl(C);
	
	game_set_commmandline_options(&startscene->gm);

	SaveState(C);
	StartKetsjiShell(C, ar, 1);
	RestoreState(C);
	
	CTX_wm_region_set(C, prevar);
	CTX_wm_area_set(C, prevsa);

	//XXX restore_all_scene_cfra(scene_cfra_store);
	set_scene_bg(startscene);
	//XXX scene_update_for_newframe(G.scene, G.scene->lay);
	
#else
	printf("GameEngine Disabled\n");
#endif
	ED_area_tag_redraw(CTX_wm_area(C));
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_game_start(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Start Game Engine";
	ot->description= "Start game engine.";
	ot->idname= "VIEW3D_OT_game_start";
	
	/* api callbacks */
	ot->exec= game_engine_exec;
	
	ot->poll= game_engine_poll;
}

/* ************************************** */

void view3d_align_axis_to_vector(View3D *v3d, RegionView3D *rv3d, int axisidx, float vec[3])
{
	float alignaxis[3] = {0.0, 0.0, 0.0};
	float norm[3], axis[3], angle, new_quat[4];
	
	if(axisidx > 0) alignaxis[axisidx-1]= 1.0;
	else alignaxis[-axisidx-1]= -1.0;
	
	VECCOPY(norm, vec);
	Normalize(norm);
	
	angle= (float)acos(Inpf(alignaxis, norm));
	Crossf(axis, alignaxis, norm);
	VecRotToQuat(axis, -angle, new_quat);
	
	rv3d->view= 0;
	
	if (rv3d->persp==V3D_CAMOB && v3d->camera) {
		/* switch out of camera view */
		float orig_ofs[3];
		float orig_dist= rv3d->dist;
		float orig_lens= v3d->lens;
		
		VECCOPY(orig_ofs, rv3d->ofs);
		rv3d->persp= V3D_PERSP;
		rv3d->dist= 0.0;
		view_settings_from_ob(v3d->camera, rv3d->ofs, NULL, NULL, &v3d->lens);
		smooth_view(NULL, NULL, NULL, orig_ofs, new_quat, &orig_dist, &orig_lens); // XXX
	} else {
		if (rv3d->persp==V3D_CAMOB) rv3d->persp= V3D_PERSP; /* switch out of camera mode */
		smooth_view(NULL, NULL, NULL, NULL, new_quat, NULL, NULL); // XXX
	}
}

