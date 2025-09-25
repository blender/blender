/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Film accumulation utils functions.
 */

#include "infos/eevee_film_infos.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_film)

#include "draw_math_geom_lib.glsl"
#include "draw_view_lib.glsl"
#include "eevee_colorspace_lib.glsl"
#include "eevee_cryptomatte_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "eevee_velocity_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

/* Return scene linear Z depth from the camera or radial depth for panoramic cameras. */
float film_depth_convert_to_scene(float depth)
{
  if (false /* Panoramic. */) {
    /* TODO */
    return 1.0f;
  }
  return -drw_depth_screen_to_view(depth);
}

/* Load a texture sample in a specific format. Combined pass needs to use this. */
float4 film_texelfetch_as_YCoCg_opacity(sampler2D tx, int2 texel)
{
  float4 color = texelFetch(combined_tx, texel, 0);
  /* Convert transmittance to opacity. */
  color.a = saturate(1.0f - color.a);
  /* Transform to YCoCg for accumulation. */
  color.rgb = colorspace_YCoCg_from_scene_linear(color.rgb);
  return color;
}

/* Returns a weight based on Luma to reduce the flickering introduced by high energy pixels. */
float film_luma_weight(float luma)
{
  /* Slide 20 of "High Quality Temporal Supersampling" by Brian Karis at SIGGRAPH 2014. */
  /* To preserve more details in dark areas, we use a bigger bias. */
  return 1.0f / (4.0f + luma * uniform_buf.film.exposure_scale);
}

/**
 * Round floats mantissa before they get written to a 16 bit float storage to avoid drifting.
 *
 * Apparently, most (if not all) hardware truncate the mantissa when writing the attribute to a 16
 * bit float texture. This biases our accumulation drastically (see #126947). Manually rounding the
 * mantissa right before storage (and thus truncation) fixes the issue.
 */
float4 film_patch_float_for_16f_storage(float4 color)
{
  return uintBitsToFloat(floatBitsToUint(color) + 0x1000);
}
float film_patch_float_for_16f_storage(float value)
{
  return uintBitsToFloat(floatBitsToUint(value) + 0x1000);
}

/* -------------------------------------------------------------------- */
/** \name Filter
 * \{ */

FilmSample film_sample_get(int sample_n, int2 texel_film)
{
#ifdef PANORAMIC
  /* TODO(fclem): Panoramic projection will be more complex. The samples will have to be retrieve
   * at runtime, maybe by scanning a whole region. Offset and weight will have to be computed by
   * reprojecting the incoming pixel data into film pixel space. */
#else

  FilmSample film_sample = uniform_buf.film.samples[sample_n];

  if (scaling_factor > 1) {
    /* We are working in the render pixel region on the film. We use film pixel units. */

    float2 film_coord = 0.5f + float2(texel_film % scaling_factor);
    /* Sample position inside the render pixel region. */
    float2 jittered_sample_coord = (0.5f - uniform_buf.film.subpixel_offset) *
                                   float(scaling_factor);
    /* Offset the film samples to always sample the 4 nearest neighbors in the render target.
     * `film_sample.texel` is set to visit all 4 neighbors in [0..1] region. */
    int2 quad_offset = -int2(lessThan(film_coord, jittered_sample_coord));
    /* Select correct sample depending on which quadrant the film pixel lies. */
    film_sample.texel += quad_offset;
    jittered_sample_coord += float2(film_sample.texel * scaling_factor);

    float sample_dist_sqr = length_squared(jittered_sample_coord - film_coord);
    film_sample.weight = film_filter_weight(uniform_buf.film.filter_radius, sample_dist_sqr);
    /* Ensure a minimum weight for each sample to avoid missing data at 4x or 8x up-scaling. */
    film_sample.weight = max(film_sample.weight, 1e-8f);
  }

  film_sample.texel += (texel_film / scaling_factor) + uniform_buf.film.overscan;

#endif /* PANORAMIC */

  /* Use extend on borders. */
  film_sample.texel = clamp(film_sample.texel, int2(0, 0), uniform_buf.film.render_extent - 1);

  return film_sample;
}

/* Returns the combined weights of all samples affecting this film pixel. */
float film_weight_accumulation(int2 texel_film)
{
  /* TODO(fclem): Reference implementation, also needed for panoramic cameras. */
  if (scaling_factor > 1) {
    float weight = 0.0f;
    for (int i = 0; i < samples_len; i++) {
      weight += film_sample_get(i, texel_film).weight;
    }
    return weight;
  }
  return uniform_buf.film.samples_weight_total;
}

void film_sample_accum(
    FilmSample samp, int pass_id, int layer, sampler2DArray tex, inout float4 accum)
{
  if (pass_id < 0 || layer < 0) {
    return;
  }
  accum += texelFetch(tex, int3(samp.texel, layer), 0) * samp.weight;
}

void film_sample_accum(
    FilmSample samp, int pass_id, int layer, sampler2DArray tex, inout float accum)
{
  if (pass_id < 0 || layer < 0) {
    return;
  }
  accum += texelFetch(tex, int3(samp.texel, layer), 0).x * samp.weight;
}

void film_sample_accum_mist(FilmSample samp, inout float accum)
{
  if (uniform_buf.film.mist_id == -1) {
    return;
  }
  float depth = reverse_z::read(texelFetch(depth_tx, samp.texel, 0).x);
  float2 uv = (float2(samp.texel) + 0.5f) / float2(textureSize(depth_tx, 0).xy);
  float3 vP = drw_point_screen_to_view(float3(uv, depth));
  bool is_persp = drw_view().winmat[3][3] == 0.0f;
  float mist = (is_persp) ? length(vP) : abs(vP.z);
  /* Remap to 0..1 range. */
  mist = saturate(mist * uniform_buf.film.mist_scale + uniform_buf.film.mist_bias);
  /* Falloff. */
  mist = pow(mist, uniform_buf.film.mist_exponent);
  accum += mist * samp.weight;
}

void film_sample_accum_combined(FilmSample samp, inout float4 accum, inout float weight_accum)
{
  if (combined_id == -1) {
    return;
  }
  float4 color = film_texelfetch_as_YCoCg_opacity(combined_tx, samp.texel);

  /* Weight by luma to remove fireflies. */
  float weight = film_luma_weight(color.x) * samp.weight;

  accum += color * weight;
  weight_accum += weight;
}

void film_sample_cryptomatte_accum(FilmSample samp,
                                   int layer,
                                   sampler2D tex,
                                   inout float2 crypto_samples[4])
{
  float hash = texelFetch(tex, samp.texel, 0)[layer];
  /* Find existing entry. */
  for (int i = 0; i < 4; i++) {
    if (crypto_samples[i].x == hash) {
      crypto_samples[i].y += samp.weight;
      return;
    }
  }
  /* Overwrite entry with less weight. */
  for (int i = 0; i < 4; i++) {
    if (crypto_samples[i].y < samp.weight) {
      crypto_samples[i] = float2(hash, samp.weight);
      return;
    }
  }
}

void film_cryptomatte_layer_accum_and_store(
    FilmSample dst, int2 texel_film, int pass_id, int layer_component, inout float4 out_color)
{
  if (pass_id == -1) {
    return;
  }
  /* x = hash, y = accumulated weight. Only keep track of 4 highest weighted samples. */
  float2 crypto_samples[4] = float2_array(float2(0.0f), float2(0.0f), float2(0.0f), float2(0.0f));
  for (int i = 0; i < samples_len; i++) {
    FilmSample src = film_sample_get(i, texel_film);
    film_sample_cryptomatte_accum(src, layer_component, cryptomatte_tx, crypto_samples);
  }
  float4 display_color = float4(0.0f);
  for (int i = 0; i < 4; i++) {
    cryptomatte_store_film_sample(dst, pass_id, crypto_samples[i], display_color);
  }

  if (uniform_buf.film.display_storage_type == PASS_STORAGE_CRYPTOMATTE) {
    out_color = display_color;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Load/Store Data
 * \{ */

/* Returns the distance used to store nearest interpolation data. */
float film_distance_load(int2 texel)
{
  /* Repeat texture coordinates as the weight can be optimized to a small portion of the film. */
  texel = texel % imageSize(in_weight_img).xy;

  if (!uniform_buf.film.use_history || use_reprojection) {
    return 0.0f;
  }
  return imageLoadFast(in_weight_img, int3(texel, FILM_WEIGHT_LAYER_DISTANCE)).x;
}

float film_weight_load(int2 texel)
{
  /* Repeat texture coordinates as the weight can be optimized to a small portion of the film. */
  texel = texel % imageSize(in_weight_img).xy;

  if (!uniform_buf.film.use_history || use_reprojection) {
    return 0.0f;
  }
  return imageLoadFast(in_weight_img, int3(texel, FILM_WEIGHT_LAYER_ACCUMULATION)).x;
}

/* Returns motion in pixel space to retrieve the pixel history. */
float2 film_pixel_history_motion_vector(int2 texel_sample)
{
  /**
   * Dilate velocity by using the nearest pixel in a cross pattern.
   * "High Quality Temporal Supersampling" by Brian Karis at SIGGRAPH 2014 (Slide 27)
   */
  constexpr int2 corners[4] = int2_array(int2(-2, -2), int2(2, -2), int2(-2, 2), int2(2, 2));
  float min_depth = reverse_z::read(texelFetch(depth_tx, texel_sample, 0).x);
  int2 nearest_texel = texel_sample;
  for (int i = 0; i < 4; i++) {
    int2 texel = clamp(texel_sample + corners[i], int2(0), textureSize(depth_tx, 0).xy - 1);
    float depth = reverse_z::read(texelFetch(depth_tx, texel, 0).x);
    if (min_depth > depth) {
      min_depth = depth;
      nearest_texel = texel;
    }
  }

  float4 vector = velocity_resolve(vector_tx, nearest_texel, min_depth);

  /* Transform to pixel space. */
  vector.xy *= float2(uniform_buf.film.extent);

  return vector.xy;
}

/* \a t is inter-pixel position. 0 means perfectly on a pixel center.
 * Returns weights in both dimensions.
 * Multiply each dimension weights to get final pixel weights. */
void film_get_catmull_rom_weights(float2 t, out float2 weights[4])
{
  float2 t2 = t * t;
  float2 t3 = t2 * t;
  float fc = 0.5f; /* Catmull-Rom. */

  float2 fct = t * fc;
  float2 fct2 = t2 * fc;
  float2 fct3 = t3 * fc;
  weights[0] = (fct2 * 2.0f - fct3) - fct;
  weights[1] = (t3 * 2.0f - fct3) + (-t2 * 3.0f + fct2) + 1.0f;
  weights[2] = (-t3 * 2.0f + fct3) + (t2 * 3.0f - (2.0f * fct2)) + fct;
  weights[3] = fct3 - fct2;
}

/* Load color using a special filter to avoid losing detail.
 * \a texel is sample position with subpixel accuracy. */
float4 film_sample_catmull_rom(sampler2D color_tx, float2 input_texel)
{
  float2 center_texel;
  float2 inter_texel = modf(input_texel, center_texel);
  float2 weights[4];
  film_get_catmull_rom_weights(inter_texel, weights);

#if 0 /* Reference. 16 Taps. */
  float4 color = float4(0.0f);
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      int2 texel = int2(center_texel) + int2(x, y) - 1;
      texel = clamp(texel, int2(0), textureSize(color_tx, 0).xy - 1);
      color += texelFetch(color_tx, texel, 0) * weights[x].x * weights[y].y;
    }
  }
  return color;

#elif 1 /* Optimize version. 5 Bilinear Taps. */
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

  color = textureLod(color_tx, uv_12, 0.0f) * weight_center;
  color += textureLod(color_tx, float2(uv_12.x, uv_0.y), 0.0f) * weight_cross.x;
  color += textureLod(color_tx, float2(uv_0.x, uv_12.y), 0.0f) * weight_cross.y;
  color += textureLod(color_tx, float2(uv_3.x, uv_12.y), 0.0f) * weight_cross.z;
  color += textureLod(color_tx, float2(uv_12.x, uv_3.y), 0.0f) * weight_cross.w;
  /* Re-normalize for the removed corners. */
  return color / (weight_center + reduce_add(weight_cross));

#else /* Nearest interpolation for debugging. 1 Tap. */
  int2 texel = int2(center_texel) + int2(greaterThan(inter_texel, float2(0.5f)));
  texel = clamp(texel, int2(0), textureSize(color_tx, 0).xy - 1);
  return texelFetch(color_tx, texel, 0);
#endif
}

/* Return history clipping bounding box in YCoCg color space. */
void film_combined_neighbor_boundbox(int2 texel, out float4 min_c, out float4 max_c)
{
  /* Plus (+) shape offsets. */
  constexpr int2 plus_offsets[5] = int2_array(int2(0, 0), /* Center */
                                              int2(-1, 0),
                                              int2(0, -1),
                                              int2(1, 0),
                                              int2(0, 1));
#if 0
  /**
   * Compute Variance of neighborhood as described in:
   * "An Excursion in Temporal Supersampling" by Marco Salvi at GDC 2016.
   * and:
   * "A Survey of Temporal Anti-aliasing Techniques" by Yang et al.
   */

  /* First 2 moments. */
  float4 mu1 = float4(0), mu2 = float4(0);
  for (int i = 0; i < 5; i++) {
    float4 color = film_texelfetch_as_YCoCg_opacity(combined_tx, texel + plus_offsets[i]);
    mu1 += color;
    mu2 += square(color);
  }
  mu1 *= (1.0f / 5.0f);
  mu2 *= (1.0f / 5.0f);

  /* Extent scaling. Range [0.75..1.25].
   * Balance between more flickering (0.75) or more ghosting (1.25). */
  constexpr float gamma = 1.25f;
  /* Standard deviation. */
  float4 sigma = sqrt(abs(mu2 - square(mu1)));
  /* eq. 6 in "A Survey of Temporal Anti-aliasing Techniques". */
  min_c = mu1 - gamma * sigma;
  max_c = mu1 + gamma * sigma;
#else
  /**
   * Simple bounding box calculation in YCoCg as described in:
   * "High Quality Temporal Supersampling" by Brian Karis at SIGGRAPH 2014
   */
  min_c = float4(1e16f);
  max_c = float4(-1e16f);
  for (int i = 0; i < 5; i++) {
    float4 color = film_texelfetch_as_YCoCg_opacity(combined_tx, texel + plus_offsets[i]);
    min_c = min(min_c, color);
    max_c = max(max_c, color);
  }
  /* (Slide 32) Simple clamp to min/max of 8 neighbors results in 3x3 box artifacts.
   * Round bbox shape by averaging 2 different min/max from 2 different neighborhood. */
  float4 min_c_3x3 = min_c;
  float4 max_c_3x3 = max_c;
  constexpr int2 corners[4] = int2_array(int2(-1, -1), int2(1, -1), int2(-1, 1), int2(1, 1));
  for (int i = 0; i < 4; i++) {
    float4 color = film_texelfetch_as_YCoCg_opacity(combined_tx, texel + corners[i]);
    min_c_3x3 = min(min_c_3x3, color);
    max_c_3x3 = max(max_c_3x3, color);
  }
  min_c = (min_c + min_c_3x3) * 0.5f;
  max_c = (max_c + max_c_3x3) * 0.5f;
#endif
}

/* 1D equivalent of line_aabb_clipping_dist(). */
float film_aabb_clipping_dist_alpha(float origin, float direction, float aabb_min, float aabb_max)
{
  if (abs(direction) < 1e-5f) {
    return 0.0f;
  }
  float nearest_plane = (direction > 0.0f) ? aabb_min : aabb_max;
  return (nearest_plane - origin) / direction;
}

/* Modulate the history color to avoid ghosting artifact. */
float4 film_amend_combined_history(
    float4 min_color, float4 max_color, float4 color_history, float4 src_color, int2 src_texel)
{
  /* Clip instead of clamping to avoid color accumulating in the AABB corners. */
  float4 clip_dir = src_color - color_history;

  float t = line_aabb_clipping_dist(color_history.rgb, clip_dir.rgb, min_color.rgb, max_color.rgb);
  color_history.rgb += clip_dir.rgb * saturate(t);

  /* Clip alpha on its own to avoid interference with other channels. */
  float t_a = film_aabb_clipping_dist_alpha(color_history.a, clip_dir.a, min_color.a, max_color.a);
  color_history.a += clip_dir.a * saturate(t_a);

  return color_history;
}

float film_history_blend_factor(float velocity,
                                float2 texel,
                                float luma_min,
                                float luma_max,
                                float luma_incoming,
                                float luma_history)
{
  /* 5% of incoming color by default. */
  float blend = 0.05f;
  /* Blend less history if the pixel has substantial velocity. */
  blend = mix(blend, 0.20f, saturate(velocity * 0.02f));
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
  /* Discard out of view history. */
  if (any(lessThan(texel, float2(0))) ||
      any(greaterThanEqual(texel, float2(uniform_buf.film.extent))))
  {
    blend = 1.0f;
  }
  /* Discard history if invalid. */
  if (uniform_buf.film.use_history == false) {
    blend = 1.0f;
  }
  return blend;
}

/* Returns resolved final color. */
void film_store_combined(
    FilmSample dst, int2 src_texel, float4 color, float color_weight, inout float4 display)
{
  if (combined_id == -1) {
    return;
  }

  float4 color_src, color_dst;
  float weight_src, weight_dst;

  /* Undo the weighting to get final spatially-filtered color. */
  color_src = color / color_weight;

  if (use_reprojection) {
    /* Interactive accumulation. Do reprojection and Temporal Anti-Aliasing. */

    /* Reproject by finding where this pixel was in the previous frame. */
    float2 motion = film_pixel_history_motion_vector(src_texel);
    float2 history_texel = float2(dst.texel) + motion;

    float velocity = length(motion);

    /* Load weight if it is not uniform across the whole buffer (i.e: upsampling, panoramic). */
    // dst.weight = film_weight_load(texel_combined);

    color_dst = film_sample_catmull_rom(in_combined_tx, history_texel);
    color_dst.rgb = colorspace_YCoCg_from_scene_linear(color_dst.rgb);

    /* Get local color bounding box of source neighborhood. */
    float4 min_color, max_color;
    film_combined_neighbor_boundbox(src_texel, min_color, max_color);

    float blend = film_history_blend_factor(
        velocity, history_texel, min_color.x, max_color.x, color_src.x, color_dst.x);

    color_dst = film_amend_combined_history(min_color, max_color, color_dst, color_src, src_texel);

    /* Luma weighted blend to avoid flickering. */
    weight_dst = film_luma_weight(color_dst.x) * (1.0f - blend);
    weight_src = film_luma_weight(color_src.x) * (blend);
  }
  else {
    /* Everything is static. Use render accumulation. */
    color_dst = texelFetch(in_combined_tx, dst.texel, 0);
    color_dst.rgb = colorspace_YCoCg_from_scene_linear(color_dst.rgb);

    /* Luma weighted blend to avoid flickering. */
    weight_dst = film_luma_weight(color_dst.x) * dst.weight;
    weight_src = color_weight;
  }
  /* Weighted blend. */
  color = color_dst * weight_dst + color_src * weight_src;
  color /= weight_src + weight_dst;

  color.rgb = colorspace_scene_linear_from_YCoCg(color.rgb);

  /* Fix alpha not accumulating to 1 because of float imprecision. */
  if (color.a > 0.995f) {
    color.a = 1.0f;
  }

  /* Filter NaNs. */
  if (any(isnan(color))) {
    color = float4(0.0f, 0.0f, 0.0f, 1.0f);
  }

  if (display_id == -1) {
    display = color;
  }
  color = film_patch_float_for_16f_storage(color);
  imageStoreFast(out_combined_img, dst.texel, color);
}

void film_store_color(FilmSample dst, int pass_id, float4 color, inout float4 display)
{
  if (pass_id == -1) {
    return;
  }

  float4 data_film = imageLoadFast(color_accum_img, int3(dst.texel, pass_id));

  color = (data_film * dst.weight + color) * dst.weight_sum_inv;

  /* Filter NaNs. */
  if (any(isnan(color))) {
    color = float4(0.0f, 0.0f, 0.0f, 1.0f);
  }

  /* Fix alpha not accumulating to 1 because of float imprecision. But here we cannot assume that
   * the alpha contains actual transparency and not user data. Only bias if very close to 1. */
  if (color.a > 0.9999f && color.a < 1.0f) {
    color.a = 1.0f;
  }

  if (display_id == pass_id) {
    display = color;
  }
  color = film_patch_float_for_16f_storage(color);
  imageStoreFast(color_accum_img, int3(dst.texel, pass_id), color);
}

void film_store_value(FilmSample dst, int pass_id, float value, inout float4 display)
{
  if (pass_id == -1) {
    return;
  }

  float data_film = imageLoadFast(value_accum_img, int3(dst.texel, pass_id)).x;

  value = (data_film * dst.weight + value) * dst.weight_sum_inv;

  /* Filter NaNs. */
  if (isnan(value)) {
    value = 0.0f;
  }

  if (display_id == pass_id) {
    display = float4(value, value, value, 1.0f);
  }
  value = film_patch_float_for_16f_storage(value);
  imageStoreFast(value_accum_img, int3(dst.texel, pass_id), float4(value));
}

/* Nearest sample variant. Always stores the data. */
void film_store_data(int2 texel_film, int pass_id, float4 data_sample, inout float4 display)
{
  if (pass_id == -1) {
    return;
  }

  if (display_id == pass_id) {
    display = data_sample;
  }
  imageStoreFast(color_accum_img, int3(texel_film, pass_id), data_sample);
}

void film_store_depth(int2 texel_film, float value, out float out_depth)
{
  if (uniform_buf.film.depth_id == -1) {
    return;
  }

  out_depth = film_depth_convert_to_scene(value);

  imageStoreFast(depth_img, texel_film, float4(out_depth));
}

void film_store_distance(int2 texel, float value)
{
  imageStoreFast(out_weight_img, int3(texel, FILM_WEIGHT_LAYER_DISTANCE), float4(value));
}

void film_store_weight(int2 texel, float value)
{
  imageStoreFast(out_weight_img, int3(texel, FILM_WEIGHT_LAYER_ACCUMULATION), float4(value));
}

float film_display_depth_amend(int2 texel, float depth)
{
  /* This effectively offsets the depth of the whole 2x2 region to the lowest value of the region
   * twice. One for X and one for Y direction. */
  /* TODO(fclem): This could be improved as it gives flickering result at depth discontinuity.
   * But this is the quickest stable result I could come with for now. */
  depth += gpu_fwidth(depth);
  /* Small offset to avoid depth test lessEqual failing because of all the conversions loss. */
  depth += 2.4e-7f * 4.0f;
  return saturate(depth);
}

/** \} */

/** NOTE: out_depth is scene linear depth from the camera origin. */
void film_process_data(int2 texel_film, out float4 out_color, out float out_depth)
{
  out_color = float4(0.0f);
  out_depth = 0.0f;

  float weight_accum = film_weight_accumulation(texel_film);
  float film_weight = film_weight_load(texel_film);
  float weight_sum = film_weight + weight_accum;
  film_store_weight(texel_film, weight_sum);

  FilmSample dst;
  dst.texel = texel_film;
  dst.weight = film_weight;
  dst.weight_sum_inv = 1.0f / weight_sum;

  /* NOTE: We split the accumulations into separate loops to avoid using too much registers and
   * maximize occupancy. */

  if (combined_id != -1) {
    /* NOTE: Do weight accumulation again since we use custom weights. */
    float weight_accum = 0.0f;
    float4 combined_accum = float4(0.0f);

    FilmSample src;
    for (int i = samples_len - 1; i >= 0; i--) {
      src = film_sample_get(i, texel_film);
      film_sample_accum_combined(src, combined_accum, weight_accum);
    }
    /* NOTE: src.texel is center texel in incoming data buffer. */
    film_store_combined(dst, src.texel, combined_accum, weight_accum, out_color);
  }

  if (flag_test(enabled_categories, PASS_CATEGORY_DATA)) {
    float film_distance = film_distance_load(texel_film);

    /* Get sample closest to target texel. It is always sample 0. */
    FilmSample film_sample = film_sample_get(0, texel_film);

    /* Using film weight as distance to the pixel. So the check is inverted. */
    if (film_sample.weight > film_distance) {
      float depth = reverse_z::read(texelFetch(depth_tx, film_sample.texel, 0).x);
      float4 vector = velocity_resolve(vector_tx, film_sample.texel, depth);
      /* Transform to pixel space, matching Cycles format. */
      vector *= float4(float2(uniform_buf.film.render_extent),
                       float2(uniform_buf.film.render_extent));

      film_store_depth(texel_film, depth, out_depth);
      if (normal_id != -1) {
        float4 normal = texelFetch(
            rp_color_tx, int3(film_sample.texel, uniform_buf.render_pass.normal_id), 0);
        film_store_data(texel_film, normal_id, normal, out_color);
      }
      if (uniform_buf.film.position_id != -1) {
        float4 position = texelFetch(
            rp_color_tx, int3(film_sample.texel, uniform_buf.render_pass.position_id), 0);
        film_store_data(texel_film, uniform_buf.film.position_id, position, out_color);
      }
      film_store_data(texel_film, uniform_buf.film.vector_id, vector, out_color);
      film_store_distance(texel_film, film_sample.weight);
    }
    else {
      out_depth = imageLoadFast(depth_img, texel_film).r;
      if (display_id == -1) {
        /* NOP. */
      }
      else if (display_id == normal_id) {
        out_color = imageLoadFast(color_accum_img, int3(texel_film, display_id));
      }
      else if (display_id == uniform_buf.film.position_id) {
        out_color = imageLoadFast(color_accum_img, int3(texel_film, uniform_buf.film.position_id));
      }
    }
  }

  if (flag_test(enabled_categories, PASS_CATEGORY_COLOR_1)) {
    float4 diffuse_light_accum = float4(0.0f);
    float4 specular_light_accum = float4(0.0f);
    float4 volume_light_accum = float4(0.0f);
    float4 emission_accum = float4(0.0f);

    for (int i = 0; i < samples_len; i++) {
      FilmSample src = film_sample_get(i, texel_film);
      film_sample_accum(src,
                        uniform_buf.film.diffuse_light_id,
                        uniform_buf.render_pass.diffuse_light_id,
                        rp_color_tx,
                        diffuse_light_accum);
      film_sample_accum(src,
                        uniform_buf.film.specular_light_id,
                        uniform_buf.render_pass.specular_light_id,
                        rp_color_tx,
                        specular_light_accum);
      film_sample_accum(src,
                        uniform_buf.film.volume_light_id,
                        uniform_buf.render_pass.volume_light_id,
                        rp_color_tx,
                        volume_light_accum);
      film_sample_accum(src,
                        uniform_buf.film.emission_id,
                        uniform_buf.render_pass.emission_id,
                        rp_color_tx,
                        emission_accum);
    }
    film_store_color(dst, uniform_buf.film.diffuse_light_id, diffuse_light_accum, out_color);
    film_store_color(dst, uniform_buf.film.specular_light_id, specular_light_accum, out_color);
    film_store_color(dst, uniform_buf.film.volume_light_id, volume_light_accum, out_color);
    film_store_color(dst, uniform_buf.film.emission_id, emission_accum, out_color);
  }

  if (flag_test(enabled_categories, PASS_CATEGORY_COLOR_2)) {
    float4 diffuse_color_accum = float4(0.0f);
    float4 specular_color_accum = float4(0.0f);
    float4 environment_accum = float4(0.0f);
    float mist_accum = 0.0f;
    float shadow_accum = 0.0f;
    float ao_accum = 0.0f;

    for (int i = 0; i < samples_len; i++) {
      FilmSample src = film_sample_get(i, texel_film);
      film_sample_accum(src,
                        uniform_buf.film.diffuse_color_id,
                        uniform_buf.render_pass.diffuse_color_id,
                        rp_color_tx,
                        diffuse_color_accum);
      film_sample_accum(src,
                        uniform_buf.film.specular_color_id,
                        uniform_buf.render_pass.specular_color_id,
                        rp_color_tx,
                        specular_color_accum);
      film_sample_accum(src,
                        uniform_buf.film.environment_id,
                        uniform_buf.render_pass.environment_id,
                        rp_color_tx,
                        environment_accum);
      film_sample_accum(src,
                        uniform_buf.film.shadow_id,
                        uniform_buf.render_pass.shadow_id,
                        rp_value_tx,
                        shadow_accum);
      film_sample_accum(src,
                        uniform_buf.film.ambient_occlusion_id,
                        uniform_buf.render_pass.ambient_occlusion_id,
                        rp_value_tx,
                        ao_accum);
      film_sample_accum_mist(src, mist_accum);
    }
    /* Monochrome render passes that have colored outputs. Set alpha to 1. */
    float4 shadow_accum_color = float4(float3(shadow_accum), weight_accum);
    float4 ao_accum_color = float4(float3(ao_accum), weight_accum);

    film_store_color(dst, uniform_buf.film.diffuse_color_id, diffuse_color_accum, out_color);
    film_store_color(dst, uniform_buf.film.specular_color_id, specular_color_accum, out_color);
    film_store_color(dst, uniform_buf.film.environment_id, environment_accum, out_color);
    film_store_color(dst, uniform_buf.film.shadow_id, shadow_accum_color, out_color);
    film_store_color(dst, uniform_buf.film.ambient_occlusion_id, ao_accum_color, out_color);
    film_store_value(dst, uniform_buf.film.mist_id, mist_accum, out_color);
  }

  if (flag_test(enabled_categories, PASS_CATEGORY_COLOR_3)) {
    float4 transparent_accum = float4(0.0f);

    for (int i = 0; i < samples_len; i++) {
      FilmSample src = film_sample_get(i, texel_film);
      film_sample_accum(src,
                        uniform_buf.film.transparent_id,
                        uniform_buf.render_pass.transparent_id,
                        rp_color_tx,
                        transparent_accum);
    }
    /* Alpha stores transmittance for transparent pass. */
    transparent_accum.a = weight_accum - transparent_accum.a;

    film_store_color(dst, uniform_buf.film.transparent_id, transparent_accum, out_color);
  }

  if (flag_test(enabled_categories, PASS_CATEGORY_AOV)) {
    for (int aov = 0; aov < uniform_buf.film.aov_color_len; aov++) {
      float4 aov_accum = float4(0.0f);

      for (int i = 0; i < samples_len; i++) {
        FilmSample src = film_sample_get(i, texel_film);
        film_sample_accum(src, 0, uniform_buf.render_pass.color_len + aov, rp_color_tx, aov_accum);
      }
      film_store_color(dst, uniform_buf.film.aov_color_id + aov, aov_accum, out_color);
    }

    for (int aov = 0; aov < uniform_buf.film.aov_value_len; aov++) {
      float aov_accum = 0.0f;

      for (int i = 0; i < samples_len; i++) {
        FilmSample src = film_sample_get(i, texel_film);
        film_sample_accum(src, 0, uniform_buf.render_pass.value_len + aov, rp_value_tx, aov_accum);
      }
      film_store_value(dst, uniform_buf.film.aov_value_id + aov, aov_accum, out_color);
    }
  }

  if (flag_test(enabled_categories, PASS_CATEGORY_CRYPTOMATTE)) {
    if (uniform_buf.film.cryptomatte_samples_len != 0) {
      /* Cryptomatte passes cannot be cleared by a weighted store like other passes. */
      if (!uniform_buf.film.use_history || use_reprojection) {
        cryptomatte_clear_samples(dst);
      }

      film_cryptomatte_layer_accum_and_store(
          dst, texel_film, uniform_buf.film.cryptomatte_object_id, 0, out_color);
      film_cryptomatte_layer_accum_and_store(
          dst, texel_film, uniform_buf.film.cryptomatte_asset_id, 1, out_color);
      film_cryptomatte_layer_accum_and_store(
          dst, texel_film, uniform_buf.film.cryptomatte_material_id, 2, out_color);
    }
  }
}
