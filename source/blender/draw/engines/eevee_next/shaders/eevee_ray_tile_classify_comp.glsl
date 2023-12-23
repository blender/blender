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

shared uint tile_contains_ray_tracing[3];
shared uint tile_contains_horizon_scan[3];

/* Returns a blend factor between different tracing method. */
float ray_roughness_factor(RayTraceData raytrace, float roughness)
{
  return saturate(roughness * raytrace.roughness_mask_scale - raytrace.roughness_mask_bias);
}

void main()
{
  if (all(equal(gl_LocalInvocationID, uvec3(0)))) {
    /* Init shared variables. */
    for (int i = 0; i < 3; i++) {
      tile_contains_ray_tracing[i] = 0;
      tile_contains_horizon_scan[i] = 0;
    }
  }

  barrier();

  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  bool valid_texel = in_texture_range(texel, gbuf_header_tx);

  if (valid_texel) {
    GBufferReader gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel);

    /* TODO(fclem): Arbitrary closure stack. */
    for (int i = 0; i < 3; i++) {
      float ray_roughness_fac;

      if (i == 0 && gbuf.has_diffuse) {
        /* Diffuse. */
        ray_roughness_fac = ray_roughness_factor(uniform_buf.raytrace, 1.0);
      }
      else if (i == 1 && gbuf.has_reflection) {
        /* Reflection. */
        ray_roughness_fac = ray_roughness_factor(uniform_buf.raytrace,
                                                 gbuf.data.reflection.roughness);
      }
      else if (i == 2 && gbuf.has_refraction) {
        /* Refraction. */
        ray_roughness_fac = 0.0; /* TODO(fclem): Apparent roughness. For now, always raytrace. */
      }
      else {
        continue;
      }

      /* We don't care about race condition here. */
      if (ray_roughness_fac > 0.0) {
        tile_contains_horizon_scan[i] = 1;
      }
      if (ray_roughness_fac < 1.0) {
        tile_contains_ray_tracing[i] = 1;
      }
    }
  }

  barrier();

  if (all(equal(gl_LocalInvocationID, uvec3(0)))) {
    ivec2 denoise_tile_co = ivec2(gl_WorkGroupID.xy);
    ivec2 tracing_tile_co = denoise_tile_co / uniform_buf.raytrace.resolution_scale;

    for (int i = 0; i < 3; i++) {
      if (tile_contains_ray_tracing[i] > 0) {
        imageStore(tile_raytrace_denoise_img, ivec3(denoise_tile_co, i), uvec4(1));
        imageStore(tile_raytrace_tracing_img, ivec3(tracing_tile_co, i), uvec4(1));
      }

      if (tile_contains_horizon_scan[i] > 0) {
        imageStore(tile_horizon_denoise_img, ivec3(denoise_tile_co, i), uvec4(1));
        imageStore(tile_horizon_tracing_img, ivec3(tracing_tile_co, i), uvec4(1));
      }
    }
  }
}
