/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_bounds_types.hh"
#include "BLI_enum_flags.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_scene_types.h"

/* ********* exports for space_view3d/ module ********** */
struct ARegion;
struct BMEdge;
struct BMElem;
struct BMEditMesh;
struct BMFace;
struct BMVert;
struct BPoint;
struct Base;
struct BezTriple;
struct BoundBox;
struct Camera;
struct CustomData_MeshMasks;
struct Depsgraph;
struct EditBone;
struct GPUSelectBuffer;
struct ID;
struct Main;
struct MetaElem;
struct Nurb;
struct Object;
struct RV3DMatrixStore;
struct RegionView3D;
struct RenderEngineType;
struct Scene;
struct ScrArea;
struct View3D;
struct ViewContext;
struct ViewLayer;
struct ViewOpsData;
struct bContext;
struct bGPDlayer;
struct bPoseChannel;
struct bScreen;
struct rctf;
struct rcti;
struct wmEvent;
struct wmGizmo;
struct wmKeyMapItem;
struct wmOperator;
struct wmWindow;
struct wmWindowManager;
namespace blender::ed::transform {
struct SnapObjectContext;
}

/** For mesh drawing callbacks, for viewport selection, etc. */
struct ViewContext {
  bContext *C;
  Main *bmain;
  /* Dependency graph is uses for depth drawing, viewport camera matrix access, and also some areas
   * are re-using this to access evaluated entities.
   *
   * Moral of the story: assign to a fully evaluated state. */
  Depsgraph *depsgraph;
  Scene *scene;
  ViewLayer *view_layer;
  Object *obact;
  Object *obedit;
  ARegion *region;
  View3D *v3d;
  wmWindow *win;
  RegionView3D *rv3d;
  BMEditMesh *em;
  int mval[2];
};

struct ViewDepths {
  unsigned short w, h;
  /* only for temp use for sub-rectangles, added to `region->winx/winy`. */
  short x, y;
  float *depths;
  double depth_range[2];
};

/* Rotate 3D cursor on placement. */
enum eV3DCursorOrient {
  V3D_CURSOR_ORIENT_NONE = 0,
  V3D_CURSOR_ORIENT_VIEW,
  V3D_CURSOR_ORIENT_XFORM,
  V3D_CURSOR_ORIENT_GEOM,
};

void ED_view3d_background_color_get(const Scene *scene, const View3D *v3d, float r_color[3]);
void ED_view3d_text_colors_get(const Scene *scene,
                               const View3D *v3d,
                               float r_text_color[4],
                               float r_shadow_color[4]);
bool ED_view3d_has_workbench_in_texture_color(const Scene *scene,
                                              const Object *ob,
                                              const View3D *v3d);
/**
 * Cursor position in `r_cursor_co`, result in `r_cursor_co`, `mval` in region coords.
 *
 * \note cannot use `event->mval` here, called by #object_add().
 */
void ED_view3d_cursor3d_position(bContext *C,
                                 const int mval[2],
                                 bool use_depth,
                                 float r_cursor_co[3]);
void ED_view3d_cursor3d_position_rotation(bContext *C,
                                          const int mval[2],
                                          bool use_depth,
                                          enum eV3DCursorOrient orientation,
                                          float r_cursor_co[3],
                                          float r_cursor_quat[4]);
void ED_view3d_cursor3d_update(bContext *C,
                               const int mval[2],
                               bool use_depth,
                               enum eV3DCursorOrient orientation);

Camera *ED_view3d_camera_data_get(View3D *v3d, RegionView3D *rv3d);

/**
 * Calculate the view transformation matrix from RegionView3D input.
 * The resulting matrix is equivalent to #RegionView3D.viewinv
 * \param mat: The view 4x4 transformation matrix to calculate.
 * \param ofs: The view offset, normally from #RegionView3D.ofs.
 * \param quat: The view rotation, quaternion normally from #RegionView3D.viewquat.
 * \param dist: The view distance from ofs, normally from #RegionView3D.dist.
 */
void ED_view3d_to_m4(float mat[4][4], const float ofs[3], const float quat[4], float dist);
/**
 * Set the view transformation from a 4x4 matrix.
 *
 * \param mat: The view 4x4 transformation matrix to assign.
 * \param ofs: The view offset, normally from #RegionView3D.ofs.
 * \param quat: The view rotation, quaternion normally from #RegionView3D.viewquat.
 * \param dist: The view distance from `ofs`, normally from #RegionView3D.dist.
 */
void ED_view3d_from_m4(const float mat[4][4], float ofs[3], float quat[4], const float *dist);

/**
 * Set the #RegionView3D members from an objects transformation and optionally lens.
 * \param ob: The object to set the view to.
 * \param ofs: The view offset to be set, normally from #RegionView3D.ofs.
 * \param quat: The view rotation to be set, quaternion normally from #RegionView3D.viewquat.
 * \param dist: The view distance from `ofs `to be set, normally from #RegionView3D.dist.
 * \param lens: The view lens angle set for cameras and lights, normally from View3D.lens.
 */
void ED_view3d_from_object(
    const Object *ob, float ofs[3], float quat[4], const float *dist, float *lens);
/**
 * Set the object transformation from #RegionView3D members.
 * \param depsgraph: The depsgraph to get the evaluated object parent
 * for the transformation calculation.
 * \param ob: The object which has the transformation assigned.
 * \param ofs: The view offset, normally from #RegionView3D.ofs.
 * \param quat: The view rotation, quaternion normally from #RegionView3D.viewquat.
 * \param dist: The view distance from `ofs`, normally from #RegionView3D.dist.
 */
void ED_view3d_to_object(
    const Depsgraph *depsgraph, Object *ob, const float ofs[3], const float quat[4], float dist);

bool ED_view3d_camera_to_view_selected(Main *bmain,
                                       Depsgraph *depsgraph,
                                       const Scene *scene,
                                       Object *camera_ob);

bool ED_view3d_camera_to_view_selected_with_set_clipping(Main *bmain,
                                                         Depsgraph *depsgraph,
                                                         const Scene *scene,
                                                         Object *camera_ob);

/**
 * Use to store the last view, before entering camera view.
 */
void ED_view3d_lastview_store(RegionView3D *rv3d);

/* Depth buffer */
enum eV3DDepthOverrideMode {
  /** Redraw viewport with all objects. */
  V3D_DEPTH_ALL = 0,
  /** Redraw viewport without Grease Pencil. */
  V3D_DEPTH_NO_GPENCIL,
  /** Redraw viewport with Grease Pencil only. */
  V3D_DEPTH_GPENCIL_ONLY,
  /** Redraw viewport with active object only. */
  V3D_DEPTH_OBJECT_ONLY,
  /** Redraw viewport with objects from the supplied collection only. */
  V3D_DEPTH_SELECTED_ONLY,

};
/**
 * Redraw the viewport depth buffer.
 * Call #ED_view3d_has_depth_buffer_updated if you want to check if the viewport already has depth
 * buffer updated.
 *
 * \param use_overlay: When enabled and the `v3d` has overlays enabled, show overlays.
 * A rule of thumb for this value is:
 * - For viewport navigation the value should be true.
 *   Since the user may want to inspect non-geometry contents of their scene.
 * - For painting and other tools, the value should be false.
 *   Since it's not typically desirable to paint onto the cameras frame or spot-light,
 *   nor use these depths for object placement.
 */
void ED_view3d_depth_override(Depsgraph *depsgraph,
                              ARegion *region,
                              View3D *v3d,
                              Object *obact,
                              eV3DDepthOverrideMode mode,
                              bool use_overlay,
                              ViewDepths **r_depths);
void ED_view3d_depths_free(ViewDepths *depths);
bool ED_view3d_depth_read_cached(const ViewDepths *vd,
                                 const int mval[2],
                                 int margin,
                                 float *r_depth);
bool ED_view3d_depth_read_cached_normal(const ARegion *region,
                                        const ViewDepths *depths,
                                        const int mval[2],
                                        float r_normal[3]);
bool ED_view3d_depth_unproject_v3(const ARegion *region,
                                  const int mval[2],
                                  double depth,
                                  float r_location_world[3]);

bool ED_view3d_has_depth_buffer_updated(const Depsgraph *depsgraph, const View3D *v3d);

/**
 * Utilities to perform navigation.
 * Call #ED_view3d_navigation_init to create a context and #ED_view3d_navigation_do to perform
 * navigation in modal operators.
 *
 * \note modal map events can also be used in #ED_view3d_navigation_do.
 */
ViewOpsData *ED_view3d_navigation_init(bContext *C, const wmKeyMapItem *kmi_merge);
bool ED_view3d_navigation_do(bContext *C,
                             ViewOpsData *vod,
                             const wmEvent *event,
                             const float depth_loc_override[3]);
void ED_view3d_navigation_free(bContext *C, ViewOpsData *vod);

/* Projection */
#define IS_CLIPPED 12000

/* return values for ED_view3d_project_...() */
enum eV3DProjStatus {
  V3D_PROJ_RET_OK = 0,
  /** Can't avoid this when in perspective mode, (can't avoid) */
  V3D_PROJ_RET_CLIP_NEAR = 1,
  /** After clip_end. */
  V3D_PROJ_RET_CLIP_FAR = 2,
  /**
   * Set when the coordinate is so close to the view-point that the projection isn't usable.
   * Where there is potential numeric error in the resulting 2D value.
   * This can be used to numeric errors even in cases where the caller* wishes to ignore
   * the near clipping plane.
   */
  V3D_PROJ_RET_CLIP_ZERO = 3,
  /** Bounding box clip - RV3D_CLIPPING */
  V3D_PROJ_RET_CLIP_BB = 4,
  /** Outside window bounds. */
  V3D_PROJ_RET_CLIP_WIN = 5,
  /** Outside range (mainly for short), (can't avoid) */
  V3D_PROJ_RET_OVERFLOW = 6,
};

/* some clipping tests are optional */
enum eV3DProjTest {
  V3D_PROJ_TEST_NOP = 0,
  V3D_PROJ_TEST_CLIP_BB = (1 << 0),
  V3D_PROJ_TEST_CLIP_WIN = (1 << 1),
  V3D_PROJ_TEST_CLIP_NEAR = (1 << 2),
  V3D_PROJ_TEST_CLIP_FAR = (1 << 3),
  V3D_PROJ_TEST_CLIP_ZERO = (1 << 4),
  /**
   * Clip the contents of the data being iterated over.
   * Currently this is only used to edges when projecting into screen space.
   *
   * Clamp the edge within the viewport limits defined by
   * #V3D_PROJ_TEST_CLIP_WIN, #V3D_PROJ_TEST_CLIP_NEAR & #V3D_PROJ_TEST_CLIP_FAR.
   * This resolves the problem of a visible edge having one of it's vertices
   * behind the viewport. See: #32214.
   *
   * This is not default behavior as it may be important for the screen-space location
   * of an edges vertex to represent that vertices location (instead of a location along the edge).
   *
   * \note Perspective views should enable #V3D_PROJ_TEST_CLIP_WIN along with
   * #V3D_PROJ_TEST_CLIP_NEAR as the near-plane-clipped location of a point
   * may become very large (even infinite) when projected into screen-space.
   * Unless that point happens to coincide with the camera's point of view.
   *
   * Use #V3D_PROJ_TEST_CLIP_CONTENT_DEFAULT instead of #V3D_PROJ_TEST_CLIP_CONTENT,
   * to avoid accidentally enabling near clipping without clipping by window bounds.
   */
  V3D_PROJ_TEST_CLIP_CONTENT = (1 << 5),
};
ENUM_OPERATORS(eV3DProjTest);

#define V3D_PROJ_TEST_CLIP_DEFAULT \
  (V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_WIN | V3D_PROJ_TEST_CLIP_NEAR)
#define V3D_PROJ_TEST_ALL \
  (V3D_PROJ_TEST_CLIP_DEFAULT | V3D_PROJ_TEST_CLIP_FAR | V3D_PROJ_TEST_CLIP_ZERO | \
   V3D_PROJ_TEST_CLIP_CONTENT)

#define V3D_PROJ_TEST_CLIP_CONTENT_DEFAULT \
  (V3D_PROJ_TEST_CLIP_CONTENT | V3D_PROJ_TEST_CLIP_NEAR | V3D_PROJ_TEST_CLIP_FAR | \
   V3D_PROJ_TEST_CLIP_WIN)

/* `view3d_snap.cc` */

bool ED_view3d_snap_selected_to_location(bContext *C,
                                         wmOperator *op,
                                         const float target_loc_global[3],
                                         int pivot_point);

/* `view3d_cursor_snap.cc` */

#define USE_SNAP_DETECT_FROM_KEYMAP_HACK
enum eV3DSnapCursor {
  V3D_SNAPCURSOR_TOGGLE_ALWAYS_TRUE = 1 << 0,
  V3D_SNAPCURSOR_OCCLUSION_ALWAYS_TRUE = 1 << 1,
  V3D_SNAPCURSOR_OCCLUSION_ALWAYS_FALSE = 1 << 2, /* TODO. */
  V3D_SNAPCURSOR_SNAP_EDIT_GEOM_FINAL = 1 << 3,
  V3D_SNAPCURSOR_SNAP_EDIT_GEOM_CAGE = 1 << 4,
};
ENUM_OPERATORS(eV3DSnapCursor)

struct V3DSnapCursorData {
  eSnapMode type_source;
  eSnapMode type_target;
  float loc[3];
  float nor[3];
  float obmat[4][4];
  int elem_index[3];
  float plane_omat[3][3];
  bool is_snap_invert;

  /** Enabled when snap is activated, even if it didn't find anything. */
  bool is_enabled;
};

struct V3DSnapCursorState {
  /* Setup. */
  eV3DSnapCursor flag;
  uchar source_color[4];
  uchar target_color[4];
  uchar color_box[4];
  float *prevpoint;
  float box_dimensions[3];
  bool draw_point;
  bool draw_plane;
  bool draw_box;

  bool (*poll)(ARegion *region, void *custom_poll_data);
  void *poll_data;
};

void ED_view3d_cursor_snap_state_default_set(V3DSnapCursorState *state);
V3DSnapCursorState *ED_view3d_cursor_snap_state_active_get();
void ED_view3d_cursor_snap_state_active_set(V3DSnapCursorState *state);
V3DSnapCursorState *ED_view3d_cursor_snap_state_create();
void ED_view3d_cursor_snap_state_free(V3DSnapCursorState *state);
void ED_view3d_cursor_snap_state_prevpoint_set(V3DSnapCursorState *state,
                                               const float prev_point[3]);
void ED_view3d_cursor_snap_data_update(V3DSnapCursorState *state,
                                       const bContext *C,
                                       const ARegion *region,
                                       const blender::int2 &mval);
V3DSnapCursorData *ED_view3d_cursor_snap_data_get();
blender::ed::transform::SnapObjectContext *ED_view3d_cursor_snap_context_ensure(Scene *scene);
void ED_view3d_cursor_snap_draw_util(RegionView3D *rv3d,
                                     const float source_loc[3],
                                     const float target_loc[3],
                                     const eSnapMode source_type,
                                     const eSnapMode target_type,
                                     const uchar source_color[4],
                                     const uchar target_color[4]);

/* `view3d_iterators.cc` */

/* foreach iterators */

void meshobject_foreachScreenVert(const ViewContext *vc,
                                  void (*func)(void *user_data,
                                               const float screen_co[2],
                                               int index),
                                  void *user_data,
                                  eV3DProjTest clip_flag);
void mesh_foreachScreenVert(
    const ViewContext *vc,
    void (*func)(void *user_data, BMVert *eve, const float screen_co[2], int index),
    void *user_data,
    eV3DProjTest clip_flag);
void mesh_foreachScreenEdge(const ViewContext *vc,
                            void (*func)(void *user_data,
                                         BMEdge *eed,
                                         const float screen_co_a[2],
                                         const float screen_co_b[2],
                                         int index),
                            void *user_data,
                            eV3DProjTest clip_flag);

/**
 * A version of #mesh_foreachScreenEdge that clips the segment when
 * there is a clipping bounding box.
 */
void mesh_foreachScreenEdge_clip_bb_segment(const ViewContext *vc,
                                            void (*func)(void *user_data,
                                                         BMEdge *eed,
                                                         const float screen_co_a[2],
                                                         const float screen_co_b[2],
                                                         int index),
                                            void *user_data,
                                            eV3DProjTest clip_flag);

void mesh_foreachScreenFace(
    const ViewContext *vc,
    void (*func)(void *user_data, BMFace *efa, const float screen_co[2], int index),
    void *user_data,
    eV3DProjTest clip_flag);
void nurbs_foreachScreenVert(const ViewContext *vc,
                             void (*func)(void *user_data,
                                          Nurb *nu,
                                          BPoint *bp,
                                          BezTriple *bezt,
                                          int beztindex,
                                          bool handle_visible,
                                          const float screen_co[2]),
                             void *user_data,
                             eV3DProjTest clip_flag);
/**
 * #ED_view3d_init_mats_rv3d must be called first.
 */
void mball_foreachScreenElem(const ViewContext *vc,
                             void (*func)(void *user_data, MetaElem *ml, const float screen_co[2]),
                             void *user_data,
                             eV3DProjTest clip_flag);
void lattice_foreachScreenVert(const ViewContext *vc,
                               void (*func)(void *user_data, BPoint *bp, const float screen_co[2]),
                               void *user_data,
                               eV3DProjTest clip_flag);
/**
 * #ED_view3d_init_mats_rv3d must be called first.
 */
void armature_foreachScreenBone(const ViewContext *vc,
                                void (*func)(void *user_data,
                                             EditBone *ebone,
                                             const float screen_co_a[2],
                                             const float screen_co_b[2]),
                                void *user_data,
                                eV3DProjTest clip_flag);

/**
 * ED_view3d_init_mats_rv3d must be called first.
 */
void pose_foreachScreenBone(const ViewContext *vc,
                            void (*func)(void *user_data,
                                         bPoseChannel *pchan,
                                         const float screen_co_a[2],
                                         const float screen_co_b[2]),
                            void *user_data,
                            eV3DProjTest clip_flag);
/* *** end iterators *** */

/* `view3d_project.cc` */

/**
 * \note use #ED_view3d_ob_project_mat_get to get the projection matrix
 */
blender::float2 ED_view3d_project_float_v2_m4(const ARegion *region,
                                              const float co[3],
                                              const blender::float4x4 &mat);
/**
 * \note use #ED_view3d_ob_project_mat_get to get projecting mat
 */
void ED_view3d_project_float_v3_m4(const ARegion *region,
                                   const float co[3],
                                   float r_co[3],
                                   const float mat[4][4]);

eV3DProjStatus ED_view3d_project_base(const ARegion *region, Base *base, float r_co[2]);

/* *** short *** */
eV3DProjStatus ED_view3d_project_short_ex(const ARegion *region,
                                          float perspmat[4][4],
                                          bool is_local,
                                          const float co[3],
                                          short r_co[2],
                                          eV3DProjTest flag);
/* --- short --- */
eV3DProjStatus ED_view3d_project_short_global(const ARegion *region,
                                              const float co[3],
                                              short r_co[2],
                                              eV3DProjTest flag);
/* object space, use ED_view3d_init_mats_rv3d before calling */
eV3DProjStatus ED_view3d_project_short_object(const ARegion *region,
                                              const float co[3],
                                              short r_co[2],
                                              eV3DProjTest flag);

/* *** int *** */
eV3DProjStatus ED_view3d_project_int_ex(const ARegion *region,
                                        float perspmat[4][4],
                                        bool is_local,
                                        const float co[3],
                                        int r_co[2],
                                        eV3DProjTest flag);
/* --- int --- */
eV3DProjStatus ED_view3d_project_int_global(const ARegion *region,
                                            const float co[3],
                                            int r_co[2],
                                            eV3DProjTest flag);
/* object space, use ED_view3d_init_mats_rv3d before calling */
eV3DProjStatus ED_view3d_project_int_object(const ARegion *region,
                                            const float co[3],
                                            int r_co[2],
                                            eV3DProjTest flag);

/* *** float *** */
eV3DProjStatus ED_view3d_project_float_ex(const ARegion *region,
                                          float perspmat[4][4],
                                          bool is_local,
                                          const float co[3],
                                          float r_co[2],
                                          eV3DProjTest flag);
/* --- float --- */
eV3DProjStatus ED_view3d_project_float_global(const ARegion *region,
                                              const float co[3],
                                              float r_co[2],
                                              eV3DProjTest flag);
/**
 * Object space, use #ED_view3d_init_mats_rv3d before calling.
 */
eV3DProjStatus ED_view3d_project_float_object(const ARegion *region,
                                              const float co[3],
                                              float r_co[2],
                                              eV3DProjTest flag);

float ED_view3d_pixel_size(const RegionView3D *rv3d, const float co[3]);
float ED_view3d_pixel_size_no_ui_scale(const RegionView3D *rv3d, const float co[3]);

/**
 * Calculate a depth value from \a co, use with #ED_view3d_win_to_delta.
 *
 * \param r_flip: Set to `zfac < 0.0` before the value is made signed.
 * Since it's important in some cases to know if the value was flipped.
 *
 * \return The unsigned depth component of `co` multiplied by `rv3d->persmat` matrix,
 * with additional sanitation to ensure the result is never negative
 * as this isn't useful for tool-code.
 */
float ED_view3d_calc_zfac_ex(const RegionView3D *rv3d, const float co[3], bool *r_flip);
/** See #ED_view3d_calc_zfac_ex doc-string. */
float ED_view3d_calc_zfac(const RegionView3D *rv3d, const float co[3]);
/**
 * Calculate a depth value from `co` (result should only be used for comparison).
 */
float ED_view3d_calc_depth_for_comparison(const RegionView3D *rv3d, const float co[3]);

bool ED_view3d_clip_segment(const RegionView3D *rv3d, float ray_start[3], float ray_end[3]);
/**
 * Calculate a 3D viewpoint and direction vector from 2D window coordinates.
 * This ray_start is located at the viewpoint, `ray_normal` is the direction towards mval.
 * ray_start is clipped by the view near limit so points in front of it are always in view.
 * In orthographic view the resulting `ray_normal` will match the view vector.
 * \param region: The region (used for the window width and height).
 * \param v3d: The 3D viewport (used for near clipping value).
 * \param mval: The area relative 2D location (such as event->mval, converted into float[2]).
 * \param r_ray_start: The world-space point where the ray intersects the window plane.
 * \param r_ray_normal: The normalized world-space direction of towards mval.
 * \param do_clip_planes: Optionally clip the start of the ray by the view clipping planes.
 * \return success, false if the ray is totally clipped.
 */
bool ED_view3d_win_to_ray_clipped(Depsgraph *depsgraph,
                                  const ARegion *region,
                                  const View3D *v3d,
                                  const float mval[2],
                                  float r_ray_start[3],
                                  float r_ray_normal[3],
                                  bool do_clip_planes);
/**
 * Calculate a 3D viewpoint and direction vector from 2D window coordinates.
 * This `ray_start` is located at the viewpoint, `ray_normal` is the direction towards `mval`.
 * ray_start is clipped by the view near limit so points in front of it are always in view.
 * In orthographic view the resulting `ray_normal` will match the view vector.
 * This version also returns the ray_co point of the ray on window plane, useful to fix precision
 * issues especially with orthographic view, where default ray_start is set rather far away.
 * \param region: The region (used for the window width and height).
 * \param v3d: The 3D viewport (used for near clipping value).
 * \param mval: The area relative 2D location (such as `event->mval`, converted into float[2]).
 * \param do_clip_planes: Optionally clip the start of the ray by the view clipping planes.
 * \param r_ray_co: The world-space point where the ray intersects the window plane.
 * \param r_ray_normal: The normalized world-space direction of towards mval.
 * \param r_ray_start: The world-space starting point of the ray.
 * \param r_ray_end: The world-space end point of the segment.
 * \return success, false if the ray is totally clipped.
 */
bool ED_view3d_win_to_ray_clipped_ex(Depsgraph *depsgraph,
                                     const ARegion *region,
                                     const View3D *v3d,
                                     const float mval[2],
                                     const bool do_clip_planes,
                                     float r_ray_co[3],
                                     float r_ray_normal[3],
                                     float r_ray_start[3],
                                     float r_ray_end[3]);
/**
 * Calculate a 3D viewpoint and direction vector from 2D window coordinates.
 * This ray_start is located at the viewpoint, `ray_normal` is the direction towards `mval`.
 * \param region: The region (used for the window width and height).
 * \param mval: The area relative 2D location (such as `event->mval`, converted into float[2]).
 * \param r_ray_start: The world-space point where the ray intersects the window plane.
 * \param r_ray_normal: The normalized world-space direction of towards mval.
 *
 * \note Ignores view near/far clipping,
 * to take this into account use #ED_view3d_win_to_ray_clipped.
 */
void ED_view3d_win_to_ray(const ARegion *region,
                          const float mval[2],
                          float r_ray_start[3],
                          float r_ray_normal[3]);
/**
 * Calculate a normalized 3D direction vector from the viewpoint towards a global location.
 * In orthographic view the resulting vector will match the view vector.
 * \param rv3d: The region (used for the window width and height).
 * \param coord: The world-space location.
 * \param r_out: The resulting normalized vector.
 */
void ED_view3d_global_to_vector(const RegionView3D *rv3d, const float coord[3], float r_out[3]);
/**
 * Calculate a 3D location from 2D window coordinates.
 * \param region: The region (used for the window width and height).
 * \param depth_pt: The reference location used to calculate the Z depth.
 * \param mval: The area relative location (such as `event->mval` converted to floats).
 * \param r_out: The resulting world-space location.
 */
void ED_view3d_win_to_3d(const View3D *v3d,
                         const ARegion *region,
                         const float depth_pt[3],
                         const float mval[2],
                         float r_out[3]);
void ED_view3d_win_to_3d_int(const View3D *v3d,
                             const ARegion *region,
                             const float depth_pt[3],
                             const int mval[2],
                             float r_out[3]);
/**
 * Calculate a 3D location from 2D window coordinates including camera shift.
 *
 * \note Does the same as #ED_view3d_win_to_3d by using the #RegionView3D::persinv translation
 * instead of #RegionView3D::viewinv, but that function cannot be changed
 * without breaking lots of operators.
 *
 * \param region: The region (used for the window width and height).
 * \param depth_pt: The reference location used to calculate the Z depth.
 * \param mval: The area relative location (such as `event->mval` converted to floats).
 * \param r_out: The resulting world-space location.
 */
void ED_view3d_win_to_3d_with_shift(const View3D *v3d,
                                    const ARegion *region,
                                    const float depth_pt[3],
                                    const float mval[2],
                                    float r_out[3]);
bool ED_view3d_win_to_3d_on_plane(const ARegion *region,
                                  const float plane[4],
                                  const float mval[2],
                                  bool do_clip,
                                  float r_out[3]);
/**
 * A wrapper for #ED_view3d_win_to_3d_on_plane that projects onto \a plane_fallback
 * then maps this back to \a plane.
 *
 * This is intended to be used when \a plane is orthogonal to the views Z axis where
 * projecting the \a mval doesn't work well (or fail completely when exactly aligned).
 */
bool ED_view3d_win_to_3d_on_plane_with_fallback(const ARegion *region,
                                                const float plane[4],
                                                const float mval[2],
                                                bool do_clip,
                                                const float plane_fallback[4],
                                                float r_out[3]);
bool ED_view3d_win_to_3d_on_plane_int(
    const ARegion *region, const float plane[4], const int mval[2], bool do_clip, float r_out[3]);
/**
 * Calculate a 3D difference vector from 2D window offset.
 *
 * \note that #ED_view3d_calc_zfac() must be called first to determine
 * the depth used to calculate the delta.
 *
 * When the `zfac` is calculated based on a world-space location directly under the cursor,
 * the value of `r_out` can be subtracted from #RegionView3D.ofs to pan the view
 * with the contents following the cursor perfectly (without sliding).
 *
 * \param region: The region (used for the window width and height).
 * \param xy_delta: 2D difference (in pixels) such as `event->mval[0] - other_x`.
 * \param zfac: The depth result typically calculated by #ED_view3d_calc_zfac
 * (see its doc-string for details).
 * \param r_out: The resulting world-space delta.
 * \param precise: Use a more precise calculation but increases the cost of this function.
 */
void ED_view3d_win_to_delta(const ARegion *region,
                            const float xy_delta[2],
                            float zfac,
                            float r_out[3],
                            bool precise = false);
/**
 * Calculate a 3D origin from 2D window coordinates.
 * \note Orthographic views have a less obvious origin,
 * Since far clip can be a very large value resulting in numeric precision issues,
 * the origin in this case is close to zero coordinate.
 *
 * \param region: The region (used for the window width and height).
 * \param mval: The area relative 2D location (such as `event->mval` converted to float).
 * \param r_out: The resulting normalized world-space direction vector.
 */
void ED_view3d_win_to_origin(const ARegion *region, const float mval[2], float r_out[3]);
/**
 * Calculate a 3D direction vector from 2D window coordinates.
 * The resulting direction points away from the view-point,
 * making the result useful to perform ray-casts into a 3D scene.
 * In orthographic view all input coordinates result in the same vector.
 * \note doesn't rely on #ED_view3d_calc_zfac
 * for perspective view, get the vector direction to
 * the mouse cursor as a normalized vector.
 *
 * \param region: The region (used for the window width and height).
 * \param mval: The area relative 2D location (such as `event->mval` converted to float).
 * \param r_out: The resulting normalized world-space direction vector.
 */
void ED_view3d_win_to_vector(const ARegion *region, const float mval[2], float r_out[3]);
/**
 * Calculate a 3D segment from 2D window coordinates.
 * This ray_start is located at the viewpoint, ray_end is a far point.
 * ray_start and ray_end are clipped by the view near and far limits
 * so points along this line are always in view.
 * In orthographic view all resulting segments will be parallel.
 * \param region: The region (used for the window width and height).
 * \param v3d: The 3D viewport (used for near and far clipping range).
 * \param mval: The area relative 2D location (such as event->mval, converted into float[2]).
 * \param r_ray_start: The world-space starting point of the segment.
 * \param r_ray_end: The world-space end point of the segment.
 * \param do_clip_planes: Optionally clip the ray by the view clipping planes.
 * \return success, false if the segment is totally clipped.
 */
bool ED_view3d_win_to_segment_clipped(const Depsgraph *depsgraph,
                                      const ARegion *region,
                                      const View3D *v3d,
                                      const float mval[2],
                                      float r_ray_start[3],
                                      float r_ray_end[3],
                                      bool do_clip_planes);
blender::float4x4 ED_view3d_ob_project_mat_get(const RegionView3D *rv3d, const Object *ob);
blender::float4x4 ED_view3d_ob_project_mat_get_from_obmat(const RegionView3D *rv3d,
                                                          const blender::float4x4 &obmat);

/**
 * Convert between region relative coordinates (x,y) and depth component z and
 * a point in world space.
 */
void ED_view3d_project_v3(const ARegion *region, const float world[3], float r_region_co[3]);
void ED_view3d_project_v2(const ARegion *region, const float world[3], float r_region_co[2]);
bool ED_view3d_unproject_v3(
    const ARegion *region, float regionx, float regiony, float regionz, float world[3]);

/* end */

/**
 * Calculate a "soft" working range for #RegionView3D::dist.
 *
 * This is an approximate range to avoid extreme values being set where nothing is visible.
 *
 * - A small `dist` may be below near-clipping plane causing nothing to be visible.
 *   It can also take a while to zoom out.
 * - A large `dist` may be so big that the viewports contents is beyond the far-clipping plane
 *   also causing nothing to be visible.
 *
 * The range is calculated based on values the user may change so the range
 * should be used as guidance for operators to follow.
 *
 * \param use_persp_range: Use an alternative range for perspective views.
 * It's not a requirement that perspective views use this, however in practice
 * it's often preferable for perspective views to calculate the minimum based on near-clipping,
 * unlike orthographic views.
 */
blender::Bounds<float> ED_view3d_dist_soft_range_get(const View3D *v3d, bool use_persp_range);

/**
 * A version of #ED_view3d_dist_soft_range_get that only returns the minimum.
 *
 * For perspective-views where setting `dist` near or below the near clip-plane
 * is likely to cause the viewport content to be clipped out of the view.
 *
 * \note While clamping by the far clip-plane is done in some cases
 * the exact value to use is more arbitrary, in practice users are less
 * likely to encounter problems from being zoomed out too far.
 */
float ED_view3d_dist_soft_min_get(const View3D *v3d, bool use_persp_range);

/**
 * \note copies logic of #ED_view3d_viewplane_get(), keep in sync.
 */
bool ED_view3d_clip_range_get(const Depsgraph *depsgraph,
                              const View3D *v3d,
                              const RegionView3D *rv3d,
                              bool use_ortho_factor,
                              float *r_clip_start,
                              float *r_clip_end);
bool ED_view3d_viewplane_get(const Depsgraph *depsgraph,
                             const View3D *v3d,
                             const RegionView3D *rv3d,
                             int winx,
                             int winy,
                             rctf *r_viewplane,
                             float *r_clip_start,
                             float *r_clip_end,
                             float *r_pixsize);

/**
 * Use instead of: `GPU_polygon_offset(rv3d->dist, ...)` see bug #37727.
 */
void ED_view3d_polygon_offset(const RegionView3D *rv3d, float dist);

void ED_view3d_calc_camera_border(const Scene *scene,
                                  const Depsgraph *depsgraph,
                                  const ARegion *region,
                                  const View3D *v3d,
                                  const RegionView3D *rv3d,
                                  bool no_shift,
                                  rctf *r_viewborder);
void ED_view3d_calc_camera_border_size(const Scene *scene,
                                       Depsgraph *depsgraph,
                                       const ARegion *region,
                                       const View3D *v3d,
                                       const RegionView3D *rv3d,
                                       float r_size[2]);
bool ED_view3d_calc_render_border(
    const Scene *scene, Depsgraph *depsgraph, View3D *v3d, ARegion *region, rcti *r_rect);

void ED_view3d_clipping_calc_from_boundbox(float clip[4][4], const BoundBox *bb, bool is_flip);
void ED_view3d_clipping_calc(
    BoundBox *bb, float planes[4][4], const ARegion *region, const Object *ob, const rcti *rect);
/**
 * Clamp min/max by the viewport clipping.
 *
 * \note This is an approximation, with the limitation that the bounding box from the (mix, max)
 * calculation might not have any geometry inside the clipped region.
 * Performing a clipping test on each vertex would work well enough for most cases,
 * although it's not perfect either as edges/faces may intersect the clipping without having any
 * of their vertices inside it.
 * A more accurate result would be quite involved.
 *
 * \return True when the arguments were clamped.
 */
bool ED_view3d_clipping_clamp_minmax(const RegionView3D *rv3d, float min[3], float max[3]);

void ED_view3d_clipping_local(RegionView3D *rv3d, const float mat[4][4]);
/**
 * Return true when `co` is hidden by the 3D views clipping planes.
 *
 * \param is_local: When true use local (object-space) #ED_view3d_clipping_local must run first,
 * then all comparisons can be done in local-space.
 * \return True when `co` is outside all clipping planes.
 *
 * \note Callers should check #RV3D_CLIPPING_ENABLED first.
 */
bool ED_view3d_clipping_test(const RegionView3D *rv3d, const float co[3], bool is_local);

float ED_view3d_radius_to_dist_persp(float angle, float radius);
float ED_view3d_radius_to_dist_ortho(float lens, float radius);
/**
 * Return a new #RegionView3D.dist value to fit the \a radius.
 *
 * \note Depth isn't taken into account, this will fit a flat plane exactly,
 * but points towards the view (with a perspective projection),
 * may be within the radius but outside the view. eg:
 *
 * <pre>
 *           +
 * pt --> + /^ radius
 *         / |
 *        /  |
 * view  +   +
 *        \  |
 *         \ |
 *          \|
 *           +
 * </pre>
 *
 * \param region: Can be NULL if \a use_aspect is false.
 * \param persp: Allow the caller to tell what kind of perspective to use (ortho/view/camera)
 * \param use_aspect: Increase the distance to account for non 1:1 view aspect.
 * \param radius: The radius will be fitted exactly,
 * typically pre-scaled by a margin (#VIEW3D_MARGIN).
 */
float ED_view3d_radius_to_dist(const View3D *v3d,
                               const ARegion *region,
                               const Depsgraph *depsgraph,
                               char persp,
                               bool use_aspect,
                               float radius);

/**
 * allow for small values [0.5 - 2.5],
 * and large values, FLT_MAX by clamping by the area size
 */
int ED_view3d_backbuf_sample_size_clamp(ARegion *region, float dist);

void ED_view3d_select_id_validate(const ViewContext *vc);

/** Check if the last auto-dist can be used. */
bool ED_view3d_autodist_last_check(wmWindow *win, const wmEvent *event);
/**
 * \return true when `r_ofs` is set.
 * \warning #ED_view3d_autodist_last_check should be called first to ensure the data is available.
 */
bool ED_view3d_autodist_last_get(wmWindow *win, float r_ofs[3]);
void ED_view3d_autodist_last_set(wmWindow *win,
                                 const wmEvent *event,
                                 const float ofs[3],
                                 const bool has_depth);
/** Clear and free auto-dist data. */
void ED_view3d_autodist_last_clear(wmWindow *win);

/**
 * Get the world-space 3D location from a screen-space 2D point.
 * It may be useful to call #ED_view3d_depth_override before.
 *
 * \param mval: Input screen-space pixel location.
 * \param mouse_worldloc: Output world-space location.
 * \param fallback_depth_pt: Use this points depth when no depth can be found.
 */
bool ED_view3d_autodist(ARegion *region,
                        View3D *v3d,
                        const int mval[2],
                        float mouse_worldloc[3],
                        const float fallback_depth_pt[3]);

/**
 * No 4x4 sampling, run #ED_view3d_depth_override first.
 */
bool ED_view3d_autodist_simple(ARegion *region,
                               const int mval[2],
                               float mouse_worldloc[3],
                               int margin,
                               const float *force_depth);
bool ED_view3d_depth_read_cached_seg(const ViewDepths *vd,
                                     const int mval_sta[2],
                                     const int mval_end[2],
                                     int margin,
                                     float *r_depth);

/**
 * Returns viewport color in linear space, matching #ED_space_node_color_sample().
 */
class ViewportColorSampleSession {
  blender::gpu::Texture *tex = nullptr;
  blender::ushort4 *data = nullptr;
  int tex_w, tex_h;
  rcti valid_rect;

 public:
  ViewportColorSampleSession() = default;
  ~ViewportColorSampleSession();

  bool init(ARegion *region);
  bool sample(const int mval[2], float r_col[3]);
};

enum eV3DSelectMode {
  /* all elements in the region, ignore depth */
  VIEW3D_SELECT_ALL = 0,
  /* pick also depth sorts (only for small regions!) */
  VIEW3D_SELECT_PICK_ALL = 1,
  /* sorts and only returns visible objects (only for small regions!) */
  VIEW3D_SELECT_PICK_NEAREST = 2,
};

enum eV3DSelectObjectFilter {
  /** Don't exclude anything. */
  VIEW3D_SELECT_FILTER_NOP = 0,
  /** Don't select objects outside the current mode. */
  VIEW3D_SELECT_FILTER_OBJECT_MODE_LOCK = 1,
  /** A version of #VIEW3D_SELECT_FILTER_OBJECT_MODE_LOCK that allows pose-bone selection. */
  VIEW3D_SELECT_FILTER_WPAINT_POSE_MODE_LOCK = 2,
};

eV3DSelectObjectFilter ED_view3d_select_filter_from_mode(const Scene *scene, const Object *obact);

/**
 * Optionally cache data for multiple calls to #view3d_gpu_select
 *
 * just avoid GPU_select headers outside this file
 */
void view3d_gpu_select_cache_begin();
void view3d_gpu_select_cache_end();

/**
 * \note (vc->obedit == NULL) can be set to explicitly skip edit-object selection.
 */
int view3d_gpu_select_ex(const ViewContext *vc,
                         GPUSelectBuffer *buffer,
                         const rcti *input,
                         eV3DSelectMode select_mode,
                         eV3DSelectObjectFilter select_filter,
                         bool do_material_slot_selection);
int view3d_gpu_select(const ViewContext *vc,
                      GPUSelectBuffer *buffer,
                      const rcti *input,
                      eV3DSelectMode select_mode,
                      eV3DSelectObjectFilter select_filter);
int view3d_gpu_select_with_id_filter(const ViewContext *vc,
                                     GPUSelectBuffer *buffer,
                                     const rcti *input,
                                     eV3DSelectMode select_mode,
                                     eV3DSelectObjectFilter select_filter,
                                     uint select_id);

/* `view3d_select.cc` */

float ED_view3d_select_dist_px();
ViewContext ED_view3d_viewcontext_init(bContext *C, Depsgraph *depsgraph);

/**
 * Re-initialize `vc` with `obact` as if it's active object (with some differences).
 *
 * This is often used when operating on multiple objects in modes (edit, pose mode etc)
 * where the `vc` is passed in as an argument which then references its object data.
 *
 * \note members #ViewContext.obedit & #ViewContext.em are only initialized if they're already set,
 * by #ED_view3d_viewcontext_init in most cases.
 * This is necessary because the active object defines the current object-mode.
 * When iterating over objects in object-mode it doesn't make sense to perform
 * an edit-mode action on an object that happens to contain edit-mode data.
 * In some cases these values are cleared allowing the owner of `vc` to explicitly
 * disable edit-mode operation (to force object selection in edit-mode for example).
 * So object-mode specific values should remain cleared when initialized with another object.
 */
void ED_view3d_viewcontext_init_object(ViewContext *vc, Object *obact);
/**
 * Use this call when executing an operator,
 * event system doesn't set for each event the OpenGL drawing context.
 */
void view3d_operator_needs_gpu(const bContext *C);
void view3d_region_operator_needs_gpu(ARegion *region);

/** XXX: should move to BLI_math */
bool edge_inside_circle(const float cent[2],
                        float radius,
                        const float screen_co_a[2],
                        const float screen_co_b[2]);

/**
 * Get 3D region from context, also if mouse is in header or toolbar.
 */
RegionView3D *ED_view3d_context_rv3d(bContext *C);
/**
 * Ideally would return an rv3d but in some cases the region is needed too
 * so return that, the caller can then access the `region->regiondata`.
 */
bool ED_view3d_context_user_region(bContext *C, View3D **r_v3d, ARegion **r_region);
/**
 * Similar to #ED_view3d_context_user_region() but does not use context. Always performs a lookup.
 * Also works if \a v3d is not the active space.
 */
bool ED_view3d_area_user_region(const ScrArea *area, const View3D *v3d, ARegion **r_region);
bool ED_operator_rv3d_user_region_poll(bContext *C);

/**
 * Most of the time this isn't needed since you could assume the view matrix was
 * set while drawing, however when functions like mesh_foreachScreenVert are
 * called by selection tools, we can't be sure this object was the last.
 *
 * for example, transparent objects are drawn after edit-mode and will cause
 * the rv3d mat's to change and break selection.
 *
 * 'ED_view3d_init_mats_rv3d' should be called before
 * view3d_project_short_clip and view3d_project_short_noclip in cases where
 * these functions are not used during draw_object
 */
void ED_view3d_init_mats_rv3d(const Object *ob, RegionView3D *rv3d);
void ED_view3d_init_mats_rv3d_gl(const Object *ob, RegionView3D *rv3d);
#ifndef NDEBUG
/**
 * Ensure we correctly initialize.
 */
void ED_view3d_clear_mats_rv3d(RegionView3D *rv3d);
void ED_view3d_check_mats_rv3d(RegionView3D *rv3d);
#else
#  define ED_view3d_clear_mats_rv3d(rv3d) (void)(rv3d)
#  define ED_view3d_check_mats_rv3d(rv3d) (void)(rv3d)
#endif

RV3DMatrixStore *ED_view3d_mats_rv3d_backup(RegionView3D *rv3d);
void ED_view3d_mats_rv3d_restore(RegionView3D *rv3d, RV3DMatrixStore *rv3dmat);
void ED_view3D_mats_rv3d_free(RV3DMatrixStore *rv3d_mat);

RenderEngineType *ED_view3d_engine_type(const Scene *scene, int drawtype);

bool ED_view3d_context_activate(bContext *C);
/**
 * Set the correct matrices
 */
void ED_view3d_draw_setup_view(const wmWindowManager *wm,
                               wmWindow *win,
                               Depsgraph *depsgraph,
                               Scene *scene,
                               ARegion *region,
                               View3D *v3d,
                               const float viewmat[4][4],
                               const float winmat[4][4],
                               const rcti *rect);

/**
 * `mval` comes from event->mval, only use within region handlers.
 */
Base *ED_view3d_give_base_under_cursor(bContext *C, const int mval[2]);
Object *ED_view3d_give_object_under_cursor(bContext *C, const int mval[2]);
Object *ED_view3d_give_material_slot_under_cursor(bContext *C,
                                                  const int mval[2],
                                                  int *r_material_slot);
bool ED_view3d_is_object_under_cursor(bContext *C, const int mval[2]);
/**
 * 'clip' is used to know if our clip setting has changed.
 */
void ED_view3d_quadview_update(ScrArea *area, ARegion *region, bool do_clip);
/**
 * \note keep this synced with #ED_view3d_mats_rv3d_backup/#ED_view3d_mats_rv3d_restore
 */
void ED_view3d_update_viewmat(const Depsgraph *depsgraph,
                              const Scene *scene,
                              View3D *v3d,
                              ARegion *region,
                              const float viewmat[4][4],
                              const float winmat[4][4],
                              const rcti *rect,
                              bool offscreen);
bool ED_view3d_quat_from_axis_view(char view, char view_axis_roll, float r_quat[4]);
bool ED_view3d_quat_to_axis_view(const float quat[4],
                                 float epsilon,
                                 char *r_view,
                                 char *r_view_axis_roll);
/**
 * A version of #ED_view3d_quat_to_axis_view that updates `quat`
 * if it's within `epsilon` to an axis-view.
 *
 * \note Include the special case function since most callers need to perform these operations.
 */
bool ED_view3d_quat_to_axis_view_and_reset_quat(float quat[4],
                                                float epsilon,
                                                char *r_view,
                                                char *r_view_axis_roll);

char ED_view3d_lock_view_from_index(int index);
char ED_view3d_axis_view_opposite(char view);
bool ED_view3d_lock(RegionView3D *rv3d);

void ED_view3d_datamask(const Scene *scene,
                        ViewLayer *view_layer,
                        const View3D *v3d,
                        CustomData_MeshMasks *r_cddata_masks);
/**
 * Goes over all modes and view3d settings.
 */
void ED_view3d_screen_datamask(const Scene *scene,
                               ViewLayer *view_layer,
                               const bScreen *screen,
                               CustomData_MeshMasks *r_cddata_masks);

bool ED_view3d_offset_lock_check(const View3D *v3d, const RegionView3D *rv3d);
/**
 * For viewport operators that exit camera perspective.
 *
 * \note This differs from simply setting `rv3d->persp = persp` because it
 * sets the `ofs` and `dist` values of the viewport so it matches the camera,
 * otherwise switching out of camera view may jump to a different part of the scene.
 */
void ED_view3d_persp_switch_from_camera(const Depsgraph *depsgraph,
                                        View3D *v3d,
                                        RegionView3D *rv3d,
                                        char persp);
/**
 * Action to take when rotating the view,
 * handle auto-perspective and logic for switching out of views.
 *
 * shared with NDOF.
 */
bool ED_view3d_persp_ensure(const Depsgraph *depsgraph, View3D *v3d, ARegion *region);

/* Camera view functions. */

/**
 * Utility to scale zoom level when in camera-view #RegionView3D.camzoom and apply limits.
 * \return true a change was made.
 */
bool ED_view3d_camera_view_zoom_scale(RegionView3D *rv3d, const float scale);
/**
 * Utility to pan when in camera view.
 * \param event_ofs: The offset the pan in screen (pixel) coordinates.
 * \return true when a change was made.
 */
bool ED_view3d_camera_view_pan(ARegion *region, const float event_ofs[2]);

/* Camera lock functions */

/**
 * \return true when the 3D Viewport is locked to its camera.
 */
bool ED_view3d_camera_lock_check(const View3D *v3d, const RegionView3D *rv3d);
/**
 * Copy the camera to the view before starting a view transformation.
 *
 * Apply the camera object transformation to the 3D Viewport.
 * (needed so we can use regular 3D Viewport manipulation operators, that sync back to the camera).
 */
void ED_view3d_camera_lock_init_ex(const Depsgraph *depsgraph,
                                   View3D *v3d,
                                   RegionView3D *rv3d,
                                   bool calc_dist);
void ED_view3d_camera_lock_init(const Depsgraph *depsgraph, View3D *v3d, RegionView3D *rv3d);
/**
 * Copy the view to the camera, return true if.
 *
 * Apply the 3D Viewport transformation back to the camera object.
 *
 * \return true if the camera (or one of it's parents) was moved.
 */
bool ED_view3d_camera_lock_sync(const Depsgraph *depsgraph, View3D *v3d, RegionView3D *rv3d);

bool ED_view3d_camera_autokey(
    const Scene *scene, ID *id_key, bContext *C, bool do_rotate, bool do_translate);
/**
 * Call after modifying a locked view.
 *
 * \note Not every view edit currently auto-keys (numeric-pad for eg),
 * this is complicated because of smooth-view.
 */
bool ED_view3d_camera_lock_autokey(
    View3D *v3d, RegionView3D *rv3d, bContext *C, bool do_rotate, bool do_translate);

void ED_view3d_lock_clear(View3D *v3d);

/**
 * Check if creating an undo step should be performed if the viewport moves.
 * \return true if #ED_view3d_camera_lock_undo_push would do an undo push.
 */
bool ED_view3d_camera_lock_undo_test(const View3D *v3d, const RegionView3D *rv3d, bContext *C);

/**
 * Create an undo step when the camera is locked to the view.
 * \param str: The name of the undo step (typically #wmOperatorType.name should be used).
 *
 * \return true when the call to push an undo step was made.
 */
bool ED_view3d_camera_lock_undo_push(const char *str,
                                     const View3D *v3d,
                                     const RegionView3D *rv3d,
                                     bContext *C);

/**
 * A version of #ED_view3d_camera_lock_undo_push that performs a grouped undo push.
 *
 * \note use for actions that are likely to be repeated such as mouse wheel to zoom,
 * where adding a separate undo step each time isn't desirable.
 */
bool ED_view3d_camera_lock_undo_grouped_push(const char *str,
                                             const View3D *v3d,
                                             const RegionView3D *rv3d,
                                             bContext *C);

#define VIEW3D_MARGIN 1.4f
#define VIEW3D_DIST_FALLBACK 1.0f

/**
 * This function solves the problem of having to switch between camera and non-camera views.
 *
 * When viewing from the perspective of \a mat, and having the view center \a ofs,
 * this calculates a distance from \a ofs to the matrix \a mat.
 * Using \a fallback_dist when the distance would be too small.
 *
 * \param mat: A matrix use for the view-point (typically the camera objects matrix).
 * \param ofs: Orbit center (negated), matching #RegionView3D.ofs, which is typically passed in.
 * \param fallback_dist: The distance to use if the object is too near or in front of \a ofs.
 * \returns A newly calculated distance or the fallback.
 */
float ED_view3d_offset_distance(const float mat[4][4], const float ofs[3], float fallback_dist);
/**
 * Set the dist without moving the view (compensate with #RegionView3D.ofs)
 *
 * \note take care that #RegionView3d.viewinv is up to date, #ED_view3d_update_viewmat first.
 */
void ED_view3d_distance_set(RegionView3D *rv3d, float dist);
/**
 * Change the distance & offset to match the depth of \a dist_co along the view axis.
 *
 * \param dist_co: A world-space location to use for the new depth.
 * \param dist_min: Resulting distances below this will be ignored.
 * \return Success if the distance was set.
 */
bool ED_view3d_distance_set_from_location(RegionView3D *rv3d,
                                          const float dist_co[3],
                                          float dist_min);

/**
 * Could move this elsewhere, but tied into #ED_view3d_grid_scale
 */
float ED_scene_grid_scale(const Scene *scene, const char **r_grid_unit);
float ED_view3d_grid_scale(const Scene *scene, const View3D *v3d, const char **r_grid_unit);
void ED_view3d_grid_steps(const Scene *scene,
                          const View3D *v3d,
                          const RegionView3D *rv3d,
                          float r_grid_steps[8]);
/**
 * Simulates the grid scale that is actually viewed.
 * The actual code is seen in `object_grid_frag.glsl` (see `grid_res`).
 * Currently the simulation is only done when RV3D_VIEW_IS_AXIS.
 */
float ED_view3d_grid_view_scale(const Scene *scene,
                                const View3D *v3d,
                                const ARegion *region,
                                const char **r_grid_unit);

/**
 * \note The info that this uses is updated in #ED_scene_fps_average_accumulate,
 * which currently gets called during #SCREEN_OT_animation_step.
 */
void ED_scene_draw_fps(const Scene *scene, int xoffset, int *yoffset);

/* Render */

void ED_view3d_stop_render_preview(wmWindowManager *wm, ARegion *region);
void ED_view3d_shade_update(Main *bmain, View3D *v3d, ScrArea *area);

#define SHADING_XRAY_ALPHA(shading) \
  (((shading).type == OB_WIRE) ? (shading).xray_alpha_wire : (shading).xray_alpha)
#define SHADING_XRAY_FLAG(shading) \
  (((shading).type == OB_WIRE) ? V3D_SHADING_XRAY_WIREFRAME : V3D_SHADING_XRAY)
#define SHADING_XRAY_FLAG_ENABLED(shading) (((shading).flag & SHADING_XRAY_FLAG(shading)) != 0)
#define SHADING_XRAY_ENABLED(shading) \
  (SHADING_XRAY_FLAG_ENABLED(shading) && (SHADING_XRAY_ALPHA(shading) < 1.0f))
#define SHADING_XRAY_ACTIVE(shading) \
  (SHADING_XRAY_ENABLED(shading) && ((shading).type < OB_MATERIAL))

#define XRAY_ALPHA(v3d) SHADING_XRAY_ALPHA((v3d)->shading)
#define XRAY_FLAG(v3d) SHADING_XRAY_FLAG((v3d)->shading)
#define XRAY_FLAG_ENABLED(v3d) SHADING_XRAY_FLAG_ENABLED((v3d)->shading)
/**
 * Checks X-ray is enabled and the alpha is less than one.
 *
 * \note In edit-mode vertices & edges behave differently,
 * using X-ray drawing irrespective of the alpha value.
 * In this case #XRAY_FLAG_ENABLED should be used instead.
 */
#define XRAY_ENABLED(v3d) SHADING_XRAY_ENABLED((v3d)->shading)
#define XRAY_ACTIVE(v3d) SHADING_XRAY_ACTIVE((v3d)->shading)

#define OVERLAY_RETOPOLOGY_ENABLED(overlay) \
  (((overlay).edit_flag & V3D_OVERLAY_EDIT_RETOPOLOGY) != 0)
#ifdef __APPLE__
/* Apple silicon tile depth test requires a higher value to reduce drawing artifacts. */
#  define OVERLAY_RETOPOLOGY_MIN_OFFSET 0.0015f
#else
#  define OVERLAY_RETOPOLOGY_MIN_OFFSET FLT_EPSILON
#endif

#define OVERLAY_RETOPOLOGY_OFFSET(overlay) \
  (OVERLAY_RETOPOLOGY_ENABLED(overlay) ? \
       max_ff((overlay).retopology_offset, OVERLAY_RETOPOLOGY_MIN_OFFSET) : \
       0.0f)

#define RETOPOLOGY_ENABLED(v3d) (OVERLAY_RETOPOLOGY_ENABLED((v3d)->overlay))
#define RETOPOLOGY_OFFSET(v3d) (OVERLAY_RETOPOLOGY_OFFSET((v3d)->overlay))

/* `view3d_gizmo_preselect_type.cc` */

void ED_view3d_gizmo_mesh_preselect_get_active(const bContext *C,
                                               const wmGizmo *gz,
                                               Base **r_base,
                                               BMElem **r_ele);
void ED_view3d_gizmo_mesh_preselect_clear(wmGizmo *gz);

/* view3d_gizmo_ruler.cc */

/**
 * Remove all rulers when Annotation layer is removed.
 */
void ED_view3d_gizmo_ruler_remove_by_gpencil_layer(struct bContext *C, bGPDlayer *gpl);

/* `space_view3d.cc` */

void ED_view3d_buttons_region_layout_ex(const bContext *C,
                                        ARegion *region,
                                        const char *category_override);

/* `view3d_view.cc` */

/**
 * Exit 'local view' of given View3D editor, if it is active and there is nothing to display in it
 * anymore.
 *
 * \param depsgraph: Optional, only required for #frame_selected.
 * \param frame_selected: Frame the newly out-of-local view to show currently visible selected
 * objects. Will only do something if a valid #depsgraph pointer is also provided.
 * \param smooth_viewtx: Smooth transition time (in milliseconds) between current view and final
 * view, if changes are happening. Currently only used if #frame_selected is enabled.
 *
 * \return `true` if the local view was actually exited.
 */
bool ED_localview_exit_if_empty(const Depsgraph *depsgraph,
                                Scene *scene,
                                ViewLayer *view_layer,
                                wmWindowManager *wm,
                                wmWindow *win,
                                View3D *v3d,
                                ScrArea *area,
                                bool frame_selected = true,
                                int smooth_viewtx = 0);
/**
 * See if current UUID is valid, otherwise set a valid UUID to v3d,
 * Try to keep the same UUID previously used to allow users to quickly toggle back and forth.
 */
bool ED_view3d_local_collections_set(const Main *bmain, View3D *v3d);
void ED_view3d_local_collections_reset(const bContext *C, bool reset_all);

#ifdef WITH_XR_OPENXR
void ED_view3d_xr_mirror_update(const ScrArea *area, const View3D *v3d, bool enable);
void ED_view3d_xr_shading_update(wmWindowManager *wm, const View3D *v3d, const Scene *scene);
bool ED_view3d_is_region_xr_mirror_active(const wmWindowManager *wm,
                                          const View3D *v3d,
                                          const ARegion *region);
#endif
