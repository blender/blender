/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_OPTIX

#  include "device/optix/queue.h"
#  include "device/optix/device_impl.h"

#  include "util/time.h"

#  define __KERNEL_OPTIX__
#  include "kernel/device/optix/globals.h"

CCL_NAMESPACE_BEGIN

/* CUDADeviceQueue */

OptiXDeviceQueue::OptiXDeviceQueue(OptiXDevice *device) : CUDADeviceQueue(device) {}

void OptiXDeviceQueue::init_execution()
{
  CUDADeviceQueue::init_execution();
}

static bool is_optix_specific_kernel(DeviceKernel kernel, bool use_osl)
{
#  ifdef WITH_OSL
  /* OSL uses direct callables to execute, so shading needs to be done in OptiX if OSL is used. */
  if (use_osl && device_kernel_has_shading(kernel)) {
    return true;
  }
#  else
  (void)use_osl;
#  endif

  return device_kernel_has_intersection(kernel);
}

bool OptiXDeviceQueue::enqueue(DeviceKernel kernel,
                               const int work_size,
                               DeviceKernelArguments const &args)
{
  OptiXDevice *const optix_device = static_cast<OptiXDevice *>(cuda_device_);

#  ifdef WITH_OSL
  const bool use_osl = static_cast<OSLGlobals *>(optix_device->get_cpu_osl_memory())->use;
#  else
  const bool use_osl = false;
#  endif

  if (!is_optix_specific_kernel(kernel, use_osl)) {
    return CUDADeviceQueue::enqueue(kernel, work_size, args);
  }

  if (cuda_device_->have_error()) {
    return false;
  }

  debug_enqueue_begin(kernel, work_size);

  const CUDAContextScope scope(cuda_device_);

  const device_ptr sbt_data_ptr = optix_device->sbt_data.device_pointer;
  const device_ptr launch_params_ptr = optix_device->launch_params.device_pointer;

  cuda_device_assert(
      cuda_device_,
      cuMemcpyHtoDAsync(launch_params_ptr + offsetof(KernelParamsOptiX, path_index_array),
                        args.values[0],  // &d_path_index
                        sizeof(device_ptr),
                        cuda_stream_));

  if (kernel == DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST || device_kernel_has_shading(kernel)) {
    cuda_device_assert(
        cuda_device_,
        cuMemcpyHtoDAsync(launch_params_ptr + offsetof(KernelParamsOptiX, render_buffer),
                          args.values[1],  // &d_render_buffer
                          sizeof(device_ptr),
                          cuda_stream_));
  }
  if (kernel == DEVICE_KERNEL_SHADER_EVAL_DISPLACE ||
      kernel == DEVICE_KERNEL_SHADER_EVAL_BACKGROUND ||
      kernel == DEVICE_KERNEL_SHADER_EVAL_CURVE_SHADOW_TRANSPARENCY)
  {
    cuda_device_assert(cuda_device_,
                       cuMemcpyHtoDAsync(launch_params_ptr + offsetof(KernelParamsOptiX, offset),
                                         args.values[2],  // &d_offset
                                         sizeof(int32_t),
                                         cuda_stream_));
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

    default:
      LOG(ERROR) << "Invalid kernel " << device_kernel_as_string(kernel)
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
  if (use_osl) {
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
