/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

#ifdef WITH_METAL

#  include "device/metal/kernel.h"
#  include "device/metal/device_impl.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/tbb.h"
#  include "util/time.h"
#  include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

/* limit to 2 MTLCompiler instances */
int max_mtlcompiler_threads = 2;

const char *kernel_type_as_string(int kernel_type)
{
  switch (kernel_type) {
    case PSO_GENERIC:
      return "PSO_GENERIC";
    case PSO_SPECIALISED:
      return "PSO_SPECIALISED";
    default:
      assert(0);
  }
  return "";
}

bool kernel_has_intersection(DeviceKernel device_kernel)
{
  return (device_kernel == DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST ||
          device_kernel == DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW ||
          device_kernel == DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE ||
          device_kernel == DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK ||
          device_kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE);
}

struct ShaderCache {
  ShaderCache(id<MTLDevice> _mtlDevice) : mtlDevice(_mtlDevice)
  {
  }
  ~ShaderCache();

  /* Get the fastest available pipeline for the specified kernel. */
  MetalKernelPipeline *get_best_pipeline(DeviceKernel kernel, const MetalDevice *device);

  /* Non-blocking request for a kernel, optionally specialized to the scene being rendered by
   * device. */
  void load_kernel(DeviceKernel kernel, MetalDevice *device, bool scene_specialized);

  void wait_for_all();

 private:
  friend ShaderCache *get_shader_cache(id<MTLDevice> mtlDevice);

  void compile_thread_func(int thread_index);

  using PipelineCollection = std::vector<unique_ptr<MetalKernelPipeline>>;

  struct PipelineRequest {
    MetalKernelPipeline *pipeline = nullptr;
    std::function<void(MetalKernelPipeline *)> completionHandler;
  };

  std::mutex cache_mutex;

  PipelineCollection pipelines[DEVICE_KERNEL_NUM];
  id<MTLDevice> mtlDevice;

  bool running = false;
  std::condition_variable cond_var;
  std::deque<PipelineRequest> request_queue;
  std::vector<std::thread> compile_threads;
  std::atomic_int incomplete_requests = 0;
};

std::mutex g_shaderCacheMutex;
std::map<id<MTLDevice>, unique_ptr<ShaderCache>> g_shaderCache;

ShaderCache *get_shader_cache(id<MTLDevice> mtlDevice)
{
  thread_scoped_lock lock(g_shaderCacheMutex);
  auto it = g_shaderCache.find(mtlDevice);
  if (it != g_shaderCache.end()) {
    return it->second.get();
  }

  g_shaderCache[mtlDevice] = make_unique<ShaderCache>(mtlDevice);
  return g_shaderCache[mtlDevice].get();
}

ShaderCache::~ShaderCache()
{
  metal_printf("ShaderCache shutting down with incomplete_requests = %d\n",
               int(incomplete_requests));

  running = false;
  cond_var.notify_all();
  for (auto &thread : compile_threads) {
    thread.join();
  }
}

void ShaderCache::wait_for_all()
{
  while (incomplete_requests > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void ShaderCache::compile_thread_func(int thread_index)
{
  while (1) {

    /* wait for / acquire next request */
    PipelineRequest request;
    {
      thread_scoped_lock lock(cache_mutex);
      cond_var.wait(lock, [&] { return !running || !request_queue.empty(); });
      if (!running) {
        break;
      }

      if (!request_queue.empty()) {
        request = request_queue.front();
        request_queue.pop_front();
      }
    }

    /* service request */
    if (request.pipeline) {
      request.pipeline->compile();
      incomplete_requests--;
    }
  }
}

void ShaderCache::load_kernel(DeviceKernel device_kernel,
                              MetalDevice *device,
                              bool scene_specialized)
{
  {
    /* create compiler threads on first run */
    thread_scoped_lock lock(cache_mutex);
    if (compile_threads.empty()) {
      running = true;
      for (int i = 0; i < max_mtlcompiler_threads; i++) {
        compile_threads.push_back(std::thread([&] { compile_thread_func(i); }));
      }
    }
  }

  if (device_kernel == DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL) {
    /* skip megakernel */
    return;
  }

  if (scene_specialized) {
    /* Only specialize kernels where it can make an impact. */
    if (device_kernel < DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST ||
        device_kernel > DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL) {
      return;
    }
  }

  {
    /* check whether the kernel has already been requested / cached */
    thread_scoped_lock lock(cache_mutex);
    for (auto &pipeline : pipelines[device_kernel]) {
      if (scene_specialized) {
        if (pipeline->source_md5 == device->source_md5[PSO_SPECIALISED]) {
          /* we already requested a pipeline that is specialized for this kernel data */
          metal_printf("Specialized kernel already requested (%s)\n",
                       device_kernel_as_string(device_kernel));
          return;
        }
      }
      else {
        if (pipeline->source_md5 == device->source_md5[PSO_GENERIC]) {
          /* we already requested a generic pipeline for this kernel */
          metal_printf("Generic kernel already requested (%s)\n",
                       device_kernel_as_string(device_kernel));
          return;
        }
      }
    }
  }

  incomplete_requests++;

  PipelineRequest request;
  request.pipeline = new MetalKernelPipeline;
  request.pipeline->scene_specialized = scene_specialized;
  request.pipeline->mtlDevice = mtlDevice;
  request.pipeline->source_md5 =
      device->source_md5[scene_specialized ? PSO_SPECIALISED : PSO_GENERIC];
  request.pipeline->mtlLibrary =
      device->mtlLibrary[scene_specialized ? PSO_SPECIALISED : PSO_GENERIC];
  request.pipeline->device_kernel = device_kernel;
  request.pipeline->threads_per_threadgroup = device->max_threads_per_threadgroup;

  /* metalrt options */
  request.pipeline->use_metalrt = device->use_metalrt;
  request.pipeline->metalrt_hair = device->use_metalrt &&
                                   (device->kernel_features & KERNEL_FEATURE_HAIR);
  request.pipeline->metalrt_hair_thick = device->use_metalrt &&
                                         (device->kernel_features & KERNEL_FEATURE_HAIR_THICK);
  request.pipeline->metalrt_pointcloud = device->use_metalrt &&
                                         (device->kernel_features & KERNEL_FEATURE_POINTCLOUD);

  {
    thread_scoped_lock lock(cache_mutex);
    pipelines[device_kernel].push_back(unique_ptr<MetalKernelPipeline>(request.pipeline));
    request_queue.push_back(request);
  }
  cond_var.notify_one();
}

MetalKernelPipeline *ShaderCache::get_best_pipeline(DeviceKernel kernel, const MetalDevice *device)
{
  thread_scoped_lock lock(cache_mutex);
  auto &collection = pipelines[kernel];
  if (collection.empty()) {
    return nullptr;
  }

  /* metalrt options */
  bool use_metalrt = device->use_metalrt;
  bool metalrt_hair = use_metalrt && (device->kernel_features & KERNEL_FEATURE_HAIR);
  bool metalrt_hair_thick = use_metalrt && (device->kernel_features & KERNEL_FEATURE_HAIR_THICK);
  bool metalrt_pointcloud = use_metalrt && (device->kernel_features & KERNEL_FEATURE_POINTCLOUD);

  MetalKernelPipeline *best_pipeline = nullptr;
  for (auto &pipeline : collection) {
    if (!pipeline->loaded) {
      /* still loading - ignore */
      continue;
    }

    if (pipeline->use_metalrt != use_metalrt || pipeline->metalrt_hair != metalrt_hair ||
        pipeline->metalrt_hair_thick != metalrt_hair_thick ||
        pipeline->metalrt_pointcloud != metalrt_pointcloud) {
      /* wrong combination of metalrt options */
      continue;
    }

    if (pipeline->scene_specialized) {
      if (pipeline->source_md5 == device->source_md5[PSO_SPECIALISED]) {
        best_pipeline = pipeline.get();
      }
    }
    else if (!best_pipeline) {
      best_pipeline = pipeline.get();
    }
  }

  return best_pipeline;
}

void MetalKernelPipeline::compile()
{
  int pso_type = scene_specialized ? PSO_SPECIALISED : PSO_GENERIC;

  const std::string function_name = std::string("cycles_metal_") +
                                    device_kernel_as_string(device_kernel);

  int threads_per_threadgroup = this->threads_per_threadgroup;
  if (device_kernel > DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL &&
      device_kernel < DEVICE_KERNEL_INTEGRATOR_RESET) {
    /* Always use 512 for the sorting kernels */
    threads_per_threadgroup = 512;
  }

  NSString *entryPoint = [@(function_name.c_str()) copy];

  NSError *error = NULL;
  if (@available(macOS 11.0, *)) {
    MTLFunctionDescriptor *func_desc = [MTLIntersectionFunctionDescriptor functionDescriptor];
    func_desc.name = entryPoint;
    function = [mtlLibrary newFunctionWithDescriptor:func_desc error:&error];
  }

  [entryPoint release];

  if (function == nil) {
    NSString *err = [error localizedDescription];
    string errors = [err UTF8String];
    metal_printf("Error getting function \"%s\": %s", function_name.c_str(), errors.c_str());
    return;
  }

  function.label = [entryPoint copy];

  if (use_metalrt) {
    if (@available(macOS 11.0, *)) {
      /* create the id<MTLFunction> for each intersection function */
      const char *function_names[] = {
          "__anyhit__cycles_metalrt_visibility_test_tri",
          "__anyhit__cycles_metalrt_visibility_test_box",
          "__anyhit__cycles_metalrt_shadow_all_hit_tri",
          "__anyhit__cycles_metalrt_shadow_all_hit_box",
          "__anyhit__cycles_metalrt_local_hit_tri",
          "__anyhit__cycles_metalrt_local_hit_box",
          "__intersection__curve_ribbon",
          "__intersection__curve_ribbon_shadow",
          "__intersection__curve_all",
          "__intersection__curve_all_shadow",
          "__intersection__point",
          "__intersection__point_shadow",
      };
      assert(sizeof(function_names) / sizeof(function_names[0]) == METALRT_FUNC_NUM);

      MTLFunctionDescriptor *desc = [MTLIntersectionFunctionDescriptor functionDescriptor];
      for (int i = 0; i < METALRT_FUNC_NUM; i++) {
        const char *function_name = function_names[i];
        desc.name = [@(function_name) copy];

        NSError *error = NULL;
        rt_intersection_function[i] = [mtlLibrary newFunctionWithDescriptor:desc error:&error];

        if (rt_intersection_function[i] == nil) {
          NSString *err = [error localizedDescription];
          string errors = [err UTF8String];

          error_str = string_printf(
              "Error getting intersection function \"%s\": %s", function_name, errors.c_str());
          break;
        }

        rt_intersection_function[i].label = [@(function_name) copy];
      }
    }
  }

  NSArray *table_functions[METALRT_TABLE_NUM] = {nil};
  NSArray *linked_functions = nil;

  if (use_metalrt) {
    id<MTLFunction> curve_intersect_default = nil;
    id<MTLFunction> curve_intersect_shadow = nil;
    id<MTLFunction> point_intersect_default = nil;
    id<MTLFunction> point_intersect_shadow = nil;
    if (metalrt_hair) {
      /* Add curve intersection programs. */
      if (metalrt_hair_thick) {
        /* Slower programs for thick hair since that also slows down ribbons.
         * Ideally this should not be needed. */
        curve_intersect_default = rt_intersection_function[METALRT_FUNC_CURVE_ALL];
        curve_intersect_shadow = rt_intersection_function[METALRT_FUNC_CURVE_ALL_SHADOW];
      }
      else {
        curve_intersect_default = rt_intersection_function[METALRT_FUNC_CURVE_RIBBON];
        curve_intersect_shadow = rt_intersection_function[METALRT_FUNC_CURVE_RIBBON_SHADOW];
      }
    }
    if (metalrt_pointcloud) {
      point_intersect_default = rt_intersection_function[METALRT_FUNC_POINT];
      point_intersect_shadow = rt_intersection_function[METALRT_FUNC_POINT_SHADOW];
    }
    table_functions[METALRT_TABLE_DEFAULT] = [NSArray
        arrayWithObjects:rt_intersection_function[METALRT_FUNC_DEFAULT_TRI],
                         curve_intersect_default ?
                             curve_intersect_default :
                             rt_intersection_function[METALRT_FUNC_DEFAULT_BOX],
                         point_intersect_default ?
                             point_intersect_default :
                             rt_intersection_function[METALRT_FUNC_DEFAULT_BOX],
                         nil];
    table_functions[METALRT_TABLE_SHADOW] = [NSArray
        arrayWithObjects:rt_intersection_function[METALRT_FUNC_SHADOW_TRI],
                         curve_intersect_shadow ?
                             curve_intersect_shadow :
                             rt_intersection_function[METALRT_FUNC_SHADOW_BOX],
                         point_intersect_shadow ?
                             point_intersect_shadow :
                             rt_intersection_function[METALRT_FUNC_SHADOW_BOX],
                         nil];
    table_functions[METALRT_TABLE_LOCAL] = [NSArray
        arrayWithObjects:rt_intersection_function[METALRT_FUNC_LOCAL_TRI],
                         rt_intersection_function[METALRT_FUNC_LOCAL_BOX],
                         rt_intersection_function[METALRT_FUNC_LOCAL_BOX],
                         nil];

    NSMutableSet *unique_functions = [NSMutableSet
        setWithArray:table_functions[METALRT_TABLE_DEFAULT]];
    [unique_functions addObjectsFromArray:table_functions[METALRT_TABLE_SHADOW]];
    [unique_functions addObjectsFromArray:table_functions[METALRT_TABLE_LOCAL]];

    if (kernel_has_intersection(device_kernel)) {
      linked_functions = [[NSArray arrayWithArray:[unique_functions allObjects]]
          sortedArrayUsingComparator:^NSComparisonResult(id<MTLFunction> f1, id<MTLFunction> f2) {
            return [f1.label compare:f2.label];
          }];
    }
    unique_functions = nil;
  }

  MTLComputePipelineDescriptor *computePipelineStateDescriptor =
      [[MTLComputePipelineDescriptor alloc] init];

  computePipelineStateDescriptor.buffers[0].mutability = MTLMutabilityImmutable;
  computePipelineStateDescriptor.buffers[1].mutability = MTLMutabilityImmutable;
  computePipelineStateDescriptor.buffers[2].mutability = MTLMutabilityImmutable;

  if (@available(macos 10.14, *)) {
    computePipelineStateDescriptor.maxTotalThreadsPerThreadgroup = threads_per_threadgroup;
  }
  computePipelineStateDescriptor.threadGroupSizeIsMultipleOfThreadExecutionWidth = true;

  computePipelineStateDescriptor.computeFunction = function;

  if (@available(macOS 11.0, *)) {
    /* Attach the additional functions to an MTLLinkedFunctions object */
    if (linked_functions) {
      computePipelineStateDescriptor.linkedFunctions = [[MTLLinkedFunctions alloc] init];
      computePipelineStateDescriptor.linkedFunctions.functions = linked_functions;
    }
    computePipelineStateDescriptor.maxCallStackDepth = 1;
    if (use_metalrt) {
      computePipelineStateDescriptor.maxCallStackDepth = 8;
    }
  }

  MTLPipelineOption pipelineOptions = MTLPipelineOptionNone;

  bool use_binary_archive = true;
  if (auto str = getenv("CYCLES_METAL_DISABLE_BINARY_ARCHIVES")) {
    use_binary_archive = (atoi(str) == 0);
  }

  id<MTLBinaryArchive> archive = nil;
  string metalbin_path;
  string metalbin_name;
  if (use_binary_archive) {
    NSProcessInfo *processInfo = [NSProcessInfo processInfo];
    string osVersion = [[processInfo operatingSystemVersionString] UTF8String];
    MD5Hash local_md5;
    local_md5.append(source_md5);
    local_md5.append(osVersion);
    local_md5.append((uint8_t *)&this->threads_per_threadgroup,
                     sizeof(this->threads_per_threadgroup));

    string options;
    if (use_metalrt && kernel_has_intersection(device_kernel)) {
      /* incorporate any MetalRT specializations into the archive name */
      options += string_printf(".hair_%d.hair_thick_%d.pointcloud_%d",
                               metalrt_hair ? 1 : 0,
                               metalrt_hair_thick ? 1 : 0,
                               metalrt_pointcloud ? 1 : 0);
    }

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
    metalbin_name = path_join(metalbin_name, local_md5.get_hex() + options + ".bin");

    metalbin_path = path_cache_get(path_join("kernels", metalbin_name));
    path_create_directories(metalbin_path);

    if (path_exists(metalbin_path) && use_binary_archive) {
      if (@available(macOS 11.0, *)) {
        MTLBinaryArchiveDescriptor *archiveDesc = [[MTLBinaryArchiveDescriptor alloc] init];
        archiveDesc.url = [NSURL fileURLWithPath:@(metalbin_path.c_str())];
        archive = [mtlDevice newBinaryArchiveWithDescriptor:archiveDesc error:nil];
        [archiveDesc release];
      }
    }
  }

  __block bool creating_new_archive = false;
  if (@available(macOS 11.0, *)) {
    if (use_binary_archive) {
      if (!archive) {
        MTLBinaryArchiveDescriptor *archiveDesc = [[MTLBinaryArchiveDescriptor alloc] init];
        archiveDesc.url = nil;
        archive = [mtlDevice newBinaryArchiveWithDescriptor:archiveDesc error:nil];
        creating_new_archive = true;
      }
      computePipelineStateDescriptor.binaryArchives = [NSArray arrayWithObjects:archive, nil];
      pipelineOptions = MTLPipelineOptionFailOnBinaryArchiveMiss;
    }
  }

  double starttime = time_dt();

  MTLNewComputePipelineStateWithReflectionCompletionHandler completionHandler = ^(
      id<MTLComputePipelineState> computePipelineState,
      MTLComputePipelineReflection *reflection,
      NSError *error) {
    bool recreate_archive = false;
    if (computePipelineState == nil && archive) {
      NSString *errStr = [error localizedDescription];
      metal_printf(
          "Failed to create compute pipeline state \"%s\" from archive - attempting recreation... "
          "(error: %s)\n",
          device_kernel_as_string((DeviceKernel)device_kernel),
          errStr ? [errStr UTF8String] : "nil");
      computePipelineState = [mtlDevice
          newComputePipelineStateWithDescriptor:computePipelineStateDescriptor
                                        options:MTLPipelineOptionNone
                                     reflection:nullptr
                                          error:&error];
      recreate_archive = true;
    }

    double duration = time_dt() - starttime;

    if (computePipelineState == nil) {
      NSString *errStr = [error localizedDescription];
      error_str = string_printf("Failed to create compute pipeline state \"%s\", error: \n",
                                device_kernel_as_string((DeviceKernel)device_kernel));
      error_str += (errStr ? [errStr UTF8String] : "nil");
      metal_printf("%16s | %2d | %-55s | %7.2fs | FAILED!\n",
                   kernel_type_as_string(pso_type),
                   device_kernel,
                   device_kernel_as_string((DeviceKernel)device_kernel),
                   duration);
      return;
    }

    int num_threads_per_block = round_down(computePipelineState.maxTotalThreadsPerThreadgroup,
                                           computePipelineState.threadExecutionWidth);
    num_threads_per_block = std::max(num_threads_per_block,
                                     (int)computePipelineState.threadExecutionWidth);
    this->pipeline = computePipelineState;
    this->num_threads_per_block = num_threads_per_block;

    if (@available(macOS 11.0, *)) {
      if (creating_new_archive || recreate_archive) {
        if (![archive serializeToURL:[NSURL fileURLWithPath:@(metalbin_path.c_str())]
                               error:&error]) {
          metal_printf("Failed to save binary archive, error:\n%s\n",
                       [[error localizedDescription] UTF8String]);
        }
      }
    }
  };

  /* Block on load to ensure we continue with a valid kernel function */
  if (creating_new_archive) {
    starttime = time_dt();
    NSError *error;
    if (![archive addComputePipelineFunctionsWithDescriptor:computePipelineStateDescriptor
                                                      error:&error]) {
      NSString *errStr = [error localizedDescription];
      metal_printf("Failed to add PSO to archive:\n%s\n", errStr ? [errStr UTF8String] : "nil");
    }
  }
  id<MTLComputePipelineState> pipeline = [mtlDevice
      newComputePipelineStateWithDescriptor:computePipelineStateDescriptor
                                    options:pipelineOptions
                                 reflection:nullptr
                                      error:&error];
  completionHandler(pipeline, nullptr, error);

  this->loaded = true;
  [computePipelineStateDescriptor release];
  computePipelineStateDescriptor = nil;

  if (use_metalrt && linked_functions) {
    for (int table = 0; table < METALRT_TABLE_NUM; table++) {
      if (@available(macOS 11.0, *)) {
        MTLIntersectionFunctionTableDescriptor *ift_desc =
            [[MTLIntersectionFunctionTableDescriptor alloc] init];
        ift_desc.functionCount = table_functions[table].count;
        intersection_func_table[table] = [this->pipeline
            newIntersectionFunctionTableWithDescriptor:ift_desc];

        /* Finally write the function handles into this pipeline's table */
        for (int i = 0; i < 2; i++) {
          id<MTLFunctionHandle> handle = [pipeline
              functionHandleWithFunction:table_functions[table][i]];
          [intersection_func_table[table] setFunction:handle atIndex:i];
        }
      }
    }
  }

  double duration = time_dt() - starttime;

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

bool MetalDeviceKernels::load(MetalDevice *device, bool scene_specialized)
{
  auto shader_cache = get_shader_cache(device->mtlDevice);
  for (int i = 0; i < DEVICE_KERNEL_NUM; i++) {
    shader_cache->load_kernel((DeviceKernel)i, device, scene_specialized);
  }

  if (!scene_specialized || getenv("CYCLES_METAL_PROFILING")) {
    shader_cache->wait_for_all();
  }
  return true;
}

const MetalKernelPipeline *MetalDeviceKernels::get_best_pipeline(const MetalDevice *device,
                                                                 DeviceKernel kernel)
{
  return get_shader_cache(device->mtlDevice)->get_best_pipeline(kernel, device);
}

CCL_NAMESPACE_END

#endif /* WITH_METAL*/
