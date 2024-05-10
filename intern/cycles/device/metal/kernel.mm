/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_METAL

#  include "device/metal/kernel.h"
#  include "device/metal/device_impl.h"
#  include "kernel/device/metal/function_constants.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/tbb.h"
#  include "util/time.h"
#  include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

const char *kernel_type_as_string(MetalPipelineType pso_type)
{
  switch (pso_type) {
    case PSO_GENERIC:
      return "PSO_GENERIC";
    case PSO_SPECIALIZED_INTERSECT:
      return "PSO_SPECIALIZED_INTERSECT";
    case PSO_SPECIALIZED_SHADE:
      return "PSO_SPECIALIZED_SHADE";
    default:
      assert(0);
  }
  return "";
}

struct ShaderCache {
  ShaderCache(id<MTLDevice> _mtlDevice) : mtlDevice(_mtlDevice)
  {
    /* Initialize occupancy tuning LUT. */

    // TODO: Look into tuning for DEVICE_KERNEL_INTEGRATOR_INTERSECT_DEDICATED_LIGHT and
    // DEVICE_KERNEL_INTEGRATOR_SHADE_DEDICATED_LIGHT.

    if (MetalInfo::get_device_vendor(mtlDevice) == METAL_GPU_APPLE) {
      switch (MetalInfo::get_apple_gpu_architecture(mtlDevice)) {
        default:
        case APPLE_M3:
          /* Peak occupancy is achieved through Dynamic Caching on M3 GPUs. */
          for (size_t i = 0; i < DEVICE_KERNEL_NUM; i++) {
            occupancy_tuning[i] = {64, 64};
          }
          break;
        case APPLE_M2_BIG:
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_COMPACT_SHADOW_STATES] = {384, 128};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_INIT_FROM_CAMERA] = {640, 128};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST] = {1024, 64};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW] = {704, 704};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE] = {640, 32};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_QUEUED_PATHS_ARRAY] = {896, 768};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND] = {512, 128};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW] = {32, 32};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE] = {768, 576};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_SORTED_PATHS_ARRAY] = {896, 768};
          break;
        case APPLE_M2:
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_COMPACT_SHADOW_STATES] = {32, 32};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_INIT_FROM_CAMERA] = {832, 32};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST] = {64, 64};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW] = {64, 64};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE] = {704, 32};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_QUEUED_PATHS_ARRAY] = {1024, 256};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND] = {64, 32};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW] = {256, 256};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE] = {448, 384};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_SORTED_PATHS_ARRAY] = {1024, 1024};
          break;
        case APPLE_M1:
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_COMPACT_SHADOW_STATES] = {256, 128};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_INIT_FROM_CAMERA] = {768, 32};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST] = {512, 128};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW] = {384, 128};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE] = {512, 64};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_QUEUED_PATHS_ARRAY] = {512, 256};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND] = {512, 128};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW] = {384, 32};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE] = {576, 384};
          occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_SORTED_PATHS_ARRAY] = {832, 832};
          break;
      }
    }

    occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_SORT_BUCKET_PASS] = {1024, 1024};
    occupancy_tuning[DEVICE_KERNEL_INTEGRATOR_SORT_WRITE_PASS] = {1024, 1024};
  }
  ~ShaderCache();

  /* Get the fastest available pipeline for the specified kernel. */
  MetalKernelPipeline *get_best_pipeline(DeviceKernel kernel, const MetalDevice *device);

  /* Non-blocking request for a kernel, optionally specialized to the scene being rendered by
   * device. */
  void load_kernel(DeviceKernel kernel, MetalDevice *device, MetalPipelineType pso_type);

  bool should_load_kernel(DeviceKernel device_kernel,
                          MetalDevice const *device,
                          MetalPipelineType pso_type);

  void wait_for_all();

  friend ShaderCache *get_shader_cache(id<MTLDevice> mtlDevice);

  void compile_thread_func();

  using PipelineCollection = std::vector<unique_ptr<MetalKernelPipeline>>;

  struct OccupancyTuningParameters {
    int threads_per_threadgroup = 0;
    int num_threads_per_block = 0;
  } occupancy_tuning[DEVICE_KERNEL_NUM];

  std::mutex cache_mutex;

  PipelineCollection pipelines[DEVICE_KERNEL_NUM];
  id<MTLDevice> mtlDevice;

  static bool running;
  std::condition_variable cond_var;
  std::deque<MetalKernelPipeline *> request_queue;
  std::vector<std::thread> compile_threads;
  std::atomic_int incomplete_requests = 0;
  std::atomic_int incomplete_specialization_requests = 0;
};

bool ShaderCache::running = true;

const int MAX_POSSIBLE_GPUS_ON_SYSTEM = 8;
using DeviceShaderCache = std::pair<id<MTLDevice>, unique_ptr<ShaderCache>>;
int g_shaderCacheCount = 0;
DeviceShaderCache g_shaderCache[MAX_POSSIBLE_GPUS_ON_SYSTEM];

ShaderCache *get_shader_cache(id<MTLDevice> mtlDevice)
{
  for (int i = 0; i < g_shaderCacheCount; i++) {
    if (g_shaderCache[i].first == mtlDevice) {
      return g_shaderCache[i].second.get();
    }
  }

  static thread_mutex g_shaderCacheCountMutex;
  g_shaderCacheCountMutex.lock();
  int index = g_shaderCacheCount++;
  g_shaderCacheCountMutex.unlock();

  assert(index < MAX_POSSIBLE_GPUS_ON_SYSTEM);
  g_shaderCache[index].first = mtlDevice;
  g_shaderCache[index].second = make_unique<ShaderCache>(mtlDevice);
  return g_shaderCache[index].second.get();
}

ShaderCache::~ShaderCache()
{
  running = false;
  cond_var.notify_all();

  metal_printf("Waiting for ShaderCache threads... (incomplete_requests = %d)\n",
               int(incomplete_requests));
  for (auto &thread : compile_threads) {
    thread.join();
  }
  metal_printf("ShaderCache shut down.\n");
}

void ShaderCache::wait_for_all()
{
  while (incomplete_requests > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void ShaderCache::compile_thread_func()
{
  while (running) {

    /* wait for / acquire next request */
    MetalKernelPipeline *pipeline;
    {
      thread_scoped_lock lock(cache_mutex);
      cond_var.wait(lock, [&] { return !running || !request_queue.empty(); });
      if (!running || request_queue.empty()) {
        continue;
      }

      pipeline = request_queue.front();
      request_queue.pop_front();
    }

    /* Service the request. */
    DeviceKernel device_kernel = pipeline->device_kernel;
    MetalPipelineType pso_type = pipeline->pso_type;

    if (MetalDevice::is_device_cancelled(pipeline->originating_device_id)) {
      /* The originating MetalDevice is no longer active, so this request is obsolete. */
      metal_printf("Cancelling compilation of %s (%s)\n",
                   device_kernel_as_string(device_kernel),
                   kernel_type_as_string(pso_type));
    }
    else {
      /* Do the actual compilation. */
      pipeline->compile();

      thread_scoped_lock lock(cache_mutex);
      auto &collection = pipelines[device_kernel];

      /* Cache up to 3 kernel variants with the same pso_type in memory, purging oldest first. */
      int max_entries_of_same_pso_type = 3;
      for (int i = (int)collection.size() - 1; i >= 0; i--) {
        if (collection[i]->pso_type == pso_type) {
          max_entries_of_same_pso_type -= 1;
          if (max_entries_of_same_pso_type == 0) {
            metal_printf("Purging oldest %s:%s kernel from ShaderCache\n",
                         kernel_type_as_string(pso_type),
                         device_kernel_as_string(device_kernel));
            collection.erase(collection.begin() + i);
            break;
          }
        }
      }
      collection.push_back(unique_ptr<MetalKernelPipeline>(pipeline));
    }
    incomplete_requests--;
    if (pso_type != PSO_GENERIC) {
      incomplete_specialization_requests--;
    }
  }
}

bool ShaderCache::should_load_kernel(DeviceKernel device_kernel,
                                     MetalDevice const *device,
                                     MetalPipelineType pso_type)
{
  if (!running) {
    return false;
  }

  if (device_kernel == DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL) {
    /* Skip megakernel. */
    return false;
  }

  if (device_kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE) {
    if ((device->kernel_features & KERNEL_FEATURE_NODE_RAYTRACE) == 0) {
      /* Skip shade_surface_raytrace kernel if the scene doesn't require it. */
      return false;
    }
  }

  if (device_kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE) {
    if ((device->kernel_features & KERNEL_FEATURE_MNEE) == 0) {
      /* Skip shade_surface_mnee kernel if the scene doesn't require it. */
      return false;
    }
  }

  if (pso_type != PSO_GENERIC) {
    /* Only specialize kernels where it can make an impact. */
    if (device_kernel < DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST ||
        device_kernel > DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL)
    {
      return false;
    }

    /* Only specialize shading / intersection kernels as requested. */
    bool is_shade_kernel = (device_kernel >= DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND);
    bool is_shade_pso = (pso_type == PSO_SPECIALIZED_SHADE);
    if (is_shade_pso != is_shade_kernel) {
      return false;
    }
  }

  {
    /* check whether the kernel has already been requested / cached */
    thread_scoped_lock lock(cache_mutex);
    for (auto &pipeline : pipelines[device_kernel]) {
      if (pipeline->kernels_md5 == device->kernels_md5[pso_type]) {
        return false;
      }
    }
  }

  return true;
}

void ShaderCache::load_kernel(DeviceKernel device_kernel,
                              MetalDevice *device,
                              MetalPipelineType pso_type)
{
  {
    /* create compiler threads on first run */
    thread_scoped_lock lock(cache_mutex);
    if (compile_threads.empty()) {
      /* Limit to 2 MTLCompiler instances by default. In macOS >= 13.3 we can query the upper
       * limit. */
      int max_mtlcompiler_threads = 2;

#  if defined(MAC_OS_VERSION_13_3)
      if (@available(macOS 13.3, *)) {
        /* Subtract one to avoid contention with the real-time GPU module. */
        max_mtlcompiler_threads = max(2,
                                      int([mtlDevice maximumConcurrentCompilationTaskCount]) - 1);
      }
#  endif

      metal_printf("Spawning %d Cycles kernel compilation threads\n", max_mtlcompiler_threads);
      for (int i = 0; i < max_mtlcompiler_threads; i++) {
        compile_threads.push_back(std::thread([this] { this->compile_thread_func(); }));
      }
    }
  }

  if (!should_load_kernel(device_kernel, device, pso_type)) {
    return;
  }

  incomplete_requests++;
  if (pso_type != PSO_GENERIC) {
    incomplete_specialization_requests++;
  }

  MetalKernelPipeline *pipeline = new MetalKernelPipeline;

  /* Keep track of the originating device's ID so that we can cancel requests if the device ceases
   * to be active. */
  pipeline->originating_device_id = device->device_id;
  memcpy(&pipeline->kernel_data_, &device->launch_params.data, sizeof(pipeline->kernel_data_));
  pipeline->pso_type = pso_type;
  pipeline->mtlDevice = mtlDevice;
  pipeline->kernels_md5 = device->kernels_md5[pso_type];
  pipeline->mtlLibrary = device->mtlLibrary[pso_type];
  pipeline->device_kernel = device_kernel;
  pipeline->threads_per_threadgroup = device->max_threads_per_threadgroup;

  if (occupancy_tuning[device_kernel].threads_per_threadgroup) {
    pipeline->threads_per_threadgroup = occupancy_tuning[device_kernel].threads_per_threadgroup;
    pipeline->num_threads_per_block = occupancy_tuning[device_kernel].num_threads_per_block;
  }

  /* metalrt options */
  pipeline->use_metalrt = device->use_metalrt;
  pipeline->kernel_features = device->kernel_features;

  {
    thread_scoped_lock lock(cache_mutex);
    request_queue.push_back(pipeline);
  }
  cond_var.notify_one();
}

MetalKernelPipeline *ShaderCache::get_best_pipeline(DeviceKernel kernel, const MetalDevice *device)
{
  while (running && !device->has_error) {
    /* Search all loaded pipelines with matching kernels_md5 checksums. */
    MetalKernelPipeline *best_match = nullptr;
    {
      thread_scoped_lock lock(cache_mutex);
      for (auto &candidate : pipelines[kernel]) {
        if (candidate->loaded &&
            candidate->kernels_md5 == device->kernels_md5[candidate->pso_type])
        {
          /* Replace existing match if candidate is more specialized. */
          if (!best_match || candidate->pso_type > best_match->pso_type) {
            best_match = candidate.get();
          }
        }
      }
    }

    if (best_match) {
      if (best_match->usage_count == 0 && best_match->pso_type != PSO_GENERIC) {
        metal_printf("Swapping in %s version of %s\n",
                     kernel_type_as_string(best_match->pso_type),
                     device_kernel_as_string(kernel));
      }
      best_match->usage_count += 1;
      return best_match;
    }

    /* Spin until a matching kernel is loaded, or we're shutting down. */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return nullptr;
}

bool MetalKernelPipeline::should_use_binary_archive() const
{
  /* Issues with binary archives in older macOS versions. */
  if (@available(macOS 13.0, *)) {
    if (auto str = getenv("CYCLES_METAL_DISABLE_BINARY_ARCHIVES")) {
      if (atoi(str) != 0) {
        /* Don't archive if we have opted out by env var. */
        return false;
      }
    }
    else {
      /* Workaround for issues using Binary Archives on non-Apple Silicon systems. */
      MetalGPUVendor gpu_vendor = MetalInfo::get_device_vendor(mtlDevice);
      if (gpu_vendor != METAL_GPU_APPLE) {
        return false;
      }
    }

    if (use_metalrt && device_kernel_has_intersection(device_kernel)) {
      /* Binary linked functions aren't supported in binary archives. */
      return false;
    }

    if (pso_type == PSO_GENERIC) {
      /* Archive the generic kernels. */
      return true;
    }

    if ((device_kernel >= DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND &&
         device_kernel <= DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW) ||
        (device_kernel >= DEVICE_KERNEL_SHADER_EVAL_DISPLACE &&
         device_kernel <= DEVICE_KERNEL_SHADER_EVAL_CURVE_SHADOW_TRANSPARENCY))
    {
      /* Archive all shade kernels - they take a long time to compile. */
      return true;
    }

    /* The remaining kernels are all fast to compile. They may get cached by the system shader
     * cache, but will be quick to regenerate if not. */
  }
  return false;
}

static MTLFunctionConstantValues *GetConstantValues(KernelData const *data = nullptr)
{
  MTLFunctionConstantValues *constant_values = [MTLFunctionConstantValues new];

  MTLDataType MTLDataType_int = MTLDataTypeInt;
  MTLDataType MTLDataType_float = MTLDataTypeFloat;
  MTLDataType MTLDataType_float4 = MTLDataTypeFloat4;
  KernelData zero_data = {0};
  if (!data) {
    data = &zero_data;
  }
  [constant_values setConstantValue:&zero_data type:MTLDataType_int atIndex:Kernel_DummyConstant];

  bool next_member_is_specialized = true;

#  define KERNEL_STRUCT_MEMBER_DONT_SPECIALIZE next_member_is_specialized = false;

#  define KERNEL_STRUCT_MEMBER(parent, _type, name) \
    [constant_values setConstantValue:next_member_is_specialized ? (void *)&data->parent.name : \
                                                                   (void *)&zero_data \
                                 type:MTLDataType_##_type \
                              atIndex:KernelData_##parent##_##name]; \
    next_member_is_specialized = true;

#  include "kernel/data_template.h"

  return constant_values;
}

id<MTLFunction> MetalKernelPipeline::make_intersection_function(const char *function_name)
{
  MTLFunctionDescriptor *desc = [MTLIntersectionFunctionDescriptor functionDescriptor];
  desc.name = [@(function_name) copy];

  if (pso_type != PSO_GENERIC) {
    desc.constantValues = GetConstantValues(&kernel_data_);
  }
  else {
    desc.constantValues = GetConstantValues();
  }

  NSError *error = NULL;
  id<MTLFunction> rt_intersection_function = [mtlLibrary newFunctionWithDescriptor:desc
                                                                             error:&error];

  if (rt_intersection_function == nil) {
    NSString *err = [error localizedDescription];
    string errors = [err UTF8String];

    error_str = string_printf(
        "Error getting intersection function \"%s\": %s", function_name, errors.c_str());
  }
  else {
    rt_intersection_function.label = [@(function_name) copy];
  }
  return rt_intersection_function;
}

void MetalKernelPipeline::compile()
{
  const std::string function_name = std::string("cycles_metal_") +
                                    device_kernel_as_string(device_kernel);

  NSError *error = NULL;

  MTLFunctionDescriptor *func_desc = [MTLIntersectionFunctionDescriptor functionDescriptor];
  func_desc.name = [@(function_name.c_str()) copy];

  if (pso_type != PSO_GENERIC) {
    func_desc.constantValues = GetConstantValues(&kernel_data_);
  }
  else {
    func_desc.constantValues = GetConstantValues();
  }

  function = [mtlLibrary newFunctionWithDescriptor:func_desc error:&error];

  if (function == nil) {
    NSString *err = [error localizedDescription];
    string errors = [err UTF8String];
    metal_printf("Error getting function \"%s\": %s", function_name.c_str(), errors.c_str());
    return;
  }

  function.label = [@(function_name.c_str()) copy];

  NSArray *table_functions[METALRT_TABLE_NUM] = {nil};
  NSArray *linked_functions = nil;

  if (use_metalrt && device_kernel_has_intersection(device_kernel)) {

    NSMutableSet *unique_functions = [[NSMutableSet alloc] init];

    auto add_intersection_functions = [&](int table_index,
                                          const char *tri_fn,
                                          const char *curve_fn = nullptr,
                                          const char *point_fn = nullptr) {
      table_functions[table_index] = [NSArray
          arrayWithObjects:make_intersection_function(tri_fn),
                           curve_fn ? make_intersection_function(curve_fn) : nil,
                           point_fn ? make_intersection_function(point_fn) : nil,
                           nil];

      [unique_functions addObjectsFromArray:table_functions[table_index]];
    };

    add_intersection_functions(METALRT_TABLE_DEFAULT,
                               "__intersection__tri",
                               "__intersection__curve",
                               "__intersection__point");
    add_intersection_functions(METALRT_TABLE_SHADOW,
                               "__intersection__tri_shadow",
                               "__intersection__curve_shadow",
                               "__intersection__point_shadow");
    add_intersection_functions(METALRT_TABLE_SHADOW_ALL,
                               "__intersection__tri_shadow_all",
                               "__intersection__curve_shadow_all",
                               "__intersection__point_shadow_all");
    add_intersection_functions(METALRT_TABLE_VOLUME, "__intersection__volume_tri");
    add_intersection_functions(METALRT_TABLE_LOCAL, "__intersection__local_tri");
    add_intersection_functions(METALRT_TABLE_LOCAL_MBLUR, "__intersection__local_tri_mblur");
    add_intersection_functions(METALRT_TABLE_LOCAL_SINGLE_HIT,
                               "__intersection__local_tri_single_hit");
    add_intersection_functions(METALRT_TABLE_LOCAL_SINGLE_HIT_MBLUR,
                               "__intersection__local_tri_single_hit_mblur");

    linked_functions = [[NSArray arrayWithArray:[unique_functions allObjects]]
        sortedArrayUsingComparator:^NSComparisonResult(id<MTLFunction> f1, id<MTLFunction> f2) {
          return [f1.label compare:f2.label];
        }];
    unique_functions = nil;
  }

  MTLComputePipelineDescriptor *computePipelineStateDescriptor =
      [[MTLComputePipelineDescriptor alloc] init];

  computePipelineStateDescriptor.buffers[0].mutability = MTLMutabilityImmutable;
  computePipelineStateDescriptor.buffers[1].mutability = MTLMutabilityImmutable;
  computePipelineStateDescriptor.buffers[2].mutability = MTLMutabilityImmutable;

  computePipelineStateDescriptor.maxTotalThreadsPerThreadgroup = threads_per_threadgroup;
  computePipelineStateDescriptor.threadGroupSizeIsMultipleOfThreadExecutionWidth = true;

  computePipelineStateDescriptor.computeFunction = function;

  /* Attach the additional functions to an MTLLinkedFunctions object */
  if (linked_functions) {
    computePipelineStateDescriptor.linkedFunctions = [[MTLLinkedFunctions alloc] init];
    computePipelineStateDescriptor.linkedFunctions.functions = linked_functions;
  }
  computePipelineStateDescriptor.maxCallStackDepth = 1;
  if (use_metalrt && device_kernel_has_intersection(device_kernel)) {
    computePipelineStateDescriptor.maxCallStackDepth = 2;
  }

  MTLPipelineOption pipelineOptions = MTLPipelineOptionNone;

  bool use_binary_archive = should_use_binary_archive();
  bool loading_existing_archive = false;
  bool creating_new_archive = false;

  id<MTLBinaryArchive> archive = nil;
  string metalbin_path;
  string metalbin_name;
  if (use_binary_archive) {
    NSProcessInfo *processInfo = [NSProcessInfo processInfo];
    string osVersion = [[processInfo operatingSystemVersionString] UTF8String];
    MD5Hash local_md5;
    local_md5.append(kernels_md5);
    local_md5.append(osVersion);
    local_md5.append((uint8_t *)&this->threads_per_threadgroup,
                     sizeof(this->threads_per_threadgroup));

    /* Replace non-alphanumerical characters with underscores. */
    string device_name = [mtlDevice.name UTF8String];
    for (char &c : device_name) {
      if ((c < '0' || c > '9') && (c < 'a' || c > 'z') && (c < 'A' || c > 'Z')) {
        c = '_';
      }
    }

    metalbin_name = device_name;
    metalbin_name = path_join(metalbin_name, device_kernel_as_string(device_kernel));
    metalbin_name = path_join(metalbin_name, kernel_type_as_string(pso_type));
    metalbin_name = path_join(metalbin_name, local_md5.get_hex() + ".bin");

    metalbin_path = path_cache_get(path_join("kernels", metalbin_name));
    path_create_directories(metalbin_path);

    /* Check if shader binary exists on disk, and if so, update the file timestamp for LRU purging
     * to work as intended. */
    loading_existing_archive = path_cache_kernel_exists_and_mark_used(metalbin_path);
    creating_new_archive = !loading_existing_archive;

    MTLBinaryArchiveDescriptor *archiveDesc = [[MTLBinaryArchiveDescriptor alloc] init];
    if (loading_existing_archive) {
      archiveDesc.url = [NSURL fileURLWithPath:@(metalbin_path.c_str())];
    }
    NSError *error = nil;
    archive = [mtlDevice newBinaryArchiveWithDescriptor:archiveDesc error:&error];
    if (!archive) {
      const char *err = error ? [[error localizedDescription] UTF8String] : nullptr;
      metal_printf("newBinaryArchiveWithDescriptor failed: %s\n", err ? err : "nil");
    }
    [archiveDesc release];

    if (loading_existing_archive) {
      pipelineOptions = MTLPipelineOptionFailOnBinaryArchiveMiss;
      computePipelineStateDescriptor.binaryArchives = [NSArray arrayWithObjects:archive, nil];
    }
  }

  bool recreate_archive = false;

  /* Lambda to do the actual pipeline compilation. */
  auto do_compilation = [&]() {
    __block bool compilation_finished = false;
    __block string error_str;

    if (loading_existing_archive || !DebugFlags().metal.use_async_pso_creation) {
      /* Use the blocking variant of newComputePipelineStateWithDescriptor if an archive exists on
       * disk. It should load almost instantaneously, and will fail gracefully when loading a
       * corrupt archive (unlike the async variant). */
      NSError *error = nil;
      pipeline = [mtlDevice newComputePipelineStateWithDescriptor:computePipelineStateDescriptor
                                                          options:pipelineOptions
                                                       reflection:nullptr
                                                            error:&error];
      const char *err = error ? [[error localizedDescription] UTF8String] : nullptr;
      error_str = err ? err : "nil";
    }
    else {
      /* Use the async variant of newComputePipelineStateWithDescriptor if no archive exists on
       * disk. This allows us to respond to app shutdown. */
      [mtlDevice
          newComputePipelineStateWithDescriptor:computePipelineStateDescriptor
                                        options:pipelineOptions
                              completionHandler:^(id<MTLComputePipelineState> computePipelineState,
                                                  MTLComputePipelineReflection * /*reflection*/,
                                                  NSError *error) {
                                pipeline = computePipelineState;

                                /* Retain the pipeline so we can use it safely past the completion
                                 * handler. */
                                if (pipeline) {
                                  [pipeline retain];
                                }
                                const char *err = error ?
                                                      [[error localizedDescription] UTF8String] :
                                                      nullptr;
                                error_str = err ? err : "nil";

                                compilation_finished = true;
                              }];

      /* Immediately wait for either the compilation to finish or for app shutdown. */
      while (ShaderCache::running && !compilation_finished) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    }

    if (creating_new_archive && pipeline) {
      /* Add pipeline into the new archive. */
      NSError *error;
      if (![archive addComputePipelineFunctionsWithDescriptor:computePipelineStateDescriptor
                                                        error:&error])
      {
        NSString *errStr = [error localizedDescription];
        metal_printf("Failed to add PSO to archive:\n%s\n", errStr ? [errStr UTF8String] : "nil");
      }
    }

    if (!pipeline) {
      metal_printf(
          "newComputePipelineStateWithDescriptor failed for \"%s\"%s. "
          "Error:\n%s\n",
          device_kernel_as_string((DeviceKernel)device_kernel),
          (archive && !recreate_archive) ? " Archive may be incomplete or corrupt - attempting "
                                           "recreation.." :
                                           "",
          error_str.c_str());
    }
  };

  double starttime = time_dt();

  do_compilation();

  /* An archive might have a corrupt entry and fail to materialize the pipeline. This shouldn't
   * happen, but if it does we recreate it. */
  if (pipeline == nil && archive) {
    recreate_archive = true;
    pipelineOptions = MTLPipelineOptionNone;
    path_remove(metalbin_path);

    do_compilation();
  }

  double duration = time_dt() - starttime;

  if (pipeline == nil) {
    metal_printf("%16s | %2d | %-55s | %7.2fs | FAILED!\n",
                 kernel_type_as_string(pso_type),
                 device_kernel,
                 device_kernel_as_string((DeviceKernel)device_kernel),
                 duration);
    return;
  }

  if (!num_threads_per_block) {
    num_threads_per_block = round_down(pipeline.maxTotalThreadsPerThreadgroup,
                                       pipeline.threadExecutionWidth);
    num_threads_per_block = std::max(num_threads_per_block, (int)pipeline.threadExecutionWidth);
  }

  if (ShaderCache::running) {
    if (creating_new_archive || recreate_archive) {
      if (![archive serializeToURL:[NSURL fileURLWithPath:@(metalbin_path.c_str())] error:&error])
      {
        metal_printf("Failed to save binary archive to %s, error:\n%s\n",
                     metalbin_path.c_str(),
                     [[error localizedDescription] UTF8String]);
      }
      else {
        path_cache_kernel_mark_added_and_clear_old(metalbin_path);
      }
    }
  }

  this->loaded = true;
  [computePipelineStateDescriptor release];
  computePipelineStateDescriptor = nil;

  if (use_metalrt && linked_functions) {
    for (int table = 0; table < METALRT_TABLE_NUM; table++) {
      MTLIntersectionFunctionTableDescriptor *ift_desc =
          [[MTLIntersectionFunctionTableDescriptor alloc] init];
      ift_desc.functionCount = table_functions[table].count;
      intersection_func_table[table] = [this->pipeline
          newIntersectionFunctionTableWithDescriptor:ift_desc];

      /* Finally write the function handles into this pipeline's table */
      int size = (int)[table_functions[table] count];
      for (int i = 0; i < size; i++) {
        id<MTLFunctionHandle> handle = [pipeline
            functionHandleWithFunction:table_functions[table][i]];
        [intersection_func_table[table] setFunction:handle atIndex:i];
      }
    }
  }

  if (!use_binary_archive) {
    metal_printf("%16s | %2d | %-55s | %7.2fs\n",
                 kernel_type_as_string(pso_type),
                 int(device_kernel),
                 device_kernel_as_string(device_kernel),
                 duration);
  }
  else {
    metal_printf("%16s | %2d | %-55s | %7.2fs | %s: %s\n",
                 kernel_type_as_string(pso_type),
                 device_kernel,
                 device_kernel_as_string((DeviceKernel)device_kernel),
                 duration,
                 creating_new_archive ? " new" : "load",
                 metalbin_name.c_str());
  }
}

bool MetalDeviceKernels::load(MetalDevice *device, MetalPipelineType pso_type)
{
  auto shader_cache = get_shader_cache(device->mtlDevice);
  for (int i = 0; i < DEVICE_KERNEL_NUM; i++) {
    shader_cache->load_kernel((DeviceKernel)i, device, pso_type);
  }
  return true;
}

void MetalDeviceKernels::wait_for_all()
{
  for (int i = 0; i < g_shaderCacheCount; i++) {
    g_shaderCache[i].second->wait_for_all();
  }
}

int MetalDeviceKernels::num_incomplete_specialization_requests()
{
  /* Return true if any ShaderCaches have ongoing specialization requests (typically there will be
   * only 1). */
  int total = 0;
  for (int i = 0; i < g_shaderCacheCount; i++) {
    total += g_shaderCache[i].second->incomplete_specialization_requests;
  }
  return total;
}

int MetalDeviceKernels::get_loaded_kernel_count(MetalDevice const *device,
                                                MetalPipelineType pso_type)
{
  auto shader_cache = get_shader_cache(device->mtlDevice);
  int loaded_count = DEVICE_KERNEL_NUM;
  for (int i = 0; i < DEVICE_KERNEL_NUM; i++) {
    if (shader_cache->should_load_kernel((DeviceKernel)i, device, pso_type)) {
      loaded_count -= 1;
    }
  }
  return loaded_count;
}

bool MetalDeviceKernels::should_load_kernels(MetalDevice const *device, MetalPipelineType pso_type)
{
  return get_loaded_kernel_count(device, pso_type) != DEVICE_KERNEL_NUM;
}

const MetalKernelPipeline *MetalDeviceKernels::get_best_pipeline(const MetalDevice *device,
                                                                 DeviceKernel kernel)
{
  return get_shader_cache(device->mtlDevice)->get_best_pipeline(kernel, device);
}

bool MetalDeviceKernels::is_benchmark_warmup()
{
  NSArray *args = [[NSProcessInfo processInfo] arguments];
  for (int i = 0; i < args.count; i++) {
    if (const char *arg = [[args objectAtIndex:i] cStringUsingEncoding:NSASCIIStringEncoding]) {
      if (!strcmp(arg, "--warm-up")) {
        return true;
      }
    }
  }
  return false;
}

CCL_NAMESPACE_END

#endif /* WITH_METAL */
