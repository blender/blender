/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_METAL

#  include "device/metal/device_impl.h"
#  include "device/metal/device.h"

#  include "scene/scene.h"

#  include "util/debug.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/time.h"

#  include <crt_externs.h>

CCL_NAMESPACE_BEGIN

class MetalDevice;

thread_mutex MetalDevice::existing_devices_mutex;
std::map<int, MetalDevice *> MetalDevice::active_device_ids;

/* Thread-safe device access for async work. Calling code must pass an appropriately scoped lock
 * to existing_devices_mutex to safeguard against destruction of the returned instance. */
MetalDevice *MetalDevice::get_device_by_ID(int ID,
                                           thread_scoped_lock & /*existing_devices_mutex_lock*/)
{
  auto it = active_device_ids.find(ID);
  if (it != active_device_ids.end()) {
    return it->second;
  }
  return nullptr;
}

bool MetalDevice::is_device_cancelled(int ID)
{
  thread_scoped_lock lock(existing_devices_mutex);
  return get_device_by_ID(ID, lock) == nullptr;
}

BVHLayoutMask MetalDevice::get_bvh_layout_mask(uint /*kernel_features*/) const
{
  return use_metalrt ? BVH_LAYOUT_METAL : BVH_LAYOUT_BVH2;
}

void MetalDevice::set_error(const string &error)
{
  static std::mutex s_error_mutex;
  std::lock_guard<std::mutex> lock(s_error_mutex);

  Device::set_error(error);

  if (!has_error) {
    fprintf(stderr, "\nRefer to the Cycles GPU rendering documentation for possible solutions:\n");
    fprintf(stderr,
            "https://docs.blender.org/manual/en/latest/render/cycles/gpu_rendering.html\n\n");
    has_error = true;
  }
}

MetalDevice::MetalDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler)
    : Device(info, stats, profiler), texture_info(this, "texture_info", MEM_GLOBAL)
{
  {
    /* Assign an ID for this device which we can use to query whether async shader compilation
     * requests are still relevant. */
    thread_scoped_lock lock(existing_devices_mutex);
    static int existing_devices_counter = 1;
    device_id = existing_devices_counter++;
    active_device_ids[device_id] = this;
  }

  mtlDevId = info.num;

  /* select chosen device */
  auto usable_devices = MetalInfo::get_usable_devices();
  assert(mtlDevId < usable_devices.size());
  mtlDevice = usable_devices[mtlDevId];
  device_vendor = MetalInfo::get_device_vendor(mtlDevice);
  assert(device_vendor != METAL_GPU_UNKNOWN);
  metal_printf("Creating new Cycles device for Metal: %s\n", info.description.c_str());

  /* determine default storage mode based on whether UMA is supported */

  default_storage_mode = MTLResourceStorageModeManaged;

  if (@available(macos 11.0, *)) {
    if ([mtlDevice hasUnifiedMemory]) {
      default_storage_mode = MTLResourceStorageModeShared;
    }
  }

  switch (device_vendor) {
    default:
      break;
    case METAL_GPU_INTEL: {
      max_threads_per_threadgroup = 64;
      break;
    }
    case METAL_GPU_AMD: {
      max_threads_per_threadgroup = 128;
      break;
    }
    case METAL_GPU_APPLE: {
      max_threads_per_threadgroup = 512;
      break;
    }
  }

  use_metalrt = info.use_hardware_raytracing;
  if (auto metalrt = getenv("CYCLES_METALRT")) {
    use_metalrt = (atoi(metalrt) != 0);
  }

  if (getenv("CYCLES_DEBUG_METAL_CAPTURE_KERNEL")) {
    capture_enabled = true;
  }

  if (device_vendor == METAL_GPU_APPLE) {
    /* Set kernel_specialization_level based on user preferences. */
    switch (info.kernel_optimization_level) {
      case KERNEL_OPTIMIZATION_LEVEL_OFF:
        kernel_specialization_level = PSO_GENERIC;
        break;
      default:
      case KERNEL_OPTIMIZATION_LEVEL_INTERSECT:
        kernel_specialization_level = PSO_SPECIALIZED_INTERSECT;
        break;
      case KERNEL_OPTIMIZATION_LEVEL_FULL:
        kernel_specialization_level = PSO_SPECIALIZED_SHADE;
        break;
    }
  }

  if (auto envstr = getenv("CYCLES_METAL_SPECIALIZATION_LEVEL")) {
    kernel_specialization_level = (MetalPipelineType)atoi(envstr);
  }
  metal_printf("kernel_specialization_level = %s\n",
               kernel_type_as_string(
                   (MetalPipelineType)min((int)kernel_specialization_level, (int)PSO_NUM - 1)));

  MTLArgumentDescriptor *arg_desc_params = [[MTLArgumentDescriptor alloc] init];
  arg_desc_params.dataType = MTLDataTypePointer;
  arg_desc_params.access = MTLArgumentAccessReadOnly;
  arg_desc_params.arrayLength = sizeof(KernelParamsMetal) / sizeof(device_ptr);
  mtlBufferKernelParamsEncoder = [mtlDevice newArgumentEncoderWithArguments:@[ arg_desc_params ]];

  MTLArgumentDescriptor *arg_desc_texture = [[MTLArgumentDescriptor alloc] init];
  arg_desc_texture.dataType = MTLDataTypeTexture;
  arg_desc_texture.access = MTLArgumentAccessReadOnly;
  mtlTextureArgEncoder = [mtlDevice newArgumentEncoderWithArguments:@[ arg_desc_texture ]];
  MTLArgumentDescriptor *arg_desc_buffer = [[MTLArgumentDescriptor alloc] init];
  arg_desc_buffer.dataType = MTLDataTypePointer;
  arg_desc_buffer.access = MTLArgumentAccessReadOnly;
  mtlBufferArgEncoder = [mtlDevice newArgumentEncoderWithArguments:@[ arg_desc_buffer ]];

  buffer_bindings_1d = [mtlDevice newBufferWithLength:8192 options:default_storage_mode];
  texture_bindings_2d = [mtlDevice newBufferWithLength:8192 options:default_storage_mode];
  texture_bindings_3d = [mtlDevice newBufferWithLength:8192 options:default_storage_mode];
  stats.mem_alloc(buffer_bindings_1d.allocatedSize + texture_bindings_2d.allocatedSize +
                  texture_bindings_3d.allocatedSize);

  /* command queue for non-tracing work on the GPU */
  mtlGeneralCommandQueue = [mtlDevice newCommandQueue];

  /* Acceleration structure arg encoder, if needed */
  if (@available(macos 12.0, *)) {
    if (use_metalrt) {
      MTLArgumentDescriptor *arg_desc_as = [[MTLArgumentDescriptor alloc] init];
      arg_desc_as.dataType = MTLDataTypeInstanceAccelerationStructure;
      arg_desc_as.access = MTLArgumentAccessReadOnly;
      mtlASArgEncoder = [mtlDevice newArgumentEncoderWithArguments:@[ arg_desc_as ]];
      [arg_desc_as release];
    }
  }

  /* Build the arg encoder for the ancillary bindings */
  {
    NSMutableArray *ancillary_desc = [[NSMutableArray alloc] init];

    int index = 0;
    MTLArgumentDescriptor *arg_desc_tex = [[MTLArgumentDescriptor alloc] init];
    arg_desc_tex.dataType = MTLDataTypePointer;
    arg_desc_tex.access = MTLArgumentAccessReadOnly;

    arg_desc_tex.index = index++;
    [ancillary_desc addObject:[arg_desc_tex copy]]; /* metal_buf_1d */
    arg_desc_tex.index = index++;
    [ancillary_desc addObject:[arg_desc_tex copy]]; /* metal_tex_2d */
    arg_desc_tex.index = index++;
    [ancillary_desc addObject:[arg_desc_tex copy]]; /* metal_tex_3d */

    [arg_desc_tex release];

    if (@available(macos 12.0, *)) {
      if (use_metalrt) {
        MTLArgumentDescriptor *arg_desc_as = [[MTLArgumentDescriptor alloc] init];
        arg_desc_as.dataType = MTLDataTypeInstanceAccelerationStructure;
        arg_desc_as.access = MTLArgumentAccessReadOnly;

        MTLArgumentDescriptor *arg_desc_ptrs = [[MTLArgumentDescriptor alloc] init];
        arg_desc_ptrs.dataType = MTLDataTypePointer;
        arg_desc_ptrs.access = MTLArgumentAccessReadOnly;

        MTLArgumentDescriptor *arg_desc_ift = [[MTLArgumentDescriptor alloc] init];
        arg_desc_ift.dataType = MTLDataTypeIntersectionFunctionTable;
        arg_desc_ift.access = MTLArgumentAccessReadOnly;

        arg_desc_as.index = index++;
        [ancillary_desc addObject:[arg_desc_as copy]]; /* accel_struct */
        arg_desc_ift.index = index++;
        [ancillary_desc addObject:[arg_desc_ift copy]]; /* ift_default */
        arg_desc_ift.index = index++;
        [ancillary_desc addObject:[arg_desc_ift copy]]; /* ift_shadow */
        arg_desc_ift.index = index++;
        [ancillary_desc addObject:[arg_desc_ift copy]]; /* ift_local */
        arg_desc_ift.index = index++;
        [ancillary_desc addObject:[arg_desc_ift copy]]; /* ift_local_prim */
        arg_desc_ptrs.index = index++;
        [ancillary_desc addObject:[arg_desc_ptrs copy]]; /* blas array */
        arg_desc_ptrs.index = index++;
        [ancillary_desc addObject:[arg_desc_ptrs copy]]; /* look up table for blas */

        [arg_desc_ift release];
        [arg_desc_as release];
        [arg_desc_ptrs release];
      }
    }

    mtlAncillaryArgEncoder = [mtlDevice newArgumentEncoderWithArguments:ancillary_desc];

    // preparing the blas arg encoder
    if (@available(macos 11.0, *)) {
      if (use_metalrt) {
        MTLArgumentDescriptor *arg_desc_blas = [[MTLArgumentDescriptor alloc] init];
        arg_desc_blas.dataType = MTLDataTypeInstanceAccelerationStructure;
        arg_desc_blas.access = MTLArgumentAccessReadOnly;
        mtlBlasArgEncoder = [mtlDevice newArgumentEncoderWithArguments:@[ arg_desc_blas ]];
        [arg_desc_blas release];
      }
    }

    for (int i = 0; i < ancillary_desc.count; i++) {
      [ancillary_desc[i] release];
    }
    [ancillary_desc release];
  }
  [arg_desc_params release];
  [arg_desc_texture release];
}

MetalDevice::~MetalDevice()
{
  /* Cancel any async shader compilations that are in flight. */
  cancel();

  /* This lock safeguards against destruction during use (see other uses of
   * existing_devices_mutex). */
  thread_scoped_lock lock(existing_devices_mutex);

  int num_resources = texture_info.size();
  for (int res = 0; res < num_resources; res++) {
    if (is_texture(texture_info[res])) {
      [texture_slot_map[res] release];
      texture_slot_map[res] = nil;
    }
  }

  flush_delayed_free_list();

  if (texture_bindings_2d) {
    stats.mem_free(buffer_bindings_1d.allocatedSize + texture_bindings_2d.allocatedSize +
                   texture_bindings_3d.allocatedSize);
    [buffer_bindings_1d release];
    [texture_bindings_2d release];
    [texture_bindings_3d release];
  }
  [mtlTextureArgEncoder release];
  [mtlBufferKernelParamsEncoder release];
  [mtlBufferArgEncoder release];
  [mtlASArgEncoder release];
  [mtlAncillaryArgEncoder release];
  [mtlGeneralCommandQueue release];
  [mtlDevice release];

  texture_info.free();
}

bool MetalDevice::support_device(const uint /*kernel_features*/)
{
  return true;
}

bool MetalDevice::check_peer_access(Device * /*peer_device*/)
{
  assert(0);
  /* does peer access make sense? */
  return false;
}

bool MetalDevice::use_adaptive_compilation()
{
  return DebugFlags().metal.adaptive_compile;
}

bool MetalDevice::use_local_atomic_sort() const
{
  return DebugFlags().metal.use_local_atomic_sort;
}

string MetalDevice::preprocess_source(MetalPipelineType pso_type,
                                      const uint kernel_features,
                                      string *source)
{
  string global_defines;
  if (use_adaptive_compilation()) {
    global_defines += "#define __KERNEL_FEATURES__ " + to_string(kernel_features) + "\n";
  }

  if (use_local_atomic_sort()) {
    global_defines += "#define __KERNEL_LOCAL_ATOMIC_SORT__\n";
  }

  if (use_metalrt) {
    global_defines += "#define __METALRT__\n";
    if (motion_blur) {
      global_defines += "#define __METALRT_MOTION__\n";
    }
  }

#  ifdef WITH_CYCLES_DEBUG
  global_defines += "#define __KERNEL_DEBUG__\n";
#  endif

  switch (device_vendor) {
    default:
      break;
    case METAL_GPU_INTEL:
      global_defines += "#define __KERNEL_METAL_INTEL__\n";
      break;
    case METAL_GPU_AMD:
      global_defines += "#define __KERNEL_METAL_AMD__\n";
      break;
    case METAL_GPU_APPLE:
      global_defines += "#define __KERNEL_METAL_APPLE__\n";
#  ifdef WITH_NANOVDB
      if (DebugFlags().metal.use_nanovdb) {
        global_defines += "#define WITH_NANOVDB\n";
      }
#  endif
      break;
  }

  NSProcessInfo *processInfo = [NSProcessInfo processInfo];
  NSOperatingSystemVersion macos_ver = [processInfo operatingSystemVersion];
  global_defines += "#define __KERNEL_METAL_MACOS__ " + to_string(macos_ver.majorVersion) + "\n";

  /* Replace specific KernelData "dot" dereferences with a Metal function_constant identifier of
   * the same character length. Build a string of all active constant values which is then hashed
   * in order to identify the PSO.
   */
  if (pso_type != PSO_GENERIC) {
    if (source) {
      const double starttime = time_dt();

#  define KERNEL_STRUCT_BEGIN(name, parent) \
    string_replace_same_length(*source, "kernel_data." #parent ".", "kernel_data_" #parent "_");

      bool next_member_is_specialized = true;

#  define KERNEL_STRUCT_MEMBER_DONT_SPECIALIZE next_member_is_specialized = false;

#  define KERNEL_STRUCT_MEMBER(parent, _type, name) \
    if (!next_member_is_specialized) { \
      string_replace( \
          *source, "kernel_data_" #parent "_" #name, "kernel_data." #parent ".__unused_" #name); \
      next_member_is_specialized = true; \
    }

#  include "kernel/data_template.h"

#  undef KERNEL_STRUCT_MEMBER
#  undef KERNEL_STRUCT_MEMBER_DONT_SPECIALIZE
#  undef KERNEL_STRUCT_BEGIN

      metal_printf("KernelData patching took %.1f ms\n", (time_dt() - starttime) * 1000.0);
    }

    /* Opt in to all of available specializations. This can be made more granular for the
     * PSO_SPECIALIZED_INTERSECT case in order to minimize the number of specialization requests,
     * but the overhead should be negligible as these are very quick to (re)build and aren't
     * serialized to disk via MTLBinaryArchives.
     */
    global_defines += "#define __KERNEL_USE_DATA_CONSTANTS__\n";
  }

#  if 0
  metal_printf("================\n%s================\n",
               global_defines.c_str());
#  endif

  if (source) {
    *source = global_defines + *source;
  }

  MD5Hash md5;
  md5.append(global_defines);
  return md5.get_hex();
}

void MetalDevice::make_source(MetalPipelineType pso_type, const uint kernel_features)
{
  string &source = this->source[pso_type];
  source = "\n#include \"kernel/device/metal/kernel.metal\"\n";
  source = path_source_replace_includes(source, path_get("source"));

  /* Perform any required specialization on the source.
   * With Metal function constants we can generate a single variant of the kernel source which can
   * be repeatedly respecialized.
   */
  global_defines_md5[pso_type] = preprocess_source(pso_type, kernel_features, &source);
}

bool MetalDevice::load_kernels(const uint _kernel_features)
{
  kernel_features = _kernel_features;

  /* check if GPU is supported */
  if (!support_device(kernel_features))
    return false;

  /* Keep track of whether motion blur is enabled, so to enable/disable motion in BVH builds
   * This is necessary since objects may be reported to have motion if the Vector pass is
   * active, but may still need to be rendered without motion blur if that isn't active as well. */
  motion_blur = kernel_features & KERNEL_FEATURE_OBJECT_MOTION;

  /* Only request generic kernels if they aren't cached in memory. */
  if (make_source_and_check_if_compile_needed(PSO_GENERIC)) {
    /* If needed, load them asynchronously in order to responsively message progress to the user.
     */
    int this_device_id = this->device_id;
    auto compile_kernels_fn = ^() {
      compile_and_load(this_device_id, PSO_GENERIC);
    };

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                   compile_kernels_fn);
  }

  return true;
}

bool MetalDevice::make_source_and_check_if_compile_needed(MetalPipelineType pso_type)
{
  string defines_md5 = preprocess_source(pso_type, kernel_features);

  /* Rebuild the source string if the injected block of #defines has changed. */
  if (global_defines_md5[pso_type] != defines_md5) {
    make_source(pso_type, kernel_features);
  }

  string constant_values;
  if (pso_type != PSO_GENERIC) {
    bool next_member_is_specialized = true;

#  define KERNEL_STRUCT_MEMBER_DONT_SPECIALIZE next_member_is_specialized = false;

    /* Add specialization constants to md5 so that 'get_best_pipeline' is able to return a suitable
     * match. */
#  define KERNEL_STRUCT_MEMBER(parent, _type, name) \
    if (next_member_is_specialized) { \
      constant_values += string(#parent "." #name "=") + \
                         to_string(_type(launch_params.data.parent.name)) + "\n"; \
    } \
    else { \
      next_member_is_specialized = true; \
    }

#  include "kernel/data_template.h"

#  undef KERNEL_STRUCT_MEMBER
#  undef KERNEL_STRUCT_MEMBER_DONT_SPECIALIZE

#  if 0
    metal_printf("================\n%s================\n",
                constant_values.c_str());
#  endif
  }

  MD5Hash md5;
  md5.append(constant_values);
  md5.append(source[pso_type]);
  if (use_metalrt) {
    md5.append(string_printf("metalrt_features=%d", kernel_features & METALRT_FEATURE_MASK));
  }
  kernels_md5[pso_type] = md5.get_hex();

  return MetalDeviceKernels::should_load_kernels(this, pso_type);
}

void MetalDevice::compile_and_load(int device_id, MetalPipelineType pso_type)
{
  /* Thread-safe front-end compilation. Typically the MSL->AIR compilation can take a few seconds,
   * so we avoid blocking device tear-down if the user cancels a render immediately. */

  id<MTLDevice> mtlDevice;
  string source;
  MetalGPUVendor device_vendor;

  /* Safely gather any state required for the MSL->AIR compilation. */
  {
    thread_scoped_lock lock(existing_devices_mutex);

    /* Check whether the device still exists. */
    MetalDevice *instance = get_device_by_ID(device_id, lock);
    if (!instance) {
      metal_printf("Ignoring %s compilation request - device no longer exists\n",
                   kernel_type_as_string(pso_type));
      return;
    }

    if (!instance->make_source_and_check_if_compile_needed(pso_type)) {
      /* We already have a full set of matching pipelines which are cached or queued. Return early
       * to avoid redundant MTLLibrary compilation. */
      metal_printf("Ignoreing %s compilation request - kernels already requested\n",
                   kernel_type_as_string(pso_type));
      return;
    }

    mtlDevice = instance->mtlDevice;
    device_vendor = instance->device_vendor;
    source = instance->source[pso_type];
  }

  /* Perform the actual compilation using our cached context. The MetalDevice can safely destruct
   * in this time. */

  MTLCompileOptions *options = [[MTLCompileOptions alloc] init];

#  if defined(MAC_OS_VERSION_13_0)
  if (@available(macos 13.0, *)) {
    if (device_vendor == METAL_GPU_INTEL) {
      [options setOptimizationLevel:MTLLibraryOptimizationLevelSize];
    }
  }
#  endif

  options.fastMathEnabled = YES;
  if (@available(macOS 12.0, *)) {
    options.languageVersion = MTLLanguageVersion2_4;
  }

  if (getenv("CYCLES_METAL_PROFILING") || getenv("CYCLES_METAL_DEBUG")) {
    path_write_text(path_cache_get(string_printf("%s.metal", kernel_type_as_string(pso_type))),
                    source);
  }

  double starttime = time_dt();

  NSError *error = NULL;
  id<MTLLibrary> mtlLibrary = [mtlDevice newLibraryWithSource:@(source.c_str())
                                                      options:options
                                                        error:&error];

  metal_printf("Front-end compilation finished in %.1f seconds (%s)\n",
               time_dt() - starttime,
               kernel_type_as_string(pso_type));

  [options release];

  bool blocking_pso_build = (getenv("CYCLES_METAL_PROFILING") ||
                             MetalDeviceKernels::is_benchmark_warmup());
  if (blocking_pso_build) {
    MetalDeviceKernels::wait_for_all();
    starttime = 0.0;
  }

  /* Save the compiled MTLLibrary and trigger the AIR->PSO builds (if the MetalDevice still
   * exists). */
  {
    thread_scoped_lock lock(existing_devices_mutex);
    if (MetalDevice *instance = get_device_by_ID(device_id, lock)) {
      if (mtlLibrary) {
        if (error && [error localizedDescription]) {
          VLOG_WARNING << "MSL compilation messages: "
                       << [[error localizedDescription] UTF8String];
        }

        instance->mtlLibrary[pso_type] = mtlLibrary;

        starttime = time_dt();
        MetalDeviceKernels::load(instance, pso_type);
      }
      else {
        NSString *err = [error localizedDescription];
        instance->set_error(string_printf("Failed to compile library:\n%s", [err UTF8String]));
      }
    }
  }

  if (starttime && blocking_pso_build) {
    MetalDeviceKernels::wait_for_all();

    metal_printf("Back-end compilation finished in %.1f seconds (%s)\n",
                 time_dt() - starttime,
                 kernel_type_as_string(pso_type));
  }
}

bool MetalDevice::is_texture(const TextureInfo &tex)
{
  return (tex.depth > 0 || tex.height > 0);
}

void MetalDevice::load_texture_info()
{
  if (need_texture_info) {
    /* Unset flag before copying. */
    need_texture_info = false;
    texture_info.copy_to_device();

    int num_textures = texture_info.size();

    for (int tex = 0; tex < num_textures; tex++) {
      uint64_t offset = tex * sizeof(void *);
      if (is_texture(texture_info[tex]) && texture_slot_map[tex]) {
        id<MTLTexture> metal_texture = texture_slot_map[tex];
        MTLTextureType type = metal_texture.textureType;
        [mtlTextureArgEncoder setArgumentBuffer:texture_bindings_2d offset:offset];
        [mtlTextureArgEncoder setTexture:type == MTLTextureType2D ? metal_texture : nil atIndex:0];
        [mtlTextureArgEncoder setArgumentBuffer:texture_bindings_3d offset:offset];
        [mtlTextureArgEncoder setTexture:type == MTLTextureType3D ? metal_texture : nil atIndex:0];
      }
      else {
        [mtlTextureArgEncoder setArgumentBuffer:texture_bindings_2d offset:offset];
        [mtlTextureArgEncoder setTexture:nil atIndex:0];
        [mtlTextureArgEncoder setArgumentBuffer:texture_bindings_3d offset:offset];
        [mtlTextureArgEncoder setTexture:nil atIndex:0];
      }
    }
    if (default_storage_mode == MTLResourceStorageModeManaged) {
      [texture_bindings_2d didModifyRange:NSMakeRange(0, num_textures * sizeof(void *))];
      [texture_bindings_3d didModifyRange:NSMakeRange(0, num_textures * sizeof(void *))];
    }
  }
}

void MetalDevice::erase_allocation(device_memory &mem)
{
  stats.mem_free(mem.device_size);
  mem.device_pointer = 0;
  mem.device_size = 0;

  auto it = metal_mem_map.find(&mem);
  if (it != metal_mem_map.end()) {
    MetalMem *mmem = it->second.get();

    /* blank out reference to MetalMem* in the launch params (fixes crash #94736) */
    if (mmem->pointer_index >= 0) {
      device_ptr *pointers = (device_ptr *)&launch_params;
      pointers[mmem->pointer_index] = 0;
    }
    metal_mem_map.erase(it);
  }
}

bool MetalDevice::max_working_set_exceeded(size_t safety_margin) const
{
  /* We're allowed to allocate beyond the safe working set size, but then if all resources are made
   * resident we will get command buffer failures at render time. */
  size_t available = [mtlDevice recommendedMaxWorkingSetSize] - safety_margin;
  return (stats.mem_used > available);
}

MetalDevice::MetalMem *MetalDevice::generic_alloc(device_memory &mem)
{
  size_t size = mem.memory_size();

  mem.device_pointer = 0;

  id<MTLBuffer> metal_buffer = nil;
  MTLResourceOptions options = default_storage_mode;

  /* Workaround for "bake" unit tests which fail if RenderBuffers is allocated with
   * MTLResourceStorageModeShared. */
  if (strstr(mem.name, "RenderBuffers")) {
    options = MTLResourceStorageModeManaged;
  }

  if (size > 0) {
    if (mem.type == MEM_DEVICE_ONLY && !capture_enabled) {
      options = MTLResourceStorageModePrivate;
    }

    metal_buffer = [mtlDevice newBufferWithLength:size options:options];

    if (!metal_buffer) {
      set_error("System is out of GPU memory");
      return nullptr;
    }
  }

  if (mem.name) {
    VLOG_WORK << "Buffer allocate: " << mem.name << ", "
              << string_human_readable_number(mem.memory_size()) << " bytes. ("
              << string_human_readable_size(mem.memory_size()) << ")";
  }

  mem.device_size = metal_buffer.allocatedSize;
  stats.mem_alloc(mem.device_size);

  metal_buffer.label = [[NSString alloc] initWithFormat:@"%s", mem.name];

  std::lock_guard<std::recursive_mutex> lock(metal_mem_map_mutex);

  assert(metal_mem_map.count(&mem) == 0); /* assert against double-alloc */
  MetalMem *mmem = new MetalMem;
  metal_mem_map[&mem] = std::unique_ptr<MetalMem>(mmem);

  mmem->mem = &mem;
  mmem->mtlBuffer = metal_buffer;
  mmem->offset = 0;
  mmem->size = size;
  if (options != MTLResourceStorageModePrivate) {
    mmem->hostPtr = [metal_buffer contents];
  }
  else {
    mmem->hostPtr = nullptr;
  }

  /* encode device_pointer as (MetalMem*) in order to handle resource relocation and device pointer
   * recalculation */
  mem.device_pointer = device_ptr(mmem);

  if (metal_buffer.storageMode == MTLResourceStorageModeShared) {
    /* Replace host pointer with our host allocation. */

    if (mem.host_pointer && mem.host_pointer != mmem->hostPtr) {
      memcpy(mmem->hostPtr, mem.host_pointer, size);

      mem.host_free();
      mem.host_pointer = mmem->hostPtr;
    }
    mem.shared_pointer = mmem->hostPtr;
    mem.shared_counter++;
    mmem->use_UMA = true;
  }
  else {
    mmem->use_UMA = false;
  }

  if (max_working_set_exceeded()) {
    set_error("System is out of GPU memory");
    return nullptr;
  }

  return mmem;
}

void MetalDevice::generic_copy_to(device_memory &mem)
{
  if (!mem.host_pointer || !mem.device_pointer) {
    return;
  }

  std::lock_guard<std::recursive_mutex> lock(metal_mem_map_mutex);
  if (!metal_mem_map.at(&mem)->use_UMA || mem.host_pointer != mem.shared_pointer) {
    MetalMem &mmem = *metal_mem_map.at(&mem);
    memcpy(mmem.hostPtr, mem.host_pointer, mem.memory_size());
    if (mmem.mtlBuffer.storageMode == MTLStorageModeManaged) {
      [mmem.mtlBuffer didModifyRange:NSMakeRange(0, mem.memory_size())];
    }
  }
}

void MetalDevice::generic_free(device_memory &mem)
{
  if (mem.device_pointer) {
    std::lock_guard<std::recursive_mutex> lock(metal_mem_map_mutex);
    MetalMem &mmem = *metal_mem_map.at(&mem);
    size_t size = mmem.size;

    /* If mmem.use_uma is true, reference counting is used
     * to safely free memory. */

    bool free_mtlBuffer = false;

    if (mmem.use_UMA) {
      assert(mem.shared_pointer);
      if (mem.shared_pointer) {
        assert(mem.shared_counter > 0);
        if (--mem.shared_counter == 0) {
          free_mtlBuffer = true;
        }
      }
    }
    else {
      free_mtlBuffer = true;
    }

    if (free_mtlBuffer) {
      if (mem.host_pointer && mem.host_pointer == mem.shared_pointer) {
        /* Safely move the device-side data back to the host before it is freed. */
        mem.host_pointer = mem.host_alloc(size);
        memcpy(mem.host_pointer, mem.shared_pointer, size);
        mmem.use_UMA = false;
      }

      mem.shared_pointer = 0;

      /* Free device memory. */
      delayed_free_list.push_back(mmem.mtlBuffer);
      mmem.mtlBuffer = nil;
    }

    erase_allocation(mem);
  }
}

void MetalDevice::mem_alloc(device_memory &mem)
{
  if (mem.type == MEM_TEXTURE) {
    assert(!"mem_alloc not supported for textures.");
  }
  else if (mem.type == MEM_GLOBAL) {
    generic_alloc(mem);
  }
  else {
    generic_alloc(mem);
  }
}

void MetalDevice::mem_copy_to(device_memory &mem)
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

void MetalDevice::mem_copy_from(device_memory &mem, size_t y, size_t w, size_t h, size_t elem)
{
  if (mem.host_pointer) {

    bool subcopy = (w >= 0 && h >= 0);
    const size_t size = subcopy ? (elem * w * h) : mem.memory_size();
    const size_t offset = subcopy ? (elem * y * w) : 0;

    if (mem.device_pointer) {
      std::lock_guard<std::recursive_mutex> lock(metal_mem_map_mutex);
      MetalMem &mmem = *metal_mem_map.at(&mem);

      if ([mmem.mtlBuffer storageMode] == MTLStorageModeManaged) {

        id<MTLCommandBuffer> cmdBuffer = [mtlGeneralCommandQueue commandBuffer];
        id<MTLBlitCommandEncoder> blitEncoder = [cmdBuffer blitCommandEncoder];
        [blitEncoder synchronizeResource:mmem.mtlBuffer];
        [blitEncoder endEncoding];
        [cmdBuffer commit];
        [cmdBuffer waitUntilCompleted];
      }

      if (mem.host_pointer != mmem.hostPtr) {
        memcpy((uchar *)mem.host_pointer + offset, (uchar *)mmem.hostPtr + offset, size);
      }
    }
    else {
      memset((char *)mem.host_pointer + offset, 0, size);
    }
  }
}

void MetalDevice::mem_zero(device_memory &mem)
{
  if (!mem.device_pointer) {
    mem_alloc(mem);
  }
  if (!mem.device_pointer) {
    return;
  }

  size_t size = mem.memory_size();
  std::lock_guard<std::recursive_mutex> lock(metal_mem_map_mutex);
  MetalMem &mmem = *metal_mem_map.at(&mem);
  memset(mmem.hostPtr, 0, size);
  if ([mmem.mtlBuffer storageMode] == MTLStorageModeManaged) {
    [mmem.mtlBuffer didModifyRange:NSMakeRange(0, size)];
  }
}

void MetalDevice::mem_free(device_memory &mem)
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

device_ptr MetalDevice::mem_alloc_sub_ptr(device_memory & /*mem*/,
                                          size_t /*offset*/,
                                          size_t /*size*/)
{
  /* METAL_WIP - revive if necessary */
  assert(0);
  return 0;
}

void MetalDevice::cancel()
{
  /* Remove this device's ID from the list of active devices. Any pending compilation requests
   * originating from this session will be cancelled. */
  thread_scoped_lock lock(existing_devices_mutex);
  if (device_id) {
    active_device_ids.erase(device_id);
    device_id = 0;
  }
}

bool MetalDevice::is_ready(string &status) const
{
  if (!error_msg.empty()) {
    /* Avoid hanging if we had an error. */
    return true;
  }

  int num_loaded = MetalDeviceKernels::get_loaded_kernel_count(this, PSO_GENERIC);
  if (num_loaded < DEVICE_KERNEL_NUM) {
    status = string_printf("%d / %d render kernels loaded (may take a few minutes the first time)",
                           num_loaded,
                           DEVICE_KERNEL_NUM);
    return false;
  }

  if (int num_requests = MetalDeviceKernels::num_incomplete_specialization_requests()) {
    status = string_printf("%d kernels to optimize", num_requests);
  }
  else if (kernel_specialization_level == PSO_SPECIALIZED_INTERSECT) {
    status = "Using optimized intersection kernels";
  }
  else if (kernel_specialization_level == PSO_SPECIALIZED_SHADE) {
    status = "Using optimized kernels";
  }

  metal_printf("MetalDevice::is_ready(...) --> true\n");
  return true;
}

void MetalDevice::optimize_for_scene(Scene *scene)
{
  MetalPipelineType specialization_level = kernel_specialization_level;

  if (!scene->params.background) {
    /* In live viewport, don't specialize beyond intersection kernels for responsiveness. */
    specialization_level = (MetalPipelineType)min(specialization_level, PSO_SPECIALIZED_INTERSECT);
  }

  /* For responsive rendering, specialize the kernels in the background, and only if there isn't an
   * existing "optimize_for_scene" request in flight. */
  int this_device_id = this->device_id;
  auto specialize_kernels_fn = ^() {
    for (int level = 1; level <= int(specialization_level); level++) {
      compile_and_load(this_device_id, MetalPipelineType(level));
    }
  };

  /* In normal use, we always compile the specialized kernels in the background. */
  bool specialize_in_background = true;

  /* Block if a per-kernel profiling is enabled (ensure steady rendering rate). */
  if (getenv("CYCLES_METAL_PROFILING") != nullptr) {
    specialize_in_background = false;
  }

  /* Block during benchmark warm-up to ensure kernels are cached prior to the observed run. */
  if (MetalDeviceKernels::is_benchmark_warmup()) {
    specialize_in_background = false;
  }

  if (specialize_in_background) {
    if (MetalDeviceKernels::num_incomplete_specialization_requests() == 0) {
      dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                     specialize_kernels_fn);
    }
    else {
      metal_printf("\"optimize_for_scene\" request already in flight - dropping request\n");
    }
  }
  else {
    specialize_kernels_fn();
  }
}

void MetalDevice::const_copy_to(const char *name, void *host, size_t size)
{
  if (strcmp(name, "data") == 0) {
    assert(size == sizeof(KernelData));
    memcpy((uint8_t *)&launch_params.data, host, sizeof(KernelData));
    return;
  }

  auto update_launch_pointers =
      [&](size_t offset, void *data, size_t data_size, size_t pointers_size) {
        memcpy((uint8_t *)&launch_params + offset, data, data_size);

        MetalMem **mmem = (MetalMem **)data;
        int pointer_count = pointers_size / sizeof(device_ptr);
        int pointer_index = offset / sizeof(device_ptr);
        for (int i = 0; i < pointer_count; i++) {
          if (mmem[i]) {
            mmem[i]->pointer_index = pointer_index + i;
          }
        }
      };

  /* Update data storage pointers in launch parameters. */
  if (strcmp(name, "integrator_state") == 0) {
    /* IntegratorStateGPU is contiguous pointers */
    const size_t pointer_block_size = offsetof(IntegratorStateGPU, sort_partition_divisor);
    update_launch_pointers(
        offsetof(KernelParamsMetal, integrator_state), host, size, pointer_block_size);
  }
#  define KERNEL_DATA_ARRAY(data_type, tex_name) \
    else if (strcmp(name, #tex_name) == 0) { \
      update_launch_pointers(offsetof(KernelParamsMetal, tex_name), host, size, size); \
    }
#  include "kernel/data_arrays.h"
#  undef KERNEL_DATA_ARRAY
}

void MetalDevice::global_alloc(device_memory &mem)
{
  if (mem.is_resident(this)) {
    generic_alloc(mem);
    generic_copy_to(mem);
  }

  const_copy_to(mem.name, &mem.device_pointer, sizeof(mem.device_pointer));
}

void MetalDevice::global_free(device_memory &mem)
{
  if (mem.is_resident(this) && mem.device_pointer) {
    generic_free(mem);
  }
}

void MetalDevice::tex_alloc_as_buffer(device_texture &mem)
{
  MetalDevice::MetalMem *mmem = generic_alloc(mem);
  generic_copy_to(mem);

  /* Resize once */
  const uint slot = mem.slot;
  if (slot >= texture_info.size()) {
    /* Allocate some slots in advance, to reduce amount
     * of re-allocations. */
    texture_info.resize(round_up(slot + 1, 128));
    texture_slot_map.resize(round_up(slot + 1, 128));
  }

  texture_info[slot] = mem.info;
  uint64_t offset = slot * sizeof(void *);
  [mtlBufferArgEncoder setArgumentBuffer:buffer_bindings_1d offset:offset];
  [mtlBufferArgEncoder setBuffer:mmem->mtlBuffer offset:0 atIndex:0];
  texture_info[slot].data = *(uint64_t *)((uint64_t)buffer_bindings_1d.contents + offset);
  texture_slot_map[slot] = nil;
  need_texture_info = true;
}

void MetalDevice::tex_alloc(device_texture &mem)
{
  /* Check that dimensions fit within maximum allowable size.
   * If 1D texture is allocated, use 1D buffer.
   * See: https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf */
  if (mem.data_height > 0) {
    if (mem.data_width > 16384 || mem.data_height > 16384) {
      set_error(string_printf(
          "Texture exceeds maximum allowed size of 16384 x 16384 (requested: %zu x %zu)",
          mem.data_width,
          mem.data_height));
      return;
    }
  }
  MTLStorageMode storage_mode = MTLStorageModeManaged;
  if (@available(macos 10.15, *)) {
    /* Intel GPUs don't support MTLStorageModeShared for MTLTextures. */
    if ([mtlDevice hasUnifiedMemory] && device_vendor != METAL_GPU_INTEL) {
      storage_mode = MTLStorageModeShared;
    }
  }

  /* General variables for both architectures */
  string bind_name = mem.name;
  size_t dsize = datatype_size(mem.data_type);
  size_t size = mem.memory_size();

  /* sampler_index maps into the GPU's constant 'metal_samplers' array */
  uint64_t sampler_index = mem.info.extension;
  if (mem.info.interpolation != INTERPOLATION_CLOSEST) {
    sampler_index += 4;
  }

  /* Image Texture Storage */
  MTLPixelFormat format;
  switch (mem.data_type) {
    case TYPE_UCHAR: {
      MTLPixelFormat formats[] = {MTLPixelFormatR8Unorm,
                                  MTLPixelFormatRG8Unorm,
                                  MTLPixelFormatInvalid,
                                  MTLPixelFormatRGBA8Unorm};
      format = formats[mem.data_elements - 1];
    } break;
    case TYPE_UINT16: {
      MTLPixelFormat formats[] = {MTLPixelFormatR16Unorm,
                                  MTLPixelFormatRG16Unorm,
                                  MTLPixelFormatInvalid,
                                  MTLPixelFormatRGBA16Unorm};
      format = formats[mem.data_elements - 1];
    } break;
    case TYPE_UINT: {
      MTLPixelFormat formats[] = {MTLPixelFormatR32Uint,
                                  MTLPixelFormatRG32Uint,
                                  MTLPixelFormatInvalid,
                                  MTLPixelFormatRGBA32Uint};
      format = formats[mem.data_elements - 1];
    } break;
    case TYPE_INT: {
      MTLPixelFormat formats[] = {MTLPixelFormatR32Sint,
                                  MTLPixelFormatRG32Sint,
                                  MTLPixelFormatInvalid,
                                  MTLPixelFormatRGBA32Sint};
      format = formats[mem.data_elements - 1];
    } break;
    case TYPE_FLOAT: {
      MTLPixelFormat formats[] = {MTLPixelFormatR32Float,
                                  MTLPixelFormatRG32Float,
                                  MTLPixelFormatInvalid,
                                  MTLPixelFormatRGBA32Float};
      format = formats[mem.data_elements - 1];
    } break;
    case TYPE_HALF: {
      MTLPixelFormat formats[] = {MTLPixelFormatR16Float,
                                  MTLPixelFormatRG16Float,
                                  MTLPixelFormatInvalid,
                                  MTLPixelFormatRGBA16Float};
      format = formats[mem.data_elements - 1];
    } break;
    default:
      assert(0);
      return;
  }

  assert(format != MTLPixelFormatInvalid);

  id<MTLTexture> mtlTexture = nil;
  size_t src_pitch = mem.data_width * dsize * mem.data_elements;

  if (mem.data_depth > 1) {
    /* 3D texture using array */
    MTLTextureDescriptor *desc;

    desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                              width:mem.data_width
                                                             height:mem.data_height
                                                          mipmapped:NO];

    desc.storageMode = storage_mode;
    desc.usage = MTLTextureUsageShaderRead;

    desc.textureType = MTLTextureType3D;
    desc.depth = mem.data_depth;

    VLOG_WORK << "Texture 3D allocate: " << mem.name << ", "
              << string_human_readable_number(mem.memory_size()) << " bytes. ("
              << string_human_readable_size(mem.memory_size()) << ")";

    mtlTexture = [mtlDevice newTextureWithDescriptor:desc];
    if (!mtlTexture) {
      set_error("System is out of GPU memory");
      return;
    }

    const size_t imageBytes = src_pitch * mem.data_height;
    for (size_t d = 0; d < mem.data_depth; d++) {
      const size_t offset = d * imageBytes;
      [mtlTexture replaceRegion:MTLRegionMake3D(0, 0, d, mem.data_width, mem.data_height, 1)
                    mipmapLevel:0
                          slice:0
                      withBytes:(uint8_t *)mem.host_pointer + offset
                    bytesPerRow:src_pitch
                  bytesPerImage:0];
    }
  }
  else if (mem.data_height > 0) {
    /* 2D texture */
    MTLTextureDescriptor *desc;

    desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                              width:mem.data_width
                                                             height:mem.data_height
                                                          mipmapped:NO];

    desc.storageMode = storage_mode;
    desc.usage = MTLTextureUsageShaderRead;

    VLOG_WORK << "Texture 2D allocate: " << mem.name << ", "
              << string_human_readable_number(mem.memory_size()) << " bytes. ("
              << string_human_readable_size(mem.memory_size()) << ")";

    mtlTexture = [mtlDevice newTextureWithDescriptor:desc];
    if (!mtlTexture) {
      set_error("System is out of GPU memory");
      return;
    }

    [mtlTexture replaceRegion:MTLRegionMake2D(0, 0, mem.data_width, mem.data_height)
                  mipmapLevel:0
                    withBytes:mem.host_pointer
                  bytesPerRow:src_pitch];
  }
  else {
    /* 1D texture, using linear memory. */
    tex_alloc_as_buffer(mem);
    return;
  }

  mem.device_pointer = (device_ptr)mtlTexture;
  mem.device_size = size;
  stats.mem_alloc(size);

  std::lock_guard<std::recursive_mutex> lock(metal_mem_map_mutex);
  MetalMem *mmem = new MetalMem;
  metal_mem_map[&mem] = std::unique_ptr<MetalMem>(mmem);
  mmem->mem = &mem;
  mmem->mtlTexture = mtlTexture;

  /* Resize once */
  const uint slot = mem.slot;
  if (slot >= texture_info.size()) {
    /* Allocate some slots in advance, to reduce amount
     * of re-allocations. */
    texture_info.resize(slot + 128);
    texture_slot_map.resize(slot + 128);

    ssize_t min_buffer_length = sizeof(void *) * texture_info.size();
    if (!texture_bindings_2d || (texture_bindings_2d.length < min_buffer_length)) {
      if (texture_bindings_2d) {
        delayed_free_list.push_back(buffer_bindings_1d);
        delayed_free_list.push_back(texture_bindings_2d);
        delayed_free_list.push_back(texture_bindings_3d);

        stats.mem_free(buffer_bindings_1d.allocatedSize + texture_bindings_2d.allocatedSize +
                       texture_bindings_3d.allocatedSize);
      }
      buffer_bindings_1d = [mtlDevice newBufferWithLength:min_buffer_length
                                                  options:default_storage_mode];
      texture_bindings_2d = [mtlDevice newBufferWithLength:min_buffer_length
                                                   options:default_storage_mode];
      texture_bindings_3d = [mtlDevice newBufferWithLength:min_buffer_length
                                                   options:default_storage_mode];

      stats.mem_alloc(buffer_bindings_1d.allocatedSize + texture_bindings_2d.allocatedSize +
                      texture_bindings_3d.allocatedSize);
    }
  }

  if (@available(macos 10.14, *)) {
    /* Optimize the texture for GPU access. */
    id<MTLCommandBuffer> commandBuffer = [mtlGeneralCommandQueue commandBuffer];
    id<MTLBlitCommandEncoder> blitCommandEncoder = [commandBuffer blitCommandEncoder];
    [blitCommandEncoder optimizeContentsForGPUAccess:mtlTexture];
    [blitCommandEncoder endEncoding];
    [commandBuffer commit];
  }

  /* Set Mapping and tag that we need to (re-)upload to device */
  texture_slot_map[slot] = mtlTexture;
  texture_info[slot] = mem.info;
  need_texture_info = true;

  texture_info[slot].data = uint64_t(slot) | (sampler_index << 32);

  if (max_working_set_exceeded()) {
    set_error("System is out of GPU memory");
  }
}

void MetalDevice::tex_free(device_texture &mem)
{
  if (mem.data_depth == 0 && mem.data_height == 0) {
    generic_free(mem);
    return;
  }

  if (metal_mem_map.count(&mem)) {
    std::lock_guard<std::recursive_mutex> lock(metal_mem_map_mutex);
    MetalMem &mmem = *metal_mem_map.at(&mem);

    assert(texture_slot_map[mem.slot] == mmem.mtlTexture);
    if (texture_slot_map[mem.slot] == mmem.mtlTexture)
      texture_slot_map[mem.slot] = nil;

    if (mmem.mtlTexture) {
      /* Free bindless texture. */
      delayed_free_list.push_back(mmem.mtlTexture);
      mmem.mtlTexture = nil;
    }
    erase_allocation(mem);
  }
}

unique_ptr<DeviceQueue> MetalDevice::gpu_queue_create()
{
  return make_unique<MetalDeviceQueue>(this);
}

bool MetalDevice::should_use_graphics_interop()
{
  /* METAL_WIP - provide fast interop */
  return false;
}

void MetalDevice::flush_delayed_free_list()
{
  /* free any Metal buffers that may have been freed by host while a command
   * buffer was being generated. This function should be called after each
   * completion of a command buffer */
  std::lock_guard<std::recursive_mutex> lock(metal_mem_map_mutex);
  for (auto &it : delayed_free_list) {
    [it release];
  }
  delayed_free_list.clear();
}

void MetalDevice::build_bvh(BVH *bvh, Progress &progress, bool refit)
{
  if (bvh->params.bvh_layout == BVH_LAYOUT_BVH2) {
    Device::build_bvh(bvh, progress, refit);
    return;
  }

  BVHMetal *bvh_metal = static_cast<BVHMetal *>(bvh);
  bvh_metal->motion_blur = motion_blur;
  if (bvh_metal->build(progress, mtlDevice, mtlGeneralCommandQueue, refit)) {

    if (@available(macos 11.0, *)) {
      if (bvh->params.top_level) {
        bvhMetalRT = bvh_metal;

        // allocate required buffers for BLAS array
        uint64_t count = bvhMetalRT->blas_array.size();
        uint64_t bufferSize = mtlBlasArgEncoder.encodedLength * count;
        blas_buffer = [mtlDevice newBufferWithLength:bufferSize options:default_storage_mode];
        stats.mem_alloc(blas_buffer.allocatedSize);

        for (uint64_t i = 0; i < count; ++i) {
          [mtlBlasArgEncoder setArgumentBuffer:blas_buffer
                                        offset:i * mtlBlasArgEncoder.encodedLength];
          [mtlBlasArgEncoder setAccelerationStructure:bvhMetalRT->blas_array[i] atIndex:0];
        }

        count = bvhMetalRT->blas_lookup.size();
        bufferSize = sizeof(uint32_t) * count;
        blas_lookup_buffer = [mtlDevice newBufferWithLength:bufferSize
                                                    options:default_storage_mode];
        stats.mem_alloc(blas_lookup_buffer.allocatedSize);

        memcpy([blas_lookup_buffer contents],
               bvhMetalRT -> blas_lookup.data(),
               blas_lookup_buffer.allocatedSize);

        if (default_storage_mode == MTLResourceStorageModeManaged) {
          [blas_buffer didModifyRange:NSMakeRange(0, blas_buffer.length)];
          [blas_lookup_buffer didModifyRange:NSMakeRange(0, blas_lookup_buffer.length)];
        }
      }
    }
  }

  if (max_working_set_exceeded()) {
    set_error("System is out of GPU memory");
  }
}

CCL_NAMESPACE_END

#endif
