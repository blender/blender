/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_horizon_scan_eval_lib.glsl)

void main()
{
  const uint tile_size = RAYTRACE_GROUP_SIZE;
  uvec2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  ivec2 texel = ivec2(gl_LocalInvocationID.xy + tile_coord * tile_size);

  ivec2 texel_fullres = texel * uniform_buf.raytrace.resolution_scale +
                        uniform_buf.raytrace.resolution_bias;

  ivec2 extent = textureSize(gbuf_header_tx, 0).xy;
  if (any(greaterThanEqual(texel_fullres, extent))) {
    return;
  }

  vec2 uv = (vec2(texel_fullres) + 0.5) * uniform_buf.raytrace.full_resolution_inv;
  float depth = texelFetch(hiz_tx, texel_fullres, 0).r;

  if (depth == 1.0) {
    /* Do not trace for background */
    imageStore(horizon_radiance_img, texel, vec4(FLT_11_11_10_MAX, 0.0));
    return;
  }

  GBufferReader gbuf = gbuffer_read(
      gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel_fullres);

  bool has_valid_closure = closure_index < gbuf.closure_count;
  if (!has_valid_closure) {
    imageStore(horizon_radiance_img, texel, vec4(FLT_11_11_10_MAX, 0.0));
    return;
  }

  HorizonScanContext ctx;
  ctx.closure = gbuffer_closure_get(gbuf, closure_index);
  ctx.closure.N = drw_normal_world_to_view(ctx.closure.N);

  vec3 vP = drw_point_screen_to_view(vec3(uv, depth));

  vec2 noise = utility_tx_fetch(utility_tx, vec2(texel), UTIL_BLUE_NOISE_LAYER).rg;
  noise = fract(noise + sampling_rng_2D_get(SAMPLING_AO_U));

  horizon_scan_eval(vP,
                    ctx,
                    noise,
                    uniform_buf.ao.pixel_size,
                    1.0e16,
                    uniform_buf.ao.thickness,
                    uniform_buf.ao.angle_bias,
                    8,
                    false);

  imageStore(horizon_radiance_img, texel, ctx.closure_result);
  imageStore(horizon_occlusion_img, texel, ctx.closure_result.wwww);
}
