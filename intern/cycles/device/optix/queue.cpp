/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_OPTIX

#  include "device/optix/queue.h"
#  include "device/optix/device_impl.h"

#  define __KERNEL_OPTIX__
#  include "kernel/device/optix/globals.h"

CCL_NAMESPACE_BEGIN

/* CUDADeviceQueue */

OptiXDeviceQueue::OptiXDeviceQueue(OptiXDevice *device) : CUDADeviceQueue(device) {}

void OptiXDeviceQueue::init_execution()
{
  CUDADeviceQueue::init_execution();
}

static bool is_optix_specific_kernel(DeviceKernel kernel, bool osl_shading, bool osl_camera)
{
#  ifdef WITH_OSL
  /* OSL uses direct callables to execute, so shading needs to be done in OptiX if OSL is used. */
  if (osl_shading && device_kernel_has_shading(kernel)) {
    return true;
  }
  if (osl_camera && kernel == DEVICE_KERNEL_INTEGRATOR_INIT_FROM_CAMERA) {
    return true;
  }
#  else
  (void)osl_shading;
  (void)osl_camera;
#  endif

  return device_kernel_has_intersection(kernel);
}

bool OptiXDeviceQueue::enqueue(DeviceKernel kernel,
                               const int work_size,
                               const DeviceKernelArguments &args)
{
  OptiXDevice *const optix_device = static_cast<OptiXDevice *>(cuda_device_);

#  ifdef WITH_OSL
  const OSLGlobals *og = static_cast<const OSLGlobals *>(optix_device->get_cpu_osl_memory());
  const bool osl_shading = og->use_shading;
  const bool osl_camera = og->use_camera;
#  else
  const bool osl_shading = false;
  const bool osl_camera = false;
#  endif

  if (!is_optix_specific_kernel(kernel, osl_shading, osl_camera)) {
    return CUDADeviceQueue::enqueue(kernel, work_size, args);
  }

  if (cuda_device_->have_error()) {
    return false;
  }

  debug_enqueue_begin(kernel, work_size);

  const CUDAContextScope scope(cuda_device_);

  const device_ptr sbt_data_ptr = optix_device->sbt_data.device_pointer;
  const device_ptr launch_params_ptr = optix_device->launch_params.device_pointer;

  auto set_launch_param = [&](size_t offset, size_t size, int arg) {
    cuda_device_assert(
        cuda_device_,
        cuMemcpyHtoDAsync(launch_params_ptr + offset, args.values[arg], size, cuda_stream_));
  };

  set_launch_param(offsetof(KernelParamsOptiX, path_index_array), sizeof(device_ptr), 0);

  if (kernel == DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST || device_kernel_has_shading(kernel)) {
    set_launch_param(offsetof(KernelParamsOptiX, render_buffer), sizeof(device_ptr), 1);
  }
  if (kernel == DEVICE_KERNEL_SHADER_EVAL_DISPLACE ||
      kernel == DEVICE_KERNEL_SHADER_EVAL_BACKGROUND ||
      kernel == DEVICE_KERNEL_SHADER_EVAL_CURVE_SHADOW_TRANSPARENCY ||
      kernel == DEVICE_KERNEL_SHADER_EVAL_VOLUME_DENSITY)
  {
    set_launch_param(offsetof(KernelParamsOptiX, offset), sizeof(int32_t), 2);
  }

  if (kernel == DEVICE_KERNEL_INTEGRATOR_INIT_FROM_CAMERA) {
    set_launch_param(offsetof(KernelParamsOptiX, num_tiles), sizeof(int32_t), 1);
    set_launch_param(offsetof(KernelParamsOptiX, render_buffer), sizeof(device_ptr), 2);
    set_launch_param(offsetof(KernelParamsOptiX, max_tile_work_size), sizeof(int32_t), 3);
  }

  cuda_device_assert(cuda_device_, cuStreamSynchronize(cuda_stream_));

  OptixPipeline pipeline = nullptr;
  OptixShaderBindingTable sbt_params = {};

  switch (kernel) {
    case DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND:
      pipeline = optix_device->pipelines[PIP_SHADE];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_SHADE_BACKGROUND * sizeof(SbtRecord);
      break;
    case DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT:
      pipeline = optix_device->pipelines[PIP_SHADE];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_SHADE_LIGHT * sizeof(SbtRecord);
      break;
    case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE:
      pipeline = optix_device->pipelines[PIP_SHADE];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_SHADE_SURFACE * sizeof(SbtRecord);
      break;
    case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE:
      pipeline = optix_device->pipelines[PIP_SHADE];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_SHADE_SURFACE_RAYTRACE * sizeof(SbtRecord);
      break;
    case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE:
      pipeline = optix_device->pipelines[PIP_SHADE];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_SHADE_SURFACE_MNEE * sizeof(SbtRecord);
      break;
    case DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME:
      pipeline = optix_device->pipelines[PIP_SHADE];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_SHADE_VOLUME * sizeof(SbtRecord);
      break;
    case DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME_RAY_MARCHING:
      pipeline = optix_device->pipelines[PIP_SHADE];
      sbt_params.raygenRecord = sbt_data_ptr +
                                PG_RGEN_SHADE_VOLUME_RAY_MARCHING * sizeof(SbtRecord);
      break;
    case DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW:
      pipeline = optix_device->pipelines[PIP_SHADE];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_SHADE_SHADOW * sizeof(SbtRecord);
      break;
    case DEVICE_KERNEL_INTEGRATOR_SHADE_DEDICATED_LIGHT:
      pipeline = optix_device->pipelines[PIP_SHADE];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_SHADE_DEDICATED_LIGHT * sizeof(SbtRecord);
      break;

    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST:
      pipeline = optix_device->pipelines[PIP_INTERSECT];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_INTERSECT_CLOSEST * sizeof(SbtRecord);
      break;
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW:
      pipeline = optix_device->pipelines[PIP_INTERSECT];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_INTERSECT_SHADOW * sizeof(SbtRecord);
      break;
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE:
      pipeline = optix_device->pipelines[PIP_INTERSECT];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_INTERSECT_SUBSURFACE * sizeof(SbtRecord);
      break;
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK:
      pipeline = optix_device->pipelines[PIP_INTERSECT];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_INTERSECT_VOLUME_STACK * sizeof(SbtRecord);
      break;
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_DEDICATED_LIGHT:
      pipeline = optix_device->pipelines[PIP_INTERSECT];
      sbt_params.raygenRecord = sbt_data_ptr +
                                PG_RGEN_INTERSECT_DEDICATED_LIGHT * sizeof(SbtRecord);
      break;

    case DEVICE_KERNEL_SHADER_EVAL_DISPLACE:
      pipeline = optix_device->pipelines[PIP_SHADE];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_EVAL_DISPLACE * sizeof(SbtRecord);
      break;
    case DEVICE_KERNEL_SHADER_EVAL_BACKGROUND:
      pipeline = optix_device->pipelines[PIP_SHADE];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_EVAL_BACKGROUND * sizeof(SbtRecord);
      break;
    case DEVICE_KERNEL_SHADER_EVAL_CURVE_SHADOW_TRANSPARENCY:
      pipeline = optix_device->pipelines[PIP_SHADE];
      sbt_params.raygenRecord = sbt_data_ptr +
                                PG_RGEN_EVAL_CURVE_SHADOW_TRANSPARENCY * sizeof(SbtRecord);
      break;
    case DEVICE_KERNEL_SHADER_EVAL_VOLUME_DENSITY:
      pipeline = optix_device->pipelines[PIP_SHADE];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_EVAL_VOLUME_DENSITY * sizeof(SbtRecord);
      break;

    case DEVICE_KERNEL_INTEGRATOR_INIT_FROM_CAMERA:
      pipeline = optix_device->pipelines[PIP_SHADE];
      sbt_params.raygenRecord = sbt_data_ptr + PG_RGEN_INIT_FROM_CAMERA * sizeof(SbtRecord);
      break;

    default:
      LOG_ERROR << "Invalid kernel " << device_kernel_as_string(kernel)
                << " is attempted to be enqueued.";
      return false;
  }

  sbt_params.missRecordBase = sbt_data_ptr + MISS_PROGRAM_GROUP_OFFSET * sizeof(SbtRecord);
  sbt_params.missRecordStrideInBytes = sizeof(SbtRecord);
  sbt_params.missRecordCount = NUM_MISS_PROGRAM_GROUPS;
  sbt_params.hitgroupRecordBase = sbt_data_ptr + HIT_PROGAM_GROUP_OFFSET * sizeof(SbtRecord);
  sbt_params.hitgroupRecordStrideInBytes = sizeof(SbtRecord);
  sbt_params.hitgroupRecordCount = NUM_HIT_PROGRAM_GROUPS;
  sbt_params.callablesRecordBase = sbt_data_ptr + CALLABLE_PROGRAM_GROUPS_BASE * sizeof(SbtRecord);
  sbt_params.callablesRecordCount = NUM_CALLABLE_PROGRAM_GROUPS;
  sbt_params.callablesRecordStrideInBytes = sizeof(SbtRecord);

#  ifdef WITH_OSL
  if (osl_shading || osl_camera) {
    sbt_params.callablesRecordCount += static_cast<unsigned int>(optix_device->osl_groups.size());
  }
#  endif

  /* Launch the ray generation program. */
  optix_device_assert(optix_device,
                      optixLaunch(pipeline,
                                  cuda_stream_,
                                  launch_params_ptr,
                                  optix_device->launch_params.data_elements,
                                  &sbt_params,
                                  work_size,
                                  1,
                                  1));

  debug_enqueue_end();

  return !(optix_device->have_error());
}

CCL_NAMESPACE_END

#endif /* WITH_OPTIX */
