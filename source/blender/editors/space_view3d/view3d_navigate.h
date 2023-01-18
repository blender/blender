/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup spview3d
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Size of the sphere being dragged for trackball rotation within the view bounds.
 * also affects speed (smaller is faster).
 */
#define V3D_OP_TRACKBALLSIZE (1.1f)

struct ARegion;
struct Depsgraph;
struct Dial;
struct Main;
struct RegionView3D;
struct Scene;
struct ScrArea;
struct View3D;
struct bContext;
struct rcti;
struct wmEvent;

enum eV3D_OpPropFlag {
  V3D_OP_PROP_MOUSE_CO = (1 << 0),
  V3D_OP_PROP_DELTA = (1 << 1),
  V3D_OP_PROP_USE_ALL_REGIONS = (1 << 2),
  V3D_OP_PROP_USE_MOUSE_INIT = (1 << 3),
};

enum {
  VIEW_PASS = 0,
  VIEW_APPLY,
  VIEW_CONFIRM,
  /** Only supported by some viewport operators. */
  VIEW_CANCEL,
};

/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */
enum {
  VIEW_MODAL_CONFIRM = 1, /* used for all view operations */
  VIEWROT_MODAL_AXIS_SNAP_ENABLE = 2,
  VIEWROT_MODAL_AXIS_SNAP_DISABLE = 3,
  VIEWROT_MODAL_SWITCH_ZOOM = 4,
  VIEWROT_MODAL_SWITCH_MOVE = 5,
  VIEWROT_MODAL_SWITCH_ROTATE = 6,
};

enum eViewOpsFlag {
  /** When enabled, rotate around the selection. */
  VIEWOPS_FLAG_ORBIT_SELECT = (1 << 0),
  /** When enabled, use the depth under the cursor for navigation. */
  VIEWOPS_FLAG_DEPTH_NAVIGATE = (1 << 1),
  /**
   * When enabled run #ED_view3d_persp_ensure this may switch out of camera view
   * when orbiting or switch from orthographic to perspective when auto-perspective is enabled.
   * Some operations don't require this (view zoom/pan or NDOF where subtle rotation is common
   * so we don't want it to trigger auto-perspective). */
  VIEWOPS_FLAG_PERSP_ENSURE = (1 << 2),
  /** When set, ignore any options that depend on initial cursor location. */
  VIEWOPS_FLAG_USE_MOUSE_INIT = (1 << 3),
};

/** Generic View Operator Custom-Data */
typedef struct ViewOpsData {
  /** Context pointers (assigned by #viewops_data_create). */
  struct Main *bmain;
  struct Scene *scene;
  struct ScrArea *area;
  struct ARegion *region;
  struct View3D *v3d;
  struct RegionView3D *rv3d;
  struct Depsgraph *depsgraph;

  /** Needed for continuous zoom. */
  struct wmTimer *timer;

  /** Viewport state on initialization, don't change afterwards. */
  struct {
    float dist;
    float camzoom;
    float quat[4];
    /** #wmEvent.xy. */
    int event_xy[2];
    /** Offset to use when #VIEWOPS_FLAG_USE_MOUSE_INIT is not set.
     * so we can simulate pressing in the middle of the screen. */
    int event_xy_offset[2];
    /** #wmEvent.type that triggered the operator. */
    int event_type;
    float ofs[3];
    /** #RegionView3D.ofs_lock */
    float ofs_lock[2];
    /** Initial distance to 'ofs'. */
    float zfac;

    /** Camera offset. */
    float camdx, camdy;

    /** Trackball rotation only. */
    float trackvec[3];
    /** Dolly only. */
    float mousevec[3];

    /**
     * #RegionView3D.persp set after auto-perspective is applied.
     * If we want the value before running the operator, add a separate member.
     */
    char persp_with_auto_persp_applied;
    /** #RegionView3D.persp set after before auto-perspective is applied. */
    char persp;
    /** #RegionView3D.view */
    char view;
    /** #RegionView3D.view_axis_roll */
    char view_axis_roll;

    /** Used for roll */
    struct Dial *dial;
  } init;

  /** Previous state (previous modal event handled). */
  struct {
    int event_xy[2];
    /** For operators that use time-steps (continuous zoom). */
    double time;
  } prev;

  /** Current state. */
  struct {
    /** Working copy of #RegionView3D.viewquat, needed for rotation calculation
     * so we can apply snap to the 3D Viewport while keeping the unsnapped rotation
     * here to use when snap is disabled and for continued calculation. */
    float viewquat[4];
  } curr;

  float reverse;
  bool axis_snap; /* view rotate only */

  /** Use for orbit selection and auto-dist. */
  float dyn_ofs[3];
  bool use_dyn_ofs;
} ViewOpsData;

/* view3d_navigate.c */

bool view3d_location_poll(struct bContext *C);
bool view3d_rotation_poll(struct bContext *C);
bool view3d_zoom_or_dolly_poll(struct bContext *C);

enum eViewOpsFlag viewops_flag_from_prefs(void);
void calctrackballvec(const struct rcti *rect, const int event_xy[2], float r_dir[3]);
void viewmove_apply(ViewOpsData *vod, int x, int y);
void viewmove_apply_reset(ViewOpsData *vod);
void view3d_orbit_apply_dyn_ofs(float r_ofs[3],
                                const float ofs_old[3],
                                const float viewquat_old[4],
                                const float viewquat_new[4],
                                const float dyn_ofs[3]);
void viewrotate_apply_dyn_ofs(ViewOpsData *vod, const float viewquat_new[4]);
bool view3d_orbit_calc_center(struct bContext *C, float r_dyn_ofs[3]);

void view3d_operator_properties_common(struct wmOperatorType *ot, const enum eV3D_OpPropFlag flag);

/**
 * Allocate and fill in context pointers for #ViewOpsData
 */
void viewops_data_free(struct bContext *C, ViewOpsData *vod);

/**
 * Allocate, fill in context pointers and calculate the values for #ViewOpsData
 */
ViewOpsData *viewops_data_create(struct bContext *C,
                                 const struct wmEvent *event,
                                 enum eViewOpsFlag viewops_flag);

void VIEW3D_OT_view_all(struct wmOperatorType *ot);
void VIEW3D_OT_view_selected(struct wmOperatorType *ot);
void VIEW3D_OT_view_center_cursor(struct wmOperatorType *ot);
void VIEW3D_OT_view_center_pick(struct wmOperatorType *ot);
void VIEW3D_OT_view_axis(struct wmOperatorType *ot);
void VIEW3D_OT_view_camera(struct wmOperatorType *ot);
void VIEW3D_OT_view_orbit(struct wmOperatorType *ot);
void VIEW3D_OT_view_pan(struct wmOperatorType *ot);

/* view3d_navigate_dolly.c */

void viewdolly_modal_keymap(struct wmKeyConfig *keyconf);
void VIEW3D_OT_dolly(struct wmOperatorType *ot);

/* view3d_navigate_fly.c */

void fly_modal_keymap(struct wmKeyConfig *keyconf);
void view3d_keymap(struct wmKeyConfig *keyconf);
void VIEW3D_OT_fly(struct wmOperatorType *ot);

/* view3d_navigate_move.c */

void viewmove_modal_keymap(struct wmKeyConfig *keyconf);
void VIEW3D_OT_move(struct wmOperatorType *ot);

/* view3d_navigate_ndof.c */

#ifdef WITH_INPUT_NDOF
struct wmNDOFMotionData;

/**
 * Called from both fly mode and walk mode,
 */
void view3d_ndof_fly(const struct wmNDOFMotionData *ndof,
                     struct View3D *v3d,
                     struct RegionView3D *rv3d,
                     bool use_precision,
                     short protectflag,
                     bool *r_has_translate,
                     bool *r_has_rotate);
void VIEW3D_OT_ndof_orbit(struct wmOperatorType *ot);
void VIEW3D_OT_ndof_orbit_zoom(struct wmOperatorType *ot);
void VIEW3D_OT_ndof_pan(struct wmOperatorType *ot);
void VIEW3D_OT_ndof_all(struct wmOperatorType *ot);
#endif /* WITH_INPUT_NDOF */

/* view3d_navigate_roll.c */

void VIEW3D_OT_view_roll(struct wmOperatorType *ot);

/* view3d_navigate_rotate.c */

void viewrotate_modal_keymap(struct wmKeyConfig *keyconf);
void VIEW3D_OT_rotate(struct wmOperatorType *ot);

/* view3d_navigate_smoothview.c */

/**
 * Parameters for setting the new 3D Viewport state.
 *
 * Each of the struct members may be NULL to signify they aren't to be adjusted.
 */
typedef struct V3D_SmoothParams {
  struct Object *camera_old, *camera;
  const float *ofs, *quat, *dist, *lens;

  /** Alternate rotation center, when set `ofs` must be NULL. */
  const float *dyn_ofs;

  /** When non-NULL, perform undo pushes when transforming the camera. */
  const char *undo_str;
  /**
   * When true use grouped undo pushes, use for incremental viewport manipulation
   * which are likely to be activated by holding a key or from the mouse-wheel.
   */
  bool undo_grouped;
} V3D_SmoothParams;

/**
 * The arguments are the desired situation.
 */
void ED_view3d_smooth_view_ex(const struct Depsgraph *depsgraph,
                              struct wmWindowManager *wm,
                              struct wmWindow *win,
                              struct ScrArea *area,
                              struct View3D *v3d,
                              struct ARegion *region,
                              int smooth_viewtx,
                              const V3D_SmoothParams *sview);

void ED_view3d_smooth_view(struct bContext *C,
                           struct View3D *v3d,
                           struct ARegion *region,
                           int smooth_viewtx,
                           const V3D_SmoothParams *sview);

/**
 * Call before multiple smooth-view operations begin to properly handle undo.
 *
 * \note Only use explicit undo calls when multiple calls to smooth-view are necessary
 * or when calling #ED_view3d_smooth_view_ex.
 * Otherwise pass in #V3D_SmoothParams.undo_str so an undo step is pushed as needed.
 */
void ED_view3d_smooth_view_undo_begin(struct bContext *C, const struct ScrArea *area);
/**
 * Run after multiple smooth-view operations have run to push undo as needed.
 */
void ED_view3d_smooth_view_undo_end(struct bContext *C,
                                    const struct ScrArea *area,
                                    const char *undo_str,
                                    bool undo_grouped);

/**
 * Apply the smooth-view immediately, use when we need to start a new view operation.
 * (so we don't end up half-applying a view operation when pressing keys quickly).
 */
void ED_view3d_smooth_view_force_finish(struct bContext *C,
                                        struct View3D *v3d,
                                        struct ARegion *region);

void VIEW3D_OT_smoothview(struct wmOperatorType *ot);

/* view3d_navigate_walk.c */

void walk_modal_keymap(struct wmKeyConfig *keyconf);
void VIEW3D_OT_walk(struct wmOperatorType *ot);

/* view3d_navigate_zoom.c */

void viewzoom_modal_keymap(struct wmKeyConfig *keyconf);
void VIEW3D_OT_zoom(struct wmOperatorType *ot);

/* view3d_navigate_zoom_border.c */

void VIEW3D_OT_zoom_border(struct wmOperatorType *ot);

#ifdef __cplusplus
}
#endif
