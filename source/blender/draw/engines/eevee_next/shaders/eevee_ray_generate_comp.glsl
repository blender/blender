/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Generate Ray direction along with other data that are then used
 * by the next pass to trace the rays.
 */

#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ray_generate_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)

void main()
{
  const uint tile_size = RAYTRACE_GROUP_SIZE;
  uvec2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  ivec2 texel = ivec2(gl_LocalInvocationID.xy + tile_coord * tile_size);

  ivec2 texel_fullres = texel * uniform_buf.raytrace.resolution_scale +
                        uniform_buf.raytrace.resolution_bias;

  GBufferData gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_color_tx, texel_fullres);

#if defined(RAYTRACE_REFLECT)
  bool valid_pixel = gbuf.has_reflection;
#elif defined(RAYTRACE_REFRACT)
  bool valid_pixel = gbuf.has_refraction;
#endif

  if (!valid_pixel) {
    imageStore(out_ray_data_img, texel, vec4(0.0));
    return;
  }

  vec2 uv = (vec2(texel_fullres) + 0.5) / vec2(textureSize(gbuf_header_tx, 0).xy);
  vec3 V = transform_direction(ViewMatrixInverse, get_view_vector_from_screen_uv(uv));
  vec2 noise = utility_tx_fetch(utility_tx, vec2(texel), UTIL_BLUE_NOISE_LAYER).rg;

#if defined(RAYTRACE_REFLECT)
  ClosureReflection closure = gbuf.reflection;
#elif defined(RAYTRACE_REFRACT)
  ClosureRefraction closure = gbuf.refraction;
#endif

  float pdf;
  vec3 ray_direction = ray_generate_direction(noise.xy, closure, V, pdf);

  /* Store inverse pdf to speedup denoising.
   * Limit to the smallest non-0 value that the format can encode.
   * Strangely it does not correspond to the IEEE spec. */
  float inv_pdf = (pdf == 0.0) ? 0.0 : max(6e-8, 1.0 / pdf);
  imageStore(out_ray_data_img, texel, vec4(ray_direction, inv_pdf));
}
