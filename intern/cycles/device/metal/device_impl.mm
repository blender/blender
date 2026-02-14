/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_METAL

#  include <map>
#  include <mutex>

#  include "device/metal/device.h"
#  include "device/metal/device_impl.h"

#  include "scene/scene.h"

#  include "session/display_driver.h"

#  include "util/debug.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/time.h"

#  include <TargetConditionals.h>
#  include <crt_externs.h>

CCL_NAMESPACE_BEGIN

class MetalDevice;

thread_mutex MetalDevice::existing_devices_mutex;
std::map<int, MetalDevice *> MetalDevice::active_device_ids;

/* Thread-safe device access for async work. Calling code must pass an appropriately scoped lock
 * to existing_devices_mutex to safeguard against destruction of the returned instance. */
MetalDevice *MetalDevice::get_device_by_ID(const int ID,
                                           thread_scoped_lock & /*existing_devices_mutex_lock*/)
{
  auto it = active_device_ids.find(ID);
  if (it != active_device_ids.end()) {
    return it->second;
  }
  return nullptr;
}

bool MetalDevice::is_device_cancelled(const int ID)
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
    LOG_ERROR << "Refer to the Cycles GPU rendering documentation for possible solutions:\n"
                 "https://docs.blender.org/manual/en/latest/render/cycles/gpu_rendering.html\n";
    has_error = true;
  }
}

MetalDevice::MetalDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler, bool headless)
    : Device(info, stats, profiler, headless), image_info(this, "image_info", MEM_GLOBAL)
{
  @autoreleasepool {
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
    metal_printf("Creating new Cycles Metal device: %s", info.description.c_str());

    /* Ensure that back-compatibility helpers for getting gpuAddress & gpuResourceID are set up. */
    metal_gpu_address_helper_init(mtlDevice);

    /* Enable increased concurrent shader compiler limit.
     * This is also done by MTLContext::MTLContext, but only in GUI mode. */
    if (@available(macOS 13.3, *)) {
      [mtlDevice setShouldMaximizeConcurrentCompilation:YES];
    }

    max_threads_per_threadgroup = 512;

    use_metalrt = info.use_hardware_raytracing;
    if (const char *metalrt = getenv("CYCLES_METALRT")) {
      use_metalrt = (atoi(metalrt) != 0);
    }

    if (const char *str = getenv("CYCLES_METALRT_EXTENDED_LIMITS")) {
      use_metalrt_extended_limits = (atoi(str) != 0);
    }

#  if defined(MAC_OS_VERSION_15_0)
    /* Use "Ray tracing with per component motion interpolation" if available.
     * Requires Apple9 support (https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf). */
    if (use_metalrt && [mtlDevice supportsFamily:MTLGPUFamilyApple9]) {
      /* Concave motion paths weren't correctly bounded prior to macOS 15.6 (#136253). */
      if (@available(macos 15.6, *)) {
        use_pcmi = DebugFlags().metal.use_metalrt_pcmi;
      }
    }
#  endif

    if (getenv("CYCLES_DEBUG_METAL_CAPTURE_KERNEL")) {
      capture_enabled = true;
    }

    /* Create a global counter sampling buffer when kernel profiling is enabled.
     * There's a limit to the number of concurrent counter sampling buffers per device, so we
     * create one that can be reused by successive device queues. */
    if (auto str = getenv("CYCLES_METAL_PROFILING")) {
      if (atoi(str) && [mtlDevice supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary])
      {
        NSArray<id<MTLCounterSet>> *counterSets = [mtlDevice counterSets];

        NSError *error = nil;
        MTLCounterSampleBufferDescriptor *desc = [[MTLCounterSampleBufferDescriptor alloc] init];
        [desc setStorageMode:MTLStorageModeShared];
        [desc setLabel:@"CounterSampleBuffer"];
        [desc setSampleCount:MAX_SAMPLE_BUFFER_LENGTH];
        [desc setCounterSet:counterSets[0]];
        mtlCounterSampleBuffer = [mtlDevice newCounterSampleBufferWithDescriptor:desc
                                                                           error:&error];
        [mtlCounterSampleBuffer retain];
      }
    }

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

    if (auto *envstr = getenv("CYCLES_METAL_SPECIALIZATION_LEVEL")) {
      kernel_specialization_level = (MetalPipelineType)atoi(envstr);
    }
    metal_printf("kernel_specialization_level = %s",
                 kernel_type_as_string(
                     (MetalPipelineType)min((int)kernel_specialization_level, (int)PSO_NUM - 1)));

    image_bindings = [mtlDevice newBufferWithLength:8192 options:MTLResourceStorageModeShared];
    stats.mem_alloc(image_bindings.allocatedSize);

    launch_params_buffer = [mtlDevice newBufferWithLength:sizeof(KernelParamsMetal)
                                                  options:MTLResourceStorageModeShared];
    stats.mem_alloc(sizeof(KernelParamsMetal));

    /* Cache unified pointer so we can write kernel params directly in place. */
    launch_params = (KernelParamsMetal *)launch_params_buffer.contents;

    /* Command queue for path-tracing work on the GPU. In a situation where multiple
     * MetalDeviceQueues are spawned from one MetalDevice, they share the same MTLCommandQueue.
     * This is thread safe and just as performant as each having their own instance. It also
     * adheres to best practices of maximizing the lifetime of each MTLCommandQueue. */
    mtlComputeCommandQueue = [mtlDevice newCommandQueue];

    /* Command queue for non-tracing work on the GPU. */
    mtlGeneralCommandQueue = [mtlDevice newCommandQueue];
  }
}

MetalDevice::~MetalDevice()
{
  /* Cancel any async shader compilations that are in flight. */
  cancel();

  /* This lock safeguards against destruction during use (see other uses of
   * existing_devices_mutex). */
  thread_scoped_lock lock(existing_devices_mutex);

  /* Release textures that weren't already freed by tex_free. */
  for (int res = 0; res < image_info.size(); res++) {
    [image_info_id_map[res] release];
    image_info_id_map[res] = nil;
  }

  free_bvh();
  flush_delayed_free_list();

  stats.mem_free(sizeof(KernelParamsMetal));
  [launch_params_buffer release];

  stats.mem_free(image_bindings.allocatedSize);
  [image_bindings release];

  [mtlComputeCommandQueue release];
  [mtlGeneralCommandQueue release];
  if (mtlCounterSampleBuffer) {
    [mtlCounterSampleBuffer release];
  }
  [mtlDevice release];

  image_info.free();
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
    global_defines += "#define __KERNEL_METALRT__\n";
    if (motion_blur) {
      global_defines += "#define __METALRT_MOTION__\n";
    }
    if (use_metalrt_extended_limits) {
      global_defines += "#define __METALRT_EXTENDED_LIMITS__\n";
    }
  }

#  ifdef WITH_CYCLES_DEBUG
  global_defines += "#define WITH_CYCLES_DEBUG\n";
#  endif

  global_defines += "#define __KERNEL_METAL_APPLE__\n";
  if (@available(macos 14.0, *)) {
    /* Use Program Scope Global Built-ins, when available. */
    global_defines += "#define __METAL_GLOBAL_BUILTINS__\n";
  }
#  ifdef WITH_NANOVDB
  /* Compiling in NanoVDB results in a marginal drop in render performance,
   * so disable it for specialized PSOs when no images are using it. */
  if ((pso_type == PSO_GENERIC || using_nanovdb) && DebugFlags().metal.use_nanovdb) {
    global_defines += "#define WITH_NANOVDB\n";
  }
#  endif

  NSProcessInfo *processInfo = [NSProcessInfo processInfo];
  NSOperatingSystemVersion macos_ver = [processInfo operatingSystemVersion];
  global_defines += "#define __KERNEL_METAL_MACOS__ " + to_string(macos_ver.majorVersion) + "\n";

#  if TARGET_CPU_ARM64
  global_defines += "#define __KERNEL_METAL_TARGET_CPU_ARM64__\n";
#  endif

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

      /* Replace "kernel_data.kernel_features" memory fetches with a function constant. */
      string_replace_same_length(
          *source, "kernel_data.kernel_features", "kernel_data_kernel_features");

      metal_printf("KernelData patching took %.1f ms", (time_dt() - starttime) * 1000.0);
    }

    /* Opt in to all of available specializations. This can be made more granular for the
     * PSO_SPECIALIZED_INTERSECT case in order to minimize the number of specialization requests,
     * but the overhead should be negligible as these are very quick to (re)build and aren't
     * serialized to disk via MTLBinaryArchives.
     */
    global_defines += "#define __KERNEL_USE_DATA_CONSTANTS__\n";
  }

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
  @autoreleasepool {
    kernel_features |= _kernel_features;

    /* check if GPU is supported */
    if (!support_device(kernel_features)) {
      return false;
    }

    /* Keep track of whether motion blur is enabled, so to enable/disable motion in BVH builds
     * This is necessary since objects may be reported to have motion if the Vector pass is
     * active, but may still need to be rendered without motion blur if that isn't active as well.
     */
    motion_blur = motion_blur || (kernel_features & KERNEL_FEATURE_OBJECT_MOTION);

    /* Only request generic kernels if they aren't cached in memory. */
    refresh_source_and_kernels_md5(PSO_GENERIC);
    if (MetalDeviceKernels::should_load_kernels(this, PSO_GENERIC)) {
      /* If needed, load them asynchronously in order to responsively message progress to the user.
       */
      int this_device_id = this->device_id;
      auto compile_kernels_fn = ^() {
        compile_and_load(this_device_id, PSO_GENERIC);
      };

      dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                     compile_kernels_fn);
    }
  }
  return true;
}

void MetalDevice::refresh_source_and_kernels_md5(MetalPipelineType pso_type)
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
                         to_string(_type(launch_params->data.parent.name)) + "\n"; \
    } \
    else { \
      next_member_is_specialized = true; \
    }

#  include "kernel/data_template.h"

#  undef KERNEL_STRUCT_MEMBER
#  undef KERNEL_STRUCT_MEMBER_DONT_SPECIALIZE
  }

  MD5Hash md5;
  md5.append(constant_values);
  md5.append(source[pso_type]);
  if (use_metalrt) {
    md5.append(string_printf("metalrt_features=%d", kernel_features & METALRT_FEATURE_MASK));
  }
  kernels_md5[pso_type] = md5.get_hex();
}

void MetalDevice::compile_and_load(const int device_id, MetalPipelineType pso_type)
{
  @autoreleasepool {
    /* Thread-safe front-end compilation. Typically the MSL->AIR compilation can take a few
     * seconds, so we avoid blocking device tear-down if the user cancels a render immediately. */

    id<MTLDevice> mtlDevice;
    string source;

    /* Safely gather any state required for the MSL->AIR compilation. */
    {
      thread_scoped_lock lock(existing_devices_mutex);

      /* Check whether the device still exists. */
      MetalDevice *instance = get_device_by_ID(device_id, lock);
      if (!instance) {
        metal_printf("Ignoring %s compilation request - device no longer exists",
                     kernel_type_as_string(pso_type));
        return;
      }

      if (!MetalDeviceKernels::should_load_kernels(instance, pso_type)) {
        /* We already have a full set of matching pipelines which are cached or queued. Return
         * early to avoid redundant MTLLibrary compilation. */
        metal_printf("Ignoreing %s compilation request - kernels already requested",
                     kernel_type_as_string(pso_type));
        return;
      }

      mtlDevice = instance->mtlDevice;
      source = instance->source[pso_type];
    }

    /* Perform the actual compilation using our cached context. The MetalDevice can safely destruct
     * in this time. */

    MTLCompileOptions *options = [[MTLCompileOptions alloc] init];

    options.fastMathEnabled = YES;
    if (@available(macos 12.0, *)) {
      options.languageVersion = MTLLanguageVersion2_4;
    }
#  if defined(MAC_OS_VERSION_13_0)
    if (@available(macos 13.0, *)) {
      options.languageVersion = MTLLanguageVersion3_0;
    }
#  endif
#  if defined(MAC_OS_VERSION_14_0)
    if (@available(macos 14.0, *)) {
      options.languageVersion = MTLLanguageVersion3_1;
    }
#  endif
#  if defined(MAC_OS_VERSION_15_0)
    if (@available(macos 15.0, *)) {
      options.languageVersion = MTLLanguageVersion3_2;
      if (const char *loglevel = getenv("MTL_LOG_LEVEL")) {
        if (strcmp(loglevel, "MTLLogLevelDebug") == 0) {
          options.enableLogging = true;
        }
      }
    }
#  endif

    if (getenv("CYCLES_METAL_PROFILING") || getenv("CYCLES_METAL_DEBUG")) {
      path_write_text(path_cache_get(string_printf("%s.metal", kernel_type_as_string(pso_type))),
                      source);
    }

    double starttime = time_dt();

    NSError *error = nullptr;
    id<MTLLibrary> mtlLibrary = [mtlDevice newLibraryWithSource:@(source.c_str())
                                                        options:options
                                                          error:&error];

    metal_printf("Front-end compilation finished in %.1f seconds (%s)",
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
            LOG_WARNING << "MSL compilation messages: "
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

      metal_printf("Back-end compilation finished in %.1f seconds (%s)",
                   time_dt() - starttime,
                   kernel_type_as_string(pso_type));
    }
  }
}

bool MetalDevice::is_texture(const KernelImageInfo &info)
{
  return info.height > 0;
}

void MetalDevice::erase_allocation(device_memory &mem)
{
  stats.mem_free(mem.device_size);
  mem.device_pointer = 0;
  mem.device_size = 0;

  auto it = metal_mem_map.find(&mem);
  if (it != metal_mem_map.end()) {
    MetalMem *mmem = it->second.get();

    /* blank out reference to resource in the launch params (fixes crash #94736) */
    if (mmem->pointer_index >= 0) {
      device_ptr *pointers = (device_ptr *)launch_params;
      pointers[mmem->pointer_index] = 0;
    }
    metal_mem_map.erase(it);
  }
}

bool MetalDevice::max_working_set_exceeded(const size_t safety_margin) const
{
  /* We're allowed to allocate beyond the safe working set size, but then if all resources are made
   * resident we will get command buffer failures at render time. */
  size_t available = [mtlDevice recommendedMaxWorkingSetSize] - safety_margin;
  return (stats.mem_used > available);
}

MetalDevice::MetalMem *MetalDevice::generic_alloc(device_memory &mem)
{
  @autoreleasepool {
    size_t size = mem.memory_size();

    mem.device_pointer = 0;

    id<MTLBuffer> metal_buffer = nil;
    MTLResourceOptions options = MTLResourceStorageModeShared;

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

    LOG_DEBUG << "Buffer allocate: " << mem.log_name() << ", "
              << string_human_readable_number(mem.memory_size()) << " bytes. ("
              << string_human_readable_size(mem.memory_size()) << ")";

    mem.device_size = metal_buffer.allocatedSize;
    stats.mem_alloc(mem.device_size);

    metal_buffer.label = [NSString stringWithFormat:@"%s", mem.log_name().c_str()];

    std::lock_guard<std::recursive_mutex> lock(metal_mem_map_mutex);

    assert(metal_mem_map.count(&mem) == 0); /* assert against double-alloc */
    unique_ptr<MetalMem> mmem = make_unique<MetalMem>();

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

    /* encode device_pointer as (MetalMem*) in order to handle resource relocation and device
     * pointer recalculation */
    mem.device_pointer = device_ptr(mmem.get());

    if (metal_buffer.storageMode == MTLStorageModeShared) {
      /* Replace host pointer with our host allocation. */
      if (mem.host_pointer && mem.host_pointer != mmem->hostPtr) {
        memcpy(mmem->hostPtr, mem.host_pointer, size);

        host_free(mem.type, mem.host_pointer, mem.memory_size());
        mem.host_pointer = mmem->hostPtr;
      }
      mem.shared_pointer = mmem->hostPtr;
      mem.shared_counter++;
    }

    MetalMem *mmem_ptr = mmem.get();
    metal_mem_map[&mem] = std::move(mmem);

    if (max_working_set_exceeded()) {
      set_error("System is out of GPU memory");
      return nullptr;
    }

    return mmem_ptr;
  }
}

void MetalDevice::generic_copy_to(device_memory & /*mem*/)
{
  /* No need to copy - Apple Silicon has Unified Memory Architecture. */
}

void MetalDevice::generic_free(device_memory &mem)
{
  if (!mem.device_pointer) {
    return;
  }

  /* Host pointer should already have been freed at this point. If not we might
   * end up freeing shared memory and can't recover original host memory. */
  assert(mem.host_pointer == nullptr);

  std::lock_guard<std::recursive_mutex> lock(metal_mem_map_mutex);
  MetalMem &mmem = *metal_mem_map.at(&mem);
  size_t size = mmem.size;

  bool free_mtlBuffer = true;

  /* If this is shared, reference counting is used to safely free memory. */
  if (mem.shared_pointer) {
    assert(mem.shared_counter > 0);
    if (--mem.shared_counter > 0) {
      free_mtlBuffer = false;
    }
  }

  if (free_mtlBuffer) {
    if (mem.host_pointer && mem.host_pointer == mem.shared_pointer) {
      /* Safely move the device-side data back to the host before it is freed.
       * We should actually never reach this code as it is inefficient, but
       * better than to crash if there is a bug. */
      assert(!"Metal device should not copy memory back to host");
      mem.host_pointer = mem.host_alloc(size);
      memcpy(mem.host_pointer, mem.shared_pointer, size);
    }

    mem.shared_pointer = nullptr;

    /* Free device memory. */
    delayed_free_list.push_back(mmem.mtlBuffer);
    mmem.mtlBuffer = nil;
  }

  erase_allocation(mem);
}

void MetalDevice::mem_alloc(device_memory &mem)
{
  if (mem.type == MEM_IMAGE_TEXTURE) {
    assert(!"mem_alloc not supported for images.");
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
  if (!mem.device_pointer) {
    if (mem.type == MEM_GLOBAL) {
      global_alloc(mem);
    }
    else if (mem.type == MEM_IMAGE_TEXTURE) {
      image_alloc((device_image &)mem);
    }
    else {
      generic_alloc(mem);
      generic_copy_to(mem);
    }
  }
  else if (mem.is_resident(this)) {
    if (mem.type == MEM_GLOBAL) {
      generic_copy_to(mem);
    }
    else if (mem.type == MEM_IMAGE_TEXTURE) {
      image_copy_to((device_image &)mem);
    }
    else {
      generic_copy_to(mem);
    }
  }
}

void MetalDevice::mem_move_to_host(device_memory & /*mem*/)
{
  /* Metal implements own mechanism for moving host memory. */
  assert(!"Metal does not support mem_move_to_host");
}

void MetalDevice::mem_copy_from(
    device_memory & /*mem*/, const size_t /*y*/, size_t /*w*/, const size_t /*h*/, size_t /*elem*/)
{
  /* No need to copy - Apple Silicon has Unified Memory Architecture. */
}

void MetalDevice::mem_zero(device_memory &mem)
{
  if (!mem.device_pointer) {
    mem_alloc(mem);
  }
  assert(mem.shared_pointer);
  memset(mem.shared_pointer, 0, mem.memory_size());
}

void MetalDevice::mem_free(device_memory &mem)
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

  metal_printf("MetalDevice::is_ready(...) --> true");
  return true;
}

bool MetalDevice::set_bvh_limits(size_t instance_count, size_t max_prim_count)
{
  /* For object & primitive counts above a certain limit, MetalRT requires extended limits to be
   * built into the kernels, and when building BVHs. Following best practices, this should only
   * be enabled when necessary. See
   * https://developer.apple.com/documentation/metal/mtlaccelerationstructureusage/mtlaccelerationstructureusageextendedlimits?language=objc
   */

  const int standard_limits_max_prim_count = (1 << 28);
  const int standard_limits_max_instance_count = (1 << 24);

  bool using_metalrt_extended_limits_before = use_metalrt_extended_limits;

  /* Enable extended limits if object count exceeds max supported by standard limits.
   * Once enabled, it remains enabled for the lifetime of the device. */
  if (instance_count > standard_limits_max_instance_count ||
      max_prim_count > standard_limits_max_prim_count)
  {
    use_metalrt_extended_limits = true;
    metal_printf("Enabling MetalRT extended limits (max_prim_count = %zu, instance_count = %zu)",
                 max_prim_count,
                 instance_count);
  }

  /* All BVHs need to be rebuilt if the extended limits state changes. */
  return using_metalrt_extended_limits_before != use_metalrt_extended_limits;
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
      metal_printf("\"optimize_for_scene\" request already in flight - dropping request");
    }
  }
  else {
    specialize_kernels_fn();
  }
}

void MetalDevice::const_copy_to(const char *name, void *host, const size_t size)
{
  if (strcmp(name, "data") == 0) {
    assert(size == sizeof(KernelData));
    memcpy((uint8_t *)&launch_params->data, host, sizeof(KernelData));

    /* Refresh the kernels_md5 checksums for specialized kernel sets. */
    for (int level = 1; level <= int(kernel_specialization_level); level++) {
      refresh_source_and_kernels_md5(MetalPipelineType(level));
    }
    return;
  }

  auto update_launch_pointers = [&](size_t offset, void *data, const size_t pointers_size) {
    uint64_t *addresses = (uint64_t *)((uint8_t *)launch_params + offset);

    MetalMem **mmem = (MetalMem **)data;
    int pointer_count = pointers_size / sizeof(device_ptr);
    int pointer_index = offset / sizeof(device_ptr);
    for (int i = 0; i < pointer_count; i++) {
      addresses[i] = 0;
      if (mmem[i]) {
        mmem[i]->pointer_index = pointer_index + i;
        if (mmem[i]->mtlBuffer) {
          if (@available(macOS 13.0, *)) {
            addresses[i] = metal_gpuAddress(mmem[i]->mtlBuffer);
          }
        }
      }
    }
  };

  /* Update data storage pointers in launch parameters. */
  if (strcmp(name, "integrator_state") == 0) {
    /* IntegratorStateGPU is contiguous pointers up until sort_partition_divisor. */
    const size_t pointer_block_size = offsetof(IntegratorStateGPU, sort_partition_divisor);
    update_launch_pointers(
        offsetof(KernelParamsMetal, integrator_state), host, pointer_block_size);

    /* Ensure the non-pointers part of IntegratorStateGPU is copied (this is the proper fix for
     * #144713). */
    memcpy((uint8_t *)&launch_params->integrator_state + pointer_block_size,
           (uint8_t *)host + pointer_block_size,
           sizeof(IntegratorStateGPU) - pointer_block_size);
  }
#  define KERNEL_DATA_ARRAY(data_type, tex_name) \
    else if (strcmp(name, #tex_name) == 0) { \
      update_launch_pointers(offsetof(KernelParamsMetal, tex_name), host, size); \
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

  const_copy_to(mem.global_name(), &mem.device_pointer, sizeof(mem.device_pointer));
}

void MetalDevice::global_free(device_memory &mem)
{
  if (mem.is_resident(this) && mem.device_pointer) {
    generic_free(mem);
  }
}

void MetalDevice::image_alloc_as_buffer(device_image &mem)
{
  MetalDevice::MetalMem *mmem = generic_alloc(mem);
  generic_copy_to(mem);

  /* Resize once */
  const uint image_info_id = mem.image_info_id;
  if (image_info_id >= image_info.size()) {
    /* Allocate some image_info_ids in advance, to reduce amount
     * of re-allocations. */
    image_info.resize(round_up(image_info_id + 1, 128));
    image_info_id_map.resize(round_up(image_info_id + 1, 128));
  }

  image_info[image_info_id] = mem.info;
  image_info_id_map[image_info_id] = mmem->mtlBuffer;

  if (is_nanovdb_type(mem.info.data_type)) {
    using_nanovdb = true;
  }
}

void MetalDevice::image_alloc(device_image &mem)
{
  @autoreleasepool {
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

    /* General variables for both architectures */
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
    size_t src_pitch = mem.data_width * datatype_size(mem.data_type) * mem.data_elements;

    if (mem.data_height > 0) {
      /* 2D texture */
      MTLTextureDescriptor *desc;

      desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                                width:mem.data_width
                                                               height:mem.data_height
                                                            mipmapped:NO];

      desc.storageMode = MTLStorageModeShared;
      desc.usage = MTLTextureUsageShaderRead;

      /* Disallow lossless texture compression. Path-tracing texture access patterns are very
       * random, and cache reuse gains are typically too low to offset the decompression overheads.
       */
      desc.allowGPUOptimizedContents = false;

      LOG_DEBUG << "Texture 2D allocate: " << mem.log_name() << ", "
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
      /* 1D image, using linear memory. */
      image_alloc_as_buffer(mem);
      return;
    }

    mem.device_pointer = (device_ptr)mtlTexture;
    mem.device_size = size;
    stats.mem_alloc(size);

    std::lock_guard<std::recursive_mutex> lock(metal_mem_map_mutex);
    unique_ptr<MetalMem> mmem = make_unique<MetalMem>();
    mmem->mem = &mem;
    mmem->mtlTexture = mtlTexture;
    metal_mem_map[&mem] = std::move(mmem);

    /* Resize once */
    const uint image_info_id = mem.image_info_id;
    if (image_info_id >= image_info.size()) {
      /* Allocate some image_info_ids in advance, to reduce amount
       * of re-allocations. */
      image_info.resize(image_info_id + 128);
      image_info_id_map.resize(image_info_id + 128);

      ssize_t min_buffer_length = sizeof(void *) * image_info.size();
      if (!image_bindings || (image_bindings.length < min_buffer_length)) {
        if (image_bindings) {
          delayed_free_list.push_back(image_bindings);
          stats.mem_free(image_bindings.allocatedSize);
        }
        image_bindings = [mtlDevice newBufferWithLength:min_buffer_length
                                                options:MTLResourceStorageModeShared];

        stats.mem_alloc(image_bindings.allocatedSize);
      }
    }

    /* Set Mapping. */
    image_info_id_map[image_info_id] = mtlTexture;
    image_info[image_info_id] = mem.info;
    image_info[image_info_id].data = uint64_t(image_info_id) | (sampler_index << 32);

    if (max_working_set_exceeded()) {
      set_error("System is out of GPU memory");
    }
  }
}

void MetalDevice::image_copy_to(device_image &mem)
{
  if (mem.is_resident(this)) {
    const size_t src_pitch = mem.data_width * datatype_size(mem.data_type) * mem.data_elements;

    if (mem.data_height > 0) {
      id<MTLTexture> mtlTexture;
      {
        std::lock_guard<std::recursive_mutex> lock(metal_mem_map_mutex);
        mtlTexture = metal_mem_map.at(&mem)->mtlTexture;
      }
      [mtlTexture replaceRegion:MTLRegionMake2D(0, 0, mem.data_width, mem.data_height)
                    mipmapLevel:0
                      withBytes:mem.host_pointer
                    bytesPerRow:src_pitch];
    }
    else {
      generic_copy_to(mem);
    }
  }
}

void MetalDevice::image_free(device_image &mem)
{
  int image_info_id = mem.image_info_id;
  if (mem.data_height == 0) {
    generic_free(mem);
  }
  else if (metal_mem_map.count(&mem)) {
    std::lock_guard<std::recursive_mutex> lock(metal_mem_map_mutex);
    MetalMem &mmem = *metal_mem_map.at(&mem);

    /* Free bindless texture. */
    delayed_free_list.push_back(mmem.mtlTexture);
    mmem.mtlTexture = nil;
    erase_allocation(mem);
  }
  image_info_id_map[image_info_id] = nil;
}

unique_ptr<DeviceQueue> MetalDevice::gpu_queue_create()
{
  return make_unique<MetalDeviceQueue>(this);
}

bool MetalDevice::should_use_graphics_interop(const GraphicsInteropDevice &interop_device,
                                              const bool /*log*/)
{
  /* Always supported with unified memory. */
  return interop_device.type == GraphicsInteropDevice::METAL;
}

void *MetalDevice::get_native_buffer(device_ptr ptr)
{
  return ((MetalMem *)ptr)->mtlBuffer;
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
  @autoreleasepool {
    if (bvh->params.bvh_layout == BVH_LAYOUT_BVH2) {
      Device::build_bvh(bvh, progress, refit);
      return;
    }

    BVHMetal *bvh_metal = static_cast<BVHMetal *>(bvh);
    bvh_metal->motion_blur = motion_blur;
    bvh_metal->use_pcmi = use_pcmi;
    bvh_metal->extended_limits = use_metalrt_extended_limits;
    if (bvh_metal->build(progress, mtlDevice, mtlGeneralCommandQueue, refit)) {

      if (bvh->params.top_level) {
        update_bvh(bvh_metal);
      }
    }

    if (max_working_set_exceeded()) {
      set_error("System is out of GPU memory");
    }
  }
}

void MetalDevice::free_bvh()
{
  for (id<MTLAccelerationStructure> &blas : unique_blas_array) {
    [blas release];
  }
  unique_blas_array.clear();
  blas_array.clear();

  if (blas_buffer) {
    [blas_buffer release];
    blas_buffer = nil;
  }

  if (accel_struct) {
    [accel_struct release];
    accel_struct = nil;
  }
}

void MetalDevice::update_bvh(BVHMetal *bvh_metal)
{
  free_bvh();

  if (!bvh_metal) {
    return;
  }

  accel_struct = bvh_metal->accel_struct;
  unique_blas_array = bvh_metal->unique_blas_array;
  blas_array = bvh_metal->blas_array;

  [accel_struct retain];
  for (id<MTLAccelerationStructure> &blas : unique_blas_array) {
    [blas retain];
  }

  // Allocate required buffers for BLAS array.
  uint64_t buffer_size = blas_array.size() * sizeof(uint64_t);
  blas_buffer = [mtlDevice newBufferWithLength:buffer_size options:MTLResourceStorageModeShared];
  stats.mem_alloc(blas_buffer.allocatedSize);
}

CCL_NAMESPACE_END

#endif
