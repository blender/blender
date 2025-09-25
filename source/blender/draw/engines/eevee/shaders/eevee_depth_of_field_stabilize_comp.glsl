/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Temporal Stabilization of the Depth of field input.
 * Corresponds to the TAA pass in the paper.
 * We actually duplicate the TAA logic but with a few changes:
 * - We run this pass at half resolution.
 * - We store CoC instead of Opacity in the alpha channel of the history.
 *
 * This is and adaption of the code found in `eevee_film_lib.glsl`.
 *
 * Inputs:
 * - Output of setup pass (half-resolution).
 * Outputs:
 * - Stabilized Color and CoC (half-resolution).
 */

#include "infos/eevee_depth_of_field_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_depth_of_field_stabilize)

#include "eevee_colorspace_lib.glsl"
#include "eevee_depth_of_field_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "eevee_velocity_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"

struct DofSample {
  float4 color;
  float coc;

  METAL_CONSTRUCTOR_2(DofSample, float4, color, float, coc)
};

/* -------------------------------------------------------------------- */
/** \name LDS Cache
 * \{ */
#define cache_size (gl_WorkGroupSize.x + 2)
shared float4 color_cache[cache_size][cache_size];
shared float coc_cache[cache_size][cache_size];
/* Need 2 pixel border for depth. */
#define cache_depth_size (gl_WorkGroupSize.x + 4)
shared float depth_cache[cache_depth_size][cache_depth_size];

void dof_cache_init()
{
  /**
   * Load enough values into LDS to perform the filter.
   *
   * ┌──────────────────────────────┐
   * │                              │  < Border texels that needs to be loaded.
   * │    x  x  x  x  x  x  x  x    │  ─┐
   * │    x  x  x  x  x  x  x  x    │   │
   * │    x  x  x  x  x  x  x  x    │   │
   * │    x  x  x  x  x  x  x  x    │   │ Thread Group Size 8x8.
   * │ L  L  L  L  L  x  x  x  x    │   │
   * │ L  L  L  L  L  x  x  x  x    │   │
   * │ L  L  L  L  L  x  x  x  x    │   │
   * │ L  L  L  L  L  x  x  x  x    │  ─┘
   * │ L  L  L  L  L                │  < Border texels that needs to be loaded.
   * └──────────────────────────────┘
   *   └───────────┘
   *    Load using 5x5 threads.
   */

  int2 texel = int2(gl_GlobalInvocationID.xy);
  for (int y = 0; y < 2; y++) {
    for (int x = 0; x < 2; x++) {
      /* 1 Pixel border. */
      if (all(lessThan(gl_LocalInvocationID.xy, uint2(cache_size / 2u)))) {
        int2 offset = int2(x, y) * int2(cache_size / 2u);
        int2 cache_texel = int2(gl_LocalInvocationID.xy) + offset;
        int2 load_texel = clamp(texel + offset - 1, int2(0), textureSize(color_tx, 0) - 1);

        float4 color = texelFetch(color_tx, load_texel, 0);
        color_cache[cache_texel.y][cache_texel.x] = colorspace_YCoCg_from_scene_linear(color);
        coc_cache[cache_texel.y][cache_texel.x] = texelFetch(coc_tx, load_texel, 0).x;
      }
      /* 2 Pixels border. */
      if (all(lessThan(gl_LocalInvocationID.xy, uint2(cache_depth_size / 2u)))) {
        int2 offset = int2(x, y) * int2(cache_depth_size / 2u);
        int2 cache_texel = int2(gl_LocalInvocationID.xy) + offset;
        /* Depth is full-resolution. Load every 2 pixels. */
        int2 load_texel = clamp((texel + offset - 2) * 2, int2(0), textureSize(depth_tx, 0) - 1);

        float depth = reverse_z::read(texelFetch(depth_tx, load_texel, 0).x);
        depth_cache[cache_texel.y][cache_texel.x] = depth;
      }
    }
  }
  barrier();
}

/* NOTE: Sample color space is already in YCoCg space. */
DofSample dof_fetch_input_sample(int2 offset)
{
  int2 coord = offset + 1 + int2(gl_LocalInvocationID.xy);
  return DofSample(color_cache[coord.y][coord.x], coc_cache[coord.y][coord.x]);
}

float dof_fetch_half_depth(int2 offset)
{
  int2 coord = offset + 2 + int2(gl_LocalInvocationID.xy);
  return depth_cache[coord.y][coord.x];
}

/** \} */

float dof_luma_weight(float luma)
{
  /* Slide 20 of "High Quality Temporal Supersampling" by Brian Karis at SIGGRAPH 2014. */
  /* To preserve more details in dark areas, we use a bigger bias. */
  constexpr float exposure_scale = 1.0f; /* TODO. */
  return 1.0f / (4.0f + luma * exposure_scale);
}

float dof_bilateral_weight(float reference_coc, float sample_coc)
{
  /* NOTE: The difference between the COCS should be inside a abs() function,
   * but we follow UE4 implementation to improve how dithered transparency looks (see slide 19).
   * Effectively bleed background into foreground.
   * Compared to dof_bilateral_coc_weights() this saturates as 2x the reference CoC. */
  return saturate(1.0f - (sample_coc - reference_coc) / max(1.0f, abs(reference_coc)));
}

DofSample dof_spatial_filtering()
{
  /* Plus (+) shape offsets. */
  constexpr int2 plus_offsets[4] = int2_array(int2(-1, 0), int2(0, -1), int2(1, 0), int2(0, 1));
  DofSample center = dof_fetch_input_sample(int2(0));
  DofSample accum = DofSample(float4(0.0f), 0.0f);
  float accum_weight = 0.0f;
  for (int i = 0; i < 4; i++) {
    DofSample samp = dof_fetch_input_sample(plus_offsets[i]);
    float weight = dof_buf.filter_samples_weight[i] * dof_luma_weight(samp.color.x) *
                   dof_bilateral_weight(center.coc, samp.coc);

    accum.color += samp.color * weight;
    accum.coc += samp.coc * weight;
    accum_weight += weight;
  }
  /* Accumulate center sample last as it does not need bilateral_weights. */
  float weight = dof_buf.filter_center_weight * dof_luma_weight(center.color.x);
  accum.color += center.color * weight;
  accum.coc += center.coc * weight;
  accum_weight += weight;

  float rcp_weight = 1.0f / accum_weight;
  accum.color *= rcp_weight;
  accum.coc *= rcp_weight;
  return accum;
}

struct DofNeighborhoodMinMax {
  DofSample min;
  DofSample max;

  METAL_CONSTRUCTOR_2(DofNeighborhoodMinMax, DofSample, min, DofSample, max)
};

/* Return history clipping bounding box in YCoCg color space. */
DofNeighborhoodMinMax dof_neighbor_boundbox()
{
  /* Plus (+) shape offsets. */
  constexpr int2 plus_offsets[4] = int2_array(int2(-1, 0), int2(0, -1), int2(1, 0), int2(0, 1));
  /**
   * Simple bounding box calculation in YCoCg as described in:
   * "High Quality Temporal Supersampling" by Brian Karis at SIGGRAPH 2014
   */
  DofSample min_c = dof_fetch_input_sample(int2(0));
  DofSample max_c = min_c;
  for (int i = 0; i < 4; i++) {
    DofSample samp = dof_fetch_input_sample(plus_offsets[i]);
    min_c.color = min(min_c.color, samp.color);
    max_c.color = max(max_c.color, samp.color);
    min_c.coc = min(min_c.coc, samp.coc);
    max_c.coc = max(max_c.coc, samp.coc);
  }
  /* (Slide 32) Simple clamp to min/max of 8 neighbors results in 3x3 box artifacts.
   * Round bbox shape by averaging 2 different min/max from 2 different neighborhood. */
  DofSample min_c_3x3 = min_c;
  DofSample max_c_3x3 = max_c;
  constexpr int2 corners[4] = int2_array(int2(-1, -1), int2(1, -1), int2(-1, 1), int2(1, 1));
  for (int i = 0; i < 4; i++) {
    DofSample samp = dof_fetch_input_sample(corners[i]);
    min_c_3x3.color = min(min_c_3x3.color, samp.color);
    max_c_3x3.color = max(max_c_3x3.color, samp.color);
    min_c_3x3.coc = min(min_c_3x3.coc, samp.coc);
    max_c_3x3.coc = max(max_c_3x3.coc, samp.coc);
  }
  min_c.color = (min_c.color + min_c_3x3.color) * 0.5f;
  max_c.color = (max_c.color + max_c_3x3.color) * 0.5f;
  min_c.coc = (min_c.coc + min_c_3x3.coc) * 0.5f;
  max_c.coc = (max_c.coc + max_c_3x3.coc) * 0.5f;

  return DofNeighborhoodMinMax(min_c, max_c);
}

/* Returns motion in pixel space to retrieve the pixel history. */
float2 dof_pixel_history_motion_vector(int2 texel_sample)
{
  /**
   * Dilate velocity by using the nearest pixel in a cross pattern.
   * "High Quality Temporal Supersampling" by Brian Karis at SIGGRAPH 2014 (Slide 27)
   */
  constexpr int2 corners[4] = int2_array(int2(-2, -2), int2(2, -2), int2(-2, 2), int2(2, 2));
  float min_depth = dof_fetch_half_depth(int2(0));
  int2 nearest_texel = int2(0);
  for (int i = 0; i < 4; i++) {
    float depth = dof_fetch_half_depth(corners[i]);
    if (min_depth > depth) {
      min_depth = depth;
      nearest_texel = corners[i];
    }
  }
  /* Convert to full resolution buffer pixel. */
  int2 velocity_texel = (texel_sample + nearest_texel) * 2;
  velocity_texel = clamp(velocity_texel, int2(0), textureSize(velocity_tx, 0).xy - 1);
  float4 vector = velocity_resolve(velocity_tx, velocity_texel, min_depth);
  /* Transform to **half** pixel space. */
  return vector.xy * float2(textureSize(color_tx, 0));
}

/* Load color using a special filter to avoid losing detail.
 * \a texel is sample position with sub-pixel accuracy. */
DofSample dof_sample_history(float2 input_texel)
{
#if 1 /* Bilinar. */
  float2 uv = float2(input_texel + 0.5f) / float2(textureSize(in_history_tx, 0));
  float4 color = textureLod(in_history_tx, uv, 0.0f);

#else /* Catmull Rom interpolation. 5 Bilinear Taps. */
  float2 center_texel;
  float2 inter_texel = modf(input_texel, center_texel);
  float2 weights[4];
  film_get_catmull_rom_weights(inter_texel, weights);

  /**
   * Use optimized version by leveraging bilinear filtering from hardware sampler and by removing
   * corner taps.
   * From "Filmic SMAA" by Jorge Jimenez at SIGGRAPH 2016
   * http://advances.realtimerendering.com/s2016/Filmic%20SMAA%20v7.pptx
   */
  center_texel += 0.5f;

  /* Slide 92. */
  float2 weight_12 = weights[1] + weights[2];
  float2 uv_12 = (center_texel + weights[2] / weight_12) * uniform_buf.film.extent_inv;
  float2 uv_0 = (center_texel - 1.0f) * uniform_buf.film.extent_inv;
  float2 uv_3 = (center_texel + 2.0f) * uniform_buf.film.extent_inv;

  float4 color;
  float4 weight_cross = weight_12.xyyx * float4(weights[0].yx, weights[3].xy);
  float weight_center = weight_12.x * weight_12.y;

  color = textureLod(in_history_tx, uv_12, 0.0f) * weight_center;
  color += textureLod(in_history_tx, float2(uv_12.x, uv_0.y), 0.0f) * weight_cross.x;
  color += textureLod(in_history_tx, float2(uv_0.x, uv_12.y), 0.0f) * weight_cross.y;
  color += textureLod(in_history_tx, float2(uv_3.x, uv_12.y), 0.0f) * weight_cross.z;
  color += textureLod(in_history_tx, float2(uv_12.x, uv_3.y), 0.0f) * weight_cross.w;
  /* Re-normalize for the removed corners. */
  color /= (weight_center + reduce_add(weight_cross));
#endif
  /* NOTE(fclem): Opacity is wrong on purpose. Final Opacity does not rely on history. */
  return DofSample(color.xyzz, color.w);
}

/* Modulate the history color to avoid ghosting artifact. */
DofSample dof_amend_history(DofNeighborhoodMinMax bbox, DofSample history, DofSample src)
{
#if 0
  /* Clip instead of clamping to avoid color accumulating in the AABB corners. */
  float3 clip_dir = src.color.rgb - history.color.rgb;

  float t = line_aabb_clipping_dist(
      history.color.rgb, clip_dir, bbox.min.color.rgb, bbox.max.color.rgb);
  history.color.rgb += clip_dir * saturate(t);
#else
  /* More responsive. */
  history.color = clamp(history.color, bbox.min.color, bbox.max.color);
#endif
  /* Clamp CoC to reduce convergence time. Otherwise the result lags. */
  history.coc = clamp(history.coc, bbox.min.coc, bbox.max.coc);

  return history;
}

float dof_history_blend_factor(
    float velocity, float2 texel, DofNeighborhoodMinMax bbox, DofSample src, DofSample dst)
{
  float luma_min = bbox.min.color.x;
  float luma_max = bbox.max.color.x;
  float luma_history = dst.color.x;

  /* 5% of incoming color by default. */
  float blend = 0.05f;
  /* Blend less history if the pixel has substantial velocity. */
  /* NOTE(fclem): velocity threshold multiplied by 2 because of half resolution. */
  blend = mix(blend, 0.20f, saturate(velocity * 0.02f * 2.0f));
  /**
   * "High Quality Temporal Supersampling" by Brian Karis at SIGGRAPH 2014 (Slide 43)
   * Bias towards history if incoming pixel is near clamping. Reduces flicker.
   */
  float distance_to_luma_clip = reduce_min(
      float2(luma_history - luma_min, luma_max - luma_history));
  /* Divide by bbox size to get a factor. 2 factor to compensate the line above. */
  distance_to_luma_clip *= 2.0f * safe_rcp(luma_max - luma_min);
  /* Linearly blend when history gets below to 25% of the bbox size. */
  blend *= saturate(distance_to_luma_clip * 4.0f + 0.1f);
  /* Progressively discard history until history CoC is twice as big as the filtered CoC.
   * Note we use absolute diff here because we are not comparing neighbors and thus do not risk to
   * dilate thin features like hair (slide 19). */
  float coc_diff_ratio = saturate(abs(src.coc - dst.coc) / max(1.0f, abs(src.coc)));
  blend = mix(blend, 1.0f, coc_diff_ratio);
  /* Discard out of view history. */
  if (any(lessThan(texel, float2(0))) ||
      any(greaterThanEqual(texel, float2(imageSize(out_history_img)))))
  {
    blend = 1.0f;
  }
  /* Discard history if invalid. */
  if (u_use_history == false) {
    blend = 1.0f;
  }
  return blend;
}

void main()
{
  dof_cache_init();

  int2 src_texel = int2(gl_GlobalInvocationID.xy);

  /**
   * Naming convention is taken from the film implementation.
   * SRC is incoming new data.
   * DST is history data.
   */
  DofSample src = dof_spatial_filtering();

  /* Reproject by finding where this pixel was in the previous frame. */
  float2 motion = dof_pixel_history_motion_vector(src_texel);
  float2 history_texel = float2(src_texel) + motion;

  float velocity = length(motion);

  DofSample dst = dof_sample_history(history_texel);

  /* Get local color bounding box of source neighborhood. */
  DofNeighborhoodMinMax bbox = dof_neighbor_boundbox();

  float blend = dof_history_blend_factor(velocity, history_texel, bbox, src, dst);

  dst = dof_amend_history(bbox, dst, src);

  /* Luma weighted blend to reduce flickering. */
  float weight_dst = dof_luma_weight(dst.color.x) * (1.0f - blend);
  float weight_src = dof_luma_weight(src.color.x) * (blend);

  DofSample result;
  /* Weighted blend. */
  result.color = float4(dst.color.rgb, dst.coc) * weight_dst +
                 float4(src.color.rgb, src.coc) * weight_src;
  result.color /= weight_src + weight_dst;

  /* Save history for next iteration. Still in YCoCg space with CoC in alpha. */
  imageStore(out_history_img, src_texel, result.color);

  /* Un-swizzle. */
  result.coc = result.color.a;
  /* Clamp opacity since we don't store it in history. */
  result.color.a = clamp(src.color.a, bbox.min.color.a, bbox.max.color.a);

  result.color = colorspace_scene_linear_from_YCoCg(result.color);

  imageStore(out_color_img, src_texel, result.color);
  imageStore(out_coc_img, src_texel, float4(result.coc));
}
