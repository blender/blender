/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Intel Corporation */

#include "device/oneapi/device.h"

#include "util/log.h"

#ifdef WITH_ONEAPI
#  include "device/device.h"
#  include "device/oneapi/device_impl.h"

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
#  ifdef _WIN32
  if (getenv("SYCL_CACHE_PERSISTENT") == nullptr) {
    _putenv_s("SYCL_CACHE_PERSISTENT", "1");
  }
  if (getenv("SYCL_CACHE_TRESHOLD") == nullptr) {
    _putenv_s("SYCL_CACHE_THRESHOLD", "0");
  }
  if (getenv("SYCL_DEVICE_FILTER") == nullptr) {
    _putenv_s("SYCL_DEVICE_FILTER", "host,level_zero");
  }
  if (getenv("SYCL_ENABLE_PCI") == nullptr) {
    _putenv_s("SYCL_ENABLE_PCI", "1");
  }
  if (getenv("SYCL_PI_LEVEL_ZERO_USE_COPY_ENGINE_FOR_IN_ORDER_QUEUE") == nullptr) {
    _putenv_s("SYCL_PI_LEVEL_ZERO_USE_COPY_ENGINE_FOR_IN_ORDER_QUEUE", "0");
  }
#  elif __linux__
  setenv("SYCL_CACHE_PERSISTENT", "1", false);
  setenv("SYCL_CACHE_THRESHOLD", "0", false);
  setenv("SYCL_DEVICE_FILTER", "host,level_zero", false);
  setenv("SYCL_ENABLE_PCI", "1", false);
  setenv("SYCL_PI_LEVEL_ZERO_USE_COPY_ENGINE_FOR_IN_ORDER_QUEUE", "0", false);
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
static void device_iterator_cb(const char *id, const char *name, int num, void *user_ptr)
{
  vector<DeviceInfo> *devices = (vector<DeviceInfo> *)user_ptr;

  DeviceInfo info;

  info.type = DEVICE_ONEAPI;
  info.description = name;
  info.num = num;

  /* NOTE(@nsirgien): Should be unique at least on proper oneapi installation. */
  info.id = id;

  info.has_nanovdb = true;
  info.denoisers = 0;

  info.has_gpu_queue = true;

  /* NOTE(@nsirgien): oneAPI right now is focused on one device usage. In future it maybe will
   * change, but right now peer access from one device to another device is not supported. */
  info.has_peer_memory = false;

  /* NOTE(@nsirgien): Seems not possible to know from SYCL/oneAPI or Level0. */
  info.display_device = false;

  devices->push_back(info);
  VLOG_INFO << "Added device \"" << name << "\" with id \"" << info.id << "\".";
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
