/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_HIP

#  include <cstdio>
#  include <cstdlib>
#  include <cstring>
#  include <iomanip>

#  include "device/hip/device_impl.h"

#  include "util/debug.h"
#  include "util/log.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/string.h"
#  include "util/system.h"
#  include "util/time.h"
#  include "util/types.h"

#  ifdef _WIN32
#    include "util/windows.h"
#  endif

#  include "kernel/device/hip/globals.h"

#  include "session/display_driver.h"

CCL_NAMESPACE_BEGIN

class HIPDevice;

bool HIPDevice::have_precompiled_kernels()
{
  string fatbins_path = path_get("lib");
  return path_exists(fatbins_path);
}

BVHLayoutMask HIPDevice::get_bvh_layout_mask(uint /*kernel_features*/) const
{
  return BVH_LAYOUT_BVH2;
}

void HIPDevice::set_error(const string &error)
{
  Device::set_error(error);

  if (first_error) {
    LOG_ERROR << "Refer to the Cycles GPU rendering documentation for possible solutions:\n"
                 "https://docs.blender.org/manual/en/latest/render/cycles/gpu_rendering.html\n";
    first_error = false;
  }
}

HIPDevice::HIPDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler, bool headless)
    : GPUDevice(info, stats, profiler, headless)
{
  /* Verify that base class types can be used with specific backend types */
  static_assert(sizeof(texMemObject) == sizeof(hipTextureObject_t));
  static_assert(sizeof(arrayMemObject) == sizeof(hArray));

  first_error = true;

  hipDevId = info.num;
  hipDevice = 0;
  hipContext = nullptr;

  hipModule = nullptr;

  need_image_info = false;

  pitch_alignment = 0;

  /* Initialize HIP. */
  hipError_t result = hipInit(0);
  if (result != hipSuccess) {
    set_error(string_printf("Failed to initialize HIP runtime (%s)", hipewErrorString(result)));
    return;
  }

  /* Setup device and context. */
  result = hipDeviceGet(&hipDevice, hipDevId);
  if (result != hipSuccess) {
    set_error(string_printf("Failed to get HIP device handle from ordinal (%s)",
                            hipewErrorString(result)));
    return;
  }

  /* hipDeviceMapHost for mapping host memory when out of device memory.
   * hipDeviceLmemResizeToMax for reserving local memory ahead of render,
   * so we can predict which memory to map to host. */
  int value;
  hip_assert(hipDeviceGetAttribute(&value, hipDeviceAttributeCanMapHostMemory, hipDevice));
  can_map_host = value != 0;

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

  /* Get hip runtime Version needed for memory types. */
  hip_assert(hipRuntimeGetVersion(&hipRuntimeVersion));

  /* Pop context set by hipCtxCreate. */
  hipCtxPopCurrent(nullptr);
}

HIPDevice::~HIPDevice()
{
  image_info.free();
  if (hipModule) {
    hip_assert(hipModuleUnload(hipModule));
  }
  hip_assert(hipCtxDestroy(hipContext));
}

bool HIPDevice::support_device(const uint /*kernel_features*/)
{
  if (hipSupportsDevice(hipDevId)) {
    return true;
  }
  /* We only support Navi and above. */
  hipDeviceProp_t props;
  hipGetDeviceProperties(&props, hipDevId);

  set_error(string_printf("HIP backend requires AMD RDNA graphics card or up, but found %s.",
                          props.name));
  return false;
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

  // Ensure array access over the link is possible as well (for 3D images)
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

/* Common HIPCC flags which stays the same regardless of shading model,
 * kernel sources md5 and only depends on compiler or compilation settings.
 */
string HIPDevice::compile_kernel_get_common_cflags(const uint kernel_features)
{
  const int machine = system_cpu_bits();
  const string source_path = path_get("source");
  const string include_path = source_path;
  string cflags = string_printf(
      "-m%d "
      "-DHIPCC "
      "-I\"%s\"",
      machine,
      include_path.c_str());
  if (use_adaptive_compilation()) {
    cflags += " -D__KERNEL_FEATURES__=" + to_string(kernel_features);
  }

  const char *extra_cflags = getenv("CYCLES_HIP_EXTRA_CFLAGS");
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

string HIPDevice::compile_kernel(const uint kernel_features, const char *name, const char *base)
{
  /* Compute kernel name. */
  int major, minor;
  hipDeviceGetAttribute(&major, hipDeviceAttributeComputeCapabilityMajor, hipDevId);
  hipDeviceGetAttribute(&minor, hipDeviceAttributeComputeCapabilityMinor, hipDevId);
  const std::string arch = hipDeviceArch(hipDevId);

  /* Attempt to use kernel provided with Blender. */
  if (!use_adaptive_compilation()) {
    const string fatbin = path_get(string_printf("lib/%s_%s.fatbin.zst", name, arch.c_str()));
    LOG_INFO << "Testing for pre-compiled kernel " << fatbin << ".";
    if (path_exists(fatbin)) {
      LOG_INFO << "Using precompiled kernel.";
      return fatbin;
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
  std::string options = "-Wno-parentheses-equality -Wno-unused-value -ffast-math -std=c++17";

#  ifndef NDEBUG
  options.append(" -save-temps");
#  endif
  if (major == 9 && minor == 0) {
    /* Reduce optimization level on VEGA GPUs to avoid some rendering artifacts */
    options.append(" -O1");
  }
  options.append(" --offload-arch=").append(arch);

  const string include_path = source_path;
  const string fatbin_file = string_printf(
      "cycles_%s_%s_%s", name, arch.c_str(), kernel_md5.c_str());
  const string fatbin = path_cache_get(path_join("kernels", fatbin_file));
  LOG_INFO << "Testing for locally compiled kernel " << fatbin << ".";
  if (path_exists(fatbin)) {
    LOG_INFO << "Using locally compiled kernel.";
    return fatbin;
  }

#  ifdef _WIN32
  if (!use_adaptive_compilation() && have_precompiled_kernels()) {
    if (!hipSupportsDevice(hipDevId)) {
      set_error(
          string_printf("HIP backend requires compute capability 10.1 or up, but found %d.%d. "
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
  if (hipcc == nullptr) {
    set_error(
        "HIP hipcc compiler not found. "
        "Install HIP toolkit in default location.");
    return string();
  }

  const int hipcc_hip_version = hipewCompilerVersion();
  LOG_INFO << "Found hipcc " << hipcc << ", HIP version " << hipcc_hip_version << ".";

  double starttime = time_dt();

  path_create_directories(fatbin);

  source_path = path_join(path_join(source_path, "kernel"),
                          path_join("device", path_join(base, string_printf("%s.cpp", name))));

  string command = string_printf("%s %s -I \"%s\" --%s \"%s\" -o \"%s\" %s",
                                 hipcc,
                                 options.c_str(),
                                 include_path.c_str(),
                                 kernel_ext,
                                 source_path.c_str(),
                                 fatbin.c_str(),
                                 common_cflags.c_str());

  LOG_INFO_IMPORTANT << "Compiling " << ((use_adaptive_compilation()) ? "adaptive " : "")
                     << "HIP kernel ... " << command;

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

  LOG_INFO_IMPORTANT << "Kernel compilation finished in " << std::fixed << std::setprecision(2)
                     << time_dt() - starttime << "s";

  return fatbin;
}

bool HIPDevice::load_kernels(const uint kernel_features)
{
  /* TODO(sergey): Support kernels re-load for HIP devices adaptive compile.
   *
   * Currently re-loading kernels will invalidate memory pointers.
   */
  if (hipModule) {
    if (use_adaptive_compilation()) {
      LOG_INFO << "Skipping HIP kernel reload for adaptive compilation, not currently supported.";
    }
    return true;
  }

  /* check if hip init succeeded */
  if (hipContext == nullptr) {
    return false;
  }

  /* check if GPU is supported */
  if (!support_device(kernel_features)) {
    return false;
  }

  /* get kernel */
  const char *kernel_name = "kernel";
  string fatbin = compile_kernel(kernel_features, kernel_name);
  if (fatbin.empty()) {
    return false;
  }

  /* open module */
  HIPContextScope scope(this);

  string fatbin_data;
  hipError_t result;

  if (path_read_compressed_text(fatbin, fatbin_data)) {
    result = hipModuleLoadData(&hipModule, fatbin_data.c_str());
  }
  else {
    result = hipErrorFileNotFound;
  }

  if (result != hipSuccess) {
    set_error(string_printf(
        "Failed to load HIP kernel from '%s' (%s)", fatbin.c_str(), hipewErrorString(result)));
  }

  if (result == hipSuccess) {
    kernels.load_all(this, hipModule);
    reserve_local_memory(kernel_features);
  }

  return (result == hipSuccess);
}

void HIPDevice::reserve_local_memory(const uint kernel_features)
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
    const DeviceKernel test_kernel = (kernel_features & KERNEL_FEATURE_NODE_RAYTRACE) ?
                                         DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE :
                                     (kernel_features & KERNEL_FEATURE_MNEE) ?
                                         DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE :
                                         DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE;

    /* Launch kernel, using just 1 block appears sufficient to reserve memory for all
     * multiprocessors. It would be good to do this in parallel for the multi GPU case
     * still to make it faster. */
    unique_ptr<DeviceQueue> queue = gpu_queue_create();

    device_ptr d_path_index = 0;
    device_ptr d_render_buffer = 0;
    int d_work_size = 0;
    DeviceKernelArguments args(&d_path_index, &d_render_buffer, &d_work_size);

    queue->init_execution();
    queue->enqueue(test_kernel, 1, args);
    queue->synchronize();
  }

  {
    HIPContextScope scope(this);
    hipMemGetInfo(&free_after, &total);
  }

  LOG_INFO << "Local memory reserved " << string_human_readable_number(free_before - free_after)
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

void HIPDevice::get_device_memory_info(size_t &total, size_t &free)
{
  HIPContextScope scope(this);

  hipMemGetInfo(&free, &total);
}

bool HIPDevice::alloc_device(void *&device_pointer, const size_t size)
{
  HIPContextScope scope(this);

  hipError_t mem_alloc_result = hipMalloc((hipDeviceptr_t *)&device_pointer, size);
  return mem_alloc_result == hipSuccess;
}

void HIPDevice::free_device(void *device_pointer)
{
  HIPContextScope scope(this);

  hip_assert(hipFree((hipDeviceptr_t)device_pointer));
}

bool HIPDevice::shared_alloc(void *&shared_pointer, const size_t size)
{
  HIPContextScope scope(this);

  hipError_t mem_alloc_result = hipHostMalloc(
      &shared_pointer, size, hipHostMallocMapped | hipHostMallocWriteCombined);

  return mem_alloc_result == hipSuccess;
}

void HIPDevice::shared_free(void *shared_pointer)
{
  HIPContextScope scope(this);

  hipHostFree(shared_pointer);
}

void *HIPDevice::shared_to_device_pointer(const void *shared_pointer)
{
  HIPContextScope scope(this);
  void *device_pointer = nullptr;
  hip_assert(
      hipHostGetDevicePointer((hipDeviceptr_t *)&device_pointer, (void *)shared_pointer, 0));
  return device_pointer;
}

void HIPDevice::copy_host_to_device(void *device_pointer, void *host_pointer, const size_t size)
{
  const HIPContextScope scope(this);

  hip_assert(hipMemcpyHtoD((hipDeviceptr_t)device_pointer, host_pointer, size));
}

void HIPDevice::mem_alloc(device_memory &mem)
{
  if (mem.type == MEM_IMAGE_TEXTURE) {
    assert(!"mem_alloc not supported for images.");
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
    global_copy_to(mem);
  }
  else if (mem.type == MEM_IMAGE_TEXTURE) {
    image_copy_to((device_image &)mem);
  }
  else {
    if (!mem.device_pointer) {
      generic_alloc(mem);
      generic_copy_to(mem);
    }
    else if (mem.is_resident(this)) {
      generic_copy_to(mem);
    }
  }
}

void HIPDevice::mem_move_to_host(device_memory &mem)
{
  if (mem.type == MEM_GLOBAL) {
    global_free(mem);
    global_alloc(mem);
  }
  else if (mem.type == MEM_IMAGE_TEXTURE) {
    image_free((device_image &)mem);
    image_alloc((device_image &)mem);
  }
  else {
    assert(!"mem_move_to_host only supported for image and global memory");
  }
}

void HIPDevice::mem_copy_from(
    device_memory &mem, const size_t y, size_t w, const size_t h, size_t elem)
{
  if (mem.type == MEM_IMAGE_TEXTURE) {
    assert(!"mem_copy_from not supported for images.");
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

  if (!(mem.is_shared(this) && mem.host_pointer == mem.shared_pointer)) {
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
  else if (mem.type == MEM_IMAGE_TEXTURE) {
    image_free((device_image &)mem);
  }
  else {
    generic_free(mem);
  }
}

device_ptr HIPDevice::mem_alloc_sub_ptr(device_memory &mem, const size_t offset, size_t /*size*/)
{
  return (device_ptr)(((char *)mem.device_pointer) + mem.memory_elements_size(offset));
}

void HIPDevice::const_copy_to(const char *name, void *host, const size_t size)
{
  HIPContextScope scope(this);
  hipDeviceptr_t mem;
  size_t bytes;

  hip_assert(hipModuleGetGlobal(&mem, &bytes, hipModule, "kernel_params"));
  assert(bytes == sizeof(KernelParamsHIP));

  /* Update data storage pointers in launch parameters. */
#  define KERNEL_DATA_ARRAY(data_type, data_name) \
    if (strcmp(name, #data_name) == 0) { \
      hip_assert(hipMemcpyHtoD(mem + offsetof(KernelParamsHIP, data_name), host, size)); \
      return; \
    }
  KERNEL_DATA_ARRAY(KernelData, data)
  KERNEL_DATA_ARRAY(IntegratorStateGPU, integrator_state)
#  include "kernel/data_arrays.h"
#  undef KERNEL_DATA_ARRAY
}

void HIPDevice::global_alloc(device_memory &mem)
{
  if (mem.is_resident(this)) {
    generic_alloc(mem);
    generic_copy_to(mem);
  }

  const_copy_to(mem.global_name(), &mem.device_pointer, sizeof(mem.device_pointer));
}

void HIPDevice::global_copy_to(device_memory &mem)
{
  if (!mem.device_pointer) {
    generic_alloc(mem);
    generic_copy_to(mem);
  }
  else if (mem.is_resident(this)) {
    generic_copy_to(mem);
  }

  const_copy_to(mem.global_name(), &mem.device_pointer, sizeof(mem.device_pointer));
}

void HIPDevice::global_free(device_memory &mem)
{
  if (mem.is_resident(this) && mem.device_pointer) {
    generic_free(mem);
  }
}

static size_t tex_src_pitch(const device_image &mem)
{
  return mem.data_width * datatype_size(mem.data_type) * mem.data_elements;
}

static hip_Memcpy2D tex_2d_copy_param(const device_image &mem, const int pitch_alignment)
{
  /* 2D image using pitch aligned linear memory. */
  const size_t src_pitch = tex_src_pitch(mem);
  const size_t dst_pitch = align_up(src_pitch, pitch_alignment);

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

  return param;
}

void HIPDevice::image_alloc(device_image &mem)
{
  HIPContextScope scope(this);

  hipTextureAddressMode address_mode = hipAddressModeWrap;
  switch (mem.info.extension) {
    case EXTENSION_REPEAT:
      address_mode = hipAddressModeWrap;
      break;
    case EXTENSION_EXTEND:
      address_mode = hipAddressModeClamp;
      break;
    case EXTENSION_CLIP:
      address_mode = hipAddressModeBorder;
      break;
    case EXTENSION_MIRROR:
      address_mode = hipAddressModeMirror;
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

  /* Image Storage. */
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

  Mem *cmem = nullptr;

  if (!mem.is_resident(this)) {
    thread_scoped_lock lock(device_mem_map_mutex);
    cmem = &device_mem_map[&mem];
    cmem->texobject = 0;
  }
  else if (mem.data_height > 0) {
    /* 2D image, using pitch aligned linear memory. */
    const size_t dst_pitch = align_up(tex_src_pitch(mem), pitch_alignment);
    const size_t dst_size = dst_pitch * mem.data_height;

    cmem = generic_alloc(mem, dst_size - mem.memory_size());
    if (!cmem) {
      return;
    }

    const hip_Memcpy2D param = tex_2d_copy_param(mem, pitch_alignment);
    hip_assert(hipDrvMemcpy2DUnaligned(&param));
  }
  else {
    /* 1D image, using linear memory. */
    cmem = generic_alloc(mem);
    if (!cmem) {
      return;
    }

    hip_assert(hipMemcpyHtoD(mem.device_pointer, mem.host_pointer, mem.memory_size()));
  }

  /* Set Mapping and tag that we need to (re-)upload to device */
  KernelImageInfo tex_info = mem.info;

  if (!is_nanovdb_type(mem.info.data_type)) {
    /* Bindless textures. */
    hipResourceDesc resDesc;
    memset(&resDesc, 0, sizeof(resDesc));

    if (mem.data_height > 0) {
      const size_t dst_pitch = align_up(tex_src_pitch(mem), pitch_alignment);

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

    thread_scoped_lock lock(device_mem_map_mutex);
    cmem = &device_mem_map[&mem];

    if (hipTexObjectCreate(&cmem->texobject, &resDesc, &texDesc, nullptr) != hipSuccess) {
      set_error(
          "Failed to create image. Maximum GPU image size or available GPU memory was likely "
          "exceeded.");
    }

    tex_info.data = (uint64_t)cmem->texobject;
  }
  else {
    tex_info.data = (uint64_t)mem.device_pointer;
  }

  {
    /* Update image info. */
    thread_scoped_lock lock(image_info_mutex);
    const uint image_info_id = mem.image_info_id;
    if (image_info_id >= image_info.size()) {
      /* Allocate some image_info_ids in advance, to reduce amount of re-allocations. */
      image_info.resize(image_info_id + 128);
    }
    image_info[image_info_id] = tex_info;
    need_image_info = true;
  }
}

void HIPDevice::image_copy_to(device_image &mem)
{
  if (!mem.device_pointer) {
    /* Not yet allocated on device. */
    image_alloc(mem);
  }
  else if (!mem.is_resident(this)) {
    /* Peering with another device, may still need to create image info and object. */
    bool image_allocated = false;
    {
      thread_scoped_lock lock(image_info_mutex);
      image_allocated = mem.image_info_id < image_info.size() &&
                        image_info[mem.image_info_id].data != 0;
    }
    if (!image_allocated) {
      image_alloc(mem);
    }
  }
  else {
    /* Resident and fully allocated, only copy. */
    if (mem.data_height > 0) {
      HIPContextScope scope(this);
      const hip_Memcpy2D param = tex_2d_copy_param(mem, pitch_alignment);
      hip_assert(hipDrvMemcpy2DUnaligned(&param));
    }
    else {
      generic_copy_to(mem);
    }
  }
}

void HIPDevice::image_free(device_image &mem)
{
  HIPContextScope scope(this);
  thread_scoped_lock lock(device_mem_map_mutex);

  /* Check if the memory was allocated for this device. */
  auto it = device_mem_map.find(&mem);
  if (it == device_mem_map.end()) {
    return;
  }

  const Mem &cmem = it->second;

  /* Always clear image info and texture object, regardless of residency. */
  {
    thread_scoped_lock lock(image_info_mutex);
    image_info[mem.image_info_id] = KernelImageInfo();
  }

  if (cmem.texobject) {
    /* Free bindless texture. */
    hipTexObjectDestroy(cmem.texobject);
  }

  if (!mem.is_resident(this)) {
    /* Do not free memory here, since it was allocated on a different device. */
    device_mem_map.erase(device_mem_map.find(&mem));
  }
  else if (cmem.array) {
    /* Free array. */
    hipArrayDestroy(reinterpret_cast<hArray>(cmem.array));
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

unique_ptr<DeviceQueue> HIPDevice::gpu_queue_create()
{
  return make_unique<HIPDeviceQueue>(this);
}

bool HIPDevice::should_use_graphics_interop(const GraphicsInteropDevice &interop_device,
                                            const bool log)
{
  if (headless) {
    /* Avoid any call which might involve interaction with a graphics backend when we know that
     * we don't have active graphics context. This avoids potential crash in the driver. */
    return false;
  }

  HIPContextScope scope(this);

  switch (interop_device.type) {
    case GraphicsInteropDevice::OPENGL: {
      /* Disable graphics interop for now, because of driver bug in 21.40. See #92972.
       * Also missing Vulkan support which is needed now. */
      return false;

      /* Check whether this device is part of OpenGL context.
       *
       * Using HIP device for graphics interoperability which is not part of the OpenGL context is
       * possible, but from the empiric measurements with CUDA it can be considerably slower than
       * using naive pixels copy. */
      int num_all_devices = 0;
      hip_assert(hipGetDeviceCount(&num_all_devices));

      if (num_all_devices == 0) {
        return false;
      }

      vector<hipDevice_t> gl_devices(num_all_devices);
      uint num_gl_devices = 0;
      hipGLGetDevices(&num_gl_devices, gl_devices.data(), num_all_devices, hipGLDeviceListAll);

      bool found = false;
      for (hipDevice_t gl_device : gl_devices) {
        if (gl_device == hipDevice) {
          found = true;
          break;
        }
      }

      if (log) {
        if (found) {
          LOG_INFO << "Graphics interop: found matching OpenGL device for HIP";
        }
        else {
          LOG_INFO << "Graphics interop: no matching OpenGL device for HIP";
        }
      }

      return found;
    }
    case GraphicsInteropDevice::VULKAN:
    case GraphicsInteropDevice::METAL:
    case GraphicsInteropDevice::NONE:
      /* TODO: Implement Vulkan support. */
      return false;
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

int HIPDevice::get_device_default_attribute(hipDeviceAttribute_t attribute,
                                            const int default_value)
{
  int value = 0;
  if (!get_device_attribute(attribute, &value)) {
    return default_value;
  }
  return value;
}

CCL_NAMESPACE_END

#endif
