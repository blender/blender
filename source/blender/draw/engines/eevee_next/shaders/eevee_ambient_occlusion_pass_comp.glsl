/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_horizon_scan_eval_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 extent = imageSize(in_normal_img).xy;
  if (any(greaterThanEqual(texel, extent))) {
    return;
  }

  vec2 uv = (vec2(texel) + vec2(0.5)) / vec2(extent);
  float depth = texelFetch(hiz_tx, texel, 0).r;

  if (depth == 1.0) {
    /* Do not trace for background */
    imageStore(out_ao_img, ivec3(texel, out_ao_img_layer_index), vec4(0.0));
    return;
  }

  vec3 vP = drw_point_screen_to_view(vec3(uv, depth));
  vec3 N = imageLoad(in_normal_img, ivec3(texel, in_normal_img_layer_index)).xyz;
  vec3 vN = drw_normal_world_to_view(N);

  vec2 noise;
  noise.x = interlieved_gradient_noise(vec2(texel), 3.0, 0.0);
  noise.y = utility_tx_fetch(utility_tx, vec2(texel), UTIL_BLUE_NOISE_LAYER).r;
  noise = fract(noise + sampling_rng_2D_get(SAMPLING_AO_U));

  HorizonScanResult scan = horizon_scan_eval(vP,
                                             vN,
                                             noise,
                                             uniform_buf.ao.pixel_size,
                                             uniform_buf.ao.distance,
                                             uniform_buf.ao.thickness_near,
                                             uniform_buf.ao.thickness_far,
                                             uniform_buf.ao.angle_bias,
                                             2,
                                             10,
                                             false,
                                             true);

  imageStore(out_ao_img, ivec3(texel, out_ao_img_layer_index), vec4(saturate(scan.result)));
}
