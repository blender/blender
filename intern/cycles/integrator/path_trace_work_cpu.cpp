/*
 * Copyright 2011-2021 Blender Foundation
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

#include "integrator/path_trace_work_cpu.h"

#include "device/cpu/kernel.h"
#include "device/device.h"

#include "kernel/integrator/path_state.h"

#include "integrator/pass_accessor_cpu.h"
#include "integrator/path_trace_display.h"

#include "scene/scene.h"
#include "session/buffers.h"

#include "util/atomic.h"
#include "util/log.h"
#include "util/tbb.h"

CCL_NAMESPACE_BEGIN

/* Create TBB arena for execution of path tracing and rendering tasks. */
static inline tbb::task_arena local_tbb_arena_create(const Device *device)
{
  /* TODO: limit this to number of threads of CPU device, it may be smaller than
   * the system number of threads when we reduce the number of CPU threads in
   * CPU + GPU rendering to dedicate some cores to handling the GPU device. */
  return tbb::task_arena(device->info.cpu_threads);
}

/* Get CPUKernelThreadGlobals for the current thread. */
static inline CPUKernelThreadGlobals *kernel_thread_globals_get(
    vector<CPUKernelThreadGlobals> &kernel_thread_globals)
{
  const int thread_index = tbb::this_task_arena::current_thread_index();
  DCHECK_GE(thread_index, 0);
  DCHECK_LE(thread_index, kernel_thread_globals.size());

  return &kernel_thread_globals[thread_index];
}

PathTraceWorkCPU::PathTraceWorkCPU(Device *device,
                                   Film *film,
                                   DeviceScene *device_scene,
                                   bool *cancel_requested_flag)
    : PathTraceWork(device, film, device_scene, cancel_requested_flag),
      kernels_(*(device->get_cpu_kernels()))
{
  DCHECK_EQ(device->info.type, DEVICE_CPU);
}

void PathTraceWorkCPU::init_execution()
{
  /* Cache per-thread kernel globals. */
  device_->get_cpu_kernel_thread_globals(kernel_thread_globals_);
}

void PathTraceWorkCPU::render_samples(RenderStatistics &statistics,
                                      int start_sample,
                                      int samples_num)
{
  const int64_t image_width = effective_buffer_params_.width;
  const int64_t image_height = effective_buffer_params_.height;
  const int64_t total_pixels_num = image_width * image_height;

  for (CPUKernelThreadGlobals &kernel_globals : kernel_thread_globals_) {
    kernel_globals.start_profiling();
  }

  tbb::task_arena local_arena = local_tbb_arena_create(device_);
  local_arena.execute([&]() {
    tbb::parallel_for(int64_t(0), total_pixels_num, [&](int64_t work_index) {
      if (is_cancel_requested()) {
        return;
      }

      const int y = work_index / image_width;
      const int x = work_index - y * image_width;

      KernelWorkTile work_tile;
      work_tile.x = effective_buffer_params_.full_x + x;
      work_tile.y = effective_buffer_params_.full_y + y;
      work_tile.w = 1;
      work_tile.h = 1;
      work_tile.start_sample = start_sample;
      work_tile.num_samples = 1;
      work_tile.offset = effective_buffer_params_.offset;
      work_tile.stride = effective_buffer_params_.stride;

      CPUKernelThreadGlobals *kernel_globals = kernel_thread_globals_get(kernel_thread_globals_);

      render_samples_full_pipeline(kernel_globals, work_tile, samples_num);
    });
  });

  for (CPUKernelThreadGlobals &kernel_globals : kernel_thread_globals_) {
    kernel_globals.stop_profiling();
  }

  statistics.occupancy = 1.0f;
}

void PathTraceWorkCPU::render_samples_full_pipeline(KernelGlobalsCPU *kernel_globals,
                                                    const KernelWorkTile &work_tile,
                                                    const int samples_num)
{
  const bool has_bake = device_scene_->data.bake.use;

  IntegratorStateCPU integrator_states[2];

  IntegratorStateCPU *state = &integrator_states[0];
  IntegratorStateCPU *shadow_catcher_state = nullptr;

  if (device_scene_->data.integrator.has_shadow_catcher) {
    shadow_catcher_state = &integrator_states[1];
    path_state_init_queues(shadow_catcher_state);
  }

  KernelWorkTile sample_work_tile = work_tile;
  float *render_buffer = buffers_->buffer.data();

  for (int sample = 0; sample < samples_num; ++sample) {
    if (is_cancel_requested()) {
      break;
    }

    if (has_bake) {
      if (!kernels_.integrator_init_from_bake(
              kernel_globals, state, &sample_work_tile, render_buffer)) {
        break;
      }
    }
    else {
      if (!kernels_.integrator_init_from_camera(
              kernel_globals, state, &sample_work_tile, render_buffer)) {
        break;
      }
    }

    kernels_.integrator_megakernel(kernel_globals, state, render_buffer);

    if (shadow_catcher_state) {
      kernels_.integrator_megakernel(kernel_globals, shadow_catcher_state, render_buffer);
    }

    ++sample_work_tile.start_sample;
  }
}

void PathTraceWorkCPU::copy_to_display(PathTraceDisplay *display,
                                       PassMode pass_mode,
                                       int num_samples)
{
  half4 *rgba_half = display->map_texture_buffer();
  if (!rgba_half) {
    /* TODO(sergey): Look into using copy_to_display() if mapping failed. Might be needed for
     * some implementations of PathTraceDisplay which can not map memory? */
    return;
  }

  const KernelFilm &kfilm = device_scene_->data.film;

  const PassAccessor::PassAccessInfo pass_access_info = get_display_pass_access_info(pass_mode);

  const PassAccessorCPU pass_accessor(pass_access_info, kfilm.exposure, num_samples);

  PassAccessor::Destination destination = get_display_destination_template(display);
  destination.pixels_half_rgba = rgba_half;

  tbb::task_arena local_arena = local_tbb_arena_create(device_);
  local_arena.execute([&]() {
    pass_accessor.get_render_tile_pixels(buffers_.get(), effective_buffer_params_, destination);
  });

  display->unmap_texture_buffer();
}

void PathTraceWorkCPU::destroy_gpu_resources(PathTraceDisplay * /*display*/)
{
}

bool PathTraceWorkCPU::copy_render_buffers_from_device()
{
  return buffers_->copy_from_device();
}

bool PathTraceWorkCPU::copy_render_buffers_to_device()
{
  buffers_->buffer.copy_to_device();
  return true;
}

bool PathTraceWorkCPU::zero_render_buffers()
{
  buffers_->zero();
  return true;
}

int PathTraceWorkCPU::adaptive_sampling_converge_filter_count_active(float threshold, bool reset)
{
  const int full_x = effective_buffer_params_.full_x;
  const int full_y = effective_buffer_params_.full_y;
  const int width = effective_buffer_params_.width;
  const int height = effective_buffer_params_.height;
  const int offset = effective_buffer_params_.offset;
  const int stride = effective_buffer_params_.stride;

  float *render_buffer = buffers_->buffer.data();

  uint num_active_pixels = 0;

  tbb::task_arena local_arena = local_tbb_arena_create(device_);

  /* Check convergency and do x-filter in a single `parallel_for`, to reduce threading overhead. */
  local_arena.execute([&]() {
    tbb::parallel_for(full_y, full_y + height, [&](int y) {
      CPUKernelThreadGlobals *kernel_globals = &kernel_thread_globals_[0];

      bool row_converged = true;
      uint num_row_pixels_active = 0;
      for (int x = 0; x < width; ++x) {
        if (!kernels_.adaptive_sampling_convergence_check(
                kernel_globals, render_buffer, full_x + x, y, threshold, reset, offset, stride)) {
          ++num_row_pixels_active;
          row_converged = false;
        }
      }

      atomic_fetch_and_add_uint32(&num_active_pixels, num_row_pixels_active);

      if (!row_converged) {
        kernels_.adaptive_sampling_filter_x(
            kernel_globals, render_buffer, y, full_x, width, offset, stride);
      }
    });
  });

  if (num_active_pixels) {
    local_arena.execute([&]() {
      tbb::parallel_for(full_x, full_x + width, [&](int x) {
        CPUKernelThreadGlobals *kernel_globals = &kernel_thread_globals_[0];
        kernels_.adaptive_sampling_filter_y(
            kernel_globals, render_buffer, x, full_y, height, offset, stride);
      });
    });
  }

  return num_active_pixels;
}

void PathTraceWorkCPU::cryptomatte_postproces()
{
  const int width = effective_buffer_params_.width;
  const int height = effective_buffer_params_.height;

  float *render_buffer = buffers_->buffer.data();

  tbb::task_arena local_arena = local_tbb_arena_create(device_);

  /* Check convergency and do x-filter in a single `parallel_for`, to reduce threading overhead. */
  local_arena.execute([&]() {
    tbb::parallel_for(0, height, [&](int y) {
      CPUKernelThreadGlobals *kernel_globals = &kernel_thread_globals_[0];
      int pixel_index = y * width;

      for (int x = 0; x < width; ++x, ++pixel_index) {
        kernels_.cryptomatte_postprocess(kernel_globals, render_buffer, pixel_index);
      }
    });
  });
}

CCL_NAMESPACE_END
