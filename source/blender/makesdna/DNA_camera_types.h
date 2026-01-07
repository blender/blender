/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_math_constants.h"

#include "DNA_ID.h"
#include "DNA_defs.h"
#include "DNA_gpu_types.h"
#include "DNA_image_types.h"
#include "DNA_movieclip_types.h"

namespace blender {

struct AnimData;
struct Object;

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

/* ------------------------------------------- */
/* Stereo Settings */
struct CameraStereoSettings {
  float interocular_distance = 0.065f;
  float convergence_distance = 30.0f * 0.065f;
  short convergence_mode = 0;
  short pivot = 0;
  short flag = 0;
  char _pad[2] = {};
  /* Cut-off angle at which interocular distance start to fade down. */
  float pole_merge_angle_from = DEG2RADF(60.0f);
  /* Cut-off angle at which interocular distance stops to fade down. */
  float pole_merge_angle_to = DEG2RADF(75.0f);
};

/* Background Picture */
struct CameraBGImage {
  struct CameraBGImage *next = nullptr, *prev = nullptr;

  struct Image *ima = nullptr;
  struct ImageUser iuser;
  struct MovieClip *clip = nullptr;
  struct MovieClipUser cuser;
  float offset[2] = {}, scale = 0, rotation = 0;
  float alpha = 0;
  short flag = 0;
  short source = 0;
};

/** Properties for dof effect. */
struct CameraDOFSettings {
  /** Focal distance for depth of field. */
  struct Object *focus_object = nullptr;
  char focus_subtarget[64] = "";
  float focus_distance = 10.0f;
  float aperture_fstop = 2.8f;
  float aperture_rotation = 0;
  float aperture_ratio = 1.0f;
  int aperture_blades = 0;
  short flag = 0;
  char _pad[2] = {};
};

struct Camera_Runtime {
  /* For draw manager. */
  float drw_corners[2][4][2] = {};
  float drw_tria[2][2] = {};
  float drw_depth[2] = {};
  float drw_focusmat[4][4] = {};
  float drw_normalmat[4][4] = {};
};

struct Camera {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_CA;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt = nullptr;

  /** CAM_PERSP, CAM_ORTHO, CAM_PANO or CAM_CUSTOM. */
  char type = 0;
  /** Draw type extra. */
  char dtx = 0;
  short flag = CAM_SHOWPASSEPARTOUT;
  float passepartalpha = 0.5f;
  float clip_start = 0.1f, clip_end = 1000.0f;
  float lens = 50.0f, ortho_scale = 6.0, drawsize = 1.0f;
  float sensor_x = DEFAULT_SENSOR_WIDTH, sensor_y = DEFAULT_SENSOR_HEIGHT;
  float shiftx = 0, shifty = 0;
  DNA_DEPRECATED float dof_distance = 0;

  char sensor_fit = 0;
  char panorama_type = CAM_PANORAMA_FISHEYE_EQUISOLID;
  char _pad[2] = {};

  /* Fish-eye properties. */
  float fisheye_fov = M_PI;
  float fisheye_lens = 10.5f;
  float latitude_min = -0.5f * float(M_PI), latitude_max = 0.5f * float(M_PI);
  float longitude_min = -M_PI, longitude_max = M_PI;
  float fisheye_polynomial_k0 = -1.1735143712967577e-05f;
  float fisheye_polynomial_k1 = -0.019988736953434998f;
  float fisheye_polynomial_k2 = -3.3525322965709175e-06f;
  float fisheye_polynomial_k3 = 3.099275275886036e-06f;
  float fisheye_polynomial_k4 = -2.6064646454854524e-08f;

  /* Central cylindrical range properties. */
  float central_cylindrical_range_u_min = DEG2RADF(-180.0f);
  float central_cylindrical_range_u_max = DEG2RADF(180.0f);
  float central_cylindrical_range_v_min = -1.0f;
  float central_cylindrical_range_v_max = 1.0f;
  float central_cylindrical_radius = 1.0f;
  float _pad2 = {};

  /* Custom Camera properties. */
  struct Text *custom_shader = nullptr;

  char custom_filepath[/*FILE_MAX*/ 1024] = "";

  char custom_bytecode_hash[64] = "";
  char *custom_bytecode = nullptr;
  int custom_mode = 0;
  int _pad3 = {};

  DNA_DEPRECATED struct Object *dof_ob = nullptr;
  DNA_DEPRECATED struct GPUDOFSettings gpu_dof;
  struct CameraDOFSettings dof;

  /* CameraBGImage reference images */
  ListBaseT<CameraBGImage> bg_images = {nullptr, nullptr};

  /* Stereo settings */
  struct CameraStereoSettings stereo;

  /* Compositional guide overlay color */
  float composition_guide_color[4] = {0.5f, 0.5f, 0.5f, 1.0f};

  /** Runtime data (keep last). */
  Camera_Runtime runtime;
};

/* **************** CAMERA ********************* */

}  // namespace blender
