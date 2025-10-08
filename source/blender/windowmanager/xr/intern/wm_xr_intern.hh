/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#pragma once

#include "CLG_log.h"

#include "GHOST_Types.h"

#include "DNA_listBase.h"
#include "DNA_xr_types.h"

#include "wm_xr.hh"

struct bContext;
struct ARegion;
struct Object;
struct wmWindow;
struct wmWindowManager;
struct wmXrActionSet;
struct wmXrData;

struct wmXrSessionState {
  bool is_started;

  /** Last known viewer pose (centroid of eyes, in world space) stored for queries. */
  GHOST_XrPose viewer_pose;
  /** The last known view matrix, calculated from above's viewer pose. */
  float viewer_viewmat[4][4];
  /** The last known viewer matrix, without navigation applied. */
  float viewer_mat_base[4][4];
  float focal_len;

  /** Copy of XrSessionSettings.base_pose_ data to detect changes that need
   * resetting to base pose. */
  char prev_base_pose_type; /* #eXRSessionBasePoseType. */
  Object *prev_base_pose_object;
  /** Copy of XrSessionSettings.flag created on the last draw call, stored to detect changes. */
  int prev_settings_flag;
  /** Copy of wmXrDrawData.base_pose. */
  GHOST_XrPose prev_base_pose;
  /** Copy of wmXrDrawData.base_scale. */
  float prev_base_scale;
  /** Copy of GHOST_XrDrawViewInfo.local_pose. */
  GHOST_XrPose prev_local_pose;
  /** Copy of wmXrDrawData.eye_position_ofs. */
  float prev_eye_position_ofs[3];

  bool force_reset_to_base_pose;
  bool is_view_data_set;
  bool swap_hands;
  bool is_raycast_shown;

  /** Current navigation transforms. */
  GHOST_XrPose nav_pose;
  float nav_scale;
  /** Navigation transforms from the last actions sync, used to calculate the viewer/controller
   * poses. */
  GHOST_XrPose nav_pose_prev;
  float nav_scale_prev;
  bool is_navigation_dirty;

  /** Last known controller data. */
  ListBase controllers; /* #wmXrController. */

  /** The currently active action set that will be updated on calls to
   * #wm_xr_session_actions_update(). If NULL, all action sets will be treated as active and
   * updated. */
  struct wmXrActionSet *active_action_set;
  /* Name of the action set (if any) to activate before the next actions sync. */
  char active_action_set_next[64]; /* #MAX_NAME. */

  /** The current state and parameters of the vignette that appears while moving. */
  struct wmXrVignetteData *vignette_data;

  /** Model used to draw teleportation raycast. */
  blender::gpu::Batch *raycast_model;
};

struct wmXrRuntimeData {
  GHOST_XrContextHandle context;

  /** The window the session was started in. Stored to be able to follow its view-layer. This may
   * be an invalid reference, i.e. the window may have been closed. */
  wmWindow *session_root_win;

  /** Off-screen area used for XR events. */
  struct ScrArea *area;

  /** Although this struct is internal, RNA gets a handle to this for state information queries. */
  wmXrSessionState session_state;
  wmXrSessionExitFn exit_fn;

  ListBase actionmaps; /* #XrActionMap. */
  short actactionmap;
  short selactionmap;
};

struct wmXrViewportPair {
  struct wmXrViewportPair *next, *prev;
  struct GPUOffScreen *offscreen;
  struct GPUViewport *viewport;
};

struct wmXrSurfaceData {
  /** Off-screen buffers/viewports for each view. */
  ListBase viewports; /* #wmXrViewportPair. */

  /** Dummy region type for controller draw callback. */
  struct ARegionType *controller_art;
  /** Controller draw callback handle. */
  void *controller_draw_handle;
};

struct wmXrDrawData {
  struct Scene *scene;
  struct Depsgraph *depsgraph;

  wmXrData *xr_data;
  wmXrSurfaceData *surface_data;

  /** The pose (location + rotation) to which eye deltas will be applied to when drawing (world
   * space). With positional tracking enabled, it should be the same as the base pose, when
   * disabled it also contains a location delta from the moment the option was toggled. */
  GHOST_XrPose base_pose;
  /** Base scale (uniform, world space). */
  float base_scale;
  /** Offset to _subtract_ from the OpenXR eye and viewer pose to get the wanted effective pose
   * (e.g. a pose exactly at the landmark position). */
  float eye_position_ofs[3]; /* Local/view space. */
};

struct wmXrController {
  struct wmXrController *next, *prev;
  /** OpenXR user path identifier. */
  char subaction_path[64]; /* #XR_MAX_USER_PATH_LENGTH. */

  /** Pose (in world space) that represents the user's hand when holding the controller. */
  bool grip_active;
  GHOST_XrPose grip_pose;
  float grip_mat[4][4];
  float grip_mat_base[4][4];
  /** Pose (in world space) that represents the controller's aiming source. */
  bool aim_active;
  GHOST_XrPose aim_pose;
  float aim_mat[4][4];
  float aim_mat_base[4][4];

  /** Controller model. */
  blender::gpu::Batch *model;
};

struct wmXrAction {
  char *name;
  eXrActionType type;
  unsigned int count_subaction_paths;
  char **subaction_paths;
  /** States for each subaction path. */
  void *states;
  /** Previous states, stored to determine XR events. */
  void *states_prev;

  /** Input thresholds/regions for each subaction path. */
  float *float_thresholds;
  eXrAxisFlag *axis_flags;

  /** The currently active subaction path (if any) for modal actions. */
  const char *active_modal_path;

  /** Operator to be called on XR events. */
  struct wmOperatorType *ot;
  IDProperty *op_properties;

  /** Haptics. */
  char *haptic_name;
  int64_t haptic_duration;
  float haptic_frequency;
  float haptic_amplitude;

  /** Flags. */
  eXrOpFlag op_flag;
  eXrActionFlag action_flag;
  eXrHapticFlag haptic_flag;
};

struct wmXrHapticAction {
  struct wmXrHapticAction *next, *prev;
  wmXrAction *action;
  const char *subaction_path;
  int64_t time_start;
};

struct wmXrActionSet {
  char *name;

  /** XR pose actions that determine the controller grip/aim transforms. */
  wmXrAction *controller_grip_action;
  wmXrAction *controller_aim_action;

  /** Currently active modal actions. */
  ListBase active_modal_actions;
  /** Currently active haptic actions. */
  ListBase active_haptic_actions;
};

struct wmXrVignetteData {
  /** Vignette state. */
  float aperture;
  float aperture_velocity;

  /** Vignette parameters. */
  float initial_aperture;
  float initial_aperture_velocity;

  float aperture_min;
  float aperture_max;

  float aperture_velocity_max;
  float aperture_velocity_delta;
};

/* `wm_xr.cc` */

wmXrRuntimeData *wm_xr_runtime_data_create();
void wm_xr_runtime_data_free(wmXrRuntimeData **runtime);

/* `wm_xr_session.cc` */

void wm_xr_session_data_free(wmXrSessionState *state);
wmWindow *wm_xr_session_root_window_or_fallback_get(const wmWindowManager *wm,
                                                    const wmXrRuntimeData *runtime_data);
void wm_xr_session_draw_data_update(wmXrSessionState *state,
                                    const XrSessionSettings *settings,
                                    const GHOST_XrDrawViewInfo *draw_view,
                                    wmXrDrawData *draw_data);
/**
 * Update information that is only stored for external state queries. E.g. for Python API to
 * request the current (as in, last known) viewer pose.
 * Controller data and action sets will be updated separately via wm_xr_session_actions_update().
 */
void wm_xr_session_state_update(const XrSessionSettings *settings,
                                const wmXrDrawData *draw_data,
                                const GHOST_XrDrawViewInfo *draw_view,
                                wmXrSessionState *state);
bool wm_xr_session_surface_offscreen_ensure(wmXrSurfaceData *surface_data,
                                            const GHOST_XrDrawViewInfo *draw_view);
void *wm_xr_session_gpu_binding_context_create();
void wm_xr_session_gpu_binding_context_destroy(GHOST_ContextHandle context);

void wm_xr_session_actions_init(wmXrData *xr);
void wm_xr_session_actions_update(wmWindowManager *wm);
void wm_xr_session_controller_data_populate(const wmXrAction *grip_action,
                                            const wmXrAction *aim_action,
                                            wmXrData *xr);
void wm_xr_session_controller_data_clear(wmXrSessionState *state);

/* `wm_xr_draw.cc` */

void wm_xr_pose_to_mat(const GHOST_XrPose *pose, float r_mat[4][4]);
void wm_xr_pose_scale_to_mat(const GHOST_XrPose *pose, float scale, float r_mat[4][4]);
void wm_xr_pose_to_imat(const GHOST_XrPose *pose, float r_imat[4][4]);
void wm_xr_pose_scale_to_imat(const GHOST_XrPose *pose, float scale, float r_imat[4][4]);
/**
 * \brief Draw a viewport for a single eye.
 *
 * This is the main viewport drawing function for VR sessions. It's assigned to Ghost-XR as a
 * callback (see GHOST_XrDrawViewFunc()) and executed for each view (read: eye).
 */
void wm_xr_draw_view(const GHOST_XrDrawViewInfo *draw_view, void *customdata);
void wm_xr_draw_controllers(const bContext *C, ARegion *region, void *customdata);

/**
 * \brief Check if XR passthrough is enabled.
 *
 * Needed to add or not the passthrough composition layer.
 * It's assigned to Ghost-XR as a callback (see GHOST_XrPassthroughEnabledFunc()).
 */
bool wm_xr_passthrough_enabled(void *customdata);
/**
 * \brief Disable XR passthrough if not supported.
 *
 * In case passthrough is not supported by the XR runtime, force un-check the toggle in the GUI.
 * It's assigned to Ghost-XR as a callback (see GHOST_XrDisablePassthroughFunc()).
 */
void wm_xr_disable_passthrough(void *customdata);
