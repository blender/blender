/*
 * Copyright 2011-2021 Blender Foundation
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

#ifdef WITH_HIP

#  include <climits>
#  include <limits.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>

#  include "device/hip/device_impl.h"

#  include "render/buffers.h"

#  include "util/util_debug.h"
#  include "util/util_foreach.h"
#  include "util/util_logging.h"
#  include "util/util_map.h"
#  include "util/util_md5.h"
#  include "util/util_opengl.h"
#  include "util/util_path.h"
#  include "util/util_string.h"
#  include "util/util_system.h"
#  include "util/util_time.h"
#  include "util/util_types.h"
#  include "util/util_windows.h"

CCL_NAMESPACE_BEGIN

class HIPDevice;

bool HIPDevice::have_precompiled_kernels()
{
  string fatbins_path = path_get("lib");
  return path_exists(fatbins_path);
}

bool HIPDevice::show_samples() const
{
  /* The HIPDevice only processes one tile at a time, so showing samples is fine. */
  return true;
}

BVHLayoutMask HIPDevice::get_bvh_layout_mask() const
{
  return BVH_LAYOUT_BVH2;
}

void HIPDevice::set_error(const string &error)
{
  Device::set_error(error);

  if (first_error) {
    fprintf(stderr, "\nRefer to the Cycles GPU rendering documentation for possible solutions:\n");
    fprintf(stderr,
            "https://docs.blender.org/manual/en/latest/render/cycles/gpu_rendering.html\n\n");
    first_error = false;
  }
}

HIPDevice::HIPDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler)
    : Device(info, stats, profiler), texture_info(this, "__texture_info", MEM_GLOBAL)
{
  first_error = true;

  hipDevId = info.num;
  hipDevice = 0;
  hipContext = 0;

  hipModule = 0;

  need_texture_info = false;

  device_texture_headroom = 0;
  device_working_headroom = 0;
  move_texture_to_host = false;
  map_host_limit = 0;
  map_host_used = 0;
  can_map_host = 0;
  pitch_alignment = 0;

  /* Initialize HIP. */
  hipError_t result = hipInit(0);
  if (result != hipSuccess) {
    set_error(string_printf("Failed to initialize HIP runtime (%s)", hipewErrorString(result)));
    return;
  }

  /* Setup device and context. */
  result = hipGetDevice(&hipDevice, hipDevId);
  if (result != hipSuccess) {
    set_error(string_printf("Failed to get HIP device handle from ordinal (%s)",
                            hipewErrorString(result)));
    return;
  }

  hip_assert(hipDeviceGetAttribute(&can_map_host, hipDeviceAttributeCanMapHostMemory, hipDevice));

  hip_assert(
      hipDeviceGetAttribute(&pitch_alignment, hipDeviceAttributeTexturePitchAlignment, hipDevice));

  unsigned int ctx_flags = hipDeviceLmemResizeToMax;
  if (can_map_host) {
    ctx_flags |= hipDeviceMapHost;
    init_host_memory();
  }

  /* Create context. */
  result = hipCtxCreate(&hipContext, ctx_flags, hipDevice);

  if (result != hipSuccess) {
    set_error(string_printf("Failed to create HIP context (%s)", hipewErrorString(result)));
    return;
  }

  int major, minor;
  hipDeviceGetAttribute(&major, hipDeviceAttributeComputeCapabilityMajor, hipDevId);
  hipDeviceGetAttribute(&minor, hipDeviceAttributeComputeCapabilityMinor, hipDevId);
  hipDevArchitecture = major * 100 + minor * 10;

  /* Pop context set by hipCtxCreate. */
  hipCtxPopCurrent(NULL);
}

HIPDevice::~HIPDevice()
{
  texture_info.free();

  hip_assert(hipCtxDestroy(hipContext));
}

bool HIPDevice::support_device(const uint /*kernel_features*/)
{
  int major, minor;
  hipDeviceGetAttribute(&major, hipDeviceAttributeComputeCapabilityMajor, hipDevId);
  hipDeviceGetAttribute(&minor, hipDeviceAttributeComputeCapabilityMinor, hipDevId);

  // TODO : (Arya) What versions do we plan to support?
  return true;
}

bool HIPDevice::check_peer_access(Device *peer_device)
{
  if (peer_device == this) {
    return false;
  }
  if (peer_device->info.type != DEVICE_HIP && peer_device->info.type != DEVICE_OPTIX) {
    return false;
  }

  HIPDevice *const peer_device_hip = static_cast<HIPDevice *>(peer_device);

  int can_access = 0;
  hip_assert(hipDeviceCanAccessPeer(&can_access, hipDevice, peer_device_hip->hipDevice));
  if (can_access == 0) {
    return false;
  }

  // Ensure array access over the link is possible as well (for 3D textures)
  hip_assert(hipDeviceGetP2PAttribute(
      &can_access, hipDevP2PAttrHipArrayAccessSupported, hipDevice, peer_device_hip->hipDevice));
  if (can_access == 0) {
    return false;
  }

  // Enable peer access in both directions
  {
    const HIPContextScope scope(this);
    hipError_t result = hipCtxEnablePeerAccess(peer_device_hip->hipContext, 0);
    if (result != hipSuccess) {
      set_error(string_printf("Failed to enable peer access on HIP context (%s)",
                              hipewErrorString(result)));
      return false;
    }
  }
  {
    const HIPContextScope scope(peer_device_hip);
    hipError_t result = hipCtxEnablePeerAccess(hipContext, 0);
    if (result != hipSuccess) {
      set_error(string_printf("Failed to enable peer access on HIP context (%s)",
                              hipewErrorString(result)));
      return false;
    }
  }

  return true;
}

bool HIPDevice::use_adaptive_compilation()
{
  return DebugFlags().hip.adaptive_compile;
}

/* Common NVCC flags which stays the same regardless of shading model,
 * kernel sources md5 and only depends on compiler or compilation settings.
 */
string HIPDevice::compile_kernel_get_common_cflags(const uint kernel_features)
{
  const int machine = system_cpu_bits();
  const string source_path = path_get("source");
  const string include_path = source_path;
  string cflags = string_printf(
      "-m%d "
      "--ptxas-options=\"-v\" "
      "--use_fast_math "
      "-DHIPCC "
      "-I\"%s\"",
      machine,
      include_path.c_str());
  if (use_adaptive_compilation()) {
    cflags += " -D__KERNEL_FEATURES__=" + to_string(kernel_features);
  }
  return cflags;
}

string HIPDevice::compile_kernel(const uint kernel_features,
                                 const char *name,
                                 const char *base,
                                 bool force_ptx)
{
  /* Compute kernel name. */
  int major, minor;
  hipDeviceGetAttribute(&major, hipDeviceAttributeComputeCapabilityMajor, hipDevId);
  hipDeviceGetAttribute(&minor, hipDeviceAttributeComputeCapabilityMinor, hipDevId);

  /* Attempt to use kernel provided with Blender. */
  if (!use_adaptive_compilation()) {
    if (!force_ptx) {
      const string fatbin = path_get(string_printf("lib/%s_sm_%d%d.cubin", name, major, minor));
      VLOG(1) << "Testing for pre-compiled kernel " << fatbin << ".";
      if (path_exists(fatbin)) {
        VLOG(1) << "Using precompiled kernel.";
        return fatbin;
      }
    }

    /* The driver can JIT-compile PTX generated for older generations, so find the closest one. */
    int ptx_major = major, ptx_minor = minor;
    while (ptx_major >= 3) {
      const string ptx = path_get(
          string_printf("lib/%s_compute_%d%d.ptx", name, ptx_major, ptx_minor));
      VLOG(1) << "Testing for pre-compiled kernel " << ptx << ".";
      if (path_exists(ptx)) {
        VLOG(1) << "Using precompiled kernel.";
        return ptx;
      }

      if (ptx_minor > 0) {
        ptx_minor--;
      }
      else {
        ptx_major--;
        ptx_minor = 9;
      }
    }
  }

  /* Try to use locally compiled kernel. */
  string source_path = path_get("source");
  const string source_md5 = path_files_md5_hash(source_path);

  /* We include cflags into md5 so changing hip toolkit or changing other
   * compiler command line arguments makes sure fatbin gets re-built.
   */
  string common_cflags = compile_kernel_get_common_cflags(kernel_features);
  const string kernel_md5 = util_md5_string(source_md5 + common_cflags);

  const char *const kernel_ext = "genco";
#  ifdef _WIN32
  const char *const options =
      "save-temps -Wno-parentheses-equality -Wno-unused-value --hipcc-func-supp";
#  else
  const char *const options =
      "save-temps -Wno-parentheses-equality -Wno-unused-value --hipcc-func-supp -O3 -ggdb";
#  endif
  const string include_path = source_path;
  const char *const kernel_arch = force_ptx ? "compute" : "sm";
  const string fatbin_file = string_printf(
      "cycles_%s_%s_%d%d_%s", name, kernel_arch, major, minor, kernel_md5.c_str());
  const string fatbin = path_cache_get(path_join("kernels", fatbin_file));
  VLOG(1) << "Testing for locally compiled kernel " << fatbin << ".";
  if (path_exists(fatbin)) {
    VLOG(1) << "Using locally compiled kernel.";
    return fatbin;
  }

#  ifdef _WIN32
  if (!use_adaptive_compilation() && have_precompiled_kernels()) {
    if (major < 3) {
      set_error(
          string_printf("HIP backend requires compute capability 3.0 or up, but found %d.%d. "
                        "Your GPU is not supported.",
                        major,
                        minor));
    }
    else {
      set_error(
          string_printf("HIP binary kernel for this graphics card compute "
                        "capability (%d.%d) not found.",
                        major,
                        minor));
    }
    return string();
  }
#  endif

  /* Compile. */
  const char *const hipcc = hipewCompilerPath();
  if (hipcc == NULL) {
    set_error(
        "HIP hipcc compiler not found. "
        "Install HIP toolkit in default location.");
    return string();
  }

  const int hipcc_hip_version = hipewCompilerVersion();
  VLOG(1) << "Found hipcc " << hipcc << ", HIP version " << hipcc_hip_version << ".";
  if (hipcc_hip_version < 40) {
    printf(
        "Unsupported HIP version %d.%d detected, "
        "you need HIP 4.0 or newer.\n",
        hipcc_hip_version / 10,
        hipcc_hip_version % 10);
    return string();
  }

  double starttime = time_dt();

  path_create_directories(fatbin);

  source_path = path_join(path_join(source_path, "kernel"),
                          path_join("device", path_join(base, string_printf("%s.cpp", name))));

  string command = string_printf("%s -%s -I %s --%s %s -o \"%s\"",
                                 hipcc,
                                 options,
                                 include_path.c_str(),
                                 kernel_ext,
                                 source_path.c_str(),
                                 fatbin.c_str());

  printf("Compiling HIP kernel ...\n%s\n", command.c_str());

#  ifdef _WIN32
  command = "call " + command;
#  endif
  if (system(command.c_str()) != 0) {
    set_error(
        "Failed to execute compilation command, "
        "see console for details.");
    return string();
  }

  /* Verify if compilation succeeded */
  if (!path_exists(fatbin)) {
    set_error(
        "HIP kernel compilation failed, "
        "see console for details.");
    return string();
  }

  printf("Kernel compilation finished in %.2lfs.\n", time_dt() - starttime);

  return fatbin;
}

bool HIPDevice::load_kernels(const uint kernel_features)
{
  /* TODO(sergey): Support kernels re-load for HIP devices.
   *
   * Currently re-loading kernel will invalidate memory pointers,
   * causing problems in hipCtxSynchronize.
   */
  if (hipModule) {
    VLOG(1) << "Skipping kernel reload, not currently supported.";
    return true;
  }

  /* check if hip init succeeded */
  if (hipContext == 0)
    return false;

  /* check if GPU is supported */
  if (!support_device(kernel_features))
    return false;

  /* get kernel */
  const char *kernel_name = "kernel";
  string fatbin = compile_kernel(kernel_features, kernel_name);
  if (fatbin.empty())
    return false;

  /* open module */
  HIPContextScope scope(this);

  string fatbin_data;
  hipError_t result;

  if (path_read_text(fatbin, fatbin_data))
    result = hipModuleLoadData(&hipModule, fatbin_data.c_str());
  else
    result = hipErrorFileNotFound;

  if (result != hipSuccess)
    set_error(string_printf(
        "Failed to load HIP kernel from '%s' (%s)", fatbin.c_str(), hipewErrorString(result)));

  if (result == hipSuccess) {
    kernels.load(this);
    reserve_local_memory(kernel_features);
  }

  return (result == hipSuccess);
}

void HIPDevice::reserve_local_memory(const uint)
{
  /* Together with hipDeviceLmemResizeToMax, this reserves local memory
   * needed for kernel launches, so that we can reliably figure out when
   * to allocate scene data in mapped host memory. */
  size_t total = 0, free_before = 0, free_after = 0;

  {
    HIPContextScope scope(this);
    hipMemGetInfo(&free_before, &total);
  }

  {
    /* Use the biggest kernel for estimation. */
    const DeviceKernel test_kernel = DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE;

    /* Launch kernel, using just 1 block appears sufficient to reserve memory for all
     * multiprocessors. It would be good to do this in parallel for the multi GPU case
     * still to make it faster. */
    HIPDeviceQueue queue(this);

    void *d_path_index = nullptr;
    void *d_render_buffer = nullptr;
    int d_work_size = 0;
    void *args[] = {&d_path_index, &d_render_buffer, &d_work_size};

    queue.init_execution();
    queue.enqueue(test_kernel, 1, args);
    queue.synchronize();
  }

  {
    HIPContextScope scope(this);
    hipMemGetInfo(&free_after, &total);
  }

  VLOG(1) << "Local memory reserved " << string_human_readable_number(free_before - free_after)
          << " bytes. (" << string_human_readable_size(free_before - free_after) << ")";

#  if 0
  /* For testing mapped host memory, fill up device memory. */
  const size_t keep_mb = 1024;

  while (free_after > keep_mb * 1024 * 1024LL) {
    hipDeviceptr_t tmp;
    hip_assert(hipMalloc(&tmp, 10 * 1024 * 1024LL));
    hipMemGetInfo(&free_after, &total);
  }
#  endif
}

void HIPDevice::init_host_memory()
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
    VLOG(1) << "Mapped host memory disabled, failed to get system RAM";
    map_host_limit = 0;
  }

  /* Amount of device memory to keep is free after texture memory
   * and working memory allocations respectively. We set the working
   * memory limit headroom lower so that some space is left after all
   * texture memory allocations. */
  device_working_headroom = 32 * 1024 * 1024LL;   // 32MB
  device_texture_headroom = 128 * 1024 * 1024LL;  // 128MB

  VLOG(1) << "Mapped host memory limit set to " << string_human_readable_number(map_host_limit)
          << " bytes. (" << string_human_readable_size(map_host_limit) << ")";
}

void HIPDevice::load_texture_info()
{
  if (need_texture_info) {
    /* Unset flag before copying, so this does not loop indefinitely if the copy below calls
     * into 'move_textures_to_host' (which calls 'load_texture_info' again). */
    need_texture_info = false;
    texture_info.copy_to_device();
  }
}

void HIPDevice::move_textures_to_host(size_t size, bool for_texture)
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

    thread_scoped_lock lock(hip_mem_map_mutex);
    foreach (HIPMemMap::value_type &pair, hip_mem_map) {
      device_memory &mem = *pair.first;
      HIPMem *cmem = &pair.second;

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
     * multiple HIP devices could be moving the memory. The
     * first one will do it, and the rest will adopt the pointer. */
    if (max_mem) {
      VLOG(1) << "Move memory from device to host: " << max_mem->name;

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

HIPDevice::HIPMem *HIPDevice::generic_alloc(device_memory &mem, size_t pitch_padding)
{
  HIPContextScope scope(this);

  hipDeviceptr_t device_pointer = 0;
  size_t size = mem.memory_size() + pitch_padding;

  hipError_t mem_alloc_result = hipErrorOutOfMemory;
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
  hipMemGetInfo(&free, &total);

  /* Move textures to host memory if needed. */
  if (!move_texture_to_host && !is_image && (size + headroom) >= free && can_map_host) {
    move_textures_to_host(size + headroom - free, is_texture);
    hipMemGetInfo(&free, &total);
  }

  /* Allocate in device memory. */
  if (!move_texture_to_host && (size + headroom) < free) {
    mem_alloc_result = hipMalloc(&device_pointer, size);
    if (mem_alloc_result == hipSuccess) {
      status = " in device memory";
    }
  }

  /* Fall back to mapped host memory if needed and possible. */

  void *shared_pointer = 0;

  if (mem_alloc_result != hipSuccess && can_map_host) {
    if (mem.shared_pointer) {
      /* Another device already allocated host memory. */
      mem_alloc_result = hipSuccess;
      shared_pointer = mem.shared_pointer;
    }
    else if (map_host_used + size < map_host_limit) {
      /* Allocate host memory ourselves. */
      mem_alloc_result = hipHostMalloc(&shared_pointer, size);

      assert((mem_alloc_result == hipSuccess && shared_pointer != 0) ||
             (mem_alloc_result != hipSuccess && shared_pointer == 0));
    }

    if (mem_alloc_result == hipSuccess) {
      hip_assert(hipHostGetDevicePointer(&device_pointer, shared_pointer, 0));
      map_host_used += size;
      status = " in host memory";
    }
  }

  if (mem_alloc_result != hipSuccess) {
    status = " failed, out of device and host memory";
    set_error("System is out of GPU and shared host memory");
  }

  if (mem.name) {
    VLOG(1) << "Buffer allocate: " << mem.name << ", "
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
  thread_scoped_lock lock(hip_mem_map_mutex);
  HIPMem *cmem = &hip_mem_map[&mem];
  if (shared_pointer != 0) {
    /* Replace host pointer with our host allocation. Only works if
     * HIP memory layout is the same and has no pitch padding. Also
     * does not work if we move textures to host during a render,
     * since other devices might be using the memory. */

    if (!move_texture_to_host && pitch_padding == 0 && mem.host_pointer &&
        mem.host_pointer != shared_pointer) {
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

void HIPDevice::generic_copy_to(device_memory &mem)
{
  if (!mem.host_pointer || !mem.device_pointer) {
    return;
  }

  /* If use_mapped_host of mem is false, the current device only uses device memory allocated by
   * hipMalloc regardless of mem.host_pointer and mem.shared_pointer, and should copy data from
   * mem.host_pointer. */
  thread_scoped_lock lock(hip_mem_map_mutex);
  if (!hip_mem_map[&mem].use_mapped_host || mem.host_pointer != mem.shared_pointer) {
    const HIPContextScope scope(this);
    hip_assert(
        hipMemcpyHtoD((hipDeviceptr_t)mem.device_pointer, mem.host_pointer, mem.memory_size()));
  }
}

void HIPDevice::generic_free(device_memory &mem)
{
  if (mem.device_pointer) {
    HIPContextScope scope(this);
    thread_scoped_lock lock(hip_mem_map_mutex);
    const HIPMem &cmem = hip_mem_map[&mem];

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
          hipHostFree(mem.shared_pointer);
          mem.shared_pointer = 0;
        }
      }
      map_host_used -= mem.device_size;
    }
    else {
      /* Free device memory. */
      hip_assert(hipFree(mem.device_pointer));
    }

    stats.mem_free(mem.device_size);
    mem.device_pointer = 0;
    mem.device_size = 0;

    hip_mem_map.erase(hip_mem_map.find(&mem));
  }
}

void HIPDevice::mem_alloc(device_memory &mem)
{
  if (mem.type == MEM_TEXTURE) {
    assert(!"mem_alloc not supported for textures.");
  }
  else if (mem.type == MEM_GLOBAL) {
    assert(!"mem_alloc not supported for global memory.");
  }
  else {
    generic_alloc(mem);
  }
}

void HIPDevice::mem_copy_to(device_memory &mem)
{
  if (mem.type == MEM_GLOBAL) {
    global_free(mem);
    global_alloc(mem);
  }
  else if (mem.type == MEM_TEXTURE) {
    tex_free((device_texture &)mem);
    tex_alloc((device_texture &)mem);
  }
  else {
    if (!mem.device_pointer) {
      generic_alloc(mem);
    }
    generic_copy_to(mem);
  }
}

void HIPDevice::mem_copy_from(device_memory &mem, size_t y, size_t w, size_t h, size_t elem)
{
  if (mem.type == MEM_TEXTURE || mem.type == MEM_GLOBAL) {
    assert(!"mem_copy_from not supported for textures.");
  }
  else if (mem.host_pointer) {
    const size_t size = elem * w * h;
    const size_t offset = elem * y * w;

    if (mem.device_pointer) {
      const HIPContextScope scope(this);
      hip_assert(hipMemcpyDtoH(
          (char *)mem.host_pointer + offset, (hipDeviceptr_t)mem.device_pointer + offset, size));
    }
    else {
      memset((char *)mem.host_pointer + offset, 0, size);
    }
  }
}

void HIPDevice::mem_zero(device_memory &mem)
{
  if (!mem.device_pointer) {
    mem_alloc(mem);
  }
  if (!mem.device_pointer) {
    return;
  }

  /* If use_mapped_host of mem is false, mem.device_pointer currently refers to device memory
   * regardless of mem.host_pointer and mem.shared_pointer. */
  thread_scoped_lock lock(hip_mem_map_mutex);
  if (!hip_mem_map[&mem].use_mapped_host || mem.host_pointer != mem.shared_pointer) {
    const HIPContextScope scope(this);
    hip_assert(hipMemsetD8((hipDeviceptr_t)mem.device_pointer, 0, mem.memory_size()));
  }
  else if (mem.host_pointer) {
    memset(mem.host_pointer, 0, mem.memory_size());
  }
}

void HIPDevice::mem_free(device_memory &mem)
{
  if (mem.type == MEM_GLOBAL) {
    global_free(mem);
  }
  else if (mem.type == MEM_TEXTURE) {
    tex_free((device_texture &)mem);
  }
  else {
    generic_free(mem);
  }
}

device_ptr HIPDevice::mem_alloc_sub_ptr(device_memory &mem, size_t offset, size_t /*size*/)
{
  return (device_ptr)(((char *)mem.device_pointer) + mem.memory_elements_size(offset));
}

void HIPDevice::const_copy_to(const char *name, void *host, size_t size)
{
  HIPContextScope scope(this);
  hipDeviceptr_t mem;
  size_t bytes;

  hip_assert(hipModuleGetGlobal(&mem, &bytes, hipModule, name));
  assert(bytes == size);
  hip_assert(hipMemcpyHtoD(mem, host, size));
}

void HIPDevice::global_alloc(device_memory &mem)
{
  if (mem.is_resident(this)) {
    generic_alloc(mem);
    generic_copy_to(mem);
  }

  const_copy_to(mem.name, &mem.device_pointer, sizeof(mem.device_pointer));
}

void HIPDevice::global_free(device_memory &mem)
{
  if (mem.is_resident(this) && mem.device_pointer) {
    generic_free(mem);
  }
}

void HIPDevice::tex_alloc(device_texture &mem)
{
  HIPContextScope scope(this);

  /* General variables for both architectures */
  string bind_name = mem.name;
  size_t dsize = datatype_size(mem.data_type);
  size_t size = mem.memory_size();

  hipTextureAddressMode address_mode = hipAddressModeWrap;
  switch (mem.info.extension) {
    case EXTENSION_REPEAT:
      address_mode = hipAddressModeWrap;
      break;
    case EXTENSION_EXTEND:
      address_mode = hipAddressModeClamp;
      break;
    case EXTENSION_CLIP:
      // TODO : (Arya) setting this to Mode Clamp instead of Mode Border because it's unsupported
      // in hip
      address_mode = hipAddressModeClamp;
      break;
    default:
      assert(0);
      break;
  }

  hipTextureFilterMode filter_mode;
  if (mem.info.interpolation == INTERPOLATION_CLOSEST) {
    filter_mode = hipFilterModePoint;
  }
  else {
    filter_mode = hipFilterModeLinear;
  }

  /* Image Texture Storage */
  hipArray_Format format;
  switch (mem.data_type) {
    case TYPE_UCHAR:
      format = HIP_AD_FORMAT_UNSIGNED_INT8;
      break;
    case TYPE_UINT16:
      format = HIP_AD_FORMAT_UNSIGNED_INT16;
      break;
    case TYPE_UINT:
      format = HIP_AD_FORMAT_UNSIGNED_INT32;
      break;
    case TYPE_INT:
      format = HIP_AD_FORMAT_SIGNED_INT32;
      break;
    case TYPE_FLOAT:
      format = HIP_AD_FORMAT_FLOAT;
      break;
    case TYPE_HALF:
      format = HIP_AD_FORMAT_HALF;
      break;
    default:
      assert(0);
      return;
  }

  HIPMem *cmem = NULL;
  hArray array_3d = NULL;
  size_t src_pitch = mem.data_width * dsize * mem.data_elements;
  size_t dst_pitch = src_pitch;

  if (!mem.is_resident(this)) {
    thread_scoped_lock lock(hip_mem_map_mutex);
    cmem = &hip_mem_map[&mem];
    cmem->texobject = 0;

    if (mem.data_depth > 1) {
      array_3d = (hArray)mem.device_pointer;
      cmem->array = array_3d;
    }
    else if (mem.data_height > 0) {
      dst_pitch = align_up(src_pitch, pitch_alignment);
    }
  }
  else if (mem.data_depth > 1) {
    /* 3D texture using array, there is no API for linear memory. */
    HIP_ARRAY3D_DESCRIPTOR desc;

    desc.Width = mem.data_width;
    desc.Height = mem.data_height;
    desc.Depth = mem.data_depth;
    desc.Format = format;
    desc.NumChannels = mem.data_elements;
    desc.Flags = 0;

    VLOG(1) << "Array 3D allocate: " << mem.name << ", "
            << string_human_readable_number(mem.memory_size()) << " bytes. ("
            << string_human_readable_size(mem.memory_size()) << ")";

    hip_assert(hipArray3DCreate(&array_3d, &desc));

    if (!array_3d) {
      return;
    }

    HIP_MEMCPY3D param;
    memset(&param, 0, sizeof(param));
    param.dstMemoryType = hipMemoryTypeArray;
    param.dstArray = &array_3d;
    param.srcMemoryType = hipMemoryTypeHost;
    param.srcHost = mem.host_pointer;
    param.srcPitch = src_pitch;
    param.WidthInBytes = param.srcPitch;
    param.Height = mem.data_height;
    param.Depth = mem.data_depth;

    hip_assert(hipDrvMemcpy3D(&param));

    mem.device_pointer = (device_ptr)array_3d;
    mem.device_size = size;
    stats.mem_alloc(size);

    thread_scoped_lock lock(hip_mem_map_mutex);
    cmem = &hip_mem_map[&mem];
    cmem->texobject = 0;
    cmem->array = array_3d;
  }
  else if (mem.data_height > 0) {
    /* 2D texture, using pitch aligned linear memory. */
    dst_pitch = align_up(src_pitch, pitch_alignment);
    size_t dst_size = dst_pitch * mem.data_height;

    cmem = generic_alloc(mem, dst_size - mem.memory_size());
    if (!cmem) {
      return;
    }

    hip_Memcpy2D param;
    memset(&param, 0, sizeof(param));
    param.dstMemoryType = hipMemoryTypeDevice;
    param.dstDevice = mem.device_pointer;
    param.dstPitch = dst_pitch;
    param.srcMemoryType = hipMemoryTypeHost;
    param.srcHost = mem.host_pointer;
    param.srcPitch = src_pitch;
    param.WidthInBytes = param.srcPitch;
    param.Height = mem.data_height;

    hip_assert(hipDrvMemcpy2DUnaligned(&param));
  }
  else {
    /* 1D texture, using linear memory. */
    cmem = generic_alloc(mem);
    if (!cmem) {
      return;
    }

    hip_assert(hipMemcpyHtoD(mem.device_pointer, mem.host_pointer, size));
  }

  /* Resize once */
  const uint slot = mem.slot;
  if (slot >= texture_info.size()) {
    /* Allocate some slots in advance, to reduce amount
     * of re-allocations. */
    texture_info.resize(slot + 128);
  }

  /* Set Mapping and tag that we need to (re-)upload to device */
  texture_info[slot] = mem.info;
  need_texture_info = true;

  if (mem.info.data_type != IMAGE_DATA_TYPE_NANOVDB_FLOAT &&
      mem.info.data_type != IMAGE_DATA_TYPE_NANOVDB_FLOAT3) {
    /* Kepler+, bindless textures. */
    hipResourceDesc resDesc;
    memset(&resDesc, 0, sizeof(resDesc));

    if (array_3d) {
      resDesc.resType = hipResourceTypeArray;
      resDesc.res.array.h_Array = &array_3d;
      resDesc.flags = 0;
    }
    else if (mem.data_height > 0) {
      resDesc.resType = hipResourceTypePitch2D;
      resDesc.res.pitch2D.devPtr = mem.device_pointer;
      resDesc.res.pitch2D.format = format;
      resDesc.res.pitch2D.numChannels = mem.data_elements;
      resDesc.res.pitch2D.height = mem.data_height;
      resDesc.res.pitch2D.width = mem.data_width;
      resDesc.res.pitch2D.pitchInBytes = dst_pitch;
    }
    else {
      resDesc.resType = hipResourceTypeLinear;
      resDesc.res.linear.devPtr = mem.device_pointer;
      resDesc.res.linear.format = format;
      resDesc.res.linear.numChannels = mem.data_elements;
      resDesc.res.linear.sizeInBytes = mem.device_size;
    }

    hipTextureDesc texDesc;
    memset(&texDesc, 0, sizeof(texDesc));
    texDesc.addressMode[0] = address_mode;
    texDesc.addressMode[1] = address_mode;
    texDesc.addressMode[2] = address_mode;
    texDesc.filterMode = filter_mode;
    texDesc.flags = HIP_TRSF_NORMALIZED_COORDINATES;

    thread_scoped_lock lock(hip_mem_map_mutex);
    cmem = &hip_mem_map[&mem];

    hip_assert(hipTexObjectCreate(&cmem->texobject, &resDesc, &texDesc, NULL));

    texture_info[slot].data = (uint64_t)cmem->texobject;
  }
  else {
    texture_info[slot].data = (uint64_t)mem.device_pointer;
  }
}

void HIPDevice::tex_free(device_texture &mem)
{
  if (mem.device_pointer) {
    HIPContextScope scope(this);
    thread_scoped_lock lock(hip_mem_map_mutex);
    const HIPMem &cmem = hip_mem_map[&mem];

    if (cmem.texobject) {
      /* Free bindless texture. */
      hipTexObjectDestroy(cmem.texobject);
    }

    if (!mem.is_resident(this)) {
      /* Do not free memory here, since it was allocated on a different device. */
      hip_mem_map.erase(hip_mem_map.find(&mem));
    }
    else if (cmem.array) {
      /* Free array. */
      hipArrayDestroy(cmem.array);
      stats.mem_free(mem.device_size);
      mem.device_pointer = 0;
      mem.device_size = 0;

      hip_mem_map.erase(hip_mem_map.find(&mem));
    }
    else {
      lock.unlock();
      generic_free(mem);
    }
  }
}

#  if 0
void HIPDevice::render(DeviceTask &task,
                        RenderTile &rtile,
                        device_vector<KernelWorkTile> &work_tiles)
{
  scoped_timer timer(&rtile.buffers->render_time);

  if (have_error())
    return;

  HIPContextScope scope(this);
  hipFunction_t hipRender;

  /* Get kernel function. */
  if (rtile.task == RenderTile::BAKE) {
    hip_assert(hipModuleGetFunction(&hipRender, hipModule, "kernel_hip_bake"));
  }
  else {
    hip_assert(hipModuleGetFunction(&hipRender, hipModule, "kernel_hip_path_trace"));
  }

  if (have_error()) {
    return;
  }

  hip_assert(hipFuncSetCacheConfig(hipRender, hipFuncCachePreferL1));

  /* Allocate work tile. */
  work_tiles.alloc(1);

  KernelWorkTile *wtile = work_tiles.data();
  wtile->x = rtile.x;
  wtile->y = rtile.y;
  wtile->w = rtile.w;
  wtile->h = rtile.h;
  wtile->offset = rtile.offset;
  wtile->stride = rtile.stride;
  wtile->buffer = (float *)(hipDeviceptr_t)rtile.buffer;

  /* Prepare work size. More step samples render faster, but for now we
   * remain conservative for GPUs connected to a display to avoid driver
   * timeouts and display freezing. */
  int min_blocks, num_threads_per_block;
  hip_assert(
      hipModuleOccupancyMaxPotentialBlockSize(&min_blocks, &num_threads_per_block, hipRender, NULL, 0, 0));
  if (!info.display_device) {
    min_blocks *= 8;
  }

  uint step_samples = divide_up(min_blocks * num_threads_per_block, wtile->w * wtile->h);

  /* Render all samples. */
  uint start_sample = rtile.start_sample;
  uint end_sample = rtile.start_sample + rtile.num_samples;

  for (int sample = start_sample; sample < end_sample;) {
    /* Setup and copy work tile to device. */
    wtile->start_sample = sample;
    wtile->num_samples = step_samples;
    if (task.adaptive_sampling.use) {
      wtile->num_samples = task.adaptive_sampling.align_samples(sample, step_samples);
    }
    wtile->num_samples = min(wtile->num_samples, end_sample - sample);
    work_tiles.copy_to_device();

    hipDeviceptr_t d_work_tiles = (hipDeviceptr_t)work_tiles.device_pointer;
    uint total_work_size = wtile->w * wtile->h * wtile->num_samples;
    uint num_blocks = divide_up(total_work_size, num_threads_per_block);

    /* Launch kernel. */
    void *args[] = {&d_work_tiles, &total_work_size};

    hip_assert(
        hipModuleLaunchKernel(hipRender, num_blocks, 1, 1, num_threads_per_block, 1, 1, 0, 0, args, 0));

    /* Run the adaptive sampling kernels at selected samples aligned to step samples. */
    uint filter_sample = sample + wtile->num_samples - 1;
    if (task.adaptive_sampling.use && task.adaptive_sampling.need_filter(filter_sample)) {
      adaptive_sampling_filter(filter_sample, wtile, d_work_tiles);
    }

    hip_assert(hipDeviceSynchronize());

    /* Update progress. */
    sample += wtile->num_samples;
    rtile.sample = sample;
    task.update_progress(&rtile, rtile.w * rtile.h * wtile->num_samples);

    if (task.get_cancel()) {
      if (task.need_finish_queue == false)
        break;
    }
  }

  /* Finalize adaptive sampling. */
  if (task.adaptive_sampling.use) {
    hipDeviceptr_t d_work_tiles = (hipDeviceptr_t)work_tiles.device_pointer;
    adaptive_sampling_post(rtile, wtile, d_work_tiles);
    hip_assert(hipDeviceSynchronize());
    task.update_progress(&rtile, rtile.w * rtile.h * wtile->num_samples);
  }
}

void HIPDevice::thread_run(DeviceTask &task)
{
  HIPContextScope scope(this);

  if (task.type == DeviceTask::RENDER) {
    device_vector<KernelWorkTile> work_tiles(this, "work_tiles", MEM_READ_ONLY);

    /* keep rendering tiles until done */
    RenderTile tile;
    DenoisingTask denoising(this, task);

    while (task.acquire_tile(this, tile, task.tile_types)) {
      if (tile.task == RenderTile::PATH_TRACE) {
        render(task, tile, work_tiles);
      }
      else if (tile.task == RenderTile::BAKE) {
        render(task, tile, work_tiles);
      }

      task.release_tile(tile);

      if (task.get_cancel()) {
        if (task.need_finish_queue == false)
          break;
      }
    }

    work_tiles.free();
  }
}
#  endif

unique_ptr<DeviceQueue> HIPDevice::gpu_queue_create()
{
  return make_unique<HIPDeviceQueue>(this);
}

bool HIPDevice::should_use_graphics_interop()
{
  /* Check whether this device is part of OpenGL context.
   *
   * Using HIP device for graphics interoperability which is not part of the OpenGL context is
   * possible, but from the empiric measurements it can be considerably slower than using naive
   * pixels copy. */

  HIPContextScope scope(this);

  int num_all_devices = 0;
  hip_assert(hipGetDeviceCount(&num_all_devices));

  if (num_all_devices == 0) {
    return false;
  }

  vector<hipDevice_t> gl_devices(num_all_devices);
  uint num_gl_devices = 0;
  hipGLGetDevices(&num_gl_devices, gl_devices.data(), num_all_devices, hipGLDeviceListAll);

  for (hipDevice_t gl_device : gl_devices) {
    if (gl_device == hipDevice) {
      return true;
    }
  }

  return false;
}

int HIPDevice::get_num_multiprocessors()
{
  return get_device_default_attribute(hipDeviceAttributeMultiprocessorCount, 0);
}

int HIPDevice::get_max_num_threads_per_multiprocessor()
{
  return get_device_default_attribute(hipDeviceAttributeMaxThreadsPerMultiProcessor, 0);
}

bool HIPDevice::get_device_attribute(hipDeviceAttribute_t attribute, int *value)
{
  HIPContextScope scope(this);

  return hipDeviceGetAttribute(value, attribute, hipDevice) == hipSuccess;
}

int HIPDevice::get_device_default_attribute(hipDeviceAttribute_t attribute, int default_value)
{
  int value = 0;
  if (!get_device_attribute(attribute, &value)) {
    return default_value;
  }
  return value;
}

CCL_NAMESPACE_END

#endif
