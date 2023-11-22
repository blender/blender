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
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 texel_fullres = texel * uniform_buf.raytrace.resolution_scale +
                        uniform_buf.raytrace.resolution_bias;

  /* Load Gbuffer. */
  GBufferData gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_color_tx, texel_fullres);

  /* Export normal. */
  /* TODO(fclem): Export the most visible normal. */
  vec3 N = gbuf.has_diffuse ? gbuf.diffuse.N : gbuf.reflection.N;
  if (is_zero(N)) {
    /* Avoid NaN. But should be fixed in any case. */
    N = vec3(1.0, 0.0, 0.0);
  }
  vec3 vN = drw_normal_world_to_view(N);
  imageStore(out_normal_img, texel, vec4(vN * 0.5 + 0.5, 0.0));

  /* Re-project radiance. */
  vec2 uv = (vec2(texel_fullres) + 0.5) / vec2(textureSize(depth_tx, 0).xy);
  float depth = texelFetch(depth_tx, texel_fullres, 0).r;
  vec3 P = drw_point_screen_to_world(vec3(uv, depth));

  vec3 ssP_prev = drw_ndc_to_screen(project_point(uniform_buf.raytrace.radiance_persmat, P));

  vec4 radiance = texture(in_radiance_tx, ssP_prev.xy);

  float luma = max(1e-8, reduce_max(radiance.rgb));
  radiance *= 1.0 - max(0.0, luma - uniform_buf.raytrace.brightness_clamp) / luma;

  imageStore(out_radiance_img, texel, radiance);
}
