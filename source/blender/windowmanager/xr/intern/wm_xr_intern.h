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
 */

#pragma once

#include "CLG_log.h"

#include "wm_xr.h"

typedef struct wmXrSessionState {
  bool is_started;

  /** Last known viewer pose (centroid of eyes, in world space) stored for queries. */
  GHOST_XrPose viewer_pose;
  /** The last known view matrix, calculated from above's viewer pose. */
  float viewer_viewmat[4][4];
  float focal_len;

  /** Copy of XrSessionSettings.base_pose_ data to detect changes that need
   * resetting to base pose. */
  char prev_base_pose_type; /* eXRSessionBasePoseType */
  Object *prev_base_pose_object;
  /** Copy of XrSessionSettings.flag created on the last draw call, stored to detect changes. */
  int prev_settings_flag;
  /** Copy of wmXrDrawData.eye_position_ofs. */
  float prev_eye_position_ofs[3];

  bool force_reset_to_base_pose;
  bool is_view_data_set;
} wmXrSessionState;

typedef struct wmXrRuntimeData {
  GHOST_XrContextHandle context;

  /** The window the session was started in. Stored to be able to follow its view-layer. This may
   * be an invalid reference, i.e. the window may have been closed. */
  wmWindow *session_root_win;

  /** Although this struct is internal, RNA gets a handle to this for state information queries. */
  wmXrSessionState session_state;
  wmXrSessionExitFn exit_fn;
} wmXrRuntimeData;

typedef struct {
  struct GPUOffScreen *offscreen;
  struct GPUViewport *viewport;
} wmXrSurfaceData;

typedef struct wmXrDrawData {
  struct Scene *scene;
  struct Depsgraph *depsgraph;

  wmXrData *xr_data;
  wmXrSurfaceData *surface_data;

  /** The pose (location + rotation) to which eye deltas will be applied to when drawing (world
   * space). With positional tracking enabled, it should be the same as the base pose, when
   * disabled it also contains a location delta from the moment the option was toggled. */
  GHOST_XrPose base_pose;
  /** Offset to _substract_ from the OpenXR eye and viewer pose to get the wanted effective pose
   * (e.g. a pose exactly at the landmark position). */
  float eye_position_ofs[3]; /* Local/view space. */
} wmXrDrawData;

wmXrRuntimeData *wm_xr_runtime_data_create(void);
void wm_xr_runtime_data_free(wmXrRuntimeData **runtime);

void wm_xr_session_draw_data_update(const wmXrSessionState *state,
                                    const XrSessionSettings *settings,
                                    const GHOST_XrDrawViewInfo *draw_view,
                                    wmXrDrawData *draw_data);
void wm_xr_session_state_update(const XrSessionSettings *settings,
                                const wmXrDrawData *draw_data,
                                const GHOST_XrDrawViewInfo *draw_view,
                                wmXrSessionState *state);
bool wm_xr_session_surface_offscreen_ensure(wmXrSurfaceData *surface_data,
                                            const GHOST_XrDrawViewInfo *draw_view);
void *wm_xr_session_gpu_binding_context_create(void);
void wm_xr_session_gpu_binding_context_destroy(GHOST_ContextHandle context);

void wm_xr_pose_to_viewmat(const GHOST_XrPose *pose, float r_viewmat[4][4]);
void wm_xr_draw_view(const GHOST_XrDrawViewInfo *draw_view, void *customdata);
