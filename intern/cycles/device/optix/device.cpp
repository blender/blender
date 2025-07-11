/* SPDX-FileCopyrightText: 2019 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2019-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/optix/device.h"
#include "device/cuda/device.h"
#include "device/device.h"

#ifdef WITH_OSL
#  include <OSL/oslconfig.h>
#  include <OSL/oslversion.h>
#endif

#ifdef WITH_OPTIX
#  include "device/optix/device_impl.h"

#  include "integrator/denoiser_oidn_gpu.h"  // IWYU pragma: keep

#  include <optix_function_table_definition.h>
#endif

#include "util/log.h"

#ifndef OPTIX_FUNCTION_TABLE_SYMBOL
#  define OPTIX_FUNCTION_TABLE_SYMBOL g_optixFunctionTable
#endif

CCL_NAMESPACE_BEGIN

bool device_optix_init()
{
#ifdef WITH_OPTIX
  if (OPTIX_FUNCTION_TABLE_SYMBOL.optixDeviceContextCreate != nullptr) {
    /* Already initialized function table. */
    return true;
  }

  /* Need to initialize CUDA as well. */
  if (!device_cuda_init()) {
    return false;
  }

  const OptixResult result = optixInit();

  if (result == OPTIX_ERROR_UNSUPPORTED_ABI_VERSION) {
    LOG_WARNING << "OptiX initialization failed because the installed NVIDIA driver is too old. "
                   "Please update to the latest driver first!";
    return false;
  }
  if (result != OPTIX_SUCCESS) {
    LOG_WARNING << "OptiX initialization failed with error code " << (unsigned int)result;
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
#  if defined(WITH_OSL) && defined(OSL_USE_OPTIX) && \
      (OSL_VERSION_MINOR >= 13 || OSL_VERSION_MAJOR > 1)
    info.has_osl = true;
#  endif
    info.denoisers |= DENOISER_OPTIX;
#  if defined(WITH_OPENIMAGEDENOISE)
#    if OIDN_VERSION >= 20300
    if (oidnIsCUDADeviceSupported(info.num)) {
#    else
    if (OIDNDenoiserGPU::is_device_supported(info)) {
#    endif
      info.denoisers |= DENOISER_OPENIMAGEDENOISE;
    }
#  endif

    devices.push_back(info);
  }
#else
  (void)cuda_devices;
  (void)devices;
#endif
}

unique_ptr<Device> device_optix_create(const DeviceInfo &info,
                                       Stats &stats,
                                       Profiler &profiler,
                                       bool headless)
{
#ifdef WITH_OPTIX
  return make_unique<OptiXDevice>(info, stats, profiler, headless);
#else
  (void)info;
  (void)stats;
  (void)profiler;
  (void)headless;

  LOG_FATAL << "Request to create OptiX device without compiled-in support. Should never happen.";

  return nullptr;
#endif
}

CCL_NAMESPACE_END
