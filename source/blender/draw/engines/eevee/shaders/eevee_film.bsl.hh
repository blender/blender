/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Film accumulation utils functions.
 */

#include "draw_math_geom_lib.glsl"
#include "eevee_colorspace_lib.bsl.hh"
#include "eevee_cryptomatte.bsl.hh"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_uniform.bsl.hh"
#include "eevee_velocity.bsl.hh"
#include "gpu_shader_fullscreen_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

namespace eevee::film {

/* Return scene linear Z depth from the camera or radial depth for panoramic cameras. */
float depth_convert_to_scene(const ViewMatrices view, float depth)
{
  if (false /* Panoramic. */) {
    /* TODO */
    return 1.0f;
  }
  return -view.depth_screen_to_view(depth);
}

float display_depth_amend(float depth)
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

/* Load a texture sample in a specific format. Combined pass needs to use this. */
float4 texelfetch_as_YCoCg_opacity(sampler2D tx, int2 texel)
{
  float4 color = texelFetch(tx, texel, 0);
  /* Convert transmittance to opacity. */
  color.a = saturate(1.0f - color.a);
  /* Transform to YCoCg for accumulation. */
  color.rgb = colorspace::YCoCg_from_scene_linear(color.rgb);
  return color;
}

/* Returns a weight based on Luma to reduce the flickering introduced by high energy pixels. */
float luma_weight([[resource_table]] const Uniform &uni, float luma)
{
  /* Slide 20 of "High Quality Temporal Supersampling" by Brian Karis at SIGGRAPH 2014. */
  /* To preserve more details in dark areas, we use a bigger bias. */
  return 1.0f / (4.0f + luma * uni.uniform_buf.film.exposure_scale);
}

/**
 * Round floats mantissa before they get written to a 16 bit float storage to avoid drifting.
 *
 * Apparently, most (if not all) hardware truncate the mantissa when writing the attribute to a 16
 * bit float texture. This biases our accumulation drastically (see #126947). Manually rounding the
 * mantissa right before storage (and thus truncation) fixes the issue.
 */
float4 patch_float_for_16f_storage(float4 color)
{
  return uintBitsToFloat(floatBitsToUint(color) + 0x1000);
}
float patch_float_for_16f_storage(float value)
{
  return uintBitsToFloat(floatBitsToUint(value) + 0x1000);
}

float4 safe_divide_even_color(float4 a, float4 b)
{
  a *= safe_rcp(b);
  /* Try to get gray even if b is zero. */
  if (b.x == 0.0f) {
    if (b.y == 0.0f) {
      a.x = a.z;
      a.y = a.z;
    }
    else if (b.z == 0.0f) {
      a.x = a.y;
      a.z = a.y;
    }
    else {
      a.x = 0.5f * (a.y + a.z);
    }
  }
  else if (b.y == 0.0f) {
    if (b.z == 0.0f) {
      a.y = a.x;
      a.z = a.x;
    }
    else {
      a.y = 0.5f * (a.x + a.z);
    }
  }
  else if (b.z == 0.0f) {
    a.z = 0.5f * (a.x + a.y);
  }
  return a;
}

struct Film {
  [[resource_table]] srt_t<CameraVelocity> camera;
  [[resource_table]] srt_t<draw::View> views_;

  [[specialization_constant(1)]] uint enabled_categories;
  [[specialization_constant(9)]] int samples_len;
  [[specialization_constant(true)]] bool use_reprojection;
  [[specialization_constant(1)]] int scaling_factor;
  [[specialization_constant(0)]] int combined_id;
  [[specialization_constant(-1)]] int display_id;
  [[specialization_constant(-1)]] int normal_id;

  /* Sample inputs. Data freshly rendered. */
  [[sampler(0)]] sampler2DDepth depth_tx;
  [[sampler(1)]] sampler2D combined_tx;
  [[sampler(2)]] sampler2D vector_tx;
  [[sampler(3)]] sampler2DArray rp_color_tx;
  [[sampler(4)]] sampler2DArray rp_value_tx;
  [[sampler(6)]] sampler2D cryptomatte_tx;

  /* Color History for TAA needs to be sampler to leverage bilinear sampling. */
  [[sampler(5)]] sampler2D in_combined_tx;

  [[image(0, read, SFLOAT_32)]] const image2DArray in_weight_img;
  [[image(1, write, SFLOAT_32)]] image2DArray out_weight_img;

  /* Accumulation buffers. */
  [[image(3, read_write, SFLOAT_16_16_16_16)]] image2D out_combined_img;
  [[image(4, read_write, SFLOAT_32)]] image2D depth_img;
  [[image(5, read_write, SFLOAT_16_16_16_16)]] image2DArray color_accum_img;
  [[image(6, read_write, SFLOAT_16)]] image2DArray value_accum_img;

  [[resource_table]] srt_t<Cryptomatte> cryptomatte;
  [[resource_table]] srt_t<Uniform> uniforms;

  /* -------------------------------------------------------------------- */
  /** \name Filter
   * \{ */

  FilmSample sample_get(int sample_n, int2 texel_film)
  {
    [[resource_table]] const Uniform &uni = this->uniforms;

#ifdef PANORAMIC
    /* TODO(fclem): Panoramic projection will be more complex. The samples will have to be retrieve
     * at runtime, maybe by scanning a whole region. Offset and weight will have to be computed by
     * reprojecting the incoming pixel data into film pixel space. */
#else

    FilmSample film_sample = uni.uniform_buf.film.samples[sample_n];

    if (scaling_factor > 1) {
      /* We are working in the render pixel region on the film. We use film pixel units. */

      float2 film_coord = 0.5f + float2(texel_film % scaling_factor);
      /* Sample position inside the render pixel region. */
      float2 jittered_sample_coord = (0.5f - uni.uniform_buf.film.subpixel_offset) *
                                     float(scaling_factor);
      /* Offset the film samples to always sample the 4 nearest neighbors in the render target.
       * `film_sample.texel` is set to visit all 4 neighbors in [0..1] region. */
      int2 quad_offset = -int2(lessThan(film_coord, jittered_sample_coord));
      /* Select correct sample depending on which quadrant the film pixel lies. */
      film_sample.texel += quad_offset;
      jittered_sample_coord += float2(film_sample.texel * scaling_factor);

      float sample_dist_sqr = length_squared(jittered_sample_coord - film_coord);
      film_sample.weight = film_filter_weight(uni.uniform_buf.film.filter_radius, sample_dist_sqr);
      /* Ensure a minimum weight for each sample to avoid missing data at 4x or 8x up-scaling. */
      film_sample.weight = max(film_sample.weight, 1e-8f);
    }

    film_sample.texel += (texel_film / scaling_factor) + uni.uniform_buf.film.overscan;

#endif /* PANORAMIC */

    /* Use extend on borders. */
    film_sample.texel = clamp(
        film_sample.texel, int2(0, 0), uni.uniform_buf.film.render_extent - 1);

    return film_sample;
  }

  /* Returns the combined weights of all samples affecting this film pixel. */
  float weight_accumulation(int2 texel_film)
  {
    [[resource_table]] const Uniform &uni = this->uniforms;
    /* TODO(fclem): Reference implementation, also needed for panoramic cameras. */
    if (scaling_factor > 1) {
      float weight = 0.0f;
      for (int i = 0; i < samples_len; i++) {
        weight += sample_get(i, texel_film).weight;
      }
      return weight;
    }
    return uni.uniform_buf.film.samples_weight_total;
  }

  void sample_accum(FilmSample samp, int pass_id, int layer, sampler2DArray tex, float4 &accum)
  {
    if (pass_id < 0 || layer < 0) {
      return;
    }
    accum += texelFetch(tex, int3(samp.texel, layer), 0) * samp.weight;
  }

  void sample_accum(FilmSample samp, int pass_id, int layer, sampler2DArray tex, float &accum)
  {
    if (pass_id < 0 || layer < 0) {
      return;
    }
    accum += texelFetch(tex, int3(samp.texel, layer), 0).x * samp.weight;
  }

  void sample_accum_mist(FilmSample samp, float &accum)
  {
    [[resource_table]] const Uniform &uni = this->uniforms;
    [[resource_table]] const draw::View &views = this->views_;

    if (uni.uniform_buf.film.mist_id == -1) {
      return;
    }
    const ViewMatrices view = views.get(0);
    float depth = reverse_z::read(texelFetch(depth_tx, samp.texel, 0).x);
    float2 uv = (float2(samp.texel) + 0.5f) / float2(textureSize(depth_tx, 0).xy);
    float3 vP = view.point_screen_to_view(float3(uv, depth));
    bool is_persp = view.winmat[3][3] == 0.0f;
    float mist = (is_persp) ? length(vP) : abs(vP.z);
    /* Remap to 0..1 range. */
    mist = saturate(mist * uni.uniform_buf.film.mist_scale + uni.uniform_buf.film.mist_bias);
    /* Falloff. */
    mist = pow(mist, uni.uniform_buf.film.mist_exponent);
    accum += mist * samp.weight;
  }

  void sample_accum_combined(FilmSample samp, float4 &accum, float &weight_accum)
  {
    [[resource_table]] const Uniform &uni = this->uniforms;
    if (combined_id == -1) {
      return;
    }
    float4 color = texelfetch_as_YCoCg_opacity(combined_tx, samp.texel);

    /* Weight by luma to remove fireflies. */
    float weight = luma_weight(uni, color.x) * samp.weight;

    accum += color * weight;
    weight_accum += weight;
  }

  void sample_cryptomatte_accum(FilmSample samp,
                                int layer,
                                sampler2D tex,
                                float2 (&crypto_samples)[4])
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

  void cryptomatte_layer_accum_and_store(
      FilmSample dst, int2 texel_film, int pass_id, int layer_component, float4 &out_color)
  {
    if (pass_id == -1) {
      return;
    }

    [[resource_table]] Cryptomatte &crypto = this->cryptomatte;
    [[resource_table]] const Uniform &uni = this->uniforms;

    /* x = hash, y = accumulated weight. Only keep track of 4 highest weighted samples. */
    float2 crypto_samples[4] = float2_array(
        float2(0.0f), float2(0.0f), float2(0.0f), float2(0.0f));
    for (int i = 0; i < samples_len; i++) {
      FilmSample src = sample_get(i, texel_film);
      sample_cryptomatte_accum(src, layer_component, cryptomatte_tx, crypto_samples);
    }
    float4 display_color = float4(0.0f);
    for (int i = 0; i < 4; i++) {
      crypto.store_film_sample(dst,
                               pass_id,
                               uni.uniform_buf.film.cryptomatte_samples_len,
                               crypto_samples[i],
                               display_color);
    }

    if (uni.uniform_buf.film.display_storage_type == PASS_STORAGE_CRYPTOMATTE) {
      out_color = display_color;
    }
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Load/Store Data
   * \{ */

  /* Returns the distance used to store nearest interpolation data. */
  float distance_load(int2 texel)
  {
    [[resource_table]] const Uniform &uni = this->uniforms;
    /* Repeat texture coordinates as the weight can be optimized to a small portion of the film. */
    texel = texel % imageSize(in_weight_img).xy;

    if (!uni.uniform_buf.film.use_history || use_reprojection) {
      return 0.0f;
    }
    return imageLoadFast(in_weight_img, int3(texel, FILM_WEIGHT_LAYER_DISTANCE)).x;
  }

  float weight_load(int2 texel)
  {
    [[resource_table]] const Uniform &uni = this->uniforms;
    /* Repeat texture coordinates as the weight can be optimized to a small portion of the film. */
    texel = texel % imageSize(in_weight_img).xy;

    if (!uni.uniform_buf.film.use_history || use_reprojection) {
      return 0.0f;
    }
    return imageLoadFast(in_weight_img, int3(texel, FILM_WEIGHT_LAYER_ACCUMULATION)).x;
  }

  /* Returns motion in pixel space to retrieve the pixel history. */
  float2 pixel_history_motion_vector(int2 texel_sample)
  {
    [[resource_table]] const CameraVelocity &cam_vel = this->camera;
    [[resource_table]] const Uniform &uni = this->uniforms;

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

    float4 vector = cam_vel.resolve(views_, vector_tx, nearest_texel, min_depth);

    /* Transform to pixel space. */
    vector.xy *= float2(uni.uniform_buf.film.extent);

    return vector.xy;
  }

  /* \a t is inter-pixel position. 0 means perfectly on a pixel center.
   * Returns weights in both dimensions.
   * Multiply each dimension weights to get final pixel weights. */
  void get_catmull_rom_weights(float2 t, float2 (&weights)[4])
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
  float4 sample_catmull_rom(sampler2D color_tx, float2 input_texel)
  {
    [[resource_table]] const Uniform &uni = this->uniforms;

    float2 center_texel;
    float2 inter_texel = modf(input_texel, center_texel);
    float2 weights[4];
    get_catmull_rom_weights(inter_texel, weights);

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
    float2 uv_12 = (center_texel + weights[2] / weight_12) * uni.uniform_buf.film.extent_inv;
    float2 uv_0 = (center_texel - 1.0f) * uni.uniform_buf.film.extent_inv;
    float2 uv_3 = (center_texel + 2.0f) * uni.uniform_buf.film.extent_inv;

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
  void combined_neighbor_boundbox(int2 texel, float4 &min_c, float4 &max_c)
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
      float4 color = texelfetch_as_YCoCg_opacity(combined_tx, texel + plus_offsets[i]);
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
      float4 color = texelfetch_as_YCoCg_opacity(combined_tx, texel + plus_offsets[i]);
      min_c = min(min_c, color);
      max_c = max(max_c, color);
    }
    /* (Slide 32) Simple clamp to min/max of 8 neighbors results in 3x3 box artifacts.
     * Round bbox shape by averaging 2 different min/max from 2 different neighborhood. */
    float4 min_c_3x3 = min_c;
    float4 max_c_3x3 = max_c;
    constexpr int2 corners[4] = int2_array(int2(-1, -1), int2(1, -1), int2(-1, 1), int2(1, 1));
    for (int i = 0; i < 4; i++) {
      float4 color = texelfetch_as_YCoCg_opacity(combined_tx, texel + corners[i]);
      min_c_3x3 = min(min_c_3x3, color);
      max_c_3x3 = max(max_c_3x3, color);
    }
    min_c = (min_c + min_c_3x3) * 0.5f;
    max_c = (max_c + max_c_3x3) * 0.5f;
#endif
  }

  /* 1D equivalent of line_aabb_clipping_dist(). */
  float aabb_clipping_dist_alpha(float origin, float direction, float aabb_min, float aabb_max)
  {
    if (abs(direction) < 1e-5f) {
      return 0.0f;
    }
    float nearest_plane = (direction > 0.0f) ? aabb_min : aabb_max;
    return (nearest_plane - origin) / direction;
  }

  /* Modulate the history color to avoid ghosting artifact. */
  float4 amend_combined_history(float4 min_color,
                                float4 max_color,
                                float4 color_history,
                                float4 src_color)
  {
    /* Clip instead of clamping to avoid color accumulating in the AABB corners. */
    float4 clip_dir = src_color - color_history;

    float t = line_aabb_clipping_dist(
        color_history.rgb, clip_dir.rgb, min_color.rgb, max_color.rgb);
    color_history.rgb += clip_dir.rgb * saturate(t);

    /* Clip alpha on its own to avoid interference with other channels. */
    float t_a = aabb_clipping_dist_alpha(color_history.a, clip_dir.a, min_color.a, max_color.a);
    color_history.a += clip_dir.a * saturate(t_a);

    return color_history;
  }

  float history_blend_factor(
      float velocity, float2 texel, float luma_min, float luma_max, float luma_history)
  {
    [[resource_table]] const Uniform &uni = this->uniforms;
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
        any(greaterThanEqual(texel, float2(uni.uniform_buf.film.extent))))
    {
      blend = 1.0f;
    }
    /* Discard history if invalid. */
    if (uni.uniform_buf.film.use_history == false) {
      blend = 1.0f;
    }
    return blend;
  }

  float4 clamp_negative_values(float4 color)
  {
    /* Clamp negative values caused by float imprecision to 0.0f. This also covers the case of
     * -0.0f, as (-0.0f > 0.0f) evaluates to false and therefore the whole ternary operator to
     * 0.0f. This is important for certain compositor operations that work differently depending on
     * the sign of the input. In theory, color = max(0.0f, color) could also be used for that,
     * however, the output of max(0.0f, -0.0f) depends on both the exact wording of the
     * specification of the max() function and the order of function parameters, which is why it is
     * not used. */
    for (int i = 0; i < 4; i++) [[unroll]] {
      color[i] = (color[i] > 0.0f) ? color[i] : 0.0f;
    }
    return color;
  }

  /* Returns resolved final color. */
  void store_combined(
      FilmSample dst, int2 src_texel, float4 color, float color_weight, float4 &display)
  {
    if (combined_id == -1) {
      return;
    }

    [[resource_table]] const Uniform &uni = this->uniforms;

    float4 color_src, color_dst;
    float weight_src, weight_dst;

    /* Undo the weighting to get final spatially-filtered color. */
    color_src = color / color_weight;

    if (use_reprojection) {
      /* Interactive accumulation. Do reprojection and Temporal Anti-Aliasing. */

      /* Reproject by finding where this pixel was in the previous frame. */
      float2 motion = pixel_history_motion_vector(src_texel);
      float2 history_texel = float2(dst.texel) + motion;

      float velocity = length(motion);

      /* Load weight if it is not uniform across the whole buffer (i.e: upsampling, panoramic). */
      // dst.weight = weight_load(texel_combined);

      color_dst = sample_catmull_rom(in_combined_tx, history_texel);
      color_dst.rgb = colorspace::YCoCg_from_scene_linear(color_dst.rgb);

      /* Get local color bounding box of source neighborhood. */
      float4 min_color, max_color;
      combined_neighbor_boundbox(src_texel, min_color, max_color);

      float blend = history_blend_factor(
          velocity, history_texel, min_color.x, max_color.x, color_dst.x);

      color_dst = amend_combined_history(min_color, max_color, color_dst, color_src);

      /* Luma weighted blend to avoid flickering. */
      weight_dst = luma_weight(uni, color_dst.x) * (1.0f - blend);
      weight_src = luma_weight(uni, color_src.x) * (blend);
    }
    else {
      /* Everything is static. Use render accumulation. */
      color_dst = texelFetch(in_combined_tx, dst.texel, 0);
      color_dst.rgb = colorspace::YCoCg_from_scene_linear(color_dst.rgb);

      /* Luma weighted blend to avoid flickering. */
      weight_dst = luma_weight(uni, color_dst.x) * dst.weight;
      weight_src = color_weight;
    }
    /* Weighted blend. */
    color = color_dst * weight_dst + color_src * weight_src;
    color /= weight_src + weight_dst;

    color.rgb = colorspace::scene_linear_from_YCoCg(color.rgb);

    /* Fix alpha not accumulating to 1 because of float imprecision. */
    if (color.a > 0.995f) {
      color.a = 1.0f;
    }

    /* Filter NaNs. */
    if (any(isnan(color))) {
      color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    color = clamp_negative_values(color);

    if (display_id == -1) {
      display = color;
    }
    color = patch_float_for_16f_storage(color);
    imageStoreFast(out_combined_img, dst.texel, color);
  }

  void store_color_ex(
      FilmSample dst, int pass_id, float4 color, float4 &display, bool do_clamp_negative_values)
  {
    /* Filter NaNs. */
    if (any(isnan(color))) {
      color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    if (do_clamp_negative_values) {
      color = clamp_negative_values(color);
    }

    /* Fix alpha not accumulating to 1 because of float imprecision. But here we cannot assume that
     * the alpha contains actual transparency and not user data. Only bias if very close to 1. */
    if (color.a > 0.9999f && color.a < 1.0f) {
      color.a = 1.0f;
    }

    if (display_id == pass_id) {
      display = color;
    }
    color = patch_float_for_16f_storage(color);
    imageStoreFast(color_accum_img, int3(dst.texel, pass_id), color);
  }

  void store_color(FilmSample dst,
                   int pass_id,
                   float4 color,
                   float4 &display,
                   bool do_clamp_negative_values = true)
  {
    if (pass_id == -1) {
      return;
    }

    float4 data_film = imageLoadFast(color_accum_img, int3(dst.texel, pass_id));

    color = (data_film * dst.weight + color) * dst.weight_sum_inv;

    store_color_ex(dst, pass_id, color, display, do_clamp_negative_values);
  }

  void store_color_and_light(FilmSample dst,
                             int color_pass_id,
                             int light_pass_id,
                             float4 color,
                             float4 light,
                             float4 &display)
  {
    if (color_pass_id == -1) {
      return;
    }

    float4 color_film = imageLoadFast(color_accum_img, int3(dst.texel, color_pass_id));
    color = (color_film * dst.weight + color) * dst.weight_sum_inv;
    store_color_ex(dst, color_pass_id, color, display, true);

    if (light_pass_id == -1) {
      return;
    }

    float4 light_film = imageLoadFast(color_accum_img, int3(dst.texel, light_pass_id));
    /* Undivide. */
    light_film *= color_film;
    light = (light_film * dst.weight + light) * dst.weight_sum_inv;
    light = safe_divide_even_color(light, color);
    store_color_ex(dst, light_pass_id, light, display, true);
  }

  void store_value(FilmSample dst, int pass_id, float value, float4 &display)
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
    value = patch_float_for_16f_storage(value);
    imageStoreFast(value_accum_img, int3(dst.texel, pass_id), float4(value));
  }

  /* Nearest sample variant. Always stores the data. */
  void store_data(int2 texel_film, int pass_id, float4 data_sample, float4 &display)
  {
    if (pass_id == -1) {
      return;
    }

    if (display_id == pass_id) {
      display = data_sample;
    }
    imageStoreFast(color_accum_img, int3(texel_film, pass_id), data_sample);
  }

  void store_depth(int2 texel_film, float value, float &out_depth)
  {
    [[resource_table]] const Uniform &uni = this->uniforms;
    [[resource_table]] const draw::View &views = this->views_;

    if (uni.uniform_buf.film.depth_id == -1) {
      return;
    }

    float depth_value = depth_convert_to_scene(views.get(0), value);
    out_depth = depth_value;

    if (value == 1.0f) {
      /* Match clear value in render_layer_allocate_pass. */
      depth_value = 1e10f;
    }

    imageStoreFast(depth_img, texel_film, float4(depth_value));
  }

  void store_distance(int2 texel, float value)
  {
    imageStoreFast(out_weight_img, int3(texel, FILM_WEIGHT_LAYER_DISTANCE), float4(value));
  }

  void store_weight(int2 texel, float value)
  {
    imageStoreFast(out_weight_img, int3(texel, FILM_WEIGHT_LAYER_ACCUMULATION), float4(value));
  }

  /** \} */

  /** NOTE: out_depth is scene linear depth from the camera origin. */
  void process_render_sample(int2 texel_film, float4 &out_color, float &out_depth)
  {
    [[resource_table]] const Uniform &uni = this->uniforms;
    out_color = float4(0.0f);
    out_depth = 0.0f;

    float weight_accum = weight_accumulation(texel_film);
    float film_weight = weight_load(texel_film);
    float weight_sum = film_weight + weight_accum;
    store_weight(texel_film, weight_sum);

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
        src = sample_get(i, texel_film);
        sample_accum_combined(src, combined_accum, weight_accum);
      }
      /* NOTE: src.texel is center texel in incoming data buffer. */
      store_combined(dst, src.texel, combined_accum, weight_accum, out_color);
    }

    if (flag_test(enabled_categories, PASS_CATEGORY_DATA)) {
      float film_distance = distance_load(texel_film);

      /* Get sample closest to target texel. It is always sample 0. */
      FilmSample film_sample = sample_get(0, texel_film);

      /* Using film weight as distance to the pixel. So the check is inverted. */
      if (film_sample.weight > film_distance) {
        [[resource_table]] const CameraVelocity &cam_vel = this->camera;

        float depth = reverse_z::read(texelFetch(depth_tx, film_sample.texel, 0).x);
        float4 vector = cam_vel.resolve(views_, vector_tx, film_sample.texel, depth);
        /* Transform to pixel space, matching Cycles format. */
        vector *= float4(float2(uni.uniform_buf.film.render_extent),
                         float2(uni.uniform_buf.film.render_extent));

        store_depth(texel_film, depth, out_depth);
        if (normal_id != -1) {
          float4 normal = texelFetch(
              rp_color_tx, int3(film_sample.texel, uni.uniform_buf.render_pass.normal_id), 0);
          store_data(texel_film, normal_id, normal, out_color);
        }
        if (uni.uniform_buf.film.position_id != -1) {
          float4 position = texelFetch(
              rp_color_tx, int3(film_sample.texel, uni.uniform_buf.render_pass.position_id), 0);
          store_data(texel_film, uni.uniform_buf.film.position_id, position, out_color);
        }
        store_data(texel_film, uni.uniform_buf.film.vector_id, vector, out_color);
        store_distance(texel_film, film_sample.weight);
      }
      else {
        out_depth = imageLoadFast(depth_img, texel_film).r;
        if (display_id == -1) {
          /* NOP. */
        }
        else if (display_id == normal_id) {
          out_color = imageLoadFast(color_accum_img, int3(texel_film, display_id));
        }
        else if (display_id == uni.uniform_buf.film.position_id) {
          out_color = imageLoadFast(color_accum_img,
                                    int3(texel_film, uni.uniform_buf.film.position_id));
        }
      }
    }

    if (flag_test(enabled_categories, PASS_CATEGORY_COLOR_1)) {
      float4 diffuse_color_accum = float4(0.0f);
      float4 specular_color_accum = float4(0.0f);
      float4 diffuse_light_accum = float4(0.0f);
      float4 specular_light_accum = float4(0.0f);

      for (int i = 0; i < samples_len; i++) {
        FilmSample src = sample_get(i, texel_film);
        sample_accum(src,
                     uni.uniform_buf.film.diffuse_color_id,
                     uni.uniform_buf.render_pass.diffuse_color_id,
                     rp_color_tx,
                     diffuse_color_accum);
        sample_accum(src,
                     uni.uniform_buf.film.specular_color_id,
                     uni.uniform_buf.render_pass.specular_color_id,
                     rp_color_tx,
                     specular_color_accum);
        sample_accum(src,
                     uni.uniform_buf.film.diffuse_light_id,
                     uni.uniform_buf.render_pass.diffuse_light_id,
                     rp_color_tx,
                     diffuse_light_accum);
        sample_accum(src,
                     uni.uniform_buf.film.specular_light_id,
                     uni.uniform_buf.render_pass.specular_light_id,
                     rp_color_tx,
                     specular_light_accum);
      }

      store_color_and_light(dst,
                            uni.uniform_buf.film.diffuse_color_id,
                            uni.uniform_buf.film.diffuse_light_id,
                            diffuse_color_accum,
                            diffuse_light_accum,
                            out_color);
      store_color_and_light(dst,
                            uni.uniform_buf.film.specular_color_id,
                            uni.uniform_buf.film.specular_light_id,
                            specular_color_accum,
                            specular_light_accum,
                            out_color);
    }

    if (flag_test(enabled_categories, PASS_CATEGORY_COLOR_2)) {
      float4 environment_accum = float4(0.0f);
      float4 volume_light_accum = float4(0.0f);
      float4 emission_accum = float4(0.0f);
      float mist_accum = 0.0f;
      float shadow_accum = 0.0f;
      float ao_accum = 0.0f;

      for (int i = 0; i < samples_len; i++) {
        FilmSample src = sample_get(i, texel_film);
        sample_accum(src,
                     uni.uniform_buf.film.volume_light_id,
                     uni.uniform_buf.render_pass.volume_light_id,
                     rp_color_tx,
                     volume_light_accum);
        sample_accum(src,
                     uni.uniform_buf.film.emission_id,
                     uni.uniform_buf.render_pass.emission_id,
                     rp_color_tx,
                     emission_accum);
        sample_accum(src,
                     uni.uniform_buf.film.environment_id,
                     uni.uniform_buf.render_pass.environment_id,
                     rp_color_tx,
                     environment_accum);
        sample_accum(src,
                     uni.uniform_buf.film.shadow_id,
                     uni.uniform_buf.render_pass.shadow_id,
                     rp_value_tx,
                     shadow_accum);
        sample_accum(src,
                     uni.uniform_buf.film.ambient_occlusion_id,
                     uni.uniform_buf.render_pass.ambient_occlusion_id,
                     rp_value_tx,
                     ao_accum);
        sample_accum_mist(src, mist_accum);
      }
      /* Monochrome render passes that have colored outputs. Set alpha to 1. */
      float4 shadow_accum_color = float4(float3(shadow_accum), weight_accum);
      float4 ao_accum_color = float4(float3(ao_accum), weight_accum);

      store_color(dst, uni.uniform_buf.film.volume_light_id, volume_light_accum, out_color);
      store_color(dst, uni.uniform_buf.film.emission_id, emission_accum, out_color);
      store_color(dst, uni.uniform_buf.film.environment_id, environment_accum, out_color);
      store_color(dst, uni.uniform_buf.film.shadow_id, shadow_accum_color, out_color);
      store_color(dst, uni.uniform_buf.film.ambient_occlusion_id, ao_accum_color, out_color);
      store_value(dst, uni.uniform_buf.film.mist_id, mist_accum, out_color);
    }

    if (flag_test(enabled_categories, PASS_CATEGORY_COLOR_3)) {
      float4 transparent_accum = float4(0.0f);

      for (int i = 0; i < samples_len; i++) {
        FilmSample src = sample_get(i, texel_film);
        sample_accum(src,
                     uni.uniform_buf.film.transparent_id,
                     uni.uniform_buf.render_pass.transparent_id,
                     rp_color_tx,
                     transparent_accum);
      }
      /* Alpha stores transmittance for transparent pass. */
      transparent_accum.a = weight_accum - transparent_accum.a;

      store_color(dst, uni.uniform_buf.film.transparent_id, transparent_accum, out_color);
    }

    if (flag_test(enabled_categories, PASS_CATEGORY_AOV)) {
      for (int aov = 0; aov < uni.uniform_buf.film.aov_color_len; aov++) {
        float4 aov_accum = float4(0.0f);

        for (int i = 0; i < samples_len; i++) {
          FilmSample src = sample_get(i, texel_film);
          sample_accum(
              src, 0, uni.uniform_buf.render_pass.color_len + aov, rp_color_tx, aov_accum);
        }
        store_color(dst, uni.uniform_buf.film.aov_color_id + aov, aov_accum, out_color, false);
      }

      for (int aov = 0; aov < uni.uniform_buf.film.aov_value_len; aov++) {
        float aov_accum = 0.0f;

        for (int i = 0; i < samples_len; i++) {
          FilmSample src = sample_get(i, texel_film);
          sample_accum(
              src, 0, uni.uniform_buf.render_pass.value_len + aov, rp_value_tx, aov_accum);
        }
        store_value(dst, uni.uniform_buf.film.aov_value_id + aov, aov_accum, out_color);
      }
    }

    if (flag_test(enabled_categories, PASS_CATEGORY_CRYPTOMATTE)) {
      if (uni.uniform_buf.film.cryptomatte_samples_len != 0) {
        [[resource_table]] Cryptomatte &crypto = this->cryptomatte;

        /* Cryptomatte passes cannot be cleared by a weighted store like other passes. */
        if (!uni.uniform_buf.film.use_history || use_reprojection) {
          crypto.clear_samples(dst);
        }

        cryptomatte_layer_accum_and_store(
            dst, texel_film, uni.uniform_buf.film.cryptomatte_object_id, 0, out_color);
        cryptomatte_layer_accum_and_store(
            dst, texel_film, uni.uniform_buf.film.cryptomatte_asset_id, 1, out_color);
        cryptomatte_layer_accum_and_store(
            dst, texel_film, uni.uniform_buf.film.cryptomatte_material_id, 2, out_color);
      }
    }
  }
};

[[vertex]] void fullscreen_vert([[vertex_id]] const int vert_id, [[position]] float4 &out_position)
{
  fullscreen_vertex(vert_id, out_position);
}

struct FilmFragOut {
  [[frag_color(0)]] float4 color;
};

struct FilmDisplay {
  /* True if we bypass the accumulation and directly output the accumulation buffer. */
  [[push_constant]] bool display_only;
};

/* Accumulate and output to the render frame-buffer.
 * Used for viewport. */
[[fragment]]
void accumulate_or_display_frag([[resource_table]] const FilmDisplay &srt,
                                [[resource_table]] Film &film,
                                [[resource_table]] const Uniform &uni,
                                [[resource_table]] const draw::View &views,
                                [[out]] FilmFragOut &frag_out,
                                [[frag_coord]] const float4 frag_co,
                                [[frag_depth(any)]] float out_depth)
{
  [[resource_table]] Cryptomatte &cryptomatte = film.cryptomatte;

  int2 texel_film = int2(frag_co.xy) - uni.uniform_buf.film.offset;

  if (srt.display_only) {
    out_depth = imageLoadFast(film.depth_img, texel_film).r;

    if (film.display_id == -1) {
      frag_out.color = texelFetch(film.in_combined_tx, texel_film, 0);
    }
    else if (uni.uniform_buf.film.display_storage_type == PASS_STORAGE_VALUE) {
      frag_out.color.rgb =
          imageLoadFast(film.value_accum_img, int3(texel_film, film.display_id)).rrr;
      frag_out.color.a = 1.0f;
    }
    else if (uni.uniform_buf.film.display_storage_type == PASS_STORAGE_COLOR) {
      frag_out.color = imageLoadFast(film.color_accum_img, int3(texel_film, film.display_id));
    }
    else /* PASS_STORAGE_CRYPTOMATTE */ {
      frag_out.color = cryptomatte::false_color(
          imageLoadFast(cryptomatte.cryptomatte_img, int3(texel_film, film.display_id)).r);
    }
  }
  else {
    film.process_render_sample(texel_film, frag_out.color, out_depth);
  }

  out_depth = views.get(0).depth_view_to_screen(-out_depth);

  out_depth = display_depth_amend(out_depth);
}

/**
 * `display_frag` is used to work around iGPU issues.
 *
 * Caches are not flushed in the eevee_film_frag shader due to unsupported read/write access.
 * We schedule the eevee_film_comp shader instead. Resources are attached read only and does the
 * part that is missing from the eevee_film_frag shader.
 */
[[fragment]]
void display_frag([[resource_table]] Film &film,
                  [[out]] FilmFragOut &frag_out,
                  [[resource_table]] const Uniform &uni,
                  [[resource_table]] const draw::View &views,
                  [[frag_coord]] const float4 frag_co,
                  [[frag_depth(any)]] float out_depth)
{
  [[resource_table]] Cryptomatte &cryptomatte = film.cryptomatte;

  int2 texel = int2(frag_co.xy);

  if (film.display_id == -1) {
    frag_out.color = texelFetch(film.in_combined_tx, texel, 0);
  }
  else if (uni.uniform_buf.film.display_storage_type == PASS_STORAGE_VALUE) {
    frag_out.color.rgb = imageLoadFast(film.value_accum_img, int3(texel, film.display_id)).rrr;
    frag_out.color.a = 1.0f;
  }
  else if (uni.uniform_buf.film.display_storage_type == PASS_STORAGE_COLOR) {
    frag_out.color = imageLoadFast(film.color_accum_img, int3(texel, film.display_id));
  }
  else /* PASS_STORAGE_CRYPTOMATTE */ {
    frag_out.color = cryptomatte::false_color(
        imageLoadFast(cryptomatte.cryptomatte_img, int3(texel, film.display_id)).r);
  }

  out_depth = imageLoadFast(film.depth_img, texel).r;
  out_depth = views.get(0).depth_view_to_screen(-out_depth);
  out_depth += 2.4e-7f * 4.0f + gpu_fwidth(out_depth);
  out_depth = saturate(out_depth);
}

/* Accumulate sample result. */
[[compute, local_size(FILM_GROUP_SIZE, FILM_GROUP_SIZE)]]
void accumulate_comp([[resource_table]] Film &film,
                     [[resource_table]] const Uniform &uni,
                     [[global_invocation_id]] const uint3 global_id)
{
  int2 texel_film = int2(global_id.xy);
  /* Not used. */
  float4 out_color;
  float out_depth;

  if (any(greaterThanEqual(texel_film, uni.uniform_buf.film.extent))) {
    return;
  }

  film.process_render_sample(texel_film, out_color, out_depth);
}

/* The combined pass is stored into its own 2D texture with a format of
 * SFLOAT_16_16_16_16. */
struct ConvertCombined {
  [[push_constant]] const int2 offset;
  [[sampler(0)]] sampler2D input_tx;
  [[image(0, write, SFLOAT_16_16_16_16)]] image2D output_img;
};

/* The depth pass is stored into its own 2D texture with a format of SFLOAT_32. */
struct ConvertDepth {
  [[push_constant]] const int2 offset;
  [[sampler(0)]] sampler2DDepth input_tx;
  [[image(0, write, SFLOAT_32)]] image2D output_img;
};

/* Value passes are stored in a slice of a 2D texture array with a format of
 * SFLOAT_16. */
struct ConvertValue {
  [[push_constant]] const int2 offset;
  [[sampler(0)]] sampler2DArray input_tx;
  [[image(0, write, SFLOAT_16)]] image2D output_img;
};

/* Color passes are stored in a slice of a 2D texture array with a format of
 * SFLOAT_16_16_16_16. */
struct ConvertColor {
  [[push_constant]] const int2 offset;
  [[sampler(0)]] sampler2DArray input_tx;
  [[image(0, write, SFLOAT_16_16_16_16)]] image2D output_img;
};

/* Cryptomatte passes are stored in a slice of a 2D texture array with a format of
 * SFLOAT_32_32_32_32. */
struct ConvertCryptomatte {
  [[push_constant]] const int2 offset;
  [[sampler(0)]] sampler2DArray input_tx;
  [[image(0, write, SFLOAT_32_32_32_32)]] image2D output_img;
};

/* Used by the Viewport Compositor to copy EEVEE passes to the compositor DRW passes textures. The
 * output passes covert the entire display extent even when border rendering because that's what
 * the compositor expects, so areas outside of the border are zeroed. */
template<typename ResourceT, bool layered>
[[compute, local_size(FILM_GROUP_SIZE, FILM_GROUP_SIZE)]]
void pass_convert([[resource_table]] ResourceT &srt,
                  [[global_invocation_id]] const uint3 global_id)
{
  int2 texel = int2(global_id.xy);
  if (any(greaterThan(texel, imageSize(srt.output_img) - int2(1)))) {
    return;
  }
  /* In case of border rendering, clear areas outside of the border. The offset is the lower left
   * corner of the border. */
  int2 input_bounds = textureSize(srt.input_tx, 0).xy - int2(1);
  if (any(lessThan(texel, srt.offset)) || any(greaterThan(texel, srt.offset + input_bounds))) {
    imageStoreFast(srt.output_img, texel, float4(0.0f));
  }
  else {
    if constexpr (layered) {
      imageStoreFast(
          srt.output_img, texel, texelFetch(srt.input_tx, int3(texel - srt.offset, 0), 0));
    }
    else {
      imageStoreFast(srt.output_img, texel, texelFetch(srt.input_tx, texel - srt.offset, 0));
    }
  }
}

template void pass_convert<ConvertCombined, false>(ConvertCombined &, const uint3);
template void pass_convert<ConvertDepth, false>(ConvertDepth &, const uint3);
template void pass_convert<ConvertValue, true>(ConvertValue &, const uint3);
template void pass_convert<ConvertColor, true>(ConvertColor &, const uint3);
template void pass_convert<ConvertCryptomatte, true>(ConvertCryptomatte &, const uint3);

}  // namespace eevee::film

PipelineCompute eevee_film_comp(eevee::film::accumulate_comp);
PipelineGraphic eevee_film_frag(eevee::film::fullscreen_vert,
                                eevee::film::accumulate_or_display_frag);
PipelineGraphic eevee_film_copy_frag(eevee::film::fullscreen_vert, eevee::film::display_frag);

PipelineCompute eevee_film_pass_convert_combined(
    eevee::film::pass_convert<eevee::film::ConvertCombined, false>);
PipelineCompute eevee_film_pass_convert_depth(
    eevee::film::pass_convert<eevee::film::ConvertDepth, false>);
PipelineCompute eevee_film_pass_convert_value(
    eevee::film::pass_convert<eevee::film::ConvertValue, true>);
PipelineCompute eevee_film_pass_convert_color(
    eevee::film::pass_convert<eevee::film::ConvertColor, true>);
PipelineCompute eevee_film_pass_convert_cryptomatte(
    eevee::film::pass_convert<eevee::film::ConvertCryptomatte, true>);
