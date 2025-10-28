/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */
#include "BLI_bounds.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"

#include "BKE_layer.hh"

#include "DEG_depsgraph_query.hh"

#include "WM_api.hh"

#include "ED_screen.hh"

#include "view3d_intern.hh"
#include "view3d_navigate.hh" /* own include */

using blender::Bounds;
using blender::float3;

#ifdef WITH_INPUT_NDOF
static bool ndof_orbit_center_is_valid(const RegionView3D *rv3d, const float3 &center);
static bool ndof_orbit_center_is_used_no_viewport();
static bool ndof_orbit_center_is_used(const View3D *v3d, const RegionView3D *rv3d);
#endif

/* -------------------------------------------------------------------- */
/** \name NDOF Utility Functions
 * \{ */

#ifdef WITH_INPUT_NDOF

/** Test if the bounding box is in view3d camera frustum. */
static bool is_bounding_box_in_frustum(const float projmat[4][4],
                                       const Bounds<float3> &bounding_box)
{
  float planes[4][4];
  planes_from_projmat(projmat, planes[0], planes[1], planes[2], planes[3], nullptr, nullptr);
  int ret = isect_aabb_planes_v3(planes, 4, bounding_box.min, bounding_box.max);

  return ret == ISECT_AABB_PLANE_IN_FRONT_ALL;
}

enum {
  HAS_TRANSLATE = (1 << 0),
  HAS_ROTATE = (1 << 0),
};

static bool ndof_has_translate(const wmNDOFMotionData &ndof,
                               const View3D *v3d,
                               const RegionView3D *rv3d)
{
  return !is_zero_v3(ndof.tvec) && !ED_view3d_offset_lock_check(v3d, rv3d);
}

static bool ndof_has_translate_pan(const wmNDOFMotionData &ndof,
                                   const View3D *v3d,
                                   const RegionView3D *rv3d)
{
  return WM_event_ndof_translation_has_pan(ndof) && !ED_view3d_offset_lock_check(v3d, rv3d);
}

static bool ndof_has_rotate(const wmNDOFMotionData &ndof, const RegionView3D *rv3d)
{
  return !is_zero_v3(ndof.rvec) && ((RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ROTATION) == 0);
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
  if ((rv3d->ndof_flag & RV3D_NDOF_OFS_IS_VALID) && ndof_orbit_center_is_used_no_viewport()) {
    negate_v3_v3(tvec, rv3d->ndof_ofs);
  }
  else {
    negate_v3_v3(tvec, rv3d->ofs);
  }

  return view3d_ndof_pan_speed_calc_ex(rv3d, tvec);
}

/**
 * Zoom and pan in the same function since sometimes zoom is interpreted as dolly (pan forward).
 *
 * \param has_zoom: zoom, otherwise dolly,
 * often `!rv3d->is_persp` since it doesn't make sense to dolly in ortho.
 */
static void view3d_ndof_pan_zoom(const wmNDOFMotionData &ndof,
                                 ScrArea *area,
                                 ARegion *region,
                                 const bool has_translate,
                                 const bool has_zoom)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  if (has_translate == false && has_zoom == false) {
    return;
  }

  blender::float3 pan_vec = WM_event_ndof_translation_get_for_navigation(ndof);
  if (has_zoom) {
    /* zoom with Z */

    /* "zoom in" or "translate"? depends on zoom mode in user settings? */
    if (pan_vec[2]) {
      float zoom_distance = rv3d->dist * ndof.time_delta * pan_vec[2];
      rv3d->dist += zoom_distance;
    }

    /* Zoom!
     * velocity should be proportional to the linear velocity attained by rotational motion
     * of same strength [got that?] proportional to `arclength = radius * angle`.
     */

    pan_vec[2] = 0.0f;
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
    pan_vec *= speed * ndof.time_delta;

    /* transform motion from view to world coordinates */
    float view_inv[4];
    invert_qt_qt_normalized(view_inv, rv3d->viewquat);
    mul_qt_v3(view_inv, pan_vec);

    /* move center of view opposite of hand motion (this is camera mode, not object mode) */
    sub_v3_v3(rv3d->ofs, pan_vec);

    /* When in Fly mode with "Auto" speed, move `ndof_ofs` as well (to keep the speed constant). */
    if (!NDOF_IS_ORBIT_AROUND_CENTER_MODE(&U) && (U.ndof_flag & NDOF_FLY_SPEED_AUTO)) {
      sub_v3_v3(rv3d->ndof_ofs, pan_vec);
    }

    if (RV3D_LOCK_FLAGS(rv3d) & RV3D_BOXVIEW) {
      view3d_boxview_sync(area, region);
    }
  }
}

static void view3d_ndof_orbit(const wmNDOFMotionData &ndof,
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

  if (U.ndof_flag & NDOF_LOCK_HORIZON) {
    /* Turntable view code adapted for 3D mouse use. */
    float angle, quat[4];
    float xvec[3] = {1, 0, 0};
    float yvec[3] = {0, 1, 0};

    /* only use XY, ignore Z */
    blender::float3 rot = WM_event_ndof_rotation_get_for_navigation(ndof);

    /* Determine the direction of the X vector (for rotating up and down). */
    mul_qt_v3(view_inv, xvec);
    /* Determine the direction of the Y vector (to check if the view is upside down). */
    mul_qt_v3(view_inv, yvec);

    /* Perform the up/down rotation */
    angle = ndof.time_delta * rot[0];
    axis_angle_to_quat(quat, xvec, angle);
    mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, quat);

    /* Perform the Z rotation. */
    angle = ndof.time_delta * rot[1];

    /* Flip the turntable angle when the view is upside down. */
    if (yvec[2] < 0.0f) {
      angle *= -1.0f;
    }

    /* Update the onscreen axis-angle indicator. */
    rv3d->ndof_rot_angle = angle;
    rv3d->ndof_rot_axis[0] = 0;
    rv3d->ndof_rot_axis[1] = 0;
    rv3d->ndof_rot_axis[2] = 1;

    axis_angle_to_quat_single(quat, 'Z', angle);
    mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, quat);
  }
  else {
    float quat[4];
    float axis[3];
    float angle = ndof.time_delta *
                  WM_event_ndof_rotation_get_axis_angle_for_navigation(ndof, axis);

    /* transform rotation axis from view to world coordinates */
    mul_qt_v3(view_inv, axis);

    /* Update the onscreen axis-angle indicator. */
    rv3d->ndof_rot_angle = angle;
    copy_v3_v3(rv3d->ndof_rot_axis, axis);

    axis_angle_to_quat(quat, axis, angle);

    /* apply rotation */
    mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, quat);
  }

  if (apply_dyn_ofs) {
    /* Use NDOF center as a dynamic offset. */
    if (ndof_orbit_center_is_used(v3d, rv3d)) {
      if (rv3d->ndof_flag & RV3D_NDOF_OFS_IS_VALID) {
        if (ndof_orbit_center_is_valid(vod->rv3d, -float3(rv3d->ndof_ofs))) {
          vod->use_dyn_ofs = true;
          copy_v3_v3(vod->dyn_ofs, rv3d->ndof_ofs);
        }
        else {
          rv3d->ndof_flag &= ~RV3D_NDOF_OFS_IS_VALID;
        }
      }
    }
    viewrotate_apply_dyn_ofs(vod, rv3d->viewquat);
  }
}

void view3d_ndof_fly(const wmNDOFMotionData &ndof,
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

  rv3d->ndof_rot_angle = 0.0f; /* Disable onscreen rotation indicator. */

  if (has_translate) {
    /* ignore real 'dist' since fly has its own speed settings,
     * also its overwritten at this point. */
    float speed = view3d_ndof_pan_speed_calc_from_dist(rv3d, 1.0f);
    float trans_orig_y;

    if (use_precision) {
      speed *= 0.2f;
    }

    blender::float3 trans = (speed * ndof.time_delta) * WM_event_ndof_translation_get(ndof);
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
    float rotation[4];
    float axis[3];
    float angle = ndof.time_delta * WM_event_ndof_rotation_get_axis_angle(ndof, axis);

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
/** \name NDOF Orbit Center Calculation
 * \{ */

static bool ndof_orbit_center_is_used_no_viewport()
{
  if (NDOF_IS_ORBIT_AROUND_CENTER_MODE(&U)) {
    if ((U.ndof_flag & NDOF_ORBIT_CENTER_AUTO) == 0) {
      return false;
    }
  }
  else {
    if ((U.ndof_flag & NDOF_FLY_SPEED_AUTO) == 0) {
      return false;
    }
  }
  return true;
}

static bool ndof_orbit_center_is_used(const View3D *v3d, const RegionView3D *rv3d)
{
  if (!ndof_orbit_center_is_used_no_viewport()) {
    return false;
  }
  if (v3d->ob_center_cursor || v3d->ob_center) {
    return false;
  }

  /* Check the caller is not calculating auto-center when there is no reason to do so. */
  BLI_assert_msg(
      !((rv3d->persp == RV3D_CAMOB) && (v3d->flag2 & V3D_LOCK_CAMERA) == 0),
      "This test should not run from a camera view unless the camera is locked to the viewport");
  UNUSED_VARS_NDEBUG(rv3d);

  return true;
}

/**
 * Return true when `center` should not be used.
 */
static bool ndof_orbit_center_is_valid(const RegionView3D *rv3d, const float3 &center)
{
  /* NOTE: this is a fairly arbitrary check mainly to avoid obvious problems
   * where the orbit center is going to seem buggy/unusable.
   *
   * Other cases could also be counted as invalid:
   * - It's beyond the clip-end.
   * - It's not inside the viewport frustum (with some margin perhaps).
   *
   * The value could also be clamped to make it valid however when function
   * returns false the #RegionView3D::ofs is used instead, so it's not necessary
   * to go to great lengths to attempt to use the value.
   */
  if (rv3d->is_persp) {
    const float zfac = mul_project_m4_v3_zfac(rv3d->persmat, center);
    if (zfac <= 0.0f) {
      return false;
    }
  }

  return true;
}

static std::optional<float3> ndof_orbit_center_calc_from_bounds(Depsgraph *depsgraph,
                                                                ScrArea *area,
                                                                ARegion *region)
{
  std::optional<Bounds<float3>> bounding_box = std::nullopt;

  if ((U.ndof_flag & NDOF_ORBIT_CENTER_SELECTED) && NDOF_IS_ORBIT_AROUND_CENTER_MODE(&U)) {
    bool do_zoom = false;
    bounding_box = view3d_calc_minmax_selected(depsgraph, area, region, false, false, &do_zoom);
  }
  else {
    bounding_box = view3d_calc_minmax_visible(depsgraph, area, region, false, false);
  }

  if (bounding_box.has_value()) {
    const RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

    /* Scale down the bounding box to provide some offset */
    bounding_box->scale_from_center(float3(0.8));

    if (is_bounding_box_in_frustum(rv3d->persmat, *bounding_box)) {
      /* TODO: for perspective views it would be good to clip the bounds by the
       * view-point's plane, so the only the portion of the bounds in front of the
       * view-point is taken into account when calculating the center. */
      const float3 center = bounding_box->center();
      if (ndof_orbit_center_is_valid(rv3d, center)) {
        return center;
      }
    }
  }
  return std::nullopt;
}

static float ndof_read_zbuf_rect(ARegion *region, const rcti &rect, int r_xy[2])
{
  /* Avoid allocating the whole depth buffer. */
  ViewDepths depth_temp = {0};
  rcti rect_clip = rect;
  view3d_depths_rect_create(region, &rect_clip, &depth_temp);

  /* Find the closest Z pixel. */
  float depth_near;

  if (r_xy) {
    depth_near = view3d_depth_near_ex(&depth_temp, r_xy);
  }
  else {
    depth_near = view3d_depth_near(&depth_temp);
  }

  MEM_SAFE_FREE(depth_temp.depths);

  return depth_near;
}

/**
 * Sample viewport region and get the nearest (depth-wise) point in screen space.
 * \return
 * - X, Y components: region space X, Y coordinate of the sample.
 * - Z component: depth of the sample (the nearest value).
 */
static std::optional<float3> ndof_get_min_depth_pt(ARegion *region, const rcti &rect)
{
  int xy[2] = {0, 0};
  const float depth_near = ndof_read_zbuf_rect(region, rect, xy);
  if (depth_near == FLT_MAX) {
    return std::nullopt;
  }
  const float3 result = {float(xy[0]), float(xy[1]), depth_near};
  return result;
}

static std::optional<float3> ndof_orbit_center_calc_from_zbuf(Depsgraph *depsgraph,
                                                              ScrArea *area,
                                                              ARegion *region)
{
  rcti sample_rect;

  if (U.ndof_navigation_mode == NDOF_NAVIGATION_MODE_FLY) {
    /* Move the region to the bottom to enhance navigation in architectural-visualization. */
    sample_rect.xmin = 0.3f * region->winx;
    sample_rect.xmax = 0.7f * region->winx;
    sample_rect.ymin = 0.2f * region->winy;
    sample_rect.ymax = 0.6f * region->winy;
  }
  else {
    int view_center[2] = {region->winx / 2, region->winy / 2};
    BLI_rcti_init_pt_radius(&sample_rect, view_center, 0.05f * region->winx);
  }

  const std::optional<float3> min_depth_pt = ndof_get_min_depth_pt(region, sample_rect);
  if (!min_depth_pt) {
    return std::nullopt;
  }

  blender::float3 zbuf_center;
  if (!ED_view3d_unproject_v3(region, UNPACK3(*min_depth_pt), zbuf_center)) {
    return std::nullopt;
  }

  /* Since the center found with Z-buffer might be in some small distance from the mesh
   * it's safer to scale the bounding box a little before testing if it contains that center. */
  const float scale_margin = 1.05f;

  /* Use the found center if either #NDOF_ORBIT_CENTER_SELECTED is not enabled,
   * there are no selected objects center is within bounding box of selected objects. */
  if (NDOF_IS_ORBIT_AROUND_CENTER_MODE(&U)) {
    if ((U.ndof_flag & NDOF_ORBIT_CENTER_SELECTED) == 0) {
      return zbuf_center;
    }
  }
  else {
    return zbuf_center;
  }

  Scene *scene = DEG_get_input_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);

  if (!BKE_layer_collection_has_selected_objects(scene, view_layer, view_layer->active_collection))
  {
    return zbuf_center;
  }

  View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  if (view3d_calc_point_in_selected_bounds(depsgraph, view_layer, v3d, zbuf_center, scale_margin))
  {
    return zbuf_center;
  }

  return std::nullopt;
}

static std::optional<float3> ndof_orbit_center_calc(Depsgraph *depsgraph,
                                                    ScrArea *area,
                                                    ARegion *region)
{
  /* Auto orbit-center implements an intelligent way to dynamically choose the orbit-center
   * based on objects on the scene and how close to the particular object is the camera.
   *
   * Auto center calculation algorithm works as following:
   * 1) Calculate the bounding box of all objects in the scene
   * 2) If at least 80% of that box is contained in view-port's camera frustum then:
   *    2a) Store the center of that bounding box as the orbit-center.
   * 3) Use Z buffer to find the depth under the middle of the view3d region
   * 4) If some finite depth value was found then:
   *    4a) Use that depth to unproject a point from the middle of the region to the 3D space
   *    4b) Store that point as the Center of Rotation
   * 5) Since no candidates were found, use the last stored value
   *    (when #RV3D_NDOF_OFS_IS_VALID is set).
   */

  std::optional<float3> center_test = ndof_orbit_center_calc_from_bounds(depsgraph, area, region);
  if (!center_test.has_value()) {
    center_test = ndof_orbit_center_calc_from_zbuf(depsgraph, area, region);
  }
  return center_test;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Camera View Support
 * \{ */

/**
 * 2D orthographic style NDOF navigation within the camera view.
 * Support navigating the camera view instead of leaving the camera-view and navigating in 3D.
 */
static wmOperatorStatus view3d_ndof_cameraview_pan_zoom(ViewOpsData *vod,
                                                        const wmNDOFMotionData &ndof)
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

  blender::float3 pan_vec = WM_event_ndof_translation_get_for_navigation(ndof);
  const float pan_speed = NDOF_PIXELS_PER_SECOND;
  const bool has_translate = !is_zero_v2(pan_vec);
  const bool has_zoom = pan_vec[2] != 0.0f;

  pan_vec *= ndof.time_delta;

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

static wmOperatorStatus ndof_orbit_invoke_impl(bContext *C,
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

  const wmNDOFMotionData &ndof = *static_cast<const wmNDOFMotionData *>(event->customdata);

  /* off by default, until changed later this function */
  rv3d->ndof_rot_angle = 0.0f;

  if (ndof.progress != P_FINISHING) {
    const bool has_rotation = ndof_has_rotate(ndof, rv3d);
    /* if we can't rotate, fall back to translate (locked axis views) */
    const bool has_translate = (RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ROTATION) &&
                               ndof_has_translate(ndof, v3d, rv3d);
    const bool has_zoom = (rv3d->is_persp == false) && WM_event_ndof_translation_has_zoom(ndof);

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

static wmOperatorStatus ndof_orbit_invoke(bContext *C, wmOperator *op, const wmEvent *event)
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

  /* API callbacks. */
  ot->invoke = ndof_orbit_invoke;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Orbit/Zoom Operator
 * \{ */

static wmOperatorStatus ndof_orbit_zoom_invoke_impl(bContext *C,
                                                    ViewOpsData *vod,
                                                    const wmEvent *event,
                                                    PointerRNA * /*ptr*/)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }

  const wmNDOFMotionData &ndof = *static_cast<const wmNDOFMotionData *>(event->customdata);

  if (U.ndof_flag & NDOF_CAMERA_PAN_ZOOM) {
    const wmOperatorStatus camera_retval = view3d_ndof_cameraview_pan_zoom(vod, ndof);
    if (camera_retval != OPERATOR_PASS_THROUGH) {
      return camera_retval;
    }
  }

  View3D *v3d = vod->v3d;
  RegionView3D *rv3d = vod->rv3d;
  char xform_flag = 0;

  /* off by default, until changed later this function */
  rv3d->ndof_rot_angle = 0.0f;

  if (ndof.progress == P_FINISHING) {
    /* pass */
  }
  else if (ndof.progress == P_STARTING) {
    if (ndof_orbit_center_is_used(v3d, rv3d)) {
      /* If center was recalculated then update the point location for drawing. */
      if (std::optional<float3> center_test = ndof_orbit_center_calc(
              vod->depsgraph, vod->area, vod->region))
      {
        negate_v3_v3(rv3d->ndof_ofs, center_test.value());
        /* When `ndof_ofs` is set `rv3d->dist` should be set based on distance to `ndof_ofs`.
         * Without this the user is unable to zoom to the `ndof_ofs` point. See: #134732. */
        if (rv3d->is_persp) {
          const float dist_min = ED_view3d_dist_soft_min_get(v3d, true);
          if (!ED_view3d_distance_set_from_location(rv3d, center_test.value(), dist_min)) {
            ED_view3d_distance_set(rv3d, dist_min);
          }
        }
        rv3d->ndof_flag |= RV3D_NDOF_OFS_IS_VALID;
      }
    }
  }
  else if ((rv3d->persp == RV3D_ORTHO) && RV3D_VIEW_IS_AXIS(rv3d->view)) {
    /* if we can't rotate, fall back to translate (locked axis views) */
    const bool has_translate = ndof_has_translate(ndof, v3d, rv3d);
    const bool has_zoom = WM_event_ndof_translation_has_zoom(ndof) &&
                          ED_view3d_offset_lock_check(v3d, rv3d);

    if (has_translate || has_zoom) {
      view3d_ndof_pan_zoom(ndof, vod->area, vod->region, has_translate, true);
      xform_flag |= HAS_TRANSLATE;
    }
  }
  else {
    /* NOTE: based on feedback from #67579, users want to have pan and orbit enabled at once.
     * It's arguable that orbit shouldn't pan (since we have a pan only operator),
     * so if there are users who like to separate orbit/pan operations - it can be a preference. */
    const bool is_orbit_around_pivot = NDOF_IS_ORBIT_AROUND_CENTER_MODE(&U) ||
                                       ED_view3d_offset_lock_check(v3d, rv3d);
    const bool has_rotation = ndof_has_rotate(ndof, rv3d);
    bool has_translate, has_zoom;

    if (is_orbit_around_pivot) {
      /* Orbit preference or forced lock (Z zooms). */
      has_translate = ndof_has_translate_pan(ndof, v3d, rv3d);
      has_zoom = WM_event_ndof_translation_has_zoom(ndof);
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

static wmOperatorStatus ndof_orbit_zoom_invoke(bContext *C, wmOperator *op, const wmEvent *event)
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

  /* API callbacks. */
  ot->invoke = ndof_orbit_zoom_invoke;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Pan/Zoom Operator
 * \{ */

static wmOperatorStatus ndof_pan_invoke_impl(bContext *C,
                                             ViewOpsData *vod,
                                             const wmEvent *event,
                                             PointerRNA * /*ptr*/)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }

  const wmNDOFMotionData &ndof = *static_cast<const wmNDOFMotionData *>(event->customdata);

  if (U.ndof_flag & NDOF_CAMERA_PAN_ZOOM) {
    const wmOperatorStatus camera_retval = view3d_ndof_cameraview_pan_zoom(vod, ndof);
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
  const bool has_zoom = (rv3d->is_persp == false) && WM_event_ndof_translation_has_zoom(ndof);

  /* we're panning here! so erase any leftover rotation from other operators */
  rv3d->ndof_rot_angle = 0.0f;

  if (!(has_translate || has_zoom)) {
    return OPERATOR_CANCELLED;
  }

  ED_view3d_camera_lock_init_ex(depsgraph, v3d, rv3d, false);

  if (ndof.progress != P_FINISHING) {
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

static wmOperatorStatus ndof_pan_invoke(bContext *C, wmOperator *op, const wmEvent *event)
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

  /* API callbacks. */
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
static wmOperatorStatus ndof_all_invoke_impl(bContext *C,
                                             ViewOpsData *vod,
                                             const wmEvent *event,
                                             PointerRNA * /*ptr*/)
{

  wmOperatorStatus ret;

  /* weak!, but it works */
  const uint8_t ndof_navigation_mode_backup = U.ndof_navigation_mode;
  U.ndof_navigation_mode = NDOF_NAVIGATION_MODE_FLY;

  ret = ndof_orbit_zoom_invoke_impl(C, vod, event, nullptr);

  U.ndof_navigation_mode = ndof_navigation_mode_backup;

  return ret;
}

static wmOperatorStatus ndof_all_invoke(bContext *C, wmOperator *op, const wmEvent *event)
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

  /* API callbacks. */
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
