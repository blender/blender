/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GPU_SHADER
#  pragma once

#  include "GPU_shader.h"
#  include "GPU_shader_shared_utils.h"

typedef struct ViewInfos ViewInfos;
typedef struct ObjectMatrices ObjectMatrices;
typedef struct ObjectInfos ObjectInfos;
typedef struct VolumeInfos VolumeInfos;
typedef struct CurvesInfos CurvesInfos;
typedef struct DrawCommand DrawCommand;
typedef struct DrawCommandIndexed DrawCommandIndexed;
typedef struct DispatchCommand DispatchCommand;
typedef struct DRWDebugPrintBuffer DRWDebugPrintBuffer;
typedef struct DRWDebugVert DRWDebugVert;
typedef struct DRWDebugDrawBuffer DRWDebugDrawBuffer;
#endif

#define DRW_SHADER_SHARED_H

#define DRW_RESOURCE_CHUNK_LEN 512

/* Define the maximum number of grid we allow in a volume UBO. */
#define DRW_GRID_PER_VOLUME_MAX 16

/* Define the maximum number of attribute we allow in a curves UBO.
 * This should be kept in sync with `GPU_ATTR_MAX` */
#define DRW_ATTRIBUTE_PER_CURVES_MAX 15

struct ViewInfos {
  /* View matrices */
  float4x4 persmat;
  float4x4 persinv;
  float4x4 viewmat;
  float4x4 viewinv;
  float4x4 winmat;
  float4x4 wininv;

  float4 clip_planes[6];
  float4 viewvecs[2];
  /* Should not be here. Not view dependent (only main view). */
  float4 viewcamtexcofac;

  float2 viewport_size;
  float2 viewport_size_inverse;

  /** Frustum culling data. */
  /** NOTE: vec3 arrays are padded to vec4. */
  float4 frustum_corners[8];
  float4 frustum_planes[6];

  /** For debugging purpose */
  /* Mouse pixel. */
  int2 mouse_pixel;

  int2 _pad0;
};
BLI_STATIC_ASSERT_ALIGN(ViewInfos, 16)

/* Do not override old definitions if the shader uses this header but not shader info. */
#ifdef USE_GPU_SHADER_CREATE_INFO
/* TODO(@fclem): Mass rename. */
#  define ViewProjectionMatrix drw_view.persmat
#  define ViewProjectionMatrixInverse drw_view.persinv
#  define ViewMatrix drw_view.viewmat
#  define ViewMatrixInverse drw_view.viewinv
#  define ProjectionMatrix drw_view.winmat
#  define ProjectionMatrixInverse drw_view.wininv
#  define clipPlanes drw_view.clip_planes
#  define ViewVecs drw_view.viewvecs
#  define CameraTexCoFactors drw_view.viewcamtexcofac
#endif

struct ObjectMatrices {
  float4x4 drw_modelMatrix;
  float4x4 drw_modelMatrixInverse;
};
BLI_STATIC_ASSERT_ALIGN(ViewInfos, 16)

struct ObjectInfos {
  float4 drw_OrcoTexCoFactors[2];
  float4 drw_ObjectColor;
  float4 drw_Infos;
};
BLI_STATIC_ASSERT_ALIGN(ViewInfos, 16)

struct VolumeInfos {
  /* Object to grid-space. */
  float4x4 grids_xform[DRW_GRID_PER_VOLUME_MAX];
  /* NOTE: vec4 for alignment. Only float3 needed. */
  float4 color_mul;
  float density_scale;
  float temperature_mul;
  float temperature_bias;
  float _pad;
};
BLI_STATIC_ASSERT_ALIGN(VolumeInfos, 16)

struct CurvesInfos {
  /* Per attribute scope, follows loading order.
   * NOTE: uint as bool in GLSL is 4 bytes.
   * NOTE: GLSL pad arrays of scalar to 16 bytes (std140). */
  uint4 is_point_attribute[DRW_ATTRIBUTE_PER_CURVES_MAX];
};
BLI_STATIC_ASSERT_ALIGN(CurvesInfos, 16)

#define OrcoTexCoFactors (drw_infos[resource_id].drw_OrcoTexCoFactors)
#define ObjectInfo (drw_infos[resource_id].drw_Infos)
#define ObjectColor (drw_infos[resource_id].drw_ObjectColor)

/* Indirect commands structures. */

struct DrawCommand {
  uint v_count;
  uint i_count;
  uint v_first;
  uint i_first;
};
BLI_STATIC_ASSERT_ALIGN(DrawCommand, 16)

struct DrawCommandIndexed {
  uint v_count;
  uint i_count;
  uint v_first;
  uint base_index;
  uint i_first;
  uint _pad0;
  uint _pad1;
  uint _pad2;
};
BLI_STATIC_ASSERT_ALIGN(DrawCommandIndexed, 16)

struct DispatchCommand {
  uint num_groups_x;
  uint num_groups_y;
  uint num_groups_z;
  uint _pad0;
};
BLI_STATIC_ASSERT_ALIGN(DispatchCommand, 16)

/* -------------------------------------------------------------------- */
/** \name Debug print
 * \{ */

/* Take the header (DrawCommand) into account. */
#define DRW_DEBUG_PRINT_MAX (8 * 1024) - 4
/* NOTE: Cannot be more than 255 (because of column encoding). */
#define DRW_DEBUG_PRINT_WORD_WRAP_COLUMN 120u

/* The debug print buffer is laid-out as the following struct.
 * But we use plain array in shader code instead because of driver issues. */
struct DRWDebugPrintBuffer {
  DrawCommand command;
  /** Each character is encoded as 3 `uchar` with char_index, row and column position. */
  uint char_array[DRW_DEBUG_PRINT_MAX];
};
BLI_STATIC_ASSERT_ALIGN(DRWDebugPrintBuffer, 16)

/* Use number of char as vertex count. Equivalent to `DRWDebugPrintBuffer.command.v_count`. */
#define drw_debug_print_cursor drw_debug_print_buf[0]
/* Reuse first instance as row index as we don't use instancing. Equivalent to
 * `DRWDebugPrintBuffer.command.i_first`. */
#define drw_debug_print_row_shared drw_debug_print_buf[3]

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug draw shapes
 * \{ */

struct DRWDebugVert {
  /* This is a weird layout, but needed to be able to use DRWDebugVert as
   * a DrawCommand and avoid alignment issues. See drw_debug_verts_buf[] definition. */
  uint pos0;
  uint pos1;
  uint pos2;
  uint color;
};
BLI_STATIC_ASSERT_ALIGN(DRWDebugVert, 16)

/* Take the header (DrawCommand) into account. */
#define DRW_DEBUG_DRAW_VERT_MAX (64 * 1024) - 1

/* The debug draw buffer is laid-out as the following struct.
 * But we use plain array in shader code instead because of driver issues. */
struct DRWDebugDrawBuffer {
  DrawCommand command;
  DRWDebugVert verts[DRW_DEBUG_DRAW_VERT_MAX];
};
BLI_STATIC_ASSERT_ALIGN(DRWDebugPrintBuffer, 16)

/* Equivalent to `DRWDebugDrawBuffer.command.v_count`. */
#define drw_debug_draw_v_count drw_debug_verts_buf[0].pos0

/** \} */
