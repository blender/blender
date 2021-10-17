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

#include "kernel/integrator/integrator_state.h"
#include "kernel/integrator/integrator_state_flow.h"
#include "kernel/integrator/integrator_state_util.h"

#include "kernel/integrator/integrator_init_from_bake.h"
#include "kernel/integrator/integrator_init_from_camera.h"
#include "kernel/integrator/integrator_intersect_closest.h"
#include "kernel/integrator/integrator_intersect_shadow.h"
#include "kernel/integrator/integrator_intersect_subsurface.h"
#include "kernel/integrator/integrator_intersect_volume_stack.h"
#include "kernel/integrator/integrator_shade_background.h"
#include "kernel/integrator/integrator_shade_light.h"
#include "kernel/integrator/integrator_shade_shadow.h"
#include "kernel/integrator/integrator_shade_surface.h"
#include "kernel/integrator/integrator_shade_volume.h"

#include "kernel/kernel_adaptive_sampling.h"
#include "kernel/kernel_bake.h"
#include "kernel/kernel_film.h"
#include "kernel/kernel_work_stealing.h"

/* --------------------------------------------------------------------
 * Integrator.
 */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_integrator_reset(int num_states)
{
  const int state = ccl_gpu_global_id_x();

  if (state < num_states) {
    INTEGRATOR_STATE_WRITE(state, path, queued_kernel) = 0;
    INTEGRATOR_STATE_WRITE(state, shadow_path, queued_kernel) = 0;
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_integrator_init_from_camera(KernelWorkTile *tiles,
                                           const int num_tiles,
                                           float *render_buffer,
                                           const int max_tile_work_size)
{
  const int work_index = ccl_gpu_global_id_x();

  if (work_index >= max_tile_work_size * num_tiles) {
    return;
  }

  const int tile_index = work_index / max_tile_work_size;
  const int tile_work_index = work_index - tile_index * max_tile_work_size;

  const KernelWorkTile *tile = &tiles[tile_index];

  if (tile_work_index >= tile->work_size) {
    return;
  }

  const int state = tile->path_index_offset + tile_work_index;

  uint x, y, sample;
  get_work_pixel(tile, tile_work_index, &x, &y, &sample);

  integrator_init_from_camera(nullptr, state, tile, render_buffer, x, y, sample);
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_integrator_init_from_bake(KernelWorkTile *tiles,
                                         const int num_tiles,
                                         float *render_buffer,
                                         const int max_tile_work_size)
{
  const int work_index = ccl_gpu_global_id_x();

  if (work_index >= max_tile_work_size * num_tiles) {
    return;
  }

  const int tile_index = work_index / max_tile_work_size;
  const int tile_work_index = work_index - tile_index * max_tile_work_size;

  const KernelWorkTile *tile = &tiles[tile_index];

  if (tile_work_index >= tile->work_size) {
    return;
  }

  const int state = tile->path_index_offset + tile_work_index;

  uint x, y, sample;
  get_work_pixel(tile, tile_work_index, &x, &y, &sample);

  integrator_init_from_bake(nullptr, state, tile, render_buffer, x, y, sample);
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_integrator_intersect_closest(const int *path_index_array, const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    integrator_intersect_closest(NULL, state);
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_integrator_intersect_shadow(const int *path_index_array, const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    integrator_intersect_shadow(NULL, state);
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_integrator_intersect_subsurface(const int *path_index_array, const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    integrator_intersect_subsurface(NULL, state);
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_integrator_intersect_volume_stack(const int *path_index_array, const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    integrator_intersect_volume_stack(NULL, state);
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_integrator_shade_background(const int *path_index_array,
                                           float *render_buffer,
                                           const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    integrator_shade_background(NULL, state, render_buffer);
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_integrator_shade_light(const int *path_index_array,
                                      float *render_buffer,
                                      const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    integrator_shade_light(NULL, state, render_buffer);
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_integrator_shade_shadow(const int *path_index_array,
                                       float *render_buffer,
                                       const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    integrator_shade_shadow(NULL, state, render_buffer);
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_integrator_shade_surface(const int *path_index_array,
                                        float *render_buffer,
                                        const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    integrator_shade_surface(NULL, state, render_buffer);
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_integrator_shade_surface_raytrace(const int *path_index_array,
                                                 float *render_buffer,
                                                 const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    integrator_shade_surface_raytrace(NULL, state, render_buffer);
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_integrator_shade_volume(const int *path_index_array,
                                       float *render_buffer,
                                       const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int state = (path_index_array) ? path_index_array[global_index] : global_index;
    integrator_shade_volume(NULL, state, render_buffer);
  }
}

extern "C" __global__ void __launch_bounds__(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    kernel_gpu_integrator_queued_paths_array(int num_states,
                                             int *indices,
                                             int *num_indices,
                                             int kernel)
{
  gpu_parallel_active_index_array<GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE>(
      num_states, indices, num_indices, [kernel](const int state) {
        return (INTEGRATOR_STATE(state, path, queued_kernel) == kernel);
      });
}

extern "C" __global__ void __launch_bounds__(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    kernel_gpu_integrator_queued_shadow_paths_array(int num_states,
                                                    int *indices,
                                                    int *num_indices,
                                                    int kernel)
{
  gpu_parallel_active_index_array<GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE>(
      num_states, indices, num_indices, [kernel](const int state) {
        return (INTEGRATOR_STATE(state, shadow_path, queued_kernel) == kernel);
      });
}

extern "C" __global__ void __launch_bounds__(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    kernel_gpu_integrator_active_paths_array(int num_states, int *indices, int *num_indices)
{
  gpu_parallel_active_index_array<GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE>(
      num_states, indices, num_indices, [](const int state) {
        return (INTEGRATOR_STATE(state, path, queued_kernel) != 0) ||
               (INTEGRATOR_STATE(state, shadow_path, queued_kernel) != 0);
      });
}

extern "C" __global__ void __launch_bounds__(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    kernel_gpu_integrator_terminated_paths_array(int num_states,
                                                 int *indices,
                                                 int *num_indices,
                                                 int indices_offset)
{
  gpu_parallel_active_index_array<GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE>(
      num_states, indices + indices_offset, num_indices, [](const int state) {
        return (INTEGRATOR_STATE(state, path, queued_kernel) == 0) &&
               (INTEGRATOR_STATE(state, shadow_path, queued_kernel) == 0);
      });
}

extern "C" __global__ void __launch_bounds__(GPU_PARALLEL_SORTED_INDEX_DEFAULT_BLOCK_SIZE)
    kernel_gpu_integrator_sorted_paths_array(
        int num_states, int *indices, int *num_indices, int *key_prefix_sum, int kernel)
{
  gpu_parallel_sorted_index_array<GPU_PARALLEL_SORTED_INDEX_DEFAULT_BLOCK_SIZE>(
      num_states, indices, num_indices, key_prefix_sum, [kernel](const int state) {
        return (INTEGRATOR_STATE(state, path, queued_kernel) == kernel) ?
                   INTEGRATOR_STATE(state, path, shader_sort_key) :
                   GPU_PARALLEL_SORTED_INDEX_INACTIVE_KEY;
      });
}

extern "C" __global__ void __launch_bounds__(GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE)
    kernel_gpu_integrator_compact_paths_array(int num_states,
                                              int *indices,
                                              int *num_indices,
                                              int num_active_paths)
{
  gpu_parallel_active_index_array<GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE>(
      num_states, indices, num_indices, [num_active_paths](const int state) {
        return (state >= num_active_paths) &&
               ((INTEGRATOR_STATE(state, path, queued_kernel) != 0) ||
                (INTEGRATOR_STATE(state, shadow_path, queued_kernel) != 0));
      });
}

extern "C" __global__ void __launch_bounds__(GPU_PARALLEL_SORTED_INDEX_DEFAULT_BLOCK_SIZE)
    kernel_gpu_integrator_compact_states(const int *active_terminated_states,
                                         const int active_states_offset,
                                         const int terminated_states_offset,
                                         const int work_size)
{
  const int global_index = ccl_gpu_global_id_x();

  if (global_index < work_size) {
    const int from_state = active_terminated_states[active_states_offset + global_index];
    const int to_state = active_terminated_states[terminated_states_offset + global_index];

    integrator_state_move(NULL, to_state, from_state);
  }
}

extern "C" __global__ void __launch_bounds__(GPU_PARALLEL_PREFIX_SUM_DEFAULT_BLOCK_SIZE)
    kernel_gpu_prefix_sum(int *values, int num_values)
{
  gpu_parallel_prefix_sum<GPU_PARALLEL_PREFIX_SUM_DEFAULT_BLOCK_SIZE>(values, num_values);
}

/* --------------------------------------------------------------------
 * Adaptive sampling.
 */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_adaptive_sampling_convergence_check(float *render_buffer,
                                                   int sx,
                                                   int sy,
                                                   int sw,
                                                   int sh,
                                                   float threshold,
                                                   bool reset,
                                                   int offset,
                                                   int stride,
                                                   uint *num_active_pixels)
{
  const int work_index = ccl_gpu_global_id_x();
  const int y = work_index / sw;
  const int x = work_index - y * sw;

  bool converged = true;

  if (x < sw && y < sh) {
    converged = kernel_adaptive_sampling_convergence_check(
        nullptr, render_buffer, sx + x, sy + y, threshold, reset, offset, stride);
  }

  /* NOTE: All threads specified in the mask must execute the intrinsic. */
  const uint num_active_pixels_mask = ccl_gpu_ballot(!converged);
  const int lane_id = ccl_gpu_thread_idx_x % ccl_gpu_warp_size;
  if (lane_id == 0) {
    atomic_fetch_and_add_uint32(num_active_pixels, __popc(num_active_pixels_mask));
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_adaptive_sampling_filter_x(
        float *render_buffer, int sx, int sy, int sw, int sh, int offset, int stride)
{
  const int y = ccl_gpu_global_id_x();

  if (y < sh) {
    kernel_adaptive_sampling_filter_x(NULL, render_buffer, sy + y, sx, sw, offset, stride);
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_adaptive_sampling_filter_y(
        float *render_buffer, int sx, int sy, int sw, int sh, int offset, int stride)
{
  const int x = ccl_gpu_global_id_x();

  if (x < sw) {
    kernel_adaptive_sampling_filter_y(NULL, render_buffer, sx + x, sy, sh, offset, stride);
  }
}

/* --------------------------------------------------------------------
 * Cryptomatte.
 */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_cryptomatte_postprocess(float *render_buffer, int num_pixels)
{
  const int pixel_index = ccl_gpu_global_id_x();

  if (pixel_index < num_pixels) {
    kernel_cryptomatte_post(nullptr, render_buffer, pixel_index);
  }
}

/* --------------------------------------------------------------------
 * Film.
 */

/* Common implementation for float destination. */
template<typename Processor>
ccl_device_inline void kernel_gpu_film_convert_common(const KernelFilmConvert *kfilm_convert,
                                                      float *pixels,
                                                      float *render_buffer,
                                                      int num_pixels,
                                                      int width,
                                                      int offset,
                                                      int stride,
                                                      int dst_offset,
                                                      int dst_stride,
                                                      const Processor &processor)
{
  const int render_pixel_index = ccl_gpu_global_id_x();
  if (render_pixel_index >= num_pixels) {
    return;
  }

  const int x = render_pixel_index % width;
  const int y = render_pixel_index / width;

  ccl_global const float *buffer = render_buffer + offset + x * kfilm_convert->pass_stride +
                                   y * stride * kfilm_convert->pass_stride;

  ccl_global float *pixel = pixels +
                            (render_pixel_index + dst_offset) * kfilm_convert->pixel_stride;

  processor(kfilm_convert, buffer, pixel);
}

/* Common implementation for half4 destination and 4-channel input pass. */
template<typename Processor>
ccl_device_inline void kernel_gpu_film_convert_half_rgba_common_rgba(
    const KernelFilmConvert *kfilm_convert,
    uchar4 *rgba,
    float *render_buffer,
    int num_pixels,
    int width,
    int offset,
    int stride,
    int rgba_offset,
    int rgba_stride,
    const Processor &processor)
{
  const int render_pixel_index = ccl_gpu_global_id_x();
  if (render_pixel_index >= num_pixels) {
    return;
  }

  const int x = render_pixel_index % width;
  const int y = render_pixel_index / width;

  ccl_global const float *buffer = render_buffer + offset + x * kfilm_convert->pass_stride +
                                   y * stride * kfilm_convert->pass_stride;

  float pixel[4];
  processor(kfilm_convert, buffer, pixel);

  film_apply_pass_pixel_overlays_rgba(kfilm_convert, buffer, pixel);

  ccl_global half4 *out = ((ccl_global half4 *)rgba) + rgba_offset + y * rgba_stride + x;
  float4_store_half((ccl_global half *)out, make_float4(pixel[0], pixel[1], pixel[2], pixel[3]));
}

/* Common implementation for half4 destination and 3-channel input pass. */
template<typename Processor>
ccl_device_inline void kernel_gpu_film_convert_half_rgba_common_rgb(
    const KernelFilmConvert *kfilm_convert,
    uchar4 *rgba,
    float *render_buffer,
    int num_pixels,
    int width,
    int offset,
    int stride,
    int rgba_offset,
    int rgba_stride,
    const Processor &processor)
{
  kernel_gpu_film_convert_half_rgba_common_rgba(
      kfilm_convert,
      rgba,
      render_buffer,
      num_pixels,
      width,
      offset,
      stride,
      rgba_offset,
      rgba_stride,
      [&processor](const KernelFilmConvert *kfilm_convert,
                   ccl_global const float *buffer,
                   float *pixel_rgba) {
        processor(kfilm_convert, buffer, pixel_rgba);
        pixel_rgba[3] = 1.0f;
      });
}

/* Common implementation for half4 destination and single channel input pass. */
template<typename Processor>
ccl_device_inline void kernel_gpu_film_convert_half_rgba_common_value(
    const KernelFilmConvert *kfilm_convert,
    uchar4 *rgba,
    float *render_buffer,
    int num_pixels,
    int width,
    int offset,
    int stride,
    int rgba_offset,
    int rgba_stride,
    const Processor &processor)
{
  kernel_gpu_film_convert_half_rgba_common_rgba(
      kfilm_convert,
      rgba,
      render_buffer,
      num_pixels,
      width,
      offset,
      stride,
      rgba_offset,
      rgba_stride,
      [&processor](const KernelFilmConvert *kfilm_convert,
                   ccl_global const float *buffer,
                   float *pixel_rgba) {
        float value;
        processor(kfilm_convert, buffer, &value);

        pixel_rgba[0] = value;
        pixel_rgba[1] = value;
        pixel_rgba[2] = value;
        pixel_rgba[3] = 1.0f;
      });
}

#define KERNEL_FILM_CONVERT_PROC(name) \
  ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS) name

#define KERNEL_FILM_CONVERT_DEFINE(variant, channels) \
  KERNEL_FILM_CONVERT_PROC(kernel_gpu_film_convert_##variant) \
  (const KernelFilmConvert kfilm_convert, \
   float *pixels, \
   float *render_buffer, \
   int num_pixels, \
   int width, \
   int offset, \
   int stride, \
   int rgba_offset, \
   int rgba_stride) \
  { \
    kernel_gpu_film_convert_common(&kfilm_convert, \
                                   pixels, \
                                   render_buffer, \
                                   num_pixels, \
                                   width, \
                                   offset, \
                                   stride, \
                                   rgba_offset, \
                                   rgba_stride, \
                                   film_get_pass_pixel_##variant); \
  } \
  KERNEL_FILM_CONVERT_PROC(kernel_gpu_film_convert_##variant##_half_rgba) \
  (const KernelFilmConvert kfilm_convert, \
   uchar4 *rgba, \
   float *render_buffer, \
   int num_pixels, \
   int width, \
   int offset, \
   int stride, \
   int rgba_offset, \
   int rgba_stride) \
  { \
    kernel_gpu_film_convert_half_rgba_common_##channels(&kfilm_convert, \
                                                        rgba, \
                                                        render_buffer, \
                                                        num_pixels, \
                                                        width, \
                                                        offset, \
                                                        stride, \
                                                        rgba_offset, \
                                                        rgba_stride, \
                                                        film_get_pass_pixel_##variant); \
  }

KERNEL_FILM_CONVERT_DEFINE(depth, value)
KERNEL_FILM_CONVERT_DEFINE(mist, value)
KERNEL_FILM_CONVERT_DEFINE(sample_count, value)
KERNEL_FILM_CONVERT_DEFINE(float, value)

KERNEL_FILM_CONVERT_DEFINE(light_path, rgb)
KERNEL_FILM_CONVERT_DEFINE(float3, rgb)

KERNEL_FILM_CONVERT_DEFINE(motion, rgba)
KERNEL_FILM_CONVERT_DEFINE(cryptomatte, rgba)
KERNEL_FILM_CONVERT_DEFINE(shadow_catcher, rgba)
KERNEL_FILM_CONVERT_DEFINE(shadow_catcher_matte_with_shadow, rgba)
KERNEL_FILM_CONVERT_DEFINE(combined, rgba)
KERNEL_FILM_CONVERT_DEFINE(float4, rgba)

#undef KERNEL_FILM_CONVERT_DEFINE
#undef KERNEL_FILM_CONVERT_HALF_RGBA_DEFINE
#undef KERNEL_FILM_CONVERT_PROC

/* --------------------------------------------------------------------
 * Shader evaluation.
 */

/* Displacement */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_shader_eval_displace(KernelShaderEvalInput *input,
                                    float *output,
                                    const int offset,
                                    const int work_size)
{
  int i = ccl_gpu_global_id_x();
  if (i < work_size) {
    kernel_displace_evaluate(NULL, input, output, offset + i);
  }
}

/* Background Shader Evaluation */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_shader_eval_background(KernelShaderEvalInput *input,
                                      float *output,
                                      const int offset,
                                      const int work_size)
{
  int i = ccl_gpu_global_id_x();
  if (i < work_size) {
    kernel_background_evaluate(NULL, input, output, offset + i);
  }
}

/* --------------------------------------------------------------------
 * Denoising.
 */

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_filter_color_preprocess(float *render_buffer,
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
  float *buffer = render_buffer + render_pixel_index * pass_stride;

  float *color_out = buffer + pass_denoised;
  color_out[0] = clamp(color_out[0], 0.0f, 10000.0f);
  color_out[1] = clamp(color_out[1], 0.0f, 10000.0f);
  color_out[2] = clamp(color_out[2], 0.0f, 10000.0f);
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_filter_guiding_preprocess(float *guiding_buffer,
                                         int guiding_pass_stride,
                                         int guiding_pass_albedo,
                                         int guiding_pass_normal,
                                         const float *render_buffer,
                                         int render_offset,
                                         int render_stride,
                                         int render_pass_stride,
                                         int render_pass_sample_count,
                                         int render_pass_denoising_albedo,
                                         int render_pass_denoising_normal,
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
  float *guiding_pixel = guiding_buffer + guiding_pixel_index * guiding_pass_stride;

  const uint64_t render_pixel_index = render_offset + (x + full_x) + (y + full_y) * render_stride;
  const float *buffer = render_buffer + render_pixel_index * render_pass_stride;

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

    const float *aledo_in = buffer + render_pass_denoising_albedo;
    float *albedo_out = guiding_pixel + guiding_pass_albedo;

    albedo_out[0] = aledo_in[0] * pixel_scale;
    albedo_out[1] = aledo_in[1] * pixel_scale;
    albedo_out[2] = aledo_in[2] * pixel_scale;
  }

  /* Normal pass. */
  if (render_pass_denoising_normal != PASS_UNUSED) {
    kernel_assert(render_pass_denoising_normal != PASS_UNUSED);

    const float *normal_in = buffer + render_pass_denoising_normal;
    float *normal_out = guiding_pixel + guiding_pass_normal;

    normal_out[0] = normal_in[0] * pixel_scale;
    normal_out[1] = normal_in[1] * pixel_scale;
    normal_out[2] = normal_in[2] * pixel_scale;
  }
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_filter_guiding_set_fake_albedo(float *guiding_buffer,
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
  float *guiding_pixel = guiding_buffer + guiding_pixel_index * guiding_pass_stride;

  float *albedo_out = guiding_pixel + guiding_pass_albedo;

  albedo_out[0] = 0.5f;
  albedo_out[1] = 0.5f;
  albedo_out[2] = 0.5f;
}

ccl_gpu_kernel(GPU_KERNEL_BLOCK_NUM_THREADS, GPU_KERNEL_MAX_REGISTERS)
    kernel_gpu_filter_color_postprocess(float *render_buffer,
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
  float *buffer = render_buffer + render_pixel_index * pass_stride;

  float pixel_scale;
  if (pass_sample_count == PASS_UNUSED) {
    pixel_scale = num_samples;
  }
  else {
    pixel_scale = __float_as_uint(buffer[pass_sample_count]);
  }

  float *denoised_pixel = buffer + pass_denoised;

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
    const float *noisy_pixel = buffer + pass_noisy;
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
    kernel_gpu_integrator_shadow_catcher_count_possible_splits(int num_states,
                                                               uint *num_possible_splits)
{
  const int state = ccl_gpu_global_id_x();

  bool can_split = false;

  if (state < num_states) {
    can_split = kernel_shadow_catcher_path_can_split(nullptr, state);
  }

  /* NOTE: All threads specified in the mask must execute the intrinsic. */
  const uint can_split_mask = ccl_gpu_ballot(can_split);
  const int lane_id = ccl_gpu_thread_idx_x % ccl_gpu_warp_size;
  if (lane_id == 0) {
    atomic_fetch_and_add_uint32(num_possible_splits, __popc(can_split_mask));
  }
}
