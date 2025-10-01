/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"
#include "DNA_gpu_types.h"
#include "DNA_image_types.h"
#include "DNA_movieclip_types.h"

struct AnimData;
struct Object;

/* ------------------------------------------- */
/* Stereo Settings */
typedef struct CameraStereoSettings {
  float interocular_distance;
  float convergence_distance;
  short convergence_mode;
  short pivot;
  short flag;
  char _pad[2];
  /* Cut-off angle at which interocular distance start to fade down. */
  float pole_merge_angle_from;
  /* Cut-off angle at which interocular distance stops to fade down. */
  float pole_merge_angle_to;
} CameraStereoSettings;

/* Background Picture */
typedef struct CameraBGImage {
  struct CameraBGImage *next, *prev;

  struct Image *ima;
  struct ImageUser iuser;
  struct MovieClip *clip;
  struct MovieClipUser cuser;
  float offset[2], scale, rotation;
  float alpha;
  short flag;
  short source;
} CameraBGImage;

/** Properties for dof effect. */
typedef struct CameraDOFSettings {
  /** Focal distance for depth of field. */
  struct Object *focus_object;
  char focus_subtarget[64];
  float focus_distance;
  float aperture_fstop;
  float aperture_rotation;
  float aperture_ratio;
  int aperture_blades;
  short flag;
  char _pad[2];
} CameraDOFSettings;

typedef struct Camera_Runtime {
  /* For draw manager. */
  float drw_corners[2][4][2];
  float drw_tria[2][2];
  float drw_depth[2];
  float drw_focusmat[4][4];
  float drw_normalmat[4][4];
} Camera_Runtime;

typedef struct Camera {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_CA;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  /** CAM_PERSP, CAM_ORTHO, CAM_PANO or CAM_CUSTOM. */
  char type;
  /** Draw type extra. */
  char dtx;
  short flag;
  float passepartalpha;
  float clip_start, clip_end;
  float lens, ortho_scale, drawsize;
  float sensor_x, sensor_y;
  float shiftx, shifty;
  float dof_distance DNA_DEPRECATED;

  char sensor_fit;
  char panorama_type;
  char _pad[2];

  /* Fish-eye properties. */
  float fisheye_fov;
  float fisheye_lens;
  float latitude_min, latitude_max;
  float longitude_min, longitude_max;
  float fisheye_polynomial_k0;
  float fisheye_polynomial_k1;
  float fisheye_polynomial_k2;
  float fisheye_polynomial_k3;
  float fisheye_polynomial_k4;

  /* Central cylindrical range properties. */
  float central_cylindrical_range_u_min;
  float central_cylindrical_range_u_max;
  float central_cylindrical_range_v_min;
  float central_cylindrical_range_v_max;
  float central_cylindrical_radius;
  float _pad2;

  /* Custom Camera properties. */
  struct Text *custom_shader;

  char custom_filepath[/*FILE_MAX*/ 1024];

  char custom_bytecode_hash[64];
  char *custom_bytecode;
  int custom_mode;
  int _pad3;

  struct Object *dof_ob DNA_DEPRECATED;
  struct GPUDOFSettings gpu_dof DNA_DEPRECATED;
  struct CameraDOFSettings dof;

  /* CameraBGImage reference images */
  struct ListBase bg_images;

  /* Stereo settings */
  struct CameraStereoSettings stereo;

  /* Compositional guide overlay color */
  float composition_guide_color[4];

  /** Runtime data (keep last). */
  Camera_Runtime runtime;
} Camera;

/* **************** CAMERA ********************* */

/* type */
enum {
  CAM_PERSP = 0,
  CAM_ORTHO = 1,
  CAM_PANO = 2,
  CAM_CUSTOM = 3,
};

/* panorama_type */
enum {
  CAM_PANORAMA_EQUIRECTANGULAR = 0,
  CAM_PANORAMA_FISHEYE_EQUIDISTANT = 1,
  CAM_PANORAMA_FISHEYE_EQUISOLID = 2,
  CAM_PANORAMA_MIRRORBALL = 3,
  CAM_PANORAMA_FISHEYE_LENS_POLYNOMIAL = 4,
  CAM_PANORAMA_EQUIANGULAR_CUBEMAP_FACE = 5,
  CAM_PANORAMA_CENTRAL_CYLINDRICAL = 6,
};

/* custom_mode */
enum {
  CAM_CUSTOM_SHADER_INTERNAL = 0,
  CAM_CUSTOM_SHADER_EXTERNAL = 1,
};

/* dtx */
enum {
  CAM_DTX_CENTER = (1 << 0),
  CAM_DTX_CENTER_DIAG = (1 << 1),
  CAM_DTX_THIRDS = (1 << 2),
  CAM_DTX_GOLDEN = (1 << 3),
  CAM_DTX_GOLDEN_TRI_A = (1 << 4),
  CAM_DTX_GOLDEN_TRI_B = (1 << 5),
  CAM_DTX_HARMONY_TRI_A = (1 << 6),
  CAM_DTX_HARMONY_TRI_B = (1 << 7),
};

/* flag */
enum {
  CAM_SHOWLIMITS = (1 << 0),
  CAM_SHOWMIST = (1 << 1),
  CAM_SHOWPASSEPARTOUT = (1 << 2),
  CAM_SHOW_SAFE_MARGINS = (1 << 3),
  CAM_SHOWNAME = (1 << 4),
  CAM_ANGLETOGGLE = (1 << 5),
  CAM_DS_EXPAND = (1 << 6),
#ifdef DNA_DEPRECATED_ALLOW
  CAM_PANORAMA = (1 << 7), /* deprecated */
#endif
  CAM_SHOWSENSOR = (1 << 8),
  CAM_SHOW_SAFE_CENTER = (1 << 9),
  CAM_SHOW_BG_IMAGE = (1 << 10),
};

/* Sensor fit */
enum {
  CAMERA_SENSOR_FIT_AUTO = 0,
  CAMERA_SENSOR_FIT_HOR = 1,
  CAMERA_SENSOR_FIT_VERT = 2,
};

#define DEFAULT_SENSOR_WIDTH 36.0f
#define DEFAULT_SENSOR_HEIGHT 24.0f

/* stereo->convergence_mode */
enum {
  CAM_S3D_OFFAXIS = 0,
  CAM_S3D_PARALLEL = 1,
  CAM_S3D_TOE = 2,
};

/* stereo->pivot */
enum {
  CAM_S3D_PIVOT_LEFT = 0,
  CAM_S3D_PIVOT_RIGHT = 1,
  CAM_S3D_PIVOT_CENTER = 2,
};

/* stereo->flag */
enum {
  CAM_S3D_SPHERICAL = (1 << 0),
  CAM_S3D_POLE_MERGE = (1 << 1),
};

/* CameraBGImage->flag */
/* may want to use 1 for select ? */
enum {
  CAM_BGIMG_FLAG_EXPANDED = (1 << 1),
  CAM_BGIMG_FLAG_CAMERACLIP = (1 << 2),
  CAM_BGIMG_FLAG_DISABLED = (1 << 3),
  CAM_BGIMG_FLAG_FOREGROUND = (1 << 4),

  /* Camera framing options */
  /** Don't stretch to fit the camera view. */
  CAM_BGIMG_FLAG_CAMERA_ASPECT = (1 << 5),
  /** Crop out the image. */
  CAM_BGIMG_FLAG_CAMERA_CROP = (1 << 6),

  /* Axis flip options */
  CAM_BGIMG_FLAG_FLIP_X = (1 << 7),
  CAM_BGIMG_FLAG_FLIP_Y = (1 << 8),

  /* That background image has been inserted in local override (i.e. it can be fully edited!). */
  CAM_BGIMG_FLAG_OVERRIDE_LIBRARY_LOCAL = (1 << 9),
};

/* CameraBGImage->source */
/* may want to use 1 for select? */
enum {
  CAM_BGIMG_SOURCE_IMAGE = 0,
  CAM_BGIMG_SOURCE_MOVIE = 1,
};

/* CameraDOFSettings->flag */
enum {
  CAM_DOF_ENABLED = (1 << 0),
};
