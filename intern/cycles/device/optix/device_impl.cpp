/* SPDX-FileCopyrightText: 2019 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2019-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_OPTIX

#  include "device/optix/device_impl.h"
#  include "device/optix/queue.h"

#  include "bvh/bvh.h"
#  include "bvh/optix.h"

#  include "scene/hair.h"
#  include "scene/mesh.h"
#  include "scene/object.h"
#  include "scene/pointcloud.h"
#  include "scene/scene.h"

#  include "util/debug.h"
#  include "util/log.h"
#  include "util/path.h"
#  include "util/progress.h"
#  include "util/task.h"

#  define __KERNEL_OPTIX__
#  include "kernel/device/optix/globals.h"

CCL_NAMESPACE_BEGIN

static void execute_optix_task(TaskPool &pool, OptixTask task, OptixResult &failure_reason)
{
  OptixTask additional_tasks[16];
  unsigned int num_additional_tasks = 0;

  const OptixResult result = optixTaskExecute(task, additional_tasks, 16, &num_additional_tasks);
  if (result == OPTIX_SUCCESS) {
    for (unsigned int i = 0; i < num_additional_tasks; ++i) {
      pool.push([&pool, additional_task = additional_tasks[i], &failure_reason] {
        execute_optix_task(pool, additional_task, failure_reason);
      });
    }
  }
  else {
    failure_reason = result;
  }
}

OptiXDevice::OptiXDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler, bool headless)
    : CUDADevice(info, stats, profiler, headless),
#  ifdef WITH_OSL
      osl_colorsystem(this, "osl_colorsystem", MEM_READ_ONLY),
#  endif
      sbt_data(this, "__sbt", MEM_READ_ONLY),
      launch_params(this, "kernel_params", false)
{
  /* Make the CUDA context current. */
  if (!cuContext) {
    /* Do not initialize if CUDA context creation failed already. */
    return;
  }
  const CUDAContextScope scope(this);

  /* Create OptiX context for this device. */
  OptixDeviceContextOptions options = {};
#  ifdef WITH_CYCLES_LOGGING
  options.logCallbackLevel = 4; /* Fatal = 1, Error = 2, Warning = 3, Print = 4. */
  options.logCallbackFunction = [](unsigned int level, const char *, const char *message, void *) {
    switch (level) {
      case 1:
        LOG_FATAL << message;
        break;
      case 2:
        LOG_ERROR << message;
        break;
      case 3:
        LOG_WARNING << message;
        break;
      case 4:
        LOG_INFO << message;
        break;
      default:
        break;
    }
  };
#  endif
  if (DebugFlags().optix.use_debug) {
    LOG_INFO << "Using OptiX debug mode.";
    options.validationMode = OPTIX_DEVICE_CONTEXT_VALIDATION_MODE_ALL;
  }
  optix_assert(optixDeviceContextCreate(cuContext, &options, &context));
#  ifdef WITH_CYCLES_LOGGING
  optix_assert(optixDeviceContextSetLogCallback(
      context, options.logCallbackFunction, options.logCallbackData, options.logCallbackLevel));
#  endif

  /* Fix weird compiler bug that assigns wrong size. */
  launch_params.data_elements = sizeof(KernelParamsOptiX);

  /* Allocate launch parameter buffer memory on device. */
  launch_params.alloc_to_device(1);
}

OptiXDevice::~OptiXDevice()
{
  /* Make CUDA context current. */
  const CUDAContextScope scope(this);

  free_bvh_memory_delayed();

  sbt_data.free();
  texture_info.free();
  launch_params.free();

  /* Unload modules. */
  if (optix_module != nullptr) {
    optixModuleDestroy(optix_module);
  }
  for (int i = 0; i < 2; ++i) {
    if (builtin_modules[i] != nullptr) {
      optixModuleDestroy(builtin_modules[i]);
    }
  }
  for (int i = 0; i < NUM_PIPELINES; ++i) {
    if (pipelines[i] != nullptr) {
      optixPipelineDestroy(pipelines[i]);
    }
  }
  for (int i = 0; i < NUM_PROGRAM_GROUPS; ++i) {
    if (groups[i] != nullptr) {
      optixProgramGroupDestroy(groups[i]);
    }
  }

#  ifdef WITH_OSL
  if (osl_camera_module != nullptr) {
    optixModuleDestroy(osl_camera_module);
  }
  for (const OptixModule &module : osl_modules) {
    if (module != nullptr) {
      optixModuleDestroy(module);
    }
  }
  for (const OptixProgramGroup &group : osl_groups) {
    if (group != nullptr) {
      optixProgramGroupDestroy(group);
    }
  }
  osl_colorsystem.free();
#  endif

  optixDeviceContextDestroy(context);
}

unique_ptr<DeviceQueue> OptiXDevice::gpu_queue_create()
{
  return make_unique<OptiXDeviceQueue>(this);
}

BVHLayoutMask OptiXDevice::get_bvh_layout_mask(uint /*kernel_features*/) const
{
  /* OptiX has its own internal acceleration structure format. */
  return BVH_LAYOUT_OPTIX;
}

static string get_optix_include_dir()
{
  const char *env_dir = getenv("OPTIX_ROOT_DIR");
  const char *default_dir = CYCLES_RUNTIME_OPTIX_ROOT_DIR;

  if (env_dir && env_dir[0]) {
    const string env_include_dir = path_join(env_dir, "include");
    return env_include_dir;
  }
  if (default_dir[0]) {
    const string default_include_dir = path_join(default_dir, "include");
    return default_include_dir;
  }

  return string();
}

string OptiXDevice::compile_kernel_get_common_cflags(const uint kernel_features)
{
  string common_cflags = CUDADevice::compile_kernel_get_common_cflags(kernel_features);

  /* Add OptiX SDK include directory to include paths. */
  common_cflags += string_printf(" -I\"%s\"", get_optix_include_dir().c_str());

  /* Specialization for shader ray-tracing. */
  if (kernel_features & KERNEL_FEATURE_NODE_RAYTRACE) {
    common_cflags += " --keep-device-functions";
  }

  return common_cflags;
}

void OptiXDevice::create_optix_module(TaskPool &pool,
                                      OptixModuleCompileOptions &module_options,
                                      string &ptx_data,
                                      OptixModule &module,
                                      OptixResult &result)
{
  OptixTask task = nullptr;
  result = optixModuleCreateWithTasks(context,
                                      &module_options,
                                      &pipeline_options,
                                      ptx_data.data(),
                                      ptx_data.size(),
                                      nullptr,
                                      nullptr,
                                      &module,
                                      &task);
  if (result == OPTIX_SUCCESS) {
    execute_optix_task(pool, task, result);
  }
}

bool OptiXDevice::load_kernels(const uint kernel_features)
{
  if (have_error()) {
    /* Abort early if context creation failed already. */
    return false;
  }

#  ifdef WITH_OSL
  /* TODO: Consider splitting kernels into an OSL-camera-only and a full-OSL variant. */
  const bool use_osl_shading = (kernel_features & KERNEL_FEATURE_OSL_SHADING);
  const bool use_osl_camera = (kernel_features & KERNEL_FEATURE_OSL_CAMERA);
#  else
  const bool use_osl_shading = false;
  const bool use_osl_camera = false;
#  endif

  /* Skip creating OptiX module if only doing denoising. */
  const bool need_optix_kernels = (kernel_features &
                                   (KERNEL_FEATURE_PATH_TRACING | KERNEL_FEATURE_BAKING));

  /* Detect existence of OptiX kernel and SDK here early. So we can error out
   * before compiling the CUDA kernels, to avoid failing right after when
   * compiling the OptiX kernel. */
  string suffix = use_osl_shading ? "_osl" :
                  (kernel_features & (KERNEL_FEATURE_NODE_RAYTRACE | KERNEL_FEATURE_MNEE)) ?
                                    "_shader_raytrace" :
                                    "";
  string ptx_filename;
  if (need_optix_kernels) {
    ptx_filename = path_get("lib/kernel_optix" + suffix + ".ptx.zst");
    if (use_adaptive_compilation() || path_file_size(ptx_filename) == -1) {
      std::string optix_include_dir = get_optix_include_dir();
      if (optix_include_dir.empty()) {
        set_error(
            "Unable to compile OptiX kernels at runtime. Set OPTIX_ROOT_DIR environment variable "
            "to a directory containing the OptiX SDK.");
        return false;
      }
      if (!path_is_directory(optix_include_dir)) {
        set_error(string_printf(
            "OptiX headers not found at %s, unable to compile OptiX kernels at runtime. Install "
            "OptiX SDK in the specified location, or set OPTIX_ROOT_DIR environment variable to a "
            "directory containing the OptiX SDK.",
            optix_include_dir.c_str()));
        return false;
      }
    }
  }

  /* Load CUDA modules because we need some of the utility kernels. */
  if (!CUDADevice::load_kernels(kernel_features)) {
    return false;
  }

  if (!need_optix_kernels) {
    return true;
  }

  const CUDAContextScope scope(this);

  /* Unload existing OptiX module and pipelines first. */
  if (optix_module != nullptr) {
    optixModuleDestroy(optix_module);
    optix_module = nullptr;
  }
  for (int i = 0; i < 2; ++i) {
    if (builtin_modules[i] != nullptr) {
      optixModuleDestroy(builtin_modules[i]);
      builtin_modules[i] = nullptr;
    }
  }
  for (int i = 0; i < NUM_PIPELINES; ++i) {
    if (pipelines[i] != nullptr) {
      optixPipelineDestroy(pipelines[i]);
      pipelines[i] = nullptr;
    }
  }
  for (int i = 0; i < NUM_PROGRAM_GROUPS; ++i) {
    if (groups[i] != nullptr) {
      optixProgramGroupDestroy(groups[i]);
      groups[i] = nullptr;
    }
  }

#  ifdef WITH_OSL
  if (osl_camera_module != nullptr) {
    optixModuleDestroy(osl_camera_module);
    osl_camera_module = nullptr;
  }

  /* Recreating base OptiX module invalidates all OSL modules too, since they link against it. */
  for (const OptixModule &module : osl_modules) {
    if (module != nullptr) {
      optixModuleDestroy(module);
    }
  }
  osl_modules.clear();

  for (const OptixProgramGroup &group : osl_groups) {
    if (group != nullptr) {
      optixProgramGroupDestroy(group);
    }
  }
  osl_groups.clear();
#  endif

  OptixModuleCompileOptions module_options = {};
  module_options.maxRegisterCount = 0; /* Do not set an explicit register limit. */

  if (DebugFlags().optix.use_debug) {
    module_options.optLevel = OPTIX_COMPILE_OPTIMIZATION_LEVEL_0;
    module_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
  }
  else {
    module_options.optLevel = OPTIX_COMPILE_OPTIMIZATION_LEVEL_3;
    module_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_NONE;
  }

  module_options.boundValues = nullptr;
  module_options.numBoundValues = 0;
  module_options.payloadTypes = nullptr;
  module_options.numPayloadTypes = 0;

  /* Default to no motion blur and two-level graph, since it is the fastest option. */
  pipeline_options.usesMotionBlur = false;
  pipeline_options.traversableGraphFlags =
      OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;
  pipeline_options.numPayloadValues = 8;
  pipeline_options.numAttributeValues = 2; /* u, v */
  pipeline_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;
  pipeline_options.pipelineLaunchParamsVariableName = "kernel_params"; /* See globals.h */

  pipeline_options.usesPrimitiveTypeFlags = OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE;
  if (kernel_features & KERNEL_FEATURE_HAIR) {
    if (kernel_features & KERNEL_FEATURE_HAIR_THICK) {
      pipeline_options.usesPrimitiveTypeFlags |= OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_CATMULLROM;
    }
    else {
      pipeline_options.usesPrimitiveTypeFlags |= OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM;
    }
  }
  if (kernel_features & KERNEL_FEATURE_POINTCLOUD) {
    pipeline_options.usesPrimitiveTypeFlags |= OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM;
  }

  /* Keep track of whether motion blur is enabled, so to enable/disable motion in BVH builds
   * This is necessary since objects may be reported to have motion if the Vector pass is
   * active, but may still need to be rendered without motion blur if that isn't active as well. */
  if (kernel_features & KERNEL_FEATURE_OBJECT_MOTION) {
    pipeline_options.usesMotionBlur = true;
    /* Motion blur can insert motion transforms into the traversal graph.
     * It is no longer a two-level graph then, so need to set flags to allow any configuration. */
    pipeline_options.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_ANY;
  }

  { /* Load and compile PTX module with OptiX kernels. */
    string ptx_data;
    if (use_adaptive_compilation() || path_file_size(ptx_filename) == -1) {
      string cflags = compile_kernel_get_common_cflags(kernel_features);
      ptx_filename = compile_kernel(cflags, ("kernel" + suffix).c_str(), "optix", true);
    }
    if (ptx_filename.empty() || !path_read_compressed_text(ptx_filename, ptx_data)) {
      set_error(string_printf("Failed to load OptiX kernel from '%s'", ptx_filename.c_str()));
      return false;
    }

    TaskPool pool;
    OptixResult result;
    create_optix_module(pool, module_options, ptx_data, optix_module, result);
    pool.wait_work();
    if (result != OPTIX_SUCCESS) {
      set_error(string_printf("Failed to load OptiX kernel from '%s' (%s)",
                              ptx_filename.c_str(),
                              optixGetErrorName(result)));
      return false;
    }
  }

  /* Create program groups. */
  OptixProgramGroupDesc group_descs[NUM_PROGRAM_GROUPS] = {};
  OptixProgramGroupOptions group_options = {}; /* There are no options currently. */
  group_descs[PG_RGEN_INTERSECT_CLOSEST].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
  group_descs[PG_RGEN_INTERSECT_CLOSEST].raygen.module = optix_module;
  group_descs[PG_RGEN_INTERSECT_CLOSEST].raygen.entryFunctionName =
      "__raygen__kernel_optix_integrator_intersect_closest";
  group_descs[PG_RGEN_INTERSECT_SHADOW].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
  group_descs[PG_RGEN_INTERSECT_SHADOW].raygen.module = optix_module;
  group_descs[PG_RGEN_INTERSECT_SHADOW].raygen.entryFunctionName =
      "__raygen__kernel_optix_integrator_intersect_shadow";
  group_descs[PG_RGEN_INTERSECT_SUBSURFACE].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
  group_descs[PG_RGEN_INTERSECT_SUBSURFACE].raygen.module = optix_module;
  group_descs[PG_RGEN_INTERSECT_SUBSURFACE].raygen.entryFunctionName =
      "__raygen__kernel_optix_integrator_intersect_subsurface";
  group_descs[PG_RGEN_INTERSECT_VOLUME_STACK].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
  group_descs[PG_RGEN_INTERSECT_VOLUME_STACK].raygen.module = optix_module;
  group_descs[PG_RGEN_INTERSECT_VOLUME_STACK].raygen.entryFunctionName =
      "__raygen__kernel_optix_integrator_intersect_volume_stack";
  group_descs[PG_RGEN_INTERSECT_DEDICATED_LIGHT].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
  group_descs[PG_RGEN_INTERSECT_DEDICATED_LIGHT].raygen.module = optix_module;
  group_descs[PG_RGEN_INTERSECT_DEDICATED_LIGHT].raygen.entryFunctionName =
      "__raygen__kernel_optix_integrator_intersect_dedicated_light";
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
  group_descs[PG_HITV].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
  group_descs[PG_HITV].hitgroup.moduleCH = optix_module;
  group_descs[PG_HITV].hitgroup.entryFunctionNameCH = "__closesthit__kernel_optix_hit";
  group_descs[PG_HITV].hitgroup.moduleAH = optix_module;
  group_descs[PG_HITV].hitgroup.entryFunctionNameAH = "__anyhit__kernel_optix_volume_test";

  if (kernel_features & KERNEL_FEATURE_HAIR) {
    if (kernel_features & KERNEL_FEATURE_HAIR_THICK) {
      /* Built-in thick curve intersection. */
      OptixBuiltinISOptions builtin_options = {};
      builtin_options.builtinISModuleType = OPTIX_PRIMITIVE_TYPE_ROUND_CATMULLROM;
      builtin_options.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE |
                                   OPTIX_BUILD_FLAG_ALLOW_COMPACTION |
                                   OPTIX_BUILD_FLAG_ALLOW_UPDATE;
      builtin_options.curveEndcapFlags = OPTIX_CURVE_ENDCAP_DEFAULT; /* Disable end-caps. */
      builtin_options.usesMotionBlur = false;

      optix_assert(optixBuiltinISModuleGet(
          context, &module_options, &pipeline_options, &builtin_options, &builtin_modules[0]));

      group_descs[PG_HITD].hitgroup.moduleIS = builtin_modules[0];
      group_descs[PG_HITD].hitgroup.entryFunctionNameIS = nullptr;
      group_descs[PG_HITS].hitgroup.moduleIS = builtin_modules[0];
      group_descs[PG_HITS].hitgroup.entryFunctionNameIS = nullptr;

      if (pipeline_options.usesMotionBlur) {
        builtin_options.usesMotionBlur = true;

        optix_assert(optixBuiltinISModuleGet(
            context, &module_options, &pipeline_options, &builtin_options, &builtin_modules[1]));

        group_descs[PG_HITD_MOTION] = group_descs[PG_HITD];
        group_descs[PG_HITD_MOTION].hitgroup.moduleIS = builtin_modules[1];
        group_descs[PG_HITS_MOTION] = group_descs[PG_HITS];
        group_descs[PG_HITS_MOTION].hitgroup.moduleIS = builtin_modules[1];
      }
    }
    else {
      /* Custom ribbon intersection. */
      group_descs[PG_HITD].hitgroup.moduleIS = optix_module;
      group_descs[PG_HITS].hitgroup.moduleIS = optix_module;
      group_descs[PG_HITD].hitgroup.entryFunctionNameIS = "__intersection__curve_ribbon";
      group_descs[PG_HITS].hitgroup.entryFunctionNameIS = "__intersection__curve_ribbon";
    }
  }

  if (kernel_features & KERNEL_FEATURE_POINTCLOUD) {
    group_descs[PG_HITD_POINTCLOUD] = group_descs[PG_HITD];
    group_descs[PG_HITD_POINTCLOUD].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    group_descs[PG_HITD_POINTCLOUD].hitgroup.moduleIS = optix_module;
    group_descs[PG_HITD_POINTCLOUD].hitgroup.entryFunctionNameIS = "__intersection__point";
    group_descs[PG_HITS_POINTCLOUD] = group_descs[PG_HITS];
    group_descs[PG_HITS_POINTCLOUD].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    group_descs[PG_HITS_POINTCLOUD].hitgroup.moduleIS = optix_module;
    group_descs[PG_HITS_POINTCLOUD].hitgroup.entryFunctionNameIS = "__intersection__point";
  }

  /* Add hit group for local intersections. */
  if (kernel_features & (KERNEL_FEATURE_SUBSURFACE | KERNEL_FEATURE_NODE_RAYTRACE)) {
    group_descs[PG_HITL].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    group_descs[PG_HITL].hitgroup.moduleAH = optix_module;
    group_descs[PG_HITL].hitgroup.entryFunctionNameAH = "__anyhit__kernel_optix_local_hit";
  }

  /* Shader ray-tracing replaces some functions with direct callables. */
  if (kernel_features & KERNEL_FEATURE_NODE_RAYTRACE) {
    group_descs[PG_RGEN_SHADE_SURFACE_RAYTRACE].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    group_descs[PG_RGEN_SHADE_SURFACE_RAYTRACE].raygen.module = optix_module;
    group_descs[PG_RGEN_SHADE_SURFACE_RAYTRACE].raygen.entryFunctionName =
        "__raygen__kernel_optix_integrator_shade_surface_raytrace";

    /* Kernels with OSL shading support are built without SVM, so can skip those direct callables
     * there. */
    if (!use_osl_shading) {
      group_descs[PG_CALL_SVM_AO].kind = OPTIX_PROGRAM_GROUP_KIND_CALLABLES;
      group_descs[PG_CALL_SVM_AO].callables.moduleDC = optix_module;
      group_descs[PG_CALL_SVM_AO].callables.entryFunctionNameDC = "__direct_callable__svm_node_ao";
      group_descs[PG_CALL_SVM_BEVEL].kind = OPTIX_PROGRAM_GROUP_KIND_CALLABLES;
      group_descs[PG_CALL_SVM_BEVEL].callables.moduleDC = optix_module;
      group_descs[PG_CALL_SVM_BEVEL].callables.entryFunctionNameDC =
          "__direct_callable__svm_node_bevel";
    }
  }

  if (kernel_features & KERNEL_FEATURE_MNEE) {
    group_descs[PG_RGEN_SHADE_SURFACE_MNEE].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    group_descs[PG_RGEN_SHADE_SURFACE_MNEE].raygen.module = optix_module;
    group_descs[PG_RGEN_SHADE_SURFACE_MNEE].raygen.entryFunctionName =
        "__raygen__kernel_optix_integrator_shade_surface_mnee";
  }

  /* OSL uses direct callables to execute, so shading needs to be done in OptiX if OSL is used. */
  if (use_osl_shading) {
    group_descs[PG_RGEN_SHADE_BACKGROUND].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    group_descs[PG_RGEN_SHADE_BACKGROUND].raygen.module = optix_module;
    group_descs[PG_RGEN_SHADE_BACKGROUND].raygen.entryFunctionName =
        "__raygen__kernel_optix_integrator_shade_background";
    group_descs[PG_RGEN_SHADE_LIGHT].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    group_descs[PG_RGEN_SHADE_LIGHT].raygen.module = optix_module;
    group_descs[PG_RGEN_SHADE_LIGHT].raygen.entryFunctionName =
        "__raygen__kernel_optix_integrator_shade_light";
    group_descs[PG_RGEN_SHADE_SURFACE].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    group_descs[PG_RGEN_SHADE_SURFACE].raygen.module = optix_module;
    group_descs[PG_RGEN_SHADE_SURFACE].raygen.entryFunctionName =
        "__raygen__kernel_optix_integrator_shade_surface";
    group_descs[PG_RGEN_SHADE_VOLUME].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    group_descs[PG_RGEN_SHADE_VOLUME].raygen.module = optix_module;
    group_descs[PG_RGEN_SHADE_VOLUME].raygen.entryFunctionName =
        "__raygen__kernel_optix_integrator_shade_volume";
    group_descs[PG_RGEN_SHADE_SHADOW].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    group_descs[PG_RGEN_SHADE_SHADOW].raygen.module = optix_module;
    group_descs[PG_RGEN_SHADE_SHADOW].raygen.entryFunctionName =
        "__raygen__kernel_optix_integrator_shade_shadow";
    group_descs[PG_RGEN_SHADE_DEDICATED_LIGHT].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    group_descs[PG_RGEN_SHADE_DEDICATED_LIGHT].raygen.module = optix_module;
    group_descs[PG_RGEN_SHADE_DEDICATED_LIGHT].raygen.entryFunctionName =
        "__raygen__kernel_optix_integrator_shade_dedicated_light";
    group_descs[PG_RGEN_EVAL_DISPLACE].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    group_descs[PG_RGEN_EVAL_DISPLACE].raygen.module = optix_module;
    group_descs[PG_RGEN_EVAL_DISPLACE].raygen.entryFunctionName =
        "__raygen__kernel_optix_shader_eval_displace";
    group_descs[PG_RGEN_EVAL_BACKGROUND].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    group_descs[PG_RGEN_EVAL_BACKGROUND].raygen.module = optix_module;
    group_descs[PG_RGEN_EVAL_BACKGROUND].raygen.entryFunctionName =
        "__raygen__kernel_optix_shader_eval_background";
    group_descs[PG_RGEN_EVAL_CURVE_SHADOW_TRANSPARENCY].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    group_descs[PG_RGEN_EVAL_CURVE_SHADOW_TRANSPARENCY].raygen.module = optix_module;
    group_descs[PG_RGEN_EVAL_CURVE_SHADOW_TRANSPARENCY].raygen.entryFunctionName =
        "__raygen__kernel_optix_shader_eval_curve_shadow_transparency";
  }

#  ifdef WITH_OSL
  /* When using custom OSL cameras, integrator_init_from_camera is its own specialized module. */
  if (use_osl_camera) {
    /* Load and compile the OSL camera PTX module. */
    string ptx_data, ptx_filename = path_get("lib/kernel_optix_osl_camera.ptx.zst");
    if (!path_read_compressed_text(ptx_filename, ptx_data)) {
      set_error(
          string_printf("Failed to load OptiX OSL camera kernel from '%s'", ptx_filename.c_str()));
      return false;
    }

    TaskPool pool;
    OptixResult result;
    create_optix_module(pool, module_options, ptx_data, osl_camera_module, result);
    pool.wait_work();
    if (result != OPTIX_SUCCESS) {
      set_error(string_printf("Failed to load OptiX kernel from '%s' (%s)",
                              ptx_filename.c_str(),
                              optixGetErrorName(result)));
      return false;
    }

    group_descs[PG_RGEN_INIT_FROM_CAMERA].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    group_descs[PG_RGEN_INIT_FROM_CAMERA].raygen.module = osl_camera_module;
    group_descs[PG_RGEN_INIT_FROM_CAMERA].raygen.entryFunctionName =
        "__raygen__kernel_optix_integrator_init_from_camera";
  }
#  endif

  optix_assert(optixProgramGroupCreate(
      context, group_descs, NUM_PROGRAM_GROUPS, &group_options, nullptr, nullptr, groups));

  /* Get program stack sizes. */
  OptixStackSizes stack_size[NUM_PROGRAM_GROUPS] = {};
  /* Set up SBT, which in this case is used only to select between different programs. */
  sbt_data.alloc(NUM_PROGRAM_GROUPS);
  memset(sbt_data.host_pointer, 0, sizeof(SbtRecord) * NUM_PROGRAM_GROUPS);
  for (int i = 0; i < NUM_PROGRAM_GROUPS; ++i) {
    optix_assert(optixSbtRecordPackHeader(groups[i], &sbt_data[i]));
    optix_assert(optixProgramGroupGetStackSize(groups[i], &stack_size[i], nullptr));
  }
  sbt_data.copy_to_device(); /* Upload SBT to device. */

  /* Calculate maximum trace continuation stack size. */
  unsigned int trace_css = stack_size[PG_HITD].cssCH;
  /* This is based on the maximum of closest-hit and any-hit/intersection programs. */
  trace_css = std::max(trace_css, stack_size[PG_HITD].cssIS + stack_size[PG_HITD].cssAH);
  trace_css = std::max(trace_css, stack_size[PG_HITS].cssIS + stack_size[PG_HITS].cssAH);
  trace_css = std::max(trace_css, stack_size[PG_HITL].cssIS + stack_size[PG_HITL].cssAH);
  trace_css = std::max(trace_css, stack_size[PG_HITV].cssIS + stack_size[PG_HITV].cssAH);
  trace_css = std::max(trace_css,
                       stack_size[PG_HITD_MOTION].cssIS + stack_size[PG_HITD_MOTION].cssAH);
  trace_css = std::max(trace_css,
                       stack_size[PG_HITS_MOTION].cssIS + stack_size[PG_HITS_MOTION].cssAH);
  trace_css = std::max(
      trace_css, stack_size[PG_HITD_POINTCLOUD].cssIS + stack_size[PG_HITD_POINTCLOUD].cssAH);
  trace_css = std::max(
      trace_css, stack_size[PG_HITS_POINTCLOUD].cssIS + stack_size[PG_HITS_POINTCLOUD].cssAH);

  OptixPipelineLinkOptions link_options = {};
  link_options.maxTraceDepth = 1;

  if (use_osl_shading || use_osl_camera) {
    /* OSL kernels will be (re)created on by OSL manager. */
  }
  else if (kernel_features & (KERNEL_FEATURE_NODE_RAYTRACE | KERNEL_FEATURE_MNEE)) {
    /* Create shader ray-tracing and MNEE pipeline. */
    vector<OptixProgramGroup> pipeline_groups;
    pipeline_groups.reserve(NUM_PROGRAM_GROUPS);
    if (kernel_features & KERNEL_FEATURE_NODE_RAYTRACE) {
      pipeline_groups.push_back(groups[PG_RGEN_SHADE_SURFACE_RAYTRACE]);
      pipeline_groups.push_back(groups[PG_CALL_SVM_AO]);
      pipeline_groups.push_back(groups[PG_CALL_SVM_BEVEL]);
    }
    if (kernel_features & KERNEL_FEATURE_MNEE) {
      pipeline_groups.push_back(groups[PG_RGEN_SHADE_SURFACE_MNEE]);
    }
    pipeline_groups.push_back(groups[PG_MISS]);
    pipeline_groups.push_back(groups[PG_HITD]);
    pipeline_groups.push_back(groups[PG_HITS]);
    pipeline_groups.push_back(groups[PG_HITL]);
    pipeline_groups.push_back(groups[PG_HITV]);
    if (pipeline_options.usesMotionBlur) {
      pipeline_groups.push_back(groups[PG_HITD_MOTION]);
      pipeline_groups.push_back(groups[PG_HITS_MOTION]);
    }
    if (kernel_features & KERNEL_FEATURE_POINTCLOUD) {
      pipeline_groups.push_back(groups[PG_HITD_POINTCLOUD]);
      pipeline_groups.push_back(groups[PG_HITS_POINTCLOUD]);
    }

    optix_assert(optixPipelineCreate(context,
                                     &pipeline_options,
                                     &link_options,
                                     pipeline_groups.data(),
                                     pipeline_groups.size(),
                                     nullptr,
                                     nullptr,
                                     &pipelines[PIP_SHADE]));

    /* Combine ray generation and trace continuation stack size. */
    const unsigned int css = std::max(stack_size[PG_RGEN_SHADE_SURFACE_RAYTRACE].cssRG,
                                      stack_size[PG_RGEN_SHADE_SURFACE_MNEE].cssRG) +
                             link_options.maxTraceDepth * trace_css;
    const unsigned int dss = std::max(stack_size[PG_CALL_SVM_AO].dssDC,
                                      stack_size[PG_CALL_SVM_BEVEL].dssDC);

    /* Set stack size depending on pipeline options. */
    optix_assert(optixPipelineSetStackSize(
        pipelines[PIP_SHADE], 0, dss, css, pipeline_options.usesMotionBlur ? 3 : 2));
  }

  { /* Create intersection-only pipeline. */
    vector<OptixProgramGroup> pipeline_groups;
    pipeline_groups.reserve(NUM_PROGRAM_GROUPS);
    pipeline_groups.push_back(groups[PG_RGEN_INTERSECT_CLOSEST]);
    pipeline_groups.push_back(groups[PG_RGEN_INTERSECT_SHADOW]);
    pipeline_groups.push_back(groups[PG_RGEN_INTERSECT_SUBSURFACE]);
    pipeline_groups.push_back(groups[PG_RGEN_INTERSECT_VOLUME_STACK]);
    pipeline_groups.push_back(groups[PG_RGEN_INTERSECT_DEDICATED_LIGHT]);
    pipeline_groups.push_back(groups[PG_MISS]);
    pipeline_groups.push_back(groups[PG_HITD]);
    pipeline_groups.push_back(groups[PG_HITS]);
    pipeline_groups.push_back(groups[PG_HITL]);
    pipeline_groups.push_back(groups[PG_HITV]);
    if (pipeline_options.usesMotionBlur) {
      pipeline_groups.push_back(groups[PG_HITD_MOTION]);
      pipeline_groups.push_back(groups[PG_HITS_MOTION]);
    }
    if (kernel_features & KERNEL_FEATURE_POINTCLOUD) {
      pipeline_groups.push_back(groups[PG_HITD_POINTCLOUD]);
      pipeline_groups.push_back(groups[PG_HITS_POINTCLOUD]);
    }

    optix_assert(optixPipelineCreate(context,
                                     &pipeline_options,
                                     &link_options,
                                     pipeline_groups.data(),
                                     pipeline_groups.size(),
                                     nullptr,
                                     nullptr,
                                     &pipelines[PIP_INTERSECT]));

    /* Calculate continuation stack size based on the maximum of all ray generation stack sizes. */
    const unsigned int css =
        std::max(stack_size[PG_RGEN_INTERSECT_CLOSEST].cssRG,
                 std::max(stack_size[PG_RGEN_INTERSECT_SHADOW].cssRG,
                          std::max(stack_size[PG_RGEN_INTERSECT_SUBSURFACE].cssRG,
                                   stack_size[PG_RGEN_INTERSECT_VOLUME_STACK].cssRG))) +
        link_options.maxTraceDepth * trace_css;

    optix_assert(optixPipelineSetStackSize(
        pipelines[PIP_INTERSECT], 0, 0, css, pipeline_options.usesMotionBlur ? 3 : 2));
  }

  return !have_error();
}

bool OptiXDevice::load_osl_kernels()
{
#  ifdef WITH_OSL
  if (have_error()) {
    return false;
  }

  struct OSLKernel {
    string ptx;
    string fused_entry;
  };

  auto get_osl_kernel = [&](const OSL::ShaderGroupRef &group) {
    if (!group) {
      return OSLKernel{};
    }
    string osl_ptx, fused_name;
    osl_globals.ss->getattribute(group.get(), "group_fused_name", fused_name);
    osl_globals.ss->getattribute(
        group.get(), "ptx_compiled_version", OSL::TypeDesc::PTR, &osl_ptx);

    int groupdata_size = 0;
    osl_globals.ss->getattribute(group.get(), "llvm_groupdata_size", groupdata_size);
    if (groupdata_size == 0) {
      // Old attribute name from our patched OSL version as fallback.
      osl_globals.ss->getattribute(group.get(), "groupdata_size", groupdata_size);
    }
    if (groupdata_size > 2048) { /* See 'group_data' array in kernel/osl/osl.h */
      set_error(
          string_printf("Requested OSL group data size (%d) is greater than the maximum "
                        "supported with OptiX (2048)",
                        groupdata_size));
      return OSLKernel{};
    }

    return OSLKernel{std::move(osl_ptx), std::move(fused_name)};
  };

  /* This has to be in the same order as the ShaderType enum, so that the index calculation in
   * osl_eval_nodes checks out */
  vector<OSLKernel> osl_kernels;
  osl_kernels.emplace_back(get_osl_kernel(osl_globals.camera_state));
  for (const OSL::ShaderGroupRef &group : osl_globals.surface_state) {
    osl_kernels.emplace_back(get_osl_kernel(group));
  }
  for (const OSL::ShaderGroupRef &group : osl_globals.volume_state) {
    osl_kernels.emplace_back(get_osl_kernel(group));
  }
  for (const OSL::ShaderGroupRef &group : osl_globals.displacement_state) {
    osl_kernels.emplace_back(get_osl_kernel(group));
  }
  for (const OSL::ShaderGroupRef &group : osl_globals.bump_state) {
    osl_kernels.emplace_back(get_osl_kernel(group));
  }

  if (have_error()) {
    return false;
  }

  const CUDAContextScope scope(this);

  if (pipelines[PIP_SHADE]) {
    optixPipelineDestroy(pipelines[PIP_SHADE]);
  }

  for (OptixModule &module : osl_modules) {
    if (module != nullptr) {
      optixModuleDestroy(module);
      module = nullptr;
    }
  }
  for (OptixProgramGroup &group : osl_groups) {
    if (group != nullptr) {
      optixProgramGroupDestroy(group);
      group = nullptr;
    }
  }

  /* We always need to reserve a spot for the camera shader group, but if it's unused
   * and there are no other shader groups, we can skip creating the pipeline. */
  if (osl_kernels.size() == 1 && osl_kernels[0].ptx.empty()) {
    return true;
  }

  OptixProgramGroupOptions group_options = {}; /* There are no options currently. */
  OptixModuleCompileOptions module_options = {};
  module_options.optLevel = OPTIX_COMPILE_OPTIMIZATION_LEVEL_3;
  module_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_NONE;

  /* In addition to the modules for each OSL group, we need to load our own osl_services.ptx
   * as well as the shadeops.ptx that's embedded in OSL. */
  size_t id_osl_services = osl_kernels.size();
  size_t id_osl_shadeops = osl_kernels.size() + 1;
  osl_groups.resize(osl_kernels.size() + 2);
  osl_modules.resize(osl_kernels.size() + 2);

  { /* Load and compile PTX module with OSL services. */
    string osl_services_ptx, ptx_filename = path_get("lib/kernel_optix_osl_services.ptx.zst");
    if (!path_read_compressed_text(ptx_filename, osl_services_ptx)) {
      set_error(string_printf("Failed to load OptiX OSL services kernel from '%s'",
                              ptx_filename.c_str()));
      return false;
    }

    const char *shadeops_ptx_ptr = nullptr;
    osl_globals.ss->getattribute("shadeops_cuda_ptx", OSL::TypeDesc::PTR, &shadeops_ptx_ptr);
    int shadeops_ptx_size = 0;
    osl_globals.ss->getattribute("shadeops_cuda_ptx_size", OSL::TypeDesc::INT, &shadeops_ptx_size);
    string shadeops_ptx(shadeops_ptx_ptr, shadeops_ptx_size);

    TaskPool pool;
    OptixResult services_result, shadeops_result;
    create_optix_module(
        pool, module_options, osl_services_ptx, osl_modules[id_osl_services], services_result);
    create_optix_module(
        pool, module_options, shadeops_ptx, osl_modules[id_osl_shadeops], shadeops_result);
    pool.wait_work();

    {
      if (services_result != OPTIX_SUCCESS) {
        set_error(string_printf("Failed to load OptiX OSL services kernel from '%s' (%s)",
                                ptx_filename.c_str(),
                                optixGetErrorName(services_result)));
        return false;
      }
      OptixProgramGroupDesc group_desc = {};
      group_desc.kind = OPTIX_PROGRAM_GROUP_KIND_CALLABLES;
      group_desc.callables.entryFunctionNameDC = "__direct_callable__dummy_services";
      group_desc.callables.moduleDC = osl_modules[id_osl_services];

      optix_assert(optixProgramGroupCreate(context,
                                           &group_desc,
                                           1,
                                           &group_options,
                                           nullptr,
                                           nullptr,
                                           &osl_groups[id_osl_services]));
    }

    {
      if (shadeops_result != OPTIX_SUCCESS) {
        set_error(string_printf("Failed to load OptiX OSL shadeops kernel (%s)",
                                optixGetErrorName(shadeops_result)));
        return false;
      }
      OptixProgramGroupDesc group_desc = {};
      group_desc.kind = OPTIX_PROGRAM_GROUP_KIND_CALLABLES;
      group_desc.callables.entryFunctionNameDC = "__direct_callable__dummy_shadeops";
      group_desc.callables.moduleDC = osl_modules[id_osl_shadeops];

      optix_assert(optixProgramGroupCreate(context,
                                           &group_desc,
                                           1,
                                           &group_options,
                                           nullptr,
                                           nullptr,
                                           &osl_groups[id_osl_shadeops]));
    }
  }

  TaskPool pool;
  vector<OptixResult> results(osl_kernels.size(), OPTIX_SUCCESS);

  for (size_t i = 0; i < osl_kernels.size(); ++i) {
    if (osl_kernels[i].ptx.empty()) {
      continue;
    }

    create_optix_module(pool, module_options, osl_kernels[i].ptx, osl_modules[i], results[i]);
  }

  pool.wait_work();

  for (size_t i = 0; i < osl_kernels.size(); ++i) {
    if (osl_kernels[i].ptx.empty()) {
      continue;
    }

    if (results[i] != OPTIX_SUCCESS) {
      set_error(string_printf("Failed to load OptiX OSL kernel for %s (%s)",
                              osl_kernels[i].fused_entry.c_str(),
                              optixGetErrorName(results[i])));
      return false;
    }

    OptixProgramGroupDesc group_desc = {};
    group_desc.kind = OPTIX_PROGRAM_GROUP_KIND_CALLABLES;
    group_desc.callables.entryFunctionNameDC = osl_kernels[i].fused_entry.c_str();
    group_desc.callables.moduleDC = osl_modules[i];

    optix_assert(optixProgramGroupCreate(
        context, &group_desc, 1, &group_options, nullptr, nullptr, &osl_groups[i]));
  }

  /* Update SBT with new entries. */
  sbt_data.alloc(NUM_PROGRAM_GROUPS + osl_groups.size());
  for (int i = 0; i < NUM_PROGRAM_GROUPS; ++i) {
    optix_assert(optixSbtRecordPackHeader(groups[i], &sbt_data[i]));
  }
  for (size_t i = 0; i < osl_groups.size(); ++i) {
    if (osl_groups[i] != nullptr) {
      optix_assert(optixSbtRecordPackHeader(osl_groups[i], &sbt_data[NUM_PROGRAM_GROUPS + i]));
    }
    else {
      /* Default to "__direct_callable__dummy_services", so that OSL evaluation for empty
       * materials has direct callables to call and does not crash. */
      optix_assert(optixSbtRecordPackHeader(osl_groups[id_osl_services],
                                            &sbt_data[NUM_PROGRAM_GROUPS + i]));
    }
  }
  sbt_data.copy_to_device(); /* Upload updated SBT to device. */

  OptixPipelineLinkOptions link_options = {};
  link_options.maxTraceDepth = 0;

  {
    vector<OptixProgramGroup> pipeline_groups;
    pipeline_groups.reserve(NUM_PROGRAM_GROUPS);
    pipeline_groups.push_back(groups[PG_RGEN_SHADE_BACKGROUND]);
    pipeline_groups.push_back(groups[PG_RGEN_SHADE_LIGHT]);
    pipeline_groups.push_back(groups[PG_RGEN_SHADE_SURFACE]);
    pipeline_groups.push_back(groups[PG_RGEN_SHADE_SURFACE_RAYTRACE]);
    pipeline_groups.push_back(groups[PG_CALL_SVM_AO]);
    pipeline_groups.push_back(groups[PG_CALL_SVM_BEVEL]);
    pipeline_groups.push_back(groups[PG_RGEN_SHADE_SURFACE_MNEE]);
    pipeline_groups.push_back(groups[PG_RGEN_SHADE_VOLUME]);
    pipeline_groups.push_back(groups[PG_RGEN_SHADE_SHADOW]);
    pipeline_groups.push_back(groups[PG_RGEN_SHADE_DEDICATED_LIGHT]);
    pipeline_groups.push_back(groups[PG_RGEN_EVAL_DISPLACE]);
    pipeline_groups.push_back(groups[PG_RGEN_EVAL_BACKGROUND]);
    pipeline_groups.push_back(groups[PG_RGEN_EVAL_CURVE_SHADOW_TRANSPARENCY]);
    pipeline_groups.push_back(groups[PG_RGEN_INIT_FROM_CAMERA]);

    for (const OptixProgramGroup &group : osl_groups) {
      if (group != nullptr) {
        pipeline_groups.push_back(group);
      }
    }

    optix_assert(optixPipelineCreate(context,
                                     &pipeline_options,
                                     &link_options,
                                     pipeline_groups.data(),
                                     pipeline_groups.size(),
                                     nullptr,
                                     nullptr,
                                     &pipelines[PIP_SHADE]));

    /* Get program stack sizes. */
    OptixStackSizes stack_size[NUM_PROGRAM_GROUPS] = {};
    vector<OptixStackSizes> osl_stack_size(osl_groups.size());

    for (int i = 0; i < NUM_PROGRAM_GROUPS; ++i) {
      optix_assert(optixProgramGroupGetStackSize(groups[i], &stack_size[i], nullptr));
    }
    for (size_t i = 0; i < osl_groups.size(); ++i) {
      if (osl_groups[i] != nullptr) {
        optix_assert(optixProgramGroupGetStackSize(
            osl_groups[i], &osl_stack_size[i], pipelines[PIP_SHADE]));
      }
    }

    const unsigned int css = std::max(stack_size[PG_RGEN_SHADE_SURFACE_RAYTRACE].cssRG,
                                      stack_size[PG_RGEN_SHADE_SURFACE_MNEE].cssRG);
    unsigned int dss = std::max(stack_size[PG_CALL_SVM_AO].dssDC,
                                stack_size[PG_CALL_SVM_BEVEL].dssDC);
    for (unsigned int i = 0; i < osl_stack_size.size(); ++i) {
      dss = std::max(dss, osl_stack_size[i].dssDC);
    }

    optix_assert(optixPipelineSetStackSize(
        pipelines[PIP_SHADE], 0, dss, css, pipeline_options.usesMotionBlur ? 3 : 2));
  }

  /* Copy colorsystem data from OSL to the device. */
  {
    /* The interface here is somewhat complex, since the colorsystem contains strings whose
     * representation is different between CPU and GPU.
     * OSL's ColorSystem type therefore consists of two parts: First the "fixed data" (e.g. floats)
     * that is identical between both, and then the strings.
     * To perform this conversion, in addition to the pointer to the CPU data, we query two sizes:
     * The total size of the CPU data and the number of strings. */
    uint8_t *cpu_data = nullptr;
    size_t cpu_data_sizes[2] = {0, 0};
    osl_globals.ss->getattribute("colorsystem", OSL::TypeDesc::PTR, &cpu_data);
    osl_globals.ss->getattribute(
        "colorsystem:sizes", TypeDesc(TypeDesc::LONGLONG, 2), (void *)cpu_data_sizes);

    size_t cpu_full_size = cpu_data_sizes[0];
    size_t num_strings = cpu_data_sizes[1];
    size_t fixed_data_size = cpu_full_size - sizeof(ustringhash) * num_strings;

    /* Allocate a buffer to fit the fixed data, as well as all the strings in GPU form. */
    uint8_t *gpu_data = osl_colorsystem.alloc(fixed_data_size + sizeof(size_t) * num_strings);

    /* Copy the fixed data as-is. */
    memcpy(gpu_data, cpu_data, fixed_data_size);

    /* Convert each string to GPU format. */
    ustringhash *cpu_strings = reinterpret_cast<ustringhash *>(cpu_data + fixed_data_size);
    size_t *gpu_strings = reinterpret_cast<size_t *>(gpu_data + fixed_data_size);
    for (int i = 0; i < num_strings; i++) {
      gpu_strings[i] = cpu_strings[i].hash();
    }

    /* Copy GPU form of the data to the device. */
    osl_colorsystem.copy_to_device();

    update_launch_params(offsetof(KernelParamsOptiX, osl_colorsystem),
                         &osl_colorsystem.device_pointer,
                         sizeof(device_ptr));
  }

  return !have_error();
#  else
  return false;
#  endif
}

OSLGlobals *OptiXDevice::get_cpu_osl_memory()
{
#  ifdef WITH_OSL
  return &osl_globals;
#  else
  return nullptr;
#  endif
}

bool OptiXDevice::build_optix_bvh(BVHOptiX *bvh,
                                  OptixBuildOperation operation,
                                  const OptixBuildInput &build_input,
                                  const uint16_t num_motion_steps)
{
  /* Allocate and build acceleration structures only one at a time, to prevent parallel builds
   * from running out of memory (since both original and compacted acceleration structure memory
   * may be allocated at the same time for the duration of this function). The builds would
   * otherwise happen on the same CUDA stream anyway. */
  static thread_mutex mutex;
  thread_scoped_lock lock(mutex);

  const CUDAContextScope scope(this);

  bool use_fast_trace_bvh = (bvh->params.bvh_type == BVH_TYPE_STATIC);

  /* Compute memory usage. */
  OptixAccelBufferSizes sizes = {};
  OptixAccelBuildOptions options = {};
  options.operation = operation;
  if (build_input.type == OPTIX_BUILD_INPUT_TYPE_CURVES) {
    /* The build flags have to match the ones used to query the built-in curve intersection
     * program (see optixBuiltinISModuleGet above) */
    options.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE | OPTIX_BUILD_FLAG_ALLOW_COMPACTION |
                         OPTIX_BUILD_FLAG_ALLOW_UPDATE;
    use_fast_trace_bvh = true;
  }
  else if (use_fast_trace_bvh) {
    LOG_INFO << "Using fast to trace OptiX BVH";
    options.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE | OPTIX_BUILD_FLAG_ALLOW_COMPACTION;
  }
  else {
    LOG_INFO << "Using fast to update OptiX BVH";
    options.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_BUILD | OPTIX_BUILD_FLAG_ALLOW_UPDATE;
  }

  options.motionOptions.numKeys = num_motion_steps;
  options.motionOptions.flags = OPTIX_MOTION_FLAG_START_VANISH | OPTIX_MOTION_FLAG_END_VANISH;
  options.motionOptions.timeBegin = 0.0f;
  options.motionOptions.timeEnd = 1.0f;

  optix_assert(optixAccelComputeMemoryUsage(context, &options, &build_input, 1, &sizes));

  /* Allocate required output buffers. */
  device_only_memory<char> temp_mem(this, "optix temp as build mem", true);
  temp_mem.alloc_to_device(align_up(sizes.tempSizeInBytes, 8) + 8);
  if (!temp_mem.device_pointer) {
    /* Make sure temporary memory allocation succeeded. */
    return false;
  }

  /* Acceleration structure memory has to be allocated on the device (not allowed on the host). */
  device_only_memory<char> &out_data = *bvh->as_data;
  if (operation == OPTIX_BUILD_OPERATION_BUILD) {
    assert(out_data.device == this);
    out_data.alloc_to_device(sizes.outputSizeInBytes);
    if (!out_data.device_pointer) {
      return false;
    }
  }
  else {
    assert(out_data.device_pointer && out_data.device_size >= sizes.outputSizeInBytes);
  }

  /* Finally build the acceleration structure. */
  OptixAccelEmitDesc compacted_size_prop = {};
  compacted_size_prop.type = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;
  /* A tiny space was allocated for this property at the end of the temporary buffer above.
   * Make sure this pointer is 8-byte aligned. */
  compacted_size_prop.result = align_up(temp_mem.device_pointer + sizes.tempSizeInBytes, 8);

  OptixTraversableHandle out_handle = 0;
  optix_assert(optixAccelBuild(context,
                               nullptr,
                               &options,
                               &build_input,
                               1,
                               temp_mem.device_pointer,
                               sizes.tempSizeInBytes,
                               out_data.device_pointer,
                               sizes.outputSizeInBytes,
                               &out_handle,
                               use_fast_trace_bvh ? &compacted_size_prop : nullptr,
                               use_fast_trace_bvh ? 1 : 0));
  bvh->traversable_handle = static_cast<uint64_t>(out_handle);

  /* Wait for all operations to finish. */
  cuda_assert(cuStreamSynchronize(nullptr));

  /* Compact acceleration structure to save memory (do not do this in viewport for faster builds).
   */
  if (use_fast_trace_bvh) {
    uint64_t compacted_size = sizes.outputSizeInBytes;
    cuda_assert(cuMemcpyDtoH(&compacted_size, compacted_size_prop.result, sizeof(compacted_size)));

    /* Temporary memory is no longer needed, so free it now to make space. */
    temp_mem.free();

    /* There is no point compacting if the size does not change. */
    if (compacted_size < sizes.outputSizeInBytes) {
      device_only_memory<char> compacted_data(this, "optix compacted as", false);
      compacted_data.alloc_to_device(compacted_size);
      if (!compacted_data.device_pointer) {
        /* Do not compact if memory allocation for compacted acceleration structure fails.
         * Can just use the uncompacted one then, so succeed here regardless. */
        return !have_error();
      }

      optix_assert(optixAccelCompact(context,
                                     nullptr,
                                     out_handle,
                                     compacted_data.device_pointer,
                                     compacted_size,
                                     &out_handle));
      bvh->traversable_handle = static_cast<uint64_t>(out_handle);

      /* Wait for compaction to finish. */
      cuda_assert(cuStreamSynchronize(nullptr));

      std::swap(out_data.device_size, compacted_data.device_size);
      std::swap(out_data.device_pointer, compacted_data.device_pointer);
      /* Original acceleration structure memory is freed when 'compacted_data' goes out of scope.
       */
    }
  }

  return !have_error();
}

void OptiXDevice::build_bvh(BVH *bvh, Progress &progress, bool refit)
{
  const bool use_fast_trace_bvh = (bvh->params.bvh_type == BVH_TYPE_STATIC);

  free_bvh_memory_delayed();

  BVHOptiX *const bvh_optix = static_cast<BVHOptiX *>(bvh);

  progress.set_substatus("Building OptiX acceleration structure");

  if (!bvh->params.top_level) {
    assert(bvh->objects.size() == 1 && bvh->geometry.size() == 1);

    /* Refit is only possible in viewport for now (because AS is built with
     * OPTIX_BUILD_FLAG_ALLOW_UPDATE only there, see above). */
    OptixBuildOperation operation = OPTIX_BUILD_OPERATION_BUILD;
    if (refit && !use_fast_trace_bvh) {
      assert(bvh_optix->traversable_handle != 0);
      operation = OPTIX_BUILD_OPERATION_UPDATE;
    }
    else {
      bvh_optix->as_data->free();
      bvh_optix->traversable_handle = 0;
    }

    /* Build bottom level acceleration structures (BLAS). */
    Geometry *const geom = bvh->geometry[0];
    if (geom->is_hair()) {
      /* Build BLAS for curve primitives. */
      Hair *const hair = static_cast<Hair *const>(geom);
      if (hair->num_segments() == 0) {
        return;
      }

      const size_t num_segments = hair->num_segments();

      size_t num_motion_steps = 1;
      Attribute *motion_keys = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
      if (pipeline_options.usesMotionBlur && hair->get_use_motion_blur() && motion_keys) {
        num_motion_steps = hair->get_motion_steps();
      }

      device_vector<OptixAabb> aabb_data(this, "optix temp aabb data", MEM_READ_ONLY);
      device_vector<int> index_data(this, "optix temp index data", MEM_READ_ONLY);
      device_vector<float4> vertex_data(this, "optix temp vertex data", MEM_READ_ONLY);
      /* Four control points for each curve segment. */
      size_t num_vertices = num_segments * 4;
      if (hair->curve_shape == CURVE_THICK) {
        num_vertices = hair->num_keys() + 2 * hair->num_curves();
        index_data.alloc(num_segments);
        vertex_data.alloc(num_vertices * num_motion_steps);
      }
      else {
        aabb_data.alloc(num_segments * num_motion_steps);
      }

      /* Get AABBs for each motion step. */
      for (size_t step = 0; step < num_motion_steps; ++step) {
        /* The center step for motion vertices is not stored in the attribute. */
        const float3 *keys = hair->get_curve_keys().data();
        size_t center_step = (num_motion_steps - 1) / 2;
        if (step != center_step) {
          size_t attr_offset = (step > center_step) ? step - 1 : step;
          /* Technically this is a float4 array, but sizeof(float3) == sizeof(float4). */
          keys = motion_keys->data_float3() + attr_offset * hair->get_curve_keys().size();
        }

        if (hair->curve_shape == CURVE_THICK) {
          for (size_t curve_index = 0, segment_index = 0, vertex_index = step * num_vertices;
               curve_index < hair->num_curves();
               ++curve_index)
          {
            const Hair::Curve curve = hair->get_curve(curve_index);
            const array<float> &curve_radius = hair->get_curve_radius();

            const int first_key_index = curve.first_key;
            {
              vertex_data[vertex_index++] = make_float4(keys[first_key_index].x,
                                                        keys[first_key_index].y,
                                                        keys[first_key_index].z,
                                                        curve_radius[first_key_index]);
            }

            for (int k = 0; k < curve.num_segments(); ++k) {
              if (step == 0) {
                index_data[segment_index++] = vertex_index - 1;
              }
              vertex_data[vertex_index++] = make_float4(keys[first_key_index + k].x,
                                                        keys[first_key_index + k].y,
                                                        keys[first_key_index + k].z,
                                                        curve_radius[first_key_index + k]);
            }

            const int last_key_index = first_key_index + curve.num_keys - 1;
            {
              vertex_data[vertex_index++] = make_float4(keys[last_key_index].x,
                                                        keys[last_key_index].y,
                                                        keys[last_key_index].z,
                                                        curve_radius[last_key_index]);
              vertex_data[vertex_index++] = make_float4(keys[last_key_index].x,
                                                        keys[last_key_index].y,
                                                        keys[last_key_index].z,
                                                        curve_radius[last_key_index]);
            }
          }
        }
        else {
          for (size_t curve_index = 0, i = 0; curve_index < hair->num_curves(); ++curve_index) {
            const Hair::Curve curve = hair->get_curve(curve_index);

            for (int segment = 0; segment < curve.num_segments(); ++segment, ++i) {
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

      /* Upload AABB data to GPU. */
      aabb_data.copy_to_device();
      index_data.copy_to_device();
      vertex_data.copy_to_device();

      vector<device_ptr> aabb_ptrs;
      aabb_ptrs.reserve(num_motion_steps);
      vector<device_ptr> width_ptrs;
      vector<device_ptr> vertex_ptrs;
      width_ptrs.reserve(num_motion_steps);
      vertex_ptrs.reserve(num_motion_steps);
      for (size_t step = 0; step < num_motion_steps; ++step) {
        aabb_ptrs.push_back(aabb_data.device_pointer + step * num_segments * sizeof(OptixAabb));
        const device_ptr base_ptr = vertex_data.device_pointer +
                                    step * num_vertices * sizeof(float4);
        width_ptrs.push_back(base_ptr + 3 * sizeof(float)); /* Offset by vertex size. */
        vertex_ptrs.push_back(base_ptr);
      }

      /* Force a single any-hit call, so shadow record-all behavior works correctly. */
      unsigned int build_flags = OPTIX_GEOMETRY_FLAG_REQUIRE_SINGLE_ANYHIT_CALL;
      OptixBuildInput build_input = {};
      if (hair->curve_shape == CURVE_THICK) {
        build_input.type = OPTIX_BUILD_INPUT_TYPE_CURVES;
        build_input.curveArray.curveType = OPTIX_PRIMITIVE_TYPE_ROUND_CATMULLROM;
        build_input.curveArray.numPrimitives = num_segments;
        build_input.curveArray.vertexBuffers = (CUdeviceptr *)vertex_ptrs.data();
        build_input.curveArray.numVertices = num_vertices;
        build_input.curveArray.vertexStrideInBytes = sizeof(float4);
        build_input.curveArray.widthBuffers = (CUdeviceptr *)width_ptrs.data();
        build_input.curveArray.widthStrideInBytes = sizeof(float4);
        build_input.curveArray.indexBuffer = (CUdeviceptr)index_data.device_pointer;
        build_input.curveArray.indexStrideInBytes = sizeof(int);
        build_input.curveArray.flag = build_flags;
        build_input.curveArray.primitiveIndexOffset = hair->curve_segment_offset;
      }
      else {
        /* Disable visibility test any-hit program, since it is already checked during
         * intersection. Those trace calls that require any-hit can force it with a ray flag. */
        build_flags |= OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT;

        build_input.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
        build_input.customPrimitiveArray.aabbBuffers = (CUdeviceptr *)aabb_ptrs.data();
        build_input.customPrimitiveArray.numPrimitives = num_segments;
        build_input.customPrimitiveArray.strideInBytes = sizeof(OptixAabb);
        build_input.customPrimitiveArray.flags = &build_flags;
        build_input.customPrimitiveArray.numSbtRecords = 1;
        build_input.customPrimitiveArray.primitiveIndexOffset = hair->curve_segment_offset;
      }

      if (!build_optix_bvh(bvh_optix, operation, build_input, num_motion_steps)) {
        progress.set_error("Failed to build OptiX acceleration structure");
      }
    }
    else if (geom->is_mesh() || geom->is_volume()) {
      /* Build BLAS for triangle primitives. */
      Mesh *const mesh = static_cast<Mesh *const>(geom);
      if (mesh->num_triangles() == 0) {
        return;
      }

      const size_t num_verts = mesh->get_verts().size();

      size_t num_motion_steps = 1;
      Attribute *motion_keys = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
      if (pipeline_options.usesMotionBlur && mesh->get_use_motion_blur() && motion_keys) {
        num_motion_steps = mesh->get_motion_steps();
      }

      device_vector<int> index_data(this, "optix temp index data", MEM_READ_ONLY);
      index_data.alloc(mesh->get_triangles().size());
      memcpy(index_data.data(),
             mesh->get_triangles().data(),
             mesh->get_triangles().size() * sizeof(int));
      device_vector<float4> vertex_data(this, "optix temp vertex data", MEM_READ_ONLY);
      vertex_data.alloc(num_verts * num_motion_steps);

      for (size_t step = 0; step < num_motion_steps; ++step) {
        const float3 *verts = mesh->get_verts().data();

        size_t center_step = (num_motion_steps - 1) / 2;
        /* The center step for motion vertices is not stored in the attribute. */
        if (step != center_step) {
          verts = motion_keys->data_float3() + (step > center_step ? step - 1 : step) * num_verts;
        }

        memcpy(vertex_data.data() + num_verts * step, verts, num_verts * sizeof(float3));
      }

      /* Upload triangle data to GPU. */
      index_data.copy_to_device();
      vertex_data.copy_to_device();

      vector<device_ptr> vertex_ptrs;
      vertex_ptrs.reserve(num_motion_steps);
      for (size_t step = 0; step < num_motion_steps; ++step) {
        vertex_ptrs.push_back(vertex_data.device_pointer + num_verts * step * sizeof(float3));
      }

      /* Force a single any-hit call, so shadow record-all behavior works correctly. */
      unsigned int build_flags = OPTIX_GEOMETRY_FLAG_REQUIRE_SINGLE_ANYHIT_CALL;
      OptixBuildInput build_input = {};
      build_input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
      build_input.triangleArray.vertexBuffers = (CUdeviceptr *)vertex_ptrs.data();
      build_input.triangleArray.numVertices = num_verts;
      build_input.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
      build_input.triangleArray.vertexStrideInBytes = sizeof(float4);
      build_input.triangleArray.indexBuffer = index_data.device_pointer;
      build_input.triangleArray.numIndexTriplets = mesh->num_triangles();
      build_input.triangleArray.indexFormat = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
      build_input.triangleArray.indexStrideInBytes = 3 * sizeof(int);
      build_input.triangleArray.flags = &build_flags;
      /* The SBT does not store per primitive data since Cycles already allocates separate
       * buffers for that purpose. OptiX does not allow this to be zero though, so just pass in
       * one and rely on that having the same meaning in this case. */
      build_input.triangleArray.numSbtRecords = 1;
      build_input.triangleArray.primitiveIndexOffset = mesh->prim_offset;

      if (!build_optix_bvh(bvh_optix, operation, build_input, num_motion_steps)) {
        progress.set_error("Failed to build OptiX acceleration structure");
      }
    }
    else if (geom->is_pointcloud()) {
      /* Build BLAS for points primitives. */
      PointCloud *const pointcloud = static_cast<PointCloud *const>(geom);
      const size_t num_points = pointcloud->num_points();
      if (num_points == 0) {
        return;
      }

      size_t num_motion_steps = 1;
      Attribute *motion_points = pointcloud->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
      if (pipeline_options.usesMotionBlur && pointcloud->get_use_motion_blur() && motion_points) {
        num_motion_steps = pointcloud->get_motion_steps();
      }

      device_vector<OptixAabb> aabb_data(this, "optix temp aabb data", MEM_READ_ONLY);
      aabb_data.alloc(num_points * num_motion_steps);

      /* Get AABBs for each motion step. */
      for (size_t step = 0; step < num_motion_steps; ++step) {
        /* The center step for motion vertices is not stored in the attribute. */
        size_t center_step = (num_motion_steps - 1) / 2;

        if (step == center_step) {
          const float3 *points = pointcloud->get_points().data();
          const float *radius = pointcloud->get_radius().data();

          for (size_t i = 0; i < num_points; ++i) {
            const PointCloud::Point point = pointcloud->get_point(i);
            BoundBox bounds = BoundBox::empty;
            point.bounds_grow(points, radius, bounds);

            const size_t index = step * num_points + i;
            aabb_data[index].minX = bounds.min.x;
            aabb_data[index].minY = bounds.min.y;
            aabb_data[index].minZ = bounds.min.z;
            aabb_data[index].maxX = bounds.max.x;
            aabb_data[index].maxY = bounds.max.y;
            aabb_data[index].maxZ = bounds.max.z;
          }
        }
        else {
          size_t attr_offset = (step > center_step) ? step - 1 : step;
          const float4 *points = motion_points->data_float4() + attr_offset * num_points;

          for (size_t i = 0; i < num_points; ++i) {
            const PointCloud::Point point = pointcloud->get_point(i);
            BoundBox bounds = BoundBox::empty;
            point.bounds_grow(points[i], bounds);

            const size_t index = step * num_points + i;
            aabb_data[index].minX = bounds.min.x;
            aabb_data[index].minY = bounds.min.y;
            aabb_data[index].minZ = bounds.min.z;
            aabb_data[index].maxX = bounds.max.x;
            aabb_data[index].maxY = bounds.max.y;
            aabb_data[index].maxZ = bounds.max.z;
          }
        }
      }

      /* Upload AABB data to GPU. */
      aabb_data.copy_to_device();

      vector<device_ptr> aabb_ptrs;
      aabb_ptrs.reserve(num_motion_steps);
      for (size_t step = 0; step < num_motion_steps; ++step) {
        aabb_ptrs.push_back(aabb_data.device_pointer + step * num_points * sizeof(OptixAabb));
      }

      /* Disable visibility test any-hit program, since it is already checked during
       * intersection. Those trace calls that require anyhit can force it with a ray flag.
       * For those, force a single any-hit call, so shadow record-all behavior works correctly. */
      unsigned int build_flags = OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT |
                                 OPTIX_GEOMETRY_FLAG_REQUIRE_SINGLE_ANYHIT_CALL;
      OptixBuildInput build_input = {};
      build_input.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
      build_input.customPrimitiveArray.aabbBuffers = (CUdeviceptr *)aabb_ptrs.data();
      build_input.customPrimitiveArray.numPrimitives = num_points;
      build_input.customPrimitiveArray.strideInBytes = sizeof(OptixAabb);
      build_input.customPrimitiveArray.flags = &build_flags;
      build_input.customPrimitiveArray.numSbtRecords = 1;
      build_input.customPrimitiveArray.primitiveIndexOffset = pointcloud->prim_offset;

      if (!build_optix_bvh(bvh_optix, operation, build_input, num_motion_steps)) {
        progress.set_error("Failed to build OptiX acceleration structure");
      }
    }
  }
  else {
    unsigned int num_instances = 0;
    unsigned int max_num_instances = 0xFFFFFFFF;

    bvh_optix->as_data->free();
    bvh_optix->traversable_handle = 0;
    bvh_optix->motion_transform_data->free();

    optixDeviceContextGetProperty(context,
                                  OPTIX_DEVICE_PROPERTY_LIMIT_MAX_INSTANCE_ID,
                                  &max_num_instances,
                                  sizeof(max_num_instances));
    /* Do not count first bit, which is used to distinguish instanced and non-instanced objects. */
    max_num_instances >>= 1;
    if (bvh->objects.size() > max_num_instances) {
      progress.set_error(
          "Failed to build OptiX acceleration structure because there are too many instances");
      return;
    }

    /* Fill instance descriptions. */
    device_vector<OptixInstance> instances(this, "optix tlas instances", MEM_READ_ONLY);
    instances.alloc(bvh->objects.size());

    /* Calculate total motion transform size and allocate memory for them. */
    size_t motion_transform_offset = 0;
    if (pipeline_options.usesMotionBlur) {
      size_t total_motion_transform_size = 0;
      for (Object *const ob : bvh->objects) {
        if (ob->is_traceable() && ob->use_motion()) {
          total_motion_transform_size = align_up(total_motion_transform_size,
                                                 OPTIX_TRANSFORM_BYTE_ALIGNMENT);
          const size_t motion_keys = max(ob->get_motion().size(), (size_t)2) - 2;
          total_motion_transform_size = total_motion_transform_size +
                                        sizeof(OptixSRTMotionTransform) +
                                        motion_keys * sizeof(OptixSRTData);
        }
      }

      assert(bvh_optix->motion_transform_data->device == this);
      bvh_optix->motion_transform_data->alloc_to_device(total_motion_transform_size);
    }

    for (Object *ob : bvh->objects) {
      /* Skip non-traceable objects. */
      if (!ob->is_traceable()) {
        continue;
      }

      BVHOptiX *const blas = static_cast<BVHOptiX *>(ob->get_geometry()->bvh.get());
      OptixTraversableHandle handle = blas->traversable_handle;
      if (handle == 0) {
        continue;
      }

      OptixInstance &instance = instances[num_instances++];
      memset(&instance, 0, sizeof(instance));

      /* Clear transform to identity matrix. */
      instance.transform[0] = 1.0f;
      instance.transform[5] = 1.0f;
      instance.transform[10] = 1.0f;

      /* Set user instance ID to object index. */
      instance.instanceId = ob->get_device_index();

      /* Add some of the object visibility bits to the mask.
       * __prim_visibility contains the combined visibility bits of all instances, so is not
       * reliable if they differ between instances. But the OptiX visibility mask can only contain
       * 8 bits, so have to trade-off here and select just a few important ones.
       */
      instance.visibilityMask = ob->visibility_for_tracing() & 0xFF;

      /* Have to have at least one bit in the mask, or else instance would always be culled. */
      if (0 == instance.visibilityMask) {
        instance.visibilityMask = 0xFF;
      }

      if (ob->get_geometry()->is_hair() &&
          static_cast<const Hair *>(ob->get_geometry())->curve_shape == CURVE_THICK)
      {
        if (pipeline_options.usesMotionBlur && ob->get_geometry()->has_motion_blur()) {
          /* Select between motion blur and non-motion blur built-in intersection module. */
          instance.sbtOffset = PG_HITD_MOTION - PG_HITD;
        }
      }
      else if (ob->get_geometry()->is_pointcloud()) {
        /* Use the hit group that has an intersection program for point clouds. */
        instance.sbtOffset = PG_HITD_POINTCLOUD - PG_HITD;

        /* Also skip point clouds in local trace calls. */
        instance.visibilityMask |= 4;
      }
      {
        /* Can disable __anyhit__kernel_optix_visibility_test by default (except for thick curves,
         * since it needs to filter out end-caps there).
         *
         * It is enabled where necessary (visibility mask exceeds 8 bits or the other any-hit
         * programs like __anyhit__kernel_optix_shadow_all_hit) via OPTIX_RAY_FLAG_ENFORCE_ANYHIT.
         */
        instance.flags = OPTIX_INSTANCE_FLAG_DISABLE_ANYHIT;
      }

      /* Insert motion traversable if object has motion. */
      if (pipeline_options.usesMotionBlur && ob->use_motion()) {
        size_t motion_keys = max(ob->get_motion().size(), (size_t)2) - 2;
        size_t motion_transform_size = sizeof(OptixSRTMotionTransform) +
                                       motion_keys * sizeof(OptixSRTData);

        const CUDAContextScope scope(this);

        motion_transform_offset = align_up(motion_transform_offset,
                                           OPTIX_TRANSFORM_BYTE_ALIGNMENT);
        CUdeviceptr motion_transform_gpu = bvh_optix->motion_transform_data->device_pointer +
                                           motion_transform_offset;
        motion_transform_offset += motion_transform_size;

        /* Allocate host side memory for motion transform and fill it with transform data. */
        array<uint8_t> motion_transform_storage(motion_transform_size);
        OptixSRTMotionTransform *motion_transform = reinterpret_cast<OptixSRTMotionTransform *>(
            motion_transform_storage.data());
        motion_transform->child = handle;
        motion_transform->motionOptions.numKeys = ob->get_motion().size();
        motion_transform->motionOptions.flags = OPTIX_MOTION_FLAG_NONE;
        motion_transform->motionOptions.timeBegin = 0.0f;
        motion_transform->motionOptions.timeEnd = 1.0f;

        OptixSRTData *const srt_data = motion_transform->srtData;
        array<DecomposedTransform> decomp(ob->get_motion().size());
        transform_motion_decompose(
            decomp.data(), ob->get_motion().data(), ob->get_motion().size());

        for (size_t i = 0; i < ob->get_motion().size(); ++i) {
          /* Scale. */
          srt_data[i].sx = decomp[i].y.w; /* scale.x.x */
          srt_data[i].sy = decomp[i].z.w; /* scale.y.y */
          srt_data[i].sz = decomp[i].w.w; /* scale.z.z */

          /* Shear. */
          srt_data[i].a = decomp[i].z.x; /* scale.x.y */
          srt_data[i].b = decomp[i].z.y; /* scale.x.z */
          srt_data[i].c = decomp[i].w.x; /* scale.y.z */
          assert(decomp[i].z.z == 0.0f); /* scale.y.x */
          assert(decomp[i].w.y == 0.0f); /* scale.z.x */
          assert(decomp[i].w.z == 0.0f); /* scale.z.y */

          /* Pivot point. */
          srt_data[i].pvx = 0.0f;
          srt_data[i].pvy = 0.0f;
          srt_data[i].pvz = 0.0f;

          /* Rotation. */
          srt_data[i].qx = decomp[i].x.x;
          srt_data[i].qy = decomp[i].x.y;
          srt_data[i].qz = decomp[i].x.z;
          srt_data[i].qw = decomp[i].x.w;

          /* Translation. */
          srt_data[i].tx = decomp[i].y.x;
          srt_data[i].ty = decomp[i].y.y;
          srt_data[i].tz = decomp[i].y.z;
        }

        /* Upload motion transform to GPU. */
        cuMemcpyHtoD(motion_transform_gpu, motion_transform, motion_transform_size);
        motion_transform = nullptr;
        motion_transform_storage.clear();

        /* Get traversable handle to motion transform. */
        optixConvertPointerToTraversableHandle(context,
                                               motion_transform_gpu,
                                               OPTIX_TRAVERSABLE_TYPE_SRT_MOTION_TRANSFORM,
                                               &instance.traversableHandle);
      }
      else {
        instance.traversableHandle = handle;

        if (ob->get_geometry()->is_instanced()) {
          /* Set transform matrix. */
          memcpy(instance.transform, &ob->get_tfm(), sizeof(instance.transform));
        }
      }
    }

    /* Upload instance descriptions. */
    instances.resize(num_instances);
    instances.copy_to_device();

    /* Build top-level acceleration structure (TLAS) */
    OptixBuildInput build_input = {};
    build_input.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
    build_input.instanceArray.instances = instances.device_pointer;
    build_input.instanceArray.numInstances = num_instances;

    if (!build_optix_bvh(bvh_optix, OPTIX_BUILD_OPERATION_BUILD, build_input, 0)) {
      progress.set_error("Failed to build OptiX acceleration structure");
    }
    tlas_handle = bvh_optix->traversable_handle;
  }
}

void OptiXDevice::release_bvh(BVH *bvh)
{
  thread_scoped_lock lock(delayed_free_bvh_mutex);
  /* Do delayed free of BVH memory, since geometry holding BVH might be deleted
   * while GPU is still rendering. */
  BVHOptiX *const bvh_optix = static_cast<BVHOptiX *>(bvh);

  delayed_free_bvh_memory.emplace_back(std::move(bvh_optix->as_data));
  delayed_free_bvh_memory.emplace_back(std::move(bvh_optix->motion_transform_data));
  bvh_optix->traversable_handle = 0;
}

void OptiXDevice::free_bvh_memory_delayed()
{
  thread_scoped_lock lock(delayed_free_bvh_mutex);
  delayed_free_bvh_memory.free_memory();
}

void OptiXDevice::const_copy_to(const char *name, void *host, const size_t size)
{
  /* Set constant memory for CUDA module. */
  CUDADevice::const_copy_to(name, host, size);

  if (strcmp(name, "data") == 0) {
    assert(size <= sizeof(KernelData));

    /* Update traversable handle (since it is different for each device on multi devices). */
    KernelData *const data = (KernelData *)host;
    *(OptixTraversableHandle *)&data->device_bvh = tlas_handle;

    update_launch_params(offsetof(KernelParamsOptiX, data), host, size);
    return;
  }

  /* Update data storage pointers in launch parameters. */
#  define KERNEL_DATA_ARRAY(data_type, data_name) \
    if (strcmp(name, #data_name) == 0) { \
      update_launch_params(offsetof(KernelParamsOptiX, data_name), host, size); \
      return; \
    }
  KERNEL_DATA_ARRAY(IntegratorStateGPU, integrator_state)
#  include "kernel/data_arrays.h"
#  undef KERNEL_DATA_ARRAY
}

void OptiXDevice::update_launch_params(const size_t offset, void *data, const size_t data_size)
{
  const CUDAContextScope scope(this);

  cuda_assert(cuMemcpyHtoD(launch_params.device_pointer + offset, data, data_size));
}

CCL_NAMESPACE_END

#endif /* WITH_OPTIX */
