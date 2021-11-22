/*
 * Copyright 2019 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "kernel/film/write_passes.h"

CCL_NAMESPACE_BEGIN

/* Check whether the pixel has converged and should not be sampled anymore. */

ccl_device_forceinline bool kernel_need_sample_pixel(KernelGlobals kg,
                                                     ConstIntegratorState state,
                                                     ccl_global float *render_buffer)
{
  if (kernel_data.film.pass_adaptive_aux_buffer == PASS_UNUSED) {
    return true;
  }

  const uint32_t render_pixel_index = INTEGRATOR_STATE(state, path, render_pixel_index);
  const uint64_t render_buffer_offset = (uint64_t)render_pixel_index *
                                        kernel_data.film.pass_stride;
  ccl_global float *buffer = render_buffer + render_buffer_offset;

  const uint aux_w_offset = kernel_data.film.pass_adaptive_aux_buffer + 3;
  return buffer[aux_w_offset] == 0.0f;
}

/* Determines whether to continue sampling a given pixel or if it has sufficiently converged. */

ccl_device bool kernel_adaptive_sampling_convergence_check(KernelGlobals kg,
                                                           ccl_global float *render_buffer,
                                                           int x,
                                                           int y,
                                                           float threshold,
                                                           bool reset,
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
  const float inv_sample = 1.0f / sample;

  /* The per pixel error as seen in section 2.1 of
   * "A hierarchical automatic stopping condition for Monte Carlo global illumination" */
  const float error_difference = (fabsf(I.x - A.x) + fabsf(I.y - A.y) + fabsf(I.z - A.z)) *
                                 inv_sample;
  const float error_normalize = sqrtf((I.x + I.y + I.z) * inv_sample);
  /* A small epsilon is added to the divisor to prevent division by zero. */
  const float error = error_difference / (0.0001f + error_normalize);
  const bool did_converge = (error < threshold);

  const uint aux_w_offset = kernel_data.film.pass_adaptive_aux_buffer + 3;
  buffer[aux_w_offset] = did_converge;

  return did_converge;
}

/* This is a simple box filter in two passes.
 * When a pixel demands more adaptive samples, let its neighboring pixels draw more samples too. */

ccl_device void kernel_adaptive_sampling_filter_x(KernelGlobals kg,
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
    ccl_global float *buffer = render_buffer + index * kernel_data.film.pass_stride;
    const uint aux_w_offset = kernel_data.film.pass_adaptive_aux_buffer + 3;

    if (buffer[aux_w_offset] == 0.0f) {
      if (x > start_x && !prev) {
        index = index - 1;
        buffer = render_buffer + index * kernel_data.film.pass_stride;
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

ccl_device void kernel_adaptive_sampling_filter_y(KernelGlobals kg,
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
    ccl_global float *buffer = render_buffer + index * kernel_data.film.pass_stride;
    const uint aux_w_offset = kernel_data.film.pass_adaptive_aux_buffer + 3;

    if (buffer[aux_w_offset] == 0.0f) {
      if (y > start_y && !prev) {
        index = index - stride;
        buffer = render_buffer + index * kernel_data.film.pass_stride;
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
