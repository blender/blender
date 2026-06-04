/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_colorspace_lib.bsl.hh"
#include "eevee_depth_of_field_lib.bsl.hh"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_velocity.bsl.hh"
#include "gpu_shader_math_safe_lib.glsl"

namespace eevee::dof::setup {

/**
 * Setup pass: CoC and luma aware downsample to half resolution of the input scene color buffer.
 *
 * An addition to the downsample CoC, we output the maximum slight out of focus CoC to be
 * sure we don't miss a pixel.
 *
 * Input:
 *  Full-resolution color & depth buffer
 * Output:
 *  Half-resolution Color, signed CoC (out_coc.x), and max slight focus abs CoC (out_coc.y).
 */
namespace setup {

struct Resources {
  [[uniform(0)]] const DepthOfFieldData &dof_buf;

  [[sampler(0)]] sampler2D color_tx;
  [[sampler(1)]] sampler2DDepth depth_tx;

  [[image(0, write, SFLOAT_16_16_16_16)]] image2D out_color_img;
  [[image(1, write, SFLOAT_16)]] image2D out_coc_img;
};

[[compute, local_size(DOF_DEFAULT_GROUP_SIZE, DOF_DEFAULT_GROUP_SIZE)]]
void comp_main([[resource_table]] Resources &srt,
               [[resource_table]] const draw::View &views,
               [[global_invocation_id]] const uint3 global_id)
{
  float2 fullres_texel_size = 1.0f / float2(textureSize(srt.color_tx, 0).xy);
  /* Center uv around the 4 full-resolution pixels. */
  float2 quad_center = float2(global_id.xy * 2 + 1) * fullres_texel_size;

  float4 colors[4];
  float4 cocs;
  for (int i = 0; i < 4; i++) {
    float2 sample_uv = quad_center + quad_offsets[i] * fullres_texel_size;
    float depth = reverse_z::read(textureLod(srt.depth_tx, sample_uv, 0.0f).r);
    /* NOTE: We use samplers without filtering. */
    colors[i] = colorspace::safe_color(textureLod(srt.color_tx, sample_uv, 0.0f));
    cocs[i] = dof_coc_from_depth(views, srt.dof_buf, sample_uv, depth);
  }

  cocs = clamp(cocs, -srt.dof_buf.coc_abs_max, srt.dof_buf.coc_abs_max);

  float4 weights = dof_bilateral_coc_weights(cocs);
  weights *= dof_bilateral_color_weights(colors);
  /* Normalize so that the sum is 1. */
  weights *= safe_rcp(reduce_add(weights));

  int2 out_texel = int2(global_id.xy);
  float4 out_color = weighted_sum_array(colors, weights);
  imageStore(srt.out_color_img, out_texel, out_color);

  float out_coc = dot(cocs, weights);
  imageStore(srt.out_coc_img, out_texel, float4(out_coc));
}

}  // namespace setup

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
namespace stabilize {

struct DofSample {
  float4 color;
  float coc;
};

struct DofNeighborhoodMinMax {
  DofSample min;
  DofSample max;
};

float luma_weight(float luma)
{
  /* Slide 20 of "High Quality Temporal Supersampling" by Brian Karis at SIGGRAPH 2014. */
  /* To preserve more details in dark areas, we use a bigger bias. */
  constexpr float exposure_scale = 1.0f; /* TODO. */
  return 1.0f / (4.0f + luma * exposure_scale);
}

float bilateral_weight(float reference_coc, float sample_coc)
{
  /* NOTE: The difference between the COCS should be inside a abs() function,
   * but we follow UE4 implementation to improve how dithered transparency looks (see slide 19).
   * Effectively bleed background into foreground.
   * Compared to dof_bilateral_coc_weights() this saturates as 2x the reference CoC. */
  return saturate(1.0f - (sample_coc - reference_coc) / max(1.0f, abs(reference_coc)));
}

struct Resources {
  [[resource_table]] srt_t<CameraVelocity> camera;
  [[resource_table]] srt_t<draw::View> views;

  [[push_constant]] const bool u_use_history;

  [[uniform(0)]] const DepthOfFieldData &dof_buf;

  [[sampler(0)]] sampler2D coc_tx;
  [[sampler(1)]] sampler2D color_tx;
  [[sampler(2)]] sampler2D velocity_tx;
  [[sampler(3)]] sampler2D in_history_tx;
  [[sampler(4)]] sampler2DDepth depth_tx;

  [[image(0, write, SFLOAT_16_16_16_16)]] image2D out_color_img;
  [[image(1, write, SFLOAT_16)]] image2D out_coc_img;
  [[image(2, write, SFLOAT_16_16_16_16)]] image2D out_history_img;

  /* -------------------------------------------------------------------- */
  /** \name LDS Cache
   * \{ */

  [[shared]] float4 color_cache[DOF_STABILIZE_GROUP_SIZE + 2][DOF_STABILIZE_GROUP_SIZE + 2];
  [[shared]] float coc_cache[DOF_STABILIZE_GROUP_SIZE + 2][DOF_STABILIZE_GROUP_SIZE + 2];
  /* Need 2 pixel border for depth. */
  [[shared]] float depth_cache[DOF_STABILIZE_GROUP_SIZE + 4][DOF_STABILIZE_GROUP_SIZE + 4];

  void cache_init(uint3 global_id, uint3 local_id)
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
    constexpr uint cache_size = uint(DOF_STABILIZE_GROUP_SIZE + 2);
    constexpr uint cache_depth_size = uint(DOF_STABILIZE_GROUP_SIZE + 4);

    int2 texel = int2(global_id.xy);
    for (int y = 0; y < 2; y++) {
      for (int x = 0; x < 2; x++) {
        /* 1 Pixel border. */
        if (all(lessThan(local_id.xy, uint2(cache_size / 2u)))) {
          int2 offset = int2(x, y) * int2(cache_size / 2u);
          int2 cache_texel = int2(local_id.xy) + offset;
          int2 load_texel = clamp(texel + offset - 1, int2(0), textureSize(color_tx, 0) - 1);

          float4 color = texelFetch(color_tx, load_texel, 0);
          color_cache[cache_texel.y][cache_texel.x] = colorspace::YCoCg_from_scene_linear(color);
          coc_cache[cache_texel.y][cache_texel.x] = texelFetch(coc_tx, load_texel, 0).x;
        }
        /* 2 Pixels border. */
        if (all(lessThan(local_id.xy, uint2(cache_depth_size / 2u)))) {
          int2 offset = int2(x, y) * int2(cache_depth_size / 2u);
          int2 cache_texel = int2(local_id.xy) + offset;
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
  DofSample fetch_input_sample(int2 offset, uint3 local_id) const
  {
    int2 coord = offset + 1 + int2(local_id.xy);
    return {color_cache[coord.y][coord.x], coc_cache[coord.y][coord.x]};
  }

  float fetch_half_depth(int2 offset, uint3 local_id) const
  {
    int2 coord = offset + 2 + int2(local_id.xy);
    return depth_cache[coord.y][coord.x];
  }

  /** \} */

  DofSample spatial_filtering(uint3 local_id) const
  {
    /* Plus (+) shape offsets. */
    constexpr int2 plus_offsets[4] = int2_array(int2(-1, 0), int2(0, -1), int2(1, 0), int2(0, 1));
    DofSample center = fetch_input_sample(int2(0), local_id);
    DofSample accum{float4(0.0f), 0.0f};
    float accum_weight = 0.0f;
    for (int i = 0; i < 4; i++) {
      DofSample samp = fetch_input_sample(plus_offsets[i], local_id);
      float weight = dof_buf.filter_samples_weight[i] * luma_weight(samp.color.x) *
                     bilateral_weight(center.coc, samp.coc);

      accum.color += samp.color * weight;
      accum.coc += samp.coc * weight;
      accum_weight += weight;
    }
    /* Accumulate center sample last as it does not need bilateral_weights. */
    float weight = dof_buf.filter_center_weight * luma_weight(center.color.x);
    accum.color += center.color * weight;
    accum.coc += center.coc * weight;
    accum_weight += weight;

    float rcp_weight = 1.0f / accum_weight;
    accum.color *= rcp_weight;
    accum.coc *= rcp_weight;
    return accum;
  }

  /* Return history clipping bounding box in YCoCg color space. */
  DofNeighborhoodMinMax neighbor_boundbox(uint3 local_id) const
  {
    /* Plus (+) shape offsets. */
    constexpr int2 plus_offsets[4] = int2_array(int2(-1, 0), int2(0, -1), int2(1, 0), int2(0, 1));
    /**
     * Simple bounding box calculation in YCoCg as described in:
     * "High Quality Temporal Supersampling" by Brian Karis at SIGGRAPH 2014
     */
    DofSample min_c = fetch_input_sample(int2(0), local_id);
    DofSample max_c = min_c;
    for (int i = 0; i < 4; i++) {
      DofSample samp = fetch_input_sample(plus_offsets[i], local_id);
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
      DofSample samp = fetch_input_sample(corners[i], local_id);
      min_c_3x3.color = min(min_c_3x3.color, samp.color);
      max_c_3x3.color = max(max_c_3x3.color, samp.color);
      min_c_3x3.coc = min(min_c_3x3.coc, samp.coc);
      max_c_3x3.coc = max(max_c_3x3.coc, samp.coc);
    }
    min_c.color = (min_c.color + min_c_3x3.color) * 0.5f;
    max_c.color = (max_c.color + max_c_3x3.color) * 0.5f;
    min_c.coc = (min_c.coc + min_c_3x3.coc) * 0.5f;
    max_c.coc = (max_c.coc + max_c_3x3.coc) * 0.5f;

    return {min_c, max_c};
  }

  /* Returns motion in pixel space to retrieve the pixel history. */
  float2 pixel_history_motion_vector(int2 texel_sample, uint3 local_id) const
  {
    [[resource_table]] const CameraVelocity &cam_vel = this->camera;

    /**
     * Dilate velocity by using the nearest pixel in a cross pattern.
     * "High Quality Temporal Supersampling" by Brian Karis at SIGGRAPH 2014 (Slide 27)
     */
    constexpr int2 corners[4] = int2_array(int2(-2, -2), int2(2, -2), int2(-2, 2), int2(2, 2));
    float min_depth = fetch_half_depth(int2(0), local_id);
    int2 nearest_texel = int2(0);
    for (int i = 0; i < 4; i++) {
      float depth = fetch_half_depth(corners[i], local_id);
      if (min_depth > depth) {
        min_depth = depth;
        nearest_texel = corners[i];
      }
    }
    /* Convert to full resolution buffer pixel. */
    int2 velocity_texel = (texel_sample + nearest_texel) * 2;
    velocity_texel = clamp(velocity_texel, int2(0), textureSize(velocity_tx, 0).xy - 1);
    float4 vector = cam_vel.resolve(views, velocity_tx, velocity_texel, min_depth);
    /* Transform to **half** pixel space. */
    return vector.xy * float2(textureSize(color_tx, 0));
  }

  /* Load color using a special filter to avoid losing detail.
   * \a texel is sample position with sub-pixel accuracy. */
  DofSample sample_history(float2 input_texel) const
  {
#if 1 /* Bilinear. */
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
    return {color.xyzz, color.w};
  }

  float history_blend_factor(
      float velocity, float2 texel, DofNeighborhoodMinMax bbox, DofSample src, DofSample dst) const
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
     * Note we use absolute diff here because we are not comparing neighbors and thus do not risk
     * to dilate thin features like hair (slide 19). */
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
};

/* Modulate the history color to avoid ghosting artifact. */
DofSample amend_history(DofNeighborhoodMinMax bbox,
                        DofSample history,
                        [[maybe_unused]] DofSample src)
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

[[compute, local_size(DOF_STABILIZE_GROUP_SIZE, DOF_STABILIZE_GROUP_SIZE)]]
void comp_main([[resource_table]] Resources &srt,
               [[global_invocation_id]] const uint3 global_id,
               [[local_invocation_id]] const uint3 local_id)
{
  srt.cache_init(global_id, local_id);

  int2 src_texel = int2(global_id.xy);

  /**
   * Naming convention is taken from the film implementation.
   * SRC is incoming new data.
   * DST is history data.
   */
  DofSample src = srt.spatial_filtering(local_id);

  /* Reproject by finding where this pixel was in the previous frame. */
  float2 motion = srt.pixel_history_motion_vector(src_texel, local_id);
  float2 history_texel = float2(src_texel) + motion;

  float velocity = length(motion);

  DofSample dst = srt.sample_history(history_texel);

  /* Get local color bounding box of source neighborhood. */
  DofNeighborhoodMinMax bbox = srt.neighbor_boundbox(local_id);

  float blend = srt.history_blend_factor(velocity, history_texel, bbox, src, dst);

  dst = amend_history(bbox, dst, src);

  /* Luma weighted blend to reduce flickering. */
  float weight_dst = luma_weight(dst.color.x) * (1.0f - blend);
  float weight_src = luma_weight(src.color.x) * (blend);

  DofSample result;
  /* Weighted blend. */
  result.color = float4(dst.color.rgb, dst.coc) * weight_dst +
                 float4(src.color.rgb, src.coc) * weight_src;
  result.color /= weight_src + weight_dst;

  /* Save history for next iteration. Still in YCoCg space with CoC in alpha. */
  imageStore(srt.out_history_img, src_texel, result.color);

  /* Un-swizzle. */
  result.coc = result.color.a;
  /* Clamp opacity since we don't store it in history. */
  result.color.a = clamp(src.color.a, bbox.min.color.a, bbox.max.color.a);

  result.color = colorspace::scene_linear_from_YCoCg(result.color);

  imageStore(srt.out_color_img, src_texel, result.color);
  imageStore(srt.out_coc_img, src_texel, float4(result.coc));
}

}  // namespace stabilize

/**
 * Downsample pass: CoC aware downsample to quarter resolution.
 *
 * Pretty much identical to the setup pass but get CoC from buffer.
 * Also does not weight luma for the bilateral weights.
 */
namespace downsample {

struct Resources {
  [[sampler(0)]] sampler2D color_tx;
  [[sampler(1)]] sampler2D coc_tx;
  [[image(0, write, SFLOAT_16_16_16_16)]] image2D out_color_img;
};

[[compute, local_size(DOF_DEFAULT_GROUP_SIZE, DOF_DEFAULT_GROUP_SIZE)]]
void comp_main([[resource_table]] Resources &srt, [[global_invocation_id]] const uint3 global_id)
{
  float2 halfres_texel_size = 1.0f / float2(textureSize(srt.color_tx, 0).xy);
  /* Center uv around the 4 half-resolution pixels. */
  float2 quad_center = float2(global_id.xy * 2 + 1) * halfres_texel_size;

  float4 colors[4];
  float4 cocs;
  for (int i = 0; i < 4; i++) {
    float2 sample_uv = quad_center + quad_offsets[i] * halfres_texel_size;
    colors[i] = textureLod(srt.color_tx, sample_uv, 0.0f);
    cocs[i] = textureLod(srt.coc_tx, sample_uv, 0.0f).r;
  }

  float4 weights = dof_bilateral_coc_weights(cocs);
  /* Normalize so that the sum is 1. */
  weights *= safe_rcp(reduce_add(weights));

  float4 out_color = weighted_sum_array(colors, weights);

  imageStore(srt.out_color_img, int2(global_id.xy), out_color);
}
}  // namespace downsample

/**
 * Reduce copy pass: filter fireflies and split color between scatter and gather input.
 *
 * NOTE: The texture can end up being too big because of the mipmap padding. We correct for
 * that during the convolution phase.
 *
 * Inputs:
 * - Output of setup pass (half-resolution) and reduce downsample pass (quarter res).
 * Outputs:
 * - Half-resolution padded to avoid mipmap misalignment (so possibly not matching input size).
 * - Gather input color (whole mip chain), Scatter rect list, Signed CoC (whole mip chain).
 */
namespace reduce {

float fast_luma(float3 color)
{
  return (2.0f * color.g) + color.r + color.b;
}

struct Resources {
  [[storage(0, write)]] ScatterRect (&scatter_fg_list_buf)[];
  [[storage(1, write)]] ScatterRect (&scatter_bg_list_buf)[];

  [[storage(2, read_write)]] DrawCommandArray &scatter_fg_indirect_buf;
  [[storage(3, read_write)]] DrawCommandArray &scatter_bg_indirect_buf;

  [[uniform(0)]] const DepthOfFieldData &dof_buf;

  [[sampler(0)]] sampler2D downsample_tx;

  [[image(0, read_write, SFLOAT_16_16_16_16)]] image2D inout_color_lod0_img;
  [[image(1, write, SFLOAT_16_16_16_16)]] image2D out_color_lod1_img;
  [[image(2, write, SFLOAT_16_16_16_16)]] image2D out_color_lod2_img;
  [[image(3, write, SFLOAT_16_16_16_16)]] image2D out_color_lod3_img;
  [[image(4, read, SFLOAT_16)]] const image2D in_coc_lod0_img;
  [[image(5, write, SFLOAT_16)]] image2D out_coc_lod1_img;
  [[image(6, write, SFLOAT_16)]] image2D out_coc_lod2_img;
  [[image(7, write, SFLOAT_16)]] image2D out_coc_lod3_img;

  /* -------------------------------------------------------------------- */
  /** \name LDS Cache
   * \{ */

  [[shared]] float4 color_cache[DOF_REDUCE_GROUP_SIZE][DOF_REDUCE_GROUP_SIZE];
  [[shared]] float coc_cache[DOF_REDUCE_GROUP_SIZE][DOF_REDUCE_GROUP_SIZE];

  void store_color_cache(uint2 coord, float4 value)
  {
    color_cache[coord.y][coord.x] = value;
  }
  float4 load_color_cache(uint2 coord) const
  {
    return color_cache[coord.y][coord.x];
  }

  void store_coc_cache(uint2 coord, float value)
  {
    coc_cache[coord.y][coord.x] = value;
  }
  float load_coc_cache(uint2 coord) const
  {
    return coc_cache[coord.y][coord.x];
  }
  float4 load4_coc_cache(uint2 coord) const
  {
    return float4(load_coc_cache(coord + quad_offsets_u[0]),
                  load_coc_cache(coord + quad_offsets_u[1]),
                  load_coc_cache(coord + quad_offsets_u[2]),
                  load_coc_cache(coord + quad_offsets_u[3]));
  }

  /** \} */

  /* NOTE: Do not compare alpha as it is not scattered by the scatter pass. */
  float scatter_neighborhood_rejection(uint2 texel, float3 color)
  {
    color = min(float3(dof_buf.scatter_neighbor_max_color), color);

    float validity = 0.0f;

    /* Centered in the middle of 4 quarter res texel. */
    float2 texel_size = 1.0f / float2(textureSize(downsample_tx, 0).xy);
    float2 uv = ((float2(texel) + 0.5f) * 0.5f) * texel_size;

    for (int i = 0; i < 4; i++) {
      float2 sample_uv = uv + quad_offsets[i] * texel_size;
      float3 ref = textureLod(downsample_tx, sample_uv, 0.0f).rgb;

      ref = min(float3(dof_buf.scatter_neighbor_max_color), ref);
      float diff = reduce_max(max(float3(0.0f), abs(ref - color)));

      constexpr float rejection_threshold = 0.7f;
      diff = saturate(diff / rejection_threshold - 1.0f);
      validity = max(validity, diff);
    }

    return validity;
  }

  /* This avoids Bokeh sprite popping in and out at the screen border and
   * drawing Bokeh sprites larger than the screen. */
  float scatter_screen_border_rejection(float coc, int2 texel) const
  {
    float2 screen_size = float2(imageSize(inout_color_lod0_img));
    float2 uv = (float2(texel) + 0.5f) / screen_size;
    float2 screen_pos = uv * screen_size;
    float min_screen_border_distance = reduce_min(min(screen_pos, screen_size - screen_pos));
    /* Full-resolution to half-resolution CoC. */
    coc *= 0.5f;
    /* Allow 10px transition. */
    constexpr float rejection_hardness = 1.0f / 10.0f;
    return saturate((min_screen_border_distance - abs(coc)) * rejection_hardness + 1.0f);
  }

  float scatter_luminosity_rejection(float3 color) const
  {
    constexpr float rejection_hardness = 1.0f;
    return saturate(reduce_max(color - dof_buf.scatter_color_threshold) * rejection_hardness);
  }

  float scatter_coc_radius_rejection(float coc) const
  {
    constexpr float rejection_hardness = 0.3f;
    return saturate((abs(coc) - dof_buf.scatter_coc_threshold) * rejection_hardness);
  }
};

[[compute, local_size(DOF_REDUCE_GROUP_SIZE, DOF_REDUCE_GROUP_SIZE)]]
void comp_main([[resource_table]] Resources &srt,
               [[global_invocation_id]] const uint3 global_id,
               [[local_invocation_id]] const uint3 local_id)
{

  int2 texel = min(int2(global_id.xy), imageSize(srt.inout_color_lod0_img) - 1);
  uint2 thread_co = local_id.xy;

  float4 local_color = imageLoad(srt.inout_color_lod0_img, texel);
  float local_coc = imageLoad(srt.in_coc_lod0_img, texel).r;

  /* Only scatter if luminous enough. */
  float do_scatter = srt.scatter_luminosity_rejection(local_color.rgb);
  /* Only scatter if CoC is big enough. */
  do_scatter *= srt.scatter_coc_radius_rejection(local_coc);
  /* Only scatter if CoC is not too big to avoid performance issues. */
  do_scatter *= srt.scatter_screen_border_rejection(local_coc, texel);
  /* Only scatter if neighborhood is different enough. */
  do_scatter *= srt.scatter_neighborhood_rejection(global_id.xy, local_color.rgb);
  /* For debugging. */
  if (no_scatter_pass) {
    do_scatter = 0.0f;
  }

  /* Use coc_cache for broadcasting the do_scatter result. */
  srt.store_coc_cache(thread_co, do_scatter);
  barrier();

  /* Load the same value for each thread quad. */
  float4 do_scatter4 = srt.load4_coc_cache(thread_co & ~1u);
  barrier();

  /* Load level 0 into cache. */
  srt.store_color_cache(thread_co, local_color);
  srt.store_coc_cache(thread_co, local_coc);
  barrier();

  /* Add a scatter sprite for each 2x2 pixel neighborhood passing the threshold. */
  bool any_scatter = any(greaterThan(do_scatter4, float4(0.0f)));
  if (all(equal(thread_co & 1u, uint2(0))) && any_scatter) {
    /* Apply energy conservation to anamorphic scattered bokeh. */
    do_scatter4 *= reduce_max(srt.dof_buf.bokeh_anisotropic_scale_inv);
    /* Circle of Confusion. */
    float4 coc4 = srt.load4_coc_cache(thread_co);
    /* We are scattering at half resolution, so divide CoC by 2. */
    coc4 *= 0.5f;
    /* Sprite center position. Center sprite around the 4 texture taps. */
    float2 offset = float2(global_id.xy) + 1;
    /* Add 2.5 to max_coc because the max_coc may not be centered on the sprite origin
     * and because we smooth the bokeh shape a bit in the pixel shader. */
    float2 half_extent = reduce_max(abs(coc4)) * srt.dof_buf.bokeh_anisotropic_scale + 2.5f;
    /* Follows quad_offsets order. */
    float3 color4_0 = srt.load_color_cache(thread_co + quad_offsets_u[0]).rgb;
    float3 color4_1 = srt.load_color_cache(thread_co + quad_offsets_u[1]).rgb;
    float3 color4_2 = srt.load_color_cache(thread_co + quad_offsets_u[2]).rgb;
    float3 color4_3 = srt.load_color_cache(thread_co + quad_offsets_u[3]).rgb;
    /* Issue a sprite for each field if any CoC matches. */
    if (any(lessThan(do_scatter4 * sign(coc4), float4(0.0f)))) {
      /* Same value for all threads. Not an issue if we don't sync access to it. */
      srt.scatter_fg_indirect_buf.vertex_len = 4u;
      /* Issue 1 strip instance per sprite. */
      uint rect_id = atomicAdd(srt.scatter_fg_indirect_buf.instance_len, 1u);
      if (rect_id < srt.dof_buf.scatter_max_rect) {

        float4 coc4_fg = max(float4(0.0f), -coc4);
        float4 fg_weights = dof_layer_weight(coc4_fg) * dof_sample_weight(coc4_fg) * do_scatter4;
        /* Filter NaNs. */
        fg_weights = select(fg_weights, float4(0.0f), equal(coc4_fg, float4(0.0f)));

        ScatterRect rect_fg;
        rect_fg.offset = offset;
        /* Negate extent to flip the sprite. Mimics optical phenomenon. */
        rect_fg.half_extent = -half_extent;
        /* NOTE: Since we flipped the quad along (1,-1) line, we need to also swap the (1,1) and
         * (0,0) values so that quad_offsets is in the right order in the vertex shader. */

        /* Circle of Confusion absolute radius in half-resolution pixels. */
        rect_fg.color_and_coc[0].a = coc4_fg[0];
        rect_fg.color_and_coc[1].a = coc4_fg[3];
        rect_fg.color_and_coc[2].a = coc4_fg[2];
        rect_fg.color_and_coc[3].a = coc4_fg[1];
        /* Apply weights. */
        rect_fg.color_and_coc[0].rgb = color4_0 * fg_weights[0];
        rect_fg.color_and_coc[1].rgb = color4_3 * fg_weights[3];
        rect_fg.color_and_coc[2].rgb = color4_2 * fg_weights[2];
        rect_fg.color_and_coc[3].rgb = color4_1 * fg_weights[1];

        srt.scatter_fg_list_buf[rect_id] = rect_fg;
      }
    }
    if (any(greaterThan(do_scatter4 * sign(coc4), float4(0.0f)))) {
      /* Same value for all threads. Not an issue if we don't sync access to it. */
      srt.scatter_bg_indirect_buf.vertex_len = 4u;
      /* Issue 1 strip instance per sprite. */
      uint rect_id = atomicAdd(srt.scatter_bg_indirect_buf.instance_len, 1u);
      if (rect_id < srt.dof_buf.scatter_max_rect) {
        float4 coc4_bg = max(float4(0.0f), coc4);
        float4 bg_weights = dof_layer_weight(coc4_bg) * dof_sample_weight(coc4_bg) * do_scatter4;
        /* Filter NaNs. */
        bg_weights = select(bg_weights, float4(0.0f), equal(coc4_bg, float4(0.0f)));

        ScatterRect rect_bg;
        rect_bg.offset = offset;
        rect_bg.half_extent = half_extent;

        /* Circle of Confusion absolute radius in half-resolution pixels. */
        rect_bg.color_and_coc[0].a = coc4_bg[0];
        rect_bg.color_and_coc[1].a = coc4_bg[1];
        rect_bg.color_and_coc[2].a = coc4_bg[2];
        rect_bg.color_and_coc[3].a = coc4_bg[3];
        /* Apply weights. */
        rect_bg.color_and_coc[0].rgb = color4_0 * bg_weights[0];
        rect_bg.color_and_coc[1].rgb = color4_1 * bg_weights[1];
        rect_bg.color_and_coc[2].rgb = color4_2 * bg_weights[2];
        rect_bg.color_and_coc[3].rgb = color4_3 * bg_weights[3];

        srt.scatter_bg_list_buf[rect_id] = rect_bg;
      }
    }
  }

  /* Remove scatter color from gather. */
  float4 color_lod0 = srt.load_color_cache(thread_co) * (1.0f - do_scatter);
  srt.store_color_cache(thread_co, color_lod0);

  imageStore(srt.inout_color_lod0_img, texel, color_lod0);

  /* Recursive downsample. */
  for (uint i = 1u; i < DOF_MIP_COUNT; i++) {
    barrier();
    uint mask = ~(~0u << i);
    if (all(equal(local_id.xy & mask, uint2(0)))) {
      uint ofs = 1u << (i - 1u);

      /* TODO(fclem): Could use wave shuffle intrinsics to avoid LDS as suggested by the paper. */
      float4 coc4;
      coc4[0] = srt.load_coc_cache(thread_co + uint2(ofs, 0));
      coc4[1] = srt.load_coc_cache(thread_co + uint2(ofs, ofs));
      coc4[2] = srt.load_coc_cache(thread_co + uint2(0, ofs));
      coc4[3] = srt.load_coc_cache(thread_co + uint2(0, 0));
      float4 colors[4];
      colors[0] = srt.load_color_cache(thread_co + uint2(ofs, 0));
      colors[1] = srt.load_color_cache(thread_co + uint2(ofs, ofs));
      colors[2] = srt.load_color_cache(thread_co + uint2(0, ofs));
      colors[3] = srt.load_color_cache(thread_co + uint2(0, 0));

      float4 weights = dof_bilateral_coc_weights(coc4) * dof_bilateral_color_weights(colors);
      /* Normalize so that the sum is 1. */
      weights *= safe_rcp(reduce_add(weights));

      float4 color_lod = weighted_sum_array(colors, weights);
      float coc_lod = dot(coc4, weights);

      srt.store_color_cache(thread_co, color_lod);
      srt.store_coc_cache(thread_co, coc_lod);

      int2 texel = int2(global_id.xy >> i);

      if (i == 1) {
        imageStore(srt.out_color_lod1_img, texel, color_lod);
        imageStore(srt.out_coc_lod1_img, texel, float4(coc_lod));
      }
      else if (i == 2) {
        imageStore(srt.out_color_lod2_img, texel, color_lod);
        imageStore(srt.out_coc_lod2_img, texel, float4(coc_lod));
      }
      else /* if (i == 3) */ {
        imageStore(srt.out_color_lod3_img, texel, color_lod);
        imageStore(srt.out_coc_lod3_img, texel, float4(coc_lod));
      }
    }
  }
}

}  // namespace reduce

}  // namespace eevee::dof::setup

PipelineCompute eevee_depth_of_field_setup(eevee::dof::setup::setup::comp_main);
PipelineCompute eevee_depth_of_field_stabilize(eevee::dof::setup::stabilize::comp_main);
PipelineCompute eevee_depth_of_field_downsample(eevee::dof::setup::downsample::comp_main);
PipelineCompute eevee_depth_of_field_reduce(eevee::dof::setup::reduce::comp_main);
