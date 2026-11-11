/* SPDX-FileCopyrightText: 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/oneapi/device.h"
#include "device/device.h"

#include "util/log.h"

#ifdef WITH_ONEAPI
#  include "device/oneapi/device_impl.h"
#  include "integrator/denoiser_oidn_gpu.h"  // IWYU pragma: keep

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

  /* Set an environment variable only if it isn't already set, so values
   * provided by end-users are never overwritten. */
  auto set_env_default = [](const char *name, const char *value) {
#  ifdef _WIN32
    if (getenv(name) == nullptr) {
      _putenv_s(name, value);
    }
#  else
    setenv(name, value, false);
#  endif
  };

  /* NOTE(@nsirgien): we need to enable JIT cache from here and
   * right now this cache policy is controlled by env. variables. */
  set_env_default("SYCL_CACHE_PERSISTENT", "1");
  set_env_default("SYCL_CACHE_THRESHOLD", "0");

  /* Enable only Level-Zero by default and if all devices are allowed,
   * everything but OpenCL, which isn't currently well supported. */
  set_env_default("ONEAPI_DEVICE_SELECTOR",
                  getenv("CYCLES_ONEAPI_ALL_DEVICES") == nullptr ? "level_zero:*" : "!opencl:*");

  /* SYSMAN is needed for free_memory queries. */
  set_env_default("ZES_ENABLE_SYSMAN", "1");

  /* NOTE(@xavierh-intel): we enable the use of copy engine, incl. for fill
   * operations as it lowers the overhead from zeCommandListAppendMemoryFill
   * when running paths_array kernels on Linux+A750. */
  set_env_default("UR_L0_USE_COPY_ENGINE", "1");
  set_env_default("UR_L0_USE_COPY_ENGINE_FOR_FILL", "1");

  /* As a fix for #159584, the v1 Level Zero adapter is not included. */
  set_env_default("SYCL_UR_USE_LEVEL_ZERO_V2", "1");

  /* Ask the driver to refuse device memory allocations beyond capacity. */
  set_env_default("NEO_LOCAL_MEMORY_ALLOCATION_MODE", "1");

  return true;
#endif
}

unique_ptr<Device> device_oneapi_create(const DeviceInfo &info,
                                        Stats &stats,
                                        Profiler &profiler,
                                        bool headless)
{
#ifdef WITH_ONEAPI
  return make_unique<OneapiDevice>(info, stats, profiler, headless);
#else
  (void)info;
  (void)stats;
  (void)profiler;
  (void)headless;

  LOG_FATAL << "Requested to create oneAPI device while not enabled for this build.";

  return nullptr;
#endif
}

#ifdef WITH_ONEAPI
static void device_iterator_cb(const char *id,
                               const char *name,
                               const int num,
                               bool hwrt_support,
                               bool oidn_support,
                               bool has_execution_optimization,
                               bool meets_driver_requirement,
                               void *user_ptr)
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
#    if OIDN_VERSION >= 20300
  if (oidn_support) {
#    else
  if (OIDNDenoiserGPU::is_device_supported(info)) {
#    endif
    info.denoisers |= DENOISER_OPENIMAGEDENOISE;
  }
#  endif
  (void)oidn_support;

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

  info.has_execution_optimization = has_execution_optimization;
  info.meets_driver_requirement = meets_driver_requirement;

  devices->push_back(info);
  LOG_INFO << "Added device \"" << info.description << "\" with id \"" << info.id << "\".";

  if (info.denoisers & DENOISER_OPENIMAGEDENOISE) {
    LOG_INFO << "Device with id \"" << info.id << "\" supports "
             << denoiserTypeToHumanReadable(DENOISER_OPENIMAGEDENOISE) << ".";
  }
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
