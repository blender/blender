/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "integrator/denoiser.h"

#include "device/device.h"
#include "integrator/denoiser_oidn.h"
#include "integrator/denoiser_optix.h"
#include "session/buffers.h"
#include "util/log.h"
#include "util/progress.h"

CCL_NAMESPACE_BEGIN

unique_ptr<Denoiser> Denoiser::create(Device *path_trace_device, const DenoiseParams &params)
{
  DCHECK(params.use);

#ifdef WITH_OPTIX
  if (params.type == DENOISER_OPTIX && Device::available_devices(DEVICE_MASK_OPTIX).size()) {
    return make_unique<OptiXDenoiser>(path_trace_device, params);
  }
#endif

  /* Always fallback to OIDN. */
  DenoiseParams oidn_params = params;
  oidn_params.type = DENOISER_OPENIMAGEDENOISE;
  return make_unique<OIDNDenoiser>(path_trace_device, oidn_params);
}

Denoiser::Denoiser(Device *path_trace_device, const DenoiseParams &params)
    : path_trace_device_(path_trace_device), params_(params)
{
  DCHECK(params.use);
}

void Denoiser::set_params(const DenoiseParams &params)
{
  DCHECK_EQ(params.type, params_.type);

  if (params.type == params_.type) {
    params_ = params;
  }
  else {
    LOG(ERROR) << "Attempt to change denoiser type.";
  }
}

const DenoiseParams &Denoiser::get_params() const
{
  return params_;
}

bool Denoiser::load_kernels(Progress *progress)
{
  const Device *denoiser_device = ensure_denoiser_device(progress);

  if (!denoiser_device) {
    path_trace_device_->set_error("No device available to denoise on");
    return false;
  }

  VLOG_WORK << "Will denoise on " << denoiser_device->info.description << " ("
            << denoiser_device->info.id << ")";

  return true;
}

Device *Denoiser::get_denoiser_device() const
{
  return denoiser_device_;
}

/* Check whether given device is single (not a MultiDevice) and supports requested denoiser. */
static bool is_single_supported_device(Device *device, DenoiserType type)
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

  /* Check the denoiser type is supported. */
  return (device->info.denoisers & type);
}

/* Find best suitable device to perform denoiser on. Will iterate over possible sub-devices of
 * multi-device.
 *
 * If there is no device available which supports given denoiser type nullptr is returned. */
static Device *find_best_device(Device *device, DenoiserType type)
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
      /* Prefer a device that can use graphics interop for faster display update. */
      if (sub_device->should_use_graphics_interop() && !best_device->should_use_graphics_interop())
      {
        best_device = sub_device;
      }

      /* TODO(sergey): Choose fastest device from available ones. Taking into account performance
       * of the device and data transfer cost. */
    }
  });

  return best_device;
}

static DeviceInfo find_best_denoiser_device_info(const vector<DeviceInfo> &device_infos,
                                                 DenoiserType denoiser_type)
{
  for (const DeviceInfo &device_info : device_infos) {
    if ((device_info.denoisers & denoiser_type) == 0) {
      continue;
    }

    /* TODO(sergey): Use one of the already configured devices, so that OptiX denoising can happen
     * on a physical CUDA device which is already used for rendering. */

    /* TODO(sergey): Choose fastest device for denoising. */

    return device_info;
  }

  DeviceInfo none_device;
  none_device.type = DEVICE_NONE;
  return none_device;
}

static unique_ptr<Device> create_denoiser_device(Device *path_trace_device,
                                                 const uint device_type_mask,
                                                 DenoiserType denoiser_type)
{
  const vector<DeviceInfo> device_infos = Device::available_devices(device_type_mask);
  if (device_infos.empty()) {
    return nullptr;
  }

  const DeviceInfo denoiser_device_info = find_best_denoiser_device_info(device_infos,
                                                                         denoiser_type);
  if (denoiser_device_info.type == DEVICE_NONE) {
    return nullptr;
  }

  unique_ptr<Device> denoiser_device(
      Device::create(denoiser_device_info, path_trace_device->stats, path_trace_device->profiler));

  if (!denoiser_device) {
    return nullptr;
  }

  if (denoiser_device->have_error()) {
    return nullptr;
  }

  /* Only need denoising feature, everything else is unused. */
  if (!denoiser_device->load_kernels(KERNEL_FEATURE_DENOISING)) {
    return nullptr;
  }

  return denoiser_device;
}

Device *Denoiser::ensure_denoiser_device(Progress *progress)
{
  /* The best device has been found already, avoid sequential lookups.
   * Additionally, avoid device re-creation if it has failed once. */
  if (denoiser_device_ || device_creation_attempted_) {
    return denoiser_device_;
  }

  /* Simple case: rendering happens on a single device which also supports denoiser. */
  if (is_single_supported_device(path_trace_device_, params_.type)) {
    denoiser_device_ = path_trace_device_;
    return denoiser_device_;
  }

  /* Find best device from the ones which are already used for rendering. */
  denoiser_device_ = find_best_device(path_trace_device_, params_.type);
  if (denoiser_device_) {
    return denoiser_device_;
  }

  if (progress) {
    progress->set_status("Loading denoising kernels (may take a few minutes the first time)");
  }

  device_creation_attempted_ = true;

  const uint device_type_mask = get_device_type_mask();
  local_denoiser_device_ = create_denoiser_device(
      path_trace_device_, device_type_mask, params_.type);
  denoiser_device_ = local_denoiser_device_.get();

  return denoiser_device_;
}

CCL_NAMESPACE_END
