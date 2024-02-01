/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Temporal Reprojection and accumulation of denoised ray-traced radiance.
 *
 * Dispatched at full-resolution using a tile list.
 *
 * Input: Spatially denoised radiance, Variance, Hit depth
 * Output: Stabilized Radiance, Stabilized Variance
 *
 * Following "Stochastic All The Things: Raytracing in Hybrid Real-Time Rendering"
 * by Tomasz Stachowiak
 * https://www.ea.com/seed/news/seed-dd18-presentation-slides-raytracing
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_colorspace_lib.glsl)

struct LocalStatistics {
  vec3 mean;
  vec3 moment;
  vec3 variance;
  vec3 deviation;
  vec3 clamp_min;
  vec3 clamp_max;
};

LocalStatistics local_statistics_get(ivec2 texel, vec3 center_radiance)
{
  vec3 center_radiance_YCoCg = colorspace_YCoCg_from_scene_linear(center_radiance);

  /* Build Local statistics (slide 46). */
  LocalStatistics result;
  result.mean = center_radiance_YCoCg;
  result.moment = square(center_radiance_YCoCg);
  float weight_accum = 1.0;

  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++) {
      if (x == 0 && y == 0) {
        continue;
      }

      ivec2 neighbor_texel = texel + ivec2(x, y);
      if (!in_image_range(neighbor_texel, in_radiance_img)) {
        continue;
      }
      vec3 radiance = imageLoad(in_radiance_img, neighbor_texel).rgb;
      /* Exclude unprocessed pixels. */
      if (all(equal(radiance, FLT_11_11_10_MAX))) {
        continue;
      }

      /* Weight corners less to avoid box artifacts.
       * Same idea as in "High Quality Temporal Supersampling" by Brian Karis at SIGGRAPH 2014
       * (Slide 32) Simple clamp to min/max of 8 neighbors results in 3x3 box artifacts. */
      float weight = (x == y) ? 0.25 : 1.0;
      /* Use YCoCg for clamping and accumulation to avoid color shift artifacts. */
      vec3 radiance_YCoCg = colorspace_YCoCg_from_scene_linear(radiance.rgb);
      result.mean += radiance_YCoCg;
      result.moment += square(radiance_YCoCg);
      weight_accum += 1.0;
    }
  }
  float inv_weight = safe_rcp(weight_accum);
  result.mean *= inv_weight;
  result.moment *= inv_weight;
  result.variance = abs(result.moment - square(result.mean));
  result.deviation = max(vec3(1e-4), sqrt(result.variance));
  result.clamp_min = result.mean - result.deviation;
  result.clamp_max = result.mean + result.deviation;
  return result;
}

vec4 bilinear_weights_from_subpixel_coord(vec2 co)
{
  /* From top left in clockwise order. */
  vec4 weights;
  weights.x = (1.0 - co.x) * co.y;
  weights.y = co.x * co.y;
  weights.z = co.x * (1.0 - co.y);
  weights.w = (1.0 - co.x) * (1.0 - co.y);
  return weights;
}

vec4 radiance_history_fetch(ivec2 texel, float bilinear_weight)
{
  /* Out of history view. Return sample without weight. */
  if (!in_texture_range(texel, radiance_history_tx)) {
    return vec4(0.0);
  }

  ivec3 history_tile = ivec3(texel / RAYTRACE_GROUP_SIZE, closure_index);
  /* Fetch previous tilemask to avoid loading invalid data. */
  bool is_valid_history = texelFetch(tilemask_history_tx, history_tile, 0).r != 0;
  /* Exclude unprocessed pixels. */
  if (!is_valid_history) {
    return vec4(0.0);
  }
  vec3 history_radiance = texelFetch(radiance_history_tx, texel, 0).rgb;
  /* Exclude unprocessed pixels. */
  if (all(equal(history_radiance, FLT_11_11_10_MAX))) {
    return vec4(0.0);
  }
  return vec4(history_radiance * bilinear_weight, bilinear_weight);
}

vec4 radiance_history_sample(vec3 P, LocalStatistics local)
{
  vec2 uv = project_point(uniform_buf.raytrace.history_persmat, P).xy * 0.5 + 0.5;

  /* FIXME(fclem): Find why we need this half pixel offset. */
  vec2 texel_co = uv * vec2(textureSize(radiance_history_tx, 0).xy) - 0.5;
  vec4 bilinear_weights = bilinear_weights_from_subpixel_coord(fract(texel_co));
  ivec2 texel = ivec2(floor(texel_co));

  /* Radiance needs to be manually interpolated because any pixel might contain invalid data. */
  vec4 history_radiance;
  history_radiance = radiance_history_fetch(texel + ivec2(0, 1), bilinear_weights.x);
  history_radiance += radiance_history_fetch(texel + ivec2(1, 1), bilinear_weights.y);
  history_radiance += radiance_history_fetch(texel + ivec2(1, 0), bilinear_weights.z);
  history_radiance += radiance_history_fetch(texel + ivec2(0, 0), bilinear_weights.w);

  /* Use YCoCg for clamping and accumulation to avoid color shift artifacts. */
  vec4 history_radiance_YCoCg;
  history_radiance_YCoCg.rgb = colorspace_YCoCg_from_scene_linear(history_radiance.rgb);
  history_radiance_YCoCg.a = history_radiance.a;

  /* Weighted contribution (slide 46). */
  vec3 dist = abs(history_radiance_YCoCg.rgb - local.mean) / local.deviation;
  float weight = exp2(-4.0 * dot(dist, vec3(1.0)));

  return history_radiance_YCoCg * weight;
}

vec2 variance_history_sample(vec3 P)
{
  vec2 uv = project_point(uniform_buf.raytrace.history_persmat, P).xy * 0.5 + 0.5;

  if (!in_range_exclusive(uv, vec2(0.0), vec2(1.0))) {
    /* Out of history view. Return sample without weight. */
    return vec2(0.0);
  }

  float history_variance = texture(variance_history_tx, uv).r;

  ivec2 history_texel = ivec2(floor(uv * vec2(textureSize(variance_history_tx, 0).xy)));
  ivec3 history_tile = ivec3(history_texel / RAYTRACE_GROUP_SIZE, closure_index);
  /* Fetch previous tilemask to avoid loading invalid data. */
  bool is_valid_history = texelFetch(tilemask_history_tx, history_tile, 0).r != 0;

  if (is_valid_history) {
    return vec2(history_variance, 1.0);
  }
  return vec2(0.0);
}

void main()
{
  const uint tile_size = RAYTRACE_GROUP_SIZE;
  uvec2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  ivec2 texel_fullres = ivec2(gl_LocalInvocationID.xy + tile_coord * tile_size);
  vec2 uv = (vec2(texel_fullres) + 0.5) * uniform_buf.raytrace.full_resolution_inv;

  float in_variance = imageLoad(in_variance_img, texel_fullres).r;
  vec3 in_radiance = imageLoad(in_radiance_img, texel_fullres).rgb;

  if (all(equal(in_radiance, FLT_11_11_10_MAX))) {
    /* Early out on pixels that were marked unprocessed by the previous pass. */
    imageStore(out_radiance_img, texel_fullres, vec4(FLT_11_11_10_MAX, 0.0));
    imageStore(out_variance_img, texel_fullres, vec4(0.0));
    return;
  }

  LocalStatistics local = local_statistics_get(texel_fullres, in_radiance);

  /* Radiance. */

  /* Surface reprojection. */
  /* TODO(fclem): Use per pixel velocity. Is this worth it? */
  float scene_depth = texelFetch(depth_tx, texel_fullres, 0).r;
  vec3 P = drw_point_screen_to_world(vec3(uv, scene_depth));
  vec4 history_radiance = radiance_history_sample(P, local);
  /* Reflection reprojection. */
  float hit_depth = imageLoad(hit_depth_img, texel_fullres).r;
  vec3 P_hit = drw_point_screen_to_world(vec3(uv, hit_depth));
  history_radiance += radiance_history_sample(P_hit, local);
  /* Finalize accumulation. */
  history_radiance *= safe_rcp(history_radiance.w);
  /* Clamp resulting history radiance (slide 47). */
  history_radiance.rgb = clamp(history_radiance.rgb, local.clamp_min, local.clamp_max);
  /* Go back from YCoCg for final blend. */
  history_radiance.rgb = colorspace_scene_linear_from_YCoCg(history_radiance.rgb);
  /* Blend history with new radiance. */
  float mix_fac = (history_radiance.w > 1e-3) ? 0.97 : 0.0;
  /* Reduce blend factor to improve low roughness reflections. Use variance instead for speed. */
  mix_fac *= mix(0.75, 1.0, saturate(in_variance * 20.0));
  vec3 out_radiance = mix(
      colorspace_safe_color(in_radiance), colorspace_safe_color(history_radiance.rgb), mix_fac);
  /* This is feedback next frame as radiance_history_tx. */
  imageStore(out_radiance_img, texel_fullres, vec4(out_radiance, 0.0));

  /* Variance. */

  /* Reflection reprojection. */
  vec2 history_variance = variance_history_sample(P_hit);
  /* Blend history with new variance. */
  float mix_variance_fac = (history_variance.y == 0.0) ? 0.0 : 0.90;
  float out_variance = mix(in_variance, history_variance.x, mix_variance_fac);
  /* This is feedback next frame as variance_history_tx. */
  imageStore(out_variance_img, texel_fullres, vec4(out_variance));
}
