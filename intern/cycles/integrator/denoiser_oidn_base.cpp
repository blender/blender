/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_OPENIMAGEDENOISE

#  include "integrator/denoiser_oidn_base.h"

#  include "util/log.h"
#  include "util/path.h"

#  if OIDN_VERSION_MAJOR < 2
#    define oidnSetFilterInt oidnSetFilter1i
#  endif

CCL_NAMESPACE_BEGIN

OIDNDenoiserBase::OIDNDenoiserBase(Denoiser *denoiser) : denoiser_(denoiser) {}

OIDNFilter OIDNDenoiserBase::new_filter()
{
  OIDNFilter filter = oidnNewFilter(oidn_device_, "RT");
  if (filter == nullptr) {
    const char *error_message = nullptr;
    const OIDNError err = oidnGetDeviceError(oidn_device_, &error_message);
    if (OIDN_ERROR_NONE != err) {
      LOG_ERROR << "OIDN error: " << error_message;
      denoiser_->set_error(error_message);
    }
  }
  else {
#  if OIDN_VERSION_MAJOR >= 2
    switch (quality_) {
      case DENOISER_QUALITY_FAST:
#    if OIDN_VERSION >= 20300
        oidnSetFilterInt(filter, "quality", OIDN_QUALITY_FAST);
        break;
#    endif
      case DENOISER_QUALITY_BALANCED:
        oidnSetFilterInt(filter, "quality", OIDN_QUALITY_BALANCED);
        break;
      case DENOISER_QUALITY_HIGH:
      default:
        oidnSetFilterInt(filter, "quality", OIDN_QUALITY_HIGH);
    }
#  endif

    oidnSetFilterBool(filter, "srgb", false);

    /* Set custom weights if available. */
    if (!custom_weights_.empty()) {
      oidnSetSharedFilterData(filter, "weights", custom_weights_.data(), custom_weights_.size());
    }
  }

  return filter;
}

bool OIDNDenoiserBase::create_filters(DenoiserQuality quality, bool use_albedo, bool use_normal)
{
  quality_ = quality;

  oidn_filter_ = new_filter();
  if (oidn_filter_ == nullptr) {
    return false;
  }

  oidnSetFilterBool(oidn_filter_, "hdr", true);

  if (use_albedo) {
    albedo_filter_ = new_filter();
    if (albedo_filter_ == nullptr) {
      return false;
    }
  }

  if (use_normal) {
    normal_filter_ = new_filter();
    if (normal_filter_ == nullptr) {
      return false;
    }
  }

  /* OIDN denoiser handle was created with the requested number of input passes. */
  use_pass_albedo_ = use_albedo;
  use_pass_normal_ = use_normal;

  /* OIDN denoiser has been created, but it needs configuration. */
  is_configured_ = false;

  return true;
}

void OIDNDenoiserBase::load_custom_weights()
{
  const char *custom_weight_path = getenv("CYCLES_OIDN_CUSTOM_WEIGHTS");
  if (!custom_weight_path) {
    return;
  }

  if (!path_read_binary(custom_weight_path, custom_weights_)) {
    LOG_ERROR << "Failed to load custom OpenImageDenoise weights";
  }
}

void OIDNDenoiserBase::release_all_resources()
{
  if (albedo_filter_) {
    oidnReleaseFilter(albedo_filter_);
    albedo_filter_ = nullptr;
  }
  if (normal_filter_) {
    oidnReleaseFilter(normal_filter_);
    normal_filter_ = nullptr;
  }
  if (oidn_filter_) {
    oidnReleaseFilter(oidn_filter_);
    oidn_filter_ = nullptr;
  }
  if (oidn_device_) {
    oidnReleaseDevice(oidn_device_);
    oidn_device_ = nullptr;
  }

  is_configured_ = false;
  use_pass_albedo_ = false;
  use_pass_normal_ = false;
}

bool OIDNDenoiserBase::denoise_configure_if_needed(int width, int height)
{
  const int2 size = make_int2(width, height);

  if (is_configured_ && configured_size_.x == size.x && configured_size_.y == size.y) {
    return true;
  }

  configured_size_ = size;
  is_configured_ = true;
  return true;
}

CCL_NAMESPACE_END

#endif /* WITH_OPENIMAGEDENOISE */
