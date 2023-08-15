/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GPU_SHADER
#  pragma once

#  include "GPU_shader.h"
#  include "GPU_shader_shared_utils.h"
#  include "draw_defines.h"

typedef struct ViewCullingData ViewCullingData;
typedef struct ViewMatrices ViewMatrices;
typedef struct ObjectMatrices ObjectMatrices;
typedef struct ObjectInfos ObjectInfos;
typedef struct ObjectBounds ObjectBounds;
typedef struct VolumeInfos VolumeInfos;
typedef struct CurvesInfos CurvesInfos;
typedef struct ObjectAttribute ObjectAttribute;
typedef struct LayerAttribute LayerAttribute;
typedef struct DrawCommand DrawCommand;
typedef struct DispatchCommand DispatchCommand;
typedef struct DRWDebugPrintBuffer DRWDebugPrintBuffer;
typedef struct DRWDebugVert DRWDebugVert;
typedef struct DRWDebugDrawBuffer DRWDebugDrawBuffer;
typedef struct FrustumCorners FrustumCorners;
typedef struct FrustumPlanes FrustumPlanes;

/* __cplusplus is true when compiling with MSL. */
#  if defined(__cplusplus) && !defined(GPU_SHADER)
/* C++ only forward declarations. */
struct Object;
struct ViewLayer;
struct GPUUniformAttr;
struct GPULayerAttr;

namespace blender::draw {

struct ObjectRef;

}  // namespace blender::draw

#  else /* __cplusplus */
/* C only forward declarations. */
typedef enum eObjectInfoFlag eObjectInfoFlag;

#  endif
#endif

#if defined(__cplusplus) && !defined(GPU_SHADER)
extern "C" {
#endif

#define DRW_SHADER_SHARED_H

#define DRW_RESOURCE_CHUNK_LEN 512

/* Define the maximum number of grid we allow in a volume UBO. */
#define DRW_GRID_PER_VOLUME_MAX 16

/* Define the maximum number of attribute we allow in a curves UBO.
 * This should be kept in sync with `GPU_ATTR_MAX` */
#define DRW_ATTRIBUTE_PER_CURVES_MAX 15

/* -------------------------------------------------------------------- */
/** \name Views
 * \{ */

/**
 * The maximum of indexable views is dictated by:
 * - The UBO limit (16KiB) of the ViewMatrices container.
 * - The maximum resource index supported for shaders using multi-view (see DRW_VIEW_SHIFT).
 */
#define DRW_VIEW_MAX 64

#ifndef DRW_VIEW_LEN
/* Single-view case (default). */
#  define drw_view_id 0
#  define DRW_VIEW_LEN 1
#  define DRW_VIEW_SHIFT 0
#  define DRW_VIEW_FROM_RESOURCE_ID
#else

/* Multi-view case. */
/** This should be already defined at shaderCreateInfo level. */
// #  define DRW_VIEW_LEN 64
/** Global that needs to be set correctly in each shader stage. */
uint drw_view_id = 0;
/**
 * In order to reduce the memory requirements, the view id is merged with resource id to avoid
 * doubling the memory required only for view indexing.
 */
/** \note This is simply log2(DRW_VIEW_LEN) but making sure it is optimized out. */
#  define DRW_VIEW_SHIFT \
    ((DRW_VIEW_LEN > 32) ? 6 : \
     (DRW_VIEW_LEN > 16) ? 5 : \
     (DRW_VIEW_LEN > 8)  ? 4 : \
     (DRW_VIEW_LEN > 4)  ? 3 : \
     (DRW_VIEW_LEN > 2)  ? 2 : \
                           1)
#  define DRW_VIEW_MASK ~(0xFFFFFFFFu << DRW_VIEW_SHIFT)
#  define DRW_VIEW_FROM_RESOURCE_ID drw_view_id = (uint(drw_ResourceID) & DRW_VIEW_MASK)
#endif

struct FrustumCorners {
  float4 corners[8];
};
BLI_STATIC_ASSERT_ALIGN(FrustumCorners, 16)

struct FrustumPlanes {
  /* [0] left
   * [1] right
   * [2] bottom
   * [3] top
   * [4] near
   * [5] far */
  float4 planes[6];
};
BLI_STATIC_ASSERT_ALIGN(FrustumPlanes, 16)

struct ViewCullingData {
  /** \note vec3 array padded to vec4. */
  /** Frustum corners. */
  FrustumCorners frustum_corners;
  FrustumPlanes frustum_planes;
  float4 bound_sphere;
};
BLI_STATIC_ASSERT_ALIGN(ViewCullingData, 16)

struct ViewMatrices {
  float4x4 viewmat;
  float4x4 viewinv;
  float4x4 winmat;
  float4x4 wininv;
};
BLI_STATIC_ASSERT_ALIGN(ViewMatrices, 16)

/* Do not override old definitions if the shader uses this header but not shader info. */
#ifdef USE_GPU_SHADER_CREATE_INFO
/* TODO(@fclem): Mass rename. */
#  define ViewMatrix drw_view.viewmat
#  define ViewMatrixInverse drw_view.viewinv
#  define ProjectionMatrix drw_view.winmat
#  define ProjectionMatrixInverse drw_view.wininv
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug draw shapes
 * \{ */

struct ObjectMatrices {
  float4x4 model;
  float4x4 model_inverse;

#if !defined(GPU_SHADER) && defined(__cplusplus)
  void sync(const Object &object);
  void sync(const float4x4 &model_matrix);
#endif
};
BLI_STATIC_ASSERT_ALIGN(ObjectMatrices, 16)

enum eObjectInfoFlag {
  OBJECT_SELECTED = (1u << 0u),
  OBJECT_FROM_DUPLI = (1u << 1u),
  OBJECT_FROM_SET = (1u << 2u),
  OBJECT_ACTIVE = (1u << 3u),
  OBJECT_NEGATIVE_SCALE = (1u << 4u),
  /* Avoid skipped info to change culling. */
  OBJECT_NO_INFO = ~OBJECT_NEGATIVE_SCALE
};

struct ObjectInfos {
#if defined(GPU_SHADER) && !defined(DRAW_FINALIZE_SHADER)
  /* TODO Rename to struct member for glsl too. */
  float4 orco_mul_bias[2];
  float4 ob_color;
  float4 infos;
#else
  /** Uploaded as center + size. Converted to mul+bias to local coord. */
  packed_float3 orco_add;
  uint object_attrs_offset;
  packed_float3 orco_mul;
  uint object_attrs_len;

  float4 ob_color;
  uint index;
  uint _pad2;
  float random;
  eObjectInfoFlag flag;
#endif

#if !defined(GPU_SHADER) && defined(__cplusplus)
  void sync();
  void sync(const blender::draw::ObjectRef ref, bool is_active_object);
#endif
};
BLI_STATIC_ASSERT_ALIGN(ObjectInfos, 16)

struct ObjectBounds {
  /**
   * Uploaded as vertex (0, 4, 3, 1) of the bbox in local space, matching XYZ axis order.
   * Then processed by GPU and stored as (0, 4-0, 3-0, 1-0) in world space for faster culling.
   */
  float4 bounding_corners[4];
  /** Bounding sphere derived from the bounding corner. Computed on GPU. */
  float4 bounding_sphere;
  /** Radius of the inscribed sphere derived from the bounding corner. Computed on GPU. */
#define _inner_sphere_radius bounding_corners[3].w

#if !defined(GPU_SHADER) && defined(__cplusplus)
  void sync();
  void sync(Object &ob);
  void sync(const float3 &center, const float3 &size);
#endif
};
BLI_STATIC_ASSERT_ALIGN(ObjectBounds, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object attributes
 * \{ */

struct VolumeInfos {
  /** Object to grid-space. */
  float4x4 grids_xform[DRW_GRID_PER_VOLUME_MAX];
  /** \note vec4 for alignment. Only float3 needed. */
  float4 color_mul;
  float density_scale;
  float temperature_mul;
  float temperature_bias;
  float _pad;
};
BLI_STATIC_ASSERT_ALIGN(VolumeInfos, 16)

struct CurvesInfos {
  /** Per attribute scope, follows loading order.
   * \note uint as bool in GLSL is 4 bytes.
   * \note GLSL pad arrays of scalar to 16 bytes (std140). */
  uint4 is_point_attribute[DRW_ATTRIBUTE_PER_CURVES_MAX];
};
BLI_STATIC_ASSERT_ALIGN(CurvesInfos, 16)

#pragma pack(push, 4)
struct ObjectAttribute {
  /* Workaround the padding cost from alignment requirements.
   * (see GL spec : 7.6.2.2 Standard Uniform Block Layout) */
  float data_x, data_y, data_z, data_w;
  uint hash_code;

#if !defined(GPU_SHADER) && defined(__cplusplus)
  bool sync(const blender::draw::ObjectRef &ref, const GPUUniformAttr &attr);
#endif
};
#pragma pack(pop)
/** \note we only align to 4 bytes and fetch data manually so make sure
 * C++ compiler gives us the same size. */
BLI_STATIC_ASSERT_ALIGN(ObjectAttribute, 20)

#pragma pack(push, 4)
struct LayerAttribute {
  float4 data;
  uint hash_code;
  uint buffer_length; /* Only in the first record. */
  uint _pad1, _pad2;

#if !defined(GPU_SHADER) && defined(__cplusplus)
  bool sync(Scene *scene, ViewLayer *layer, const GPULayerAttr &attr);
#endif
};
#pragma pack(pop)
BLI_STATIC_ASSERT_ALIGN(LayerAttribute, 32)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Indirect commands structures.
 * \{ */

struct DrawCommand {
  /* TODO(fclem): Rename */
  uint vertex_len;
  uint instance_len;
  uint vertex_first;
#if defined(GPU_SHADER)
  uint base_index;
  /** \note base_index is i_first for non-indexed draw-calls. */
#  define _instance_first_array base_index
#else
  union {
    uint base_index;
    /* Use this instead of instance_first_indexed for non indexed draw calls. */
    uint instance_first_array;
  };
#endif

  uint instance_first_indexed;

  uint _pad0, _pad1, _pad2;
};
BLI_STATIC_ASSERT_ALIGN(DrawCommand, 16)

struct DispatchCommand {
  uint num_groups_x;
  uint num_groups_y;
  uint num_groups_z;
  uint _pad0;
};
BLI_STATIC_ASSERT_ALIGN(DispatchCommand, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug print
 * \{ */

/* Take the header (DrawCommand) into account. */
#define DRW_DEBUG_PRINT_MAX (8 * 1024) - 4
/** \note Cannot be more than 255 (because of column encoding). */
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
/** Offset to the first data. Equal to: `sizeof(DrawCommand) / sizeof(uint)`.
 * This is needed because we bind the whole buffer as a `uint` array. */
#define drw_debug_print_offset 8

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
  /* Named vert_color to avoid global namespace collision with uniform color. */
  uint vert_color;
};
BLI_STATIC_ASSERT_ALIGN(DRWDebugVert, 16)

inline DRWDebugVert debug_vert_make(uint in_pos0, uint in_pos1, uint in_pos2, uint in_vert_color)
{
  DRWDebugVert debug_vert;
  debug_vert.pos0 = in_pos0;
  debug_vert.pos1 = in_pos1;
  debug_vert.pos2 = in_pos2;
  debug_vert.vert_color = in_vert_color;
  return debug_vert;
}

/* Take the header (DrawCommand) into account. */
#define DRW_DEBUG_DRAW_VERT_MAX (64 * 8192) - 1

/* The debug draw buffer is laid-out as the following struct.
 * But we use plain array in shader code instead because of driver issues. */
struct DRWDebugDrawBuffer {
  DrawCommand command;
  DRWDebugVert verts[DRW_DEBUG_DRAW_VERT_MAX];
};
BLI_STATIC_ASSERT_ALIGN(DRWDebugPrintBuffer, 16)

/* Equivalent to `DRWDebugDrawBuffer.command.v_count`. */
#define drw_debug_draw_v_count drw_debug_verts_buf[0].pos0
/** Offset to the first data. Equal to: `sizeof(DrawCommand) / sizeof(DRWDebugVert)`.
 * This is needed because we bind the whole buffer as a `DRWDebugVert` array. */
#define drw_debug_draw_offset 2

/** \} */

#if defined(__cplusplus) && !defined(GPU_SHADER)
}
#endif
