/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_OPENIMAGEDENOISE

#  include "integrator/denoiser.h"
#  include "util/openimagedenoise.h"
#  include "util/vector.h"

CCL_NAMESPACE_BEGIN

/* Shared OIDN denoiser functionality for both CPU and GPU implementations.
 * Uses composition pattern to avoid multiple inheritance. */
class OIDNDenoiserBase {
 public:
  explicit OIDNDenoiserBase(Denoiser *denoiser);
  ~OIDNDenoiserBase()
  {
    release_all_resources();
  }

  /* OIDN handles and state. */
  OIDNDevice oidn_device_ = nullptr;
  OIDNFilter oidn_filter_ = nullptr;
  OIDNFilter albedo_filter_ = nullptr;
  OIDNFilter normal_filter_ = nullptr;
  DenoiserQuality quality_ = DENOISER_QUALITY_HIGH;
  bool is_configured_ = false;
  int2 configured_size_ = make_int2(0, 0);
  vector<uint8_t> custom_weights_;
  bool use_pass_albedo_ = false;
  bool use_pass_normal_ = false;

  /* Shared methods. */
  bool create_filters(DenoiserQuality quality, bool denoise_abledo, bool denoise_normal);
  void load_custom_weights();
  void release_all_resources();
  bool denoise_configure_if_needed(int width, int height);

 private:
  OIDNFilter new_filter();
  Denoiser *denoiser_; /* Back-pointer for error reporting. */
};

CCL_NAMESPACE_END

#endif /* WITH_OPENIMAGEDENOISE */
