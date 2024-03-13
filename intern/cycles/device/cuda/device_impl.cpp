/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_CUDA

#  include <climits>
#  include <limits.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>

#  include "device/cuda/device_impl.h"

#  include "util/debug.h"
#  include "util/foreach.h"
#  include "util/log.h"
#  include "util/map.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/string.h"
#  include "util/system.h"
#  include "util/time.h"
#  include "util/types.h"
#  include "util/windows.h"

#  include "kernel/device/cuda/globals.h"

CCL_NAMESPACE_BEGIN

class CUDADevice;

bool CUDADevice::have_precompiled_kernels()
{
  string cubins_path = path_get("lib");
  return path_exists(cubins_path);
}

BVHLayoutMask CUDADevice::get_bvh_layout_mask(uint /*kernel_features*/) const
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

CUDADevice::CUDADevice(const DeviceInfo &info, Stats &stats, Profiler &profiler)
    : GPUDevice(info, stats, profiler)
{
  /* Verify that base class types can be used with specific backend types */
  static_assert(sizeof(texMemObject) == sizeof(CUtexObject));
  static_assert(sizeof(arrayMemObject) == sizeof(CUarray));

  first_error = true;

  cuDevId = info.num;
  cuDevice = 0;
  cuContext = 0;

  cuModule = 0;

  need_texture_info = false;

  pitch_alignment = 0;

  /* Initialize CUDA. */
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
  int value;
  cuda_assert(cuDeviceGetAttribute(&value, CU_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY, cuDevice));
  can_map_host = value != 0;

  cuda_assert(cuDeviceGetAttribute(
      &pitch_alignment, CU_DEVICE_ATTRIBUTE_TEXTURE_PITCH_ALIGNMENT, cuDevice));

  if (can_map_host) {
    init_host_memory();
  }

  int active = 0;
  unsigned int ctx_flags = 0;
  cuda_assert(cuDevicePrimaryCtxGetState(cuDevice, &ctx_flags, &active));

  /* Configure primary context only once. */
  if (active == 0) {
    ctx_flags |= CU_CTX_LMEM_RESIZE_TO_MAX;
    result = cuDevicePrimaryCtxSetFlags(cuDevice, ctx_flags);
    if (result != CUDA_SUCCESS && result != CUDA_ERROR_PRIMARY_CONTEXT_ACTIVE) {
      set_error(string_printf("Failed to configure CUDA context (%s)", cuewErrorString(result)));
      return;
    }
  }

  /* Create context. */
  result = cuDevicePrimaryCtxRetain(&cuContext, cuDevice);

  if (result != CUDA_SUCCESS) {
    set_error(string_printf("Failed to retain CUDA context (%s)", cuewErrorString(result)));
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
  texture_info.free();
  if (cuModule) {
    cuda_assert(cuModuleUnload(cuModule));
  }
  cuda_assert(cuDevicePrimaryCtxRelease(cuDevice));
}

bool CUDADevice::support_device(const uint /*kernel_features*/)
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
                                      CU_DEVICE_P2P_ATTRIBUTE_CUDA_ARRAY_ACCESS_SUPPORTED,
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

/* Common NVCC flags which stays the same regardless of shading model,
 * kernel sources md5 and only depends on compiler or compilation settings.
 */
string CUDADevice::compile_kernel_get_common_cflags(const uint kernel_features)
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
  if (use_adaptive_compilation()) {
    cflags += " -D__KERNEL_FEATURES__=" + to_string(kernel_features);
  }
  const char *extra_cflags = getenv("CYCLES_CUDA_EXTRA_CFLAGS");
  if (extra_cflags) {
    cflags += string(" ") + string(extra_cflags);
  }

#  ifdef WITH_NANOVDB
  cflags += " -DWITH_NANOVDB";
#  endif

#  ifdef WITH_CYCLES_DEBUG
  cflags += " -DWITH_CYCLES_DEBUG";
#  endif

  return cflags;
}

string CUDADevice::compile_kernel(const string &common_cflags,
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
      VLOG_INFO << "Testing for pre-compiled kernel " << cubin << ".";
      if (path_exists(cubin)) {
        VLOG_INFO << "Using precompiled kernel.";
        return cubin;
      }
    }

    /* The driver can JIT-compile PTX generated for older generations, so find the closest one. */
    int ptx_major = major, ptx_minor = minor;
    while (ptx_major >= 3) {
      const string ptx = path_get(
          string_printf("lib/%s_compute_%d%d.ptx", name, ptx_major, ptx_minor));
      VLOG_INFO << "Testing for pre-compiled kernel " << ptx << ".";
      if (path_exists(ptx)) {
        VLOG_INFO << "Using precompiled kernel.";
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

  /* We include cflags into md5 so changing cuda toolkit or changing other
   * compiler command line arguments makes sure cubin gets re-built.
   */
  const string kernel_md5 = util_md5_string(source_md5 + common_cflags);

  const char *const kernel_ext = force_ptx ? "ptx" : "cubin";
  const char *const kernel_arch = force_ptx ? "compute" : "sm";
  const string cubin_file = string_printf(
      "cycles_%s_%s_%d%d_%s.%s", name, kernel_arch, major, minor, kernel_md5.c_str(), kernel_ext);
  const string cubin = path_cache_get(path_join("kernels", cubin_file));
  VLOG_INFO << "Testing for locally compiled kernel " << cubin << ".";
  if (path_exists(cubin)) {
    VLOG_INFO << "Using locally compiled kernel.";
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
  VLOG_INFO << "Found nvcc " << nvcc << ", CUDA version " << nvcc_cuda_version << ".";
  if (nvcc_cuda_version < 101) {
    printf(
        "Unsupported CUDA version %d.%d detected, "
        "you need CUDA 10.1 or newer.\n",
        nvcc_cuda_version / 10,
        nvcc_cuda_version % 10);
    return string();
  }
  else if (!(nvcc_cuda_version == 101 || nvcc_cuda_version == 102 || nvcc_cuda_version == 111 ||
             nvcc_cuda_version == 112 || nvcc_cuda_version == 113 || nvcc_cuda_version == 114))
  {
    printf(
        "CUDA version %d.%d detected, build may succeed but only "
        "CUDA 10.1 to 11.4 are officially supported.\n",
        nvcc_cuda_version / 10,
        nvcc_cuda_version % 10);
  }

  double starttime = time_dt();

  path_create_directories(cubin);

  source_path = path_join(path_join(source_path, "kernel"),
                          path_join("device", path_join(base, string_printf("%s.cu", name))));

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

  printf("Compiling %sCUDA kernel ...\n%s\n",
         (use_adaptive_compilation()) ? "adaptive " : "",
         command.c_str());

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

bool CUDADevice::load_kernels(const uint kernel_features)
{
  /* TODO(sergey): Support kernels re-load for CUDA devices adaptive compile.
   *
   * Currently re-loading kernel will invalidate memory pointers,
   * causing problems in cuCtxSynchronize.
   */
  if (cuModule) {
    if (use_adaptive_compilation()) {
      VLOG_INFO
          << "Skipping CUDA kernel reload for adaptive compilation, not currently supported.";
    }
    return true;
  }

  /* check if cuda init succeeded */
  if (cuContext == 0) {
    return false;
  }

  /* check if GPU is supported */
  if (!support_device(kernel_features)) {
    return false;
  }

  /* get kernel */
  const char *kernel_name = "kernel";
  string cflags = compile_kernel_get_common_cflags(kernel_features);
  string cubin = compile_kernel(cflags, kernel_name);
  if (cubin.empty()) {
    return false;
  }

  /* open module */
  CUDAContextScope scope(this);

  string cubin_data;
  CUresult result;

  if (path_read_text(cubin, cubin_data)) {
    result = cuModuleLoadData(&cuModule, cubin_data.c_str());
  }
  else {
    result = CUDA_ERROR_FILE_NOT_FOUND;
  }

  if (result != CUDA_SUCCESS) {
    set_error(string_printf(
        "Failed to load CUDA kernel from '%s' (%s)", cubin.c_str(), cuewErrorString(result)));
  }

  if (result == CUDA_SUCCESS) {
    kernels.load(this);
    reserve_local_memory(kernel_features);
  }

  return (result == CUDA_SUCCESS);
}

void CUDADevice::reserve_local_memory(const uint kernel_features)
{
  /* Together with CU_CTX_LMEM_RESIZE_TO_MAX, this reserves local memory
   * needed for kernel launches, so that we can reliably figure out when
   * to allocate scene data in mapped host memory. */
  size_t total = 0, free_before = 0, free_after = 0;

  {
    CUDAContextScope scope(this);
    cuMemGetInfo(&free_before, &total);
  }

  {
    /* Use the biggest kernel for estimation. */
    const DeviceKernel test_kernel = (kernel_features & KERNEL_FEATURE_NODE_RAYTRACE) ?
                                         DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE :
                                     (kernel_features & KERNEL_FEATURE_MNEE) ?
                                         DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE :
                                         DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE;

    /* Launch kernel, using just 1 block appears sufficient to reserve memory for all
     * multiprocessors. It would be good to do this in parallel for the multi GPU case
     * still to make it faster. */
    CUDADeviceQueue queue(this);

    device_ptr d_path_index = 0;
    device_ptr d_render_buffer = 0;
    int d_work_size = 0;
    DeviceKernelArguments args(&d_path_index, &d_render_buffer, &d_work_size);

    queue.init_execution();
    queue.enqueue(test_kernel, 1, args);
    queue.synchronize();
  }

  {
    CUDAContextScope scope(this);
    cuMemGetInfo(&free_after, &total);
  }

  VLOG_INFO << "Local memory reserved " << string_human_readable_number(free_before - free_after)
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

void CUDADevice::get_device_memory_info(size_t &total, size_t &free)
{
  CUDAContextScope scope(this);

  cuMemGetInfo(&free, &total);
}

bool CUDADevice::alloc_device(void *&device_pointer, size_t size)
{
  CUDAContextScope scope(this);

  CUresult mem_alloc_result = cuMemAlloc((CUdeviceptr *)&device_pointer, size);
  return mem_alloc_result == CUDA_SUCCESS;
}

void CUDADevice::free_device(void *device_pointer)
{
  CUDAContextScope scope(this);

  cuda_assert(cuMemFree((CUdeviceptr)device_pointer));
}

bool CUDADevice::alloc_host(void *&shared_pointer, size_t size)
{
  CUDAContextScope scope(this);

  CUresult mem_alloc_result = cuMemHostAlloc(
      &shared_pointer, size, CU_MEMHOSTALLOC_DEVICEMAP | CU_MEMHOSTALLOC_WRITECOMBINED);
  return mem_alloc_result == CUDA_SUCCESS;
}

void CUDADevice::free_host(void *shared_pointer)
{
  CUDAContextScope scope(this);

  cuMemFreeHost(shared_pointer);
}

void CUDADevice::transform_host_pointer(void *&device_pointer, void *&shared_pointer)
{
  CUDAContextScope scope(this);

  cuda_assert(cuMemHostGetDevicePointer_v2((CUdeviceptr *)&device_pointer, shared_pointer, 0));
}

void CUDADevice::copy_host_to_device(void *device_pointer, void *host_pointer, size_t size)
{
  const CUDAContextScope scope(this);

  cuda_assert(cuMemcpyHtoD((CUdeviceptr)device_pointer, host_pointer, size));
}

void CUDADevice::mem_alloc(device_memory &mem)
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

void CUDADevice::mem_copy_to(device_memory &mem)
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

void CUDADevice::mem_copy_from(device_memory &mem, size_t y, size_t w, size_t h, size_t elem)
{
  if (mem.type == MEM_TEXTURE || mem.type == MEM_GLOBAL) {
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
  thread_scoped_lock lock(device_mem_map_mutex);
  if (!device_mem_map[&mem].use_mapped_host || mem.host_pointer != mem.shared_pointer) {
    const CUDAContextScope scope(this);
    cuda_assert(cuMemsetD8((CUdeviceptr)mem.device_pointer, 0, mem.memory_size()));
  }
  else if (mem.host_pointer) {
    memset(mem.host_pointer, 0, mem.memory_size());
  }
}

void CUDADevice::mem_free(device_memory &mem)
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

device_ptr CUDADevice::mem_alloc_sub_ptr(device_memory &mem, size_t offset, size_t /*size*/)
{
  return (device_ptr)(((char *)mem.device_pointer) + mem.memory_elements_size(offset));
}

void CUDADevice::const_copy_to(const char *name, void *host, size_t size)
{
  CUDAContextScope scope(this);
  CUdeviceptr mem;
  size_t bytes;

  cuda_assert(cuModuleGetGlobal(&mem, &bytes, cuModule, "kernel_params"));
  assert(bytes == sizeof(KernelParamsCUDA));

  /* Update data storage pointers in launch parameters. */
#  define KERNEL_DATA_ARRAY(data_type, data_name) \
    if (strcmp(name, #data_name) == 0) { \
      cuda_assert(cuMemcpyHtoD(mem + offsetof(KernelParamsCUDA, data_name), host, size)); \
      return; \
    }
  KERNEL_DATA_ARRAY(KernelData, data)
  KERNEL_DATA_ARRAY(IntegratorStateGPU, integrator_state)
#  include "kernel/data_arrays.h"
#  undef KERNEL_DATA_ARRAY
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
    case EXTENSION_MIRROR:
      address_mode = CU_TR_ADDRESS_MODE_MIRROR;
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
  /* Cycles expects to read all texture data as normalized float values in
   * kernel/device/gpu/image.h. But storing all data as floats would be very inefficient due to the
   * huge size of float textures. So in the code below, we define different texture types including
   * integer types, with the aim of using CUDA's default promotion behavior of integer data to
   * floating point data in the range [0, 1], as noted in the CUDA documentation on
   * cuTexObjectCreate API Call.
   * Note that 32-bit integers are not supported by this promotion behavior and cannot be used
   * with Cycles's current implementation in kernel/device/gpu/image.h.
   */
  CUarray_format_enum format;
  switch (mem.data_type) {
    case TYPE_UCHAR:
      format = CU_AD_FORMAT_UNSIGNED_INT8;
      break;
    case TYPE_UINT16:
      format = CU_AD_FORMAT_UNSIGNED_INT16;
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

  Mem *cmem = NULL;
  CUarray array_3d = NULL;
  size_t src_pitch = mem.data_width * dsize * mem.data_elements;
  size_t dst_pitch = src_pitch;

  if (!mem.is_resident(this)) {
    thread_scoped_lock lock(device_mem_map_mutex);
    cmem = &device_mem_map[&mem];
    cmem->texobject = 0;

    if (mem.data_depth > 1) {
      array_3d = (CUarray)mem.device_pointer;
      cmem->array = reinterpret_cast<arrayMemObject>(array_3d);
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

    VLOG_WORK << "Array 3D allocate: " << mem.name << ", "
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

    thread_scoped_lock lock(device_mem_map_mutex);
    cmem = &device_mem_map[&mem];
    cmem->texobject = 0;
    cmem->array = reinterpret_cast<arrayMemObject>(array_3d);
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
      mem.info.data_type != IMAGE_DATA_TYPE_NANOVDB_FLOAT3 &&
      mem.info.data_type != IMAGE_DATA_TYPE_NANOVDB_FPN &&
      mem.info.data_type != IMAGE_DATA_TYPE_NANOVDB_FP16)
  {
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
    /* CUDA's flag CU_TRSF_READ_AS_INTEGER is intentionally not used and it is
     * significant, see above an explanation about how Blender treat textures. */
    texDesc.flags = CU_TRSF_NORMALIZED_COORDINATES;

    thread_scoped_lock lock(device_mem_map_mutex);
    cmem = &device_mem_map[&mem];

    cuda_assert(cuTexObjectCreate(&cmem->texobject, &resDesc, &texDesc, NULL));

    texture_info[slot].data = (uint64_t)cmem->texobject;
  }
  else {
    texture_info[slot].data = (uint64_t)mem.device_pointer;
  }
}

void CUDADevice::tex_free(device_texture &mem)
{
  if (mem.device_pointer) {
    CUDAContextScope scope(this);
    thread_scoped_lock lock(device_mem_map_mutex);
    DCHECK(device_mem_map.find(&mem) != device_mem_map.end());
    const Mem &cmem = device_mem_map[&mem];

    if (cmem.texobject) {
      /* Free bindless texture. */
      cuTexObjectDestroy(cmem.texobject);
    }

    if (!mem.is_resident(this)) {
      /* Do not free memory here, since it was allocated on a different device. */
      device_mem_map.erase(device_mem_map.find(&mem));
    }
    else if (cmem.array) {
      /* Free array. */
      cuArrayDestroy(reinterpret_cast<CUarray>(cmem.array));
      stats.mem_free(mem.device_size);
      mem.device_pointer = 0;
      mem.device_size = 0;

      device_mem_map.erase(device_mem_map.find(&mem));
    }
    else {
      lock.unlock();
      generic_free(mem);
    }
  }
}

unique_ptr<DeviceQueue> CUDADevice::gpu_queue_create()
{
  return make_unique<CUDADeviceQueue>(this);
}

bool CUDADevice::should_use_graphics_interop()
{
  /* Check whether this device is part of OpenGL context.
   *
   * Using CUDA device for graphics interoperability which is not part of the OpenGL context is
   * possible, but from the empiric measurements it can be considerably slower than using naive
   * pixels copy. */

  CUDAContextScope scope(this);

  int num_all_devices = 0;
  cuda_assert(cuDeviceGetCount(&num_all_devices));

  if (num_all_devices == 0) {
    return false;
  }

  vector<CUdevice> gl_devices(num_all_devices);
  uint num_gl_devices = 0;
  cuGLGetDevices(&num_gl_devices, gl_devices.data(), num_all_devices, CU_GL_DEVICE_LIST_ALL);

  for (uint i = 0; i < num_gl_devices; ++i) {
    if (gl_devices[i] == cuDevice) {
      return true;
    }
  }

  return false;
}

int CUDADevice::get_num_multiprocessors()
{
  return get_device_default_attribute(CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, 0);
}

int CUDADevice::get_max_num_threads_per_multiprocessor()
{
  return get_device_default_attribute(CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR, 0);
}

bool CUDADevice::get_device_attribute(CUdevice_attribute attribute, int *value)
{
  CUDAContextScope scope(this);

  return cuDeviceGetAttribute(value, attribute, cuDevice) == CUDA_SUCCESS;
}

int CUDADevice::get_device_default_attribute(CUdevice_attribute attribute, int default_value)
{
  int value = 0;
  if (!get_device_attribute(attribute, &value)) {
    return default_value;
  }
  return value;
}

CCL_NAMESPACE_END

#endif
