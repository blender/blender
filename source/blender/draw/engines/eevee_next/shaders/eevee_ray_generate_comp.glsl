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
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)

void main()
{
  const uint tile_size = RAYTRACE_GROUP_SIZE;
  uvec2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  ivec2 texel = ivec2(gl_LocalInvocationID.xy + tile_coord * tile_size);

  ivec2 texel_fullres = texel * uniform_buf.raytrace.resolution_scale +
                        uniform_buf.raytrace.resolution_bias;

  GBufferReader gbuf = gbuffer_read(
      gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel_fullres);

  bool valid_pixel = closure_index < gbuf.closure_count;
  if (!valid_pixel) {
    imageStore(out_ray_data_img, texel, vec4(0.0));
    return;
  }

  vec2 uv = (vec2(texel_fullres) + 0.5) / vec2(textureSize(gbuf_header_tx, 0).xy);
  vec3 P = drw_point_screen_to_world(vec3(uv, 0.5));
  vec3 V = drw_world_incident_vector(P);
  vec2 noise = utility_tx_fetch(utility_tx, vec2(texel), UTIL_BLUE_NOISE_LAYER).rg;
  noise = fract(noise + sampling_rng_2D_get(SAMPLING_RAYTRACE_U));

  BsdfSample samp = ray_generate_direction(noise.xy, gbuffer_closure_get(gbuf, closure_index), V);

  /* Store inverse pdf to speedup denoising.
   * Limit to the smallest non-0 value that the format can encode.
   * Strangely it does not correspond to the IEEE spec. */
  float inv_pdf = (samp.pdf == 0.0) ? 0.0 : max(6e-8, 1.0 / samp.pdf);
  imageStore(out_ray_data_img, texel, vec4(samp.direction, inv_pdf));
}
