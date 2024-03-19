/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Load the extracted spherical harmonics from the world into the probe volume atlas.
 *
 * The whole thread group will load the same data and write a brick worth of data.
 */

void atlas_store(vec4 sh_coefficient, ivec2 atlas_coord, int layer)
{
  imageStore(irradiance_atlas_img,
             ivec3(atlas_coord, layer * IRRADIANCE_GRID_BRICK_SIZE) + ivec3(gl_LocalInvocationID),
             sh_coefficient);
}

void main()
{
  int brick_index = grids_infos_buf[grid_index].brick_offset;

  /* Brick coordinate in the destination atlas. */
  IrradianceBrick brick = irradiance_brick_unpack(bricks_infos_buf[brick_index]);
  ivec2 output_coord = ivec2(brick.atlas_coord);

  atlas_store(harmonic_buf.L0_M0, output_coord, 0);
  atlas_store(harmonic_buf.L1_Mn1, output_coord, 1);
  atlas_store(harmonic_buf.L1_M0, output_coord, 2);
  atlas_store(harmonic_buf.L1_Mp1, output_coord, 3);
}
