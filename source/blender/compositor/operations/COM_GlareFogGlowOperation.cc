/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <complex>
#include <cstring>
#include <memory>
#include <numeric>

#if defined(WITH_FFTW3)
#  include <fftw3.h>
#endif

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_fftw.hh"
#include "BLI_index_range.hh"
#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "COM_GlareFogGlowOperation.h"

namespace blender::compositor {

/* Given the x and y location in the range from 0 to kernel_size - 1, where kernel_size is odd,
 * compute the fog glow kernel value. The equations are arbitrary and were chosen using visual
 * judgement. The kernel is not normalized and need normalization. */
[[maybe_unused]] static float compute_fog_glow_kernel_value(int x, int y, int kernel_size)
{
  const int half_kernel_size = kernel_size / 2;
  const float scale = 0.25f * math::sqrt(math::square(kernel_size));
  const float v = ((y - half_kernel_size) / float(half_kernel_size));
  const float u = ((x - half_kernel_size) / float(half_kernel_size));
  const float r = (math::square(u) + math::square(v)) * scale;
  const float d = -math::sqrt(math::sqrt(math::sqrt(r))) * 9.0f;
  const float kernel_value = math::exp(d);

  const float window = (0.5f + 0.5f * math::cos(u * math::numbers::pi)) *
                       (0.5f + 0.5f * math::cos(v * math::numbers::pi));
  const float windowed_kernel_value = window * kernel_value;

  return windowed_kernel_value;
}

void GlareFogGlowOperation::generate_glare(float *output,
                                           MemoryBuffer *image,
                                           const NodeGlare *settings)
{
#if defined(WITH_FFTW3)
  fftw::initialize_float();

  /* We use an odd sized kernel since an even one will typically introduce a tiny offset as it has
   * no exact center value. */
  const int kernel_size = (1 << settings->size) + 1;

  /* Since we will be doing a circular convolution, we need to zero pad our input image by half the
   * kernel size to avoid the kernel affecting the pixels at the other side of image. Therefore,
   * zero boundary is assumed. */
  const int needed_padding_amount = kernel_size / 2;
  const int2 image_size = int2(image->get_width(), image->get_height());
  const int2 needed_spatial_size = image_size + needed_padding_amount;
  const int2 spatial_size = fftw::optimal_size_for_real_transform(needed_spatial_size);

  /* The FFTW real to complex transforms utilizes the hermitian symmetry of real transforms and
   * stores only half the output since the other half is redundant, so we only allocate half of the
   * first dimension. See Section 4.3.4 Real-data DFT Array Format in the FFTW manual for more
   * information. */
  const int2 frequency_size = int2(spatial_size.x / 2 + 1, spatial_size.y);

  float *kernel_spatial_domain = fftwf_alloc_real(spatial_size.x * spatial_size.y);
  std::complex<float> *kernel_frequency_domain = reinterpret_cast<std::complex<float> *>(
      fftwf_alloc_complex(frequency_size.x * frequency_size.y));

  /* Create a real to complex plan to transform the kernel to the frequency domain, but the same
   * plan will be used for the image since they both have the same dimensions. */
  fftwf_plan forward_plan = fftwf_plan_dft_r2c_2d(
      spatial_size.y,
      spatial_size.x,
      kernel_spatial_domain,
      reinterpret_cast<fftwf_complex *>(kernel_frequency_domain),
      FFTW_ESTIMATE);

  /* Use a double to sum the kernel since floats are not stable with threaded summation. */
  threading::EnumerableThreadSpecific<double> sum_by_thread([]() { return 0.0; });

  /* Compute the kernel while zero padding to match the padded image size. */
  threading::parallel_for(IndexRange(spatial_size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(spatial_size.x)) {
        /* We offset the computed kernel with wrap around such that it is centered at the zero
         * point, which is the expected format for doing circular convolutions in the frequency
         * domain. */
        const int half_kernel_size = kernel_size / 2;
        int64_t output_x = mod_i(x - half_kernel_size, spatial_size.x);
        int64_t output_y = mod_i(y - half_kernel_size, spatial_size.y);

        const bool is_inside_kernel = x < kernel_size && y < kernel_size;
        if (is_inside_kernel) {
          const float kernel_value = compute_fog_glow_kernel_value(x, y, kernel_size);
          kernel_spatial_domain[output_x + output_y * spatial_size.x] = kernel_value;
          sum_by_thread.local() += kernel_value;
        }
        else {
          kernel_spatial_domain[output_x + output_y * spatial_size.x] = 0.0f;
        }
      }
    }
  });

  /* Instead of normalizing the kernel now, we normalize it in the frequency domain since we will
   * be doing sample normalization anyways. This is okay since the Fourier transform is linear. */
  const float kernel_normalization_factor = float(
      std::accumulate(sum_by_thread.begin(), sum_by_thread.end(), 0.0));

  fftwf_execute_dft_r2c(forward_plan,
                        kernel_spatial_domain,
                        reinterpret_cast<fftwf_complex *>(kernel_frequency_domain));
  fftwf_free(kernel_spatial_domain);

  /* We only process the color channels, the alpha channel is written to the output as is. */
  const int channels_count = 3;
  const int64_t spatial_pixels_per_channel = int64_t(spatial_size.x) * spatial_size.y;
  const int64_t frequency_pixels_per_channel = int64_t(spatial_size.x) * spatial_size.y;
  const int64_t spatial_pixels_count = spatial_pixels_per_channel * channels_count;
  const int64_t frequency_pixels_count = frequency_pixels_per_channel * channels_count;

  float *image_spatial_domain = fftwf_alloc_real(spatial_pixels_count);
  std::complex<float> *image_frequency_domain = reinterpret_cast<std::complex<float> *>(
      fftwf_alloc_complex(frequency_pixels_count));

  /* Zero pad the image to the required spatial domain size, storing each channel in planar
   * format for better cache locality, that is, RRRR...GGGG...BBBB. */
  threading::parallel_for(IndexRange(spatial_size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(spatial_size.x)) {
        const bool is_inside_image = x < image_size.x && y < image_size.y;
        for (const int64_t channel : IndexRange(channels_count)) {
          const int64_t base_index = x + y * spatial_size.x;
          const int64_t output_index = base_index + spatial_pixels_per_channel * channel;
          if (is_inside_image) {
            image_spatial_domain[output_index] = image->get_elem(x, y)[channel];
          }
          else {
            image_spatial_domain[output_index] = 0.0f;
          }
        }
      }
    }
  });

  threading::parallel_for(IndexRange(channels_count), 1, [&](const IndexRange sub_range) {
    for (const int64_t channel : sub_range) {
      fftwf_execute_dft_r2c(forward_plan,
                            image_spatial_domain + spatial_pixels_per_channel * channel,
                            reinterpret_cast<fftwf_complex *>(image_frequency_domain) +
                                frequency_pixels_per_channel * channel);
    }
  });

  /* Multiply the kernel and the image in the frequency domain to perform the convolution. The
   * FFT is not normalized, meaning the result of the FFT followed by an inverse FFT will result
   * in an image that is scaled by a factor of the product of the width and height, so we take
   * that into account by dividing by that scale. See Section 4.8.6 Multi-dimensional Transforms of
   * the FFTW manual for more information. */
  const float normalization_scale = float(spatial_size.x) * spatial_size.y *
                                    kernel_normalization_factor;
  threading::parallel_for(IndexRange(frequency_size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t channel : IndexRange(channels_count)) {
      for (const int64_t y : sub_y_range) {
        for (const int64_t x : IndexRange(frequency_size.x)) {
          const int64_t base_index = x + y * frequency_size.x;
          const int64_t output_index = base_index + frequency_pixels_per_channel * channel;
          const std::complex<float> kernel_value = kernel_frequency_domain[base_index];
          image_frequency_domain[output_index] *= kernel_value / normalization_scale;
        }
      }
    }
  });

  /* Create a complex to real plan to transform the image to the real domain. */
  fftwf_plan backward_plan = fftwf_plan_dft_c2r_2d(
      spatial_size.y,
      spatial_size.x,
      reinterpret_cast<fftwf_complex *>(image_frequency_domain),
      image_spatial_domain,
      FFTW_ESTIMATE);

  threading::parallel_for(IndexRange(channels_count), 1, [&](const IndexRange sub_range) {
    for (const int64_t channel : sub_range) {
      fftwf_execute_dft_c2r(backward_plan,
                            reinterpret_cast<fftwf_complex *>(image_frequency_domain) +
                                frequency_pixels_per_channel * channel,
                            image_spatial_domain + spatial_pixels_per_channel * channel);
    }
  });

  /* Copy the result to the output. */
  threading::parallel_for(IndexRange(image_size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(image_size.x)) {
        for (const int64_t channel : IndexRange(channels_count)) {
          const int64_t output_index = (x + y * image_size.x) * image->get_num_channels();
          const int64_t base_index = x + y * spatial_size.x;
          const int64_t input_index = base_index + spatial_pixels_per_channel * channel;
          output[output_index + channel] = image_spatial_domain[input_index];
          output[output_index + 3] = image->get_buffer()[output_index + 3];
        }
      }
    }
  });

  fftwf_destroy_plan(forward_plan);
  fftwf_destroy_plan(backward_plan);
  fftwf_free(image_spatial_domain);
  fftwf_free(image_frequency_domain);
  fftwf_free(kernel_frequency_domain);
#else
  UNUSED_VARS(output, image, settings);
#endif
}

}  // namespace blender::compositor
