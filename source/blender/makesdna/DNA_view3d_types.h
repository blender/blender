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

#ifndef __DNA_VIEW3D_TYPES_H__
#define __DNA_VIEW3D_TYPES_H__

struct BoundBox;
struct GPUViewport;
struct Image;
struct Material;
struct MovieClip;
struct MovieClipUser;
struct Object;
struct RenderEngine;
struct SmoothView3DStore;
struct SpaceLink;
struct ViewDepths;
struct bGPdata;
struct wmTimer;

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_gpu_types.h"

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

  /** Allocated backup of its self while in localview. */
  struct RegionView3D *localvd;
  struct RenderEngine *render_engine;
  struct ViewDepths *depths;

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
   * Check if persp/ortho view, since 'persp' cant be used for this since
   * it can have cameras assigned as well. (only set in #view3d_winmatrix_set)
   */
  char is_persp;
  char persp;
  char view;
  char viewlock;
  /** Options for quadview (store while out of quad view). */
  char viewlock_quad;
  char _pad[3];
  /** Normalized offset for locked view: (-1, -1) bottom left, (1, 1) upper right. */
  float ofs_lock[2];

  /** XXX can easily get rid of this (Julian). */
  short twdrawflag;
  short rflag;

  /** Last view (use when switching out of camera view). */
  float lviewquat[4];
  /** Lpersp can never be set to 'RV3D_CAMOB'. */
  short lpersp, lview;

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
  /** Shading type (VIEW3D_SHADE_SOLID, ..). */
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
  char _pad[6];

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

  float object_outline_color[3];
  float xray_alpha;
  float xray_alpha_wire;

  float cavity_valley_factor;
  float cavity_ridge_factor;

  float background_color[3];

  float curvature_ridge_factor;
  float curvature_valley_factor;

} View3DShading;

/** 3D Viewport Overlay settings. */
typedef struct View3DOverlay {
  int flag;

  /** Edit mode settings. */
  int edit_flag;
  float normals_length;
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

  /** Armature edit/pose mode settings. */
  int _pad3;
  float xray_alpha_bone;

  /** Other settings. */
  float wireframe_threshold;

  /** Grease pencil settings. */
  float gpencil_paper_opacity;
  float gpencil_grid_opacity;
  float gpencil_fade_layer;

} View3DOverlay;

typedef struct View3D_Runtime {
  /** Nkey panel stores stuff here. */
  void *properties_storage;
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

  struct Object *camera, *ob_centre;
  rctf render_border;

  /** Allocated backup of its self while in localview. */
  struct View3D *localvd;

  /** Optional string for armature bone to define center, MAXBONENAME. */
  char ob_centre_bone[64];

  unsigned short local_view_uuid;
  char _pad6[2];
  int layact DNA_DEPRECATED;

  /** Optional bool for 3d cursor to define center. */
  short ob_centre_cursor;
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

  /* note, 'fx_settings.dof' is currently _not_ allocated,
   * instead set (temporarily) from camera */
  struct GPUFXSettings fx_settings;

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
#define V3D_FLAG_UNUSED_0 (1 << 0) /* cleared */
#define V3D_FLAG_UNUSED_1 (1 << 1) /* cleared */
#define V3D_HIDE_HELPLINES (1 << 2)
#define V3D_INVALID_BACKBUF (1 << 3)

#define V3D_FLAG_UNUSED_10 (1 << 10) /* cleared */
#define V3D_SELECT_OUTLINE (1 << 11)
#define V3D_FLAG_UNUSED_12 (1 << 12) /* cleared */
#define V3D_GLOBAL_STATS (1 << 13)
#define V3D_DRAW_CENTERS (1 << 15)

/** #RegionView3D.persp */
#define RV3D_ORTHO 0
#define RV3D_PERSP 1
#define RV3D_CAMOB 2

/** #RegionView3D.rflag */
#define RV3D_CLIPPING (1 << 2)
#define RV3D_NAVIGATING (1 << 3)
#define RV3D_GPULIGHT_UPDATE (1 << 4)
/*#define RV3D_IS_GAME_ENGINE       (1 << 5) */ /* UNUSED */
/**
 * Disable zbuffer offset, skip calls to #ED_view3d_polygon_offset.
 * Use when precise surface depth is needed and picking bias isn't, see T45434).
 */
#define RV3D_ZOFFSET_DISABLED 64

/** #RegionView3D.viewlock */
#define RV3D_LOCKED (1 << 0)
#define RV3D_BOXVIEW (1 << 1)
#define RV3D_BOXCLIP (1 << 2)
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

/** #View3D.gp_flag (short) */
#define V3D_GP_SHOW_PAPER (1 << 0) /* Activate paper to cover all viewport */
#define V3D_GP_SHOW_GRID (1 << 1)  /* Activate paper grid */
#define V3D_GP_SHOW_EDIT_LINES (1 << 2)
#define V3D_GP_SHOW_MULTIEDIT_LINES (1 << 3)
#define V3D_GP_SHOW_ONION_SKIN (1 << 4)      /* main switch at view level */
#define V3D_GP_FADE_NOACTIVE_LAYERS (1 << 5) /* fade layers not active */

/** #View3DShading.light */
enum {
  V3D_LIGHTING_FLAT = 0,
  V3D_LIGHTING_STUDIO = 1,
  V3D_LIGHTING_MATCAP = 2,
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
  V3D_SHADING_XRAY_BONE = (1 << 8),
  V3D_SHADING_WORLD_ORIENTATION = (1 << 9),
  V3D_SHADING_BACKFACE_CULLING = (1 << 10),
  V3D_SHADING_DEPTH_OF_FIELD = (1 << 11),
};

/** #View3DShading.color_type */
enum {
  V3D_SHADING_MATERIAL_COLOR = 0,
  V3D_SHADING_RANDOM_COLOR = 1,
  V3D_SHADING_SINGLE_COLOR = 2,
  V3D_SHADING_TEXTURE_COLOR = 3,
  V3D_SHADING_OBJECT_COLOR = 4,
  V3D_SHADING_VERTEX_COLOR = 5,
};

/** #View3DShading.background_type */
enum {
  V3D_SHADING_BACKGROUND_THEME = 0,
  V3D_SHADING_BACKGROUND_WORLD = 1,
  V3D_SHADING_BACKGROUND_VIEWPORT = 2,
};

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

  V3D_OVERLAY_EDIT_CU_HANDLES = (1 << 20),
  V3D_OVERLAY_EDIT_CU_NORMALS = (1 << 21),
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

/** #View3d.gridflag */
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

/** Settings for offscreen rendering */
enum {
  V3D_OFSDRAW_NONE = (0),
  V3D_OFSDRAW_USE_FULL_SAMPLE = (1 << 0),
  V3D_OFSDRAW_SHOW_ANNOTATION = (1 << 1),
  V3D_OFSDRAW_OVERRIDE_SCENE_SETTINGS = (1 << 2),
};

#define RV3D_CAMZOOM_MIN -30
#define RV3D_CAMZOOM_MAX 600

/** #BKE_screen_view3d_zoom_to_fac() values above */
#define RV3D_CAMZOOM_MIN_FACTOR 0.1657359312880714853f
#define RV3D_CAMZOOM_MAX_FACTOR 44.9852813742385702928f

#endif
