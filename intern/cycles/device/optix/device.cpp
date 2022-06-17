/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2019, NVIDIA Corporation.
 * Copyright 2019-2022 Blender Foundation. */

#include "device/optix/device.h"

#include "device/cuda/device.h"
#include "device/optix/device_impl.h"

#include "util/log.h"

#ifdef WITH_OPTIX
#  include <optix_function_table_definition.h>
#endif

CCL_NAMESPACE_BEGIN

bool device_optix_init()
{
#ifdef WITH_OPTIX
  if (g_optixFunctionTable.optixDeviceContextCreate != NULL) {
    /* Already initialized function table. */
    return true;
  }

  /* Need to initialize CUDA as well. */
  if (!device_cuda_init()) {
    return false;
  }

  const OptixResult result = optixInit();

  if (result == OPTIX_ERROR_UNSUPPORTED_ABI_VERSION) {
    VLOG_WARNING << "OptiX initialization failed because the installed NVIDIA driver is too old. "
                    "Please update to the latest driver first!";
    return false;
  }
  else if (result != OPTIX_SUCCESS) {
    VLOG_WARNING << "OptiX initialization failed with error code " << (unsigned int)result;
    return false;
  }

  /* Loaded OptiX successfully! */
  return true;
#else
  return false;
#endif
}

void device_optix_info(const vector<DeviceInfo> &cuda_devices, vector<DeviceInfo> &devices)
{
#ifdef WITH_OPTIX
  devices.reserve(cuda_devices.size());

  /* Simply add all supported CUDA devices as OptiX devices again. */
  for (DeviceInfo info : cuda_devices) {
    assert(info.type == DEVICE_CUDA);

    int major;
    cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, info.num);
    if (major < 5) {
      /* Only Maxwell and up are supported by OptiX. */
      continue;
    }

    info.type = DEVICE_OPTIX;
    info.id += "_OptiX";
    info.denoisers |= DENOISER_OPTIX;

    devices.push_back(info);
  }
#else
  (void)cuda_devices;
  (void)devices;
#endif
}

Device *device_optix_create(const DeviceInfo &info, Stats &stats, Profiler &profiler)
{
#ifdef WITH_OPTIX
  return new OptiXDevice(info, stats, profiler);
#else
  (void)info;
  (void)stats;
  (void)profiler;

  LOG(FATAL) << "Request to create OptiX device without compiled-in support. Should never happen.";

  return nullptr;
#endif
}

CCL_NAMESPACE_END
