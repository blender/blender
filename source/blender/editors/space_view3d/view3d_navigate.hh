/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#pragma once

#include <optional>

#include "MEM_guardedalloc.h"

#include "BLI_bounds_types.hh"
#include "BLI_enum_flags.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_windowmanager_enums.h"

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
struct ViewOpsData;
struct bContext;
struct Object;
struct PointerRNA;
struct rcti;
struct wmEvent;
struct wmKeyConfig;
struct wmOperator;
struct wmOperatorType;
struct wmTimer;
struct wmWindow;
struct wmWindowManager;
struct ViewLayer;

enum eV3D_OpPropFlag {
  V3D_OP_PROP_MOUSE_CO = (1 << 0),
  V3D_OP_PROP_DELTA = (1 << 1),
  V3D_OP_PROP_USE_ALL_REGIONS = (1 << 2),
  V3D_OP_PROP_USE_MOUSE_INIT = (1 << 3),
};
ENUM_OPERATORS(eV3D_OpPropFlag);

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

  VIEWOPS_FLAG_ZOOM_TO_MOUSE = (1 << 3),

  VIEWOPS_FLAG_INIT_ZFAC = (1 << 4),
};
ENUM_OPERATORS(eViewOpsFlag);

struct ViewOpsType {
  eViewOpsFlag flag;
  const char *idname;
  bool (*poll_fn)(bContext *C);
  wmOperatorStatus (*init_fn)(bContext *C,
                              ViewOpsData *vod,
                              const wmEvent *event,
                              PointerRNA *ptr);
  wmOperatorStatus (*apply_fn)(bContext *C,
                               ViewOpsData *vod,
                               const eV3D_OpEvent event_code,
                               const int xy[2]);
};

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
    blender::int2 event_xy;
    /* Offset used when "use_cursor_init" is false to simulate pressing in the middle of the
     * region. */
    blender::int2 event_xy_offset;
    /** #wmEvent.type that triggered the operator. */
    int event_type;

    /** Initial distance to 'ofs'. */
    float zfac;

    /** Trackball rotation only. */
    float trackvec[3];
    /** Dolly only. */
    float mousevec[3];

    /** Used for roll */
    Dial *dial;
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

  const ViewOpsType *nav_type;
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

  void init_context(bContext *C);
  void state_backup();
  void state_restore();
  void init_navigation(bContext *C,
                       const wmEvent *event,
                       const ViewOpsType *nav_type,
                       const float dyn_ofs_override[3] = nullptr,
                       const bool use_cursor_init = false);
  void end_navigation(bContext *C);

  MEM_CXX_CLASS_ALLOC_FUNCS("ViewOpsData")
};

/* view3d_navigate.cc */

bool view3d_location_poll(bContext *C);
bool view3d_rotation_poll(bContext *C);
bool view3d_zoom_or_dolly_poll(bContext *C);
bool view3d_zoom_or_dolly_or_rotation_poll(bContext *C);

wmOperatorStatus view3d_navigate_invoke_impl(bContext *C,
                                             wmOperator *op,
                                             const wmEvent *event,
                                             const ViewOpsType *nav_type);
wmOperatorStatus view3d_navigate_modal_fn(bContext *C, wmOperator *op, const wmEvent *event);
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
                                 const ViewOpsType *nav_type,
                                 const bool use_cursor_init);
/**
 * \param align_to_quat: When not nullptr, set the axis relative to this rotation.
 */
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

extern ViewOpsType ViewOpsType_dolly;

/* view3d_navigate_fly.cc */

void fly_modal_keymap(wmKeyConfig *keyconf);
void view3d_keymap(wmKeyConfig *keyconf);
void VIEW3D_OT_fly(wmOperatorType *ot);

/* view3d_navigate_move.cc */

void viewmove_modal_keymap(wmKeyConfig *keyconf);
void VIEW3D_OT_move(wmOperatorType *ot);

extern const ViewOpsType ViewOpsType_move;

/* view3d_navigate_ndof.cc */

#ifdef WITH_INPUT_NDOF
struct wmNDOFMotionData;
/**
 * Called from both fly mode and walk mode,
 */
void view3d_ndof_fly(const wmNDOFMotionData &ndof,
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

extern const ViewOpsType ViewOpsType_ndof_orbit;
extern const ViewOpsType ViewOpsType_ndof_orbit_zoom;
extern const ViewOpsType ViewOpsType_ndof_pan;
extern const ViewOpsType ViewOpsType_ndof_all;
#endif /* WITH_INPUT_NDOF */

/* view3d_navigate_roll.cc */

void VIEW3D_OT_view_roll(wmOperatorType *ot);

extern const ViewOpsType ViewOpsType_roll;

/* view3d_navigate_rotate.cc */

void viewrotate_modal_keymap(wmKeyConfig *keyconf);
void VIEW3D_OT_rotate(wmOperatorType *ot);

extern const ViewOpsType ViewOpsType_rotate;

/* view3d_navigate_smoothview.cc */

/**
 * Parameters for setting the new 3D Viewport state.
 *
 * Each of the struct members may be NULL to signify they aren't to be adjusted.
 */
struct V3D_SmoothParams {
  Object *camera_old, *camera;
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

/**
 * A version of #ED_view3d_smooth_view_force_finish
 * that doesn't support camera locking or auto-keying.
 * Use for viewport actions that don't control the camera,
 * entering/exiting the local-view for example (see code-comments for details).
 */
void ED_view3d_smooth_view_force_finish_no_camera_lock(const Depsgraph *depsgraph,
                                                       wmWindowManager *wm,
                                                       wmWindow *win,
                                                       const Scene *scene,
                                                       View3D *v3d,
                                                       ARegion *region);

void VIEW3D_OT_smoothview(wmOperatorType *ot);

/* view3d_navigate_view_all.cc */

/**
 * Return the bounds of visible contents of the 3D viewport.
 *
 * \param depsgraph: The evaluated depsgraph.
 * \param clip_bounds: Clip the bounds by the viewport clipping.
 */
std::optional<blender::Bounds<blender::float3>> view3d_calc_minmax_visible(
    Depsgraph *depsgraph, ScrArea *area, ARegion *region, bool use_all_regions, bool clip_bounds);
/**
 * Return the bounds of selected contents of the 3D viewport.
 * \param depsgraph: The evaluated depsgraph.
 * \param clip_bounds: Clip the bounds by the viewport clipping.
 * \param r_do_zoom: When false, the bounds should be treated as a point
 * (don't zoom to view the point).
 */
std::optional<blender::Bounds<blender::float3>> view3d_calc_minmax_selected(Depsgraph *depsgraph,
                                                                            ScrArea *area,
                                                                            ARegion *region,
                                                                            bool use_all_regions,
                                                                            bool clip_bounds,
                                                                            bool *r_do_zoom);

/**
 * Iterate over objects and check if `point` might is inside any of them.
 */
bool view3d_calc_point_in_selected_bounds(Depsgraph *depsgraph,
                                          struct ViewLayer *view_layer_eval,
                                          const View3D *v3d,
                                          const blender::float3 &point,
                                          const float scale_margin);

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

extern const ViewOpsType ViewOpsType_orbit;

/* view3d_navigate_view_pan.cc */

void VIEW3D_OT_view_pan(wmOperatorType *ot);

extern const ViewOpsType ViewOpsType_pan;

/* view3d_navigate_walk.cc */

void walk_modal_keymap(wmKeyConfig *keyconf);
void VIEW3D_OT_walk(wmOperatorType *ot);

/* view3d_navigate_zoom.cc */

void viewzoom_modal_keymap(wmKeyConfig *keyconf);
void VIEW3D_OT_zoom(wmOperatorType *ot);

extern const ViewOpsType ViewOpsType_zoom;

/* view3d_navigate_zoom_border.cc */

void VIEW3D_OT_zoom_border(wmOperatorType *ot);
