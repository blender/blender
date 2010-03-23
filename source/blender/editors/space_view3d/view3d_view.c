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

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_anim_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_anim.h"
#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"
#include "BKE_depsgraph.h" /* for fly mode updating */


#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_armature.h"



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
		glLoadMatrixf(rv3d->winmat);
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(rv3d->viewmat);
	}
}

float *give_cursor(Scene *scene, View3D *v3d)
{
	if(v3d && v3d->localvd) return v3d->cursor;
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
	if (ofs)
		negate_v3_v3(ofs, ob->obmat[3]);

	/* Quat */
	if (quat) {
		copy_m4_m4(bmat, ob->obmat);
		normalize_m4(bmat);
		invert_m4_m4(imat, bmat);
		copy_m3_m4(tmat, imat);
		mat3_to_quat( quat,tmat);
	}
	
	if (dist) {
		float vec[3];
		copy_m3_m4(tmat, ob->obmat);
		
		vec[0]= vec[1] = 0.0;
		vec[2]= -(*dist);
		mul_m3_v3(tmat, vec);
		sub_v3_v3v3(ofs, ofs, vec);
	}
	
	/* Lens */
	if (lens)
		object_lens_clip_settings(ob, lens, NULL, NULL);
}


/* ****************** smooth view operator ****************** */
/* This operator is one of the 'timer refresh' ones like animation playback */

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
				 normalize_v3(vec1);
				 normalize_v3(vec2);
				 /* scale the time allowed by the rotation */
				 sms.time_allowed *= angle_normalized_v3v3(vec1, vec2)/(M_PI/2); 
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
			if((rv3d->viewlock & RV3D_LOCKED)==0) {
				/* use existing if exists, means multiple calls to smooth view wont loose the original 'view' setting */
				sms.orig_view= rv3d->sms ? rv3d->sms->orig_view : rv3d->view;
				rv3d->view= 0;
			}

			/* ensure it shows correct */
			if(sms.to_camera) rv3d->persp= RV3D_PERSP;

			rv3d->rflag |= RV3D_NAVIGATING;
			
			/* keep track of running timer! */
			if(rv3d->sms==NULL)
				rv3d->sms= MEM_mallocN(sizeof(struct SmoothViewStore), "smoothview v3d");
			*rv3d->sms= sms;
			if(rv3d->smooth_timer)
				WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), rv3d->smooth_timer);
			/* TIMER1 is hardcoded in keymap */
			rv3d->smooth_timer= WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER1, 1.0/100.0);	/* max 30 frs/sec */
			
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
			rv3d->persp= RV3D_CAMOB;
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
		
		if((rv3d->viewlock & RV3D_LOCKED)==0) {
			rv3d->view= sms->orig_view;
		}

		MEM_freeN(rv3d->sms);
		rv3d->sms= NULL;
		
		WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), rv3d->smooth_timer);
		rv3d->smooth_timer= NULL;
		rv3d->rflag &= ~RV3D_NAVIGATING;
	}
	else {
		int i;
		
		/* ease in/out */
		if (step < 0.5)	step = (float)pow(step*2, 2)/2;
		else			step = (float)1-(pow(2*(1-step),2)/2);

		step_inv = 1.0-step;

		for (i=0; i<3; i++)
			rv3d->ofs[i] = sms->new_ofs[i]*step + sms->orig_ofs[i]*step_inv;

		interp_qt_qtqt(rv3d->viewquat, sms->orig_quat, sms->new_quat, step);
		
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

/* ****************** change view operators ****************** */

static void setcameratoview3d(View3D *v3d, RegionView3D *rv3d, Object *ob)
{
	float dvec[3];
	float mat3[3][3];

	mul_v3_v3fl(dvec, rv3d->viewinv[2], rv3d->dist);
	sub_v3_v3v3(ob->loc, dvec, rv3d->ofs);
	rv3d->viewquat[0]= -rv3d->viewquat[0];

	// quat_to_eul( ob->rot,rv3d->viewquat); // in 2.4x for xyz eulers only
	quat_to_mat3(mat3, rv3d->viewquat);
	object_mat3_to_rot(ob, mat3, 0);

	rv3d->viewquat[0]= -rv3d->viewquat[0];
	
	ob->recalc= OB_RECALC_OB;
}


static int view3d_setcameratoview_exec(bContext *C, wmOperator *op)
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d= CTX_wm_region_view3d(C);

	setcameratoview3d(v3d, rv3d, v3d->camera);
	rv3d->persp = RV3D_CAMOB;
	
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, v3d->camera);
	
	return OPERATOR_FINISHED;

}

int view3d_setcameratoview_poll(bContext *C)
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d= CTX_wm_region_view3d(C);

	if (v3d==NULL || v3d->camera==NULL)	return 0;
	if (rv3d && rv3d->viewlock != 0)		return 0;
	return 1;
}

void VIEW3D_OT_setcameratoview(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Align Camera To View";
	ot->description= "Set camera view to active view";
	ot->idname= "VIEW3D_OT_camera_to_view";
	
	/* api callbacks */
	ot->exec= view3d_setcameratoview_exec;	
	ot->poll= view3d_setcameratoview_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}


static int view3d_setobjectascamera_exec(bContext *C, wmOperator *op)
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	Scene *scene= CTX_data_scene(C);
	
	if(BASACT) {
		rv3d->persp= RV3D_CAMOB;
		v3d->camera= OBACT;
		if(v3d->scenelock)
			scene->camera= OBACT;
		smooth_view(C, NULL, v3d->camera, rv3d->ofs, rv3d->viewquat, &rv3d->dist, &v3d->lens);
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_RENDER_OPTIONS|NC_OBJECT|ND_DRAW, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_setobjectascamera(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Set Active Object as Camera";
	ot->description= "Set the active object as the active camera for this view or scene";
	ot->idname= "VIEW3D_OT_object_as_camera";
	
	/* api callbacks */
	ot->exec= view3d_setobjectascamera_exec;	
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ********************************** */

void view3d_calculate_clipping(BoundBox *bb, float planes[4][4], bglMats *mats, rcti *rect)
{
	double xs, ys, p[3];
	short val;

	/* near zero floating point values can give issues with gluUnProject
		in side view on some implementations */
	if(fabs(mats->modelview[0]) < 1e-6) mats->modelview[0]= 0.0;
	if(fabs(mats->modelview[5]) < 1e-6) mats->modelview[5]= 0.0;

	/* Set up viewport so that gluUnProject will give correct values */
	mats->viewport[0] = 0;
	mats->viewport[1] = 0;

	/* four clipping planes and bounding volume */
	/* first do the bounding volume */
	for(val=0; val<4; val++) {
		xs= (val==0||val==3)?rect->xmin:rect->xmax;
		ys= (val==0||val==1)?rect->ymin:rect->ymax;

		gluUnProject(xs, ys, 0.0, mats->modelview, mats->projection, mats->viewport, &p[0], &p[1], &p[2]);
		VECCOPY(bb->vec[val], p);

		gluUnProject(xs, ys, 1.0, mats->modelview, mats->projection, mats->viewport, &p[0], &p[1], &p[2]);
		VECCOPY(bb->vec[4+val], p);
	}

	/* then plane equations */
	for(val=0; val<4; val++) {

		normal_tri_v3(planes[val], bb->vec[val], bb->vec[val==3?0:val+1], bb->vec[val+4]);

		planes[val][3]= - planes[val][0]*bb->vec[val][0]
			- planes[val][1]*bb->vec[val][1]
			- planes[val][2]*bb->vec[val][2];
	}
}

/* create intersection coordinates in view Z direction at mouse coordinates */
void viewline(ARegion *ar, View3D *v3d, float mval[2], float ray_start[3], float ray_end[3])
{
	RegionView3D *rv3d= ar->regiondata;
	float vec[4];
	int a;
	
	if(!get_view3d_ortho(v3d, rv3d)) {
		vec[0]= 2.0f * mval[0] / ar->winx - 1;
		vec[1]= 2.0f * mval[1] / ar->winy - 1;
		vec[2]= -1.0f;
		vec[3]= 1.0f;
		
		mul_m4_v4(rv3d->persinv, vec);
		mul_v3_fl(vec, 1.0f / vec[3]);
		
		copy_v3_v3(ray_start, rv3d->viewinv[3]);
		sub_v3_v3v3(vec, vec, ray_start);
		normalize_v3(vec);
		
		VECADDFAC(ray_start, rv3d->viewinv[3], vec, v3d->near);
		VECADDFAC(ray_end, rv3d->viewinv[3], vec, v3d->far);
	}
	else {
		vec[0] = 2.0f * mval[0] / ar->winx - 1;
		vec[1] = 2.0f * mval[1] / ar->winy - 1;
		vec[2] = 0.0f;
		vec[3] = 1.0f;
		
		mul_m4_v4(rv3d->persinv, vec);
		
		VECADDFAC(ray_start, vec, rv3d->viewinv[2],  1000.0f);
		VECADDFAC(ray_end, vec, rv3d->viewinv[2], -1000.0f);
	}

	/* clipping */
	if(rv3d->rflag & RV3D_CLIPPING)
		for(a=0; a<4; a++)
			clip_line_plane(ray_start, ray_end, rv3d->clip[a]);
}

/* create intersection ray in view Z direction at mouse coordinates */
void viewray(ARegion *ar, View3D *v3d, float mval[2], float ray_start[3], float ray_normal[3])
{
	float ray_end[3];
	
	viewline(ar, v3d, mval, ray_start, ray_end);
	sub_v3_v3v3(ray_normal, ray_end, ray_start);
	normalize_v3(ray_normal);
}

void viewvector(RegionView3D *rv3d, float coord[3], float vec[3])
{
	if (rv3d->persp != RV3D_ORTHO)
	{
		float p1[4], p2[4];

		VECCOPY(p1, coord);
		p1[3] = 1.0f;
		VECCOPY(p2, p1);
		p2[3] = 1.0f;
		mul_m4_v4(rv3d->viewmat, p2);

		mul_v3_fl(p2, 2.0f);

		mul_m4_v4(rv3d->viewinv, p2);

		sub_v3_v3v3(vec, p1, p2);
	}
	else {
		VECCOPY(vec, rv3d->viewinv[2]);
	}
	normalize_v3(vec);
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
	
	mul_m4_m4m4(vmat, ob->obmat, rv3d->viewmat);
	mul_m4_m4m4(pmat, vmat, rv3d->winmat);
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
	
	mul_m4_v4(mat, vec4);
	
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
	
	mul_m4_m4m4(mat, obmat, rv3d->persmat);
	
	for(a=0; a<8; a++) {
		VECCOPY(vec, bb->vec[a]);
		vec[3]= 1.0;
		mul_m4_v4(mat, vec);
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
		if(view3d_test_clipping(rv3d, vec, 0))
			return;
	}
	
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	mul_m4_v4(rv3d->persmat, vec4);
	
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
	
	mul_m4_v4(rv3d->persmat, vec4);
	
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
	
	mul_m4_v4(rv3d->persmat, vec4);
	
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
	
	mul_m4_v4(rv3d->persmat, vec4);
	
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
	
	mul_m4_v4(rv3d->persmat, vec4);
	
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
	
	mul_m4_v4(rv3d->persmat, vec4);
	
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
  
  if(rv3d->persp==RV3D_CAMOB) {
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
  
  if(rv3d->persp==RV3D_ORTHO)
	  return 1;

  return 0;
}

/* copies logic of get_view3d_viewplane(), keep in sync */
int get_view3d_cliprange(View3D *v3d, RegionView3D *rv3d, float *clipsta, float *clipend)
{
	int orth= 0;

	*clipsta= v3d->near;
	*clipend= v3d->far;

	if(rv3d->persp==RV3D_CAMOB) {
		if(v3d->camera) {
			if(v3d->camera->type==OB_LAMP ) {
				Lamp *la= v3d->camera->data;
				*clipsta= la->clipsta;
				*clipend= la->clipend;
			}
			else if(v3d->camera->type==OB_CAMERA) {
				Camera *cam= v3d->camera->data;
				*clipsta= cam->clipsta;
				*clipend= cam->clipend;

				if(cam->type==CAM_ORTHO)
					orth= 1;
			}
		}
	}

	if(rv3d->persp==RV3D_ORTHO) {
		*clipend *= 0.5;	// otherwise too extreme low zbuffer quality
		*clipsta= - *clipend;
		orth= 1;
	}

	return orth;
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
	
	if(rv3d->persp==RV3D_CAMOB) {
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
	
	if(rv3d->persp==RV3D_ORTHO) {
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
		if(rv3d->persp==RV3D_CAMOB) {
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

	/* update matrix in 3d view region */
	glGetFloatv(GL_PROJECTION_MATRIX, (float*)rv3d->winmat);
}

static void obmat_to_viewmat(View3D *v3d, RegionView3D *rv3d, Object *ob, short smooth)
{
	float bmat[4][4];
	float tmat[3][3];
	
	rv3d->view= 0; /* dont show the grid */
	
	copy_m4_m4(bmat, ob->obmat);
	normalize_m4(bmat);
	invert_m4_m4(rv3d->viewmat, bmat);
	
	/* view quat calculation, needed for add object */
	copy_m3_m4(tmat, rv3d->viewmat);
	if (smooth) {
		float new_quat[4];
		if (rv3d->persp==RV3D_CAMOB && v3d->camera) {
			/* were from a camera view */
			
			float orig_ofs[3];
			float orig_dist= rv3d->dist;
			float orig_lens= v3d->lens;
			VECCOPY(orig_ofs, rv3d->ofs);
			
			/* Switch from camera view */
			mat3_to_quat( new_quat,tmat);
			
			rv3d->persp=RV3D_PERSP;
			rv3d->dist= 0.0;
			
			view_settings_from_ob(v3d->camera, rv3d->ofs, NULL, NULL, &v3d->lens);
			smooth_view(NULL, NULL, NULL, orig_ofs, new_quat, &orig_dist, &orig_lens); // XXX
			
			rv3d->persp=RV3D_CAMOB; /* just to be polite, not needed */
			
		} else {
			mat3_to_quat( new_quat,tmat);
			smooth_view(NULL, NULL, NULL, NULL, new_quat, NULL, NULL); // XXX
		}
	} else {
		mat3_to_quat( rv3d->viewquat,tmat);
	}
}

#define QUATSET(a, b, c, d, e)	a[0]=b; a[1]=c; a[2]=d; a[3]=e; 

static void view3d_viewlock(RegionView3D *rv3d)
{
	switch(rv3d->view) {
	case RV3D_VIEW_BOTTOM :
		QUATSET(rv3d->viewquat,0.0, -1.0, 0.0, 0.0);
		break;
		
	case RV3D_VIEW_BACK:
		QUATSET(rv3d->viewquat,0.0, 0.0, (float)-cos(M_PI/4.0), (float)-cos(M_PI/4.0));
		break;
		
	case RV3D_VIEW_LEFT:
		QUATSET(rv3d->viewquat,0.5, -0.5, 0.5, 0.5);
		break;
		
	case RV3D_VIEW_TOP:
		QUATSET(rv3d->viewquat,1.0, 0.0, 0.0, 0.0);
		break;
		
	case RV3D_VIEW_FRONT:
		QUATSET(rv3d->viewquat,(float)cos(M_PI/4.0), (float)-sin(M_PI/4.0), 0.0, 0.0);
		break;
		
	case RV3D_VIEW_RIGHT:
		QUATSET(rv3d->viewquat, 0.5, -0.5, -0.5, -0.5);
		break;
	}
}

/* give a 4x4 matrix from a perspective view, only needs viewquat, ofs and dist
 * basically the same as...
 *     rv3d->persp= RV3D_PERSP
 *     setviewmatrixview3d(scene, v3d, rv3d);
 *     setcameratoview3d(v3d, rv3d, v3d->camera);
 * ...but less of a hassle
 * */
static void view3d_persp_mat4(RegionView3D *rv3d, float mat[][4])
{
	float qt[4], dvec[3];
	copy_qt_qt(qt, rv3d->viewquat);
	qt[0]= -qt[0];
	quat_to_mat4(mat, qt);
	mat[3][2] -= rv3d->dist;
	translate_m4(mat, rv3d->ofs[0], rv3d->ofs[1], rv3d->ofs[2]);
	mul_v3_v3fl(dvec, mat[2], -rv3d->dist);
	sub_v3_v3v3(mat[3], dvec, rv3d->ofs);
}

/* dont set windows active in in here, is used by renderwin too */
void setviewmatrixview3d(Scene *scene, View3D *v3d, RegionView3D *rv3d)
{
	if(rv3d->persp==RV3D_CAMOB) {	    /* obs/camera */
		if(v3d->camera) {
			where_is_object(scene, v3d->camera);	
			obmat_to_viewmat(v3d, rv3d, v3d->camera, 0);
		}
		else {
			quat_to_mat4( rv3d->viewmat,rv3d->viewquat);
			rv3d->viewmat[3][2]-= rv3d->dist;
		}
	}
	else {
		/* should be moved to better initialize later on XXX */
		if(rv3d->viewlock)
			view3d_viewlock(rv3d);
		
		quat_to_mat4( rv3d->viewmat,rv3d->viewquat);
		if(rv3d->persp==RV3D_PERSP) rv3d->viewmat[3][2]-= rv3d->dist;
		if(v3d->ob_centre) {
			Object *ob= v3d->ob_centre;
			float vec[3];
			
			VECCOPY(vec, ob->obmat[3]);
			if(ob->type==OB_ARMATURE && v3d->ob_centre_bone[0]) {
				bPoseChannel *pchan= get_pose_channel(ob->pose, v3d->ob_centre_bone);
				if(pchan) {
					VECCOPY(vec, pchan->pose_mat[3]);
					mul_m4_v3(ob->obmat, vec);
				}
			}
			translate_m4( rv3d->viewmat,-vec[0], -vec[1], -vec[2]);
		}
		else translate_m4( rv3d->viewmat,rv3d->ofs[0], rv3d->ofs[1], rv3d->ofs[2]);
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
	char dt, dtx;
	
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
	mul_m4_m4m4(vc->rv3d->persmat, vc->rv3d->viewmat, vc->rv3d->winmat);
	
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
							copy_m4_m4(dob->ob->obmat, dob->mat);
							
							/* extra service: draw the duplicator in drawtype of parent */
							/* MIN2 for the drawtype to allow bounding box objects in groups for lods */
							dt= tbase.object->dt;	tbase.object->dt= MIN2(tbase.object->dt, base->object->dt);
							dtx= tbase.object->dtx; tbase.object->dtx= base->object->dtx;

							draw_object(scene, ar, v3d, &tbase, DRAW_PICKING|DRAW_CONSTCOLOR);
							
							tbase.object->dt= dt;
							tbase.object->dtx= dtx;

							copy_m4_m4(dob->ob->obmat, dob->omat);
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
	mul_m4_m4m4(vc->rv3d->persmat, vc->rv3d->viewmat, vc->rv3d->winmat);
	
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

static void copy_view3d_lock_space(View3D *v3d, Scene *scene)
{
	int bit;
	
	if(v3d->scenelock && v3d->localvd==NULL) {
		v3d->lay= scene->lay;
		v3d->camera= scene->camera;
		
		if(v3d->camera==NULL) {
			ARegion *ar;
			
			for(ar=v3d->regionbase.first; ar; ar= ar->next) {
				if(ar->regiontype == RGN_TYPE_WINDOW) {
					RegionView3D *rv3d= ar->regiondata;
					if(rv3d->persp==RV3D_CAMOB)
						rv3d->persp= RV3D_PERSP;
				}
			}
		}
		
		if((v3d->lay & v3d->layact) == 0) {
			for(bit= 0; bit<32; bit++) {
				if(v3d->lay & (1<<bit)) {
					v3d->layact= 1<<bit;
					break;
				}
			}
		}
	}
}

void ED_view3d_scene_layers_update(Main *bmain, Scene *scene)
{
	bScreen *sc;
	ScrArea *sa;
	SpaceLink *sl;
	
	/* from scene copy to the other views */
	for(sc=bmain->screen.first; sc; sc=sc->id.next) {
		if(sc->scene!=scene)
			continue;
		
		for(sa=sc->areabase.first; sa; sa=sa->next)
			for(sl=sa->spacedata.first; sl; sl=sl->next)
				if(sl->spacetype==SPACE_VIEW3D)
					copy_view3d_lock_space((View3D*)sl, scene);
	}
}

int ED_view3d_scene_layer_set(int lay, const int *values)
{
	int i, tot= 0;
	
	/* ensure we always have some layer selected */
	for(i=0; i<20; i++)
		if(values[i])
			tot++;
	
	if(tot==0)
		return lay;
	
	for(i=0; i<20; i++) {
		if(values[i]) lay |= (1<<i);
		else lay &= ~(1<<i);
	}
	
	return lay;
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
				if(rv3d->persp==RV3D_ORTHO)
					rv3d->dist*= 0.7;

				// correction for window aspect ratio
				if(ar->winy>2 && ar->winx>2) {
					float asp= (float)ar->winx/(float)ar->winy;
					if(asp<1.0) asp= 1.0/asp;
					rv3d->dist*= asp;
				}
				
				if (rv3d->persp==RV3D_CAMOB) rv3d->persp= RV3D_PERSP;
				
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
	ot->description= "Toggle display of selected object(s) separately and centered in view";
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
	
	GPU_state_init();

	glPopAttrib();
}

/* was space_set_commmandline_options in 2.4x */
void game_set_commmandline_options(GameData *gm)
{
	SYS_SystemHandle syshandle;
	int test;

	if ( (syshandle = SYS_GetSystem()) ) {
		/* User defined settings */
		test= (U.gameflags & USER_DISABLE_MIPMAP);
		GPU_set_mipmap(!test);
		SYS_WriteCommandLineInt(syshandle, "nomipmap", test);

		/* File specific settings: */
		/* Only test the first one. These two are switched
		 * simultaneously. */
		test= (gm->flag & GAME_SHOW_FRAMERATE);
		SYS_WriteCommandLineInt(syshandle, "show_framerate", test);
		SYS_WriteCommandLineInt(syshandle, "show_profile", test);

		test = (gm->flag & GAME_SHOW_DEBUG_PROPS);
		SYS_WriteCommandLineInt(syshandle, "show_properties", test);

		test= (gm->flag & GAME_SHOW_PHYSICS);
		SYS_WriteCommandLineInt(syshandle, "show_physics", test);

		test= (gm->flag & GAME_ENABLE_ALL_FRAMES);
		SYS_WriteCommandLineInt(syshandle, "fixedtime", test);

		test= (gm->flag & GAME_ENABLE_ANIMATION_RECORD);
		SYS_WriteCommandLineInt(syshandle, "animation_record", test);

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
extern void StartKetsjiShell(struct bContext *C, struct ARegion *ar, rcti *cam_frame, int always_use_expand_framing);

#endif // GAMEBLENDER == 1

int game_engine_poll(bContext *C)
{
	/* we need a context and area to launch BGE
	it's a temporary solution to avoid crash at load time
	if we try to auto run the BGE. Ideally we want the
	context to be set as soon as we load the file. */

	if(CTX_wm_window(C)==NULL) return 0;
	if(CTX_wm_screen(C)==NULL) return 0;
	if(CTX_wm_area(C)==NULL) return 0;

	if(CTX_data_mode_enum(C)!=CTX_MODE_OBJECT)
		return 0;

	return 1;
}

int ED_view3d_context_activate(bContext *C)
{
	bScreen *sc= CTX_wm_screen(C);
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar;
	RegionView3D *rv3d;

	if(sa->spacetype != SPACE_VIEW3D)
		for(sa=sc->areabase.first; sa; sa= sa->next)
			if(sa->spacetype==SPACE_VIEW3D)
				break;

	if(!sa)
		return 0;
	
	for(ar=sa->regionbase.first; ar; ar=ar->next)
		if(ar->regiontype == RGN_TYPE_WINDOW)
			break;
	
	if(!ar)
		return 0;
	
	// bad context switch ..
	CTX_wm_area_set(C, sa);
	CTX_wm_region_set(C, ar);
	rv3d= ar->regiondata;

	return 1;
}

static int game_engine_exec(bContext *C, wmOperator *op)
{
#if GAMEBLENDER == 1
	Scene *startscene = CTX_data_scene(C);
	ScrArea *sa, *prevsa= CTX_wm_area(C);
	ARegion *ar, *prevar= CTX_wm_region(C);
	wmWindow *prevwin= CTX_wm_window(C);
	RegionView3D *rv3d;
	rcti cam_frame;

	// bad context switch ..
	if(!ED_view3d_context_activate(C))
		return OPERATOR_CANCELLED;
	
	rv3d= CTX_wm_region_view3d(C);
	sa= CTX_wm_area(C);
	ar= CTX_wm_region(C);

	view3d_operator_needs_opengl(C);
	
	game_set_commmandline_options(&startscene->gm);

	if(rv3d->persp==RV3D_CAMOB && startscene->gm.framing.type == SCE_GAMEFRAMING_BARS && startscene->gm.stereoflag != STEREO_DOME) { /* Letterbox */
		rctf cam_framef;
		calc_viewborder(startscene, ar, CTX_wm_view3d(C), &cam_framef);
		cam_frame.xmin = cam_framef.xmin + ar->winrct.xmin;
		cam_frame.xmax = cam_framef.xmax + ar->winrct.xmin;
		cam_frame.ymin = cam_framef.ymin + ar->winrct.ymin;
		cam_frame.ymax = cam_framef.ymax + ar->winrct.ymin;
		BLI_isect_rcti(&ar->winrct, &cam_frame, &cam_frame);
	}
	else {
		cam_frame.xmin = ar->winrct.xmin;
		cam_frame.xmax = ar->winrct.xmax;
		cam_frame.ymin = ar->winrct.ymin;
		cam_frame.ymax = ar->winrct.ymax;
	}


	SaveState(C);

	StartKetsjiShell(C, ar, &cam_frame, 1);
	
	/* restore context, in case it changed in the meantime, for
	   example by working in another window or closing it */
	CTX_wm_region_set(C, prevar);
	CTX_wm_window_set(C, prevwin);
	CTX_wm_area_set(C, prevsa);
	RestoreState(C);

	//XXX restore_all_scene_cfra(scene_cfra_store);
	set_scene_bg(startscene);
	//XXX scene_update_for_newframe(G.scene, G.scene->lay);
	
	ED_area_tag_redraw(CTX_wm_area(C));

	return OPERATOR_FINISHED;
#else
	BKE_report(op->reports, RPT_ERROR, "Game engine is disabled in this build.");
	return OPERATOR_CANCELLED;
#endif
}

void VIEW3D_OT_game_start(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Start Game Engine";
	ot->description= "Start game engine";
	ot->idname= "VIEW3D_OT_game_start";
	
	/* api callbacks */
	ot->exec= game_engine_exec;
	
	ot->poll= game_engine_poll;
}


/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */
#define FLY_MODAL_CANCEL			1
#define FLY_MODAL_CONFIRM			2
#define FLY_MODAL_ACCELERATE 		3
#define FLY_MODAL_DECELERATE		4
#define FLY_MODAL_PAN_ENABLE		5
#define FLY_MODAL_PAN_DISABLE		6
#define FLY_MODAL_DIR_FORWARD		7
#define FLY_MODAL_DIR_BACKWARD		8
#define FLY_MODAL_DIR_LEFT			9
#define FLY_MODAL_DIR_RIGHT			10
#define FLY_MODAL_DIR_UP			11
#define FLY_MODAL_DIR_DOWN			12
#define FLY_MODAL_AXIS_LOCK_X		13
#define FLY_MODAL_AXIS_LOCK_Z		14
#define FLY_MODAL_PRECISION_ENABLE	15
#define FLY_MODAL_PRECISION_DISABLE	16

/* called in transform_ops.c, on each regeneration of keymaps  */
void fly_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
	{FLY_MODAL_CANCEL,	"CANCEL", 0, "Cancel", ""},
	{FLY_MODAL_CONFIRM,	"CONFIRM", 0, "Confirm", ""},
	{FLY_MODAL_ACCELERATE, "ACCELERATE", 0, "Accelerate", ""},
	{FLY_MODAL_DECELERATE, "DECELERATE", 0, "Decelerate", ""},

	{FLY_MODAL_PAN_ENABLE,	"PAN_ENABLE", 0, "Pan Enable", ""},
	{FLY_MODAL_PAN_DISABLE,	"PAN_DISABLE", 0, "Pan Disable", ""},

	{FLY_MODAL_DIR_FORWARD,	"FORWARD", 0, "Fly Forward", ""},
	{FLY_MODAL_DIR_BACKWARD,"BACKWARD", 0, "Fly Backward", ""},
	{FLY_MODAL_DIR_LEFT,	"LEFT", 0, "Fly Left", ""},
	{FLY_MODAL_DIR_RIGHT,	"RIGHT", 0, "Fly Right", ""},
	{FLY_MODAL_DIR_UP,		"UP", 0, "Fly Up", ""},
	{FLY_MODAL_DIR_DOWN,	"DOWN", 0, "Fly Down", ""},

	{FLY_MODAL_AXIS_LOCK_X,	"AXIS_LOCK_X", 0, "X Axis Correction", "X axis correction (toggle)"},
	{FLY_MODAL_AXIS_LOCK_Z,	"AXIS_LOCK_Z", 0, "X Axis Correction", "Z axis correction (toggle)"},

	{FLY_MODAL_PRECISION_ENABLE,	"PRECISION_ENABLE", 0, "Precision Enable", ""},
	{FLY_MODAL_PRECISION_DISABLE,	"PRECISION_DISABLE", 0, "Precision Disable", ""},

	{0, NULL, 0, NULL, NULL}};

	wmKeyMap *keymap= WM_modalkeymap_get(keyconf, "View3D Fly Modal");

	/* this function is called for each spacetype, only needs to add map once */
	if(keymap) return;

	keymap= WM_modalkeymap_add(keyconf, "View3D Fly Modal", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY,    KM_PRESS, KM_ANY, 0, FLY_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_ANY, KM_ANY, 0, FLY_MODAL_CANCEL);

	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_ANY, KM_ANY, 0, FLY_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, RETKEY, KM_PRESS, KM_ANY, 0, FLY_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, PADENTER, KM_PRESS, KM_ANY, 0, FLY_MODAL_CONFIRM);

	WM_modalkeymap_add_item(keymap, PADPLUSKEY, KM_PRESS, 0, 0, FLY_MODAL_ACCELERATE);
	WM_modalkeymap_add_item(keymap, PADMINUS, KM_PRESS, 0, 0, FLY_MODAL_DECELERATE);
	WM_modalkeymap_add_item(keymap, WHEELUPMOUSE, KM_PRESS, 0, 0, FLY_MODAL_ACCELERATE);
	WM_modalkeymap_add_item(keymap, WHEELDOWNMOUSE, KM_PRESS, 0, 0, FLY_MODAL_DECELERATE);

	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_PRESS, KM_ANY, 0, FLY_MODAL_PAN_ENABLE);
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_RELEASE, KM_ANY, 0, FLY_MODAL_PAN_DISABLE); /* XXX - Bug in the event system, middle mouse release doesnt work */

	/* WASD */
	WM_modalkeymap_add_item(keymap, WKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_FORWARD);
	WM_modalkeymap_add_item(keymap, SKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_BACKWARD);
	WM_modalkeymap_add_item(keymap, AKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_LEFT);
	WM_modalkeymap_add_item(keymap, DKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_RIGHT);
	WM_modalkeymap_add_item(keymap, RKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_UP);
	WM_modalkeymap_add_item(keymap, FKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_DOWN);

	WM_modalkeymap_add_item(keymap, XKEY, KM_PRESS, 0, 0, FLY_MODAL_AXIS_LOCK_X);
	WM_modalkeymap_add_item(keymap, ZKEY, KM_PRESS, 0, 0, FLY_MODAL_AXIS_LOCK_Z);

	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_PRESS, KM_ANY, 0, FLY_MODAL_PRECISION_ENABLE);
	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_RELEASE, KM_ANY, 0, FLY_MODAL_PRECISION_DISABLE);

	/* assign map to operators */
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_fly");

}

typedef struct FlyInfo {
	/* context stuff */
	RegionView3D *rv3d;
	View3D *v3d;
	ARegion *ar;
	Scene *scene;

	wmTimer *timer; /* needed for redraws */

	short state;
	short use_precision;
	short redraw;
	short mval[2];

	/* fly state state */
	float speed; /* the speed the view is moving per redraw */
	short axis; /* Axis index to move allong by default Z to move allong the view */
	short pan_view; /* when true, pan the view instead of rotating */

	/* relative view axis locking - xlock, zlock
	0; disabled
	1; enabled but not checking because mouse hasnt moved outside the margin since locking was checked an not needed
	   when the mouse moves, locking is set to 2 so checks are done.
	2; mouse moved and checking needed, if no view altering is donem its changed back to 1 */
	short xlock, zlock;
	float xlock_momentum, zlock_momentum; /* nicer dynamics */
	float grid; /* world scale 1.0 default */

	/* root most parent */
	Object *root_parent;

	/* backup values */
	float dist_backup; /* backup the views distance since we use a zero dist for fly mode */
	float ofs_backup[3]; /* backup the views offset incase the user cancels flying in non camera mode */
	float rot_backup[4]; /* backup the views quat incase the user cancels flying in non camera mode. (quat for view, eul for camera) */
	short persp_backup; /* remember if were ortho or not, only used for restoring the view if it was a ortho view */

	void *obtfm; /* backup the objects transform */

	/* compare between last state */
	double time_lastwheel; /* used to accelerate when using the mousewheel a lot */
	double time_lastdraw; /* time between draws */

	/* use for some lag */
	float dvec_prev[3]; /* old for some lag */

} FlyInfo;

/* FlyInfo->state */
#define FLY_RUNNING		0
#define FLY_CANCEL		1
#define FLY_CONFIRM		2

static int initFlyInfo (bContext *C, FlyInfo *fly, wmOperator *op, wmEvent *event)
{
	float upvec[3]; // tmp
	float mat[3][3];

	fly->rv3d= CTX_wm_region_view3d(C);
	fly->v3d = CTX_wm_view3d(C);
	fly->ar = CTX_wm_region(C);
	fly->scene= CTX_data_scene(C);

	if(fly->rv3d->persp==RV3D_CAMOB && fly->v3d->camera->id.lib) {
		BKE_report(op->reports, RPT_ERROR, "Cannot fly a camera from an external library");
		return FALSE;
	}

	if(fly->v3d->ob_centre) {
		BKE_report(op->reports, RPT_ERROR, "Cannot fly when the view is locked to an object");
		return FALSE;
	}

	if(fly->rv3d->persp==RV3D_CAMOB && fly->v3d->camera->constraints.first) {
		BKE_report(op->reports, RPT_ERROR, "Cannot fly an object with constraints");
		return FALSE;
	}

	fly->state= FLY_RUNNING;
	fly->speed= 0.0f;
	fly->axis= 2;
	fly->pan_view= FALSE;
	fly->xlock= FALSE;
	fly->zlock= TRUE;
	fly->xlock_momentum=0.0f;
	fly->zlock_momentum=0.0f;
	fly->grid= 1.0f;
	fly->use_precision= 0;

	fly->dvec_prev[0]= fly->dvec_prev[1]= fly->dvec_prev[2]= 0.0f;

	fly->timer= WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.01f);

	fly->mval[0] = event->x - fly->ar->winrct.xmin;
	fly->mval[1] = event->y - fly->ar->winrct.ymin;


	fly->time_lastdraw= fly->time_lastwheel= PIL_check_seconds_timer();

	fly->rv3d->rflag |= RV3D_FLYMODE|RV3D_NAVIGATING; /* so we draw the corner margins */

	/* detect weather to start with Z locking */
	upvec[0]=1.0f; upvec[1]=0.0f; upvec[2]=0.0f;
	copy_m3_m4(mat, fly->rv3d->viewinv);
	mul_m3_v3(mat, upvec);
	if (fabs(upvec[2]) < 0.1)
		fly->zlock = 1;
	upvec[0]=0; upvec[1]=0; upvec[2]=0;

	fly->persp_backup= fly->rv3d->persp;
	fly->dist_backup= fly->rv3d->dist;
	if (fly->rv3d->persp==RV3D_CAMOB) {
		Object *ob_back;
		if((fly->root_parent=fly->v3d->camera->parent)) {
			while(fly->root_parent->parent)
				fly->root_parent= fly->root_parent->parent;
			ob_back= fly->root_parent;
		}
		else {
			ob_back= fly->v3d->camera;
		}

		/* store the original camera loc and rot */
		/* TODO. axis angle etc */

		fly->obtfm= object_tfm_backup(ob_back);

		where_is_object(fly->scene, fly->v3d->camera);
		negate_v3_v3(fly->rv3d->ofs, fly->v3d->camera->obmat[3]);

		fly->rv3d->dist=0.0;
	} else {
		/* perspective or ortho */
		if (fly->rv3d->persp==RV3D_ORTHO)
			fly->rv3d->persp= RV3D_PERSP; /*if ortho projection, make perspective */
		QUATCOPY(fly->rot_backup, fly->rv3d->viewquat);
		VECCOPY(fly->ofs_backup, fly->rv3d->ofs);
		fly->rv3d->dist= 0.0;

		upvec[2]= fly->dist_backup; /*x and y are 0*/
		mul_m3_v3(mat, upvec);
		sub_v3_v3v3(fly->rv3d->ofs, fly->rv3d->ofs, upvec);
		/*Done with correcting for the dist*/
	}

	
	/* center the mouse, probably the UI mafia are against this but without its quite annoying */
	WM_cursor_warp(CTX_wm_window(C), fly->ar->winrct.xmin + fly->ar->winx/2, fly->ar->winrct.ymin + fly->ar->winy/2);
	
	return 1;
}

static int flyEnd(bContext *C, FlyInfo *fly)
{
	RegionView3D *rv3d= fly->rv3d;
	View3D *v3d = fly->v3d;

	float upvec[3];

	if(fly->state == FLY_RUNNING)
		return OPERATOR_RUNNING_MODAL;

	WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), fly->timer);

	rv3d->dist= fly->dist_backup;

	if (fly->state == FLY_CANCEL) {
	/* Revert to original view? */
		if (fly->persp_backup==RV3D_CAMOB) { /* a camera view */
			Object *ob_back;
			if(fly->root_parent)ob_back= fly->root_parent;
			else				ob_back= fly->v3d->camera;

			/* store the original camera loc and rot */
			object_tfm_restore(ob_back, fly->obtfm);

			DAG_id_flush_update(&ob_back->id, OB_RECALC_OB);
		} else {
			/* Non Camera we need to reset the view back to the original location bacause the user canceled*/
			QUATCOPY(rv3d->viewquat, fly->rot_backup);
			VECCOPY(rv3d->ofs, fly->ofs_backup);
			rv3d->persp= fly->persp_backup;
		}
	}
	else if (fly->persp_backup==RV3D_CAMOB) {	/* camera */
		float mat3[3][3];
		if(fly->root_parent) {
			DAG_id_flush_update(&fly->root_parent->id, OB_RECALC_OB);
		}
		else {
			copy_m3_m4(mat3, v3d->camera->obmat);
			object_mat3_to_rot(v3d->camera, mat3, TRUE);
			DAG_id_flush_update(&v3d->camera->id, OB_RECALC_OB);
		}

#if 0 //XXX2.5
		if (IS_AUTOKEY_MODE(NORMAL)) {
			allqueue(REDRAWIPO, 0);
			allspace(REMAKEIPO, 0);
			allqueue(REDRAWNLA, 0);
			allqueue(REDRAWTIME, 0);
		}
#endif
	}
	else { /* not camera */
		/* Apply the fly mode view */
		/*restore the dist*/
		float mat[3][3];
		upvec[0]= upvec[1]= 0;
		upvec[2]= fly->dist_backup; /*x and y are 0*/
		copy_m3_m4(mat, rv3d->viewinv);
		mul_m3_v3(mat, upvec);
		add_v3_v3v3(rv3d->ofs, rv3d->ofs, upvec);
		/*Done with correcting for the dist */
	}

	rv3d->rflag &= ~(RV3D_FLYMODE|RV3D_NAVIGATING);
//XXX2.5	BIF_view3d_previewrender_signal(fly->sa, PR_DBASE|PR_DISPRECT); /* not working at the moment not sure why */

	if(fly->obtfm)
		MEM_freeN(fly->obtfm);

	if(fly->state == FLY_CONFIRM) {
		MEM_freeN(fly);
		return OPERATOR_FINISHED;
	}

	MEM_freeN(fly);
	return OPERATOR_CANCELLED;
}

static void flyEvent(FlyInfo *fly, wmEvent *event)
{
	if (event->type == TIMER && event->customdata == fly->timer) {
		fly->redraw = 1;
	}
	else if (event->type == MOUSEMOVE) {
		fly->mval[0] = event->x - fly->ar->winrct.xmin;
		fly->mval[1] = event->y - fly->ar->winrct.ymin;
	} /* handle modal keymap first */
	else if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case FLY_MODAL_CANCEL:
				fly->state = FLY_CANCEL;
				break;
			case FLY_MODAL_CONFIRM:
				fly->state = FLY_CONFIRM;
				break;

			case FLY_MODAL_ACCELERATE:
			{
				double time_currwheel;
				float time_wheel;

				time_currwheel= PIL_check_seconds_timer();
				time_wheel = (float)(time_currwheel - fly->time_lastwheel);
				fly->time_lastwheel = time_currwheel;
				/*printf("Wheel %f\n", time_wheel);*/
				/*Mouse wheel delays range from 0.5==slow to 0.01==fast*/
				time_wheel = 1+ (10 - (20*MIN2(time_wheel, 0.5))); /* 0-0.5 -> 0-5.0 */

				if (fly->speed<0.0f) fly->speed= 0.0f;
				else {
					if (event->shift)
						fly->speed+= fly->grid*time_wheel*0.1;
					else
						fly->speed+= fly->grid*time_wheel;
				}
				break;
			}
			case FLY_MODAL_DECELERATE:
			{
				double time_currwheel;
				float time_wheel;

				time_currwheel= PIL_check_seconds_timer();
				time_wheel = (float)(time_currwheel - fly->time_lastwheel);
				fly->time_lastwheel = time_currwheel;
				time_wheel = 1+ (10 - (20*MIN2(time_wheel, 0.5))); /* 0-0.5 -> 0-5.0 */

				if (fly->speed>0) fly->speed=0;
				else {
					if (event->shift)
						fly->speed-= fly->grid*time_wheel*0.1;
					else
						fly->speed-= fly->grid*time_wheel;
				}
				break;
			}
			case FLY_MODAL_PAN_ENABLE:
				fly->pan_view= TRUE;
				break;
			case FLY_MODAL_PAN_DISABLE:
//XXX2.5		warp_pointer(cent_orig[0], cent_orig[1]);
				fly->pan_view= FALSE;
				break;

				/* impliment WASD keys */
			case FLY_MODAL_DIR_FORWARD:
				if (fly->speed < 0.0f) fly->speed= -fly->speed; /* flip speed rather then stopping, game like motion */
				else fly->speed += fly->grid; /* increse like mousewheel if were alredy moving in that difection*/
				fly->axis= 2;
				break;
			case FLY_MODAL_DIR_BACKWARD:
				if (fly->speed>0) fly->speed= -fly->speed;
				else fly->speed -= fly->grid;
				fly->axis= 2;
				break;
			case FLY_MODAL_DIR_LEFT:
				if (fly->speed < 0.0f) fly->speed= -fly->speed;
				fly->axis= 0;
				break;
			case FLY_MODAL_DIR_RIGHT:
				if (fly->speed > 0.0f) fly->speed= -fly->speed;
				fly->axis= 0;
				break;

			case FLY_MODAL_DIR_UP:
				if (fly->speed > 0.0f) fly->speed= -fly->speed;
				fly->axis= 1;
				break;

			case FLY_MODAL_DIR_DOWN:
				if (fly->speed < 0.0f) fly->speed= -fly->speed;
				fly->axis= 1;
				break;

			case FLY_MODAL_AXIS_LOCK_X:
				if (fly->xlock) fly->xlock=0;
				else {
					fly->xlock = 2;
					fly->xlock_momentum = 0.0;
				}
				break;
			case FLY_MODAL_AXIS_LOCK_Z:
				if (fly->zlock) fly->zlock=0;
				else {
					fly->zlock = 2;
					fly->zlock_momentum = 0.0;
				}
				break;

			case FLY_MODAL_PRECISION_ENABLE:
				fly->use_precision= TRUE;
				break;
			case FLY_MODAL_PRECISION_DISABLE:
				fly->use_precision= FALSE;
				break;

		}
	}
}

static int flyApply(bContext *C, FlyInfo *fly)
{
	/*
	fly mode - Shift+F
	a fly loop where the user can move move the view as if they are flying
	*/
	RegionView3D *rv3d= fly->rv3d;
	View3D *v3d = fly->v3d;
	ARegion *ar = fly->ar;
	Scene *scene= fly->scene;

	float prev_view_mat[4][4];

	float mat[3][3], /* 3x3 copy of the view matrix so we can move allong the view axis */
	dvec[3]={0,0,0}, /* this is the direction thast added to the view offset per redraw */

	/* Camera Uprighting variables */
	upvec[3]={0,0,0}, /* stores the view's up vector */

	moffset[2], /* mouse offset from the views center */
	tmp_quat[4]; /* used for rotating the view */

	int cent_orig[2], /* view center */
//XXX- can avoid using // 	cent[2], /* view center modified */
	xmargin, ymargin; /* x and y margin are define the safe area where the mouses movement wont rotate the view */
	unsigned char
	apply_rotation= 1; /* if the user presses shift they can look about without movinf the direction there looking*/

	if(fly->root_parent)
		view3d_persp_mat4(rv3d, prev_view_mat);

	/* the dist defines a vector that is infront of the offset
	to rotate the view about.
	this is no good for fly mode because we
	want to rotate about the viewers center.
	but to correct the dist removal we must
	alter offset so the view doesn't jump. */

	xmargin= ar->winx/20.0f;
	ymargin= ar->winy/20.0f;

	cent_orig[0]= ar->winrct.xmin + ar->winx/2;
	cent_orig[1]= ar->winrct.ymin + ar->winy/2;

	{

		/* mouse offset from the center */
		moffset[0]= fly->mval[0]- ar->winx/2;
		moffset[1]= fly->mval[1]- ar->winy/2;

		/* enforce a view margin */
		if (moffset[0]>xmargin)			moffset[0]-=xmargin;
		else if (moffset[0] < -xmargin)	moffset[0]+=xmargin;
		else							moffset[0]=0;

		if (moffset[1]>ymargin)			moffset[1]-=ymargin;
		else if (moffset[1] < -ymargin)	moffset[1]+=ymargin;
		else							moffset[1]=0;


		/* scale the mouse movement by this value - scales mouse movement to the view size
		 * moffset[0]/(ar->winx-xmargin*2) - window size minus margin (same for y)
		 *
		 * the mouse moves isnt linear */

		if(moffset[0]) {
			moffset[0] /= ar->winx - (xmargin*2);
			moffset[0] *= fabs(moffset[0]);
		}

		if(moffset[1]) {
			moffset[1] /= ar->winy - (ymargin*2);
			moffset[1] *= fabs(moffset[1]);
		}

		/* Should we redraw? */
		if(fly->speed != 0.0f || moffset[0] || moffset[1] || fly->zlock || fly->xlock || dvec[0] || dvec[1] || dvec[2] ) {
			float dvec_tmp[3];
			double time_current, time_redraw; /*time how fast it takes for us to redraw, this is so simple scenes dont fly too fast */
			float time_redraw_clamped;

			time_current= PIL_check_seconds_timer();
			time_redraw= (float)(time_current - fly->time_lastdraw);
			time_redraw_clamped= MIN2(0.05f, time_redraw); /* clamt the redraw time to avoid jitter in roll correction */
			fly->time_lastdraw= time_current;
			/*fprintf(stderr, "%f\n", time_redraw);*/ /* 0.002 is a small redraw 0.02 is larger */

			/* Scale the time to use shift to scale the speed down- just like
			shift slows many other areas of blender down */
			if (fly->use_precision)
				fly->speed= fly->speed * (1.0f-time_redraw_clamped);

			copy_m3_m4(mat, rv3d->viewinv);

			if (fly->pan_view==TRUE) {
				/* pan only */
				dvec_tmp[0]= -moffset[0];
				dvec_tmp[1]= -moffset[1];
				dvec_tmp[2]= 0;

				if (fly->use_precision) {
					dvec_tmp[0] *= 0.1;
					dvec_tmp[1] *= 0.1;
				}

				mul_m3_v3(mat, dvec_tmp);
				mul_v3_fl(dvec_tmp, time_redraw*200.0 * fly->grid);

			} else {
				float roll; /* similar to the angle between the camera's up and the Z-up, but its very rough so just roll*/

				/* rotate about the X axis- look up/down */
				if (moffset[1]) {
					upvec[0]=1;
					upvec[1]=0;
					upvec[2]=0;
					mul_m3_v3(mat, upvec);
					axis_angle_to_quat( tmp_quat, upvec, (float)moffset[1]*-time_redraw*20); /* Rotate about the relative up vec */
					mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

					if (fly->xlock) fly->xlock = 2; /*check for rotation*/
					if (fly->zlock) fly->zlock = 2;
					fly->xlock_momentum= 0.0f;
				}

				/* rotate about the Y axis- look left/right */
				if (moffset[0]) {

					/* if we're upside down invert the moffset */
					upvec[0]=0;
					upvec[1]=1;
					upvec[2]=0;
					mul_m3_v3(mat, upvec);

					if(upvec[2] < 0.0f)
						moffset[0]= -moffset[0];

					/* make the lock vectors */
					if (fly->zlock) {
						upvec[0]=0;
						upvec[1]=0;
						upvec[2]=1;
					} else {
						upvec[0]=0;
						upvec[1]=1;
						upvec[2]=0;
						mul_m3_v3(mat, upvec);
					}

					axis_angle_to_quat( tmp_quat, upvec, (float)moffset[0]*time_redraw*20); /* Rotate about the relative up vec */
					mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

					if (fly->xlock) fly->xlock = 2;/*check for rotation*/
					if (fly->zlock) fly->zlock = 2;
				}

				if (fly->zlock==2) {
					upvec[0]=1;
					upvec[1]=0;
					upvec[2]=0;
					mul_m3_v3(mat, upvec);

					/*make sure we have some z rolling*/
					if (fabs(upvec[2]) > 0.00001f) {
						roll= upvec[2]*5;
						upvec[0]=0; /*rotate the view about this axis*/
						upvec[1]=0;
						upvec[2]=1;

						mul_m3_v3(mat, upvec);
						axis_angle_to_quat( tmp_quat, upvec, roll*time_redraw_clamped*fly->zlock_momentum*0.1); /* Rotate about the relative up vec */
						mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

						fly->zlock_momentum += 0.05f;
					} else {
						fly->zlock=1; /* dont check until the view rotates again */
						fly->zlock_momentum= 0.0f;
					}
				}

				if (fly->xlock==2 && moffset[1]==0) { /*only apply xcorrect when mouse isnt applying x rot*/
					upvec[0]=0;
					upvec[1]=0;
					upvec[2]=1;
					mul_m3_v3(mat, upvec);
					/*make sure we have some z rolling*/
					if (fabs(upvec[2]) > 0.00001) {
						roll= upvec[2] * -5;

						upvec[0]= 1.0f; /*rotate the view about this axis*/
						upvec[1]= 0.0f;
						upvec[2]= 0.0f;

						mul_m3_v3(mat, upvec);

						axis_angle_to_quat( tmp_quat, upvec, roll*time_redraw_clamped*fly->xlock_momentum*0.1f); /* Rotate about the relative up vec */
						mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

						fly->xlock_momentum += 0.05f;
					} else {
						fly->xlock=1; /* see above */
						fly->xlock_momentum= 0.0f;
					}
				}


				if (apply_rotation) {
					/* Normal operation */
					/* define dvec, view direction vector */
					dvec_tmp[0]= dvec_tmp[1]= dvec_tmp[2]= 0.0f;
					/* move along the current axis */
					dvec_tmp[fly->axis]= 1.0f;

					mul_m3_v3(mat, dvec_tmp);

					mul_v3_fl(dvec_tmp, fly->speed * time_redraw * 0.25f);
				}
			}

			/* impose a directional lag */
			interp_v3_v3v3(dvec, dvec_tmp, fly->dvec_prev, (1.0f/(1.0f+(time_redraw*5.0f))));

			if (rv3d->persp==RV3D_CAMOB) {
				Object *lock_ob= fly->root_parent ? fly->root_parent : fly->v3d->camera;
				if (lock_ob->protectflag & OB_LOCK_LOCX) dvec[0] = 0.0;
				if (lock_ob->protectflag & OB_LOCK_LOCY) dvec[1] = 0.0;
				if (lock_ob->protectflag & OB_LOCK_LOCZ) dvec[2] = 0.0;
			}

			add_v3_v3v3(rv3d->ofs, rv3d->ofs, dvec);

			/* todo, dynamic keys */
#if 0
			if (fly->zlock && fly->xlock)
				ED_area_headerprint(fly->ar, "FlyKeys  Speed:(+/- | Wheel),  Upright Axis:X  on/Z on,   Slow:Shift,  Direction:WASDRF,  Ok:LMB,  Pan:MMB,  Cancel:RMB");
			else if (fly->zlock)
				ED_area_headerprint(fly->ar, "FlyKeys  Speed:(+/- | Wheel),  Upright Axis:X off/Z on,   Slow:Shift,  Direction:WASDRF,  Ok:LMB,  Pan:MMB,  Cancel:RMB");
			else if (fly->xlock)
				ED_area_headerprint(fly->ar, "FlyKeys  Speed:(+/- | Wheel),  Upright Axis:X  on/Z off,  Slow:Shift,  Direction:WASDRF,  Ok:LMB,  Pan:MMB,  Cancel:RMB");
			else
				ED_area_headerprint(fly->ar, "FlyKeys  Speed:(+/- | Wheel),  Upright Axis:X off/Z off,  Slow:Shift,  Direction:WASDRF,  Ok:LMB,  Pan:MMB,  Cancel:RMB");
#endif

			/* we are in camera view so apply the view ofs and quat to the view matrix and set the camera to the view */
			if (rv3d->persp==RV3D_CAMOB) {
				ID *id_key;
				/* transform the parent or the camera? */
				if(fly->root_parent) {
					Object *ob_update;
                    
					float view_mat[4][4];
					float prev_view_imat[4][4];
					float diff_mat[4][4];
					float parent_mat[4][4];

					invert_m4_m4(prev_view_imat, prev_view_mat);
					view3d_persp_mat4(rv3d, view_mat);
					mul_m4_m4m4(diff_mat, prev_view_imat, view_mat);
					mul_m4_m4m4(parent_mat, fly->root_parent->obmat, diff_mat);
					object_apply_mat4(fly->root_parent, parent_mat);

					// where_is_object(scene, fly->root_parent);

					ob_update= v3d->camera->parent;
					while(ob_update) {
						DAG_id_flush_update(&ob_update->id, OB_RECALC_OB);
						ob_update= ob_update->parent;
					}

					copy_m4_m4(prev_view_mat, view_mat);

					id_key= &fly->root_parent->id;

				}
				else {
					float view_mat[4][4];
					view3d_persp_mat4(rv3d, view_mat);
					object_apply_mat4(v3d->camera, view_mat);
					id_key= &v3d->camera->id;
				}

				/* record the motion */
				if (autokeyframe_cfra_can_key(scene, id_key)) {
					ListBase dsources = {NULL, NULL};
					
					/* add datasource override for the camera object */
					ANIM_relative_keyingset_add_source(&dsources, id_key, NULL, NULL); 
					
					/* insert keyframes 
					 *	1) on the first frame
					 *	2) on each subsequent frame
					 *		TODO: need to check in future that frame changed before doing this 
					 */
					if (fly->xlock || fly->zlock || moffset[0] || moffset[1]) {
						KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "Rotation");
						ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
					}
					if (fly->speed) {
						KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "Location");
						ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
					}
					
					/* free temp data */
					BLI_freelistN(&dsources);
				}
			}
		} else
			/*were not redrawing but we need to update the time else the view will jump */
			fly->time_lastdraw= PIL_check_seconds_timer();
		/* end drawing */
		VECCOPY(fly->dvec_prev, dvec);
	}

/* moved to flyEnd() */

	return OPERATOR_FINISHED;
}



static int fly_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	FlyInfo *fly;

	if(rv3d->viewlock)
		return OPERATOR_CANCELLED;

	fly= MEM_callocN(sizeof(FlyInfo), "FlyOperation");

	op->customdata= fly;

	if(initFlyInfo(C, fly, op, event)==FALSE) {
		MEM_freeN(op->customdata);
		return OPERATOR_CANCELLED;
	}

	flyEvent(fly, event);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int fly_cancel(bContext *C, wmOperator *op)
{
	FlyInfo *fly = op->customdata;

	fly->state = FLY_CANCEL;
	flyEnd(C, fly);
	op->customdata= NULL;

	return OPERATOR_CANCELLED;
}

static int fly_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	int exit_code;

	FlyInfo *fly = op->customdata;

	fly->redraw= 0;

	flyEvent(fly, event);

	if(event->type==TIMER && event->customdata == fly->timer)
		flyApply(C, fly);

	if(fly->redraw) {
		ED_region_tag_redraw(CTX_wm_region(C));
	}

	exit_code = flyEnd(C, fly);

	if(exit_code!=OPERATOR_RUNNING_MODAL)
		ED_region_tag_redraw(CTX_wm_region(C));

	return exit_code;
}

void VIEW3D_OT_fly(wmOperatorType *ot)
{

	/* identifiers */
	ot->name= "Fly Navigation";
	ot->description= "Interactively fly around the scene";
	ot->idname= "VIEW3D_OT_fly";

	/* api callbacks */
	ot->invoke= fly_invoke;
	ot->cancel= fly_cancel;
	ot->modal= fly_modal;
	ot->poll= ED_operator_view3d_active;

	/* flags */
	ot->flag= OPTYPE_BLOCKING;

}

/* ************************************** */

void view3d_align_axis_to_vector(View3D *v3d, RegionView3D *rv3d, int axisidx, float vec[3])
{
	float alignaxis[3] = {0.0, 0.0, 0.0};
	float norm[3], axis[3], angle, new_quat[4];
	
	if(axisidx > 0) alignaxis[axisidx-1]= 1.0;
	else alignaxis[-axisidx-1]= -1.0;

	normalize_v3_v3(norm, vec);

	angle= (float)acos(dot_v3v3(alignaxis, norm));
	cross_v3_v3v3(axis, alignaxis, norm);
	axis_angle_to_quat( new_quat,axis, -angle);
	
	rv3d->view= 0;
	
	if (rv3d->persp==RV3D_CAMOB && v3d->camera) {
		/* switch out of camera view */
		float orig_ofs[3];
		float orig_dist= rv3d->dist;
		float orig_lens= v3d->lens;
		
		VECCOPY(orig_ofs, rv3d->ofs);
		rv3d->persp= RV3D_PERSP;
		rv3d->dist= 0.0;
		view_settings_from_ob(v3d->camera, rv3d->ofs, NULL, NULL, &v3d->lens);
		smooth_view(NULL, NULL, NULL, orig_ofs, new_quat, &orig_dist, &orig_lens); // XXX
	} else {
		if (rv3d->persp==RV3D_CAMOB) rv3d->persp= RV3D_PERSP; /* switch out of camera mode */
		smooth_view(NULL, NULL, NULL, NULL, new_quat, NULL, NULL); // XXX
	}
}

