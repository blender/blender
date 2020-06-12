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

#ifdef WITH_CUDA

#  include <climits>
#  include <limits.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>

#  include "device/cuda/device_cuda.h"
#  include "device/device_intern.h"
#  include "device/device_split_kernel.h"

#  include "render/buffers.h"

#  include "kernel/filter/filter_defines.h"

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

#  include "kernel/split/kernel_split_data_types.h"

CCL_NAMESPACE_BEGIN

#  ifndef WITH_CUDA_DYNLOAD

/* Transparently implement some functions, so majority of the file does not need
 * to worry about difference between dynamically loaded and linked CUDA at all.
 */

namespace {

const char *cuewErrorString(CUresult result)
{
  /* We can only give error code here without major code duplication, that
   * should be enough since dynamic loading is only being disabled by folks
   * who knows what they're doing anyway.
   *
   * NOTE: Avoid call from several threads.
   */
  static string error;
  error = string_printf("%d", result);
  return error.c_str();
}

const char *cuewCompilerPath()
{
  return CYCLES_CUDA_NVCC_EXECUTABLE;
}

int cuewCompilerVersion()
{
  return (CUDA_VERSION / 100) + (CUDA_VERSION % 100 / 10);
}

} /* namespace */
#  endif /* WITH_CUDA_DYNLOAD */

class CUDADevice;

class CUDASplitKernel : public DeviceSplitKernel {
  CUDADevice *device;

 public:
  explicit CUDASplitKernel(CUDADevice *device);

  virtual uint64_t state_buffer_size(device_memory &kg, device_memory &data, size_t num_threads);

  virtual bool enqueue_split_kernel_data_init(const KernelDimensions &dim,
                                              RenderTile &rtile,
                                              int num_global_elements,
                                              device_memory &kernel_globals,
                                              device_memory &kernel_data_,
                                              device_memory &split_data,
                                              device_memory &ray_state,
                                              device_memory &queue_index,
                                              device_memory &use_queues_flag,
                                              device_memory &work_pool_wgs);

  virtual SplitKernelFunction *get_split_kernel_function(const string &kernel_name,
                                                         const DeviceRequestedFeatures &);
  virtual int2 split_kernel_local_size();
  virtual int2 split_kernel_global_size(device_memory &kg, device_memory &data, DeviceTask *task);
};

/* Utility to push/pop CUDA context. */
class CUDAContextScope {
 public:
  CUDAContextScope(CUDADevice *device);
  ~CUDAContextScope();

 private:
  CUDADevice *device;
};

bool CUDADevice::have_precompiled_kernels()
{
  string cubins_path = path_get("lib");
  return path_exists(cubins_path);
}

bool CUDADevice::show_samples() const
{
  /* The CUDADevice only processes one tile at a time, so showing samples is fine. */
  return true;
}

BVHLayoutMask CUDADevice::get_bvh_layout_mask() const
{
  return BVH_LAYOUT_BVH2;
}

void CUDADevice::set_error(const string &error)
{
  Device::set_error(error);

  if (first_error) {
    fprintf(stderr, "\nRefer to the Cycles GPU rendering documentation for possible solutions:\n");
    fprintf(stderr,
            "https://docs.blender.org/manual/en/latest/render/cycles/gpu_rendering.html\n\n");
    first_error = false;
  }
}

#  define cuda_assert(stmt) \
    { \
      CUresult result = stmt; \
      if (result != CUDA_SUCCESS) { \
        const char *name = cuewErrorString(result); \
        set_error(string_printf("%s in %s (device_cuda_impl.cpp:%d)", name, #stmt, __LINE__)); \
      } \
    } \
    (void)0

CUDADevice::CUDADevice(DeviceInfo &info, Stats &stats, Profiler &profiler, bool background_)
    : Device(info, stats, profiler, background_), texture_info(this, "__texture_info", MEM_GLOBAL)
{
  first_error = true;
  background = background_;

  cuDevId = info.num;
  cuDevice = 0;
  cuContext = 0;

  cuModule = 0;
  cuFilterModule = 0;

  split_kernel = NULL;

  need_texture_info = false;

  device_texture_headroom = 0;
  device_working_headroom = 0;
  move_texture_to_host = false;
  map_host_limit = 0;
  map_host_used = 0;
  can_map_host = 0;
  pitch_alignment = 0;

  functions.loaded = false;

  /* Intialize CUDA. */
  CUresult result = cuInit(0);
  if (result != CUDA_SUCCESS) {
    set_error(string_printf("Failed to initialize CUDA runtime (%s)", cuewErrorString(result)));
    return;
  }

  /* Setup device and context. */
  result = cuDeviceGet(&cuDevice, cuDevId);
  if (result != CUDA_SUCCESS) {
    set_error(string_printf("Failed to get CUDA device handle from ordinal (%s)",
                            cuewErrorString(result)));
    return;
  }

  /* CU_CTX_MAP_HOST for mapping host memory when out of device memory.
   * CU_CTX_LMEM_RESIZE_TO_MAX for reserving local memory ahead of render,
   * so we can predict which memory to map to host. */
  cuda_assert(
      cuDeviceGetAttribute(&can_map_host, CU_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY, cuDevice));

  cuda_assert(cuDeviceGetAttribute(
      &pitch_alignment, CU_DEVICE_ATTRIBUTE_TEXTURE_PITCH_ALIGNMENT, cuDevice));

  unsigned int ctx_flags = CU_CTX_LMEM_RESIZE_TO_MAX;
  if (can_map_host) {
    ctx_flags |= CU_CTX_MAP_HOST;
    init_host_memory();
  }

  /* Create context. */
  if (background) {
    result = cuCtxCreate(&cuContext, ctx_flags, cuDevice);
  }
  else {
    result = cuGLCtxCreate(&cuContext, ctx_flags, cuDevice);

    if (result != CUDA_SUCCESS) {
      result = cuCtxCreate(&cuContext, ctx_flags, cuDevice);
      background = true;
    }
  }

  if (result != CUDA_SUCCESS) {
    set_error(string_printf("Failed to create CUDA context (%s)", cuewErrorString(result)));
    return;
  }

  int major, minor;
  cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, cuDevId);
  cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, cuDevId);
  cuDevArchitecture = major * 100 + minor * 10;

  /* Pop context set by cuCtxCreate. */
  cuCtxPopCurrent(NULL);
}

CUDADevice::~CUDADevice()
{
  task_pool.stop();

  delete split_kernel;

  texture_info.free();

  cuda_assert(cuCtxDestroy(cuContext));
}

bool CUDADevice::support_device(const DeviceRequestedFeatures & /*requested_features*/)
{
  int major, minor;
  cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, cuDevId);
  cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, cuDevId);

  /* We only support sm_30 and above */
  if (major < 3) {
    set_error(string_printf(
        "CUDA backend requires compute capability 3.0 or up, but found %d.%d.", major, minor));
    return false;
  }

  return true;
}

bool CUDADevice::check_peer_access(Device *peer_device)
{
  if (peer_device == this) {
    return false;
  }
  if (peer_device->info.type != DEVICE_CUDA && peer_device->info.type != DEVICE_OPTIX) {
    return false;
  }

  CUDADevice *const peer_device_cuda = static_cast<CUDADevice *>(peer_device);

  int can_access = 0;
  cuda_assert(cuDeviceCanAccessPeer(&can_access, cuDevice, peer_device_cuda->cuDevice));
  if (can_access == 0) {
    return false;
  }

  // Ensure array access over the link is possible as well (for 3D textures)
  cuda_assert(cuDeviceGetP2PAttribute(&can_access,
                                      CU_DEVICE_P2P_ATTRIBUTE_ARRAY_ACCESS_ACCESS_SUPPORTED,
                                      cuDevice,
                                      peer_device_cuda->cuDevice));
  if (can_access == 0) {
    return false;
  }

  // Enable peer access in both directions
  {
    const CUDAContextScope scope(this);
    CUresult result = cuCtxEnablePeerAccess(peer_device_cuda->cuContext, 0);
    if (result != CUDA_SUCCESS) {
      set_error(string_printf("Failed to enable peer access on CUDA context (%s)",
                              cuewErrorString(result)));
      return false;
    }
  }
  {
    const CUDAContextScope scope(peer_device_cuda);
    CUresult result = cuCtxEnablePeerAccess(cuContext, 0);
    if (result != CUDA_SUCCESS) {
      set_error(string_printf("Failed to enable peer access on CUDA context (%s)",
                              cuewErrorString(result)));
      return false;
    }
  }

  return true;
}

bool CUDADevice::use_adaptive_compilation()
{
  return DebugFlags().cuda.adaptive_compile;
}

bool CUDADevice::use_split_kernel()
{
  return DebugFlags().cuda.split_kernel;
}

/* Common NVCC flags which stays the same regardless of shading model,
 * kernel sources md5 and only depends on compiler or compilation settings.
 */
string CUDADevice::compile_kernel_get_common_cflags(
    const DeviceRequestedFeatures &requested_features, bool filter, bool split)
{
  const int machine = system_cpu_bits();
  const string source_path = path_get("source");
  const string include_path = source_path;
  string cflags = string_printf(
      "-m%d "
      "--ptxas-options=\"-v\" "
      "--use_fast_math "
      "-DNVCC "
      "-I\"%s\"",
      machine,
      include_path.c_str());
  if (!filter && use_adaptive_compilation()) {
    cflags += " " + requested_features.get_build_options();
  }
  const char *extra_cflags = getenv("CYCLES_CUDA_EXTRA_CFLAGS");
  if (extra_cflags) {
    cflags += string(" ") + string(extra_cflags);
  }
#  ifdef WITH_CYCLES_DEBUG
  cflags += " -D__KERNEL_DEBUG__";
#  endif

  if (split) {
    cflags += " -D__SPLIT__";
  }

  return cflags;
}

string CUDADevice::compile_kernel(const DeviceRequestedFeatures &requested_features,
                                  const char *name,
                                  const char *base,
                                  bool force_ptx)
{
  /* Compute kernel name. */
  int major, minor;
  cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, cuDevId);
  cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, cuDevId);

  /* Attempt to use kernel provided with Blender. */
  if (!use_adaptive_compilation()) {
    if (!force_ptx) {
      const string cubin = path_get(string_printf("lib/%s_sm_%d%d.cubin", name, major, minor));
      VLOG(1) << "Testing for pre-compiled kernel " << cubin << ".";
      if (path_exists(cubin)) {
        VLOG(1) << "Using precompiled kernel.";
        return cubin;
      }
    }

    const string ptx = path_get(string_printf("lib/%s_compute_%d%d.ptx", name, major, minor));
    VLOG(1) << "Testing for pre-compiled kernel " << ptx << ".";
    if (path_exists(ptx)) {
      VLOG(1) << "Using precompiled kernel.";
      return ptx;
    }
  }

  /* Try to use locally compiled kernel. */
  string source_path = path_get("source");
  const string source_md5 = path_files_md5_hash(source_path);

  /* We include cflags into md5 so changing cuda toolkit or changing other
   * compiler command line arguments makes sure cubin gets re-built.
   */
  string common_cflags = compile_kernel_get_common_cflags(
      requested_features, strstr(name, "filter") != NULL, strstr(name, "split") != NULL);
  const string kernel_md5 = util_md5_string(source_md5 + common_cflags);

  const char *const kernel_ext = force_ptx ? "ptx" : "cubin";
  const char *const kernel_arch = force_ptx ? "compute" : "sm";
  const string cubin_file = string_printf(
      "cycles_%s_%s_%d%d_%s.%s", name, kernel_arch, major, minor, kernel_md5.c_str(), kernel_ext);
  const string cubin = path_cache_get(path_join("kernels", cubin_file));
  VLOG(1) << "Testing for locally compiled kernel " << cubin << ".";
  if (path_exists(cubin)) {
    VLOG(1) << "Using locally compiled kernel.";
    return cubin;
  }

#  ifdef _WIN32
  if (!use_adaptive_compilation() && have_precompiled_kernels()) {
    if (major < 3) {
      set_error(
          string_printf("CUDA backend requires compute capability 3.0 or up, but found %d.%d. "
                        "Your GPU is not supported.",
                        major,
                        minor));
    }
    else {
      set_error(
          string_printf("CUDA binary kernel for this graphics card compute "
                        "capability (%d.%d) not found.",
                        major,
                        minor));
    }
    return string();
  }
#  endif

  /* Compile. */
  const char *const nvcc = cuewCompilerPath();
  if (nvcc == NULL) {
    set_error(
        "CUDA nvcc compiler not found. "
        "Install CUDA toolkit in default location.");
    return string();
  }

  const int nvcc_cuda_version = cuewCompilerVersion();
  VLOG(1) << "Found nvcc " << nvcc << ", CUDA version " << nvcc_cuda_version << ".";
  if (nvcc_cuda_version < 80) {
    printf(
        "Unsupported CUDA version %d.%d detected, "
        "you need CUDA 8.0 or newer.\n",
        nvcc_cuda_version / 10,
        nvcc_cuda_version % 10);
    return string();
  }
  else if (!(nvcc_cuda_version == 101 || nvcc_cuda_version == 102)) {
    printf(
        "CUDA version %d.%d detected, build may succeed but only "
        "CUDA 10.1 and 10.2 are officially supported.\n",
        nvcc_cuda_version / 10,
        nvcc_cuda_version % 10);
  }

  double starttime = time_dt();

  path_create_directories(cubin);

  source_path = path_join(path_join(source_path, "kernel"),
                          path_join("kernels", path_join(base, string_printf("%s.cu", name))));

  string command = string_printf(
      "\"%s\" "
      "-arch=%s_%d%d "
      "--%s \"%s\" "
      "-o \"%s\" "
      "%s",
      nvcc,
      kernel_arch,
      major,
      minor,
      kernel_ext,
      source_path.c_str(),
      cubin.c_str(),
      common_cflags.c_str());

  printf("Compiling CUDA kernel ...\n%s\n", command.c_str());

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
  if (!path_exists(cubin)) {
    set_error(
        "CUDA kernel compilation failed, "
        "see console for details.");
    return string();
  }

  printf("Kernel compilation finished in %.2lfs.\n", time_dt() - starttime);

  return cubin;
}

bool CUDADevice::load_kernels(const DeviceRequestedFeatures &requested_features)
{
  /* TODO(sergey): Support kernels re-load for CUDA devices.
   *
   * Currently re-loading kernel will invalidate memory pointers,
   * causing problems in cuCtxSynchronize.
   */
  if (cuFilterModule && cuModule) {
    VLOG(1) << "Skipping kernel reload, not currently supported.";
    return true;
  }

  /* check if cuda init succeeded */
  if (cuContext == 0)
    return false;

  /* check if GPU is supported */
  if (!support_device(requested_features))
    return false;

  /* get kernel */
  const char *kernel_name = use_split_kernel() ? "kernel_split" : "kernel";
  string cubin = compile_kernel(requested_features, kernel_name);
  if (cubin.empty())
    return false;

  const char *filter_name = "filter";
  string filter_cubin = compile_kernel(requested_features, filter_name);
  if (filter_cubin.empty())
    return false;

  /* open module */
  CUDAContextScope scope(this);

  string cubin_data;
  CUresult result;

  if (path_read_text(cubin, cubin_data))
    result = cuModuleLoadData(&cuModule, cubin_data.c_str());
  else
    result = CUDA_ERROR_FILE_NOT_FOUND;

  if (result != CUDA_SUCCESS)
    set_error(string_printf(
        "Failed to load CUDA kernel from '%s' (%s)", cubin.c_str(), cuewErrorString(result)));

  if (path_read_text(filter_cubin, cubin_data))
    result = cuModuleLoadData(&cuFilterModule, cubin_data.c_str());
  else
    result = CUDA_ERROR_FILE_NOT_FOUND;

  if (result != CUDA_SUCCESS)
    set_error(string_printf("Failed to load CUDA kernel from '%s' (%s)",
                            filter_cubin.c_str(),
                            cuewErrorString(result)));

  if (result == CUDA_SUCCESS) {
    reserve_local_memory(requested_features);
  }

  load_functions();

  return (result == CUDA_SUCCESS);
}

void CUDADevice::load_functions()
{
  /* TODO: load all functions here. */
  if (functions.loaded) {
    return;
  }
  functions.loaded = true;

  cuda_assert(cuModuleGetFunction(
      &functions.adaptive_stopping, cuModule, "kernel_cuda_adaptive_stopping"));
  cuda_assert(cuModuleGetFunction(
      &functions.adaptive_filter_x, cuModule, "kernel_cuda_adaptive_filter_x"));
  cuda_assert(cuModuleGetFunction(
      &functions.adaptive_filter_y, cuModule, "kernel_cuda_adaptive_filter_y"));
  cuda_assert(cuModuleGetFunction(
      &functions.adaptive_scale_samples, cuModule, "kernel_cuda_adaptive_scale_samples"));

  cuda_assert(cuFuncSetCacheConfig(functions.adaptive_stopping, CU_FUNC_CACHE_PREFER_L1));
  cuda_assert(cuFuncSetCacheConfig(functions.adaptive_filter_x, CU_FUNC_CACHE_PREFER_L1));
  cuda_assert(cuFuncSetCacheConfig(functions.adaptive_filter_y, CU_FUNC_CACHE_PREFER_L1));
  cuda_assert(cuFuncSetCacheConfig(functions.adaptive_scale_samples, CU_FUNC_CACHE_PREFER_L1));

  int unused_min_blocks;
  cuda_assert(cuOccupancyMaxPotentialBlockSize(&unused_min_blocks,
                                               &functions.adaptive_num_threads_per_block,
                                               functions.adaptive_scale_samples,
                                               NULL,
                                               0,
                                               0));
}

void CUDADevice::reserve_local_memory(const DeviceRequestedFeatures &requested_features)
{
  if (use_split_kernel()) {
    /* Split kernel mostly uses global memory and adaptive compilation,
     * difficult to predict how much is needed currently. */
    return;
  }

  /* Together with CU_CTX_LMEM_RESIZE_TO_MAX, this reserves local memory
   * needed for kernel launches, so that we can reliably figure out when
   * to allocate scene data in mapped host memory. */
  CUDAContextScope scope(this);

  size_t total = 0, free_before = 0, free_after = 0;
  cuMemGetInfo(&free_before, &total);

  /* Get kernel function. */
  CUfunction cuRender;

  if (requested_features.use_baking) {
    cuda_assert(cuModuleGetFunction(&cuRender, cuModule, "kernel_cuda_bake"));
  }
  else if (requested_features.use_integrator_branched) {
    cuda_assert(cuModuleGetFunction(&cuRender, cuModule, "kernel_cuda_branched_path_trace"));
  }
  else {
    cuda_assert(cuModuleGetFunction(&cuRender, cuModule, "kernel_cuda_path_trace"));
  }

  cuda_assert(cuFuncSetCacheConfig(cuRender, CU_FUNC_CACHE_PREFER_L1));

  int min_blocks, num_threads_per_block;
  cuda_assert(
      cuOccupancyMaxPotentialBlockSize(&min_blocks, &num_threads_per_block, cuRender, NULL, 0, 0));

  /* Launch kernel, using just 1 block appears sufficient to reserve
   * memory for all multiprocessors. It would be good to do this in
   * parallel for the multi GPU case still to make it faster. */
  CUdeviceptr d_work_tiles = 0;
  uint total_work_size = 0;

  void *args[] = {&d_work_tiles, &total_work_size};

  cuda_assert(cuLaunchKernel(cuRender, 1, 1, 1, num_threads_per_block, 1, 1, 0, 0, args, 0));

  cuda_assert(cuCtxSynchronize());

  cuMemGetInfo(&free_after, &total);
  VLOG(1) << "Local memory reserved " << string_human_readable_number(free_before - free_after)
          << " bytes. (" << string_human_readable_size(free_before - free_after) << ")";

#  if 0
  /* For testing mapped host memory, fill up device memory. */
  const size_t keep_mb = 1024;

  while (free_after > keep_mb * 1024 * 1024LL) {
    CUdeviceptr tmp;
    cuda_assert(cuMemAlloc(&tmp, 10 * 1024 * 1024LL));
    cuMemGetInfo(&free_after, &total);
  }
#  endif
}

void CUDADevice::init_host_memory()
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

void CUDADevice::load_texture_info()
{
  if (need_texture_info) {
    texture_info.copy_to_device();
    need_texture_info = false;
  }
}

void CUDADevice::move_textures_to_host(size_t size, bool for_texture)
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

    foreach (CUDAMemMap::value_type &pair, cuda_mem_map) {
      device_memory &mem = *pair.first;
      CUDAMem *cmem = &pair.second;

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

    /* Move to host memory. This part is mutex protected since
     * multiple CUDA devices could be moving the memory. The
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

CUDADevice::CUDAMem *CUDADevice::generic_alloc(device_memory &mem, size_t pitch_padding)
{
  CUDAContextScope scope(this);

  CUdeviceptr device_pointer = 0;
  size_t size = mem.memory_size() + pitch_padding;

  CUresult mem_alloc_result = CUDA_ERROR_OUT_OF_MEMORY;
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
  cuMemGetInfo(&free, &total);

  /* Move textures to host memory if needed. */
  if (!move_texture_to_host && !is_image && (size + headroom) >= free && can_map_host) {
    move_textures_to_host(size + headroom - free, is_texture);
    cuMemGetInfo(&free, &total);
  }

  /* Allocate in device memory. */
  if (!move_texture_to_host && (size + headroom) < free) {
    mem_alloc_result = cuMemAlloc(&device_pointer, size);
    if (mem_alloc_result == CUDA_SUCCESS) {
      status = " in device memory";
    }
  }

  /* Fall back to mapped host memory if needed and possible. */

  void *shared_pointer = 0;

  if (mem_alloc_result != CUDA_SUCCESS && can_map_host) {
    if (mem.shared_pointer) {
      /* Another device already allocated host memory. */
      mem_alloc_result = CUDA_SUCCESS;
      shared_pointer = mem.shared_pointer;
    }
    else if (map_host_used + size < map_host_limit) {
      /* Allocate host memory ourselves. */
      mem_alloc_result = cuMemHostAlloc(
          &shared_pointer, size, CU_MEMHOSTALLOC_DEVICEMAP | CU_MEMHOSTALLOC_WRITECOMBINED);

      assert((mem_alloc_result == CUDA_SUCCESS && shared_pointer != 0) ||
             (mem_alloc_result != CUDA_SUCCESS && shared_pointer == 0));
    }

    if (mem_alloc_result == CUDA_SUCCESS) {
      cuda_assert(cuMemHostGetDevicePointer_v2(&device_pointer, shared_pointer, 0));
      map_host_used += size;
      status = " in host memory";
    }
  }

  if (mem_alloc_result != CUDA_SUCCESS) {
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
  CUDAMem *cmem = &cuda_mem_map[&mem];
  if (shared_pointer != 0) {
    /* Replace host pointer with our host allocation. Only works if
     * CUDA memory layout is the same and has no pitch padding. Also
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

void CUDADevice::generic_copy_to(device_memory &mem)
{
  if (!mem.host_pointer || !mem.device_pointer) {
    return;
  }

  /* If use_mapped_host of mem is false, the current device only uses device memory allocated by
   * cuMemAlloc regardless of mem.host_pointer and mem.shared_pointer, and should copy data from
   * mem.host_pointer. */
  if (!cuda_mem_map[&mem].use_mapped_host || mem.host_pointer != mem.shared_pointer) {
    const CUDAContextScope scope(this);
    cuda_assert(
        cuMemcpyHtoD((CUdeviceptr)mem.device_pointer, mem.host_pointer, mem.memory_size()));
  }
}

void CUDADevice::generic_free(device_memory &mem)
{
  if (mem.device_pointer) {
    CUDAContextScope scope(this);
    const CUDAMem &cmem = cuda_mem_map[&mem];

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
          cuMemFreeHost(mem.shared_pointer);
          mem.shared_pointer = 0;
        }
      }
      map_host_used -= mem.device_size;
    }
    else {
      /* Free device memory. */
      cuda_assert(cuMemFree(mem.device_pointer));
    }

    stats.mem_free(mem.device_size);
    mem.device_pointer = 0;
    mem.device_size = 0;

    cuda_mem_map.erase(cuda_mem_map.find(&mem));
  }
}

void CUDADevice::mem_alloc(device_memory &mem)
{
  if (mem.type == MEM_PIXELS && !background) {
    pixels_alloc(mem);
  }
  else if (mem.type == MEM_TEXTURE) {
    assert(!"mem_alloc not supported for textures.");
  }
  else if (mem.type == MEM_GLOBAL) {
    assert(!"mem_alloc not supported for global memory.");
  }
  else {
    generic_alloc(mem);
  }
}

void CUDADevice::mem_copy_to(device_memory &mem)
{
  if (mem.type == MEM_PIXELS) {
    assert(!"mem_copy_to not supported for pixels.");
  }
  else if (mem.type == MEM_GLOBAL) {
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

void CUDADevice::mem_copy_from(device_memory &mem, int y, int w, int h, int elem)
{
  if (mem.type == MEM_PIXELS && !background) {
    pixels_copy_from(mem, y, w, h);
  }
  else if (mem.type == MEM_TEXTURE || mem.type == MEM_GLOBAL) {
    assert(!"mem_copy_from not supported for textures.");
  }
  else if (mem.host_pointer) {
    const size_t size = elem * w * h;
    const size_t offset = elem * y * w;

    if (mem.device_pointer) {
      const CUDAContextScope scope(this);
      cuda_assert(cuMemcpyDtoH(
          (char *)mem.host_pointer + offset, (CUdeviceptr)mem.device_pointer + offset, size));
    }
    else {
      memset((char *)mem.host_pointer + offset, 0, size);
    }
  }
}

void CUDADevice::mem_zero(device_memory &mem)
{
  if (!mem.device_pointer) {
    mem_alloc(mem);
  }
  if (!mem.device_pointer) {
    return;
  }

  /* If use_mapped_host of mem is false, mem.device_pointer currently refers to device memory
   * regardless of mem.host_pointer and mem.shared_pointer. */
  if (!cuda_mem_map[&mem].use_mapped_host || mem.host_pointer != mem.shared_pointer) {
    const CUDAContextScope scope(this);
    cuda_assert(cuMemsetD8((CUdeviceptr)mem.device_pointer, 0, mem.memory_size()));
  }
  else if (mem.host_pointer) {
    memset(mem.host_pointer, 0, mem.memory_size());
  }
}

void CUDADevice::mem_free(device_memory &mem)
{
  if (mem.type == MEM_PIXELS && !background) {
    pixels_free(mem);
  }
  else if (mem.type == MEM_GLOBAL) {
    global_free(mem);
  }
  else if (mem.type == MEM_TEXTURE) {
    tex_free((device_texture &)mem);
  }
  else {
    generic_free(mem);
  }
}

device_ptr CUDADevice::mem_alloc_sub_ptr(device_memory &mem, int offset, int /*size*/)
{
  return (device_ptr)(((char *)mem.device_pointer) + mem.memory_elements_size(offset));
}

void CUDADevice::const_copy_to(const char *name, void *host, size_t size)
{
  CUDAContextScope scope(this);
  CUdeviceptr mem;
  size_t bytes;

  cuda_assert(cuModuleGetGlobal(&mem, &bytes, cuModule, name));
  // assert(bytes == size);
  cuda_assert(cuMemcpyHtoD(mem, host, size));
}

void CUDADevice::global_alloc(device_memory &mem)
{
  if (mem.is_resident(this)) {
    generic_alloc(mem);
    generic_copy_to(mem);
  }

  const_copy_to(mem.name, &mem.device_pointer, sizeof(mem.device_pointer));
}

void CUDADevice::global_free(device_memory &mem)
{
  if (mem.is_resident(this) && mem.device_pointer) {
    generic_free(mem);
  }
}

void CUDADevice::tex_alloc(device_texture &mem)
{
  CUDAContextScope scope(this);

  /* General variables for both architectures */
  string bind_name = mem.name;
  size_t dsize = datatype_size(mem.data_type);
  size_t size = mem.memory_size();

  CUaddress_mode address_mode = CU_TR_ADDRESS_MODE_WRAP;
  switch (mem.info.extension) {
    case EXTENSION_REPEAT:
      address_mode = CU_TR_ADDRESS_MODE_WRAP;
      break;
    case EXTENSION_EXTEND:
      address_mode = CU_TR_ADDRESS_MODE_CLAMP;
      break;
    case EXTENSION_CLIP:
      address_mode = CU_TR_ADDRESS_MODE_BORDER;
      break;
    default:
      assert(0);
      break;
  }

  CUfilter_mode filter_mode;
  if (mem.info.interpolation == INTERPOLATION_CLOSEST) {
    filter_mode = CU_TR_FILTER_MODE_POINT;
  }
  else {
    filter_mode = CU_TR_FILTER_MODE_LINEAR;
  }

  /* Image Texture Storage */
  CUarray_format_enum format;
  switch (mem.data_type) {
    case TYPE_UCHAR:
      format = CU_AD_FORMAT_UNSIGNED_INT8;
      break;
    case TYPE_UINT16:
      format = CU_AD_FORMAT_UNSIGNED_INT16;
      break;
    case TYPE_UINT:
      format = CU_AD_FORMAT_UNSIGNED_INT32;
      break;
    case TYPE_INT:
      format = CU_AD_FORMAT_SIGNED_INT32;
      break;
    case TYPE_FLOAT:
      format = CU_AD_FORMAT_FLOAT;
      break;
    case TYPE_HALF:
      format = CU_AD_FORMAT_HALF;
      break;
    default:
      assert(0);
      return;
  }

  CUDAMem *cmem = NULL;
  CUarray array_3d = NULL;
  size_t src_pitch = mem.data_width * dsize * mem.data_elements;
  size_t dst_pitch = src_pitch;

  if (!mem.is_resident(this)) {
    cmem = &cuda_mem_map[&mem];
    cmem->texobject = 0;

    if (mem.data_depth > 1) {
      array_3d = (CUarray)mem.device_pointer;
      cmem->array = array_3d;
    }
    else if (mem.data_height > 0) {
      dst_pitch = align_up(src_pitch, pitch_alignment);
    }
  }
  else if (mem.data_depth > 1) {
    /* 3D texture using array, there is no API for linear memory. */
    CUDA_ARRAY3D_DESCRIPTOR desc;

    desc.Width = mem.data_width;
    desc.Height = mem.data_height;
    desc.Depth = mem.data_depth;
    desc.Format = format;
    desc.NumChannels = mem.data_elements;
    desc.Flags = 0;

    VLOG(1) << "Array 3D allocate: " << mem.name << ", "
            << string_human_readable_number(mem.memory_size()) << " bytes. ("
            << string_human_readable_size(mem.memory_size()) << ")";

    cuda_assert(cuArray3DCreate(&array_3d, &desc));

    if (!array_3d) {
      return;
    }

    CUDA_MEMCPY3D param;
    memset(&param, 0, sizeof(param));
    param.dstMemoryType = CU_MEMORYTYPE_ARRAY;
    param.dstArray = array_3d;
    param.srcMemoryType = CU_MEMORYTYPE_HOST;
    param.srcHost = mem.host_pointer;
    param.srcPitch = src_pitch;
    param.WidthInBytes = param.srcPitch;
    param.Height = mem.data_height;
    param.Depth = mem.data_depth;

    cuda_assert(cuMemcpy3D(&param));

    mem.device_pointer = (device_ptr)array_3d;
    mem.device_size = size;
    stats.mem_alloc(size);

    cmem = &cuda_mem_map[&mem];
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

    CUDA_MEMCPY2D param;
    memset(&param, 0, sizeof(param));
    param.dstMemoryType = CU_MEMORYTYPE_DEVICE;
    param.dstDevice = mem.device_pointer;
    param.dstPitch = dst_pitch;
    param.srcMemoryType = CU_MEMORYTYPE_HOST;
    param.srcHost = mem.host_pointer;
    param.srcPitch = src_pitch;
    param.WidthInBytes = param.srcPitch;
    param.Height = mem.data_height;

    cuda_assert(cuMemcpy2DUnaligned(&param));
  }
  else {
    /* 1D texture, using linear memory. */
    cmem = generic_alloc(mem);
    if (!cmem) {
      return;
    }

    cuda_assert(cuMemcpyHtoD(mem.device_pointer, mem.host_pointer, size));
  }

  /* Kepler+, bindless textures. */
  CUDA_RESOURCE_DESC resDesc;
  memset(&resDesc, 0, sizeof(resDesc));

  if (array_3d) {
    resDesc.resType = CU_RESOURCE_TYPE_ARRAY;
    resDesc.res.array.hArray = array_3d;
    resDesc.flags = 0;
  }
  else if (mem.data_height > 0) {
    resDesc.resType = CU_RESOURCE_TYPE_PITCH2D;
    resDesc.res.pitch2D.devPtr = mem.device_pointer;
    resDesc.res.pitch2D.format = format;
    resDesc.res.pitch2D.numChannels = mem.data_elements;
    resDesc.res.pitch2D.height = mem.data_height;
    resDesc.res.pitch2D.width = mem.data_width;
    resDesc.res.pitch2D.pitchInBytes = dst_pitch;
  }
  else {
    resDesc.resType = CU_RESOURCE_TYPE_LINEAR;
    resDesc.res.linear.devPtr = mem.device_pointer;
    resDesc.res.linear.format = format;
    resDesc.res.linear.numChannels = mem.data_elements;
    resDesc.res.linear.sizeInBytes = mem.device_size;
  }

  CUDA_TEXTURE_DESC texDesc;
  memset(&texDesc, 0, sizeof(texDesc));
  texDesc.addressMode[0] = address_mode;
  texDesc.addressMode[1] = address_mode;
  texDesc.addressMode[2] = address_mode;
  texDesc.filterMode = filter_mode;
  texDesc.flags = CU_TRSF_NORMALIZED_COORDINATES;

  cuda_assert(cuTexObjectCreate(&cmem->texobject, &resDesc, &texDesc, NULL));

  /* Resize once */
  const uint slot = mem.slot;
  if (slot >= texture_info.size()) {
    /* Allocate some slots in advance, to reduce amount
     * of re-allocations. */
    texture_info.resize(slot + 128);
  }

  /* Set Mapping and tag that we need to (re-)upload to device */
  texture_info[slot] = mem.info;
  texture_info[slot].data = (uint64_t)cmem->texobject;
  need_texture_info = true;
}

void CUDADevice::tex_free(device_texture &mem)
{
  if (mem.device_pointer) {
    CUDAContextScope scope(this);
    const CUDAMem &cmem = cuda_mem_map[&mem];

    if (cmem.texobject) {
      /* Free bindless texture. */
      cuTexObjectDestroy(cmem.texobject);
    }

    if (!mem.is_resident(this)) {
      /* Do not free memory here, since it was allocated on a different device. */
      cuda_mem_map.erase(cuda_mem_map.find(&mem));
    }
    else if (cmem.array) {
      /* Free array. */
      cuArrayDestroy(cmem.array);
      stats.mem_free(mem.device_size);
      mem.device_pointer = 0;
      mem.device_size = 0;

      cuda_mem_map.erase(cuda_mem_map.find(&mem));
    }
    else {
      generic_free(mem);
    }
  }
}

#  define CUDA_GET_BLOCKSIZE(func, w, h) \
    int threads_per_block; \
    cuda_assert( \
        cuFuncGetAttribute(&threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, func)); \
    int threads = (int)sqrt((float)threads_per_block); \
    int xblocks = ((w) + threads - 1) / threads; \
    int yblocks = ((h) + threads - 1) / threads;

#  define CUDA_LAUNCH_KERNEL(func, args) \
    cuda_assert(cuLaunchKernel(func, xblocks, yblocks, 1, threads, threads, 1, 0, 0, args, 0));

/* Similar as above, but for 1-dimensional blocks. */
#  define CUDA_GET_BLOCKSIZE_1D(func, w, h) \
    int threads_per_block; \
    cuda_assert( \
        cuFuncGetAttribute(&threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, func)); \
    int xblocks = ((w) + threads_per_block - 1) / threads_per_block; \
    int yblocks = h;

#  define CUDA_LAUNCH_KERNEL_1D(func, args) \
    cuda_assert(cuLaunchKernel(func, xblocks, yblocks, 1, threads_per_block, 1, 1, 0, 0, args, 0));

bool CUDADevice::denoising_non_local_means(device_ptr image_ptr,
                                           device_ptr guide_ptr,
                                           device_ptr variance_ptr,
                                           device_ptr out_ptr,
                                           DenoisingTask *task)
{
  if (have_error())
    return false;

  CUDAContextScope scope(this);

  int stride = task->buffer.stride;
  int w = task->buffer.width;
  int h = task->buffer.h;
  int r = task->nlm_state.r;
  int f = task->nlm_state.f;
  float a = task->nlm_state.a;
  float k_2 = task->nlm_state.k_2;

  int pass_stride = task->buffer.pass_stride;
  int num_shifts = (2 * r + 1) * (2 * r + 1);
  int channel_offset = task->nlm_state.is_color ? task->buffer.pass_stride : 0;
  int frame_offset = 0;

  if (have_error())
    return false;

  CUdeviceptr difference = (CUdeviceptr)task->buffer.temporary_mem.device_pointer;
  CUdeviceptr blurDifference = difference + sizeof(float) * pass_stride * num_shifts;
  CUdeviceptr weightAccum = difference + 2 * sizeof(float) * pass_stride * num_shifts;
  CUdeviceptr scale_ptr = 0;

  cuda_assert(cuMemsetD8(weightAccum, 0, sizeof(float) * pass_stride));
  cuda_assert(cuMemsetD8(out_ptr, 0, sizeof(float) * pass_stride));

  {
    CUfunction cuNLMCalcDifference, cuNLMBlur, cuNLMCalcWeight, cuNLMUpdateOutput;
    cuda_assert(cuModuleGetFunction(
        &cuNLMCalcDifference, cuFilterModule, "kernel_cuda_filter_nlm_calc_difference"));
    cuda_assert(cuModuleGetFunction(&cuNLMBlur, cuFilterModule, "kernel_cuda_filter_nlm_blur"));
    cuda_assert(cuModuleGetFunction(
        &cuNLMCalcWeight, cuFilterModule, "kernel_cuda_filter_nlm_calc_weight"));
    cuda_assert(cuModuleGetFunction(
        &cuNLMUpdateOutput, cuFilterModule, "kernel_cuda_filter_nlm_update_output"));

    cuda_assert(cuFuncSetCacheConfig(cuNLMCalcDifference, CU_FUNC_CACHE_PREFER_L1));
    cuda_assert(cuFuncSetCacheConfig(cuNLMBlur, CU_FUNC_CACHE_PREFER_L1));
    cuda_assert(cuFuncSetCacheConfig(cuNLMCalcWeight, CU_FUNC_CACHE_PREFER_L1));
    cuda_assert(cuFuncSetCacheConfig(cuNLMUpdateOutput, CU_FUNC_CACHE_PREFER_L1));

    CUDA_GET_BLOCKSIZE_1D(cuNLMCalcDifference, w * h, num_shifts);

    void *calc_difference_args[] = {&guide_ptr,
                                    &variance_ptr,
                                    &scale_ptr,
                                    &difference,
                                    &w,
                                    &h,
                                    &stride,
                                    &pass_stride,
                                    &r,
                                    &channel_offset,
                                    &frame_offset,
                                    &a,
                                    &k_2};
    void *blur_args[] = {&difference, &blurDifference, &w, &h, &stride, &pass_stride, &r, &f};
    void *calc_weight_args[] = {
        &blurDifference, &difference, &w, &h, &stride, &pass_stride, &r, &f};
    void *update_output_args[] = {&blurDifference,
                                  &image_ptr,
                                  &out_ptr,
                                  &weightAccum,
                                  &w,
                                  &h,
                                  &stride,
                                  &pass_stride,
                                  &channel_offset,
                                  &r,
                                  &f};

    CUDA_LAUNCH_KERNEL_1D(cuNLMCalcDifference, calc_difference_args);
    CUDA_LAUNCH_KERNEL_1D(cuNLMBlur, blur_args);
    CUDA_LAUNCH_KERNEL_1D(cuNLMCalcWeight, calc_weight_args);
    CUDA_LAUNCH_KERNEL_1D(cuNLMBlur, blur_args);
    CUDA_LAUNCH_KERNEL_1D(cuNLMUpdateOutput, update_output_args);
  }

  {
    CUfunction cuNLMNormalize;
    cuda_assert(
        cuModuleGetFunction(&cuNLMNormalize, cuFilterModule, "kernel_cuda_filter_nlm_normalize"));
    cuda_assert(cuFuncSetCacheConfig(cuNLMNormalize, CU_FUNC_CACHE_PREFER_L1));
    void *normalize_args[] = {&out_ptr, &weightAccum, &w, &h, &stride};
    CUDA_GET_BLOCKSIZE(cuNLMNormalize, w, h);
    CUDA_LAUNCH_KERNEL(cuNLMNormalize, normalize_args);
    cuda_assert(cuCtxSynchronize());
  }

  return !have_error();
}

bool CUDADevice::denoising_construct_transform(DenoisingTask *task)
{
  if (have_error())
    return false;

  CUDAContextScope scope(this);

  CUfunction cuFilterConstructTransform;
  cuda_assert(cuModuleGetFunction(
      &cuFilterConstructTransform, cuFilterModule, "kernel_cuda_filter_construct_transform"));
  cuda_assert(cuFuncSetCacheConfig(cuFilterConstructTransform, CU_FUNC_CACHE_PREFER_SHARED));
  CUDA_GET_BLOCKSIZE(cuFilterConstructTransform, task->storage.w, task->storage.h);

  void *args[] = {&task->buffer.mem.device_pointer,
                  &task->tile_info_mem.device_pointer,
                  &task->storage.transform.device_pointer,
                  &task->storage.rank.device_pointer,
                  &task->filter_area,
                  &task->rect,
                  &task->radius,
                  &task->pca_threshold,
                  &task->buffer.pass_stride,
                  &task->buffer.frame_stride,
                  &task->buffer.use_time};
  CUDA_LAUNCH_KERNEL(cuFilterConstructTransform, args);
  cuda_assert(cuCtxSynchronize());

  return !have_error();
}

bool CUDADevice::denoising_accumulate(device_ptr color_ptr,
                                      device_ptr color_variance_ptr,
                                      device_ptr scale_ptr,
                                      int frame,
                                      DenoisingTask *task)
{
  if (have_error())
    return false;

  CUDAContextScope scope(this);

  int r = task->radius;
  int f = 4;
  float a = 1.0f;
  float k_2 = task->nlm_k_2;

  int w = task->reconstruction_state.source_w;
  int h = task->reconstruction_state.source_h;
  int stride = task->buffer.stride;
  int frame_offset = frame * task->buffer.frame_stride;
  int t = task->tile_info->frames[frame];

  int pass_stride = task->buffer.pass_stride;
  int num_shifts = (2 * r + 1) * (2 * r + 1);

  if (have_error())
    return false;

  CUdeviceptr difference = (CUdeviceptr)task->buffer.temporary_mem.device_pointer;
  CUdeviceptr blurDifference = difference + sizeof(float) * pass_stride * num_shifts;

  CUfunction cuNLMCalcDifference, cuNLMBlur, cuNLMCalcWeight, cuNLMConstructGramian;
  cuda_assert(cuModuleGetFunction(
      &cuNLMCalcDifference, cuFilterModule, "kernel_cuda_filter_nlm_calc_difference"));
  cuda_assert(cuModuleGetFunction(&cuNLMBlur, cuFilterModule, "kernel_cuda_filter_nlm_blur"));
  cuda_assert(
      cuModuleGetFunction(&cuNLMCalcWeight, cuFilterModule, "kernel_cuda_filter_nlm_calc_weight"));
  cuda_assert(cuModuleGetFunction(
      &cuNLMConstructGramian, cuFilterModule, "kernel_cuda_filter_nlm_construct_gramian"));

  cuda_assert(cuFuncSetCacheConfig(cuNLMCalcDifference, CU_FUNC_CACHE_PREFER_L1));
  cuda_assert(cuFuncSetCacheConfig(cuNLMBlur, CU_FUNC_CACHE_PREFER_L1));
  cuda_assert(cuFuncSetCacheConfig(cuNLMCalcWeight, CU_FUNC_CACHE_PREFER_L1));
  cuda_assert(cuFuncSetCacheConfig(cuNLMConstructGramian, CU_FUNC_CACHE_PREFER_SHARED));

  CUDA_GET_BLOCKSIZE_1D(cuNLMCalcDifference,
                        task->reconstruction_state.source_w * task->reconstruction_state.source_h,
                        num_shifts);

  void *calc_difference_args[] = {&color_ptr,
                                  &color_variance_ptr,
                                  &scale_ptr,
                                  &difference,
                                  &w,
                                  &h,
                                  &stride,
                                  &pass_stride,
                                  &r,
                                  &pass_stride,
                                  &frame_offset,
                                  &a,
                                  &k_2};
  void *blur_args[] = {&difference, &blurDifference, &w, &h, &stride, &pass_stride, &r, &f};
  void *calc_weight_args[] = {&blurDifference, &difference, &w, &h, &stride, &pass_stride, &r, &f};
  void *construct_gramian_args[] = {&t,
                                    &blurDifference,
                                    &task->buffer.mem.device_pointer,
                                    &task->storage.transform.device_pointer,
                                    &task->storage.rank.device_pointer,
                                    &task->storage.XtWX.device_pointer,
                                    &task->storage.XtWY.device_pointer,
                                    &task->reconstruction_state.filter_window,
                                    &w,
                                    &h,
                                    &stride,
                                    &pass_stride,
                                    &r,
                                    &f,
                                    &frame_offset,
                                    &task->buffer.use_time};

  CUDA_LAUNCH_KERNEL_1D(cuNLMCalcDifference, calc_difference_args);
  CUDA_LAUNCH_KERNEL_1D(cuNLMBlur, blur_args);
  CUDA_LAUNCH_KERNEL_1D(cuNLMCalcWeight, calc_weight_args);
  CUDA_LAUNCH_KERNEL_1D(cuNLMBlur, blur_args);
  CUDA_LAUNCH_KERNEL_1D(cuNLMConstructGramian, construct_gramian_args);
  cuda_assert(cuCtxSynchronize());

  return !have_error();
}

bool CUDADevice::denoising_solve(device_ptr output_ptr, DenoisingTask *task)
{
  CUfunction cuFinalize;
  cuda_assert(cuModuleGetFunction(&cuFinalize, cuFilterModule, "kernel_cuda_filter_finalize"));
  cuda_assert(cuFuncSetCacheConfig(cuFinalize, CU_FUNC_CACHE_PREFER_L1));
  void *finalize_args[] = {&output_ptr,
                           &task->storage.rank.device_pointer,
                           &task->storage.XtWX.device_pointer,
                           &task->storage.XtWY.device_pointer,
                           &task->filter_area,
                           &task->reconstruction_state.buffer_params.x,
                           &task->render_buffer.samples};
  CUDA_GET_BLOCKSIZE(
      cuFinalize, task->reconstruction_state.source_w, task->reconstruction_state.source_h);
  CUDA_LAUNCH_KERNEL(cuFinalize, finalize_args);
  cuda_assert(cuCtxSynchronize());

  return !have_error();
}

bool CUDADevice::denoising_combine_halves(device_ptr a_ptr,
                                          device_ptr b_ptr,
                                          device_ptr mean_ptr,
                                          device_ptr variance_ptr,
                                          int r,
                                          int4 rect,
                                          DenoisingTask *task)
{
  if (have_error())
    return false;

  CUDAContextScope scope(this);

  CUfunction cuFilterCombineHalves;
  cuda_assert(cuModuleGetFunction(
      &cuFilterCombineHalves, cuFilterModule, "kernel_cuda_filter_combine_halves"));
  cuda_assert(cuFuncSetCacheConfig(cuFilterCombineHalves, CU_FUNC_CACHE_PREFER_L1));
  CUDA_GET_BLOCKSIZE(
      cuFilterCombineHalves, task->rect.z - task->rect.x, task->rect.w - task->rect.y);

  void *args[] = {&mean_ptr, &variance_ptr, &a_ptr, &b_ptr, &rect, &r};
  CUDA_LAUNCH_KERNEL(cuFilterCombineHalves, args);
  cuda_assert(cuCtxSynchronize());

  return !have_error();
}

bool CUDADevice::denoising_divide_shadow(device_ptr a_ptr,
                                         device_ptr b_ptr,
                                         device_ptr sample_variance_ptr,
                                         device_ptr sv_variance_ptr,
                                         device_ptr buffer_variance_ptr,
                                         DenoisingTask *task)
{
  if (have_error())
    return false;

  CUDAContextScope scope(this);

  CUfunction cuFilterDivideShadow;
  cuda_assert(cuModuleGetFunction(
      &cuFilterDivideShadow, cuFilterModule, "kernel_cuda_filter_divide_shadow"));
  cuda_assert(cuFuncSetCacheConfig(cuFilterDivideShadow, CU_FUNC_CACHE_PREFER_L1));
  CUDA_GET_BLOCKSIZE(
      cuFilterDivideShadow, task->rect.z - task->rect.x, task->rect.w - task->rect.y);

  void *args[] = {&task->render_buffer.samples,
                  &task->tile_info_mem.device_pointer,
                  &a_ptr,
                  &b_ptr,
                  &sample_variance_ptr,
                  &sv_variance_ptr,
                  &buffer_variance_ptr,
                  &task->rect,
                  &task->render_buffer.pass_stride,
                  &task->render_buffer.offset};
  CUDA_LAUNCH_KERNEL(cuFilterDivideShadow, args);
  cuda_assert(cuCtxSynchronize());

  return !have_error();
}

bool CUDADevice::denoising_get_feature(int mean_offset,
                                       int variance_offset,
                                       device_ptr mean_ptr,
                                       device_ptr variance_ptr,
                                       float scale,
                                       DenoisingTask *task)
{
  if (have_error())
    return false;

  CUDAContextScope scope(this);

  CUfunction cuFilterGetFeature;
  cuda_assert(
      cuModuleGetFunction(&cuFilterGetFeature, cuFilterModule, "kernel_cuda_filter_get_feature"));
  cuda_assert(cuFuncSetCacheConfig(cuFilterGetFeature, CU_FUNC_CACHE_PREFER_L1));
  CUDA_GET_BLOCKSIZE(cuFilterGetFeature, task->rect.z - task->rect.x, task->rect.w - task->rect.y);

  void *args[] = {&task->render_buffer.samples,
                  &task->tile_info_mem.device_pointer,
                  &mean_offset,
                  &variance_offset,
                  &mean_ptr,
                  &variance_ptr,
                  &scale,
                  &task->rect,
                  &task->render_buffer.pass_stride,
                  &task->render_buffer.offset};
  CUDA_LAUNCH_KERNEL(cuFilterGetFeature, args);
  cuda_assert(cuCtxSynchronize());

  return !have_error();
}

bool CUDADevice::denoising_write_feature(int out_offset,
                                         device_ptr from_ptr,
                                         device_ptr buffer_ptr,
                                         DenoisingTask *task)
{
  if (have_error())
    return false;

  CUDAContextScope scope(this);

  CUfunction cuFilterWriteFeature;
  cuda_assert(cuModuleGetFunction(
      &cuFilterWriteFeature, cuFilterModule, "kernel_cuda_filter_write_feature"));
  cuda_assert(cuFuncSetCacheConfig(cuFilterWriteFeature, CU_FUNC_CACHE_PREFER_L1));
  CUDA_GET_BLOCKSIZE(cuFilterWriteFeature, task->filter_area.z, task->filter_area.w);

  void *args[] = {&task->render_buffer.samples,
                  &task->reconstruction_state.buffer_params,
                  &task->filter_area,
                  &from_ptr,
                  &buffer_ptr,
                  &out_offset,
                  &task->rect};
  CUDA_LAUNCH_KERNEL(cuFilterWriteFeature, args);
  cuda_assert(cuCtxSynchronize());

  return !have_error();
}

bool CUDADevice::denoising_detect_outliers(device_ptr image_ptr,
                                           device_ptr variance_ptr,
                                           device_ptr depth_ptr,
                                           device_ptr output_ptr,
                                           DenoisingTask *task)
{
  if (have_error())
    return false;

  CUDAContextScope scope(this);

  CUfunction cuFilterDetectOutliers;
  cuda_assert(cuModuleGetFunction(
      &cuFilterDetectOutliers, cuFilterModule, "kernel_cuda_filter_detect_outliers"));
  cuda_assert(cuFuncSetCacheConfig(cuFilterDetectOutliers, CU_FUNC_CACHE_PREFER_L1));
  CUDA_GET_BLOCKSIZE(
      cuFilterDetectOutliers, task->rect.z - task->rect.x, task->rect.w - task->rect.y);

  void *args[] = {
      &image_ptr, &variance_ptr, &depth_ptr, &output_ptr, &task->rect, &task->buffer.pass_stride};

  CUDA_LAUNCH_KERNEL(cuFilterDetectOutliers, args);
  cuda_assert(cuCtxSynchronize());

  return !have_error();
}

void CUDADevice::denoise(RenderTile &rtile, DenoisingTask &denoising)
{
  denoising.functions.construct_transform = function_bind(
      &CUDADevice::denoising_construct_transform, this, &denoising);
  denoising.functions.accumulate = function_bind(
      &CUDADevice::denoising_accumulate, this, _1, _2, _3, _4, &denoising);
  denoising.functions.solve = function_bind(&CUDADevice::denoising_solve, this, _1, &denoising);
  denoising.functions.divide_shadow = function_bind(
      &CUDADevice::denoising_divide_shadow, this, _1, _2, _3, _4, _5, &denoising);
  denoising.functions.non_local_means = function_bind(
      &CUDADevice::denoising_non_local_means, this, _1, _2, _3, _4, &denoising);
  denoising.functions.combine_halves = function_bind(
      &CUDADevice::denoising_combine_halves, this, _1, _2, _3, _4, _5, _6, &denoising);
  denoising.functions.get_feature = function_bind(
      &CUDADevice::denoising_get_feature, this, _1, _2, _3, _4, _5, &denoising);
  denoising.functions.write_feature = function_bind(
      &CUDADevice::denoising_write_feature, this, _1, _2, _3, &denoising);
  denoising.functions.detect_outliers = function_bind(
      &CUDADevice::denoising_detect_outliers, this, _1, _2, _3, _4, &denoising);

  denoising.filter_area = make_int4(rtile.x, rtile.y, rtile.w, rtile.h);
  denoising.render_buffer.samples = rtile.sample;
  denoising.buffer.gpu_temporary_mem = true;

  denoising.run_denoising(&rtile);
}

void CUDADevice::adaptive_sampling_filter(uint filter_sample,
                                          WorkTile *wtile,
                                          CUdeviceptr d_wtile,
                                          CUstream stream)
{
  const int num_threads_per_block = functions.adaptive_num_threads_per_block;

  /* These are a series of tiny kernels because there is no grid synchronization
   * from within a kernel, so multiple kernel launches it is. */
  uint total_work_size = wtile->h * wtile->w;
  void *args2[] = {&d_wtile, &filter_sample, &total_work_size};
  uint num_blocks = divide_up(total_work_size, num_threads_per_block);
  cuda_assert(cuLaunchKernel(functions.adaptive_stopping,
                             num_blocks,
                             1,
                             1,
                             num_threads_per_block,
                             1,
                             1,
                             0,
                             stream,
                             args2,
                             0));
  total_work_size = wtile->h;
  num_blocks = divide_up(total_work_size, num_threads_per_block);
  cuda_assert(cuLaunchKernel(functions.adaptive_filter_x,
                             num_blocks,
                             1,
                             1,
                             num_threads_per_block,
                             1,
                             1,
                             0,
                             stream,
                             args2,
                             0));
  total_work_size = wtile->w;
  num_blocks = divide_up(total_work_size, num_threads_per_block);
  cuda_assert(cuLaunchKernel(functions.adaptive_filter_y,
                             num_blocks,
                             1,
                             1,
                             num_threads_per_block,
                             1,
                             1,
                             0,
                             stream,
                             args2,
                             0));
}

void CUDADevice::adaptive_sampling_post(RenderTile &rtile,
                                        WorkTile *wtile,
                                        CUdeviceptr d_wtile,
                                        CUstream stream)
{
  const int num_threads_per_block = functions.adaptive_num_threads_per_block;
  uint total_work_size = wtile->h * wtile->w;

  void *args[] = {&d_wtile, &rtile.start_sample, &rtile.sample, &total_work_size};
  uint num_blocks = divide_up(total_work_size, num_threads_per_block);
  cuda_assert(cuLaunchKernel(functions.adaptive_scale_samples,
                             num_blocks,
                             1,
                             1,
                             num_threads_per_block,
                             1,
                             1,
                             0,
                             stream,
                             args,
                             0));
}

void CUDADevice::render(DeviceTask &task, RenderTile &rtile, device_vector<WorkTile> &work_tiles)
{
  scoped_timer timer(&rtile.buffers->render_time);

  if (have_error())
    return;

  CUDAContextScope scope(this);
  CUfunction cuRender;

  /* Get kernel function. */
  if (rtile.task == RenderTile::BAKE) {
    cuda_assert(cuModuleGetFunction(&cuRender, cuModule, "kernel_cuda_bake"));
  }
  else if (task.integrator_branched) {
    cuda_assert(cuModuleGetFunction(&cuRender, cuModule, "kernel_cuda_branched_path_trace"));
  }
  else {
    cuda_assert(cuModuleGetFunction(&cuRender, cuModule, "kernel_cuda_path_trace"));
  }

  if (have_error()) {
    return;
  }

  cuda_assert(cuFuncSetCacheConfig(cuRender, CU_FUNC_CACHE_PREFER_L1));

  /* Allocate work tile. */
  work_tiles.alloc(1);

  WorkTile *wtile = work_tiles.data();
  wtile->x = rtile.x;
  wtile->y = rtile.y;
  wtile->w = rtile.w;
  wtile->h = rtile.h;
  wtile->offset = rtile.offset;
  wtile->stride = rtile.stride;
  wtile->buffer = (float *)(CUdeviceptr)rtile.buffer;

  /* Prepare work size. More step samples render faster, but for now we
   * remain conservative for GPUs connected to a display to avoid driver
   * timeouts and display freezing. */
  int min_blocks, num_threads_per_block;
  cuda_assert(
      cuOccupancyMaxPotentialBlockSize(&min_blocks, &num_threads_per_block, cuRender, NULL, 0, 0));
  if (!info.display_device) {
    min_blocks *= 8;
  }

  uint step_samples = divide_up(min_blocks * num_threads_per_block, wtile->w * wtile->h);
  if (task.adaptive_sampling.use) {
    step_samples = task.adaptive_sampling.align_static_samples(step_samples);
  }

  /* Render all samples. */
  int start_sample = rtile.start_sample;
  int end_sample = rtile.start_sample + rtile.num_samples;

  for (int sample = start_sample; sample < end_sample; sample += step_samples) {
    /* Setup and copy work tile to device. */
    wtile->start_sample = sample;
    wtile->num_samples = min(step_samples, end_sample - sample);
    work_tiles.copy_to_device();

    CUdeviceptr d_work_tiles = (CUdeviceptr)work_tiles.device_pointer;
    uint total_work_size = wtile->w * wtile->h * wtile->num_samples;
    uint num_blocks = divide_up(total_work_size, num_threads_per_block);

    /* Launch kernel. */
    void *args[] = {&d_work_tiles, &total_work_size};

    cuda_assert(
        cuLaunchKernel(cuRender, num_blocks, 1, 1, num_threads_per_block, 1, 1, 0, 0, args, 0));

    /* Run the adaptive sampling kernels at selected samples aligned to step samples. */
    uint filter_sample = sample + wtile->num_samples - 1;
    if (task.adaptive_sampling.use && task.adaptive_sampling.need_filter(filter_sample)) {
      adaptive_sampling_filter(filter_sample, wtile, d_work_tiles);
    }

    cuda_assert(cuCtxSynchronize());

    /* Update progress. */
    rtile.sample = sample + wtile->num_samples;
    task.update_progress(&rtile, rtile.w * rtile.h * wtile->num_samples);

    if (task.get_cancel()) {
      if (task.need_finish_queue == false)
        break;
    }
  }

  /* Finalize adaptive sampling. */
  if (task.adaptive_sampling.use) {
    CUdeviceptr d_work_tiles = (CUdeviceptr)work_tiles.device_pointer;
    adaptive_sampling_post(rtile, wtile, d_work_tiles);
    cuda_assert(cuCtxSynchronize());
    task.update_progress(&rtile, rtile.w * rtile.h * wtile->num_samples);
  }
}

void CUDADevice::film_convert(DeviceTask &task,
                              device_ptr buffer,
                              device_ptr rgba_byte,
                              device_ptr rgba_half)
{
  if (have_error())
    return;

  CUDAContextScope scope(this);

  CUfunction cuFilmConvert;
  CUdeviceptr d_rgba = map_pixels((rgba_byte) ? rgba_byte : rgba_half);
  CUdeviceptr d_buffer = (CUdeviceptr)buffer;

  /* get kernel function */
  if (rgba_half) {
    cuda_assert(
        cuModuleGetFunction(&cuFilmConvert, cuModule, "kernel_cuda_convert_to_half_float"));
  }
  else {
    cuda_assert(cuModuleGetFunction(&cuFilmConvert, cuModule, "kernel_cuda_convert_to_byte"));
  }

  float sample_scale = 1.0f / (task.sample + 1);

  /* pass in parameters */
  void *args[] = {&d_rgba,
                  &d_buffer,
                  &sample_scale,
                  &task.x,
                  &task.y,
                  &task.w,
                  &task.h,
                  &task.offset,
                  &task.stride};

  /* launch kernel */
  int threads_per_block;
  cuda_assert(cuFuncGetAttribute(
      &threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, cuFilmConvert));

  int xthreads = (int)sqrt(threads_per_block);
  int ythreads = (int)sqrt(threads_per_block);
  int xblocks = (task.w + xthreads - 1) / xthreads;
  int yblocks = (task.h + ythreads - 1) / ythreads;

  cuda_assert(cuFuncSetCacheConfig(cuFilmConvert, CU_FUNC_CACHE_PREFER_L1));

  cuda_assert(cuLaunchKernel(cuFilmConvert,
                             xblocks,
                             yblocks,
                             1, /* blocks */
                             xthreads,
                             ythreads,
                             1, /* threads */
                             0,
                             0,
                             args,
                             0));

  unmap_pixels((rgba_byte) ? rgba_byte : rgba_half);

  cuda_assert(cuCtxSynchronize());
}

void CUDADevice::shader(DeviceTask &task)
{
  if (have_error())
    return;

  CUDAContextScope scope(this);

  CUfunction cuShader;
  CUdeviceptr d_input = (CUdeviceptr)task.shader_input;
  CUdeviceptr d_output = (CUdeviceptr)task.shader_output;

  /* get kernel function */
  if (task.shader_eval_type == SHADER_EVAL_DISPLACE) {
    cuda_assert(cuModuleGetFunction(&cuShader, cuModule, "kernel_cuda_displace"));
  }
  else {
    cuda_assert(cuModuleGetFunction(&cuShader, cuModule, "kernel_cuda_background"));
  }

  /* do tasks in smaller chunks, so we can cancel it */
  const int shader_chunk_size = 65536;
  const int start = task.shader_x;
  const int end = task.shader_x + task.shader_w;
  int offset = task.offset;

  bool canceled = false;
  for (int sample = 0; sample < task.num_samples && !canceled; sample++) {
    for (int shader_x = start; shader_x < end; shader_x += shader_chunk_size) {
      int shader_w = min(shader_chunk_size, end - shader_x);

      /* pass in parameters */
      void *args[8];
      int arg = 0;
      args[arg++] = &d_input;
      args[arg++] = &d_output;
      args[arg++] = &task.shader_eval_type;
      if (task.shader_eval_type >= SHADER_EVAL_BAKE) {
        args[arg++] = &task.shader_filter;
      }
      args[arg++] = &shader_x;
      args[arg++] = &shader_w;
      args[arg++] = &offset;
      args[arg++] = &sample;

      /* launch kernel */
      int threads_per_block;
      cuda_assert(cuFuncGetAttribute(
          &threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, cuShader));

      int xblocks = (shader_w + threads_per_block - 1) / threads_per_block;

      cuda_assert(cuFuncSetCacheConfig(cuShader, CU_FUNC_CACHE_PREFER_L1));
      cuda_assert(cuLaunchKernel(cuShader,
                                 xblocks,
                                 1,
                                 1, /* blocks */
                                 threads_per_block,
                                 1,
                                 1, /* threads */
                                 0,
                                 0,
                                 args,
                                 0));

      cuda_assert(cuCtxSynchronize());

      if (task.get_cancel()) {
        canceled = true;
        break;
      }
    }

    task.update_progress(NULL);
  }
}

CUdeviceptr CUDADevice::map_pixels(device_ptr mem)
{
  if (!background) {
    PixelMem pmem = pixel_mem_map[mem];
    CUdeviceptr buffer;

    size_t bytes;
    cuda_assert(cuGraphicsMapResources(1, &pmem.cuPBOresource, 0));
    cuda_assert(cuGraphicsResourceGetMappedPointer(&buffer, &bytes, pmem.cuPBOresource));

    return buffer;
  }

  return (CUdeviceptr)mem;
}

void CUDADevice::unmap_pixels(device_ptr mem)
{
  if (!background) {
    PixelMem pmem = pixel_mem_map[mem];

    cuda_assert(cuGraphicsUnmapResources(1, &pmem.cuPBOresource, 0));
  }
}

void CUDADevice::pixels_alloc(device_memory &mem)
{
  PixelMem pmem;

  pmem.w = mem.data_width;
  pmem.h = mem.data_height;

  CUDAContextScope scope(this);

  glGenBuffers(1, &pmem.cuPBO);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pmem.cuPBO);
  if (mem.data_type == TYPE_HALF)
    glBufferData(
        GL_PIXEL_UNPACK_BUFFER, pmem.w * pmem.h * sizeof(GLhalf) * 4, NULL, GL_DYNAMIC_DRAW);
  else
    glBufferData(
        GL_PIXEL_UNPACK_BUFFER, pmem.w * pmem.h * sizeof(uint8_t) * 4, NULL, GL_DYNAMIC_DRAW);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &pmem.cuTexId);
  glBindTexture(GL_TEXTURE_2D, pmem.cuTexId);
  if (mem.data_type == TYPE_HALF)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, pmem.w, pmem.h, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
  else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pmem.w, pmem.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, 0);

  CUresult result = cuGraphicsGLRegisterBuffer(
      &pmem.cuPBOresource, pmem.cuPBO, CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);

  if (result == CUDA_SUCCESS) {
    mem.device_pointer = pmem.cuTexId;
    pixel_mem_map[mem.device_pointer] = pmem;

    mem.device_size = mem.memory_size();
    stats.mem_alloc(mem.device_size);

    return;
  }
  else {
    /* failed to register buffer, fallback to no interop */
    glDeleteBuffers(1, &pmem.cuPBO);
    glDeleteTextures(1, &pmem.cuTexId);

    background = true;
  }
}

void CUDADevice::pixels_copy_from(device_memory &mem, int y, int w, int h)
{
  PixelMem pmem = pixel_mem_map[mem.device_pointer];

  CUDAContextScope scope(this);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pmem.cuPBO);
  uchar *pixels = (uchar *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_ONLY);
  size_t offset = sizeof(uchar) * 4 * y * w;
  memcpy((uchar *)mem.host_pointer + offset, pixels + offset, sizeof(uchar) * 4 * w * h);
  glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void CUDADevice::pixels_free(device_memory &mem)
{
  if (mem.device_pointer) {
    PixelMem pmem = pixel_mem_map[mem.device_pointer];

    CUDAContextScope scope(this);

    cuda_assert(cuGraphicsUnregisterResource(pmem.cuPBOresource));
    glDeleteBuffers(1, &pmem.cuPBO);
    glDeleteTextures(1, &pmem.cuTexId);

    pixel_mem_map.erase(pixel_mem_map.find(mem.device_pointer));
    mem.device_pointer = 0;

    stats.mem_free(mem.device_size);
    mem.device_size = 0;
  }
}

void CUDADevice::draw_pixels(device_memory &mem,
                             int y,
                             int w,
                             int h,
                             int width,
                             int height,
                             int dx,
                             int dy,
                             int dw,
                             int dh,
                             bool transparent,
                             const DeviceDrawParams &draw_params)
{
  assert(mem.type == MEM_PIXELS);

  if (!background) {
    const bool use_fallback_shader = (draw_params.bind_display_space_shader_cb == NULL);
    PixelMem pmem = pixel_mem_map[mem.device_pointer];
    float *vpointer;

    CUDAContextScope scope(this);

    /* for multi devices, this assumes the inefficient method that we allocate
     * all pixels on the device even though we only render to a subset */
    size_t offset = 4 * y * w;

    if (mem.data_type == TYPE_HALF)
      offset *= sizeof(GLhalf);
    else
      offset *= sizeof(uint8_t);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pmem.cuPBO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, pmem.cuTexId);
    if (mem.data_type == TYPE_HALF) {
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_HALF_FLOAT, (void *)offset);
    }
    else {
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, (void *)offset);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    if (transparent) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }

    GLint shader_program;
    if (use_fallback_shader) {
      if (!bind_fallback_display_space_shader(dw, dh)) {
        return;
      }
      shader_program = fallback_shader_program;
    }
    else {
      draw_params.bind_display_space_shader_cb();
      glGetIntegerv(GL_CURRENT_PROGRAM, &shader_program);
    }

    if (!vertex_buffer) {
      glGenBuffers(1, &vertex_buffer);
    }

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    /* invalidate old contents -
     * avoids stalling if buffer is still waiting in queue to be rendered */
    glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), NULL, GL_STREAM_DRAW);

    vpointer = (float *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

    if (vpointer) {
      /* texture coordinate - vertex pair */
      vpointer[0] = 0.0f;
      vpointer[1] = 0.0f;
      vpointer[2] = dx;
      vpointer[3] = dy;

      vpointer[4] = (float)w / (float)pmem.w;
      vpointer[5] = 0.0f;
      vpointer[6] = (float)width + dx;
      vpointer[7] = dy;

      vpointer[8] = (float)w / (float)pmem.w;
      vpointer[9] = (float)h / (float)pmem.h;
      vpointer[10] = (float)width + dx;
      vpointer[11] = (float)height + dy;

      vpointer[12] = 0.0f;
      vpointer[13] = (float)h / (float)pmem.h;
      vpointer[14] = dx;
      vpointer[15] = (float)height + dy;

      glUnmapBuffer(GL_ARRAY_BUFFER);
    }

    GLuint vertex_array_object;
    GLuint position_attribute, texcoord_attribute;

    glGenVertexArrays(1, &vertex_array_object);
    glBindVertexArray(vertex_array_object);

    texcoord_attribute = glGetAttribLocation(shader_program, "texCoord");
    position_attribute = glGetAttribLocation(shader_program, "pos");

    glEnableVertexAttribArray(texcoord_attribute);
    glEnableVertexAttribArray(position_attribute);

    glVertexAttribPointer(
        texcoord_attribute, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const GLvoid *)0);
    glVertexAttribPointer(position_attribute,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          4 * sizeof(float),
                          (const GLvoid *)(sizeof(float) * 2));

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    if (use_fallback_shader) {
      glUseProgram(0);
    }
    else {
      draw_params.unbind_display_space_shader_cb();
    }

    if (transparent) {
      glDisable(GL_BLEND);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    return;
  }

  Device::draw_pixels(mem, y, w, h, width, height, dx, dy, dw, dh, transparent, draw_params);
}

void CUDADevice::thread_run(DeviceTask *task)
{
  CUDAContextScope scope(this);

  if (task->type == DeviceTask::RENDER) {
    DeviceRequestedFeatures requested_features;
    if (use_split_kernel()) {
      if (split_kernel == NULL) {
        split_kernel = new CUDASplitKernel(this);
        split_kernel->load_kernels(requested_features);
      }
    }

    device_vector<WorkTile> work_tiles(this, "work_tiles", MEM_READ_ONLY);

    /* keep rendering tiles until done */
    RenderTile tile;
    DenoisingTask denoising(this, *task);

    while (task->acquire_tile(this, tile, task->tile_types)) {
      if (tile.task == RenderTile::PATH_TRACE) {
        if (use_split_kernel()) {
          device_only_memory<uchar> void_buffer(this, "void_buffer");
          split_kernel->path_trace(task, tile, void_buffer, void_buffer);
        }
        else {
          render(*task, tile, work_tiles);
        }
      }
      else if (tile.task == RenderTile::BAKE) {
        render(*task, tile, work_tiles);
      }
      else if (tile.task == RenderTile::DENOISE) {
        tile.sample = tile.start_sample + tile.num_samples;

        denoise(tile, denoising);

        task->update_progress(&tile, tile.w * tile.h);
      }

      task->release_tile(tile);

      if (task->get_cancel()) {
        if (task->need_finish_queue == false)
          break;
      }
    }

    work_tiles.free();
  }
  else if (task->type == DeviceTask::SHADER) {
    shader(*task);

    cuda_assert(cuCtxSynchronize());
  }
  else if (task->type == DeviceTask::DENOISE_BUFFER) {
    RenderTile tile;
    tile.x = task->x;
    tile.y = task->y;
    tile.w = task->w;
    tile.h = task->h;
    tile.buffer = task->buffer;
    tile.sample = task->sample + task->num_samples;
    tile.num_samples = task->num_samples;
    tile.start_sample = task->sample;
    tile.offset = task->offset;
    tile.stride = task->stride;
    tile.buffers = task->buffers;

    DenoisingTask denoising(this, *task);
    denoise(tile, denoising);
    task->update_progress(&tile, tile.w * tile.h);
  }
}

class CUDADeviceTask : public DeviceTask {
 public:
  CUDADeviceTask(CUDADevice *device, DeviceTask &task) : DeviceTask(task)
  {
    run = function_bind(&CUDADevice::thread_run, device, this);
  }
};

void CUDADevice::task_add(DeviceTask &task)
{
  CUDAContextScope scope(this);

  /* Load texture info. */
  load_texture_info();

  /* Synchronize all memory copies before executing task. */
  cuda_assert(cuCtxSynchronize());

  if (task.type == DeviceTask::FILM_CONVERT) {
    /* must be done in main thread due to opengl access */
    film_convert(task, task.buffer, task.rgba_byte, task.rgba_half);
  }
  else {
    task_pool.push(new CUDADeviceTask(this, task));
  }
}

void CUDADevice::task_wait()
{
  task_pool.wait();
}

void CUDADevice::task_cancel()
{
  task_pool.cancel();
}

/* redefine the cuda_assert macro so it can be used outside of the CUDADevice class
 * now that the definition of that class is complete
 */
#  undef cuda_assert
#  define cuda_assert(stmt) \
    { \
      CUresult result = stmt; \
      if (result != CUDA_SUCCESS) { \
        const char *name = cuewErrorString(result); \
        device->set_error( \
            string_printf("%s in %s (device_cuda_impl.cpp:%d)", name, #stmt, __LINE__)); \
      } \
    } \
    (void)0

/* CUDA context scope. */

CUDAContextScope::CUDAContextScope(CUDADevice *device) : device(device)
{
  cuda_assert(cuCtxPushCurrent(device->cuContext));
}

CUDAContextScope::~CUDAContextScope()
{
  cuda_assert(cuCtxPopCurrent(NULL));
}

/* split kernel */

class CUDASplitKernelFunction : public SplitKernelFunction {
  CUDADevice *device;
  CUfunction func;

 public:
  CUDASplitKernelFunction(CUDADevice *device, CUfunction func) : device(device), func(func)
  {
  }

  /* enqueue the kernel, returns false if there is an error */
  bool enqueue(const KernelDimensions &dim, device_memory & /*kg*/, device_memory & /*data*/)
  {
    return enqueue(dim, NULL);
  }

  /* enqueue the kernel, returns false if there is an error */
  bool enqueue(const KernelDimensions &dim, void *args[])
  {
    if (device->have_error())
      return false;

    CUDAContextScope scope(device);

    /* we ignore dim.local_size for now, as this is faster */
    int threads_per_block;
    cuda_assert(
        cuFuncGetAttribute(&threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, func));

    int xblocks = (dim.global_size[0] * dim.global_size[1] + threads_per_block - 1) /
                  threads_per_block;

    cuda_assert(cuFuncSetCacheConfig(func, CU_FUNC_CACHE_PREFER_L1));

    cuda_assert(cuLaunchKernel(func,
                               xblocks,
                               1,
                               1, /* blocks */
                               threads_per_block,
                               1,
                               1, /* threads */
                               0,
                               0,
                               args,
                               0));

    return !device->have_error();
  }
};

CUDASplitKernel::CUDASplitKernel(CUDADevice *device) : DeviceSplitKernel(device), device(device)
{
}

uint64_t CUDASplitKernel::state_buffer_size(device_memory & /*kg*/,
                                            device_memory & /*data*/,
                                            size_t num_threads)
{
  CUDAContextScope scope(device);

  device_vector<uint64_t> size_buffer(device, "size_buffer", MEM_READ_WRITE);
  size_buffer.alloc(1);
  size_buffer.zero_to_device();

  uint threads = num_threads;
  CUdeviceptr d_size = (CUdeviceptr)size_buffer.device_pointer;

  struct args_t {
    uint *num_threads;
    CUdeviceptr *size;
  };

  args_t args = {&threads, &d_size};

  CUfunction state_buffer_size;
  cuda_assert(
      cuModuleGetFunction(&state_buffer_size, device->cuModule, "kernel_cuda_state_buffer_size"));

  cuda_assert(cuLaunchKernel(state_buffer_size, 1, 1, 1, 1, 1, 1, 0, 0, (void **)&args, 0));

  size_buffer.copy_from_device(0, 1, 1);
  size_t size = size_buffer[0];
  size_buffer.free();

  return size;
}

bool CUDASplitKernel::enqueue_split_kernel_data_init(const KernelDimensions &dim,
                                                     RenderTile &rtile,
                                                     int num_global_elements,
                                                     device_memory & /*kernel_globals*/,
                                                     device_memory & /*kernel_data*/,
                                                     device_memory &split_data,
                                                     device_memory &ray_state,
                                                     device_memory &queue_index,
                                                     device_memory &use_queues_flag,
                                                     device_memory &work_pool_wgs)
{
  CUDAContextScope scope(device);

  CUdeviceptr d_split_data = (CUdeviceptr)split_data.device_pointer;
  CUdeviceptr d_ray_state = (CUdeviceptr)ray_state.device_pointer;
  CUdeviceptr d_queue_index = (CUdeviceptr)queue_index.device_pointer;
  CUdeviceptr d_use_queues_flag = (CUdeviceptr)use_queues_flag.device_pointer;
  CUdeviceptr d_work_pool_wgs = (CUdeviceptr)work_pool_wgs.device_pointer;

  CUdeviceptr d_buffer = (CUdeviceptr)rtile.buffer;

  int end_sample = rtile.start_sample + rtile.num_samples;
  int queue_size = dim.global_size[0] * dim.global_size[1];

  struct args_t {
    CUdeviceptr *split_data_buffer;
    int *num_elements;
    CUdeviceptr *ray_state;
    int *start_sample;
    int *end_sample;
    int *sx;
    int *sy;
    int *sw;
    int *sh;
    int *offset;
    int *stride;
    CUdeviceptr *queue_index;
    int *queuesize;
    CUdeviceptr *use_queues_flag;
    CUdeviceptr *work_pool_wgs;
    int *num_samples;
    CUdeviceptr *buffer;
  };

  args_t args = {&d_split_data,
                 &num_global_elements,
                 &d_ray_state,
                 &rtile.start_sample,
                 &end_sample,
                 &rtile.x,
                 &rtile.y,
                 &rtile.w,
                 &rtile.h,
                 &rtile.offset,
                 &rtile.stride,
                 &d_queue_index,
                 &queue_size,
                 &d_use_queues_flag,
                 &d_work_pool_wgs,
                 &rtile.num_samples,
                 &d_buffer};

  CUfunction data_init;
  cuda_assert(
      cuModuleGetFunction(&data_init, device->cuModule, "kernel_cuda_path_trace_data_init"));
  if (device->have_error()) {
    return false;
  }

  CUDASplitKernelFunction(device, data_init).enqueue(dim, (void **)&args);

  return !device->have_error();
}

SplitKernelFunction *CUDASplitKernel::get_split_kernel_function(const string &kernel_name,
                                                                const DeviceRequestedFeatures &)
{
  const CUDAContextScope scope(device);

  CUfunction func;
  const CUresult result = cuModuleGetFunction(
      &func, device->cuModule, (string("kernel_cuda_") + kernel_name).data());
  if (result != CUDA_SUCCESS) {
    device->set_error(string_printf("Could not find kernel \"kernel_cuda_%s\" in module (%s)",
                                    kernel_name.data(),
                                    cuewErrorString(result)));
    return NULL;
  }

  return new CUDASplitKernelFunction(device, func);
}

int2 CUDASplitKernel::split_kernel_local_size()
{
  return make_int2(32, 1);
}

int2 CUDASplitKernel::split_kernel_global_size(device_memory &kg,
                                               device_memory &data,
                                               DeviceTask * /*task*/)
{
  CUDAContextScope scope(device);
  size_t free;
  size_t total;

  cuda_assert(cuMemGetInfo(&free, &total));

  VLOG(1) << "Maximum device allocation size: " << string_human_readable_number(free)
          << " bytes. (" << string_human_readable_size(free) << ").";

  size_t num_elements = max_elements_for_max_buffer_size(kg, data, free / 2);
  size_t side = round_down((int)sqrt(num_elements), 32);
  int2 global_size = make_int2(side, round_down(num_elements / side, 16));
  VLOG(1) << "Global size: " << global_size << ".";
  return global_size;
}

CCL_NAMESPACE_END

#endif
