/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Shader to convert cube-map to octahedral projection. */

#include "infos/eevee_lightprobe_sphere_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_lightprobe_sphere_remap)

#include "eevee_colorspace_lib.glsl"
#include "eevee_lightprobe_sphere_mapping_lib.glsl"
#include "eevee_spherical_harmonics_lib.glsl"

shared float4 local_radiance[gl_WorkGroupSize.x * gl_WorkGroupSize.y];

float triangle_solid_angle(float3 A, float3 B, float3 C)
{
  return 2.0f * atan(abs(dot(A, cross(B, C))), (1.0f + dot(B, C) + dot(A, C) + dot(A, B)));
}

float quad_solid_angle(float3 A, float3 B, float3 C, float3 D)
{
  return triangle_solid_angle(A, B, C) + triangle_solid_angle(C, B, D);
}

float octahedral_texel_solid_angle(int2 local_texel,
                                   SphereProbePixelArea write_co,
                                   SphereProbeUvArea sample_co)
{
  if (any(equal(local_texel, int2(write_co.extent - 1)))) {
    /* Do not weight these border pixels that are redundant. */
    return 0.0f;
  }
  /* Since we are putting texel centers on the edges of the octahedron, the shape of a texel can be
   * anything from a simple quad (at the Z=0 poles), to a 4 pointed start (at the Z=+-1 poles)
   * passing by arrow tail shapes (at the X=0 and Y=0 edges).
   *
   * But we can leverage the symmetries of the octahedral mapping. Given that all oddly shaped
   * texels are on the X=0 and Y=0 planes, we first fold all texels to the first quadrant.
   *
   * The texel footprint clipped to a quadrant is a well defined spherical quad. We then multiply
   * by the number of clipped shape the real texel sustains. This number is 2 at the X=0 and Y=0
   * edges (the arrow shaped pixels) and 4 at the Z=+-1 poles (4 pointed start shaped pixels).
   *
   * The sum of all texels solid angle should be 4 PI (area of sphere). */

  /* Wrap to bottom left quadrant. */
  int half_size = write_co.extent >> 1;
  int padded_size = write_co.extent - 2;
  int2 wrapped_texel;
  wrapped_texel.x = (local_texel.x >= half_size) ? (padded_size - local_texel.x) : local_texel.x;
  wrapped_texel.y = (local_texel.y >= half_size) ? (padded_size - local_texel.y) : local_texel.y;

  float2 texel_corner_v00 = float2(wrapped_texel) + float2(-0.5f, -0.5f);
  float2 texel_corner_v10 = float2(wrapped_texel) + float2(+0.5f, -0.5f);
  float2 texel_corner_v01 = float2(wrapped_texel) + float2(-0.5f, +0.5f);
  float2 texel_corner_v11 = float2(wrapped_texel) + float2(+0.5f, +0.5f);
  /* Clamp to well defined shape in spherical domain. */
  texel_corner_v00 = clamp(texel_corner_v00, float2(0.0f), float2(half_size - 1));
  texel_corner_v10 = clamp(texel_corner_v10, float2(0.0f), float2(half_size - 1));
  texel_corner_v01 = clamp(texel_corner_v01, float2(0.0f), float2(half_size - 1));
  texel_corner_v11 = clamp(texel_corner_v11, float2(0.0f), float2(half_size - 1));
  /* Convert to point on sphere. */
  float3 v00 = sphere_probe_texel_to_direction(texel_corner_v00, write_co, sample_co);
  float3 v10 = sphere_probe_texel_to_direction(texel_corner_v10, write_co, sample_co);
  float3 v01 = sphere_probe_texel_to_direction(texel_corner_v01, write_co, sample_co);
  float3 v11 = sphere_probe_texel_to_direction(texel_corner_v11, write_co, sample_co);
  /* The solid angle functions expect normalized vectors. */
  v00 = normalize(v00);
  v10 = normalize(v10);
  v01 = normalize(v01);
  v11 = normalize(v11);
  /* Compute solid angle of the spherical quad. */
  float texel_clipped_solid_angle = quad_solid_angle(v00, v10, v01, v11);
  /* Multiply by the symmetric halves that we omitted.
   * Also important to note that we avoid weighting the same pixel more than it's total sampled
   * footprint if it is duplicated in another pixel of the map. So border pixels do not require any
   * special treatment. Only the center cross needs it. */
  if (wrapped_texel.x == half_size - 1) {
    texel_clipped_solid_angle *= 2.0f;
  }
  if (wrapped_texel.y == half_size - 1) {
    texel_clipped_solid_angle *= 2.0f;
  }
  return texel_clipped_solid_angle;
}

void main()
{
  uint work_group_index = gl_NumWorkGroups.x * gl_WorkGroupID.y + gl_WorkGroupID.x;
  const uint local_index = gl_LocalInvocationIndex;
  constexpr uint group_size = gl_WorkGroupSize.x * gl_WorkGroupSize.y;

  SphereProbeUvArea world_coord = reinterpret_as_atlas_coord(world_coord_packed);
  SphereProbeUvArea sample_coord = reinterpret_as_atlas_coord(probe_coord_packed);
  SphereProbePixelArea write_coord = reinterpret_as_write_coord(write_coord_packed);

  /* Texel in probe. */
  int2 local_texel = int2(gl_GlobalInvocationID.xy);

  float2 wrapped_uv;
  float3 direction = sphere_probe_texel_to_direction(
      float2(local_texel), write_coord, sample_coord, wrapped_uv);
  float4 radiance_and_transmittance = texture(cubemap_tx, direction);
  float3 radiance = radiance_and_transmittance.xyz;

  float opacity = 1.0f - radiance_and_transmittance.a;

  /* Composite world into reflection probes. */
  bool is_world = all(equal(probe_coord_packed, world_coord_packed));
  if (!is_world && opacity != 1.0f) {
    float2 biased_uv = sphere_probe_miplvl_scale_bias(0.0f, world_coord, saturate(wrapped_uv));
    float2 world_uv = biased_uv * world_coord.scale + world_coord.offset;
    float4 world_radiance = textureLod(atlas_tx, float3(world_uv, world_coord.layer), 0.0f);
    radiance.rgb = mix(world_radiance.rgb, radiance.rgb, opacity);
  }

  float sun_threshold = uniform_buf.clamp.sun_threshold;
  float3 radiance_clamped = colorspace_brightness_clamp_max(radiance, sun_threshold);
  float3 radiance_sun = radiance - radiance_clamped;
  radiance = radiance_clamped;

  if (!any(greaterThanEqual(local_texel, int2(write_coord.extent)))) {
    float clamp_indirect = uniform_buf.clamp.surface_indirect;
    float3 out_radiance = colorspace_brightness_clamp_max(radiance, clamp_indirect);

    int3 texel = int3(local_texel + write_coord.offset, write_coord.layer);
    imageStore(atlas_img, texel, float4(out_radiance, 1.0f));
  }

  float sample_weight = octahedral_texel_solid_angle(local_texel, write_coord, sample_coord);

  if (extract_sun) {
    /* Parallel sum. Result is stored inside local_radiance[0]. */
    local_radiance[local_index] = radiance_sun.xyzz * sample_weight;
    /* OpenGL/Intel drivers have known issues where it isn't able to compile barriers inside for
     * loops. Unroll is needed as driver might decide to not unroll in shaders with more
     * complexity. */
    [[gpu::unroll]] for (uint i = 0; i < 10; i++)
    {
      barrier();
      uint stride = group_size >> (i + 1u);
      if (local_index < stride) {
        local_radiance[local_index] += local_radiance[local_index + stride];
      }
    }
    barrier();

    if (gl_LocalInvocationIndex == 0u) {
      out_sun[work_group_index].radiance = local_radiance[0].xyz;
    }
    barrier();

    /* Reusing local_radiance for directions. */
    auto &local_direction = local_radiance;

    local_direction[local_index] = float4(normalize(direction), 1.0f) * sample_weight *
                                   length(radiance_sun.xyz);
    /* OpenGL/Intel drivers have known issues where it isn't able to compile barriers inside for
     * loops. Unroll is needed as driver might decide to not unroll in shaders with more
     * complexity. */
    [[gpu::unroll]] for (uint i = 0; i < 10; i++)
    {
      barrier();
      uint stride = group_size >> (i + 1u);
      if (local_index < stride) {
        local_direction[local_index] += local_direction[local_index + stride];
      }
    }
    barrier();

    if (gl_LocalInvocationIndex == 0u) {
      out_sun[work_group_index].direction = local_direction[0];
    }
    barrier();
  }

  if (extract_sh) {
    /* Parallel sum. Result is stored inside local_radiance[0]. */
    local_radiance[local_index] = radiance.xyzz * sample_weight;
    /* OpenGL/Intel drivers have known issues where it isn't able to compile barriers inside for
     * loops. Unroll is needed as driver might decide to not unroll in shaders with more
     * complexity. */
    [[gpu::unroll]] for (uint i = 0; i < 10; i++)
    {
      barrier();
      uint stride = group_size >> (i + 1u);
      if (local_index < stride) {
        local_radiance[local_index] += local_radiance[local_index + stride];
      }
    }
    barrier();

    if (gl_LocalInvocationIndex == 0u) {
      /* Find the middle point of the whole thread-group. Use it as light vector.
       * Note that this is an approximation since the footprint of a thread-group is not
       * necessarily a convex polygons (with center of gravity at midpoint).
       * But the actual error introduce by this approximation is not perceivable. */
      /* FIXME(fclem): The error IS very perceivable for resolution lower than a quadrant. */
      int2 max_group_texel = local_texel + int2(gl_WorkGroupSize.xy);
      /* Min direction is the local direction since this is only ran by thread 0. */
      float3 min_direction = normalize(direction);
      float3 max_direction = normalize(
          sphere_probe_texel_to_direction(float2(max_group_texel), write_coord, sample_coord));
      float3 L = normalize(min_direction + max_direction);
      /* Convert radiance to spherical harmonics. */
      SphericalHarmonicL1 sh;
      sh.L0.M0 = float4(0.0f);
      sh.L1.Mn1 = float4(0.0f);
      sh.L1.M0 = float4(0.0f);
      sh.L1.Mp1 = float4(0.0f);
      /* TODO(fclem): Cleanup: Should spherical_harmonics_encode_signal_sample return a new sh
       * instead of adding to it? */
      spherical_harmonics_encode_signal_sample(L, local_radiance[0], sh);
      /* Outputs one SH for each thread-group. */
      out_sh[work_group_index].L0_M0 = sh.L0.M0;
      out_sh[work_group_index].L1_Mn1 = sh.L1.Mn1;
      out_sh[work_group_index].L1_M0 = sh.L1.M0;
      out_sh[work_group_index].L1_Mp1 = sh.L1.Mp1;
    }
  }
}
