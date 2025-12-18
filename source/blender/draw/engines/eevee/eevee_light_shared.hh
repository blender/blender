/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared code between host and client code-bases.
 */

#pragma once

#include "eevee_defines.hh"
#include "eevee_transform.hh"

#ifndef GPU_SHADER
#  include "BLI_math_bits.h"

namespace blender::eevee {
#endif

#define LIGHT_NO_SHADOW -1

/**
 * Index inside the world sun buffer.
 * In the case the world uses the light path node, multiple suns can be extracted from the world.
 */
enum [[host_shared]] WorldSunIndex : uint32_t {
  /** When the world node-tree doesn't use the light path node, there is only 1 extracted. */
  WORLD_SUN_COMBINED = 0u,

  /* Index of each components. */
  WORLD_SUN_DIFFUSE = 0u,
  WORLD_SUN_GLOSSY = 1u,

  WORLD_SUN_MAX = 2u,
};

enum [[host_shared]] eLightType : uint32_t {
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

struct [[host_shared]] LightLocalCommon {
  /** Shift to apply to the light origin to get the shadow projection origin. In light space. */
  packed_float3 shadow_position;
  /** Radius of the light for shadow ray casting. Simple scaling factor for rectangle lights. */
  float shadow_radius;
  /** Radius of the light for shading. Bounding radius for rectangle lights. */
  float shape_radius;
  /** Maximum influence radius. Used for culling. Equal to clip far distance. */
  float influence_radius_max;
  /** Influence radius (inverted and squared) adjusted for Surface Volume power. */
  float influence_radius_invsqr_surface;
  float influence_radius_invsqr_volume;
};

/* Untyped local light data. Gets reinterpreted to LightSpotData and LightAreaData.
 * Allow access to local light common data without casting. */
struct [[host_shared]] LightLocalData {
  struct LightLocalCommon local;

  /** Number of allocated tilemap for this local light. */
  int tilemaps_count; /* Leaked from LightLocalCommon because of alignment. */
  float _pad0;
  float _pad1;
  float _pad2;

  float2 _pad3;
  float _pad4;
  float _pad5;
};

/* Despite the name, is also used for omni light. */
struct [[host_shared]] LightSpotData {
  struct LightLocalCommon local;

  /** Number of allocated tilemap for this local light. */
  int tilemaps_count; /* Leaked from LightLocalCommon because of alignment. */
  float _pad0;
  float _pad1;
  /** Scale and bias to spot equation parameter. Used for adjusting the falloff. */
  float spot_mul;

  /** Inverse spot size (in X and Y axes). */
  float2 spot_size_inv;
  /** Spot angle tangent. */
  float spot_tan;
  float spot_bias;
};

struct [[host_shared]] LightAreaData {
  struct LightLocalCommon local;

  /** Number of allocated tilemap for this local light. */
  int tilemaps_count; /* Leaked from LightLocalCommon because of alignment. */
  float _pad0;
  float _pad1;
  float _pad2;

  /** Shape size. */
  float2 size;
  /** Scale to apply on top of `size` to get shadow tracing shape size. */
  float shadow_scale;
  float _pad6;
};

struct [[host_shared]] LightSunData {
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
  float shadow_angle_cos;
  float _pad3;
  float _pad4;

  /** Offset to convert from world units to tile space of the clipmap_lod_max. */
  float2 clipmap_origin;
  /** Clip-map LOD range to avoid sampling outside of valid range. */
  int clipmap_lod_min;
  int clipmap_lod_max;
};

struct [[host_shared]] LightData {
  /**
   * Normalized object to world matrix. Stored transposed for compactness.
   * Used for shading and shadowing local lights, or shadowing sun lights.
   * IMPORTANT: Not used for shading sun lights as this matrix is jittered.
   */
  struct Transform object_to_world;

  /** Power depending on shader type. Referenced by LightingType. */
  float4 power;
  /** Light Color. */
  packed_float3 color;
  /** Light Type. */
  enum eLightType type;

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

  union {
    union_t<struct LightLocalData> local;
    union_t<struct LightSpotData> spot;
    union_t<struct LightAreaData> area;
    union_t<struct LightSunData> sun;
  };
};

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

static inline int light_tilemap_max_get(LightData light)
{
  /* This is not something we need in performance critical code. */
  if (is_sun_light(light.type)) {
    return light.tilemap_index + (light.sun().clipmap_lod_max - light.sun().clipmap_lod_min);
  }
  return light.tilemap_index + light.local().tilemaps_count - 1;
}

/* Return the number of tilemap needed for a local light. */
static inline int light_local_tilemap_count(LightData light)
{
  if (is_spot_light(light.type)) {
    return (light.spot().spot_tan > tanf(EEVEE_PI / 4.0)) ? 5 : 1;
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

struct [[host_shared]] LightCullingData {
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

/** \} */

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
