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

#  include "device/optix/device_impl.h"

#  include "bvh/bvh.h"
#  include "bvh/optix.h"

#  include "integrator/pass_accessor_gpu.h"

#  include "scene/hair.h"
#  include "scene/mesh.h"
#  include "scene/object.h"
#  include "scene/pass.h"
#  include "scene/pointcloud.h"
#  include "scene/scene.h"

#  include "util/debug.h"
#  include "util/log.h"
#  include "util/md5.h"
#  include "util/path.h"
#  include "util/progress.h"
#  include "util/time.h"

#  undef __KERNEL_CPU__
#  define __KERNEL_OPTIX__
#  include "kernel/device/optix/globals.h"

#  include <optix_denoiser_tiling.h>

CCL_NAMESPACE_BEGIN

OptiXDevice::Denoiser::Denoiser(OptiXDevice *device)
    : device(device), queue(device), state(device, "__denoiser_state", true)
{
}

OptiXDevice::OptiXDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler)
    : CUDADevice(info, stats, profiler),
      sbt_data(this, "__sbt", MEM_READ_ONLY),
      launch_params(this, "__params", false),
      denoiser_(this)
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
  if (DebugFlags().optix.use_debug) {
    VLOG(1) << "Using OptiX debug mode.";
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
  if (optix_module != NULL) {
    optixModuleDestroy(optix_module);
  }
  for (unsigned int i = 0; i < 2; ++i) {
    if (builtin_modules[i] != NULL) {
      optixModuleDestroy(builtin_modules[i]);
    }
  }
  for (unsigned int i = 0; i < NUM_PIPELINES; ++i) {
    if (pipelines[i] != NULL) {
      optixPipelineDestroy(pipelines[i]);
    }
  }

  /* Make sure denoiser is destroyed before device context! */
  if (denoiser_.optix_denoiser != nullptr) {
    optixDenoiserDestroy(denoiser_.optix_denoiser);
  }

  optixDeviceContextDestroy(context);
}

unique_ptr<DeviceQueue> OptiXDevice::gpu_queue_create()
{
  return make_unique<OptiXDeviceQueue>(this);
}

BVHLayoutMask OptiXDevice::get_bvh_layout_mask() const
{
  /* OptiX has its own internal acceleration structure format. */
  return BVH_LAYOUT_OPTIX;
}

string OptiXDevice::compile_kernel_get_common_cflags(const uint kernel_features)
{
  string common_cflags = CUDADevice::compile_kernel_get_common_cflags(kernel_features);

  /* Add OptiX SDK include directory to include paths. */
  const char *optix_sdk_path = getenv("OPTIX_ROOT_DIR");
  if (optix_sdk_path) {
    common_cflags += string_printf(" -I\"%s/include\"", optix_sdk_path);
  }

  /* Specialization for shader raytracing. */
  if (kernel_features & KERNEL_FEATURE_NODE_RAYTRACE) {
    common_cflags += " --keep-device-functions";
  }

  return common_cflags;
}

bool OptiXDevice::load_kernels(const uint kernel_features)
{
  if (have_error()) {
    /* Abort early if context creation failed already. */
    return false;
  }

  /* Load CUDA modules because we need some of the utility kernels. */
  if (!CUDADevice::load_kernels(kernel_features)) {
    return false;
  }

  /* Skip creating OptiX module if only doing denoising. */
  if (!(kernel_features & (KERNEL_FEATURE_PATH_TRACING | KERNEL_FEATURE_BAKING))) {
    return true;
  }

  const CUDAContextScope scope(this);

  /* Unload existing OptiX module and pipelines first. */
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
#  if OPTIX_ABI_VERSION >= 55
  module_options.payloadTypes = nullptr;
  module_options.numPayloadTypes = 0;
#  endif

  OptixPipelineCompileOptions pipeline_options = {};
  /* Default to no motion blur and two-level graph, since it is the fastest option. */
  pipeline_options.usesMotionBlur = false;
  pipeline_options.traversableGraphFlags =
      OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;
  pipeline_options.numPayloadValues = 6;
  pipeline_options.numAttributeValues = 2; /* u, v */
  pipeline_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;
  pipeline_options.pipelineLaunchParamsVariableName = "__params"; /* See globals.h */

  pipeline_options.usesPrimitiveTypeFlags = OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE;
  if (kernel_features & KERNEL_FEATURE_HAIR) {
    if (kernel_features & KERNEL_FEATURE_HAIR_THICK) {
#  if OPTIX_ABI_VERSION >= 55
      pipeline_options.usesPrimitiveTypeFlags |= OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_CATMULLROM;
#  else
      pipeline_options.usesPrimitiveTypeFlags |= OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_CUBIC_BSPLINE;
#  endif
    }
    else
      pipeline_options.usesPrimitiveTypeFlags |= OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM;
  }
  if (kernel_features & KERNEL_FEATURE_POINTCLOUD) {
    pipeline_options.usesPrimitiveTypeFlags |= OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM;
  }

  /* Keep track of whether motion blur is enabled, so to enable/disable motion in BVH builds
   * This is necessary since objects may be reported to have motion if the Vector pass is
   * active, but may still need to be rendered without motion blur if that isn't active as well. */
  motion_blur = (kernel_features & KERNEL_FEATURE_OBJECT_MOTION) != 0;

  if (motion_blur) {
    pipeline_options.usesMotionBlur = true;
    /* Motion blur can insert motion transforms into the traversal graph.
     * It is no longer a two-level graph then, so need to set flags to allow any configuration. */
    pipeline_options.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_ANY;
  }

  { /* Load and compile PTX module with OptiX kernels. */
    string ptx_data, ptx_filename = path_get((kernel_features & KERNEL_FEATURE_NODE_RAYTRACE) ?
                                                 "lib/kernel_optix_shader_raytrace.ptx" :
                                                 "lib/kernel_optix.ptx");
    if (use_adaptive_compilation() || path_file_size(ptx_filename) == -1) {
      if (!getenv("OPTIX_ROOT_DIR")) {
        set_error(
            "Missing OPTIX_ROOT_DIR environment variable (which must be set with the path to "
            "the Optix SDK to be able to compile Optix kernels on demand).");
        return false;
      }
      ptx_filename = compile_kernel(
          kernel_features,
          (kernel_features & KERNEL_FEATURE_NODE_RAYTRACE) ? "kernel_shader_raytrace" : "kernel",
          "optix",
          true);
    }
    if (ptx_filename.empty() || !path_read_text(ptx_filename, ptx_data)) {
      set_error(string_printf("Failed to load OptiX kernel from '%s'", ptx_filename.c_str()));
      return false;
    }

    const OptixResult result = optixModuleCreateFromPTX(context,
                                                        &module_options,
                                                        &pipeline_options,
                                                        ptx_data.data(),
                                                        ptx_data.size(),
                                                        nullptr,
                                                        0,
                                                        &optix_module);
    if (result != OPTIX_SUCCESS) {
      set_error(string_printf("Failed to load OptiX kernel from '%s' (%s)",
                              ptx_filename.c_str(),
                              optixGetErrorName(result)));
      return false;
    }
  }

  /* Create program groups. */
  OptixProgramGroup groups[NUM_PROGRAM_GROUPS] = {};
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
#  if OPTIX_ABI_VERSION >= 55
      builtin_options.builtinISModuleType = OPTIX_PRIMITIVE_TYPE_ROUND_CATMULLROM;
      builtin_options.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
      builtin_options.curveEndcapFlags = OPTIX_CURVE_ENDCAP_DEFAULT; /* Disable end-caps. */
#  else
      builtin_options.builtinISModuleType = OPTIX_PRIMITIVE_TYPE_ROUND_CUBIC_BSPLINE;
#  endif
      builtin_options.usesMotionBlur = false;

      optix_assert(optixBuiltinISModuleGet(
          context, &module_options, &pipeline_options, &builtin_options, &builtin_modules[0]));

      group_descs[PG_HITD].hitgroup.moduleIS = builtin_modules[0];
      group_descs[PG_HITD].hitgroup.entryFunctionNameIS = nullptr;
      group_descs[PG_HITS].hitgroup.moduleIS = builtin_modules[0];
      group_descs[PG_HITS].hitgroup.entryFunctionNameIS = nullptr;

      if (motion_blur) {
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

  /* Pointclouds */
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

  if (kernel_features & (KERNEL_FEATURE_SUBSURFACE | KERNEL_FEATURE_NODE_RAYTRACE)) {
    /* Add hit group for local intersections. */
    group_descs[PG_HITL].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    group_descs[PG_HITL].hitgroup.moduleAH = optix_module;
    group_descs[PG_HITL].hitgroup.entryFunctionNameAH = "__anyhit__kernel_optix_local_hit";
  }

  /* Shader raytracing replaces some functions with direct callables. */
  if (kernel_features & KERNEL_FEATURE_NODE_RAYTRACE) {
    group_descs[PG_RGEN_SHADE_SURFACE_RAYTRACE].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    group_descs[PG_RGEN_SHADE_SURFACE_RAYTRACE].raygen.module = optix_module;
    group_descs[PG_RGEN_SHADE_SURFACE_RAYTRACE].raygen.entryFunctionName =
        "__raygen__kernel_optix_integrator_shade_surface_raytrace";
    group_descs[PG_CALL_SVM_AO].kind = OPTIX_PROGRAM_GROUP_KIND_CALLABLES;
    group_descs[PG_CALL_SVM_AO].callables.moduleDC = optix_module;
    group_descs[PG_CALL_SVM_AO].callables.entryFunctionNameDC = "__direct_callable__svm_node_ao";
    group_descs[PG_CALL_SVM_BEVEL].kind = OPTIX_PROGRAM_GROUP_KIND_CALLABLES;
    group_descs[PG_CALL_SVM_BEVEL].callables.moduleDC = optix_module;
    group_descs[PG_CALL_SVM_BEVEL].callables.entryFunctionNameDC =
        "__direct_callable__svm_node_bevel";
  }

  optix_assert(optixProgramGroupCreate(
      context, group_descs, NUM_PROGRAM_GROUPS, &group_options, nullptr, 0, groups));

  /* Get program stack sizes. */
  OptixStackSizes stack_size[NUM_PROGRAM_GROUPS] = {};
  /* Set up SBT, which in this case is used only to select between different programs. */
  sbt_data.alloc(NUM_PROGRAM_GROUPS);
  memset(sbt_data.host_pointer, 0, sizeof(SbtRecord) * NUM_PROGRAM_GROUPS);
  for (unsigned int i = 0; i < NUM_PROGRAM_GROUPS; ++i) {
    optix_assert(optixSbtRecordPackHeader(groups[i], &sbt_data[i]));
    optix_assert(optixProgramGroupGetStackSize(groups[i], &stack_size[i]));
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

  if (DebugFlags().optix.use_debug) {
    link_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
  }
  else {
    link_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_NONE;
  }

  if (kernel_features & KERNEL_FEATURE_NODE_RAYTRACE) {
    /* Create shader raytracing pipeline. */
    vector<OptixProgramGroup> pipeline_groups;
    pipeline_groups.reserve(NUM_PROGRAM_GROUPS);
    pipeline_groups.push_back(groups[PG_RGEN_SHADE_SURFACE_RAYTRACE]);
    pipeline_groups.push_back(groups[PG_MISS]);
    pipeline_groups.push_back(groups[PG_HITD]);
    pipeline_groups.push_back(groups[PG_HITS]);
    pipeline_groups.push_back(groups[PG_HITL]);
    pipeline_groups.push_back(groups[PG_HITV]);
    if (motion_blur) {
      pipeline_groups.push_back(groups[PG_HITD_MOTION]);
      pipeline_groups.push_back(groups[PG_HITS_MOTION]);
    }
    if (kernel_features & KERNEL_FEATURE_POINTCLOUD) {
      pipeline_groups.push_back(groups[PG_HITD_POINTCLOUD]);
      pipeline_groups.push_back(groups[PG_HITS_POINTCLOUD]);
    }
    pipeline_groups.push_back(groups[PG_CALL_SVM_AO]);
    pipeline_groups.push_back(groups[PG_CALL_SVM_BEVEL]);

    optix_assert(optixPipelineCreate(context,
                                     &pipeline_options,
                                     &link_options,
                                     pipeline_groups.data(),
                                     pipeline_groups.size(),
                                     nullptr,
                                     0,
                                     &pipelines[PIP_SHADE_RAYTRACE]));

    /* Combine ray generation and trace continuation stack size. */
    const unsigned int css = stack_size[PG_RGEN_SHADE_SURFACE_RAYTRACE].cssRG +
                             link_options.maxTraceDepth * trace_css;
    const unsigned int dss = std::max(stack_size[PG_CALL_SVM_AO].dssDC,
                                      stack_size[PG_CALL_SVM_BEVEL].dssDC);

    /* Set stack size depending on pipeline options. */
    optix_assert(optixPipelineSetStackSize(
        pipelines[PIP_SHADE_RAYTRACE], 0, dss, css, motion_blur ? 3 : 2));
  }

  { /* Create intersection-only pipeline. */
    vector<OptixProgramGroup> pipeline_groups;
    pipeline_groups.reserve(NUM_PROGRAM_GROUPS);
    pipeline_groups.push_back(groups[PG_RGEN_INTERSECT_CLOSEST]);
    pipeline_groups.push_back(groups[PG_RGEN_INTERSECT_SHADOW]);
    pipeline_groups.push_back(groups[PG_RGEN_INTERSECT_SUBSURFACE]);
    pipeline_groups.push_back(groups[PG_RGEN_INTERSECT_VOLUME_STACK]);
    pipeline_groups.push_back(groups[PG_MISS]);
    pipeline_groups.push_back(groups[PG_HITD]);
    pipeline_groups.push_back(groups[PG_HITS]);
    pipeline_groups.push_back(groups[PG_HITL]);
    pipeline_groups.push_back(groups[PG_HITV]);
    if (motion_blur) {
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
                                     0,
                                     &pipelines[PIP_INTERSECT]));

    /* Calculate continuation stack size based on the maximum of all ray generation stack sizes. */
    const unsigned int css =
        std::max(stack_size[PG_RGEN_INTERSECT_CLOSEST].cssRG,
                 std::max(stack_size[PG_RGEN_INTERSECT_SHADOW].cssRG,
                          std::max(stack_size[PG_RGEN_INTERSECT_SUBSURFACE].cssRG,
                                   stack_size[PG_RGEN_INTERSECT_VOLUME_STACK].cssRG))) +
        link_options.maxTraceDepth * trace_css;

    optix_assert(
        optixPipelineSetStackSize(pipelines[PIP_INTERSECT], 0, 0, css, motion_blur ? 3 : 2));
  }

  /* Clean up program group objects. */
  for (unsigned int i = 0; i < NUM_PROGRAM_GROUPS; ++i) {
    optixProgramGroupDestroy(groups[i]);
  }

  return true;
}

/* --------------------------------------------------------------------
 * Buffer denoising.
 */

class OptiXDevice::DenoiseContext {
 public:
  explicit DenoiseContext(OptiXDevice *device, const DeviceDenoiseTask &task)
      : denoise_params(task.params),
        render_buffers(task.render_buffers),
        buffer_params(task.buffer_params),
        guiding_buffer(device, "denoiser guiding passes buffer", true),
        num_samples(task.num_samples)
  {
    num_input_passes = 1;
    if (denoise_params.use_pass_albedo) {
      num_input_passes += 1;
      use_pass_albedo = true;
      pass_denoising_albedo = buffer_params.get_pass_offset(PASS_DENOISING_ALBEDO);
      if (denoise_params.use_pass_normal) {
        num_input_passes += 1;
        use_pass_normal = true;
        pass_denoising_normal = buffer_params.get_pass_offset(PASS_DENOISING_NORMAL);
      }
    }

    if (denoise_params.temporally_stable) {
      prev_output.device_pointer = render_buffers->buffer.device_pointer;

      prev_output.offset = buffer_params.get_pass_offset(PASS_DENOISING_PREVIOUS);

      prev_output.stride = buffer_params.stride;
      prev_output.pass_stride = buffer_params.pass_stride;

      num_input_passes += 1;
      use_pass_flow = true;
      pass_motion = buffer_params.get_pass_offset(PASS_MOTION);
    }

    use_guiding_passes = (num_input_passes - 1) > 0;

    if (use_guiding_passes) {
      if (task.allow_inplace_modification) {
        guiding_params.device_pointer = render_buffers->buffer.device_pointer;

        guiding_params.pass_albedo = pass_denoising_albedo;
        guiding_params.pass_normal = pass_denoising_normal;
        guiding_params.pass_flow = pass_motion;

        guiding_params.stride = buffer_params.stride;
        guiding_params.pass_stride = buffer_params.pass_stride;
      }
      else {
        guiding_params.pass_stride = 0;
        if (use_pass_albedo) {
          guiding_params.pass_albedo = guiding_params.pass_stride;
          guiding_params.pass_stride += 3;
        }
        if (use_pass_normal) {
          guiding_params.pass_normal = guiding_params.pass_stride;
          guiding_params.pass_stride += 3;
        }
        if (use_pass_flow) {
          guiding_params.pass_flow = guiding_params.pass_stride;
          guiding_params.pass_stride += 2;
        }

        guiding_params.stride = buffer_params.width;

        guiding_buffer.alloc_to_device(buffer_params.width * buffer_params.height *
                                       guiding_params.pass_stride);
        guiding_params.device_pointer = guiding_buffer.device_pointer;
      }
    }

    pass_sample_count = buffer_params.get_pass_offset(PASS_SAMPLE_COUNT);
  }

  const DenoiseParams &denoise_params;

  RenderBuffers *render_buffers = nullptr;
  const BufferParams &buffer_params;

  /* Previous output. */
  struct {
    device_ptr device_pointer = 0;

    int offset = PASS_UNUSED;

    int stride = -1;
    int pass_stride = -1;
  } prev_output;

  /* Device-side storage of the guiding passes. */
  device_only_memory<float> guiding_buffer;

  struct {
    device_ptr device_pointer = 0;

    /* NOTE: Are only initialized when the corresponding guiding pass is enabled. */
    int pass_albedo = PASS_UNUSED;
    int pass_normal = PASS_UNUSED;
    int pass_flow = PASS_UNUSED;

    int stride = -1;
    int pass_stride = -1;
  } guiding_params;

  /* Number of input passes. Including the color and extra auxiliary passes. */
  int num_input_passes = 0;
  bool use_guiding_passes = false;
  bool use_pass_albedo = false;
  bool use_pass_normal = false;
  bool use_pass_flow = false;

  int num_samples = 0;

  int pass_sample_count = PASS_UNUSED;

  /* NOTE: Are only initialized when the corresponding guiding pass is enabled. */
  int pass_denoising_albedo = PASS_UNUSED;
  int pass_denoising_normal = PASS_UNUSED;
  int pass_motion = PASS_UNUSED;

  /* For passes which don't need albedo channel for denoising we replace the actual albedo with
   * the (0.5, 0.5, 0.5). This flag indicates that the real albedo pass has been replaced with
   * the fake values and denoising of passes which do need albedo can no longer happen. */
  bool albedo_replaced_with_fake = false;
};

class OptiXDevice::DenoisePass {
 public:
  DenoisePass(const PassType type, const BufferParams &buffer_params) : type(type)
  {
    noisy_offset = buffer_params.get_pass_offset(type, PassMode::NOISY);
    denoised_offset = buffer_params.get_pass_offset(type, PassMode::DENOISED);

    const PassInfo pass_info = Pass::get_info(type);
    num_components = pass_info.num_components;
    use_compositing = pass_info.use_compositing;
    use_denoising_albedo = pass_info.use_denoising_albedo;
  }

  PassType type;

  int noisy_offset;
  int denoised_offset;

  int num_components;
  bool use_compositing;
  bool use_denoising_albedo;
};

bool OptiXDevice::denoise_buffer(const DeviceDenoiseTask &task)
{
  const CUDAContextScope scope(this);

  DenoiseContext context(this, task);

  if (!denoise_ensure(context)) {
    return false;
  }

  if (!denoise_filter_guiding_preprocess(context)) {
    LOG(ERROR) << "Error preprocessing guiding passes.";
    return false;
  }

  /* Passes which will use real albedo when it is available. */
  denoise_pass(context, PASS_COMBINED);
  denoise_pass(context, PASS_SHADOW_CATCHER_MATTE);

  /* Passes which do not need albedo and hence if real is present it needs to become fake. */
  denoise_pass(context, PASS_SHADOW_CATCHER);

  return true;
}

DeviceQueue *OptiXDevice::get_denoise_queue()
{
  return &denoiser_.queue;
}

bool OptiXDevice::denoise_filter_guiding_preprocess(DenoiseContext &context)
{
  const BufferParams &buffer_params = context.buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  DeviceKernelArguments args(&context.guiding_params.device_pointer,
                             &context.guiding_params.pass_stride,
                             &context.guiding_params.pass_albedo,
                             &context.guiding_params.pass_normal,
                             &context.guiding_params.pass_flow,
                             &context.render_buffers->buffer.device_pointer,
                             &buffer_params.offset,
                             &buffer_params.stride,
                             &buffer_params.pass_stride,
                             &context.pass_sample_count,
                             &context.pass_denoising_albedo,
                             &context.pass_denoising_normal,
                             &context.pass_motion,
                             &buffer_params.full_x,
                             &buffer_params.full_y,
                             &buffer_params.width,
                             &buffer_params.height,
                             &context.num_samples);

  return denoiser_.queue.enqueue(DEVICE_KERNEL_FILTER_GUIDING_PREPROCESS, work_size, args);
}

bool OptiXDevice::denoise_filter_guiding_set_fake_albedo(DenoiseContext &context)
{
  const BufferParams &buffer_params = context.buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  DeviceKernelArguments args(&context.guiding_params.device_pointer,
                             &context.guiding_params.pass_stride,
                             &context.guiding_params.pass_albedo,
                             &buffer_params.width,
                             &buffer_params.height);

  return denoiser_.queue.enqueue(DEVICE_KERNEL_FILTER_GUIDING_SET_FAKE_ALBEDO, work_size, args);
}

void OptiXDevice::denoise_pass(DenoiseContext &context, PassType pass_type)
{
  const BufferParams &buffer_params = context.buffer_params;

  const DenoisePass pass(pass_type, buffer_params);

  if (pass.noisy_offset == PASS_UNUSED) {
    return;
  }
  if (pass.denoised_offset == PASS_UNUSED) {
    LOG(DFATAL) << "Missing denoised pass " << pass_type_as_string(pass_type);
    return;
  }

  if (pass.use_denoising_albedo) {
    if (context.albedo_replaced_with_fake) {
      LOG(ERROR) << "Pass which requires albedo is denoised after fake albedo has been set.";
      return;
    }
  }
  else if (context.use_guiding_passes && !context.albedo_replaced_with_fake) {
    context.albedo_replaced_with_fake = true;
    if (!denoise_filter_guiding_set_fake_albedo(context)) {
      LOG(ERROR) << "Error replacing real albedo with the fake one.";
      return;
    }
  }

  /* Read and preprocess noisy color input pass. */
  denoise_color_read(context, pass);
  if (!denoise_filter_color_preprocess(context, pass)) {
    LOG(ERROR) << "Error connverting denoising passes to RGB buffer.";
    return;
  }

  if (!denoise_run(context, pass)) {
    LOG(ERROR) << "Error running OptiX denoiser.";
    return;
  }

  /* Store result in the combined pass of the render buffer.
   *
   * This will scale the denoiser result up to match the number of, possibly per-pixel, samples. */
  if (!denoise_filter_color_postprocess(context, pass)) {
    LOG(ERROR) << "Error copying denoiser result to the denoised pass.";
    return;
  }

  denoiser_.queue.synchronize();
}

void OptiXDevice::denoise_color_read(DenoiseContext &context, const DenoisePass &pass)
{
  PassAccessor::PassAccessInfo pass_access_info;
  pass_access_info.type = pass.type;
  pass_access_info.mode = PassMode::NOISY;
  pass_access_info.offset = pass.noisy_offset;

  /* Denoiser operates on passes which are used to calculate the approximation, and is never used
   * on the approximation. The latter is not even possible because OptiX does not support
   * denoising of semi-transparent pixels. */
  pass_access_info.use_approximate_shadow_catcher = false;
  pass_access_info.use_approximate_shadow_catcher_background = false;
  pass_access_info.show_active_pixels = false;

  /* TODO(sergey): Consider adding support of actual exposure, to avoid clamping in extreme cases.
   */
  const PassAccessorGPU pass_accessor(
      &denoiser_.queue, pass_access_info, 1.0f, context.num_samples);

  PassAccessor::Destination destination(pass_access_info.type);
  destination.d_pixels = context.render_buffers->buffer.device_pointer +
                         pass.denoised_offset * sizeof(float);
  destination.num_components = 3;
  destination.pixel_stride = context.buffer_params.pass_stride;

  BufferParams buffer_params = context.buffer_params;
  buffer_params.window_x = 0;
  buffer_params.window_y = 0;
  buffer_params.window_width = buffer_params.width;
  buffer_params.window_height = buffer_params.height;

  pass_accessor.get_render_tile_pixels(context.render_buffers, buffer_params, destination);
}

bool OptiXDevice::denoise_filter_color_preprocess(DenoiseContext &context, const DenoisePass &pass)
{
  const BufferParams &buffer_params = context.buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  DeviceKernelArguments args(&context.render_buffers->buffer.device_pointer,
                             &buffer_params.full_x,
                             &buffer_params.full_y,
                             &buffer_params.width,
                             &buffer_params.height,
                             &buffer_params.offset,
                             &buffer_params.stride,
                             &buffer_params.pass_stride,
                             &pass.denoised_offset);

  return denoiser_.queue.enqueue(DEVICE_KERNEL_FILTER_COLOR_PREPROCESS, work_size, args);
}

bool OptiXDevice::denoise_filter_color_postprocess(DenoiseContext &context,
                                                   const DenoisePass &pass)
{
  const BufferParams &buffer_params = context.buffer_params;

  const int work_size = buffer_params.width * buffer_params.height;

  DeviceKernelArguments args(&context.render_buffers->buffer.device_pointer,
                             &buffer_params.full_x,
                             &buffer_params.full_y,
                             &buffer_params.width,
                             &buffer_params.height,
                             &buffer_params.offset,
                             &buffer_params.stride,
                             &buffer_params.pass_stride,
                             &context.num_samples,
                             &pass.noisy_offset,
                             &pass.denoised_offset,
                             &context.pass_sample_count,
                             &pass.num_components,
                             &pass.use_compositing);

  return denoiser_.queue.enqueue(DEVICE_KERNEL_FILTER_COLOR_POSTPROCESS, work_size, args);
}

bool OptiXDevice::denoise_ensure(DenoiseContext &context)
{
  if (!denoise_create_if_needed(context)) {
    LOG(ERROR) << "OptiX denoiser creation has failed.";
    return false;
  }

  if (!denoise_configure_if_needed(context)) {
    LOG(ERROR) << "OptiX denoiser configuration has failed.";
    return false;
  }

  return true;
}

bool OptiXDevice::denoise_create_if_needed(DenoiseContext &context)
{
  const bool recreate_denoiser = (denoiser_.optix_denoiser == nullptr) ||
                                 (denoiser_.use_pass_albedo != context.use_pass_albedo) ||
                                 (denoiser_.use_pass_normal != context.use_pass_normal) ||
                                 (denoiser_.use_pass_flow != context.use_pass_flow);
  if (!recreate_denoiser) {
    return true;
  }

  /* Destroy existing handle before creating new one. */
  if (denoiser_.optix_denoiser) {
    optixDenoiserDestroy(denoiser_.optix_denoiser);
  }

  /* Create OptiX denoiser handle on demand when it is first used. */
  OptixDenoiserOptions denoiser_options = {};
  denoiser_options.guideAlbedo = context.use_pass_albedo;
  denoiser_options.guideNormal = context.use_pass_normal;

  OptixDenoiserModelKind model = OPTIX_DENOISER_MODEL_KIND_HDR;
  if (context.use_pass_flow) {
    model = OPTIX_DENOISER_MODEL_KIND_TEMPORAL;
  }

  const OptixResult result = optixDenoiserCreate(
      this->context, model, &denoiser_options, &denoiser_.optix_denoiser);

  if (result != OPTIX_SUCCESS) {
    set_error("Failed to create OptiX denoiser");
    return false;
  }

  /* OptiX denoiser handle was created with the requested number of input passes. */
  denoiser_.use_pass_albedo = context.use_pass_albedo;
  denoiser_.use_pass_normal = context.use_pass_normal;
  denoiser_.use_pass_flow = context.use_pass_flow;

  /* OptiX denoiser has been created, but it needs configuration. */
  denoiser_.is_configured = false;

  return true;
}

bool OptiXDevice::denoise_configure_if_needed(DenoiseContext &context)
{
  /* Limit maximum tile size denoiser can be invoked with. */
  const int2 tile_size = make_int2(min(context.buffer_params.width, 4096),
                                   min(context.buffer_params.height, 4096));

  if (denoiser_.is_configured &&
      (denoiser_.configured_size.x == tile_size.x && denoiser_.configured_size.y == tile_size.y)) {
    return true;
  }

  optix_assert(optixDenoiserComputeMemoryResources(
      denoiser_.optix_denoiser, tile_size.x, tile_size.y, &denoiser_.sizes));

  /* Allocate denoiser state if tile size has changed since last setup. */
  denoiser_.state.alloc_to_device(denoiser_.sizes.stateSizeInBytes +
                                  denoiser_.sizes.withOverlapScratchSizeInBytes);

  /* Initialize denoiser state for the current tile size. */
  const OptixResult result = optixDenoiserSetup(
      denoiser_.optix_denoiser,
      0, /* Work around bug in r495 drivers that causes artifacts when denoiser setup is called
            on a stream that is not the default stream */
      tile_size.x + denoiser_.sizes.overlapWindowSizeInPixels * 2,
      tile_size.y + denoiser_.sizes.overlapWindowSizeInPixels * 2,
      denoiser_.state.device_pointer,
      denoiser_.sizes.stateSizeInBytes,
      denoiser_.state.device_pointer + denoiser_.sizes.stateSizeInBytes,
      denoiser_.sizes.withOverlapScratchSizeInBytes);
  if (result != OPTIX_SUCCESS) {
    set_error("Failed to set up OptiX denoiser");
    return false;
  }

  cuda_assert(cuCtxSynchronize());

  denoiser_.is_configured = true;
  denoiser_.configured_size = tile_size;

  return true;
}

bool OptiXDevice::denoise_run(DenoiseContext &context, const DenoisePass &pass)
{
  const BufferParams &buffer_params = context.buffer_params;
  const int width = buffer_params.width;
  const int height = buffer_params.height;

  /* Set up input and output layer information. */
  OptixImage2D color_layer = {0};
  OptixImage2D albedo_layer = {0};
  OptixImage2D normal_layer = {0};
  OptixImage2D flow_layer = {0};

  OptixImage2D output_layer = {0};
  OptixImage2D prev_output_layer = {0};

  /* Color pass. */
  {
    const int pass_denoised = pass.denoised_offset;
    const int64_t pass_stride_in_bytes = context.buffer_params.pass_stride * sizeof(float);

    color_layer.data = context.render_buffers->buffer.device_pointer +
                       pass_denoised * sizeof(float);
    color_layer.width = width;
    color_layer.height = height;
    color_layer.rowStrideInBytes = pass_stride_in_bytes * context.buffer_params.stride;
    color_layer.pixelStrideInBytes = pass_stride_in_bytes;
    color_layer.format = OPTIX_PIXEL_FORMAT_FLOAT3;
  }

  /* Previous output. */
  if (context.prev_output.offset != PASS_UNUSED) {
    const int64_t pass_stride_in_bytes = context.prev_output.pass_stride * sizeof(float);

    prev_output_layer.data = context.prev_output.device_pointer +
                             context.prev_output.offset * sizeof(float);
    prev_output_layer.width = width;
    prev_output_layer.height = height;
    prev_output_layer.rowStrideInBytes = pass_stride_in_bytes * context.prev_output.stride;
    prev_output_layer.pixelStrideInBytes = pass_stride_in_bytes;
    prev_output_layer.format = OPTIX_PIXEL_FORMAT_FLOAT3;
  }

  /* Optional albedo and color passes. */
  if (context.num_input_passes > 1) {
    const device_ptr d_guiding_buffer = context.guiding_params.device_pointer;
    const int64_t pixel_stride_in_bytes = context.guiding_params.pass_stride * sizeof(float);
    const int64_t row_stride_in_bytes = context.guiding_params.stride * pixel_stride_in_bytes;

    if (context.use_pass_albedo) {
      albedo_layer.data = d_guiding_buffer + context.guiding_params.pass_albedo * sizeof(float);
      albedo_layer.width = width;
      albedo_layer.height = height;
      albedo_layer.rowStrideInBytes = row_stride_in_bytes;
      albedo_layer.pixelStrideInBytes = pixel_stride_in_bytes;
      albedo_layer.format = OPTIX_PIXEL_FORMAT_FLOAT3;
    }

    if (context.use_pass_normal) {
      normal_layer.data = d_guiding_buffer + context.guiding_params.pass_normal * sizeof(float);
      normal_layer.width = width;
      normal_layer.height = height;
      normal_layer.rowStrideInBytes = row_stride_in_bytes;
      normal_layer.pixelStrideInBytes = pixel_stride_in_bytes;
      normal_layer.format = OPTIX_PIXEL_FORMAT_FLOAT3;
    }

    if (context.use_pass_flow) {
      flow_layer.data = d_guiding_buffer + context.guiding_params.pass_flow * sizeof(float);
      flow_layer.width = width;
      flow_layer.height = height;
      flow_layer.rowStrideInBytes = row_stride_in_bytes;
      flow_layer.pixelStrideInBytes = pixel_stride_in_bytes;
      flow_layer.format = OPTIX_PIXEL_FORMAT_FLOAT2;
    }
  }

  /* Denoise in-place of the noisy input in the render buffers. */
  output_layer = color_layer;

  OptixDenoiserGuideLayer guide_layers = {};
  guide_layers.albedo = albedo_layer;
  guide_layers.normal = normal_layer;
  guide_layers.flow = flow_layer;

  OptixDenoiserLayer image_layers = {};
  image_layers.input = color_layer;
  image_layers.previousOutput = prev_output_layer;
  image_layers.output = output_layer;

  /* Finally run denoising. */
  OptixDenoiserParams params = {}; /* All parameters are disabled/zero. */

  optix_assert(optixUtilDenoiserInvokeTiled(denoiser_.optix_denoiser,
                                            denoiser_.queue.stream(),
                                            &params,
                                            denoiser_.state.device_pointer,
                                            denoiser_.sizes.stateSizeInBytes,
                                            &guide_layers,
                                            &image_layers,
                                            1,
                                            denoiser_.state.device_pointer +
                                                denoiser_.sizes.stateSizeInBytes,
                                            denoiser_.sizes.withOverlapScratchSizeInBytes,
                                            denoiser_.sizes.overlapWindowSizeInPixels,
                                            denoiser_.configured_size.x,
                                            denoiser_.configured_size.y));

  return true;
}

bool OptiXDevice::build_optix_bvh(BVHOptiX *bvh,
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

  const CUDAContextScope scope(this);

  const bool use_fast_trace_bvh = (bvh->params.bvh_type == BVH_TYPE_STATIC);

  /* Compute memory usage. */
  OptixAccelBufferSizes sizes = {};
  OptixAccelBuildOptions options = {};
  options.operation = operation;
  if (use_fast_trace_bvh) {
    VLOG(2) << "Using fast to trace OptiX BVH";
    options.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE | OPTIX_BUILD_FLAG_ALLOW_COMPACTION;
  }
  else {
    VLOG(2) << "Using fast to update OptiX BVH";
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
                               NULL,
                               &options,
                               &build_input,
                               1,
                               temp_mem.device_pointer,
                               sizes.tempSizeInBytes,
                               out_data.device_pointer,
                               sizes.outputSizeInBytes,
                               &out_handle,
                               use_fast_trace_bvh ? &compacted_size_prop : NULL,
                               use_fast_trace_bvh ? 1 : 0));
  bvh->traversable_handle = static_cast<uint64_t>(out_handle);

  /* Wait for all operations to finish. */
  cuda_assert(cuStreamSynchronize(NULL));

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

      optix_assert(optixAccelCompact(
          context, NULL, out_handle, compacted_data.device_pointer, compacted_size, &out_handle));
      bvh->traversable_handle = static_cast<uint64_t>(out_handle);

      /* Wait for compaction to finish. */
      cuda_assert(cuStreamSynchronize(NULL));

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
    if (geom->geometry_type == Geometry::HAIR) {
      /* Build BLAS for curve primitives. */
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
      device_vector<int> index_data(this, "optix temp index data", MEM_READ_ONLY);
      device_vector<float4> vertex_data(this, "optix temp vertex data", MEM_READ_ONLY);
      /* Four control points for each curve segment. */
      const size_t num_vertices = num_segments * 4;
      if (hair->curve_shape == CURVE_THICK) {
        index_data.alloc(num_segments);
        vertex_data.alloc(num_vertices * num_motion_steps);
      }
      else
        aabb_data.alloc(num_segments * num_motion_steps);

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

        for (size_t j = 0, i = 0; j < hair->num_curves(); ++j) {
          const Hair::Curve curve = hair->get_curve(j);
          const array<float> &curve_radius = hair->get_curve_radius();

          for (int segment = 0; segment < curve.num_segments(); ++segment, ++i) {
            if (hair->curve_shape == CURVE_THICK) {
              int k0 = curve.first_key + segment;
              int k1 = k0 + 1;
              int ka = max(k0 - 1, curve.first_key);
              int kb = min(k1 + 1, curve.first_key + curve.num_keys - 1);

              index_data[i] = i * 4;
              float4 *const v = vertex_data.data() + step * num_vertices + index_data[i];

#  if OPTIX_ABI_VERSION >= 55
              v[0] = make_float4(keys[ka].x, keys[ka].y, keys[ka].z, curve_radius[ka]);
              v[1] = make_float4(keys[k0].x, keys[k0].y, keys[k0].z, curve_radius[k0]);
              v[2] = make_float4(keys[k1].x, keys[k1].y, keys[k1].z, curve_radius[k1]);
              v[3] = make_float4(keys[kb].x, keys[kb].y, keys[kb].z, curve_radius[kb]);
#  else
              const float4 px = make_float4(keys[ka].x, keys[k0].x, keys[k1].x, keys[kb].x);
              const float4 py = make_float4(keys[ka].y, keys[k0].y, keys[k1].y, keys[kb].y);
              const float4 pz = make_float4(keys[ka].z, keys[k0].z, keys[k1].z, keys[kb].z);
              const float4 pw = make_float4(
                  curve_radius[ka], curve_radius[k0], curve_radius[k1], curve_radius[kb]);

              /* Convert Catmull-Rom data to B-spline. */
              static const float4 cr2bsp0 = make_float4(+7, -4, +5, -2) / 6.f;
              static const float4 cr2bsp1 = make_float4(-2, 11, -4, +1) / 6.f;
              static const float4 cr2bsp2 = make_float4(+1, -4, 11, -2) / 6.f;
              static const float4 cr2bsp3 = make_float4(-2, +5, -4, +7) / 6.f;

              v[0] = make_float4(
                  dot(cr2bsp0, px), dot(cr2bsp0, py), dot(cr2bsp0, pz), dot(cr2bsp0, pw));
              v[1] = make_float4(
                  dot(cr2bsp1, px), dot(cr2bsp1, py), dot(cr2bsp1, pz), dot(cr2bsp1, pw));
              v[2] = make_float4(
                  dot(cr2bsp2, px), dot(cr2bsp2, py), dot(cr2bsp2, pz), dot(cr2bsp2, pw));
              v[3] = make_float4(
                  dot(cr2bsp3, px), dot(cr2bsp3, py), dot(cr2bsp3, pz), dot(cr2bsp3, pw));
#  endif
            }
            else {
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
#  if OPTIX_ABI_VERSION >= 55
        build_input.curveArray.curveType = OPTIX_PRIMITIVE_TYPE_ROUND_CATMULLROM;
#  else
        build_input.curveArray.curveType = OPTIX_PRIMITIVE_TYPE_ROUND_CUBIC_BSPLINE;
#  endif
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
    else if (geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::VOLUME) {
      /* Build BLAS for triangle primitives. */
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
    else if (geom->geometry_type == Geometry::POINTCLOUD) {
      /* Build BLAS for points primitives. */
      PointCloud *const pointcloud = static_cast<PointCloud *const>(geom);
      const size_t num_points = pointcloud->num_points();
      if (num_points == 0) {
        return;
      }

      size_t num_motion_steps = 1;
      Attribute *motion_points = pointcloud->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
      if (motion_blur && pointcloud->get_use_motion_blur() && motion_points) {
        num_motion_steps = pointcloud->get_motion_steps();
      }

      device_vector<OptixAabb> aabb_data(this, "optix temp aabb data", MEM_READ_ONLY);
      aabb_data.alloc(num_points * num_motion_steps);

      /* Get AABBs for each motion step. */
      for (size_t step = 0; step < num_motion_steps; ++step) {
        /* The center step for motion vertices is not stored in the attribute. */
        const float3 *points = pointcloud->get_points().data();
        const float *radius = pointcloud->get_radius().data();
        size_t center_step = (num_motion_steps - 1) / 2;
        if (step != center_step) {
          size_t attr_offset = (step > center_step) ? step - 1 : step;
          /* Technically this is a float4 array, but sizeof(float3) == sizeof(float4). */
          points = motion_points->data_float3() + attr_offset * num_points;
        }

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
#  if OPTIX_ABI_VERSION < 23
      build_input.aabbArray.aabbBuffers = (CUdeviceptr *)aabb_ptrs.data();
      build_input.aabbArray.numPrimitives = num_points;
      build_input.aabbArray.strideInBytes = sizeof(OptixAabb);
      build_input.aabbArray.flags = &build_flags;
      build_input.aabbArray.numSbtRecords = 1;
      build_input.aabbArray.primitiveIndexOffset = pointcloud->prim_offset;
#  else
      build_input.customPrimitiveArray.aabbBuffers = (CUdeviceptr *)aabb_ptrs.data();
      build_input.customPrimitiveArray.numPrimitives = num_points;
      build_input.customPrimitiveArray.strideInBytes = sizeof(OptixAabb);
      build_input.customPrimitiveArray.flags = &build_flags;
      build_input.customPrimitiveArray.numSbtRecords = 1;
      build_input.customPrimitiveArray.primitiveIndexOffset = pointcloud->prim_offset;
#  endif

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

      assert(bvh_optix->motion_transform_data->device == this);
      bvh_optix->motion_transform_data->alloc_to_device(total_motion_transform_size);
    }

    for (Object *ob : bvh->objects) {
      /* Skip non-traceable objects. */
      if (!ob->is_traceable()) {
        continue;
      }

      BVHOptiX *const blas = static_cast<BVHOptiX *>(ob->get_geometry()->bvh);
      OptixTraversableHandle handle = blas->traversable_handle;

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

      if (ob->get_geometry()->geometry_type == Geometry::HAIR &&
          static_cast<const Hair *>(ob->get_geometry())->curve_shape == CURVE_THICK) {
        if (motion_blur && ob->get_geometry()->has_motion_blur()) {
          /* Select between motion blur and non-motion blur built-in intersection module. */
          instance.sbtOffset = PG_HITD_MOTION - PG_HITD;
        }
      }
      else if (ob->get_geometry()->geometry_type == Geometry::POINTCLOUD) {
        /* Use the hit group that has an intersection program for point clouds. */
        instance.sbtOffset = PG_HITD_POINTCLOUD - PG_HITD;

        /* Also skip point clouds in local trace calls. */
        instance.visibilityMask |= 4;
      }

#  if OPTIX_ABI_VERSION < 55
      /* Cannot disable any-hit program for thick curves, since it needs to filter out end-caps. */
      else
#  endif
      {
        /* Can disable __anyhit__kernel_optix_visibility_test by default (except for thick curves,
         * since it needs to filter out end-caps there).

         * It is enabled where necessary (visibility mask exceeds 8 bits or the other any-hit
         * programs like __anyhit__kernel_optix_shadow_all_hit) via OPTIX_RAY_FLAG_ENFORCE_ANYHIT.
         */
        instance.flags = OPTIX_INSTANCE_FLAG_DISABLE_ANYHIT;
      }

      /* Insert motion traversable if object has motion. */
      if (motion_blur && ob->use_motion()) {
        size_t motion_keys = max(ob->get_motion().size(), 2) - 2;
        size_t motion_transform_size = sizeof(OptixSRTMotionTransform) +
                                       motion_keys * sizeof(OptixSRTData);

        const CUDAContextScope scope(this);

        motion_transform_offset = align_up(motion_transform_offset,
                                           OPTIX_TRANSFORM_BYTE_ALIGNMENT);
        CUdeviceptr motion_transform_gpu = bvh_optix->motion_transform_data->device_pointer +
                                           motion_transform_offset;
        motion_transform_offset += motion_transform_size;

        /* Allocate host side memory for motion transform and fill it with transform data. */
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
        cuMemcpyHtoD(motion_transform_gpu, &motion_transform, motion_transform_size);
        delete[] reinterpret_cast<uint8_t *>(&motion_transform);

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

void OptiXDevice::release_optix_bvh(BVH *bvh)
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

void OptiXDevice::const_copy_to(const char *name, void *host, size_t size)
{
  /* Set constant memory for CUDA module. */
  CUDADevice::const_copy_to(name, host, size);

  if (strcmp(name, "__data") == 0) {
    assert(size <= sizeof(KernelData));

    /* Update traversable handle (since it is different for each device on multi devices). */
    KernelData *const data = (KernelData *)host;
    *(OptixTraversableHandle *)&data->bvh.scene = tlas_handle;

    update_launch_params(offsetof(KernelParamsOptiX, data), host, size);
    return;
  }

  /* Update data storage pointers in launch parameters. */
#  define KERNEL_TEX(data_type, tex_name) \
    if (strcmp(name, #tex_name) == 0) { \
      update_launch_params(offsetof(KernelParamsOptiX, tex_name), host, size); \
      return; \
    }
  KERNEL_TEX(IntegratorStateGPU, __integrator_state)
#  include "kernel/textures.h"
#  undef KERNEL_TEX
}

void OptiXDevice::update_launch_params(size_t offset, void *data, size_t data_size)
{
  const CUDAContextScope scope(this);

  cuda_assert(cuMemcpyHtoD(launch_params.device_pointer + offset, data, data_size));
}

CCL_NAMESPACE_END

#endif /* WITH_OPTIX */
