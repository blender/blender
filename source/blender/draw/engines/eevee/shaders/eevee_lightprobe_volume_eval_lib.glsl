/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * The resources expected to be defined are:
 * - grids_infos_buf
 * - bricks_infos_buf
 * - irradiance_atlas_tx
 * Needed for sampling (not for upload):
 * - util_tx
 * - sampling_buf
 */

#include "eevee_lightprobe_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_spherical_harmonics_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"

/**
 * Return the brick coordinate inside the grid.
 */
int3 lightprobe_volume_grid_brick_coord(float3 lP)
{
  int3 brick_coord = int3((lP - 0.5f) / float(IRRADIANCE_GRID_BRICK_SIZE - 1));
  /* Avoid sampling adjacent bricks. */
  return max(brick_coord, int3(0));
}

/**
 * Return the local coordinated of the shading point inside the brick in unnormalized coordinate.
 */
float3 lightprobe_volume_grid_brick_local_coord(VolumeProbeData grid_data,
                                                float3 lP,
                                                int3 brick_coord)
{
  /* Avoid sampling adjacent bricks around the origin. */
  lP = max(lP, float3(0.5f));
  /* Local position inside the brick (still in grid sample spacing unit). */
  float3 brick_lP = lP - float3(brick_coord) * float(IRRADIANCE_GRID_BRICK_SIZE - 1);
  return brick_lP;
}

/**
 * Return the biased local brick local coordinated.
 */
float3 lightprobe_volume_grid_bias_sample_coord(VolumeProbeData grid_data,
                                                uint2 brick_atlas_coord,
                                                float3 brick_lP,
                                                float3 lNg)
{
  /* A cell is the interpolation region between 8 texels. */
  float3 cell_lP = brick_lP - 0.5f;
  float3 cell_start = floor(cell_lP);
  float3 cell_fract = cell_lP - cell_start;

  /* NOTE(fclem): Use uint to avoid signed int modulo. */
  uint vis_comp = uint(cell_start.z) % 4u;
  /* Visibility is stored after the irradiance. */
  int3 vis_coord = int3(int2(brick_atlas_coord), IRRADIANCE_GRID_BRICK_SIZE * 4) +
                   int3(cell_start);
  /* Visibility is stored packed 1 cell per channel. */
  vis_coord.z -= int(vis_comp);
  float cell_visibility = texelFetch(irradiance_atlas_tx, vis_coord, 0)[vis_comp];
  int cell_visibility_bits = int(cell_visibility);
  /**
   * References:
   *
   * "Probe-based lighting, strand-based hair system, and physical hair shading in Unity's Enemies"
   * by Francesco Cifariello Ciardi, Lasse Jon Fuglsang Pedersen and John Parsaie.
   *
   * "Multi-Scale Global Illumination in Quantum Break"
   * by Ari Silvennoinen and Ville Timonen.
   *
   * "Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields"
   * by Morgan McGuire.
   */
  float trilinear_weights[8];
  float total_weight = 0.0f;
  for (int i = 0; i < 8; i++) {
    int3 sample_position = lightprobe_volume_grid_cell_corner(i);

    float3 trilinear = select(
        1.0f - cell_fract, cell_fract, greaterThan(sample_position, int3(0)));
    float positional_weight = trilinear.x * trilinear.y * trilinear.z;

    float len;
    float3 corner_vec = float3(sample_position) - cell_fract;
    float3 corner_dir = normalize_and_get_length(corner_vec, len);
    float cos_theta = (len > 1e-8f) ? dot(lNg, corner_dir) : 1.0f;
    float geometry_weight = saturate(cos_theta * 0.5f + 0.5f);

    float validity_weight = float((cell_visibility_bits >> i) & 1);

    /* Biases. See McGuire's presentation. */
    positional_weight += 0.001f;
    geometry_weight = square(geometry_weight) + 0.2f + grid_data.facing_bias;

    trilinear_weights[i] = saturate(positional_weight * geometry_weight * validity_weight);
    total_weight += trilinear_weights[i];
  }
  float total_weight_inv = safe_rcp(total_weight);

  float3 trilinear_coord = float3(0.0f);
  for (int i = 0; i < 8; i++) {
    float3 sample_position = float3((int3(i) >> int3(0, 1, 2)) & 1);
    trilinear_coord += sample_position * trilinear_weights[i] * total_weight_inv;
  }
  /* Replace sampling coordinates with manually weighted trilinear coordinates. */
  return 0.5f + cell_start + trilinear_coord;
}

SphericalHarmonicL1 lightprobe_volume_sample_atlas(sampler3D atlas_tx, float3 atlas_coord)
{
  float4 texture_coord = float4(atlas_coord, float(IRRADIANCE_GRID_BRICK_SIZE)) /
                         float3(textureSize(atlas_tx, 0)).xyzz;
  SphericalHarmonicL1 sh;
  sh.L0.M0 = textureLod(atlas_tx, texture_coord.xyz, 0.0f);
  texture_coord.z += texture_coord.w;
  sh.L1.Mn1 = textureLod(atlas_tx, texture_coord.xyz, 0.0f);
  texture_coord.z += texture_coord.w;
  sh.L1.M0 = textureLod(atlas_tx, texture_coord.xyz, 0.0f);
  texture_coord.z += texture_coord.w;
  sh.L1.Mp1 = textureLod(atlas_tx, texture_coord.xyz, 0.0f);
  return sh;
}

SphericalHarmonicL1 lightprobe_volume_sample(
    sampler3D atlas_tx, float3 P, float3 V, float3 Ng, const bool do_bias)
{
  float3 lP;
  int index = -1;
  int i = 0;
#ifdef IRRADIANCE_GRID_UPLOAD
  i = grid_start_index;
#endif
#ifdef IRRADIANCE_GRID_SAMPLING
  float random = square(pcg4d(float4(P, sampling_rng_1D_get(SAMPLING_LIGHTPROBE))).x) * 0.75f;
#endif
#ifdef GPU_METAL
/* NOTE: Performs a chunked unroll to avoid the compiler unrolling the entire loop, avoiding
 * very high instruction counts and long compilation time. Full unroll results in 90k +
 * instructions. Chunked unroll is 5.1k instructions with reduced register pressure, while
 * retaining most of the benefits of unrolling. */
#  pragma clang loop unroll_count(16)
#endif
  for (; i < IRRADIANCE_GRID_MAX; i++) {
    /* Last grid is tagged as invalid to stop the iteration. */
    if (grids_infos_buf[i].grid_size_padded.x == -1) {
      /* Sample the last grid instead. */
      index = i - 1;
      break;
    }

    /* If sample fall inside the grid, step out of the loop. */
    if (lightprobe_volume_grid_local_coord(grids_infos_buf[i], P, lP)) {
      index = i;
#ifdef IRRADIANCE_GRID_SAMPLING
      float distance_to_border = reduce_min(
          min(lP, float3(grids_infos_buf[i].grid_size_padded) - lP));
      if (distance_to_border < 0.5f + random) {
        /* Try to sample another grid to get smooth transitions at borders. */
        continue;
      }
#endif
      break;
    }
  }

  VolumeProbeData grid_data = grids_infos_buf[index];

  float3x3 world_to_grid_transposed = to_float3x3(grid_data.world_to_grid_transposed);
  float3 lNg = safe_normalize(Ng * world_to_grid_transposed);
  float3 lV = safe_normalize(V * world_to_grid_transposed);

  if (do_bias) {
    /* Shading point bias. */
    lP += lNg * grid_data.normal_bias;
    lP += lV * grid_data.view_bias;
  }
  else {
    lNg = float3(0.0f);
  }

  int3 brick_coord = lightprobe_volume_grid_brick_coord(lP);
  int brick_index = lightprobe_volume_grid_brick_index_get(grid_data, brick_coord);
  IrradianceBrick brick = irradiance_brick_unpack(bricks_infos_buf[brick_index]);

  float3 brick_lP = lightprobe_volume_grid_brick_local_coord(grid_data, lP, brick_coord);

  /* Sampling point bias. */
  brick_lP = lightprobe_volume_grid_bias_sample_coord(grid_data, brick.atlas_coord, brick_lP, lNg);

  float3 atlas_coord = float3(float2(brick.atlas_coord), 0.0f) + brick_lP;

  return lightprobe_volume_sample_atlas(atlas_tx, atlas_coord);
}

SphericalHarmonicL1 lightprobe_volume_world()
{
  /* We need a 0.5 offset because of filtering. */
  return lightprobe_volume_sample_atlas(irradiance_atlas_tx, float3(0.5001f));
}

SphericalHarmonicL1 lightprobe_volume_sample(float3 P)
{
  return lightprobe_volume_sample(irradiance_atlas_tx, P, float3(0), float3(0), false);
}

SphericalHarmonicL1 lightprobe_volume_sample(float3 P, float3 V, float3 Ng)
{
  return lightprobe_volume_sample(irradiance_atlas_tx, P, V, Ng, true);
}
