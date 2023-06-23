
/**
 * Load an input lightgrid cache texture into the atlas.
 *
 * Each thread group will load a brick worth of data and add the needed padding texels.
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_lightprobe_lib.glsl)

void atlas_store(vec4 sh_coefficient, ivec2 atlas_coord, int layer)
{
  imageStore(irradiance_atlas_img,
             ivec3(atlas_coord, layer * IRRADIANCE_GRID_BRICK_SIZE) + ivec3(gl_LocalInvocationID),
             sh_coefficient);
}

void main()
{
  int brick_index = lightprobe_irradiance_grid_brick_index_get(grids_infos_buf[grid_index],
                                                               ivec3(gl_WorkGroupID));
  /* Brick coordinate in the source grid. */
  ivec3 brick_coord = ivec3(gl_WorkGroupID);
  /* Add padding border to allow bilinear filtering. */
  ivec3 texel_coord = brick_coord * (IRRADIANCE_GRID_BRICK_SIZE - 1) + ivec3(gl_LocalInvocationID);
  ivec3 input_coord = min(texel_coord, textureSize(irradiance_a_tx, 0) - 1);

  /* Brick coordinate in the destination atlas. */
  IrradianceBrick brick = irradiance_brick_unpack(bricks_infos_buf[brick_index]);
  ivec2 output_coord = ivec2(brick.atlas_coord);

  SphericalHarmonicL1 sh;
  sh.L0.M0 = texelFetch(irradiance_a_tx, input_coord, 0);
  sh.L1.Mn1 = texelFetch(irradiance_b_tx, input_coord, 0);
  sh.L1.M0 = texelFetch(irradiance_c_tx, input_coord, 0);
  sh.L1.Mp1 = texelFetch(irradiance_d_tx, input_coord, 0);

  /* Rotate Spherical Harmonic into world space. */
  mat3 world_to_grid_transposed = mat3(grids_infos_buf[grid_index].world_to_grid_transposed);
  mat3 rotation = normalize(world_to_grid_transposed);
  spherical_harmonics_L1_rotate(rotation, sh.L1);

  atlas_store(sh.L0.M0, output_coord, 0);
  atlas_store(sh.L1.Mn1, output_coord, 1);
  atlas_store(sh.L1.M0, output_coord, 2);
  atlas_store(sh.L1.Mp1, output_coord, 3);
}
