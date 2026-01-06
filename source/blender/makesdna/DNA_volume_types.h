/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"

namespace blender {

struct PackedFile;

namespace bke {
struct VolumeRuntime;
}

/** #Volume.flag */
enum {
  VO_DS_EXPAND = (1 << 0),
};

/** #Volume.sequence_mode */
enum VolumeSequenceMode {
  VOLUME_SEQUENCE_CLIP = 0,
  VOLUME_SEQUENCE_EXTEND = 1,
  VOLUME_SEQUENCE_REPEAT = 2,
  VOLUME_SEQUENCE_PING_PONG = 3,
};

/** #VolumeDisplay.wireframe_type */
enum VolumeWireframeType {
  VOLUME_WIREFRAME_NONE = 0,
  VOLUME_WIREFRAME_BOUNDS = 1,
  VOLUME_WIREFRAME_BOXES = 2,
  VOLUME_WIREFRAME_POINTS = 3,
};

/** #VolumeDisplay.wireframe_detail */
enum VolumeWireframeDetail {
  VOLUME_WIREFRAME_COARSE = 0,
  VOLUME_WIREFRAME_FINE = 1,
};

/** #VolumeRender.precision */
enum VolumeRenderPrecision {
  VOLUME_PRECISION_HALF = 0,
  VOLUME_PRECISION_FULL = 1,
  VOLUME_PRECISION_VARIABLE = 2,
};

/** #VolumeRender.space */
enum VolumeRenderSpace {
  VOLUME_SPACE_OBJECT = 0,
  VOLUME_SPACE_WORLD = 1,
};

/** #VolumeDisplay.interpolation_method */
enum VolumeDisplayInterpMethod {
  VOLUME_DISPLAY_INTERP_LINEAR = 0,
  VOLUME_DISPLAY_INTERP_CUBIC = 1,
  VOLUME_DISPLAY_INTERP_CLOSEST = 2,
};

/** #VolumeDisplay.axis_slice_method */
enum AxisAlignedSlicingMethod {
  VOLUME_AXIS_SLICE_FULL = 0,
  VOLUME_AXIS_SLICE_SINGLE = 1,
};

/** #VolumeDisplay.slice_axis */
enum SliceAxis {
  VOLUME_SLICE_AXIS_AUTO = 0,
  VOLUME_SLICE_AXIS_X = 1,
  VOLUME_SLICE_AXIS_Y = 2,
  VOLUME_SLICE_AXIS_Z = 3,
};

struct VolumeDisplay {
  float density = 1.0f;
  int wireframe_type = VOLUME_WIREFRAME_BOXES;
  int wireframe_detail = VOLUME_WIREFRAME_COARSE;
  int interpolation_method = 0;
  int axis_slice_method = 0;
  int slice_axis = 0;
  float slice_depth = 0.5f;
  int _pad[1] = {};
};

struct VolumeRender {
  int precision = VOLUME_PRECISION_HALF;
  int space = VOLUME_SPACE_OBJECT;
  float step_size = 0.0f;
  float clipping = 0.001f;
};

struct Volume {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_VO;
#endif

  ID id;
  struct AnimData *adt = nullptr; /* animation data (must be immediately after id) */

  /* File */
  char filepath[/*FILE_MAX*/ 1024] = "";
  struct PackedFile *packedfile = nullptr;

  /* Sequence */
  char is_sequence = 0;
  char sequence_mode = 0;
  char _pad1[2] = {};
  int frame_start = 1;
  int frame_duration = 0;
  int frame_offset = 0;

  /* Flag */
  int flag = 0;

  /* Grids */
  int active_grid = 0;

  /* Material */
  struct Material **mat = nullptr;
  short totcol = 0;
  short _pad2[3] = {};

  /* Render & Display Settings */
  VolumeRender render;
  VolumeDisplay display;

  /* Velocity field name. */
  char velocity_grid[64] = "";

  char _pad3[3] = {};

  /* Unit of time the velocity vectors are expressed in.
   * This uses the same enumeration values as #CacheFile.velocity_unit. */
  char velocity_unit = 0;

  /* Factor for velocity vector for artistic control. */
  float velocity_scale = 1.0f;

  /* Draw Cache */
  void *batch_cache = nullptr;

  /* Runtime Data */
  bke::VolumeRuntime *runtime = nullptr;
};

/* Only one material supported currently. */
#define VOLUME_MATERIAL_NR 1

}  // namespace blender
