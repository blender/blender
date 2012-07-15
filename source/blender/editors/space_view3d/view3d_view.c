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

/** \file blender/editors/space_view3d/view3d_view.c
 *  \ingroup spview3d
 */


#include "DNA_camera_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_anim.h"
#include "BKE_action.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_draw.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_armature.h"

#ifdef WITH_GAMEENGINE
#include "BL_System.h"
#endif

#include "RNA_access.h"
#include "RNA_define.h"

#include "view3d_intern.h"  /* own include */

/* use this call when executing an operator,
 * event system doesn't set for each event the
 * opengl drawing context */
void view3d_operator_needs_opengl(const bContext *C)
{
	wmWindow *win = CTX_wm_window(C);
	ARegion *ar = CTX_wm_region(C);
	
	view3d_region_operator_needs_opengl(win, ar);
}

void view3d_region_operator_needs_opengl(wmWindow *win, ARegion *ar)
{
	/* for debugging purpose, context should always be OK */
	if ((ar == NULL) || (ar->regiontype != RGN_TYPE_WINDOW))
		printf("view3d_region_operator_needs_opengl error, wrong region\n");
	else {
		RegionView3D *rv3d = ar->regiondata;
		
		wmSubWindowSet(win, ar->swinid);
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(rv3d->winmat);
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(rv3d->viewmat);
	}
}

float *give_cursor(Scene *scene, View3D *v3d)
{
	if (v3d && v3d->localvd) return v3d->cursor;
	else return scene->cursor;
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
void smooth_view(bContext *C, View3D *v3d, ARegion *ar, Object *oldcamera, Object *camera,
                 float *ofs, float *quat, float *dist, float *lens)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = CTX_wm_area(C);

	RegionView3D *rv3d = ar->regiondata;
	struct SmoothViewStore sms = {0};
	short ok = FALSE;
	
	/* initialize sms */
	copy_v3_v3(sms.new_ofs, rv3d->ofs);
	copy_qt_qt(sms.new_quat, rv3d->viewquat);
	sms.new_dist = rv3d->dist;
	sms.new_lens = v3d->lens;
	sms.to_camera = 0;

	/* note on camera locking, this is a little confusing but works ok.
	 * we may be changing the view 'as if' there is no active camera, but in fact
	 * there is an active camera which is locked to the view.
	 *
	 * In the case where smooth view is moving _to_ a camera we don't want that
	 * camera to be moved or changed, so only when the camera is not being set should
	 * we allow camera option locking to initialize the view settings from the camera.
	 */
	if (camera == NULL && oldcamera == NULL) {
		ED_view3d_camera_lock_init(v3d, rv3d);
	}

	/* store the options we want to end with */
	if (ofs) copy_v3_v3(sms.new_ofs, ofs);
	if (quat) copy_qt_qt(sms.new_quat, quat);
	if (dist) sms.new_dist = *dist;
	if (lens) sms.new_lens = *lens;

	if (camera) {
		ED_view3d_from_object(camera, sms.new_ofs, sms.new_quat, &sms.new_dist, &sms.new_lens);
		sms.to_camera = 1; /* restore view3d values in end */
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
				sms.orig_dist = rv3d->dist;  /* below function does weird stuff with it... */
				ED_view3d_from_object(oldcamera, sms.orig_ofs, sms.orig_quat, &sms.orig_dist, &sms.orig_lens);
			}
			else {
				copy_v3_v3(sms.orig_ofs, rv3d->ofs);
				copy_qt_qt(sms.orig_quat, rv3d->viewquat);
				sms.orig_dist = rv3d->dist;
				sms.orig_lens = v3d->lens;
			}
			/* grid draw as floor */
			if ((rv3d->viewlock & RV3D_LOCKED) == 0) {
				/* use existing if exists, means multiple calls to smooth view wont loose the original 'view' setting */
				sms.orig_view = rv3d->sms ? rv3d->sms->orig_view : rv3d->view;
				rv3d->view = RV3D_VIEW_USER;
			}

			sms.time_allowed = (double)U.smooth_viewtx / 1000.0;
			
			/* if this is view rotation only
			 * we can decrease the time allowed by
			 * the angle between quats 
			 * this means small rotations wont lag */
			if (quat && !ofs && !dist) {
				float vec1[3] = {0, 0, 1}, vec2[3] = {0, 0, 1};
				float q1[4], q2[4];

				invert_qt_qt(q1, sms.new_quat);
				invert_qt_qt(q2, sms.orig_quat);

				mul_qt_v3(q1, vec1);
				mul_qt_v3(q2, vec2);

				/* scale the time allowed by the rotation */
				sms.time_allowed *= (double)angle_v3v3(vec1, vec2) / M_PI; /* 180deg == 1.0 */
			}

			/* ensure it shows correct */
			if (sms.to_camera) rv3d->persp = RV3D_PERSP;

			rv3d->rflag |= RV3D_NAVIGATING;
			
			/* keep track of running timer! */
			if (rv3d->sms == NULL)
				rv3d->sms = MEM_mallocN(sizeof(struct SmoothViewStore), "smoothview v3d");
			*rv3d->sms = sms;
			if (rv3d->smooth_timer)
				WM_event_remove_timer(wm, win, rv3d->smooth_timer);
			/* TIMER1 is hardcoded in keymap */
			rv3d->smooth_timer = WM_event_add_timer(wm, win, TIMER1, 1.0 / 100.0); /* max 30 frs/sec */
			
			ok = TRUE;
		}
	}
	
	/* if we get here nothing happens */
	if (ok == FALSE) {
		if (sms.to_camera == 0) {
			copy_v3_v3(rv3d->ofs, sms.new_ofs);
			copy_qt_qt(rv3d->viewquat, sms.new_quat);
			rv3d->dist = sms.new_dist;
			v3d->lens = sms.new_lens;
		}

		if (rv3d->viewlock & RV3D_BOXVIEW)
			view3d_boxview_copy(sa, ar);

		ED_region_tag_redraw(ar);
	}
}

/* only meant for timer usage */
static int view3d_smoothview_invoke(bContext *C, wmOperator *UNUSED(op), wmEvent *event)
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	struct SmoothViewStore *sms = rv3d->sms;
	float step, step_inv;
	
	/* escape if not our timer */
	if (rv3d->smooth_timer == NULL || rv3d->smooth_timer != event->customdata)
		return OPERATOR_PASS_THROUGH;
	
	if (sms->time_allowed != 0.0)
		step = (float)((rv3d->smooth_timer->duration) / sms->time_allowed);
	else
		step = 1.0f;
	
	/* end timer */
	if (step >= 1.0f) {
		
		/* if we went to camera, store the original */
		if (sms->to_camera) {
			rv3d->persp = RV3D_CAMOB;
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

			ED_view3d_camera_lock_sync(v3d, rv3d);
		}
		
		if ((rv3d->viewlock & RV3D_LOCKED) == 0) {
			rv3d->view = sms->orig_view;
		}

		MEM_freeN(rv3d->sms);
		rv3d->sms = NULL;
		
		WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), rv3d->smooth_timer);
		rv3d->smooth_timer = NULL;
		rv3d->rflag &= ~RV3D_NAVIGATING;
	}
	else {
		int i;
		
		/* ease in/out */
		if (step < 0.5f) step = (float)pow(step * 2.0f, 2.0) / 2.0f;
		else step = (float)1.0f - (powf(2.0f * (1.0f - step), 2.0f) / 2.0f);

		step_inv = 1.0f - step;

		for (i = 0; i < 3; i++)
			rv3d->ofs[i] = sms->new_ofs[i] * step + sms->orig_ofs[i] * step_inv;

		interp_qt_qtqt(rv3d->viewquat, sms->orig_quat, sms->new_quat, step);
		
		rv3d->dist = sms->new_dist * step + sms->orig_dist * step_inv;
		v3d->lens = sms->new_lens * step + sms->orig_lens * step_inv;

		ED_view3d_camera_lock_sync(v3d, rv3d);
	}
	
	if (rv3d->viewlock & RV3D_BOXVIEW)
		view3d_boxview_copy(CTX_wm_area(C), CTX_wm_region(C));
	
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_smoothview(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Smooth View";
	ot->idname = "VIEW3D_OT_smoothview";
	ot->description = "The time to animate the change of view (in milliseconds)";
	
	/* api callbacks */
	ot->invoke = view3d_smoothview_invoke;
	
	ot->poll = ED_operator_view3d_active;
}

/* ****************** change view operators ****************** */

static int view3d_camera_to_view_exec(bContext *C, wmOperator *UNUSED(op))
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	ObjectTfmProtectedChannels obtfm;

	copy_qt_qt(rv3d->lviewquat, rv3d->viewquat);
	rv3d->lview = rv3d->view;
	if (rv3d->persp != RV3D_CAMOB) {
		rv3d->lpersp = rv3d->persp;
	}

	BKE_object_tfm_protected_backup(v3d->camera, &obtfm);

	ED_view3d_to_object(v3d->camera, rv3d->ofs, rv3d->viewquat, rv3d->dist);

	BKE_object_tfm_protected_restore(v3d->camera, &obtfm, v3d->camera->protectflag);

	DAG_id_tag_update(&v3d->camera->id, OB_RECALC_OB);
	rv3d->persp = RV3D_CAMOB;
	
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, v3d->camera);
	
	return OPERATOR_FINISHED;

}

static int view3d_camera_to_view_poll(bContext *C)
{
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d && v3d->camera && v3d->camera->id.lib == NULL) {
		RegionView3D *rv3d = CTX_wm_region_view3d(C);
		if (rv3d && !rv3d->viewlock) {
			return 1;
		}
	}

	return 0;
}

void VIEW3D_OT_camera_to_view(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Align Camera To View";
	ot->description = "Set camera view to active view";
	ot->idname = "VIEW3D_OT_camera_to_view";
	
	/* api callbacks */
	ot->exec = view3d_camera_to_view_exec;
	ot->poll = view3d_camera_to_view_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* unlike VIEW3D_OT_view_selected this is for framing a render and not
 * meant to take into account vertex/bone selection for eg. */
static int view3d_camera_to_view_selected_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	Object *camera_ob = v3d->camera;

	float r_co[3]; /* the new location to apply */

	/* this function does all the important stuff */
	if (BKE_camera_view_frame_fit_to_scene(scene, v3d, camera_ob, r_co)) {

		ObjectTfmProtectedChannels obtfm;
		float obmat_new[4][4];

		copy_m4_m4(obmat_new, camera_ob->obmat);
		copy_v3_v3(obmat_new[3], r_co);

		/* only touch location */
		BKE_object_tfm_protected_backup(camera_ob, &obtfm);
		BKE_object_apply_mat4(camera_ob, obmat_new, TRUE, TRUE);
		BKE_object_tfm_protected_restore(camera_ob, &obtfm, OB_LOCK_SCALE | OB_LOCK_ROT4D);

		/* notifiers */
		DAG_id_tag_update(&camera_ob->id, OB_RECALC_OB);
		WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, camera_ob);
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static int view3d_camera_to_view_selected_poll(bContext *C)
{
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d && v3d->camera && v3d->camera->id.lib == NULL) {
		RegionView3D *rv3d = CTX_wm_region_view3d(C);
		if (rv3d) {
			if (rv3d->is_persp == FALSE) {
				CTX_wm_operator_poll_msg_set(C, "Only valid for a perspective camera view");
			}
			else if (!rv3d->viewlock) {
				return 1;
			}
		}
	}

	return 0;
}

void VIEW3D_OT_camera_to_view_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Camera Fit Frame to Selected";
	ot->description = "Move the camera so selected objects are framed";
	ot->idname = "VIEW3D_OT_camera_to_view_selected";

	/* api callbacks */
	ot->exec = view3d_camera_to_view_selected_exec;
	ot->poll = view3d_camera_to_view_selected_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static int view3d_setobjectascamera_exec(bContext *C, wmOperator *UNUSED(op))
{	
	View3D *v3d;
	ARegion *ar;
	RegionView3D *rv3d;

	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);

	/* no NULL check is needed, poll checks */
	ED_view3d_context_user_region(C, &v3d, &ar);
	rv3d = ar->regiondata;

	if (ob) {
		Object *camera_old = (rv3d->persp == RV3D_CAMOB) ? V3D_CAMERA_SCENE(scene, v3d) : NULL;
		rv3d->persp = RV3D_CAMOB;
		v3d->camera = ob;
		if (v3d->scenelock)
			scene->camera = ob;

		if (camera_old != ob) /* unlikely but looks like a glitch when set to the same */
			smooth_view(C, v3d, ar, camera_old, v3d->camera, rv3d->ofs, rv3d->viewquat, &rv3d->dist, &v3d->lens);

		WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS | NC_OBJECT | ND_DRAW, CTX_data_scene(C));
	}
	
	return OPERATOR_FINISHED;
}

int ED_operator_rv3d_user_region_poll(bContext *C)
{
	View3D *v3d_dummy;
	ARegion *ar_dummy;

	return ED_view3d_context_user_region(C, &v3d_dummy, &ar_dummy);
}

void VIEW3D_OT_object_as_camera(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Set Active Object as Camera";
	ot->description = "Set the active object as the active camera for this view or scene";
	ot->idname = "VIEW3D_OT_object_as_camera";
	
	/* api callbacks */
	ot->exec = view3d_setobjectascamera_exec;
	ot->poll = ED_operator_rv3d_user_region_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************** */

void ED_view3d_calc_clipping(BoundBox *bb, float planes[4][4], bglMats *mats, const rcti *rect)
{
	float modelview[4][4];
	double xs, ys, p[3];
	int val, flip_sign, a;

	/* near zero floating point values can give issues with gluUnProject
	 * in side view on some implementations */
	if (fabs(mats->modelview[0]) < 1e-6) mats->modelview[0] = 0.0;
	if (fabs(mats->modelview[5]) < 1e-6) mats->modelview[5] = 0.0;

	/* Set up viewport so that gluUnProject will give correct values */
	mats->viewport[0] = 0;
	mats->viewport[1] = 0;

	/* four clipping planes and bounding volume */
	/* first do the bounding volume */
	for (val = 0; val < 4; val++) {
		xs = (val == 0 || val == 3) ? rect->xmin : rect->xmax;
		ys = (val == 0 || val == 1) ? rect->ymin : rect->ymax;

		gluUnProject(xs, ys, 0.0, mats->modelview, mats->projection, mats->viewport, &p[0], &p[1], &p[2]);
		copy_v3fl_v3db(bb->vec[val], p);

		gluUnProject(xs, ys, 1.0, mats->modelview, mats->projection, mats->viewport, &p[0], &p[1], &p[2]);
		copy_v3fl_v3db(bb->vec[4 + val], p);
	}

	/* verify if we have negative scale. doing the transform before cross
	 * product flips the sign of the vector compared to doing cross product
	 * before transform then, so we correct for that. */
	for (a = 0; a < 16; a++)
		((float *)modelview)[a] = mats->modelview[a];
	flip_sign = is_negative_m4(modelview);

	/* then plane equations */
	for (val = 0; val < 4; val++) {

		normal_tri_v3(planes[val], bb->vec[val], bb->vec[val == 3 ? 0 : val + 1], bb->vec[val + 4]);

		if (flip_sign)
			negate_v3(planes[val]);

		planes[val][3] = -planes[val][0] * bb->vec[val][0] -
		                  planes[val][1] * bb->vec[val][1] -
		                  planes[val][2] * bb->vec[val][2];
	}
}

/* create intersection coordinates in view Z direction at mouse coordinates */
void ED_view3d_win_to_segment_clip(ARegion *ar, View3D *v3d, const float mval[2], float ray_start[3], float ray_end[3])
{
	RegionView3D *rv3d = ar->regiondata;
	
	if (rv3d->is_persp) {
		float vec[3];
		ED_view3d_win_to_vector(ar, mval, vec);

		copy_v3_v3(ray_start, rv3d->viewinv[3]);
		madd_v3_v3v3fl(ray_start, rv3d->viewinv[3], vec, v3d->near);
		madd_v3_v3v3fl(ray_end, rv3d->viewinv[3], vec, v3d->far);
	}
	else {
		float vec[4];
		vec[0] = 2.0f * mval[0] / ar->winx - 1;
		vec[1] = 2.0f * mval[1] / ar->winy - 1;
		vec[2] = 0.0f;
		vec[3] = 1.0f;
		
		mul_m4_v4(rv3d->persinv, vec);
		
		madd_v3_v3v3fl(ray_start, vec, rv3d->viewinv[2],  1000.0f);
		madd_v3_v3v3fl(ray_end, vec, rv3d->viewinv[2], -1000.0f);
	}

	/* clipping */
	if (rv3d->rflag & RV3D_CLIPPING) {
		int a;
		for (a = 0; a < 4; a++) {
			clip_line_plane(ray_start, ray_end, rv3d->clip[a]);
		}
	}
}

/* create intersection ray in view Z direction at mouse coordinates */
void ED_view3d_win_to_ray(ARegion *ar, View3D *v3d, const float mval[2], float ray_start[3], float ray_normal[3])
{
	float ray_end[3];
	
	ED_view3d_win_to_segment_clip(ar, v3d, mval, ray_start, ray_end);
	sub_v3_v3v3(ray_normal, ray_end, ray_start);
	normalize_v3(ray_normal);
}

void ED_view3d_global_to_vector(RegionView3D *rv3d, const float coord[3], float vec[3])
{
	if (rv3d->is_persp) {
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
	int flip = FALSE;
	if (rv3d == NULL) return flip;
	rv3d->zfac = rv3d->persmat[0][3] * x + rv3d->persmat[1][3] * y + rv3d->persmat[2][3] * z + rv3d->persmat[3][3];
	if (rv3d->zfac < 0.0f)
		flip = TRUE;
	/* if x,y,z is exactly the viewport offset, zfac is 0 and we don't want that 
	 * (accounting for near zero values)
	 */
	if (rv3d->zfac < 1.e-6f && rv3d->zfac > -1.e-6f) rv3d->zfac = 1.0f;
	
	/* Negative zfac means x, y, z was behind the camera (in perspective).
	 * This gives flipped directions, so revert back to ok default case.
	 */
	/* NOTE: I've changed this to flip zfac to be positive again for now so that GPencil draws ok
	 * Aligorith, 2009Aug31 */
	//if (rv3d->zfac < 0.0f) rv3d->zfac = 1.0f;
	if (rv3d->zfac < 0.0f) rv3d->zfac = -rv3d->zfac;
	
	return flip;
}

void ED_view3d_win_to_3d(ARegion *ar, const float depth_pt[3], const float mval[2], float out[3])
{
	RegionView3D *rv3d = ar->regiondata;
	
	float line_sta[3];
	float line_end[3];

	if (rv3d->is_persp) {
		float mousevec[3];
		copy_v3_v3(line_sta, rv3d->viewinv[3]);
		ED_view3d_win_to_vector(ar, mval, mousevec);
		add_v3_v3v3(line_end, line_sta, mousevec);

		if (isect_line_plane_v3(out, line_sta, line_end, depth_pt, rv3d->viewinv[2], TRUE) == 0) {
			/* highly unlikely to ever happen, mouse vec paralelle with view plane */
			zero_v3(out);
		}
	}
	else {
		const float dx = (2.0f * mval[0] / (float)ar->winx) - 1.0f;
		const float dy = (2.0f * mval[1] / (float)ar->winy) - 1.0f;
		line_sta[0] = (rv3d->persinv[0][0] * dx) + (rv3d->persinv[1][0] * dy) + rv3d->viewinv[3][0];
		line_sta[1] = (rv3d->persinv[0][1] * dx) + (rv3d->persinv[1][1] * dy) + rv3d->viewinv[3][1];
		line_sta[2] = (rv3d->persinv[0][2] * dx) + (rv3d->persinv[1][2] * dy) + rv3d->viewinv[3][2];

		add_v3_v3v3(line_end, line_sta, rv3d->viewinv[2]);
		closest_to_line_v3(out, depth_pt, line_sta, line_end);
	}
}

/* always call initgrabz */
/* only to detect delta motion */
void ED_view3d_win_to_delta(ARegion *ar, const float mval[2], float out[3])
{
	RegionView3D *rv3d = ar->regiondata;
	float dx, dy;
	
	dx = 2.0f * mval[0] * rv3d->zfac / ar->winx;
	dy = 2.0f * mval[1] * rv3d->zfac / ar->winy;
	
	out[0] = (rv3d->persinv[0][0] * dx + rv3d->persinv[1][0] * dy);
	out[1] = (rv3d->persinv[0][1] * dx + rv3d->persinv[1][1] * dy);
	out[2] = (rv3d->persinv[0][2] * dx + rv3d->persinv[1][2] * dy);
}

/* doesn't rely on initgrabz */
/* for perspective view, get the vector direction to
 * the mouse cursor as a normalized vector */
void ED_view3d_win_to_vector(ARegion *ar, const float mval[2], float out[3])
{
	RegionView3D *rv3d = ar->regiondata;

	if (rv3d->is_persp) {
		out[0] = 2.0f * (mval[0] / ar->winx) - 1.0f;
		out[1] = 2.0f * (mval[1] / ar->winy) - 1.0f;
		out[2] = -0.5f;
		mul_project_m4_v3(rv3d->persinv, out);
		sub_v3_v3(out, rv3d->viewinv[3]);
	}
	else {
		copy_v3_v3(out, rv3d->viewinv[2]);
	}
	normalize_v3(out);
}

float ED_view3d_depth_read_cached(ViewContext *vc, int x, int y)
{
	ViewDepths *vd = vc->rv3d->depths;
		
	x -= vc->ar->winrct.xmin;
	y -= vc->ar->winrct.ymin;

	if (vd && vd->depths && x > 0 && y > 0 && x < vd->w && y < vd->h)
		return vd->depths[y * vd->w + x];
	else
		return 1;
}

void ED_view3d_depth_tag_update(RegionView3D *rv3d)
{
	if (rv3d->depths)
		rv3d->depths->damaged = 1;
}

void ED_view3d_ob_project_mat_get(RegionView3D *rv3d, Object *ob, float pmat[4][4])
{
	float vmat[4][4];
	
	mult_m4_m4m4(vmat, rv3d->viewmat, ob->obmat);
	mult_m4_m4m4(pmat, rv3d->winmat, vmat);
}

/* Uses window coordinates (x,y) and depth component z to find a point in
 * modelspace */
void view3d_unproject(bglMats *mats, float out[3], const short x, const short y, const float z)
{
	double ux, uy, uz;

	gluUnProject(x, y, z, mats->modelview, mats->projection,
	             (GLint *)mats->viewport, &ux, &uy, &uz);

	out[0] = ux;
	out[1] = uy;
	out[2] = uz;
}

/* use view3d_get_object_project_mat to get projecting mat */
void ED_view3d_project_float_v2(const ARegion *ar, const float vec[3], float adr[2], float mat[4][4])
{
	float vec4[4];
	
	copy_v3_v3(vec4, vec);
	vec4[3] = 1.0;
	/* adr[0]= IS_CLIPPED; */ /* always overwritten */
	
	mul_m4_v4(mat, vec4);
	
	if (vec4[3] > FLT_EPSILON) {
		adr[0] = (float)(ar->winx / 2.0f) + (ar->winx / 2.0f) * vec4[0] / vec4[3];
		adr[1] = (float)(ar->winy / 2.0f) + (ar->winy / 2.0f) * vec4[1] / vec4[3];
	}
	else {
		adr[0] = adr[1] = 0.0f;
	}
}

/* use view3d_get_object_project_mat to get projecting mat */
void ED_view3d_project_float_v3(ARegion *ar, const float vec[3], float adr[3], float mat[4][4])
{
	float vec4[4];
	
	copy_v3_v3(vec4, vec);
	vec4[3] = 1.0;
	/* adr[0]= IS_CLIPPED; */ /* always overwritten */
	
	mul_m4_v4(mat, vec4);
	
	if (vec4[3] > FLT_EPSILON) {
		adr[0] = (float)(ar->winx / 2.0f) + (ar->winx / 2.0f) * vec4[0] / vec4[3];
		adr[1] = (float)(ar->winy / 2.0f) + (ar->winy / 2.0f) * vec4[1] / vec4[3];
		adr[2] = vec4[2] / vec4[3];
	}
	else {
		zero_v3(adr);
	}
}

int ED_view3d_boundbox_clip(RegionView3D *rv3d, float obmat[][4], BoundBox *bb)
{
	/* return 1: draw */
	
	float mat[4][4];
	float vec[4], min, max;
	int a, flag = -1, fl;
	
	if (bb == NULL) return 1;
	if (bb->flag & OB_BB_DISABLED) return 1;
	
	mult_m4_m4m4(mat, rv3d->persmat, obmat);
	
	for (a = 0; a < 8; a++) {
		copy_v3_v3(vec, bb->vec[a]);
		vec[3] = 1.0;
		mul_m4_v4(mat, vec);
		max = vec[3];
		min = -vec[3];
		
		fl = 0;
		if (vec[0] < min) fl += 1;
		if (vec[0] > max) fl += 2;
		if (vec[1] < min) fl += 4;
		if (vec[1] > max) fl += 8;
		if (vec[2] < min) fl += 16;
		if (vec[2] > max) fl += 32;
		
		flag &= fl;
		if (flag == 0) return 1;
	}
	
	return 0;
}

void project_short(ARegion *ar, const float vec[3], short adr[2])   /* clips */
{
	RegionView3D *rv3d = ar->regiondata;
	float fx, fy, vec4[4];
	
	adr[0] = IS_CLIPPED;
	
	if (rv3d->rflag & RV3D_CLIPPING) {
		if (ED_view3d_clipping_test(rv3d, vec, FALSE)) {
			return;
		}
	}
	
	copy_v3_v3(vec4, vec);
	vec4[3] = 1.0;
	mul_m4_v4(rv3d->persmat, vec4);
	
	if (vec4[3] > (float)BL_NEAR_CLIP) {    /* 0.001 is the NEAR clipping cutoff for picking */
		fx = (ar->winx / 2) * (1 + vec4[0] / vec4[3]);
		
		if (fx > 0 && fx < ar->winx) {
			
			fy = (ar->winy / 2) * (1 + vec4[1] / vec4[3]);
			
			if (fy > 0.0f && fy < (float)ar->winy) {
				adr[0] = (short)floor(fx);
				adr[1] = (short)floor(fy);
			}
		}
	}
}

void project_int(ARegion *ar, const float vec[3], int adr[2])
{
	RegionView3D *rv3d = ar->regiondata;
	float fx, fy, vec4[4];
	
	copy_v3_v3(vec4, vec);
	vec4[3] = 1.0;
	adr[0] = (int)2140000000.0f;
	
	mul_m4_v4(rv3d->persmat, vec4);
	
	if (vec4[3] > (float)BL_NEAR_CLIP) {    /* 0.001 is the NEAR clipping cutoff for picking */
		fx = (ar->winx / 2) * (1 + vec4[0] / vec4[3]);
		
		if (fx > -2140000000.0f && fx < 2140000000.0f) {
			fy = (ar->winy / 2) * (1 + vec4[1] / vec4[3]);
			
			if (fy > -2140000000.0f && fy < 2140000000.0f) {
				adr[0] = (int)floor(fx);
				adr[1] = (int)floor(fy);
			}
		}
	}
}

void project_int_noclip(ARegion *ar, const float vec[3], int adr[2])
{
	RegionView3D *rv3d = ar->regiondata;
	float fx, fy, vec4[4];
	
	copy_v3_v3(vec4, vec);
	vec4[3] = 1.0;
	
	mul_m4_v4(rv3d->persmat, vec4);
	
	if (fabs(vec4[3]) > BL_NEAR_CLIP) {
		fx = (ar->winx / 2) * (1 + vec4[0] / vec4[3]);
		fy = (ar->winy / 2) * (1 + vec4[1] / vec4[3]);
		
		adr[0] = (int)floor(fx); 
		adr[1] = (int)floor(fy);
	}
	else {
		adr[0] = ar->winx / 2;
		adr[1] = ar->winy / 2;
	}
}

void project_short_noclip(ARegion *ar, const float vec[3], short adr[2])
{
	RegionView3D *rv3d = ar->regiondata;
	float fx, fy, vec4[4];
	
	copy_v3_v3(vec4, vec);
	vec4[3] = 1.0;
	adr[0] = IS_CLIPPED;
	
	mul_m4_v4(rv3d->persmat, vec4);
	
	if (vec4[3] > (float)BL_NEAR_CLIP) {    /* 0.001 is the NEAR clipping cutoff for picking */
		fx = (ar->winx / 2) * (1 + vec4[0] / vec4[3]);
		
		if (fx > -32700 && fx < 32700) {
			
			fy = (ar->winy / 2) * (1 + vec4[1] / vec4[3]);
			
			if (fy > -32700.0f && fy < 32700.0f) {
				adr[0] = (short)floor(fx);
				adr[1] = (short)floor(fy);
			}
		}
	}
}

void apply_project_float(float persmat[4][4], int winx, int winy, const float vec[3], float adr[2])
{
	float vec4[4];

	copy_v3_v3(vec4, vec);
	vec4[3] = 1.0;
	adr[0] = IS_CLIPPED;

	mul_m4_v4(persmat, vec4);

	if (vec4[3] > (float)BL_NEAR_CLIP) {
		adr[0] = (float)(winx / 2.0f) + (winx / 2.0f) * vec4[0] / vec4[3];
		adr[1] = (float)(winy / 2.0f) + (winy / 2.0f) * vec4[1] / vec4[3];
	}
}

void project_float(ARegion *ar, const float vec[3], float adr[2])
{
	RegionView3D *rv3d = ar->regiondata;

	apply_project_float(rv3d->persmat, ar->winx, ar->winy, vec, adr);
}

void project_float_noclip(ARegion *ar, const float vec[3], float adr[2])
{
	RegionView3D *rv3d = ar->regiondata;
	float vec4[4];
	
	copy_v3_v3(vec4, vec);
	vec4[3] = 1.0;
	
	mul_m4_v4(rv3d->persmat, vec4);
	
	if (fabs(vec4[3]) > BL_NEAR_CLIP) {
		adr[0] = (float)(ar->winx / 2.0f) + (ar->winx / 2.0f) * vec4[0] / vec4[3];
		adr[1] = (float)(ar->winy / 2.0f) + (ar->winy / 2.0f) * vec4[1] / vec4[3];
	}
	else {
		adr[0] = ar->winx / 2.0f;
		adr[1] = ar->winy / 2.0f;
	}
}

/* copies logic of get_view3d_viewplane(), keep in sync */
int ED_view3d_clip_range_get(View3D *v3d, RegionView3D *rv3d, float *clipsta, float *clipend)
{
	CameraParams params;

	BKE_camera_params_init(&params);
	BKE_camera_params_from_view3d(&params, v3d, rv3d);

	if (clipsta) *clipsta = params.clipsta;
	if (clipend) *clipend = params.clipend;

	return params.is_ortho;
}

/* also exposed in previewrender.c */
int ED_view3d_viewplane_get(View3D *v3d, RegionView3D *rv3d, int winx, int winy,
                            rctf *viewplane, float *clipsta, float *clipend)
{
	CameraParams params;

	BKE_camera_params_init(&params);
	BKE_camera_params_from_view3d(&params, v3d, rv3d);
	BKE_camera_params_compute_viewplane(&params, winx, winy, 1.0f, 1.0f);

	if (viewplane) *viewplane = params.viewplane;
	if (clipsta) *clipsta = params.clipsta;
	if (clipend) *clipend = params.clipend;
	
	return params.is_ortho;
}

void setwinmatrixview3d(ARegion *ar, View3D *v3d, rctf *rect)       /* rect: for picking */
{
	RegionView3D *rv3d = ar->regiondata;
	rctf viewplane;
	float clipsta, clipend, x1, y1, x2, y2;
	int orth;
	
	orth = ED_view3d_viewplane_get(v3d, rv3d, ar->winx, ar->winy, &viewplane, &clipsta, &clipend);
	rv3d->is_persp = !orth;

#if 0
	printf("%s: %d %d %f %f %f %f %f %f\n", __func__, winx, winy,
	       viewplane.xmin, viewplane.ymin, viewplane.xmax, viewplane.ymax,
	       clipsta, clipend);
#endif

	x1 = viewplane.xmin;
	y1 = viewplane.ymin;
	x2 = viewplane.xmax;
	y2 = viewplane.ymax;

	if (rect) {  /* picking */
		rect->xmin /= (float)ar->winx;
		rect->xmin = x1 + rect->xmin * (x2 - x1);
		rect->ymin /= (float)ar->winy;
		rect->ymin = y1 + rect->ymin * (y2 - y1);
		rect->xmax /= (float)ar->winx;
		rect->xmax = x1 + rect->xmax * (x2 - x1);
		rect->ymax /= (float)ar->winy;
		rect->ymax = y1 + rect->ymax * (y2 - y1);
		
		if (orth) wmOrtho(rect->xmin, rect->xmax, rect->ymin, rect->ymax, -clipend, clipend);
		else wmFrustum(rect->xmin, rect->xmax, rect->ymin, rect->ymax, clipsta, clipend);
		
	}
	else {
		if (orth) wmOrtho(x1, x2, y1, y2, clipsta, clipend);
		else wmFrustum(x1, x2, y1, y2, clipsta, clipend);
	}

	/* update matrix in 3d view region */
	glGetFloatv(GL_PROJECTION_MATRIX, (float *)rv3d->winmat);
}

static void obmat_to_viewmat(View3D *v3d, RegionView3D *rv3d, Object *ob, short smooth)
{
	float bmat[4][4];
	float tmat[3][3];
	
	rv3d->view = RV3D_VIEW_USER; /* don't show the grid */
	
	copy_m4_m4(bmat, ob->obmat);
	normalize_m4(bmat);
	invert_m4_m4(rv3d->viewmat, bmat);
	
	/* view quat calculation, needed for add object */
	copy_m3_m4(tmat, rv3d->viewmat);
	if (smooth) {
		float new_quat[4];
		if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
			/* were from a camera view */
			
			float orig_ofs[3];
			float orig_dist = rv3d->dist;
			float orig_lens = v3d->lens;
			copy_v3_v3(orig_ofs, rv3d->ofs);
			
			/* Switch from camera view */
			mat3_to_quat(new_quat, tmat);
			
			rv3d->persp = RV3D_PERSP;
			rv3d->dist = 0.0;
			
			ED_view3d_from_object(v3d->camera, rv3d->ofs, NULL, NULL, &v3d->lens);
			smooth_view(NULL, NULL, NULL, NULL, NULL, orig_ofs, new_quat, &orig_dist, &orig_lens); /* XXX */

			rv3d->persp = RV3D_CAMOB; /* just to be polite, not needed */
			
		}
		else {
			mat3_to_quat(new_quat, tmat);
			smooth_view(NULL, NULL, NULL, NULL, NULL, NULL, new_quat, NULL, NULL); /* XXX */
		}
	}
	else {
		mat3_to_quat(rv3d->viewquat, tmat);
	}
}

#define QUATSET(a, b, c, d, e) { a[0] = b; a[1] = c; a[2] = d; a[3] = e; } (void)0

int ED_view3d_lock(RegionView3D *rv3d)
{
	switch (rv3d->view) {
		case RV3D_VIEW_BOTTOM:
			QUATSET(rv3d->viewquat, 0.0, -1.0, 0.0, 0.0);
			break;

		case RV3D_VIEW_BACK:
			QUATSET(rv3d->viewquat, 0.0, 0.0, -cosf(M_PI / 4.0), -cosf(M_PI / 4.0));
			break;

		case RV3D_VIEW_LEFT:
			QUATSET(rv3d->viewquat, 0.5, -0.5, 0.5, 0.5);
			break;

		case RV3D_VIEW_TOP:
			QUATSET(rv3d->viewquat, 1.0, 0.0, 0.0, 0.0);
			break;

		case RV3D_VIEW_FRONT:
			QUATSET(rv3d->viewquat, (float)cos(M_PI / 4.0), -sinf(M_PI / 4.0), 0.0, 0.0);
			break;

		case RV3D_VIEW_RIGHT:
			QUATSET(rv3d->viewquat, 0.5, -0.5, -0.5, -0.5);
			break;
		default:
			return FALSE;
	}

	return TRUE;
}

/* don't set windows active in here, is used by renderwin too */
void setviewmatrixview3d(Scene *scene, View3D *v3d, RegionView3D *rv3d)
{
	if (rv3d->persp == RV3D_CAMOB) {      /* obs/camera */
		if (v3d->camera) {
			BKE_object_where_is_calc(scene, v3d->camera);	
			obmat_to_viewmat(v3d, rv3d, v3d->camera, 0);
		}
		else {
			quat_to_mat4(rv3d->viewmat, rv3d->viewquat);
			rv3d->viewmat[3][2] -= rv3d->dist;
		}
	}
	else {
		/* should be moved to better initialize later on XXX */
		if (rv3d->viewlock)
			ED_view3d_lock(rv3d);
		
		quat_to_mat4(rv3d->viewmat, rv3d->viewquat);
		if (rv3d->persp == RV3D_PERSP) rv3d->viewmat[3][2] -= rv3d->dist;
		if (v3d->ob_centre) {
			Object *ob = v3d->ob_centre;
			float vec[3];
			
			copy_v3_v3(vec, ob->obmat[3]);
			if (ob->type == OB_ARMATURE && v3d->ob_centre_bone[0]) {
				bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, v3d->ob_centre_bone);
				if (pchan) {
					copy_v3_v3(vec, pchan->pose_mat[3]);
					mul_m4_v3(ob->obmat, vec);
				}
			}
			translate_m4(rv3d->viewmat, -vec[0], -vec[1], -vec[2]);
		}
		else if (v3d->ob_centre_cursor) {
			float vec[3];
			copy_v3_v3(vec, give_cursor(scene, v3d));
			translate_m4(rv3d->viewmat, -vec[0], -vec[1], -vec[2]);
		}
		else translate_m4(rv3d->viewmat, rv3d->ofs[0], rv3d->ofs[1], rv3d->ofs[2]);
	}
}

/* IGLuint-> GLuint */
/* Warning: be sure to account for a negative return value
 *   This is an error, "Too many objects in select buffer"
 *   and no action should be taken (can crash blender) if this happens
 */
short view3d_opengl_select(ViewContext *vc, unsigned int *buffer, unsigned int bufsize, rcti *input)
{
	Scene *scene = vc->scene;
	View3D *v3d = vc->v3d;
	ARegion *ar = vc->ar;
	rctf rect;
	short code, hits;
	char dt, dtx;
	
	G.f |= G_PICKSEL;
	
	/* case not a border select */
	if (input->xmin == input->xmax) {
		rect.xmin = input->xmin - 12;  /* seems to be default value for bones only now */
		rect.xmax = input->xmin + 12;
		rect.ymin = input->ymin - 12;
		rect.ymax = input->ymin + 12;
	}
	else {
		rect.xmin = input->xmin;
		rect.xmax = input->xmax;
		rect.ymin = input->ymin;
		rect.ymax = input->ymax;
	}
	
	setwinmatrixview3d(ar, v3d, &rect);
	mult_m4_m4m4(vc->rv3d->persmat, vc->rv3d->winmat, vc->rv3d->viewmat);
	
	if (v3d->drawtype > OB_WIRE) {
		v3d->zbuf = TRUE;
		glEnable(GL_DEPTH_TEST);
	}
	
	if (vc->rv3d->rflag & RV3D_CLIPPING)
		ED_view3d_clipping_set(vc->rv3d);
	
	glSelectBuffer(bufsize, (GLuint *)buffer);
	glRenderMode(GL_SELECT);
	glInitNames();  /* these two calls whatfor? It doesnt work otherwise */
	glPushName(-1);
	code = 1;
	
	if (vc->obedit && vc->obedit->type == OB_MBALL) {
		draw_object(scene, ar, v3d, BASACT, DRAW_PICKING | DRAW_CONSTCOLOR);
	}
	else if ((vc->obedit && vc->obedit->type == OB_ARMATURE)) {
		/* if not drawing sketch, draw bones */
		if (!BDR_drawSketchNames(vc)) {
			draw_object(scene, ar, v3d, BASACT, DRAW_PICKING | DRAW_CONSTCOLOR);
		}
	}
	else {
		Base *base;
		
		v3d->xray = TRUE;  /* otherwise it postpones drawing */
		for (base = scene->base.first; base; base = base->next) {
			if (base->lay & v3d->lay) {
				
				if (base->object->restrictflag & OB_RESTRICT_SELECT)
					base->selcol = 0;
				else {
					base->selcol = code;
					glLoadName(code);
					draw_object(scene, ar, v3d, base, DRAW_PICKING | DRAW_CONSTCOLOR);
					
					/* we draw group-duplicators for selection too */
					if ((base->object->transflag & OB_DUPLI) && base->object->dup_group) {
						ListBase *lb;
						DupliObject *dob;
						Base tbase;
						
						tbase.flag = OB_FROMDUPLI;
						lb = object_duplilist(scene, base->object);
						
						for (dob = lb->first; dob; dob = dob->next) {
							tbase.object = dob->ob;
							copy_m4_m4(dob->ob->obmat, dob->mat);
							
							/* extra service: draw the duplicator in drawtype of parent */
							/* MIN2 for the drawtype to allow bounding box objects in groups for lods */
							dt = tbase.object->dt;   tbase.object->dt = MIN2(tbase.object->dt, base->object->dt);
							dtx = tbase.object->dtx; tbase.object->dtx = base->object->dtx;

							draw_object(scene, ar, v3d, &tbase, DRAW_PICKING | DRAW_CONSTCOLOR);
							
							tbase.object->dt = dt;
							tbase.object->dtx = dtx;

							copy_m4_m4(dob->ob->obmat, dob->omat);
						}
						free_object_duplilist(lb);
					}
					code++;
				}				
			}
		}
		v3d->xray = FALSE;  /* restore */
	}
	
	glPopName();    /* see above (pushname) */
	hits = glRenderMode(GL_RENDER);
	
	G.f &= ~G_PICKSEL;
	setwinmatrixview3d(ar, v3d, NULL);
	mult_m4_m4m4(vc->rv3d->persmat, vc->rv3d->winmat, vc->rv3d->viewmat);
	
	if (v3d->drawtype > OB_WIRE) {
		v3d->zbuf = 0;
		glDisable(GL_DEPTH_TEST);
	}
// XXX	persp(PERSP_WIN);
	
	if (vc->rv3d->rflag & RV3D_CLIPPING)
		ED_view3d_clipping_disable();
	
	if (hits < 0) printf("Too many objects in select buffer\n");  /* XXX make error message */

	return hits;
}

/* ********************** local view operator ******************** */

static unsigned int free_localbit(Main *bmain)
{
	unsigned int lay;
	ScrArea *sa;
	bScreen *sc;
	
	lay = 0;
	
	/* sometimes we loose a localview: when an area is closed */
	/* check all areas: which localviews are in use? */
	for (sc = bmain->screen.first; sc; sc = sc->id.next) {
		for (sa = sc->areabase.first; sa; sa = sa->next) {
			SpaceLink *sl = sa->spacedata.first;
			for (; sl; sl = sl->next) {
				if (sl->spacetype == SPACE_VIEW3D) {
					View3D *v3d = (View3D *) sl;
					lay |= v3d->lay;
				}
			}
		}
	}
	
	if ((lay & 0x01000000) == 0) return 0x01000000;
	if ((lay & 0x02000000) == 0) return 0x02000000;
	if ((lay & 0x04000000) == 0) return 0x04000000;
	if ((lay & 0x08000000) == 0) return 0x08000000;
	if ((lay & 0x10000000) == 0) return 0x10000000;
	if ((lay & 0x20000000) == 0) return 0x20000000;
	if ((lay & 0x40000000) == 0) return 0x40000000;
	if ((lay & 0x80000000) == 0) return 0x80000000;
	
	return 0;
}

int ED_view3d_scene_layer_set(int lay, const int *values, int *active)
{
	int i, tot = 0;
	
	/* ensure we always have some layer selected */
	for (i = 0; i < 20; i++)
		if (values[i])
			tot++;
	
	if (tot == 0)
		return lay;
	
	for (i = 0; i < 20; i++) {
		
		if (active) {
			/* if this value has just been switched on, make that layer active */
			if (values[i] && (lay & (1 << i)) == 0) {
				*active = (1 << i);
			}
		}
			
		if (values[i]) lay |= (1 << i);
		else lay &= ~(1 << i);
	}
	
	/* ensure always an active layer */
	if (active && (lay & *active) == 0) {
		for (i = 0; i < 20; i++) {
			if (lay & (1 << i)) {
				*active = 1 << i;
				break;
			}
		}
	}
	
	return lay;
}

static int view3d_localview_init(Main *bmain, Scene *scene, ScrArea *sa, ReportList *reports)
{
	View3D *v3d = sa->spacedata.first;
	Base *base;
	float size = 0.0, min[3], max[3], box[3];
	unsigned int locallay;
	int ok = FALSE;

	if (v3d->localvd) {
		return ok;
	}

	INIT_MINMAX(min, max);

	locallay = free_localbit(bmain);

	if (locallay == 0) {
		BKE_reportf(reports, RPT_ERROR, "No more than 8 localviews");
		ok = FALSE;
	}
	else {
		if (scene->obedit) {
			BKE_object_minmax(scene->obedit, min, max);
			
			ok = TRUE;
		
			BASACT->lay |= locallay;
			scene->obedit->lay = BASACT->lay;
		}
		else {
			for (base = FIRSTBASE; base; base = base->next) {
				if (TESTBASE(v3d, base)) {
					BKE_object_minmax(base->object, min, max);
					base->lay |= locallay;
					base->object->lay = base->lay;
					ok = TRUE;
				}
			}
		}
		
		box[0] = (max[0] - min[0]);
		box[1] = (max[1] - min[1]);
		box[2] = (max[2] - min[2]);
		size = MAX3(box[0], box[1], box[2]);
		if (size <= 0.01f) size = 0.01f;
	}
	
	if (ok == TRUE) {
		ARegion *ar;
		
		v3d->localvd = MEM_mallocN(sizeof(View3D), "localview");
		
		memcpy(v3d->localvd, v3d, sizeof(View3D));

		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->regiontype == RGN_TYPE_WINDOW) {
				RegionView3D *rv3d = ar->regiondata;

				rv3d->localvd = MEM_mallocN(sizeof(RegionView3D), "localview region");
				memcpy(rv3d->localvd, rv3d, sizeof(RegionView3D));
				
				rv3d->ofs[0] = -(min[0] + max[0]) / 2.0f;
				rv3d->ofs[1] = -(min[1] + max[1]) / 2.0f;
				rv3d->ofs[2] = -(min[2] + max[2]) / 2.0f;

				rv3d->dist = size;
				/* perspective should be a bit farther away to look nice */
				if (rv3d->persp == RV3D_ORTHO)
					rv3d->dist *= 0.7f;

				/* correction for window aspect ratio */
				if (ar->winy > 2 && ar->winx > 2) {
					float asp = (float)ar->winx / (float)ar->winy;
					if (asp < 1.0f) asp = 1.0f / asp;
					rv3d->dist *= asp;
				}
				
				if (rv3d->persp == RV3D_CAMOB) rv3d->persp = RV3D_PERSP;
				
				v3d->cursor[0] = -rv3d->ofs[0];
				v3d->cursor[1] = -rv3d->ofs[1];
				v3d->cursor[2] = -rv3d->ofs[2];
			}
		}
		
		v3d->lay = locallay;
	}
	else {
		/* clear flags */ 
		for (base = FIRSTBASE; base; base = base->next) {
			if (base->lay & locallay) {
				base->lay -= locallay;
				if (base->lay == 0) base->lay = v3d->layact;
				if (base->object != scene->obedit) base->flag |= SELECT;
				base->object->lay = base->lay;
			}
		}
	}

	return ok;
}

static void restore_localviewdata(ScrArea *sa, int free)
{
	ARegion *ar;
	View3D *v3d = sa->spacedata.first;
	
	if (v3d->localvd == NULL) return;
	
	v3d->near = v3d->localvd->near;
	v3d->far = v3d->localvd->far;
	v3d->lay = v3d->localvd->lay;
	v3d->layact = v3d->localvd->layact;
	v3d->drawtype = v3d->localvd->drawtype;
	v3d->camera = v3d->localvd->camera;
	
	if (free) {
		MEM_freeN(v3d->localvd);
		v3d->localvd = NULL;
	}
	
	for (ar = sa->regionbase.first; ar; ar = ar->next) {
		if (ar->regiontype == RGN_TYPE_WINDOW) {
			RegionView3D *rv3d = ar->regiondata;
			
			if (rv3d->localvd) {
				rv3d->dist = rv3d->localvd->dist;
				copy_v3_v3(rv3d->ofs, rv3d->localvd->ofs);
				copy_qt_qt(rv3d->viewquat, rv3d->localvd->viewquat);
				rv3d->view = rv3d->localvd->view;
				rv3d->persp = rv3d->localvd->persp;
				rv3d->camzoom = rv3d->localvd->camzoom;

				if (free) {
					MEM_freeN(rv3d->localvd);
					rv3d->localvd = NULL;
				}
			}
		}
	}
}

static int view3d_localview_exit(Main *bmain, Scene *scene, ScrArea *sa)
{
	View3D *v3d = sa->spacedata.first;
	struct Base *base;
	unsigned int locallay;
	
	if (v3d->localvd) {
		
		locallay = v3d->lay & 0xFF000000;
		
		restore_localviewdata(sa, 1); /* 1 = free */

		/* for when in other window the layers have changed */
		if (v3d->scenelock) v3d->lay = scene->lay;
		
		for (base = FIRSTBASE; base; base = base->next) {
			if (base->lay & locallay) {
				base->lay -= locallay;
				if (base->lay == 0) base->lay = v3d->layact;
				if (base->object != scene->obedit) {
					base->flag |= SELECT;
					base->object->flag |= SELECT;
				}
				base->object->lay = base->lay;
			}
		}
		
		DAG_on_visible_update(bmain, FALSE);

		return TRUE;
	} 
	else {
		return FALSE;
	}
}

static int localview_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = CTX_wm_view3d(C);
	int change;
	
	if (v3d->localvd) {
		change = view3d_localview_exit(bmain, scene, sa);
	}
	else {
		change = view3d_localview_init(bmain, scene, sa, op->reports);
	}

	if (change) {
		DAG_id_type_tag(bmain, ID_OB);
		ED_area_tag_redraw(CTX_wm_area(C));

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void VIEW3D_OT_localview(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Local View";
	ot->description = "Toggle display of selected object(s) separately and centered in view";
	ot->idname = "VIEW3D_OT_localview";
	
	/* api callbacks */
	ot->exec = localview_exec;
	ot->flag = OPTYPE_UNDO; /* localview changes object layer bitflags */
	
	ot->poll = ED_operator_view3d_active;
}

#ifdef WITH_GAMEENGINE

static ListBase queue_back;
static void SaveState(bContext *C, wmWindow *win)
{
	Object *obact = CTX_data_active_object(C);
	
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	if (obact && obact->mode & OB_MODE_TEXTURE_PAINT)
		GPU_paint_set_mipmap(1);
	
	queue_back = win->queue;
	
	win->queue.first = win->queue.last = NULL;
	
	//XXX waitcursor(1);
}

static void RestoreState(bContext *C, wmWindow *win)
{
	Object *obact = CTX_data_active_object(C);
	
	if (obact && obact->mode & OB_MODE_TEXTURE_PAINT)
		GPU_paint_set_mipmap(0);

	//XXX curarea->win_swap = 0;
	//XXX curarea->head_swap=0;
	//XXX allqueue(REDRAWVIEW3D, 1);
	//XXX allqueue(REDRAWBUTSALL, 0);
	//XXX reset_slowparents();
	//XXX waitcursor(0);
	//XXX G.qual= 0;
	
	if (win) /* check because closing win can set to NULL */
		win->queue = queue_back;
	
	GPU_state_init();
	GPU_set_tpage(NULL, 0, 0);

	glPopAttrib();
}

/* was space_set_commmandline_options in 2.4x */
static void game_set_commmandline_options(GameData *gm)
{
	SYS_SystemHandle syshandle;
	int test;

	if ( (syshandle = SYS_GetSystem()) ) {
		/* User defined settings */
		test = (U.gameflags & USER_DISABLE_MIPMAP);
		GPU_set_mipmap(!test);
		SYS_WriteCommandLineInt(syshandle, "nomipmap", test);

		/* File specific settings: */
		/* Only test the first one. These two are switched
		 * simultaneously. */
		test = (gm->flag & GAME_SHOW_FRAMERATE);
		SYS_WriteCommandLineInt(syshandle, "show_framerate", test);
		SYS_WriteCommandLineInt(syshandle, "show_profile", test);

		test = (gm->flag & GAME_SHOW_DEBUG_PROPS);
		SYS_WriteCommandLineInt(syshandle, "show_properties", test);

		test = (gm->flag & GAME_SHOW_PHYSICS);
		SYS_WriteCommandLineInt(syshandle, "show_physics", test);

		test = (gm->flag & GAME_ENABLE_ALL_FRAMES);
		SYS_WriteCommandLineInt(syshandle, "fixedtime", test);

		test = (gm->flag & GAME_ENABLE_ANIMATION_RECORD);
		SYS_WriteCommandLineInt(syshandle, "animation_record", test);

		test = (gm->flag & GAME_IGNORE_DEPRECATION_WARNINGS);
		SYS_WriteCommandLineInt(syshandle, "ignore_deprecation_warnings", test);

		test = (gm->matmode == GAME_MAT_MULTITEX);
		SYS_WriteCommandLineInt(syshandle, "blender_material", test);
		test = (gm->matmode == GAME_MAT_GLSL);
		SYS_WriteCommandLineInt(syshandle, "blender_glsl_material", test);
		test = (gm->flag & GAME_DISPLAY_LISTS);
		SYS_WriteCommandLineInt(syshandle, "displaylists", test);


	}
}

#endif /* WITH_GAMEENGINE */

static int game_engine_poll(bContext *C)
{
	/* we need a context and area to launch BGE
	 * it's a temporary solution to avoid crash at load time
	 * if we try to auto run the BGE. Ideally we want the
	 * context to be set as soon as we load the file. */

	if (CTX_wm_window(C) == NULL) return 0;
	if (CTX_wm_screen(C) == NULL) return 0;
	if (CTX_wm_area(C) == NULL) return 0;

	if (CTX_data_mode_enum(C) != CTX_MODE_OBJECT)
		return 0;

	return 1;
}

int ED_view3d_context_activate(bContext *C)
{
	bScreen *sc = CTX_wm_screen(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar;

	/* sa can be NULL when called from python */
	if (sa == NULL || sa->spacetype != SPACE_VIEW3D)
		for (sa = sc->areabase.first; sa; sa = sa->next)
			if (sa->spacetype == SPACE_VIEW3D)
				break;

	if (!sa)
		return 0;
	
	for (ar = sa->regionbase.first; ar; ar = ar->next)
		if (ar->regiontype == RGN_TYPE_WINDOW)
			break;
	
	if (!ar)
		return 0;
	
	/* bad context switch .. */
	CTX_wm_area_set(C, sa);
	CTX_wm_region_set(C, ar);

	return 1;
}

static int game_engine_exec(bContext *C, wmOperator *op)
{
#ifdef WITH_GAMEENGINE
	Scene *startscene = CTX_data_scene(C);
	ScrArea /* *sa, */ /* UNUSED */ *prevsa = CTX_wm_area(C);
	ARegion *ar, *prevar = CTX_wm_region(C);
	wmWindow *prevwin = CTX_wm_window(C);
	RegionView3D *rv3d;
	rcti cam_frame;

	(void)op; /* unused */
	
	/* bad context switch .. */
	if (!ED_view3d_context_activate(C))
		return OPERATOR_CANCELLED;
	
	/* redraw to hide any menus/popups, we don't go back to
	 * the window manager until after this operator exits */
	WM_redraw_windows(C);

	rv3d = CTX_wm_region_view3d(C);
	/* sa= CTX_wm_area(C); */ /* UNUSED */
	ar = CTX_wm_region(C);

	view3d_operator_needs_opengl(C);
	
	game_set_commmandline_options(&startscene->gm);

	if ((rv3d->persp == RV3D_CAMOB) &&
	    (startscene->gm.framing.type == SCE_GAMEFRAMING_BARS) &&
	    (startscene->gm.stereoflag != STEREO_DOME))
	{
		/* Letterbox */
		rctf cam_framef;
		ED_view3d_calc_camera_border(startscene, ar, CTX_wm_view3d(C), rv3d, &cam_framef, FALSE);
		cam_frame.xmin = cam_framef.xmin + ar->winrct.xmin;
		cam_frame.xmax = cam_framef.xmax + ar->winrct.xmin;
		cam_frame.ymin = cam_framef.ymin + ar->winrct.ymin;
		cam_frame.ymax = cam_framef.ymax + ar->winrct.ymin;
		BLI_rcti_isect(&ar->winrct, &cam_frame, &cam_frame);
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
	if (BLI_findindex(&CTX_wm_manager(C)->windows, prevwin) == -1) {
		prevwin = NULL;
		CTX_wm_window_set(C, NULL);
	}
	
	ED_area_tag_redraw(CTX_wm_area(C));

	if (prevwin) {
		/* restore context, in case it changed in the meantime, for
		 * example by working in another window or closing it */
		CTX_wm_region_set(C, prevar);
		CTX_wm_window_set(C, prevwin);
		CTX_wm_area_set(C, prevsa);
	}

	RestoreState(C, prevwin);

	//XXX restore_all_scene_cfra(scene_cfra_store);
	BKE_scene_set_background(CTX_data_main(C), startscene);
	//XXX BKE_scene_update_for_newframe(bmain, scene, scene->lay);

	return OPERATOR_FINISHED;
#else
	(void)C; /* unused */
	BKE_report(op->reports, RPT_ERROR, "Game engine is disabled in this build");
	return OPERATOR_CANCELLED;
#endif
}

void VIEW3D_OT_game_start(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Start Game Engine";
	ot->description = "Start game engine";
	ot->idname = "VIEW3D_OT_game_start";
	
	/* api callbacks */
	ot->exec = game_engine_exec;
	
	ot->poll = game_engine_poll;
}

/* ************************************** */

static void UNUSED_FUNCTION(view3d_align_axis_to_vector)(View3D *v3d, RegionView3D *rv3d, int axisidx, float vec[3])
{
	float alignaxis[3] = {0.0, 0.0, 0.0};
	float norm[3], axis[3], angle, new_quat[4];
	
	if (axisidx > 0) alignaxis[axisidx - 1] = 1.0;
	else alignaxis[-axisidx - 1] = -1.0;

	normalize_v3_v3(norm, vec);

	angle = (float)acos(dot_v3v3(alignaxis, norm));
	cross_v3_v3v3(axis, alignaxis, norm);
	axis_angle_to_quat(new_quat, axis, -angle);
	
	rv3d->view = RV3D_VIEW_USER;
	
	if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
		/* switch out of camera view */
		float orig_ofs[3];
		float orig_dist = rv3d->dist;
		float orig_lens = v3d->lens;
		
		copy_v3_v3(orig_ofs, rv3d->ofs);
		rv3d->persp = RV3D_PERSP;
		rv3d->dist = 0.0;
		ED_view3d_from_object(v3d->camera, rv3d->ofs, NULL, NULL, &v3d->lens);
		smooth_view(NULL, NULL, NULL, NULL, NULL, orig_ofs, new_quat, &orig_dist, &orig_lens); /* XXX */
	}
	else {
		if (rv3d->persp == RV3D_CAMOB) rv3d->persp = RV3D_PERSP;  /* switch out of camera mode */
		smooth_view(NULL, NULL, NULL, NULL, NULL, NULL, new_quat, NULL, NULL); /* XXX */
	}
}

float ED_view3d_pixel_size(struct RegionView3D *rv3d, const float co[3])
{
	return (rv3d->persmat[3][3] + (
	            rv3d->persmat[0][3] * co[0] +
	            rv3d->persmat[1][3] * co[1] +
	            rv3d->persmat[2][3] * co[2])
	        ) * rv3d->pixsize;
}

/* view matrix properties utilities */

/* unused */
#if 0
void ED_view3d_operator_properties_viewmat(wmOperatorType *ot)
{
	PropertyRNA *prop;

	prop = RNA_def_int(ot->srna, "region_width", 0, 0, INT_MAX, "Region Width", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_HIDDEN);

	prop = RNA_def_int(ot->srna, "region_height", 0, 0, INT_MAX, "Region height", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_HIDDEN);

	prop = RNA_def_float_matrix(ot->srna, "perspective_matrix", 4, 4, NULL, 0.0f, 0.0f, "", "Perspective Matrix", 0.0f, 0.0f);
	RNA_def_property_flag(prop, PROP_HIDDEN);
}

void ED_view3d_operator_properties_viewmat_set(bContext *C, wmOperator *op)
{
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);

	if (!RNA_struct_property_is_set(op->ptr, "region_width"))
		RNA_int_set(op->ptr, "region_width", ar->winx);

	if (!RNA_struct_property_is_set(op->ptr, "region_height"))
		RNA_int_set(op->ptr, "region_height", ar->winy);

	if (!RNA_struct_property_is_set(op->ptr, "perspective_matrix"))
		RNA_float_set_array(op->ptr, "perspective_matrix", (float *)rv3d->persmat);
}

void ED_view3d_operator_properties_viewmat_get(wmOperator *op, int *winx, int *winy, float persmat[4][4])
{
	*winx = RNA_int_get(op->ptr, "region_width");
	*winy = RNA_int_get(op->ptr, "region_height");

	RNA_float_get_array(op->ptr, "perspective_matrix", (float *)persmat);
}
#endif
