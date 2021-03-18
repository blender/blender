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

#  include "bvh/bvh.h"
#  include "bvh/bvh_optix.h"
#  include "device/cuda/device_cuda.h"
#  include "device/device_denoising.h"
#  include "device/device_intern.h"
#  include "render/buffers.h"
#  include "render/hair.h"
#  include "render/mesh.h"
#  include "render/object.h"
#  include "render/scene.h"
#  include "util/util_debug.h"
#  include "util/util_logging.h"
#  include "util/util_md5.h"
#  include "util/util_path.h"
#  include "util/util_progress.h"
#  include "util/util_time.h"

#  ifdef WITH_CUDA_DYNLOAD
#    include <cuew.h>
// Do not use CUDA SDK headers when using CUEW
#    define OPTIX_DONT_INCLUDE_CUDA
#  endif
#  include <optix_function_table_definition.h>
#  include <optix_stubs.h>

// TODO(pmours): Disable this once drivers have native support
#  define OPTIX_DENOISER_NO_PIXEL_STRIDE 1

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
        set_error(string_printf("%s in %s (device_optix.cpp:%d)", name, #stmt, __LINE__)); \
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
        set_error(string_printf("%s in %s (device_optix.cpp:%d)", name, #stmt, __LINE__)); \
        return false; \
      } \
    } \
    (void)0

#  define check_result_optix(stmt) \
    { \
      enum OptixResult res = stmt; \
      if (res != OPTIX_SUCCESS) { \
        const char *name = optixGetErrorName(res); \
        set_error(string_printf("%s in %s (device_optix.cpp:%d)", name, #stmt, __LINE__)); \
        return; \
      } \
    } \
    (void)0
#  define check_result_optix_ret(stmt) \
    { \
      enum OptixResult res = stmt; \
      if (res != OPTIX_SUCCESS) { \
        const char *name = optixGetErrorName(res); \
        set_error(string_printf("%s in %s (device_optix.cpp:%d)", name, #stmt, __LINE__)); \
        return false; \
      } \
    } \
    (void)0

#  define launch_filter_kernel(func_name, w, h, args) \
    { \
      CUfunction func; \
      check_result_cuda_ret(cuModuleGetFunction(&func, cuFilterModule, func_name)); \
      check_result_cuda_ret(cuFuncSetCacheConfig(func, CU_FUNC_CACHE_PREFER_L1)); \
      int threads; \
      check_result_cuda_ret( \
          cuFuncGetAttribute(&threads, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, func)); \
      threads = (int)sqrt((float)threads); \
      int xblocks = ((w) + threads - 1) / threads; \
      int yblocks = ((h) + threads - 1) / threads; \
      check_result_cuda_ret( \
          cuLaunchKernel(func, xblocks, yblocks, 1, threads, threads, 1, 0, 0, args, 0)); \
    } \
    (void)0

class OptiXDevice : public CUDADevice {

  // List of OptiX program groups
  enum {
    PG_RGEN,
    PG_MISS,
    PG_HITD,  // Default hit group
    PG_HITS,  // __SHADOW_RECORD_ALL__ hit group
    PG_HITL,  // __BVH_LOCAL__ hit group (only used for triangles)
#  if OPTIX_ABI_VERSION >= 36
    PG_HITD_MOTION,
    PG_HITS_MOTION,
#  endif
    PG_BAKE,  // kernel_bake_evaluate
    PG_DISP,  // kernel_displace_evaluate
    PG_BACK,  // kernel_background_evaluate
    PG_CALL,
    NUM_PROGRAM_GROUPS = PG_CALL + 3
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
    bool use_mapped_host = false;
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

  vector<CUstream> cuda_stream;
  OptixDeviceContext context = NULL;

  OptixModule optix_module = NULL;  // All necessary OptiX kernels are in one module
  OptixModule builtin_modules[2] = {};
  OptixPipeline pipelines[NUM_PIPELINES] = {};

  bool motion_blur = false;
  device_vector<SbtRecord> sbt_data;
  device_only_memory<KernelParams> launch_params;
  OptixTraversableHandle tlas_handle = 0;

  OptixDenoiser denoiser = NULL;
  device_only_memory<unsigned char> denoiser_state;
  int denoiser_input_passes = 0;

 public:
  OptiXDevice(DeviceInfo &info_, Stats &stats_, Profiler &profiler_, bool background_)
      : CUDADevice(info_, stats_, profiler_, background_),
        sbt_data(this, "__sbt", MEM_READ_ONLY),
        launch_params(this, "__params", false),
        denoiser_state(this, "__denoiser_state", true)
  {
    // Store number of CUDA streams in device info
    info.cpu_threads = DebugFlags().optix.cuda_streams;

    // Make the CUDA context current
    if (!cuContext) {
      return;  // Do not initialize if CUDA context creation failed already
    }
    const CUDAContextScope scope(cuContext);

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
#  if OPTIX_ABI_VERSION >= 41 && defined(WITH_CYCLES_DEBUG)
    options.validationMode = OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_ALL;
#  endif
    check_result_optix(optixDeviceContextCreate(cuContext, &options, &context));
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
    task_pool.cancel();

    // Make CUDA context current
    const CUDAContextScope scope(cuContext);

    sbt_data.free();
    texture_info.free();
    launch_params.free();
    denoiser_state.free();

    // Unload modules
    if (optix_module != NULL)
      optixModuleDestroy(optix_module);
    for (unsigned int i = 0; i < 2; ++i)
      if (builtin_modules[i] != NULL)
        optixModuleDestroy(builtin_modules[i]);
    for (unsigned int i = 0; i < NUM_PIPELINES; ++i)
      if (pipelines[i] != NULL)
        optixPipelineDestroy(pipelines[i]);

    // Destroy launch streams
    for (CUstream stream : cuda_stream)
      cuStreamDestroy(stream);

    if (denoiser != NULL)
      optixDenoiserDestroy(denoiser);

    optixDeviceContextDestroy(context);
  }

 private:
  bool show_samples() const override
  {
    // Only show samples if not rendering multiple tiles in parallel
    return info.cpu_threads == 1;
  }

  BVHLayoutMask get_bvh_layout_mask() const override
  {
    // CUDA kernels are used when doing baking, so need to build a BVH those can understand too!
    if (optix_module == NULL)
      return CUDADevice::get_bvh_layout_mask();

    // OptiX has its own internal acceleration structure format
    return BVH_LAYOUT_OPTIX;
  }

  string compile_kernel_get_common_cflags(const DeviceRequestedFeatures &requested_features,
                                          bool filter,
                                          bool /*split*/) override
  {
    // Split kernel is not supported in OptiX
    string common_cflags = CUDADevice::compile_kernel_get_common_cflags(
        requested_features, filter, false);

    // Add OptiX SDK include directory to include paths
    const char *optix_sdk_path = getenv("OPTIX_ROOT_DIR");
    if (optix_sdk_path) {
      common_cflags += string_printf(" -I\"%s/include\"", optix_sdk_path);
    }

    // Specialization for shader raytracing
    if (requested_features.use_shader_raytrace) {
      common_cflags += " --keep-device-functions";
    }
    else {
      common_cflags += " -D __NO_SHADER_RAYTRACE__";
    }

    return common_cflags;
  }

  bool load_kernels(const DeviceRequestedFeatures &requested_features) override
  {
    if (have_error()) {
      // Abort early if context creation failed already
      return false;
    }

    // Load CUDA modules because we need some of the utility kernels
    if (!CUDADevice::load_kernels(requested_features)) {
      return false;
    }

    // Baking is currently performed using CUDA, so no need to load OptiX kernels
    if (requested_features.use_baking) {
      return true;
    }

    const CUDAContextScope scope(cuContext);

    // Unload existing OptiX module and pipelines first
    if (optix_module != NULL) {
      optixModuleDestroy(optix_module);
      optix_module = NULL;
    }
    for (unsigned int i = 0; i < 2; ++i) {
      if (builtin_modules[i] != NULL) {
        optixModuleDestroy(builtin_modules[i]);
        builtin_modules[i] = NULL;
      }
    }
    for (unsigned int i = 0; i < NUM_PIPELINES; ++i) {
      if (pipelines[i] != NULL) {
        optixPipelineDestroy(pipelines[i]);
        pipelines[i] = NULL;
      }
    }

    OptixModuleCompileOptions module_options;
    module_options.maxRegisterCount = 0;  // Do not set an explicit register limit
#  ifdef WITH_CYCLES_DEBUG
    module_options.optLevel = OPTIX_COMPILE_OPTIMIZATION_LEVEL_0;
    module_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
#  else
    module_options.optLevel = OPTIX_COMPILE_OPTIMIZATION_LEVEL_3;
    module_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_LINEINFO;
#  endif

#  if OPTIX_ABI_VERSION >= 41
    module_options.boundValues = nullptr;
    module_options.numBoundValues = 0;
#  endif

    OptixPipelineCompileOptions pipeline_options;
    // Default to no motion blur and two-level graph, since it is the fastest option
    pipeline_options.usesMotionBlur = false;
    pipeline_options.traversableGraphFlags =
        OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;
    pipeline_options.numPayloadValues = 6;
    pipeline_options.numAttributeValues = 2;  // u, v
    pipeline_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;
    pipeline_options.pipelineLaunchParamsVariableName = "__params";  // See kernel_globals.h

#  if OPTIX_ABI_VERSION >= 36
    pipeline_options.usesPrimitiveTypeFlags = OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE;
    if (requested_features.use_hair) {
      if (DebugFlags().optix.curves_api && requested_features.use_hair_thick) {
        pipeline_options.usesPrimitiveTypeFlags |= OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_CUBIC_BSPLINE;
      }
      else {
        pipeline_options.usesPrimitiveTypeFlags |= OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM;
      }
    }
#  endif

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
      string ptx_data, ptx_filename = path_get(requested_features.use_shader_raytrace ?
                                                   "lib/kernel_optix_shader_raytrace.ptx" :
                                                   "lib/kernel_optix.ptx");
      if (use_adaptive_compilation() || path_file_size(ptx_filename) == -1) {
        if (!getenv("OPTIX_ROOT_DIR")) {
          set_error(
              "Missing OPTIX_ROOT_DIR environment variable (which must be set with the path to "
              "the Optix SDK to be able to compile Optix kernels on demand).");
          return false;
        }
        ptx_filename = compile_kernel(requested_features, "kernel_optix", "optix", true);
      }
      if (ptx_filename.empty() || !path_read_text(ptx_filename, ptx_data)) {
        set_error("Failed to load OptiX kernel from '" + ptx_filename + "'");
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
      group_descs[PG_HITD].hitgroup.moduleIS = optix_module;
      group_descs[PG_HITS].hitgroup.moduleIS = optix_module;

      // Add curve intersection programs
      if (requested_features.use_hair_thick) {
        // Slower programs for thick hair since that also slows down ribbons.
        // Ideally this should not be needed.
        group_descs[PG_HITD].hitgroup.entryFunctionNameIS = "__intersection__curve_all";
        group_descs[PG_HITS].hitgroup.entryFunctionNameIS = "__intersection__curve_all";
      }
      else {
        group_descs[PG_HITD].hitgroup.entryFunctionNameIS = "__intersection__curve_ribbon";
        group_descs[PG_HITS].hitgroup.entryFunctionNameIS = "__intersection__curve_ribbon";
      }

#  if OPTIX_ABI_VERSION >= 36
      if (DebugFlags().optix.curves_api && requested_features.use_hair_thick) {
        OptixBuiltinISOptions builtin_options;
        builtin_options.builtinISModuleType = OPTIX_PRIMITIVE_TYPE_ROUND_CUBIC_BSPLINE;
        builtin_options.usesMotionBlur = false;

        check_result_optix_ret(optixBuiltinISModuleGet(
            context, &module_options, &pipeline_options, &builtin_options, &builtin_modules[0]));

        group_descs[PG_HITD].hitgroup.moduleIS = builtin_modules[0];
        group_descs[PG_HITD].hitgroup.entryFunctionNameIS = nullptr;
        group_descs[PG_HITS].hitgroup.moduleIS = builtin_modules[0];
        group_descs[PG_HITS].hitgroup.entryFunctionNameIS = nullptr;

        if (motion_blur) {
          builtin_options.usesMotionBlur = true;

          check_result_optix_ret(optixBuiltinISModuleGet(
              context, &module_options, &pipeline_options, &builtin_options, &builtin_modules[1]));

          group_descs[PG_HITD_MOTION] = group_descs[PG_HITD];
          group_descs[PG_HITD_MOTION].hitgroup.moduleIS = builtin_modules[1];
          group_descs[PG_HITS_MOTION] = group_descs[PG_HITS];
          group_descs[PG_HITS_MOTION].hitgroup.moduleIS = builtin_modules[1];
        }
      }
#  endif
    }

    if (requested_features.use_subsurface || requested_features.use_shader_raytrace) {
      // Add hit group for local intersections
      group_descs[PG_HITL].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
      group_descs[PG_HITL].hitgroup.moduleAH = optix_module;
      group_descs[PG_HITL].hitgroup.entryFunctionNameAH = "__anyhit__kernel_optix_local_hit";
    }

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

    // Shader raytracing replaces some functions with direct callables
    if (requested_features.use_shader_raytrace) {
      group_descs[PG_CALL + 0].kind = OPTIX_PROGRAM_GROUP_KIND_CALLABLES;
      group_descs[PG_CALL + 0].callables.moduleDC = optix_module;
      group_descs[PG_CALL + 0].callables.entryFunctionNameDC = "__direct_callable__svm_eval_nodes";
      group_descs[PG_CALL + 1].kind = OPTIX_PROGRAM_GROUP_KIND_CALLABLES;
      group_descs[PG_CALL + 1].callables.moduleDC = optix_module;
      group_descs[PG_CALL + 1].callables.entryFunctionNameDC =
          "__direct_callable__kernel_volume_shadow";
      group_descs[PG_CALL + 2].kind = OPTIX_PROGRAM_GROUP_KIND_CALLABLES;
      group_descs[PG_CALL + 2].callables.moduleDC = optix_module;
      group_descs[PG_CALL + 2].callables.entryFunctionNameDC =
          "__direct_callable__subsurface_scatter_multi_setup";
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
    trace_css = std::max(trace_css, stack_size[PG_HITD].cssIS + stack_size[PG_HITD].cssAH);
    trace_css = std::max(trace_css, stack_size[PG_HITS].cssIS + stack_size[PG_HITS].cssAH);
    trace_css = std::max(trace_css, stack_size[PG_HITL].cssIS + stack_size[PG_HITL].cssAH);
#  if OPTIX_ABI_VERSION >= 36
    trace_css = std::max(trace_css,
                         stack_size[PG_HITD_MOTION].cssIS + stack_size[PG_HITD_MOTION].cssAH);
    trace_css = std::max(trace_css,
                         stack_size[PG_HITS_MOTION].cssIS + stack_size[PG_HITS_MOTION].cssAH);
#  endif

    OptixPipelineLinkOptions link_options;
    link_options.maxTraceDepth = 1;
#  ifdef WITH_CYCLES_DEBUG
    link_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
#  else
    link_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_LINEINFO;
#  endif
#  if OPTIX_ABI_VERSION < 24
    link_options.overrideUsesMotionBlur = motion_blur;
#  endif

    {  // Create path tracing pipeline
      vector<OptixProgramGroup> pipeline_groups;
      pipeline_groups.reserve(NUM_PROGRAM_GROUPS);
      pipeline_groups.push_back(groups[PG_RGEN]);
      pipeline_groups.push_back(groups[PG_MISS]);
      pipeline_groups.push_back(groups[PG_HITD]);
      pipeline_groups.push_back(groups[PG_HITS]);
      pipeline_groups.push_back(groups[PG_HITL]);
#  if OPTIX_ABI_VERSION >= 36
      if (motion_blur) {
        pipeline_groups.push_back(groups[PG_HITD_MOTION]);
        pipeline_groups.push_back(groups[PG_HITS_MOTION]);
      }
#  endif
      if (requested_features.use_shader_raytrace) {
        pipeline_groups.push_back(groups[PG_CALL + 0]);
        pipeline_groups.push_back(groups[PG_CALL + 1]);
        pipeline_groups.push_back(groups[PG_CALL + 2]);
      }

      check_result_optix_ret(optixPipelineCreate(context,
                                                 &pipeline_options,
                                                 &link_options,
                                                 pipeline_groups.data(),
                                                 pipeline_groups.size(),
                                                 nullptr,
                                                 0,
                                                 &pipelines[PIP_PATH_TRACE]));

      // Combine ray generation and trace continuation stack size
      const unsigned int css = stack_size[PG_RGEN].cssRG + link_options.maxTraceDepth * trace_css;
      // Max direct callable depth is one of the following, so combine accordingly
      // - __raygen__ -> svm_eval_nodes
      // - __raygen__ -> kernel_volume_shadow -> svm_eval_nodes
      // - __raygen__ -> subsurface_scatter_multi_setup -> svm_eval_nodes
      const unsigned int dss = stack_size[PG_CALL + 0].dssDC +
                               std::max(stack_size[PG_CALL + 1].dssDC,
                                        stack_size[PG_CALL + 2].dssDC);

      // Set stack size depending on pipeline options
      check_result_optix_ret(
          optixPipelineSetStackSize(pipelines[PIP_PATH_TRACE],
                                    0,
                                    requested_features.use_shader_raytrace ? dss : 0,
                                    css,
                                    motion_blur ? 3 : 2));
    }

    // Only need to create shader evaluation pipeline if one of these features is used:
    const bool use_shader_eval_pipeline = requested_features.use_baking ||
                                          requested_features.use_background_light ||
                                          requested_features.use_true_displacement;

    if (use_shader_eval_pipeline) {  // Create shader evaluation pipeline
      vector<OptixProgramGroup> pipeline_groups;
      pipeline_groups.reserve(NUM_PROGRAM_GROUPS);
      pipeline_groups.push_back(groups[PG_BAKE]);
      pipeline_groups.push_back(groups[PG_DISP]);
      pipeline_groups.push_back(groups[PG_BACK]);
      pipeline_groups.push_back(groups[PG_MISS]);
      pipeline_groups.push_back(groups[PG_HITD]);
      pipeline_groups.push_back(groups[PG_HITS]);
      pipeline_groups.push_back(groups[PG_HITL]);
#  if OPTIX_ABI_VERSION >= 36
      if (motion_blur) {
        pipeline_groups.push_back(groups[PG_HITD_MOTION]);
        pipeline_groups.push_back(groups[PG_HITS_MOTION]);
      }
#  endif
      if (requested_features.use_shader_raytrace) {
        pipeline_groups.push_back(groups[PG_CALL + 0]);
        pipeline_groups.push_back(groups[PG_CALL + 1]);
        pipeline_groups.push_back(groups[PG_CALL + 2]);
      }

      check_result_optix_ret(optixPipelineCreate(context,
                                                 &pipeline_options,
                                                 &link_options,
                                                 pipeline_groups.data(),
                                                 pipeline_groups.size(),
                                                 nullptr,
                                                 0,
                                                 &pipelines[PIP_SHADER_EVAL]));

      // Calculate continuation stack size based on the maximum of all ray generation stack sizes
      const unsigned int css = std::max(stack_size[PG_BAKE].cssRG,
                                        std::max(stack_size[PG_DISP].cssRG,
                                                 stack_size[PG_BACK].cssRG)) +
                               link_options.maxTraceDepth * trace_css;
      const unsigned int dss = stack_size[PG_CALL + 0].dssDC +
                               std::max(stack_size[PG_CALL + 1].dssDC,
                                        stack_size[PG_CALL + 2].dssDC);

      check_result_optix_ret(
          optixPipelineSetStackSize(pipelines[PIP_SHADER_EVAL],
                                    0,
                                    requested_features.use_shader_raytrace ? dss : 0,
                                    css,
                                    motion_blur ? 3 : 2));
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
      if (thread_index != 0) {
        // Only execute denoising in a single thread (see also 'task_add')
        task.tile_types &= ~RenderTile::DENOISE;
      }

      RenderTile tile;
      while (task.acquire_tile(this, tile, task.tile_types)) {
        if (tile.task == RenderTile::PATH_TRACE)
          launch_render(task, tile, thread_index);
        else if (tile.task == RenderTile::BAKE) {
          // Perform baking using CUDA, since it is not currently implemented in OptiX
          device_vector<WorkTile> work_tiles(this, "work_tiles", MEM_READ_ONLY);
          CUDADevice::render(task, tile, work_tiles);
        }
        else if (tile.task == RenderTile::DENOISE)
          launch_denoise(task, tile);
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
    else if (task.type == DeviceTask::DENOISE_BUFFER) {
      // Set up a single tile that covers the whole task and denoise it
      RenderTile tile;
      tile.x = task.x;
      tile.y = task.y;
      tile.w = task.w;
      tile.h = task.h;
      tile.buffer = task.buffer;
      tile.num_samples = task.num_samples;
      tile.start_sample = task.sample;
      tile.offset = task.offset;
      tile.stride = task.stride;
      tile.buffers = task.buffers;

      launch_denoise(task, tile);
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
    int step_samples = (info.display_device ? 8 : 32);

    // Offset into launch params buffer so that streams use separate data
    device_ptr launch_params_ptr = launch_params.device_pointer +
                                   thread_index * launch_params.data_elements;

    const CUDAContextScope scope(cuContext);

    for (int sample = rtile.start_sample; sample < end_sample;) {
      // Copy work tile information to device
      wtile.start_sample = sample;
      wtile.num_samples = step_samples;
      if (task.adaptive_sampling.use) {
        wtile.num_samples = task.adaptive_sampling.align_samples(sample, step_samples);
      }
      wtile.num_samples = min(wtile.num_samples, end_sample - sample);
      device_ptr d_wtile_ptr = launch_params_ptr + offsetof(KernelParams, tile);
      check_result_cuda(
          cuMemcpyHtoDAsync(d_wtile_ptr, &wtile, sizeof(wtile), cuda_stream[thread_index]));

      OptixShaderBindingTable sbt_params = {};
      sbt_params.raygenRecord = sbt_data.device_pointer + PG_RGEN * sizeof(SbtRecord);
      sbt_params.missRecordBase = sbt_data.device_pointer + PG_MISS * sizeof(SbtRecord);
      sbt_params.missRecordStrideInBytes = sizeof(SbtRecord);
      sbt_params.missRecordCount = 1;
      sbt_params.hitgroupRecordBase = sbt_data.device_pointer + PG_HITD * sizeof(SbtRecord);
      sbt_params.hitgroupRecordStrideInBytes = sizeof(SbtRecord);
#  if OPTIX_ABI_VERSION >= 36
      sbt_params.hitgroupRecordCount = 5;  // PG_HITD(_MOTION), PG_HITS(_MOTION), PG_HITL
#  else
      sbt_params.hitgroupRecordCount = 3;  // PG_HITD, PG_HITS, PG_HITL
#  endif
      sbt_params.callablesRecordBase = sbt_data.device_pointer + PG_CALL * sizeof(SbtRecord);
      sbt_params.callablesRecordCount = 3;
      sbt_params.callablesRecordStrideInBytes = sizeof(SbtRecord);

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

      // Run the adaptive sampling kernels at selected samples aligned to step samples.
      uint filter_sample = wtile.start_sample + wtile.num_samples - 1;
      if (task.adaptive_sampling.use && task.adaptive_sampling.need_filter(filter_sample)) {
        adaptive_sampling_filter(filter_sample, &wtile, d_wtile_ptr, cuda_stream[thread_index]);
      }

      // Wait for launch to finish
      check_result_cuda(cuStreamSynchronize(cuda_stream[thread_index]));

      // Update current sample, so it is displayed correctly
      sample += wtile.num_samples;
      rtile.sample = sample;
      // Update task progress after the kernel completed rendering
      task.update_progress(&rtile, wtile.w * wtile.h * wtile.num_samples);

      if (task.get_cancel() && !task.need_finish_queue)
        return;  // Cancel rendering
    }

    // Finalize adaptive sampling
    if (task.adaptive_sampling.use) {
      device_ptr d_wtile_ptr = launch_params_ptr + offsetof(KernelParams, tile);
      adaptive_sampling_post(rtile, &wtile, d_wtile_ptr, cuda_stream[thread_index]);
      check_result_cuda(cuStreamSynchronize(cuda_stream[thread_index]));
      task.update_progress(&rtile, rtile.w * rtile.h * wtile.num_samples);
    }
  }

  bool launch_denoise(DeviceTask &task, RenderTile &rtile)
  {
    // Update current sample (for display and NLM denoising task)
    rtile.sample = rtile.start_sample + rtile.num_samples;

    // Make CUDA context current now, since it is used for both denoising tasks
    const CUDAContextScope scope(cuContext);

    // Choose between OptiX and NLM denoising
    if (task.denoising.type == DENOISER_OPTIX) {
      // Map neighboring tiles onto this device, indices are as following:
      // Where index 4 is the center tile and index 9 is the target for the result.
      //   0 1 2
      //   3 4 5
      //   6 7 8  9
      RenderTileNeighbors neighbors(rtile);
      task.map_neighbor_tiles(neighbors, this);
      RenderTile &center_tile = neighbors.tiles[RenderTileNeighbors::CENTER];
      RenderTile &target_tile = neighbors.target;
      rtile = center_tile;  // Tile may have been modified by mapping code

      // Calculate size of the tile to denoise (including overlap)
      int4 rect = center_tile.bounds();
      // Overlap between tiles has to be at least 64 pixels
      // TODO(pmours): Query this value from OptiX
      rect = rect_expand(rect, 64);
      int4 clip_rect = neighbors.bounds();
      rect = rect_clip(rect, clip_rect);
      int2 rect_size = make_int2(rect.z - rect.x, rect.w - rect.y);
      int2 overlap_offset = make_int2(rtile.x - rect.x, rtile.y - rect.y);

      // Calculate byte offsets and strides
      int pixel_stride = task.pass_stride * (int)sizeof(float);
      int pixel_offset = (rtile.offset + rtile.x + rtile.y * rtile.stride) * pixel_stride;
      const int pass_offset[3] = {
          (task.pass_denoising_data + DENOISING_PASS_COLOR) * (int)sizeof(float),
          (task.pass_denoising_data + DENOISING_PASS_ALBEDO) * (int)sizeof(float),
          (task.pass_denoising_data + DENOISING_PASS_NORMAL) * (int)sizeof(float)};

      // Start with the current tile pointer offset
      int input_stride = pixel_stride;
      device_ptr input_ptr = rtile.buffer + pixel_offset;

      // Copy tile data into a common buffer if necessary
      device_only_memory<float> input(this, "denoiser input", true);
      device_vector<TileInfo> tile_info_mem(this, "denoiser tile info", MEM_READ_ONLY);

      bool contiguous_memory = true;
      for (int i = 0; i < RenderTileNeighbors::SIZE; i++) {
        if (neighbors.tiles[i].buffer && neighbors.tiles[i].buffer != rtile.buffer) {
          contiguous_memory = false;
        }
      }

      if (contiguous_memory) {
        // Tiles are in continous memory, so can just subtract overlap offset
        input_ptr -= (overlap_offset.x + overlap_offset.y * rtile.stride) * pixel_stride;
        // Stride covers the whole width of the image and not just a single tile
        input_stride *= rtile.stride;
      }
      else {
        // Adjacent tiles are in separate memory regions, so need to copy them into a single one
        input.alloc_to_device(rect_size.x * rect_size.y * task.pass_stride);
        // Start with the new input buffer
        input_ptr = input.device_pointer;
        // Stride covers the width of the new input buffer, which includes tile width and overlap
        input_stride *= rect_size.x;

        TileInfo *tile_info = tile_info_mem.alloc(1);
        for (int i = 0; i < RenderTileNeighbors::SIZE; i++) {
          tile_info->offsets[i] = neighbors.tiles[i].offset;
          tile_info->strides[i] = neighbors.tiles[i].stride;
          tile_info->buffers[i] = neighbors.tiles[i].buffer;
        }
        tile_info->x[0] = neighbors.tiles[3].x;
        tile_info->x[1] = neighbors.tiles[4].x;
        tile_info->x[2] = neighbors.tiles[5].x;
        tile_info->x[3] = neighbors.tiles[5].x + neighbors.tiles[5].w;
        tile_info->y[0] = neighbors.tiles[1].y;
        tile_info->y[1] = neighbors.tiles[4].y;
        tile_info->y[2] = neighbors.tiles[7].y;
        tile_info->y[3] = neighbors.tiles[7].y + neighbors.tiles[7].h;
        tile_info_mem.copy_to_device();

        void *args[] = {
            &input.device_pointer, &tile_info_mem.device_pointer, &rect.x, &task.pass_stride};
        launch_filter_kernel("kernel_cuda_filter_copy_input", rect_size.x, rect_size.y, args);
      }

#  if OPTIX_DENOISER_NO_PIXEL_STRIDE
      device_only_memory<float> input_rgb(this, "denoiser input rgb", true);
      input_rgb.alloc_to_device(rect_size.x * rect_size.y * 3 * task.denoising.input_passes);

      void *input_args[] = {&input_rgb.device_pointer,
                            &input_ptr,
                            &rect_size.x,
                            &rect_size.y,
                            &input_stride,
                            &task.pass_stride,
                            const_cast<int *>(pass_offset),
                            &task.denoising.input_passes,
                            &rtile.sample};
      launch_filter_kernel(
          "kernel_cuda_filter_convert_to_rgb", rect_size.x, rect_size.y, input_args);

      input_ptr = input_rgb.device_pointer;
      pixel_stride = 3 * sizeof(float);
      input_stride = rect_size.x * pixel_stride;
#  endif

      const bool recreate_denoiser = (denoiser == NULL) ||
                                     (task.denoising.input_passes != denoiser_input_passes);
      if (recreate_denoiser) {
        // Destroy existing handle before creating new one
        if (denoiser != NULL) {
          optixDenoiserDestroy(denoiser);
        }

        // Create OptiX denoiser handle on demand when it is first used
        OptixDenoiserOptions denoiser_options;
        assert(task.denoising.input_passes >= 1 && task.denoising.input_passes <= 3);
        denoiser_options.inputKind = static_cast<OptixDenoiserInputKind>(
            OPTIX_DENOISER_INPUT_RGB + (task.denoising.input_passes - 1));
#  if OPTIX_ABI_VERSION < 28
        denoiser_options.pixelFormat = OPTIX_PIXEL_FORMAT_FLOAT3;
#  endif
        check_result_optix_ret(optixDenoiserCreate(context, &denoiser_options, &denoiser));
        check_result_optix_ret(
            optixDenoiserSetModel(denoiser, OPTIX_DENOISER_MODEL_KIND_HDR, NULL, 0));

        // OptiX denoiser handle was created with the requested number of input passes
        denoiser_input_passes = task.denoising.input_passes;
      }

      OptixDenoiserSizes sizes = {};
      check_result_optix_ret(
          optixDenoiserComputeMemoryResources(denoiser, rect_size.x, rect_size.y, &sizes));

#  if OPTIX_ABI_VERSION < 28
      const size_t scratch_size = sizes.recommendedScratchSizeInBytes;
#  else
      const size_t scratch_size = sizes.withOverlapScratchSizeInBytes;
#  endif
      const size_t scratch_offset = sizes.stateSizeInBytes;

      // Allocate denoiser state if tile size has changed since last setup
      if (recreate_denoiser || (denoiser_state.data_width != rect_size.x ||
                                denoiser_state.data_height != rect_size.y)) {
        denoiser_state.alloc_to_device(scratch_offset + scratch_size);

        // Initialize denoiser state for the current tile size
        check_result_optix_ret(optixDenoiserSetup(denoiser,
                                                  0,
                                                  rect_size.x,
                                                  rect_size.y,
                                                  denoiser_state.device_pointer,
                                                  scratch_offset,
                                                  denoiser_state.device_pointer + scratch_offset,
                                                  scratch_size));

        denoiser_state.data_width = rect_size.x;
        denoiser_state.data_height = rect_size.y;
      }

      // Set up input and output layer information
      OptixImage2D input_layers[3] = {};
      OptixImage2D output_layers[1] = {};

      for (int i = 0; i < 3; ++i) {
#  if OPTIX_DENOISER_NO_PIXEL_STRIDE
        input_layers[i].data = input_ptr + (rect_size.x * rect_size.y * pixel_stride * i);
#  else
        input_layers[i].data = input_ptr + pass_offset[i];
#  endif
        input_layers[i].width = rect_size.x;
        input_layers[i].height = rect_size.y;
        input_layers[i].rowStrideInBytes = input_stride;
        input_layers[i].pixelStrideInBytes = pixel_stride;
        input_layers[i].format = OPTIX_PIXEL_FORMAT_FLOAT3;
      }

#  if OPTIX_DENOISER_NO_PIXEL_STRIDE
      output_layers[0].data = input_ptr;
      output_layers[0].width = rect_size.x;
      output_layers[0].height = rect_size.y;
      output_layers[0].rowStrideInBytes = input_stride;
      output_layers[0].pixelStrideInBytes = pixel_stride;
      int2 output_offset = overlap_offset;
      overlap_offset = make_int2(0, 0);  // Not supported by denoiser API, so apply manually
#  else
      output_layers[0].data = target_tile.buffer + pixel_offset;
      output_layers[0].width = target_tile.w;
      output_layers[0].height = target_tile.h;
      output_layers[0].rowStrideInBytes = target_tile.stride * pixel_stride;
      output_layers[0].pixelStrideInBytes = pixel_stride;
#  endif
      output_layers[0].format = OPTIX_PIXEL_FORMAT_FLOAT3;

      // Finally run denonising
      OptixDenoiserParams params = {};  // All parameters are disabled/zero
      check_result_optix_ret(optixDenoiserInvoke(denoiser,
                                                 0,
                                                 &params,
                                                 denoiser_state.device_pointer,
                                                 scratch_offset,
                                                 input_layers,
                                                 task.denoising.input_passes,
                                                 overlap_offset.x,
                                                 overlap_offset.y,
                                                 output_layers,
                                                 denoiser_state.device_pointer + scratch_offset,
                                                 scratch_size));

#  if OPTIX_DENOISER_NO_PIXEL_STRIDE
      void *output_args[] = {&input_ptr,
                             &target_tile.buffer,
                             &output_offset.x,
                             &output_offset.y,
                             &rect_size.x,
                             &rect_size.y,
                             &target_tile.x,
                             &target_tile.y,
                             &target_tile.w,
                             &target_tile.h,
                             &target_tile.offset,
                             &target_tile.stride,
                             &task.pass_stride,
                             &rtile.sample};
      launch_filter_kernel(
          "kernel_cuda_filter_convert_from_rgb", target_tile.w, target_tile.h, output_args);
#  endif

      check_result_cuda_ret(cuStreamSynchronize(0));

      task.unmap_neighbor_tiles(neighbors, this);
    }
    else {
      // Run CUDA denoising kernels
      DenoisingTask denoising(this, task);
      CUDADevice::denoise(rtile, denoising);
    }

    // Update task progress after the denoiser completed processing
    task.update_progress(&rtile, rtile.w * rtile.h);

    return true;
  }

  void launch_shader_eval(DeviceTask &task, int thread_index)
  {
    unsigned int rgen_index = PG_BACK;
    if (task.shader_eval_type >= SHADER_EVAL_BAKE)
      rgen_index = PG_BAKE;
    if (task.shader_eval_type == SHADER_EVAL_DISPLACE)
      rgen_index = PG_DISP;

    const CUDAContextScope scope(cuContext);

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
      sbt_params.missRecordBase = sbt_data.device_pointer + PG_MISS * sizeof(SbtRecord);
      sbt_params.missRecordStrideInBytes = sizeof(SbtRecord);
      sbt_params.missRecordCount = 1;
      sbt_params.hitgroupRecordBase = sbt_data.device_pointer + PG_HITD * sizeof(SbtRecord);
      sbt_params.hitgroupRecordStrideInBytes = sizeof(SbtRecord);
#  if OPTIX_ABI_VERSION >= 36
      sbt_params.hitgroupRecordCount = 5;  // PG_HITD(_MOTION), PG_HITS(_MOTION), PG_HITL
#  else
      sbt_params.hitgroupRecordCount = 3;  // PG_HITD, PG_HITS, PG_HITL
#  endif
      sbt_params.callablesRecordBase = sbt_data.device_pointer + PG_CALL * sizeof(SbtRecord);
      sbt_params.callablesRecordCount = 3;
      sbt_params.callablesRecordStrideInBytes = sizeof(SbtRecord);

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

  bool build_optix_bvh(BVHOptiX *bvh,
                       OptixBuildOperation operation,
                       const OptixBuildInput &build_input,
                       uint16_t num_motion_steps)
  {
    /* Allocate and build acceleration structures only one at a time, to prevent parallel builds
     * from running out of memory (since both original and compacted acceleration structure memory
     * may be allocated at the same time for the duration of this function). The builds would
     * otherwise happen on the same CUDA stream anyway. */
    static thread_mutex mutex;
    thread_scoped_lock lock(mutex);

    const CUDAContextScope scope(cuContext);

    // Compute memory usage
    OptixAccelBufferSizes sizes = {};
    OptixAccelBuildOptions options;
    options.operation = operation;
    if (background) {
      // Prefer best performance and lowest memory consumption in background
      options.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE | OPTIX_BUILD_FLAG_ALLOW_COMPACTION;
    }
    else {
      // Prefer fast updates in viewport
      options.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_BUILD | OPTIX_BUILD_FLAG_ALLOW_UPDATE;
    }

    options.motionOptions.numKeys = num_motion_steps;
    options.motionOptions.flags = OPTIX_MOTION_FLAG_START_VANISH | OPTIX_MOTION_FLAG_END_VANISH;
    options.motionOptions.timeBegin = 0.0f;
    options.motionOptions.timeEnd = 1.0f;

    check_result_optix_ret(
        optixAccelComputeMemoryUsage(context, &options, &build_input, 1, &sizes));

    // Allocate required output buffers
    device_only_memory<char> temp_mem(this, "optix temp as build mem", true);
    temp_mem.alloc_to_device(align_up(sizes.tempSizeInBytes, 8) + 8);
    if (!temp_mem.device_pointer)
      return false;  // Make sure temporary memory allocation succeeded

    // Acceleration structure memory has to be allocated on the device (not allowed to be on host)
    device_only_memory<char> &out_data = bvh->as_data;
    if (operation == OPTIX_BUILD_OPERATION_BUILD) {
      assert(out_data.device == this);
      out_data.alloc_to_device(sizes.outputSizeInBytes);
      if (!out_data.device_pointer)
        return false;
    }
    else {
      assert(out_data.device_pointer && out_data.device_size >= sizes.outputSizeInBytes);
    }

    // Finally build the acceleration structure
    OptixAccelEmitDesc compacted_size_prop;
    compacted_size_prop.type = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;
    // A tiny space was allocated for this property at the end of the temporary buffer above
    // Make sure this pointer is 8-byte aligned
    compacted_size_prop.result = align_up(temp_mem.device_pointer + sizes.tempSizeInBytes, 8);

    OptixTraversableHandle out_handle = 0;
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
                                           background ? &compacted_size_prop : NULL,
                                           background ? 1 : 0));
    bvh->traversable_handle = static_cast<uint64_t>(out_handle);

    // Wait for all operations to finish
    check_result_cuda_ret(cuStreamSynchronize(NULL));

    // Compact acceleration structure to save memory (do not do this in viewport for faster builds)
    if (background) {
      uint64_t compacted_size = sizes.outputSizeInBytes;
      check_result_cuda_ret(
          cuMemcpyDtoH(&compacted_size, compacted_size_prop.result, sizeof(compacted_size)));

      // Temporary memory is no longer needed, so free it now to make space
      temp_mem.free();

      // There is no point compacting if the size does not change
      if (compacted_size < sizes.outputSizeInBytes) {
        device_only_memory<char> compacted_data(this, "optix compacted as", false);
        compacted_data.alloc_to_device(compacted_size);
        if (!compacted_data.device_pointer)
          // Do not compact if memory allocation for compacted acceleration structure fails
          // Can just use the uncompacted one then, so succeed here regardless
          return true;

        check_result_optix_ret(optixAccelCompact(context,
                                                 NULL,
                                                 out_handle,
                                                 compacted_data.device_pointer,
                                                 compacted_size,
                                                 &out_handle));
        bvh->traversable_handle = static_cast<uint64_t>(out_handle);

        // Wait for compaction to finish
        check_result_cuda_ret(cuStreamSynchronize(NULL));

        std::swap(out_data.device_size, compacted_data.device_size);
        std::swap(out_data.device_pointer, compacted_data.device_pointer);
        // Original acceleration structure memory is freed when 'compacted_data' goes out of scope
      }
    }

    return true;
  }

  void build_bvh(BVH *bvh, Progress &progress, bool refit) override
  {
    if (bvh->params.bvh_layout == BVH_LAYOUT_BVH2) {
      /* For baking CUDA is used, build appropriate BVH for that. */
      Device::build_bvh(bvh, progress, refit);
      return;
    }

    BVHOptiX *const bvh_optix = static_cast<BVHOptiX *>(bvh);

    progress.set_substatus("Building OptiX acceleration structure");

    if (!bvh->params.top_level) {
      assert(bvh->objects.size() == 1 && bvh->geometry.size() == 1);

      // Refit is only possible in viewport for now (because AS is built with
      // OPTIX_BUILD_FLAG_ALLOW_UPDATE only there, see above)
      OptixBuildOperation operation = OPTIX_BUILD_OPERATION_BUILD;
      if (refit && !background) {
        assert(bvh_optix->traversable_handle != 0);
        operation = OPTIX_BUILD_OPERATION_UPDATE;
      }
      else {
        bvh_optix->as_data.free();
        bvh_optix->traversable_handle = 0;
      }

      // Build bottom level acceleration structures (BLAS)
      Geometry *const geom = bvh->geometry[0];
      if (geom->geometry_type == Geometry::HAIR) {
        // Build BLAS for curve primitives
        Hair *const hair = static_cast<Hair *const>(geom);
        if (hair->num_curves() == 0) {
          return;
        }

        const size_t num_segments = hair->num_segments();

        size_t num_motion_steps = 1;
        Attribute *motion_keys = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
        if (motion_blur && hair->get_use_motion_blur() && motion_keys) {
          num_motion_steps = hair->get_motion_steps();
        }

        device_vector<OptixAabb> aabb_data(this, "optix temp aabb data", MEM_READ_ONLY);
#  if OPTIX_ABI_VERSION >= 36
        device_vector<int> index_data(this, "optix temp index data", MEM_READ_ONLY);
        device_vector<float4> vertex_data(this, "optix temp vertex data", MEM_READ_ONLY);
        // Four control points for each curve segment
        const size_t num_vertices = num_segments * 4;
        if (DebugFlags().optix.curves_api && hair->curve_shape == CURVE_THICK) {
          index_data.alloc(num_segments);
          vertex_data.alloc(num_vertices * num_motion_steps);
        }
        else
#  endif
          aabb_data.alloc(num_segments * num_motion_steps);

        // Get AABBs for each motion step
        for (size_t step = 0; step < num_motion_steps; ++step) {
          // The center step for motion vertices is not stored in the attribute
          const float3 *keys = hair->get_curve_keys().data();
          size_t center_step = (num_motion_steps - 1) / 2;
          if (step != center_step) {
            size_t attr_offset = (step > center_step) ? step - 1 : step;
            // Technically this is a float4 array, but sizeof(float3) == sizeof(float4)
            keys = motion_keys->data_float3() + attr_offset * hair->get_curve_keys().size();
          }

          for (size_t j = 0, i = 0; j < hair->num_curves(); ++j) {
            const Hair::Curve curve = hair->get_curve(j);
#  if OPTIX_ABI_VERSION >= 36
            const array<float> &curve_radius = hair->get_curve_radius();
#  endif

            for (int segment = 0; segment < curve.num_segments(); ++segment, ++i) {
#  if OPTIX_ABI_VERSION >= 36
              if (DebugFlags().optix.curves_api && hair->curve_shape == CURVE_THICK) {
                int k0 = curve.first_key + segment;
                int k1 = k0 + 1;
                int ka = max(k0 - 1, curve.first_key);
                int kb = min(k1 + 1, curve.first_key + curve.num_keys - 1);

                const float4 px = make_float4(keys[ka].x, keys[k0].x, keys[k1].x, keys[kb].x);
                const float4 py = make_float4(keys[ka].y, keys[k0].y, keys[k1].y, keys[kb].y);
                const float4 pz = make_float4(keys[ka].z, keys[k0].z, keys[k1].z, keys[kb].z);
                const float4 pw = make_float4(
                    curve_radius[ka], curve_radius[k0], curve_radius[k1], curve_radius[kb]);

                // Convert Catmull-Rom data to Bezier spline
                static const float4 cr2bsp0 = make_float4(+7, -4, +5, -2) / 6.f;
                static const float4 cr2bsp1 = make_float4(-2, 11, -4, +1) / 6.f;
                static const float4 cr2bsp2 = make_float4(+1, -4, 11, -2) / 6.f;
                static const float4 cr2bsp3 = make_float4(-2, +5, -4, +7) / 6.f;

                index_data[i] = i * 4;
                float4 *const v = vertex_data.data() + step * num_vertices + index_data[i];
                v[0] = make_float4(
                    dot(cr2bsp0, px), dot(cr2bsp0, py), dot(cr2bsp0, pz), dot(cr2bsp0, pw));
                v[1] = make_float4(
                    dot(cr2bsp1, px), dot(cr2bsp1, py), dot(cr2bsp1, pz), dot(cr2bsp1, pw));
                v[2] = make_float4(
                    dot(cr2bsp2, px), dot(cr2bsp2, py), dot(cr2bsp2, pz), dot(cr2bsp2, pw));
                v[3] = make_float4(
                    dot(cr2bsp3, px), dot(cr2bsp3, py), dot(cr2bsp3, pz), dot(cr2bsp3, pw));
              }
              else
#  endif
              {
                BoundBox bounds = BoundBox::empty;
                curve.bounds_grow(segment, keys, hair->get_curve_radius().data(), bounds);

                const size_t index = step * num_segments + i;
                aabb_data[index].minX = bounds.min.x;
                aabb_data[index].minY = bounds.min.y;
                aabb_data[index].minZ = bounds.min.z;
                aabb_data[index].maxX = bounds.max.x;
                aabb_data[index].maxY = bounds.max.y;
                aabb_data[index].maxZ = bounds.max.z;
              }
            }
          }
        }

        // Upload AABB data to GPU
        aabb_data.copy_to_device();
#  if OPTIX_ABI_VERSION >= 36
        index_data.copy_to_device();
        vertex_data.copy_to_device();
#  endif

        vector<device_ptr> aabb_ptrs;
        aabb_ptrs.reserve(num_motion_steps);
#  if OPTIX_ABI_VERSION >= 36
        vector<device_ptr> width_ptrs;
        vector<device_ptr> vertex_ptrs;
        width_ptrs.reserve(num_motion_steps);
        vertex_ptrs.reserve(num_motion_steps);
#  endif
        for (size_t step = 0; step < num_motion_steps; ++step) {
          aabb_ptrs.push_back(aabb_data.device_pointer + step * num_segments * sizeof(OptixAabb));
#  if OPTIX_ABI_VERSION >= 36
          const device_ptr base_ptr = vertex_data.device_pointer +
                                      step * num_vertices * sizeof(float4);
          width_ptrs.push_back(base_ptr + 3 * sizeof(float));  // Offset by vertex size
          vertex_ptrs.push_back(base_ptr);
#  endif
        }

        // Force a single any-hit call, so shadow record-all behavior works correctly
        unsigned int build_flags = OPTIX_GEOMETRY_FLAG_REQUIRE_SINGLE_ANYHIT_CALL;
        OptixBuildInput build_input = {};
#  if OPTIX_ABI_VERSION >= 36
        if (DebugFlags().optix.curves_api && hair->curve_shape == CURVE_THICK) {
          build_input.type = OPTIX_BUILD_INPUT_TYPE_CURVES;
          build_input.curveArray.curveType = OPTIX_PRIMITIVE_TYPE_ROUND_CUBIC_BSPLINE;
          build_input.curveArray.numPrimitives = num_segments;
          build_input.curveArray.vertexBuffers = (CUdeviceptr *)vertex_ptrs.data();
          build_input.curveArray.numVertices = num_vertices;
          build_input.curveArray.vertexStrideInBytes = sizeof(float4);
          build_input.curveArray.widthBuffers = (CUdeviceptr *)width_ptrs.data();
          build_input.curveArray.widthStrideInBytes = sizeof(float4);
          build_input.curveArray.indexBuffer = (CUdeviceptr)index_data.device_pointer;
          build_input.curveArray.indexStrideInBytes = sizeof(int);
          build_input.curveArray.flag = build_flags;
          build_input.curveArray.primitiveIndexOffset = hair->optix_prim_offset;
        }
        else
#  endif
        {
          // Disable visibility test any-hit program, since it is already checked during
          // intersection. Those trace calls that require anyhit can force it with a ray flag.
          build_flags |= OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT;

          build_input.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
#  if OPTIX_ABI_VERSION < 23
          build_input.aabbArray.aabbBuffers = (CUdeviceptr *)aabb_ptrs.data();
          build_input.aabbArray.numPrimitives = num_segments;
          build_input.aabbArray.strideInBytes = sizeof(OptixAabb);
          build_input.aabbArray.flags = &build_flags;
          build_input.aabbArray.numSbtRecords = 1;
          build_input.aabbArray.primitiveIndexOffset = hair->optix_prim_offset;
#  else
          build_input.customPrimitiveArray.aabbBuffers = (CUdeviceptr *)aabb_ptrs.data();
          build_input.customPrimitiveArray.numPrimitives = num_segments;
          build_input.customPrimitiveArray.strideInBytes = sizeof(OptixAabb);
          build_input.customPrimitiveArray.flags = &build_flags;
          build_input.customPrimitiveArray.numSbtRecords = 1;
          build_input.customPrimitiveArray.primitiveIndexOffset = hair->optix_prim_offset;
#  endif
        }

        if (!build_optix_bvh(bvh_optix, operation, build_input, num_motion_steps)) {
          progress.set_error("Failed to build OptiX acceleration structure");
        }
      }
      else if (geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::VOLUME) {
        // Build BLAS for triangle primitives
        Mesh *const mesh = static_cast<Mesh *const>(geom);
        if (mesh->num_triangles() == 0) {
          return;
        }

        const size_t num_verts = mesh->get_verts().size();

        size_t num_motion_steps = 1;
        Attribute *motion_keys = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
        if (motion_blur && mesh->get_use_motion_blur() && motion_keys) {
          num_motion_steps = mesh->get_motion_steps();
        }

        device_vector<int> index_data(this, "optix temp index data", MEM_READ_ONLY);
        index_data.alloc(mesh->get_triangles().size());
        memcpy(index_data.data(),
               mesh->get_triangles().data(),
               mesh->get_triangles().size() * sizeof(int));
        device_vector<float3> vertex_data(this, "optix temp vertex data", MEM_READ_ONLY);
        vertex_data.alloc(num_verts * num_motion_steps);

        for (size_t step = 0; step < num_motion_steps; ++step) {
          const float3 *verts = mesh->get_verts().data();

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

        // Force a single any-hit call, so shadow record-all behavior works correctly
        unsigned int build_flags = OPTIX_GEOMETRY_FLAG_REQUIRE_SINGLE_ANYHIT_CALL;
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
        build_input.triangleArray.primitiveIndexOffset = mesh->optix_prim_offset;

        if (!build_optix_bvh(bvh_optix, operation, build_input, num_motion_steps)) {
          progress.set_error("Failed to build OptiX acceleration structure");
        }
      }
    }
    else {
      unsigned int num_instances = 0;
      unsigned int max_num_instances = 0xFFFFFFFF;

      bvh_optix->as_data.free();
      bvh_optix->traversable_handle = 0;
      bvh_optix->motion_transform_data.free();

      optixDeviceContextGetProperty(context,
                                    OPTIX_DEVICE_PROPERTY_LIMIT_MAX_INSTANCE_ID,
                                    &max_num_instances,
                                    sizeof(max_num_instances));
      // Do not count first bit, which is used to distinguish instanced and non-instanced objects
      max_num_instances >>= 1;
      if (bvh->objects.size() > max_num_instances) {
        progress.set_error(
            "Failed to build OptiX acceleration structure because there are too many instances");
        return;
      }

      // Fill instance descriptions
#  if OPTIX_ABI_VERSION < 41
      device_vector<OptixAabb> aabbs(this, "optix tlas aabbs", MEM_READ_ONLY);
      aabbs.alloc(bvh->objects.size());
#  endif
      device_vector<OptixInstance> instances(this, "optix tlas instances", MEM_READ_ONLY);
      instances.alloc(bvh->objects.size());

      // Calculate total motion transform size and allocate memory for them
      size_t motion_transform_offset = 0;
      if (motion_blur) {
        size_t total_motion_transform_size = 0;
        for (Object *const ob : bvh->objects) {
          if (ob->is_traceable() && ob->use_motion()) {
            total_motion_transform_size = align_up(total_motion_transform_size,
                                                   OPTIX_TRANSFORM_BYTE_ALIGNMENT);
            const size_t motion_keys = max(ob->get_motion().size(), 2) - 2;
            total_motion_transform_size = total_motion_transform_size +
                                          sizeof(OptixSRTMotionTransform) +
                                          motion_keys * sizeof(OptixSRTData);
          }
        }

        assert(bvh_optix->motion_transform_data.device == this);
        bvh_optix->motion_transform_data.alloc_to_device(total_motion_transform_size);
      }

      for (Object *ob : bvh->objects) {
        // Skip non-traceable objects
        if (!ob->is_traceable())
          continue;

        BVHOptiX *const blas = static_cast<BVHOptiX *>(ob->get_geometry()->bvh);
        OptixTraversableHandle handle = blas->traversable_handle;

#  if OPTIX_ABI_VERSION < 41
        OptixAabb &aabb = aabbs[num_instances];
        aabb.minX = ob->bounds.min.x;
        aabb.minY = ob->bounds.min.y;
        aabb.minZ = ob->bounds.min.z;
        aabb.maxX = ob->bounds.max.x;
        aabb.maxY = ob->bounds.max.y;
        aabb.maxZ = ob->bounds.max.z;
#  endif

        OptixInstance &instance = instances[num_instances++];
        memset(&instance, 0, sizeof(instance));

        // Clear transform to identity matrix
        instance.transform[0] = 1.0f;
        instance.transform[5] = 1.0f;
        instance.transform[10] = 1.0f;

        // Set user instance ID to object index (but leave low bit blank)
        instance.instanceId = ob->get_device_index() << 1;

        // Have to have at least one bit in the mask, or else instance would always be culled
        instance.visibilityMask = 1;

        if (ob->get_geometry()->has_volume) {
          // Volumes have a special bit set in the visibility mask so a trace can mask only volumes
          instance.visibilityMask |= 2;
        }

        if (ob->get_geometry()->geometry_type == Geometry::HAIR) {
          // Same applies to curves (so they can be skipped in local trace calls)
          instance.visibilityMask |= 4;

#  if OPTIX_ABI_VERSION >= 36
          if (motion_blur && ob->get_geometry()->has_motion_blur() &&
              DebugFlags().optix.curves_api &&
              static_cast<const Hair *>(ob->get_geometry())->curve_shape == CURVE_THICK) {
            // Select between motion blur and non-motion blur built-in intersection module
            instance.sbtOffset = PG_HITD_MOTION - PG_HITD;
          }
#  endif
        }

        // Insert motion traversable if object has motion
        if (motion_blur && ob->use_motion()) {
          size_t motion_keys = max(ob->get_motion().size(), 2) - 2;
          size_t motion_transform_size = sizeof(OptixSRTMotionTransform) +
                                         motion_keys * sizeof(OptixSRTData);

          const CUDAContextScope scope(cuContext);

          motion_transform_offset = align_up(motion_transform_offset,
                                             OPTIX_TRANSFORM_BYTE_ALIGNMENT);
          CUdeviceptr motion_transform_gpu = bvh_optix->motion_transform_data.device_pointer +
                                             motion_transform_offset;
          motion_transform_offset += motion_transform_size;

          // Allocate host side memory for motion transform and fill it with transform data
          OptixSRTMotionTransform &motion_transform = *reinterpret_cast<OptixSRTMotionTransform *>(
              new uint8_t[motion_transform_size]);
          motion_transform.child = handle;
          motion_transform.motionOptions.numKeys = ob->get_motion().size();
          motion_transform.motionOptions.flags = OPTIX_MOTION_FLAG_NONE;
          motion_transform.motionOptions.timeBegin = 0.0f;
          motion_transform.motionOptions.timeEnd = 1.0f;

          OptixSRTData *const srt_data = motion_transform.srtData;
          array<DecomposedTransform> decomp(ob->get_motion().size());
          transform_motion_decompose(
              decomp.data(), ob->get_motion().data(), ob->get_motion().size());

          for (size_t i = 0; i < ob->get_motion().size(); ++i) {
            // Scale
            srt_data[i].sx = decomp[i].y.w;  // scale.x.x
            srt_data[i].sy = decomp[i].z.w;  // scale.y.y
            srt_data[i].sz = decomp[i].w.w;  // scale.z.z

            // Shear
            srt_data[i].a = decomp[i].z.x;  // scale.x.y
            srt_data[i].b = decomp[i].z.y;  // scale.x.z
            srt_data[i].c = decomp[i].w.x;  // scale.y.z
            assert(decomp[i].z.z == 0.0f);  // scale.y.x
            assert(decomp[i].w.y == 0.0f);  // scale.z.x
            assert(decomp[i].w.z == 0.0f);  // scale.z.y

            // Pivot point
            srt_data[i].pvx = 0.0f;
            srt_data[i].pvy = 0.0f;
            srt_data[i].pvz = 0.0f;

            // Rotation
            srt_data[i].qx = decomp[i].x.x;
            srt_data[i].qy = decomp[i].x.y;
            srt_data[i].qz = decomp[i].x.z;
            srt_data[i].qw = decomp[i].x.w;

            // Translation
            srt_data[i].tx = decomp[i].y.x;
            srt_data[i].ty = decomp[i].y.y;
            srt_data[i].tz = decomp[i].y.z;
          }

          // Upload motion transform to GPU
          cuMemcpyHtoD(motion_transform_gpu, &motion_transform, motion_transform_size);
          delete[] reinterpret_cast<uint8_t *>(&motion_transform);

          // Disable instance transform if object uses motion transform already
          instance.flags = OPTIX_INSTANCE_FLAG_DISABLE_TRANSFORM;

          // Get traversable handle to motion transform
          optixConvertPointerToTraversableHandle(context,
                                                 motion_transform_gpu,
                                                 OPTIX_TRAVERSABLE_TYPE_SRT_MOTION_TRANSFORM,
                                                 &instance.traversableHandle);
        }
        else {
          instance.traversableHandle = handle;

          if (ob->get_geometry()->is_instanced()) {
            // Set transform matrix
            memcpy(instance.transform, &ob->get_tfm(), sizeof(instance.transform));
          }
          else {
            // Disable instance transform if geometry already has it applied to vertex data
            instance.flags = OPTIX_INSTANCE_FLAG_DISABLE_TRANSFORM;
            // Non-instanced objects read ID from 'prim_object', so distinguish
            // them from instanced objects with the low bit set
            instance.instanceId |= 1;
          }
        }
      }

      // Upload instance descriptions
#  if OPTIX_ABI_VERSION < 41
      aabbs.resize(num_instances);
      aabbs.copy_to_device();
#  endif
      instances.resize(num_instances);
      instances.copy_to_device();

      // Build top-level acceleration structure (TLAS)
      OptixBuildInput build_input = {};
      build_input.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
#  if OPTIX_ABI_VERSION < 41  // Instance AABBs no longer need to be set since OptiX 7.2
      build_input.instanceArray.aabbs = aabbs.device_pointer;
      build_input.instanceArray.numAabbs = num_instances;
#  endif
      build_input.instanceArray.instances = instances.device_pointer;
      build_input.instanceArray.numInstances = num_instances;

      if (!build_optix_bvh(bvh_optix, OPTIX_BUILD_OPERATION_BUILD, build_input, 0)) {
        progress.set_error("Failed to build OptiX acceleration structure");
      }
      tlas_handle = bvh_optix->traversable_handle;
    }
  }

  void const_copy_to(const char *name, void *host, size_t size) override
  {
    // Set constant memory for CUDA module
    // TODO(pmours): This is only used for tonemapping (see 'film_convert').
    //               Could be removed by moving those functions to filter CUDA module.
    CUDADevice::const_copy_to(name, host, size);

    if (strcmp(name, "__data") == 0) {
      assert(size <= sizeof(KernelData));

      // Update traversable handle (since it is different for each device on multi devices)
      KernelData *const data = (KernelData *)host;
      *(OptixTraversableHandle *)&data->bvh.scene = tlas_handle;

      update_launch_params(offsetof(KernelParams, data), host, size);
      return;
    }

    // Update data storage pointers in launch parameters
#  define KERNEL_TEX(data_type, tex_name) \
    if (strcmp(name, #tex_name) == 0) { \
      update_launch_params(offsetof(KernelParams, tex_name), host, size); \
      return; \
    }
#  include "kernel/kernel_textures.h"
#  undef KERNEL_TEX
  }

  void update_launch_params(size_t offset, void *data, size_t data_size)
  {
    const CUDAContextScope scope(cuContext);

    for (int i = 0; i < info.cpu_threads; ++i)
      check_result_cuda(
          cuMemcpyHtoD(launch_params.device_pointer + i * launch_params.data_elements + offset,
                       data,
                       data_size));
  }

  void task_add(DeviceTask &task) override
  {
    // Upload texture information to device if it has changed since last launch
    load_texture_info();

    if (task.type == DeviceTask::FILM_CONVERT) {
      // Execute in main thread because of OpenGL access
      film_convert(task, task.buffer, task.rgba_byte, task.rgba_half);
      return;
    }

    if (task.type == DeviceTask::DENOISE_BUFFER) {
      // Execute denoising in a single thread (e.g. to avoid race conditions during creation)
      task_pool.push([=] {
        DeviceTask task_copy = task;
        thread_run(task_copy, 0);
      });
      return;
    }

    // Split task into smaller ones
    list<DeviceTask> tasks;
    task.split(tasks, info.cpu_threads);

    // Queue tasks in internal task pool
    int task_index = 0;
    for (DeviceTask &task : tasks) {
      task_pool.push([=] {
        // Using task index parameter instead of thread index, since number of CUDA streams may
        // differ from number of threads
        DeviceTask task_copy = task;
        thread_run(task_copy, task_index);
      });
      task_index++;
    }
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
};

bool device_optix_init()
{
  if (g_optixFunctionTable.optixDeviceContextCreate != NULL)
    return true;  // Already initialized function table

  // Need to initialize CUDA as well
  if (!device_cuda_init())
    return false;

  const OptixResult result = optixInit();

  if (result == OPTIX_ERROR_UNSUPPORTED_ABI_VERSION) {
    VLOG(1) << "OptiX initialization failed because the installed NVIDIA driver is too old. "
               "Please update to the latest driver first!";
    return false;
  }
  else if (result != OPTIX_SUCCESS) {
    VLOG(1) << "OptiX initialization failed with error code " << (unsigned int)result;
    return false;
  }

  // Loaded OptiX successfully!
  return true;
}

void device_optix_info(const vector<DeviceInfo> &cuda_devices, vector<DeviceInfo> &devices)
{
  devices.reserve(cuda_devices.size());

  // Simply add all supported CUDA devices as OptiX devices again
  for (DeviceInfo info : cuda_devices) {
    assert(info.type == DEVICE_CUDA);

    int major;
    cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, info.num);
    if (major < 5) {
      continue;  // Only Maxwell and up are supported by OptiX
    }

    info.type = DEVICE_OPTIX;
    info.id += "_OptiX";
    info.denoisers |= DENOISER_OPTIX;
    info.has_branched_path = false;

    devices.push_back(info);
  }
}

Device *device_optix_create(DeviceInfo &info, Stats &stats, Profiler &profiler, bool background)
{
  return new OptiXDevice(info, stats, profiler, background);
}

CCL_NAMESPACE_END

#endif
