/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifndef GPU_SHADER
#  include "BLI_math_vector.hh"
#  include "GPU_shader.hh"
#  include "GPU_shader_shared_utils.hh"
#  include "draw_defines.hh"

struct ViewCullingData;
struct ViewMatrices;
struct ObjectMatrices;
struct ObjectInfos;
struct ObjectBounds;
struct VolumeInfos;
struct CurvesInfos;
struct ObjectAttribute;
struct LayerAttribute;
struct DrawCommand;
struct DispatchCommand;
struct DRWDebugPrintBuffer;
struct DRWDebugVertPair;
struct DRWDebugDrawBuffer;
struct FrustumCorners;
struct FrustumPlanes;

/* __cplusplus is true when compiling with MSL. */
#  if defined(__cplusplus) && !defined(GPU_SHADER)
/* C++ only forward declarations. */
struct Object;
struct Scene;
struct ViewLayer;
struct GPUUniformAttr;
struct GPULayerAttr;

namespace blender::draw {

class ObjectRef;

}  // namespace blender::draw

using namespace blender::math;

#  endif
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

#if !defined(DRW_VIEW_LEN) && !defined(GLSL_CPP_STUBS)
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
#  define DRW_VIEW_FROM_RESOURCE_ID drw_view_id = (drw_resource_id_raw() & DRW_VIEW_MASK)
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
  /** \note float3 array padded to float4. */
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug draw shapes
 * \{ */

struct ObjectMatrices {
  float4x4 model;
  float4x4 model_inverse;

#ifndef GPU_SHADER
  void sync(const Object &object);
  void sync(const float4x4 &model_matrix);
#endif
};
BLI_STATIC_ASSERT_ALIGN(ObjectMatrices, 16)

enum eObjectInfoFlag : uint32_t {
  OBJECT_SELECTED = (1u << 0u),
  OBJECT_FROM_DUPLI = (1u << 1u),
  OBJECT_FROM_SET = (1u << 2u),
  OBJECT_ACTIVE = (1u << 3u),
  OBJECT_NEGATIVE_SCALE = (1u << 4u),
  OBJECT_HOLDOUT = (1u << 5u),
  /* Implies all objects that match the current active object's mode and able to be edited
   * simultaneously. Currently only applicable for edit mode. */
  OBJECT_ACTIVE_EDIT_MODE = (1u << 6u),
  /* Avoid skipped info to change culling. */
  OBJECT_NO_INFO = ~OBJECT_HOLDOUT
};

#ifndef GPU_SHADER
ENUM_OPERATORS(eObjectInfoFlag);
#endif

struct ObjectInfos {
  /** Uploaded as center + size. Converted to mul+bias to local coord. */
  packed_float3 orco_add;
  uint object_attrs_offset;
  packed_float3 orco_mul;
  uint object_attrs_len;

  float4 ob_color;
  uint index;
  /** Used for Light Linking in EEVEE */
  uint light_and_shadow_set_membership;
  float random;
  eObjectInfoFlag flag;
  float shadow_terminator_normal_offset;
  float shadow_terminator_geometry_offset;
  float _pad1;
  float _pad2;

#ifndef GPU_SHADER
  void sync();
  void sync(const blender::draw::ObjectRef ref, bool is_active_object, bool is_active_edit_mode);
#endif
};
BLI_STATIC_ASSERT_ALIGN(ObjectInfos, 16)

inline uint receiver_light_set_get(ObjectInfos object_infos)
{
  return object_infos.light_and_shadow_set_membership & 0xFFu;
}

inline uint blocker_shadow_set_get(ObjectInfos object_infos)
{
  return (object_infos.light_and_shadow_set_membership >> 8u) & 0xFFu;
}

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

#ifndef GPU_SHADER
  void sync();
  void sync(const Object &ob, float inflate_bounds = 0.0f);
  void sync(const float3 &center, const float3 &size);
#endif
};
BLI_STATIC_ASSERT_ALIGN(ObjectBounds, 16)

/* Return true if `bounding_corners` are valid. Should be checked before accessing them.
 * Does not guarantee that `bounding_sphere` is valid.
 * Converting these bounds to an `IsectBox` may generate invalid clip planes.
 * For safe `IsectBox` generation check `drw_bounds_are_valid`. */
inline bool drw_bounds_corners_are_valid(ObjectBounds bounds)
{
  return bounds.bounding_sphere.w != -1.0f;
}

/* Return true if bounds are ready for culling.
 * In this case, both `bounding_corners` and `bounding_sphere` are valid.
 * These bounds can be safely converted to an `IsectBox` with valid clip planes. */
inline bool drw_bounds_are_valid(ObjectBounds bounds)
{
  return bounds.bounding_sphere.w >= 0.0f;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object attributes
 * \{ */

struct VolumeInfos {
  /** Object to grid-space. */
  float4x4 grids_xform[DRW_GRID_PER_VOLUME_MAX];
  /** \note float4 for alignment. Only float3 needed. */
  float4 color_mul;
  float density_scale;
  float temperature_mul;
  float temperature_bias;
  float _pad;
};
BLI_STATIC_ASSERT_ALIGN(VolumeInfos, 16)

struct CurvesInfos {
  /* TODO(fclem): Make it a single uint. */
  /** Per attribute scope, follows loading order.
   * \note uint as bool in GLSL is 4 bytes.
   * \note GLSL pad arrays of scalar to 16 bytes (std140). */
  uint4 is_point_attribute[DRW_ATTRIBUTE_PER_CURVES_MAX];

  /* Number of vertex in a segment (including restart vertex for cylinder). */
  uint vertex_per_segment;
  /* Edge count for the visible half cylinder. Equal to face count + 1. */
  uint half_cylinder_face_count;
  uint _pad0;
  uint _pad1;
};
BLI_STATIC_ASSERT_ALIGN(CurvesInfos, 16)

#pragma pack(push, 4)
struct ObjectAttribute {
  /* Workaround the padding cost from alignment requirements.
   * (see GL spec : 7.6.2.2 Standard Uniform Block Layout) */
  float data_x, data_y, data_z, data_w;
  uint hash_code;

#ifndef GPU_SHADER
  /**
   * Go through all possible source of the given object uniform attribute.
   * Returns true if the attribute was correctly filled.
   */
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

#ifndef GPU_SHADER
  bool sync(const Scene *scene, const ViewLayer *layer, const GPULayerAttr &attr);
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
/** \name Debug draw shapes
 * \{ */

struct DRWDebugVertPair {
  /* This is a weird layout, but needed to be able to use DRWDebugVertPair as
   * a DrawCommand and avoid alignment issues. See drw_debug_lines_buf[] definition. */
  uint pos1_x;
  uint pos1_y;
  uint pos1_z;
  /* Named vert_color to avoid global namespace collision with uniform color. */
  uint vert_color;

  uint pos2_x;
  uint pos2_y;
  uint pos2_z;
  /* Number of time this line is supposed to be displayed. Decremented by one on display. */
  uint lifetime;
};
BLI_STATIC_ASSERT_ALIGN(DRWDebugVertPair, 16)

inline DRWDebugVertPair debug_line_make(uint in_pos1_x,
                                        uint in_pos1_y,
                                        uint in_pos1_z,
                                        uint in_pos2_x,
                                        uint in_pos2_y,
                                        uint in_pos2_z,
                                        uint in_vert_color,
                                        uint in_lifetime)
{
  DRWDebugVertPair debug_vert;
  debug_vert.pos1_x = in_pos1_x;
  debug_vert.pos1_y = in_pos1_y;
  debug_vert.pos1_z = in_pos1_z;
  debug_vert.pos2_x = in_pos2_x;
  debug_vert.pos2_y = in_pos2_y;
  debug_vert.pos2_z = in_pos2_z;
  debug_vert.vert_color = in_vert_color;
  debug_vert.lifetime = in_lifetime;
  return debug_vert;
}

inline uint debug_color_pack(float4 v_color)
{
  v_color = clamp(v_color, 0.0f, 1.0f);
  uint result = 0;
  result |= uint(v_color.x * 255.0) << 0u;
  result |= uint(v_color.y * 255.0) << 8u;
  result |= uint(v_color.z * 255.0) << 16u;
  result |= uint(v_color.w * 255.0) << 24u;
  return result;
}

/* Take the header (DrawCommand) into account. */
#define DRW_DEBUG_DRAW_VERT_MAX (2 * 1024) - 1

/* The debug draw buffer is laid-out as the following struct.
 * But we use plain array in shader code instead because of driver issues. */
struct DRWDebugDrawBuffer {
  DrawCommand command;
  DRWDebugVertPair verts[DRW_DEBUG_DRAW_VERT_MAX];
};
BLI_STATIC_ASSERT_ALIGN(DRWDebugDrawBuffer, 16)

/* Equivalent to `DRWDebugDrawBuffer.command.v_count`. */
#define drw_debug_draw_v_count(buf) buf[0].pos1_x
/**
 * Offset to the first data. Equal to: `sizeof(DrawCommand) / sizeof(DRWDebugVertPair)`.
 * This is needed because we bind the whole buffer as a `DRWDebugVertPair` array.
 */
#define drw_debug_draw_offset 1

/** \} */
