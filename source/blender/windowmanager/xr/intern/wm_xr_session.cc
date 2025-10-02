/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#include "BKE_callbacks.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_time.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "DNA_camera_types.h"
#include "DNA_space_types.h"

#include "DRW_engine.hh"

#include "ED_screen.hh"
#include "ED_space_api.hh"

#include "GHOST_C-api.h"

#include "GPU_batch.hh"
#include "GPU_viewport.hh"

#include "MEM_guardedalloc.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm_event_system.hh"
#include "wm_surface.hh"
#include "wm_window.hh"
#include "wm_xr_intern.hh"

static wmSurface *g_xr_surface = nullptr;
static CLG_LogRef LOG = {"xr"};

/* -------------------------------------------------------------------- */

static void wm_xr_session_create_cb()
{
  Main *bmain = G_MAIN;
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  wmXrData *xr_data = &wm->xr;
  wmXrSessionState *state = &xr_data->runtime->session_state;
  XrSessionSettings *settings = &xr_data->session_settings;

  /* Get action set data from Python. */
  BKE_callback_exec_null(bmain, BKE_CB_EVT_XR_SESSION_START_PRE);

  wm_xr_session_actions_init(xr_data);

  /* Initialize navigation. */
  WM_xr_session_state_navigation_reset(state);
  if (settings->base_scale < FLT_EPSILON) {
    settings->base_scale = 1.0f;
  }
  state->prev_base_scale = settings->base_scale;

  /* Initialize vignette. */
  state->vignette_data = MEM_callocN<wmXrVignetteData>(__func__);
  WM_xr_session_state_vignette_reset(state);
}

static void wm_xr_session_controller_data_free(wmXrSessionState *state)
{
  ListBase *lb = &state->controllers;
  while (wmXrController *c = static_cast<wmXrController *>(BLI_pophead(lb))) {
    if (c->model) {
      GPU_batch_discard(c->model);
    }
    BLI_freelinkN(lb, c);
  }
}

static void wm_xr_session_vignette_data_free(wmXrSessionState *state)
{
  if (state->vignette_data) {
    MEM_freeN(state->vignette_data);
    state->vignette_data = nullptr;
  }
}

static void wm_xr_session_raycast_model_free(wmXrSessionState *state)
{
  if (state->raycast_model) {
    GPU_batch_discard(state->raycast_model);
    state->raycast_model = nullptr;
  }
}

void wm_xr_session_data_free(wmXrSessionState *state)
{
  wm_xr_session_controller_data_free(state);
  wm_xr_session_vignette_data_free(state);
  wm_xr_session_raycast_model_free(state);
}

static void wm_xr_session_exit_cb(void *customdata)
{
  wmXrData *xr_data = static_cast<wmXrData *>(customdata);
  if (!xr_data->runtime) {
    return;
  }

  xr_data->runtime->session_state.is_started = false;

  if (xr_data->runtime->exit_fn) {
    xr_data->runtime->exit_fn(xr_data);
  }

  /* Free the entire runtime data (including session state and context), to play safe. */
  wm_xr_runtime_data_free(&xr_data->runtime);
}

static void wm_xr_session_begin_info_create(wmXrData *xr_data,
                                            GHOST_XrSessionBeginInfo *r_begin_info)
{
  /* Callback for when the session is created. This is needed to create and bind OpenXR actions
   * after the session is created but before it is started. */
  r_begin_info->create_fn = wm_xr_session_create_cb;

  /* WM-XR exit function, does some of its own stuff and calls callback passed to
   * wm_xr_session_toggle(), to allow external code to execute its own session-exit logic. */
  r_begin_info->exit_fn = wm_xr_session_exit_cb;
  r_begin_info->exit_customdata = xr_data;
}

void wm_xr_session_toggle(wmWindowManager *wm,
                          wmWindow *session_root_win,
                          wmXrSessionExitFn session_exit_fn)
{
  wmXrData *xr_data = &wm->xr;

  if (WM_xr_session_exists(xr_data)) {
    /* Must set first, since #GHOST_XrSessionEnd() may immediately free the runtime. */
    xr_data->runtime->session_state.is_started = false;

    GHOST_XrSessionEnd(xr_data->runtime->context);
  }
  else {
    GHOST_XrSessionBeginInfo begin_info;

    xr_data->runtime->session_root_win = session_root_win;
    xr_data->runtime->session_state.is_started = true;
    xr_data->runtime->exit_fn = session_exit_fn;

    wm_xr_session_begin_info_create(xr_data, &begin_info);
    GHOST_XrSessionStart(xr_data->runtime->context, &begin_info);
  }
}

bool WM_xr_session_exists(const wmXrData *xr)
{
  return xr->runtime && xr->runtime->context && xr->runtime->session_state.is_started;
}

void WM_xr_session_base_pose_reset(wmXrData *xr)
{
  xr->runtime->session_state.force_reset_to_base_pose = true;
}

bool WM_xr_session_is_ready(const wmXrData *xr)
{
  return WM_xr_session_exists(xr) && GHOST_XrSessionIsRunning(xr->runtime->context);
}

static void wm_xr_session_base_pose_calc(const Scene *scene,
                                         const XrSessionSettings *settings,
                                         GHOST_XrPose *r_base_pose,
                                         float *r_base_scale)
{
  const Object *base_pose_object = ((settings->base_pose_type == XR_BASE_POSE_OBJECT) &&
                                    settings->base_pose_object) ?
                                       settings->base_pose_object :
                                       scene->camera;

  if (settings->base_pose_type == XR_BASE_POSE_CUSTOM) {
    float tmp_quatx[4], tmp_quatz[4];

    copy_v3_v3(r_base_pose->position, settings->base_pose_location);
    axis_angle_to_quat_single(tmp_quatx, 'X', M_PI_2);
    axis_angle_to_quat_single(tmp_quatz, 'Z', settings->base_pose_angle);
    mul_qt_qtqt(r_base_pose->orientation_quat, tmp_quatz, tmp_quatx);
  }
  else if (base_pose_object) {
    float tmp_quat[4];
    float tmp_eul[3];

    mat4_to_loc_quat(r_base_pose->position, tmp_quat, base_pose_object->object_to_world().ptr());

    /* Only use rotation around Z-axis to align view with floor. */
    quat_to_eul(tmp_eul, tmp_quat);
    tmp_eul[0] = M_PI_2;
    tmp_eul[1] = 0;
    eul_to_quat(r_base_pose->orientation_quat, tmp_eul);
  }
  else {
    copy_v3_fl(r_base_pose->position, 0.0f);
    axis_angle_to_quat_single(r_base_pose->orientation_quat, 'X', M_PI_2);
  }

  *r_base_scale = settings->base_scale;
}

static void wm_xr_session_draw_data_populate(wmXrData *xr_data,
                                             Scene *scene,
                                             Depsgraph *depsgraph,
                                             wmXrDrawData *r_draw_data)
{
  const XrSessionSettings *settings = &xr_data->session_settings;

  memset(r_draw_data, 0, sizeof(*r_draw_data));
  r_draw_data->scene = scene;
  r_draw_data->depsgraph = depsgraph;
  r_draw_data->xr_data = xr_data;
  r_draw_data->surface_data = static_cast<wmXrSurfaceData *>(g_xr_surface->customdata);

  wm_xr_session_base_pose_calc(
      r_draw_data->scene, settings, &r_draw_data->base_pose, &r_draw_data->base_scale);
}

wmWindow *wm_xr_session_root_window_or_fallback_get(const wmWindowManager *wm,
                                                    const wmXrRuntimeData *runtime_data)
{
  if (runtime_data->session_root_win &&
      BLI_findindex(&wm->windows, runtime_data->session_root_win) != -1)
  {
    /* Root window is still valid, use it. */
    return runtime_data->session_root_win;
  }
  /* Otherwise, fall back. */
  return static_cast<wmWindow *>(wm->windows.first);
}

/**
 * Get the scene and depsgraph shown in the VR session's root window (the window the session was
 * started from) if still available. If it's not available, use some fallback window.
 *
 * It's important that the VR session follows some existing window, otherwise it would need to have
 * its own depsgraph, which is an expense we should avoid.
 */
static void wm_xr_session_scene_and_depsgraph_get(const wmWindowManager *wm,
                                                  Scene **r_scene,
                                                  Depsgraph **r_depsgraph)
{
  const wmWindow *root_win = wm_xr_session_root_window_or_fallback_get(wm, wm->xr.runtime);

  /* Follow the scene & view layer shown in the root 3D View. */
  Scene *scene = WM_window_get_active_scene(root_win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(root_win);

  Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer);
  BLI_assert(scene && view_layer && depsgraph);
  *r_scene = scene;
  *r_depsgraph = depsgraph;
}

enum wmXrSessionStateEvent {
  SESSION_STATE_EVENT_NONE = 0,
  SESSION_STATE_EVENT_START,
  SESSION_STATE_EVENT_RESET_TO_BASE_POSE,
  SESSION_STATE_EVENT_POSITION_TRACKING_TOGGLE,
};

static bool wm_xr_session_draw_data_needs_reset_to_base_pose(const wmXrSessionState *state,
                                                             const XrSessionSettings *settings)
{
  if (state->force_reset_to_base_pose) {
    return true;
  }
  return ((settings->flag & XR_SESSION_USE_POSITION_TRACKING) == 0) &&
         ((state->prev_base_pose_type != settings->base_pose_type) ||
          (state->prev_base_pose_object != settings->base_pose_object));
}

static wmXrSessionStateEvent wm_xr_session_state_to_event(const wmXrSessionState *state,
                                                          const XrSessionSettings *settings)
{
  if (!state->is_view_data_set) {
    return SESSION_STATE_EVENT_START;
  }
  if (wm_xr_session_draw_data_needs_reset_to_base_pose(state, settings)) {
    return SESSION_STATE_EVENT_RESET_TO_BASE_POSE;
  }

  const bool position_tracking_toggled = ((state->prev_settings_flag &
                                           XR_SESSION_USE_POSITION_TRACKING) !=
                                          (settings->flag & XR_SESSION_USE_POSITION_TRACKING));
  if (position_tracking_toggled) {
    return SESSION_STATE_EVENT_POSITION_TRACKING_TOGGLE;
  }

  return SESSION_STATE_EVENT_NONE;
}

void wm_xr_session_draw_data_update(wmXrSessionState *state,
                                    const XrSessionSettings *settings,
                                    const GHOST_XrDrawViewInfo *draw_view,
                                    wmXrDrawData *draw_data)
{
  const wmXrSessionStateEvent event = wm_xr_session_state_to_event(state, settings);
  const bool use_position_tracking = (settings->flag & XR_SESSION_USE_POSITION_TRACKING);

  switch (event) {
    case SESSION_STATE_EVENT_START:
      if (use_position_tracking) {
        /* We want to start the session exactly at landmark position.
         * Run-times may have a non-[0,0,0] starting position that we have to subtract for that. */
        copy_v3_v3(draw_data->eye_position_ofs, draw_view->local_pose.position);
      }
      else {
        copy_v3_fl(draw_data->eye_position_ofs, 0.0f);
      }
      break;
    /* This should be triggered by the VR add-on if a landmark changes. */
    case SESSION_STATE_EVENT_RESET_TO_BASE_POSE:
      if (use_position_tracking) {
        /* Switch exactly to base pose, so use eye offset to cancel out current position delta. */
        copy_v3_v3(draw_data->eye_position_ofs, draw_view->local_pose.position);
      }
      else {
        copy_v3_fl(draw_data->eye_position_ofs, 0.0f);
      }
      /* Reset navigation. */
      WM_xr_session_state_navigation_reset(state);
      break;
    case SESSION_STATE_EVENT_POSITION_TRACKING_TOGGLE:
      if (use_position_tracking) {
        /* Keep the current position, and let the user move from there. */
        copy_v3_v3(draw_data->eye_position_ofs, state->prev_eye_position_ofs);
      }
      else {
        /* Back to the exact base-pose position. */
        copy_v3_fl(draw_data->eye_position_ofs, 0.0f);
      }
      break;
    case SESSION_STATE_EVENT_NONE:
      /* Keep previous offset when positional tracking is disabled. */
      copy_v3_v3(draw_data->eye_position_ofs, state->prev_eye_position_ofs);
      break;
  }
}

void wm_xr_session_state_update(const XrSessionSettings *settings,
                                const wmXrDrawData *draw_data,
                                const GHOST_XrDrawViewInfo *draw_view,
                                wmXrSessionState *state)
{
  GHOST_XrPose viewer_pose;
  float viewer_mat[4][4], base_mat[4][4], nav_mat[4][4];

  /* Calculate viewer matrix. */
  copy_qt_qt(viewer_pose.orientation_quat, draw_view->local_pose.orientation_quat);
  if ((settings->flag & XR_SESSION_USE_POSITION_TRACKING) == 0) {
    zero_v3(viewer_pose.position);
  }
  else {
    copy_v3_v3(viewer_pose.position, draw_view->local_pose.position);
  }
  if ((settings->flag & XR_SESSION_USE_ABSOLUTE_TRACKING) == 0) {
    sub_v3_v3(viewer_pose.position, draw_data->eye_position_ofs);
  }
  wm_xr_pose_to_mat(&viewer_pose, viewer_mat);

  /* Apply base pose and navigation. */
  wm_xr_pose_scale_to_mat(&draw_data->base_pose, draw_data->base_scale, base_mat);
  wm_xr_pose_scale_to_mat(&state->nav_pose_prev, state->nav_scale_prev, nav_mat);
  mul_m4_m4m4(state->viewer_mat_base, base_mat, viewer_mat);
  mul_m4_m4m4(viewer_mat, nav_mat, state->viewer_mat_base);

  /* Save final viewer pose and viewmat. */
  mat4_to_loc_quat(state->viewer_pose.position, state->viewer_pose.orientation_quat, viewer_mat);
  wm_xr_pose_scale_to_imat(
      &state->viewer_pose, draw_data->base_scale * state->nav_scale_prev, state->viewer_viewmat);

  /* No idea why, but multiplying by two seems to make it match the VR view more. */
  state->focal_len = 2.0f *
                     fov_to_focallength(draw_view->fov.angle_right - draw_view->fov.angle_left,
                                        DEFAULT_SENSOR_WIDTH);

  copy_v3_v3(state->prev_eye_position_ofs, draw_data->eye_position_ofs);
  memcpy(&state->prev_base_pose, &draw_data->base_pose, sizeof(state->prev_base_pose));
  state->prev_base_scale = draw_data->base_scale;
  memcpy(&state->prev_local_pose, &draw_view->local_pose, sizeof(state->prev_local_pose));
  copy_v3_v3(state->prev_eye_position_ofs, draw_data->eye_position_ofs);

  state->prev_settings_flag = settings->flag;
  state->prev_base_pose_type = settings->base_pose_type;
  state->prev_base_pose_object = settings->base_pose_object;
  state->is_view_data_set = true;
  /* Assume this was already done through wm_xr_session_draw_data_update(). */
  state->force_reset_to_base_pose = false;

  WM_xr_session_state_vignette_update(state);
}

wmXrSessionState *WM_xr_session_state_handle_get(const wmXrData *xr)
{
  return xr->runtime ? &xr->runtime->session_state : nullptr;
}

ScrArea *WM_xr_session_area_get(const wmXrData *xr)
{
  return xr->runtime ? xr->runtime->area : nullptr;
}

bool WM_xr_session_state_viewer_pose_location_get(const wmXrData *xr, float r_location[3])
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set) {
    zero_v3(r_location);
    return false;
  }

  copy_v3_v3(r_location, xr->runtime->session_state.viewer_pose.position);
  return true;
}

bool WM_xr_session_state_viewer_pose_rotation_get(const wmXrData *xr, float r_rotation[4])
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set) {
    unit_qt(r_rotation);
    return false;
  }

  copy_v4_v4(r_rotation, xr->runtime->session_state.viewer_pose.orientation_quat);
  return true;
}

bool WM_xr_session_state_viewer_pose_matrix_info_get(const wmXrData *xr,
                                                     float r_viewmat[4][4],
                                                     float *r_focal_len)
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set) {
    unit_m4(r_viewmat);
    *r_focal_len = 0.0f;
    return false;
  }

  copy_m4_m4(r_viewmat, xr->runtime->session_state.viewer_viewmat);
  *r_focal_len = xr->runtime->session_state.focal_len;

  return true;
}

bool WM_xr_session_state_controller_grip_location_get(const wmXrData *xr,
                                                      uint subaction_idx,
                                                      float r_location[3])
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set ||
      (subaction_idx >= BLI_listbase_count(&xr->runtime->session_state.controllers)))
  {
    zero_v3(r_location);
    return false;
  }

  const wmXrController *controller = static_cast<const wmXrController *>(
      BLI_findlink(&xr->runtime->session_state.controllers, subaction_idx));
  BLI_assert(controller);
  copy_v3_v3(r_location, controller->grip_pose.position);
  return true;
}

bool WM_xr_session_state_controller_grip_rotation_get(const wmXrData *xr,
                                                      uint subaction_idx,
                                                      float r_rotation[4])
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set ||
      (subaction_idx >= BLI_listbase_count(&xr->runtime->session_state.controllers)))
  {
    unit_qt(r_rotation);
    return false;
  }

  const wmXrController *controller = static_cast<const wmXrController *>(
      BLI_findlink(&xr->runtime->session_state.controllers, subaction_idx));
  BLI_assert(controller);
  copy_qt_qt(r_rotation, controller->grip_pose.orientation_quat);
  return true;
}

bool WM_xr_session_state_controller_aim_location_get(const wmXrData *xr,
                                                     uint subaction_idx,
                                                     float r_location[3])
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set ||
      (subaction_idx >= BLI_listbase_count(&xr->runtime->session_state.controllers)))
  {
    zero_v3(r_location);
    return false;
  }

  const wmXrController *controller = static_cast<const wmXrController *>(
      BLI_findlink(&xr->runtime->session_state.controllers, subaction_idx));
  BLI_assert(controller);
  copy_v3_v3(r_location, controller->aim_pose.position);
  return true;
}

bool WM_xr_session_state_controller_aim_rotation_get(const wmXrData *xr,
                                                     uint subaction_idx,
                                                     float r_rotation[4])
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set ||
      (subaction_idx >= BLI_listbase_count(&xr->runtime->session_state.controllers)))
  {
    unit_qt(r_rotation);
    return false;
  }

  const wmXrController *controller = static_cast<const wmXrController *>(
      BLI_findlink(&xr->runtime->session_state.controllers, subaction_idx));
  BLI_assert(controller);
  copy_qt_qt(r_rotation, controller->aim_pose.orientation_quat);
  return true;
}

bool WM_xr_session_state_nav_location_get(const wmXrData *xr, float r_location[3])
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set) {
    zero_v3(r_location);
    return false;
  }

  copy_v3_v3(r_location, xr->runtime->session_state.nav_pose.position);
  return true;
}

void WM_xr_session_state_nav_location_set(wmXrData *xr, const float location[3])
{
  if (WM_xr_session_exists(xr)) {
    copy_v3_v3(xr->runtime->session_state.nav_pose.position, location);
    xr->runtime->session_state.is_navigation_dirty = true;
  }
}

bool WM_xr_session_state_nav_rotation_get(const wmXrData *xr, float r_rotation[4])
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set) {
    unit_qt(r_rotation);
    return false;
  }

  copy_qt_qt(r_rotation, xr->runtime->session_state.nav_pose.orientation_quat);
  return true;
}

void WM_xr_session_state_nav_rotation_set(wmXrData *xr, const float rotation[4])
{
  if (WM_xr_session_exists(xr)) {
    BLI_ASSERT_UNIT_QUAT(rotation);
    copy_qt_qt(xr->runtime->session_state.nav_pose.orientation_quat, rotation);
    xr->runtime->session_state.is_navigation_dirty = true;
  }
}

bool WM_xr_session_state_nav_scale_get(const wmXrData *xr, float *r_scale)
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set) {
    *r_scale = 1.0f;
    return false;
  }

  *r_scale = xr->runtime->session_state.nav_scale;
  return true;
}

void WM_xr_session_state_nav_scale_set(wmXrData *xr, float scale)
{
  if (WM_xr_session_exists(xr)) {
    /* Clamp to reasonable values. */
    CLAMP(scale, xr->session_settings.clip_start, xr->session_settings.clip_end);
    xr->runtime->session_state.nav_scale = scale;
    xr->runtime->session_state.is_navigation_dirty = true;
  }
}

void WM_xr_session_state_navigation_reset(wmXrSessionState *state)
{
  zero_v3(state->nav_pose.position);
  unit_qt(state->nav_pose.orientation_quat);
  state->nav_scale = 1.0f;
  state->is_navigation_dirty = true;
  state->swap_hands = false;
}

void WM_xr_session_state_vignette_reset(wmXrSessionState *state)
{
  wmXrVignetteData *data = state->vignette_data;

  /* Reset vignette state */
  data->aperture = 1.0f;
  data->aperture_velocity = 0.0f;

  /* Set default vignette parameters */
  data->initial_aperture = 0.25f;
  data->initial_aperture_velocity = -0.03f;

  data->aperture_min = 0.08f;
  data->aperture_max = 0.3f;

  data->aperture_velocity_max = 0.002f;
  data->aperture_velocity_delta = 0.01f;
}

void WM_xr_session_state_vignette_activate(wmXrData *xr)
{
  if (WM_xr_session_exists(xr)) {
    wmXrVignetteData *data = xr->runtime->session_state.vignette_data;
    data->aperture_velocity = data->initial_aperture_velocity;
    data->aperture = min_ff(data->aperture, data->initial_aperture);
  }
}

void WM_xr_session_state_vignette_update(wmXrSessionState *state)
{
  wmXrVignetteData *data = state->vignette_data;

  const float vignette_intensity = U.xr_navigation.vignette_intensity;
  const float aperture_min = interpf(
      data->aperture_min, data->aperture_max, vignette_intensity * 0.01f);
  data->aperture_velocity = min_ff(data->aperture_velocity_max,
                                   data->aperture_velocity + data->aperture_velocity_delta);

  if (data->aperture == aperture_min) {
    data->aperture_velocity = data->aperture_velocity_max;
  }

  data->aperture = clamp_f(data->aperture + data->aperture_velocity, aperture_min, 1.0f);
}

/* -------------------------------------------------------------------- */
/** \name XR-Session Actions
 *
 * XR action processing and event dispatching.
 *
 * \{ */

void wm_xr_session_actions_init(wmXrData *xr)
{
  if (!xr->runtime) {
    return;
  }

  GHOST_XrAttachActionSets(xr->runtime->context);
}

static void wm_xr_session_controller_pose_calc(const GHOST_XrPose *raw_pose,
                                               const float view_ofs[3],
                                               const float base_mat[4][4],
                                               const float nav_mat[4][4],
                                               GHOST_XrPose *r_pose,
                                               float r_mat[4][4],
                                               float r_mat_base[4][4])
{
  float m[4][4];
  /* Calculate controller matrix in world space. */
  wm_xr_pose_to_mat(raw_pose, m);

  /* Apply eye position offset. */
  sub_v3_v3(m[3], view_ofs);

  /* Apply base pose and navigation. */
  mul_m4_m4m4(r_mat_base, base_mat, m);
  mul_m4_m4m4(r_mat, nav_mat, r_mat_base);

  /* Save final pose. */
  mat4_to_loc_quat(r_pose->position, r_pose->orientation_quat, r_mat);
}

static void wm_xr_session_controller_data_update(const XrSessionSettings *settings,
                                                 const wmXrAction *grip_action,
                                                 const wmXrAction *aim_action,
                                                 GHOST_XrContextHandle xr_context,
                                                 wmXrSessionState *state)
{
  BLI_assert(grip_action->count_subaction_paths == aim_action->count_subaction_paths);
  BLI_assert(grip_action->count_subaction_paths == BLI_listbase_count(&state->controllers));

  uint subaction_idx = 0;
  float view_ofs[3], base_mat[4][4], nav_mat[4][4];

  if ((settings->flag & XR_SESSION_USE_POSITION_TRACKING) == 0) {
    copy_v3_v3(view_ofs, state->prev_local_pose.position);
  }
  else {
    zero_v3(view_ofs);
  }
  if ((settings->flag & XR_SESSION_USE_ABSOLUTE_TRACKING) == 0) {
    add_v3_v3(view_ofs, state->prev_eye_position_ofs);
  }

  wm_xr_pose_scale_to_mat(&state->prev_base_pose, state->prev_base_scale, base_mat);
  wm_xr_pose_scale_to_mat(&state->nav_pose, state->nav_scale, nav_mat);

  LISTBASE_FOREACH_INDEX (wmXrController *, controller, &state->controllers, subaction_idx) {
    controller->grip_active = ((GHOST_XrPose *)grip_action->states)[subaction_idx].is_active;
    wm_xr_session_controller_pose_calc(&((GHOST_XrPose *)grip_action->states)[subaction_idx],
                                       view_ofs,
                                       base_mat,
                                       nav_mat,
                                       &controller->grip_pose,
                                       controller->grip_mat,
                                       controller->grip_mat_base);
    controller->aim_active = ((GHOST_XrPose *)aim_action->states)[subaction_idx].is_active;
    wm_xr_session_controller_pose_calc(&((GHOST_XrPose *)aim_action->states)[subaction_idx],
                                       view_ofs,
                                       base_mat,
                                       nav_mat,
                                       &controller->aim_pose,
                                       controller->aim_mat,
                                       controller->aim_mat_base);

    if (!controller->model) {
      /* Notify GHOST to load/continue loading the controller model data. This can be called more
       * than once since the model may not be available from the runtime yet. The batch itself will
       * be created in wm_xr_draw_controllers(). */
      GHOST_XrLoadControllerModel(xr_context, controller->subaction_path);
    }
    else {
      GHOST_XrUpdateControllerModelComponents(xr_context, controller->subaction_path);
    }
  }
}

static const GHOST_XrPose *wm_xr_session_controller_aim_pose_find(const wmXrSessionState *state,
                                                                  const char *subaction_path)
{
  const wmXrController *controller = static_cast<const wmXrController *>(BLI_findstring(
      &state->controllers, subaction_path, offsetof(wmXrController, subaction_path)));
  return controller ? &controller->aim_pose : nullptr;
}

BLI_INLINE bool test_float_state(const float *state, float threshold, eXrAxisFlag flag)
{
  if ((flag & XR_AXIS0_POS) != 0) {
    if (*state > threshold) {
      return true;
    }
  }
  else if ((flag & XR_AXIS0_NEG) != 0) {
    if (*state < -threshold) {
      return true;
    }
  }
  else {
    if (fabsf(*state) > threshold) {
      return true;
    }
  }
  return false;
}

BLI_INLINE bool test_vec2f_state(const float state[2], float threshold, eXrAxisFlag flag)
{
  if ((flag & XR_AXIS0_POS) != 0) {
    if (state[0] < 0.0f) {
      return false;
    }
  }
  else if ((flag & XR_AXIS0_NEG) != 0) {
    if (state[0] > 0.0f) {
      return false;
    }
  }
  if ((flag & XR_AXIS1_POS) != 0) {
    if (state[1] < 0.0f) {
      return false;
    }
  }
  else if ((flag & XR_AXIS1_NEG) != 0) {
    if (state[1] > 0.0f) {
      return false;
    }
  }
  return (len_v2(state) > threshold);
}

static bool wm_xr_session_modal_action_test(const ListBase *active_modal_actions,
                                            const wmXrAction *action,
                                            bool *r_found)
{
  if (r_found) {
    *r_found = false;
  }

  LISTBASE_FOREACH (LinkData *, ld, active_modal_actions) {
    wmXrAction *active_modal_action = static_cast<wmXrAction *>(ld->data);
    if (action == active_modal_action) {
      if (r_found) {
        *r_found = true;
      }
      return true;
    }
    if (action->ot == active_modal_action->ot &&
        IDP_EqualsProperties(action->op_properties, active_modal_action->op_properties))
    {
      /* Don't allow duplicate modal operators since this can lead to unwanted modal handler
       * behavior. */
      return false;
    }
  }

  return true;
}

static void wm_xr_session_modal_action_test_add(ListBase *active_modal_actions,
                                                const wmXrAction *action)
{
  bool found;
  if (wm_xr_session_modal_action_test(active_modal_actions, action, &found) && !found) {
    LinkData *ld = MEM_callocN<LinkData>(__func__);
    ld->data = (void *)action;
    BLI_addtail(active_modal_actions, ld);
  }
}

static void wm_xr_session_modal_action_remove(ListBase *active_modal_actions,
                                              const wmXrAction *action)
{
  LISTBASE_FOREACH (LinkData *, ld, active_modal_actions) {
    if (action == ld->data) {
      BLI_freelinkN(active_modal_actions, ld);
      return;
    }
  }
}

static wmXrHapticAction *wm_xr_session_haptic_action_find(ListBase *active_haptic_actions,
                                                          const wmXrAction *action,
                                                          const char *subaction_path)
{
  LISTBASE_FOREACH (wmXrHapticAction *, ha, active_haptic_actions) {
    if ((action == ha->action) && (subaction_path == ha->subaction_path)) {
      return ha;
    }
  }
  return nullptr;
}

static void wm_xr_session_haptic_action_add(ListBase *active_haptic_actions,
                                            const wmXrAction *action,
                                            const char *subaction_path,
                                            int64_t time_now)
{
  wmXrHapticAction *ha = wm_xr_session_haptic_action_find(
      active_haptic_actions, action, subaction_path);
  if (ha) {
    /* Reset start time since OpenXR restarts haptics if they are already active. */
    ha->time_start = time_now;
  }
  else {
    ha = MEM_callocN<wmXrHapticAction>(__func__);
    ha->action = (wmXrAction *)action;
    ha->subaction_path = subaction_path;
    ha->time_start = time_now;
    BLI_addtail(active_haptic_actions, ha);
  }
}

static void wm_xr_session_haptic_action_remove(ListBase *active_haptic_actions,
                                               const wmXrAction *action)
{
  LISTBASE_FOREACH (wmXrHapticAction *, ha, active_haptic_actions) {
    if (action == ha->action) {
      BLI_freelinkN(active_haptic_actions, ha);
      return;
    }
  }
}

static void wm_xr_session_haptic_timers_check(ListBase *active_haptic_actions, int64_t time_now)
{
  LISTBASE_FOREACH_MUTABLE (wmXrHapticAction *, ha, active_haptic_actions) {
    if (time_now - ha->time_start >= ha->action->haptic_duration) {
      BLI_freelinkN(active_haptic_actions, ha);
    }
  }
}

static void wm_xr_session_action_states_interpret(wmXrData *xr,
                                                  const char *action_set_name,
                                                  wmXrAction *action,
                                                  uint subaction_idx,
                                                  ListBase *active_modal_actions,
                                                  ListBase *active_haptic_actions,
                                                  int64_t time_now,
                                                  bool modal,
                                                  bool haptic,
                                                  short *r_val)
{
  const char *haptic_subaction_path = ((action->haptic_flag & XR_HAPTIC_MATCHUSERPATHS) != 0) ?
                                          action->subaction_paths[subaction_idx] :
                                          nullptr;
  bool curr = false;
  bool prev = false;

  switch (action->type) {
    case XR_BOOLEAN_INPUT: {
      const bool *state = &((bool *)action->states)[subaction_idx];
      bool *state_prev = &((bool *)action->states_prev)[subaction_idx];
      if (*state) {
        curr = true;
      }
      if (*state_prev) {
        prev = true;
      }
      *state_prev = *state;
      break;
    }
    case XR_FLOAT_INPUT: {
      const float *state = &((float *)action->states)[subaction_idx];
      float *state_prev = &((float *)action->states_prev)[subaction_idx];
      if (test_float_state(
              state, action->float_thresholds[subaction_idx], action->axis_flags[subaction_idx]))
      {
        curr = true;
      }
      if (test_float_state(state_prev,
                           action->float_thresholds[subaction_idx],
                           action->axis_flags[subaction_idx]))
      {
        prev = true;
      }
      *state_prev = *state;
      break;
    }
    case XR_VECTOR2F_INPUT: {
      const float (*state)[2] = &((float (*)[2])action->states)[subaction_idx];
      float (*state_prev)[2] = &((float (*)[2])action->states_prev)[subaction_idx];
      if (test_vec2f_state(
              *state, action->float_thresholds[subaction_idx], action->axis_flags[subaction_idx]))
      {
        curr = true;
      }
      if (test_vec2f_state(*state_prev,
                           action->float_thresholds[subaction_idx],
                           action->axis_flags[subaction_idx]))
      {
        prev = true;
      }
      copy_v2_v2(*state_prev, *state);
      break;
    }
    case XR_POSE_INPUT:
    case XR_VIBRATION_OUTPUT:
      BLI_assert_unreachable();
      break;
  }

  if (curr) {
    if (!prev) {
      if (modal || (action->op_flag == XR_OP_PRESS)) {
        *r_val = KM_PRESS;
      }
      if (haptic && (action->haptic_flag & (XR_HAPTIC_PRESS | XR_HAPTIC_REPEAT)) != 0) {
        /* Apply haptics. */
        if (WM_xr_haptic_action_apply(xr,
                                      action_set_name,
                                      action->haptic_name,
                                      haptic_subaction_path,
                                      &action->haptic_duration,
                                      &action->haptic_frequency,
                                      &action->haptic_amplitude))
        {
          wm_xr_session_haptic_action_add(
              active_haptic_actions, action, haptic_subaction_path, time_now);
        }
      }
    }
    else if (modal) {
      *r_val = KM_PRESS;
    }
    if (modal && !action->active_modal_path) {
      /* Set active modal path. */
      action->active_modal_path = action->subaction_paths[subaction_idx];
      /* Add to active modal actions. */
      wm_xr_session_modal_action_test_add(active_modal_actions, action);
    }
    if (haptic && ((action->haptic_flag & XR_HAPTIC_REPEAT) != 0)) {
      if (!wm_xr_session_haptic_action_find(active_haptic_actions, action, haptic_subaction_path))
      {
        /* Apply haptics. */
        if (WM_xr_haptic_action_apply(xr,
                                      action_set_name,
                                      action->haptic_name,
                                      haptic_subaction_path,
                                      &action->haptic_duration,
                                      &action->haptic_frequency,
                                      &action->haptic_amplitude))
        {
          wm_xr_session_haptic_action_add(
              active_haptic_actions, action, haptic_subaction_path, time_now);
        }
      }
    }
  }
  else if (prev) {
    if (modal || (action->op_flag == XR_OP_RELEASE)) {
      *r_val = KM_RELEASE;
      if (modal && (action->subaction_paths[subaction_idx] == action->active_modal_path)) {
        /* Unset active modal path. */
        action->active_modal_path = nullptr;
        /* Remove from active modal actions. */
        wm_xr_session_modal_action_remove(active_modal_actions, action);
      }
    }
    if (haptic) {
      if ((action->haptic_flag & XR_HAPTIC_RELEASE) != 0) {
        /* Apply haptics. */
        if (WM_xr_haptic_action_apply(xr,
                                      action_set_name,
                                      action->haptic_name,
                                      haptic_subaction_path,
                                      &action->haptic_duration,
                                      &action->haptic_frequency,
                                      &action->haptic_amplitude))
        {
          wm_xr_session_haptic_action_add(
              active_haptic_actions, action, haptic_subaction_path, time_now);
        }
      }
      else if ((action->haptic_flag & XR_HAPTIC_REPEAT) != 0) {
        /* Stop any active haptics. */
        WM_xr_haptic_action_stop(xr, action_set_name, action->haptic_name, haptic_subaction_path);
        wm_xr_session_haptic_action_remove(active_haptic_actions, action);
      }
    }
  }
}

static bool wm_xr_session_action_test_bimanual(const wmXrSessionState *session_state,
                                               wmXrAction *action,
                                               uint subaction_idx,
                                               uint *r_subaction_idx_other,
                                               const GHOST_XrPose **r_aim_pose_other)
{
  if ((action->action_flag & XR_ACTION_BIMANUAL) == 0) {
    return false;
  }

  bool bimanual = false;

  *r_subaction_idx_other = (subaction_idx == 0) ?
                               uint(min_ii(1, action->count_subaction_paths - 1)) :
                               0;

  switch (action->type) {
    case XR_BOOLEAN_INPUT: {
      const bool *state = &((bool *)action->states)[*r_subaction_idx_other];
      if (*state) {
        bimanual = true;
      }
      break;
    }
    case XR_FLOAT_INPUT: {
      const float *state = &((float *)action->states)[*r_subaction_idx_other];
      if (test_float_state(state,
                           action->float_thresholds[*r_subaction_idx_other],
                           action->axis_flags[*r_subaction_idx_other]))
      {
        bimanual = true;
      }
      break;
    }
    case XR_VECTOR2F_INPUT: {
      const float (*state)[2] = &((float (*)[2])action->states)[*r_subaction_idx_other];
      if (test_vec2f_state(*state,
                           action->float_thresholds[*r_subaction_idx_other],
                           action->axis_flags[*r_subaction_idx_other]))
      {
        bimanual = true;
      }
      break;
    }
    case XR_POSE_INPUT:
    case XR_VIBRATION_OUTPUT:
      BLI_assert_unreachable();
      break;
  }

  if (bimanual) {
    *r_aim_pose_other = wm_xr_session_controller_aim_pose_find(
        session_state, action->subaction_paths[*r_subaction_idx_other]);
  }

  return bimanual;
}

static wmXrActionData *wm_xr_session_event_create(const char *action_set_name,
                                                  const wmXrAction *action,
                                                  const GHOST_XrPose *controller_aim_pose,
                                                  const GHOST_XrPose *controller_aim_pose_other,
                                                  uint subaction_idx,
                                                  uint subaction_idx_other,
                                                  bool bimanual)
{
  wmXrActionData *data = MEM_callocN<wmXrActionData>(__func__);
  STRNCPY(data->action_set, action_set_name);
  STRNCPY(data->action, action->name);
  STRNCPY(data->user_path, action->subaction_paths[subaction_idx]);
  if (bimanual) {
    STRNCPY(data->user_path_other, action->subaction_paths[subaction_idx_other]);
  }
  data->type = action->type;

  switch (action->type) {
    case XR_BOOLEAN_INPUT:
      data->state[0] = ((bool *)action->states)[subaction_idx] ? 1.0f : 0.0f;
      if (bimanual) {
        data->state_other[0] = ((bool *)action->states)[subaction_idx_other] ? 1.0f : 0.0f;
      }
      break;
    case XR_FLOAT_INPUT:
      data->state[0] = ((float *)action->states)[subaction_idx];
      if (bimanual) {
        data->state_other[0] = ((float *)action->states)[subaction_idx_other];
      }
      data->float_threshold = action->float_thresholds[subaction_idx];
      break;
    case XR_VECTOR2F_INPUT:
      copy_v2_v2(data->state, ((float (*)[2])action->states)[subaction_idx]);
      if (bimanual) {
        copy_v2_v2(data->state_other, ((float (*)[2])action->states)[subaction_idx_other]);
      }
      data->float_threshold = action->float_thresholds[subaction_idx];
      break;
    case XR_POSE_INPUT:
    case XR_VIBRATION_OUTPUT:
      BLI_assert_unreachable();
      break;
  }

  if (controller_aim_pose) {
    copy_v3_v3(data->controller_loc, controller_aim_pose->position);
    copy_qt_qt(data->controller_rot, controller_aim_pose->orientation_quat);

    if (bimanual && controller_aim_pose_other) {
      copy_v3_v3(data->controller_loc_other, controller_aim_pose_other->position);
      copy_qt_qt(data->controller_rot_other, controller_aim_pose_other->orientation_quat);
    }
    else {
      data->controller_rot_other[0] = 1.0f;
    }
  }
  else {
    data->controller_rot[0] = 1.0f;
    data->controller_rot_other[0] = 1.0f;
  }

  data->ot = action->ot;
  data->op_properties = action->op_properties;

  data->bimanual = bimanual;

  return data;
}

/* Dispatch events to window queues. */
static void wm_xr_session_events_dispatch(wmXrData *xr,
                                          GHOST_XrContextHandle xr_context,
                                          wmXrActionSet *action_set,
                                          wmXrSessionState *session_state,
                                          wmWindow *win)
{
  const char *action_set_name = action_set->name;

  const uint count = GHOST_XrGetActionCount(xr_context, action_set_name);
  if (count < 1) {
    return;
  }

  const int64_t time_now = int64_t(BLI_time_now_seconds() * 1000);

  ListBase *active_modal_actions = &action_set->active_modal_actions;
  ListBase *active_haptic_actions = &action_set->active_haptic_actions;

  wmXrAction **actions = MEM_calloc_arrayN<wmXrAction *>(count, __func__);

  GHOST_XrGetActionCustomdataArray(xr_context, action_set_name, (void **)actions);

  /* Check haptic action timers. */
  wm_xr_session_haptic_timers_check(active_haptic_actions, time_now);

  for (uint action_idx = 0; action_idx < count; ++action_idx) {
    wmXrAction *action = actions[action_idx];
    if (action && action->ot) {
      const bool modal = action->ot->modal;
      const bool haptic = (GHOST_XrGetActionCustomdata(
                               xr_context, action_set_name, action->haptic_name) != nullptr);

      for (uint subaction_idx = 0; subaction_idx < action->count_subaction_paths; ++subaction_idx)
      {
        short val = KM_NOTHING;

        /* Interpret action states (update modal/haptic action lists, apply haptics, etc). */
        wm_xr_session_action_states_interpret(xr,
                                              action_set_name,
                                              action,
                                              subaction_idx,
                                              active_modal_actions,
                                              active_haptic_actions,
                                              time_now,
                                              modal,
                                              haptic,
                                              &val);

        const bool is_active_modal_action = wm_xr_session_modal_action_test(
            active_modal_actions, action, nullptr);
        const bool is_active_modal_subaction = (!action->active_modal_path ||
                                                (action->subaction_paths[subaction_idx] ==
                                                 action->active_modal_path));

        if ((val != KM_NOTHING) &&
            (!modal || (is_active_modal_action && is_active_modal_subaction)))
        {
          const GHOST_XrPose *aim_pose = wm_xr_session_controller_aim_pose_find(
              session_state, action->subaction_paths[subaction_idx]);
          const GHOST_XrPose *aim_pose_other = nullptr;
          uint subaction_idx_other = 0;

          /* Test for bimanual interaction. */
          const bool bimanual = wm_xr_session_action_test_bimanual(
              session_state, action, subaction_idx, &subaction_idx_other, &aim_pose_other);

          wmXrActionData *actiondata = wm_xr_session_event_create(action_set_name,
                                                                  action,
                                                                  aim_pose,
                                                                  aim_pose_other,
                                                                  subaction_idx,
                                                                  subaction_idx_other,
                                                                  bimanual);
          wm_event_add_xrevent(win, actiondata, val);
        }
      }
    }
  }

  MEM_freeN(actions);
}

void wm_xr_session_actions_update(wmWindowManager *wm)
{
  wmXrData *xr = &wm->xr;
  if (!xr->runtime) {
    return;
  }

  XrSessionSettings *settings = &xr->session_settings;
  GHOST_XrContextHandle xr_context = xr->runtime->context;
  wmXrSessionState *state = &xr->runtime->session_state;

  if (state->is_navigation_dirty) {
    memcpy(&state->nav_pose_prev, &state->nav_pose, sizeof(state->nav_pose_prev));
    state->nav_scale_prev = state->nav_scale;
    state->is_navigation_dirty = false;

    /* Update viewer pose with any navigation changes since the last actions sync so that data
     * is correct for queries. */
    float m[4][4], viewer_mat[4][4];
    wm_xr_pose_scale_to_mat(&state->nav_pose, state->nav_scale, m);
    mul_m4_m4m4(viewer_mat, m, state->viewer_mat_base);
    mat4_to_loc_quat(state->viewer_pose.position, state->viewer_pose.orientation_quat, viewer_mat);
    wm_xr_pose_scale_to_imat(
        &state->viewer_pose, settings->base_scale * state->nav_scale, state->viewer_viewmat);
  }

  /* Set active action set if requested previously. */
  if (state->active_action_set_next[0]) {
    WM_xr_active_action_set_set(xr, state->active_action_set_next, false);
    state->active_action_set_next[0] = '\0';
  }
  wmXrActionSet *active_action_set = state->active_action_set;

  const bool synced = GHOST_XrSyncActions(xr_context,
                                          active_action_set ? active_action_set->name : nullptr);
  if (!synced) {
    return;
  }

  /* Only update controller data and dispatch events for active action set. */
  if (active_action_set) {
    wmWindow *win = wm_xr_session_root_window_or_fallback_get(wm, xr->runtime);

    if (active_action_set->controller_grip_action && active_action_set->controller_aim_action) {
      wm_xr_session_controller_data_update(settings,
                                           active_action_set->controller_grip_action,
                                           active_action_set->controller_aim_action,
                                           xr_context,
                                           state);
    }

    if (win) {
      /* Ensure an XR area exists for events. */
      if (!xr->runtime->area) {
        xr->runtime->area = ED_area_offscreen_create(win, SPACE_VIEW3D);
      }

      /* Set XR area object type flags for operators. */
      View3D *v3d = static_cast<View3D *>(xr->runtime->area->spacedata.first);
      v3d->object_type_exclude_viewport = settings->object_type_exclude_viewport;
      v3d->object_type_exclude_select = settings->object_type_exclude_select;

      wm_xr_session_events_dispatch(xr, xr_context, active_action_set, state, win);
    }
  }
}

void wm_xr_session_controller_data_populate(const wmXrAction *grip_action,
                                            const wmXrAction *aim_action,
                                            wmXrData *xr)
{
  UNUSED_VARS(aim_action); /* Only used for asserts. */

  wmXrSessionState *state = &xr->runtime->session_state;
  ListBase *controllers = &state->controllers;

  BLI_assert(grip_action->count_subaction_paths == aim_action->count_subaction_paths);
  const uint count = grip_action->count_subaction_paths;

  wm_xr_session_controller_data_free(state);

  for (uint i = 0; i < count; ++i) {
    wmXrController *controller = MEM_callocN<wmXrController>(__func__);

    BLI_assert(STREQ(grip_action->subaction_paths[i], aim_action->subaction_paths[i]));
    STRNCPY(controller->subaction_path, grip_action->subaction_paths[i]);

    BLI_addtail(controllers, controller);
  }

  /* Activate draw callback. */
  if (g_xr_surface) {
    wmXrSurfaceData *surface_data = static_cast<wmXrSurfaceData *>(g_xr_surface->customdata);
    if (surface_data && !surface_data->controller_draw_handle) {
      if (surface_data->controller_art) {
        surface_data->controller_draw_handle = ED_region_draw_cb_activate(
            surface_data->controller_art, wm_xr_draw_controllers, xr, REGION_DRAW_POST_VIEW);
      }
    }
  }
}

void wm_xr_session_controller_data_clear(wmXrSessionState *state)
{
  wm_xr_session_controller_data_free(state);

  /* Deactivate draw callback. */
  if (g_xr_surface) {
    wmXrSurfaceData *surface_data = static_cast<wmXrSurfaceData *>(g_xr_surface->customdata);
    if (surface_data && surface_data->controller_draw_handle) {
      if (surface_data->controller_art) {
        ED_region_draw_cb_exit(surface_data->controller_art, surface_data->controller_draw_handle);
      }
      surface_data->controller_draw_handle = nullptr;
    }
  }
}

/** \} */ /* XR-Session Actions. */

/* -------------------------------------------------------------------- */
/** \name XR-Session Surface
 *
 * A wmSurface is used to manage drawing of the VR viewport. It's created and destroyed with the
 * session.
 *
 * \{ */

/**
 * \brief Call Ghost-XR to draw a frame
 *
 * Draw callback for the XR-session surface. It's expected to be called on each main loop
 * iteration and tells Ghost-XR to submit a new frame by drawing its views. Note that for drawing
 * each view, #wm_xr_draw_view() will be called through Ghost-XR (see GHOST_XrDrawViewFunc()).
 */
static void wm_xr_session_surface_draw(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmXrDrawData draw_data;

  if (!WM_xr_session_is_ready(&wm->xr)) {
    return;
  }

  Scene *scene;
  Depsgraph *depsgraph;
  wm_xr_session_scene_and_depsgraph_get(wm, &scene, &depsgraph);
  /* Might fail when force-redrawing windows with #WM_redraw_windows(), which is done on file
   * writing for example. */
  // BLI_assert(DEG_is_fully_evaluated(depsgraph));
  wm_xr_session_draw_data_populate(&wm->xr, scene, depsgraph, &draw_data);

  GHOST_XrSessionDrawViews(wm->xr.runtime->context, &draw_data);

  /* There's no active frame-buffer if the session was canceled (exception while drawing views). */
  if (GPU_framebuffer_active_get()) {
    GPU_framebuffer_restore();
  }
}

static void wm_xr_session_do_depsgraph(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  if (!WM_xr_session_is_ready(&wm->xr)) {
    return;
  }

  Scene *scene;
  Depsgraph *depsgraph;
  wm_xr_session_scene_and_depsgraph_get(wm, &scene, &depsgraph);
  BKE_scene_graph_evaluated_ensure(depsgraph, CTX_data_main(C));
}

bool wm_xr_session_surface_offscreen_ensure(wmXrSurfaceData *surface_data,
                                            const GHOST_XrDrawViewInfo *draw_view)
{
  wmXrViewportPair *vp = nullptr;
  if (draw_view->view_idx >= BLI_listbase_count(&surface_data->viewports)) {
    vp = MEM_callocN<wmXrViewportPair>(__func__);
    BLI_addtail(&surface_data->viewports, vp);
  }
  else {
    vp = static_cast<wmXrViewportPair *>(
        BLI_findlink(&surface_data->viewports, draw_view->view_idx));
  }
  BLI_assert(vp);

  GPUOffScreen *offscreen = vp->offscreen;
  GPUViewport *viewport = vp->viewport;
  const bool size_changed = offscreen && (GPU_offscreen_width(offscreen) != draw_view->width) &&
                            (GPU_offscreen_height(offscreen) != draw_view->height);
  if (offscreen) {
    BLI_assert(viewport);

    if (!size_changed) {
      return true;
    }
    GPU_viewport_free(viewport);
    GPU_offscreen_free(offscreen);
  }

  char err_out[256] = "unknown";
  bool failure = false;

  /* Initialize with some unsupported format to check following switch statement. */
  blender::gpu::TextureFormat format = blender::gpu::TextureFormat::UNORM_8;

  switch (draw_view->swapchain_format) {
    case GHOST_kXrSwapchainFormatRGBA8:
      format = blender::gpu::TextureFormat::UNORM_8_8_8_8;
      break;
    case GHOST_kXrSwapchainFormatRGBA16:
      format = blender::gpu::TextureFormat::UNORM_16_16_16_16;
      break;
    case GHOST_kXrSwapchainFormatRGBA16F:
      format = blender::gpu::TextureFormat::SFLOAT_16_16_16_16;
      break;
    case GHOST_kXrSwapchainFormatRGB10_A2:
      format = blender::gpu::TextureFormat::UNORM_10_10_10_2;
      break;
  }
  BLI_assert(format != blender::gpu::TextureFormat::UNORM_8);

  offscreen = vp->offscreen = GPU_offscreen_create(draw_view->width,
                                                   draw_view->height,
                                                   true,
                                                   format,
                                                   GPU_TEXTURE_USAGE_SHADER_READ |
                                                       GPU_TEXTURE_USAGE_MEMORY_EXPORT,
                                                   false,
                                                   err_out);
  if (offscreen) {
    viewport = vp->viewport = GPU_viewport_create();
    if (!viewport) {
      GPU_offscreen_free(offscreen);
      offscreen = vp->offscreen = nullptr;
      failure = true;
    }
  }
  else {
    failure = true;
  }

  if (failure) {
    CLOG_ERROR(&LOG, "Failed to get buffer, %s", err_out);
    return false;
  }

  return true;
}

static void wm_xr_session_surface_free_data(wmSurface *surface)
{
  wmXrSurfaceData *data = static_cast<wmXrSurfaceData *>(surface->customdata);
  ListBase *lb = &data->viewports;

  while (wmXrViewportPair *vp = static_cast<wmXrViewportPair *>(BLI_pophead(lb))) {
    if (vp->viewport) {
      GPU_viewport_free(vp->viewport);
    }
    if (vp->offscreen) {
      GPU_offscreen_free(vp->offscreen);
    }
    BLI_freelinkN(lb, vp);
  }

  if (data->controller_art) {
    BLI_freelistN(&data->controller_art->drawcalls);
    MEM_freeN(data->controller_art);
  }

  MEM_freeN(surface->customdata);

  g_xr_surface = nullptr;
}

static wmSurface *wm_xr_session_surface_create()
{
  if (g_xr_surface) {
    BLI_assert(false);
    return g_xr_surface;
  }

  wmSurface *surface = MEM_callocN<wmSurface>(__func__);
  wmXrSurfaceData *data = MEM_callocN<wmXrSurfaceData>("XrSurfaceData");
  data->controller_art = MEM_callocN<ARegionType>("XrControllerRegionType");

  surface->draw = wm_xr_session_surface_draw;
  surface->do_depsgraph = wm_xr_session_do_depsgraph;
  surface->free_data = wm_xr_session_surface_free_data;
  surface->activate = DRW_xr_drawing_begin;
  surface->deactivate = DRW_xr_drawing_end;

  surface->system_gpu_context = static_cast<GHOST_ContextHandle>(DRW_system_gpu_context_get());
  surface->blender_gpu_context = static_cast<GPUContext *>(DRW_xr_blender_gpu_context_get());

  data->controller_art->regionid = RGN_TYPE_XR;
  surface->customdata = data;

  g_xr_surface = surface;

  return surface;
}

void *wm_xr_session_gpu_binding_context_create()
{
  wmSurface *surface = wm_xr_session_surface_create();

  wm_surface_add(surface);

  /* Some regions may need to redraw with updated session state after the session is entirely up
   * and running. */
  WM_main_add_notifier(NC_WM | ND_XR_DATA_CHANGED, nullptr);

  return surface->system_gpu_context;
}

void wm_xr_session_gpu_binding_context_destroy(GHOST_ContextHandle /*context*/)
{
  if (g_xr_surface) { /* Might have been freed already. */
    wm_surface_remove(g_xr_surface);
  }

  wm_window_reset_drawable();

  /* Some regions may need to redraw with updated session state after the session is entirely
   * stopped. */
  WM_main_add_notifier(NC_WM | ND_XR_DATA_CHANGED, nullptr);
}

ARegionType *WM_xr_surface_controller_region_type_get()
{
  if (g_xr_surface) {
    wmXrSurfaceData *data = static_cast<wmXrSurfaceData *>(g_xr_surface->customdata);
    return data->controller_art;
  }

  return nullptr;
}

/** \} */ /* XR-Session Surface. */
