/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef __HIP_DEVICE_COMPILE__

#  include "kernel/device/hip/compat.h"
#  include "kernel/device/hip/config.h"

#  include <hiprt/impl/hiprt_device_impl.h>

#  include "kernel/device/hiprt/globals.h"

#  include "kernel/device/gpu/image.h"

#  include "kernel/tables.h"

#  include "kernel/integrator/state.h"
#  include "kernel/integrator/state_flow.h"
#  include "kernel/integrator/state_util.h"

#  include "kernel/integrator/intersect_closest.h"
#  include "kernel/integrator/intersect_dedicated_light.h"
#  include "kernel/integrator/intersect_shadow.h"
#  include "kernel/integrator/intersect_subsurface.h"
#  include "kernel/integrator/intersect_volume_stack.h"
#  include "kernel/integrator/shade_surface.h"

ccl_gpu_kernel_threads(GPU_HIPRT_KERNEL_BLOCK_NUM_THREADS)
    ccl_gpu_kernel_signature(integrator_intersect_closest,
                             const ccl_global int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size,
                             ccl_global hiprtGlobalStackBuffer stack_buffer)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    HIPRT_INIT_KERNEL_GLOBAL()
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_intersect_closest(kg, state, render_buffer));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel_threads(GPU_HIPRT_KERNEL_BLOCK_NUM_THREADS)
    ccl_gpu_kernel_signature(integrator_intersect_shadow,
                             const ccl_global int *path_index_array,
                             const int work_size,
                             ccl_global hiprtGlobalStackBuffer stack_buffer)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    HIPRT_INIT_KERNEL_GLOBAL()
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_intersect_shadow(kg, state));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel_threads(GPU_HIPRT_KERNEL_BLOCK_NUM_THREADS)
    ccl_gpu_kernel_signature(integrator_intersect_subsurface,
                             const ccl_global int *path_index_array,
                             const int work_size,
                             ccl_global hiprtGlobalStackBuffer stack_buffer)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    HIPRT_INIT_KERNEL_GLOBAL()
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_intersect_subsurface(kg, state));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel_threads(GPU_HIPRT_KERNEL_BLOCK_NUM_THREADS)
    ccl_gpu_kernel_signature(integrator_intersect_volume_stack,
                             const ccl_global int *path_index_array,
                             const int work_size,
                             ccl_global hiprtGlobalStackBuffer stack_buffer)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    HIPRT_INIT_KERNEL_GLOBAL()
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_intersect_volume_stack(kg, state));
  }
}

ccl_gpu_kernel_threads(GPU_HIPRT_KERNEL_BLOCK_NUM_THREADS)
    ccl_gpu_kernel_signature(integrator_intersect_dedicated_light,
                             const ccl_global int *path_index_array,
                             const int work_size,
                             ccl_global hiprtGlobalStackBuffer stack_buffer)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    HIPRT_INIT_KERNEL_GLOBAL()
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_intersect_dedicated_light(kg, state));
  }
}

ccl_gpu_kernel_postfix
ccl_gpu_kernel_threads(GPU_HIPRT_KERNEL_BLOCK_NUM_THREADS)
    ccl_gpu_kernel_signature(integrator_shade_surface_raytrace,
                             const ccl_global int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size,
                             ccl_global hiprtGlobalStackBuffer stack_buffer)
{
  const int global_index = ccl_gpu_global_id_x();
  if (global_index < work_size) {
    HIPRT_INIT_KERNEL_GLOBAL()
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_shade_surface_raytrace(kg, state, render_buffer));
  }
}
ccl_gpu_kernel_postfix
ccl_gpu_kernel_threads(GPU_HIPRT_KERNEL_BLOCK_NUM_THREADS)
    ccl_gpu_kernel_signature(integrator_shade_surface_mnee,
                             const ccl_global int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size,
                             ccl_global hiprtGlobalStackBuffer stack_buffer)
{
  const int global_index = ccl_gpu_global_id_x();
  if (global_index < work_size) {
    HIPRT_INIT_KERNEL_GLOBAL()
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_shade_surface_mnee(kg, state, render_buffer));
  }
}
ccl_gpu_kernel_postfix

#endif /* __HIP_DEVICE_COMPILE__ */
