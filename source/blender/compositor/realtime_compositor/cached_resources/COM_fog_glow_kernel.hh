/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <complex>
#include <cstdint>
#include <memory>

#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"

#include "COM_cached_resource.hh"

namespace blender::realtime_compositor {

/* ------------------------------------------------------------------------------------------------
 * Fog Glow Kernel Key.
 */
class FogGlowKernelKey {
 public:
  int kernel_size;
  int2 spatial_size;

  FogGlowKernelKey(int kernel_size, int2 spatial_size);

  uint64_t hash() const;
};

bool operator==(const FogGlowKernelKey &a, const FogGlowKernelKey &b);

/* -------------------------------------------------------------------------------------------------
 * Fog Glow Kernel.
 *
 * A cached resource that computes and caches a Fog Glow convolution kernel in the frequency domain
 * using FFTW's real to complex transform. The kernel is computed within a specific kernel size but
 * zero padded to match a certain spatial size. */
class FogGlowKernel : public CachedResource {
 private:
  /* The normalization factor that should be used to normalize the kernel frequencies. See the
   * implementation for more information. */
  float normalization_factor_ = 1.0f;

  /* The kernel in the frequency domain. See the implementation for more information. */
  std::complex<float> *frequencies_ = nullptr;

 public:
  FogGlowKernel(int kernel_size, int2 spatial_size);

  ~FogGlowKernel();

  std::complex<float> *frequencies() const;

  float normalization_factor() const;
};

/* ------------------------------------------------------------------------------------------------
 * Fog Glow Kernel Container.
 */
class FogGlowKernelContainer : CachedResourceContainer {
 private:
  Map<FogGlowKernelKey, std::unique_ptr<FogGlowKernel>> map_;

 public:
  void reset() override;

  /* Check if there is an available FogGlowKernel cached resource with the given parameters in the
   * container, if one exists, return it, otherwise, return a newly created one and add it to the
   * container. In both cases, tag the cached resource as needed to keep it cached for the next
   * evaluation. */
  FogGlowKernel &get(int kernel_size, int2 spatial_size);
};

}  // namespace blender::realtime_compositor
