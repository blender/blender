/*
 * Copyright 2019, NVIDIA Corporation.
 * Copyright 2019, Blender Foundation.
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

#ifdef WITH_OPTIX

#  include "device/device.h"
#  include "device/device_intern.h"
#  include "device/device_denoising.h"
#  include "bvh/bvh.h"
#  include "render/scene.h"
#  include "render/mesh.h"
#  include "render/object.h"
#  include "render/buffers.h"
#  include "util/util_md5.h"
#  include "util/util_path.h"
#  include "util/util_time.h"
#  include "util/util_debug.h"
#  include "util/util_logging.h"

#  undef _WIN32_WINNT  // Need minimum API support for Windows 7
#  define _WIN32_WINNT _WIN32_WINNT_WIN7

#  ifdef WITH_CUDA_DYNLOAD
#    include <cuew.h>
// Do not use CUDA SDK headers when using CUEW
#    define OPTIX_DONT_INCLUDE_CUDA
#  endif
#  include <optix_stubs.h>
#  include <optix_function_table_definition.h>

CCL_NAMESPACE_BEGIN

/* Make sure this stays in sync with kernel_globals.h */
struct ShaderParams {
  uint4 *input;
  float4 *output;
  int type;
  int filter;
  int sx;
  int offset;
  int sample;
};
struct KernelParams {
  WorkTile tile;
  KernelData data;
  ShaderParams shader;
#  define KERNEL_TEX(type, name) const type *name;
#  include "kernel/kernel_textures.h"
#  undef KERNEL_TEX
};

#  define check_result_cuda(stmt) \
    { \
      CUresult res = stmt; \
      if (res != CUDA_SUCCESS) { \
        const char *name; \
        cuGetErrorName(res, &name); \
        set_error(string_printf("OptiX CUDA error %s in %s, line %d", name, #stmt, __LINE__)); \
        return; \
      } \
    } \
    (void)0
#  define check_result_cuda_ret(stmt) \
    { \
      CUresult res = stmt; \
      if (res != CUDA_SUCCESS) { \
        const char *name; \
        cuGetErrorName(res, &name); \
        set_error(string_printf("OptiX CUDA error %s in %s, line %d", name, #stmt, __LINE__)); \
        return false; \
      } \
    } \
    (void)0

#  define check_result_optix(stmt) \
    { \
      enum OptixResult res = stmt; \
      if (res != OPTIX_SUCCESS) { \
        const char *name = optixGetErrorName(res); \
        set_error(string_printf("OptiX error %s in %s, line %d", name, #stmt, __LINE__)); \
        return; \
      } \
    } \
    (void)0
#  define check_result_optix_ret(stmt) \
    { \
      enum OptixResult res = stmt; \
      if (res != OPTIX_SUCCESS) { \
        const char *name = optixGetErrorName(res); \
        set_error(string_printf("OptiX error %s in %s, line %d", name, #stmt, __LINE__)); \
        return false; \
      } \
    } \
    (void)0

class OptiXDevice : public Device {

  // List of OptiX program groups
  enum {
    PG_RGEN,
    PG_MISS,
    PG_HITD,  // Default hit group
    PG_HITL,  // __BVH_LOCAL__ hit group
    PG_HITS,  // __SHADOW_RECORD_ALL__ hit group
#  ifdef WITH_CYCLES_DEBUG
    PG_EXCP,
#  endif
    PG_BAKE,  // kernel_bake_evaluate
    PG_DISP,  // kernel_displace_evaluate
    PG_BACK,  // kernel_background_evaluate
    NUM_PROGRAM_GROUPS
  };

  // List of OptiX pipelines
  enum { PIP_PATH_TRACE, PIP_SHADER_EVAL, NUM_PIPELINES };

  // A single shader binding table entry
  struct SbtRecord {
    char header[OPTIX_SBT_RECORD_HEADER_SIZE];
  };

  // Information stored about CUDA memory allocations
  struct CUDAMem {
    bool free_map_host = false;
    CUarray array = NULL;
    CUtexObject texobject = 0;
    void *map_host_pointer = nullptr;
  };

  // Helper class to manage current CUDA context
  struct CUDAContextScope {
    CUDAContextScope(CUcontext ctx)
    {
      cuCtxPushCurrent(ctx);
    }
    ~CUDAContextScope()
    {
      cuCtxPopCurrent(NULL);
    }
  };

  // Use a pool with multiple threads to support launches with multiple CUDA streams
  TaskPool task_pool;

  // CUDA/OptiX context handles
  CUdevice cuda_device = 0;
  CUcontext cuda_context = NULL;
  vector<CUstream> cuda_stream;
  OptixDeviceContext context = NULL;

  // Need CUDA kernel module for some utility functions
  CUmodule cuda_module = NULL;
  CUmodule cuda_filter_module = NULL;
  // All necessary OptiX kernels are in one module
  OptixModule optix_module = NULL;
  OptixPipeline pipelines[NUM_PIPELINES] = {};

  bool motion_blur = false;
  bool need_texture_info = false;
  device_vector<SbtRecord> sbt_data;
  device_vector<TextureInfo> texture_info;
  device_only_memory<KernelParams> launch_params;
  vector<device_only_memory<uint8_t>> blas;
  OptixTraversableHandle tlas_handle = 0;

  // TODO(pmours): This is copied from device_cuda.cpp, so move to common code eventually
  int can_map_host = 0;
  size_t map_host_used = 0;
  size_t map_host_limit = 0;
  size_t device_working_headroom = 32 * 1024 * 1024LL;   // 32MB
  size_t device_texture_headroom = 128 * 1024 * 1024LL;  // 128MB
  map<device_memory *, CUDAMem> cuda_mem_map;
  bool move_texture_to_host = false;

 public:
  OptiXDevice(DeviceInfo &info_, Stats &stats_, Profiler &profiler_, bool background_)
      : Device(info_, stats_, profiler_, background_),
        sbt_data(this, "__sbt", MEM_READ_ONLY),
        texture_info(this, "__texture_info", MEM_TEXTURE),
        launch_params(this, "__params")
  {
    // Store number of CUDA streams in device info
    info.cpu_threads = DebugFlags().optix.cuda_streams;

    // Initialize CUDA driver API
    check_result_cuda(cuInit(0));

    // Retrieve the primary CUDA context for this device
    check_result_cuda(cuDeviceGet(&cuda_device, info.num));
    check_result_cuda(cuDevicePrimaryCtxRetain(&cuda_context, cuda_device));

    // Make that CUDA context current
    const CUDAContextScope scope(cuda_context);

    // Limit amount of host mapped memory (see init_host_memory in device_cuda.cpp)
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
    }

    // Check device support for pinned host memory
    check_result_cuda(
        cuDeviceGetAttribute(&can_map_host, CU_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY, cuda_device));

    // Create OptiX context for this device
    OptixDeviceContextOptions options = {};
#  ifdef WITH_CYCLES_LOGGING
    options.logCallbackLevel = 4;  // Fatal = 1, Error = 2, Warning = 3, Print = 4
    options.logCallbackFunction =
        [](unsigned int level, const char *, const char *message, void *) {
          switch (level) {
            case 1:
              LOG_IF(FATAL, VLOG_IS_ON(1)) << message;
              break;
            case 2:
              LOG_IF(ERROR, VLOG_IS_ON(1)) << message;
              break;
            case 3:
              LOG_IF(WARNING, VLOG_IS_ON(1)) << message;
              break;
            case 4:
              LOG_IF(INFO, VLOG_IS_ON(1)) << message;
              break;
          }
        };
#  endif
    check_result_optix(optixDeviceContextCreate(cuda_context, &options, &context));
#  ifdef WITH_CYCLES_LOGGING
    check_result_optix(optixDeviceContextSetLogCallback(
        context, options.logCallbackFunction, options.logCallbackData, options.logCallbackLevel));
#  endif

    // Create launch streams
    cuda_stream.resize(info.cpu_threads);
    for (int i = 0; i < info.cpu_threads; ++i)
      check_result_cuda(cuStreamCreate(&cuda_stream[i], CU_STREAM_NON_BLOCKING));

    // Fix weird compiler bug that assigns wrong size
    launch_params.data_elements = sizeof(KernelParams);
    // Allocate launch parameter buffer memory on device
    launch_params.alloc_to_device(info.cpu_threads);
  }
  ~OptiXDevice()
  {
    // Stop processing any more tasks
    task_pool.stop();

    // Clean up all memory before destroying context
    blas.clear();

    sbt_data.free();
    texture_info.free();
    launch_params.free();

    // Make CUDA context current
    const CUDAContextScope scope(cuda_context);

    // Unload modules
    if (cuda_module != NULL)
      cuModuleUnload(cuda_module);
    if (cuda_filter_module != NULL)
      cuModuleUnload(cuda_filter_module);
    if (optix_module != NULL)
      optixModuleDestroy(optix_module);
    for (unsigned int i = 0; i < NUM_PIPELINES; ++i)
      if (pipelines[i] != NULL)
        optixPipelineDestroy(pipelines[i]);

    // Destroy launch streams
    for (int i = 0; i < info.cpu_threads; ++i)
      cuStreamDestroy(cuda_stream[i]);

    // Destroy OptiX and CUDA context
    optixDeviceContextDestroy(context);
    cuDevicePrimaryCtxRelease(cuda_device);
  }

 private:
  bool show_samples() const override
  {
    // Only show samples if not rendering multiple tiles in parallel
    return info.cpu_threads == 1;
  }

  BVHLayoutMask get_bvh_layout_mask() const override
  {
    // OptiX has its own internal acceleration structure format
    return BVH_LAYOUT_OPTIX;
  }

  bool load_kernels(const DeviceRequestedFeatures &requested_features) override
  {
    if (have_error())
      return false;  // Abort early if context creation failed already

    // Disable baking for now, since its kernel is not well-suited for inlining and is very slow
    if (requested_features.use_baking) {
      set_error("OptiX implementation does not support baking yet");
      return false;
    }
    // Disable shader raytracing support for now, since continuation callables are slow
    if (requested_features.use_shader_raytrace) {
      set_error("OptiX implementation does not support shader raytracing yet");
      return false;
    }

    const CUDAContextScope scope(cuda_context);

    // Unload any existing modules first
    if (cuda_module != NULL)
      cuModuleUnload(cuda_module);
    if (cuda_filter_module != NULL)
      cuModuleUnload(cuda_filter_module);
    if (optix_module != NULL)
      optixModuleDestroy(optix_module);
    for (unsigned int i = 0; i < NUM_PIPELINES; ++i)
      if (pipelines[i] != NULL)
        optixPipelineDestroy(pipelines[i]);

    OptixModuleCompileOptions module_options;
    module_options.maxRegisterCount = 0;  // Do not set an explicit register limit
#  ifdef WITH_CYCLES_DEBUG
    module_options.optLevel = OPTIX_COMPILE_OPTIMIZATION_LEVEL_0;
    module_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
#  else
    module_options.optLevel = OPTIX_COMPILE_OPTIMIZATION_LEVEL_3;
    module_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_LINEINFO;
#  endif
    OptixPipelineCompileOptions pipeline_options;
    // Default to no motion blur and two-level graph, since it is the fastest option
    pipeline_options.usesMotionBlur = false;
    pipeline_options.traversableGraphFlags =
        OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;
    pipeline_options.numPayloadValues = 6;
    pipeline_options.numAttributeValues = 2;  // u, v
#  ifdef WITH_CYCLES_DEBUG
    pipeline_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_STACK_OVERFLOW |
                                      OPTIX_EXCEPTION_FLAG_TRACE_DEPTH;
#  else
    pipeline_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;
#  endif
    pipeline_options.pipelineLaunchParamsVariableName = "__params";  // See kernel_globals.h

    // Keep track of whether motion blur is enabled, so to enable/disable motion in BVH builds
    // This is necessary since objects may be reported to have motion if the Vector pass is
    // active, but may still need to be rendered without motion blur if that isn't active as well
    motion_blur = requested_features.use_object_motion;

    if (motion_blur) {
      pipeline_options.usesMotionBlur = true;
      // Motion blur can insert motion transforms into the traversal graph
      // It is no longer a two-level graph then, so need to set flags to allow any configuration
      pipeline_options.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_ANY;
    }

    {  // Load and compile PTX module with OptiX kernels
      string ptx_data;
      const string ptx_filename = "lib/kernel_optix.ptx";
      if (!path_read_text(path_get(ptx_filename), ptx_data)) {
        set_error("Failed loading OptiX kernel " + ptx_filename + ".");
        return false;
      }

      check_result_optix_ret(optixModuleCreateFromPTX(context,
                                                      &module_options,
                                                      &pipeline_options,
                                                      ptx_data.data(),
                                                      ptx_data.size(),
                                                      nullptr,
                                                      0,
                                                      &optix_module));
    }

    {  // Load CUDA modules because we need some of the utility kernels
      int major, minor;
      cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, info.num);
      cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, info.num);

      string cubin_data;
      const string cubin_filename = string_printf("lib/kernel_sm_%d%d.cubin", major, minor);
      if (!path_read_text(path_get(cubin_filename), cubin_data)) {
        set_error("Failed loading pre-compiled CUDA kernel " + cubin_filename + ".");
        return false;
      }

      check_result_cuda_ret(cuModuleLoadData(&cuda_module, cubin_data.data()));

      if (requested_features.use_denoising) {
        string filter_data;
        const string filter_filename = string_printf("lib/filter_sm_%d%d.cubin", major, minor);
        if (!path_read_text(path_get(filter_filename), filter_data)) {
          set_error("Failed loading pre-compiled CUDA filter kernel " + filter_filename + ".");
          return false;
        }

        check_result_cuda_ret(cuModuleLoadData(&cuda_filter_module, filter_data.data()));
      }
    }

    // Create program groups
    OptixProgramGroup groups[NUM_PROGRAM_GROUPS] = {};
    OptixProgramGroupDesc group_descs[NUM_PROGRAM_GROUPS] = {};
    OptixProgramGroupOptions group_options = {};  // There are no options currently
    group_descs[PG_RGEN].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    group_descs[PG_RGEN].raygen.module = optix_module;
    // Ignore branched integrator for now (see "requested_features.use_integrator_branched")
    group_descs[PG_RGEN].raygen.entryFunctionName = "__raygen__kernel_optix_path_trace";
    group_descs[PG_MISS].kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
    group_descs[PG_MISS].miss.module = optix_module;
    group_descs[PG_MISS].miss.entryFunctionName = "__miss__kernel_optix_miss";
    group_descs[PG_HITD].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    group_descs[PG_HITD].hitgroup.moduleCH = optix_module;
    group_descs[PG_HITD].hitgroup.entryFunctionNameCH = "__closesthit__kernel_optix_hit";
    group_descs[PG_HITD].hitgroup.moduleAH = optix_module;
    group_descs[PG_HITD].hitgroup.entryFunctionNameAH = "__anyhit__kernel_optix_visibility_test";
    group_descs[PG_HITS].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    group_descs[PG_HITS].hitgroup.moduleAH = optix_module;
    group_descs[PG_HITS].hitgroup.entryFunctionNameAH = "__anyhit__kernel_optix_shadow_all_hit";

    if (requested_features.use_hair) {
      // Add curve intersection programs
      group_descs[PG_HITD].hitgroup.moduleIS = optix_module;
      group_descs[PG_HITD].hitgroup.entryFunctionNameIS = "__intersection__curve";
      group_descs[PG_HITS].hitgroup.moduleIS = optix_module;
      group_descs[PG_HITS].hitgroup.entryFunctionNameIS = "__intersection__curve";
    }

    if (requested_features.use_subsurface || requested_features.use_shader_raytrace) {
      // Add hit group for local intersections
      group_descs[PG_HITL].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
      group_descs[PG_HITL].hitgroup.moduleAH = optix_module;
      group_descs[PG_HITL].hitgroup.entryFunctionNameAH = "__anyhit__kernel_optix_local_hit";
    }

#  ifdef WITH_CYCLES_DEBUG
    group_descs[PG_EXCP].kind = OPTIX_PROGRAM_GROUP_KIND_EXCEPTION;
    group_descs[PG_EXCP].exception.module = optix_module;
    group_descs[PG_EXCP].exception.entryFunctionName = "__exception__kernel_optix_exception";
#  endif

    if (requested_features.use_baking) {
      group_descs[PG_BAKE].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
      group_descs[PG_BAKE].raygen.module = optix_module;
      group_descs[PG_BAKE].raygen.entryFunctionName = "__raygen__kernel_optix_bake";
    }

    if (requested_features.use_true_displacement) {
      group_descs[PG_DISP].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
      group_descs[PG_DISP].raygen.module = optix_module;
      group_descs[PG_DISP].raygen.entryFunctionName = "__raygen__kernel_optix_displace";
    }

    if (requested_features.use_background_light) {
      group_descs[PG_BACK].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
      group_descs[PG_BACK].raygen.module = optix_module;
      group_descs[PG_BACK].raygen.entryFunctionName = "__raygen__kernel_optix_background";
    }

    check_result_optix_ret(optixProgramGroupCreate(
        context, group_descs, NUM_PROGRAM_GROUPS, &group_options, nullptr, 0, groups));

    // Get program stack sizes
    OptixStackSizes stack_size[NUM_PROGRAM_GROUPS] = {};
    // Set up SBT, which in this case is used only to select between different programs
    sbt_data.alloc(NUM_PROGRAM_GROUPS);
    memset(sbt_data.host_pointer, 0, sizeof(SbtRecord) * NUM_PROGRAM_GROUPS);
    for (unsigned int i = 0; i < NUM_PROGRAM_GROUPS; ++i) {
      check_result_optix_ret(optixSbtRecordPackHeader(groups[i], &sbt_data[i]));
      check_result_optix_ret(optixProgramGroupGetStackSize(groups[i], &stack_size[i]));
    }
    sbt_data.copy_to_device();  // Upload SBT to device

    // Calculate maximum trace continuation stack size
    unsigned int trace_css = stack_size[PG_HITD].cssCH;
    // This is based on the maximum of closest-hit and any-hit/intersection programs
    trace_css = max(trace_css, stack_size[PG_HITD].cssIS + stack_size[PG_HITD].cssAH);
    trace_css = max(trace_css, stack_size[PG_HITL].cssIS + stack_size[PG_HITL].cssAH);
    trace_css = max(trace_css, stack_size[PG_HITS].cssIS + stack_size[PG_HITS].cssAH);

    OptixPipelineLinkOptions link_options;
    link_options.maxTraceDepth = 1;
#  ifdef WITH_CYCLES_DEBUG
    link_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
#  else
    link_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_LINEINFO;
#  endif
    link_options.overrideUsesMotionBlur = pipeline_options.usesMotionBlur;

    {  // Create path tracing pipeline
      OptixProgramGroup pipeline_groups[] = {
          groups[PG_RGEN],
          groups[PG_MISS],
          groups[PG_HITD],
          groups[PG_HITS],
          groups[PG_HITL],
#  ifdef WITH_CYCLES_DEBUG
          groups[PG_EXCP],
#  endif
      };
      check_result_optix_ret(
          optixPipelineCreate(context,
                              &pipeline_options,
                              &link_options,
                              pipeline_groups,
                              (sizeof(pipeline_groups) / sizeof(pipeline_groups[0])),
                              nullptr,
                              0,
                              &pipelines[PIP_PATH_TRACE]));

      // Combine ray generation and trace continuation stack size
      const unsigned int css = stack_size[PG_RGEN].cssRG + link_options.maxTraceDepth * trace_css;

      // Set stack size depending on pipeline options
      check_result_optix_ret(optixPipelineSetStackSize(
          pipelines[PIP_PATH_TRACE], 0, 0, css, (pipeline_options.usesMotionBlur ? 3 : 2)));
    }

    // Only need to create shader evaluation pipeline if one of these features is used:
    const bool use_shader_eval_pipeline = requested_features.use_baking ||
                                          requested_features.use_background_light ||
                                          requested_features.use_true_displacement;

    if (use_shader_eval_pipeline) {  // Create shader evaluation pipeline
      OptixProgramGroup pipeline_groups[] = {
          groups[PG_BAKE],
          groups[PG_DISP],
          groups[PG_BACK],
          groups[PG_MISS],
          groups[PG_HITD],
          groups[PG_HITS],
          groups[PG_HITL],
#  ifdef WITH_CYCLES_DEBUG
          groups[PG_EXCP],
#  endif
      };
      check_result_optix_ret(
          optixPipelineCreate(context,
                              &pipeline_options,
                              &link_options,
                              pipeline_groups,
                              (sizeof(pipeline_groups) / sizeof(pipeline_groups[0])),
                              nullptr,
                              0,
                              &pipelines[PIP_SHADER_EVAL]));

      // Calculate continuation stack size based on the maximum of all ray generation stack sizes
      const unsigned int css = max(stack_size[PG_BAKE].cssRG,
                                   max(stack_size[PG_DISP].cssRG, stack_size[PG_BACK].cssRG)) +
                               link_options.maxTraceDepth * trace_css;

      check_result_optix_ret(optixPipelineSetStackSize(
          pipelines[PIP_SHADER_EVAL], 0, 0, css, (pipeline_options.usesMotionBlur ? 3 : 2)));
    }

    // Clean up program group objects
    for (unsigned int i = 0; i < NUM_PROGRAM_GROUPS; ++i) {
      optixProgramGroupDestroy(groups[i]);
    }

    return true;
  }

  void thread_run(DeviceTask &task, int thread_index)  // Main task entry point
  {
    if (have_error())
      return;  // Abort early if there was an error previously

    if (task.type == DeviceTask::RENDER) {
      RenderTile tile;
      while (task.acquire_tile(this, tile)) {
        if (tile.task == RenderTile::PATH_TRACE)
          launch_render(task, tile, thread_index);
        else if (tile.task == RenderTile::DENOISE)
          launch_denoise(task, tile, thread_index);
        task.release_tile(tile);
        if (task.get_cancel() && !task.need_finish_queue)
          break;  // User requested cancellation
        else if (have_error())
          break;  // Abort rendering when encountering an error
      }
    }
    else if (task.type == DeviceTask::SHADER) {
      launch_shader_eval(task, thread_index);
    }
    else if (task.type == DeviceTask::FILM_CONVERT) {
      launch_film_convert(task, thread_index);
    }
  }

  void launch_render(DeviceTask &task, RenderTile &rtile, int thread_index)
  {
    assert(thread_index < launch_params.data_size);

    // Keep track of total render time of this tile
    const scoped_timer timer(&rtile.buffers->render_time);

    WorkTile wtile;
    wtile.x = rtile.x;
    wtile.y = rtile.y;
    wtile.w = rtile.w;
    wtile.h = rtile.h;
    wtile.offset = rtile.offset;
    wtile.stride = rtile.stride;
    wtile.buffer = (float *)rtile.buffer;

    const int end_sample = rtile.start_sample + rtile.num_samples;
    // Keep this number reasonable to avoid running into TDRs
    const int step_samples = (info.display_device ? 8 : 32);
    // Offset into launch params buffer so that streams use separate data
    device_ptr launch_params_ptr = launch_params.device_pointer +
                                   thread_index * launch_params.data_elements;

    const CUDAContextScope scope(cuda_context);

    for (int sample = rtile.start_sample; sample < end_sample; sample += step_samples) {
      // Copy work tile information to device
      wtile.num_samples = min(step_samples, end_sample - sample);
      wtile.start_sample = sample;
      check_result_cuda(cuMemcpyHtoDAsync(launch_params_ptr + offsetof(KernelParams, tile),
                                          &wtile,
                                          sizeof(wtile),
                                          cuda_stream[thread_index]));

      OptixShaderBindingTable sbt_params = {};
      sbt_params.raygenRecord = sbt_data.device_pointer + PG_RGEN * sizeof(SbtRecord);
#  ifdef WITH_CYCLES_DEBUG
      sbt_params.exceptionRecord = sbt_data.device_pointer + PG_EXCP * sizeof(SbtRecord);
#  endif
      sbt_params.missRecordBase = sbt_data.device_pointer + PG_MISS * sizeof(SbtRecord);
      sbt_params.missRecordStrideInBytes = sizeof(SbtRecord);
      sbt_params.missRecordCount = 1;
      sbt_params.hitgroupRecordBase = sbt_data.device_pointer + PG_HITD * sizeof(SbtRecord);
      sbt_params.hitgroupRecordStrideInBytes = sizeof(SbtRecord);
      sbt_params.hitgroupRecordCount = 3;  // PG_HITD, PG_HITL, PG_HITS

      // Launch the ray generation program
      check_result_optix(optixLaunch(pipelines[PIP_PATH_TRACE],
                                     cuda_stream[thread_index],
                                     launch_params_ptr,
                                     launch_params.data_elements,
                                     &sbt_params,
                                     // Launch with samples close to each other for better locality
                                     wtile.w * wtile.num_samples,
                                     wtile.h,
                                     1));

      // Wait for launch to finish
      check_result_cuda(cuStreamSynchronize(cuda_stream[thread_index]));

      // Update current sample, so it is displayed correctly
      rtile.sample = wtile.start_sample + wtile.num_samples;
      // Update task progress after the kernel completed rendering
      task.update_progress(&rtile, wtile.w * wtile.h * wtile.num_samples);

      if (task.get_cancel() && !task.need_finish_queue)
        return;  // Cancel rendering
    }
  }

  void launch_denoise(DeviceTask &task, RenderTile &rtile, int thread_index)
  {
    const CUDAContextScope scope(cuda_context);

    // Run CUDA denoising kernels
    DenoisingTask denoising(this, task);
    denoising.functions.construct_transform = function_bind(
        &OptiXDevice::denoising_construct_transform, this, &denoising, thread_index);
    denoising.functions.accumulate = function_bind(
        &OptiXDevice::denoising_accumulate, this, _1, _2, _3, _4, &denoising, thread_index);
    denoising.functions.solve = function_bind(
        &OptiXDevice::denoising_solve, this, _1, &denoising, thread_index);
    denoising.functions.divide_shadow = function_bind(
        &OptiXDevice::denoising_divide_shadow, this, _1, _2, _3, _4, _5, &denoising, thread_index);
    denoising.functions.non_local_means = function_bind(
        &OptiXDevice::denoising_non_local_means, this, _1, _2, _3, _4, &denoising, thread_index);
    denoising.functions.combine_halves = function_bind(&OptiXDevice::denoising_combine_halves,
                                                       this,
                                                       _1,
                                                       _2,
                                                       _3,
                                                       _4,
                                                       _5,
                                                       _6,
                                                       &denoising,
                                                       thread_index);
    denoising.functions.get_feature = function_bind(
        &OptiXDevice::denoising_get_feature, this, _1, _2, _3, _4, _5, &denoising, thread_index);
    denoising.functions.write_feature = function_bind(
        &OptiXDevice::denoising_write_feature, this, _1, _2, _3, &denoising, thread_index);
    denoising.functions.detect_outliers = function_bind(
        &OptiXDevice::denoising_detect_outliers, this, _1, _2, _3, _4, &denoising, thread_index);

    denoising.filter_area = make_int4(rtile.x, rtile.y, rtile.w, rtile.h);
    denoising.render_buffer.samples = rtile.sample = rtile.start_sample + rtile.num_samples;
    denoising.buffer.gpu_temporary_mem = true;

    denoising.run_denoising(&rtile);

    task.update_progress(&rtile, rtile.w * rtile.h);
  }

  void launch_shader_eval(DeviceTask &task, int thread_index)
  {
    unsigned int rgen_index = PG_BACK;
    if (task.shader_eval_type >= SHADER_EVAL_BAKE)
      rgen_index = PG_BAKE;
    if (task.shader_eval_type == SHADER_EVAL_DISPLACE)
      rgen_index = PG_DISP;

    const CUDAContextScope scope(cuda_context);

    device_ptr launch_params_ptr = launch_params.device_pointer +
                                   thread_index * launch_params.data_elements;

    for (int sample = 0; sample < task.num_samples; ++sample) {
      ShaderParams params;
      params.input = (uint4 *)task.shader_input;
      params.output = (float4 *)task.shader_output;
      params.type = task.shader_eval_type;
      params.filter = task.shader_filter;
      params.sx = task.shader_x;
      params.offset = task.offset;
      params.sample = sample;

      check_result_cuda(cuMemcpyHtoDAsync(launch_params_ptr + offsetof(KernelParams, shader),
                                          &params,
                                          sizeof(params),
                                          cuda_stream[thread_index]));

      OptixShaderBindingTable sbt_params = {};
      sbt_params.raygenRecord = sbt_data.device_pointer + rgen_index * sizeof(SbtRecord);
#  ifdef WITH_CYCLES_DEBUG
      sbt_params.exceptionRecord = sbt_data.device_pointer + PG_EXCP * sizeof(SbtRecord);
#  endif
      sbt_params.missRecordBase = sbt_data.device_pointer + PG_MISS * sizeof(SbtRecord);
      sbt_params.missRecordStrideInBytes = sizeof(SbtRecord);
      sbt_params.missRecordCount = 1;
      sbt_params.hitgroupRecordBase = sbt_data.device_pointer + PG_HITD * sizeof(SbtRecord);
      sbt_params.hitgroupRecordStrideInBytes = sizeof(SbtRecord);
      sbt_params.hitgroupRecordCount = 3;  // PG_HITD, PG_HITL, PG_HITS

      check_result_optix(optixLaunch(pipelines[PIP_SHADER_EVAL],
                                     cuda_stream[thread_index],
                                     launch_params_ptr,
                                     launch_params.data_elements,
                                     &sbt_params,
                                     task.shader_w,
                                     1,
                                     1));

      check_result_cuda(cuStreamSynchronize(cuda_stream[thread_index]));

      task.update_progress(NULL);
    }
  }

  void launch_film_convert(DeviceTask &task, int thread_index)
  {
    const CUDAContextScope scope(cuda_context);

    CUfunction film_convert_func;
    check_result_cuda(cuModuleGetFunction(&film_convert_func,
                                          cuda_module,
                                          task.rgba_byte ? "kernel_cuda_convert_to_byte" :
                                                           "kernel_cuda_convert_to_half_float"));

    float sample_scale = 1.0f / (task.sample + 1);
    CUdeviceptr rgba = (task.rgba_byte ? task.rgba_byte : task.rgba_half);

    void *args[] = {&rgba,
                    &task.buffer,
                    &sample_scale,
                    &task.x,
                    &task.y,
                    &task.w,
                    &task.h,
                    &task.offset,
                    &task.stride};

    int threads_per_block;
    check_result_cuda(cuFuncGetAttribute(
        &threads_per_block, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, film_convert_func));

    const int num_threads_x = (int)sqrt(threads_per_block);
    const int num_blocks_x = (task.w + num_threads_x - 1) / num_threads_x;
    const int num_threads_y = (int)sqrt(threads_per_block);
    const int num_blocks_y = (task.h + num_threads_y - 1) / num_threads_y;

    check_result_cuda(cuLaunchKernel(film_convert_func,
                                     num_blocks_x,
                                     num_blocks_y,
                                     1, /* blocks */
                                     num_threads_x,
                                     num_threads_y,
                                     1, /* threads */
                                     0,
                                     cuda_stream[thread_index],
                                     args,
                                     0));

    check_result_cuda(cuStreamSynchronize(cuda_stream[thread_index]));

    task.update_progress(NULL);
  }

  bool build_optix_bvh(const OptixBuildInput &build_input,
                       uint16_t num_motion_steps,
                       device_memory &out_data,
                       OptixTraversableHandle &out_handle)
  {
    out_handle = 0;

    const CUDAContextScope scope(cuda_context);

    // Compute memory usage
    OptixAccelBufferSizes sizes = {};
    OptixAccelBuildOptions options;
    options.operation = OPTIX_BUILD_OPERATION_BUILD;
    options.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
    options.motionOptions.numKeys = num_motion_steps;
    options.motionOptions.flags = OPTIX_MOTION_FLAG_START_VANISH | OPTIX_MOTION_FLAG_END_VANISH;
    options.motionOptions.timeBegin = 0.0f;
    options.motionOptions.timeEnd = 1.0f;

    check_result_optix_ret(
        optixAccelComputeMemoryUsage(context, &options, &build_input, 1, &sizes));

    // Allocate required output buffers
    device_only_memory<char> temp_mem(this, "temp_build_mem");
    temp_mem.alloc_to_device(sizes.tempSizeInBytes);

    out_data.type = MEM_DEVICE_ONLY;
    out_data.data_type = TYPE_UNKNOWN;
    out_data.data_elements = 1;
    out_data.data_size = sizes.outputSizeInBytes;
    mem_alloc(out_data);

    // Finally build the acceleration structure
    check_result_optix_ret(optixAccelBuild(context,
                                           NULL,
                                           &options,
                                           &build_input,
                                           1,
                                           temp_mem.device_pointer,
                                           sizes.tempSizeInBytes,
                                           out_data.device_pointer,
                                           sizes.outputSizeInBytes,
                                           &out_handle,
                                           NULL,
                                           0));

    // Wait for all operations to finish
    check_result_cuda_ret(cuStreamSynchronize(NULL));

    return true;
  }

  bool build_optix_bvh(BVH *bvh, device_memory &out_data) override
  {
    assert(bvh->params.top_level);

    unsigned int num_instances = 0;
    unordered_map<Mesh *, vector<OptixTraversableHandle>> meshes;

    // Clear all previous AS
    blas.clear();

    // Build bottom level acceleration structures (BLAS)
    // Note: Always keep this logic in sync with bvh_optix.cpp!
    for (Object *ob : bvh->objects) {
      // Skip meshes for which acceleration structure already exists
      if (meshes.find(ob->mesh) != meshes.end())
        continue;

      Mesh *const mesh = ob->mesh;
      vector<OptixTraversableHandle> handles;

      // Build BLAS for curve primitives
      if (bvh->params.primitive_mask & PRIMITIVE_ALL_CURVE && mesh->num_curves() > 0) {
        const size_t num_curves = mesh->num_curves();
        const size_t num_segments = mesh->num_segments();

        size_t num_motion_steps = 1;
        Attribute *motion_keys = mesh->curve_attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
        if (motion_blur && mesh->use_motion_blur && motion_keys) {
          num_motion_steps = mesh->motion_steps;
        }

        device_vector<OptixAabb> aabb_data(this, "temp_aabb_data", MEM_READ_ONLY);
        aabb_data.alloc(num_segments * num_motion_steps);

        // Get AABBs for each motion step
        for (size_t step = 0; step < num_motion_steps; ++step) {
          // The center step for motion vertices is not stored in the attribute
          const float3 *keys = mesh->curve_keys.data();
          size_t center_step = (num_motion_steps - 1) / 2;
          if (step != center_step) {
            size_t attr_offset = (step > center_step) ? step - 1 : step;
            // Technically this is a float4 array, but sizeof(float3) is the same as sizeof(float4)
            keys = motion_keys->data_float3() + attr_offset * mesh->curve_keys.size();
          }

          size_t i = step * num_segments;
          for (size_t j = 0; j < num_curves; ++j) {
            const Mesh::Curve c = mesh->get_curve(j);

            for (size_t k = 0; k < c.num_segments(); ++i, ++k) {
              BoundBox bounds = BoundBox::empty;
              c.bounds_grow(k, keys, mesh->curve_radius.data(), bounds);

              aabb_data[i].minX = bounds.min.x;
              aabb_data[i].minY = bounds.min.y;
              aabb_data[i].minZ = bounds.min.z;
              aabb_data[i].maxX = bounds.max.x;
              aabb_data[i].maxY = bounds.max.y;
              aabb_data[i].maxZ = bounds.max.z;
            }
          }
        }

        // Upload AABB data to GPU
        aabb_data.copy_to_device();

        vector<device_ptr> aabb_ptrs;
        aabb_ptrs.reserve(num_motion_steps);
        for (size_t step = 0; step < num_motion_steps; ++step) {
          aabb_ptrs.push_back(aabb_data.device_pointer + step * num_segments * sizeof(OptixAabb));
        }

        // Disable visibility test anyhit program, since it is already checked during intersection
        // Those trace calls that require anyhit can force it with OPTIX_RAY_FLAG_ENFORCE_ANYHIT
        unsigned int build_flags = OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT;
        OptixBuildInput build_input = {};
        build_input.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
        build_input.aabbArray.aabbBuffers = (CUdeviceptr *)aabb_ptrs.data();
        build_input.aabbArray.numPrimitives = num_segments;
        build_input.aabbArray.strideInBytes = sizeof(OptixAabb);
        build_input.aabbArray.flags = &build_flags;
        build_input.aabbArray.numSbtRecords = 1;
        build_input.aabbArray.primitiveIndexOffset = mesh->prim_offset;

        // Allocate memory for new BLAS and build it
        blas.emplace_back(this, "blas");
        handles.emplace_back();
        if (!build_optix_bvh(build_input, num_motion_steps, blas.back(), handles.back()))
          return false;
      }

      // Build BLAS for triangle primitives
      if (bvh->params.primitive_mask & PRIMITIVE_ALL_TRIANGLE && mesh->num_triangles() > 0) {
        const size_t num_verts = mesh->verts.size();

        size_t num_motion_steps = 1;
        Attribute *motion_keys = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
        if (motion_blur && mesh->use_motion_blur && motion_keys) {
          num_motion_steps = mesh->motion_steps;
        }

        device_vector<int> index_data(this, "temp_index_data", MEM_READ_ONLY);
        index_data.alloc(mesh->triangles.size());
        memcpy(index_data.data(), mesh->triangles.data(), mesh->triangles.size() * sizeof(int));
        device_vector<float3> vertex_data(this, "temp_vertex_data", MEM_READ_ONLY);
        vertex_data.alloc(num_verts * num_motion_steps);

        for (size_t step = 0; step < num_motion_steps; ++step) {
          const float3 *verts = mesh->verts.data();

          size_t center_step = (num_motion_steps - 1) / 2;
          // The center step for motion vertices is not stored in the attribute
          if (step != center_step) {
            verts = motion_keys->data_float3() +
                    (step > center_step ? step - 1 : step) * num_verts;
          }

          memcpy(vertex_data.data() + num_verts * step, verts, num_verts * sizeof(float3));
        }

        // Upload triangle data to GPU
        index_data.copy_to_device();
        vertex_data.copy_to_device();

        vector<device_ptr> vertex_ptrs;
        vertex_ptrs.reserve(num_motion_steps);
        for (size_t step = 0; step < num_motion_steps; ++step) {
          vertex_ptrs.push_back(vertex_data.device_pointer + num_verts * step * sizeof(float3));
        }

        // No special build flags for triangle primitives
        unsigned int build_flags = OPTIX_GEOMETRY_FLAG_NONE;
        OptixBuildInput build_input = {};
        build_input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
        build_input.triangleArray.vertexBuffers = (CUdeviceptr *)vertex_ptrs.data();
        build_input.triangleArray.numVertices = num_verts;
        build_input.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
        build_input.triangleArray.vertexStrideInBytes = sizeof(float3);
        build_input.triangleArray.indexBuffer = index_data.device_pointer;
        build_input.triangleArray.numIndexTriplets = mesh->num_triangles();
        build_input.triangleArray.indexFormat = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
        build_input.triangleArray.indexStrideInBytes = 3 * sizeof(int);
        build_input.triangleArray.flags = &build_flags;
        // The SBT does not store per primitive data since Cycles already allocates separate
        // buffers for that purpose. OptiX does not allow this to be zero though, so just pass in
        // one and rely on that having the same meaning in this case.
        build_input.triangleArray.numSbtRecords = 1;
        // Triangle primitives are packed right after the curve primitives of this mesh
        build_input.triangleArray.primitiveIndexOffset = mesh->prim_offset + mesh->num_segments();

        // Allocate memory for new BLAS and build it
        blas.emplace_back(this, "blas");
        handles.emplace_back();
        if (!build_optix_bvh(build_input, num_motion_steps, blas.back(), handles.back()))
          return false;
      }

      meshes.insert({mesh, handles});
    }

    // Fill instance descriptions
    device_vector<OptixAabb> aabbs(this, "tlas_aabbs", MEM_READ_ONLY);
    aabbs.alloc(bvh->objects.size() * 2);
    device_vector<OptixInstance> instances(this, "tlas_instances", MEM_READ_ONLY);
    instances.alloc(bvh->objects.size() * 2);

    for (Object *ob : bvh->objects) {
      // Skip non-traceable objects
      if (!ob->is_traceable())
        continue;
      // Create separate instance for triangle/curve meshes of an object
      for (OptixTraversableHandle handle : meshes[ob->mesh]) {
        OptixAabb &aabb = aabbs[num_instances];
        aabb.minX = ob->bounds.min.x;
        aabb.minY = ob->bounds.min.y;
        aabb.minZ = ob->bounds.min.z;
        aabb.maxX = ob->bounds.max.x;
        aabb.maxY = ob->bounds.max.y;
        aabb.maxZ = ob->bounds.max.z;

        OptixInstance &instance = instances[num_instances++];
        memset(&instance, 0, sizeof(instance));

        // Clear transform to identity matrix
        instance.transform[0] = 1.0f;
        instance.transform[5] = 1.0f;
        instance.transform[10] = 1.0f;

        // Set user instance ID to object index
        instance.instanceId = ob->get_device_index();

        // Volumes have a special bit set in the visibility mask so a trace can mask only volumes
        // See 'scene_intersect_volume' in bvh.h
        instance.visibilityMask = (ob->mesh->has_volume ? 3 : 1);

        // Insert motion traversable if object has motion
        if (motion_blur && ob->use_motion()) {
          blas.emplace_back(this, "motion_transform");
          device_only_memory<uint8_t> &motion_transform_gpu = blas.back();
          motion_transform_gpu.alloc_to_device(sizeof(OptixSRTMotionTransform) +
                                               (max(ob->motion.size(), 2) - 2) *
                                                   sizeof(OptixSRTData));

          // Allocate host side memory for motion transform and fill it with transform data
          OptixSRTMotionTransform &motion_transform = *reinterpret_cast<OptixSRTMotionTransform *>(
              motion_transform_gpu.host_pointer = new uint8_t[motion_transform_gpu.memory_size()]);
          motion_transform.child = handle;
          motion_transform.motionOptions.numKeys = ob->motion.size();
          motion_transform.motionOptions.flags = OPTIX_MOTION_FLAG_NONE;
          motion_transform.motionOptions.timeBegin = 0.0f;
          motion_transform.motionOptions.timeEnd = 1.0f;

          OptixSRTData *const srt_data = motion_transform.srtData;
          array<DecomposedTransform> decomp(ob->motion.size());
          transform_motion_decompose(decomp.data(), ob->motion.data(), ob->motion.size());

          for (size_t i = 0; i < ob->motion.size(); ++i) {
            // scaling
            srt_data[i].a = decomp[i].z.x;   // scale.x.y
            srt_data[i].b = decomp[i].z.y;   // scale.x.z
            srt_data[i].c = decomp[i].w.x;   // scale.y.z
            srt_data[i].sx = decomp[i].y.w;  // scale.x.x
            srt_data[i].sy = decomp[i].z.w;  // scale.y.y
            srt_data[i].sz = decomp[i].w.w;  // scale.z.z
            srt_data[i].pvx = 0;
            srt_data[i].pvy = 0;
            srt_data[i].pvz = 0;
            // rotation
            srt_data[i].qx = decomp[i].x.x;
            srt_data[i].qy = decomp[i].x.y;
            srt_data[i].qz = decomp[i].x.z;
            srt_data[i].qw = decomp[i].x.w;
            // transform
            srt_data[i].tx = decomp[i].y.x;
            srt_data[i].ty = decomp[i].y.y;
            srt_data[i].tz = decomp[i].y.z;
          }

          // Upload motion transform to GPU
          mem_copy_to(motion_transform_gpu);
          delete[] reinterpret_cast<uint8_t *>(motion_transform_gpu.host_pointer);
          motion_transform_gpu.host_pointer = 0;

          // Disable instance transform if object uses motion transform already
          instance.flags = OPTIX_INSTANCE_FLAG_DISABLE_TRANSFORM;

          // Get traversable handle to motion transform
          optixConvertPointerToTraversableHandle(context,
                                                 motion_transform_gpu.device_pointer,
                                                 OPTIX_TRAVERSABLE_TYPE_SRT_MOTION_TRANSFORM,
                                                 &instance.traversableHandle);
        }
        else {
          instance.traversableHandle = handle;

          if (ob->mesh->is_instanced()) {
            // Set transform matrix
            memcpy(instance.transform, &ob->tfm, sizeof(instance.transform));
          }
          else {
            // Disable instance transform if mesh already has it applied to vertex data
            instance.flags = OPTIX_INSTANCE_FLAG_DISABLE_TRANSFORM;
            // Non-instanced objects read ID from prim_object, so
            // distinguish them from instanced objects with high bit set
            instance.instanceId |= 0x800000;
          }
        }
      }
    }

    // Upload instance descriptions
    aabbs.resize(num_instances);
    aabbs.copy_to_device();
    instances.resize(num_instances);
    instances.copy_to_device();

    // Build top-level acceleration structure
    OptixBuildInput build_input = {};
    build_input.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
    build_input.instanceArray.instances = instances.device_pointer;
    build_input.instanceArray.numInstances = num_instances;
    build_input.instanceArray.aabbs = aabbs.device_pointer;
    build_input.instanceArray.numAabbs = num_instances;

    return build_optix_bvh(build_input, 0 /* TLAS has no motion itself */, out_data, tlas_handle);
  }

  void update_texture_info()
  {
    if (need_texture_info) {
      texture_info.copy_to_device();
      need_texture_info = false;
    }
  }

  void update_launch_params(const char *name, size_t offset, void *data, size_t data_size)
  {
    const CUDAContextScope scope(cuda_context);

    for (int i = 0; i < info.cpu_threads; ++i)
      check_result_cuda(
          cuMemcpyHtoD(launch_params.device_pointer + i * launch_params.data_elements + offset,
                       data,
                       data_size));

    // Set constant memory for CUDA module
    // TODO(pmours): This is only used for tonemapping (see 'launch_film_convert').
    //               Could be removed by moving those functions to filter CUDA module.
    size_t bytes = 0;
    CUdeviceptr mem = 0;
    check_result_cuda(cuModuleGetGlobal(&mem, &bytes, cuda_module, name));
    assert(mem != NULL && bytes == data_size);
    check_result_cuda(cuMemcpyHtoD(mem, data, data_size));
  }

  void mem_alloc(device_memory &mem) override
  {
    if (mem.type == MEM_PIXELS && !background) {
      // Always fall back to no interop for now
      // TODO(pmours): Support OpenGL interop when moving CUDA memory management to common code
      background = true;
    }
    else if (mem.type == MEM_TEXTURE) {
      assert(!"mem_alloc not supported for textures.");
      return;
    }

    generic_alloc(mem);
  }

  CUDAMem *generic_alloc(device_memory &mem, size_t pitch_padding = 0)
  {
    CUDAContextScope scope(cuda_context);

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
    bool is_texture = (mem.type == MEM_TEXTURE) && (&mem != &texture_info);
    bool is_image = is_texture && (mem.data_height > 1);

    size_t headroom = (is_texture) ? device_texture_headroom : device_working_headroom;

    size_t total = 0, free = 0;
    cuMemGetInfo(&free, &total);

    /* Move textures to host memory if needed. */
    if (!move_texture_to_host && !is_image && (size + headroom) >= free) {
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
    void *map_host_pointer = 0;
    bool free_map_host = false;

    if (mem_alloc_result != CUDA_SUCCESS && can_map_host &&
        map_host_used + size < map_host_limit) {
      if (mem.shared_pointer) {
        /* Another device already allocated host memory. */
        mem_alloc_result = CUDA_SUCCESS;
        map_host_pointer = mem.shared_pointer;
      }
      else {
        /* Allocate host memory ourselves. */
        mem_alloc_result = cuMemHostAlloc(
            &map_host_pointer, size, CU_MEMHOSTALLOC_DEVICEMAP | CU_MEMHOSTALLOC_WRITECOMBINED);
        mem.shared_pointer = map_host_pointer;
        free_map_host = true;
      }

      if (mem_alloc_result == CUDA_SUCCESS) {
        cuMemHostGetDevicePointer_v2(&device_pointer, mem.shared_pointer, 0);
        map_host_used += size;
        status = " in host memory";

        /* Replace host pointer with our host allocation. Only works if
         * CUDA memory layout is the same and has no pitch padding. Also
         * does not work if we move textures to host during a render,
         * since other devices might be using the memory. */
        if (!move_texture_to_host && pitch_padding == 0 && mem.host_pointer &&
            mem.host_pointer != mem.shared_pointer) {
          memcpy(mem.shared_pointer, mem.host_pointer, size);
          mem.host_free();
          mem.host_pointer = mem.shared_pointer;
        }
      }
      else {
        status = " failed, out of host memory";
      }
    }
    else if (mem_alloc_result != CUDA_SUCCESS) {
      status = " failed, out of device and host memory";
    }

    if (mem.name) {
      VLOG(1) << "Buffer allocate: " << mem.name << ", "
              << string_human_readable_number(mem.memory_size()) << " bytes. ("
              << string_human_readable_size(mem.memory_size()) << ")" << status;
    }

    if (mem_alloc_result != CUDA_SUCCESS) {
      set_error(string_printf("Buffer allocate %s", status));
      return NULL;
    }

    mem.device_pointer = (device_ptr)device_pointer;
    mem.device_size = size;
    stats.mem_alloc(size);

    if (!mem.device_pointer) {
      return NULL;
    }

    /* Insert into map of allocations. */
    CUDAMem *cmem = &cuda_mem_map[&mem];
    cmem->map_host_pointer = map_host_pointer;
    cmem->free_map_host = free_map_host;
    return cmem;
  }

  void tex_alloc(device_memory &mem)
  {
    CUDAContextScope scope(cuda_context);

    /* General variables for both architectures */
    string bind_name = mem.name;
    size_t dsize = datatype_size(mem.data_type);
    size_t size = mem.memory_size();

    CUaddress_mode address_mode = CU_TR_ADDRESS_MODE_WRAP;
    switch (mem.extension) {
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
    if (mem.interpolation == INTERPOLATION_CLOSEST) {
      filter_mode = CU_TR_FILTER_MODE_POINT;
    }
    else {
      filter_mode = CU_TR_FILTER_MODE_LINEAR;
    }

    /* Data Storage */
    if (mem.interpolation == INTERPOLATION_NONE) {
      generic_alloc(mem);
      generic_copy_to(mem);

      // Update data storage pointers in launch parameters
#  define KERNEL_TEX(data_type, tex_name) \
    if (strcmp(mem.name, #tex_name) == 0) \
      update_launch_params( \
          mem.name, offsetof(KernelParams, tex_name), &mem.device_pointer, sizeof(device_ptr));
#  include "kernel/kernel_textures.h"
#  undef KERNEL_TEX
      return;
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

    if (mem.data_depth > 1) {
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

      check_result_cuda(cuArray3DCreate(&array_3d, &desc));

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

      check_result_cuda(cuMemcpy3D(&param));

      mem.device_pointer = (device_ptr)array_3d;
      mem.device_size = size;
      stats.mem_alloc(size);

      cmem = &cuda_mem_map[&mem];
      cmem->texobject = 0;
      cmem->array = array_3d;
    }
    else if (mem.data_height > 0) {
      /* 2D texture, using pitch aligned linear memory. */
      int alignment = 0;
      check_result_cuda(cuDeviceGetAttribute(
          &alignment, CU_DEVICE_ATTRIBUTE_TEXTURE_PITCH_ALIGNMENT, cuda_device));
      dst_pitch = align_up(src_pitch, alignment);
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

      check_result_cuda(cuMemcpy2DUnaligned(&param));
    }
    else {
      /* 1D texture, using linear memory. */
      cmem = generic_alloc(mem);
      if (!cmem) {
        return;
      }

      check_result_cuda(cuMemcpyHtoD(mem.device_pointer, mem.host_pointer, size));
    }

    /* Kepler+, bindless textures. */
    int flat_slot = 0;
    if (string_startswith(mem.name, "__tex_image")) {
      int pos = string(mem.name).rfind("_");
      flat_slot = atoi(mem.name + pos + 1);
    }
    else {
      assert(0);
    }

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

    check_result_cuda(cuTexObjectCreate(&cmem->texobject, &resDesc, &texDesc, NULL));

    /* Resize once */
    if (flat_slot >= texture_info.size()) {
      /* Allocate some slots in advance, to reduce amount
       * of re-allocations. */
      texture_info.resize(flat_slot + 128);
    }

    /* Set Mapping and tag that we need to (re-)upload to device */
    TextureInfo &info = texture_info[flat_slot];
    info.data = (uint64_t)cmem->texobject;
    info.cl_buffer = 0;
    info.interpolation = mem.interpolation;
    info.extension = mem.extension;
    info.width = mem.data_width;
    info.height = mem.data_height;
    info.depth = mem.data_depth;
    need_texture_info = true;
  }

  void mem_copy_to(device_memory &mem) override
  {
    if (mem.type == MEM_PIXELS) {
      assert(!"mem_copy_to not supported for pixels.");
    }
    else if (mem.type == MEM_TEXTURE) {
      tex_free(mem);
      tex_alloc(mem);
    }
    else {
      if (!mem.device_pointer) {
        generic_alloc(mem);
      }

      generic_copy_to(mem);
    }
  }

  void generic_copy_to(device_memory &mem)
  {
    if (mem.host_pointer && mem.device_pointer) {
      CUDAContextScope scope(cuda_context);

      if (mem.host_pointer != mem.shared_pointer) {
        check_result_cuda(
            cuMemcpyHtoD((CUdeviceptr)mem.device_pointer, mem.host_pointer, mem.memory_size()));
      }
    }
  }

  void mem_copy_from(device_memory &mem, int y, int w, int h, int elem) override
  {
    if (mem.type == MEM_PIXELS && !background) {
      assert(!"mem_copy_from not supported for pixels.");
    }
    else if (mem.type == MEM_TEXTURE) {
      assert(!"mem_copy_from not supported for textures.");
    }
    else {
      // Calculate linear memory offset and size
      const size_t size = elem * w * h;
      const size_t offset = elem * y * w;

      if (mem.host_pointer && mem.device_pointer) {
        const CUDAContextScope scope(cuda_context);
        check_result_cuda(cuMemcpyDtoH(
            (char *)mem.host_pointer + offset, (CUdeviceptr)mem.device_pointer + offset, size));
      }
      else if (mem.host_pointer) {
        memset((char *)mem.host_pointer + offset, 0, size);
      }
    }
  }

  void mem_zero(device_memory &mem) override
  {
    if (mem.host_pointer)
      memset(mem.host_pointer, 0, mem.memory_size());
    if (mem.host_pointer && mem.host_pointer == mem.shared_pointer)
      return;  // This is shared host memory, so no device memory to update

    if (!mem.device_pointer)
      mem_alloc(mem);  // Need to allocate memory first if it does not exist yet

    const CUDAContextScope scope(cuda_context);
    check_result_cuda(cuMemsetD8((CUdeviceptr)mem.device_pointer, 0, mem.memory_size()));
  }

  void mem_free(device_memory &mem) override
  {
    if (mem.type == MEM_PIXELS && !background) {
      assert(!"mem_free not supported for pixels.");
    }
    else if (mem.type == MEM_TEXTURE) {
      tex_free(mem);
    }
    else {
      generic_free(mem);
    }
  }

  void generic_free(device_memory &mem)
  {
    if (mem.device_pointer) {
      CUDAContextScope scope(cuda_context);
      const CUDAMem &cmem = cuda_mem_map[&mem];

      if (cmem.map_host_pointer) {
        /* Free host memory. */
        if (cmem.free_map_host) {
          cuMemFreeHost(cmem.map_host_pointer);
          if (mem.host_pointer == mem.shared_pointer) {
            mem.host_pointer = 0;
          }
          mem.shared_pointer = 0;
        }

        map_host_used -= mem.device_size;
      }
      else {
        /* Free device memory. */
        cuMemFree(mem.device_pointer);
      }

      stats.mem_free(mem.device_size);
      mem.device_pointer = 0;
      mem.device_size = 0;

      cuda_mem_map.erase(cuda_mem_map.find(&mem));
    }
  }

  void tex_free(device_memory &mem)
  {
    if (mem.device_pointer) {
      CUDAContextScope scope(cuda_context);
      const CUDAMem &cmem = cuda_mem_map[&mem];

      if (cmem.texobject) {
        /* Free bindless texture. */
        cuTexObjectDestroy(cmem.texobject);
      }

      if (cmem.array) {
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

  void move_textures_to_host(size_t size, bool for_texture)
  {
    /* Signal to reallocate textures in host memory only. */
    move_texture_to_host = true;

    while (size > 0) {
      /* Find suitable memory allocation to move. */
      device_memory *max_mem = NULL;
      size_t max_size = 0;
      bool max_is_image = false;

      foreach (auto &pair, cuda_mem_map) {
        device_memory &mem = *pair.first;
        CUDAMem *cmem = &pair.second;

        bool is_texture = (mem.type == MEM_TEXTURE) && (&mem != &texture_info);
        bool is_image = is_texture && (mem.data_height > 1);

        /* Can't move this type of memory. */
        if (!is_texture || cmem->array) {
          continue;
        }

        /* Already in host memory. */
        if (cmem->map_host_pointer) {
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

        /* Preserve the original device pointer, in case of multi device
         * we can't change it because the pointer mapping would break. */
        device_ptr prev_pointer = max_mem->device_pointer;
        size_t prev_size = max_mem->device_size;

        tex_free(*max_mem);
        tex_alloc(*max_mem);
        size = (max_size >= size) ? 0 : size - max_size;

        max_mem->device_pointer = prev_pointer;
        max_mem->device_size = prev_size;
      }
      else {
        break;
      }
    }

    /* Update texture info array with new pointers. */
    update_texture_info();

    move_texture_to_host = false;
  }

  void const_copy_to(const char *name, void *host, size_t size) override
  {
    if (strcmp(name, "__data") == 0) {
      assert(size <= sizeof(KernelData));

      // Fix traversable handle on multi devices
      KernelData *const data = (KernelData *)host;
      *(OptixTraversableHandle *)&data->bvh.scene = tlas_handle;

      update_launch_params(name, offsetof(KernelParams, data), host, size);
    }
  }

  device_ptr mem_alloc_sub_ptr(device_memory &mem, int offset, int /*size*/) override
  {
    return (device_ptr)(((char *)mem.device_pointer) + mem.memory_elements_size(offset));
  }

  void task_add(DeviceTask &task) override
  {
    // Upload texture information to device if it has changed since last launch
    update_texture_info();

    // Split task into smaller ones
    list<DeviceTask> tasks;
    task.split(tasks, info.cpu_threads);

    // Queue tasks in internal task pool
    struct OptiXDeviceTask : public DeviceTask {
      OptiXDeviceTask(OptiXDevice *device, DeviceTask &task, int task_index) : DeviceTask(task)
      {
        // Using task index parameter instead of thread index, since number of CUDA streams may
        // differ from number of threads
        run = function_bind(&OptiXDevice::thread_run, device, *this, task_index);
      }
    };

    int task_index = 0;
    for (DeviceTask &task : tasks)
      task_pool.push(new OptiXDeviceTask(this, task, task_index++));
  }

  void task_wait() override
  {
    // Wait for all queued tasks to finish
    task_pool.wait_work();
  }

  void task_cancel() override
  {
    // Cancel any remaining tasks in the internal pool
    task_pool.cancel();
  }

#  define CUDA_GET_BLOCKSIZE(func, w, h) \
    int threads; \
    check_result_cuda_ret( \
        cuFuncGetAttribute(&threads, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, func)); \
    threads = (int)sqrt((float)threads); \
    int xblocks = ((w) + threads - 1) / threads; \
    int yblocks = ((h) + threads - 1) / threads;

#  define CUDA_LAUNCH_KERNEL(func, args) \
    check_result_cuda_ret(cuLaunchKernel( \
        func, xblocks, yblocks, 1, threads, threads, 1, 0, cuda_stream[thread_index], args, 0));

  /* Similar as above, but for 1-dimensional blocks. */
#  define CUDA_GET_BLOCKSIZE_1D(func, w, h) \
    int threads; \
    check_result_cuda_ret( \
        cuFuncGetAttribute(&threads, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, func)); \
    int xblocks = ((w) + threads - 1) / threads; \
    int yblocks = h;

#  define CUDA_LAUNCH_KERNEL_1D(func, args) \
    check_result_cuda_ret(cuLaunchKernel( \
        func, xblocks, yblocks, 1, threads, 1, 1, 0, cuda_stream[thread_index], args, 0));

  bool denoising_non_local_means(device_ptr image_ptr,
                                 device_ptr guide_ptr,
                                 device_ptr variance_ptr,
                                 device_ptr out_ptr,
                                 DenoisingTask *task,
                                 int thread_index)
  {
    if (have_error())
      return false;

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

    CUdeviceptr difference = (CUdeviceptr)task->buffer.temporary_mem.device_pointer;
    CUdeviceptr blurDifference = difference + sizeof(float) * pass_stride * num_shifts;
    CUdeviceptr weightAccum = difference + 2 * sizeof(float) * pass_stride * num_shifts;
    CUdeviceptr scale_ptr = 0;

    check_result_cuda_ret(
        cuMemsetD8Async(weightAccum, 0, sizeof(float) * pass_stride, cuda_stream[thread_index]));
    check_result_cuda_ret(
        cuMemsetD8Async(out_ptr, 0, sizeof(float) * pass_stride, cuda_stream[thread_index]));

    {
      CUfunction cuNLMCalcDifference, cuNLMBlur, cuNLMCalcWeight, cuNLMUpdateOutput;
      check_result_cuda_ret(cuModuleGetFunction(
          &cuNLMCalcDifference, cuda_filter_module, "kernel_cuda_filter_nlm_calc_difference"));
      check_result_cuda_ret(
          cuModuleGetFunction(&cuNLMBlur, cuda_filter_module, "kernel_cuda_filter_nlm_blur"));
      check_result_cuda_ret(cuModuleGetFunction(
          &cuNLMCalcWeight, cuda_filter_module, "kernel_cuda_filter_nlm_calc_weight"));
      check_result_cuda_ret(cuModuleGetFunction(
          &cuNLMUpdateOutput, cuda_filter_module, "kernel_cuda_filter_nlm_update_output"));

      check_result_cuda_ret(cuFuncSetCacheConfig(cuNLMCalcDifference, CU_FUNC_CACHE_PREFER_L1));
      check_result_cuda_ret(cuFuncSetCacheConfig(cuNLMBlur, CU_FUNC_CACHE_PREFER_L1));
      check_result_cuda_ret(cuFuncSetCacheConfig(cuNLMCalcWeight, CU_FUNC_CACHE_PREFER_L1));
      check_result_cuda_ret(cuFuncSetCacheConfig(cuNLMUpdateOutput, CU_FUNC_CACHE_PREFER_L1));

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
      check_result_cuda_ret(cuModuleGetFunction(
          &cuNLMNormalize, cuda_filter_module, "kernel_cuda_filter_nlm_normalize"));
      check_result_cuda_ret(cuFuncSetCacheConfig(cuNLMNormalize, CU_FUNC_CACHE_PREFER_L1));
      void *normalize_args[] = {&out_ptr, &weightAccum, &w, &h, &stride};
      CUDA_GET_BLOCKSIZE(cuNLMNormalize, w, h);
      CUDA_LAUNCH_KERNEL(cuNLMNormalize, normalize_args);
      check_result_cuda_ret(cuStreamSynchronize(cuda_stream[thread_index]));
    }

    return !have_error();
  }

  bool denoising_construct_transform(DenoisingTask *task, int thread_index)
  {
    if (have_error())
      return false;

    CUfunction cuFilterConstructTransform;
    check_result_cuda_ret(cuModuleGetFunction(&cuFilterConstructTransform,
                                              cuda_filter_module,
                                              "kernel_cuda_filter_construct_transform"));
    check_result_cuda_ret(
        cuFuncSetCacheConfig(cuFilterConstructTransform, CU_FUNC_CACHE_PREFER_SHARED));
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
    check_result_cuda_ret(cuCtxSynchronize());

    return !have_error();
  }

  bool denoising_accumulate(device_ptr color_ptr,
                            device_ptr color_variance_ptr,
                            device_ptr scale_ptr,
                            int frame,
                            DenoisingTask *task,
                            int thread_index)
  {
    if (have_error())
      return false;

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

    CUdeviceptr difference = (CUdeviceptr)task->buffer.temporary_mem.device_pointer;
    CUdeviceptr blurDifference = difference + sizeof(float) * pass_stride * num_shifts;

    CUfunction cuNLMCalcDifference, cuNLMBlur, cuNLMCalcWeight, cuNLMConstructGramian;
    check_result_cuda_ret(cuModuleGetFunction(
        &cuNLMCalcDifference, cuda_filter_module, "kernel_cuda_filter_nlm_calc_difference"));
    check_result_cuda_ret(
        cuModuleGetFunction(&cuNLMBlur, cuda_filter_module, "kernel_cuda_filter_nlm_blur"));
    check_result_cuda_ret(cuModuleGetFunction(
        &cuNLMCalcWeight, cuda_filter_module, "kernel_cuda_filter_nlm_calc_weight"));
    check_result_cuda_ret(cuModuleGetFunction(
        &cuNLMConstructGramian, cuda_filter_module, "kernel_cuda_filter_nlm_construct_gramian"));

    check_result_cuda_ret(cuFuncSetCacheConfig(cuNLMCalcDifference, CU_FUNC_CACHE_PREFER_L1));
    check_result_cuda_ret(cuFuncSetCacheConfig(cuNLMBlur, CU_FUNC_CACHE_PREFER_L1));
    check_result_cuda_ret(cuFuncSetCacheConfig(cuNLMCalcWeight, CU_FUNC_CACHE_PREFER_L1));
    check_result_cuda_ret(
        cuFuncSetCacheConfig(cuNLMConstructGramian, CU_FUNC_CACHE_PREFER_SHARED));

    CUDA_GET_BLOCKSIZE_1D(cuNLMCalcDifference,
                          task->reconstruction_state.source_w *
                              task->reconstruction_state.source_h,
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
    void *calc_weight_args[] = {
        &blurDifference, &difference, &w, &h, &stride, &pass_stride, &r, &f};
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
    check_result_cuda_ret(cuCtxSynchronize());

    return !have_error();
  }

  bool denoising_solve(device_ptr output_ptr, DenoisingTask *task, int thread_index)
  {
    if (have_error())
      return false;

    CUfunction cuFinalize;
    check_result_cuda_ret(
        cuModuleGetFunction(&cuFinalize, cuda_filter_module, "kernel_cuda_filter_finalize"));
    check_result_cuda_ret(cuFuncSetCacheConfig(cuFinalize, CU_FUNC_CACHE_PREFER_L1));
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
    check_result_cuda_ret(cuStreamSynchronize(cuda_stream[thread_index]));

    return !have_error();
  }

  bool denoising_combine_halves(device_ptr a_ptr,
                                device_ptr b_ptr,
                                device_ptr mean_ptr,
                                device_ptr variance_ptr,
                                int r,
                                int4 rect,
                                DenoisingTask *task,
                                int thread_index)
  {
    if (have_error())
      return false;

    CUfunction cuFilterCombineHalves;
    check_result_cuda_ret(cuModuleGetFunction(
        &cuFilterCombineHalves, cuda_filter_module, "kernel_cuda_filter_combine_halves"));
    check_result_cuda_ret(cuFuncSetCacheConfig(cuFilterCombineHalves, CU_FUNC_CACHE_PREFER_L1));
    CUDA_GET_BLOCKSIZE(
        cuFilterCombineHalves, task->rect.z - task->rect.x, task->rect.w - task->rect.y);

    void *args[] = {&mean_ptr, &variance_ptr, &a_ptr, &b_ptr, &rect, &r};
    CUDA_LAUNCH_KERNEL(cuFilterCombineHalves, args);
    check_result_cuda_ret(cuStreamSynchronize(cuda_stream[thread_index]));

    return !have_error();
  }

  bool denoising_divide_shadow(device_ptr a_ptr,
                               device_ptr b_ptr,
                               device_ptr sample_variance_ptr,
                               device_ptr sv_variance_ptr,
                               device_ptr buffer_variance_ptr,
                               DenoisingTask *task,
                               int thread_index)
  {
    if (have_error())
      return false;

    CUfunction cuFilterDivideShadow;
    check_result_cuda_ret(cuModuleGetFunction(
        &cuFilterDivideShadow, cuda_filter_module, "kernel_cuda_filter_divide_shadow"));
    check_result_cuda_ret(cuFuncSetCacheConfig(cuFilterDivideShadow, CU_FUNC_CACHE_PREFER_L1));
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
    check_result_cuda_ret(cuStreamSynchronize(cuda_stream[thread_index]));

    return !have_error();
  }

  bool denoising_get_feature(int mean_offset,
                             int variance_offset,
                             device_ptr mean_ptr,
                             device_ptr variance_ptr,
                             float scale,
                             DenoisingTask *task,
                             int thread_index)
  {
    if (have_error())
      return false;

    CUfunction cuFilterGetFeature;
    check_result_cuda_ret(cuModuleGetFunction(
        &cuFilterGetFeature, cuda_filter_module, "kernel_cuda_filter_get_feature"));
    check_result_cuda_ret(cuFuncSetCacheConfig(cuFilterGetFeature, CU_FUNC_CACHE_PREFER_L1));
    CUDA_GET_BLOCKSIZE(
        cuFilterGetFeature, task->rect.z - task->rect.x, task->rect.w - task->rect.y);

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
    check_result_cuda_ret(cuStreamSynchronize(cuda_stream[thread_index]));

    return !have_error();
  }

  bool denoising_write_feature(int out_offset,
                               device_ptr from_ptr,
                               device_ptr buffer_ptr,
                               DenoisingTask *task,
                               int thread_index)
  {
    if (have_error())
      return false;

    CUfunction cuFilterWriteFeature;
    check_result_cuda_ret(cuModuleGetFunction(
        &cuFilterWriteFeature, cuda_filter_module, "kernel_cuda_filter_write_feature"));
    check_result_cuda_ret(cuFuncSetCacheConfig(cuFilterWriteFeature, CU_FUNC_CACHE_PREFER_L1));
    CUDA_GET_BLOCKSIZE(cuFilterWriteFeature, task->filter_area.z, task->filter_area.w);

    void *args[] = {&task->render_buffer.samples,
                    &task->reconstruction_state.buffer_params,
                    &task->filter_area,
                    &from_ptr,
                    &buffer_ptr,
                    &out_offset,
                    &task->rect};
    CUDA_LAUNCH_KERNEL(cuFilterWriteFeature, args);
    check_result_cuda_ret(cuStreamSynchronize(cuda_stream[thread_index]));

    return !have_error();
  }

  bool denoising_detect_outliers(device_ptr image_ptr,
                                 device_ptr variance_ptr,
                                 device_ptr depth_ptr,
                                 device_ptr output_ptr,
                                 DenoisingTask *task,
                                 int thread_index)
  {
    if (have_error())
      return false;

    CUfunction cuFilterDetectOutliers;
    check_result_cuda_ret(cuModuleGetFunction(
        &cuFilterDetectOutliers, cuda_filter_module, "kernel_cuda_filter_detect_outliers"));
    check_result_cuda_ret(cuFuncSetCacheConfig(cuFilterDetectOutliers, CU_FUNC_CACHE_PREFER_L1));
    CUDA_GET_BLOCKSIZE(
        cuFilterDetectOutliers, task->rect.z - task->rect.x, task->rect.w - task->rect.y);

    void *args[] = {&image_ptr,
                    &variance_ptr,
                    &depth_ptr,
                    &output_ptr,
                    &task->rect,
                    &task->buffer.pass_stride};

    CUDA_LAUNCH_KERNEL(cuFilterDetectOutliers, args);
    check_result_cuda_ret(cuStreamSynchronize(cuda_stream[thread_index]));

    return !have_error();
  }
};

bool device_optix_init()
{
  if (g_optixFunctionTable.optixDeviceContextCreate != NULL)
    return true;  // Already initialized function table

  // Need to initialize CUDA as well
  if (!device_cuda_init())
    return false;

#  ifdef WITH_CUDA_DYNLOAD
  // Load NVRTC function pointers for adaptive kernel compilation
  if (DebugFlags().cuda.adaptive_compile && cuewInit(CUEW_INIT_NVRTC) != CUEW_SUCCESS) {
    VLOG(1)
        << "CUEW initialization failed for NVRTC. Adaptive kernel compilation won't be available.";
  }
#  endif

  const OptixResult result = optixInit();

  if (result == OPTIX_ERROR_UNSUPPORTED_ABI_VERSION) {
    VLOG(1)
        << "OptiX initialization failed because the installed driver does not support ABI version "
        << OPTIX_ABI_VERSION;
    return false;
  }
  else if (result != OPTIX_SUCCESS) {
    VLOG(1) << "OptiX initialization failed with error code " << (unsigned int)result;
    return false;
  }

  // Loaded OptiX successfully!
  return true;
}

void device_optix_info(vector<DeviceInfo> &devices)
{
  // Simply add all supported CUDA devices as OptiX devices again
  vector<DeviceInfo> cuda_devices;
  device_cuda_info(cuda_devices);

  for (auto it = cuda_devices.begin(); it != cuda_devices.end();) {
    DeviceInfo &info = *it;
    assert(info.type == DEVICE_CUDA);
    info.type = DEVICE_OPTIX;
    info.id += "_OptiX";

    // Figure out RTX support
    CUdevice cuda_device = 0;
    CUcontext cuda_context = NULL;
    unsigned int rtcore_version = 0;
    if (cuDeviceGet(&cuda_device, info.num) == CUDA_SUCCESS &&
        cuDevicePrimaryCtxRetain(&cuda_context, cuda_device) == CUDA_SUCCESS) {
      OptixDeviceContext optix_context = NULL;
      if (optixDeviceContextCreate(cuda_context, nullptr, &optix_context) == OPTIX_SUCCESS) {
        optixDeviceContextGetProperty(optix_context,
                                      OPTIX_DEVICE_PROPERTY_RTCORE_VERSION,
                                      &rtcore_version,
                                      sizeof(rtcore_version));
        optixDeviceContextDestroy(optix_context);
      }
      cuDevicePrimaryCtxRelease(cuda_device);
    }

    // Only add devices with RTX support
    if (rtcore_version == 0)
      it = cuda_devices.erase(it);
    else
      ++it;
  }

  devices.insert(devices.end(), cuda_devices.begin(), cuda_devices.end());
}

Device *device_optix_create(DeviceInfo &info, Stats &stats, Profiler &profiler, bool background)
{
  return new OptiXDevice(info, stats, profiler, background);
}

CCL_NAMESPACE_END

#endif
