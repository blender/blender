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

#include "infos/eevee_tracing_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_ray_denoise_temporal)

#include "draw_view_lib.glsl"
#include "eevee_colorspace_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"

#include "gpu_shader_utildefines_lib.glsl"

struct LocalStatistics {
  float3 mean;
  float3 moment;
  float3 variance;
  float3 deviation;
  float3 clamp_min;
  float3 clamp_max;
};

LocalStatistics local_statistics_get(int2 texel, float3 center_radiance)
{
  float3 center_radiance_YCoCg = colorspace_YCoCg_from_scene_linear(center_radiance);

  /* Build Local statistics (slide 46). */
  LocalStatistics result;
  result.mean = center_radiance_YCoCg;
  result.moment = square(center_radiance_YCoCg);
  float weight_accum = 1.0f;

  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++) {
      if (x == 0 && y == 0) {
        continue;
      }

      int2 neighbor_texel = texel + int2(x, y);
      if (!in_image_range(neighbor_texel, in_radiance_img)) {
        continue;
      }
      float3 radiance = imageLoad(in_radiance_img, neighbor_texel).rgb;
      /* Exclude unprocessed pixels. */
      if (all(equal(radiance, FLT_11_11_10_MAX))) {
        continue;
      }

      /* Weight corners less to avoid box artifacts.
       * Same idea as in "High Quality Temporal Supersampling" by Brian Karis at SIGGRAPH 2014
       * (Slide 32) Simple clamp to min/max of 8 neighbors results in 3x3 box artifacts. */
      /* TODO(@fclem): Evaluate if beneficial. Currently not use to soak more noise? Unsure. */
      // float weight = (abs(x) == abs(y)) ? 0.25f : 1.0f;
      /* Use YCoCg for clamping and accumulation to avoid color shift artifacts. */
      float3 radiance_YCoCg = colorspace_YCoCg_from_scene_linear(radiance.rgb);
      result.mean += radiance_YCoCg;
      result.moment += square(radiance_YCoCg);
      weight_accum += 1.0f;
    }
  }
  float inv_weight = safe_rcp(weight_accum);
  result.mean *= inv_weight;
  result.moment *= inv_weight;
  result.variance = abs(result.moment - square(result.mean));
  result.deviation = max(float3(1e-4f), sqrt(result.variance));
  result.clamp_min = result.mean - result.deviation;
  result.clamp_max = result.mean + result.deviation;
  return result;
}

float4 bilinear_weights_from_subpixel_coord(float2 co)
{
  /* From top left in clockwise order. */
  float4 weights;
  weights.x = (1.0f - co.x) * co.y;
  weights.y = co.x * co.y;
  weights.z = co.x * (1.0f - co.y);
  weights.w = (1.0f - co.x) * (1.0f - co.y);
  return weights;
}

float4 radiance_history_fetch(int2 texel, float bilinear_weight)
{
  /* Out of history view. Return sample without weight. */
  if (!in_texture_range(texel, radiance_history_tx)) {
    return float4(0.0f);
  }

  int3 history_tile = int3(texel / RAYTRACE_GROUP_SIZE, closure_index);
  /* Fetch previous tilemask to avoid loading invalid data. */
  bool is_valid_history = texelFetch(tilemask_history_tx, history_tile, 0).r != 0;
  /* Exclude unprocessed pixels. */
  if (!is_valid_history) {
    return float4(0.0f);
  }
  float3 history_radiance = texelFetch(radiance_history_tx, texel, 0).rgb;
  /* Exclude unprocessed pixels. */
  if (all(equal(history_radiance, FLT_11_11_10_MAX))) {
    return float4(0.0f);
  }
  return float4(history_radiance * bilinear_weight, bilinear_weight);
}

float4 radiance_history_sample(float3 P, LocalStatistics local)
{
  float2 uv = project_point(uniform_buf.raytrace.history_persmat, P).xy * 0.5f + 0.5f;

  /* FIXME(fclem): Find why we need this half pixel offset. */
  float2 texel_co = uv * float2(textureSize(radiance_history_tx, 0).xy) - 0.5f;
  float4 bilinear_weights = bilinear_weights_from_subpixel_coord(fract(texel_co));
  int2 texel = int2(floor(texel_co));

  /* Radiance needs to be manually interpolated because any pixel might contain invalid data. */
  float4 history_radiance;
  history_radiance = radiance_history_fetch(texel + int2(0, 1), bilinear_weights.x);
  history_radiance += radiance_history_fetch(texel + int2(1, 1), bilinear_weights.y);
  history_radiance += radiance_history_fetch(texel + int2(1, 0), bilinear_weights.z);
  history_radiance += radiance_history_fetch(texel + int2(0, 0), bilinear_weights.w);

  /* Use YCoCg for clamping and accumulation to avoid color shift artifacts. */
  float4 history_radiance_YCoCg;
  history_radiance_YCoCg.rgb = colorspace_YCoCg_from_scene_linear(history_radiance.rgb);
  history_radiance_YCoCg.a = history_radiance.a;

  /* Weighted contribution (slide 46). */
  float3 dist = abs(history_radiance_YCoCg.rgb - local.mean) / local.deviation;
  float weight = exp2(-4.0f * dot(dist, float3(1.0f)));

  return history_radiance_YCoCg * weight;
}

float2 variance_history_sample(float3 P)
{
  float2 uv = project_point(uniform_buf.raytrace.history_persmat, P).xy * 0.5f + 0.5f;

  if (!in_range_exclusive(uv, float2(0.0f), float2(1.0f))) {
    /* Out of history view. Return sample without weight. */
    return float2(0.0f);
  }

  float history_variance = texture(variance_history_tx, uv).r;

  int2 history_texel = int2(floor(uv * float2(textureSize(variance_history_tx, 0).xy)));
  int3 history_tile = int3(history_texel / RAYTRACE_GROUP_SIZE, closure_index);
  /* Fetch previous tilemask to avoid loading invalid data. */
  bool is_valid_history = texelFetch(tilemask_history_tx, history_tile, 0).r != 0;

  if (is_valid_history) {
    return float2(history_variance, 1.0f);
  }
  return float2(0.0f);
}

void main()
{
  constexpr uint tile_size = RAYTRACE_GROUP_SIZE;
  uint2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  int2 texel_fullres = int2(gl_LocalInvocationID.xy + tile_coord * tile_size);
  float2 uv = (float2(texel_fullres) + 0.5f) * uniform_buf.raytrace.full_resolution_inv;

  /* Check if texel is out of bounds,
   * so we can utilize fast texture functions and early-out if not. */
  if (any(greaterThanEqual(texel_fullres, imageSize(in_radiance_img).xy))) {
    return;
  }

  float in_variance = imageLoadFast(in_variance_img, texel_fullres).r;
  float3 in_radiance = imageLoadFast(in_radiance_img, texel_fullres).rgb;

  if (all(equal(in_radiance, FLT_11_11_10_MAX))) {
    /* Early out on pixels that were marked unprocessed by the previous pass. */
    imageStoreFast(out_radiance_img, texel_fullres, float4(FLT_11_11_10_MAX, 0.0f));
    imageStoreFast(out_variance_img, texel_fullres, float4(0.0f));
    return;
  }

  LocalStatistics local = local_statistics_get(texel_fullres, in_radiance);

  /* Radiance. */

  /* Surface reprojection. */
  /* TODO(fclem): Use per pixel velocity. Is this worth it? */
  float scene_depth = reverse_z::read(texelFetch(depth_tx, texel_fullres, 0).r);
  float3 P = drw_point_screen_to_world(float3(uv, scene_depth));
  float4 history_radiance = radiance_history_sample(P, local);
  /* Reflection reprojection. */
  float hit_depth = imageLoadFast(hit_depth_img, texel_fullres).r;
  float3 P_hit = drw_point_screen_to_world(float3(uv, hit_depth));
  history_radiance += radiance_history_sample(P_hit, local);
  /* Finalize accumulation. */
  history_radiance *= safe_rcp(history_radiance.w);
  /* Clamp resulting history radiance (slide 47). */
  history_radiance.rgb = clamp(history_radiance.rgb, local.clamp_min, local.clamp_max);
  /* Go back from YCoCg for final blend. */
  history_radiance.rgb = colorspace_scene_linear_from_YCoCg(history_radiance.rgb);
  /* Blend history with new radiance. */
  float mix_fac = (history_radiance.w > 1e-3f) ? 0.97f : 0.0f;
  /* Reduce blend factor to improve low roughness reflections. Use variance instead for speed. */
  mix_fac *= mix(0.75f, 1.0f, saturate(in_variance * 20.0f));
  float3 out_radiance = mix(
      colorspace_safe_color(in_radiance), colorspace_safe_color(history_radiance.rgb), mix_fac);
  /* This is feedback next frame as radiance_history_tx. */
  imageStoreFast(out_radiance_img, texel_fullres, float4(out_radiance, 0.0f));

  /* Variance. */

  /* Reflection reprojection. */
  float2 history_variance = variance_history_sample(P_hit);
  /* Blend history with new variance. */
  float mix_variance_fac = (history_variance.y == 0.0f) ? 0.0f : 0.90f;
  float out_variance = mix(in_variance, history_variance.x, mix_variance_fac);
  /* This is feedback next frame as variance_history_tx. */
  imageStoreFast(out_variance_img, texel_fullres, float4(out_variance));
}
