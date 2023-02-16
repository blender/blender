/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "DNA_camera_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_context.h"

#include "DEG_depsgraph_query.h"

#include "WM_api.h"

#include "ED_screen.h"

#include "view3d_intern.h"
#include "view3d_navigate.h" /* own include */

static void view3d_smoothview_apply_with_interp(
    bContext *C, View3D *v3d, RegionView3D *rv3d, const bool use_autokey, const float factor);

/* -------------------------------------------------------------------- */
/** \name Smooth View Undo Handling
 *
 * When the camera is locked to the viewport smooth-view operations
 * may need to perform an undo push.
 *
 * In this case the smooth-view camera transformation is temporarily completed,
 * undo is pushed then the change is rewound, and smooth-view completes from it's timer.
 * In the case smooth-view executed the change immediately - an undo push is called.
 *
 * NOTE(@ideasman42): While this is not ideal it's necessary as making the undo-push
 * once smooth-view is complete because smooth-view is non-blocking and it's possible other
 * operations are executed once smooth-view has started.
 * \{ */

void ED_view3d_smooth_view_undo_begin(bContext *C, const ScrArea *area)
{
  const View3D *v3d = area->spacedata.first;
  Object *camera = v3d->camera;
  if (!camera) {
    return;
  }

  /* Tag the camera object so it's known smooth-view is applied to the view-ports camera
   * (needed to detect when a locked camera is being manipulated).
   * NOTE: It doesn't matter if the actual object being manipulated is the camera or not. */
  camera->id.tag &= ~LIB_TAG_DOIT;

  LISTBASE_FOREACH (const ARegion *, region, &area->regionbase) {
    if (region->regiontype != RGN_TYPE_WINDOW) {
      continue;
    }
    const RegionView3D *rv3d = region->regiondata;
    if (ED_view3d_camera_lock_undo_test(v3d, rv3d, C)) {
      camera->id.tag |= LIB_TAG_DOIT;
      break;
    }
  }
}

void ED_view3d_smooth_view_undo_end(bContext *C,
                                    const ScrArea *area,
                                    const char *undo_str,
                                    const bool undo_grouped)
{
  View3D *v3d = area->spacedata.first;
  Object *camera = v3d->camera;
  if (!camera) {
    return;
  }
  if (camera->id.tag & LIB_TAG_DOIT) {
    /* Smooth view didn't touch the camera. */
    camera->id.tag &= ~LIB_TAG_DOIT;
    return;
  }

  if ((U.uiflag & USER_GLOBALUNDO) == 0) {
    return;
  }

  /* NOTE(@ideasman42): It is not possible that a single viewport references different cameras
   * so even in the case there is a quad-view with multiple camera views set, these will all
   * reference the same camera. In this case it doesn't matter which region is used.
   * If in the future multiple cameras are supported, this logic can be extended. */
  const ARegion *region_camera = NULL;

  /* An undo push should be performed. */
  bool is_interactive = false;
  LISTBASE_FOREACH (const ARegion *, region, &area->regionbase) {
    if (region->regiontype != RGN_TYPE_WINDOW) {
      continue;
    }
    const RegionView3D *rv3d = region->regiondata;
    if (ED_view3d_camera_lock_undo_test(v3d, rv3d, C)) {
      region_camera = region;
      if (rv3d->sms) {
        is_interactive = true;
      }
    }
  }

  if (region_camera == NULL) {
    return;
  }

  RegionView3D *rv3d = region_camera->regiondata;

  /* Fast forward, undo push, then rewind. */
  if (is_interactive) {
    view3d_smoothview_apply_with_interp(C, v3d, rv3d, false, 1.0f);
  }

  if (undo_grouped) {
    ED_view3d_camera_lock_undo_grouped_push(undo_str, v3d, rv3d, C);
  }
  else {
    ED_view3d_camera_lock_undo_push(undo_str, v3d, rv3d, C);
  }

  if (is_interactive) {
    view3d_smoothview_apply_with_interp(C, v3d, rv3d, false, 0.0f);
  }
}

/** \} */

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
  /* Source. */
  struct SmoothView3DState src; /* source */
  struct SmoothView3DState dst; /* destination */
  struct SmoothView3DState org; /* original */

  bool to_camera;

  bool use_dyn_ofs;
  float dyn_ofs[3];

  /* When smooth-view is enabled, store the 'rv3d->view' here,
   * assign back when the view motion is completed. */
  char org_view;

  double time_allowed;
};

static void view3d_smooth_view_state_backup(struct SmoothView3DState *sms_state,
                                            const View3D *v3d,
                                            const RegionView3D *rv3d)
{
  copy_v3_v3(sms_state->ofs, rv3d->ofs);
  copy_qt_qt(sms_state->quat, rv3d->viewquat);
  sms_state->dist = rv3d->dist;
  sms_state->lens = v3d->lens;
}

static void view3d_smooth_view_state_restore(const struct SmoothView3DState *sms_state,
                                             View3D *v3d,
                                             RegionView3D *rv3d)
{
  copy_v3_v3(rv3d->ofs, sms_state->ofs);
  copy_qt_qt(rv3d->viewquat, sms_state->quat);
  rv3d->dist = sms_state->dist;
  v3d->lens = sms_state->lens;
}

/* will start timer if appropriate */
void ED_view3d_smooth_view_ex(
    /* avoid passing in the context */
    const Depsgraph *depsgraph,
    wmWindowManager *wm,
    wmWindow *win,
    ScrArea *area,
    View3D *v3d,
    ARegion *region,
    const int smooth_viewtx,
    const V3D_SmoothParams *sview)
{
  /* In this case use #ED_view3d_smooth_view_undo_begin & end functions
   * instead of passing in undo. */
  BLI_assert_msg(sview->undo_str == NULL,
                 "Only the 'ED_view3d_smooth_view' version of this function handles undo!");

  RegionView3D *rv3d = region->regiondata;
  struct SmoothView3DStore sms = {{0}};

  /* initialize sms */
  view3d_smooth_view_state_backup(&sms.dst, v3d, rv3d);
  view3d_smooth_view_state_backup(&sms.src, v3d, rv3d);
  /* If smooth-view runs multiple times. */
  if (rv3d->sms == NULL) {
    view3d_smooth_view_state_backup(&sms.org, v3d, rv3d);
  }
  else {
    sms.org = rv3d->sms->org;
  }
  sms.org_view = rv3d->view;

  /* sms.to_camera = false; */ /* initialized to zero anyway */

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
  if (sview->ofs) {
    copy_v3_v3(sms.dst.ofs, sview->ofs);
  }
  if (sview->quat) {
    copy_qt_qt(sms.dst.quat, sview->quat);
  }
  if (sview->dist) {
    sms.dst.dist = *sview->dist;
  }
  if (sview->lens) {
    sms.dst.lens = *sview->lens;
  }

  if (sview->dyn_ofs) {
    BLI_assert(sview->ofs == NULL);
    BLI_assert(sview->quat != NULL);

    copy_v3_v3(sms.dyn_ofs, sview->dyn_ofs);
    sms.use_dyn_ofs = true;

    /* calculate the final destination offset */
    view3d_orbit_apply_dyn_ofs(sms.dst.ofs, sms.src.ofs, sms.src.quat, sms.dst.quat, sms.dyn_ofs);
  }

  if (sview->camera) {
    Object *ob_camera_eval = DEG_get_evaluated_object(depsgraph, sview->camera);
    if (sview->ofs != NULL) {
      sms.dst.dist = ED_view3d_offset_distance(
          ob_camera_eval->object_to_world, sview->ofs, VIEW3D_DIST_FALLBACK);
    }
    ED_view3d_from_object(ob_camera_eval, sms.dst.ofs, sms.dst.quat, &sms.dst.dist, &sms.dst.lens);
    sms.to_camera = true; /* restore view3d values in end */
  }

  if ((sview->camera_old == sview->camera) &&   /* Camera. */
      (sms.dst.dist == rv3d->dist) &&           /* Distance. */
      (sms.dst.lens == v3d->lens) &&            /* Lens. */
      equals_v3v3(sms.dst.ofs, rv3d->ofs) &&    /* Offset. */
      equals_v4v4(sms.dst.quat, rv3d->viewquat) /* Rotation. */
  ) {
    /* Early return if nothing changed. */
    return;
  }

  /* Skip smooth viewing for external render engine draw. */
  if (smooth_viewtx && !(v3d->shading.type == OB_RENDER && rv3d->render_engine)) {

    /* original values */
    if (sview->camera_old) {
      Object *ob_camera_old_eval = DEG_get_evaluated_object(depsgraph, sview->camera_old);
      if (sview->ofs != NULL) {
        sms.src.dist = ED_view3d_offset_distance(
            ob_camera_old_eval->object_to_world, sview->ofs, 0.0f);
      }
      ED_view3d_from_object(
          ob_camera_old_eval, sms.src.ofs, sms.src.quat, &sms.src.dist, &sms.src.lens);
    }
    /* grid draw as floor */
    if ((RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ROTATION) == 0) {
      /* use existing if exists, means multiple calls to smooth view
       * won't lose the original 'view' setting */
      rv3d->view = RV3D_VIEW_USER;
    }

    sms.time_allowed = (double)smooth_viewtx / 1000.0;

    /* If this is view rotation only we can decrease the time allowed by the angle between quats
     * this means small rotations won't lag. */
    if (sview->quat && !sview->ofs && !sview->dist) {
      /* scale the time allowed by the rotation */
      /* 180deg == 1.0 */
      sms.time_allowed *= (double)fabsf(angle_signed_normalized_qtqt(sms.dst.quat, sms.src.quat)) /
                          M_PI;
    }

    /* ensure it shows correct */
    if (sms.to_camera) {
      /* use ortho if we move from an ortho view to an ortho camera */
      Object *ob_camera_eval = DEG_get_evaluated_object(depsgraph, sview->camera);
      rv3d->persp = (((rv3d->is_persp == false) && (ob_camera_eval->type == OB_CAMERA) &&
                      (((Camera *)ob_camera_eval->data)->type == CAM_ORTHO)) ?
                         RV3D_ORTHO :
                         RV3D_PERSP);
    }

    rv3d->rflag |= RV3D_NAVIGATING;

    /* not essential but in some cases the caller will tag the area for redraw, and in that
     * case we can get a flicker of the 'org' user view but we want to see 'src' */
    view3d_smooth_view_state_restore(&sms.src, v3d, rv3d);

    /* keep track of running timer! */
    if (rv3d->sms == NULL) {
      rv3d->sms = MEM_mallocN(sizeof(struct SmoothView3DStore), "smoothview v3d");
    }
    *rv3d->sms = sms;
    if (rv3d->smooth_timer) {
      WM_event_remove_timer(wm, win, rv3d->smooth_timer);
    }
    /* #TIMER1 is hard-coded in key-map. */
    rv3d->smooth_timer = WM_event_add_timer(wm, win, TIMER1, 1.0 / 100.0);
  }
  else {
    /* Animation is disabled, apply immediately. */
    if (sms.to_camera == false) {
      copy_v3_v3(rv3d->ofs, sms.dst.ofs);
      copy_qt_qt(rv3d->viewquat, sms.dst.quat);
      rv3d->dist = sms.dst.dist;
      v3d->lens = sms.dst.lens;

      ED_view3d_camera_lock_sync(depsgraph, v3d, rv3d);
    }

    if (RV3D_LOCK_FLAGS(rv3d) & RV3D_BOXVIEW) {
      view3d_boxview_copy(area, region);
    }

    ED_region_tag_redraw(region);

    WM_event_add_mousemove(win);
  }

  if (sms.to_camera == false) {
    /* See comments in #ED_view3d_smooth_view_undo_begin for why this is needed. */
    if (v3d->camera) {
      v3d->camera->id.tag &= ~LIB_TAG_DOIT;
    }
  }
}

void ED_view3d_smooth_view(bContext *C,
                           View3D *v3d,
                           ARegion *region,
                           const int smooth_viewtx,
                           const struct V3D_SmoothParams *sview)
{
  const Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);

  /* #ED_view3d_smooth_view_ex asserts this is not set as it doesn't support undo. */
  struct V3D_SmoothParams sview_no_undo = *sview;
  sview_no_undo.undo_str = NULL;
  sview_no_undo.undo_grouped = false;

  const bool do_undo = (sview->undo_str != NULL);
  if (do_undo) {
    ED_view3d_smooth_view_undo_begin(C, area);
  }

  ED_view3d_smooth_view_ex(depsgraph, wm, win, area, v3d, region, smooth_viewtx, &sview_no_undo);

  if (do_undo) {
    ED_view3d_smooth_view_undo_end(C, area, sview->undo_str, sview->undo_grouped);
  }
}

/**
 * Apply with interpolation, on completion run #view3d_smoothview_apply_and_finish.
 */
static void view3d_smoothview_apply_with_interp(
    bContext *C, View3D *v3d, RegionView3D *rv3d, const bool use_autokey, const float factor)
{
  struct SmoothView3DStore *sms = rv3d->sms;

  interp_qt_qtqt(rv3d->viewquat, sms->src.quat, sms->dst.quat, factor);

  if (sms->use_dyn_ofs) {
    view3d_orbit_apply_dyn_ofs(
        rv3d->ofs, sms->src.ofs, sms->src.quat, rv3d->viewquat, sms->dyn_ofs);
  }
  else {
    interp_v3_v3v3(rv3d->ofs, sms->src.ofs, sms->dst.ofs, factor);
  }

  rv3d->dist = interpf(sms->dst.dist, sms->src.dist, factor);
  v3d->lens = interpf(sms->dst.lens, sms->src.lens, factor);

  const Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  if (ED_view3d_camera_lock_sync(depsgraph, v3d, rv3d)) {
    if (use_autokey) {
      ED_view3d_camera_lock_autokey(v3d, rv3d, C, true, true);
    }
  }
}

/**
 * Apply the view-port transformation & free smooth-view related data.
 */
static void view3d_smoothview_apply_and_finish(bContext *C, View3D *v3d, RegionView3D *rv3d)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  struct SmoothView3DStore *sms = rv3d->sms;

  wmWindow *win = CTX_wm_window(C);

  /* if we went to camera, store the original */
  if (sms->to_camera) {
    rv3d->persp = RV3D_CAMOB;
    view3d_smooth_view_state_restore(&sms->org, v3d, rv3d);
  }
  else {
    const Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

    view3d_smooth_view_state_restore(&sms->dst, v3d, rv3d);

    if (ED_view3d_camera_lock_sync(depsgraph, v3d, rv3d)) {
      ED_view3d_camera_lock_autokey(v3d, rv3d, C, true, true);
    }
  }

  if ((RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ROTATION) == 0) {
    rv3d->view = sms->org_view;
  }

  MEM_freeN(rv3d->sms);
  rv3d->sms = NULL;

  WM_event_remove_timer(wm, win, rv3d->smooth_timer);
  rv3d->smooth_timer = NULL;
  rv3d->rflag &= ~RV3D_NAVIGATING;

  /* Event handling won't know if a UI item has been moved under the pointer. */
  WM_event_add_mousemove(win);

  /* NOTE: this doesn't work right because the v3d->lens is now used in ortho mode r51636,
   * when switching camera in quad-view the other ortho views would zoom & reset.
   *
   * For now only redraw all regions when smooth-view finishes.
   */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
}

/* only meant for timer usage */

static void view3d_smoothview_apply_from_timer(bContext *C, View3D *v3d, ARegion *region)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  RegionView3D *rv3d = region->regiondata;
  struct SmoothView3DStore *sms = rv3d->sms;
  float factor;

  if (sms->time_allowed != 0.0) {
    factor = (float)((rv3d->smooth_timer->duration) / sms->time_allowed);
  }
  else {
    factor = 1.0f;
  }
  if (factor >= 1.0f) {
    view3d_smoothview_apply_and_finish(C, v3d, rv3d);
  }
  else {
    /* Ease in/out smoothing. */
    factor = (3.0f * factor * factor - 2.0f * factor * factor * factor);
    const bool use_autokey = ED_screen_animation_playing(wm);
    view3d_smoothview_apply_with_interp(C, v3d, rv3d, use_autokey, factor);
  }

  if (RV3D_LOCK_FLAGS(rv3d) & RV3D_BOXVIEW) {
    view3d_boxview_copy(CTX_wm_area(C), region);
  }

  ED_region_tag_redraw(region);
}

static int view3d_smoothview_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  View3D *v3d = CTX_wm_view3d(C);
  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = region->regiondata;

  /* escape if not our timer */
  if (rv3d->smooth_timer == NULL || rv3d->smooth_timer != event->customdata) {
    return OPERATOR_PASS_THROUGH;
  }

  view3d_smoothview_apply_from_timer(C, v3d, region);

  return OPERATOR_FINISHED;
}

void ED_view3d_smooth_view_force_finish(bContext *C, View3D *v3d, ARegion *region)
{
  RegionView3D *rv3d = region->regiondata;
  if (rv3d && rv3d->sms) {
    view3d_smoothview_apply_and_finish(C, v3d, rv3d);

    /* Force update of view matrix so tools that run immediately after
     * can use them without redrawing first */
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    Scene *scene = CTX_data_scene(C);
    ED_view3d_update_viewmat(depsgraph, scene, v3d, region, NULL, NULL, NULL, false);
  }
}

void VIEW3D_OT_smoothview(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Smooth View";
  ot->idname = "VIEW3D_OT_smoothview";

  /* api callbacks */
  ot->invoke = view3d_smoothview_invoke;

  /* flags */
  ot->flag = OPTYPE_INTERNAL;

  ot->poll = ED_operator_view3d_active;
}

/** \} */
