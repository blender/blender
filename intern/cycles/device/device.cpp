/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <string.h>

#include "bvh/bvh2.h"

#include "device/device.h"
#include "device/queue.h"

#include "device/cpu/device.h"
#include "device/cpu/kernel.h"
#include "device/cuda/device.h"
#include "device/dummy/device.h"
#include "device/hip/device.h"
#include "device/metal/device.h"
#include "device/multi/device.h"
#include "device/optix/device.h"

#include "util/foreach.h"
#include "util/half.h"
#include "util/log.h"
#include "util/math.h"
#include "util/string.h"
#include "util/system.h"
#include "util/task.h"
#include "util/time.h"
#include "util/types.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

bool Device::need_types_update = true;
bool Device::need_devices_update = true;
thread_mutex Device::device_mutex;
vector<DeviceInfo> Device::cuda_devices;
vector<DeviceInfo> Device::optix_devices;
vector<DeviceInfo> Device::cpu_devices;
vector<DeviceInfo> Device::hip_devices;
vector<DeviceInfo> Device::metal_devices;
uint Device::devices_initialized_mask = 0;

/* Device */

Device::~Device() noexcept(false)
{
}

void Device::build_bvh(BVH *bvh, Progress &progress, bool refit)
{
  assert(bvh->params.bvh_layout == BVH_LAYOUT_BVH2);

  BVH2 *const bvh2 = static_cast<BVH2 *>(bvh);
  if (refit) {
    bvh2->refit(progress);
  }
  else {
    bvh2->build(progress, &stats);
  }
}

Device *Device::create(const DeviceInfo &info, Stats &stats, Profiler &profiler)
{
  if (!info.multi_devices.empty()) {
    /* Always create a multi device when info contains multiple devices.
     * This is done so that the type can still be e.g. DEVICE_CPU to indicate
     * that it is a homogeneous collection of devices, which simplifies checks. */
    return device_multi_create(info, stats, profiler);
  }

  Device *device = NULL;

  switch (info.type) {
    case DEVICE_CPU:
      device = device_cpu_create(info, stats, profiler);
      break;
#ifdef WITH_CUDA
    case DEVICE_CUDA:
      if (device_cuda_init())
        device = device_cuda_create(info, stats, profiler);
      break;
#endif
#ifdef WITH_OPTIX
    case DEVICE_OPTIX:
      if (device_optix_init())
        device = device_optix_create(info, stats, profiler);
      break;
#endif

#ifdef WITH_HIP
    case DEVICE_HIP:
      if (device_hip_init())
        device = device_hip_create(info, stats, profiler);
      break;
#endif

#ifdef WITH_METAL
    case DEVICE_METAL:
      if (device_metal_init())
        device = device_metal_create(info, stats, profiler);
      break;
#endif
    default:
      break;
  }

  if (device == NULL) {
    device = device_dummy_create(info, stats, profiler);
  }

  return device;
}

DeviceType Device::type_from_string(const char *name)
{
  if (strcmp(name, "CPU") == 0)
    return DEVICE_CPU;
  else if (strcmp(name, "CUDA") == 0)
    return DEVICE_CUDA;
  else if (strcmp(name, "OPTIX") == 0)
    return DEVICE_OPTIX;
  else if (strcmp(name, "MULTI") == 0)
    return DEVICE_MULTI;
  else if (strcmp(name, "HIP") == 0)
    return DEVICE_HIP;
  else if (strcmp(name, "METAL") == 0)
    return DEVICE_METAL;

  return DEVICE_NONE;
}

string Device::string_from_type(DeviceType type)
{
  if (type == DEVICE_CPU)
    return "CPU";
  else if (type == DEVICE_CUDA)
    return "CUDA";
  else if (type == DEVICE_OPTIX)
    return "OPTIX";
  else if (type == DEVICE_MULTI)
    return "MULTI";
  else if (type == DEVICE_HIP)
    return "HIP";
  else if (type == DEVICE_METAL)
    return "METAL";

  return "";
}

vector<DeviceType> Device::available_types()
{
  vector<DeviceType> types;
  types.push_back(DEVICE_CPU);
#ifdef WITH_CUDA
  types.push_back(DEVICE_CUDA);
#endif
#ifdef WITH_OPTIX
  types.push_back(DEVICE_OPTIX);
#endif
#ifdef WITH_HIP
  types.push_back(DEVICE_HIP);
#endif
#ifdef WITH_METAL
  types.push_back(DEVICE_METAL);
#endif
  return types;
}

vector<DeviceInfo> Device::available_devices(uint mask)
{
  /* Lazy initialize devices. On some platforms OpenCL or CUDA drivers can
   * be broken and cause crashes when only trying to get device info, so
   * we don't want to do any initialization until the user chooses to. */
  thread_scoped_lock lock(device_mutex);
  vector<DeviceInfo> devices;

#if defined(WITH_CUDA) || defined(WITH_OPTIX)
  if (mask & (DEVICE_MASK_CUDA | DEVICE_MASK_OPTIX)) {
    if (!(devices_initialized_mask & DEVICE_MASK_CUDA)) {
      if (device_cuda_init()) {
        device_cuda_info(cuda_devices);
      }
      devices_initialized_mask |= DEVICE_MASK_CUDA;
    }
    if (mask & DEVICE_MASK_CUDA) {
      foreach (DeviceInfo &info, cuda_devices) {
        devices.push_back(info);
      }
    }
  }
#endif

#ifdef WITH_OPTIX
  if (mask & DEVICE_MASK_OPTIX) {
    if (!(devices_initialized_mask & DEVICE_MASK_OPTIX)) {
      if (device_optix_init()) {
        device_optix_info(cuda_devices, optix_devices);
      }
      devices_initialized_mask |= DEVICE_MASK_OPTIX;
    }
    foreach (DeviceInfo &info, optix_devices) {
      devices.push_back(info);
    }
  }
#endif

#ifdef WITH_HIP
  if (mask & DEVICE_MASK_HIP) {
    if (!(devices_initialized_mask & DEVICE_MASK_HIP)) {
      if (device_hip_init()) {
        device_hip_info(hip_devices);
      }
      devices_initialized_mask |= DEVICE_MASK_HIP;
    }
    foreach (DeviceInfo &info, hip_devices) {
      devices.push_back(info);
    }
  }
#endif

  if (mask & DEVICE_MASK_CPU) {
    if (!(devices_initialized_mask & DEVICE_MASK_CPU)) {
      device_cpu_info(cpu_devices);
      devices_initialized_mask |= DEVICE_MASK_CPU;
    }
    foreach (DeviceInfo &info, cpu_devices) {
      devices.push_back(info);
    }
  }

#ifdef WITH_METAL
  if (mask & DEVICE_MASK_METAL) {
    if (!(devices_initialized_mask & DEVICE_MASK_METAL)) {
      if (device_metal_init()) {
        device_metal_info(metal_devices);
      }
      devices_initialized_mask |= DEVICE_MASK_METAL;
    }
    foreach (DeviceInfo &info, metal_devices) {
      devices.push_back(info);
    }
  }
#endif

  return devices;
}

DeviceInfo Device::dummy_device(const string &error_msg)
{
  DeviceInfo info;
  info.type = DEVICE_DUMMY;
  info.error_msg = error_msg;
  return info;
}

string Device::device_capabilities(uint mask)
{
  thread_scoped_lock lock(device_mutex);
  string capabilities = "";

  if (mask & DEVICE_MASK_CPU) {
    capabilities += "\nCPU device capabilities: ";
    capabilities += device_cpu_capabilities() + "\n";
  }

#ifdef WITH_CUDA
  if (mask & DEVICE_MASK_CUDA) {
    if (device_cuda_init()) {
      capabilities += "\nCUDA device capabilities:\n";
      capabilities += device_cuda_capabilities();
    }
  }
#endif

#ifdef WITH_HIP
  if (mask & DEVICE_MASK_HIP) {
    if (device_hip_init()) {
      capabilities += "\nHIP device capabilities:\n";
      capabilities += device_hip_capabilities();
    }
  }
#endif

#ifdef WITH_METAL
  if (mask & DEVICE_MASK_METAL) {
    if (device_metal_init()) {
      capabilities += "\nMetal device capabilities:\n";
      capabilities += device_metal_capabilities();
    }
  }
#endif

  return capabilities;
}

DeviceInfo Device::get_multi_device(const vector<DeviceInfo> &subdevices,
                                    int threads,
                                    bool background)
{
  assert(subdevices.size() > 0);

  if (subdevices.size() == 1) {
    /* No multi device needed. */
    return subdevices.front();
  }

  DeviceInfo info;
  info.type = DEVICE_NONE;
  info.id = "MULTI";
  info.description = "Multi Device";
  info.num = 0;

  info.has_nanovdb = true;
  info.has_osl = true;
  info.has_profiling = true;
  info.has_peer_memory = false;
  info.denoisers = DENOISER_ALL;

  foreach (const DeviceInfo &device, subdevices) {
    /* Ensure CPU device does not slow down GPU. */
    if (device.type == DEVICE_CPU && subdevices.size() > 1) {
      if (background) {
        int orig_cpu_threads = (threads) ? threads : TaskScheduler::max_concurrency();
        int cpu_threads = max(orig_cpu_threads - (subdevices.size() - 1), 0);

        VLOG(1) << "CPU render threads reduced from " << orig_cpu_threads << " to " << cpu_threads
                << ", to dedicate to GPU.";

        if (cpu_threads >= 1) {
          DeviceInfo cpu_device = device;
          cpu_device.cpu_threads = cpu_threads;
          info.multi_devices.push_back(cpu_device);
        }
        else {
          continue;
        }
      }
      else {
        VLOG(1) << "CPU render threads disabled for interactive render.";
        continue;
      }
    }
    else {
      info.multi_devices.push_back(device);
    }

    /* Create unique ID for this combination of devices. */
    info.id += device.id;

    /* Set device type to MULTI if subdevices are not of a common type. */
    if (info.type == DEVICE_NONE) {
      info.type = device.type;
    }
    else if (device.type != info.type) {
      info.type = DEVICE_MULTI;
    }

    /* Accumulate device info. */
    info.has_nanovdb &= device.has_nanovdb;
    info.has_osl &= device.has_osl;
    info.has_profiling &= device.has_profiling;
    info.has_peer_memory |= device.has_peer_memory;
    info.denoisers &= device.denoisers;
  }

  return info;
}

void Device::tag_update()
{
  free_memory();
}

void Device::free_memory()
{
  devices_initialized_mask = 0;
  cuda_devices.free_memory();
  optix_devices.free_memory();
  hip_devices.free_memory();
  cpu_devices.free_memory();
  metal_devices.free_memory();
}

unique_ptr<DeviceQueue> Device::gpu_queue_create()
{
  LOG(FATAL) << "Device does not support queues.";
  return nullptr;
}

const CPUKernels &Device::get_cpu_kernels()
{
  /* Initialize CPU kernels once and reuse. */
  static CPUKernels kernels;
  return kernels;
}

void Device::get_cpu_kernel_thread_globals(
    vector<CPUKernelThreadGlobals> & /*kernel_thread_globals*/)
{
  LOG(FATAL) << "Device does not support CPU kernels.";
}

void *Device::get_cpu_osl_memory()
{
  return nullptr;
}

/* DeviceInfo */

CCL_NAMESPACE_END
