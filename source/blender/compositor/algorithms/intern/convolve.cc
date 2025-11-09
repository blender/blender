/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <complex>
#include <numeric>

#include "BLI_array.hh"
#include "BLI_assert.h"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_fftw.hh"
#include "BLI_index_range.hh"
#include "BLI_memory_utils.hh"
#include "BLI_task.hh"

#if defined(WITH_FFTW3)
#  include <fftw3.h>
#endif

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_convolve.hh"

namespace blender::compositor {

void convolve(Context &context,
              const Result &input,
              const Result &kernel,
              Result &output,
              const bool normalize_kernel)
{
#if defined(WITH_FFTW3)
  BLI_assert(input.type() == ResultType::Color);
  BLI_assert(kernel.type() == ResultType::Float || kernel.type() == ResultType::Color);
  BLI_assert(output.type() == ResultType::Color);

  /* Since we will be doing a circular convolution, we need to zero pad the input image by the
   * kernel size and vice versa to avoid the kernel affecting the pixels at the other side of
   * image. The kernel size is limited by the image size since it will have no effect on the image
   * during convolution. */
  const int2 image_size = input.domain().size;
  const int2 kernel_size = kernel.domain().size;
  const int2 needed_padding_amount = math::max(kernel_size, image_size);
  const int2 needed_spatial_size = image_size + needed_padding_amount - 1;
  const int2 spatial_size = fftw::optimal_size_for_real_transform(needed_spatial_size);

  /* The FFTW real to complex transforms utilizes the hermitian symmetry of real transforms and
   * stores only half the output since the other half is redundant, so we only allocate half of
   * the first dimension. See Section 4.3.4 Real-data DFT Array Format in the FFTW manual for
   * more information. */
  const int2 frequency_size = int2(spatial_size.x / 2 + 1, spatial_size.y);

  constexpr int input_channels_count = 4;
  const int64_t spatial_pixels_count = int64_t(spatial_size.x) * spatial_size.y;
  const int64_t frequency_pixels_count = int64_t(frequency_size.x) * frequency_size.y;

  /* A structure to gather all buffers that need to be forward transformed from the real to the
   * frequency domain. */
  struct ForwardTransformTask {
    float *input;
    std::complex<float> *output;
  };
  Vector<ForwardTransformTask> forward_transform_tasks;

  /* Allocate a real buffer and a complex buffer for each of the input channels for the FFT input
   * and output respectively, then add a forward transform task for it. */
  Array<float *> image_spatial_domain_channels(input_channels_count);
  Array<std::complex<float> *> image_frequency_domain_channels(input_channels_count);
  for (const int channel : image_spatial_domain_channels.index_range()) {
    image_spatial_domain_channels[channel] = fftwf_alloc_real(spatial_pixels_count);
    image_frequency_domain_channels[channel] = reinterpret_cast<std::complex<float> *>(
        fftwf_alloc_complex(frequency_pixels_count));
    forward_transform_tasks.append(ForwardTransformTask{image_spatial_domain_channels[channel],
                                                        image_frequency_domain_channels[channel]});
  }

  BLI_SCOPED_DEFER([&]() {
    for (const int channel : image_spatial_domain_channels.index_range()) {
      fftwf_free(image_spatial_domain_channels[channel]);
      fftwf_free(image_frequency_domain_channels[channel]);
    }
  });

  const int kernel_channels_count = kernel.channels_count();
  const bool is_color_kernel = kernel_channels_count == 4;

  /* Allocate a real buffer and a complex buffer for each of the kernel channels for the FFT input
   * and output respectively, then add a forward transform task for it. */
  Array<float *> kernel_spatial_domain_channels(kernel_channels_count);
  Array<std::complex<float> *> kernel_frequency_domain_channels(kernel_channels_count);
  for (const int channel : kernel_spatial_domain_channels.index_range()) {
    kernel_spatial_domain_channels[channel] = fftwf_alloc_real(spatial_pixels_count);
    kernel_frequency_domain_channels[channel] = reinterpret_cast<std::complex<float> *>(
        fftwf_alloc_complex(frequency_pixels_count));
    forward_transform_tasks.append(ForwardTransformTask{
        kernel_spatial_domain_channels[channel], kernel_frequency_domain_channels[channel]});
  }

  BLI_SCOPED_DEFER([&]() {
    for (const int channel : kernel_spatial_domain_channels.index_range()) {
      fftwf_free(kernel_spatial_domain_channels[channel]);
      fftwf_free(kernel_frequency_domain_channels[channel]);
    }
  });

  /* Create a real to complex and complex to real plans to transform the image to the frequency
   * domain.
   *
   * Notice that FFTW provides an advanced interface as per Section 4.4.2 Advanced Real-data DFTs
   * to transform all image channels simultaneously with interleaved pixel layouts. But profiling
   * showed better performance when running a single plan in parallel for all image channels with a
   * planner pixel format, so this is what we will be doing.
   *
   * The input and output buffers here are dummy buffers and still not initialized, because they
   * are required by the planner internally for planning and their data will be overwritten. So
   * make sure not to initialize the buffers before creating the plan. */
  fftwf_plan forward_plan = fftwf_plan_dft_r2c_2d(
      spatial_size.y,
      spatial_size.x,
      image_spatial_domain_channels[0],
      reinterpret_cast<fftwf_complex *>(image_frequency_domain_channels[0]),
      FFTW_ESTIMATE);
  fftwf_plan backward_plan = fftwf_plan_dft_c2r_2d(
      spatial_size.y,
      spatial_size.x,
      reinterpret_cast<fftwf_complex *>(image_frequency_domain_channels[0]),
      image_spatial_domain_channels[0],
      FFTW_ESTIMATE);

  BLI_SCOPED_DEFER([&]() {
    fftwf_destroy_plan(forward_plan);
    fftwf_destroy_plan(backward_plan);
  });

  /* Download GPU results to CPU for GPU contexts. */
  Result input_cpu = context.use_gpu() ? input.download_to_cpu() : input;
  Result kernel_cpu = context.use_gpu() ? kernel.download_to_cpu() : kernel;

  BLI_SCOPED_DEFER([&]() {
    if (context.use_gpu()) {
      input_cpu.release();
      kernel_cpu.release();
    }
  });

  /* Zero pad the image to the required spatial domain size, storing each channel in planar
   * format for better cache locality, that is, RRRR...GGGG...BBBB...AAAA. */
  threading::memory_bandwidth_bound_task(spatial_pixels_count * sizeof(float), [&]() {
    parallel_for(spatial_size, [&](const int2 texel) {
      const Color pixel_color = input_cpu.load_pixel_zero<Color>(texel);
      for (const int channel : IndexRange(input_channels_count)) {
        float *buffer = image_spatial_domain_channels[channel];
        const int64_t index = texel.y * int64_t(spatial_size.x) + texel.x;
        buffer[index] = pixel_color[channel];
      }
    });
  });

  /* Use doubles to sum the kernel since floats are not stable with threaded summation. We always
   * use a double4 even for float kernels for generality, in that case, only the first component
   * is initialized. */
  threading::EnumerableThreadSpecific<double4> sum_by_thread([]() { return double4(0.0); });

  /* Compute the kernel while zero padding to match the spatial size. */
  const int2 kernel_center = kernel_size / 2;
  parallel_for(spatial_size, [&](const int2 texel) {
    /* We offset the computed kernel with wrap around such that it is centered at the zero
     * point, which is the expected format for doing circular convolutions in the frequency
     * domain. */
    const int2 centered_texel = kernel_center - texel;
    const int2 wrapped_texel = int2(mod_i(centered_texel.x, spatial_size.x),
                                    mod_i(centered_texel.y, spatial_size.y));

    const float4 kernel_value = is_color_kernel ?
                                    float4(kernel_cpu.load_pixel_zero<Color>(wrapped_texel)) :
                                    float4(kernel_cpu.load_pixel_zero<float>(wrapped_texel));
    for (const int channel : IndexRange(kernel_channels_count)) {
      float *buffer = kernel_spatial_domain_channels[channel];
      buffer[texel.x + texel.y * int64_t(spatial_size.x)] = kernel_value[channel];
    }
    sum_by_thread.local() += double4(kernel_value);
  });

  /* The computed kernel is not normalized and should be normalized, but instead of normalizing the
   * kernel during computation, we normalize it in the frequency domain when convolving the kernel
   * to the image since we will be doing sample normalization anyways. This is okay since the
   * Fourier transform is linear. */
  const float4 sum = float4(
      std::accumulate(sum_by_thread.begin(), sum_by_thread.end(), double4(0.0)));
  const float4 sanitized_sum = float4(sum[0] == 0.0f ? 1.0f : sum[0],
                                      sum[1] == 0.0f ? 1.0f : sum[1],
                                      sum[2] == 0.0f ? 1.0f : sum[2],
                                      sum[3] == 0.0f ? 1.0f : sum[3]);
  const float4 normalization_factor = normalize_kernel ? sanitized_sum : float4(1.0f);

  /* Transform all necessary data from the real domain to the frequency domain. */
  threading::parallel_for(
      forward_transform_tasks.index_range(), 1, [&](const IndexRange sub_range) {
        for (const int64_t i : sub_range) {
          fftwf_execute_dft_r2c(
              forward_plan,
              forward_transform_tasks[i].input,
              reinterpret_cast<fftwf_complex *>(forward_transform_tasks[i].output));
        }
      });

  /* Multiply the kernel and the image in the frequency domain to perform the convolution. The
   * FFT is not normalized, meaning the result of the FFT followed by an inverse FFT will result
   * in an image that is scaled by a factor of the product of the width and height, so we take
   * that into account by dividing by that scale. See Section 4.8.6 Multi-dimensional Transforms
   * of the FFTW manual for more information. */
  const float4 normalization_scale = float(spatial_size.x) * spatial_size.y * normalization_factor;
  threading::parallel_for(IndexRange(frequency_size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t channel : IndexRange(input_channels_count)) {
      const int kernel_channel = is_color_kernel ? channel : 0;
      std::complex<float> *image_buffer = image_frequency_domain_channels[channel];
      const std::complex<float> *kernel_buffer = kernel_frequency_domain_channels[kernel_channel];
      for (const int64_t y : sub_y_range) {
        for (const int64_t x : IndexRange(frequency_size.x)) {
          const int64_t index = x + y * int64_t(frequency_size.x);
          image_buffer[index] *= kernel_buffer[index] / normalization_scale[kernel_channel];
        }
      }
    }
  });

  /* Transform channels from the frequency domain to the real domain. */
  threading::parallel_for(IndexRange(input_channels_count), 1, [&](const IndexRange sub_range) {
    for (const int64_t channel : sub_range) {
      fftwf_execute_dft_c2r(
          backward_plan,
          reinterpret_cast<fftwf_complex *>(image_frequency_domain_channels[channel]),
          image_spatial_domain_channels[channel]);
    }
  });

  Result output_cpu = context.create_result(input.type());
  output_cpu.allocate_texture(input.domain(), true, ResultStorageType::CPU);

  /* Copy the result to the output. */
  threading::memory_bandwidth_bound_task(input.size_in_bytes(), [&]() {
    parallel_for(image_size, [&](const int2 texel) {
      float4 color = float4(0.0f);
      for (const int channel : IndexRange(input_channels_count)) {
        const int64_t index = texel.x + texel.y * int64_t(spatial_size.x);
        color[channel] = image_spatial_domain_channels[channel][index];
      }
      output_cpu.store_pixel(texel, Color(color));
    });
  });

  if (context.use_gpu()) {
    Result output_gpu = output_cpu.upload_to_gpu(true);
    output.steal_data(output_gpu);
    output_cpu.release();
  }
  else {
    output.steal_data(output_cpu);
  }
#else
  UNUSED_VARS(kernel, normalize_kernel);
  output.allocate_texture(input.domain());
  if (context.use_gpu()) {
    GPU_texture_copy(output, input);
  }
  else {
    parallel_for(output.domain().size, [&](const int2 texel) {
      output.store_pixel(texel, input.load_pixel<float4>(texel));
    });
  }
#endif
}

}  // namespace blender::compositor
