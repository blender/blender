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
#include "BLI_hash.hh"
#include "BLI_index_range.hh"
#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "COM_fog_glow_kernel.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * Fog Glow Kernel Key.
 */

FogGlowKernelKey::FogGlowKernelKey(int kernel_size,
                                   int2 spatial_size,
                                   math::AngleRadian field_of_view)
    : kernel_size(kernel_size), spatial_size(spatial_size), field_of_view(field_of_view)
{
}

uint64_t FogGlowKernelKey::hash() const
{
  return get_default_hash(kernel_size, spatial_size, field_of_view.degree());
}

bool operator==(const FogGlowKernelKey &a, const FogGlowKernelKey &b)
{
  return a.kernel_size == b.kernel_size && a.spatial_size == b.spatial_size &&
         a.field_of_view == b.field_of_view;
}

/* --------------------------------------------------------------------
 * Fog Glow Kernel.
 */

/* Given the texel coordinates and the constant field-of-view-per-pixel value, under the assumption
 * of a relatively small field of view as discussed in Section 3.2, this function computes the
 * fog glow kernel value. The kernel value is derived from Equation (5) of the following paper:
 *
 *   Spencer, Greg, et al. "Physically-Based Glare Effects for Digital Images."
 *   Proceedings of the 22nd Annual Conference on Computer Graphics and Interactive Techniques,
 *   1995.
 */

[[maybe_unused]] static float compute_fog_glow_kernel_value(
    int2 texel, math::AngleRadian field_of_view_per_pixel)
{
  const float theta_degree = math::length(float2(texel)) * field_of_view_per_pixel.degree();
  const float f0 = 2.61f * 1e6f * math::exp(-math::square(theta_degree / 0.02f));
  const float f1 = 20.91f / math::cube(theta_degree + 0.02f);
  const float f2 = 72.37f / math::square(theta_degree + 0.02f);
  const float kernel_value = 0.384f * f0 + 0.478f * f1 + 0.138f * f2;

  return kernel_value;
}

FogGlowKernel::FogGlowKernel(int kernel_size, int2 spatial_size, math::AngleRadian field_of_view)
{
#if defined(WITH_FFTW3)

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

  /* Compute the entire kernel's spatial space using compute_fog_glow_kernel_value. */
  threading::parallel_for(IndexRange(spatial_size.y), 1, [&](const IndexRange sub_y_range) {
    double &sum = sum_by_thread.local();
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(spatial_size.x)) {
        const int2 texel = int2(x, y);
        const int2 center_texel = spatial_size / 2;
        const int2 kernel_texel = texel - center_texel;
        const math::AngleRadian field_of_view_per_pixel = field_of_view / kernel_size;

        const float kernel_value = compute_fog_glow_kernel_value(kernel_texel,
                                                                 field_of_view_per_pixel);
        sum += kernel_value;

        /* We offset the computed kernel with wrap around such that it is centered at the zero
         * point, which is the expected format for doing circular convolutions in the frequency
         * domain. */
        int64_t output_x = mod_i(kernel_texel.x, spatial_size.x);
        int64_t output_y = mod_i(kernel_texel.y, spatial_size.y);
        kernel_spatial_domain[output_x + output_y * spatial_size.x] = kernel_value;
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
  UNUSED_VARS(kernel_size, spatial_size, field_of_view);
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

FogGlowKernel &FogGlowKernelContainer::get(int kernel_size,
                                           int2 spatial_size,
                                           math::AngleRadian field_of_view)
{
  const FogGlowKernelKey key(kernel_size, spatial_size, field_of_view);

  auto &kernel = *map_.lookup_or_add_cb(key, [&]() {
    return std::make_unique<FogGlowKernel>(kernel_size, spatial_size, field_of_view);
  });

  kernel.needed = true;
  return kernel;
}

}  // namespace blender::compositor
