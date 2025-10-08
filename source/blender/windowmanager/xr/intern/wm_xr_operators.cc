/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * \name Window-Manager XR Operators
 *
 * Collection of XR-related operators.
 */

#include "BLI_kdopbvh.hh"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_time.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_main.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"

#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_transform_snap_object_context.hh"
#include "ED_view3d.hh"

#include "GHOST_Types.h"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "GPU_batch_presets.hh"
#include "GPU_matrix.hh"
#include "GPU_xr_defines.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm_xr_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Operator Conditions
 * \{ */

/* `op->poll`. */
static bool wm_xr_operator_sessionactive(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  return WM_xr_session_is_ready(&wm->xr);
}

static bool wm_xr_operator_test_event(const wmOperator *op, const wmEvent *event)
{
  if (event->type != EVT_XR_ACTION) {
    return false;
  }

  BLI_assert(event->custom == EVT_DATA_XR);
  BLI_assert(event->customdata);

  wmXrActionData *actiondata = static_cast<wmXrActionData *>(event->customdata);
  return (actiondata->ot == op->type &&
          IDP_EqualsProperties(actiondata->op_properties, op->properties));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name XR Session Toggle
 *
 * Toggles an XR session, creating an XR context if necessary.
 * \{ */

static void wm_xr_session_update_screen(Main *bmain, const wmXrData *xr_data)
{
  const bool session_exists = WM_xr_session_exists(xr_data);

  for (bScreen *screen = static_cast<bScreen *>(bmain->screens.first); screen;
       screen = static_cast<bScreen *>(screen->id.next))
  {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, slink, &area->spacedata) {
        if (slink->spacetype == SPACE_VIEW3D) {
          View3D *v3d = (View3D *)slink;

          if (v3d->flag & V3D_XR_SESSION_MIRROR) {
            ED_view3d_xr_mirror_update(area, v3d, session_exists);
          }

          if (session_exists) {
            wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
            const Scene *scene = WM_windows_scene_get_from_screen(wm, screen);

            ED_view3d_xr_shading_update(wm, v3d, scene);
          }
          /* Ensure no 3D View is tagged as session root. */
          else {
            v3d->runtime.flag &= ~V3D_RUNTIME_XR_SESSION_ROOT;
          }
        }
      }
    }
  }

  WM_main_add_notifier(NC_WM | ND_XR_DATA_CHANGED, nullptr);
}

static void wm_xr_session_update_screen_on_exit_cb(const wmXrData *xr_data)
{
  /* Just use G_MAIN here, storing main isn't reliable enough on file read or exit. */
  wm_xr_session_update_screen(G_MAIN, xr_data);
}

static wmOperatorStatus wm_xr_session_toggle_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  View3D *v3d = CTX_wm_view3d(C);

  /* Lazily-create XR context - tries to dynamic-link to the runtime,
   * reading `active_runtime.json`. */
  if (wm_xr_init(wm) == false) {
    return OPERATOR_CANCELLED;
  }

  v3d->runtime.flag |= V3D_RUNTIME_XR_SESSION_ROOT;
  wm_xr_session_toggle(wm, win, wm_xr_session_update_screen_on_exit_cb);
  wm_xr_session_update_screen(bmain, &wm->xr);

  WM_event_add_notifier(C, NC_WM | ND_XR_DATA_CHANGED, nullptr);

  return OPERATOR_FINISHED;
}

static void WM_OT_xr_session_toggle(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Toggle VR Session";
  ot->idname = "WM_OT_xr_session_toggle";
  ot->description =
      "Open a view for use with virtual reality headsets, or close it if already "
      "opened";

  /* Callbacks. */
  ot->exec = wm_xr_session_toggle_exec;
  ot->poll = ED_operator_view3d_active;

  /* XXX INTERNAL just to hide it from the search menu by default, an Add-on will expose it in the
   * UI instead. Not meant as a permanent solution. */
  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name XR Grab Utilities
 * \{ */

struct XrGrabData {
  float mat_prev[4][4];
  float mat_other_prev[4][4];
  bool bimanual_prev;
  bool loc_lock, locz_lock, rot_lock, rotz_lock, scale_lock;
};

static void wm_xr_grab_init(wmOperator *op)
{
  BLI_assert(op->customdata == nullptr);

  op->customdata = MEM_callocN<XrGrabData>(__func__);
}

static void wm_xr_grab_uninit(wmOperator *op)
{
  XrGrabData *data = static_cast<XrGrabData *>(op->customdata);
  MEM_SAFE_FREE(data);
  op->customdata = nullptr;
}

static void wm_xr_grab_update(wmOperator *op, const wmXrActionData *actiondata)
{
  XrGrabData *data = static_cast<XrGrabData *>(op->customdata);

  quat_to_mat4(data->mat_prev, actiondata->controller_rot);
  copy_v3_v3(data->mat_prev[3], actiondata->controller_loc);

  if (actiondata->bimanual) {
    quat_to_mat4(data->mat_other_prev, actiondata->controller_rot_other);
    copy_v3_v3(data->mat_other_prev[3], actiondata->controller_loc_other);
    data->bimanual_prev = true;
  }
  else {
    data->bimanual_prev = false;
  }
}

static void orient_mat_z_normalized(float R[4][4], const float z_axis[3])
{
  const float scale = len_v3(R[0]);
  float x_axis[3], y_axis[3];

  cross_v3_v3v3(y_axis, z_axis, R[0]);
  normalize_v3(y_axis);
  mul_v3_v3fl(R[1], y_axis, scale);

  cross_v3_v3v3(x_axis, R[1], z_axis);
  normalize_v3(x_axis);
  mul_v3_v3fl(R[0], x_axis, scale);

  mul_v3_v3fl(R[2], z_axis, scale);
}

static void wm_xr_navlocks_apply(const float nav_mat[4][4],
                                 const float nav_inv[4][4],
                                 bool loc_lock,
                                 bool locz_lock,
                                 bool rotz_lock,
                                 float r_prev[4][4],
                                 float r_curr[4][4])
{
  /* Locked in base pose coordinates. */
  float prev_base[4][4], curr_base[4][4];

  mul_m4_m4m4(prev_base, nav_inv, r_prev);
  mul_m4_m4m4(curr_base, nav_inv, r_curr);

  if (rotz_lock) {
    const float z_axis[3] = {0.0f, 0.0f, 1.0f};
    orient_mat_z_normalized(prev_base, z_axis);
    orient_mat_z_normalized(curr_base, z_axis);
  }

  if (loc_lock) {
    copy_v3_v3(curr_base[3], prev_base[3]);
  }
  else if (locz_lock) {
    curr_base[3][2] = prev_base[3][2];
  }

  mul_m4_m4m4(r_prev, nav_mat, prev_base);
  mul_m4_m4m4(r_curr, nav_mat, curr_base);
}

/**
 * Compute transformation delta for a one-handed grab interaction.
 *
 * \param actiondata: Contains current controller pose in world space.
 * \param data: Contains previous controller pose in world space.
 *
 * The delta is computed as the difference between the current and previous
 * controller poses i.e. delta = curr * prev^-1.
 */
static void wm_xr_grab_compute(const wmXrActionData *actiondata,
                               const XrGrabData *data,
                               const float nav_mat[4][4],
                               const float nav_inv[4][4],
                               bool reverse,
                               float r_delta[4][4])
{
  const bool nav_lock = (nav_mat && nav_inv);
  float prev[4][4], curr[4][4];

  if (!data->rot_lock) {
    copy_m4_m4(prev, data->mat_prev);
    zero_v3(prev[3]);
    quat_to_mat4(curr, actiondata->controller_rot);
  }
  else {
    unit_m4(prev);
    unit_m4(curr);
  }

  if (!data->loc_lock || nav_lock) {
    copy_v3_v3(prev[3], data->mat_prev[3]);
    copy_v3_v3(curr[3], actiondata->controller_loc);
  }

  if (nav_lock) {
    wm_xr_navlocks_apply(
        nav_mat, nav_inv, data->loc_lock, data->locz_lock, data->rotz_lock, prev, curr);
  }

  if (reverse) {
    invert_m4(curr);
    mul_m4_m4m4(r_delta, prev, curr);
  }
  else {
    invert_m4(prev);
    mul_m4_m4m4(r_delta, curr, prev);
  }
}

/**
 * Compute transformation delta for a two-handed (bimanual) grab interaction.
 *
 * \param actiondata: Contains current controller poses in world space.
 * \param data: Contains previous controller poses in world space.
 *
 * The delta is computed as the difference (delta = curr * prev^-1) between the current
 * and previous transformations, where the transformations themselves are determined as follows:
 * - Translation: Averaged controller positions.
 * - Rotation: Rotation of axis line between controllers.
 * - Scale: Distance between controllers.
 */
static void wm_xr_grab_compute_bimanual(const wmXrActionData *actiondata,
                                        const XrGrabData *data,
                                        const float nav_mat[4][4],
                                        const float nav_inv[4][4],
                                        bool reverse,
                                        float r_delta[4][4])
{
  const bool nav_lock = (nav_mat && nav_inv);
  float prev[4][4], curr[4][4];
  unit_m4(prev);
  unit_m4(curr);

  if (!data->rot_lock) {
    /* Rotation. */
    float x_axis_prev[3], x_axis_curr[3], y_axis_prev[3], y_axis_curr[3], z_axis_prev[3],
        z_axis_curr[3];
    float m0[3][3], m1[3][3];
    quat_to_mat3(m0, actiondata->controller_rot);
    quat_to_mat3(m1, actiondata->controller_rot_other);

    /* X-axis is the base line between the two controllers. */
    sub_v3_v3v3(x_axis_prev, data->mat_prev[3], data->mat_other_prev[3]);
    sub_v3_v3v3(x_axis_curr, actiondata->controller_loc, actiondata->controller_loc_other);
    /* Y-axis is the average of the controllers' y-axes. */
    add_v3_v3v3(y_axis_prev, data->mat_prev[1], data->mat_other_prev[1]);
    mul_v3_fl(y_axis_prev, 0.5f);
    add_v3_v3v3(y_axis_curr, m0[1], m1[1]);
    mul_v3_fl(y_axis_curr, 0.5f);
    /* Z-axis is the cross product of the two. */
    cross_v3_v3v3(z_axis_prev, x_axis_prev, y_axis_prev);
    cross_v3_v3v3(z_axis_curr, x_axis_curr, y_axis_curr);
    /* Fix the y-axis to be orthogonal. */
    cross_v3_v3v3(y_axis_prev, z_axis_prev, x_axis_prev);
    cross_v3_v3v3(y_axis_curr, z_axis_curr, x_axis_curr);
    /* Normalize. */
    normalize_v3_v3(prev[0], x_axis_prev);
    normalize_v3_v3(prev[1], y_axis_prev);
    normalize_v3_v3(prev[2], z_axis_prev);
    normalize_v3_v3(curr[0], x_axis_curr);
    normalize_v3_v3(curr[1], y_axis_curr);
    normalize_v3_v3(curr[2], z_axis_curr);
  }

  if (!data->loc_lock || nav_lock) {
    /* Translation: translation of the averaged controller locations. */
    add_v3_v3v3(prev[3], data->mat_prev[3], data->mat_other_prev[3]);
    mul_v3_fl(prev[3], 0.5f);
    add_v3_v3v3(curr[3], actiondata->controller_loc, actiondata->controller_loc_other);
    mul_v3_fl(curr[3], 0.5f);
  }

  if (!data->scale_lock) {
    /* Scaling: distance between controllers. */
    float scale, v[3];

    sub_v3_v3v3(v, data->mat_prev[3], data->mat_other_prev[3]);
    scale = len_v3(v);
    mul_v3_fl(prev[0], scale);
    mul_v3_fl(prev[1], scale);
    mul_v3_fl(prev[2], scale);

    sub_v3_v3v3(v, actiondata->controller_loc, actiondata->controller_loc_other);
    scale = len_v3(v);
    mul_v3_fl(curr[0], scale);
    mul_v3_fl(curr[1], scale);
    mul_v3_fl(curr[2], scale);
  }

  if (nav_lock) {
    wm_xr_navlocks_apply(
        nav_mat, nav_inv, data->loc_lock, data->locz_lock, data->rotz_lock, prev, curr);
  }

  if (reverse) {
    invert_m4(curr);
    mul_m4_m4m4(r_delta, prev, curr);
  }
  else {
    invert_m4(prev);
    mul_m4_m4m4(r_delta, curr, prev);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name XR Navigation Grab
 *
 * Navigates the scene by grabbing with XR controllers.
 * \{ */

static wmOperatorStatus wm_xr_navigation_grab_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent *event)
{
  if (!wm_xr_operator_test_event(op, event)) {
    return OPERATOR_PASS_THROUGH;
  }

  const wmXrActionData *actiondata = static_cast<const wmXrActionData *>(event->customdata);

  wm_xr_grab_init(op);
  wm_xr_grab_update(op, actiondata);

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus wm_xr_navigation_grab_exec(bContext * /*C*/, wmOperator * /*op*/)
{
  return OPERATOR_CANCELLED;
}

static bool wm_xr_navigation_grab_can_do_bimanual(const wmXrActionData *actiondata,
                                                  const XrGrabData *data)
{
  /* Returns true if: 1) Bimanual interaction is currently occurring (i.e. inputs on both
   * controllers are pressed) and 2) bimanual interaction occurred on the last update. This second
   * part is needed to avoid "jumpy" navigation changes when transitioning from one-handed to
   * two-handed interaction (see #wm_xr_grab_compute/compute_bimanual() for how navigation deltas
   * are calculated). */
  return (actiondata->bimanual && data->bimanual_prev);
}

static bool wm_xr_navigation_grab_is_bimanual_ending(const wmXrActionData *actiondata,
                                                     const XrGrabData *data)
{
  return (!actiondata->bimanual && data->bimanual_prev);
}

static bool wm_xr_navigation_grab_is_locked(const XrGrabData *data, const bool bimanual)
{
  if (bimanual) {
    return data->loc_lock && data->rot_lock && data->scale_lock;
  }
  /* Ignore scale lock, as one-handed interaction cannot change navigation scale. */
  return data->loc_lock && data->rot_lock;
}

static void wm_xr_navigation_grab_apply(wmXrData *xr,
                                        const wmXrActionData *actiondata,
                                        const XrGrabData *data,
                                        bool bimanual)
{
  GHOST_XrPose nav_pose;
  float nav_scale;
  float nav_mat[4][4], nav_inv[4][4], delta[4][4], out[4][4];

  const bool need_navinv = (data->loc_lock || data->locz_lock || data->rotz_lock);

  WM_xr_session_state_nav_location_get(xr, nav_pose.position);
  WM_xr_session_state_nav_rotation_get(xr, nav_pose.orientation_quat);
  WM_xr_session_state_nav_scale_get(xr, &nav_scale);

  wm_xr_pose_scale_to_mat(&nav_pose, nav_scale, nav_mat);
  if (need_navinv) {
    wm_xr_pose_scale_to_imat(&nav_pose, nav_scale, nav_inv);
  }

  if (bimanual) {
    wm_xr_grab_compute_bimanual(actiondata,
                                data,
                                need_navinv ? nav_mat : nullptr,
                                need_navinv ? nav_inv : nullptr,
                                true,
                                delta);
  }
  else {
    wm_xr_grab_compute(actiondata,
                       data,
                       need_navinv ? nav_mat : nullptr,
                       need_navinv ? nav_inv : nullptr,
                       true,
                       delta);
  }

  mul_m4_m4m4(out, delta, nav_mat);

  /* Limit scale to reasonable values. */
  nav_scale = len_v3(out[0]);

  if (!(nav_scale < xr->session_settings.clip_start || nav_scale > xr->session_settings.clip_end))
  {
    WM_xr_session_state_nav_location_set(xr, out[3]);
    if (!data->rot_lock) {
      mat4_to_quat(nav_pose.orientation_quat, out);
      normalize_qt(nav_pose.orientation_quat);
      WM_xr_session_state_nav_rotation_set(xr, nav_pose.orientation_quat);
    }
    if (!data->scale_lock && bimanual) {
      WM_xr_session_state_nav_scale_set(xr, nav_scale);
    }
  }
}

static void wm_xr_navigation_grab_bimanual_state_update(const wmXrActionData *actiondata,
                                                        XrGrabData *data)
{
  if (actiondata->bimanual) {
    if (!data->bimanual_prev) {
      quat_to_mat4(data->mat_prev, actiondata->controller_rot);
      copy_v3_v3(data->mat_prev[3], actiondata->controller_loc);
      quat_to_mat4(data->mat_other_prev, actiondata->controller_rot_other);
      copy_v3_v3(data->mat_other_prev[3], actiondata->controller_loc_other);
    }
    data->bimanual_prev = true;
  }
  else {
    if (data->bimanual_prev) {
      quat_to_mat4(data->mat_prev, actiondata->controller_rot);
      copy_v3_v3(data->mat_prev[3], actiondata->controller_loc);
    }
    data->bimanual_prev = false;
  }
}

static void wm_xr_navigation_grab_cancel(bContext * /*C*/, wmOperator *op)
{
  wm_xr_grab_uninit(op);
}

static wmOperatorStatus wm_xr_navigation_grab_modal(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  if (!wm_xr_operator_test_event(op, event)) {
    return OPERATOR_PASS_THROUGH;
  }

  const wmXrActionData *actiondata = static_cast<const wmXrActionData *>(event->customdata);
  XrGrabData *data = static_cast<XrGrabData *>(op->customdata);
  wmWindowManager *wm = CTX_wm_manager(C);
  wmXrData *xr = &wm->xr;

  WM_xr_session_state_vignette_activate(xr);

  const bool do_bimanual = wm_xr_navigation_grab_can_do_bimanual(actiondata, data);

  data->loc_lock = RNA_boolean_get(op->ptr, "lock_location");
  data->locz_lock = RNA_boolean_get(op->ptr, "lock_location_z");
  data->rot_lock = RNA_boolean_get(op->ptr, "lock_rotation");
  data->rotz_lock = RNA_boolean_get(op->ptr, "lock_rotation_z");
  data->scale_lock = RNA_boolean_get(op->ptr, "lock_scale");

  /* Check if navigation is locked. */
  if (!wm_xr_navigation_grab_is_locked(data, do_bimanual)) {
    /* Prevent unwanted snapping (i.e. "jumpy" navigation changes when transitioning from
     * two-handed to one-handed interaction) at the end of a bimanual interaction. */
    if (!wm_xr_navigation_grab_is_bimanual_ending(actiondata, data)) {
      wm_xr_navigation_grab_apply(xr, actiondata, data, do_bimanual);
    }
  }

  wm_xr_navigation_grab_bimanual_state_update(actiondata, data);

  /* NOTE: #KM_PRESS and #KM_RELEASE are the only two values supported by XR events during event
   * dispatching (see #wm_xr_session_action_states_interpret()). For modal XR operators, modal
   * handling starts when an input is "pressed" (action state exceeds the action threshold) and
   * ends when the input is "released" (state falls below the threshold). */
  switch (event->val) {
    case KM_PRESS:
      return OPERATOR_RUNNING_MODAL;
    case KM_RELEASE:
      wm_xr_grab_uninit(op);
      return OPERATOR_FINISHED;
    default:
      BLI_assert_unreachable();
      wm_xr_grab_uninit(op);
      return OPERATOR_CANCELLED;
  }
}

static void WM_OT_xr_navigation_grab(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "XR Navigation Grab";
  ot->idname = "WM_OT_xr_navigation_grab";
  ot->description = "Navigate the VR scene by grabbing with controllers";

  /* Callbacks. */
  ot->invoke = wm_xr_navigation_grab_invoke;
  ot->exec = wm_xr_navigation_grab_exec;
  ot->cancel = wm_xr_navigation_grab_cancel;
  ot->modal = wm_xr_navigation_grab_modal;
  ot->poll = wm_xr_operator_sessionactive;

  /* Properties. */
  RNA_def_boolean(
      ot->srna, "lock_location", false, "Lock Location", "Prevent changes to viewer location");
  RNA_def_boolean(
      ot->srna, "lock_location_z", false, "Lock Elevation", "Prevent changes to viewer elevation");
  RNA_def_boolean(
      ot->srna, "lock_rotation", false, "Lock Rotation", "Prevent changes to viewer rotation");
  RNA_def_boolean(ot->srna,
                  "lock_rotation_z",
                  false,
                  "Lock Up Orientation",
                  "Prevent changes to viewer up orientation");
  RNA_def_boolean(ot->srna, "lock_scale", false, "Lock Scale", "Prevent changes to viewer scale");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name XR Raycast Utilities
 * \{ */

static const float g_xr_default_raycast_axis[3] = {0.0f, 0.0f, -1.0f};
static const float g_xr_default_raycast_hit_color[4] = {0.35f, 0.35f, 1.0f, 1.0f};
static const float g_xr_default_raycast_miss_color[4] = {1.0f, 0.35f, 0.35f, 1.0f};
static const float g_xr_default_raycast_fallback_color[4] = {0.35f, 0.35f, 1.0f, 1.0f};

enum XrRaycastResult : uint8_t {
  XR_RAYCAST_MISS,
  XR_RAYCAST_HIT,
  XR_RAYCAST_FALLBACK,
};

struct XrRaycastData {
  /** Raycast info */
  bool from_viewer;

  /** Raycast results */
  XrRaycastResult result;
  int num_points;
  float points[XR_MAX_RAYCASTS + 1][4];
  float direction[3];

  /** Raycast visualization parameters */
  float color[4];
  float raycast_width;
  float destination_size;
  int sample_count;

  blender::gpu::Batch *raycast_model;

  void *draw_handle;
};

static void wm_xr_raycast_destination_draw(const XrRaycastData *data)
{
  GPU_depth_test(GPU_DEPTH_LESS_EQUAL);

  blender::gpu::Batch *sphere = GPU_batch_preset_sphere(2);
  GPU_batch_program_set_builtin(sphere, GPU_SHADER_3D_UNIFORM_COLOR);
  GPU_batch_uniform_4fv(sphere, "color", data->color);

  GPU_matrix_push();
  GPU_matrix_translate_3fv(data->points[data->num_points - 1]);
  GPU_matrix_scale_1f(data->destination_size);
  GPU_batch_draw(sphere);
  GPU_matrix_pop();
}

static void wm_xr_raycast_draw(const bContext * /*C*/, ARegion * /*region*/, void *customdata)
{
  const XrRaycastData *data = static_cast<const XrRaycastData *>(customdata);

  if (data->result != XR_RAYCAST_MISS) {
    wm_xr_raycast_destination_draw(data);
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);

  if (data->from_viewer) {
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor4fv(data->color);

    GPU_depth_test(GPU_DEPTH_NONE);
    GPU_point_size(7.0f);

    immBegin(GPU_PRIM_POINTS, data->num_points - 1);

    for (int i = 1; i < data->num_points; ++i) {
      immVertex3fv(pos, data->points[i]);
    }

    immEnd();
    immUnbindProgram();
  }
  else {
    BLI_assert(data->raycast_model != nullptr);

    float forward[3];
    float right[3];

    sub_v3_v3v3(forward, data->points[data->num_points - 1], data->points[0]);
    copy_v3_fl3(right, forward[1], -forward[0], 0.0f);
    normalize_v3(right);

    GPU_depth_test(GPU_DEPTH_LESS_EQUAL);

    GPU_batch_program_set_builtin(data->raycast_model, GPU_SHADER_XR_RAYCAST);
    GPU_batch_uniform_4fv_array(
        data->raycast_model, "control_points", XR_MAX_RAYCASTS + 1, data->points);
    GPU_batch_uniform_4fv(data->raycast_model, "color", data->color);
    GPU_batch_uniform_3fv(data->raycast_model, "right_vector", right);
    GPU_batch_uniform_1f(data->raycast_model, "width", data->raycast_width);
    GPU_batch_uniform_1i(data->raycast_model, "control_point_count", data->num_points);
    GPU_batch_uniform_1i(data->raycast_model, "sample_count", data->sample_count);
    GPU_batch_draw(data->raycast_model);
  }
}

static void wm_xr_raycast_init(wmOperator *op)
{
  BLI_assert(op->customdata == nullptr);

  op->customdata = MEM_callocN<XrRaycastData>(__func__);

  SpaceType *st = BKE_spacetype_from_id(SPACE_VIEW3D);
  if (!st) {
    return;
  }

  ARegionType *art = BKE_regiontype_from_id(st, RGN_TYPE_XR);
  if (!art) {
    return;
  }

  XrRaycastData *data = static_cast<XrRaycastData *>(op->customdata);
  data->draw_handle = ED_region_draw_cb_activate(
      art, wm_xr_raycast_draw, op->customdata, REGION_DRAW_POST_VIEW);
}

static void wm_xr_raycast_uninit(wmOperator *op)
{
  if (!op->customdata) {
    return;
  }

  XrRaycastData *data = static_cast<XrRaycastData *>(op->customdata);

  SpaceType *st = BKE_spacetype_from_id(SPACE_VIEW3D);
  if (st) {
    ARegionType *art = BKE_regiontype_from_id(st, RGN_TYPE_XR);
    if (art) {
      ED_region_draw_cb_exit(art, data->draw_handle);
    }
  }

  MEM_SAFE_FREE(data);
  op->customdata = nullptr;
}

static void wm_xr_raycast_update(wmOperator *op,
                                 const wmXrData *xr,
                                 const wmXrActionData *actiondata)
{
  XrRaycastData *data = static_cast<XrRaycastData *>(op->customdata);
  float axis[3], nav_scale;

  WM_xr_session_state_nav_scale_get(xr, &nav_scale);

  data->from_viewer = RNA_boolean_get(op->ptr, "from_viewer");
  data->raycast_width = RNA_float_get(op->ptr, "raycast_scale") * nav_scale;
  data->sample_count = RNA_int_get(op->ptr, "sample_count");
  RNA_float_get_array(op->ptr, "axis", axis);

  if (data->from_viewer) {
    float viewer_rot[4];
    WM_xr_session_state_viewer_pose_location_get(xr, data->points[0]);
    WM_xr_session_state_viewer_pose_rotation_get(xr, viewer_rot);
    mul_qt_v3(viewer_rot, axis);
  }
  else {
    if (!xr->runtime->session_state.raycast_model) {
      xr->runtime->session_state.raycast_model = GPU_batch_create_procedural(
          GPU_PRIM_TRI_STRIP, 2 * data->sample_count);
    }

    data->raycast_model = xr->runtime->session_state.raycast_model;

    copy_v3_v3(data->points[0], actiondata->controller_loc);
    mul_qt_v3(actiondata->controller_rot, axis);
  }

  copy_v3_v3(data->direction, axis);
}

static void wm_xr_raycast(Scene *scene,
                          Depsgraph *depsgraph,
                          const float origin[3],
                          const float direction[3],
                          float *ray_dist,
                          bool selectable_only,
                          float r_location[3],
                          float r_normal[3],
                          int *r_index,
                          const Object **r_ob,
                          float r_obmat[4][4])
{
  /* Uses same raycast method as Scene.ray_cast(). */
  blender::ed::transform::SnapObjectContext *sctx =
      blender::ed::transform::snap_object_context_create(scene, 0);

  blender::ed::transform::SnapObjectParams params{};
  params.snap_target_select = (selectable_only ? SCE_SNAP_TARGET_ONLY_SELECTABLE :
                                                 SCE_SNAP_TARGET_ALL);
  blender::ed::transform::snap_object_project_ray_ex(sctx,
                                                     depsgraph,
                                                     nullptr,
                                                     &params,
                                                     origin,
                                                     direction,
                                                     ray_dist,
                                                     r_location,
                                                     r_normal,
                                                     r_index,
                                                     r_ob,
                                                     r_obmat);

  blender::ed::transform::snap_object_context_destroy(sctx);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name XR Navigation Fly
 *
 * Navigates the scene by moving/turning relative to navigation space or the XR viewer or
 * controller.
 * \{ */

enum eXrFlyMode {
  XR_FLY_FORWARD = 0,
  XR_FLY_BACK = 1,
  XR_FLY_LEFT = 2,
  XR_FLY_RIGHT = 3,
  XR_FLY_UP = 4,
  XR_FLY_DOWN = 5,
  XR_FLY_TURNLEFT = 6,
  XR_FLY_TURNRIGHT = 7,
  XR_FLY_VIEWER_FORWARD = 8,
  XR_FLY_VIEWER_BACK = 9,
  XR_FLY_VIEWER_LEFT = 10,
  XR_FLY_VIEWER_RIGHT = 11,
  XR_FLY_CONTROLLER_FORWARD = 12,
};

struct XrFlyData {
  float viewer_rot[4];
  double time_prev;

  /* Only used for snap turn, where the action should be executed only once. */
  bool is_finished;
};

static void wm_xr_fly_init(wmOperator *op, const wmXrData *xr)
{
  BLI_assert(op->customdata == nullptr);

  XrFlyData *data = MEM_callocN<XrFlyData>(__func__);
  op->customdata = data;

  WM_xr_session_state_viewer_pose_rotation_get(xr, data->viewer_rot);
  data->time_prev = BLI_time_now_seconds();
}

static void wm_xr_fly_uninit(wmOperator *op)
{
  XrFlyData *data = static_cast<XrFlyData *>(op->customdata);
  MEM_SAFE_FREE(data);
  op->customdata = nullptr;
}

static void wm_xr_fly_compute_move(eXrFlyMode mode,
                                   float speed,
                                   const float ref_quat[4],
                                   const float nav_mat[4][4],
                                   bool locz_lock,
                                   float r_delta[4][4])
{
  float ref_axes[3][3];
  quat_to_mat3(ref_axes, ref_quat);

  unit_m4(r_delta);

  switch (mode) {
    /* Navigation space reference. */
    case XR_FLY_FORWARD:
      madd_v3_v3fl(r_delta[3], ref_axes[1], speed);
      return;
    case XR_FLY_BACK:
      madd_v3_v3fl(r_delta[3], ref_axes[1], -speed);
      return;
    case XR_FLY_LEFT:
      madd_v3_v3fl(r_delta[3], ref_axes[0], -speed);
      return;
    case XR_FLY_RIGHT:
      madd_v3_v3fl(r_delta[3], ref_axes[0], speed);
      return;
    case XR_FLY_UP:
    case XR_FLY_DOWN:
      if (!locz_lock) {
        madd_v3_v3fl(r_delta[3], ref_axes[2], (mode == XR_FLY_UP) ? speed : -speed);
      }
      return;
    /* Viewer/controller space reference. */
    case XR_FLY_VIEWER_FORWARD:
    case XR_FLY_CONTROLLER_FORWARD:
      negate_v3_v3(r_delta[3], ref_axes[2]);
      break;
    case XR_FLY_VIEWER_BACK:
      copy_v3_v3(r_delta[3], ref_axes[2]);
      break;
    case XR_FLY_VIEWER_LEFT:
      negate_v3_v3(r_delta[3], ref_axes[0]);
      break;
    case XR_FLY_VIEWER_RIGHT:
      copy_v3_v3(r_delta[3], ref_axes[0]);
      break;
    /* Unused. */
    case XR_FLY_TURNLEFT:
    case XR_FLY_TURNRIGHT:
      BLI_assert_unreachable();
      return;
  }

  if (locz_lock) {
    /* Lock elevation in navigation space. */
    float z_axis[3], projected[3];

    normalize_v3_v3(z_axis, nav_mat[2]);
    project_v3_v3v3_normalized(projected, r_delta[3], z_axis);
    sub_v3_v3(r_delta[3], projected);

    normalize_v3(r_delta[3]);
  }

  mul_v3_fl(r_delta[3], speed);
}

static void wm_xr_fly_compute_turn(eXrFlyMode mode,
                                   float speed,
                                   const float viewer_mat[4][4],
                                   const float nav_mat[4][4],
                                   const float nav_inv[4][4],
                                   float r_delta[4][4])
{
  BLI_assert(ELEM(mode, XR_FLY_TURNLEFT, XR_FLY_TURNRIGHT));

  float z_axis[3], m[3][3], prev[4][4], curr[4][4];

  /* Turn around Z-axis in navigation space. */
  normalize_v3_v3(z_axis, nav_mat[2]);
  axis_angle_normalized_to_mat3(m, z_axis, (mode == XR_FLY_TURNLEFT) ? speed : -speed);
  copy_m4_m3(r_delta, m);

  copy_m4_m4(prev, viewer_mat);
  mul_m4_m4m4(curr, r_delta, viewer_mat);

  /* Lock location in base pose space. */
  wm_xr_navlocks_apply(nav_mat, nav_inv, true, false, false, prev, curr);

  invert_m4(prev);
  mul_m4_m4m4(r_delta, curr, prev);
}

static void wm_xr_basenav_rotation_calc(const wmXrData *xr,
                                        const float nav_rotation[4],
                                        float r_rotation[4])
{
  /* Apply nav rotation to base pose Z-rotation. */
  float base_eul[3], base_quatz[4];
  quat_to_eul(base_eul, xr->runtime->session_state.prev_base_pose.orientation_quat);
  axis_angle_to_quat_single(base_quatz, 'Z', base_eul[2]);
  mul_qt_qtqt(r_rotation, nav_rotation, base_quatz);
}

static wmOperatorStatus wm_xr_navigation_fly_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  if (!wm_xr_operator_test_event(op, event)) {
    return OPERATOR_PASS_THROUGH;
  }

  wmWindowManager *wm = CTX_wm_manager(C);

  wm_xr_fly_init(op, &wm->xr);

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus wm_xr_navigation_fly_exec(bContext * /*C*/, wmOperator * /*op*/)
{
  return OPERATOR_CANCELLED;
}

static void wm_xr_navigation_fly_cancel(bContext * /*C*/, wmOperator *op)
{
  wm_xr_fly_uninit(op);
}

static wmOperatorStatus wm_xr_navigation_fly_modal(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  if (!wm_xr_operator_test_event(op, event)) {
    return OPERATOR_PASS_THROUGH;
  }

  if (event->val == KM_RELEASE) {
    wm_xr_fly_uninit(op);
    return OPERATOR_FINISHED;
  }

  const wmXrActionData *actiondata = static_cast<const wmXrActionData *>(event->customdata);
  XrFlyData *data = static_cast<XrFlyData *>(op->customdata);
  wmWindowManager *wm = CTX_wm_manager(C);
  wmXrData *xr = &wm->xr;
  eXrFlyMode mode;
  bool turn, snap_turn, invert_rotation, swap_hands, locz_lock, dir_lock, speed_frame_based;
  bool speed_interp_cubic = false;
  float speed, speed_max, speed_p0[2], speed_p1[2], button_state;
  GHOST_XrPose nav_pose;
  float nav_mat[4][4], delta[4][4], out[4][4];

  const double time_now = BLI_time_now_seconds(), delta_time = time_now - data->time_prev;
  data->time_prev = time_now;

  swap_hands = xr->runtime->session_state.swap_hands;
  mode = (eXrFlyMode)RNA_enum_get(op->ptr, swap_hands ? "alt_mode" : "mode");
  turn = ELEM(mode, XR_FLY_TURNLEFT, XR_FLY_TURNRIGHT);
  snap_turn = U.xr_navigation.flag & USER_XR_NAV_SNAP_TURN;
  invert_rotation = U.xr_navigation.flag & USER_XR_NAV_INVERT_ROTATION;

  locz_lock = RNA_boolean_get(op->ptr, swap_hands ? "alt_lock_location_z" : "lock_location_z");
  dir_lock = RNA_boolean_get(op->ptr, swap_hands ? "alt_lock_direction" : "lock_direction");

  if (turn) {
    speed_frame_based = false;

    if (snap_turn) {
      speed_max = U.xr_navigation.turn_amount;
      speed = speed_max;
    }
    else {
      speed_max = U.xr_navigation.turn_speed;
      speed = speed_max * RNA_boolean_get(op->ptr, "turn_speed_factor");
    }
  }
  else {
    speed_frame_based = RNA_boolean_get(op->ptr, "speed_frame_based");
    speed_max = xr->session_settings.fly_speed;
    speed = speed_max * RNA_float_get(op->ptr, "fly_speed_factor");
  }

  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "speed_interpolation0");
  if (prop && RNA_property_is_set(op->ptr, prop)) {
    RNA_property_float_get_array(op->ptr, prop, speed_p0);
    speed_interp_cubic = true;
  }
  else {
    speed_p0[0] = speed_p0[1] = 0.0f;
  }

  prop = RNA_struct_find_property(op->ptr, "speed_interpolation1");
  if (prop && RNA_property_is_set(op->ptr, prop)) {
    RNA_property_float_get_array(op->ptr, prop, speed_p1);
    speed_interp_cubic = true;
  }
  else {
    speed_p1[0] = speed_p1[1] = 1.0f;
  }

  /* Ensure valid interpolation. */
  if (speed_max < speed) {
    speed_max = speed;
  }

  /* Interpolate between min/max speeds based on button state. */
  switch (actiondata->type) {
    case XR_BOOLEAN_INPUT:
      button_state = 1.0f;
      speed = speed_max;
      break;
    case XR_FLOAT_INPUT:
    case XR_VECTOR2F_INPUT: {
      button_state = (actiondata->type == XR_FLOAT_INPUT) ? fabsf(actiondata->state[0]) :
                                                            len_v2(actiondata->state);
      float speed_t = (actiondata->float_threshold < 1.0f) ?
                          (button_state - actiondata->float_threshold) /
                              (1.0f - actiondata->float_threshold) :
                          1.0f;
      if (speed_interp_cubic) {
        float start[2], end[2], p[2];

        start[0] = 0.0f;
        start[1] = speed;
        speed_p0[1] = speed + speed_p0[1] * (speed_max - speed);
        speed_p1[1] = speed + speed_p1[1] * (speed_max - speed);
        end[0] = 1.0f;
        end[1] = speed_max;

        interp_v2_v2v2v2v2_cubic(p, start, speed_p0, speed_p1, end, speed_t);
        speed = p[1];
      }
      else {
        speed += speed_t * (speed_max - speed);
      }
      break;
    }
    case XR_POSE_INPUT:
    case XR_VIBRATION_OUTPUT:
      BLI_assert_unreachable();
      break;
  }

  WM_xr_session_state_nav_location_get(xr, nav_pose.position);
  WM_xr_session_state_nav_rotation_get(xr, nav_pose.orientation_quat);
  wm_xr_pose_to_mat(&nav_pose, nav_mat);

  if (turn) {
    if (dir_lock || (snap_turn && data->is_finished) ||
        (snap_turn && button_state < RNA_float_get(op->ptr, "snap_turn_threshold")))
    {
      unit_m4(delta);
    }
    else {
      if (!snap_turn) {
        WM_xr_session_state_vignette_activate(xr);
        speed *= delta_time;
      }
      else {
        data->is_finished = true;
      }

      if (invert_rotation) {
        speed *= -1.0f;
      }

      GHOST_XrPose viewer_pose;
      float viewer_mat[4][4], nav_inv[4][4];

      WM_xr_session_state_viewer_pose_location_get(xr, viewer_pose.position);
      WM_xr_session_state_viewer_pose_rotation_get(xr, viewer_pose.orientation_quat);
      wm_xr_pose_to_mat(&viewer_pose, viewer_mat);
      wm_xr_pose_to_imat(&nav_pose, nav_inv);

      wm_xr_fly_compute_turn(mode, speed, viewer_mat, nav_mat, nav_inv, delta);
    }
  }
  else {
    float nav_scale, ref_quat[4];

    WM_xr_session_state_vignette_activate(xr);

    /* Adjust speed for base and navigation scale. */
    WM_xr_session_state_nav_scale_get(xr, &nav_scale);
    speed *= xr->session_settings.base_scale * nav_scale;

    if (!speed_frame_based) {
      speed *= delta_time;
    }

    switch (mode) {
      /* Move relative to navigation space. */
      case XR_FLY_FORWARD:
      case XR_FLY_BACK:
      case XR_FLY_LEFT:
      case XR_FLY_RIGHT:
      case XR_FLY_UP:
      case XR_FLY_DOWN:
        wm_xr_basenav_rotation_calc(xr, nav_pose.orientation_quat, ref_quat);
        break;
      /* Move relative to viewer. */
      case XR_FLY_VIEWER_FORWARD:
      case XR_FLY_VIEWER_BACK:
      case XR_FLY_VIEWER_LEFT:
      case XR_FLY_VIEWER_RIGHT:
        if (dir_lock) {
          copy_qt_qt(ref_quat, data->viewer_rot);
        }
        else {
          WM_xr_session_state_viewer_pose_rotation_get(xr, ref_quat);
        }
        break;
      /* Move relative to controller. */
      case XR_FLY_CONTROLLER_FORWARD:
        copy_qt_qt(ref_quat, actiondata->controller_rot);
        break;
      /* Unused. */
      case XR_FLY_TURNLEFT:
      case XR_FLY_TURNRIGHT:
        BLI_assert_unreachable();
        break;
    }

    wm_xr_fly_compute_move(mode, speed, ref_quat, nav_mat, locz_lock, delta);
  }

  mul_m4_m4m4(out, delta, nav_mat);

  WM_xr_session_state_nav_location_set(xr, out[3]);
  if (turn) {
    mat4_to_quat(nav_pose.orientation_quat, out);
    WM_xr_session_state_nav_rotation_set(xr, nav_pose.orientation_quat);
  }

  if (event->val == KM_PRESS) {
    return OPERATOR_RUNNING_MODAL;
  }

  /* XR events currently only support press and release. */
  BLI_assert_unreachable();
  wm_xr_fly_uninit(op);
  return OPERATOR_CANCELLED;
}

static void WM_OT_xr_navigation_fly(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "XR Navigation Fly";
  ot->idname = "WM_OT_xr_navigation_fly";
  ot->description = "Move/turn relative to the VR viewer or controller";

  /* Callbacks. */
  ot->invoke = wm_xr_navigation_fly_invoke;
  ot->exec = wm_xr_navigation_fly_exec;
  ot->cancel = wm_xr_navigation_fly_cancel;
  ot->modal = wm_xr_navigation_fly_modal;
  ot->poll = wm_xr_operator_sessionactive;

  /* Properties. */
  static const EnumPropertyItem fly_modes[] = {
      {XR_FLY_FORWARD, "FORWARD", 0, "Forward", "Move along navigation forward axis"},
      {XR_FLY_BACK, "BACK", 0, "Back", "Move along navigation back axis"},
      {XR_FLY_LEFT, "LEFT", 0, "Left", "Move along navigation left axis"},
      {XR_FLY_RIGHT, "RIGHT", 0, "Right", "Move along navigation right axis"},
      {XR_FLY_UP, "UP", 0, "Up", "Move along navigation up axis"},
      {XR_FLY_DOWN, "DOWN", 0, "Down", "Move along navigation down axis"},
      {XR_FLY_TURNLEFT,
       "TURNLEFT",
       0,
       "Turn Left",
       "Turn counter-clockwise around navigation up axis"},
      {XR_FLY_TURNRIGHT, "TURNRIGHT", 0, "Turn Right", "Turn clockwise around navigation up axis"},
      {XR_FLY_VIEWER_FORWARD,
       "VIEWER_FORWARD",
       0,
       "Viewer Forward",
       "Move along viewer's forward axis"},
      {XR_FLY_VIEWER_BACK, "VIEWER_BACK", 0, "Viewer Back", "Move along viewer's back axis"},
      {XR_FLY_VIEWER_LEFT, "VIEWER_LEFT", 0, "Viewer Left", "Move along viewer's left axis"},
      {XR_FLY_VIEWER_RIGHT, "VIEWER_RIGHT", 0, "Viewer Right", "Move along viewer's right axis"},
      {XR_FLY_CONTROLLER_FORWARD,
       "CONTROLLER_FORWARD",
       0,
       "Controller Forward",
       "Move along controller's forward axis"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const float default_speed_p0[2] = {0.0f, 0.0f};
  static const float default_speed_p1[2] = {1.0f, 1.0f};

  prop = RNA_def_enum(ot->srna, "mode", fly_modes, XR_FLY_VIEWER_FORWARD, "Mode", "Fly mode");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_NAVIGATION);

  RNA_def_float(ot->srna,
                "snap_turn_threshold",
                0.95f,
                0.0f,
                1.0f,
                "Snap Turn Threshold",
                "Input state threshold when using snap turn",
                0.0f,
                1.0f);
  RNA_def_boolean(
      ot->srna, "lock_location_z", false, "Lock Elevation", "Prevent changes to viewer elevation");
  RNA_def_boolean(ot->srna,
                  "lock_direction",
                  false,
                  "Lock Direction",
                  "Limit movement to viewer's initial direction");
  RNA_def_boolean(ot->srna,
                  "speed_frame_based",
                  false,
                  "Frame Based Speed",
                  "Apply fixed movement deltas every update");
  RNA_def_float(ot->srna,
                "turn_speed_factor",
                1.0 / 3.0f,
                0.0f,
                1.0f,
                "Turn Speed Factor",
                "Ratio between the min and max turn speed",
                0.0f,
                1.0f);
  RNA_def_float(ot->srna,
                "fly_speed_factor",
                1.0 / 3.0f,
                0.0f,
                1.0f,
                "Fly Speed Factor",
                "Ratio between the min and max fly speed",
                0.0f,
                1.0f);
  RNA_def_float_vector(ot->srna,
                       "speed_interpolation0",
                       2,
                       default_speed_p0,
                       0.0f,
                       1.0f,
                       "Speed Interpolation 0",
                       "First cubic spline control point between min/max speeds",
                       0.0f,
                       1.0f);
  RNA_def_float_vector(ot->srna,
                       "speed_interpolation1",
                       2,
                       default_speed_p1,
                       0.0f,
                       1.0f,
                       "Speed Interpolation 1",
                       "Second cubic spline control point between min/max speeds",
                       0.0f,
                       1.0f);

  RNA_def_enum(ot->srna,
               "alt_mode",
               fly_modes,
               XR_FLY_VIEWER_FORWARD,
               "Mode (Alt)",
               "Fly mode when hands are swapped");
  RNA_def_boolean(ot->srna,
                  "alt_lock_location_z",
                  false,
                  "Lock Elevation (Alt)",
                  "When hands are swapped, prevent changes to viewer elevation");
  RNA_def_boolean(ot->srna,
                  "alt_lock_direction",
                  false,
                  "Lock Direction (Alt)",
                  "When hands are swapped, limit movement to viewer's initial direction");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name XR Navigation Teleport
 *
 * Casts a ray from an XR controller's pose and teleports to any hit geometry.
 * \{ */

static float wm_xr_navigation_teleport_pose_calc(wmXrData *xr,
                                                 float nav_destination[3],
                                                 const float destination[4],
                                                 const float normal[3],
                                                 const bool teleport_axes[3],
                                                 float teleport_t,
                                                 float teleport_ofs,
                                                 float vertical_ofs)
{
  float nav_location[3], nav_rotation[4], viewer_location[3];
  WM_xr_session_state_nav_location_get(xr, nav_location);
  WM_xr_session_state_nav_rotation_get(xr, nav_rotation);
  WM_xr_session_state_viewer_pose_location_get(xr, viewer_location);

  float nav_axes[3][3], projected[3], v0[3], v1[3], destination_with_ofs[3];

  copy_v3_fl(nav_destination, 0.0f);
  copy_v3_v3(destination_with_ofs, destination);
  destination_with_ofs[2] += vertical_ofs;

  wm_xr_basenav_rotation_calc(xr, nav_rotation, nav_rotation);
  quat_to_mat3(nav_axes, nav_rotation);

  /* Project locations onto navigation axes. */
  for (int a = 0; a < 3; ++a) {
    project_v3_v3v3_normalized(projected, nav_location, nav_axes[a]);
    if (teleport_axes[a]) {
      /* Interpolate between projected locations. */
      project_v3_v3v3_normalized(v0, destination_with_ofs, nav_axes[a]);
      project_v3_v3v3_normalized(v1, viewer_location, nav_axes[a]);
      sub_v3_v3(v0, v1);
      madd_v3_v3fl(projected, v0, teleport_t);
      /* Subtract offset. */
      project_v3_v3v3_normalized(v0, normal, nav_axes[a]);
      madd_v3_v3fl(projected, v0, teleport_ofs);
    }
    /* Add to final location. */
    add_v3_v3(nav_destination, projected);
  }

  return len_v3v3(viewer_location, destination);
}

static bool wm_xr_navigation_teleport_ground_plane(float points[XR_MAX_RAYCASTS + 1][4],
                                                   int *num_points,
                                                   float *ray_dist)
{
  constexpr uint z = 2;
  for (int i = 1; i < *num_points; ++i) {
    float *startpoint = points[i - 1], *endpoint = points[i];

    if ((startpoint[z] < 0) == (endpoint[z] < 0)) {
      continue;
    }

    if (startpoint[z] == endpoint[z]) {
      break;
    }

    float segment_ray_dist = len_v3v3(startpoint, endpoint);
    float alpha = startpoint[z] / (startpoint[z] - endpoint[z]);
    interp_v3_v3v3(endpoint, startpoint, endpoint, alpha);

    *ray_dist = segment_ray_dist * (i - 1) + len_v3v3(startpoint, endpoint);
    *num_points = i + 1;
    return true;
  }

  return false;
}

static XrRaycastResult wm_xr_navigation_teleport(bContext *C,
                                                 wmXrData *xr,
                                                 float nav_destination[3],
                                                 float points[XR_MAX_RAYCASTS + 1][4],
                                                 const float direction[3],
                                                 int *num_points,
                                                 float *ray_dist,
                                                 float *destination_dist,
                                                 bool selectable_only,
                                                 const bool teleport_axes[3],
                                                 float teleport_t,
                                                 float teleport_ofs,
                                                 float gravity,
                                                 float head_height)
{
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  int index;
  const Object *ob = nullptr;
  float obmat[4][4];

  float normal[3], segment_direction[3];
  float vertical_ofs = 0;
  XrRaycastResult result = XR_RAYCAST_MISS;

  copy_v3_v3(segment_direction, direction);
  copy_v3_fl3(normal, 0, 1, 0);

  /* When ray_dist == 0 or -1, the raycast is a line of infinite length. */
  if (*ray_dist <= 0.0f) {
    *num_points = 2;
  }

  const float segment_length = *ray_dist / (*num_points - 1);
  float segment_ray_dist = 0.0f;
  *ray_dist = 0.0f;

  for (int i = 1; i < *num_points; ++i) {
    segment_ray_dist = segment_length;
    wm_xr_raycast(scene,
                  depsgraph,
                  points[i - 1],
                  segment_direction,
                  &segment_ray_dist,
                  selectable_only,
                  points[i],
                  normal,
                  &index,
                  &ob,
                  obmat);

    *ray_dist += segment_ray_dist;

    if (ob) {
      *num_points = i + 1;

      /** Ensure normal faces the correct direction */
      if (dot_v3v3(segment_direction, normal) > 0) {
        mul_v3_fl(normal, -1.0f);
      }

      result = XR_RAYCAST_HIT;
      break;
    }

    madd_v3_v3v3fl(points[i], points[i - 1], segment_direction, segment_length);

    /* Apply gravity */
    segment_direction[2] -= gravity;
    normalize_v3(segment_direction);
  }

  /** Fall back to raycast intersecting with the ground plane. */
  if (result == XR_RAYCAST_MISS) {
    vertical_ofs = head_height;

    if (wm_xr_navigation_teleport_ground_plane(points, num_points, ray_dist)) {
      result = XR_RAYCAST_FALLBACK;
    }
  }

  if (result != XR_RAYCAST_MISS) {
    float origin[3], dummy_dest[3], dummy_normal[3];

    /* Raycast downward to see if we're on the floor */
    copy_v3_fl3(segment_direction, 0, 0, -1);

    copy_v3_v3(origin, points[*num_points - 1]);
    madd_v3_v3fl(origin, normal, teleport_ofs);
    madd_v3_v3fl(origin, segment_direction, -vertical_ofs);

    segment_ray_dist = head_height;
    ob = nullptr;
    wm_xr_raycast(scene,
                  depsgraph,
                  origin,
                  segment_direction,
                  &segment_ray_dist,
                  selectable_only,
                  dummy_dest,
                  dummy_normal,
                  &index,
                  &ob,
                  obmat);

    /* Raycast upward to make sure we don't clip through the ceiling */
    if (ob) {
      vertical_ofs = head_height - segment_ray_dist;
      copy_v3_fl3(segment_direction, 0, 0, 1);

      copy_v3_v3(origin, points[*num_points - 1]);
      madd_v3_v3fl(origin, normal, teleport_ofs);

      segment_ray_dist = vertical_ofs;
      ob = nullptr;
      wm_xr_raycast(scene,
                    depsgraph,
                    origin,
                    segment_direction,
                    &segment_ray_dist,
                    selectable_only,
                    dummy_dest,
                    dummy_normal,
                    &index,
                    &ob,
                    obmat);

      if (ob) {
        vertical_ofs = max_ff(0.0f, segment_ray_dist - teleport_ofs);
      }
    }

    /* Calculate teleportation destination in navigation space */
    *destination_dist = wm_xr_navigation_teleport_pose_calc(xr,
                                                            nav_destination,
                                                            points[*num_points - 1],
                                                            normal,
                                                            teleport_axes,
                                                            teleport_t,
                                                            teleport_ofs,
                                                            vertical_ofs);
  }

  return result;
}

static wmOperatorStatus wm_xr_navigation_teleport_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent *event)
{
  if (!wm_xr_operator_test_event(op, event)) {
    return OPERATOR_PASS_THROUGH;
  }

  wm_xr_raycast_init(op);

  const wmOperatorStatus retval = op->type->modal(C, op, event);
  OPERATOR_RETVAL_CHECK(retval);

  if (retval & OPERATOR_RUNNING_MODAL) {
    WM_event_add_modal_handler(C, op);
  }

  return retval;
}

static wmOperatorStatus wm_xr_navigation_teleport_exec(bContext * /*C*/, wmOperator * /*op*/)
{
  return OPERATOR_CANCELLED;
}

static void wm_xr_navigation_teleport_cancel(bContext * /*C*/, wmOperator *op)
{
  wm_xr_raycast_uninit(op);
}

static wmOperatorStatus wm_xr_navigation_teleport_modal(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent *event)
{
  if (!wm_xr_operator_test_event(op, event)) {
    return OPERATOR_PASS_THROUGH;
  }

  const wmXrActionData *actiondata = static_cast<const wmXrActionData *>(event->customdata);
  wmWindowManager *wm = CTX_wm_manager(C);
  wmXrData *xr = &wm->xr;

  xr->runtime->session_state.is_raycast_shown = true;
  wm_xr_raycast_update(op, xr, actiondata);

  XrRaycastData *data = static_cast<XrRaycastData *>(op->customdata);
  float nav_scale, ray_dist, destination_dist, nav_destination[3];
  bool teleport_axes[3];

  WM_xr_session_state_nav_scale_get(xr, &nav_scale);

  RNA_boolean_get_array(op->ptr, "teleport_axes", teleport_axes);
  const float teleport_t = RNA_float_get(op->ptr, "interpolation");
  const float teleport_ofs = RNA_float_get(op->ptr, "offset") * nav_scale;
  const float gravity = RNA_float_get(op->ptr, "gravity");
  const float head_height = xr->runtime->session_state.prev_local_pose.position[1] * nav_scale;
  const bool selectable_only = RNA_boolean_get(op->ptr, "selectable_only");
  ray_dist = RNA_float_get(op->ptr, "distance") * nav_scale;

  data->num_points = XR_MAX_RAYCASTS + 1;
  data->result = wm_xr_navigation_teleport(C,
                                           xr,
                                           nav_destination,
                                           data->points,
                                           data->direction,
                                           &data->num_points,
                                           &ray_dist,
                                           &destination_dist,
                                           selectable_only,
                                           teleport_axes,
                                           teleport_t,
                                           teleport_ofs,
                                           gravity,
                                           head_height);

  data->destination_size = RNA_float_get(op->ptr, "destination_scale") *
                           sqrt(destination_dist / nav_scale) * nav_scale;

  switch (data->result) {
    case XR_RAYCAST_MISS:
      RNA_float_get_array(op->ptr, "miss_color", data->color);
      break;
    case XR_RAYCAST_HIT:
      RNA_float_get_array(op->ptr, "hit_color", data->color);
      break;
    case XR_RAYCAST_FALLBACK:
      RNA_float_get_array(op->ptr, "fallback_color", data->color);
      break;
    default:
      BLI_assert_unreachable();
      break;
  }

  switch (event->val) {
    case KM_PRESS:
      return OPERATOR_RUNNING_MODAL;
    case KM_RELEASE: {
      if (data->result != XR_RAYCAST_MISS) {
        WM_xr_session_state_nav_location_set(xr, nav_destination);
      }

      xr->runtime->session_state.is_raycast_shown = false;
      wm_xr_raycast_uninit(op);

      return OPERATOR_FINISHED;
    }
    default:
      /* XR events currently only support press and release. */
      BLI_assert_unreachable();
      wm_xr_raycast_uninit(op);
      return OPERATOR_CANCELLED;
  }
}

static void WM_OT_xr_navigation_teleport(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "XR Navigation Teleport";
  ot->idname = "WM_OT_xr_navigation_teleport";
  ot->description = "Set VR viewer location to controller raycast hit location";

  /* Callbacks. */
  ot->invoke = wm_xr_navigation_teleport_invoke;
  ot->exec = wm_xr_navigation_teleport_exec;
  ot->cancel = wm_xr_navigation_teleport_cancel;
  ot->modal = wm_xr_navigation_teleport_modal;
  ot->poll = wm_xr_operator_sessionactive;

  /* Properties. */
  static const bool default_teleport_axes[3] = {true, true, true};

  RNA_def_boolean_vector(ot->srna,
                         "teleport_axes",
                         3,
                         default_teleport_axes,
                         "Teleport Axes",
                         "Enabled teleport axes in navigation space");
  RNA_def_float(ot->srna,
                "interpolation",
                1.0f,
                0.0f,
                1.0f,
                "Interpolation",
                "Interpolation factor between viewer and hit locations",
                0.0f,
                1.0f);
  RNA_def_float(ot->srna,
                "offset",
                0.25f,
                0.0f,
                FLT_MAX,
                "Offset",
                "Offset along hit normal to subtract from final location",
                0.0f,
                FLT_MAX);
  RNA_def_boolean(ot->srna,
                  "selectable_only",
                  true,
                  "Selectable Only",
                  "Only allow selectable objects to influence raycast result");
  RNA_def_float(ot->srna,
                "distance",
                80.0,
                0.0,
                BVH_RAYCAST_DIST_MAX,
                "",
                "Maximum raycast distance",
                0.0,
                BVH_RAYCAST_DIST_MAX);
  RNA_def_float(ot->srna,
                "gravity",
                0.1,
                0.0,
                FLT_MAX,
                "Gravity",
                "Downward curvature applied to raycast",
                0.0,
                FLT_MAX);
  RNA_def_float(ot->srna,
                "raycast_scale",
                0.02f,
                0.0f,
                FLT_MAX,
                "Raycast Scale",
                "Width of the raycast visualization",
                0.0f,
                FLT_MAX);
  RNA_def_float(ot->srna,
                "destination_scale",
                0.05f,
                0.0f,
                FLT_MAX,
                "Destination Scale",
                "Width of the destination visualization",
                0.0f,
                FLT_MAX);
  RNA_def_int(ot->srna,
              "sample_count",
              48,
              2,
              INT_MAX,
              "Sample Count",
              "Number of interpolation samples for the raycast visualization",
              2,
              INT_MAX);
  RNA_def_boolean(
      ot->srna, "from_viewer", false, "From Viewer", "Use viewer pose as raycast origin");
  RNA_def_float_vector(ot->srna,
                       "axis",
                       3,
                       g_xr_default_raycast_axis,
                       -1.0f,
                       1.0f,
                       "Axis",
                       "Raycast axis in controller/viewer space",
                       -1.0f,
                       1.0f);
  RNA_def_float_color(ot->srna,
                      "hit_color",
                      4,
                      g_xr_default_raycast_hit_color,
                      0.0f,
                      1.0f,
                      "Hit Color",
                      "Color of raycast when it succeeds",
                      0.0f,
                      1.0f);
  RNA_def_float_color(ot->srna,
                      "miss_color",
                      4,
                      g_xr_default_raycast_miss_color,
                      0.0f,
                      1.0f,
                      "Miss Color",
                      "Color of raycast when it misses",
                      0.0f,
                      1.0f);
  RNA_def_float_color(ot->srna,
                      "fallback_color",
                      4,
                      g_xr_default_raycast_fallback_color,
                      0.0f,
                      1.0f,
                      "Fallback Color",
                      "Color of raycast when a fallback case succeeds",
                      0.0f,
                      1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name XR Navigation Reset
 *
 * Resets XR navigation deltas relative to session base pose.
 * \{ */

static wmOperatorStatus wm_xr_navigation_reset_exec(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmXrData *xr = &wm->xr;
  bool reset_loc, reset_rot, reset_scale;

  reset_loc = RNA_boolean_get(op->ptr, "location");
  reset_rot = RNA_boolean_get(op->ptr, "rotation");
  reset_scale = RNA_boolean_get(op->ptr, "scale");

  if (reset_loc) {
    float loc[3];
    if (!reset_scale) {
      float nav_rotation[4], nav_scale;

      WM_xr_session_state_nav_rotation_get(xr, nav_rotation);
      WM_xr_session_state_nav_scale_get(xr, &nav_scale);

      /* Adjust location based on scale. */
      mul_v3_v3fl(loc, xr->runtime->session_state.prev_base_pose.position, nav_scale);
      sub_v3_v3(loc, xr->runtime->session_state.prev_base_pose.position);
      mul_qt_v3(nav_rotation, loc);
      negate_v3(loc);
    }
    else {
      zero_v3(loc);
    }
    WM_xr_session_state_nav_location_set(xr, loc);
  }

  if (reset_rot) {
    float rot[4];
    unit_qt(rot);
    WM_xr_session_state_nav_rotation_set(xr, rot);
  }

  if (reset_scale) {
    if (!reset_loc) {
      float nav_location[3], nav_rotation[4], nav_scale;
      float nav_axes[3][3], v[3];

      WM_xr_session_state_nav_location_get(xr, nav_location);
      WM_xr_session_state_nav_rotation_get(xr, nav_rotation);
      WM_xr_session_state_nav_scale_get(xr, &nav_scale);

      /* Offset any location changes when changing scale. */
      mul_v3_v3fl(v, xr->runtime->session_state.prev_base_pose.position, nav_scale);
      sub_v3_v3(v, xr->runtime->session_state.prev_base_pose.position);
      mul_qt_v3(nav_rotation, v);
      add_v3_v3(nav_location, v);

      /* Reset elevation to base pose value. */
      quat_to_mat3(nav_axes, nav_rotation);
      project_v3_v3v3_normalized(v, nav_location, nav_axes[2]);
      sub_v3_v3(nav_location, v);

      WM_xr_session_state_nav_location_set(xr, nav_location);
    }
    WM_xr_session_state_nav_scale_set(xr, 1.0f);
  }

  return OPERATOR_FINISHED;
}

static void WM_OT_xr_navigation_reset(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "XR Navigation Reset";
  ot->idname = "WM_OT_xr_navigation_reset";
  ot->description = "Reset VR navigation deltas relative to session base pose";

  /* Callbacks. */
  ot->exec = wm_xr_navigation_reset_exec;
  ot->poll = wm_xr_operator_sessionactive;

  /* Properties. */
  RNA_def_boolean(ot->srna, "location", true, "Location", "Reset location deltas");
  RNA_def_boolean(ot->srna, "rotation", true, "Rotation", "Reset rotation deltas");
  RNA_def_boolean(ot->srna, "scale", true, "Scale", "Reset scale deltas");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name XR Navigation Swap Hands
 *
 * Resets XR navigation deltas relative to session base pose.
 * \{ */

static wmOperatorStatus wm_xr_navigation_swap_hands_invoke(bContext *C,
                                                           wmOperator *op,
                                                           const wmEvent *event)
{
  if (!wm_xr_operator_test_event(op, event)) {
    return OPERATOR_PASS_THROUGH;
  }

  WM_event_add_modal_handler(C, op);

  wmWindowManager *wm = CTX_wm_manager(C);
  wmXrData *xr = &wm->xr;

  xr->runtime->session_state.swap_hands = true;

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus wm_xr_navigation_swap_hands_exec(bContext * /*C*/, wmOperator * /*op*/)
{
  return OPERATOR_CANCELLED;
}

static wmOperatorStatus wm_xr_navigation_swap_hands_modal(bContext *C,
                                                          wmOperator *op,
                                                          const wmEvent *event)
{
  if (!wm_xr_operator_test_event(op, event)) {
    return OPERATOR_PASS_THROUGH;
  }

  wmWindowManager *wm = CTX_wm_manager(C);
  wmXrData *xr = &wm->xr;

  switch (event->val) {
    case KM_PRESS:
      return OPERATOR_RUNNING_MODAL;
    case KM_RELEASE:
      xr->runtime->session_state.swap_hands = false;
      return OPERATOR_FINISHED;
    default:
      BLI_assert_unreachable();
      return OPERATOR_CANCELLED;
  }
}

static void WM_OT_xr_navigation_swap_hands(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "XR Navigation Swap Hands";
  ot->idname = "WM_OT_xr_navigation_swap_hands";
  ot->description = "Swap VR navigation controls between left / right controllers";

  /* Callbacks. */
  ot->invoke = wm_xr_navigation_swap_hands_invoke;
  ot->exec = wm_xr_navigation_swap_hands_exec;
  ot->modal = wm_xr_navigation_swap_hands_modal;
  ot->poll = wm_xr_operator_sessionactive;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Registration
 * \{ */

void wm_xr_operatortypes_register()
{
  WM_operatortype_append(WM_OT_xr_session_toggle);
  WM_operatortype_append(WM_OT_xr_navigation_grab);
  WM_operatortype_append(WM_OT_xr_navigation_fly);
  WM_operatortype_append(WM_OT_xr_navigation_teleport);
  WM_operatortype_append(WM_OT_xr_navigation_reset);
  WM_operatortype_append(WM_OT_xr_navigation_swap_hands);
}

/** \} */
