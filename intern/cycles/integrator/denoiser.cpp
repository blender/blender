/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "integrator/denoiser.h"

#include "device/device.h"

#include "integrator/denoiser_oidn.h"
#include "session/display_driver.h"
#ifdef WITH_OPENIMAGEDENOISE
#  include "integrator/denoiser_oidn_gpu.h"
#endif
#ifdef WITH_OPTIX
#  include "integrator/denoiser_optix.h"
#endif
#include "session/buffers.h"

#include "util/log.h"
#include "util/openimagedenoise.h"
#include "util/progress.h"

CCL_NAMESPACE_BEGIN

/* Check whether given device is single (not a MultiDevice). */
static bool is_single_device(const Device *device)
{
  if (device->info.type == DEVICE_MULTI) {
    /* Assume multi-device is never created with a single sub-device.
     * If one requests such configuration it should be checked on the session level. */
    return false;
  }

  if (!device->info.multi_devices.empty()) {
    /* Some configurations will use multi_devices, but keep the type of an individual device.
     * This does simplify checks for homogeneous setups, but here we really need a single device.
     */
    return false;
  }

  return true;
}

/* Find best suitable device to perform denoiser on. Will iterate over possible sub-devices of
 * multi-device. */
static Device *find_best_device(Device *device,
                                const DenoiserType type,
                                const GraphicsInteropDevice &interop_device)
{
  Device *best_device = nullptr;

  device->foreach_device([&](Device *sub_device) {
    if ((sub_device->info.denoisers & type) == 0) {
      return;
    }

    if (!best_device) {
      best_device = sub_device;
    }
    else {
      /* Prefer non-CPU devices over CPU for performance reasons. */
      if (sub_device->info.type != DEVICE_CPU && best_device->info.type == DEVICE_CPU) {
        best_device = sub_device;
      }

      /* Prefer a device that can use graphics interop for faster display update. */
      if (sub_device->should_use_graphics_interop(interop_device) &&
          !best_device->should_use_graphics_interop(interop_device))
      {
        best_device = sub_device;
      }

      /* TODO(sergey): Choose fastest device from available ones. Taking into account performance
       * of the device and data transfer cost. */
    }
  });

  return best_device;
}

bool use_optix_denoiser(Device *denoiser_device, const DenoiseParams &params)
{
#ifdef WITH_OPTIX
  return (params.type == DENOISER_OPTIX &&
          OptiXDenoiser::is_device_supported(denoiser_device->info));
#else
  (void)denoiser_device;
  (void)params;
  return false;
#endif
}

bool use_gpu_oidn_denoiser(Device *denoiser_device, const DenoiseParams &params)
{
#ifdef WITH_OPENIMAGEDENOISE
  return (params.type == DENOISER_OPENIMAGEDENOISE && params.use_gpu &&
          OIDNDenoiserGPU::is_device_supported(denoiser_device->info));
#else
  (void)denoiser_device;
  (void)params;
  return false;
#endif
}

DenoiseParams get_effective_denoise_params(Device *denoiser_device,
                                           Device *cpu_fallback_device,
                                           const DenoiseParams &params,
                                           const GraphicsInteropDevice &interop_device,
                                           Device *&single_denoiser_device)
{
  DCHECK(params.use);

  DenoiseParams effective_denoise_params = params;

  single_denoiser_device = nullptr;
  if (is_single_device(denoiser_device)) {
    /* Simple case: denoising happens on a single device. */
    single_denoiser_device = denoiser_device;
  }
  else {
    /* Find best device from the ones which are proposed for denoising.
     * The choice is expected to be between a few GPUs, or between a GPU and a CPU
     * or between a few GPUs and a CPU. */
    single_denoiser_device = find_best_device(denoiser_device, params.type, interop_device);
  }
  /* Ensure that we have a device to be used later in the code below. */
  if (single_denoiser_device == nullptr) {
    single_denoiser_device = cpu_fallback_device;
  }

  const bool is_cpu_denoiser_device = single_denoiser_device->info.type == DEVICE_CPU;
  if (is_cpu_denoiser_device == false) {
    if (use_optix_denoiser(single_denoiser_device, effective_denoise_params) ||
        use_gpu_oidn_denoiser(single_denoiser_device, effective_denoise_params))
    {
      /* Denoising parameters are correct and there is no need to fall back to CPU OIDN. */
      return effective_denoise_params;
    }
  }

  /* Always fallback to OIDN on CPU. */
  effective_denoise_params.type = DENOISER_OPENIMAGEDENOISE;
  effective_denoise_params.use_gpu = false;

  return effective_denoise_params;
}

unique_ptr<Denoiser> Denoiser::create(Device *denoiser_device,
                                      Device *cpu_fallback_device,
                                      const DenoiseParams &params,
                                      const GraphicsInteropDevice &interop_device)
{

  Device *single_denoiser_device;
  const DenoiseParams effective_denoiser_params = get_effective_denoise_params(
      denoiser_device, cpu_fallback_device, params, interop_device, single_denoiser_device);

  const bool is_cpu_denoiser_device = single_denoiser_device->info.type == DEVICE_CPU;
  if (is_cpu_denoiser_device == false) {
#ifdef WITH_OPTIX
    if (use_optix_denoiser(single_denoiser_device, effective_denoiser_params)) {
      return make_unique<OptiXDenoiser>(single_denoiser_device, effective_denoiser_params);
    }
#endif

#ifdef WITH_OPENIMAGEDENOISE
    /* If available and allowed, then we will use OpenImageDenoise on GPU, otherwise on CPU. */
    if (use_gpu_oidn_denoiser(single_denoiser_device, effective_denoiser_params)) {
      return make_unique<OIDNDenoiserGPU>(single_denoiser_device, effective_denoiser_params);
    }
#endif
  }

  if (!openimagedenoise_supported()) {
    return nullptr;
  }

  /* Used preference CPU when possible, and fallback on CPU fallback device otherwise. */
  return make_unique<OIDNDenoiser>(is_cpu_denoiser_device ? single_denoiser_device :
                                                            cpu_fallback_device,
                                   effective_denoiser_params);
}

DenoiserType Denoiser::automatic_viewport_denoiser_type(const DeviceInfo &denoise_device_info)
{
#ifdef WITH_OPENIMAGEDENOISE
  if (denoise_device_info.type != DEVICE_CPU &&
      OIDNDenoiserGPU::is_device_supported(denoise_device_info))
  {
    return DENOISER_OPENIMAGEDENOISE;
  }
#else
  (void)denoise_device_info;
#endif

#ifdef WITH_OPTIX
  if (OptiXDenoiser::is_device_supported(denoise_device_info)) {
    return DENOISER_OPTIX;
  }
#endif

#ifdef WITH_OPENIMAGEDENOISE
  if (openimagedenoise_supported()) {
    return DENOISER_OPENIMAGEDENOISE;
  }
#endif

  return DENOISER_NONE;
}

Denoiser::Denoiser(Device *denoiser_device, const DenoiseParams &params)
    : denoiser_device_(denoiser_device), denoise_kernels_are_loaded_(false), params_(params)
{
  DCHECK(denoiser_device_);
  DCHECK(params.use);
}

void Denoiser::set_params(const DenoiseParams &params)
{
  DCHECK_EQ(params.type, params_.type);

  if (params.type == params_.type) {
    params_ = params;
  }
  else {
    LOG_ERROR << "Attempt to change denoiser type.";
  }
}

const DenoiseParams &Denoiser::get_params() const
{
  return params_;
}

bool Denoiser::load_kernels(Progress *progress)
{
  /* If we have successfully loaded kernels once, then there is no need to repeat this again. */
  if (denoise_kernels_are_loaded_) {
    return denoise_kernels_are_loaded_;
  }

  if (progress) {
    progress->set_status("Loading denoising kernels (may take a few minutes the first time)");
  }

  if (!denoiser_device_) {
    set_error("No device available to denoise on");
    return false;
  }

  /* Only need denoising feature, everything else is unused. */
  if (!denoiser_device_->load_kernels(KERNEL_FEATURE_DENOISING)) {
    string message = denoiser_device_->error_message();
    if (message.empty()) {
      message = "Failed loading denoising kernel, see console for errors";
    }
    set_error(message);
    return false;
  }

  LOG_WORK << "Will denoise on " << denoiser_device_->info.description << " ("
           << denoiser_device_->info.id << ")";

  denoise_kernels_are_loaded_ = true;
  return true;
}

Device *Denoiser::get_denoiser_device() const
{
  return denoiser_device_;
}

CCL_NAMESPACE_END
