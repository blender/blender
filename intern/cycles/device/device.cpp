/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

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
#include "device/hiprt/device_impl.h"
#include "device/metal/device.h"
#include "device/multi/device.h"
#include "device/oneapi/device.h"
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
vector<DeviceInfo> Device::oneapi_devices;
uint Device::devices_initialized_mask = 0;

/* Device */

Device::~Device() noexcept(false) {}

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

#ifdef WITH_ONEAPI
    case DEVICE_ONEAPI:
      device = device_oneapi_create(info, stats, profiler);
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
  else if (strcmp(name, "ONEAPI") == 0)
    return DEVICE_ONEAPI;
  else if (strcmp(name, "HIPRT") == 0)
    return DEVICE_HIPRT;

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
  else if (type == DEVICE_ONEAPI)
    return "ONEAPI";
  else if (type == DEVICE_HIPRT)
    return "HIPRT";

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
#ifdef WITH_ONEAPI
  types.push_back(DEVICE_ONEAPI);
#endif
#ifdef WITH_HIPRT
  if (hiprtewInit())
    types.push_back(DEVICE_HIPRT);
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

#ifdef WITH_ONEAPI
  if (mask & DEVICE_MASK_ONEAPI) {
    if (!(devices_initialized_mask & DEVICE_MASK_ONEAPI)) {
      if (device_oneapi_init()) {
        device_oneapi_info(oneapi_devices);
      }
      devices_initialized_mask |= DEVICE_MASK_ONEAPI;
    }
    foreach (DeviceInfo &info, oneapi_devices) {
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

#ifdef WITH_ONEAPI
  if (mask & DEVICE_MASK_ONEAPI) {
    if (device_oneapi_init()) {
      capabilities += "\noneAPI device capabilities:\n";
      capabilities += device_oneapi_capabilities();
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
  info.has_light_tree = true;
  info.has_mnee = true;
  info.has_osl = true;
  info.has_guiding = true;
  info.has_profiling = true;
  info.has_peer_memory = false;
  info.use_hardware_raytracing = false;
  info.denoisers = DENOISER_ALL;

  foreach (const DeviceInfo &device, subdevices) {
    /* Ensure CPU device does not slow down GPU. */
    if (device.type == DEVICE_CPU && subdevices.size() > 1) {
      if (background) {
        int orig_cpu_threads = (threads) ? threads : TaskScheduler::max_concurrency();
        int cpu_threads = max(orig_cpu_threads - (subdevices.size() - 1), size_t(0));

        VLOG_INFO << "CPU render threads reduced from " << orig_cpu_threads << " to "
                  << cpu_threads << ", to dedicate to GPU.";

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
        VLOG_INFO << "CPU render threads disabled for interactive render.";
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
    info.has_light_tree &= device.has_light_tree;
    info.has_mnee &= device.has_mnee;
    info.has_osl &= device.has_osl;
    info.has_guiding &= device.has_guiding;
    info.has_profiling &= device.has_profiling;
    info.has_peer_memory |= device.has_peer_memory;
    info.use_hardware_raytracing |= device.use_hardware_raytracing;
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
  oneapi_devices.free_memory();
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

GPUDevice::~GPUDevice() noexcept(false) {}

bool GPUDevice::load_texture_info()
{
  if (need_texture_info) {
    /* Unset flag before copying, so this does not loop indefinitely if the copy below calls
     * into 'move_textures_to_host' (which calls 'load_texture_info' again). */
    need_texture_info = false;
    texture_info.copy_to_device();
    return true;
  }
  else {
    return false;
  }
}

void GPUDevice::init_host_memory(size_t preferred_texture_headroom,
                                 size_t preferred_working_headroom)
{
  /* Limit amount of host mapped memory, because allocating too much can
   * cause system instability. Leave at least half or 4 GB of system
   * memory free, whichever is smaller. */
  size_t default_limit = 4 * 1024 * 1024 * 1024LL;
  size_t system_ram = system_physical_ram();

  if (system_ram > 0) {
    if (system_ram / 2 > default_limit) {
      map_host_limit = system_ram - default_limit;
    }
    else {
      map_host_limit = system_ram / 2;
    }
  }
  else {
    VLOG_WARNING << "Mapped host memory disabled, failed to get system RAM";
    map_host_limit = 0;
  }

  /* Amount of device memory to keep free after texture memory
   * and working memory allocations respectively. We set the working
   * memory limit headroom lower than the working one so there
   * is space left for it. */
  device_working_headroom = preferred_working_headroom > 0 ? preferred_working_headroom :
                                                             32 * 1024 * 1024LL;  // 32MB
  device_texture_headroom = preferred_texture_headroom > 0 ? preferred_texture_headroom :
                                                             128 * 1024 * 1024LL;  // 128MB

  VLOG_INFO << "Mapped host memory limit set to " << string_human_readable_number(map_host_limit)
            << " bytes. (" << string_human_readable_size(map_host_limit) << ")";
}

void GPUDevice::move_textures_to_host(size_t size, bool for_texture)
{
  /* Break out of recursive call, which can happen when moving memory on a multi device. */
  static bool any_device_moving_textures_to_host = false;
  if (any_device_moving_textures_to_host) {
    return;
  }

  /* Signal to reallocate textures in host memory only. */
  move_texture_to_host = true;

  while (size > 0) {
    /* Find suitable memory allocation to move. */
    device_memory *max_mem = NULL;
    size_t max_size = 0;
    bool max_is_image = false;

    thread_scoped_lock lock(device_mem_map_mutex);
    foreach (MemMap::value_type &pair, device_mem_map) {
      device_memory &mem = *pair.first;
      Mem *cmem = &pair.second;

      /* Can only move textures allocated on this device (and not those from peer devices).
       * And need to ignore memory that is already on the host. */
      if (!mem.is_resident(this) || cmem->use_mapped_host) {
        continue;
      }

      bool is_texture = (mem.type == MEM_TEXTURE || mem.type == MEM_GLOBAL) &&
                        (&mem != &texture_info);
      bool is_image = is_texture && (mem.data_height > 1);

      /* Can't move this type of memory. */
      if (!is_texture || cmem->array) {
        continue;
      }

      /* For other textures, only move image textures. */
      if (for_texture && !is_image) {
        continue;
      }

      /* Try to move largest allocation, prefer moving images. */
      if (is_image > max_is_image || (is_image == max_is_image && mem.device_size > max_size)) {
        max_is_image = is_image;
        max_size = mem.device_size;
        max_mem = &mem;
      }
    }
    lock.unlock();

    /* Move to host memory. This part is mutex protected since
     * multiple backend devices could be moving the memory. The
     * first one will do it, and the rest will adopt the pointer. */
    if (max_mem) {
      VLOG_WORK << "Move memory from device to host: " << max_mem->name;

      static thread_mutex move_mutex;
      thread_scoped_lock lock(move_mutex);

      any_device_moving_textures_to_host = true;

      /* Potentially need to call back into multi device, so pointer mapping
       * and peer devices are updated. This is also necessary since the device
       * pointer may just be a key here, so cannot be accessed and freed directly.
       * Unfortunately it does mean that memory is reallocated on all other
       * devices as well, which is potentially dangerous when still in use (since
       * a thread rendering on another devices would only be caught in this mutex
       * if it so happens to do an allocation at the same time as well. */
      max_mem->device_copy_to();
      size = (max_size >= size) ? 0 : size - max_size;

      any_device_moving_textures_to_host = false;
    }
    else {
      break;
    }
  }

  /* Unset flag before texture info is reloaded, since it should stay in device memory. */
  move_texture_to_host = false;

  /* Update texture info array with new pointers. */
  load_texture_info();
}

GPUDevice::Mem *GPUDevice::generic_alloc(device_memory &mem, size_t pitch_padding)
{
  void *device_pointer = 0;
  size_t size = mem.memory_size() + pitch_padding;

  bool mem_alloc_result = false;
  const char *status = "";

  /* First try allocating in device memory, respecting headroom. We make
   * an exception for texture info. It is small and frequently accessed,
   * so treat it as working memory.
   *
   * If there is not enough room for working memory, we will try to move
   * textures to host memory, assuming the performance impact would have
   * been worse for working memory. */
  bool is_texture = (mem.type == MEM_TEXTURE || mem.type == MEM_GLOBAL) && (&mem != &texture_info);
  bool is_image = is_texture && (mem.data_height > 1);

  size_t headroom = (is_texture) ? device_texture_headroom : device_working_headroom;

  size_t total = 0, free = 0;
  get_device_memory_info(total, free);

  /* Move textures to host memory if needed. */
  if (!move_texture_to_host && !is_image && (size + headroom) >= free && can_map_host) {
    move_textures_to_host(size + headroom - free, is_texture);
    get_device_memory_info(total, free);
  }

  /* Allocate in device memory. */
  if (!move_texture_to_host && (size + headroom) < free) {
    mem_alloc_result = alloc_device(device_pointer, size);
    if (mem_alloc_result) {
      device_mem_in_use += size;
      status = " in device memory";
    }
  }

  /* Fall back to mapped host memory if needed and possible. */

  void *shared_pointer = 0;

  if (!mem_alloc_result && can_map_host && mem.type != MEM_DEVICE_ONLY) {
    if (mem.shared_pointer) {
      /* Another device already allocated host memory. */
      mem_alloc_result = true;
      shared_pointer = mem.shared_pointer;
    }
    else if (map_host_used + size < map_host_limit) {
      /* Allocate host memory ourselves. */
      mem_alloc_result = alloc_host(shared_pointer, size);

      assert((mem_alloc_result && shared_pointer != 0) ||
             (!mem_alloc_result && shared_pointer == 0));
    }

    if (mem_alloc_result) {
      transform_host_pointer(device_pointer, shared_pointer);
      map_host_used += size;
      status = " in host memory";
    }
  }

  if (!mem_alloc_result) {
    if (mem.type == MEM_DEVICE_ONLY) {
      status = " failed, out of device memory";
      set_error("System is out of GPU memory");
    }
    else {
      status = " failed, out of device and host memory";
      set_error("System is out of GPU and shared host memory");
    }
  }

  if (mem.name) {
    VLOG_WORK << "Buffer allocate: " << mem.name << ", "
              << string_human_readable_number(mem.memory_size()) << " bytes. ("
              << string_human_readable_size(mem.memory_size()) << ")" << status;
  }

  mem.device_pointer = (device_ptr)device_pointer;
  mem.device_size = size;
  stats.mem_alloc(size);

  if (!mem.device_pointer) {
    return NULL;
  }

  /* Insert into map of allocations. */
  thread_scoped_lock lock(device_mem_map_mutex);
  Mem *cmem = &device_mem_map[&mem];
  if (shared_pointer != 0) {
    /* Replace host pointer with our host allocation. Only works if
     * memory layout is the same and has no pitch padding. Also
     * does not work if we move textures to host during a render,
     * since other devices might be using the memory. */

    if (!move_texture_to_host && pitch_padding == 0 && mem.host_pointer &&
        mem.host_pointer != shared_pointer)
    {
      memcpy(shared_pointer, mem.host_pointer, size);

      /* A Call to device_memory::host_free() should be preceded by
       * a call to device_memory::device_free() for host memory
       * allocated by a device to be handled properly. Two exceptions
       * are here and a call in OptiXDevice::generic_alloc(), where
       * the current host memory can be assumed to be allocated by
       * device_memory::host_alloc(), not by a device */

      mem.host_free();
      mem.host_pointer = shared_pointer;
    }
    mem.shared_pointer = shared_pointer;
    mem.shared_counter++;
    cmem->use_mapped_host = true;
  }
  else {
    cmem->use_mapped_host = false;
  }

  return cmem;
}

void GPUDevice::generic_free(device_memory &mem)
{
  if (mem.device_pointer) {
    thread_scoped_lock lock(device_mem_map_mutex);
    DCHECK(device_mem_map.find(&mem) != device_mem_map.end());
    const Mem &cmem = device_mem_map[&mem];

    /* If cmem.use_mapped_host is true, reference counting is used
     * to safely free a mapped host memory. */

    if (cmem.use_mapped_host) {
      assert(mem.shared_pointer);
      if (mem.shared_pointer) {
        assert(mem.shared_counter > 0);
        if (--mem.shared_counter == 0) {
          if (mem.host_pointer == mem.shared_pointer) {
            mem.host_pointer = 0;
          }
          free_host(mem.shared_pointer);
          mem.shared_pointer = 0;
        }
      }
      map_host_used -= mem.device_size;
    }
    else {
      /* Free device memory. */
      free_device((void *)mem.device_pointer);
      device_mem_in_use -= mem.device_size;
    }

    stats.mem_free(mem.device_size);
    mem.device_pointer = 0;
    mem.device_size = 0;

    device_mem_map.erase(device_mem_map.find(&mem));
  }
}

void GPUDevice::generic_copy_to(device_memory &mem)
{
  if (!mem.host_pointer || !mem.device_pointer) {
    return;
  }

  /* If use_mapped_host of mem is false, the current device only uses device memory allocated by
   * backend device allocation regardless of mem.host_pointer and mem.shared_pointer, and should
   * copy data from mem.host_pointer. */
  thread_scoped_lock lock(device_mem_map_mutex);
  if (!device_mem_map[&mem].use_mapped_host || mem.host_pointer != mem.shared_pointer) {
    copy_host_to_device((void *)mem.device_pointer, mem.host_pointer, mem.memory_size());
  }
}

/* DeviceInfo */

CCL_NAMESPACE_END
