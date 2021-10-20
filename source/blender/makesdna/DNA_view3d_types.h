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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 */

#pragma once

struct BoundBox;
struct Object;
struct RenderEngine;
struct SmoothView3DStore;
struct SpaceLink;
struct ViewDepths;
struct bGPdata;
struct wmTimer;

#include "DNA_defs.h"
#include "DNA_image_types.h"
#include "DNA_listBase.h"
#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_enums.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RegionView3D {

  /** GL_PROJECTION matrix. */
  float winmat[4][4];
  /** GL_MODELVIEW matrix. */
  float viewmat[4][4];
  /** Inverse of viewmat. */
  float viewinv[4][4];
  /** Viewmat*winmat. */
  float persmat[4][4];
  /** Inverse of persmat. */
  float persinv[4][4];
  /** Offset/scale for camera glsl texcoords. */
  float viewcamtexcofac[4];

  /** viewmat/persmat multiplied with object matrix, while drawing and selection. */
  float viewmatob[4][4];
  float persmatob[4][4];

  /** User defined clipping planes. */
  float clip[6][4];
  /** Clip in object space,
   * means we can test for clipping in editmode without first going into worldspace. */
  float clip_local[6][4];
  struct BoundBox *clipbb;

  /** Allocated backup of its self while in local-view. */
  struct RegionView3D *localvd;
  struct RenderEngine *render_engine;

  /** Animated smooth view. */
  struct SmoothView3DStore *sms;
  struct wmTimer *smooth_timer;

  /** Transform gizmo matrix. */
  float twmat[4][4];
  /** min/max dot product on twmat xyz axis. */
  float tw_axis_min[3], tw_axis_max[3];
  float tw_axis_matrix[3][3];

  float gridview DNA_DEPRECATED;

  /** View rotation, must be kept normalized. */
  float viewquat[4];
  /** Distance from 'ofs' along -viewinv[2] vector, where result is negative as is 'ofs'. */
  float dist;
  /** Camera view offsets, 1.0 = viewplane moves entire width/height. */
  float camdx, camdy;
  /** Runtime only. */
  float pixsize;
  /**
   * View center & orbit pivot, negative of worldspace location,
   * also matches -viewinv[3][0:3] in ortho mode.
   */
  float ofs[3];
  /** Viewport zoom on the camera frame, see BKE_screen_view3d_zoom_to_fac. */
  float camzoom;
  /**
   * Check if persp/ortho view, since 'persp' can't be used for this since
   * it can have cameras assigned as well. (only set in #view3d_winmatrix_set)
   */
  char is_persp;
  char persp;
  char view;
  char view_axis_roll;
  char viewlock; /* Should usually be accessed with RV3D_LOCK_FLAGS()! */
  /** Options for runtime only locking (cleared on file read) */
  char runtime_viewlock; /* Should usually be accessed with RV3D_LOCK_FLAGS()! */
  /** Options for quadview (store while out of quad view). */
  char viewlock_quad;
  char _pad[1];
  /** Normalized offset for locked view: (-1, -1) bottom left, (1, 1) upper right. */
  float ofs_lock[2];

  /** XXX can easily get rid of this (Julian). */
  short twdrawflag;
  short rflag;

  /** Last view (use when switching out of camera view). */
  float lviewquat[4];
  /** Lpersp can never be set to 'RV3D_CAMOB'. */
  char lpersp;
  char lview;
  char lview_axis_roll;
  char _pad8[1];

  /** Active rotation from NDOF or elsewhere. */
  float rot_angle;
  float rot_axis[3];
} RegionView3D;

typedef struct View3DCursor {
  float location[3];

  float rotation_quaternion[4];
  float rotation_euler[3];
  float rotation_axis[3], rotation_angle;
  short rotation_mode;

  char _pad[6];
} View3DCursor;

/** 3D Viewport Shading settings. */
typedef struct View3DShading {
  /** Shading type (OB_SOLID, ..). */
  char type;
  /** Runtime, for toggle between rendered viewport. */
  char prev_type;
  char prev_type_wire;

  char color_type;
  short flag;

  char light;
  char background_type;
  char cavity_type;
  char wire_color_type;
  char _pad[2];

  /** FILE_MAXFILE. */
  char studio_light[256];
  /** FILE_MAXFILE. */
  char lookdev_light[256];
  /** FILE_MAXFILE. */
  char matcap[256];

  float shadow_intensity;
  float single_color[3];

  float studiolight_rot_z;
  float studiolight_background;
  float studiolight_intensity;
  float studiolight_blur;

  float object_outline_color[3];
  float xray_alpha;
  float xray_alpha_wire;

  float cavity_valley_factor;
  float cavity_ridge_factor;

  float background_color[3];

  float curvature_ridge_factor;
  float curvature_valley_factor;

  /* Render pass displayed in the viewport. Is an `eScenePassType` where one bit is set */
  int render_pass;
  char aov_name[64];

  struct IDProperty *prop;
  void *_pad2;
} View3DShading;

/** 3D Viewport Overlay settings. */
typedef struct View3DOverlay {
  int flag;

  /** Edit mode settings. */
  int edit_flag;
  float normals_length;
  float normals_constant_screen_size;
  float backwire_opacity;

  /** Paint mode settings. */
  int paint_flag;

  /** Weight paint mode settings. */
  int wpaint_flag;

  /** Alpha for texture, weight, vertex paint overlay. */
  float texture_paint_mode_opacity;
  float vertex_paint_mode_opacity;
  float weight_paint_mode_opacity;
  float sculpt_mode_mask_opacity;
  float sculpt_mode_face_sets_opacity;

  /** Armature edit/pose mode settings. */
  float xray_alpha_bone;

  /** Darken Inactive. */
  float fade_alpha;

  /** Other settings. */
  float wireframe_threshold;
  float wireframe_opacity;

  /** Grease pencil settings. */
  float gpencil_paper_opacity;
  float gpencil_grid_opacity;
  float gpencil_fade_layer;

  /** Factor for mixing vertex paint with original color */
  float gpencil_vertex_paint_opacity;
  /** Handles display type for curves. */
  int handle_display;

  char _pad[4];
} View3DOverlay;

/* View3DOverlay->handle_display */
typedef enum eHandleDisplay {
  /* Display only selected points. */
  CURVE_HANDLE_SELECTED = 0,
  /* Display all handles. */
  CURVE_HANDLE_ALL = 1,
  /* No display handles. */
  CURVE_HANDLE_NONE = 2,
} eHandleDisplay;

typedef struct View3D_Runtime {
  /** Nkey panel stores stuff here. */
  void *properties_storage;
  /** Runtime only flags. */
  int flag;

  char _pad1[4];
  /* Only used for overlay stats while in local-view. */
  struct SceneStats *local_stats;
} View3D_Runtime;

/** 3D ViewPort Struct. */
typedef struct View3D {
  struct SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  float viewquat[4] DNA_DEPRECATED;
  float dist DNA_DEPRECATED;

  /** Size of bundles in reconstructed data. */
  float bundle_size;
  /** Display style for bundle. */
  char bundle_drawtype;

  char drawtype DNA_DEPRECATED;

  char _pad3[1];

  /** Multiview current eye - for internal use. */
  char multiview_eye;

  int object_type_exclude_viewport;
  int object_type_exclude_select;

  short persp DNA_DEPRECATED;
  short view DNA_DEPRECATED;

  struct Object *camera, *ob_center;
  rctf render_border;

  /** Allocated backup of its self while in local-view. */
  struct View3D *localvd;

  /** Optional string for armature bone to define center, MAXBONENAME. */
  char ob_center_bone[64];

  unsigned short local_view_uuid;
  char _pad6[2];
  int layact DNA_DEPRECATED;
  unsigned short local_collections_uuid;
  short _pad7[3];

  /** Optional bool for 3d cursor to define center. */
  short ob_center_cursor;
  short scenelock;
  short gp_flag;
  short flag;
  int flag2;

  float lens, grid;
  float clip_start, clip_end;
  float ofs[3] DNA_DEPRECATED;

  char _pad[1];

  /** Transform gizmo info. */
  /** #V3D_GIZMO_SHOW_* */
  char gizmo_flag;

  char gizmo_show_object;
  char gizmo_show_armature;
  char gizmo_show_empty;
  char gizmo_show_light;
  char gizmo_show_camera;

  char gridflag;

  short gridlines;
  /** Number of subdivisions in the grid between each highlighted grid line. */
  short gridsubdiv;

  /** Actually only used to define the opacity of the grease pencil vertex in edit mode. */
  float vertex_opacity;

  /* XXX deprecated? */
  /** Grease-Pencil Data (annotation layers). */
  struct bGPdata *gpd DNA_DEPRECATED;

  /** Stereoscopy settings. */
  short stereo3d_flag;
  char stereo3d_camera;
  char _pad4;
  float stereo3d_convergence_factor;
  float stereo3d_volume_alpha;
  float stereo3d_convergence_alpha;

  /** Display settings. */
  View3DShading shading;
  View3DOverlay overlay;

  /** Runtime evaluation data (keep last). */
  View3D_Runtime runtime;
} View3D;

/** #View3D.stereo3d_flag */
#define V3D_S3D_DISPCAMERAS (1 << 0)
#define V3D_S3D_DISPPLANE (1 << 1)
#define V3D_S3D_DISPVOLUME (1 << 2)

/** #View3D.flag */
#define V3D_LOCAL_COLLECTIONS (1 << 0)
#define V3D_FLAG_UNUSED_1 (1 << 1) /* cleared */
#define V3D_HIDE_HELPLINES (1 << 2)
#define V3D_FLAG_UNUSED_2 (1 << 3) /* cleared */
#define V3D_XR_SESSION_MIRROR (1 << 4)
#define V3D_XR_SESSION_SURFACE (1 << 5)

#define V3D_FLAG_UNUSED_10 (1 << 10) /* cleared */
#define V3D_SELECT_OUTLINE (1 << 11)
#define V3D_FLAG_UNUSED_12 (1 << 12) /* cleared */
#define V3D_GLOBAL_STATS (1 << 13)
#define V3D_DRAW_CENTERS (1 << 15)

/** #View3D_Runtime.flag */
enum {
  /** The 3D view which the XR session was created in is flagged with this. */
  V3D_RUNTIME_XR_SESSION_ROOT = (1 << 0),
  /** Some operators override the depth buffer for dedicated occlusion operations. */
  V3D_RUNTIME_DEPTHBUF_OVERRIDDEN = (1 << 1),
};

/** #RegionView3D.persp */
#define RV3D_ORTHO 0
#define RV3D_PERSP 1
#define RV3D_CAMOB 2

/** #RegionView3D.rflag */
#define RV3D_CLIPPING (1 << 2)
#define RV3D_NAVIGATING (1 << 3)
#define RV3D_GPULIGHT_UPDATE (1 << 4)
#define RV3D_PAINTING (1 << 5)
/*#define RV3D_IS_GAME_ENGINE       (1 << 5) */ /* UNUSED */
/**
 * Disable zbuffer offset, skip calls to #ED_view3d_polygon_offset.
 * Use when precise surface depth is needed and picking bias isn't, see T45434).
 */
#define RV3D_ZOFFSET_DISABLED 64

/** #RegionView3D.viewlock */
enum {
  RV3D_LOCK_ROTATION = (1 << 0),
  RV3D_BOXVIEW = (1 << 1),
  RV3D_BOXCLIP = (1 << 2),
  RV3D_LOCK_LOCATION = (1 << 3),
  RV3D_LOCK_ZOOM_AND_DOLLY = (1 << 4),

  RV3D_LOCK_ANY_TRANSFORM = (RV3D_LOCK_LOCATION | RV3D_LOCK_ROTATION | RV3D_LOCK_ZOOM_AND_DOLLY),
};

/* Bitwise OR of the regular lock-flags with runtime only lock-flags. */
#define RV3D_LOCK_FLAGS(rv3d) ((rv3d)->viewlock | ((rv3d)->runtime_viewlock))

/** #RegionView3D.viewlock_quad */
#define RV3D_VIEWLOCK_INIT (1 << 7)

/** #RegionView3D.view */
#define RV3D_VIEW_USER 0
#define RV3D_VIEW_FRONT 1
#define RV3D_VIEW_BACK 2
#define RV3D_VIEW_LEFT 3
#define RV3D_VIEW_RIGHT 4
#define RV3D_VIEW_TOP 5
#define RV3D_VIEW_BOTTOM 6
#define RV3D_VIEW_CAMERA 8

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

#define RV3D_CLIPPING_ENABLED(v3d, rv3d) \
  ((rv3d) && (v3d) && ((rv3d)->rflag & RV3D_CLIPPING) && \
   ELEM((v3d)->shading.type, OB_WIRE, OB_SOLID) && (rv3d)->clipbb)

/** #View3D.flag2 (int) */
#define V3D_HIDE_OVERLAYS (1 << 2)
#define V3D_FLAG2_UNUSED_3 (1 << 3) /* cleared */
#define V3D_SHOW_ANNOTATION (1 << 4)
#define V3D_LOCK_CAMERA (1 << 5)
#define V3D_FLAG2_UNUSED_6 (1 << 6) /* cleared */
#define V3D_SHOW_RECONSTRUCTION (1 << 7)
#define V3D_SHOW_CAMERAPATH (1 << 8)
#define V3D_SHOW_BUNDLENAME (1 << 9)
#define V3D_FLAG2_UNUSED_10 (1 << 10) /* cleared */
#define V3D_RENDER_BORDER (1 << 11)
#define V3D_FLAG2_UNUSED_12 (1 << 12) /* cleared */
#define V3D_FLAG2_UNUSED_13 (1 << 13) /* cleared */
#define V3D_FLAG2_UNUSED_14 (1 << 14) /* cleared */
#define V3D_FLAG2_UNUSED_15 (1 << 15) /* cleared */
#define V3D_XR_SHOW_CONTROLLERS (1 << 16)
#define V3D_XR_SHOW_CUSTOM_OVERLAYS (1 << 17)

/** #View3D.gp_flag (short) */
#define V3D_GP_FADE_OBJECTS (1 << 0) /* Fade all non GP objects */
#define V3D_GP_SHOW_GRID (1 << 1)    /* Activate paper grid */
#define V3D_GP_SHOW_EDIT_LINES (1 << 2)
#define V3D_GP_SHOW_MULTIEDIT_LINES (1 << 3)
#define V3D_GP_SHOW_ONION_SKIN (1 << 4)       /* main switch at view level */
#define V3D_GP_FADE_NOACTIVE_LAYERS (1 << 5)  /* fade layers not active */
#define V3D_GP_FADE_NOACTIVE_GPENCIL (1 << 6) /* Fade other GPencil objects */
#define V3D_GP_SHOW_STROKE_DIRECTION (1 << 7) /* Show Strokes Directions */
#define V3D_GP_SHOW_MATERIAL_NAME (1 << 8)    /* Show Material names */
#define V3D_GP_SHOW_GRID_XRAY (1 << 9)        /* Show Canvas Grid on Top */

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
};

/** #View3DOverlay.edit_flag */
enum {
  V3D_OVERLAY_EDIT_VERT_NORMALS = (1 << 0),
  V3D_OVERLAY_EDIT_LOOP_NORMALS = (1 << 1),
  V3D_OVERLAY_EDIT_FACE_NORMALS = (1 << 2),

  V3D_OVERLAY_EDIT_OCCLUDE_WIRE = (1 << 3),

  V3D_OVERLAY_EDIT_WEIGHT = (1 << 4),

  V3D_OVERLAY_EDIT_EDGES = (1 << 5),
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
#define V3D_SHOW_FLOOR (1 << 0)
#define V3D_SHOW_X (1 << 1)
#define V3D_SHOW_Y (1 << 2)
#define V3D_SHOW_Z (1 << 3)
#define V3D_SHOW_ORTHO_GRID (1 << 4)

/** #TransformOrientationSlot.type */
enum {
  V3D_ORIENT_GLOBAL = 0,
  V3D_ORIENT_LOCAL = 1,
  V3D_ORIENT_NORMAL = 2,
  V3D_ORIENT_VIEW = 3,
  V3D_ORIENT_GIMBAL = 4,
  V3D_ORIENT_CURSOR = 5,
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

#define RV3D_CAMZOOM_MIN -30
#define RV3D_CAMZOOM_MAX 600

/** #BKE_screen_view3d_zoom_to_fac() values above */
#define RV3D_CAMZOOM_MIN_FACTOR 0.1657359312880714853f
#define RV3D_CAMZOOM_MAX_FACTOR 44.9852813742385702928f

#ifdef __cplusplus
}
#endif
