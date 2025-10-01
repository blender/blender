/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Common GPU kernels. */

#include "kernel/device/gpu/parallel_active_index.h"
#include "kernel/device/gpu/parallel_prefix_sum.h"
#include "kernel/device/gpu/parallel_sorted_index.h"

#include "kernel/sample/lcg.h"

/* Include constant tables before entering Metal's context class scope (context_begin.h) */
#include "kernel/tables.h"

#ifdef __KERNEL_METAL__
#  include "kernel/device/metal/context_begin.h"
#elif defined(__KERNEL_ONEAPI__)
#  include "kernel/device/oneapi/context_begin.h"
#endif

#include "kernel/device/gpu/work_stealing.h"

#include "kernel/integrator/state.h"
#include "kernel/integrator/state_flow.h"
#include "kernel/integrator/state_util.h"

#include "kernel/integrator/init_from_bake.h"
#include "kernel/integrator/init_from_camera.h"
#include "kernel/integrator/intersect_closest.h"
#include "kernel/integrator/intersect_dedicated_light.h"
#include "kernel/integrator/intersect_shadow.h"
#include "kernel/integrator/intersect_subsurface.h"
#include "kernel/integrator/intersect_volume_stack.h"
#include "kernel/integrator/shade_background.h"
#include "kernel/integrator/shade_dedicated_light.h"
#include "kernel/integrator/shade_light.h"
#include "kernel/integrator/shade_shadow.h"
#include "kernel/integrator/shade_surface.h"
#include "kernel/integrator/shade_volume.h"

#include "kernel/bake/bake.h"

#include "kernel/film/adaptive_sampling.h"
#include "kernel/film/volume_guiding_denoise.h"

#ifdef __KERNEL_METAL__
#  include "kernel/device/metal/context_end.h"
#elif defined(__KERNEL_ONEAPI__)
#  include "kernel/device/oneapi/context_end.h"
#endif

#include "kernel/film/read.h"

#if defined(__HIPRT__)
#  include "kernel/device/hiprt/hiprt_kernels.h"
#endif
/* --------------------------------------------------------------------
 * Integrator.
 */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_reset, const int num_states)
{
  const int state = ccl_gpu_global_id_x();

  if (state < num_states) {
    INTEGRATOR_STATE_WRITE(state, path, queued_kernel) = 0;
    INTEGRATOR_STATE_WRITE(state, shadow_path, queued_kernel) = 0;
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_init_from_camera,
                             ccl_global KernelWorkTile *tiles,
                             const int num_tiles,
                             ccl_global float *render_buffer,
                             const int max_tile_work_size)
{
  const int work_index = ccl_gpu_global_id_x();

  if (work_index >= max_tile_work_size * num_tiles) {
    return;
  }

  const int tile_index = work_index / max_tile_work_size;
  const int tile_work_index = work_index - tile_index * max_tile_work_size;

  const ccl_global KernelWorkTile *tile = &tiles[tile_index];

  if (tile_work_index >= tile->work_size) {
    return;
  }

  const int state = tile->path_index_offset + tile_work_index;

  uint x, y, sample;
  ccl_gpu_kernel_call(get_work_pixel(tile, tile_work_index, &x, &y, &sample));

  ccl_gpu_kernel_call(
      integrator_init_from_camera(nullptr, state, tile, render_buffer, x, y, sample));
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_init_from_bake,
                             ccl_global KernelWorkTile *tiles,
                             const int num_tiles,
                             ccl_global float *render_buffer,
                             const int max_tile_work_size)
{
  const int work_index = ccl_gpu_global_id_x();

  if (work_index >= max_tile_work_size * num_tiles) {
    return;
  }

  const int tile_index = work_index / max_tile_work_size;
  const int tile_work_index = work_index - tile_index * max_tile_work_size;

  const ccl_global KernelWorkTile *tile = &tiles[tile_index];

  if (tile_work_index >= tile->work_size) {
    return;
  }

  const int state = tile->path_index_offset + tile_work_index;

  uint x, y, sample;
  ccl_gpu_kernel_call(get_work_pixel(tile, tile_work_index, &x, &y, &sample));

  ccl_gpu_kernel_call(
      integrator_init_from_bake(nullptr, state, tile, render_buffer, x, y, sample));
}
ccl_gpu_kernel_postfix

#if !defined(__HIPRT__)

/* Intersection kernels need access to the kernel handler for specialization constants to work
 * properly. */
#  ifdef __KERNEL_ONEAPI__
#    include "kernel/device/oneapi/context_intersect_begin.h"
#  endif

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_intersect_closest,
                             const ccl_global int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_intersect_closest(nullptr, state, render_buffer));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_intersect_shadow,
                             const ccl_global int *path_index_array,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_intersect_shadow(nullptr, state));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_intersect_subsurface,
                             const ccl_global int *path_index_array,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_intersect_subsurface(nullptr, state));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_intersect_volume_stack,
                             const ccl_global int *path_index_array,
                             const int work_size)
{
#  ifdef __VOLUME__
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_intersect_volume_stack(nullptr, state));
  }
#  endif
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_intersect_dedicated_light,
                             const ccl_global int *path_index_array,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_intersect_dedicated_light(nullptr, state));
  }
}
ccl_gpu_kernel_postfix

#  ifdef __KERNEL_ONEAPI__
#    include "kernel/device/oneapi/context_intersect_end.h"
#  endif

#endif

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shade_background,
                             const ccl_global int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_shade_background(nullptr, state, render_buffer));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shade_light,
                             const ccl_global int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_shade_light(nullptr, state, render_buffer));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shade_shadow,
                             const ccl_global int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_shade_shadow(nullptr, state, render_buffer));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shade_surface,
                             const ccl_global int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_shade_surface(nullptr, state, render_buffer));
  }
}
ccl_gpu_kernel_postfix

#if defined(__KERNEL_METAL_APPLE__) && defined(__METALRT__)
constant int __dummy_constant [[function_constant(Kernel_DummyConstant)]];
#endif

#if !defined(__HIPRT__)

/* Kernels using intersections need access to the kernel handler for specialization constants to
 * work properly. */
#  ifdef __KERNEL_ONEAPI__
#    include "kernel/device/oneapi/context_intersect_begin.h"
#  endif

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shade_surface_raytrace,
                             const ccl_global int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;

#  if defined(__KERNEL_METAL_APPLE__) && defined(__METALRT__)
    KernelGlobals kg = nullptr;
    /* Workaround Ambient Occlusion and Bevel nodes not working with Metal.
     * Dummy offset should not affect result, but somehow fixes bug! */
    kg += __dummy_constant;
    ccl_gpu_kernel_call(integrator_shade_surface_raytrace(kg, state, render_buffer));
#  else
    ccl_gpu_kernel_call(integrator_shade_surface_raytrace(nullptr, state, render_buffer));
#  endif
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shade_surface_mnee,
                             const ccl_global int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_shade_surface_mnee(nullptr, state, render_buffer));
  }
}
ccl_gpu_kernel_postfix

#  ifdef __KERNEL_ONEAPI__
#    include "kernel/device/oneapi/context_intersect_end.h"
#  endif

#endif

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shade_volume,
                             const ccl_global int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_shade_volume(nullptr, state, render_buffer));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shade_volume_ray_marching,
                             const ccl_global int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_shade_volume_ray_marching(nullptr, state, render_buffer));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shade_dedicated_light,
                             const ccl_global int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_shade_dedicated_light(nullptr, state, render_buffer));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel_threads(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_queued_paths_array,
                             const int num_states,
                             ccl_global int *indices,
                             ccl_global int *num_indices,
                             const int kernel_index)
{
  ccl_gpu_kernel_lambda(INTEGRATOR_STATE(state, path, queued_kernel) == kernel_index,
                        int kernel_index);
  ccl_gpu_kernel_lambda_pass.kernel_index = kernel_index;

  gpu_parallel_active_index_array(num_states, indices, num_indices, ccl_gpu_kernel_lambda_pass);
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel_threads(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_queued_shadow_paths_array,
                             const int num_states,
                             ccl_global int *indices,
                             ccl_global int *num_indices,
                             const int kernel_index)
{
  ccl_gpu_kernel_lambda(INTEGRATOR_STATE(state, shadow_path, queued_kernel) == kernel_index,
                        int kernel_index);
  ccl_gpu_kernel_lambda_pass.kernel_index = kernel_index;

  gpu_parallel_active_index_array(num_states, indices, num_indices, ccl_gpu_kernel_lambda_pass);
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel_threads(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_active_paths_array,
                             const int num_states,
                             ccl_global int *indices,
                             ccl_global int *num_indices)
{
  ccl_gpu_kernel_lambda(INTEGRATOR_STATE(state, path, queued_kernel) != 0);

  gpu_parallel_active_index_array(num_states, indices, num_indices, ccl_gpu_kernel_lambda_pass);
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel_threads(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_terminated_paths_array,
                             const int num_states,
                             ccl_global int *indices,
                             ccl_global int *num_indices,
                             const int indices_offset)
{
  ccl_gpu_kernel_lambda(INTEGRATOR_STATE(state, path, queued_kernel) == 0);

  gpu_parallel_active_index_array(
      num_states, indices + indices_offset, num_indices, ccl_gpu_kernel_lambda_pass);
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel_threads(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_terminated_shadow_paths_array,
                             const int num_states,
                             ccl_global int *indices,
                             ccl_global int *num_indices,
                             const int indices_offset)
{
  ccl_gpu_kernel_lambda(INTEGRATOR_STATE(state, shadow_path, queued_kernel) == 0);

  gpu_parallel_active_index_array(
      num_states, indices + indices_offset, num_indices, ccl_gpu_kernel_lambda_pass);
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel_threads(GPU_PARALLEL_SORTED_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_sorted_paths_array,
                             const int num_states,
                             const int num_states_limit,
                             ccl_global int *indices,
                             ccl_global int *num_indices,
                             ccl_global int *key_counter,
                             ccl_global int *key_prefix_sum,
                             const int kernel_index)
{
  ccl_gpu_kernel_lambda((INTEGRATOR_STATE(state, path, queued_kernel) == kernel_index) ?
                            INTEGRATOR_STATE(state, path, shader_sort_key) :
                            GPU_PARALLEL_SORTED_INDEX_INACTIVE_KEY,
                        int kernel_index);
  ccl_gpu_kernel_lambda_pass.kernel_index = kernel_index;

  const uint state_index = ccl_gpu_global_id_x();
  gpu_parallel_sorted_index_array(state_index,
                                  num_states,
                                  num_states_limit,
                                  indices,
                                  num_indices,
                                  key_counter,
                                  key_prefix_sum,
                                  ccl_gpu_kernel_lambda_pass);
}
ccl_gpu_kernel_postfix

/* oneAPI Verizon needs the local_mem accessor in the arguments. */
#ifdef __KERNEL_ONEAPI__
ccl_gpu_kernel_threads(GPU_PARALLEL_SORT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_sort_bucket_pass,
                             const int num_states,
                             const int partition_size,
                             const int num_states_limit,
                             ccl_global int *indices,
                             const int kernel_index,
                             sycl::local_accessor<int> &local_mem)
#else
ccl_gpu_kernel_threads(GPU_PARALLEL_SORT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_sort_bucket_pass,
                             const int num_states,
                             const int partition_size,
                             const int num_states_limit,
                             ccl_global int *indices,
                             const int kernel_index)
#endif
{
#if defined(__KERNEL_LOCAL_ATOMIC_SORT__)
  ccl_global ushort *d_queued_kernel = (ccl_global ushort *)
                                           kernel_integrator_state.path.queued_kernel;
  ccl_global uint *d_shader_sort_key = (ccl_global uint *)
                                           kernel_integrator_state.path.shader_sort_key;
  ccl_global int *key_offsets = (ccl_global int *)
                                    kernel_integrator_state.sort_partition_key_offsets;

#  ifdef __KERNEL_METAL__
  int max_shaders = context.launch_params_metal.data.max_shaders;
#  endif

#  ifdef __KERNEL_ONEAPI__
  /* Metal backend doesn't have these particular ccl_gpu_* defines and current kernel code
   * uses metal_*, we need the below to be compatible with these kernels. */
  int max_shaders = ((ONEAPIKernelContext *)kg)->__data->max_shaders;
  int metal_local_id = ccl_gpu_thread_idx_x;
  int metal_local_size = ccl_gpu_block_dim_x;
  int metal_grid_id = ccl_gpu_block_idx_x;
  /* There is no difference here between different access decorations, as we are requesting
   * a raw pointer immediately, so the simplest decoration option is used (no decoration). */
  ccl_gpu_shared int *threadgroup_array =
      local_mem.get_multi_ptr<sycl::access::decorated::no>().get();
#  endif

  gpu_parallel_sort_bucket_pass(num_states,
                                partition_size,
                                max_shaders,
                                kernel_index,
                                d_queued_kernel,
                                d_shader_sort_key,
                                key_offsets,
                                (ccl_gpu_shared int *)threadgroup_array,
                                metal_local_id,
                                metal_local_size,
                                metal_grid_id);
#endif
}
ccl_gpu_kernel_postfix

/* oneAPI version needs the local_mem accessor in the arguments. */
#ifdef __KERNEL_ONEAPI__
ccl_gpu_kernel_threads(GPU_PARALLEL_SORT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_sort_write_pass,
                             const int num_states,
                             const int partition_size,
                             const int num_states_limit,
                             ccl_global int *indices,
                             const int kernel_index,
                             sycl::local_accessor<int> &local_mem)
#else
ccl_gpu_kernel_threads(GPU_PARALLEL_SORT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_sort_write_pass,
                             const int num_states,
                             const int partition_size,
                             const int num_states_limit,
                             ccl_global int *indices,
                             const int kernel_index)
#endif

{
#if defined(__KERNEL_LOCAL_ATOMIC_SORT__)
  ccl_global ushort *d_queued_kernel = (ccl_global ushort *)
                                           kernel_integrator_state.path.queued_kernel;
  ccl_global uint *d_shader_sort_key = (ccl_global uint *)
                                           kernel_integrator_state.path.shader_sort_key;
  ccl_global int *key_offsets = (ccl_global int *)
                                    kernel_integrator_state.sort_partition_key_offsets;

#  ifdef __KERNEL_METAL__
  int max_shaders = context.launch_params_metal.data.max_shaders;
#  endif

#  ifdef __KERNEL_ONEAPI__
  /* Metal backend doesn't have these particular ccl_gpu_* defines and current kernel code
   * uses metal_*, we need the below to be compatible with these kernels. */
  int max_shaders = ((ONEAPIKernelContext *)kg)->__data->max_shaders;
  int metal_local_id = ccl_gpu_thread_idx_x;
  int metal_local_size = ccl_gpu_block_dim_x;
  int metal_grid_id = ccl_gpu_block_idx_x;
  /* There is no difference here between different access decorations, as we are requesting
   * a raw pointer immediately, so the simplest decoration option is used (no decoration). */
  ccl_gpu_shared int *threadgroup_array =
      local_mem.get_multi_ptr<sycl::access::decorated::no>().get();
#  endif

  gpu_parallel_sort_write_pass(num_states,
                               partition_size,
                               max_shaders,
                               kernel_index,
                               num_states_limit,
                               indices,
                               d_queued_kernel,
                               d_shader_sort_key,
                               key_offsets,
                               (ccl_gpu_shared int *)threadgroup_array,
                               metal_local_id,
                               metal_local_size,
                               metal_grid_id);
#endif
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel_threads(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_compact_paths_array,
                             const int num_states,
                             ccl_global int *indices,
                             ccl_global int *num_indices,
                             const int num_active_paths)
{
  ccl_gpu_kernel_lambda((state >= num_active_paths) &&
                            (INTEGRATOR_STATE(state, path, queued_kernel) != 0),
                        int num_active_paths);
  ccl_gpu_kernel_lambda_pass.num_active_paths = num_active_paths;

  gpu_parallel_active_index_array(num_states, indices, num_indices, ccl_gpu_kernel_lambda_pass);
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel_threads(GPU_PARALLEL_SORTED_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_compact_states,
                             const ccl_global int *active_terminated_states,
                             const int active_states_offset,
                             const int terminated_states_offset,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int from_state = active_terminated_states[active_states_offset + global_index];
    const int to_state = active_terminated_states[terminated_states_offset + global_index];

    ccl_gpu_kernel_call(integrator_state_move(nullptr, to_state, from_state));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel_threads(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_compact_shadow_paths_array,
                             const int num_states,
                             ccl_global int *indices,
                             ccl_global int *num_indices,
                             const int num_active_paths)
{
  ccl_gpu_kernel_lambda((state >= num_active_paths) &&
                            (INTEGRATOR_STATE(state, shadow_path, queued_kernel) != 0),
                        int num_active_paths);
  ccl_gpu_kernel_lambda_pass.num_active_paths = num_active_paths;

  gpu_parallel_active_index_array(num_states, indices, num_indices, ccl_gpu_kernel_lambda_pass);
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel_threads(GPU_PARALLEL_SORTED_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_compact_shadow_states,
                             const ccl_global int *active_terminated_states,
                             const int active_states_offset,
                             const int terminated_states_offset,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (ccl_gpu_kernel_within_bounds(global_index, work_size)) {
    const int from_state = active_terminated_states[active_states_offset + global_index];
    const int to_state = active_terminated_states[terminated_states_offset + global_index];

    ccl_gpu_kernel_call(integrator_shadow_state_move(nullptr, to_state, from_state));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel_threads(GPU_PARALLEL_PREFIX_SUM_DEFAULT_BLOCK_SIZE) ccl_gpu_kernel_signature(
    prefix_sum, ccl_global int *counter, ccl_global int *prefix_sum, const int num_values)
{
  gpu_parallel_prefix_sum(ccl_gpu_global_id_x(), counter, prefix_sum, num_values);
}
ccl_gpu_kernel_postfix

/* --------------------------------------------------------------------
 * Adaptive sampling.
 */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(adaptive_sampling_convergence_check,
                             ccl_global float *render_buffer,
                             const int sx,
                             const int sy,
                             const int sw,
                             const int sh,
                             const float threshold,
                             const int reset,
                             const int offset,
                             const int stride,
                             ccl_global uint *num_active_pixels)
{
  const int work_index = ccl_gpu_global_id_x();
  const int y = work_index / sw;
  const int x = work_index - y * sw;

  bool converged = true;

  if (x < sw && y < sh) {
    converged = ccl_gpu_kernel_call(film_adaptive_sampling_convergence_check(
        nullptr, render_buffer, sx + x, sy + y, threshold, reset, offset, stride));
  }

  /* NOTE: All threads specified in the mask must execute the intrinsic. */
  const auto num_active_pixels_mask = ccl_gpu_ballot(!converged);
  const int lane_id = ccl_gpu_thread_idx_x % ccl_gpu_warp_size;
  if (lane_id == 0) {
    atomic_fetch_and_add_uint32(num_active_pixels, popcount(num_active_pixels_mask));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(adaptive_sampling_filter_x,
                             ccl_global float *render_buffer,
                             const int sx,
                             const int sy,
                             const int sw,
                             const int sh,
                             const int offset,
                             const int stride)
{
  const int y = ccl_gpu_global_id_x();

  if (y < sh) {
    ccl_gpu_kernel_call(
        film_adaptive_sampling_filter_x(nullptr, render_buffer, sy + y, sx, sw, offset, stride));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(adaptive_sampling_filter_y,
                             ccl_global float *render_buffer,
                             const int sx,
                             const int sy,
                             const int sw,
                             const int sh,
                             const int offset,
                             const int stride)
{
  const int x = ccl_gpu_global_id_x();

  if (x < sw) {
    ccl_gpu_kernel_call(
        film_adaptive_sampling_filter_y(nullptr, render_buffer, sx + x, sy, sh, offset, stride));
  }
}
ccl_gpu_kernel_postfix

/* --------------------------------------------------------------------
 * Cryptomatte.
 */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(cryptomatte_postprocess,
                             ccl_global float *render_buffer,
                             const int num_pixels)
{
  const int pixel_index = ccl_gpu_global_id_x();

  if (pixel_index < num_pixels) {
    ccl_gpu_kernel_call(film_cryptomatte_post(nullptr, render_buffer, pixel_index));
  }
}
ccl_gpu_kernel_postfix

/* --------------------------------------------------------------------
 * Film.
 */

ccl_device_inline void kernel_gpu_film_convert_half_write(ccl_global uchar4 *rgba,
                                                          const int rgba_offset,
                                                          const int rgba_stride,
                                                          const int x,
                                                          const int y,
                                                          const half4 half_pixel)
{
  /* Work around HIP issue with half float display, see #92972. */
#ifdef __KERNEL_HIP__
  ccl_global half *out = ((ccl_global half *)rgba) + (rgba_offset + y * rgba_stride + x) * 4;
  out[0] = half_pixel.x;
  out[1] = half_pixel.y;
  out[2] = half_pixel.z;
  out[3] = half_pixel.w;
#else
  ccl_global half4 *out = ((ccl_global half4 *)rgba) + rgba_offset + y * rgba_stride + x;
  *out = half_pixel;
#endif
}

#ifdef __KERNEL_METAL__

/* Fetch into a local variable on Metal - there is minimal overhead. Templating the
 * film_get_pass_pixel_... functions works on MSL, but not on other compilers. */
#  define FILM_GET_PASS_PIXEL_F32(variant, input_channel_count) \
    float local_pixel[4]; \
    film_get_pass_pixel_##variant(&kfilm_convert, buffer, local_pixel); \
    if (input_channel_count >= 1) { \
      pixel[0] = local_pixel[0]; \
    } \
    if (input_channel_count >= 2) { \
      pixel[1] = local_pixel[1]; \
    } \
    if (input_channel_count >= 3) { \
      pixel[2] = local_pixel[2]; \
    } \
    if (input_channel_count >= 4) { \
      pixel[3] = local_pixel[3]; \
    }

#else

#  define FILM_GET_PASS_PIXEL_F32(variant, input_channel_count) \
    film_get_pass_pixel_##variant(&kfilm_convert, buffer, pixel);

#endif

#define KERNEL_FILM_CONVERT_VARIANT(variant, input_channel_count) \
  ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS) \
      ccl_gpu_kernel_signature(film_convert_##variant, \
                               const KernelFilmConvert kfilm_convert, \
                               ccl_global float *pixels, \
                               ccl_global float *render_buffer, \
                               int num_pixels, \
                               int width, \
                               int offset, \
                               int stride, \
                               int channel_offset, \
                               int rgba_offset, \
                               int rgba_stride) \
  { \
    const int render_pixel_index = ccl_gpu_global_id_x(); \
    if (render_pixel_index >= num_pixels) { \
      return; \
    } \
\
    const int x = render_pixel_index % width; \
    const int y = render_pixel_index / width; \
\
    const uint64_t buffer_pixel_index = x + y * stride; \
    ccl_global const float *buffer = render_buffer + offset + \
                                     buffer_pixel_index * kfilm_convert.pass_stride; \
\
    ccl_global float *pixel = pixels + channel_offset + \
                              (render_pixel_index + rgba_offset) * kfilm_convert.pixel_stride; \
\
    FILM_GET_PASS_PIXEL_F32(variant, input_channel_count); \
  } \
  ccl_gpu_kernel_postfix \
\
  ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS) \
      ccl_gpu_kernel_signature(film_convert_##variant##_half_rgba, \
                               const KernelFilmConvert kfilm_convert, \
                               ccl_global uchar4 *rgba, \
                               ccl_global float *render_buffer, \
                               int num_pixels, \
                               int width, \
                               int offset, \
                               int stride, \
                               int rgba_offset, \
                               int rgba_stride) \
  { \
    const int render_pixel_index = ccl_gpu_global_id_x(); \
    if (render_pixel_index >= num_pixels) { \
      return; \
    } \
\
    const int x = render_pixel_index % width; \
    const int y = render_pixel_index / width; \
\
    const uint64_t buffer_pixel_index = x + y * stride; \
    ccl_global const float *buffer = render_buffer + offset + \
                                     buffer_pixel_index * kfilm_convert.pass_stride; \
\
    float pixel[4]; \
    film_get_pass_pixel_##variant(&kfilm_convert, buffer, pixel); \
\
    if (input_channel_count == 1) { \
      pixel[1] = pixel[2] = pixel[0]; \
    } \
    if (input_channel_count <= 3) { \
      pixel[3] = 1.0f; \
    } \
\
    film_apply_pass_pixel_overlays_rgba(&kfilm_convert, buffer, pixel); \
\
    const half4 half_pixel = float4_to_half4_display( \
        make_float4(pixel[0], pixel[1], pixel[2], pixel[3])); \
    kernel_gpu_film_convert_half_write(rgba, rgba_offset, rgba_stride, x, y, half_pixel); \
  } \
  ccl_gpu_kernel_postfix

/* 1 channel inputs */
KERNEL_FILM_CONVERT_VARIANT(depth, 1)
KERNEL_FILM_CONVERT_VARIANT(mist, 1)
KERNEL_FILM_CONVERT_VARIANT(volume_majorant, 1)
KERNEL_FILM_CONVERT_VARIANT(sample_count, 1)
KERNEL_FILM_CONVERT_VARIANT(float, 1)

/* 3 channel inputs */
KERNEL_FILM_CONVERT_VARIANT(light_path, 3)
KERNEL_FILM_CONVERT_VARIANT(rgbe, 3)
KERNEL_FILM_CONVERT_VARIANT(float3, 3)

/* 4 channel inputs */
KERNEL_FILM_CONVERT_VARIANT(motion, 4)
KERNEL_FILM_CONVERT_VARIANT(cryptomatte, 4)
KERNEL_FILM_CONVERT_VARIANT(shadow_catcher, 4)
KERNEL_FILM_CONVERT_VARIANT(shadow_catcher_matte_with_shadow, 4)
KERNEL_FILM_CONVERT_VARIANT(combined, 4)
KERNEL_FILM_CONVERT_VARIANT(float4, 4)

#undef KERNEL_FILM_CONVERT_VARIANT

/* --------------------------------------------------------------------
 * Shader evaluation.
 */

/* Displacement */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(shader_eval_displace,
                             ccl_global KernelShaderEvalInput *input,
                             ccl_global float *output,
                             const int offset,
                             const int work_size)
{
  int i = ccl_gpu_global_id_x();
  if (i < work_size) {
    ccl_gpu_kernel_call(kernel_displace_evaluate(nullptr, input, output, offset + i));
  }
}
ccl_gpu_kernel_postfix

/* Background */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(shader_eval_background,
                             ccl_global KernelShaderEvalInput *input,
                             ccl_global float *output,
                             const int offset,
                             const int work_size)
{
  int i = ccl_gpu_global_id_x();
  if (i < work_size) {
    ccl_gpu_kernel_call(kernel_background_evaluate(nullptr, input, output, offset + i));
  }
}
ccl_gpu_kernel_postfix

/* Curve Shadow Transparency */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(shader_eval_curve_shadow_transparency,
                             ccl_global KernelShaderEvalInput *input,
                             ccl_global float *output,
                             const int offset,
                             const int work_size)
{
  int i = ccl_gpu_global_id_x();
  if (i < work_size) {
    ccl_gpu_kernel_call(
        kernel_curve_shadow_transparency_evaluate(nullptr, input, output, offset + i));
  }
}
ccl_gpu_kernel_postfix

/* Volume Density. */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(shader_eval_volume_density,
                             ccl_global KernelShaderEvalInput *input,
                             ccl_global float *output,
                             const int offset,
                             const int work_size)
{
  int i = ccl_gpu_global_id_x();
  if (i < work_size) {
    ccl_gpu_kernel_call(kernel_volume_density_evaluate(nullptr, input, output, offset + i));
  }
}
ccl_gpu_kernel_postfix

/* --------------------------------------------------------------------
 * Denoising.
 */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(filter_color_preprocess,
                             ccl_global float *render_buffer,
                             const int full_x,
                             const int full_y,
                             const int width,
                             const int height,
                             const int offset,
                             const int stride,
                             const int pass_stride,
                             const int pass_denoised)
{
  const int work_index = ccl_gpu_global_id_x();
  const int y = work_index / width;
  const int x = work_index - y * width;

  if (x >= width || y >= height) {
    return;
  }

  const uint64_t render_pixel_index = offset + (x + full_x) + (y + full_y) * stride;
  ccl_global float *buffer = render_buffer + render_pixel_index * pass_stride;

  ccl_global float *color_out = buffer + pass_denoised;
  color_out[0] = clamp(color_out[0], 0.0f, 10000.0f);
  color_out[1] = clamp(color_out[1], 0.0f, 10000.0f);
  color_out[2] = clamp(color_out[2], 0.0f, 10000.0f);
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(filter_guiding_preprocess,
                             ccl_global float *guiding_buffer,
                             const int guiding_pass_stride,
                             const int guiding_pass_albedo,
                             const int guiding_pass_normal,
                             const int guiding_pass_flow,
                             const ccl_global float *render_buffer,
                             const int render_offset,
                             const int render_stride,
                             const int render_pass_stride,
                             const int render_pass_sample_count,
                             const int render_pass_denoising_albedo,
                             const int render_pass_denoising_normal,
                             const int render_pass_motion,
                             const int full_x,
                             const int full_y,
                             const int width,
                             const int height,
                             const int num_samples)
{
  const int work_index = ccl_gpu_global_id_x();
  const int y = work_index / width;
  const int x = work_index - y * width;

  if (x >= width || y >= height) {
    return;
  }

  const uint64_t guiding_pixel_index = x + y * width;
  ccl_global float *guiding_pixel = guiding_buffer + guiding_pixel_index * guiding_pass_stride;

  const uint64_t render_pixel_index = render_offset + (x + full_x) + (y + full_y) * render_stride;
  const ccl_global float *buffer = render_buffer + render_pixel_index * render_pass_stride;

  float pixel_scale;
  if (render_pass_sample_count == PASS_UNUSED) {
    pixel_scale = 1.0f / num_samples;
  }
  else {
    pixel_scale = 1.0f / __float_as_uint(buffer[render_pass_sample_count]);
  }

  /* Albedo pass. */
  if (guiding_pass_albedo != PASS_UNUSED) {
    kernel_assert(render_pass_denoising_albedo != PASS_UNUSED);

    const ccl_global float *albedo_in = buffer + render_pass_denoising_albedo;
    ccl_global float *albedo_out = guiding_pixel + guiding_pass_albedo;

    albedo_out[0] = albedo_in[0] * pixel_scale;
    albedo_out[1] = albedo_in[1] * pixel_scale;
    albedo_out[2] = albedo_in[2] * pixel_scale;
  }

  /* Normal pass. */
  if (guiding_pass_normal != PASS_UNUSED) {
    kernel_assert(render_pass_denoising_normal != PASS_UNUSED);

    const ccl_global float *normal_in = buffer + render_pass_denoising_normal;
    ccl_global float *normal_out = guiding_pixel + guiding_pass_normal;

    normal_out[0] = normal_in[0] * pixel_scale;
    normal_out[1] = normal_in[1] * pixel_scale;
    normal_out[2] = normal_in[2] * pixel_scale;
  }

  /* Flow pass. */
  if (guiding_pass_flow != PASS_UNUSED) {
    kernel_assert(render_pass_motion != PASS_UNUSED);

    const ccl_global float *motion_in = buffer + render_pass_motion;
    ccl_global float *flow_out = guiding_pixel + guiding_pass_flow;

    flow_out[0] = -motion_in[0] * pixel_scale;
    flow_out[1] = -motion_in[1] * pixel_scale;
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(filter_guiding_set_fake_albedo,
                             ccl_global float *guiding_buffer,
                             const int guiding_pass_stride,
                             const int guiding_pass_albedo,
                             const int width,
                             const int height)
{
  kernel_assert(guiding_pass_albedo != PASS_UNUSED);

  const int work_index = ccl_gpu_global_id_x();
  const int y = work_index / width;
  const int x = work_index - y * width;

  if (x >= width || y >= height) {
    return;
  }

  const uint64_t guiding_pixel_index = x + y * width;
  ccl_global float *guiding_pixel = guiding_buffer + guiding_pixel_index * guiding_pass_stride;

  ccl_global float *albedo_out = guiding_pixel + guiding_pass_albedo;

  albedo_out[0] = 0.5f;
  albedo_out[1] = 0.5f;
  albedo_out[2] = 0.5f;
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(filter_color_postprocess,
                             ccl_global float *render_buffer,
                             const int full_x,
                             const int full_y,
                             const int width,
                             const int height,
                             const int offset,
                             const int stride,
                             const int pass_stride,
                             const int num_samples,
                             const int pass_noisy,
                             const int pass_denoised,
                             const int pass_sample_count,
                             const int num_components,
                             const int use_compositing)
{
  const int work_index = ccl_gpu_global_id_x();
  const int y = work_index / width;
  const int x = work_index - y * width;

  if (x >= width || y >= height) {
    return;
  }

  const uint64_t render_pixel_index = offset + (x + full_x) + (y + full_y) * stride;
  ccl_global float *buffer = render_buffer + render_pixel_index * pass_stride;

  float pixel_scale;
  if (pass_sample_count == PASS_UNUSED) {
    pixel_scale = num_samples;
  }
  else {
    pixel_scale = __float_as_uint(buffer[pass_sample_count]);
  }

  ccl_global float *denoised_pixel = buffer + pass_denoised;

  denoised_pixel[0] *= pixel_scale;
  denoised_pixel[1] *= pixel_scale;
  denoised_pixel[2] *= pixel_scale;

  if (num_components == 3) {
    /* Pass without alpha channel. */
  }
  else if (!use_compositing) {
    /* Currently compositing passes are either 3-component (derived by dividing light passes)
     * or do not have transparency (shadow catcher). Implicitly rely on this logic, as it
     * simplifies logic and avoids extra memory allocation. */
    const ccl_global float *noisy_pixel = buffer + pass_noisy;
    denoised_pixel[3] = noisy_pixel[3];
  }
  else {
    /* Assigning to zero since this is a default alpha value for 3-component passes, and it
     * is an opaque pixel for 4 component passes. */
    denoised_pixel[3] = 0;
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(filter_color_flip_y,
                             ccl_global float *render_buffer,
                             const int full_x,
                             const int full_y,
                             const int width,
                             const int height,
                             const int offset,
                             const int stride,
                             const int pass_stride,
                             const int pass_denoised)
{
  const int work_index = ccl_gpu_global_id_x();
  const int y = work_index / width;
  const int x = work_index - y * width;

  if (x >= width || y >= height / 2) {
    return;
  }

  const uint64_t render_pixel_index = offset + (x + full_x) + (y + full_y) * stride;
  ccl_global float *buffer = render_buffer + render_pixel_index * pass_stride + pass_denoised;
  ccl_global float *buffer_flipped = buffer + (height - 1 - y * 2) * stride * pass_stride;

  float3 temp;
  temp.x = buffer[0];
  temp.y = buffer[1];
  temp.z = buffer[2];
  buffer[0] = buffer_flipped[0];
  buffer[1] = buffer_flipped[1];
  buffer[2] = buffer_flipped[2];
  buffer_flipped[0] = temp.x;
  buffer_flipped[1] = temp.y;
  buffer_flipped[2] = temp.z;
}
ccl_gpu_kernel_postfix

/* --------------------------------------------------------------------
 * Shadow catcher.
 */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shadow_catcher_count_possible_splits,
                             const int num_states,
                             ccl_global uint *num_possible_splits)
{
  const int state = ccl_gpu_global_id_x();

  bool can_split = false;

  if (state < num_states) {
    can_split = ccl_gpu_kernel_call(kernel_shadow_catcher_path_can_split(state));
  }

  /* NOTE: All threads specified in the mask must execute the intrinsic. */
  const auto can_split_mask = ccl_gpu_ballot(can_split);
  const int lane_id = ccl_gpu_thread_idx_x % ccl_gpu_warp_size;
  if (lane_id == 0) {
    atomic_fetch_and_add_uint32(num_possible_splits, popcount(can_split_mask));
  }
}
ccl_gpu_kernel_postfix

/* --------------------------------------------------------------------
 * Volume Scattering Probability Guiding.
 */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(volume_guiding_filter_x,
                             ccl_global float *render_buffer,
                             const int sx,
                             const int sy,
                             const int sw,
                             const int sh,
                             const int offset,
                             const int stride)
{
  const int work_index = ccl_gpu_global_id_x();
  const int y = work_index / sw;
  const int x = work_index % sw;

  if (y < sh) {
    ccl_gpu_kernel_call(volume_guiding_filter_x(
        nullptr, render_buffer, sy + y, sx + x, sx, sx + sw, offset, stride));
  }
}
ccl_gpu_kernel_postfix

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(volume_guiding_filter_y,
                             ccl_global float *render_buffer,
                             const int sx,
                             const int sy,
                             const int sw,
                             const int sh,
                             const int offset,
                             const int stride)
{
  const int x = ccl_gpu_global_id_x();

  if (x < sw) {
    ccl_gpu_kernel_call(
        volume_guiding_filter_y(nullptr, render_buffer, sx + x, sy, sy + sh, offset, stride));
  }
}
ccl_gpu_kernel_postfix
