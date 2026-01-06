/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_math_matrix_types.hh"
#include "BLI_math_quaternion_types.hh"

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_scene_enums.h"
#include "DNA_space_enums.h"
#include "DNA_view3d_enums.h"
#include "DNA_viewer_path_types.h"

namespace blender {

struct BoundBox;
struct Object;
struct ViewRender;
struct SmoothView3DStore;
struct SpaceLink;
struct bGPdata;
struct wmTimer;

/** #View3DOverlay.handle_display */
enum eHandleDisplay {
  /* Display only selected points. */
  CURVE_HANDLE_SELECTED = 0,
  /* Display all handles. */
  CURVE_HANDLE_ALL = 1,
  /* No display handles. */
  CURVE_HANDLE_NONE = 2,
};

/** #View3D::stereo3d_flag */
enum {
  V3D_S3D_DISPCAMERAS = 1 << 0,
  V3D_S3D_DISPPLANE = 1 << 1,
  V3D_S3D_DISPVOLUME = 1 << 2,
};

/** #View3D::flag */
enum {
  V3D_LOCAL_COLLECTIONS = 1 << 0,
  V3D_FLAG_UNUSED_1 = 1 << 1, /* cleared */
  V3D_HIDE_HELPLINES = 1 << 2,
  V3D_FLAG_UNUSED_2 = 1 << 3, /* cleared */
  V3D_XR_SESSION_MIRROR = 1 << 4,
  V3D_XR_SESSION_SURFACE = 1 << 5,

  V3D_FLAG_UNUSED_10 = 1 << 10, /* cleared */
  V3D_SELECT_OUTLINE = 1 << 11,
  V3D_FLAG_UNUSED_12 = 1 << 12, /* cleared */
  V3D_GLOBAL_STATS = 1 << 13,
  V3D_DRAW_CENTERS = 1 << 15,
};

/** #View3D_Runtime.flag */
enum {
  /** The 3D view which the XR session was created in is flagged with this. */
  V3D_RUNTIME_XR_SESSION_ROOT = (1 << 0),
  /** Some operators override the depth buffer for dedicated occlusion operations. */
  V3D_RUNTIME_DEPTHBUF_OVERRIDDEN = (1 << 1),
  /** Local view may have become empty, and may need to be exited. */
  V3D_RUNTIME_LOCAL_MAYBE_EMPTY = (1 << 2),
  /** Last offset is valid. */
  V3D_RUNTIME_OFS_LAST_CENTER_IS_VALID = (1 << 3),
};

/** #RegionView3D::persp */
enum {
  RV3D_ORTHO = 0,
  RV3D_PERSP = 1,
  RV3D_CAMOB = 2,
};

/** #RegionView3D::rflag */
enum {
  RV3D_CLIPPING = 1 << 2,
  RV3D_NAVIGATING = 1 << 3,
  RV3D_GPULIGHT_UPDATE = 1 << 4,
  RV3D_PAINTING = 1 << 5,
  /**
   * Disable Z-buffer offset, skip calls to #ED_view3d_polygon_offset.
   * Use when precise surface depth is needed and picking bias isn't, see #45434).
   */
  RV3D_ZOFFSET_DISABLED = 1 << 6,
  RV3D_WAS_CAMOB = 1 << 7,
};

/** #RegionView3D.viewlock */
enum {
  /**
   * Used to lock axis views when quad-view is enabled.
   *
   * \note this implies locking the perspective as these views
   * should use an orthographic projection.
   */
  RV3D_LOCK_ROTATION = (1 << 0),
  RV3D_BOXVIEW = (1 << 1),
  RV3D_BOXCLIP = (1 << 2),
  RV3D_LOCK_LOCATION = (1 << 3),
  RV3D_LOCK_ZOOM_AND_DOLLY = (1 << 4),

  RV3D_LOCK_ANY_TRANSFORM = (RV3D_LOCK_LOCATION | RV3D_LOCK_ROTATION | RV3D_LOCK_ZOOM_AND_DOLLY),
};

/** Bit-wise OR of the regular lock-flags with runtime only lock-flags. */
#define RV3D_LOCK_FLAGS(rv3d) ((rv3d)->viewlock | ((rv3d)->runtime_viewlock))

/** #RegionView3D::viewlock_quad */
enum {
  RV3D_VIEWLOCK_INIT = 1 << 7,
};

/** #RegionView3D::view */
enum {
  RV3D_VIEW_USER = 0,
  RV3D_VIEW_FRONT = 1,
  RV3D_VIEW_BACK = 2,
  RV3D_VIEW_LEFT = 3,
  RV3D_VIEW_RIGHT = 4,
  RV3D_VIEW_TOP = 5,
  RV3D_VIEW_BOTTOM = 6,
  RV3D_VIEW_CAMERA = 8,
};

#define RV3D_VIEW_IS_AXIS(view) (((view) >= RV3D_VIEW_FRONT) && ((view) <= RV3D_VIEW_BOTTOM))

/**
 * #RegionView3D.view_axis_roll
 *
 * Clockwise rotation to use for axis-views, when #RV3D_VIEW_IS_AXIS is true.
 */
enum {
  RV3D_VIEW_AXIS_ROLL_0 = 0,
  RV3D_VIEW_AXIS_ROLL_90 = 1,
  RV3D_VIEW_AXIS_ROLL_180 = 2,
  RV3D_VIEW_AXIS_ROLL_270 = 3,
};

/** #RegionView3D::ndof_flag */
enum {
  /**
   * When set, #RegionView3D::ndof_ofs may be used instead of #RegionView3D::ofs,
   *
   * This value will be recalculated when starting NDOF motion,
   * however if the center can *not* be calculated, the previous value may be used.
   *
   * To prevent strange behavior some checks should be used
   * to ensure the previously calculated value makes sense.
   *
   * The most common case is for perspective views, where orbiting around a point behind
   * the view (while possible) often seems like a bug from a user perspective.
   * We could consider other cases invalid too (e.g. values beyond the clipping plane),
   * although in practice these cases should be fairly rare.
   */
  RV3D_NDOF_OFS_IS_VALID = (1 << 0),
};

#define RV3D_CLIPPING_ENABLED(v3d, rv3d) \
  ((rv3d) && (v3d) && ((rv3d)->rflag & RV3D_CLIPPING) && \
   ELEM((v3d)->shading.type, OB_WIRE, OB_SOLID) && (rv3d)->clipbb)

/** #View3D::flag2 (int) */
enum {
  V3D_HIDE_OVERLAYS = 1 << 2,
  V3D_SHOW_VIEWER = 1 << 3,
  V3D_SHOW_ANNOTATION = 1 << 4,
  V3D_LOCK_CAMERA = 1 << 5,
  V3D_FLAG2_UNUSED_6 = 1 << 6, /* cleared */
  V3D_SHOW_RECONSTRUCTION = 1 << 7,
  V3D_SHOW_CAMERAPATH = 1 << 8,
  V3D_SHOW_BUNDLENAME = 1 << 9,
  V3D_FLAG2_UNUSED_10 = 1 << 10, /* cleared */
  V3D_RENDER_BORDER = 1 << 11,
  V3D_FLAG2_UNUSED_12 = 1 << 12, /* cleared */
  V3D_FLAG2_UNUSED_13 = 1 << 13, /* cleared */
  V3D_FLAG2_UNUSED_14 = 1 << 14, /* cleared */
  V3D_FLAG2_UNUSED_15 = 1 << 15, /* cleared */
  V3D_XR_SHOW_CONTROLLERS = 1 << 16,
  V3D_XR_SHOW_CUSTOM_OVERLAYS = 1 << 17,
  V3D_SHOW_CAMERA_GUIDES = (1 << 18),
  V3D_SHOW_CAMERA_PASSEPARTOUT = (1 << 19),
  V3D_XR_SHOW_PASSTHROUGH = 1 << 20,
};

/** #View3D::gp_flag (short) */
enum {
  /** Fade all non GP objects. */
  V3D_GP_FADE_OBJECTS = 1 << 0,
  /** Activate paper grid. */
  V3D_GP_SHOW_GRID = 1 << 1,
  V3D_GP_SHOW_EDIT_LINES = 1 << 2,
  V3D_GP_SHOW_MULTIEDIT_LINES = 1 << 3,
  /** main switch at view level. */
  V3D_GP_SHOW_ONION_SKIN = 1 << 4,
  /** fade layers not active. */
  V3D_GP_FADE_NOACTIVE_LAYERS = 1 << 5,
  /** Fade other GPencil objects. */
  V3D_GP_FADE_NOACTIVE_GPENCIL = 1 << 6,
  /** Show Strokes Directions. */
  V3D_GP_SHOW_STROKE_DIRECTION = 1 << 7,
  /** Show Material names. */
  V3D_GP_SHOW_MATERIAL_NAME = 1 << 8,
  /** Show Canvas Grid on Top. */
  V3D_GP_SHOW_GRID_XRAY = 1 << 9,
  /** Force 3D depth rendering and ignore per-object stroke depth mode. */
  V3D_GP_FORCE_STROKE_ORDER_3D = 1 << 10,
  /** Onion skin for active object only. */
  V3D_GP_ONION_SKIN_ACTIVE_OBJECT = 1 << 11,
};

/** #View3DShading.flag */
enum {
  V3D_SHADING_OBJECT_OUTLINE = (1 << 0),
  V3D_SHADING_XRAY = (1 << 1),
  V3D_SHADING_SHADOW = (1 << 2),
  V3D_SHADING_SCENE_LIGHTS = (1 << 3),
  V3D_SHADING_SPECULAR_HIGHLIGHT = (1 << 4),
  V3D_SHADING_CAVITY = (1 << 5),
  V3D_SHADING_MATCAP_FLIP_X = (1 << 6),
  V3D_SHADING_SCENE_WORLD = (1 << 7),
  V3D_SHADING_XRAY_WIREFRAME = (1 << 8),
  V3D_SHADING_WORLD_ORIENTATION = (1 << 9),
  V3D_SHADING_BACKFACE_CULLING = (1 << 10),
  V3D_SHADING_DEPTH_OF_FIELD = (1 << 11),
  V3D_SHADING_SCENE_LIGHTS_RENDER = (1 << 12),
  V3D_SHADING_SCENE_WORLD_RENDER = (1 << 13),
  V3D_SHADING_STUDIOLIGHT_VIEW_ROTATION = (1 << 14),
};

/** #View3D.debug_flag */
enum {
  V3D_DEBUG_FREEZE_CULLING = (1 << 0),
};

#define V3D_USES_SCENE_LIGHTS(v3d) \
  ((((v3d)->shading.type == OB_MATERIAL) && ((v3d)->shading.flag & V3D_SHADING_SCENE_LIGHTS)) || \
   (((v3d)->shading.type == OB_RENDER) && \
    ((v3d)->shading.flag & V3D_SHADING_SCENE_LIGHTS_RENDER)))

#define V3D_USES_SCENE_WORLD(v3d) \
  ((((v3d)->shading.type == OB_MATERIAL) && ((v3d)->shading.flag & V3D_SHADING_SCENE_WORLD)) || \
   (((v3d)->shading.type == OB_RENDER) && \
    ((v3d)->shading.flag & V3D_SHADING_SCENE_WORLD_RENDER)))

/** #View3DShading.cavity_type */
enum {
  V3D_SHADING_CAVITY_SSAO = 0,
  V3D_SHADING_CAVITY_CURVATURE = 1,
  V3D_SHADING_CAVITY_BOTH = 2,
};

/** #View3DShading.use_compositor */
enum View3DShadingUseCompositor {
  V3D_SHADING_USE_COMPOSITOR_DISABLED = 0,
  /** The compositor is enabled only in camera view. */
  V3D_SHADING_USE_COMPOSITOR_CAMERA = 1,
  /** The compositor is always enabled regardless of the view. */
  V3D_SHADING_USE_COMPOSITOR_ALWAYS = 2,
};

/** #View3DOverlay.flag */
enum {
  V3D_OVERLAY_FACE_ORIENTATION = (1 << 0),
  V3D_OVERLAY_HIDE_CURSOR = (1 << 1),
  V3D_OVERLAY_BONE_SELECT = (1 << 2),
  V3D_OVERLAY_LOOK_DEV = (1 << 3),
  V3D_OVERLAY_WIREFRAMES = (1 << 4),
  V3D_OVERLAY_HIDE_TEXT = (1 << 5),
  V3D_OVERLAY_HIDE_MOTION_PATHS = (1 << 6),
  V3D_OVERLAY_ONION_SKINS = (1 << 7),
  V3D_OVERLAY_HIDE_BONES = (1 << 8),
  V3D_OVERLAY_HIDE_OBJECT_XTRAS = (1 << 9),
  V3D_OVERLAY_HIDE_OBJECT_ORIGINS = (1 << 10),
  V3D_OVERLAY_STATS = (1 << 11),
  V3D_OVERLAY_FADE_INACTIVE = (1 << 12),
  V3D_OVERLAY_VIEWER_ATTRIBUTE = (1 << 13),
  V3D_OVERLAY_SCULPT_SHOW_MASK = (1 << 14),
  V3D_OVERLAY_SCULPT_SHOW_FACE_SETS = (1 << 15),
  V3D_OVERLAY_SCULPT_CURVES_CAGE = (1 << 16),
  V3D_OVERLAY_SHOW_LIGHT_COLORS = (1 << 17),
  V3D_OVERLAY_VIEWER_ATTRIBUTE_TEXT = (1 << 18),
  V3D_OVERLAY_PERFORMANCE = (1 << 19),
};

/** #View3DOverlay.edit_flag */
enum {
  V3D_OVERLAY_EDIT_VERT_NORMALS = (1 << 0),
  V3D_OVERLAY_EDIT_LOOP_NORMALS = (1 << 1),
  V3D_OVERLAY_EDIT_FACE_NORMALS = (1 << 2),

  V3D_OVERLAY_EDIT_RETOPOLOGY = (1 << 3),

  V3D_OVERLAY_EDIT_WEIGHT = (1 << 4),

  V3D_OVERLAY_EDIT_EDGES_DEPRECATED = (1 << 5),
  V3D_OVERLAY_EDIT_FACES = (1 << 6),
  V3D_OVERLAY_EDIT_FACE_DOT = (1 << 7),

  V3D_OVERLAY_EDIT_SEAMS = (1 << 8),
  V3D_OVERLAY_EDIT_SHARP = (1 << 9),
  V3D_OVERLAY_EDIT_CREASES = (1 << 10),
  V3D_OVERLAY_EDIT_BWEIGHTS = (1 << 11),

  V3D_OVERLAY_EDIT_FREESTYLE_EDGE = (1 << 12),
  V3D_OVERLAY_EDIT_FREESTYLE_FACE = (1 << 13),

  V3D_OVERLAY_EDIT_STATVIS = (1 << 14),
  V3D_OVERLAY_EDIT_EDGE_LEN = (1 << 15),
  V3D_OVERLAY_EDIT_EDGE_ANG = (1 << 16),
  V3D_OVERLAY_EDIT_FACE_ANG = (1 << 17),
  V3D_OVERLAY_EDIT_FACE_AREA = (1 << 18),
  V3D_OVERLAY_EDIT_INDICES = (1 << 19),

  /* Deprecated. */
  // V3D_OVERLAY_EDIT_CU_HANDLES = (1 << 20),

  V3D_OVERLAY_EDIT_CU_NORMALS = (1 << 21),
  V3D_OVERLAY_EDIT_CONSTANT_SCREEN_SIZE_NORMALS = (1 << 22),
};

/** #View3DOverlay.paint_flag */
enum {
  V3D_OVERLAY_PAINT_WIRE = (1 << 0),
};

/** #View3DOverlay.wpaint_flag */
enum {
  V3D_OVERLAY_WPAINT_CONTOURS = (1 << 0),
};

/** #View3D.around */
enum {
  /* center of the bounding box */
  V3D_AROUND_CENTER_BOUNDS = 0,
  /* center from the sum of all points divided by the total */
  V3D_AROUND_CENTER_MEDIAN = 3,
  /* pivot around the 2D/3D cursor */
  V3D_AROUND_CURSOR = 1,
  /* pivot around each items own origin */
  V3D_AROUND_LOCAL_ORIGINS = 2,
  /* pivot around the active items origin */
  V3D_AROUND_ACTIVE = 4,
};

/** #View3D.gridflag */
enum {
  V3D_SHOW_FLOOR = 1 << 0,
  V3D_SHOW_X = 1 << 1,
  V3D_SHOW_Y = 1 << 2,
  V3D_SHOW_Z = 1 << 3,
  V3D_SHOW_ORTHO_GRID = 1 << 4,
};

/** #TransformOrientationSlot.type */
enum {
  V3D_ORIENT_GLOBAL = 0,
  V3D_ORIENT_LOCAL = 1,
  V3D_ORIENT_NORMAL = 2,
  V3D_ORIENT_VIEW = 3,
  V3D_ORIENT_GIMBAL = 4,
  V3D_ORIENT_CURSOR = 5,
  V3D_ORIENT_PARENT = 6,
  V3D_ORIENT_CUSTOM = 1024,
  /** Runtime only, never saved to DNA. */
  V3D_ORIENT_CUSTOM_MATRIX = (V3D_ORIENT_CUSTOM - 1),
};

/** #View3d.gizmo_flag */
enum {
  /** All gizmos. */
  V3D_GIZMO_HIDE = (1 << 0),
  V3D_GIZMO_HIDE_NAVIGATE = (1 << 1),
  V3D_GIZMO_HIDE_CONTEXT = (1 << 2),
  V3D_GIZMO_HIDE_TOOL = (1 << 3),
  V3D_GIZMO_HIDE_MODIFIER = (1 << 4),
};

/** #View3d.gizmo_show_object */
enum {
  V3D_GIZMO_SHOW_OBJECT_TRANSLATE = (1 << 0),
  V3D_GIZMO_SHOW_OBJECT_ROTATE = (1 << 1),
  V3D_GIZMO_SHOW_OBJECT_SCALE = (1 << 2),
};
/** #View3d.gizmo_show_armature */
enum {
  /** Currently unused (WIP gizmo). */
  V3D_GIZMO_SHOW_ARMATURE_BBONE = (1 << 0),
  /** Not yet implemented. */
  V3D_GIZMO_SHOW_ARMATURE_ROLL = (1 << 1),
};
/** #View3d.gizmo_show_empty */
enum {
  V3D_GIZMO_SHOW_EMPTY_IMAGE = (1 << 0),
  V3D_GIZMO_SHOW_EMPTY_FORCE_FIELD = (1 << 1),
};
/** #View3d.gizmo_show_light */
enum {
  /** Use for both spot & area size. */
  V3D_GIZMO_SHOW_LIGHT_SIZE = (1 << 0),
  V3D_GIZMO_SHOW_LIGHT_LOOK_AT = (1 << 1),
};
/** #View3d.gizmo_show_camera */
enum {
  /** Also used for ortho size. */
  V3D_GIZMO_SHOW_CAMERA_LENS = (1 << 0),
  V3D_GIZMO_SHOW_CAMERA_DOF_DIST = (1 << 2),
};

/** #ToolSettings.plane_depth */
enum eV3DPlaceDepth {
  V3D_PLACE_DEPTH_SURFACE = 0,
  V3D_PLACE_DEPTH_CURSOR_PLANE = 1,
  V3D_PLACE_DEPTH_CURSOR_VIEW = 2,
};
/** #ToolSettings.plane_orient */
enum eV3DPlaceOrient {
  V3D_PLACE_ORIENT_SURFACE = 0,
  V3D_PLACE_ORIENT_DEFAULT = 1,
};

struct RegionView3D {
  DNA_DEFINE_CXX_METHODS(RegionView3D)

  /** GL_PROJECTION matrix. */
  float winmat[4][4] = {};
  /** GL_MODELVIEW matrix. */
  float viewmat[4][4] = {};
  /** Inverse of viewmat. */
  float viewinv[4][4] = {};
  /** Viewmat*winmat. */
  float persmat[4][4] = {};
  /** Inverse of persmat. */
  float persinv[4][4] = {};
  /** Offset/scale for camera GLSL texture-coordinates. */
  float viewcamtexcofac[4] = {};

  /** viewmat/persmat multiplied with object matrix, while drawing and selection. */
  float viewmatob[4][4] = {};
  float persmatob[4][4] = {};

  /** User defined clipping planes. */
  float clip[6][4] = {};
  /**
   * Clip in object space,
   * means we can test for clipping in edit-mode without first going into world-space.
   */
  float clip_local[6][4] = {};
  struct BoundBox *clipbb = nullptr;

  /** Allocated backup of itself while in local-view. */
  struct RegionView3D *localvd = nullptr;
  struct ViewRender *view_render = nullptr;

  /** Animated smooth view. */
  struct SmoothView3DStore *sms = nullptr;
  struct wmTimer *smooth_timer = nullptr;

  /** Transform gizmo matrix. */
  float twmat[4][4] = {};
  /** min/max dot product on `twmat` XYZ axis. */
  float tw_axis_min[3] = {}, tw_axis_max[3] = {};
  float tw_axis_matrix[3][3] = {};

  DNA_DEPRECATED float gridview = 0;

  /** View rotation, must be kept normalized. */
  float viewquat[4] = {};
  /**
   * Distance from `ofs` along `-viewinv[2]` vector, where result is negative as is `ofs`.
   *
   * \note Besides being above zero, the range of this value is not strictly defined,
   * see #ED_view3d_dist_soft_range_get to calculate a working range
   * viewport "zoom" functions to use.
   */
  float dist = 0;
  /** Camera view offsets, 1.0 = viewplane moves entire width/height. */
  float camdx = 0, camdy = 0;
  /** Runtime only. */
  float pixsize = 0;
  /**
   * View center & orbit pivot, negative of world-space location,
   * also matches `-viewinv[3][0:3]` in orthographic mode.
   */
  float ofs[3] = {};
  /** Viewport zoom on the camera frame, see BKE_screen_view3d_zoom_to_fac. */
  float camzoom = 0;
  /**
   * Check if persp/ortho view, since 'persp' can't be used for this since
   * it can have cameras assigned as well. (only set in #view3d_winmatrix_set)
   */
  char is_persp = 0;
  char persp = 0;
  char view = 0;
  char view_axis_roll = 0;
  char viewlock = 0; /* Should usually be accessed with RV3D_LOCK_FLAGS()! */
  /** Options for runtime only locking (cleared on file read) */
  char runtime_viewlock = 0; /* Should usually be accessed with RV3D_LOCK_FLAGS()! */
  /** Options for quadview (store while out of quad view). */
  char viewlock_quad = 0;
  char _pad[1] = {};
  /** Normalized offset for locked view: (-1, -1) bottom left, (1, 1) upper right. */
  float ofs_lock[2] = {};

  /** XXX can easily get rid of this (Julian). */
  short twdrawflag = 0;
  short rflag = 0;

  /** Last view (use when switching out of camera view). */
  float lviewquat[4] = {};
  /** The last perspective can never be set to #RV3D_CAMOB. */
  char lpersp = 0;
  char lview = 0;
  char lview_axis_roll = 0;
  char _pad8[4] = {};

  char ndof_flag = 0;
  /**
   * Rotation center used for "Auto Orbit" (see #NDOF_ORBIT_CENTER_AUTO).
   * Any modification should be followed by adjusting #RegionView3D::dist
   * to prevent problems zooming in after navigation. See: #134732.
   */
  float ndof_ofs[3] = {};

  /** Active rotation from NDOF (run-time only). */
  float ndof_rot_angle = 0;
  float ndof_rot_axis[3] = {};
};

struct View3DCursor {
  float location[3] = {};

  float rotation_quaternion[4] = {1, 0, 0, 0};
  float rotation_euler[3] = {};
  float rotation_axis[3] = {0, 1, 0}, rotation_angle = 0;
  short rotation_mode = ROT_MODE_XYZ;

  char _pad[6] = {};

#ifdef __cplusplus
  template<typename T> T matrix() const;
  math::Quaternion rotation() const;

  void set_rotation(const math::Quaternion &quat, bool use_compat);
  void set_matrix(const float3x3 &mat, bool use_compat);
  void set_matrix(const float4x4 &mat, bool use_compat);
#endif
};

/** 3D Viewport Shading settings. */
struct View3DShading {
  /** Shading type (OB_SOLID, ..). */
  char type = OB_SOLID;
  /** Runtime, for toggle between rendered viewport. */
  char prev_type = OB_SOLID;
  char prev_type_wire = 0;

  char color_type = 0;
  short flag = V3D_SHADING_SPECULAR_HIGHLIGHT | V3D_SHADING_XRAY_WIREFRAME |
               V3D_SHADING_SCENE_LIGHTS_RENDER | V3D_SHADING_SCENE_WORLD_RENDER;

  char light = V3D_LIGHTING_STUDIO;
  char background_type = 0;
  char cavity_type = V3D_SHADING_CAVITY_CURVATURE;
  char wire_color_type = V3D_SHADING_SINGLE_COLOR;

  /** When to preview the compositor output in the viewport. View3DShadingUseCompositor. */
  char use_compositor = 0;

  char _pad = {};

  char studio_light[/*FILE_MAXFILE*/ 256] = "";
  char lookdev_light[/*FILE_MAXFILE*/ 256] = "";
  char matcap[/*FILE_MAXFILE*/ 256] = "";

  float shadow_intensity = 0.5f;
  float single_color[3] = {0.8f, 0.8f, 0.8f};

  float studiolight_rot_z = 0;
  float studiolight_background = 0;
  float studiolight_intensity = 1.0f;
  float studiolight_blur = 0;

  float object_outline_color[3] = {};
  float xray_alpha = 0.5f;
  float xray_alpha_wire = 0.5f;

  float cavity_valley_factor = 1.0f;
  float cavity_ridge_factor = 1.0f;

  float background_color[3] = {0.05f, 0.05f, 0.05f};

  float curvature_ridge_factor = 1.0f;
  float curvature_valley_factor = 1.0f;

  /* Render pass displayed in the viewport. Is an `eScenePassType` where one bit is set */
  int render_pass = SCE_PASS_COMBINED;
  char aov_name[64] = "";

  struct IDProperty *prop = nullptr;
  void *_pad2 = nullptr;
};

/** 3D Viewport Overlay settings. */
struct View3DOverlay {
  int flag = V3D_OVERLAY_VIEWER_ATTRIBUTE | V3D_OVERLAY_SCULPT_SHOW_MASK |
             V3D_OVERLAY_SCULPT_SHOW_FACE_SETS;

  /** Edit mode settings. */
  int edit_flag = V3D_OVERLAY_EDIT_FACES | V3D_OVERLAY_EDIT_SEAMS | V3D_OVERLAY_EDIT_SHARP |
                  V3D_OVERLAY_EDIT_FREESTYLE_EDGE | V3D_OVERLAY_EDIT_FREESTYLE_FACE |
                  V3D_OVERLAY_EDIT_CREASES | V3D_OVERLAY_EDIT_BWEIGHTS;
  float normals_length = 0.1f;
  float normals_constant_screen_size = 7.0f;

  /** Paint mode settings. */
  int paint_flag = 0;

  /** Weight paint mode settings. */
  int wpaint_flag = 0;

  /** Alpha for texture, weight, vertex paint overlay. */
  float texture_paint_mode_opacity = 1.0f;
  float vertex_paint_mode_opacity = 1.0f;
  float weight_paint_mode_opacity = 1.0f;
  float sculpt_mode_mask_opacity = 0.75f;
  float sculpt_mode_face_sets_opacity = 1.0f;
  float viewer_attribute_opacity = 1.0f;

  /** Armature edit/pose mode settings. */
  float xray_alpha_bone = 0.5f;
  float bone_wire_alpha = 1.0f;

  /** Darken Inactive. */
  float fade_alpha = 0.40f;

  /** Other settings. */
  float wireframe_threshold = 1.0f;
  float wireframe_opacity = 1.0f;
  float retopology_offset = 0.01f;

  /** Grease pencil settings. */
  float gpencil_paper_opacity = 0.5f;
  float gpencil_grid_opacity = 0.9f;
  float gpencil_fade_layer = 0;

  /* Grease Pencil canvas settings. */
  float gpencil_grid_color[3] = {0.5f, 0.5f, 0.5f};
  float gpencil_grid_scale[2] = {1.0f, 1.0f};
  float gpencil_grid_offset[2] = {0.0f, 0.0f};
  int gpencil_grid_subdivisions = 4;

  /** Factor for mixing vertex paint with original color */
  float gpencil_vertex_paint_opacity = 1.0f;
  /** Handles display type for curves. */
  int handle_display = CURVE_HANDLE_SELECTED;

  /** Curves sculpt mode settings. */
  float sculpt_curves_cage_opacity = 0;
};

struct View3D_Runtime {
  /** Nkey panel stores stuff here. */
  void *properties_storage = nullptr;
  void (*properties_storage_free)(void *properties_storage) = nullptr;
  /** Runtime only flags. */
  int flag = 0;

  /**
   * The previously calculated selection center.
   * Only use when `flag` #V3D_RUNTIME_OFS_LAST_IS_VALID is set.
   */
  float ofs_last_center[3] = {};

  /* Only used for overlay stats while in local-view. */
  struct SceneStats *local_stats = nullptr;

  /* Times recorded for performance overlay. */
  float last_sync_time = 0;
  float last_submission_time = 0;
};

/** 3D ViewPort Struct. */
struct View3D {
  DNA_DEFINE_CXX_METHODS(View3D)

  struct SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = SPACE_VIEW3D;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  DNA_DEPRECATED float viewquat[4] = {};
  DNA_DEPRECATED float dist = 0;

  /** Size of bundles in reconstructed data. */
  float bundle_size = 0.2f;
  /** Display style for bundle. */
  char bundle_drawtype = OB_PLAINAXES;

  DNA_DEPRECATED char drawtype = 0;

  char _pad3[1] = {};

  /** Multiview current eye - for internal use. */
  char multiview_eye = 0;

  int object_type_exclude_viewport = 0;
  int object_type_exclude_select = 0;

  DNA_DEPRECATED short persp = 0;
  DNA_DEPRECATED short view = 0;

  struct Object *camera = nullptr, *ob_center = nullptr;
  rctf render_border = {};

  /** Allocated backup of itself while in local-view. */
  struct View3D *localvd = nullptr;

  /** Optional string for armature bone to define center. */
  char ob_center_bone[/*MAXBONENAME*/ 64] = "";

  unsigned short local_view_uid = 0;
  char _pad6[2] = {};
  DNA_DEPRECATED int layact = 0;
  unsigned short local_collections_uid = 0;
  short _pad7[2] = {};

  short debug_flag = 0;

  /** Optional bool for 3d cursor to define center. */
  short ob_center_cursor = 0;
  short scenelock = true;
  short gp_flag = V3D_GP_SHOW_EDIT_LINES;
  short flag = V3D_SELECT_OUTLINE;
  int flag2 = V3D_SHOW_RECONSTRUCTION | V3D_SHOW_ANNOTATION | V3D_SHOW_VIEWER |
              V3D_SHOW_CAMERA_GUIDES | V3D_SHOW_CAMERA_PASSEPARTOUT;

  float lens = 50.0f, grid = 1.0f;
  float clip_start = 0.01f, clip_end = 1000.0f;
  float vignette_aperture = 0;
  DNA_DEPRECATED float ofs[2] = {};

  char _pad[1] = {};

  /** Transform gizmo info. */
  /** #V3D_GIZMO_SHOW_* */
  char gizmo_flag = 0;

  char gizmo_show_object = 0;
  char gizmo_show_armature = 0;
  char gizmo_show_empty = 0;
  char gizmo_show_light = 0;
  char gizmo_show_camera = 0;

  char gridflag = V3D_SHOW_X | V3D_SHOW_Y | V3D_SHOW_FLOOR | V3D_SHOW_ORTHO_GRID;

  short gridlines = 16;
  /** Number of subdivisions in the grid between each highlighted grid line. */
  short gridsubdiv = 10;

  /** Actually only used to define the opacity of the grease pencil vertex in edit mode. */
  float vertex_opacity = 1.0f;

  /* XXX deprecated? */
  /** Grease-Pencil Data (annotation layers). */
  DNA_DEPRECATED struct bGPdata *gpd = nullptr;

  /** Stereoscopy settings. */
  short stereo3d_flag = V3D_S3D_DISPPLANE;
  char stereo3d_camera = STEREO_3D_ID;
  char _pad4 = {};
  float stereo3d_convergence_factor = 0;
  float stereo3d_volume_alpha = 0.05f;
  float stereo3d_convergence_alpha = 0.15f;

  /** Display settings. */
  View3DShading shading;
  View3DOverlay overlay;

  /** Path to the viewer node that is currently previewed. This is retrieved from the workspace. */
  ViewerPath viewer_path;

  /** Runtime evaluation data (keep last). */
  View3D_Runtime runtime;
};

#define RV3D_CAMZOOM_MIN -30
#define RV3D_CAMZOOM_MAX 600

/** #BKE_screen_view3d_zoom_to_fac() values above */
#define RV3D_CAMZOOM_MIN_FACTOR 0.1657359312880714853f
#define RV3D_CAMZOOM_MAX_FACTOR 44.9852813742385702928f

}  // namespace blender
