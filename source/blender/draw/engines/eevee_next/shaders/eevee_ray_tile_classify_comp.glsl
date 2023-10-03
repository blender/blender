/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * This pass load Gbuffer data and output a mask of tiles to process.
 * This mask is then processed by the compaction phase.
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)

shared uint tile_contains_glossy_rays;

/* Returns a blend factor between different irradiance fetching method for reflections. */
float ray_glossy_factor(float roughness)
{
  /* TODO */
  return 1.0;
}

void main()
{
  if (all(equal(gl_LocalInvocationID, uvec3(0)))) {
    /* Clear num_groups_x to 0 so that we can use it as counter in the compaction phase.
     * Note that these writes are subject to race condition, but we write the same value
     * from all work-groups. */
    denoise_dispatch_buf.num_groups_x = 0u;
    denoise_dispatch_buf.num_groups_y = 1u;
    denoise_dispatch_buf.num_groups_z = 1u;
    ray_dispatch_buf.num_groups_x = 0u;
    ray_dispatch_buf.num_groups_y = 1u;
    ray_dispatch_buf.num_groups_z = 1u;

    /* Init shared variables. */
    tile_contains_glossy_rays = 0;
  }

  barrier();

  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  bool valid_texel = in_texture_range(texel, gbuf_header_tx);
  uint closure_bits = (!valid_texel) ? 0u : texelFetch(gbuf_header_tx, texel, 0).r;

  if (flag_test(closure_bits, uniform_buf.raytrace.closure_active)) {
    GBufferData gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_color_tx, texel);

    float roughness = (uniform_buf.raytrace.closure_active == CLOSURE_REFRACTION) ?
                          gbuf.refraction.roughness :
                          gbuf.reflection.roughness;

    if (ray_glossy_factor(roughness) > 0.0) {
      /* We don't care about race condition here. */
      tile_contains_glossy_rays = 1;
    }
  }

  barrier();

  if (all(equal(gl_LocalInvocationID, uvec3(0)))) {
    ivec2 tile_co = ivec2(gl_WorkGroupID.xy);

    uint tile_mask = 0u;
    if (tile_contains_glossy_rays > 0) {
      tile_mask = 1u;
    }

    imageStore(tile_mask_img, tile_co, uvec4(tile_mask));
  }
}
