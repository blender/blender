/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"

struct PackedFile;
struct VolumeGridVector;

typedef struct Volume_Runtime {
  /** OpenVDB Grids. */
  struct VolumeGridVector *grids;

  /** Current frame in sequence for evaluated volume. */
  int frame;

  /** Default simplify level for volume grids loaded from files. */
  int default_simplify_level;

  /* Names for scalar grids which would need to be merged to recompose the velocity grid. */
  char velocity_x_grid[64];
  char velocity_y_grid[64];
  char velocity_z_grid[64];
} Volume_Runtime;

typedef struct VolumeDisplay {
  float density;
  int wireframe_type;
  int wireframe_detail;
  int interpolation_method;
  int axis_slice_method;
  int slice_axis;
  float slice_depth;
  int _pad[1];
} VolumeDisplay;

typedef struct VolumeRender {
  int precision;
  int space;
  float step_size;
  float clipping;
} VolumeRender;

typedef struct Volume {
  ID id;
  struct AnimData *adt; /* animation data (must be immediately after id) */

  /* File */
  char filepath[1024]; /* FILE_MAX */
  struct PackedFile *packedfile;

  /* Sequence */
  char is_sequence;
  char sequence_mode;
  char _pad1[2];
  int frame_start;
  int frame_duration;
  int frame_offset;

  /* Flag */
  int flag;

  /* Grids */
  int active_grid;

  /* Material */
  struct Material **mat;
  short totcol;
  short _pad2[3];

  /* Render & Display Settings */
  VolumeRender render;
  VolumeDisplay display;

  /* Velocity field name. */
  char velocity_grid[64];

  char _pad3[3];

  /* Unit of time the velocity vectors are expressed in.
   * This uses the same enumeration values as #CacheFile.velocity_unit. */
  char velocity_unit;

  /* Factor for velocity vector for artistic control. */
  float velocity_scale;

  /* Draw Cache */
  void *batch_cache;

  /* Runtime Data */
  Volume_Runtime runtime;
} Volume;

/** #Volume.flag */
enum {
  VO_DS_EXPAND = (1 << 0),
};

/** #Volume.sequence_mode */
typedef enum VolumeSequenceMode {
  VOLUME_SEQUENCE_CLIP = 0,
  VOLUME_SEQUENCE_EXTEND = 1,
  VOLUME_SEQUENCE_REPEAT = 2,
  VOLUME_SEQUENCE_PING_PONG = 3,
} VolumeSequenceMode;

/** #VolumeDisplay.wireframe_type */
typedef enum VolumeWireframeType {
  VOLUME_WIREFRAME_NONE = 0,
  VOLUME_WIREFRAME_BOUNDS = 1,
  VOLUME_WIREFRAME_BOXES = 2,
  VOLUME_WIREFRAME_POINTS = 3,
} VolumeWireframeType;

/** #VolumeDisplay.wireframe_detail */
typedef enum VolumeWireframeDetail {
  VOLUME_WIREFRAME_COARSE = 0,
  VOLUME_WIREFRAME_FINE = 1,
} VolumeWireframeDetail;

/** #VolumeRender.precision */
typedef enum VolumeRenderPrecision {
  VOLUME_PRECISION_HALF = 0,
  VOLUME_PRECISION_FULL = 1,
  VOLUME_PRECISION_VARIABLE = 2,
} VolumeRenderPrecision;

/** #VolumeRender.space */
typedef enum VolumeRenderSpace {
  VOLUME_SPACE_OBJECT = 0,
  VOLUME_SPACE_WORLD = 1,
} VolumeRenderSpace;

/** #VolumeDisplay.interpolation_method */
typedef enum VolumeDisplayInterpMethod {
  VOLUME_DISPLAY_INTERP_LINEAR = 0,
  VOLUME_DISPLAY_INTERP_CUBIC = 1,
  VOLUME_DISPLAY_INTERP_CLOSEST = 2,
} VolumeDisplayInterpMethod;

/** #VolumeDisplay.axis_slice_method */
typedef enum AxisAlignedSlicingMethod {
  VOLUME_AXIS_SLICE_FULL = 0,
  VOLUME_AXIS_SLICE_SINGLE = 1,
} AxisAlignedSlicingMethod;

/** #VolumeDisplay.slice_axis */
typedef enum SliceAxis {
  VOLUME_SLICE_AXIS_AUTO = 0,
  VOLUME_SLICE_AXIS_X = 1,
  VOLUME_SLICE_AXIS_Y = 2,
  VOLUME_SLICE_AXIS_Z = 3,
} SliceAxis;

/* Only one material supported currently. */
#define VOLUME_MATERIAL_NR 1
