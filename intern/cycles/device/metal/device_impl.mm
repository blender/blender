/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

#ifdef WITH_METAL

#  include "device/metal/device_impl.h"
#  include "device/metal/device.h"

#  include "scene/scene.h"

#  include "util/debug.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/time.h"

CCL_NAMESPACE_BEGIN

class MetalDevice;

BVHLayoutMask MetalDevice::get_bvh_layout_mask() const
{
  return use_metalrt ? BVH_LAYOUT_METAL : BVH_LAYOUT_BVH2;
}

void MetalDevice::set_error(const string &error)
{
  static std::mutex s_error_mutex;
  std::lock_guard<std::mutex> lock(s_error_mutex);

  Device::set_error(error);

  if (first_error) {
    fprintf(stderr, "\nRefer to the Cycles GPU rendering documentation for possible solutions:\n");
    fprintf(stderr,
            "https://docs.blender.org/manual/en/latest/render/cycles/gpu_rendering.html\n\n");
    first_error = false;
  }
}

MetalDevice::MetalDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler)
    : Device(info, stats, profiler), texture_info(this, "texture_info", MEM_GLOBAL)
{
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
      init_host_memory();
    }
  }

  texture_bindings_2d = [mtlDevice newBufferWithLength:4096 options:default_storage_mode];
  texture_bindings_3d = [mtlDevice newBufferWithLength:4096 options:default_storage_mode];

  stats.mem_alloc(texture_bindings_2d.allocatedSize + texture_bindings_3d.allocatedSize);

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
      use_metalrt = info.use_metalrt;

      /* Specialize the intersection kernels on Apple GPUs by default as these can be built very
       * quickly. */
      kernel_specialization_level = PSO_SPECIALIZED_INTERSECT;
      break;
    }
  }

  if (auto metalrt = getenv("CYCLES_METALRT")) {
    use_metalrt = (atoi(metalrt) != 0);
  }

  if (getenv("CYCLES_DEBUG_METAL_CAPTURE_KERNEL")) {
    capture_enabled = true;
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
    [ancillary_desc addObject:[arg_desc_tex copy]]; /* metal_tex_2d */
    arg_desc_tex.index = index++;
    [ancillary_desc addObject:[arg_desc_tex copy]]; /* metal_tex_3d */

    [arg_desc_tex release];

    if (@available(macos 12.0, *)) {
      if (use_metalrt) {
        MTLArgumentDescriptor *arg_desc_as = [[MTLArgumentDescriptor alloc] init];
        arg_desc_as.dataType = MTLDataTypeInstanceAccelerationStructure;
        arg_desc_as.access = MTLArgumentAccessReadOnly;

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

        [arg_desc_ift release];
        [arg_desc_as release];
      }
    }

    mtlAncillaryArgEncoder = [mtlDevice newArgumentEncoderWithArguments:ancillary_desc];

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
  for (auto &tex : texture_slot_map) {
    if (tex) {
      [tex release];
      tex = nil;
    }
  }
  flush_delayed_free_list();

  if (texture_bindings_2d) {
    stats.mem_free(texture_bindings_2d.allocatedSize + texture_bindings_3d.allocatedSize);

    [texture_bindings_2d release];
    [texture_bindings_3d release];
  }
  [mtlTextureArgEncoder release];
  [mtlBufferKernelParamsEncoder release];
  [mtlASArgEncoder release];
  [mtlAncillaryArgEncoder release];
  [mtlGeneralCommandQueue release];
  [mtlDevice release];

  texture_info.free();
}

bool MetalDevice::support_device(const uint kernel_features /*requested_features*/)
{
  return true;
}

bool MetalDevice::check_peer_access(Device *peer_device)
{
  assert(0);
  /* does peer access make sense? */
  return false;
}

bool MetalDevice::use_adaptive_compilation()
{
  return DebugFlags().metal.adaptive_compile;
}

void MetalDevice::make_source(MetalPipelineType pso_type, const uint kernel_features)
{
  string global_defines;
  if (use_adaptive_compilation()) {
    global_defines += "#define __KERNEL_FEATURES__ " + to_string(kernel_features) + "\n";
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
      break;
  }

  NSProcessInfo *processInfo = [NSProcessInfo processInfo];
  NSOperatingSystemVersion macos_ver = [processInfo operatingSystemVersion];
  global_defines += "#define __KERNEL_METAL_MACOS__ " + to_string(macos_ver.majorVersion) + "\n";

  string &source = this->source[pso_type];
  source = "\n#include \"kernel/device/metal/kernel.metal\"\n";
  source = path_source_replace_includes(source, path_get("source"));

  /* Perform any required specialization on the source.
   * With Metal function constants we can generate a single variant of the kernel source which can
   * be repeatedly respecialized.
   */
  string baked_constants;

  /* Replace specific KernelData "dot" dereferences with a Metal function_constant identifier of
   * the same character length. Build a string of all active constant values which is then hashed
   * in order to identify the PSO.
   */
  if (pso_type != PSO_GENERIC) {
    const double starttime = time_dt();

#  define KERNEL_STRUCT_BEGIN(name, parent) \
    string_replace_same_length(source, "kernel_data." #parent ".", "kernel_data_" #parent "_");

    /* Add constants to md5 so that 'get_best_pipeline' is able to return a suitable match. */
#  define KERNEL_STRUCT_MEMBER(parent, _type, name) \
    baked_constants += string(#parent "." #name "=") + \
                       to_string(_type(launch_params.data.parent.name)) + "\n";

#  include "kernel/data_template.h"

    /* Opt in to all of available specializations. This can be made more granular for the
     * PSO_SPECIALIZED_INTERSECT case in order to minimize the number of specialization requests,
     * but the overhead should be negligible as these are very quick to (re)build and aren't
     * serialized to disk via MTLBinaryArchives.
     */
    global_defines += "#define __KERNEL_USE_DATA_CONSTANTS__\n";

    metal_printf("KernelData patching took %.1f ms\n", (time_dt() - starttime) * 1000.0);
  }

  source = global_defines + source;
#  if 0
  metal_printf("================\n%s================\n\%s================\n",
               global_defines.c_str(),
               baked_constants.c_str());
#  endif

  /* Generate an MD5 from the source and include any baked constants. This is used when caching
   * PSOs. */
  MD5Hash md5;
  md5.append(baked_constants);
  md5.append(source);
  if (use_metalrt) {
    md5.append(std::to_string(kernel_features & METALRT_FEATURE_MASK));
  }
  source_md5[pso_type] = md5.get_hex();
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

  bool result = compile_and_load(PSO_GENERIC);

  reserve_local_memory(kernel_features);
  return result;
}

bool MetalDevice::compile_and_load(MetalPipelineType pso_type)
{
  make_source(pso_type, kernel_features);

  if (!MetalDeviceKernels::should_load_kernels(this, pso_type)) {
    /* We already have a full set of matching pipelines which are cached or queued. */
    metal_printf("%s kernels already requested\n", kernel_type_as_string(pso_type));
    return true;
  }

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
                    source[pso_type]);
  }

  const double starttime = time_dt();

  NSError *error = NULL;
  mtlLibrary[pso_type] = [mtlDevice newLibraryWithSource:@(source[pso_type].c_str())
                                                 options:options
                                                   error:&error];

  if (!mtlLibrary[pso_type]) {
    NSString *err = [error localizedDescription];
    set_error(string_printf("Failed to compile library:\n%s", [err UTF8String]));
  }

  metal_printf("Front-end compilation finished in %.1f seconds (%s)\n",
               time_dt() - starttime,
               kernel_type_as_string(pso_type));

  [options release];

  return MetalDeviceKernels::load(this, pso_type);
}

void MetalDevice::reserve_local_memory(const uint kernel_features)
{
  /* METAL_WIP - implement this */
}

void MetalDevice::init_host_memory()
{
  /* METAL_WIP - implement this */
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

      id<MTLTexture> metal_texture = texture_slot_map[tex];
      if (!metal_texture) {
        [mtlTextureArgEncoder setArgumentBuffer:texture_bindings_2d offset:offset];
        [mtlTextureArgEncoder setTexture:nil atIndex:0];
        [mtlTextureArgEncoder setArgumentBuffer:texture_bindings_3d offset:offset];
        [mtlTextureArgEncoder setTexture:nil atIndex:0];
      }
      else {
        MTLTextureType type = metal_texture.textureType;
        [mtlTextureArgEncoder setArgumentBuffer:texture_bindings_2d offset:offset];
        [mtlTextureArgEncoder setTexture:type == MTLTextureType2D ? metal_texture : nil atIndex:0];
        [mtlTextureArgEncoder setArgumentBuffer:texture_bindings_3d offset:offset];
        [mtlTextureArgEncoder setTexture:type == MTLTextureType3D ? metal_texture : nil atIndex:0];
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

    /* blank out reference to MetalMem* in the launch params (fixes crash T94736) */
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

device_ptr MetalDevice::mem_alloc_sub_ptr(device_memory &mem, size_t offset, size_t /*size*/)
{
  /* METAL_WIP - revive if necessary */
  assert(0);
  return 0;
}

void MetalDevice::optimize_for_scene(Scene *scene)
{
  MetalPipelineType specialization_level = kernel_specialization_level;

  if (specialization_level < PSO_SPECIALIZED_INTERSECT) {
    return;
  }

  /* PSO_SPECIALIZED_INTERSECT kernels are fast to specialize, so we always load them
   * synchronously. */
  compile_and_load(PSO_SPECIALIZED_INTERSECT);

  if (specialization_level < PSO_SPECIALIZED_SHADE) {
    return;
  }
  if (!scene->params.background) {
    /* Don't load PSO_SPECIALIZED_SHADE kernels during viewport rendering as they are slower to
     * build. */
    return;
  }

  /* PSO_SPECIALIZED_SHADE kernels are slower to specialize, so we load them asynchronously, and
   * only if there isn't an existing load in flight.
   */
  auto specialize_shade_fn = ^() {
    compile_and_load(PSO_SPECIALIZED_SHADE);
    async_compile_and_load = false;
  };

  bool async_specialize_shade = true;

  /* Block if a per-kernel profiling is enabled (ensure steady rendering rate). */
  if (getenv("CYCLES_METAL_PROFILING") != nullptr) {
    async_specialize_shade = false;
  }

  if (async_specialize_shade) {
    if (!async_compile_and_load) {
      async_compile_and_load = true;
      dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                     specialize_shade_fn);
    }
    else {
      metal_printf(
          "Async PSO_SPECIALIZED_SHADE load request already in progress - dropping request\n");
    }
  }
  else {
    specialize_shade_fn();
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
    else if (strcmp(name, #tex_name) == 0) \
    { \
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
  generic_alloc(mem);
  generic_copy_to(mem);

  /* Resize once */
  const uint slot = mem.slot;
  if (slot >= texture_info.size()) {
    /* Allocate some slots in advance, to reduce amount
     * of re-allocations. */
    texture_info.resize(round_up(slot + 1, 128));
  }

  mem.info.data = (uint64_t)mem.device_pointer;

  /* Set Mapping and tag that we need to (re-)upload to device */
  texture_info[slot] = mem.info;
  need_texture_info = true;
}

void MetalDevice::tex_alloc(device_texture &mem)
{
  /* Check that dimensions fit within maximum allowable size.
   * See: https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf */
  if (mem.data_width > 16384 || mem.data_height > 16384) {
    set_error(string_printf(
        "Texture exceeds maximum allowed size of 16384 x 16384 (requested: %zu x %zu)",
        mem.data_width,
        mem.data_height));
    return;
  }

  MTLStorageMode storage_mode = MTLStorageModeManaged;
  if (@available(macos 10.15, *)) {
    if ([mtlDevice hasUnifiedMemory] &&
        device_vendor !=
            METAL_GPU_INTEL) { /* Intel GPUs don't support MTLStorageModeShared for MTLTextures */
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
    assert(0);
    /* 1D texture, using linear memory. */
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
        delayed_free_list.push_back(texture_bindings_2d);
        delayed_free_list.push_back(texture_bindings_3d);

        stats.mem_free(texture_bindings_2d.allocatedSize + texture_bindings_3d.allocatedSize);
      }
      texture_bindings_2d = [mtlDevice newBufferWithLength:min_buffer_length
                                                   options:default_storage_mode];
      texture_bindings_3d = [mtlDevice newBufferWithLength:min_buffer_length
                                                   options:default_storage_mode];

      stats.mem_alloc(texture_bindings_2d.allocatedSize + texture_bindings_3d.allocatedSize);
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
  if (metal_mem_map.count(&mem)) {
    std::lock_guard<std::recursive_mutex> lock(metal_mem_map_mutex);
    MetalMem &mmem = *metal_mem_map.at(&mem);

    assert(texture_slot_map[mem.slot] == mmem.mtlTexture);
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
      }
    }
  }

  if (max_working_set_exceeded()) {
    set_error("System is out of GPU memory");
  }
}

CCL_NAMESPACE_END

#endif
