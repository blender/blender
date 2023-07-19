/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#pragma once

#include "BLI_utildefines.h"

/**
 * Size of the sphere being dragged for trackball rotation within the view bounds.
 * also affects speed (smaller is faster).
 */
#define V3D_OP_TRACKBALLSIZE (1.1f)

struct ARegion;
struct Depsgraph;
struct Dial;
struct RegionView3D;
struct Scene;
struct ScrArea;
struct View3D;
struct bContext;
struct rcti;
struct wmEvent;
struct wmKeyConfig;
struct wmKeyMap;
struct wmOperator;
struct wmOperatorType;
struct wmWindowManager;

enum eV3D_OpMode {
  V3D_OP_MODE_NONE = -1,
  V3D_OP_MODE_ZOOM = 0,
  V3D_OP_MODE_ROTATE,
  V3D_OP_MODE_MOVE,
  V3D_OP_MODE_VIEW_PAN,
  V3D_OP_MODE_VIEW_ROLL,
  V3D_OP_MODE_DOLLY,
#ifdef WITH_INPUT_NDOF
  V3D_OP_MODE_NDOF_ORBIT,
  V3D_OP_MODE_NDOF_ORBIT_ZOOM,
  V3D_OP_MODE_NDOF_PAN,
  V3D_OP_MODE_NDOF_ALL,
#endif
};
#ifndef WITH_INPUT_NDOF
#  define V3D_OP_MODE_LEN V3D_OP_MODE_DOLLY + 1
#else
#  define V3D_OP_MODE_LEN V3D_OP_MODE_NDOF_ALL + 1
#endif

enum eV3D_OpPropFlag {
  V3D_OP_PROP_MOUSE_CO = (1 << 0),
  V3D_OP_PROP_DELTA = (1 << 1),
  V3D_OP_PROP_USE_ALL_REGIONS = (1 << 2),
  V3D_OP_PROP_USE_MOUSE_INIT = (1 << 3),
};
ENUM_OPERATORS(eV3D_OpPropFlag, V3D_OP_PROP_USE_MOUSE_INIT);

enum eV3D_OpEvent {
  VIEW_PASS = 0,
  VIEW_APPLY,
  VIEW_CONFIRM,
  /** Only supported by some viewport operators. */
  VIEW_CANCEL,
};

/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */
enum {
  VIEW_MODAL_CANCEL = 0,  /* used for all view operations */
  VIEW_MODAL_CONFIRM = 1, /* used for all view operations */
  VIEWROT_MODAL_AXIS_SNAP_ENABLE = 2,
  VIEWROT_MODAL_AXIS_SNAP_DISABLE = 3,
  VIEWROT_MODAL_SWITCH_ZOOM = 4,
  VIEWROT_MODAL_SWITCH_MOVE = 5,
  VIEWROT_MODAL_SWITCH_ROTATE = 6,
};

enum eViewOpsFlag {
  VIEWOPS_FLAG_NONE = 0,
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

  VIEWOPS_FLAG_ZOOM_TO_MOUSE = (1 << 4),
};
ENUM_OPERATORS(eViewOpsFlag, VIEWOPS_FLAG_ZOOM_TO_MOUSE);

/** Generic View Operator Custom-Data */
struct ViewOpsData {
  /** Context pointers (assigned by #viewops_data_create). */
  Scene *scene;
  ScrArea *area;
  ARegion *region;
  View3D *v3d;
  RegionView3D *rv3d;
  Depsgraph *depsgraph;

  /** Needed for continuous zoom. */
  wmTimer *timer;

  /** Viewport state on initialization, don't change afterwards. */
  struct {

    /** These variables reflect the same in #RegionView3D. */

    float ofs[3];        /* DOLLY, MOVE, ROTATE and ZOOM. */
    float ofs_lock[2];   /* MOVE. */
    float camdx, camdy;  /* MOVE and ZOOM. */
    float camzoom;       /* ZOOM. */
    float dist;          /* ROTATE and ZOOM. */
    float quat[4];       /* ROLL and ROTATE. */
    char persp;          /* ROTATE. */
    char view;           /* ROTATE. */
    char view_axis_roll; /* ROTATE. */

    /**
     * #RegionView3D.persp set after auto-perspective is applied.
     * If we want the value before running the operator, add a separate member.
     */
    char persp_with_auto_persp_applied;

    /** The ones below are unrelated to the state of the 3D view. */

    /** #wmEvent.xy. */
    int event_xy[2];
    /** Offset to use when #VIEWOPS_FLAG_USE_MOUSE_INIT is not set.
     * so we can simulate pressing in the middle of the screen. */
    int event_xy_offset[2];
    /** #wmEvent.type that triggered the operator. */
    int event_type;

    /** Initial distance to 'ofs'. */
    float zfac;

    /** Trackball rotation only. */
    float trackvec[3];
    /** Dolly only. */
    float mousevec[3];

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

  eV3D_OpMode nav_type;
  eViewOpsFlag viewops_flag;

  float reverse;
  bool axis_snap; /* view rotate only */

  /** Use for orbit selection and auto-dist. */
  float dyn_ofs[3];
  bool use_dyn_ofs;

  /**
   * In orthographic views, a dynamic offset should not cause #RegionView3D::ofs to end up
   * at a location that has no relation to the content where `ofs` originated or to `dyn_ofs`.
   * Failing to do so can cause the orthographic views `ofs` to be far away from the content
   * to the point it gets clipped out of the view.
   * See #view3d_orbit_apply_dyn_ofs code-comments for an example, also see: #104385.
   */
  bool use_dyn_ofs_ortho_correction;

  /** Used for navigation on non view3d operators. */
  wmKeyMap *keymap;
  bool is_modal_event;

  void init_context(bContext *C);
  void state_backup();
  void state_restore();
  void init_navigation(bContext *C,
                       const wmEvent *event,
                       const eV3D_OpMode nav_type,
                       const bool use_cursor_init);
  void end_navigation(bContext *C);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("ViewOpsData")
#endif
};

/* view3d_navigate.cc */

/**
 * Navigation operators that share the `ViewOpsData` utility.
 */
const char *viewops_operator_idname_get(eV3D_OpMode nav_type);

bool view3d_location_poll(bContext *C);
bool view3d_rotation_poll(bContext *C);
bool view3d_zoom_or_dolly_poll(bContext *C);

int view3d_navigate_invoke_impl(bContext *C,
                                wmOperator *op,
                                const wmEvent *event,
                                const eV3D_OpMode nav_type);
int view3d_navigate_modal_fn(bContext *C, wmOperator *op, const wmEvent *event);
void view3d_navigate_cancel_fn(bContext *C, wmOperator *op);

void calctrackballvec(const rcti *rect, const int event_xy[2], float r_dir[3]);
void viewmove_apply(ViewOpsData *vod, int x, int y);
void view3d_orbit_apply_dyn_ofs(float r_ofs[3],
                                const float ofs_old[3],
                                const float viewquat_old[4],
                                const float viewquat_new[4],
                                const float dyn_ofs[3]);
void viewrotate_apply_dyn_ofs(ViewOpsData *vod, const float viewquat_new[4]);
bool view3d_orbit_calc_center(bContext *C, float r_dyn_ofs[3]);

void view3d_operator_properties_common(wmOperatorType *ot, const eV3D_OpPropFlag flag);

/**
 * Allocate and fill in context pointers for #ViewOpsData
 */
void viewops_data_free(bContext *C, ViewOpsData *vod);

/**
 * Allocate, fill in context pointers and calculate the values for #ViewOpsData
 */
ViewOpsData *viewops_data_create(bContext *C,
                                 const wmEvent *event,
                                 const eV3D_OpMode nav_type,
                                 const bool use_cursor_init);
void axis_set_view(bContext *C,
                   View3D *v3d,
                   ARegion *region,
                   const float quat_[4],
                   char view,
                   char view_axis_roll,
                   int perspo,
                   const float *align_to_quat,
                   const int smooth_viewtx);

/* view3d_navigate_dolly.cc */

void viewdolly_modal_keymap(wmKeyConfig *keyconf);
void VIEW3D_OT_dolly(wmOperatorType *ot);

/* view3d_navigate_fly.cc */

void fly_modal_keymap(wmKeyConfig *keyconf);
void view3d_keymap(wmKeyConfig *keyconf);
void VIEW3D_OT_fly(wmOperatorType *ot);

/* view3d_navigate_move.cc */

int viewmove_modal_impl(bContext *C,
                        ViewOpsData *vod,
                        const eV3D_OpEvent event_code,
                        const int xy[2]);
int viewmove_invoke_impl(ViewOpsData *vod, const wmEvent *event);
void viewmove_modal_keymap(wmKeyConfig *keyconf);
void VIEW3D_OT_move(wmOperatorType *ot);

/* view3d_navigate_ndof.cc */

#ifdef WITH_INPUT_NDOF
struct wmNDOFMotionData;

int ndof_orbit_invoke_impl(bContext *C, ViewOpsData *vod, const wmEvent *event);
int ndof_orbit_zoom_invoke_impl(bContext *C, ViewOpsData *vod, const wmEvent *event);
int ndof_pan_invoke_impl(bContext *C, ViewOpsData *vod, const wmEvent *event);
int ndof_all_invoke_impl(bContext *C, ViewOpsData *vod, const wmEvent *event);

/**
 * Called from both fly mode and walk mode,
 */
void view3d_ndof_fly(const wmNDOFMotionData *ndof,
                     View3D *v3d,
                     RegionView3D *rv3d,
                     bool use_precision,
                     short protectflag,
                     bool *r_has_translate,
                     bool *r_has_rotate);
void VIEW3D_OT_ndof_orbit(wmOperatorType *ot);
void VIEW3D_OT_ndof_orbit_zoom(wmOperatorType *ot);
void VIEW3D_OT_ndof_pan(wmOperatorType *ot);
void VIEW3D_OT_ndof_all(wmOperatorType *ot);
#endif /* WITH_INPUT_NDOF */

/* view3d_navigate_roll.cc */

void VIEW3D_OT_view_roll(wmOperatorType *ot);

/* view3d_navigate_rotate.cc */

int viewrotate_modal_impl(bContext *C,
                          ViewOpsData *vod,
                          const eV3D_OpEvent event_code,
                          const int xy[2]);
int viewrotate_invoke_impl(ViewOpsData *vod, const wmEvent *event);
void viewrotate_modal_keymap(wmKeyConfig *keyconf);
void VIEW3D_OT_rotate(wmOperatorType *ot);

/* view3d_navigate_smoothview.cc */

/**
 * Parameters for setting the new 3D Viewport state.
 *
 * Each of the struct members may be NULL to signify they aren't to be adjusted.
 */
struct V3D_SmoothParams {
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
};

/**
 * The arguments are the desired situation.
 */
void ED_view3d_smooth_view_ex(const Depsgraph *depsgraph,
                              wmWindowManager *wm,
                              wmWindow *win,
                              ScrArea *area,
                              View3D *v3d,
                              ARegion *region,
                              int smooth_viewtx,
                              const V3D_SmoothParams *sview);

void ED_view3d_smooth_view(
    bContext *C, View3D *v3d, ARegion *region, int smooth_viewtx, const V3D_SmoothParams *sview);

/**
 * Call before multiple smooth-view operations begin to properly handle undo.
 *
 * \note Only use explicit undo calls when multiple calls to smooth-view are necessary
 * or when calling #ED_view3d_smooth_view_ex.
 * Otherwise pass in #V3D_SmoothParams.undo_str so an undo step is pushed as needed.
 */
void ED_view3d_smooth_view_undo_begin(bContext *C, const ScrArea *area);
/**
 * Run after multiple smooth-view operations have run to push undo as needed.
 */
void ED_view3d_smooth_view_undo_end(bContext *C,
                                    const ScrArea *area,
                                    const char *undo_str,
                                    bool undo_grouped);

/**
 * Apply the smooth-view immediately, use when we need to start a new view operation.
 * (so we don't end up half-applying a view operation when pressing keys quickly).
 */
void ED_view3d_smooth_view_force_finish(bContext *C, View3D *v3d, ARegion *region);

void VIEW3D_OT_smoothview(wmOperatorType *ot);

/* view3d_navigate_view_all.cc */

void VIEW3D_OT_view_all(wmOperatorType *ot);
void VIEW3D_OT_view_selected(wmOperatorType *ot);

/* view3d_navigate_view_axis.cc */

void VIEW3D_OT_view_axis(wmOperatorType *ot);

/* view3d_navigate_view_camera.cc */

void VIEW3D_OT_view_camera(wmOperatorType *ot);

/* view3d_navigate_view_center_cursor.cc */

void VIEW3D_OT_view_center_cursor(wmOperatorType *ot);

/* view3d_navigate_view_center_pick.cc */

void VIEW3D_OT_view_center_pick(wmOperatorType *ot);

/* view3d_navigate_view_orbit.cc */

void VIEW3D_OT_view_orbit(wmOperatorType *ot);

/* view3d_navigate_view_pan.cc */

int viewpan_invoke_impl(ViewOpsData *vod, PointerRNA *ptr);
void VIEW3D_OT_view_pan(wmOperatorType *ot);

/* view3d_navigate_walk.cc */

void walk_modal_keymap(wmKeyConfig *keyconf);
void VIEW3D_OT_walk(wmOperatorType *ot);

/* view3d_navigate_zoom.cc */

int viewzoom_modal_impl(bContext *C,
                        ViewOpsData *vod,
                        const eV3D_OpEvent event_code,
                        const int xy[2]);
int viewzoom_invoke_impl(bContext *C, ViewOpsData *vod, const wmEvent *event, PointerRNA *ptr);
void viewzoom_modal_keymap(wmKeyConfig *keyconf);
void VIEW3D_OT_zoom(wmOperatorType *ot);

/* view3d_navigate_zoom_border.cc */

void VIEW3D_OT_zoom_border(wmOperatorType *ot);
