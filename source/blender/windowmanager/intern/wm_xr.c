/*
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
 */

/** \file
 * \ingroup wm
 *
 * \name Window-Manager XR API
 *
 * Implements Blender specific functionality for the GHOST_Xr API.
 */

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "BLI_ghash.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"

#include "CLG_log.h"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_xr_types.h"

#include "DRW_engine.h"

#include "ED_view3d.h"
#include "ED_view3d_offscreen.h"

#include "GHOST_C-api.h"

#include "GPU_context.h"
#include "GPU_draw.h"
#include "GPU_matrix.h"
#include "GPU_viewport.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"

#include "WM_types.h"
#include "WM_api.h"

#include "wm.h"
#include "wm_surface.h"
#include "wm_window.h"

struct wmXrRuntimeData *wm_xr_runtime_data_create(void);
void wm_xr_runtime_data_free(struct wmXrRuntimeData **runtime);
void wm_xr_draw_view(const GHOST_XrDrawViewInfo *, void *);
void *wm_xr_session_gpu_binding_context_create(GHOST_TXrGraphicsBinding);
void wm_xr_session_gpu_binding_context_destroy(GHOST_TXrGraphicsBinding, GHOST_ContextHandle);
wmSurface *wm_xr_session_surface_create(wmWindowManager *, unsigned int);
void wm_xr_pose_to_viewmat(const GHOST_XrPose *pose, float r_viewmat[4][4]);

/* -------------------------------------------------------------------- */

typedef struct wmXrSessionState {
  bool is_started;

  /** Last known viewer pose (centroid of eyes, in world space) stored for queries. */
  GHOST_XrPose viewer_pose;
  /** The last known view matrix, calculated from above's viewer pose. */
  float viewer_viewmat[4][4];
  float focal_len;

  /** Copy of XrSessionSettings.flag created on the last draw call, stored to detect changes. */
  int prev_settings_flag;
  /** Copy of wmXrDrawData.eye_position_ofs. */
  float prev_eye_position_ofs[3];

  bool is_view_data_set;
} wmXrSessionState;

typedef struct wmXrRuntimeData {
  GHOST_XrContextHandle context;

  /* Although this struct is internal, RNA gets a handle to this for state information queries. */
  wmXrSessionState session_state;
  wmXrSessionExitFn exit_fn;
} wmXrRuntimeData;

typedef struct wmXrDrawData {
  /** The pose (location + rotation) to which eye deltas will be applied to when drawing (world
   * space). With positional tracking enabled, it should be the same as the base pose, when
   * disabled it also contains a location delta from the moment the option was toggled. */
  GHOST_XrPose base_pose;
  float eye_position_ofs[3]; /* Local/view space. */
} wmXrDrawData;

typedef struct {
  GHOST_TXrGraphicsBinding gpu_binding_type;
  GPUOffScreen *offscreen;
  GPUViewport *viewport;

  GHOST_ContextHandle secondary_ghost_ctx;
} wmXrSurfaceData;

typedef struct {
  wmWindowManager *wm;
} wmXrErrorHandlerData;

/* -------------------------------------------------------------------- */

static wmSurface *g_xr_surface = NULL;
static CLG_LogRef LOG = {"wm.xr"};

/* -------------------------------------------------------------------- */
/** \name XR-Context
 *
 * All XR functionality is accessed through a #GHOST_XrContext handle.
 * The lifetime of this context also determines the lifetime of the OpenXR instance, which is the
 * representation of the OpenXR runtime connection within the application.
 *
 * \{ */

static void wm_xr_error_handler(const GHOST_XrError *error)
{
  wmXrErrorHandlerData *handler_data = error->customdata;
  wmWindowManager *wm = handler_data->wm;

  BKE_reports_clear(&wm->reports);
  WM_report(RPT_ERROR, error->user_message);
  WM_report_banner_show();

  if (wm->xr.runtime) {
    /* Just play safe and destroy the entire runtime data, including context. */
    wm_xr_runtime_data_free(&wm->xr.runtime);
  }
}

bool wm_xr_init(wmWindowManager *wm)
{
  if (wm->xr.runtime && wm->xr.runtime->context) {
    return true;
  }
  static wmXrErrorHandlerData error_customdata;

  /* Set up error handling */
  error_customdata.wm = wm;
  GHOST_XrErrorHandler(wm_xr_error_handler, &error_customdata);

  {
    const GHOST_TXrGraphicsBinding gpu_bindings_candidates[] = {
        GHOST_kXrGraphicsOpenGL,
#ifdef WIN32
        GHOST_kXrGraphicsD3D11,
#endif
    };
    GHOST_XrContextCreateInfo create_info = {
        .gpu_binding_candidates = gpu_bindings_candidates,
        .gpu_binding_candidates_count = ARRAY_SIZE(gpu_bindings_candidates),
    };
    GHOST_XrContextHandle context;

    if (G.debug & G_DEBUG_XR) {
      create_info.context_flag |= GHOST_kXrContextDebug;
    }
    if (G.debug & G_DEBUG_XR_TIME) {
      create_info.context_flag |= GHOST_kXrContextDebugTime;
    }

    if (!(context = GHOST_XrContextCreate(&create_info))) {
      return false;
    }

    /* Set up context callbacks */
    GHOST_XrGraphicsContextBindFuncs(context,
                                     wm_xr_session_gpu_binding_context_create,
                                     wm_xr_session_gpu_binding_context_destroy);
    GHOST_XrDrawViewFunc(context, wm_xr_draw_view);

    if (!wm->xr.runtime) {
      wm->xr.runtime = wm_xr_runtime_data_create();
      wm->xr.runtime->context = context;
    }
  }
  BLI_assert(wm->xr.runtime && wm->xr.runtime->context);

  return true;
}

void wm_xr_exit(wmWindowManager *wm)
{
  if (wm->xr.runtime != NULL) {
    wm_xr_runtime_data_free(&wm->xr.runtime);
  }
  if (wm->xr.session_settings.shading.prop) {
    IDP_FreeProperty(wm->xr.session_settings.shading.prop);
    wm->xr.session_settings.shading.prop = NULL;
  }
}

bool wm_xr_events_handle(wmWindowManager *wm)
{
  if (wm->xr.runtime && wm->xr.runtime->context) {
    return GHOST_XrEventsHandle(wm->xr.runtime->context);
  }
  return false;
}

/** \} */ /* XR-Context */

/* -------------------------------------------------------------------- */
/** \name XR Runtime Data
 *
 * \{ */

wmXrRuntimeData *wm_xr_runtime_data_create(void)
{
  wmXrRuntimeData *runtime = MEM_callocN(sizeof(*runtime), __func__);
  return runtime;
}

void wm_xr_runtime_data_free(wmXrRuntimeData **runtime)
{
  /* Note that this function may be called twice, because of an indirect recursion: If a session is
   * running while WM-XR calls this function, calling GHOST_XrContextDestroy() will call this
   * again, because it's also set as the session exit callback. So NULL-check and NULL everything
   * that is freed here. */

  /* We free all runtime XR data here, so if the context is still alive, destroy it. */
  if ((*runtime)->context != NULL) {
    GHOST_XrContextHandle context = (*runtime)->context;
    /* Prevent recursive GHOST_XrContextDestroy() call by NULL'ing the context pointer before the
     * first call, see comment above. */
    (*runtime)->context = NULL;
    GHOST_XrContextDestroy(context);
  }
  MEM_SAFE_FREE(*runtime);
}

static void wm_xr_base_pose_calc(const Scene *scene,
                                 const XrSessionSettings *settings,
                                 GHOST_XrPose *r_base_pose)
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

    mat4_to_loc_quat(r_base_pose->position, tmp_quat, base_pose_object->obmat);

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
}

static void wm_xr_draw_data_populate(const wmXrSessionState *state,
                                     const GHOST_XrDrawViewInfo *draw_view,
                                     const XrSessionSettings *settings,
                                     const Scene *scene,
                                     wmXrDrawData *r_draw_data)
{
  const bool position_tracking_toggled = ((state->prev_settings_flag &
                                           XR_SESSION_USE_POSITION_TRACKING) !=
                                          (settings->flag & XR_SESSION_USE_POSITION_TRACKING));
  const bool use_position_tracking = settings->flag & XR_SESSION_USE_POSITION_TRACKING;

  memset(r_draw_data, 0, sizeof(*r_draw_data));

  wm_xr_base_pose_calc(scene, settings, &r_draw_data->base_pose);

  if (position_tracking_toggled || !state->is_view_data_set) {
    if (use_position_tracking) {
      copy_v3_fl(r_draw_data->eye_position_ofs, 0.0f);
    }
    else {
      /* Store the current local offset (local pose) so that we can apply that to the eyes. This
       * way the eyes stay exactly where they are when disabling positional tracking. */
      copy_v3_v3(r_draw_data->eye_position_ofs, draw_view->local_pose.position);
    }
  }
  else if (!use_position_tracking) {
    /* Keep previous offset when positional tracking is disabled. */
    copy_v3_v3(r_draw_data->eye_position_ofs, state->prev_eye_position_ofs);
  }
}

/**
 * Update information that is only stored for external state queries. E.g. for Python API to
 * request the current (as in, last known) viewer pose.
 */
static void wm_xr_session_state_update(wmXrSessionState *state,
                                       const GHOST_XrDrawViewInfo *draw_view,
                                       const XrSessionSettings *settings,
                                       const wmXrDrawData *draw_data)
{
  GHOST_XrPose viewer_pose;
  const bool use_position_tracking = settings->flag & XR_SESSION_USE_POSITION_TRACKING;

  mul_qt_qtqt(viewer_pose.orientation_quat,
              draw_data->base_pose.orientation_quat,
              draw_view->local_pose.orientation_quat);
  copy_v3_v3(viewer_pose.position, draw_data->base_pose.position);
  /* The local pose and the eye pose (which is copied from an earlier local pose) both are view
   * space, so Y-up. In this case we need them in regular Z-up. */
  viewer_pose.position[0] += draw_data->eye_position_ofs[0];
  viewer_pose.position[1] -= draw_data->eye_position_ofs[2];
  viewer_pose.position[2] += draw_data->eye_position_ofs[1];
  if (use_position_tracking) {
    viewer_pose.position[0] += draw_view->local_pose.position[0];
    viewer_pose.position[1] -= draw_view->local_pose.position[2];
    viewer_pose.position[2] += draw_view->local_pose.position[1];
  }

  copy_v3_v3(state->viewer_pose.position, viewer_pose.position);
  copy_qt_qt(state->viewer_pose.orientation_quat, viewer_pose.orientation_quat);
  wm_xr_pose_to_viewmat(&viewer_pose, state->viewer_viewmat);
  /* No idea why, but multiplying by two seems to make it match the VR view more. */
  state->focal_len = 2.0f *
                     fov_to_focallength(draw_view->fov.angle_right - draw_view->fov.angle_left,
                                        DEFAULT_SENSOR_WIDTH);

  copy_v3_v3(state->prev_eye_position_ofs, draw_data->eye_position_ofs);
  state->prev_settings_flag = settings->flag;
  state->is_view_data_set = true;
}

wmXrSessionState *WM_xr_session_state_handle_get(const wmXrData *xr)
{
  return xr->runtime ? &xr->runtime->session_state : NULL;
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

/** \} */ /* XR Runtime Data */

/* -------------------------------------------------------------------- */
/** \name XR-Session
 *
 * \{ */

void *wm_xr_session_gpu_binding_context_create(GHOST_TXrGraphicsBinding graphics_binding)
{
  wmSurface *surface = wm_xr_session_surface_create(G_MAIN->wm.first, graphics_binding);
  wmXrSurfaceData *data = surface->customdata;

  wm_surface_add(surface);

  /* Some regions may need to redraw with updated session state after the session is entirely up
   * and running. */
  WM_main_add_notifier(NC_WM | ND_XR_DATA_CHANGED, NULL);

  return data->secondary_ghost_ctx ? data->secondary_ghost_ctx : surface->ghost_ctx;
}

void wm_xr_session_gpu_binding_context_destroy(GHOST_TXrGraphicsBinding UNUSED(graphics_lib),
                                               GHOST_ContextHandle UNUSED(context))
{
  if (g_xr_surface) { /* Might have been freed already */
    wm_surface_remove(g_xr_surface);
  }

  wm_window_reset_drawable();

  /* Some regions may need to redraw with updated session state after the session is entirely
   * stopped. */
  WM_main_add_notifier(NC_WM | ND_XR_DATA_CHANGED, NULL);
}

static void wm_xr_session_exit_cb(void *customdata)
{
  wmXrData *xr_data = customdata;

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
  /* WM-XR exit function, does some own stuff and calls callback passed to wm_xr_session_toggle(),
   * to allow external code to execute its own session-exit logic. */
  r_begin_info->exit_fn = wm_xr_session_exit_cb;
  r_begin_info->exit_customdata = xr_data;
}

void wm_xr_session_toggle(wmWindowManager *wm, wmXrSessionExitFn session_exit_fn)
{
  wmXrData *xr_data = &wm->xr;

  if (WM_xr_session_exists(xr_data)) {
    GHOST_XrSessionEnd(xr_data->runtime->context);
  }
  else {
    GHOST_XrSessionBeginInfo begin_info;

    xr_data->runtime->session_state.is_started = true;
    xr_data->runtime->exit_fn = session_exit_fn;

    wm_xr_session_begin_info_create(xr_data, &begin_info);
    GHOST_XrSessionStart(xr_data->runtime->context, &begin_info);
  }
}

/**
 * Check if the XR-Session was triggered.
 * If an error happened while trying to start a session, this returns false too.
 */
bool WM_xr_session_exists(const wmXrData *xr)
{
  return xr->runtime && xr->runtime->context && xr->runtime->session_state.is_started;
}

/**
 * Check if the session is running, according to the OpenXR definition.
 */
bool WM_xr_session_is_ready(const wmXrData *xr)
{
  return WM_xr_session_exists(xr) && GHOST_XrSessionIsRunning(xr->runtime->context);
}

/** \} */ /* XR-Session */

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
 * Draw callback for the XR-session surface. It's expected to be called on each main loop iteration
 * and tells Ghost-XR to submit a new frame by drawing its views. Note that for drawing each view,
 * #wm_xr_draw_view() will be called through Ghost-XR (see GHOST_XrDrawViewFunc()).
 */
static void wm_xr_session_surface_draw(bContext *C)
{
  wmXrSurfaceData *surface_data = g_xr_surface->customdata;
  wmWindowManager *wm = CTX_wm_manager(C);

  if (!GHOST_XrSessionIsRunning(wm->xr.runtime->context)) {
    return;
  }
  DRW_xr_drawing_begin();
  GHOST_XrSessionDrawViews(wm->xr.runtime->context, C);
  GPU_offscreen_unbind(surface_data->offscreen, false);
  DRW_xr_drawing_end();
}

static void wm_xr_session_free_data(wmSurface *surface)
{
  wmXrSurfaceData *data = surface->customdata;

  if (data->secondary_ghost_ctx) {
#ifdef WIN32
    if (data->gpu_binding_type == GHOST_kXrGraphicsD3D11) {
      WM_directx_context_dispose(data->secondary_ghost_ctx);
    }
#endif
  }
  if (data->viewport) {
    GPU_viewport_free(data->viewport);
  }
  if (data->offscreen) {
    GPU_offscreen_free(data->offscreen);
  }

  MEM_freeN(surface->customdata);

  g_xr_surface = NULL;
}

static bool wm_xr_session_surface_offscreen_ensure(const GHOST_XrDrawViewInfo *draw_view)
{
  wmXrSurfaceData *surface_data = g_xr_surface->customdata;
  const bool size_changed = surface_data->offscreen &&
                            (GPU_offscreen_width(surface_data->offscreen) != draw_view->width) &&
                            (GPU_offscreen_height(surface_data->offscreen) != draw_view->height);
  char err_out[256] = "unknown";
  bool failure = false;

  if (surface_data->offscreen) {
    BLI_assert(surface_data->viewport);

    if (!size_changed) {
      return true;
    }
    GPU_viewport_free(surface_data->viewport);
    GPU_offscreen_free(surface_data->offscreen);
  }

  if (!(surface_data->offscreen = GPU_offscreen_create(
            draw_view->width, draw_view->height, 0, true, false, err_out))) {
    failure = true;
  }

  if (failure) {
    /* Pass. */
  }
  else if (!(surface_data->viewport = GPU_viewport_create())) {
    GPU_offscreen_free(surface_data->offscreen);
    failure = true;
  }

  if (failure) {
    CLOG_ERROR(&LOG, "Failed to get buffer, %s\n", err_out);
    return false;
  }

  return true;
}

wmSurface *wm_xr_session_surface_create(wmWindowManager *UNUSED(wm), unsigned int gpu_binding_type)
{
  if (g_xr_surface) {
    BLI_assert(false);
    return g_xr_surface;
  }

  wmSurface *surface = MEM_callocN(sizeof(*surface), __func__);
  wmXrSurfaceData *data = MEM_callocN(sizeof(*data), "XrSurfaceData");

#ifndef WIN32
  BLI_assert(gpu_binding_type == GHOST_kXrGraphicsOpenGL);
#endif

  surface->draw = wm_xr_session_surface_draw;
  surface->free_data = wm_xr_session_free_data;

  data->gpu_binding_type = gpu_binding_type;
  surface->customdata = data;

  surface->ghost_ctx = DRW_xr_opengl_context_get();

  switch (gpu_binding_type) {
    case GHOST_kXrGraphicsOpenGL:
      break;
#ifdef WIN32
    case GHOST_kXrGraphicsD3D11:
      data->secondary_ghost_ctx = WM_directx_context_create();
      break;
#endif
  }

  surface->gpu_ctx = DRW_xr_gpu_context_get();

  g_xr_surface = surface;

  return surface;
}

/** \} */ /* XR-Session Surface */

/* -------------------------------------------------------------------- */
/** \name XR Drawing
 *
 * \{ */

void wm_xr_pose_to_viewmat(const GHOST_XrPose *pose, float r_viewmat[4][4])
{
  float iquat[4];
  invert_qt_qt_normalized(iquat, pose->orientation_quat);
  quat_to_mat4(r_viewmat, iquat);
  translate_m4(r_viewmat, -pose->position[0], -pose->position[1], -pose->position[2]);
}

static void wm_xr_draw_matrices_create(const wmXrDrawData *draw_data,
                                       const GHOST_XrDrawViewInfo *draw_view,
                                       const XrSessionSettings *session_settings,
                                       float r_view_mat[4][4],
                                       float r_proj_mat[4][4])
{
  GHOST_XrPose eye_pose;

  copy_qt_qt(eye_pose.orientation_quat, draw_view->eye_pose.orientation_quat);
  copy_v3_v3(eye_pose.position, draw_view->eye_pose.position);
  add_v3_v3(eye_pose.position, draw_data->eye_position_ofs);
  if ((session_settings->flag & XR_SESSION_USE_POSITION_TRACKING) == 0) {
    sub_v3_v3(eye_pose.position, draw_view->local_pose.position);
  }

  perspective_m4_fov(r_proj_mat,
                     draw_view->fov.angle_left,
                     draw_view->fov.angle_right,
                     draw_view->fov.angle_up,
                     draw_view->fov.angle_down,
                     session_settings->clip_start,
                     session_settings->clip_end);

  float eye_mat[4][4];
  float base_mat[4][4];

  wm_xr_pose_to_viewmat(&eye_pose, eye_mat);
  /* Calculate the base pose matrix (in world space!). */
  wm_xr_pose_to_viewmat(&draw_data->base_pose, base_mat);

  mul_m4_m4m4(r_view_mat, eye_mat, base_mat);
}

static void wm_xr_draw_viewport_buffers_to_active_framebuffer(
    const wmXrSurfaceData *surface_data, const GHOST_XrDrawViewInfo *draw_view)
{
  const bool is_upside_down = surface_data->secondary_ghost_ctx &&
                              GHOST_isUpsideDownContext(surface_data->secondary_ghost_ctx);
  rcti rect = {.xmin = 0, .ymin = 0, .xmax = draw_view->width - 1, .ymax = draw_view->height - 1};

  wmViewport(&rect);

  /* For upside down contexts, draw with inverted y-values. */
  if (is_upside_down) {
    SWAP(int, rect.ymin, rect.ymax);
  }
  GPU_viewport_draw_to_screen_ex(surface_data->viewport, &rect, draw_view->expects_srgb_buffer);
}

/**
 * \brief Draw a viewport for a single eye.
 *
 * This is the main viewport drawing function for VR sessions. It's assigned to Ghost-XR as a
 * callback (see GHOST_XrDrawViewFunc()) and executed for each view (read: eye).
 */
void wm_xr_draw_view(const GHOST_XrDrawViewInfo *draw_view, void *customdata)
{
  bContext *C = customdata;
  wmWindowManager *wm = CTX_wm_manager(C);
  wmXrSurfaceData *surface_data = g_xr_surface->customdata;
  wmXrSessionState *session_state = &wm->xr.runtime->session_state;
  XrSessionSettings *settings = &wm->xr.session_settings;
  wmXrDrawData draw_data;
  Scene *scene = CTX_data_scene(C);

  const int display_flags = V3D_OFSDRAW_OVERRIDE_SCENE_SETTINGS | settings->draw_flags;

  float viewmat[4][4], winmat[4][4];

  BLI_assert(WM_xr_session_is_ready(&wm->xr));

  wm_xr_draw_data_populate(session_state, draw_view, settings, scene, &draw_data);
  wm_xr_draw_matrices_create(&draw_data, draw_view, settings, viewmat, winmat);
  wm_xr_session_state_update(session_state, draw_view, settings, &draw_data);

  if (!wm_xr_session_surface_offscreen_ensure(draw_view)) {
    return;
  }

  /* In case a framebuffer is still bound from drawing the last eye. */
  GPU_framebuffer_restore();
  /* Some systems have drawing glitches without this. */
  GPU_clear(GPU_DEPTH_BIT);

  /* Draws the view into the surface_data->viewport's framebuffers */
  ED_view3d_draw_offscreen_simple(CTX_data_ensure_evaluated_depsgraph(C),
                                  scene,
                                  &wm->xr.session_settings.shading,
                                  wm->xr.session_settings.shading.type,
                                  draw_view->width,
                                  draw_view->height,
                                  display_flags,
                                  viewmat,
                                  winmat,
                                  settings->clip_start,
                                  settings->clip_end,
                                  false,
                                  true,
                                  true,
                                  NULL,
                                  false,
                                  surface_data->offscreen,
                                  surface_data->viewport);

  /* The draw-manager uses both GPUOffscreen and GPUViewport to manage frame and texture buffers. A
   * call to GPU_viewport_draw_to_screen() is still needed to get the final result from the
   * viewport buffers composited together and potentially color managed for display on screen.
   * It needs a bound frame-buffer to draw into, for which we simply reuse the GPUOffscreen one.
   *
   * In a next step, Ghost-XR will use the the currently bound frame-buffer to retrieve the image
   * to be submitted to the OpenXR swap-chain. So do not un-bind the offscreen yet! */

  GPU_offscreen_bind(surface_data->offscreen, false);

  wm_xr_draw_viewport_buffers_to_active_framebuffer(surface_data, draw_view);
}

/** \} */ /* XR Drawing */
