/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Bilateral filtering of denoised ray-traced radiance.
 *
 * Dispatched at full-resolution using a tile list.
 *
 * Input: Temporally Stabilized Radiance, Stabilized Variance
 * Output: Denoised radiance
 *
 * Following "Stochastic All The Things: Raytracing in Hybrid Real-Time Rendering"
 * by Tomasz Stachowiak
 * https://www.ea.com/seed/news/seed-dd18-presentation-slides-raytracing
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_closure_lib.glsl)

float bilateral_depth_weight(vec3 center_N, vec3 center_P, vec3 sample_P)
{
  vec4 center_plane_eq = vec4(center_N, -dot(center_N, center_P));
  /* Only compare distance to the center plane formed by the normal. */
  float depth_delta = dot(center_plane_eq, vec4(sample_P, 1.0));
  /* TODO(fclem): Scene parameter. This is dependent on scene scale. */
  const float scale = 10000.0;
  float weight = exp2(-scale * square(depth_delta));
  return weight;
}

float bilateral_spatial_weight(float sigma, vec2 offset_from_center)
{
  /* From https://github.com/tranvansang/bilateral-filter/blob/master/fshader.frag */
  float fac = -1.0 / square(sigma);
  /* Take two standard deviation. */
  fac *= 2.0;
  float weight = exp2(fac * length_squared(offset_from_center));
  return weight;
}

float bilateral_normal_weight(vec3 center_N, vec3 sample_N)
{
  float facing_ratio = dot(center_N, sample_N);
  float weight = saturate(pow8f(facing_ratio));
  return weight;
}

/* In order to remove some more fireflies, "tone-map" the color samples during the accumulation. */
vec3 to_accumulation_space(vec3 color)
{
  return color / (1.0 + reduce_add(color));
}
vec3 from_accumulation_space(vec3 color)
{
  return color / (1.0 - reduce_add(color));
}

void main()
{
  const uint tile_size = RAYTRACE_GROUP_SIZE;
  uvec2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  ivec2 texel_fullres = ivec2(gl_LocalInvocationID.xy + tile_coord * tile_size);
  vec2 center_uv = (vec2(texel_fullres) + 0.5) * uniform_buf.raytrace.full_resolution_inv;

  float center_depth = texelFetch(depth_tx, texel_fullres, 0).r;
  vec3 center_P = drw_point_screen_to_world(vec3(center_uv, center_depth));

  GBufferReader gbuf = gbuffer_read(
      gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel_fullres);

  bool has_valid_closure = closure_index < gbuf.closure_count;
  if (!has_valid_closure) {
    /* Output nothing. This shouldn't even be loaded. */
    return;
  }

  ClosureUndetermined center_closure = gbuffer_closure_get(gbuf, closure_index);

  float roughness = closure_apparent_roughness_get(center_closure);
  float variance = imageLoad(in_variance_img, texel_fullres).r;
  vec3 in_radiance = imageLoad(in_radiance_img, texel_fullres).rgb;

  bool is_background = (center_depth == 0.0);
  bool is_smooth = (roughness < 0.05);
  bool is_low_variance = (variance < 0.05);
  bool is_high_variance = (variance > 0.5);

  /* Width of the box filter in pixels. */
  float filter_size_factor = saturate(roughness * 8.0);
  float filter_size = mix(0.0, 9.0, filter_size_factor);
  uint sample_count = uint(mix(1.0, 10.0, filter_size_factor) * (is_high_variance ? 1.5 : 1.0));

  if (is_smooth || is_background || is_low_variance) {
    /* Early out cases. */
    imageStore(out_radiance_img, texel_fullres, vec4(in_radiance, 0.0));
    return;
  }

  vec2 noise = interlieved_gradient_noise(vec2(texel_fullres) + 0.5, vec2(3, 5), vec2(0.0));
  noise += sampling_rng_2D_get(SAMPLING_RAYTRACE_W);

  vec3 accum_radiance = to_accumulation_space(in_radiance);
  float accum_weight = 1.0;
  /* We want to resize the blur depending on the roughness and keep the amount of sample low.
   * So we do a random sampling around the center point. */
  for (uint i = 0u; i < sample_count; i++) {
    /* Essentially a box radius overtime. */
    vec2 offset_f = (fract(hammersley_2d(i, sample_count) + noise) - 0.5) * filter_size;
    ivec2 offset = ivec2(floor(offset_f + 0.5));

    ivec2 sample_texel = texel_fullres + offset;
    ivec3 sample_tile = ivec3(sample_texel / RAYTRACE_GROUP_SIZE, closure_index);
    /* Make sure the sample has been processed and do not contain garbage data. */
    if (imageLoad(tile_mask_img, sample_tile).r == 0u) {
      continue;
    }

    float sample_depth = texelFetch(depth_tx, sample_texel, 0).r;
    vec2 sample_uv = (vec2(sample_texel) + 0.5) * uniform_buf.raytrace.full_resolution_inv;
    vec3 sample_P = drw_point_screen_to_world(vec3(sample_uv, sample_depth));

    /* Background case. */
    if (sample_depth == 0.0) {
      continue;
    }

    vec3 radiance = imageLoad(in_radiance_img, sample_texel).rgb;

    /* Do not gather unprocessed pixels. */
    if (all(equal(radiance, FLT_11_11_10_MAX))) {
      continue;
    }

    GBufferReader sample_gbuf = gbuffer_read(
        gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, sample_texel);

    if (closure_index >= sample_gbuf.closure_count) {
      continue;
    }

    ClosureUndetermined sample_closure = gbuffer_closure_get(sample_gbuf, closure_index);

    float depth_weight = bilateral_depth_weight(center_closure.N, center_P, sample_P);
    float spatial_weight = bilateral_spatial_weight(filter_size, vec2(offset));
    float normal_weight = bilateral_normal_weight(center_closure.N, sample_closure.N);
    float weight = depth_weight * spatial_weight * normal_weight;

    accum_radiance += to_accumulation_space(radiance) * weight;
    accum_weight += weight;
  }

  vec3 out_radiance = accum_radiance * safe_rcp(accum_weight);
  out_radiance = from_accumulation_space(out_radiance);

  imageStore(out_radiance_img, texel_fullres, vec4(out_radiance, 0.0));
}
