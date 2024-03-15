/* SPDX-FileCopyrightText: 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/oneapi/device.h"

#include "util/log.h"

#ifdef WITH_ONEAPI
#  include "device/device.h"
#  include "device/oneapi/device_impl.h"
#  include "integrator/denoiser_oidn_gpu.h"

#  include "util/path.h"
#  include "util/string.h"

#  ifdef __linux__
#    include <dlfcn.h>
#  endif
#endif /* WITH_ONEAPI */

CCL_NAMESPACE_BEGIN

bool device_oneapi_init()
{
#if !defined(WITH_ONEAPI)
  return false;
#else

  /* NOTE(@nsirgien): we need to enable JIT cache from here and
   * right now this cache policy is controlled by env. variables. */
  /* NOTE(hallade) we also disable use of copy engine as it
   * improves stability as of intel/LLVM SYCL-nightly/20220529.
   * All these env variable can be set beforehand by end-users and
   * will in that case -not- be overwritten. */
  /* By default, enable only Level-Zero and if all devices are allowed, also CUDA and HIP.
   * OpenCL backend isn't currently well supported. */
#  ifdef _WIN32
  if (getenv("SYCL_CACHE_PERSISTENT") == nullptr) {
    _putenv_s("SYCL_CACHE_PERSISTENT", "1");
  }
  if (getenv("SYCL_CACHE_TRESHOLD") == nullptr) {
    _putenv_s("SYCL_CACHE_THRESHOLD", "0");
  }
  if (getenv("ONEAPI_DEVICE_SELECTOR") == nullptr) {
    if (getenv("CYCLES_ONEAPI_ALL_DEVICES") == nullptr) {
      _putenv_s("ONEAPI_DEVICE_SELECTOR", "level_zero:*");
    }
    else {
      _putenv_s("ONEAPI_DEVICE_SELECTOR", "!opencl:*");
    }
  }
  if (getenv("SYCL_ENABLE_PCI") == nullptr) {
    _putenv_s("SYCL_ENABLE_PCI", "1");
  }
  if (getenv("SYCL_PI_LEVEL_ZERO_USE_COPY_ENGINE_FOR_IN_ORDER_QUEUE") == nullptr) {
    _putenv_s("SYCL_PI_LEVEL_ZERO_USE_COPY_ENGINE_FOR_IN_ORDER_QUEUE", "0");
  }
  if (getenv("SYCL_PI_LEVEL_ZERO_USE_COPY_ENGINE") == nullptr) {
    _putenv_s("SYCL_PI_LEVEL_ZERO_USE_COPY_ENGINE", "0");
  }
#  elif __linux__
  setenv("SYCL_CACHE_PERSISTENT", "1", false);
  setenv("SYCL_CACHE_THRESHOLD", "0", false);
  if (getenv("CYCLES_ONEAPI_ALL_DEVICES") == nullptr) {
    setenv("ONEAPI_DEVICE_SELECTOR", "level_zero:*", false);
  }
  else {
    setenv("ONEAPI_DEVICE_SELECTOR", "!opencl:*", false);
  }
  setenv("SYCL_ENABLE_PCI", "1", false);
  setenv("SYCL_PI_LEVEL_ZERO_USE_COPY_ENGINE_FOR_IN_ORDER_QUEUE", "0", false);
  setenv("SYCL_PI_LEVEL_ZERO_USE_COPY_ENGINE", "0", false);
#  endif

  return true;
#endif
}

Device *device_oneapi_create(const DeviceInfo &info, Stats &stats, Profiler &profiler)
{
#ifdef WITH_ONEAPI
  return new OneapiDevice(info, stats, profiler);
#else
  (void)info;
  (void)stats;
  (void)profiler;

  LOG(FATAL) << "Requested to create oneAPI device while not enabled for this build.";

  return nullptr;
#endif
}

#ifdef WITH_ONEAPI
static void device_iterator_cb(
    const char *id, const char *name, int num, bool hwrt_support, void *user_ptr)
{
  vector<DeviceInfo> *devices = (vector<DeviceInfo> *)user_ptr;

  DeviceInfo info;

  info.type = DEVICE_ONEAPI;
  info.description = name;
  info.num = num;

  /* NOTE(@nsirgien): Should be unique at least on proper oneapi installation. */
  info.id = id;

  info.has_nanovdb = true;
#  if defined(WITH_OPENIMAGEDENOISE)
  if (OIDNDenoiserGPU::is_device_supported(info)) {
    info.denoisers |= DENOISER_OPENIMAGEDENOISE;
  }
#  endif

  info.has_gpu_queue = true;

  /* NOTE(@nsirgien): oneAPI right now is focused on one device usage. In future it maybe will
   * change, but right now peer access from one device to another device is not supported. */
  info.has_peer_memory = false;

  /* NOTE(@nsirgien): Seems not possible to know from SYCL/oneAPI or Level0. */
  info.display_device = false;

#  ifdef WITH_EMBREE_GPU
  info.use_hardware_raytracing = hwrt_support;
#  else
  info.use_hardware_raytracing = false;
  (void)hwrt_support;
#  endif

  devices->push_back(info);
  VLOG_INFO << "Added device \"" << info.description << "\" with id \"" << info.id << "\".";

  if (info.denoisers & DENOISER_OPENIMAGEDENOISE)
    VLOG_INFO << "Device with id \"" << info.id << "\" supports "
              << denoiserTypeToHumanReadable(DENOISER_OPENIMAGEDENOISE) << ".";
}
#endif

void device_oneapi_info(vector<DeviceInfo> &devices)
{
#ifdef WITH_ONEAPI
  OneapiDevice::iterate_devices(device_iterator_cb, &devices);
#else  /* WITH_ONEAPI */
  (void)devices;
#endif /* WITH_ONEAPI */
}

string device_oneapi_capabilities()
{
  string capabilities;
#ifdef WITH_ONEAPI
  char *c_capabilities = OneapiDevice::device_capabilities();
  if (c_capabilities) {
    capabilities = c_capabilities;
    free(c_capabilities);
  }
#endif
  return capabilities;
}

CCL_NAMESPACE_END
