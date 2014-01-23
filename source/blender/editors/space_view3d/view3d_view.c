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
#include "BLI_callbacks.h"

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

#include "RE_engine.h"

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
	if ((ar == NULL) || (ar->regiontype != RGN_TYPE_WINDOW)) {
		printf("view3d_region_operator_needs_opengl error, wrong region\n");
	}
	else {
		RegionView3D *rv3d = ar->regiondata;
		
		wmSubWindowSet(win, ar->swinid);
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(rv3d->winmat);
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(rv3d->viewmat);
	}
}

float *ED_view3d_cursor3d_get(Scene *scene, View3D *v3d)
{
	if (v3d && v3d->localvd) return v3d->cursor;
	else return scene->cursor;
}

Camera *ED_view3d_camera_data_get(View3D *v3d, RegionView3D *rv3d)
{
	/* establish the camera object, so we can default to view mapping if anything is wrong with it */
	if ((rv3d->persp == RV3D_CAMOB) && v3d->camera && (v3d->camera->type == OB_CAMERA)) {
		return v3d->camera->data;
	}
	else {
		return NULL;
	}
}

/* ****************** smooth view operator ****************** */
/* This operator is one of the 'timer refresh' ones like animation playback */

struct SmoothView3DState {
	float dist;
	float lens;
	float quat[4];
	float ofs[3];
};

struct SmoothView3DStore {
	/* source*/
	struct SmoothView3DState src;  /* source */
	struct SmoothView3DState dst;  /* destination */
	struct SmoothView3DState org;  /* original */

	bool to_camera;
	char org_view;

	double time_allowed;
};

static void view3d_smooth_view_state_backup(struct SmoothView3DState *sms_state,
                                            const View3D *v3d, const RegionView3D *rv3d)
{
	copy_v3_v3(sms_state->ofs,   rv3d->ofs);
	copy_qt_qt(sms_state->quat,  rv3d->viewquat);
	sms_state->dist            = rv3d->dist;
	sms_state->lens            = v3d->lens;
}

static void view3d_smooth_view_state_restore(const struct SmoothView3DState *sms_state,
                                             View3D *v3d, RegionView3D *rv3d)
{
	copy_v3_v3(rv3d->ofs,      sms_state->ofs);
	copy_qt_qt(rv3d->viewquat, sms_state->quat);
	rv3d->dist               = sms_state->dist;
	v3d->lens                = sms_state->lens;
}

/* will start timer if appropriate */
/* the arguments are the desired situation */
void ED_view3d_smooth_view(bContext *C, View3D *v3d, ARegion *ar, Object *oldcamera, Object *camera,
                           float *ofs, float *quat, float *dist, float *lens,
                           const int smooth_viewtx)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = CTX_wm_area(C);

	RegionView3D *rv3d = ar->regiondata;
	struct SmoothView3DStore sms = {{0}};
	bool ok = false;
	
	/* initialize sms */
	view3d_smooth_view_state_backup(&sms.dst, v3d, rv3d);
	view3d_smooth_view_state_backup(&sms.src, v3d, rv3d);
	/* if smoothview runs multiple times... */
	if (rv3d->sms == NULL) {
		view3d_smooth_view_state_backup(&sms.org, v3d, rv3d);
		sms.org_view = rv3d->view;
	}
	else {
		sms.org = rv3d->sms->org;
		sms.org_view = rv3d->sms->org_view;
	}
	/* sms.to_camera = false; */  /* initizlized to zero anyway */

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
	if (ofs)  copy_v3_v3(sms.dst.ofs, ofs);
	if (quat) copy_qt_qt(sms.dst.quat, quat);
	if (dist) sms.dst.dist = *dist;
	if (lens) sms.dst.lens = *lens;

	if (camera) {
		sms.dst.dist = ED_view3d_offset_distance(camera->obmat, ofs, VIEW3D_DIST_FALLBACK);
		ED_view3d_from_object(camera, sms.dst.ofs, sms.dst.quat, &sms.dst.dist, &sms.dst.lens);
		sms.to_camera = true; /* restore view3d values in end */
	}
	
	/* skip smooth viewing for render engine draw */
	if (smooth_viewtx && v3d->drawtype != OB_RENDER) {
		bool changed = false; /* zero means no difference */
		
		if (oldcamera != camera)
			changed = true;
		else if (sms.dst.dist != rv3d->dist)
			changed = true;
		else if (sms.dst.lens != v3d->lens)
			changed = true;
		else if (!equals_v3v3(sms.dst.ofs, rv3d->ofs))
			changed = true;
		else if (!equals_v4v4(sms.dst.quat, rv3d->viewquat))
			changed = true;
		
		/* The new view is different from the old one
		 * so animate the view */
		if (changed) {
			/* original values */
			if (oldcamera) {
				sms.src.dist = ED_view3d_offset_distance(oldcamera->obmat, rv3d->ofs, 0.0f);
				/* this */
				ED_view3d_from_object(oldcamera, sms.src.ofs, sms.src.quat, &sms.src.dist, &sms.src.lens);
			}
			/* grid draw as floor */
			if ((rv3d->viewlock & RV3D_LOCKED) == 0) {
				/* use existing if exists, means multiple calls to smooth view wont loose the original 'view' setting */
				rv3d->view = RV3D_VIEW_USER;
			}

			sms.time_allowed = (double)smooth_viewtx / 1000.0;
			
			/* if this is view rotation only
			 * we can decrease the time allowed by
			 * the angle between quats 
			 * this means small rotations wont lag */
			if (quat && !ofs && !dist) {
				float vec1[3] = {0, 0, 1}, vec2[3] = {0, 0, 1};
				float q1[4], q2[4];

				invert_qt_qt(q1, sms.dst.quat);
				invert_qt_qt(q2, sms.src.quat);

				mul_qt_v3(q1, vec1);
				mul_qt_v3(q2, vec2);

				/* scale the time allowed by the rotation */
				sms.time_allowed *= (double)angle_v3v3(vec1, vec2) / M_PI; /* 180deg == 1.0 */
			}

			/* ensure it shows correct */
			if (sms.to_camera) {
				rv3d->persp = RV3D_PERSP;
			}

			rv3d->rflag |= RV3D_NAVIGATING;
			
			/* not essential but in some cases the caller will tag the area for redraw,
			 * and in that case we can get a ficker of the 'org' user view but we want to see 'src' */
			view3d_smooth_view_state_restore(&sms.src, v3d, rv3d);

			/* keep track of running timer! */
			if (rv3d->sms == NULL) {
				rv3d->sms = MEM_mallocN(sizeof(struct SmoothView3DStore), "smoothview v3d");
			}
			*rv3d->sms = sms;
			if (rv3d->smooth_timer) {
				WM_event_remove_timer(wm, win, rv3d->smooth_timer);
			}
			/* TIMER1 is hardcoded in keymap */
			rv3d->smooth_timer = WM_event_add_timer(wm, win, TIMER1, 1.0 / 100.0); /* max 30 frs/sec */

			ok = true;
		}
	}
	
	/* if we get here nothing happens */
	if (ok == false) {
		if (sms.to_camera == false) {
			copy_v3_v3(rv3d->ofs, sms.dst.ofs);
			copy_qt_qt(rv3d->viewquat, sms.dst.quat);
			rv3d->dist = sms.dst.dist;
			v3d->lens = sms.dst.lens;

			ED_view3d_camera_lock_sync(v3d, rv3d);
		}

		if (rv3d->viewlock & RV3D_BOXVIEW) {
			view3d_boxview_copy(sa, ar);
		}

		ED_region_tag_redraw(ar);
	}
}

/* only meant for timer usage */
static int view3d_smoothview_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	struct SmoothView3DStore *sms = rv3d->sms;
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
			view3d_smooth_view_state_restore(&sms->org, v3d, rv3d);
		}
		else {
			view3d_smooth_view_state_restore(&sms->dst, v3d, rv3d);

			ED_view3d_camera_lock_sync(v3d, rv3d);
		}
		
		if ((rv3d->viewlock & RV3D_LOCKED) == 0) {
			rv3d->view = sms->org_view;
		}

		MEM_freeN(rv3d->sms);
		rv3d->sms = NULL;
		
		WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), rv3d->smooth_timer);
		rv3d->smooth_timer = NULL;
		rv3d->rflag &= ~RV3D_NAVIGATING;
	}
	else {
		/* ease in/out */
		step = (3.0f * step * step - 2.0f * step * step * step);

		step_inv = 1.0f - step;

		interp_v3_v3v3(rv3d->ofs,      sms->src.ofs,  sms->dst.ofs,  step);
		interp_qt_qtqt(rv3d->viewquat, sms->src.quat, sms->dst.quat, step);
		
		rv3d->dist = sms->dst.dist * step + sms->src.dist * step_inv;
		v3d->lens  = sms->dst.lens * step + sms->src.lens * step_inv;

		ED_view3d_camera_lock_sync(v3d, rv3d);
	}
	
	if (rv3d->viewlock & RV3D_BOXVIEW)
		view3d_boxview_copy(CTX_wm_area(C), CTX_wm_region(C));

	/* note: this doesn't work right because the v3d->lens is now used in ortho mode r51636,
	 * when switching camera in quad-view the other ortho views would zoom & reset.
	 *
	 * For now only redraw all regions when smoothview finishes.
	 */
	if (step >= 1.0f) {
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
	}
	else {
		ED_region_tag_redraw(CTX_wm_region(C));
	}
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_smoothview(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Smooth View";
	ot->description = "";
	ot->idname = "VIEW3D_OT_smoothview";
	
	/* api callbacks */
	ot->invoke = view3d_smoothview_invoke;
	
	/* flags */
	ot->flag = OPTYPE_INTERNAL;

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
static int view3d_camera_to_view_selected_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);  /* can be NULL */
	Object *camera_ob = v3d ? v3d->camera : scene->camera;

	float r_co[3]; /* the new location to apply */

	if (camera_ob == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active camera");
		return OPERATOR_CANCELLED;
	}
	else if (camera_ob->type != OB_CAMERA) {
		BKE_report(op->reports, RPT_ERROR, "Object not a camera");
		return OPERATOR_CANCELLED;
	}
	else if (((Camera *)camera_ob->data)->type == R_ORTHO) {
		BKE_report(op->reports, RPT_ERROR, "Orthographic cameras not supported");
		return OPERATOR_CANCELLED;
	}

	/* this function does all the important stuff */
	if (BKE_camera_view_frame_fit_to_scene(scene, v3d, camera_ob, r_co)) {

		ObjectTfmProtectedChannels obtfm;
		float obmat_new[4][4];

		copy_m4_m4(obmat_new, camera_ob->obmat);
		copy_v3_v3(obmat_new[3], r_co);

		/* only touch location */
		BKE_object_tfm_protected_backup(camera_ob, &obtfm);
		BKE_object_apply_mat4(camera_ob, obmat_new, true, true);
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

void VIEW3D_OT_camera_to_view_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Camera Fit Frame to Selected";
	ot->description = "Move the camera so selected objects are framed";
	ot->idname = "VIEW3D_OT_camera_to_view_selected";

	/* api callbacks */
	ot->exec = view3d_camera_to_view_selected_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static int view3d_setobjectascamera_exec(bContext *C, wmOperator *op)
{	
	View3D *v3d;
	ARegion *ar;
	RegionView3D *rv3d;

	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);

	const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

	/* no NULL check is needed, poll checks */
	ED_view3d_context_user_region(C, &v3d, &ar);
	rv3d = ar->regiondata;

	if (ob) {
		Object *camera_old = (rv3d->persp == RV3D_CAMOB) ? V3D_CAMERA_SCENE(scene, v3d) : NULL;
		rv3d->persp = RV3D_CAMOB;
		v3d->camera = ob;
		if (v3d->scenelock)
			scene->camera = ob;

		if (camera_old != ob) {  /* unlikely but looks like a glitch when set to the same */
			ED_view3d_smooth_view(C, v3d, ar, camera_old, v3d->camera,
			                      rv3d->ofs, rv3d->viewquat, &rv3d->dist, &v3d->lens,
			                      smooth_viewtx);
		}

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

void ED_view3d_clipping_calc(BoundBox *bb, float planes[4][4], bglMats *mats, const rcti *rect)
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

		planes[val][3] = -dot_v3v3(planes[val], bb->vec[val]);
	}
}


bool ED_view3d_boundbox_clip(RegionView3D *rv3d, float obmat[4][4], const BoundBox *bb)
{
	/* return 1: draw */

	float mat[4][4];
	float vec[4], min, max;
	int a, flag = -1, fl;

	if (bb == NULL) return true;
	if (bb->flag & BOUNDBOX_DISABLED) return true;

	mul_m4_m4m4(mat, rv3d->persmat, obmat);

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
		if (flag == 0) return true;
	}

	return false;
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
		rv3d->depths->damaged = true;
}

/* copies logic of get_view3d_viewplane(), keep in sync */
bool ED_view3d_clip_range_get(View3D *v3d, RegionView3D *rv3d, float *r_clipsta, float *r_clipend,
                              const bool use_ortho_factor)
{
	CameraParams params;

	BKE_camera_params_init(&params);
	BKE_camera_params_from_view3d(&params, v3d, rv3d);

	if (use_ortho_factor && params.is_ortho) {
		const float fac = 2.0f / (params.clipend - params.clipsta);
		params.clipsta *= fac;
		params.clipend *= fac;
	}

	if (r_clipsta) *r_clipsta = params.clipsta;
	if (r_clipend) *r_clipend = params.clipend;

	return params.is_ortho;
}

/* also exposed in previewrender.c */
bool ED_view3d_viewplane_get(View3D *v3d, RegionView3D *rv3d, int winx, int winy,
                             rctf *r_viewplane, float *r_clipsta, float *r_clipend, float *r_pixsize)
{
	CameraParams params;

	BKE_camera_params_init(&params);
	BKE_camera_params_from_view3d(&params, v3d, rv3d);
	BKE_camera_params_compute_viewplane(&params, winx, winy, 1.0f, 1.0f);

	if (r_viewplane) *r_viewplane = params.viewplane;
	if (r_clipsta) *r_clipsta = params.clipsta;
	if (r_clipend) *r_clipend = params.clipend;
	if (r_pixsize) *r_pixsize = params.viewdx;
	
	return params.is_ortho;
}

/**
 * Use instead of: ``bglPolygonOffset(rv3d->dist, ...)`` see bug [#37727]
 */
void ED_view3d_polygon_offset(const RegionView3D *rv3d, float dist)
{
	float viewdist = rv3d->dist;

	/* special exception for ortho camera (viewdist isnt used for perspective cameras) */
	if (dist != 0.0f) {
		if (rv3d->persp == RV3D_CAMOB) {
			if (rv3d->is_persp == false) {
				viewdist = 1.0f / max_ff(fabsf(rv3d->winmat[0][0]), fabsf(rv3d->winmat[1][1]));
			}
		}
	}

	bglPolygonOffset(viewdist, dist);
}

/*!
 * \param rect for picking, NULL not to use.
 */
void setwinmatrixview3d(ARegion *ar, View3D *v3d, rctf *rect)
{
	RegionView3D *rv3d = ar->regiondata;
	rctf viewplane;
	float clipsta, clipend, x1, y1, x2, y2;
	int orth;
	
	orth = ED_view3d_viewplane_get(v3d, rv3d, ar->winx, ar->winy, &viewplane, &clipsta, &clipend, NULL);
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

static void obmat_to_viewmat(RegionView3D *rv3d, Object *ob)
{
	float bmat[4][4];
	float tmat[3][3];
	
	rv3d->view = RV3D_VIEW_USER; /* don't show the grid */
	
	copy_m4_m4(bmat, ob->obmat);
	normalize_m4(bmat);
	invert_m4_m4(rv3d->viewmat, bmat);
	
	/* view quat calculation, needed for add object */
	copy_m3_m4(tmat, rv3d->viewmat);
	mat3_to_quat(rv3d->viewquat, tmat);
}

bool ED_view3d_lock(RegionView3D *rv3d)
{
	switch (rv3d->view) {
		case RV3D_VIEW_BOTTOM:
			copy_v4_fl4(rv3d->viewquat, 0.0, -1.0, 0.0, 0.0);
			break;

		case RV3D_VIEW_BACK:
			copy_v4_fl4(rv3d->viewquat, 0.0, 0.0, -M_SQRT1_2, -M_SQRT1_2);
			break;

		case RV3D_VIEW_LEFT:
			copy_v4_fl4(rv3d->viewquat, 0.5, -0.5, 0.5, 0.5);
			break;

		case RV3D_VIEW_TOP:
			copy_v4_fl4(rv3d->viewquat, 1.0, 0.0, 0.0, 0.0);
			break;

		case RV3D_VIEW_FRONT:
			copy_v4_fl4(rv3d->viewquat, M_SQRT1_2, -M_SQRT1_2, 0.0, 0.0);
			break;

		case RV3D_VIEW_RIGHT:
			copy_v4_fl4(rv3d->viewquat, 0.5, -0.5, -0.5, -0.5);
			break;
		default:
			return false;
	}

	return true;
}

/* don't set windows active in here, is used by renderwin too */
void setviewmatrixview3d(Scene *scene, View3D *v3d, RegionView3D *rv3d)
{
	if (rv3d->persp == RV3D_CAMOB) {      /* obs/camera */
		if (v3d->camera) {
			BKE_object_where_is_calc(scene, v3d->camera);
			obmat_to_viewmat(rv3d, v3d->camera);
		}
		else {
			quat_to_mat4(rv3d->viewmat, rv3d->viewquat);
			rv3d->viewmat[3][2] -= rv3d->dist;
		}
	}
	else {
		bool use_lock_ofs = false;


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
			use_lock_ofs = true;
		}
		else if (v3d->ob_centre_cursor) {
			float vec[3];
			copy_v3_v3(vec, ED_view3d_cursor3d_get(scene, v3d));
			translate_m4(rv3d->viewmat, -vec[0], -vec[1], -vec[2]);
			use_lock_ofs = true;
		}
		else {
			translate_m4(rv3d->viewmat, rv3d->ofs[0], rv3d->ofs[1], rv3d->ofs[2]);
		}

		/* lock offset */
		if (use_lock_ofs) {
			float persmat[4][4], persinv[4][4];
			float vec[3];

			/* we could calculate the real persmat/persinv here
			 * but it would be unreliable so better to later */
			mul_m4_m4m4(persmat, rv3d->winmat, rv3d->viewmat);
			invert_m4_m4(persinv, persmat);

			mul_v2_v2fl(vec, rv3d->ofs_lock, rv3d->is_persp ? rv3d->dist : 1.0f);
			vec[2] = 0.0f;
			mul_mat3_m4_v3(persinv, vec);
			translate_m4(rv3d->viewmat, vec[0], vec[1], vec[2]);
		}
		/* end lock offset */
	}
}

/**
 * \warning be sure to account for a negative return value
 * This is an error, "Too many objects in select buffer"
 * and no action should be taken (can crash blender) if this happens
 *
 * \note (vc->obedit == NULL) can be set to explicitly skip edit-object selection.
 */
short view3d_opengl_select(ViewContext *vc, unsigned int *buffer, unsigned int bufsize, rcti *input)
{
	Scene *scene = vc->scene;
	View3D *v3d = vc->v3d;
	ARegion *ar = vc->ar;
	rctf rect;
	short code, hits;
	char dt;
	short dtx;
	const bool use_obedit_skip = (scene->obedit != NULL) && (vc->obedit == NULL);
	
	G.f |= G_PICKSEL;
	
	/* case not a border select */
	if (input->xmin == input->xmax) {
		rect.xmin = input->xmin - 12;  /* seems to be default value for bones only now */
		rect.xmax = input->xmin + 12;
		rect.ymin = input->ymin - 12;
		rect.ymax = input->ymin + 12;
	}
	else {
		BLI_rctf_rcti_copy(&rect, input);
	}
	
	setwinmatrixview3d(ar, v3d, &rect);
	mul_m4_m4m4(vc->rv3d->persmat, vc->rv3d->winmat, vc->rv3d->viewmat);
	
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
				
				if ((base->object->restrictflag & OB_RESTRICT_SELECT) ||
				    (use_obedit_skip && (scene->obedit->data == base->object->data)))
				{
					base->selcol = 0;
				}
				else {
					base->selcol = code;
					glLoadName(code);
					draw_object(scene, ar, v3d, base, DRAW_PICKING | DRAW_CONSTCOLOR);
					
					/* we draw duplicators for selection too */
					if ((base->object->transflag & OB_DUPLI)) {
						ListBase *lb;
						DupliObject *dob;
						Base tbase;
						
						tbase.flag = OB_FROMDUPLI;
						lb = object_duplilist(G.main->eval_ctx, scene, base->object);
						
						for (dob = lb->first; dob; dob = dob->next) {
							float omat[4][4];
							
							tbase.object = dob->ob;
							copy_m4_m4(omat, dob->ob->obmat);
							copy_m4_m4(dob->ob->obmat, dob->mat);
							
							/* extra service: draw the duplicator in drawtype of parent */
							/* MIN2 for the drawtype to allow bounding box objects in groups for lods */
							dt = tbase.object->dt;   tbase.object->dt = MIN2(tbase.object->dt, base->object->dt);
							dtx = tbase.object->dtx; tbase.object->dtx = base->object->dtx;

							draw_object(scene, ar, v3d, &tbase, DRAW_PICKING | DRAW_CONSTCOLOR);
							
							tbase.object->dt = dt;
							tbase.object->dtx = dtx;

							copy_m4_m4(dob->ob->obmat, omat);
						}
						free_object_duplilist(lb);
					}
					code++;
				}
			}
		}
		v3d->xray = false;  /* restore */
	}
	
	glPopName();    /* see above (pushname) */
	hits = glRenderMode(GL_RENDER);
	
	G.f &= ~G_PICKSEL;
	setwinmatrixview3d(ar, v3d, NULL);
	mul_m4_m4m4(vc->rv3d->persmat, vc->rv3d->winmat, vc->rv3d->viewmat);
	
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

static bool view3d_localview_init(Main *bmain, Scene *scene, ScrArea *sa, ReportList *reports)
{
	View3D *v3d = sa->spacedata.first;
	Base *base;
	float min[3], max[3], box[3];
	float size = 0.0f, size_persp = 0.0f, size_ortho = 0.0f;
	unsigned int locallay;
	bool ok = false;

	if (v3d->localvd) {
		return ok;
	}

	INIT_MINMAX(min, max);

	locallay = free_localbit(bmain);

	if (locallay == 0) {
		BKE_report(reports, RPT_ERROR, "No more than 8 local views");
		ok = false;
	}
	else {
		if (scene->obedit) {
			BKE_object_minmax(scene->obedit, min, max, false);
			
			ok = true;
		
			BASACT->lay |= locallay;
			scene->obedit->lay = BASACT->lay;
		}
		else {
			for (base = FIRSTBASE; base; base = base->next) {
				if (TESTBASE(v3d, base)) {
					BKE_object_minmax(base->object, min, max, false);
					base->lay |= locallay;
					base->object->lay = base->lay;
					ok = true;
				}
			}
		}

		sub_v3_v3v3(box, max, min);
		size = max_fff(box[0], box[1], box[2]);

		/* do not zoom closer than the near clipping plane */
		size = max_ff(size, v3d->near * 1.5f);

		/* perspective size (we always switch out of camera view so no need to use its lens size) */
		size_persp = ED_view3d_radius_to_persp_dist(focallength_to_fov(v3d->lens, DEFAULT_SENSOR_WIDTH), size / 2.0f) * VIEW3D_MARGIN;
		size_ortho = ED_view3d_radius_to_ortho_dist(v3d->lens, size / 2.0f) * VIEW3D_MARGIN;
	}
	
	if (ok == true) {
		ARegion *ar;
		
		v3d->localvd = MEM_mallocN(sizeof(View3D), "localview");
		
		memcpy(v3d->localvd, v3d, sizeof(View3D));

		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->regiontype == RGN_TYPE_WINDOW) {
				RegionView3D *rv3d = ar->regiondata;

				rv3d->localvd = MEM_mallocN(sizeof(RegionView3D), "localview region");
				memcpy(rv3d->localvd, rv3d, sizeof(RegionView3D));
				
				mid_v3_v3v3(v3d->cursor, min, max);
				negate_v3_v3(rv3d->ofs, v3d->cursor);

				if (rv3d->persp == RV3D_CAMOB) {
					rv3d->persp = RV3D_PERSP;
				}

				/* perspective should be a bit farther away to look nice */
				if (rv3d->persp != RV3D_ORTHO) {
					rv3d->dist = size_persp;
				}
				else {
					rv3d->dist = size_ortho;
				}

				/* correction for window aspect ratio */
				if (ar->winy > 2 && ar->winx > 2) {
					float asp = (float)ar->winx / (float)ar->winy;
					if (asp < 1.0f) asp = 1.0f / asp;
					rv3d->dist *= asp;
				}
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

static void restore_localviewdata(Main *bmain, ScrArea *sa, int free)
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

			ED_view3d_shade_update(bmain, v3d, sa);
		}
	}
}

static bool view3d_localview_exit(Main *bmain, Scene *scene, ScrArea *sa)
{
	View3D *v3d = sa->spacedata.first;
	struct Base *base;
	unsigned int locallay;
	
	if (v3d->localvd) {
		
		locallay = v3d->lay & 0xFF000000;
		
		restore_localviewdata(bmain, sa, 1); /* 1 = free */

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
		
		DAG_on_visible_update(bmain, false);

		return true;
	}
	else {
		return false;
	}
}

static int localview_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = CTX_wm_view3d(C);
	bool changed;
	
	if (v3d->localvd) {
		changed = view3d_localview_exit(bmain, scene, sa);
	}
	else {
		changed = view3d_localview_init(bmain, scene, sa, op->reports);
	}

	if (changed) {
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
	//XXX curarea->head_swap = 0;
	//XXX allqueue(REDRAWVIEW3D, 1);
	//XXX allqueue(REDRAWBUTSALL, 0);
	//XXX reset_slowparents();
	//XXX waitcursor(0);
	//XXX G.qual = 0;
	
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

	if ((syshandle = SYS_GetSystem())) {
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

bool ED_view3d_context_activate(bContext *C)
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
		return false;
	
	for (ar = sa->regionbase.first; ar; ar = ar->next)
		if (ar->regiontype == RGN_TYPE_WINDOW)
			break;
	
	if (!ar)
		return false;
	
	/* bad context switch .. */
	CTX_wm_area_set(C, sa);
	CTX_wm_region_set(C, ar);

	return true;
}

static int game_engine_exec(bContext *C, wmOperator *op)
{
#ifdef WITH_GAMEENGINE
	Scene *startscene = CTX_data_scene(C);
	Main *bmain = CTX_data_main(C);
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

	BLI_callback_exec(bmain, &startscene->id, BLI_CB_EVT_GAME_PRE);

	rv3d = CTX_wm_region_view3d(C);
	/* sa = CTX_wm_area(C); */ /* UNUSED */
	ar = CTX_wm_region(C);

	view3d_operator_needs_opengl(C);
	
	game_set_commmandline_options(&startscene->gm);

	if ((rv3d->persp == RV3D_CAMOB) &&
	    (startscene->gm.framing.type == SCE_GAMEFRAMING_BARS) &&
	    (startscene->gm.stereoflag != STEREO_DOME))
	{
		/* Letterbox */
		rctf cam_framef;
		ED_view3d_calc_camera_border(startscene, ar, CTX_wm_view3d(C), rv3d, &cam_framef, false);
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
	//XXX BKE_scene_update_for_newframe(bmain->eval_ctx, bmain, scene, scene->lay);

	BLI_callback_exec(bmain, &startscene->id, BLI_CB_EVT_GAME_POST);

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

float ED_view3d_pixel_size(RegionView3D *rv3d, const float co[3])
{
	return mul_project_m4_v3_zfac(rv3d->persmat, co) * rv3d->pixsize * U.pixelsize;
}

float ED_view3d_radius_to_persp_dist(const float angle, const float radius)
{
	return (radius / 2.0f) * fabsf(1.0f / cosf((((float)M_PI) - angle) / 2.0f));
}

float ED_view3d_radius_to_ortho_dist(const float lens, const float radius)
{
	return radius / (DEFAULT_SENSOR_WIDTH / lens);
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
