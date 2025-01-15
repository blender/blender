/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef __HIPRT__

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

#endif /* __HIPRT__ */
