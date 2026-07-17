/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_shader_shared_utils.hh"

#ifndef GPU_SHADER
#  include "BLI_math_vector.hh"
#  include "GPU_shader.hh"
#  include "draw_defines.hh"

#  if defined(__cplusplus) && !defined(GPU_SHADER)

/* Defined on windows in `windows.h`. */
#    undef far
#    undef near

namespace blender {
#  endif

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

namespace draw {

class ObjectRef;

}  // namespace draw

using namespace math;

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

struct [[host_shared]] FrustumCorners {
  float4 corners[8];
};

struct [[host_shared]] FrustumPlanes {
  /* [0] left
   * [1] right
   * [2] bottom
   * [3] top
   * [4] near
   * [5] far */
  float4 planes[6];
};

struct [[host_shared]] ViewCullingData {
  /** \note float3 array padded to float4. */
  /** Frustum corners. */
  struct FrustumCorners frustum_corners;
  struct FrustumPlanes frustum_planes;
  float4 bound_sphere;
};

struct [[host_shared]] ViewMatrices {
  float4x4 viewmat;
  float4x4 viewinv;
  float4x4 winmat;
  float4x4 wininv;

  /* Returns true if the current view has a perspective projection matrix. */
  bool is_perspective() const
  {
    return winmat[3][3] == 0.0f;
  }

  /* Returns the view forward vector, going towards the viewer. */
  float3 forward() const
  {
    return viewinv[2].xyz();
  }

  /* Returns the view up vector. */
  float3 up() const
  {
    return viewinv[1].xyz();
  }

  /* Returns the view origin. */
  float3 position() const
  {
    return viewinv[3].xyz();
  }

  /* Positive Z distance from the view origin. Faster than using `drw_point_world_to_view`. */
  float z_distance(float3 P) const
  {
    return dot(P - position(), -forward());
  }

  /* Returns the projection matrix far clip distance. */
  float far() const
  {
    if (is_perspective()) {
      return -winmat[3][2] / (winmat[2][2] + 1.0f);
    }
    return -(winmat[3][2] - 1.0f) / winmat[2][2];
  }

  /* Returns the projection matrix near clip distance. */
  float near() const
  {
    if (is_perspective()) {
      return -winmat[3][2] / (winmat[2][2] - 1.0f);
    }
    return -(winmat[3][2] + 1.0f) / winmat[2][2];
  }

  /**
   * Returns the world incident vector `V` (going towards the viewer)
   * from the world position `P` and the current view.
   */
  float3 world_incident_vector(float3 P) const
  {
    return is_perspective() ? normalize(position() - P) : forward();
  }

  /**
   * Returns the view incident vector `vV` (going towards the viewer)
   * from the view position `vP` and the current view.
   */
  float3 view_incident_vector(float3 vP) const
  {
    return is_perspective() ? normalize(-vP) : float3(0.0f, 0.0f, 1.0f);
  }

  /**
   * Transform position on screen UV space [0..1] to Normalized Device Coordinate space [-1..1].
   */
  template<typename T> T screen_to_ndc(T ss_P) const
  {
    return ss_P * 2.0f - 1.0f;
  }

  /**
   * Transform position in Normalized Device Coordinate [-1..1] to screen UV space [0..1].
   */
  template<typename T> T ndc_to_screen(T ndc_P) const
  {
    return ndc_P * 0.5f + 0.5f;
  }

  /* -------------------------------------------------------------------- */
  /** \name Transform Normal
   * \{ */

#ifdef GPU_SHADER /* `to_float3x3` doesn't exist on CPU side. */
  float3 normal_view_to_world(float3 vN) const
  {
    return (to_float3x3(viewinv) * vN);
  }

  float3 normal_world_to_view(float3 N) const
  {
    return (to_float3x3(viewmat) * N);
  }
#endif

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Transform Position
   * \{ */

  float3 perspective_divide(float4 hs_P) const
  {
    return hs_P.xyz() / hs_P.w;
  }

  float3 point_view_to_world(float3 vP) const
  {
    return (viewinv * float4(vP, 1.0f)).xyz();
  }

  float4 point_view_to_homogenous(float3 vP) const
  {
    return (winmat * float4(vP, 1.0f));
  }

  float3 point_view_to_ndc(float3 vP) const
  {
    return perspective_divide(point_view_to_homogenous(vP));
  }

  float3 point_world_to_view(float3 P) const
  {
    return (viewmat * float4(P, 1.0f)).xyz();
  }

  float4 point_world_to_homogenous(float3 P) const
  {
    return (winmat * (viewmat * float4(P, 1.0f)));
  }

  float3 point_world_to_ndc(float3 P) const
  {
    return perspective_divide(point_world_to_homogenous(P));
  }

  float3 point_ndc_to_view(float3 ssP) const
  {
    return perspective_divide(wininv * float4(ssP, 1.0f));
  }

  float3 point_ndc_to_world(float3 ssP) const
  {
    return point_view_to_world(point_ndc_to_view(ssP));
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Transform Screen Positions
   * \{ */

  float3 point_view_to_screen(float3 vP) const
  {
    return ndc_to_screen(point_view_to_ndc(vP));
  }

  float3 point_world_to_screen(float3 vP) const
  {
    return ndc_to_screen(point_world_to_ndc(vP));
  }

  float3 point_screen_to_view(float3 ssP) const
  {
    return point_ndc_to_view(screen_to_ndc(ssP));
  }

  float3 point_screen_to_world(float3 ssP) const
  {
    return point_view_to_world(point_screen_to_view(ssP));
  }

  float depth_view_to_screen(float v_depth) const
  {
    return point_view_to_screen(float3(0.0f, 0.0f, v_depth)).z;
  }

  float depth_screen_to_view(float ss_depth) const
  {
    return point_screen_to_view(float3(0.0f, 0.0f, ss_depth)).z;
  }

  /** \} */
};

template float ViewMatrices::screen_to_ndc<float>(float ss_P) const;
template float2 ViewMatrices::screen_to_ndc<float2>(float2 ss_P) const;
template float3 ViewMatrices::screen_to_ndc<float3>(float3 ss_P) const;
template float ViewMatrices::ndc_to_screen<float>(float ss_P) const;
template float2 ViewMatrices::ndc_to_screen<float2>(float2 ss_P) const;
template float3 ViewMatrices::ndc_to_screen<float3>(float3 ss_P) const;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug draw shapes
 * \{ */

struct [[host_shared]] ObjectMatrices {
  float4x4 model;
  float4x4 model_inverse;

#ifndef GPU_SHADER
  void sync(const Object &object);
  void sync(const float4x4 &model_matrix);
#endif

#ifdef GPU_SHADER /* `to_float3x3` doesn't exist on CPU side. */
  /**
   * Usually Normal matrix is `transpose(inverse(ViewMatrix * ModelMatrix))`.
   *
   * But since it is slow to multiply matrices we decompose it. Decomposing
   * inversion and transposition both invert the product order leaving us with
   * the same original order:
   * transpose(ViewMatrixInverse) * transpose(ModelMatrixInverse)
   *
   * Knowing that the view matrix is orthogonal, the transpose is also the inverse.
   * NOTE: This is only valid because we are only using the mat3 of the ViewMatrixInverse.
   * ViewMatrix * transpose(ModelMatrixInverse)
   */
  float3x3 normal() const
  {
    return transpose(to_float3x3(model_inverse));
  }
  float3x3 normal_inverse() const
  {
    return transpose(to_float3x3(model));
  }
#endif

  /* -------------------------------------------------------------------- */
  /** \name Transform Normal
   *
   * Space conversion helpers for normal vectors.
   * \{ */

#ifdef GPU_SHADER /* `to_float3x3` doesn't exist on CPU side. */
  float3 normal_object_to_world(float3 lN) const
  {
    return normal() * lN;
  }

  float3 normal_world_to_object(float3 N) const
  {
    return normal_inverse() * N;
  }

  float3 normal_object_to_view(ViewMatrices view, float3 lN) const
  {
    return to_float3x3(view.viewmat) * (normal() * lN);
  }

  float3 normal_view_to_object(ViewMatrices view, float3 vN) const
  {
    return normal_inverse() * (to_float3x3(view.viewinv) * vN);
  }
#endif

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Transform Point
   *
   * Space conversion helpers for points (coordinates).
   * \{ */

  float3 point_object_to_world(float3 lP) const
  {
    return (model * float4(lP, 1.0f)).xyz();
  }

  float3 point_world_to_object(float3 P) const
  {
    return (model_inverse * float4(P, 1.0f)).xyz();
  }

  float3 point_object_to_view(ViewMatrices view, float3 lP) const
  {
    return (view.viewmat * (model * float4(lP, 1.0f))).xyz();
  }

  float3 point_view_to_object(ViewMatrices view, float3 vP) const
  {
    return (model_inverse * (view.viewinv * float4(vP, 1.0f))).xyz();
  }

  float4 point_object_to_homogenous(ViewMatrices view, float3 lP) const
  {
    return view.winmat * (view.viewmat * (model * float4(lP, 1.0f)));
  }

  float3 point_object_to_ndc(ViewMatrices view, float3 lP) const
  {
    return view.perspective_divide(point_object_to_homogenous(view, lP));
  }

  /** \} */
};

enum [[host_shared]] eObjectInfoFlag : uint32_t {
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

struct [[host_shared]] ObjectInfos {
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
  enum eObjectInfoFlag flag;
  float shadow_terminator_normal_offset;
  float shadow_terminator_geometry_offset;
  float _pad1;
  float _pad2;

#ifndef GPU_SHADER
  void sync();
  void sync(const draw::ObjectRef ref, bool is_active_object, bool is_active_edit_mode);
#endif
};

inline uint receiver_light_set_get(ObjectInfos object_infos)
{
  return object_infos.light_and_shadow_set_membership & 0xFFu;
}

inline uint blocker_shadow_set_get(ObjectInfos object_infos)
{
  return (object_infos.light_and_shadow_set_membership >> 8u) & 0xFFu;
}

struct [[host_shared]] ObjectBounds {
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

struct [[host_shared]] VolumeInfos {
  /** Object to grid-space. */
  float4x4 grids_xform[DRW_GRID_PER_VOLUME_MAX];
  /** \note float4 for alignment. Only float3 needed. */
  float4 color_mul;
  float density_scale;
  float temperature_mul;
  float temperature_bias;
  float _pad;
};

struct [[host_shared]] CurvesInfos {
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

#pragma pack(push, 4)
struct [[host_shared]] ObjectAttribute {
  /* Workaround the padding cost from alignment requirements.
   * (see GL spec : 7.6.2.2 Standard Uniform Block Layout) */
  float data_x;
  float data_y;
  float data_z;
  float data_w;
  uint hash_code;

#ifndef GPU_SHADER
  /**
   * Go through all possible source of the given object uniform attribute.
   * Returns true if the attribute was correctly filled.
   *
   * \param instance_index: The index of synced the instance within the ObjectRef::duplis_,
   *                        or just 0 if the ObjectRef doesn't contain any instances.
   */
  bool sync(const draw::ObjectRef &ref, const GPUUniformAttr &attr, int instance_index);
#endif
};
#pragma pack(pop)
/** \note we only align to 4 bytes and fetch data manually so make sure
 * C++ compiler gives us the same size. */
#ifndef GPU_SHADER
BLI_STATIC_ASSERT_ALIGN(ObjectAttribute, 20)
#endif

struct [[host_shared]] LayerAttribute {
  float4 data;
  uint hash_code;
  uint buffer_length; /* Only in the first record. */
  uint _pad1;
  uint _pad2;

#ifndef GPU_SHADER
  bool sync(const Scene *scene, const ViewLayer *layer, const GPULayerAttr &attr);
#endif
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Indirect commands structures.
 * \{ */

/* Regular draw commands (no index buffer). */
struct [[host_shared]] DrawCommandArray {
  uint vertex_len;
  uint instance_len;
  uint vertex_first;
  uint instance_first;

  uint _pad0;
  uint _pad1;
  uint _pad2;
  uint _pad3;
};

/* Indexed draw commands (with index buffer). */
struct [[host_shared]] DrawCommandIndexed {
  uint vertex_len;
  uint instance_len;
  uint vertex_first;
  uint base_index;

  uint instance_first;
  uint _pad0;
  uint _pad1;
  uint _pad2;
};

struct [[host_shared]] DrawCommand {
  union {
    union_t<DrawCommandArray> array;
    union_t<DrawCommandIndexed> indexed;
  };
};

struct [[host_shared]] DispatchCommand {
  uint num_groups_x;
  uint num_groups_y;
  uint num_groups_z;
  uint _pad0;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug draw shapes
 * \{ */

struct [[host_shared]] DRWDebugVertPair {
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
struct [[host_shared]] DRWDebugDrawBuffer {
  struct DrawCommand command;
  struct DRWDebugVertPair verts[DRW_DEBUG_DRAW_VERT_MAX];
};

/* Equivalent to `DRWDebugDrawBuffer.command.v_count`. */
#define drw_debug_draw_v_count(buf) buf[0].pos1_x
/**
 * Offset to the first data. Equal to: `sizeof(DrawCommand) / sizeof(DRWDebugVertPair)`.
 * This is needed because we bind the whole buffer as a `DRWDebugVertPair` array.
 */
#define drw_debug_draw_offset 1

/** \} */

#if !defined(GPU_SHADER)
}  // namespace blender
#endif
