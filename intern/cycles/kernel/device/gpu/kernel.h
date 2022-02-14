/*
 * Copyright 2011-2013 Blender Foundation
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

/* Common GPU kernels. */

#include "kernel/device/gpu/parallel_active_index.h"
#include "kernel/device/gpu/parallel_prefix_sum.h"
#include "kernel/device/gpu/parallel_sorted_index.h"

#include "kernel/sample/lcg.h"

/* Include constant tables before entering Metal's context class scope (context_begin.h) */
#include "kernel/tables.h"

#ifdef __KERNEL_METAL__
#  include "kernel/device/metal/context_begin.h"
#endif

#include "kernel/device/gpu/work_stealing.h"

#include "kernel/integrator/state.h"
#include "kernel/integrator/state_flow.h"
#include "kernel/integrator/state_util.h"

#include "kernel/integrator/init_from_bake.h"
#include "kernel/integrator/init_from_camera.h"
#include "kernel/integrator/intersect_closest.h"
#include "kernel/integrator/intersect_shadow.h"
#include "kernel/integrator/intersect_subsurface.h"
#include "kernel/integrator/intersect_volume_stack.h"
#include "kernel/integrator/shade_background.h"
#include "kernel/integrator/shade_light.h"
#include "kernel/integrator/shade_shadow.h"
#include "kernel/integrator/shade_surface.h"
#include "kernel/integrator/shade_volume.h"

#include "kernel/bake/bake.h"

#include "kernel/film/adaptive_sampling.h"

#ifdef __KERNEL_METAL__
#  include "kernel/device/metal/context_end.h"
#endif

#include "kernel/film/read.h"

/* --------------------------------------------------------------------
 * Integrator.
 */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_reset, int num_states)
{
  const int state = ccl_gpu_global_id_x();

  if (state < num_states) {
    INTEGRATOR_STATE_WRITE(state, path, queued_kernel) = 0;
    INTEGRATOR_STATE_WRITE(state, shadow_path, queued_kernel) = 0;
  }
}

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

  ccl_global const KernelWorkTile *tile = &tiles[tile_index];

  if (tile_work_index >= tile->work_size) {
    return;
  }

  const int state = tile->path_index_offset + tile_work_index;

  uint x, y, sample;
  ccl_gpu_kernel_call(get_work_pixel(tile, tile_work_index, &x, &y, &sample));

  ccl_gpu_kernel_call(
      integrator_init_from_camera(nullptr, state, tile, render_buffer, x, y, sample));
}

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

  ccl_global const KernelWorkTile *tile = &tiles[tile_index];

  if (tile_work_index >= tile->work_size) {
    return;
  }

  const int state = tile->path_index_offset + tile_work_index;

  uint x, y, sample;
  ccl_gpu_kernel_call(get_work_pixel(tile, tile_work_index, &x, &y, &sample));

  ccl_gpu_kernel_call(
      integrator_init_from_bake(nullptr, state, tile, render_buffer, x, y, sample));
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_intersect_closest,
                             ccl_global const int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_intersect_closest(NULL, state, render_buffer));
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_intersect_shadow,
                             ccl_global const int *path_index_array,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_intersect_shadow(NULL, state));
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_intersect_subsurface,
                             ccl_global const int *path_index_array,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_intersect_subsurface(NULL, state));
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_intersect_volume_stack,
                             ccl_global const int *path_index_array,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_intersect_volume_stack(NULL, state));
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shade_background,
                             ccl_global const int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_shade_background(NULL, state, render_buffer));
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shade_light,
                             ccl_global const int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_shade_light(NULL, state, render_buffer));
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shade_shadow,
                             ccl_global const int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_shade_shadow(NULL, state, render_buffer));
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shade_surface,
                             ccl_global const int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_shade_surface(NULL, state, render_buffer));
  }
}

#ifdef __KERNEL_METAL__
constant int __dummy_constant [[function_constant(0)]];
#endif

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shade_surface_raytrace,
                             ccl_global const int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;

#ifdef __KERNEL_METAL__
    KernelGlobals kg = NULL;
    /* Workaround Ambient Occlusion and Bevel nodes not working with Metal.
     * Dummy offset should not affect result, but somehow fixes bug! */
    kg += __dummy_constant;
    ccl_gpu_kernel_call(integrator_shade_surface_raytrace(kg, state, render_buffer));
#else
    ccl_gpu_kernel_call(integrator_shade_surface_raytrace(NULL, state, render_buffer));
#endif
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shade_volume,
                             ccl_global const int *path_index_array,
                             ccl_global float *render_buffer,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    ccl_gpu_kernel_call(integrator_shade_volume(NULL, state, render_buffer));
  }
}

ccl_gpu_kernel_threads(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_queued_paths_array,
                             int num_states,
                             ccl_global int *indices,
                             ccl_global int *num_indices,
                             int kernel_index)
{
  ccl_gpu_kernel_lambda(INTEGRATOR_STATE(state, path, queued_kernel) == kernel_index,
                        int kernel_index);
  ccl_gpu_kernel_lambda_pass.kernel_index = kernel_index;

  gpu_parallel_active_index_array(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE,
                                  num_states,
                                  indices,
                                  num_indices,
                                  ccl_gpu_kernel_lambda_pass);
}

ccl_gpu_kernel_threads(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_queued_shadow_paths_array,
                             int num_states,
                             ccl_global int *indices,
                             ccl_global int *num_indices,
                             int kernel_index)
{
  ccl_gpu_kernel_lambda(INTEGRATOR_STATE(state, shadow_path, queued_kernel) == kernel_index,
                        int kernel_index);
  ccl_gpu_kernel_lambda_pass.kernel_index = kernel_index;

  gpu_parallel_active_index_array(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE,
                                  num_states,
                                  indices,
                                  num_indices,
                                  ccl_gpu_kernel_lambda_pass);
}

ccl_gpu_kernel_threads(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_active_paths_array,
                             int num_states,
                             ccl_global int *indices,
                             ccl_global int *num_indices)
{
  ccl_gpu_kernel_lambda(INTEGRATOR_STATE(state, path, queued_kernel) != 0);

  gpu_parallel_active_index_array(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE,
                                  num_states,
                                  indices,
                                  num_indices,
                                  ccl_gpu_kernel_lambda_pass);
}

ccl_gpu_kernel_threads(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_terminated_paths_array,
                             int num_states,
                             ccl_global int *indices,
                             ccl_global int *num_indices,
                             int indices_offset)
{
  ccl_gpu_kernel_lambda(INTEGRATOR_STATE(state, path, queued_kernel) == 0);

  gpu_parallel_active_index_array(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE,
                                  num_states,
                                  indices + indices_offset,
                                  num_indices,
                                  ccl_gpu_kernel_lambda_pass);
}

ccl_gpu_kernel_threads(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_terminated_shadow_paths_array,
                             int num_states,
                             ccl_global int *indices,
                             ccl_global int *num_indices,
                             int indices_offset)
{
  ccl_gpu_kernel_lambda(INTEGRATOR_STATE(state, shadow_path, queued_kernel) == 0);

  gpu_parallel_active_index_array(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE,
                                  num_states,
                                  indices + indices_offset,
                                  num_indices,
                                  ccl_gpu_kernel_lambda_pass);
}

ccl_gpu_kernel_threads(GPU_PARALLEL_SORTED_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_sorted_paths_array,
                             int num_states,
                             int num_states_limit,
                             ccl_global int *indices,
                             ccl_global int *num_indices,
                             ccl_global int *key_counter,
                             ccl_global int *key_prefix_sum,
                             int kernel_index)
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

ccl_gpu_kernel_threads(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_compact_paths_array,
                             int num_states,
                             ccl_global int *indices,
                             ccl_global int *num_indices,
                             int num_active_paths)
{
  ccl_gpu_kernel_lambda((state >= num_active_paths) &&
                            (INTEGRATOR_STATE(state, path, queued_kernel) != 0),
                        int num_active_paths);
  ccl_gpu_kernel_lambda_pass.num_active_paths = num_active_paths;

  gpu_parallel_active_index_array(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE,
                                  num_states,
                                  indices,
                                  num_indices,
                                  ccl_gpu_kernel_lambda_pass);
}

ccl_gpu_kernel_threads(GPU_PARALLEL_SORTED_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_compact_states,
                             ccl_global const int *active_terminated_states,
                             const int active_states_offset,
                             const int terminated_states_offset,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int from_state = active_terminated_states[active_states_offset + global_index];
    const int to_state = active_terminated_states[terminated_states_offset + global_index];

    ccl_gpu_kernel_call(integrator_state_move(NULL, to_state, from_state));
  }
}

ccl_gpu_kernel_threads(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_compact_shadow_paths_array,
                             int num_states,
                             ccl_global int *indices,
                             ccl_global int *num_indices,
                             int num_active_paths)
{
  ccl_gpu_kernel_lambda((state >= num_active_paths) &&
                            (INTEGRATOR_STATE(state, shadow_path, queued_kernel) != 0),
                        int num_active_paths);
  ccl_gpu_kernel_lambda_pass.num_active_paths = num_active_paths;

  gpu_parallel_active_index_array(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE,
                                  num_states,
                                  indices,
                                  num_indices,
                                  ccl_gpu_kernel_lambda_pass);
}

ccl_gpu_kernel_threads(GPU_PARALLEL_SORTED_INDEX_DEFAULT_BLOCK_SIZE)
    ccl_gpu_kernel_signature(integrator_compact_shadow_states,
                             ccl_global const int *active_terminated_states,
                             const int active_states_offset,
                             const int terminated_states_offset,
                             const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int from_state = active_terminated_states[active_states_offset + global_index];
    const int to_state = active_terminated_states[terminated_states_offset + global_index];

    ccl_gpu_kernel_call(integrator_shadow_state_move(NULL, to_state, from_state));
  }
}

ccl_gpu_kernel_threads(GPU_PARALLEL_PREFIX_SUM_DEFAULT_BLOCK_SIZE) ccl_gpu_kernel_signature(
    prefix_sum, ccl_global int *counter, ccl_global int *prefix_sum, int num_values)
{
  gpu_parallel_prefix_sum(ccl_gpu_global_id_x(), counter, prefix_sum, num_values);
}

/* --------------------------------------------------------------------
 * Adaptive sampling.
 */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(adaptive_sampling_convergence_check,
                             ccl_global float *render_buffer,
                             int sx,
                             int sy,
                             int sw,
                             int sh,
                             float threshold,
                             bool reset,
                             int offset,
                             int stride,
                             ccl_global uint *num_active_pixels)
{
  const int work_index = ccl_gpu_global_id_x();
  const int y = work_index / sw;
  const int x = work_index - y * sw;

  bool converged = true;

  if (x < sw && y < sh) {
    converged = ccl_gpu_kernel_call(kernel_adaptive_sampling_convergence_check(
        nullptr, render_buffer, sx + x, sy + y, threshold, reset, offset, stride));
  }

  /* NOTE: All threads specified in the mask must execute the intrinsic. */
  const auto num_active_pixels_mask = ccl_gpu_ballot(!converged);
  const int lane_id = ccl_gpu_thread_idx_x % ccl_gpu_warp_size;
  if (lane_id == 0) {
    atomic_fetch_and_add_uint32(num_active_pixels, popcount(num_active_pixels_mask));
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(adaptive_sampling_filter_x,
                             ccl_global float *render_buffer,
                             int sx,
                             int sy,
                             int sw,
                             int sh,
                             int offset,
                             int stride)
{
  const int y = ccl_gpu_global_id_x();

  if (y < sh) {
    ccl_gpu_kernel_call(
        kernel_adaptive_sampling_filter_x(NULL, render_buffer, sy + y, sx, sw, offset, stride));
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(adaptive_sampling_filter_y,
                             ccl_global float *render_buffer,
                             int sx,
                             int sy,
                             int sw,
                             int sh,
                             int offset,
                             int stride)
{
  const int x = ccl_gpu_global_id_x();

  if (x < sw) {
    ccl_gpu_kernel_call(
        kernel_adaptive_sampling_filter_y(NULL, render_buffer, sx + x, sy, sh, offset, stride));
  }
}

/* --------------------------------------------------------------------
 * Cryptomatte.
 */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(cryptomatte_postprocess,
                             ccl_global float *render_buffer,
                             int num_pixels)
{
  const int pixel_index = ccl_gpu_global_id_x();

  if (pixel_index < num_pixels) {
    ccl_gpu_kernel_call(kernel_cryptomatte_post(nullptr, render_buffer, pixel_index));
  }
}

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
  /* Work around HIP issue with half float display, see T92972. */
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
    ccl_global const float *buffer = render_buffer + offset + x * kfilm_convert.pass_stride + \
                                     y * stride * kfilm_convert.pass_stride; \
\
    ccl_global float *pixel = pixels + \
                              (render_pixel_index + rgba_offset) * kfilm_convert.pixel_stride; \
\
    FILM_GET_PASS_PIXEL_F32(variant, input_channel_count); \
  } \
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
    ccl_global const float *buffer = render_buffer + offset + x * kfilm_convert.pass_stride + \
                                     y * stride * kfilm_convert.pass_stride; \
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
  }

/* 1 channel inputs */
KERNEL_FILM_CONVERT_VARIANT(depth, 1)
KERNEL_FILM_CONVERT_VARIANT(mist, 1)
KERNEL_FILM_CONVERT_VARIANT(sample_count, 1)
KERNEL_FILM_CONVERT_VARIANT(float, 1)

/* 3 channel inputs */
KERNEL_FILM_CONVERT_VARIANT(light_path, 3)
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
    ccl_gpu_kernel_call(kernel_displace_evaluate(NULL, input, output, offset + i));
  }
}

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
    ccl_gpu_kernel_call(kernel_background_evaluate(NULL, input, output, offset + i));
  }
}

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
        kernel_curve_shadow_transparency_evaluate(NULL, input, output, offset + i));
  }
}

/* --------------------------------------------------------------------
 * Denoising.
 */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(filter_color_preprocess,
                             ccl_global float *render_buffer,
                             int full_x,
                             int full_y,
                             int width,
                             int height,
                             int offset,
                             int stride,
                             int pass_stride,
                             int pass_denoised)
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

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(filter_guiding_preprocess,
                             ccl_global float *guiding_buffer,
                             int guiding_pass_stride,
                             int guiding_pass_albedo,
                             int guiding_pass_normal,
                             int guiding_pass_flow,
                             ccl_global const float *render_buffer,
                             int render_offset,
                             int render_stride,
                             int render_pass_stride,
                             int render_pass_sample_count,
                             int render_pass_denoising_albedo,
                             int render_pass_denoising_normal,
                             int render_pass_motion,
                             int full_x,
                             int full_y,
                             int width,
                             int height,
                             int num_samples)
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
  ccl_global const float *buffer = render_buffer + render_pixel_index * render_pass_stride;

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

    ccl_global const float *aledo_in = buffer + render_pass_denoising_albedo;
    ccl_global float *albedo_out = guiding_pixel + guiding_pass_albedo;

    albedo_out[0] = aledo_in[0] * pixel_scale;
    albedo_out[1] = aledo_in[1] * pixel_scale;
    albedo_out[2] = aledo_in[2] * pixel_scale;
  }

  /* Normal pass. */
  if (guiding_pass_normal != PASS_UNUSED) {
    kernel_assert(render_pass_denoising_normal != PASS_UNUSED);

    ccl_global const float *normal_in = buffer + render_pass_denoising_normal;
    ccl_global float *normal_out = guiding_pixel + guiding_pass_normal;

    normal_out[0] = normal_in[0] * pixel_scale;
    normal_out[1] = normal_in[1] * pixel_scale;
    normal_out[2] = normal_in[2] * pixel_scale;
  }

  /* Flow pass. */
  if (guiding_pass_flow != PASS_UNUSED) {
    kernel_assert(render_pass_motion != PASS_UNUSED);

    ccl_global const float *motion_in = buffer + render_pass_motion;
    ccl_global float *flow_out = guiding_pixel + guiding_pass_flow;

    flow_out[0] = -motion_in[0] * pixel_scale;
    flow_out[1] = -motion_in[1] * pixel_scale;
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(filter_guiding_set_fake_albedo,
                             ccl_global float *guiding_buffer,
                             int guiding_pass_stride,
                             int guiding_pass_albedo,
                             int width,
                             int height)
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

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(filter_color_postprocess,
                             ccl_global float *render_buffer,
                             int full_x,
                             int full_y,
                             int width,
                             int height,
                             int offset,
                             int stride,
                             int pass_stride,
                             int num_samples,
                             int pass_noisy,
                             int pass_denoised,
                             int pass_sample_count,
                             int num_components,
                             bool use_compositing)
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
    ccl_global const float *noisy_pixel = buffer + pass_noisy;
    denoised_pixel[3] = noisy_pixel[3];
  }
  else {
    /* Assigning to zero since this is a default alpha value for 3-component passes, and it
     * is an opaque pixel for 4 component passes. */
    denoised_pixel[3] = 0;
  }
}

/* --------------------------------------------------------------------
 * Shadow catcher.
 */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    ccl_gpu_kernel_signature(integrator_shadow_catcher_count_possible_splits,
                             int num_states,
                             ccl_global uint *num_possible_splits)
{
  const int state = ccl_gpu_global_id_x();

  bool can_split = false;

  if (state < num_states) {
    can_split = ccl_gpu_kernel_call(kernel_shadow_catcher_path_can_split(nullptr, state));
  }

  /* NOTE: All threads specified in the mask must execute the intrinsic. */
  const auto can_split_mask = ccl_gpu_ballot(can_split);
  const int lane_id = ccl_gpu_thread_idx_x % ccl_gpu_warp_size;
  if (lane_id == 0) {
    atomic_fetch_and_add_uint32(num_possible_splits, popcount(can_split_mask));
  }
}
