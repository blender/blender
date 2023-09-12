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

  ivec2 texel = min(ivec2(gl_GlobalInvocationID.xy), textureSize(stencil_tx, 0) - 1);

  eClosureBits closure_bits = eClosureBits(texelFetch(stencil_tx, texel, 0).r);

  if (flag_test(closure_bits, uniform_buf.raytrace.closure_active)) {
    int gbuffer_layer = uniform_buf.raytrace.closure_active == CLOSURE_REFRACTION ? 1 : 0;

    vec4 gbuffer_packed = texelFetch(gbuffer_closure_tx, ivec3(texel, gbuffer_layer), 0);
    float roughness = gbuffer_packed.z;

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
