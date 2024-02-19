/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "WM_api.hh"

#include "ED_screen.hh"

#include "view3d_intern.h"
#include "view3d_navigate.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name NDOF Utility Functions
 * \{ */

#ifdef WITH_INPUT_NDOF

enum {
  HAS_TRANSLATE = (1 << 0),
  HAS_ROTATE = (1 << 0),
};

static bool ndof_has_translate(const wmNDOFMotionData *ndof,
                               const View3D *v3d,
                               const RegionView3D *rv3d)
{
  return !is_zero_v3(ndof->tvec) && !ED_view3d_offset_lock_check(v3d, rv3d);
}

static bool ndof_has_rotate(const wmNDOFMotionData *ndof, const RegionView3D *rv3d)
{
  return !is_zero_v3(ndof->rvec) && ((RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ROTATION) == 0);
}

/**
 * \param depth_pt: A point to calculate the depth (in perspective mode)
 */
static float view3d_ndof_pan_speed_calc_ex(RegionView3D *rv3d, const float depth_pt[3])
{
  float speed = rv3d->pixsize * NDOF_PIXELS_PER_SECOND;

  if (rv3d->is_persp) {
    speed *= ED_view3d_calc_zfac(rv3d, depth_pt);
  }

  return speed;
}

static float view3d_ndof_pan_speed_calc_from_dist(RegionView3D *rv3d, const float dist)
{
  float viewinv[4];
  float tvec[3];

  BLI_assert(dist >= 0.0f);

  copy_v3_fl3(tvec, 0.0f, 0.0f, dist);
  /* rv3d->viewinv isn't always valid */
#  if 0
  mul_mat3_m4_v3(rv3d->viewinv, tvec);
#  else
  invert_qt_qt_normalized(viewinv, rv3d->viewquat);
  mul_qt_v3(viewinv, tvec);
#  endif

  return view3d_ndof_pan_speed_calc_ex(rv3d, tvec);
}

static float view3d_ndof_pan_speed_calc(RegionView3D *rv3d)
{
  float tvec[3];
  negate_v3_v3(tvec, rv3d->ofs);

  return view3d_ndof_pan_speed_calc_ex(rv3d, tvec);
}

/**
 * Zoom and pan in the same function since sometimes zoom is interpreted as dolly (pan forward).
 *
 * \param has_zoom: zoom, otherwise dolly,
 * often `!rv3d->is_persp` since it doesn't make sense to dolly in ortho.
 */
static void view3d_ndof_pan_zoom(const wmNDOFMotionData *ndof,
                                 ScrArea *area,
                                 ARegion *region,
                                 const bool has_translate,
                                 const bool has_zoom)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  float view_inv[4];
  float pan_vec[3];

  if (has_translate == false && has_zoom == false) {
    return;
  }

  WM_event_ndof_pan_get(ndof, pan_vec, false);

  if (has_zoom) {
    /* zoom with Z */

    /* Zoom!
     * velocity should be proportional to the linear velocity attained by rotational motion
     * of same strength [got that?] proportional to `arclength = radius * angle`.
     */

    pan_vec[2] = 0.0f;

    /* "zoom in" or "translate"? depends on zoom mode in user settings? */
    if (ndof->tvec[2]) {
      float zoom_distance = rv3d->dist * ndof->dt * ndof->tvec[2];

      if (U.ndof_flag & NDOF_ZOOM_INVERT) {
        zoom_distance = -zoom_distance;
      }

      rv3d->dist += zoom_distance;
    }
  }
  else {
    /* dolly with Z */

    /* all callers must check */
    if (has_translate) {
      BLI_assert(ED_view3d_offset_lock_check((View3D *)area->spacedata.first, rv3d) == false);
    }
  }

  if (has_translate) {
    const float speed = view3d_ndof_pan_speed_calc(rv3d);

    mul_v3_fl(pan_vec, speed * ndof->dt);

    /* transform motion from view to world coordinates */
    invert_qt_qt_normalized(view_inv, rv3d->viewquat);
    mul_qt_v3(view_inv, pan_vec);

    /* move center of view opposite of hand motion (this is camera mode, not object mode) */
    sub_v3_v3(rv3d->ofs, pan_vec);

    if (RV3D_LOCK_FLAGS(rv3d) & RV3D_BOXVIEW) {
      view3d_boxview_sync(area, region);
    }
  }
}

static void view3d_ndof_orbit(const wmNDOFMotionData *ndof,
                              ScrArea *area,
                              ARegion *region,
                              ViewOpsData *vod,
                              const bool apply_dyn_ofs)
{
  View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  float view_inv[4];

  BLI_assert((RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ROTATION) == 0);

  ED_view3d_persp_ensure(vod->depsgraph, v3d, region);

  rv3d->view = RV3D_VIEW_USER;

  invert_qt_qt_normalized(view_inv, rv3d->viewquat);

  if (U.ndof_flag & NDOF_TURNTABLE) {
    float rot[3];

    /* Turntable view code adapted for 3D mouse use. */
    float angle, quat[4];
    float xvec[3] = {1, 0, 0};

    /* only use XY, ignore Z */
    WM_event_ndof_rotate_get(ndof, rot);

    /* Determine the direction of the x vector (for rotating up and down) */
    mul_qt_v3(view_inv, xvec);

    /* Perform the up/down rotation */
    angle = ndof->dt * rot[0];
    axis_angle_to_quat(quat, xvec, angle);
    mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, quat);

    /* Perform the orbital rotation */
    angle = ndof->dt * rot[1];

    /* Update the onscreen axis-angle indicator. */
    rv3d->rot_angle = angle;
    rv3d->rot_axis[0] = 0;
    rv3d->rot_axis[1] = 0;
    rv3d->rot_axis[2] = 1;

    axis_angle_to_quat_single(quat, 'Z', angle);
    mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, quat);
  }
  else {
    float quat[4];
    float axis[3];
    float angle = WM_event_ndof_to_axis_angle(ndof, axis);

    /* transform rotation axis from view to world coordinates */
    mul_qt_v3(view_inv, axis);

    /* Update the onscreen axis-angle indicator. */
    rv3d->rot_angle = angle;
    copy_v3_v3(rv3d->rot_axis, axis);

    axis_angle_to_quat(quat, axis, angle);

    /* apply rotation */
    mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, quat);
  }

  if (apply_dyn_ofs) {
    viewrotate_apply_dyn_ofs(vod, rv3d->viewquat);
  }
}

void view3d_ndof_fly(const wmNDOFMotionData *ndof,
                     View3D *v3d,
                     RegionView3D *rv3d,
                     const bool use_precision,
                     const short protectflag,
                     bool *r_has_translate,
                     bool *r_has_rotate)
{
  bool has_translate = ndof_has_translate(ndof, v3d, rv3d);
  bool has_rotate = ndof_has_rotate(ndof, rv3d);

  float view_inv[4];
  invert_qt_qt_normalized(view_inv, rv3d->viewquat);

  rv3d->rot_angle = 0.0f; /* Disable onscreen rotation indicator. */

  if (has_translate) {
    /* ignore real 'dist' since fly has its own speed settings,
     * also its overwritten at this point. */
    float speed = view3d_ndof_pan_speed_calc_from_dist(rv3d, 1.0f);
    float trans[3], trans_orig_y;

    if (use_precision) {
      speed *= 0.2f;
    }

    WM_event_ndof_pan_get(ndof, trans, false);
    mul_v3_fl(trans, speed * ndof->dt);
    trans_orig_y = trans[1];

    if (U.ndof_flag & NDOF_FLY_HELICOPTER) {
      trans[1] = 0.0f;
    }

    /* transform motion from view to world coordinates */
    mul_qt_v3(view_inv, trans);

    if (U.ndof_flag & NDOF_FLY_HELICOPTER) {
      /* replace world z component with device y (yes it makes sense) */
      trans[2] = trans_orig_y;
    }

    if (rv3d->persp == RV3D_CAMOB) {
      /* respect camera position locks */
      if (protectflag & OB_LOCK_LOCX) {
        trans[0] = 0.0f;
      }
      if (protectflag & OB_LOCK_LOCY) {
        trans[1] = 0.0f;
      }
      if (protectflag & OB_LOCK_LOCZ) {
        trans[2] = 0.0f;
      }
    }

    if (!is_zero_v3(trans)) {
      /* move center of view opposite of hand motion
       * (this is camera mode, not object mode) */
      sub_v3_v3(rv3d->ofs, trans);
      has_translate = true;
    }
    else {
      has_translate = false;
    }
  }

  if (has_rotate) {
    const float turn_sensitivity = 1.0f;

    float rotation[4];
    float axis[3];
    float angle = turn_sensitivity * WM_event_ndof_to_axis_angle(ndof, axis);

    if (fabsf(angle) > 0.0001f) {
      has_rotate = true;

      if (use_precision) {
        angle *= 0.2f;
      }

      /* transform rotation axis from view to world coordinates */
      mul_qt_v3(view_inv, axis);

      /* apply rotation to view */
      axis_angle_to_quat(rotation, axis, angle);
      mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, rotation);

      if (U.ndof_flag & NDOF_LOCK_HORIZON) {
        /* force an upright viewpoint
         * TODO: make this less... sudden */
        float view_horizon[3] = {1.0f, 0.0f, 0.0f};    /* view +x */
        float view_direction[3] = {0.0f, 0.0f, -1.0f}; /* view -z (into screen) */

        /* find new inverse since viewquat has changed */
        invert_qt_qt_normalized(view_inv, rv3d->viewquat);
        /* could apply reverse rotation to existing view_inv to save a few cycles */

        /* transform view vectors to world coordinates */
        mul_qt_v3(view_inv, view_horizon);
        mul_qt_v3(view_inv, view_direction);

        /* find difference between view & world horizons
         * true horizon lives in world xy plane, so look only at difference in z */
        angle = -asinf(view_horizon[2]);

        /* rotate view so view horizon = world horizon */
        axis_angle_to_quat(rotation, view_direction, angle);
        mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, rotation);
      }

      rv3d->view = RV3D_VIEW_USER;
    }
    else {
      has_rotate = false;
    }
  }

  *r_has_translate = has_translate;
  *r_has_rotate = has_rotate;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Camera View Support
 * \{ */

/**
 * 2D orthographic style NDOF navigation within the camera view.
 * Support navigating the camera view instead of leaving the camera-view and navigating in 3D.
 */
static int view3d_ndof_cameraview_pan_zoom(ViewOpsData *vod, const wmNDOFMotionData *ndof)
{
  View3D *v3d = vod->v3d;
  ARegion *region = vod->region;
  RegionView3D *rv3d = vod->rv3d;

  if (v3d->camera && (rv3d->persp == RV3D_CAMOB) && (v3d->flag2 & V3D_LOCK_CAMERA) == 0) {
    /* pass */
  }
  else {
    return OPERATOR_PASS_THROUGH;
  }

  const float pan_speed = NDOF_PIXELS_PER_SECOND;
  const bool has_translate = !is_zero_v2(ndof->tvec);
  const bool has_zoom = ndof->tvec[2] != 0.0f;

  float pan_vec[3];
  WM_event_ndof_pan_get(ndof, pan_vec, true);

  mul_v3_fl(pan_vec, ndof->dt);
  /* NOTE: unlike image and clip views, the 2D pan doesn't have to be scaled by the zoom level.
   * #ED_view3d_camera_view_pan already takes the zoom level into account. */
  mul_v2_fl(pan_vec, pan_speed);

  /* NOTE(@ideasman42): In principle rotating could pass through to regular
   * non-camera NDOF behavior (exiting the camera-view and rotating).
   * Disabled this block since in practice it's difficult to control NDOF devices
   * to perform some rotation with absolutely no translation. Causing rotation to
   * randomly exit from the user perspective. Adjusting the dead-zone could avoid
   * the motion feeling *glitchy* although in my own tests even then it didn't work reliably.
   * Leave rotating out of camera-view disabled unless it can be made to work reliably. */
  if (!(has_translate || has_zoom)) {
    // return OPERATOR_PASS_THROUGH;
  }

  bool changed = false;

  if (has_translate) {
    /* Use the X & Y of `pan_vec`. */
    if (ED_view3d_camera_view_pan(region, pan_vec)) {
      changed = true;
    }
  }

  if (has_zoom) {
    if (ED_view3d_camera_view_zoom_scale(rv3d, max_ff(0.0f, 1.0f - pan_vec[2]))) {
      changed = true;
    }
  }

  if (changed) {
    ED_region_tag_redraw(region);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Orbit/Translate Operator
 * \{ */

static int ndof_orbit_invoke_impl(bContext *C,
                                  ViewOpsData *vod,
                                  const wmEvent *event,
                                  PointerRNA * /*ptr*/)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }

  const Depsgraph *depsgraph = vod->depsgraph;
  View3D *v3d = vod->v3d;
  RegionView3D *rv3d = vod->rv3d;
  char xform_flag = 0;

  const wmNDOFMotionData *ndof = static_cast<const wmNDOFMotionData *>(event->customdata);

  /* off by default, until changed later this function */
  rv3d->rot_angle = 0.0f;

  if (ndof->progress != P_FINISHING) {
    const bool has_rotation = ndof_has_rotate(ndof, rv3d);
    /* if we can't rotate, fallback to translate (locked axis views) */
    const bool has_translate = ndof_has_translate(ndof, v3d, rv3d) &&
                               (RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ROTATION);
    const bool has_zoom = (ndof->tvec[2] != 0.0f) && !rv3d->is_persp;

    if (has_translate || has_zoom) {
      view3d_ndof_pan_zoom(ndof, vod->area, vod->region, has_translate, has_zoom);
      xform_flag |= HAS_TRANSLATE;
    }

    if (has_rotation) {
      view3d_ndof_orbit(ndof, vod->area, vod->region, vod, true);
      xform_flag |= HAS_ROTATE;
    }
  }

  ED_view3d_camera_lock_sync(depsgraph, v3d, rv3d);
  if (xform_flag) {
    ED_view3d_camera_lock_autokey(
        v3d, rv3d, C, xform_flag & HAS_ROTATE, xform_flag & HAS_TRANSLATE);
  }

  ED_region_tag_redraw(vod->region);

  return OPERATOR_FINISHED;
}

static int ndof_orbit_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }

  return view3d_navigate_invoke_impl(C, op, event, &ViewOpsType_ndof_orbit);
}

void VIEW3D_OT_ndof_orbit(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "NDOF Orbit View";
  ot->description = "Orbit the view using the 3D mouse";
  ot->idname = ViewOpsType_ndof_orbit.idname;

  /* api callbacks */
  ot->invoke = ndof_orbit_invoke;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Orbit/Zoom Operator
 * \{ */

static int ndof_orbit_zoom_invoke_impl(bContext *C,
                                       ViewOpsData *vod,
                                       const wmEvent *event,
                                       PointerRNA * /*ptr*/)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }

  const wmNDOFMotionData *ndof = static_cast<const wmNDOFMotionData *>(event->customdata);

  if (U.ndof_flag & NDOF_CAMERA_PAN_ZOOM) {
    const int camera_retval = view3d_ndof_cameraview_pan_zoom(vod, ndof);
    if (camera_retval != OPERATOR_PASS_THROUGH) {
      return camera_retval;
    }
  }

  View3D *v3d = vod->v3d;
  RegionView3D *rv3d = vod->rv3d;
  char xform_flag = 0;

  /* off by default, until changed later this function */
  rv3d->rot_angle = 0.0f;

  if (ndof->progress == P_FINISHING) {
    /* pass */
  }
  else if ((rv3d->persp == RV3D_ORTHO) && RV3D_VIEW_IS_AXIS(rv3d->view)) {
    /* if we can't rotate, fallback to translate (locked axis views) */
    const bool has_translate = ndof_has_translate(ndof, v3d, rv3d);
    const bool has_zoom = (ndof->tvec[2] != 0.0f) && ED_view3d_offset_lock_check(v3d, rv3d);

    if (has_translate || has_zoom) {
      view3d_ndof_pan_zoom(ndof, vod->area, vod->region, has_translate, true);
      xform_flag |= HAS_TRANSLATE;
    }
  }
  else {
    /* NOTE: based on feedback from #67579, users want to have pan and orbit enabled at once.
     * It's arguable that orbit shouldn't pan (since we have a pan only operator),
     * so if there are users who like to separate orbit/pan operations - it can be a preference. */
    const bool is_orbit_around_pivot = (U.ndof_flag & NDOF_MODE_ORBIT) ||
                                       ED_view3d_offset_lock_check(v3d, rv3d);
    const bool has_rotation = ndof_has_rotate(ndof, rv3d);
    bool has_translate, has_zoom;

    if (is_orbit_around_pivot) {
      /* Orbit preference or forced lock (Z zooms). */
      has_translate = !is_zero_v2(ndof->tvec) && ndof_has_translate(ndof, v3d, rv3d);
      has_zoom = (ndof->tvec[2] != 0.0f);
    }
    else {
      /* Free preference (Z translates). */
      has_translate = ndof_has_translate(ndof, v3d, rv3d);
      has_zoom = false;
    }

    /* Rotation first because dynamic offset resets offset otherwise (and disables panning). */
    if (has_rotation) {
      const float dist_backup = rv3d->dist;
      if (!is_orbit_around_pivot) {
        ED_view3d_distance_set(rv3d, 0.0f);
      }
      view3d_ndof_orbit(ndof, vod->area, vod->region, vod, is_orbit_around_pivot);
      xform_flag |= HAS_ROTATE;
      if (!is_orbit_around_pivot) {
        ED_view3d_distance_set(rv3d, dist_backup);
      }
    }

    if (has_translate || has_zoom) {
      view3d_ndof_pan_zoom(ndof, vod->area, vod->region, has_translate, has_zoom);
      xform_flag |= HAS_TRANSLATE;
    }
  }

  ED_view3d_camera_lock_sync(vod->depsgraph, v3d, rv3d);
  if (xform_flag) {
    ED_view3d_camera_lock_autokey(
        v3d, rv3d, C, xform_flag & HAS_ROTATE, xform_flag & HAS_TRANSLATE);
  }

  ED_region_tag_redraw(vod->region);

  return OPERATOR_FINISHED;
}

static int ndof_orbit_zoom_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }

  return view3d_navigate_invoke_impl(C, op, event, &ViewOpsType_ndof_orbit_zoom);
}

void VIEW3D_OT_ndof_orbit_zoom(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "NDOF Orbit View with Zoom";
  ot->description = "Orbit and zoom the view using the 3D mouse";
  ot->idname = ViewOpsType_ndof_orbit_zoom.idname;

  /* api callbacks */
  ot->invoke = ndof_orbit_zoom_invoke;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Pan/Zoom Operator
 * \{ */

static int ndof_pan_invoke_impl(bContext *C,
                                ViewOpsData *vod,
                                const wmEvent *event,
                                PointerRNA * /*ptr*/)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }

  const wmNDOFMotionData *ndof = static_cast<const wmNDOFMotionData *>(event->customdata);

  if (U.ndof_flag & NDOF_CAMERA_PAN_ZOOM) {
    const int camera_retval = view3d_ndof_cameraview_pan_zoom(vod, ndof);
    if (camera_retval != OPERATOR_PASS_THROUGH) {
      return camera_retval;
    }
  }

  const Depsgraph *depsgraph = vod->depsgraph;
  View3D *v3d = vod->v3d;
  RegionView3D *rv3d = vod->rv3d;
  ARegion *region = vod->region;
  char xform_flag = 0;

  const bool has_translate = ndof_has_translate(ndof, v3d, rv3d);
  const bool has_zoom = (ndof->tvec[2] != 0.0f) && !rv3d->is_persp;

  /* we're panning here! so erase any leftover rotation from other operators */
  rv3d->rot_angle = 0.0f;

  if (!(has_translate || has_zoom)) {
    return OPERATOR_CANCELLED;
  }

  ED_view3d_camera_lock_init_ex(depsgraph, v3d, rv3d, false);

  if (ndof->progress != P_FINISHING) {
    ScrArea *area = vod->area;

    if (has_translate || has_zoom) {
      view3d_ndof_pan_zoom(ndof, area, region, has_translate, has_zoom);
      xform_flag |= HAS_TRANSLATE;
    }
  }

  ED_view3d_camera_lock_sync(depsgraph, v3d, rv3d);
  if (xform_flag) {
    ED_view3d_camera_lock_autokey(v3d, rv3d, C, false, xform_flag & HAS_TRANSLATE);
  }

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

static int ndof_pan_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }

  return view3d_navigate_invoke_impl(C, op, event, &ViewOpsType_ndof_pan);
}

void VIEW3D_OT_ndof_pan(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "NDOF Pan View";
  ot->description = "Pan the view with the 3D mouse";
  ot->idname = ViewOpsType_ndof_pan.idname;

  /* api callbacks */
  ot->invoke = ndof_pan_invoke;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Transform All Operator
 * \{ */

/**
 * wraps #ndof_orbit_zoom but never restrict to orbit.
 */
static int ndof_all_invoke_impl(bContext *C,
                                ViewOpsData *vod,
                                const wmEvent *event,
                                PointerRNA * /*ptr*/)
{
  /* weak!, but it works */
  const int ndof_flag = U.ndof_flag;
  int ret;

  U.ndof_flag &= ~NDOF_MODE_ORBIT;

  ret = ndof_orbit_zoom_invoke_impl(C, vod, event, nullptr);

  U.ndof_flag = ndof_flag;

  return ret;
}

static int ndof_all_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }

  return view3d_navigate_invoke_impl(C, op, event, &ViewOpsType_ndof_all);
}

void VIEW3D_OT_ndof_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "NDOF Transform View";
  ot->description = "Pan and rotate the view with the 3D mouse";
  ot->idname = ViewOpsType_ndof_all.idname;

  /* api callbacks */
  ot->invoke = ndof_all_invoke;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = 0;
}

const ViewOpsType ViewOpsType_ndof_orbit = {
    /*flag*/ VIEWOPS_FLAG_ORBIT_SELECT,
    /*idname*/ "VIEW3D_OT_ndof_orbit",
    /*poll_fn*/ nullptr,
    /*init_fn*/ ndof_orbit_invoke_impl,
    /*apply_fn*/ nullptr,
};

const ViewOpsType ViewOpsType_ndof_orbit_zoom = {
    /*flag*/ VIEWOPS_FLAG_ORBIT_SELECT,
    /*idname*/ "VIEW3D_OT_ndof_orbit_zoom",
    /*poll_fn*/ nullptr,
    /*init_fn*/ ndof_orbit_zoom_invoke_impl,
    /*apply_fn*/ nullptr,
};

const ViewOpsType ViewOpsType_ndof_pan = {
    /*flag*/ VIEWOPS_FLAG_NONE,
    /*idname*/ "VIEW3D_OT_ndof_pan",
    /*poll_fn*/ nullptr,
    /*init_fn*/ ndof_pan_invoke_impl,
    /*apply_fn*/ nullptr,
};

const ViewOpsType ViewOpsType_ndof_all = {
    /*flag*/ VIEWOPS_FLAG_ORBIT_SELECT,
    /*idname*/ "VIEW3D_OT_ndof_all",
    /*poll_fn*/ nullptr,
    /*init_fn*/ ndof_all_invoke_impl,
    /*apply_fn*/ nullptr,
};

#endif /* WITH_INPUT_NDOF */

/** \} */
