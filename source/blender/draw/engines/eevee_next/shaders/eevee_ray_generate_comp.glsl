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

  ivec2 texel_fullres = texel * uniform_buf.raytrace.resolution_scale + uniform_buf.raytrace.resolution_bias;

  bool valid_texel = in_texture_range(texel_fullres, stencil_tx);
  uint closure_bits = (!valid_texel) ? 0u : texelFetch(stencil_tx, texel_fullres, 0).r;

#if defined(RAYTRACE_REFLECT)
  ClosureReflection closure;
  eClosureBits closure_active = CLOSURE_REFLECTION;
  const int gbuf_layer = 0;
#elif defined(RAYTRACE_REFRACT)
  ClosureRefraction closure;
  eClosureBits closure_active = CLOSURE_REFRACTION;
  const int gbuf_layer = 1;
#endif

  if (!flag_test(closure_bits, closure_active)) {
    imageStore(out_ray_data_img, texel, vec4(0.0));
    return;
  }

  vec2 uv = (vec2(texel_fullres) + 0.5) / vec2(textureSize(stencil_tx, 0).xy);
  vec3 V = transform_direction(ViewMatrixInverse, get_view_vector_from_screen_uv(uv));
  vec2 noise = utility_tx_fetch(utility_tx, vec2(texel), UTIL_BLUE_NOISE_LAYER).rg;

  /* Load GBuffer data. */
  vec4 gbuffer_packed = texelFetch(gbuffer_closure_tx, ivec3(texel_fullres, gbuf_layer), 0);

  closure.N = gbuffer_normal_unpack(gbuffer_packed.xy);

#if defined(RAYTRACE_REFLECT)
  closure.roughness = gbuffer_packed.z;

#elif defined(RAYTRACE_REFRACT)
  if (gbuffer_is_refraction(gbuffer_packed)) {
    closure.roughness = gbuffer_packed.z;
    closure.ior = gbuffer_ior_unpack(gbuffer_packed.w);
  }
  else {
    /* Avoid producing incorrect ray directions. */
    closure.ior = 1.1;
    closure.roughness = 0.0;
  }
#endif

  float pdf;
  vec3 ray_direction = ray_generate_direction(noise.xy, closure, V, pdf);

#if defined(RAYTRACE_REFRACT)
  if (gbuffer_is_refraction(gbuffer_packed) && closure_active != CLOSURE_REFRACTION) {
    /* Discard incorrect rays. */
    pdf = 0.0;
  }
#endif

  /* Store inverse pdf to speedup denoising.
   * Limit to the smallest non-0 value that the format can encode.
   * Strangely it does not correspond to the IEEE spec. */
  float inv_pdf = (pdf == 0.0) ? 0.0 : max(6e-8, 1.0 / pdf);
  imageStore(out_ray_data_img, texel, vec4(ray_direction, inv_pdf));
}
