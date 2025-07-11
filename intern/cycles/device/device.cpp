/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <cstdlib>
#include <cstring>

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
#include "device/oneapi/device.h"
#include "device/optix/device.h"

#ifdef WITH_HIPRT
#  include <hiprtew.h>
#endif

#include "util/log.h"
#include "util/math.h"
#include "util/string.h"
#include "util/system.h"
#include "util/task.h"
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

Device::~Device() noexcept(false) = default;

void Device::set_error(const string &error)
{
  if (!have_error()) {
    error_msg = error;
  }
  LOG_ERROR << error;
  fflush(stderr);
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

unique_ptr<Device> Device::create(const DeviceInfo &info,
                                  Stats &stats,
                                  Profiler &profiler,
                                  bool headless)
{
  if (!info.multi_devices.empty()) {
    /* Always create a multi device when info contains multiple devices.
     * This is done so that the type can still be e.g. DEVICE_CPU to indicate
     * that it is a homogeneous collection of devices, which simplifies checks. */
    return device_multi_create(info, stats, profiler, headless);
  }

  unique_ptr<Device> device;

  switch (info.type) {
    case DEVICE_CPU:
      device = device_cpu_create(info, stats, profiler, headless);
      break;
#ifdef WITH_CUDA
    case DEVICE_CUDA:
      if (device_cuda_init()) {
        device = device_cuda_create(info, stats, profiler, headless);
      }
      break;
#endif
#ifdef WITH_OPTIX
    case DEVICE_OPTIX:
      if (device_optix_init()) {
        device = device_optix_create(info, stats, profiler, headless);
      }
      break;
#endif

#ifdef WITH_HIP
    case DEVICE_HIP:
      if (device_hip_init()) {
        device = device_hip_create(info, stats, profiler, headless);
      }
      break;
#endif

#ifdef WITH_METAL
    case DEVICE_METAL:
      if (device_metal_init()) {
        device = device_metal_create(info, stats, profiler, headless);
      }
      break;
#endif

#ifdef WITH_ONEAPI
    case DEVICE_ONEAPI:
      device = device_oneapi_create(info, stats, profiler, headless);
      break;
#endif

    default:
      break;
  }

  if (device == nullptr) {
    device = device_dummy_create(info, stats, profiler, headless);
  }

  return device;
}

DeviceType Device::type_from_string(const char *name)
{
  if (strcmp(name, "CPU") == 0) {
    return DEVICE_CPU;
  }
  if (strcmp(name, "CUDA") == 0) {
    return DEVICE_CUDA;
  }
  if (strcmp(name, "OPTIX") == 0) {
    return DEVICE_OPTIX;
  }
  if (strcmp(name, "MULTI") == 0) {
    return DEVICE_MULTI;
  }
  if (strcmp(name, "HIP") == 0) {
    return DEVICE_HIP;
  }
  if (strcmp(name, "METAL") == 0) {
    return DEVICE_METAL;
  }
  if (strcmp(name, "ONEAPI") == 0) {
    return DEVICE_ONEAPI;
  }
  if (strcmp(name, "HIPRT") == 0) {
    return DEVICE_HIPRT;
  }

  return DEVICE_NONE;
}

string Device::string_from_type(DeviceType type)
{
  if (type == DEVICE_CPU) {
    return "CPU";
  }
  if (type == DEVICE_CUDA) {
    return "CUDA";
  }
  if (type == DEVICE_OPTIX) {
    return "OPTIX";
  }
  if (type == DEVICE_MULTI) {
    return "MULTI";
  }
  if (type == DEVICE_HIP) {
    return "HIP";
  }
  if (type == DEVICE_METAL) {
    return "METAL";
  }
  if (type == DEVICE_ONEAPI) {
    return "ONEAPI";
  }
  if (type == DEVICE_HIPRT) {
    return "HIPRT";
  }

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
  if (hiprtewInit()) {
    types.push_back(DEVICE_HIPRT);
  }
#endif
  return types;
}

vector<DeviceInfo> Device::available_devices(const uint mask)
{
  /* Lazy initialize devices. On some platforms OpenCL or CUDA drivers can
   * be broken and cause crashes when only trying to get device info, so
   * we don't want to do any initialization until the user chooses to. */
  const thread_scoped_lock lock(device_mutex);
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
      for (DeviceInfo &info : cuda_devices) {
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
    for (DeviceInfo &info : optix_devices) {
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
    for (DeviceInfo &info : hip_devices) {
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
    for (DeviceInfo &info : oneapi_devices) {
      devices.push_back(info);
    }
  }
#endif

  if (mask & DEVICE_MASK_CPU) {
    if (!(devices_initialized_mask & DEVICE_MASK_CPU)) {
      device_cpu_info(cpu_devices);
      devices_initialized_mask |= DEVICE_MASK_CPU;
    }
    for (const DeviceInfo &info : cpu_devices) {
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
    for (const DeviceInfo &info : metal_devices) {
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

string Device::device_capabilities(const uint mask)
{
  const thread_scoped_lock lock(device_mutex);
  string capabilities;

  if (mask & DEVICE_MASK_CPU) {
    capabilities += "\nCPU device capabilities: ";
    capabilities += device_cpu_capabilities() + "\n";
  }

#ifdef WITH_CUDA
  if (mask & DEVICE_MASK_CUDA) {
    if (device_cuda_init()) {
      const string device_capabilities = device_cuda_capabilities();
      if (!device_capabilities.empty()) {
        capabilities += "\nCUDA device capabilities:\n";
        capabilities += device_capabilities;
      }
    }
  }
#endif

#ifdef WITH_HIP
  if (mask & DEVICE_MASK_HIP) {
    if (device_hip_init()) {
      const string device_capabilities = device_hip_capabilities();
      if (!device_capabilities.empty()) {
        capabilities += "\nHIP device capabilities:\n";
        capabilities += device_capabilities;
      }
    }
  }
#endif

#ifdef WITH_ONEAPI
  if (mask & DEVICE_MASK_ONEAPI) {
    if (device_oneapi_init()) {
      const string device_capabilities = device_oneapi_capabilities();
      if (!device_capabilities.empty()) {
        capabilities += "\noneAPI device capabilities:\n";
        capabilities += device_capabilities;
      }
    }
  }
#endif

#ifdef WITH_METAL
  if (mask & DEVICE_MASK_METAL) {
    if (device_metal_init()) {
      const string device_capabilities = device_metal_capabilities();
      if (!device_capabilities.empty()) {
        capabilities += "\nMetal device capabilities:\n";
        capabilities += device_capabilities;
      }
    }
  }
#endif

  return capabilities;
}

DeviceInfo Device::get_multi_device(const vector<DeviceInfo> &subdevices,
                                    const int threads,
                                    bool background)
{
  assert(!subdevices.empty());

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
  info.has_mnee = true;
  info.has_osl = true;
  info.has_guiding = true;
  info.has_profiling = true;
  info.has_peer_memory = false;
  info.use_hardware_raytracing = false;
  info.denoisers = DENOISER_ALL;

  for (const DeviceInfo &device : subdevices) {
    /* Ensure CPU device does not slow down GPU. */
    if (device.type == DEVICE_CPU && subdevices.size() > 1) {
      if (background) {
        const int orig_cpu_threads = (threads) ? threads : TaskScheduler::max_concurrency();
        const int cpu_threads = max(orig_cpu_threads - (subdevices.size() - 1), size_t(0));

        LOG_INFO << "CPU render threads reduced from " << orig_cpu_threads << " to " << cpu_threads
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
        LOG_INFO << "CPU render threads disabled for interactive render.";
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
  LOG_FATAL << "Device does not support queues.";
  return nullptr;
}

const CPUKernels &Device::get_cpu_kernels()
{
  /* Initialize CPU kernels once and reuse. */
  static const CPUKernels kernels;
  return kernels;
}

void Device::get_cpu_kernel_thread_globals(
    vector<ThreadKernelGlobalsCPU> & /*kernel_thread_globals*/)
{
  LOG_FATAL << "Device does not support CPU kernels.";
}

OSLGlobals *Device::get_cpu_osl_memory()
{
  return nullptr;
}

void *Device::get_guiding_device() const
{
  LOG_ERROR << "Request guiding field from a device which does not support it.";
  return nullptr;
}

void *Device::host_alloc(const MemoryType /*type*/, const size_t size)
{
  return util_aligned_malloc(size, MIN_ALIGNMENT_DEVICE_MEMORY);
}

void Device::host_free(const MemoryType /*type*/, void *host_pointer, const size_t size)
{
  util_aligned_free(host_pointer, size);
}

GPUDevice::~GPUDevice() noexcept(false) = default;

bool GPUDevice::load_texture_info()
{
  /* Note texture_info is never host mapped, and load_texture_info() should only
   * be called right before kernel enqueue when all memory operations have completed. */
  if (need_texture_info) {
    texture_info.copy_to_device();
    need_texture_info = false;
    return true;
  }
  return false;
}

void GPUDevice::init_host_memory(const size_t preferred_texture_headroom,
                                 const size_t preferred_working_headroom)
{
  /* Limit amount of host mapped memory, because allocating too much can
   * cause system instability. Leave at least half or 4 GB of system
   * memory free, whichever is smaller. */
  const size_t default_limit = 4 * 1024 * 1024 * 1024LL;
  const size_t system_ram = system_physical_ram();

  if (system_ram > 0) {
    if (system_ram / 2 > default_limit) {
      map_host_limit = system_ram - default_limit;
    }
    else {
      map_host_limit = system_ram / 2;
    }
  }
  else {
    LOG_WARNING << "Mapped host memory disabled, failed to get system RAM";
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

  LOG_INFO << "Mapped host memory limit set to " << string_human_readable_number(map_host_limit)
           << " bytes. (" << string_human_readable_size(map_host_limit) << ")";
}

void GPUDevice::move_textures_to_host(size_t size, const size_t headroom, const bool for_texture)
{
  static thread_mutex move_mutex;
  const thread_scoped_lock lock(move_mutex);

  /* Check if there is enough space. Within mutex locks so that multiple threads
   * calling take into account memory freed by another thread. */
  size_t total = 0;
  size_t free = 0;
  get_device_memory_info(total, free);
  if (size + headroom < free) {
    return;
  }

  while (size > 0) {
    /* Find suitable memory allocation to move. */
    device_memory *max_mem = nullptr;
    size_t max_size = 0;
    bool max_is_image = false;

    thread_scoped_lock lock(device_mem_map_mutex);
    for (MemMap::value_type &pair : device_mem_map) {
      device_memory &mem = *pair.first;
      Mem *cmem = &pair.second;

      /* Can only move textures allocated on this device (and not those from peer devices).
       * And need to ignore memory that is already on the host. */
      if (!mem.is_resident(this) || mem.is_shared(this)) {
        continue;
      }

      const bool is_texture = (mem.type == MEM_TEXTURE || mem.type == MEM_GLOBAL) &&
                              (&mem != &texture_info);
      const bool is_image = is_texture && (mem.data_height > 1);

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
      LOG_WORK << "Move memory from device to host: " << max_mem->name;

      /* Potentially need to call back into multi device, so pointer mapping
       * and peer devices are updated. This is also necessary since the device
       * pointer may just be a key here, so cannot be accessed and freed directly.
       * Unfortunately it does mean that memory is reallocated on all other
       * devices as well, which is potentially dangerous when still in use (since
       * a thread rendering on another devices would only be caught in this mutex
       * if it so happens to do an allocation at the same time as well. */
      max_mem->move_to_host = true;
      max_mem->device_move_to_host();
      max_mem->move_to_host = false;
      size = (max_size >= size) ? 0 : size - max_size;

      /* Tag texture info update for new pointers. */
      need_texture_info = true;
    }
    else {
      break;
    }
  }
}

GPUDevice::Mem *GPUDevice::generic_alloc(device_memory &mem, const size_t pitch_padding)
{
  void *device_pointer = nullptr;
  const size_t size = mem.memory_size() + pitch_padding;

  bool mem_alloc_result = false;
  const char *status = "";

  /* First try allocating in device memory, respecting headroom. We make
   * an exception for texture info. It is small and frequently accessed,
   * so treat it as working memory.
   *
   * If there is not enough room for working memory, we will try to move
   * textures to host memory, assuming the performance impact would have
   * been worse for working memory. */
  const bool is_texture = (mem.type == MEM_TEXTURE || mem.type == MEM_GLOBAL) &&
                          (&mem != &texture_info);
  const bool is_image = is_texture && (mem.data_height > 1);

  const size_t headroom = (is_texture) ? device_texture_headroom : device_working_headroom;

  /* Move textures to host memory if needed. */
  if (!mem.move_to_host && !is_image && can_map_host) {
    move_textures_to_host(size, headroom, is_texture);
  }

  size_t total = 0;
  size_t free = 0;
  get_device_memory_info(total, free);

  /* Allocate in device memory. */
  if ((!mem.move_to_host && (size + headroom) < free) || (mem.type == MEM_DEVICE_ONLY)) {
    mem_alloc_result = alloc_device(device_pointer, size);
    if (mem_alloc_result) {
      device_mem_in_use += size;
      status = " in device memory";
    }
  }

  /* Fall back to mapped host memory if needed and possible. */

  void *shared_pointer = nullptr;

  if (!mem_alloc_result && can_map_host && mem.type != MEM_DEVICE_ONLY) {
    if (mem.shared_pointer) {
      /* Another device already allocated host memory. */
      mem_alloc_result = true;
      shared_pointer = mem.shared_pointer;
    }
    else if (map_host_used + size < map_host_limit) {
      /* Allocate host memory ourselves. */
      mem_alloc_result = shared_alloc(shared_pointer, size);

      assert((mem_alloc_result && shared_pointer != nullptr) ||
             (!mem_alloc_result && shared_pointer == nullptr));
    }

    if (mem_alloc_result) {
      device_pointer = shared_to_device_pointer(shared_pointer);
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
    LOG_WORK << "Buffer allocate: " << mem.name << ", "
             << string_human_readable_number(mem.memory_size()) << " bytes. ("
             << string_human_readable_size(mem.memory_size()) << ")" << status;
  }

  mem.device_pointer = (device_ptr)device_pointer;
  mem.device_size = size;
  stats.mem_alloc(size);

  if (!mem.device_pointer) {
    return nullptr;
  }

  /* Insert into map of allocations. */
  const thread_scoped_lock lock(device_mem_map_mutex);
  Mem *cmem = &device_mem_map[&mem];
  if (shared_pointer != nullptr) {
    /* Replace host pointer with our host allocation. Only works if
     * memory layout is the same and has no pitch padding. Also
     * does not work if we move textures to host during a render,
     * since other devices might be using the memory. */

    if (!mem.move_to_host && pitch_padding == 0 && mem.host_pointer &&
        mem.host_pointer != shared_pointer)
    {
      memcpy(shared_pointer, mem.host_pointer, size);
      host_free(mem.type, mem.host_pointer, mem.memory_size());
      mem.host_pointer = shared_pointer;
    }
    mem.shared_pointer = shared_pointer;
    mem.shared_counter++;
  }

  return cmem;
}

void GPUDevice::generic_free(device_memory &mem)
{
  if (!(mem.device_pointer && mem.is_resident(this))) {
    return;
  }

  /* Host pointer should already have been freed at this point. If not we might
   * end up freeing shared memory and can't recover original host memory. */
  assert(mem.host_pointer == nullptr || mem.move_to_host);

  const thread_scoped_lock lock(device_mem_map_mutex);
  DCHECK(device_mem_map.find(&mem) != device_mem_map.end());

  /* For host mapped memory, reference counting is used to safely free it. */
  if (mem.is_shared(this)) {
    assert(mem.shared_counter > 0);
    if (--mem.shared_counter == 0) {
      if (mem.host_pointer == mem.shared_pointer) {
        /* Safely move the device-side data back to the host before it is freed.
         * We should actually never reach this code as it is inefficient, but
         * better than to crash if there is a bug. */
        assert(!"GPU device should not copy memory back to host");
        const size_t size = mem.memory_size();
        mem.host_pointer = mem.host_alloc(size);
        memcpy(mem.host_pointer, mem.shared_pointer, size);
      }
      shared_free(mem.shared_pointer);
      mem.shared_pointer = nullptr;
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

void GPUDevice::generic_copy_to(device_memory &mem)
{
  if (!mem.host_pointer || !mem.device_pointer) {
    return;
  }

  /* If not host mapped, the current device only uses device memory allocated by backend
   * device allocation regardless of mem.host_pointer and mem.shared_pointer, and should
   * copy data from mem.host_pointer. */
  if (!(mem.is_shared(this) && mem.host_pointer == mem.shared_pointer)) {
    copy_host_to_device((void *)mem.device_pointer, mem.host_pointer, mem.memory_size());
  }
}

bool GPUDevice::is_shared(const void *shared_pointer,
                          const device_ptr device_pointer,
                          Device * /*sub_device*/)
{
  return (shared_pointer && device_pointer &&
          (device_ptr)shared_to_device_pointer(shared_pointer) == device_pointer);
}

/* DeviceInfo */

bool DeviceInfo::contains_device_type(const DeviceType type) const
{
  if (this->type == type) {
    return true;
  }
  for (const DeviceInfo &info : multi_devices) {
    if (info.contains_device_type(type)) {
      return true;
    }
  }
  return false;
}

CCL_NAMESPACE_END
