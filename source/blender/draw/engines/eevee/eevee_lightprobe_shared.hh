/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared code between host and client codebases.
 */

#pragma once

#include "GPU_shader_shared_utils.hh"

#include "eevee_camera_shared.hh"

#ifndef GPU_SHADER
namespace blender::eevee {
#endif

/* -------------------------------------------------------------------- */
/** \name Probe Spheres
 * \{ */

struct ReflectionProbeLowFreqLight {
  packed_float3 direction;
  float ambient;
};
BLI_STATIC_ASSERT_ALIGN(ReflectionProbeLowFreqLight, 16)

enum LightProbeShape : uint32_t {
  SHAPE_ELIPSOID = 0u,
  SHAPE_CUBOID = 1u,
};

/* Sampling coordinates using UV space. */
struct SphereProbeUvArea {
  /* Offset in UV space to the start of the sampling space of the octahedron map. */
  float2 offset;
  /* Scaling of the squared UV space of the octahedron map. */
  float scale;
  /* Layer of the atlas where the octahedron map is stored. */
  float layer;
};
BLI_STATIC_ASSERT_ALIGN(SphereProbeUvArea, 16)

/* Pixel read/write coordinates using pixel space. */
struct SphereProbePixelArea {
  /* Offset in pixel space to the start of the writing space of the octahedron map.
   * Note that the writing space is not the same as the sampling space as we have borders. */
  int2 offset;
  /* Size of the area in pixel that is covered by this probe mip-map. */
  int extent;
  /* Layer of the atlas where the octahedron map is stored. */
  int layer;
};
BLI_STATIC_ASSERT_ALIGN(SphereProbePixelArea, 16)

/** Mapping data to locate a reflection probe in texture. */
struct SphereProbeData {
  /** Transform to probe local position with non-uniform scaling. */
  float3x4 world_to_probe_transposed;

  packed_float3 location;
  /** Shape of the parallax projection. */
  float parallax_distance;
  LightProbeShape parallax_shape;
  LightProbeShape influence_shape;
  /** Influence factor based on the distance to the parallax shape. */
  float influence_scale;
  float influence_bias;

  SphereProbeUvArea atlas_coord;

  /**
   * Irradiance at the probe location encoded as spherical harmonics.
   * Only contain the average luminance. Used for cube-map normalization.
   */
  ReflectionProbeLowFreqLight low_freq_light;
};
BLI_STATIC_ASSERT_ALIGN(SphereProbeData, 16)

/** Viewport Display Pass. */
struct SphereProbeDisplayData {
  int probe_index;
  float display_size;
  float _pad0;
  float _pad1;
};
BLI_STATIC_ASSERT_ALIGN(SphereProbeDisplayData, 16)

/* Used for sphere probe spherical harmonics extraction. Output one for each thread-group
 * and do a sum afterward. Reduces bandwidth usage. */
struct SphereProbeHarmonic {
  float4 L0_M0;
  float4 L1_Mn1;
  float4 L1_M0;
  float4 L1_Mp1;
};
BLI_STATIC_ASSERT_ALIGN(SphereProbeHarmonic, 16)

struct SphereProbeSunLight {
  float4 direction;
  packed_float3 radiance;
  float _pad0;
};
BLI_STATIC_ASSERT_ALIGN(SphereProbeSunLight, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Planar Volume
 * \{ */

struct PlanarProbeData {
  /** Matrices used to render the planar capture. */
  float4x4 viewmat;
  float4x4 winmat;
  float4x4 wininv;
  /** Transform world to local position with influence distance as Z scale. */
  float3x4 world_to_object_transposed;
  /** World space plane normal. */
  packed_float3 normal;
  /** Layer in the planar capture textures used by this probe. */
  int layer_id;
};
BLI_STATIC_ASSERT_ALIGN(PlanarProbeData, 16)

/** Viewport Display Pass. */
struct PlanarProbeDisplayData {
  float4x4 plane_to_world;
  int probe_index;
  float _pad0;
  float _pad1;
  float _pad2;
};
BLI_STATIC_ASSERT_ALIGN(PlanarProbeDisplayData, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Probe Volume
 * \{ */

struct VolumeProbeData {
  /** World to non-normalized local grid space [0..size-1]. Stored transposed for compactness. */
  float3x4 world_to_grid_transposed;
  /** Number of bricks for this grid. */
  packed_int3 grid_size_padded;
  /** Index in brick descriptor list of the first brick of this grid. */
  int brick_offset;
  /** Biases to apply to the shading point in order to sample a valid probe. */
  float normal_bias;
  float view_bias;
  float facing_bias;
  int _pad1;
};
BLI_STATIC_ASSERT_ALIGN(VolumeProbeData, 16)

struct IrradianceBrick {
  /* Offset in pixel to the start of the data inside the atlas texture. */
  uint2 atlas_coord;
};
/** \note Stored packed as a uint. */
#define IrradianceBrickPacked uint

static inline IrradianceBrickPacked irradiance_brick_pack(IrradianceBrick brick)
{
  uint2 data = (uint2(brick.atlas_coord) & 0xFFFFu) << uint2(0u, 16u);
  IrradianceBrickPacked brick_packed = data.x | data.y;
  return brick_packed;
}

static inline IrradianceBrick irradiance_brick_unpack(IrradianceBrickPacked brick_packed)
{
  IrradianceBrick brick;
  brick.atlas_coord = (uint2(brick_packed) >> uint2(0u, 16u)) & uint2(0xFFFFu);
  return brick;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Baking structures
 * \{ */

struct SurfelRadiance {
  /* Actually stores radiance and world (sky) visibility. Stored normalized. */
  float4 front;
  float4 back;
  /* Accumulated weights per face. */
  float front_weight;
  float back_weight;
  float _pad0;
  float _pad1;
};
BLI_STATIC_ASSERT_ALIGN(SurfelRadiance, 16)

struct Surfel {
  /** World position of the surfel. */
  packed_float3 position;
  /** Previous surfel index in the ray link-list. Only valid after sorting. */
  int prev;
  /** World orientation of the surface. */
  packed_float3 normal;
  /** Next surfel index in the ray link-list. */
  int next;
  /** Surface albedo to apply to incoming radiance. */
  packed_float3 albedo_front;
  /** Distance along the ray direction for sorting. */
  float ray_distance;
  /** Surface albedo to apply to incoming radiance. */
  packed_float3 albedo_back;
  /** Cluster this surfel is assigned to. */
  int cluster_id;
  /** True if the light can bounce or be emitted by the surfel back face. */
  bool32_t double_sided;
  /** Surface receiver light set for light linking. */
  uint receiver_light_set;
  /** List index this surfel is in. */
  int list_id;
  /** Index of this surfel inside the sorted list. Allow access to previous and next surfel id. */
  int index_in_sorted_list;
  /** Surface radiance: Emission + Direct Lighting. */
  SurfelRadiance radiance_direct;
  /** Surface radiance: Indirect Lighting. Double buffered to avoid race conditions. */
  SurfelRadiance radiance_indirect[2];
};
BLI_STATIC_ASSERT_ALIGN(Surfel, 16)

struct CaptureInfoData {
  /** Grid size without padding. */
  packed_int3 irradiance_grid_size;
  /** True if the surface shader needs to write the surfel data. */
  bool32_t do_surfel_output;
  /** True if the surface shader needs to increment the surfel_len. */
  bool32_t do_surfel_count;
  /** Number of surfels inside the surfel buffer or the needed len. */
  uint surfel_len;
  /** Total number of a ray for light transportation. */
  float sample_count;
  /** 0 based sample index. */
  float sample_index;
  /** Transform of the light-probe object. */
  float4x4 irradiance_grid_local_to_world;
  /** Transform of the light-probe object. */
  float4x4 irradiance_grid_world_to_local;
  /** Transform vectors from world space to local space. Does not have location component. */
  /** TODO(fclem): This could be a float3x4 or a float3x3 if padded correctly. */
  float4x4 irradiance_grid_world_to_local_rotation;
  /** Scene bounds. Stored as min & max and as int for atomic operations. */
  int scene_bound_x_min;
  int scene_bound_y_min;
  int scene_bound_z_min;
  int scene_bound_x_max;
  int scene_bound_y_max;
  int scene_bound_z_max;
  /* Max intensity a ray can have. */
  float clamp_direct;
  float clamp_indirect;
  float _pad1;
  float _pad2;
  /** Minimum distance between a grid sample and a surface. Used to compute virtual offset. */
  float min_distance_to_surface;
  /** Maximum world scale offset an irradiance grid sample can be baked with. */
  float max_virtual_offset;
  /** Radius of surfels. */
  float surfel_radius;
  /** Capture options. */
  bool32_t capture_world_direct;
  bool32_t capture_world_indirect;
  bool32_t capture_visibility_direct;
  bool32_t capture_visibility_indirect;
  bool32_t capture_indirect;
  bool32_t capture_emission;
  int _pad0;
  /* World light probe atlas coordinate. */
  SphereProbeUvArea world_atlas_coord;
};
BLI_STATIC_ASSERT_ALIGN(CaptureInfoData, 16)

struct SurfelListInfoData {
  /** Size of the grid used to project the surfels into linked lists. */
  int2 ray_grid_size;
  /** Maximum number of list. Is equal to `ray_grid_size.x * ray_grid_size.y`. */
  int list_max;

  int list_prefix_sum;
};
BLI_STATIC_ASSERT_ALIGN(SurfelListInfoData, 16)

/** \} */

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
