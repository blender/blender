/*
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

/** \file blender/editors/space_view3d/view3d_view.c
 *  \ingroup spview3d
 */


#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_anim.h"
#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_draw.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_armature.h"

#ifdef WITH_GAMEENGINE
#include "SYS_System.h"
#endif

#include "view3d_intern.h"	// own include

/* use this call when executing an operator,
   event system doesn't set for each event the
   opengl drawing context */
void view3d_operator_needs_opengl(const bContext *C)
{
	wmWindow *win = CTX_wm_window(C);
	ARegion *ar= CTX_wm_region(C);
	
	view3d_region_operator_needs_opengl(win, ar);
}

void view3d_region_operator_needs_opengl(wmWindow *win, ARegion *ar)
{
	/* for debugging purpose, context should always be OK */
	if ((ar == NULL) || (ar->regiontype!=RGN_TYPE_WINDOW))
		printf("view3d_region_operator_needs_opengl error, wrong region\n");
	else {
		RegionView3D *rv3d= ar->regiondata;
		
		wmSubWindowSet(win, ar->swinid);
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
			fac= cosf((float)M_PI*la->spotsize/360.0f);
			x1= saacos(fac);
			*lens= 16.0f*fac/sinf(x1);
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
void view3d_settings_from_ob(Object *ob, float *ofs, float *quat, float *dist, float *lens)
{
	if (!ob) return;

	/* Offset */
	if (ofs)
		negate_v3_v3(ofs, ob->obmat[3]);

	/* Quat */
	if (quat) {
		float imat[4][4];
		invert_m4_m4(imat, ob->obmat);
		mat4_to_quat(quat, imat);
	}

	if (dist) {
		float tquat[4];
		float vec[3];

		vec[0]= 0.0f;
		vec[1]= 0.0f;
		vec[2]= -(*dist);

		mat4_to_quat(tquat, ob->obmat);

		mul_qt_v3(tquat, vec);

		sub_v3_v3(ofs, vec);
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
	struct SmoothViewStore sms= {0};
	short ok= FALSE;
	
	/* initialize sms */
	copy_v3_v3(sms.new_ofs, rv3d->ofs);
	copy_qt_qt(sms.new_quat, rv3d->viewquat);
	sms.new_dist= rv3d->dist;
	sms.new_lens= v3d->lens;
	sms.to_camera= 0;
	
	/* store the options we want to end with */
	if(ofs) copy_v3_v3(sms.new_ofs, ofs);
	if(quat) copy_qt_qt(sms.new_quat, quat);
	if(dist) sms.new_dist= *dist;
	if(lens) sms.new_lens= *lens;

	if (camera) {
		view3d_settings_from_ob(camera, sms.new_ofs, sms.new_quat, &sms.new_dist, &sms.new_lens);
		sms.to_camera= 1; /* restore view3d values in end */
	}
	
	if (C && U.smooth_viewtx) {
		int changed = 0; /* zero means no difference */
		
		if (oldcamera != camera)
			changed = 1;
		else if (sms.new_dist != rv3d->dist)
			changed = 1;
		else if (sms.new_lens != v3d->lens)
			changed = 1;
		else if (!equals_v3v3(sms.new_ofs, rv3d->ofs))
			changed = 1;
		else if (!equals_v4v4(sms.new_quat, rv3d->viewquat))
			changed = 1;
		
		/* The new view is different from the old one
			* so animate the view */
		if (changed) {

			/* original values */
			if (oldcamera) {
				sms.orig_dist= rv3d->dist; // below function does weird stuff with it...
				view3d_settings_from_ob(oldcamera, sms.orig_ofs, sms.orig_quat, &sms.orig_dist, &sms.orig_lens);
			}
			else {
				copy_v3_v3(sms.orig_ofs, rv3d->ofs);
				copy_qt_qt(sms.orig_quat, rv3d->viewquat);
				sms.orig_dist= rv3d->dist;
				sms.orig_lens= v3d->lens;
			}
			/* grid draw as floor */
			if((rv3d->viewlock & RV3D_LOCKED)==0) {
				/* use existing if exists, means multiple calls to smooth view wont loose the original 'view' setting */
				sms.orig_view= rv3d->sms ? rv3d->sms->orig_view : rv3d->view;
				rv3d->view= 0;
			}

			sms.time_allowed= (double)U.smooth_viewtx / 1000.0;
			
			/* if this is view rotation only
				* we can decrease the time allowed by
				* the angle between quats 
				* this means small rotations wont lag */
			if (quat && !ofs && !dist) {
				float vec1[3]={0,0,1}, vec2[3]= {0,0,1};
				float q1[4], q2[4];

				invert_qt_qt(q1, sms.new_quat);
				invert_qt_qt(q2, sms.orig_quat);

				mul_qt_v3(q1, vec1);
				mul_qt_v3(q2, vec2);

				/* scale the time allowed by the rotation */
				sms.time_allowed *= (double)angle_v3v3(vec1, vec2) / M_PI; /* 180deg == 1.0 */
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
			
			ok= TRUE;
		}
	}
	
	/* if we get here nothing happens */
	if(ok == FALSE) {
		ARegion *ar= CTX_wm_region(C);

		if(sms.to_camera==0) {
			copy_v3_v3(rv3d->ofs, sms.new_ofs);
			copy_qt_qt(rv3d->viewquat, sms.new_quat);
			rv3d->dist = sms.new_dist;
			v3d->lens = sms.new_lens;
		}

		if(rv3d->viewlock & RV3D_BOXVIEW)
			view3d_boxview_copy(CTX_wm_area(C), ar);

		ED_region_tag_redraw(ar);
	}
}

/* only meant for timer usage */
static int view3d_smoothview_invoke(bContext *C, wmOperator *UNUSED(op), wmEvent *event)
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	struct SmoothViewStore *sms= rv3d->sms;
	float step, step_inv;
	
	/* escape if not our timer */
	if(rv3d->smooth_timer==NULL || rv3d->smooth_timer!=event->customdata)
		return OPERATOR_PASS_THROUGH;
	
	if(sms->time_allowed != 0.0)
		step = (float)((rv3d->smooth_timer->duration)/sms->time_allowed);
	else
		step = 1.0f;
	
	/* end timer */
	if(step >= 1.0f) {
		
		/* if we went to camera, store the original */
		if(sms->to_camera) {
			rv3d->persp= RV3D_CAMOB;
			copy_v3_v3(rv3d->ofs, sms->orig_ofs);
			copy_qt_qt(rv3d->viewquat, sms->orig_quat);
			rv3d->dist = sms->orig_dist;
			v3d->lens = sms->orig_lens;
		}
		else {
			copy_v3_v3(rv3d->ofs, sms->new_ofs);
			copy_qt_qt(rv3d->viewquat, sms->new_quat);
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
		if (step < 0.5f)	step = (float)pow(step*2.0f, 2.0)/2.0f;
		else				step = (float)1.0f-(powf(2.0f*(1.0f-step),2.0f)/2.0f);

		step_inv = 1.0f-step;

		for (i=0; i<3; i++)
			rv3d->ofs[i] = sms->new_ofs[i] * step + sms->orig_ofs[i]*step_inv;

		interp_qt_qtqt(rv3d->viewquat, sms->orig_quat, sms->new_quat, step);
		
		rv3d->dist = sms->new_dist * step + sms->orig_dist*step_inv;
		v3d->lens = sms->new_lens * step + sms->orig_lens*step_inv;
	}
	
	if(rv3d->viewlock & RV3D_BOXVIEW)
		view3d_boxview_copy(CTX_wm_area(C), CTX_wm_region(C));
	
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_VIEW3D, v3d);
	
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

static void setcameratoview3d(RegionView3D *rv3d, Object *ob)
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


static int view3d_setcameratoview_exec(bContext *C, wmOperator *UNUSED(op))
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d= CTX_wm_region_view3d(C);

	copy_qt_qt(rv3d->lviewquat, rv3d->viewquat);
	rv3d->lview= rv3d->view;
	if(rv3d->persp != RV3D_CAMOB) {
		rv3d->lpersp= rv3d->persp;
	}

	setcameratoview3d(rv3d, v3d->camera);
	DAG_id_tag_update(&v3d->camera->id, OB_RECALC_OB);
	rv3d->persp = RV3D_CAMOB;
	
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, v3d->camera);
	
	return OPERATOR_FINISHED;

}

static int view3d_setcameratoview_poll(bContext *C)
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


static int view3d_setobjectascamera_exec(bContext *C, wmOperator *UNUSED(op))
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	Scene *scene= CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);

	if(ob) {
		Object *camera_old= (rv3d->persp == RV3D_CAMOB && scene->camera) ? scene->camera : NULL;
		rv3d->persp= RV3D_CAMOB;
		v3d->camera= ob;
		if(v3d->scenelock)
			scene->camera= ob;

		if(camera_old != ob) /* unlikely but looks like a glitch when set to the same */
			smooth_view(C, camera_old, v3d->camera, rv3d->ofs, rv3d->viewquat, &rv3d->dist, &v3d->lens);

		WM_event_add_notifier(C, NC_SCENE|ND_RENDER_OPTIONS|NC_OBJECT|ND_DRAW, CTX_data_scene(C));
	}
	
	return OPERATOR_FINISHED;
}

static int region3d_unlocked_poll(bContext *C)
{
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	return (rv3d && rv3d->viewlock==0);
}


void VIEW3D_OT_object_as_camera(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Set Active Object as Camera";
	ot->description= "Set the active object as the active camera for this view or scene";
	ot->idname= "VIEW3D_OT_object_as_camera";
	
	/* api callbacks */
	ot->exec= view3d_setobjectascamera_exec;	
	ot->poll= region3d_unlocked_poll;
	
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
		sub_v3_v3(vec, ray_start);
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

		copy_v3_v3(p1, coord);
		p1[3] = 1.0f;
		copy_v3_v3(p2, p1);
		p2[3] = 1.0f;
		mul_m4_v4(rv3d->viewmat, p2);

		mul_v3_fl(p2, 2.0f);

		mul_m4_v4(rv3d->viewinv, p2);

		sub_v3_v3v3(vec, p1, p2);
	}
	else {
		copy_v3_v3(vec, rv3d->viewinv[2]);
	}
	normalize_v3(vec);
}

int initgrabz(RegionView3D *rv3d, float x, float y, float z)
{
	int flip= FALSE;
	if(rv3d==NULL) return flip;
	rv3d->zfac= rv3d->persmat[0][3]*x+ rv3d->persmat[1][3]*y+ rv3d->persmat[2][3]*z+ rv3d->persmat[3][3];
	if (rv3d->zfac < 0.0f)
		flip= TRUE;
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
	
	return flip;
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

/* doesn't rely on initgrabz */
/* for perspective view, get the vector direction to
 * the mouse cursor as a normalized vector */
void window_to_3d_vector(ARegion *ar, float *vec, short mx, short my)
{
	RegionView3D *rv3d= ar->regiondata;
	float dx, dy;
	float viewvec[3];

	dx= 2.0f*mx/ar->winx;
	dy= 2.0f*my/ar->winy;

	/* normalize here so vecs are proportional to eachother */
	normalize_v3_v3(viewvec, rv3d->viewinv[2]);

	vec[0]= viewvec[0] - (rv3d->persinv[0][0]*dx + rv3d->persinv[1][0]*dy);
	vec[1]= viewvec[1] - (rv3d->persinv[0][1]*dx + rv3d->persinv[1][1]*dy);
	vec[2]= viewvec[2] - (rv3d->persinv[0][2]*dx + rv3d->persinv[1][2]*dy);

	normalize_v3(vec);
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
	copy_v3_v3(vec4, vec);
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
		copy_v3_v3(vec, bb->vec[a]);
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
	
	copy_v3_v3(vec4, vec);
	vec4[3]= 1.0;
	mul_m4_v4(rv3d->persmat, vec4);
	
	if( vec4[3] > (float)BL_NEAR_CLIP ) {	/* 0.001 is the NEAR clipping cutoff for picking */
		fx= (ar->winx/2)*(1 + vec4[0]/vec4[3]);
		
		if( fx>0 && fx<ar->winx) {
			
			fy= (ar->winy/2)*(1 + vec4[1]/vec4[3]);
			
			if(fy > 0.0f && fy < (float)ar->winy) {
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
	copy_v3_v3(vec4, vec);
	vec4[3]= 1.0;
	
	mul_m4_v4(rv3d->persmat, vec4);
	
	if( vec4[3] > (float)BL_NEAR_CLIP ) {	/* 0.001 is the NEAR clipping cutoff for picking */
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
	
	copy_v3_v3(vec4, vec);
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
	copy_v3_v3(vec4, vec);
	vec4[3]= 1.0;
	
	mul_m4_v4(rv3d->persmat, vec4);
	
	if( vec4[3] > (float)BL_NEAR_CLIP ) {	/* 0.001 is the NEAR clipping cutoff for picking */
		fx= (ar->winx/2)*(1 + vec4[0]/vec4[3]);
		
		if( fx>-32700 && fx<32700) {
			
			fy= (ar->winy/2)*(1 + vec4[1]/vec4[3]);
			
			if(fy > -32700.0f && fy < 32700.0f) {
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
	copy_v3_v3(vec4, vec);
	vec4[3]= 1.0;
	
	mul_m4_v4(rv3d->persmat, vec4);
	
	if(vec4[3] > (float)BL_NEAR_CLIP) {
		adr[0] = (float)(ar->winx/2.0f)+(ar->winx/2.0f)*vec4[0]/vec4[3];
		adr[1] = (float)(ar->winy/2.0f)+(ar->winy/2.0f)*vec4[1]/vec4[3];
	}
}

void project_float_noclip(ARegion *ar, float *vec, float *adr)
{
	RegionView3D *rv3d= ar->regiondata;
	float vec4[4];
	
	copy_v3_v3(vec4, vec);
	vec4[3]= 1.0;
	
	mul_m4_v4(rv3d->persmat, vec4);
	
	if( fabs(vec4[3]) > BL_NEAR_CLIP ) {
		adr[0] = (float)(ar->winx/2.0f)+(ar->winx/2.0f)*vec4[0]/vec4[3];
		adr[1] = (float)(ar->winy/2.0f)+(ar->winy/2.0f)*vec4[1]/vec4[3];
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
		*clipend *= 0.5f;	// otherwise too extreme low zbuffer quality
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
				fac= cosf(((float)M_PI)*la->spotsize/360.0f);
				
				x1= saacos(fac);
				lens= 16.0f*fac/sinf(x1);
				
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
		
		*clipend *= 0.5f;	// otherwise too extreme low zbuffer quality
		*clipsta= - *clipend;
		orth= 1;
	}
	else {
		/* fac for zoom, also used for camdx */
		if(rv3d->persp==RV3D_CAMOB) {
			fac= (1.41421f + ( (float)rv3d->camzoom )/50.0f);
			fac*= fac;
		}
		else fac= 2.0;
		
		/* viewplane size depends... */
		if(cam && cam->type==CAM_ORTHO) {
			/* ortho_scale == 1 means exact 1 to 1 mapping */
			float dfac= 2.0f*cam->ortho_scale/fac;
			
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
			
			if(winx>winy) dfac= 64.0f/(fac*winx*lens);
			else dfac= 64.0f/(fac*winy*lens);
			
			x1= - *clipsta * winx*dfac;
			x2= -x1;
			y1= - *clipsta * winy*dfac;
			y2= -y1;
			orth= 0;
		}
		/* cam view offset */
		if(cam) {
			float dx= 0.5f*fac*rv3d->camdx*(x2-x1);
			float dy= 0.5f*fac*rv3d->camdy*(y2-y1);

			/* shift offset */		
			if(cam->type==CAM_ORTHO) {
				dx += cam->shiftx * cam->ortho_scale;
				dy += cam->shifty * cam->ortho_scale;
			}
			else {
				dx += cam->shiftx * (cam->clipsta / cam->lens) * 32.0f;
				dy += cam->shifty * (cam->clipsta / cam->lens) * 32.0f;
			}

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
			viewfac= (((winx >= winy)? winx: winy)*lens)/32.0f;
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
			copy_v3_v3(orig_ofs, rv3d->ofs);
			
			/* Switch from camera view */
			mat3_to_quat( new_quat,tmat);
			
			rv3d->persp=RV3D_PERSP;
			rv3d->dist= 0.0;
			
			view3d_settings_from_ob(v3d->camera, rv3d->ofs, NULL, NULL, &v3d->lens);
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

int ED_view3d_lock(RegionView3D *rv3d)
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
	default:
		return FALSE;
	}

	return TRUE;
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
			ED_view3d_lock(rv3d);
		
		quat_to_mat4( rv3d->viewmat,rv3d->viewquat);
		if(rv3d->persp==RV3D_PERSP) rv3d->viewmat[3][2]-= rv3d->dist;
		if(v3d->ob_centre) {
			Object *ob= v3d->ob_centre;
			float vec[3];
			
			copy_v3_v3(vec, ob->obmat[3]);
			if(ob->type==OB_ARMATURE && v3d->ob_centre_bone[0]) {
				bPoseChannel *pchan= get_pose_channel(ob->pose, v3d->ob_centre_bone);
				if(pchan) {
					copy_v3_v3(vec, pchan->pose_mat[3]);
					mul_m4_v3(ob->obmat, vec);
				}
			}
			translate_m4( rv3d->viewmat,-vec[0], -vec[1], -vec[2]);
		}
		else if (v3d->ob_centre_cursor) {
			float vec[3];
			copy_v3_v3(vec, give_cursor(scene, v3d));
			translate_m4(rv3d->viewmat, -vec[0], -vec[1], -vec[2]);
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

static unsigned int free_localbit(Main *bmain)
{
	unsigned int lay;
	ScrArea *sa;
	bScreen *sc;
	
	lay= 0;
	
	/* sometimes we loose a localview: when an area is closed */
	/* check all areas: which localviews are in use? */
	for(sc= bmain->screen.first; sc; sc= sc->id.next) {
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

int ED_view3d_scene_layer_set(int lay, const int *values, int *active)
{
	int i, tot= 0;
	
	/* ensure we always have some layer selected */
	for(i=0; i<20; i++)
		if(values[i])
			tot++;
	
	if(tot==0)
		return lay;
	
	for(i=0; i<20; i++) {
		
		if (active) {
			/* if this value has just been switched on, make that layer active */
			if (values[i] && (lay & (1<<i))==0) {
				*active = (1<<i);
			}
		}
			
		if (values[i]) lay |= (1<<i);
		else lay &= ~(1<<i);
	}
	
	/* ensure always an active layer */
	if (active && (lay & *active)==0) {
		for(i=0; i<20; i++) {
			if(lay & (1<<i)) {
				*active= 1<<i;
				break;
			}
		}
	}
	
	return lay;
}

static void initlocalview(Main *bmain, Scene *scene, ScrArea *sa)
{
	View3D *v3d= sa->spacedata.first;
	Base *base;
	float size = 0.0, min[3], max[3], box[3];
	unsigned int locallay;
	int ok=0;

	if(v3d->localvd) return;

	INIT_MINMAX(min, max);

	locallay= free_localbit(bmain);

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
		if(size <= 0.01f) size= 0.01f;
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
				
				rv3d->ofs[0]= -(min[0]+max[0])/2.0f;
				rv3d->ofs[1]= -(min[1]+max[1])/2.0f;
				rv3d->ofs[2]= -(min[2]+max[2])/2.0f;

				rv3d->dist= size;
				/* perspective should be a bit farther away to look nice */
				if(rv3d->persp==RV3D_ORTHO)
					rv3d->dist*= 0.7f;

				// correction for window aspect ratio
				if(ar->winy>2 && ar->winx>2) {
					float asp= (float)ar->winx/(float)ar->winy;
					if(asp < 1.0f) asp= 1.0f/asp;
					rv3d->dist*= asp;
				}
				
				if (rv3d->persp==RV3D_CAMOB) rv3d->persp= RV3D_PERSP;
				
				v3d->cursor[0]= -rv3d->ofs[0];
				v3d->cursor[1]= -rv3d->ofs[1];
				v3d->cursor[2]= -rv3d->ofs[2];
			}
		}
		
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
				copy_v3_v3(rv3d->ofs, rv3d->localvd->ofs);
				copy_qt_qt(rv3d->viewquat, rv3d->localvd->viewquat);
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

static int localview_exec(bContext *C, wmOperator *UNUSED(unused))
{
	View3D *v3d= CTX_wm_view3d(C);
	
	if(v3d->localvd)
		endlocalview(CTX_data_scene(C), CTX_wm_area(C));
	else
		initlocalview(CTX_data_main(C), CTX_data_scene(C), CTX_wm_area(C));
	
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
	ot->flag= OPTYPE_UNDO; /* localview changes object layer bitflags */
	
	ot->poll= ED_operator_view3d_active;
}

#ifdef WITH_GAMEENGINE

static ListBase queue_back;
static void SaveState(bContext *C, wmWindow *win)
{
	Object *obact = CTX_data_active_object(C);
	
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	if(obact && obact->mode & OB_MODE_TEXTURE_PAINT)
		GPU_paint_set_mipmap(1);
	
	queue_back= win->queue;
	
	win->queue.first= win->queue.last= NULL;
	
	//XXX waitcursor(1);
}

static void RestoreState(bContext *C, wmWindow *win)
{
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
	
	if(win) /* check because closing win can set to NULL */
		win->queue= queue_back;
	
	GPU_state_init();
	GPU_set_tpage(NULL, 0);

	glPopAttrib();
}

/* was space_set_commmandline_options in 2.4x */
static void game_set_commmandline_options(GameData *gm)
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

#endif // WITH_GAMEENGINE

static int game_engine_poll(bContext *C)
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

	/* sa can be NULL when called from python */
	if(sa==NULL || sa->spacetype != SPACE_VIEW3D)
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

	return 1;
}

static int game_engine_exec(bContext *C, wmOperator *op)
{
#ifdef WITH_GAMEENGINE
	Scene *startscene = CTX_data_scene(C);
	ScrArea *sa, *prevsa= CTX_wm_area(C);
	ARegion *ar, *prevar= CTX_wm_region(C);
	wmWindow *prevwin= CTX_wm_window(C);
	RegionView3D *rv3d;
	rcti cam_frame;

	(void)op; /* unused */
	
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
		view3d_calc_camera_border(startscene, ar, rv3d, CTX_wm_view3d(C), &cam_framef, FALSE);
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


	SaveState(C, prevwin);

	StartKetsjiShell(C, ar, &cam_frame, 1);

	/* window wasnt closed while the BGE was running */
	if(BLI_findindex(&CTX_wm_manager(C)->windows, prevwin) == -1) {
		prevwin= NULL;
		CTX_wm_window_set(C, NULL);
	}
	
	if(prevwin) {
		/* restore context, in case it changed in the meantime, for
		   example by working in another window or closing it */
		CTX_wm_region_set(C, prevar);
		CTX_wm_window_set(C, prevwin);
		CTX_wm_area_set(C, prevsa);
	}

	RestoreState(C, prevwin);

	//XXX restore_all_scene_cfra(scene_cfra_store);
	set_scene_bg(CTX_data_main(C), startscene);
	//XXX scene_update_for_newframe(bmain, scene, scene->lay);
	
	ED_area_tag_redraw(CTX_wm_area(C));

	return OPERATOR_FINISHED;
#else
	(void)C; /* unused */
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
		
		copy_v3_v3(orig_ofs, rv3d->ofs);
		rv3d->persp= RV3D_PERSP;
		rv3d->dist= 0.0;
		view3d_settings_from_ob(v3d->camera, rv3d->ofs, NULL, NULL, &v3d->lens);
		smooth_view(NULL, NULL, NULL, orig_ofs, new_quat, &orig_dist, &orig_lens); // XXX
	} else {
		if (rv3d->persp==RV3D_CAMOB) rv3d->persp= RV3D_PERSP; /* switch out of camera mode */
		smooth_view(NULL, NULL, NULL, NULL, new_quat, NULL, NULL); // XXX
	}
}

int view3d_is_ortho(View3D *v3d, RegionView3D *rv3d)
{
	return (rv3d->persp == RV3D_ORTHO || (v3d->camera && ((Camera *)v3d->camera->data)->type == CAM_ORTHO));
}

float view3d_pixel_size(struct RegionView3D *rv3d, const float co[3])
{
	return  (rv3d->persmat[3][3] + (
				rv3d->persmat[0][3]*co[0] +
				rv3d->persmat[1][3]*co[1] +
				rv3d->persmat[2][3]*co[2])
			) * rv3d->pixsize;
}
