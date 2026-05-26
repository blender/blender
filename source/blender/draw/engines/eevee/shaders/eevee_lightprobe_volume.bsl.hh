/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_defines.hh"
#include "eevee_lightprobe_shared.hh"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_spherical_harmonics.bsl.hh"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

namespace eevee::lightprobe::volume {

/**
 * Returns world position of a volume lightprobe sample (center of cell).
 * Returned position take into account the half voxel padding on each sides.
 * `grid_local_to_world_mat` is the unmodified object matrix.
 * `grid_res` is the un-padded grid resolution.
 * `cell_coord` is the coordinate of the sample in [0..grid_res) range.
 */
float3 grid_sample_position(float4x4 grid_local_to_world_mat, int3 grid_res, int3 cell_coord)
{
  float3 ls_cell_pos = (float3(cell_coord + 1)) / float3(grid_res + 1);
  ls_cell_pos = ls_cell_pos * 2.0f - 1.0f;
  float3 ws_cell_pos = (grid_local_to_world_mat * float4(ls_cell_pos, 1.0f)).xyz;
  return ws_cell_pos;
}

/**
 * Return true if sample position is valid.
 * \a r_lP is the local position in grid units [0..grid_size).
 */
bool grid_local_coord(VolumeProbeData grid_data, float3 P, float3 &r_lP)
{
  /* Position in cell units. */
  /* NOTE: The vector-matrix multiplication swapped on purpose to cancel the matrix transpose. */
  float3 lP = (float4(P, 1.0f) * grid_data.world_to_grid_transposed).xyz;
  r_lP = clamp(lP, float3(0.5f), float3(grid_data.grid_size_padded) - 0.5f);
  /* Sample is valid if position wasn't clamped. */
  return all(equal(lP, r_lP));
}

int grid_brick_index_get(VolumeProbeData grid_data, int3 brick_coord)
{
  int3 grid_size_in_bricks = divide_ceil(grid_data.grid_size_padded,
                                         int3(IRRADIANCE_GRID_BRICK_SIZE - 1));
  int brick_index = grid_data.brick_offset;
  brick_index += brick_coord.x;
  brick_index += brick_coord.y * grid_size_in_bricks.x;
  brick_index += brick_coord.z * grid_size_in_bricks.x * grid_size_in_bricks.y;
  return brick_index;
}

/* Return cell corner from a corner ID [0..7]. */
int3 grid_cell_corner(int cell_corner_id)
{
  return (int3(cell_corner_id) >> int3(0, 1, 2)) & 1;
}

/**
 * Return the brick coordinate inside the grid.
 */
int3 grid_brick_coord(float3 lP)
{
  int3 brick_coord = int3((lP - 0.5f) / float(IRRADIANCE_GRID_BRICK_SIZE - 1));
  /* Avoid sampling adjacent bricks. */
  return max(brick_coord, int3(0));
}

/**
 * Return the local coordinated of the shading point inside the brick in unnormalized coordinate.
 */
float3 grid_brick_local_coord(float3 lP, int3 brick_coord)
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
float3 grid_bias_sample_coord(sampler3D atlas_tx,
                              VolumeProbeData grid_data,
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
  float cell_visibility = texelFetch(atlas_tx, vis_coord, 0)[vis_comp];
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
    int3 sample_position = grid_cell_corner(i);

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

}  // namespace eevee::lightprobe::volume

namespace eevee {

struct LightprobeVolumeRenderData {
  /* NOTE: Use uint instead of IrradianceBrickPacked because Metal needs to know the exact type. */
  [[storage(IRRADIANCE_BRICK_BUF_SLOT, read)]] const uint (&bricks_infos_buf)[];
  [[uniform(IRRADIANCE_GRID_BUF_SLOT)]] VolumeProbeData (&grids_infos_buf)[IRRADIANCE_GRID_MAX];
  [[sampler(VOLUME_PROBE_TEX_SLOT)]] sampler3D irradiance_atlas_tx;

  SphericalHarmonicL1<float4> sample_atlas(float3 atlas_coord) const
  {
    float4 texture_coord = float4(atlas_coord, float(IRRADIANCE_GRID_BRICK_SIZE)) /
                           float3(textureSize(irradiance_atlas_tx, 0)).xyzz;
    SphericalHarmonicL1<float4> sh;
    sh.L0.M0 = textureLod(irradiance_atlas_tx, texture_coord.xyz, 0.0f);
    texture_coord.z += texture_coord.w;
    sh.L1.Mn1 = textureLod(irradiance_atlas_tx, texture_coord.xyz, 0.0f);
    texture_coord.z += texture_coord.w;
    sh.L1.M0 = textureLod(irradiance_atlas_tx, texture_coord.xyz, 0.0f);
    texture_coord.z += texture_coord.w;
    sh.L1.Mp1 = textureLod(irradiance_atlas_tx, texture_coord.xyz, 0.0f);
    return sh;
  }

  SphericalHarmonicL1<float4> world() const
  {
    /* We need a 0.5 offset because of filtering. */
    return sample_atlas(float3(0.5001f));
  }

  SphericalHarmonicL1<float4> sample_probe_no_dithered_no_biases(float3 P,
                                                                 int grid_index_start = 0) const
  {
    float3 lP;
    int index = select_volume(P, grid_index_start, lP);
    VolumeProbeData grid_data = grids_infos_buf[index];
    return sample_probe(grid_data, lP);
  }

  SphericalHarmonicL1<float4> sample_probe_no_bias([[resource_table]] const Sampling &sampling,
                                                   float3 P) const
  {
    float3 lP;
    int index = select_volume_dithered(sampling, P, 0, lP);
    VolumeProbeData grid_data = grids_infos_buf[index];
    return sample_probe(grid_data, lP);
  }

  SphericalHarmonicL1<float4> sample_probe([[resource_table]] const Sampling &sampling,
                                           float3 P,
                                           float3 V,
                                           float3 Ng) const
  {
    float3 lP;
    int index = select_volume_dithered(sampling, P, 0, lP);
    VolumeProbeData grid_data = grids_infos_buf[index];
    return sample_probe_with_bias(grid_data, lP, Ng, V);
  }

 private:
  int select_volume(float3 P, int grid_index_start, float3 &lP) const
  {
    int index = -1;
#ifdef GPU_METAL
/* NOTE: Performs a chunked unroll to avoid the compiler unrolling the entire loop, avoiding
 * very high instruction counts and long compilation time. Full unroll results in 90k +
 * instructions. Chunked unroll is 5.1k instructions with reduced register pressure, while
 * retaining most of the benefits of unrolling. */
#  pragma clang loop unroll_count(16)
#endif
    for (int i = grid_index_start; i < IRRADIANCE_GRID_MAX; i++) {
      /* Last grid is tagged as invalid to stop the iteration. */
      if (grids_infos_buf[i].grid_size_padded.x == -1) {
        /* Sample the last grid instead. */
        index = i - 1;
        break;
      }

      /* If sample fall inside the grid, step out of the loop. */
      if (lightprobe::volume::grid_local_coord(grids_infos_buf[i], P, lP)) {
        index = i;
        break;
      }
    }
    return index;
  }

  int select_volume_dithered([[resource_table]] const Sampling &sampling,
                             float3 P,
                             int grid_index_start,
                             float3 &lP) const
  {
    int index = -1;
    float random = square(pcg4d(float4(P, sampling.rng_1D_get(SAMPLING_LIGHTPROBE))).x) * 0.75f;
#ifdef GPU_METAL
/* NOTE: Performs a chunked unroll to avoid the compiler unrolling the entire loop, avoiding
 * very high instruction counts and long compilation time. Full unroll results in 90k +
 * instructions. Chunked unroll is 5.1k instructions with reduced register pressure, while
 * retaining most of the benefits of unrolling. */
#  pragma clang loop unroll_count(16)
#endif
    for (int i = grid_index_start; i < IRRADIANCE_GRID_MAX; i++) {
      /* Last grid is tagged as invalid to stop the iteration. */
      if (grids_infos_buf[i].grid_size_padded.x == -1) {
        /* Sample the last grid instead. */
        index = i - 1;
        break;
      }

      /* If sample fall inside the grid, step out of the loop. */
      if (lightprobe::volume::grid_local_coord(grids_infos_buf[i], P, lP)) {
        index = i;
        float distance_to_border = reduce_min(
            min(lP, float3(grids_infos_buf[i].grid_size_padded) - lP));
        if (distance_to_border < 0.5f + random) {
          /* Try to sample another grid to get smooth transitions at borders. */
          continue;
        }
        break;
      }
    }
    return index;
  }

  SphericalHarmonicL1<float4> sample_probe_with_bias(VolumeProbeData grid_data,
                                                     float3 lP,
                                                     float3 Ng,
                                                     float3 V) const
  {
    float3x3 world_to_grid_transposed = to_float3x3(grid_data.world_to_grid_transposed);
    float3 lNg = safe_normalize(Ng * world_to_grid_transposed);
    float3 lV = safe_normalize(V * world_to_grid_transposed);

    /* Shading point bias. */
    lP += lNg * grid_data.normal_bias;
    lP += lV * grid_data.view_bias;

    return sample_probe(grid_data, lP, lNg);
  }

  SphericalHarmonicL1<float4> sample_probe(VolumeProbeData grid_data,
                                           float3 lP,
                                           float3 lNg = float3(0)) const
  {
    int3 brick_coord = lightprobe::volume::grid_brick_coord(lP);
    int brick_index = lightprobe::volume::grid_brick_index_get(grid_data, brick_coord);
    IrradianceBrick brick = irradiance_brick_unpack(bricks_infos_buf[brick_index]);

    float3 brick_lP = lightprobe::volume::grid_brick_local_coord(lP, brick_coord);

    /* Sampling point bias. */
    brick_lP = lightprobe::volume::grid_bias_sample_coord(
        irradiance_atlas_tx, grid_data, brick.atlas_coord, brick_lP, lNg);

    float3 atlas_coord = float3(float2(brick.atlas_coord), 0.0f) + brick_lP;

    return sample_atlas(atlas_coord);
  }
};

}  // namespace eevee
