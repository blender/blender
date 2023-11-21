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

shared uint tile_contains_ray_tracing;
shared uint tile_contains_horizon_scan;

/* Returns a blend factor between different tracing method. */
float ray_roughness_factor(RayTraceData raytrace, float roughness)
{
  return saturate(roughness * raytrace.roughness_mask_scale - raytrace.roughness_mask_bias);
}

void main()
{
  if (all(equal(gl_LocalInvocationID, uvec3(0)))) {
    /* Clear num_groups_x to 0 so that we can use it as counter in the compaction phase.
     * Note that these writes are subject to race condition, but we write the same value
     * from all work-groups. */
    ray_denoise_dispatch_buf.num_groups_x = 0u;
    ray_denoise_dispatch_buf.num_groups_y = 1u;
    ray_denoise_dispatch_buf.num_groups_z = 1u;
    ray_dispatch_buf.num_groups_x = 0u;
    ray_dispatch_buf.num_groups_y = 1u;
    ray_dispatch_buf.num_groups_z = 1u;
    horizon_dispatch_buf.num_groups_x = 0u;
    horizon_dispatch_buf.num_groups_y = 1u;
    horizon_dispatch_buf.num_groups_z = 1u;
    horizon_denoise_dispatch_buf.num_groups_x = 0u;
    horizon_denoise_dispatch_buf.num_groups_y = 1u;
    horizon_denoise_dispatch_buf.num_groups_z = 1u;

    /* Init shared variables. */
    tile_contains_ray_tracing = 0;
    tile_contains_horizon_scan = 0;
  }

  barrier();

  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  bool valid_texel = in_texture_range(texel, gbuf_header_tx);
  uint closure_bits = (!valid_texel) ? 0u : texelFetch(gbuf_header_tx, texel, 0).r;

  if (flag_test(closure_bits, uniform_buf.raytrace.closure_active)) {
    GBufferData gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_color_tx, texel);

    float roughness = 1.0;
    if (uniform_buf.raytrace.closure_active == eClosureBits(CLOSURE_REFLECTION)) {
      roughness = gbuf.reflection.roughness;
    }
    if (uniform_buf.raytrace.closure_active == eClosureBits(CLOSURE_REFRACTION)) {
      roughness = 0.0; /* TODO(fclem): Apparent roughness. For now, always raytrace. */
    }

    float ray_roughness_fac = ray_roughness_factor(uniform_buf.raytrace, roughness);
    if (ray_roughness_fac > 0.0) {
      /* We don't care about race condition here. */
      tile_contains_horizon_scan = 1;
    }
    if (ray_roughness_fac < 1.0) {
      /* We don't care about race condition here. */
      tile_contains_ray_tracing = 1;
    }
  }

  barrier();

  if (all(equal(gl_LocalInvocationID, uvec3(0)))) {
    ivec2 tile_co = ivec2(gl_WorkGroupID.xy);

    uint tile_mask = 0u;
    if (tile_contains_ray_tracing > 0) {
      tile_mask |= 1u << 0u;
    }
    if (tile_contains_horizon_scan > 0) {
      tile_mask |= 1u << 1u;
    }

    imageStore(tile_mask_img, tile_co, uvec4(tile_mask));
  }
}
