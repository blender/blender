/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/film/write.h"

/* Denoise volume scattering probability guiding buffers. */

CCL_NAMESPACE_BEGIN

/* Two-pass Gaussian filter. */
ccl_device void volume_guiding_filter_x(KernelGlobals kg,
                                        ccl_global float *render_buffer,
                                        const int y,
                                        const int center_x,
                                        const int min_x,
                                        const int max_x,
                                        const int offset,
                                        const int stride)
{
  kernel_assert(kernel_data.film.pass_volume_scatter != PASS_UNUSED);
  kernel_assert(kernel_data.film.pass_sample_count != PASS_UNUSED);

  const int radius = 5;
  const int filter_width = radius * 2 + 1;

  /* sigma = 1.5 with integral according to
   * https://lisyarus.github.io/blog/posts/blur-coefficients-generator.html
   * https://bartwronski.com/2021/10/31/practical-gaussian-filter-binomial-filter-and-small-sigma-gaussians/
   */
  const float gaussian_params[filter_width] = {0.0012273699895602f,
                                               0.0084674212370284f,
                                               0.0379843612914121f,
                                               0.1108921888487800f,
                                               0.2108379677336155f,
                                               0.2611813817992076f,
                                               0.2108379677336155f,
                                               0.1108921888487800f,
                                               0.0379843612914121f,
                                               0.0084674212370284f,
                                               0.0012273699895602f};

  ccl_global float *buffer = film_pass_pixel_render_buffer(
      kg, center_x, y, offset, stride, render_buffer);

  /* Apply Gaussian filter in x direction. */
  float3 scatter = zero_float3(), transmit = zero_float3();
  for (int dx = 0; dx < filter_width; dx++) {
    const int x = center_x + dx - radius;
    if (x < min_x || x >= max_x) {
      /* Ignore boundary pixels. */
      continue;
    }

    ccl_global float *buffer = film_pass_pixel_render_buffer(
        kg, x, y, offset, stride, render_buffer);

    const float weight = gaussian_params[dx] /
                         __float_as_uint(buffer[kernel_data.film.pass_sample_count]);

    scatter += kernel_read_pass_float3(buffer + kernel_data.film.pass_volume_scatter) * weight;
    transmit += kernel_read_pass_float3(buffer + kernel_data.film.pass_volume_transmit) * weight;
  }

  /* Write to the buffer. */
  film_overwrite_pass_rgbe(buffer + kernel_data.film.pass_volume_scatter_denoised, scatter);
  film_overwrite_pass_rgbe(buffer + kernel_data.film.pass_volume_transmit_denoised, transmit);
}

ccl_device void volume_guiding_filter_y(KernelGlobals kg,
                                        ccl_global float *render_buffer,
                                        const int x,
                                        const int min_y,
                                        const int max_y,
                                        const int offset,
                                        const int stride)
{
  kernel_assert(kernel_data.film.pass_volume_scatter != PASS_UNUSED);

  const int radius = 5;
  const int filter_width = radius * 2 + 1;

  const float gaussian_params[filter_width] = {0.0012273699895602f,
                                               0.0084674212370284f,
                                               0.0379843612914121f,
                                               0.1108921888487800f,
                                               0.2108379677336155f,
                                               0.2611813817992076f,
                                               0.2108379677336155f,
                                               0.1108921888487800f,
                                               0.0379843612914121f,
                                               0.0084674212370284f,
                                               0.0012273699895602f};

  /* Store neighboring values to avoid overwriting. */
  float3 scatter_neighbors[filter_width], transmit_neighbors[filter_width];

  /* Initialze neighbors. */
  for (int i = 0; i < filter_width; i++) {
    const int y = min_y + i;
    if (i >= radius || y < min_y || y >= max_y) {
      /* Out-of-boundary neighbors are initialized with zero. */
      scatter_neighbors[i] = transmit_neighbors[i] = zero_float3();
    }
    else {
      ccl_global float *buffer = film_pass_pixel_render_buffer(
          kg, x, y, offset, stride, render_buffer);
      scatter_neighbors[i] = kernel_read_pass_rgbe(buffer +
                                                   kernel_data.film.pass_volume_scatter_denoised);
      transmit_neighbors[i] = kernel_read_pass_rgbe(
          buffer + kernel_data.film.pass_volume_transmit_denoised);
    }
  }

  /* Apply Gaussian filter in y direction. */
  int index = radius;
  for (int y = min_y; y < max_y; y++) {
    /* Fetch the furthest neighbor to the right. */
    const int next_y = y + radius;
    if (next_y < min_y || next_y >= max_y) {
      scatter_neighbors[index] = zero_float3();
      transmit_neighbors[index] = zero_float3();
    }
    else {
      ccl_global float *buffer = film_pass_pixel_render_buffer(
          kg, x, next_y, offset, stride, render_buffer);
      scatter_neighbors[index] = kernel_read_pass_rgbe(
          buffer + kernel_data.film.pass_volume_scatter_denoised);
      transmit_neighbors[index] = kernel_read_pass_rgbe(
          buffer + kernel_data.film.pass_volume_transmit_denoised);
    }

    /* Slide the kernel to the right. */
    index = (index + 1) % filter_width;

    /* Apply convolution. */
    float3 scatter = zero_float3(), transmit = zero_float3();
    for (int i = 0; i < filter_width; i++) {
      scatter += gaussian_params[i] * scatter_neighbors[(index + i) % filter_width];
      transmit += gaussian_params[i] * transmit_neighbors[(index + i) % filter_width];
    }

    /* Write to the buffers. */
    ccl_global float *buffer = film_pass_pixel_render_buffer(
        kg, x, y, offset, stride, render_buffer);
    film_overwrite_pass_rgbe(buffer + kernel_data.film.pass_volume_scatter_denoised,
                             fabs(scatter));
    film_overwrite_pass_rgbe(buffer + kernel_data.film.pass_volume_transmit_denoised,
                             fabs(transmit));
  }
}

CCL_NAMESPACE_END
