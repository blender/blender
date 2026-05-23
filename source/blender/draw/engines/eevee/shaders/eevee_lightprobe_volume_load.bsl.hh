/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_global_ubo)

#include "eevee_lightprobe_volume.bsl.hh"
#include "eevee_spherical_harmonics.bsl.hh"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_matrix_normalize_lib.glsl"

namespace eevee::lightprobe::volume {

struct AtlasStore {
  [[image(0, write, VOLUME_PROBE_FORMAT)]] image3D irradiance_atlas_img;

  void store(float4 sh_coefficient, int2 atlas_coord, int layer, uint3 local_id)
  {
    imageStore(irradiance_atlas_img,
               int3(atlas_coord, layer * IRRADIANCE_GRID_BRICK_SIZE) + int3(local_id),
               sh_coefficient);
  }
};

struct LoadGrid {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;

  [[push_constant]] const float4x4 grid_local_to_world;
  [[push_constant]] const int grid_index;
  [[push_constant]] const int grid_start_index;
  [[push_constant]] const float validity_threshold;
  [[push_constant]] const float dilation_threshold;
  [[push_constant]] const float dilation_radius;
  [[push_constant]] const float grid_intensity_factor;

  [[sampler(0)]] sampler3D irradiance_a_tx;
  [[sampler(1)]] sampler3D irradiance_b_tx;
  [[sampler(2)]] sampler3D irradiance_c_tx;
  [[sampler(3)]] sampler3D irradiance_d_tx;
  [[sampler(4)]] sampler3D visibility_a_tx;
  [[sampler(5)]] sampler3D visibility_b_tx;
  [[sampler(7)]] sampler3D visibility_c_tx;
  [[sampler(8)]] sampler3D visibility_d_tx;

  [[sampler(9)]] sampler3D validity_tx;

  SphericalHarmonicL1<float4> irradiance_load(int3 input_coord) const
  {
    input_coord = clamp(input_coord, int3(0), textureSize(irradiance_a_tx, 0) - 1);

    SphericalHarmonicL1<float4> sh;
    sh.L0.M0 = texelFetch(irradiance_a_tx, input_coord, 0);
    sh.L1.Mn1 = texelFetch(irradiance_b_tx, input_coord, 0);
    sh.L1.M0 = texelFetch(irradiance_c_tx, input_coord, 0);
    sh.L1.Mp1 = texelFetch(irradiance_d_tx, input_coord, 0);

    /* Load visibility separately as it might not be in the same texture. */
    sh.L0.M0.a = texelFetch(visibility_a_tx, input_coord, 0).a;
    sh.L1.Mn1.a = texelFetch(visibility_b_tx, input_coord, 0).a;
    sh.L1.M0.a = texelFetch(visibility_c_tx, input_coord, 0).a;
    sh.L1.Mp1.a = texelFetch(visibility_d_tx, input_coord, 0).a;
    return sh;
  }
};

/**
 * Load an input light-grid cache texture into the atlas.
 * Takes care of dilating valid lighting into invalid samples and composite light-probes.
 *
 * Each thread group will load a brick worth of data and add the needed padding texels.
 */
[[compute]] [[local_size(IRRADIANCE_GRID_BRICK_SIZE,
                         IRRADIANCE_GRID_BRICK_SIZE,
                         IRRADIANCE_GRID_BRICK_SIZE)]]
void load_grid([[resource_table]] const LoadGrid &srt,
               [[resource_table]] const LightprobeVolumeRenderData &lvd,
               [[resource_table]] AtlasStore &atlas,
               [[global_invocation_id]] const uint3 global_id,
               [[work_group_id]] const uint3 group_id,
               [[local_invocation_id]] const uint3 local_id,
               [[local_invocation_index]] const uint local_index)
{
  int brick_index = lightprobe::volume::grid_brick_index_get(lvd.grids_infos_buf[srt.grid_index],
                                                             int3(group_id));

  int3 grid_size = textureSize(srt.irradiance_a_tx, 0);
  /* Brick coordinate in the source grid. */
  int3 brick_coord = int3(group_id);
  /* Add padding border to allow bilinear filtering. */
  int3 texel_coord = brick_coord * (IRRADIANCE_GRID_BRICK_SIZE - 1) + int3(local_id);
  /* Add padding to the grid to allow interpolation to outside grid. */
  texel_coord -= 1;

  int3 input_coord = clamp(texel_coord, int3(0), grid_size - 1);

  bool is_padding_voxel = !all(equal(texel_coord, input_coord));

  /* Brick coordinate in the destination atlas. */
  IrradianceBrick brick = irradiance_brick_unpack(lvd.bricks_infos_buf[brick_index]);
  int2 output_coord = int2(brick.atlas_coord);

  SphericalHarmonicL1<float4> sh_local;

  float validity = texelFetch(srt.validity_tx, input_coord, 0).r;
  if (validity > srt.dilation_threshold) {
    /* Grid sample is valid. Single load. */
    sh_local = srt.irradiance_load(input_coord);
  }
  else {
    /* Grid sample is invalid. Dilate adjacent samples inside the search region. */
    /* NOTE: Still load the center sample and give it low weight in case there is not valid sample
     * in the neighborhood. */
    float weight_accum = 1e-8f;
    sh_local = spherical_harmonics::mul(srt.irradiance_load(input_coord), weight_accum);
    int radius = int(srt.dilation_radius);
    for (int x = -radius; x <= radius; x++) {
      for (int y = -radius; y <= radius; y++) {
        for (int z = -radius; z <= radius; z++) {
          if (x == 0 && y == 0 && z == 0) {
            continue;
          }
          int3 offset = int3(x, y, z);
          int3 neighbor_coord = input_coord + offset;
          float neighbor_validity = texelFetch(srt.validity_tx, neighbor_coord, 0).r;
          /* Skip invalid neighbor samples. */
          if (neighbor_validity < srt.dilation_threshold) {
            continue;
          }
          float dist_sqr = length_squared(float3(offset));
          if (dist_sqr > square(srt.dilation_radius)) {
            continue;
          }
          float weight = 1.0f / dist_sqr;
          sh_local = spherical_harmonics::madd(
              srt.irradiance_load(neighbor_coord), weight, sh_local);
          weight_accum += weight;
        }
      }
    }
    float inv_weight_accum = safe_rcp(weight_accum);
    sh_local = spherical_harmonics::mul(sh_local, inv_weight_accum);
  }

  /* Rotate Spherical Harmonic into world space. */
  float3x3 grid_to_world_rot = normalize(
      to_float3x3(lvd.grids_infos_buf[srt.grid_index].world_to_grid_transposed));
  sh_local = spherical_harmonics::rotate(grid_to_world_rot, sh_local);

  SphericalHarmonicL1<float4> sh_visibility;
  sh_visibility.L0.M0 = sh_local.L0.M0.aaaa;
  sh_visibility.L1.Mn1 = sh_local.L1.Mn1.aaaa;
  sh_visibility.L1.M0 = sh_local.L1.M0.aaaa;
  sh_visibility.L1.Mp1 = sh_local.L1.Mp1.aaaa;

  float3 P = lightprobe::volume::grid_sample_position(
      srt.grid_local_to_world, grid_size, input_coord);

  SphericalHarmonicL1<float4> sh_distant = lvd.sample_probe_no_dithered_no_biases(
      P, srt.grid_start_index);

  if (is_padding_voxel) {
    /* Padding voxels just contain the distant lighting. */
    sh_local = sh_distant;
  }
  else {
    /* Mask distant lighting by local visibility. */
    sh_distant = spherical_harmonics::triple_product(sh_visibility, sh_distant);
    /* Apply intensity scaling. */
    sh_local = spherical_harmonics::mul(sh_local, srt.grid_intensity_factor);
    /* Add local lighting to distant lighting. */
    sh_local = spherical_harmonics::add(sh_local, sh_distant);
  }

  sh_local = spherical_harmonics::dering(sh_local);

  atlas.store(sh_local.L0.M0, output_coord, 0, local_id);
  atlas.store(sh_local.L1.Mn1, output_coord, 1, local_id);
  atlas.store(sh_local.L1.M0, output_coord, 2, local_id);
  atlas.store(sh_local.L1.Mp1, output_coord, 3, local_id);

  if ((local_id.z % 4u) == 0u) {
    /* Encode 4 cells into one volume sample. */
    int4 cell_validity_bits = int4(0);
    /* Encode validity of each samples in the grid cell. */
    for (int cell = 0; cell < 4; cell++) [[unroll]] {
      for (int i = 0; i < 8; i++) {
        int3 sample_position = lightprobe::volume::grid_cell_corner(i);
        int3 coord_texel = texel_coord + int3(0, 0, cell) + sample_position;
        int3 coord_input = clamp(coord_texel, int3(0), grid_size - 1);
        float validity = texelFetch(srt.validity_tx, coord_input, 0).r;
        bool is_padding_voxel = !all(equal(coord_texel, coord_input));
        if ((validity > srt.validity_threshold) || is_padding_voxel) {
          cell_validity_bits[cell] |= (1 << i);
        }
      }
    }
    /* NOTE: We could use another sampler to reduce the memory overhead, but that would take
     * another sampler slot for forward materials. */
    atlas.store(float4(cell_validity_bits), output_coord, 4, local_id);
  }
}

struct LoadWorld {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;

  [[storage(1, read)]] const SphereProbeHarmonic &harmonic_buf;

  [[push_constant]] const int grid_index;
};

/**
 * Load the extracted spherical harmonics from the world into the probe volume atlas.
 *
 * The whole thread group will load the same data and write a brick worth of data.
 */
[[compute]] [[local_size(IRRADIANCE_GRID_BRICK_SIZE,
                         IRRADIANCE_GRID_BRICK_SIZE,
                         IRRADIANCE_GRID_BRICK_SIZE)]]
void load_world([[resource_table]] const LoadWorld &srt,
                [[resource_table]] const LightprobeVolumeRenderData &lvd,
                [[resource_table]] AtlasStore &atlas,
                [[global_invocation_id]] const uint3 global_id,
                [[local_invocation_id]] const uint3 local_id,
                [[local_invocation_index]] const uint local_index)
{
  int brick_index = lvd.grids_infos_buf[srt.grid_index].brick_offset;

  /* Brick coordinate in the destination atlas. */
  IrradianceBrick brick = irradiance_brick_unpack(lvd.bricks_infos_buf[brick_index]);
  int2 output_coord = int2(brick.atlas_coord);

  SphericalHarmonicL1<float4> sh;
  sh.L0.M0 = srt.harmonic_buf.L0_M0;
  sh.L1.Mn1 = srt.harmonic_buf.L1_Mn1;
  sh.L1.M0 = srt.harmonic_buf.L1_M0;
  sh.L1.Mp1 = srt.harmonic_buf.L1_Mp1;

  sh = spherical_harmonics::dering(sh);

  atlas.store(sh.L0.M0, output_coord, 0, local_id);
  atlas.store(sh.L1.Mn1, output_coord, 1, local_id);
  atlas.store(sh.L1.M0, output_coord, 2, local_id);
  atlas.store(sh.L1.Mp1, output_coord, 3, local_id);
}

}  // namespace eevee::lightprobe::volume

PipelineCompute eevee_lightprobe_volume_load(eevee::lightprobe::volume::load_grid);
PipelineCompute eevee_lightprobe_volume_world(eevee::lightprobe::volume::load_world);
