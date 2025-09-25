/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Functions to retrieving render passes for display or output. Reading from
 * the raw render buffer and normalizing based on the number of samples,
 * computing alpha, compositing shadow catchers, etc. */

#pragma once

#include "kernel/types.h"

#include "util/color.h"

CCL_NAMESPACE_BEGIN

/* --------------------------------------------------------------------
 * Common utilities.
 */

/* The input buffer contains transparency = 1 - alpha, this converts it to
 * alpha. Also clamp since alpha might end up outside of 0..1 due to Russian
 * roulette. */
ccl_device_forceinline float film_transparency_to_alpha(const float transparency)
{
  return saturatef(1.0f - transparency);
}

ccl_device_inline float film_get_scale(const ccl_global KernelFilmConvert *ccl_restrict
                                           kfilm_convert,
                                       const ccl_global float *ccl_restrict buffer)
{
  if (kfilm_convert->pass_sample_count == PASS_UNUSED) {
    return kfilm_convert->scale;
  }

  if (kfilm_convert->pass_use_filter) {
    const uint sample_count = *(
        (const ccl_global uint *)(buffer + kfilm_convert->pass_sample_count));
    return kfilm_convert->scale / sample_count;
  }

  return kfilm_convert->scale;
}

ccl_device_inline float film_get_scale_exposure(const ccl_global KernelFilmConvert *ccl_restrict
                                                    kfilm_convert,
                                                const ccl_global float *ccl_restrict buffer)
{
  if (kfilm_convert->pass_sample_count == PASS_UNUSED) {
    return kfilm_convert->scale_exposure;
  }

  const float scale = film_get_scale(kfilm_convert, buffer);

  if (kfilm_convert->pass_use_exposure) {
    return scale * kfilm_convert->exposure;
  }

  return scale;
}

ccl_device_inline bool film_get_scale_and_scale_exposure(
    const ccl_global KernelFilmConvert *ccl_restrict kfilm_convert,
    const ccl_global float *ccl_restrict buffer,
    ccl_private float *ccl_restrict scale,
    ccl_private float *ccl_restrict scale_exposure)
{
  if (kfilm_convert->pass_sample_count == PASS_UNUSED) {
    *scale = kfilm_convert->scale;
    *scale_exposure = kfilm_convert->scale_exposure;
    return true;
  }

  const uint sample_count = *(
      (const ccl_global uint *)(buffer + kfilm_convert->pass_sample_count));
  if (!sample_count) {
    *scale = 0.0f;
    *scale_exposure = 0.0f;
    return false;
  }

  if (kfilm_convert->pass_use_filter) {
    *scale = kfilm_convert->scale / sample_count;
  }
  else {
    *scale = kfilm_convert->scale;
  }

  if (kfilm_convert->pass_use_exposure) {
    *scale_exposure = *scale * kfilm_convert->exposure;
  }
  else {
    *scale_exposure = *scale;
  }

  return true;
}

/* --------------------------------------------------------------------
 * Float (scalar) passes.
 */

ccl_device_inline void film_get_pass_pixel_depth(const ccl_global KernelFilmConvert *ccl_restrict
                                                     kfilm_convert,
                                                 const ccl_global float *ccl_restrict buffer,
                                                 ccl_private float *ccl_restrict pixel)
{
  kernel_assert(kfilm_convert->num_components >= 1);
  kernel_assert(kfilm_convert->pass_offset != PASS_UNUSED);

  const float scale_exposure = film_get_scale_exposure(kfilm_convert, buffer);

  const ccl_global float *in = buffer + kfilm_convert->pass_offset;
  const float f = *in;

  pixel[0] = (f == 0.0f) ? 1e10f : f * scale_exposure;
}

ccl_device_inline void film_get_pass_pixel_mist(const ccl_global KernelFilmConvert *ccl_restrict
                                                    kfilm_convert,
                                                const ccl_global float *ccl_restrict buffer,
                                                ccl_private float *ccl_restrict pixel)
{
  kernel_assert(kfilm_convert->num_components >= 1);
  kernel_assert(kfilm_convert->pass_offset != PASS_UNUSED);

  const float scale_exposure = film_get_scale_exposure(kfilm_convert, buffer);

  const ccl_global float *in = buffer + kfilm_convert->pass_offset;
  const float f = *in;

  /* Note that we accumulate 1 - mist in the kernel to avoid having to
   * track the mist values in the integrator state. */
  pixel[0] = saturatef(1.0f - f * scale_exposure);
}

ccl_device_inline void film_get_pass_pixel_sample_count(
    const ccl_global KernelFilmConvert *ccl_restrict kfilm_convert,
    const ccl_global float *ccl_restrict buffer,
    ccl_private float *ccl_restrict pixel)
{
  /* TODO(sergey): Consider normalizing into the [0..1] range, so that it is possible to see
   * meaningful value when adaptive sampler stopped rendering image way before the maximum
   * number of samples was reached (for examples when number of samples is set to 0 in
   * viewport). */

  kernel_assert(kfilm_convert->num_components >= 1);
  kernel_assert(kfilm_convert->pass_offset != PASS_UNUSED);

  const ccl_global float *in = buffer + kfilm_convert->pass_offset;
  const float f = *in;

  pixel[0] = __float_as_uint(f) * kfilm_convert->scale;
}

ccl_device_inline void film_get_pass_pixel_volume_majorant(
    const ccl_global KernelFilmConvert *ccl_restrict kfilm_convert,
    const ccl_global float *ccl_restrict buffer,
    ccl_private float *ccl_restrict pixel)
{
  kernel_assert(kfilm_convert->num_components >= 1);
  kernel_assert(kfilm_convert->pass_offset != PASS_UNUSED);

  const float scale_exposure = film_get_scale_exposure(kfilm_convert, buffer);

  const ccl_global float *in = buffer + kfilm_convert->pass_offset;
  const ccl_global float *count = buffer + kfilm_convert->pass_divide;
  const float f = *in;

  pixel[0] = (*count != 0.0f) ? expf(-(f * scale_exposure) / *count) : 0.0f;
}

ccl_device_inline void film_get_pass_pixel_rgbe(const ccl_global KernelFilmConvert *ccl_restrict
                                                    kfilm_convert,
                                                const ccl_global float *ccl_restrict buffer,
                                                ccl_private float *ccl_restrict pixel)
{
  kernel_assert(kfilm_convert->num_components >= 1);
  kernel_assert(kfilm_convert->pass_offset != PASS_UNUSED);

  const ccl_global float *in = buffer + kfilm_convert->pass_offset;
  const float3 f = rgbe_to_rgb(RGBE(*in));

  pixel[0] = f.x;
  pixel[1] = f.y;
  pixel[2] = f.z;
}

ccl_device_inline void film_get_pass_pixel_float(const ccl_global KernelFilmConvert *ccl_restrict
                                                     kfilm_convert,
                                                 const ccl_global float *ccl_restrict buffer,
                                                 ccl_private float *ccl_restrict pixel)
{
  kernel_assert(kfilm_convert->num_components >= 1);
  kernel_assert(kfilm_convert->pass_offset != PASS_UNUSED);

  const float scale_exposure = film_get_scale_exposure(kfilm_convert, buffer);

  const ccl_global float *in = buffer + kfilm_convert->pass_offset;
  const float f = *in;

  pixel[0] = f * scale_exposure;
}

/* --------------------------------------------------------------------
 * Float 3 passes.
 */

ccl_device_inline void film_get_pass_pixel_light_path(
    const ccl_global KernelFilmConvert *ccl_restrict kfilm_convert,
    const ccl_global float *ccl_restrict buffer,
    ccl_private float *ccl_restrict pixel)
{
  kernel_assert(kfilm_convert->num_components >= 3);
  kernel_assert(kfilm_convert->pass_offset != PASS_UNUSED);

  /* Read light pass. */
  const ccl_global float *in = buffer + kfilm_convert->pass_offset;
  float3 f = make_float3(in[0], in[1], in[2]);

  /* Optionally add indirect light pass. */
  if (kfilm_convert->pass_indirect != PASS_UNUSED) {
    const ccl_global float *in_indirect = buffer + kfilm_convert->pass_indirect;
    const float3 f_indirect = make_float3(in_indirect[0], in_indirect[1], in_indirect[2]);
    f += f_indirect;
  }

  /* Optionally divide out color. */
  if (kfilm_convert->pass_divide != PASS_UNUSED) {
    const ccl_global float *in_divide = buffer + kfilm_convert->pass_divide;
    const float3 f_divide = make_float3(in_divide[0], in_divide[1], in_divide[2]);
    f = safe_divide_even_color(f, f_divide);

    /* Exposure only, sample scale cancels out. */
    f *= kfilm_convert->exposure;
  }
  else {
    /* Sample scale and exposure. */
    f *= film_get_scale_exposure(kfilm_convert, buffer);
  }

  pixel[0] = f.x;
  pixel[1] = f.y;
  pixel[2] = f.z;

  /* Optional alpha channel. */
  if (kfilm_convert->num_components >= 4) {
    if (kfilm_convert->pass_combined != PASS_UNUSED) {
      float scale;
      float scale_exposure;
      film_get_scale_and_scale_exposure(kfilm_convert, buffer, &scale, &scale_exposure);

      const ccl_global float *in_combined = buffer + kfilm_convert->pass_combined;
      const float alpha = in_combined[3] * scale;
      pixel[3] = film_transparency_to_alpha(alpha);
    }
    else {
      pixel[3] = 1.0f;
    }
  }
}

ccl_device_inline void film_get_pass_pixel_float3(const ccl_global KernelFilmConvert *ccl_restrict
                                                      kfilm_convert,
                                                  const ccl_global float *ccl_restrict buffer,
                                                  ccl_private float *ccl_restrict pixel)
{
  kernel_assert(kfilm_convert->num_components >= 3);
  kernel_assert(kfilm_convert->pass_offset != PASS_UNUSED);

  const float scale_exposure = film_get_scale_exposure(kfilm_convert, buffer);

  const ccl_global float *in = buffer + kfilm_convert->pass_offset;

  const float3 f = make_float3(in[0], in[1], in[2]) * scale_exposure;

  pixel[0] = f.x;
  pixel[1] = f.y;
  pixel[2] = f.z;

  /* Optional alpha channel. */
  if (kfilm_convert->num_components >= 4) {
    if (kfilm_convert->pass_combined != PASS_UNUSED) {
      float scale;
      float scale_exposure;
      film_get_scale_and_scale_exposure(kfilm_convert, buffer, &scale, &scale_exposure);

      const ccl_global float *in_combined = buffer + kfilm_convert->pass_combined;
      const float alpha = in_combined[3] * scale;
      pixel[3] = film_transparency_to_alpha(alpha);
    }
    else {
      pixel[3] = 1.0f;
    }
  }
}

/* --------------------------------------------------------------------
 * Float4 passes.
 */

ccl_device_inline void film_get_pass_pixel_motion(const ccl_global KernelFilmConvert *ccl_restrict
                                                      kfilm_convert,
                                                  const ccl_global float *ccl_restrict buffer,
                                                  ccl_private float *ccl_restrict pixel)
{
  kernel_assert(kfilm_convert->num_components == 4);
  kernel_assert(kfilm_convert->pass_offset != PASS_UNUSED);
  kernel_assert(kfilm_convert->pass_motion_weight != PASS_UNUSED);

  const ccl_global float *in = buffer + kfilm_convert->pass_offset;
  const ccl_global float *in_weight = buffer + kfilm_convert->pass_motion_weight;

  const float weight = in_weight[0];
  const float weight_inv = (weight > 0.0f) ? 1.0f / weight : 0.0f;

  const float4 motion = make_float4(in[0], in[1], in[2], in[3]) * weight_inv;

  pixel[0] = motion.x;
  pixel[1] = motion.y;
  pixel[2] = motion.z;
  pixel[3] = motion.w;
}

ccl_device_inline void film_get_pass_pixel_cryptomatte(
    const ccl_global KernelFilmConvert *ccl_restrict kfilm_convert,
    const ccl_global float *ccl_restrict buffer,
    ccl_private float *ccl_restrict pixel)
{
  kernel_assert(kfilm_convert->num_components == 4);
  kernel_assert(kfilm_convert->pass_offset != PASS_UNUSED);

  const float scale = film_get_scale(kfilm_convert, buffer);

  const ccl_global float *in = buffer + kfilm_convert->pass_offset;

  const float4 f = make_float4(in[0], in[1], in[2], in[3]);

  /* x and z contain integer IDs, don't rescale them.
   * y and w contain matte weights, they get scaled. */
  pixel[0] = f.x;
  pixel[1] = f.y * scale;
  pixel[2] = f.z;
  pixel[3] = f.w * scale;
}

ccl_device_inline void film_get_pass_pixel_float4(const ccl_global KernelFilmConvert *ccl_restrict
                                                      kfilm_convert,
                                                  const ccl_global float *ccl_restrict buffer,
                                                  ccl_private float *ccl_restrict pixel)
{
  kernel_assert(kfilm_convert->num_components == 4);
  kernel_assert(kfilm_convert->pass_offset != PASS_UNUSED);

  float scale;
  float scale_exposure;
  film_get_scale_and_scale_exposure(kfilm_convert, buffer, &scale, &scale_exposure);

  const ccl_global float *in = buffer + kfilm_convert->pass_offset;

  const float3 color = make_float3(in[0], in[1], in[2]) * scale_exposure;
  const float alpha = in[3] * scale;

  pixel[0] = color.x;
  pixel[1] = color.y;
  pixel[2] = color.z;
  pixel[3] = alpha;
}

ccl_device_inline void film_get_pass_pixel_combined(
    const ccl_global KernelFilmConvert *ccl_restrict kfilm_convert,
    const ccl_global float *ccl_restrict buffer,
    ccl_private float *ccl_restrict pixel)
{
  kernel_assert(kfilm_convert->num_components == 4);

  /* 3rd channel contains transparency = 1 - alpha for the combined pass. */

  kernel_assert(kfilm_convert->num_components == 4);
  kernel_assert(kfilm_convert->pass_offset != PASS_UNUSED);

  float scale;
  float scale_exposure;
  if (!film_get_scale_and_scale_exposure(kfilm_convert, buffer, &scale, &scale_exposure)) {
    pixel[0] = 0.0f;
    pixel[1] = 0.0f;
    pixel[2] = 0.0f;
    pixel[3] = 0.0f;
    return;
  }

  const ccl_global float *in = buffer + kfilm_convert->pass_offset;

  const float3 color = make_float3(in[0], in[1], in[2]) * scale_exposure;
  const float alpha = in[3] * scale;

  pixel[0] = color.x;
  pixel[1] = color.y;
  pixel[2] = color.z;
  pixel[3] = film_transparency_to_alpha(alpha);
}

/* --------------------------------------------------------------------
 * Shadow catcher.
 */

ccl_device_inline float3 film_calculate_shadow_catcher_denoised(
    const ccl_global KernelFilmConvert *ccl_restrict kfilm_convert,
    const ccl_global float *ccl_restrict buffer)
{
  kernel_assert(kfilm_convert->pass_shadow_catcher != PASS_UNUSED);

  float scale;
  float scale_exposure;
  film_get_scale_and_scale_exposure(kfilm_convert, buffer, &scale, &scale_exposure);

  const ccl_global float *in_catcher = buffer + kfilm_convert->pass_shadow_catcher;

  const float3 pixel = make_float3(in_catcher[0], in_catcher[1], in_catcher[2]) * scale_exposure;

  return pixel;
}

ccl_device_inline float3 safe_divide_shadow_catcher(const float3 a, const float3 b)
{
  float x;
  float y;
  float z;

  x = (b.x != 0.0f) ? a.x / b.x : 1.0f;
  y = (b.y != 0.0f) ? a.y / b.y : 1.0f;
  z = (b.z != 0.0f) ? a.z / b.z : 1.0f;

  return make_float3(x, y, z);
}

ccl_device_inline float3
film_calculate_shadow_catcher(const ccl_global KernelFilmConvert *ccl_restrict kfilm_convert,
                              const ccl_global float *ccl_restrict buffer)
{
  /* For the shadow catcher pass we divide combined pass by the shadow catcher.
   * Note that denoised shadow catcher pass contains value which only needs ot be scaled (but not
   * to be calculated as division). */

  if (kfilm_convert->is_denoised) {
    return film_calculate_shadow_catcher_denoised(kfilm_convert, buffer);
  }

  kernel_assert(kfilm_convert->pass_shadow_catcher_sample_count != PASS_UNUSED);

  /* If there is no shadow catcher object in this pixel, there is no modification of the light
   * needed, so return one. */
  const ccl_global float *in_catcher_sample_count =
      buffer + kfilm_convert->pass_shadow_catcher_sample_count;
  const float num_samples = in_catcher_sample_count[0];
  if (num_samples == 0.0f) {
    return one_float3();
  }

  kernel_assert(kfilm_convert->pass_shadow_catcher != PASS_UNUSED);
  const ccl_global float *in_catcher = buffer + kfilm_convert->pass_shadow_catcher;

  /* NOTE: It is possible that the Shadow Catcher pass is requested as an output without actual
   * shadow catcher objects in the scene. In this case there will be no auxiliary passes required
   * for the decision (to save up memory). So delay the asserts to this point so that the number of
   * samples check handles such configuration. */
  kernel_assert(kfilm_convert->pass_offset != PASS_UNUSED);
  kernel_assert(kfilm_convert->pass_combined != PASS_UNUSED);
  kernel_assert(kfilm_convert->pass_shadow_catcher_matte != PASS_UNUSED);

  const ccl_global float *in_combined = buffer + kfilm_convert->pass_combined;
  const ccl_global float *in_matte = buffer + kfilm_convert->pass_shadow_catcher_matte;

  /* No scaling needed. The integration works in way that number of samples in the combined and
   * shadow catcher passes are the same, and exposure is canceled during the division. */
  const float3 color_catcher = make_float3(in_catcher[0], in_catcher[1], in_catcher[2]);
  const float3 color_combined = make_float3(in_combined[0], in_combined[1], in_combined[2]);
  const float3 color_matte = make_float3(in_matte[0], in_matte[1], in_matte[2]);

  /* Need to ignore contribution of the matte object when doing division (otherwise there will be
   * artifacts caused by anti-aliasing). Since combined pass is used for adaptive sampling and need
   * to contain matte objects, we subtract matte objects contribution here. This is the same as if
   * the matte objects were not accumulated to the combined pass. */
  const float3 combined_no_matte = color_combined - color_matte;

  const float3 shadow_catcher = safe_divide_shadow_catcher(combined_no_matte, color_catcher);

  const float scale = film_get_scale(kfilm_convert, buffer);
  const float transparency = in_combined[3] * scale;
  const float alpha = film_transparency_to_alpha(transparency);

  /* Alpha-over on white using transparency of the combined pass. This allows to eliminate
   * artifacts which are happening on an edge of a shadow catcher when using transparent film.
   * Note that we treat shadow catcher as straight alpha here because alpha got canceled out
   * during the division. */
  const float3 pixel = (1.0f - alpha) * one_float3() + alpha * shadow_catcher;

  return pixel;
}

ccl_device_inline float4 film_calculate_shadow_catcher_matte_with_shadow(
    const ccl_global KernelFilmConvert *ccl_restrict kfilm_convert,
    const ccl_global float *ccl_restrict buffer)
{
  /* The approximation of the shadow is 1 - average(shadow_catcher_pass). A better approximation
   * is possible.
   *
   * The matte is alpha-overed onto the shadow (which is kind of alpha-overing shadow onto footage,
   * and then alpha-overing synthetic objects on top). */

  kernel_assert(kfilm_convert->pass_offset != PASS_UNUSED);
  kernel_assert(kfilm_convert->pass_shadow_catcher != PASS_UNUSED);
  kernel_assert(kfilm_convert->pass_shadow_catcher_matte != PASS_UNUSED);

  float scale;
  float scale_exposure;
  if (!film_get_scale_and_scale_exposure(kfilm_convert, buffer, &scale, &scale_exposure)) {
    return zero_float4();
  }

  const ccl_global float *in_matte = buffer + kfilm_convert->pass_shadow_catcher_matte;

  const float3 shadow_catcher = film_calculate_shadow_catcher(kfilm_convert, buffer);
  const float3 color_matte = make_float3(in_matte[0], in_matte[1], in_matte[2]) * scale_exposure;

  const float transparency = in_matte[3] * scale;
  const float alpha = saturatef(1.0f - transparency);

  const float alpha_matte = (1.0f - alpha) * (1.0f - saturatef(average(shadow_catcher))) + alpha;

  if (kfilm_convert->use_approximate_shadow_catcher_background) {
    kernel_assert(kfilm_convert->pass_background != PASS_UNUSED);

    const ccl_global float *in_background = buffer + kfilm_convert->pass_background;
    const float3 color_background = make_float3(
                                        in_background[0], in_background[1], in_background[2]) *
                                    scale_exposure;
    const float3 alpha_over = color_matte + color_background * (1.0f - alpha_matte);
    return make_float4(alpha_over, 1.0f);
  }

  return make_float4(color_matte, alpha_matte);
}

ccl_device_inline void film_get_pass_pixel_shadow_catcher(
    const ccl_global KernelFilmConvert *ccl_restrict kfilm_convert,
    const ccl_global float *ccl_restrict buffer,
    ccl_private float *ccl_restrict pixel)
{
  kernel_assert(kfilm_convert->num_components >= 3);

  const float3 pixel_value = film_calculate_shadow_catcher(kfilm_convert, buffer);

  pixel[0] = pixel_value.x;
  pixel[1] = pixel_value.y;
  pixel[2] = pixel_value.z;
}

ccl_device_inline void film_get_pass_pixel_shadow_catcher_matte_with_shadow(
    const ccl_global KernelFilmConvert *ccl_restrict kfilm_convert,
    const ccl_global float *ccl_restrict buffer,
    ccl_private float *ccl_restrict pixel)
{
  kernel_assert(kfilm_convert->num_components == 3 || kfilm_convert->num_components == 4);

  const float4 pixel_value = film_calculate_shadow_catcher_matte_with_shadow(kfilm_convert,
                                                                             buffer);

  pixel[0] = pixel_value.x;
  pixel[1] = pixel_value.y;
  pixel[2] = pixel_value.z;
  if (kfilm_convert->num_components == 4) {
    pixel[3] = pixel_value.w;
  }
}

/* --------------------------------------------------------------------
 * Compositing and overlays.
 */

ccl_device_inline void film_apply_pass_pixel_overlays_rgba(
    const ccl_global KernelFilmConvert *ccl_restrict kfilm_convert,
    const ccl_global float *ccl_restrict buffer,
    ccl_private float *ccl_restrict pixel)
{
  if (kfilm_convert->show_active_pixels && kfilm_convert->pass_adaptive_aux_buffer != PASS_UNUSED)
  {
    if (buffer[kfilm_convert->pass_adaptive_aux_buffer + 3] == 0.0f) {
      const float3 active_rgb = make_float3(1.0f, 0.0f, 0.0f);
      const float3 mix_rgb = interp(make_float3(pixel[0], pixel[1], pixel[2]), active_rgb, 0.5f);
      pixel[0] = mix_rgb.x;
      pixel[1] = mix_rgb.y;
      pixel[2] = mix_rgb.z;
    }
  }
}

CCL_NAMESPACE_END
