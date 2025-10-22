/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Load an input light-grid cache texture into the atlas.
 * Takes care of dilating valid lighting into invalid samples and composite light-probes.
 *
 * Each thread group will load a brick worth of data and add the needed padding texels.
 */

#include "infos/eevee_lightprobe_volume_infos.hh"

#ifdef GLSL_CPP_STUBS
#  define IRRADIANCE_GRID_UPLOAD
#endif

COMPUTE_SHADER_CREATE_INFO(eevee_lightprobe_volume_load)

#include "eevee_lightprobe_volume_eval_lib.glsl"
#include "eevee_spherical_harmonics_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"

#include "gpu_shader_math_matrix_normalize_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

void atlas_store(float4 sh_coefficient, int2 atlas_coord, int layer)
{
  imageStore(irradiance_atlas_img,
             int3(atlas_coord, layer * IRRADIANCE_GRID_BRICK_SIZE) + int3(gl_LocalInvocationID),
             sh_coefficient);
}

SphericalHarmonicL1 irradiance_load(int3 input_coord)
{
  input_coord = clamp(input_coord, int3(0), textureSize(irradiance_a_tx, 0) - 1);

  SphericalHarmonicL1 sh;
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

void main()
{
  int brick_index = lightprobe_volume_grid_brick_index_get(grids_infos_buf[grid_index],
                                                           int3(gl_WorkGroupID));

  int3 grid_size = textureSize(irradiance_a_tx, 0);
  /* Brick coordinate in the source grid. */
  int3 brick_coord = int3(gl_WorkGroupID);
  /* Add padding border to allow bilinear filtering. */
  int3 texel_coord = brick_coord * (IRRADIANCE_GRID_BRICK_SIZE - 1) + int3(gl_LocalInvocationID);
  /* Add padding to the grid to allow interpolation to outside grid. */
  texel_coord -= 1;

  int3 input_coord = clamp(texel_coord, int3(0), grid_size - 1);

  bool is_padding_voxel = !all(equal(texel_coord, input_coord));

  /* Brick coordinate in the destination atlas. */
  IrradianceBrick brick = irradiance_brick_unpack(bricks_infos_buf[brick_index]);
  int2 output_coord = int2(brick.atlas_coord);

  SphericalHarmonicL1 sh_local;

  float validity = texelFetch(validity_tx, input_coord, 0).r;
  if (validity > dilation_threshold) {
    /* Grid sample is valid. Single load. */
    sh_local = irradiance_load(input_coord);
  }
  else {
    /* Grid sample is invalid. Dilate adjacent samples inside the search region. */
    /* NOTE: Still load the center sample and give it low weight in case there is not valid sample
     * in the neighborhood. */
    float weight_accum = 1e-8f;
    sh_local = spherical_harmonics_mul(irradiance_load(input_coord), weight_accum);
    int radius = int(dilation_radius);
    for (int x = -radius; x <= radius; x++) {
      for (int y = -radius; y <= radius; y++) {
        for (int z = -radius; z <= radius; z++) {
          if (x == 0 && y == 0 && z == 0) {
            continue;
          }
          int3 offset = int3(x, y, z);
          int3 neighbor_coord = input_coord + offset;
          float neighbor_validity = texelFetch(validity_tx, neighbor_coord, 0).r;
          /* Skip invalid neighbor samples. */
          if (neighbor_validity < dilation_threshold) {
            continue;
          }
          float dist_sqr = length_squared(float3(offset));
          if (dist_sqr > square(dilation_radius)) {
            continue;
          }
          float weight = 1.0f / dist_sqr;
          sh_local = spherical_harmonics_madd(irradiance_load(neighbor_coord), weight, sh_local);
          weight_accum += weight;
        }
      }
    }
    float inv_weight_accum = safe_rcp(weight_accum);
    sh_local = spherical_harmonics_mul(sh_local, inv_weight_accum);
  }

  /* Rotate Spherical Harmonic into world space. */
  float3x3 grid_to_world_rot = normalize(
      to_float3x3(grids_infos_buf[grid_index].world_to_grid_transposed));
  sh_local = spherical_harmonics_rotate(grid_to_world_rot, sh_local);

  SphericalHarmonicL1 sh_visibility;
  sh_visibility.L0.M0 = sh_local.L0.M0.aaaa;
  sh_visibility.L1.Mn1 = sh_local.L1.Mn1.aaaa;
  sh_visibility.L1.M0 = sh_local.L1.M0.aaaa;
  sh_visibility.L1.Mp1 = sh_local.L1.Mp1.aaaa;

  float3 P = lightprobe_volume_grid_sample_position(grid_local_to_world, grid_size, input_coord);

  SphericalHarmonicL1 sh_distant = lightprobe_volume_sample(P);

  if (is_padding_voxel) {
    /* Padding voxels just contain the distant lighting. */
    sh_local = sh_distant;
  }
  else {
    /* Mask distant lighting by local visibility. */
    sh_distant = spherical_harmonics_triple_product(sh_visibility, sh_distant);
    /* Apply intensity scaling. */
    sh_local = spherical_harmonics_mul(sh_local, grid_intensity_factor);
    /* Add local lighting to distant lighting. */
    sh_local = spherical_harmonics_add(sh_local, sh_distant);
  }

  sh_local = spherical_harmonics_dering(sh_local);

  atlas_store(sh_local.L0.M0, output_coord, 0);
  atlas_store(sh_local.L1.Mn1, output_coord, 1);
  atlas_store(sh_local.L1.M0, output_coord, 2);
  atlas_store(sh_local.L1.Mp1, output_coord, 3);

  if ((gl_LocalInvocationID.z % 4u) == 0u) {
    /* Encode 4 cells into one volume sample. */
    int4 cell_validity_bits = int4(0);
    /* Encode validity of each samples in the grid cell. */
    [[gpu::unroll]] for (int cell = 0; cell < 4; cell++)
    {
      for (int i = 0; i < 8; i++) {
        int3 sample_position = lightprobe_volume_grid_cell_corner(i);
        int3 coord_texel = texel_coord + int3(0, 0, cell) + sample_position;
        int3 coord_input = clamp(coord_texel, int3(0), grid_size - 1);
        float validity = texelFetch(validity_tx, coord_input, 0).r;
        bool is_padding_voxel = !all(equal(coord_texel, coord_input));
        if ((validity > validity_threshold) || is_padding_voxel) {
          cell_validity_bits[cell] |= (1 << i);
        }
      }
    }
    /* NOTE: We could use another sampler to reduce the memory overhead, but that would take
     * another sampler slot for forward materials. */
    atlas_store(float4(cell_validity_bits), output_coord, 4);
  }
}
