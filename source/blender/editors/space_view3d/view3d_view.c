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

#include "BKE_action.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "UI_resources.h"

#include "GPU_glew.h"
#include "GPU_select.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "DRW_engine.h"

#include "view3d_intern.h"  /* own include */

/* -------------------------------------------------------------------- */
/** \name Smooth View Operator & Utilities
 *
 * Use for view transitions to have smooth (animated) transitions.
 * \{ */

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
        const Depsgraph *depsgraph, wmWindowManager *wm, wmWindow *win, ScrArea *sa,
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
		ED_view3d_camera_lock_init(depsgraph, v3d, rv3d);
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
		Object *ob_camera_eval = DEG_get_evaluated_object(depsgraph, sview->camera);
		sms.dst.dist = ED_view3d_offset_distance(ob_camera_eval->obmat, sview->ofs, VIEW3D_DIST_FALLBACK);
		ED_view3d_from_object(ob_camera_eval, sms.dst.ofs, sms.dst.quat, &sms.dst.dist, &sms.dst.lens);
		sms.to_camera = true; /* restore view3d values in end */
	}

	/* skip smooth viewing for render engine draw */
	if (smooth_viewtx && v3d->shading.type != OB_RENDER) {
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
				Object *ob_camera_old_eval = DEG_get_evaluated_object(depsgraph, sview->camera_old);
				sms.src.dist = ED_view3d_offset_distance(ob_camera_old_eval->obmat, rv3d->ofs, 0.0f);
				/* this */
				ED_view3d_from_object(ob_camera_old_eval, sms.src.ofs, sms.src.quat, &sms.src.dist, &sms.src.lens);
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
				Object *ob_camera_eval = DEG_get_evaluated_object(depsgraph, sview->camera);
				rv3d->persp = (((rv3d->is_persp == false) &&
				                (ob_camera_eval->type == OB_CAMERA) &&
				                (((Camera *)ob_camera_eval->data)->type == CAM_ORTHO)) ?
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

			ED_view3d_camera_lock_sync(depsgraph, v3d, rv3d);
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
	const Depsgraph *depsgraph = CTX_data_depsgraph(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = CTX_wm_area(C);

	ED_view3d_smooth_view_ex(
	        depsgraph,
	        wm, win, sa,
	        v3d, ar, smooth_viewtx,
	        sview);
}

/* only meant for timer usage */
static void view3d_smoothview_apply(bContext *C, View3D *v3d, ARegion *ar, bool sync_boxview)
{
	const Depsgraph *depsgraph = CTX_data_depsgraph(C);
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

			ED_view3d_camera_lock_sync(depsgraph, v3d, rv3d);
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

		ED_view3d_camera_lock_sync(depsgraph, v3d, rv3d);
		if (ED_screen_animation_playing(CTX_wm_manager(C))) {
			ED_view3d_camera_lock_autokey(v3d, rv3d, C, true, true);
		}

		/* Event handling won't know if a UI item has been moved under the pointer. */
		WM_event_add_mousemove(C);
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
		Depsgraph *depsgraph = CTX_data_depsgraph(C);
		Scene *scene = CTX_data_scene(C);
		ED_view3d_update_viewmat(depsgraph, scene, v3d, ar, NULL, NULL, NULL);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera to View Operator
 * \{ */

static int view3d_camera_to_view_exec(bContext *C, wmOperator *UNUSED(op))
{
	const Depsgraph *depsgraph = CTX_data_depsgraph(C);
	View3D *v3d;
	ARegion *ar;
	RegionView3D *rv3d;

	ObjectTfmProtectedChannels obtfm;

	ED_view3d_context_user_region(C, &v3d, &ar);
	rv3d = ar->regiondata;

	ED_view3d_lastview_store(rv3d);

	BKE_object_tfm_protected_backup(v3d->camera, &obtfm);

	ED_view3d_to_object(depsgraph, v3d->camera, rv3d->ofs, rv3d->viewquat, rv3d->dist);

	BKE_object_tfm_protected_restore(v3d->camera, &obtfm, v3d->camera->protectflag);

	DEG_id_tag_update(&v3d->camera->id, OB_RECALC_OB);
	rv3d->persp = RV3D_CAMOB;

	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, v3d->camera);

	return OPERATOR_FINISHED;

}

static bool view3d_camera_to_view_poll(bContext *C)
{
	View3D *v3d;
	ARegion *ar;

	if (ED_view3d_context_user_region(C, &v3d, &ar)) {
		RegionView3D *rv3d = ar->regiondata;
		if (v3d && v3d->camera && !ID_IS_LINKED(v3d->camera)) {
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera Fit Frame to Selected Operator
 * \{ */

/* unlike VIEW3D_OT_view_selected this is for framing a render and not
 * meant to take into account vertex/bone selection for eg. */
static int view3d_camera_to_view_selected_exec(bContext *C, wmOperator *op)
{
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);  /* can be NULL */
	Object *camera_ob = v3d ? v3d->camera : scene->camera;
	Object *camera_ob_eval = DEG_get_evaluated_object(depsgraph, camera_ob);

	float r_co[3]; /* the new location to apply */
	float r_scale; /* only for ortho cameras */

	if (camera_ob_eval == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No active camera");
		return OPERATOR_CANCELLED;
	}

	/* this function does all the important stuff */
	if (BKE_camera_view_frame_fit_to_scene(depsgraph, scene, camera_ob_eval, r_co, &r_scale)) {
		ObjectTfmProtectedChannels obtfm;
		float obmat_new[4][4];

		if ((camera_ob_eval->type == OB_CAMERA) && (((Camera *)camera_ob_eval->data)->type == CAM_ORTHO)) {
			((Camera *)camera_ob->data)->ortho_scale = r_scale;
		}

		copy_m4_m4(obmat_new, camera_ob_eval->obmat);
		copy_v3_v3(obmat_new[3], r_co);

		/* only touch location */
		BKE_object_tfm_protected_backup(camera_ob, &obtfm);
		BKE_object_apply_mat4(camera_ob, obmat_new, true, true);
		BKE_object_tfm_protected_restore(camera_ob, &obtfm, OB_LOCK_SCALE | OB_LOCK_ROT4D);

		/* notifiers */
		DEG_id_tag_update(&camera_ob->id, OB_RECALC_OB);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object as Camera Operator
 * \{ */

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

bool ED_operator_rv3d_user_region_poll(bContext *C)
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window and View Matrix Calculation
 * \{ */

/**
 * \param rect optional for picking (can be NULL).
 */
void view3d_winmatrix_set(Depsgraph *depsgraph, ARegion *ar, const View3D *v3d, const rcti *rect)
{
	RegionView3D *rv3d = ar->regiondata;
	rctf viewplane;
	float clipsta, clipend;
	bool is_ortho;

	is_ortho = ED_view3d_viewplane_get(depsgraph, v3d, rv3d, ar->winx, ar->winy, &viewplane, &clipsta, &clipend, NULL);
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
		GPU_matrix_ortho_set(viewplane.xmin, viewplane.xmax, viewplane.ymin, viewplane.ymax, clipsta, clipend);
	}
	else {
		GPU_matrix_frustum_set(viewplane.xmin, viewplane.xmax, viewplane.ymin, viewplane.ymax, clipsta, clipend);
	}

	/* update matrix in 3d view region */
	GPU_matrix_projection_get(rv3d->winmat);
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

/**
 * Sets #RegionView3D.viewmat
 *
 * \param depsgraph: Depsgraph.
 * \param scene: Scene for camera and cursor location.
 * \param v3d: View 3D space data.
 * \param rv3d: 3D region which stores the final matrices.
 * \param rect_scale: Optional 2D scale argument,
 * Use when displaying a sub-region, eg: when #view3d_winmatrix_set takes a 'rect' argument.
 *
 * \note don't set windows active in here, is used by renderwin too.
 */
void view3d_viewmatrix_set(
        Depsgraph *depsgraph, Scene *scene,
        const View3D *v3d, RegionView3D *rv3d, const float rect_scale[2])
{
	if (rv3d->persp == RV3D_CAMOB) {      /* obs/camera */
		if (v3d->camera) {
			Object *ob_camera_eval = DEG_get_evaluated_object(depsgraph, v3d->camera);
			BKE_object_where_is_calc(depsgraph, scene, ob_camera_eval);
			obmat_to_viewmat(rv3d, ob_camera_eval);
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
			Object *ob_eval = DEG_get_evaluated_object(depsgraph, v3d->ob_centre);
			float vec[3];

			copy_v3_v3(vec, ob_eval->obmat[3]);
			if (ob_eval->type == OB_ARMATURE && v3d->ob_centre_bone[0]) {
				bPoseChannel *pchan = BKE_pose_channel_find_name(ob_eval->pose, v3d->ob_centre_bone);
				if (pchan) {
					copy_v3_v3(vec, pchan->pose_mat[3]);
					mul_m4_v3(ob_eval->obmat, vec);
				}
			}
			translate_m4(rv3d->viewmat, -vec[0], -vec[1], -vec[2]);
			use_lock_ofs = true;
		}
		else if (v3d->ob_centre_cursor) {
			float vec[3];
			copy_v3_v3(vec, ED_view3d_cursor3d_get(scene, (View3D *)v3d)->location);
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
				/* Since 'RegionView3D.winmat' has been calculated and this function doesn't take the 'ARegion'
				 * we don't know about the region size.
				 * Use 'rect_scale' when drawing a sub-region to apply 2D offset,
				 * scaled by the difference between the sub-region and the region size.
				 */
				vec[0] /= rect_scale[0];
				vec[1] /= rect_scale[1];
			}

			mul_mat3_m4_v3(persinv, vec);
			translate_m4(rv3d->viewmat, vec[0], vec[1], vec[2]);
		}
		/* end lock offset */
	}
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name OpenGL Select Utilities
 * \{ */

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

#ifndef WITH_OPENGL_LEGACY
struct DrawSelectLoopUserData {
	uint  pass;
	uint  hits;
	uint *buffer;
	uint  buffer_len;
	const rcti *rect;
	char gpu_select_mode;
};

static bool drw_select_loop_pass(eDRWSelectStage stage, void *user_data)
{
	bool continue_pass = false;
	struct DrawSelectLoopUserData *data = user_data;
	if (stage == DRW_SELECT_PASS_PRE) {
		GPU_select_begin(data->buffer, data->buffer_len, data->rect, data->gpu_select_mode, data->hits);
		/* always run POST after PRE. */
		continue_pass = true;
	}
	else if (stage == DRW_SELECT_PASS_POST) {
		int hits = GPU_select_end();
		if (data->pass == 0) {
			/* quirk of GPU_select_end, only take hits value from first call. */
			data->hits = hits;
		}
		if (data->gpu_select_mode == GPU_SELECT_NEAREST_FIRST_PASS) {
			data->gpu_select_mode = GPU_SELECT_NEAREST_SECOND_PASS;
			continue_pass = (hits > 0);
		}
		data->pass += 1;
	}
	else {
		BLI_assert(0);
	}
	return continue_pass;

}
#endif /* WITH_OPENGL_LEGACY */

/** Implement #VIEW3D_SELECT_FILTER_OBJECT_MODE_LOCK. */
static bool drw_select_filter_object_mode_lock(Object *ob, void *user_data)
{
	const Object *obact = user_data;
	return BKE_object_is_mode_compat(ob, obact->mode);
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
        eV3DSelectMode select_mode, eV3DSelectObjectFilter select_filter)
{
	struct bThemeState theme_state;
	Depsgraph *depsgraph = vc->depsgraph;
	Scene *scene = vc->scene;
	View3D *v3d = vc->v3d;
	ARegion *ar = vc->ar;
	rcti rect;
	int hits;
	const bool use_obedit_skip = (OBEDIT_FROM_VIEW_LAYER(vc->view_layer) != NULL) && (vc->obedit == NULL);
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

	struct {
		DRW_ObjectFilterFn fn;
		void *user_data;
	} object_filter = {NULL, NULL};
	switch (select_filter) {
		case VIEW3D_SELECT_FILTER_OBJECT_MODE_LOCK:
		{
			Object *obact = OBACT(vc->view_layer);
			if (obact && obact->mode != OB_MODE_OBJECT) {
				object_filter.fn = drw_select_filter_object_mode_lock;
				object_filter.user_data = obact;
			}
			break;
		}
		case VIEW3D_SELECT_FILTER_NOP:
			break;

	}

	/* Tools may request depth outside of regular drawing code. */
	UI_Theme_Store(&theme_state);
	UI_SetTheme(SPACE_VIEW3D, RGN_TYPE_WINDOW);

	/* Re-use cache (rect must be smaller then the cached)
	 * other context is assumed to be unchanged */
	if (GPU_select_is_cached()) {
		GPU_select_begin(buffer, bufsize, &rect, gpu_select_mode, 0);
		GPU_select_cache_load_id();
		hits = GPU_select_end();
		goto finally;
	}

#ifndef WITH_OPENGL_LEGACY
	/* All of the queries need to be perform on the drawing context. */
	DRW_opengl_context_enable();
#endif

	G.f |= G_PICKSEL;

	/* Important we use the 'viewmat' and don't re-calculate since
	 * the object & bone view locking takes 'rect' into account, see: T51629. */
	ED_view3d_draw_setup_view(vc->win, depsgraph, scene, ar, v3d, vc->rv3d->viewmat, NULL, &rect);

	if (v3d->shading.type > OB_WIRE) {
		GPU_depth_test(true);
	}

	if (vc->rv3d->rflag & RV3D_CLIPPING)
		ED_view3d_clipping_set(vc->rv3d);


#ifdef WITH_OPENGL_LEGACY
	if (IS_VIEWPORT_LEGACY(vc->v3d)) {
		GPU_select_begin(buffer, bufsize, &rect, gpu_select_mode, 0);
		ED_view3d_draw_select_loop(vc, scene, sl, v3d, ar, use_obedit_skip, use_nearest);
		hits = GPU_select_end();

		if (do_passes && (hits > 0)) {
			GPU_select_begin(buffer, bufsize, &rect, GPU_SELECT_NEAREST_SECOND_PASS, hits);
			ED_view3d_draw_select_loop(vc, scene, sl, v3d, ar, use_obedit_skip, use_nearest);
			GPU_select_end();
		}
	}
	else
#else
	{
		/* We need to call "GPU_select_*" API's inside DRW_draw_select_loop
		 * because the OpenGL context created & destroyed inside this function. */
		struct DrawSelectLoopUserData drw_select_loop_user_data = {
			.pass = 0,
			.hits = 0,
			.buffer = buffer,
			.buffer_len = bufsize,
			.rect = &rect,
			.gpu_select_mode = gpu_select_mode,
		};
		DRW_draw_select_loop(
		        depsgraph, ar, v3d,
		        use_obedit_skip, use_nearest, &rect,
		        drw_select_loop_pass, &drw_select_loop_user_data,
		        object_filter.fn, object_filter.user_data);
		hits = drw_select_loop_user_data.hits;
	}
#endif /* WITH_OPENGL_LEGACY */

	G.f &= ~G_PICKSEL;
	ED_view3d_draw_setup_view(vc->win, depsgraph, scene, ar, v3d, vc->rv3d->viewmat, NULL, NULL);

	if (v3d->shading.type > OB_WIRE) {
		GPU_depth_test(false);
	}

	if (vc->rv3d->rflag & RV3D_CLIPPING)
		ED_view3d_clipping_disable();

#ifndef WITH_OPENGL_LEGACY
	DRW_opengl_context_disable();
#endif

finally:

	if (hits < 0) printf("Too many objects in select buffer\n");  /* XXX make error message */

	UI_Theme_Restore(&theme_state);

	return hits;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Layer Utilities
 * \{ */

int ED_view3d_view_layer_set(int lay, const bool *values, int *active)
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

/** \} */
