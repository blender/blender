/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <complex>
#include <cstdint>
#include <memory>
#include <numeric>

#if defined(WITH_FFTW3)
#  include <fftw3.h>
#endif

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_fftw.hh"
#include "BLI_hash.hh"
#include "BLI_index_range.hh"
#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_math_numbers.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "COM_fog_glow_kernel.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Fog Glow Kernel Key.
 */

FogGlowKernelKey::FogGlowKernelKey(int kernel_size, int2 spatial_size)
    : kernel_size(kernel_size), spatial_size(spatial_size)
{
}

uint64_t FogGlowKernelKey::hash() const
{
  return get_default_hash(kernel_size, spatial_size);
}

bool operator==(const FogGlowKernelKey &a, const FogGlowKernelKey &b)
{
  return a.kernel_size == b.kernel_size && a.spatial_size == b.spatial_size;
}

/* --------------------------------------------------------------------
 * Fog Glow Kernel.
 */

/* Given the x and y location in the range from 0 to kernel_size - 1, where kernel_size is odd,
 * compute the fog glow kernel value. The equations are arbitrary and were chosen using visual
 * judgment. The kernel is not normalized and need normalization. */
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

FogGlowKernel::FogGlowKernel(int kernel_size, int2 spatial_size)
{
#if defined(WITH_FFTW3)
  fftw::initialize_float();

  /* The FFTW real to complex transforms utilizes the hermitian symmetry of real transforms and
   * stores only half the output since the other half is redundant, so we only allocate half of
   * the first dimension. See Section 4.3.4 Real-data DFT Array Format in the FFTW manual for
   * more information. */
  const int2 frequency_size = int2(spatial_size.x / 2 + 1, spatial_size.y);

  float *kernel_spatial_domain = fftwf_alloc_real(spatial_size.x * spatial_size.y);
  frequencies_ = reinterpret_cast<std::complex<float> *>(
      fftwf_alloc_complex(frequency_size.x * frequency_size.y));

  /* Create a real to complex plan to transform the kernel to the frequency domain. */
  fftwf_plan forward_plan = fftwf_plan_dft_r2c_2d(spatial_size.y,
                                                  spatial_size.x,
                                                  kernel_spatial_domain,
                                                  reinterpret_cast<fftwf_complex *>(frequencies_),
                                                  FFTW_ESTIMATE);

  /* Use a double to sum the kernel since floats are not stable with threaded summation. */
  threading::EnumerableThreadSpecific<double> sum_by_thread([]() { return 0.0; });

  /* Compute the kernel while zero padding to match the padded image size. */
  threading::parallel_for(IndexRange(spatial_size.y), 1, [&](const IndexRange sub_y_range) {
    double &sum = sum_by_thread.local();
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
          sum += kernel_value;
        }
        else {
          kernel_spatial_domain[output_x + output_y * spatial_size.x] = 0.0f;
        }
      }
    }
  });

  fftwf_execute_dft_r2c(
      forward_plan, kernel_spatial_domain, reinterpret_cast<fftwf_complex *>(frequencies_));
  fftwf_destroy_plan(forward_plan);
  fftwf_free(kernel_spatial_domain);

  /* The computed kernel is not normalized and should be normalized, but instead of normalizing the
   * kernel during computation, we normalize it in the frequency domain when convolving the kernel
   * to the image since we will be doing sample normalization anyways. This is okay since the
   * Fourier transform is linear. */
  normalization_factor_ = float(std::accumulate(sum_by_thread.begin(), sum_by_thread.end(), 0.0));
#else
  UNUSED_VARS(kernel_size, spatial_size);
#endif
}

FogGlowKernel::~FogGlowKernel()
{
#if defined(WITH_FFTW3)
  fftwf_free(frequencies_);
#endif
}

std::complex<float> *FogGlowKernel::frequencies() const
{
  return frequencies_;
}

float FogGlowKernel::normalization_factor() const
{
  return normalization_factor_;
}

/* --------------------------------------------------------------------
 * Fog Glow Kernel Container.
 */

void FogGlowKernelContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

FogGlowKernel &FogGlowKernelContainer::get(int kernel_size, int2 spatial_size)
{
  const FogGlowKernelKey key(kernel_size, spatial_size);

  auto &kernel = *map_.lookup_or_add_cb(
      key, [&]() { return std::make_unique<FogGlowKernel>(kernel_size, spatial_size); });

  kernel.needed = true;
  return kernel;
}

}  // namespace blender::realtime_compositor
