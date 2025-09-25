/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared code between host and client codebases.
 */

#pragma once

#include "eevee_defines.hh"
#include "eevee_transform.hh"

#ifndef GPU_SHADER
#  include "BLI_math_bits.h"

namespace blender::eevee {
#endif

#define LIGHT_NO_SHADOW -1

enum eLightType : uint32_t {
  LIGHT_SUN = 0u,
  LIGHT_SUN_ORTHO = 1u,
  /* Point light. */
  LIGHT_OMNI_SPHERE = 10u,
  LIGHT_OMNI_DISK = 11u,
  /* Spot light. */
  LIGHT_SPOT_SPHERE = 12u,
  LIGHT_SPOT_DISK = 13u,
  /* Area light. */
  LIGHT_RECT = 20u,
  LIGHT_ELLIPSE = 21u
};

static inline bool is_area_light(eLightType type)
{
  return type >= LIGHT_RECT;
}

static inline bool is_point_light(eLightType type)
{
  return type >= LIGHT_OMNI_SPHERE && type <= LIGHT_SPOT_DISK;
}

static inline bool is_spot_light(eLightType type)
{
  return type == LIGHT_SPOT_SPHERE || type == LIGHT_SPOT_DISK;
}

static inline bool is_sphere_light(eLightType type)
{
  return type == LIGHT_SPOT_SPHERE || type == LIGHT_OMNI_SPHERE;
}

static inline bool is_oriented_disk_light(eLightType type)
{
  return type == LIGHT_SPOT_DISK || type == LIGHT_OMNI_DISK;
}

static inline bool is_sun_light(eLightType type)
{
  return type < LIGHT_OMNI_SPHERE;
}

static inline bool is_local_light(eLightType type)
{
  return type >= LIGHT_OMNI_SPHERE;
}

/* Using define because GLSL doesn't have inheritance, and encapsulation forces us to add some
 * unneeded padding. */
#define LOCAL_LIGHT_COMMON \
  /** --- Shadow Data --- */ \
  /** Shift to apply to the light origin to get the shadow projection origin. In light space. */ \
  packed_float3 shadow_position; \
  float _pad0; \
  /** Radius of the light for shadow ray casting. Simple scaling factor for rectangle lights. */ \
  float shadow_radius; \
  /** Radius of the light for shading. Bounding radius for rectangle lights. */ \
  float shape_radius; \
  /** Maximum influence radius. Used for culling. Equal to clip far distance. */ \
  float influence_radius_max; \
  /** Influence radius (inverted and squared) adjusted for Surface / Volume power. */ \
  float influence_radius_invsqr_surface; \
  float influence_radius_invsqr_volume; \
  /** Number of allocated tilemap for this local light. */ \
  int tilemaps_count;

/* Untyped local light data. Gets reinterpreted to LightSpotData and LightAreaData.
 * Allow access to local light common data without casting. */
struct LightLocalData {
  LOCAL_LIGHT_COMMON

  float _pad1;
  float _pad2;

  float2 _pad3;
  float _pad4;
  float _pad5;
};
BLI_STATIC_ASSERT_ALIGN(LightLocalData, 16)

/* Despite the name, is also used for omni light. */
struct LightSpotData {
  LOCAL_LIGHT_COMMON

  float _pad1;
  /** Scale and bias to spot equation parameter. Used for adjusting the falloff. */
  float spot_mul;

  /** Inverse spot size (in X and Y axes). */
  float2 spot_size_inv;
  /** Spot angle tangent. */
  float spot_tan;
  float spot_bias;
};
BLI_STATIC_ASSERT(sizeof(LightSpotData) == sizeof(LightLocalData), "Data size must match")

struct LightAreaData {
  LOCAL_LIGHT_COMMON

  float _pad2;
  float _pad3;

  /** Shape size. */
  float2 size;
  /** Scale to apply on top of `size` to get shadow tracing shape size. */
  float shadow_scale;
  float _pad6;
};
BLI_STATIC_ASSERT(sizeof(LightAreaData) == sizeof(LightLocalData), "Data size must match")

struct LightSunData {
  /* Sun direction for shading. Use object_to_world for getting into shadow space. */
  packed_float3 direction;
  /* Radius of the sun disk, one unit away from a shading point. */
  float shape_radius;

  /** --- Shadow Data --- */
  /** Offset of the LOD min in LOD min tile units. Split positive and negative for bit-shift. */
  int2 clipmap_base_offset_neg;
  int2 clipmap_base_offset_pos;

  /** Angle covered by the light shape for shadow ray casting. */
  float shadow_angle;
  float _pad5;
  float _pad3;
  float _pad4;

  /** Offset to convert from world units to tile space of the clipmap_lod_max. */
  float2 clipmap_origin;
  /** Clip-map LOD range to avoid sampling outside of valid range. */
  int clipmap_lod_min;
  int clipmap_lod_max;
};
BLI_STATIC_ASSERT(sizeof(LightSunData) == sizeof(LightLocalData), "Data size must match")

/* Enable when debugging. This is quite costly. */
#define SAFE_UNION_ACCESS 0

#ifndef GPU_SHADER
/* C++ always uses union. */
#  define USE_LIGHT_UNION 1
#elif defined(GPU_BACKEND_METAL) && !SAFE_UNION_ACCESS
/* Metal supports union, but force usage of the getters if SAFE_UNION_ACCESS is enabled. */
#  define USE_LIGHT_UNION 1
#else
/* Use getter functions on GPU if not supported or if SAFE_UNION_ACCESS is enabled. */
#  define USE_LIGHT_UNION 0
#endif

struct LightData {
  /**
   * Normalized object to world matrix. Stored transposed for compactness.
   * Used for shading and shadowing local lights, or shadowing sun lights.
   * IMPORTANT: Not used for shading sun lights as this matrix is jittered.
   */
  Transform object_to_world;

  /** Power depending on shader type. Referenced by LightingType. */
  float4 power;
  /** Light Color. */
  packed_float3 color;
  /** Light Type. */
  eLightType type;

  /** --- Shadow Data --- */
  /** Near clip distances. Float stored as orderedIntBitsToFloat for atomic operations. */
  int clip_near;
  int clip_far;
  /** Index of the first tile-map. Set to LIGHT_NO_SHADOW if light is not casting shadow. */
  int tilemap_index;
  /* Radius in pixels for shadow filtering. */
  float filter_radius;

  /* Shadow Map resolution bias. */
  float lod_bias;
  /* Shadow Map resolution maximum resolution. */
  float lod_min;
  /* True if the light uses jittered soft shadows. */
  bool32_t shadow_jitter;
  float _pad2;
  uint2 light_set_membership;
  /** Used by shadow sync. */
  /* TODO(fclem): this should be part of #eevee::Light struct. But for some reason it gets cleared
   * to zero after each sync cycle. */
  uint2 shadow_set_membership;

#if USE_LIGHT_UNION
  union {
    LightLocalData local;
    LightSpotData spot;
    LightAreaData area;
    LightSunData sun;
  };
#else
  /* Use `light_*_data_get(light)` to access typed data. */
  LightLocalData do_not_access_directly;
#endif
};
BLI_STATIC_ASSERT_ALIGN(LightData, 16)

static inline float3 light_x_axis(LightData light)
{
  return transform_x_axis(light.object_to_world);
}
static inline float3 light_y_axis(LightData light)
{
  return transform_y_axis(light.object_to_world);
}
static inline float3 light_z_axis(LightData light)
{
  return transform_z_axis(light.object_to_world);
}
static inline float3 light_position_get(LightData light)
{
  return transform_location(light.object_to_world);
}

#ifdef GPU_SHADER
#  define CHECK_TYPE_PAIR(a, b)
#  define CHECK_TYPE(a, b)
#  define FLOAT_AS_INT floatBitsToInt
#  define INT_AS_FLOAT intBitsToFloat
#  define TYPECAST_NOOP

#else /* C++ */
#  define FLOAT_AS_INT float_as_int
#  define INT_AS_FLOAT int_as_float
#  define TYPECAST_NOOP
#endif

/* In addition to the static asserts that verify correct member assignment, also verify access on
 * the GPU so that only lights of a certain type can read for the appropriate union member.
 * Return cross platform garbage data as some platform can return cleared memory if we early exit.
 */
#if SAFE_UNION_ACCESS
#  ifdef GPU_SHADER
/* Should result in a beautiful zebra pattern on invalid load. */
#    if defined(GPU_FRAGMENT_SHADER)
#      define GARBAGE_VALUE sin(gl_FragCoord.x + gl_FragCoord.y)
#    elif defined(GPU_COMPUTE_SHADER)
#      define GARBAGE_VALUE \
        sin(float(gl_GlobalInvocationID.x + gl_GlobalInvocationID.y + gl_GlobalInvocationID.z))
#    else
#      define GARBAGE_VALUE sin(float(gl_VertexID))
#    endif

/* Can be set to zero if zebra creates out-of-bound accesses and crashes. At least avoid UB. */
// #    define GARBAGE_VALUE 0.0

#  else /* C++ */
#    define GARBAGE_VALUE 0.0f
#  endif

#  define SAFE_BEGIN(dst_type, src_type, src_, check) \
    src_type _src = src_; \
    dst_type _dst; \
    bool _validity_check = check; \
    float _garbage = GARBAGE_VALUE;

/* Assign garbage value if the light type check fails. */
#  define SAFE_ASSIGN_LIGHT_TYPE_CHECK(_type, _value) \
    (_validity_check ? (_value) : _type(_garbage))
#else
#  define SAFE_BEGIN(dst_type, src_type, src_, check) \
    UNUSED_VARS(check); \
    src_type _src = src_; \
    dst_type _dst;

#  define SAFE_ASSIGN_LIGHT_TYPE_CHECK(_type, _value) _value
#endif

#if USE_LIGHT_UNION
#  define DATA_MEMBER local
#else
#  define DATA_MEMBER do_not_access_directly
#endif

#define SAFE_READ_BEGIN(dst_type, light, check) \
  SAFE_BEGIN(dst_type, LightLocalData, light.DATA_MEMBER, check)
#define SAFE_READ_END() _dst

#define SAFE_WRITE_BEGIN(src_type, src, check) SAFE_BEGIN(LightLocalData, src_type, src, check)
#define SAFE_WRITE_END(light) light.DATA_MEMBER = _dst;

#define ERROR_OFS(a, b) "Offset of " STRINGIFY(a) " mismatch offset of " STRINGIFY(b)

/* This is a dangerous process, make sure to static assert every assignment. */
#define SAFE_ASSIGN(a, reinterpret_fn, in_type, b) \
  CHECK_TYPE_PAIR(_src.b, in_type(_dst.a)); \
  CHECK_TYPE_PAIR(_dst.a, reinterpret_fn(_src.b)); \
  _dst.a = reinterpret_fn(SAFE_ASSIGN_LIGHT_TYPE_CHECK(in_type, _src.b)); \
  BLI_STATIC_ASSERT(offsetof(decltype(_dst), a) == offsetof(decltype(_src), b), ERROR_OFS(a, b))

#define SAFE_ASSIGN_FLOAT(a, b) SAFE_ASSIGN(a, TYPECAST_NOOP, float, b);
#define SAFE_ASSIGN_FLOAT2(a, b) SAFE_ASSIGN(a, TYPECAST_NOOP, float2, b);
#define SAFE_ASSIGN_FLOAT3(a, b) SAFE_ASSIGN(a, TYPECAST_NOOP, float3, b);
#define SAFE_ASSIGN_INT(a, b) SAFE_ASSIGN(a, TYPECAST_NOOP, int, b);
#define SAFE_ASSIGN_FLOAT_AS_INT(a, b) SAFE_ASSIGN(a, FLOAT_AS_INT, float, b);
#define SAFE_ASSIGN_INT_AS_FLOAT(a, b) SAFE_ASSIGN(a, INT_AS_FLOAT, int, b);

#if !USE_LIGHT_UNION || !defined(GPU_SHADER)

/* These functions are not meant to be used in C++ code. They are only defined on the C++ side for
 * static assertions. Hide them. */
#  if !defined(GPU_SHADER)
namespace do_not_use {
#  endif

static inline LightSpotData light_local_data_get_ex(LightData light, bool check)
{
  SAFE_READ_BEGIN(LightSpotData, light, check)
  SAFE_ASSIGN_FLOAT3(shadow_position, shadow_position)
  SAFE_ASSIGN_FLOAT(_pad0, _pad0)
  SAFE_ASSIGN_FLOAT(shadow_radius, shadow_radius)
  SAFE_ASSIGN_FLOAT(shape_radius, shape_radius)
  SAFE_ASSIGN_FLOAT(influence_radius_max, influence_radius_max)
  SAFE_ASSIGN_FLOAT(influence_radius_invsqr_surface, influence_radius_invsqr_surface)
  SAFE_ASSIGN_FLOAT(influence_radius_invsqr_volume, influence_radius_invsqr_volume)
  SAFE_ASSIGN_INT(tilemaps_count, tilemaps_count)
  SAFE_ASSIGN_FLOAT(spot_mul, _pad2)
  SAFE_ASSIGN_FLOAT2(spot_size_inv, _pad3)
  SAFE_ASSIGN_FLOAT(spot_tan, _pad4)
  SAFE_ASSIGN_FLOAT(spot_bias, _pad5)
  return SAFE_READ_END();
}

static inline LightData light_local_data_set(LightData light, LightSpotData spot_data)
{
  SAFE_WRITE_BEGIN(LightSpotData, spot_data, is_local_light(light.type))
  SAFE_ASSIGN_FLOAT3(shadow_position, shadow_position)
  SAFE_ASSIGN_FLOAT(_pad0, _pad0)
  SAFE_ASSIGN_FLOAT(shadow_radius, shadow_radius)
  SAFE_ASSIGN_FLOAT(shape_radius, shape_radius)
  SAFE_ASSIGN_FLOAT(influence_radius_max, influence_radius_max)
  SAFE_ASSIGN_FLOAT(influence_radius_invsqr_surface, influence_radius_invsqr_surface)
  SAFE_ASSIGN_FLOAT(influence_radius_invsqr_volume, influence_radius_invsqr_volume)
  SAFE_ASSIGN_INT(tilemaps_count, tilemaps_count)
  SAFE_ASSIGN_FLOAT(_pad2, spot_mul)
  SAFE_ASSIGN_FLOAT2(_pad3, spot_size_inv)
  SAFE_ASSIGN_FLOAT(_pad4, spot_tan)
  SAFE_ASSIGN_FLOAT(_pad5, spot_bias)
  SAFE_WRITE_END(light)
  return light;
}

static inline LightSpotData light_local_data_get(LightData light)
{
  return light_local_data_get_ex(light, is_local_light(light.type));
}

static inline LightSpotData light_spot_data_get(LightData light)
{
  return light_local_data_get_ex(light, is_spot_light(light.type) || is_point_light(light.type));
}

static inline LightAreaData light_area_data_get(LightData light)
{
  SAFE_READ_BEGIN(LightAreaData, light, is_area_light(light.type))
  SAFE_ASSIGN_FLOAT(shape_radius, shape_radius)
  SAFE_ASSIGN_FLOAT(influence_radius_max, influence_radius_max)
  SAFE_ASSIGN_FLOAT(influence_radius_invsqr_surface, influence_radius_invsqr_surface)
  SAFE_ASSIGN_FLOAT(influence_radius_invsqr_volume, influence_radius_invsqr_volume)
  SAFE_ASSIGN_FLOAT3(shadow_position, shadow_position)
  SAFE_ASSIGN_FLOAT(shadow_radius, shadow_radius)
  SAFE_ASSIGN_INT(tilemaps_count, tilemaps_count)
  SAFE_ASSIGN_FLOAT2(size, _pad3)
  SAFE_ASSIGN_FLOAT(shadow_scale, _pad4)
  return SAFE_READ_END();
}

static inline LightSunData light_sun_data_get(LightData light)
{
  SAFE_READ_BEGIN(LightSunData, light, is_sun_light(light.type))
  SAFE_ASSIGN_FLOAT3(direction, shadow_position)
  SAFE_ASSIGN_FLOAT(shape_radius, _pad0)
  SAFE_ASSIGN_FLOAT_AS_INT(clipmap_base_offset_neg.x, shadow_radius)
  SAFE_ASSIGN_FLOAT_AS_INT(clipmap_base_offset_neg.y, shape_radius)
  SAFE_ASSIGN_FLOAT_AS_INT(clipmap_base_offset_pos.x, influence_radius_max)
  SAFE_ASSIGN_FLOAT_AS_INT(clipmap_base_offset_pos.y, influence_radius_invsqr_surface)
  SAFE_ASSIGN_FLOAT(shadow_angle, influence_radius_invsqr_volume)
  SAFE_ASSIGN_FLOAT2(clipmap_origin, _pad3)
  SAFE_ASSIGN_FLOAT_AS_INT(clipmap_lod_min, _pad4)
  SAFE_ASSIGN_FLOAT_AS_INT(clipmap_lod_max, _pad5)
  return SAFE_READ_END();
}

static inline LightData light_sun_data_set(LightData light, LightSunData sun_data)
{
  SAFE_WRITE_BEGIN(LightSunData, sun_data, is_sun_light(light.type))
  SAFE_ASSIGN_FLOAT3(shadow_position, direction)
  SAFE_ASSIGN_FLOAT(_pad0, shape_radius)
  SAFE_ASSIGN_INT_AS_FLOAT(shadow_radius, clipmap_base_offset_neg.x)
  SAFE_ASSIGN_INT_AS_FLOAT(shape_radius, clipmap_base_offset_neg.y)
  SAFE_ASSIGN_INT_AS_FLOAT(influence_radius_max, clipmap_base_offset_pos.x)
  SAFE_ASSIGN_INT_AS_FLOAT(influence_radius_invsqr_surface, clipmap_base_offset_pos.y)
  SAFE_ASSIGN_FLOAT(influence_radius_invsqr_volume, shadow_angle)
  SAFE_ASSIGN_FLOAT2(_pad3, clipmap_origin)
  SAFE_ASSIGN_INT_AS_FLOAT(_pad4, clipmap_lod_min)
  SAFE_ASSIGN_INT_AS_FLOAT(_pad5, clipmap_lod_max)
  SAFE_WRITE_END(light)
  return light;
}

#  if !defined(GPU_SHADER)
}  // namespace do_not_use
#  endif

#endif

#if USE_LIGHT_UNION
#  define light_local_data_get(light) light.local
#  define light_spot_data_get(light) light.spot
#  define light_area_data_get(light) light.area
#  define light_sun_data_get(light) light.sun
#endif

#undef DATA_MEMBER
#undef GARBAGE_VALUE
#undef FLOAT_AS_INT
#undef TYPECAST_NOOP
#undef SAFE_BEGIN
#undef SAFE_ASSIGN_LIGHT_TYPE_CHECK
#undef ERROR_OFS
#undef SAFE_ASSIGN
#undef SAFE_ASSIGN_FLOAT
#undef SAFE_ASSIGN_FLOAT2
#undef SAFE_ASSIGN_INT
#undef SAFE_ASSIGN_FLOAT_AS_INT
#undef SAFE_ASSIGN_INT_AS_FLOAT

static inline int light_tilemap_max_get(LightData light)
{
  /* This is not something we need in performance critical code. */
  if (is_sun_light(light.type)) {
    return light.tilemap_index +
           (light_sun_data_get(light).clipmap_lod_max - light_sun_data_get(light).clipmap_lod_min);
  }
  return light.tilemap_index + light_local_data_get(light).tilemaps_count - 1;
}

/* Return the number of tilemap needed for a local light. */
static inline int light_local_tilemap_count(LightData light)
{
  if (is_spot_light(light.type)) {
    return (light_spot_data_get(light).spot_tan > tanf(EEVEE_PI / 4.0)) ? 5 : 1;
  }
  if (is_area_light(light.type)) {
    return 5;
  }
  return 6;
}

/* -------------------------------------------------------------------- */
/** \name Light Culling
 * \{ */

/* Number of items we can cull. Limited by how we store CullingZBin. */
#define CULLING_MAX_ITEM 65536
/* Fine grained subdivision in the Z direction. Limited by the LDS in z-binning compute shader. */
#define CULLING_ZBIN_COUNT 4096
/* Max tile map resolution per axes. */
#define CULLING_TILE_RES 16

struct LightCullingData {
  /** Scale applied to tile pixel coordinates to get target UV coordinate. */
  float2 tile_to_uv_fac;
  /** Scale and bias applied to linear Z to get zbin. */
  float zbin_scale;
  float zbin_bias;
  /** Valid item count in the source data array. */
  uint items_count;
  /** Items that are processed by the 2.5D culling. */
  uint local_lights_len;
  /** Items that are **NOT** processed by the 2.5D culling (i.e: Sun Lights). */
  uint sun_lights_len;
  /** Number of items that passes the first culling test. (local lights only) */
  uint visible_count;
  /** Extent of one square tile in pixels. */
  float tile_size;
  /** Number of tiles on the X/Y axis. */
  uint tile_x_len;
  uint tile_y_len;
  /** Number of word per tile. Depends on the maximum number of lights. */
  uint tile_word_len;
  /** Is the view being processed by light culling flipped (true for light probe planes). */
  bool32_t view_is_flipped;
  uint _pad0;
  uint _pad1;
  uint _pad2;
};
BLI_STATIC_ASSERT_ALIGN(LightCullingData, 16)

/** \} */

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
