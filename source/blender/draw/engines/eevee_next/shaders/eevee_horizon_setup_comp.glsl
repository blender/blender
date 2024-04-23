/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * This pass reprojects the input radiance if needed, downsample it and output the matching normal.
 *
 * Dispatched as one thread for each trace resolution pixel.
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_colorspace_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 texel_fullres = texel * uniform_buf.raytrace.horizon_resolution_scale +
                        uniform_buf.raytrace.horizon_resolution_bias;

  /* Return early for padding threads so we can use imageStoreFast. */
  if (any(greaterThanEqual(texel, imageSize(out_radiance_img).xy))) {
    return;
  }

  /* Avoid loading texels outside texture range. */
  ivec2 extent = textureSize(gbuf_header_tx, 0).xy;
  texel_fullres = min(texel_fullres, extent - 1);

  /* Load Gbuffer. */
  GBufferReader gbuf = gbuffer_read(
      gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel_fullres);

  /* Export normal. */
  vec3 N = gbuf.surface_N;
  /* Background has invalid data. */
  /* FIXME: This is zero for opaque layer when we are processing the refraction layer. */
  if (is_zero(N)) {
    /* Avoid NaN. But should be fixed in any case. */
    N = vec3(1.0, 0.0, 0.0);
  }
  vec3 vN = drw_normal_world_to_view(N);
  /* Tag processed pixel in the normal buffer for denoising speed. */
  bool is_processed = gbuf.header != 0u;
  imageStore(out_normal_img, texel, vec4(vN * 0.5 + 0.5, float(is_processed)));

  /* Re-project radiance. */
  vec2 uv = (vec2(texel_fullres) + 0.5) / vec2(textureSize(depth_tx, 0).xy);
  float depth = texelFetch(depth_tx, texel_fullres, 0).r;
  vec3 P = drw_point_screen_to_world(vec3(uv, depth));

  vec3 ssP_prev = drw_ndc_to_screen(project_point(uniform_buf.raytrace.radiance_persmat, P));

  vec4 radiance = texture(in_radiance_tx, ssP_prev.xy);
  radiance = colorspace_brightness_clamp_max(radiance, uniform_buf.clamp.surface_indirect);

  imageStore(out_radiance_img, texel, radiance);
}
