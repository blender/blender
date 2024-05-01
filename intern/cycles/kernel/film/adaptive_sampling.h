/* SPDX-FileCopyrightText: 2019-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/film/write.h"

CCL_NAMESPACE_BEGIN

/* Check whether the pixel has converged and should not be sampled anymore. */

ccl_device_forceinline bool film_need_sample_pixel(KernelGlobals kg,
                                                   ConstIntegratorState state,
                                                   ccl_global float *render_buffer)
{
  if (kernel_data.film.pass_adaptive_aux_buffer == PASS_UNUSED) {
    return true;
  }

  ccl_global float *buffer = film_pass_pixel_render_buffer(kg, state, render_buffer);

  const uint aux_w_offset = kernel_data.film.pass_adaptive_aux_buffer + 3;
  return buffer[aux_w_offset] == 0.0f;
}

/* Determines whether to continue sampling a given pixel or if it has sufficiently converged. */

ccl_device bool film_adaptive_sampling_convergence_check(KernelGlobals kg,
                                                         ccl_global float *render_buffer,
                                                         int x,
                                                         int y,
                                                         float threshold,
                                                         int reset,
                                                         int offset,
                                                         int stride)
{
  kernel_assert(kernel_data.film.pass_adaptive_aux_buffer != PASS_UNUSED);
  kernel_assert(kernel_data.film.pass_sample_count != PASS_UNUSED);

  const int render_pixel_index = offset + x + y * stride;
  ccl_global float *buffer = render_buffer +
                             (uint64_t)render_pixel_index * kernel_data.film.pass_stride;

  /* TODO(Stefan): Is this better in linear, sRGB or something else? */

  const float4 A = kernel_read_pass_float4(buffer + kernel_data.film.pass_adaptive_aux_buffer);
  if (!reset && A.w != 0.0f) {
    /* If the pixel was considered converged, its state will not change in this kernel. Early
     * output before doing any math.
     *
     * TODO(sergey): On a GPU it might be better to keep thread alive for better coherency? */
    return true;
  }

  const float4 I = kernel_read_pass_float4(buffer + kernel_data.film.pass_combined);

  const float sample = __float_as_uint(buffer[kernel_data.film.pass_sample_count]);
  const float intensity_scale = kernel_data.film.exposure / sample;

  /* The per pixel error as seen in section 2.1 of
   * "A hierarchical automatic stopping condition for Monte Carlo global illumination" */
  const float error_difference = (fabsf(I.x - A.x) + fabsf(I.y - A.y) + fabsf(I.z - A.z)) *
                                 intensity_scale;
  const float intensity = (I.x + I.y + I.z) * intensity_scale;

  /* Anything with R+G+B > 1 is highly exposed - even in sRGB it's a range that
   * some displays aren't even able to display without significant losses in
   * detalization. Everything with R+G+B > 3 is overexposed and should receive
   * even less samples. Filmic-like curves need maximum sampling rate at
   * intensity near 0.1-0.2, so threshold of 1 for R+G+B leaves an additional
   * fstop in case it is needed for compositing.
   */
  float error_normalize;
  if (intensity < 1.0f) {
    error_normalize = sqrtf(intensity);
  }
  else {
    error_normalize = intensity;
  }

  /* A small epsilon is added to the divisor to prevent division by zero. */
  const float error = error_difference / (0.0001f + error_normalize);
  const bool did_converge = (error < threshold);

  const uint aux_w_offset = kernel_data.film.pass_adaptive_aux_buffer + 3;
  buffer[aux_w_offset] = did_converge;

  return did_converge;
}

/* This is a simple box filter in two passes.
 * When a pixel demands more adaptive samples, let its neighboring pixels draw more samples too. */

ccl_device void film_adaptive_sampling_filter_x(KernelGlobals kg,
                                                ccl_global float *render_buffer,
                                                int y,
                                                int start_x,
                                                int width,
                                                int offset,
                                                int stride)
{
  kernel_assert(kernel_data.film.pass_adaptive_aux_buffer != PASS_UNUSED);

  bool prev = false;
  for (int x = start_x; x < start_x + width; ++x) {
    int index = offset + x + y * stride;
    ccl_global float *buffer = render_buffer + (uint64_t)index * kernel_data.film.pass_stride;
    const uint aux_w_offset = kernel_data.film.pass_adaptive_aux_buffer + 3;

    if (buffer[aux_w_offset] == 0.0f) {
      if (x > start_x && !prev) {
        index = index - 1;
        buffer = render_buffer + (uint64_t)index * kernel_data.film.pass_stride;
        buffer[aux_w_offset] = 0.0f;
      }
      prev = true;
    }
    else {
      if (prev) {
        buffer[aux_w_offset] = 0.0f;
      }
      prev = false;
    }
  }
}

ccl_device void film_adaptive_sampling_filter_y(KernelGlobals kg,
                                                ccl_global float *render_buffer,
                                                int x,
                                                int start_y,
                                                int height,
                                                int offset,
                                                int stride)
{
  kernel_assert(kernel_data.film.pass_adaptive_aux_buffer != PASS_UNUSED);

  bool prev = false;
  for (int y = start_y; y < start_y + height; ++y) {
    int index = offset + x + y * stride;
    ccl_global float *buffer = render_buffer + (uint64_t)index * kernel_data.film.pass_stride;
    const uint aux_w_offset = kernel_data.film.pass_adaptive_aux_buffer + 3;

    if (buffer[aux_w_offset] == 0.0f) {
      if (y > start_y && !prev) {
        index = index - stride;
        buffer = render_buffer + (uint64_t)index * kernel_data.film.pass_stride;
        buffer[aux_w_offset] = 0.0f;
      }
      prev = true;
    }
    else {
      if (prev) {
        buffer[aux_w_offset] = 0.0f;
      }
      prev = false;
    }
  }
}

CCL_NAMESPACE_END
