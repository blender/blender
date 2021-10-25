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

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rect.h"
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

#include "GPU_select.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_armature.h"


#ifdef WITH_GAMEENGINE
#  include "BLI_listbase.h"
#  include "BLI_callbacks.h"

#  include "GPU_draw.h"

#  include "BL_System.h"
#endif


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

	bool use_dyn_ofs;
	float dyn_ofs[3];

	/* When smooth-view is enabled, store the 'rv3d->view' here,
	 * assign back when the view motion is completed. */
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
void ED_view3d_smooth_view_ex(
        /* avoid passing in the context */
        wmWindowManager *wm, wmWindow *win, ScrArea *sa,
        View3D *v3d, ARegion *ar, const int smooth_viewtx,
        const V3D_SmoothParams *sview)
{
	RegionView3D *rv3d = ar->regiondata;
	struct SmoothView3DStore sms = {{0}};
	bool ok = false;
	
	/* initialize sms */
	view3d_smooth_view_state_backup(&sms.dst, v3d, rv3d);
	view3d_smooth_view_state_backup(&sms.src, v3d, rv3d);
	/* if smoothview runs multiple times... */
	if (rv3d->sms == NULL) {
		view3d_smooth_view_state_backup(&sms.org, v3d, rv3d);
	}
	else {
		sms.org = rv3d->sms->org;
	}
	sms.org_view = rv3d->view;

	/* sms.to_camera = false; */  /* initizlized to zero anyway */

	/* note on camera locking, this is a little confusing but works ok.
	 * we may be changing the view 'as if' there is no active camera, but in fact
	 * there is an active camera which is locked to the view.
	 *
	 * In the case where smooth view is moving _to_ a camera we don't want that
	 * camera to be moved or changed, so only when the camera is not being set should
	 * we allow camera option locking to initialize the view settings from the camera.
	 */
	if (sview->camera == NULL && sview->camera_old == NULL) {
		ED_view3d_camera_lock_init(v3d, rv3d);
	}

	/* store the options we want to end with */
	if (sview->ofs)
		copy_v3_v3(sms.dst.ofs, sview->ofs);
	if (sview->quat)
		copy_qt_qt(sms.dst.quat, sview->quat);
	if (sview->dist)
		sms.dst.dist = *sview->dist;
	if (sview->lens)
		sms.dst.lens = *sview->lens;

	if (sview->dyn_ofs) {
		BLI_assert(sview->ofs  == NULL);
		BLI_assert(sview->quat != NULL);

		copy_v3_v3(sms.dyn_ofs, sview->dyn_ofs);
		sms.use_dyn_ofs = true;

		/* calculate the final destination offset */
		view3d_orbit_apply_dyn_ofs(sms.dst.ofs, sms.src.ofs, sms.src.quat, sms.dst.quat, sms.dyn_ofs);
	}

	if (sview->camera) {
		sms.dst.dist = ED_view3d_offset_distance(sview->camera->obmat, sview->ofs, VIEW3D_DIST_FALLBACK);
		ED_view3d_from_object(sview->camera, sms.dst.ofs, sms.dst.quat, &sms.dst.dist, &sms.dst.lens);
		sms.to_camera = true; /* restore view3d values in end */
	}
	
	/* skip smooth viewing for render engine draw */
	if (smooth_viewtx && v3d->drawtype != OB_RENDER) {
		bool changed = false; /* zero means no difference */
		
		if (sview->camera_old != sview->camera)
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
			if (sview->camera_old) {
				sms.src.dist = ED_view3d_offset_distance(sview->camera_old->obmat, rv3d->ofs, 0.0f);
				/* this */
				ED_view3d_from_object(sview->camera_old, sms.src.ofs, sms.src.quat, &sms.src.dist, &sms.src.lens);
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
			if (sview->quat && !sview->ofs && !sview->dist) {
				/* scale the time allowed by the rotation */
				sms.time_allowed *= (double)fabsf(angle_signed_normalized_qtqt(sms.dst.quat, sms.src.quat)) / M_PI; /* 180deg == 1.0 */
			}

			/* ensure it shows correct */
			if (sms.to_camera) {
				/* use ortho if we move from an ortho view to an ortho camera */
				rv3d->persp = (((rv3d->is_persp == false) &&
				                (sview->camera->type == OB_CAMERA) &&
				                (((Camera *)sview->camera->data)->type == CAM_ORTHO)) ?
				                RV3D_ORTHO : RV3D_PERSP);
			}

			rv3d->rflag |= RV3D_NAVIGATING;
			
			/* not essential but in some cases the caller will tag the area for redraw,
			 * and in that case we can get a flicker of the 'org' user view but we want to see 'src' */
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

void ED_view3d_smooth_view(
        bContext *C,
        View3D *v3d, ARegion *ar, const int smooth_viewtx,
        const struct V3D_SmoothParams *sview)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = CTX_wm_area(C);

	ED_view3d_smooth_view_ex(
	        wm, win, sa,
	        v3d, ar, smooth_viewtx,
	        sview);
}

/* only meant for timer usage */
static void view3d_smoothview_apply(bContext *C, View3D *v3d, ARegion *ar, bool sync_boxview)
{
	RegionView3D *rv3d = ar->regiondata;
	struct SmoothView3DStore *sms = rv3d->sms;
	float step, step_inv;
	
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
			ED_view3d_camera_lock_autokey(v3d, rv3d, C, true, true);
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

		interp_qt_qtqt(rv3d->viewquat, sms->src.quat, sms->dst.quat, step);

		if (sms->use_dyn_ofs) {
			view3d_orbit_apply_dyn_ofs(rv3d->ofs, sms->src.ofs, sms->src.quat, rv3d->viewquat, sms->dyn_ofs);
		}
		else {
			interp_v3_v3v3(rv3d->ofs, sms->src.ofs,  sms->dst.ofs,  step);
		}
		
		rv3d->dist = sms->dst.dist * step + sms->src.dist * step_inv;
		v3d->lens  = sms->dst.lens * step + sms->src.lens * step_inv;

		ED_view3d_camera_lock_sync(v3d, rv3d);
		if (ED_screen_animation_playing(CTX_wm_manager(C))) {
			ED_view3d_camera_lock_autokey(v3d, rv3d, C, true, true);
		}

	}
	
	if (sync_boxview && (rv3d->viewlock & RV3D_BOXVIEW)) {
		view3d_boxview_copy(CTX_wm_area(C), ar);
	}

	/* note: this doesn't work right because the v3d->lens is now used in ortho mode r51636,
	 * when switching camera in quad-view the other ortho views would zoom & reset.
	 *
	 * For now only redraw all regions when smoothview finishes.
	 */
	if (step >= 1.0f) {
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
	}
	else {
		ED_region_tag_redraw(ar);
	}
}

static int view3d_smoothview_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	View3D *v3d = CTX_wm_view3d(C);
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	/* escape if not our timer */
	if (rv3d->smooth_timer == NULL || rv3d->smooth_timer != event->customdata) {
		return OPERATOR_PASS_THROUGH;
	}

	view3d_smoothview_apply(C, v3d, ar, true);

	return OPERATOR_FINISHED;
}

/**
 * Apply the smoothview immediately, use when we need to start a new view operation.
 * (so we don't end up half-applying a view operation when pressing keys quickly).
 */
void ED_view3d_smooth_view_force_finish(
        bContext *C,
        View3D *v3d, ARegion *ar)
{
	RegionView3D *rv3d = ar->regiondata;

	if (rv3d && rv3d->sms) {
		rv3d->sms->time_allowed = 0.0;  /* force finishing */
		view3d_smoothview_apply(C, v3d, ar, false);

		/* force update of view matrix so tools that run immediately after
		 * can use them without redrawing first */
		Scene *scene = CTX_data_scene(C);
		ED_view3d_update_viewmat(scene, v3d, ar, NULL, NULL, NULL);
	}
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
	View3D *v3d;
	ARegion *ar;
	RegionView3D *rv3d;

	ObjectTfmProtectedChannels obtfm;

	ED_view3d_context_user_region(C, &v3d, &ar);
	rv3d = ar->regiondata;

	ED_view3d_lastview_store(rv3d);

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
	View3D *v3d;
	ARegion *ar;

	if (ED_view3d_context_user_region(C, &v3d, &ar)) {
		RegionView3D *rv3d = ar->regiondata;
		if (v3d && v3d->camera && !ID_IS_LINKED_DATABLOCK(v3d->camera)) {
			if (rv3d && (rv3d->viewlock & RV3D_LOCKED) == 0) {
				if (rv3d->persp != RV3D_CAMOB) {
					return 1;
				}
			}
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
	float r_scale; /* only for ortho cameras */

	if (camera_ob == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active camera");
		return OPERATOR_CANCELLED;
	}

	/* this function does all the important stuff */
	if (BKE_camera_view_frame_fit_to_scene(scene, v3d, camera_ob, r_co, &r_scale)) {
		ObjectTfmProtectedChannels obtfm;
		float obmat_new[4][4];

		if ((camera_ob->type == OB_CAMERA) && (((Camera *)camera_ob->data)->type == CAM_ORTHO)) {
			((Camera *)camera_ob->data)->ortho_scale = r_scale;
		}

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

static void sync_viewport_camera_smoothview(bContext *C, View3D *v3d, Object *ob, const int smooth_viewtx)
{
	Main *bmain = CTX_data_main(C);
	for (bScreen *screen = bmain->screen.first; screen != NULL; screen = screen->id.next) {
		for (ScrArea *area = screen->areabase.first; area != NULL; area = area->next) {
			for (SpaceLink *space_link = area->spacedata.first; space_link != NULL; space_link = space_link->next) {
				if (space_link->spacetype == SPACE_VIEW3D) {
					View3D *other_v3d = (View3D *)space_link;
					if (other_v3d == v3d) {
						continue;
					}
					if (other_v3d->camera == ob) {
						continue;
					}
					if (v3d->scenelock) {
						ListBase *lb = (space_link == area->spacedata.first)
						                   ? &area->regionbase
						                   : &space_link->regionbase;
						for (ARegion *other_ar = lb->first; other_ar != NULL; other_ar = other_ar->next) {
							if (other_ar->regiontype == RGN_TYPE_WINDOW) {
								if (other_ar->regiondata) {
									RegionView3D *other_rv3d = other_ar->regiondata;
									if (other_rv3d->persp == RV3D_CAMOB) {
										Object *other_camera_old = other_v3d->camera;
										other_v3d->camera = ob;
										ED_view3d_lastview_store(other_rv3d);
										ED_view3d_smooth_view(
										        C, other_v3d, other_ar, smooth_viewtx,
										        &(const V3D_SmoothParams) {
										            .camera_old = other_camera_old,
										            .camera = other_v3d->camera,
										            .ofs = other_rv3d->ofs,
										            .quat = other_rv3d->viewquat,
										            .dist = &other_rv3d->dist,
										            .lens = &other_v3d->lens});
									}
									else {
										other_v3d->camera = ob;
									}
								}
							}
						}
					}
				}
			}
		}
	}
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

		/* unlikely but looks like a glitch when set to the same */
		if (camera_old != ob) {
			ED_view3d_lastview_store(rv3d);

			ED_view3d_smooth_view(
			        C, v3d, ar, smooth_viewtx,
			        &(const V3D_SmoothParams) {
			            .camera_old = camera_old, .camera = v3d->camera,
			            .ofs = rv3d->ofs, .quat = rv3d->viewquat,
			            .dist = &rv3d->dist, .lens = &v3d->lens});
		}

		if (v3d->scenelock) {
			sync_viewport_camera_smoothview(C, v3d, ob, smooth_viewtx);
			WM_event_add_notifier(C, NC_SCENE, scene);
		}
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, scene);
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

void ED_view3d_clipping_calc_from_boundbox(float clip[4][4], const BoundBox *bb, const bool is_flip)
{
	int val;

	for (val = 0; val < 4; val++) {
		normal_tri_v3(clip[val], bb->vec[val], bb->vec[val == 3 ? 0 : val + 1], bb->vec[val + 4]);
		if (UNLIKELY(is_flip)) {
			negate_v3(clip[val]);
		}

		clip[val][3] = -dot_v3v3(clip[val], bb->vec[val]);
	}
}

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

	ED_view3d_clipping_calc_from_boundbox(planes, bb, flip_sign);
}

static bool view3d_boundbox_clip_m4(const BoundBox *bb, float persmatob[4][4])
{
	int a, flag = -1, fl;

	for (a = 0; a < 8; a++) {
		float vec[4], min, max;
		copy_v3_v3(vec, bb->vec[a]);
		vec[3] = 1.0;
		mul_m4_v4(persmatob, vec);
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

bool ED_view3d_boundbox_clip_ex(const RegionView3D *rv3d, const BoundBox *bb, float obmat[4][4])
{
	/* return 1: draw */

	float persmatob[4][4];

	if (bb == NULL) return true;
	if (bb->flag & BOUNDBOX_DISABLED) return true;

	mul_m4_m4m4(persmatob, (float(*)[4])rv3d->persmat, obmat);

	return view3d_boundbox_clip_m4(bb, persmatob);
}

bool ED_view3d_boundbox_clip(RegionView3D *rv3d, const BoundBox *bb)
{
	if (bb == NULL) return true;
	if (bb->flag & BOUNDBOX_DISABLED) return true;

	return view3d_boundbox_clip_m4(bb, rv3d->persmatob);
}

float ED_view3d_depth_read_cached(const ViewContext *vc, int x, int y)
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

void ED_view3d_dist_range_get(
        const View3D *v3d,
        float r_dist_range[2])
{
	r_dist_range[0] = v3d->grid * 0.001f;
	r_dist_range[1] = v3d->far * 10.0f;
}

/* copies logic of get_view3d_viewplane(), keep in sync */
bool ED_view3d_clip_range_get(
        const View3D *v3d, const RegionView3D *rv3d,
        float *r_clipsta, float *r_clipend,
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

bool ED_view3d_viewplane_get(
        const View3D *v3d, const RegionView3D *rv3d, int winx, int winy,
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
void ED_view3d_polygon_offset(const RegionView3D *rv3d, const float dist)
{
	float viewdist;

	if (rv3d->rflag & RV3D_ZOFFSET_DISABLED) {
		return;
	}

	viewdist = rv3d->dist;

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

/**
 * \param rect optional for picking (can be NULL).
 */
void view3d_winmatrix_set(ARegion *ar, const View3D *v3d, const rcti *rect)
{
	RegionView3D *rv3d = ar->regiondata;
	rctf viewplane;
	float clipsta, clipend;
	bool is_ortho;
	
	is_ortho = ED_view3d_viewplane_get(v3d, rv3d, ar->winx, ar->winy, &viewplane, &clipsta, &clipend, NULL);
	rv3d->is_persp = !is_ortho;

#if 0
	printf("%s: %d %d %f %f %f %f %f %f\n", __func__, winx, winy,
	       viewplane.xmin, viewplane.ymin, viewplane.xmax, viewplane.ymax,
	       clipsta, clipend);
#endif

	if (rect) {  /* picking */
		rctf r;
		r.xmin = viewplane.xmin + (BLI_rctf_size_x(&viewplane) * (rect->xmin / (float)ar->winx));
		r.ymin = viewplane.ymin + (BLI_rctf_size_y(&viewplane) * (rect->ymin / (float)ar->winy));
		r.xmax = viewplane.xmin + (BLI_rctf_size_x(&viewplane) * (rect->xmax / (float)ar->winx));
		r.ymax = viewplane.ymin + (BLI_rctf_size_y(&viewplane) * (rect->ymax / (float)ar->winy));
		viewplane = r;
	}

	if (is_ortho) {
		wmOrtho(viewplane.xmin, viewplane.xmax, viewplane.ymin, viewplane.ymax, clipsta, clipend);
	}
	else {
		wmFrustum(viewplane.xmin, viewplane.xmax, viewplane.ymin, viewplane.ymax, clipsta, clipend);
	}

	/* update matrix in 3d view region */
	glGetFloatv(GL_PROJECTION_MATRIX, (float *)rv3d->winmat);
}

static void obmat_to_viewmat(RegionView3D *rv3d, Object *ob)
{
	float bmat[4][4];

	rv3d->view = RV3D_VIEW_USER; /* don't show the grid */

	normalize_m4_m4(bmat, ob->obmat);
	invert_m4_m4(rv3d->viewmat, bmat);

	/* view quat calculation, needed for add object */
	mat4_normalized_to_quat(rv3d->viewquat, rv3d->viewmat);
}

static float view3d_quat_axis[6][4] = {
	{M_SQRT1_2, -M_SQRT1_2, 0.0f, 0.0f},    /* RV3D_VIEW_FRONT */
	{0.0f, 0.0f, -M_SQRT1_2, -M_SQRT1_2},   /* RV3D_VIEW_BACK */
	{0.5f, -0.5f, 0.5f, 0.5f},              /* RV3D_VIEW_LEFT */
	{0.5f, -0.5f, -0.5f, -0.5f},            /* RV3D_VIEW_RIGHT */
	{1.0f, 0.0f, 0.0f, 0.0f},               /* RV3D_VIEW_TOP */
	{0.0f, -1.0f, 0.0f, 0.0f},              /* RV3D_VIEW_BOTTOM */
};


bool ED_view3d_quat_from_axis_view(const char view, float quat[4])
{
	if (RV3D_VIEW_IS_AXIS(view)) {
		copy_qt_qt(quat, view3d_quat_axis[view - RV3D_VIEW_FRONT]);
		return true;
	}
	else {
		return false;
	}
}

char ED_view3d_quat_to_axis_view(const float quat[4], const float epsilon)
{
	/* quat values are all unit length */

	char view;

	for (view = RV3D_VIEW_FRONT; view <= RV3D_VIEW_BOTTOM; view++) {
		if (fabsf(angle_signed_qtqt(quat, view3d_quat_axis[view - RV3D_VIEW_FRONT])) < epsilon) {
			return view;
		}
	}

	return RV3D_VIEW_USER;
}

char ED_view3d_lock_view_from_index(int index)
{
	switch (index) {
		case 0:  return RV3D_VIEW_FRONT;
		case 1:  return RV3D_VIEW_TOP;
		case 2:  return RV3D_VIEW_RIGHT;
		default: return RV3D_VIEW_USER;
	}

}

char ED_view3d_axis_view_opposite(char view)
{
	switch (view) {
		case RV3D_VIEW_FRONT:   return RV3D_VIEW_BACK;
		case RV3D_VIEW_BACK:    return RV3D_VIEW_FRONT;
		case RV3D_VIEW_LEFT:    return RV3D_VIEW_RIGHT;
		case RV3D_VIEW_RIGHT:   return RV3D_VIEW_LEFT;
		case RV3D_VIEW_TOP:     return RV3D_VIEW_BOTTOM;
		case RV3D_VIEW_BOTTOM:  return RV3D_VIEW_TOP;
	}

	return RV3D_VIEW_USER;
}


bool ED_view3d_lock(RegionView3D *rv3d)
{
	return ED_view3d_quat_from_axis_view(rv3d->view, rv3d->viewquat);
}

/**
 * Sets #RegionView3D.viewmat
 *
 * \param scene: Scene for camera and cursor location.
 * \param v3d: View 3D space data.
 * \param rv3d: 3D region which stores the final matrices.
 * \param rect_scale: Optional 2D scale argument,
 * Use when displaying a sub-region, eg: when #view3d_winmatrix_set takes a 'rect' argument.
 *
 * \note don't set windows active in here, is used by renderwin too.
 * */
void view3d_viewmatrix_set(Scene *scene, const View3D *v3d, RegionView3D *rv3d, const float rect_scale[2])
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
		if (rv3d->viewlock & RV3D_LOCKED)
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
			copy_v3_v3(vec, ED_view3d_cursor3d_get(scene, (View3D *)v3d));
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

			if (rect_scale) {
				vec[0] /= rect_scale[0];
				vec[1] /= rect_scale[1];
			}

			mul_mat3_m4_v3(persinv, vec);
			translate_m4(rv3d->viewmat, vec[0], vec[1], vec[2]);
		}
		/* end lock offset */
	}
}

/**
 * Optionally cache data for multiple calls to #view3d_opengl_select
 *
 * just avoid GPU_select headers outside this file
 */
void view3d_opengl_select_cache_begin(void)
{
	GPU_select_cache_begin();
}

void view3d_opengl_select_cache_end(void)
{
	GPU_select_cache_end();
}

/**
 * \warning be sure to account for a negative return value
 * This is an error, "Too many objects in select buffer"
 * and no action should be taken (can crash blender) if this happens
 *
 * \note (vc->obedit == NULL) can be set to explicitly skip edit-object selection.
 */
int view3d_opengl_select(
        ViewContext *vc, unsigned int *buffer, unsigned int bufsize, const rcti *input,
        eV3DSelectMode select_mode)
{
	Scene *scene = vc->scene;
	View3D *v3d = vc->v3d;
	ARegion *ar = vc->ar;
	rcti rect;
	int hits;
	const bool use_obedit_skip = (scene->obedit != NULL) && (vc->obedit == NULL);
	const bool is_pick_select = (U.gpu_select_pick_deph != 0);
	const bool do_passes = (
	        (is_pick_select == false) &&
	        (select_mode == VIEW3D_SELECT_PICK_NEAREST) &&
	        GPU_select_query_check_active());
	const bool use_nearest = (is_pick_select && select_mode == VIEW3D_SELECT_PICK_NEAREST);

	char gpu_select_mode;

	/* case not a border select */
	if (input->xmin == input->xmax) {
		/* seems to be default value for bones only now */
		BLI_rcti_init_pt_radius(&rect, (const int[2]){input->xmin, input->ymin}, 12);
	}
	else {
		rect = *input;
	}

	if (is_pick_select) {
		if (is_pick_select && select_mode == VIEW3D_SELECT_PICK_NEAREST) {
			gpu_select_mode = GPU_SELECT_PICK_NEAREST;
		}
		else if (is_pick_select && select_mode == VIEW3D_SELECT_PICK_ALL) {
			gpu_select_mode = GPU_SELECT_PICK_ALL;
		}
		else {
			gpu_select_mode = GPU_SELECT_ALL;
		}
	}
	else {
		if (do_passes) {
			gpu_select_mode = GPU_SELECT_NEAREST_FIRST_PASS;
		}
		else {
			gpu_select_mode = GPU_SELECT_ALL;
		}
	}

	/* Re-use cache (rect must be smaller then the cached)
	 * other context is assumed to be unchanged */
	if (GPU_select_is_cached()) {
		GPU_select_begin(buffer, bufsize, &rect, gpu_select_mode, 0);
		GPU_select_cache_load_id();
		hits = GPU_select_end();
		goto finally;
	}

	G.f |= G_PICKSEL;

	/* Important we use the 'viewmat' and don't re-calculate since
	 * the object & bone view locking takes 'rect' into account, see: T51629. */
	ED_view3d_draw_setup_view(vc->win, scene, ar, v3d, vc->rv3d->viewmat, NULL, &rect);

	if (v3d->drawtype > OB_WIRE) {
		v3d->zbuf = true;
		glEnable(GL_DEPTH_TEST);
	}
	
	if (vc->rv3d->rflag & RV3D_CLIPPING)
		ED_view3d_clipping_set(vc->rv3d);
	
	GPU_select_begin(buffer, bufsize, &rect, gpu_select_mode, 0);

	ED_view3d_draw_select_loop(vc, scene, v3d, ar, use_obedit_skip, use_nearest);

	hits = GPU_select_end();
	
	/* second pass, to get the closest object to camera */
	if (do_passes && (hits > 0)) {
		GPU_select_begin(buffer, bufsize, &rect, GPU_SELECT_NEAREST_SECOND_PASS, hits);

		ED_view3d_draw_select_loop(vc, scene, v3d, ar, use_obedit_skip, use_nearest);

		GPU_select_end();
	}

	G.f &= ~G_PICKSEL;
	ED_view3d_draw_setup_view(vc->win, scene, ar, v3d, vc->rv3d->viewmat, NULL, NULL);
	
	if (v3d->drawtype > OB_WIRE) {
		v3d->zbuf = 0;
		glDisable(GL_DEPTH_TEST);
	}
	
	if (vc->rv3d->rflag & RV3D_CLIPPING)
		ED_view3d_clipping_disable();

finally:
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

static bool view3d_localview_init(
        wmWindowManager *wm, wmWindow *win,
        Main *bmain, Scene *scene, ScrArea *sa, const int smooth_viewtx,
        ReportList *reports)
{
	View3D *v3d = sa->spacedata.first;
	Base *base;
	float min[3], max[3], box[3], mid[3];
	float size = 0.0f;
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
	}
	
	if (ok == true) {
		ARegion *ar;
		
		v3d->localvd = MEM_mallocN(sizeof(View3D), "localview");
		
		memcpy(v3d->localvd, v3d, sizeof(View3D));

		mid_v3_v3v3(mid, min, max);

		copy_v3_v3(v3d->cursor, mid);

		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->regiontype == RGN_TYPE_WINDOW) {
				RegionView3D *rv3d = ar->regiondata;
				bool ok_dist = true;

				/* new view values */
				Object *camera_old = NULL;
				float dist_new, ofs_new[3];

				rv3d->localvd = MEM_mallocN(sizeof(RegionView3D), "localview region");
				memcpy(rv3d->localvd, rv3d, sizeof(RegionView3D));

				negate_v3_v3(ofs_new, mid);

				if (rv3d->persp == RV3D_CAMOB) {
					rv3d->persp = RV3D_PERSP;
					camera_old = v3d->camera;
				}

				if (rv3d->persp == RV3D_ORTHO) {
					if (size < 0.0001f) {
						ok_dist = false;
					}
				}

				if (ok_dist) {
					dist_new = ED_view3d_radius_to_dist(v3d, ar, rv3d->persp, true, (size / 2) * VIEW3D_MARGIN);
					if (rv3d->persp == RV3D_PERSP) {
						/* don't zoom closer than the near clipping plane */
						dist_new = max_ff(dist_new, v3d->near * 1.5f);
					}
				}

				ED_view3d_smooth_view_ex(
				        wm, win, sa, v3d, ar, smooth_viewtx,
				            &(const V3D_SmoothParams) {
				                .camera_old = camera_old,
				                .ofs = ofs_new, .quat = rv3d->viewquat,
				                .dist = ok_dist ? &dist_new : NULL, .lens = &v3d->lens});
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

	DAG_on_visible_update(bmain, false);

	return ok;
}

static void restore_localviewdata(wmWindowManager *wm, wmWindow *win, Main *bmain, Scene *scene, ScrArea *sa, const int smooth_viewtx)
{
	const bool free = true;
	ARegion *ar;
	View3D *v3d = sa->spacedata.first;
	Object *camera_old, *camera_new;
	
	if (v3d->localvd == NULL) return;
	
	camera_old = v3d->camera;
	camera_new = v3d->localvd->camera;

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
				Object *camera_old_rv3d, *camera_new_rv3d;

				camera_old_rv3d = (rv3d->persp          == RV3D_CAMOB) ? camera_old : NULL;
				camera_new_rv3d = (rv3d->localvd->persp == RV3D_CAMOB) ? camera_new : NULL;

				rv3d->view = rv3d->localvd->view;
				rv3d->persp = rv3d->localvd->persp;
				rv3d->camzoom = rv3d->localvd->camzoom;

				ED_view3d_smooth_view_ex(
				        wm, win, sa,
				        v3d, ar, smooth_viewtx,
				        &(const V3D_SmoothParams) {
				            .camera_old = camera_old_rv3d, .camera = camera_new_rv3d,
				            .ofs = rv3d->localvd->ofs, .quat = rv3d->localvd->viewquat,
				            .dist = &rv3d->localvd->dist});

				if (free) {
					MEM_freeN(rv3d->localvd);
					rv3d->localvd = NULL;
				}
			}

			ED_view3d_shade_update(bmain, scene, v3d, sa);
		}
	}
}

static bool view3d_localview_exit(
        wmWindowManager *wm, wmWindow *win,
        Main *bmain, Scene *scene, ScrArea *sa, const int smooth_viewtx)
{
	View3D *v3d = sa->spacedata.first;
	struct Base *base;
	unsigned int locallay;
	
	if (v3d->localvd) {
		
		locallay = v3d->lay & 0xFF000000;

		restore_localviewdata(wm, win, bmain, scene, sa, smooth_viewtx);

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
	const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = CTX_wm_view3d(C);
	bool changed;
	
	if (v3d->localvd) {
		changed = view3d_localview_exit(wm, win, bmain, scene, sa, smooth_viewtx);
	}
	else {
		changed = view3d_localview_init(wm, win, bmain, scene, sa, smooth_viewtx, op->reports);
	}

	if (changed) {
		DAG_id_type_tag(bmain, ID_OB);
		ED_area_tag_redraw(sa);

		/* unselected objects become selected when exiting */
		if (v3d->localvd == NULL) {
			WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		}

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
	
	BLI_listbase_clear(&win->queue);
	
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
	bScreen *screen;
	/* we need a context and area to launch BGE
	 * it's a temporary solution to avoid crash at load time
	 * if we try to auto run the BGE. Ideally we want the
	 * context to be set as soon as we load the file. */

	if (CTX_wm_window(C) == NULL) return 0;
	if ((screen = CTX_wm_screen(C)) == NULL) return 0;

	if (CTX_data_mode_enum(C) != CTX_MODE_OBJECT)
		return 0;

	if (!BKE_scene_uses_blender_game(screen->scene))
		return 0;

	return 1;
}

bool ED_view3d_context_activate(bContext *C)
{
	bScreen *sc = CTX_wm_screen(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar;

	/* sa can be NULL when called from python */
	if (sa == NULL || sa->spacetype != SPACE_VIEW3D) {
		sa = BKE_screen_find_big_area(sc, SPACE_VIEW3D, 0);
	}

	if (sa == NULL) {
		return false;
	}
	
	ar = BKE_area_find_region_active_win(sa);
	if (ar == NULL) {
		return false;
	}
	
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

float ED_view3d_pixel_size(const RegionView3D *rv3d, const float co[3])
{
	return mul_project_m4_v3_zfac((float(*)[4])rv3d->persmat, co) * rv3d->pixsize * U.pixelsize;
}

float ED_view3d_radius_to_dist_persp(const float angle, const float radius)
{
	return radius * (1.0f / tanf(angle / 2.0f));
}

float ED_view3d_radius_to_dist_ortho(const float lens, const float radius)
{
	return radius / (DEFAULT_SENSOR_WIDTH / lens);
}

/**
 * Return a new RegionView3D.dist value to fit the \a radius.
 *
 * \note Depth isn't taken into account, this will fit a flat plane exactly,
 * but points towards the view (with a perspective projection),
 * may be within the radius but outside the view. eg:
 *
 * <pre>
 *           +
 * pt --> + /^ radius
 *         / |
 *        /  |
 * view  +   +
 *        \  |
 *         \ |
 *          \|
 *           +
 * </pre>
 *
 * \param ar  Can be NULL if \a use_aspect is false.
 * \param persp  Allow the caller to tell what kind of perspective to use (ortho/view/camera)
 * \param use_aspect  Increase the distance to account for non 1:1 view aspect.
 * \param radius  The radius will be fitted exactly, typically pre-scaled by a margin (#VIEW3D_MARGIN).
 */
float ED_view3d_radius_to_dist(
        const View3D *v3d, const ARegion *ar,
        const char persp, const bool use_aspect,
        const float radius)
{
	float dist;

	BLI_assert(ELEM(persp, RV3D_ORTHO, RV3D_PERSP, RV3D_CAMOB));
	BLI_assert((persp != RV3D_CAMOB) || v3d->camera);

	if (persp == RV3D_ORTHO) {
		dist = ED_view3d_radius_to_dist_ortho(v3d->lens, radius);
	}
	else {
		float lens, sensor_size, zoom;
		float angle;

		if (persp == RV3D_CAMOB) {
			CameraParams params;
			BKE_camera_params_init(&params);
			params.clipsta = v3d->near;
			params.clipend = v3d->far;
			BKE_camera_params_from_object(&params, v3d->camera);

			lens = params.lens;
			sensor_size = BKE_camera_sensor_size(params.sensor_fit, params.sensor_x, params.sensor_y);

			/* ignore 'rv3d->camzoom' because we want to fit to the cameras frame */
			zoom = CAMERA_PARAM_ZOOM_INIT_CAMOB;
		}
		else {
			lens = v3d->lens;
			sensor_size = DEFAULT_SENSOR_WIDTH;
			zoom = CAMERA_PARAM_ZOOM_INIT_PERSP;
		}

		angle = focallength_to_fov(lens, sensor_size);

		/* zoom influences lens, correct this by scaling the angle as a distance (by the zoom-level) */
		angle = atanf(tanf(angle / 2.0f) * zoom) * 2.0f;

		dist = ED_view3d_radius_to_dist_persp(angle, radius);
	}

	if (use_aspect) {
		const RegionView3D *rv3d = ar->regiondata;

		float winx, winy;

		if (persp == RV3D_CAMOB) {
			/* camera frame x/y in pixels */
			winx = ar->winx / rv3d->viewcamtexcofac[0];
			winy = ar->winy / rv3d->viewcamtexcofac[1];
		}
		else {
			winx = ar->winx;
			winy = ar->winy;
		}

		if (winx && winy) {
			float aspect = winx / winy;
			if (aspect < 1.0f) {
				aspect = 1.0f / aspect;
			}
			dist *= aspect;
		}
	}

	return dist;
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
