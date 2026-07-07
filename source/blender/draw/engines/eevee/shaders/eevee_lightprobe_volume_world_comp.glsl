/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Load the extracted spherical harmonics from the world into the probe volume atlas.
 *
 * The whole thread group will load the same data and write a brick worth of data.
 */

#include "infos/eevee_lightprobe_volume_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_lightprobe_volume_world)

#include "eevee_spherical_harmonics_lib.glsl"

void atlas_store(float4 sh_coefficient, int2 atlas_coord, int layer)
{
  imageStore(irradiance_atlas_img,
             int3(atlas_coord, layer * IRRADIANCE_GRID_BRICK_SIZE) + int3(gl_LocalInvocationID),
             sh_coefficient);
}

void main()
{
  int brick_index = grids_infos_buf[grid_index].brick_offset;

  /* Brick coordinate in the destination atlas. */
  IrradianceBrick brick = irradiance_brick_unpack(bricks_infos_buf[brick_index]);
  int2 output_coord = int2(brick.atlas_coord);

  SphericalHarmonicL1 sh;
  sh.L0.M0 = harmonic_buf.L0_M0;
  sh.L1.Mn1 = harmonic_buf.L1_Mn1;
  sh.L1.M0 = harmonic_buf.L1_M0;
  sh.L1.Mp1 = harmonic_buf.L1_Mp1;

  sh = spherical_harmonics_dering(sh);

  atlas_store(sh.L0.M0, output_coord, 0);
  atlas_store(sh.L1.Mn1, output_coord, 1);
  atlas_store(sh.L1.M0, output_coord, 2);
  atlas_store(sh.L1.Mp1, output_coord, 3);
}
