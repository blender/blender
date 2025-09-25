/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "integrator/path_trace_work_cpu.h"

#include "device/cpu/kernel.h"
#include "device/device.h"

#ifdef WITH_CYCLES_DEBUG
#  include "kernel/film/write.h"
#endif

#include "kernel/integrator/path_state.h"

#include "integrator/pass_accessor_cpu.h"
#include "integrator/path_trace_display.h"

#include "scene/scene.h"
#include "session/buffers.h"

#include "util/tbb.h"
#include "util/time.h"

CCL_NAMESPACE_BEGIN

/* Create TBB arena for execution of path tracing and rendering tasks. */
static inline tbb::task_arena local_tbb_arena_create(const Device *device)
{
  /* TODO: limit this to number of threads of CPU device, it may be smaller than
   * the system number of threads when we reduce the number of CPU threads in
   * CPU + GPU rendering to dedicate some cores to handling the GPU device. */
  return tbb::task_arena(device->info.cpu_threads);
}

/* Get ThreadKernelGlobalsCPU for the current thread. */
static inline ThreadKernelGlobalsCPU *kernel_thread_globals_get(
    vector<ThreadKernelGlobalsCPU> &kernel_thread_globals)
{
  const int thread_index = tbb::this_task_arena::current_thread_index();
  DCHECK_GE(thread_index, 0);
  DCHECK_LE(thread_index, kernel_thread_globals.size());

  return &kernel_thread_globals[thread_index];
}

PathTraceWorkCPU::PathTraceWorkCPU(Device *device,
                                   Film *film,
                                   DeviceScene *device_scene,
                                   const bool *cancel_requested_flag)
    : PathTraceWork(device, film, device_scene, cancel_requested_flag),
      kernels_(Device::get_cpu_kernels())
{
  DCHECK_EQ(device->info.type, DEVICE_CPU);
}

void PathTraceWorkCPU::init_execution()
{
  /* Cache per-thread kernel globals. */
  device_->get_cpu_kernel_thread_globals(kernel_thread_globals_);
}

void PathTraceWorkCPU::render_samples(RenderStatistics &statistics,
                                      const int start_sample,
                                      const int samples_num,
                                      const int sample_offset)
{
  const int64_t image_width = effective_buffer_params_.width;
  const int64_t image_height = effective_buffer_params_.height;
  const int64_t total_pixels_num = image_width * image_height;

  if (device_->profiler.active()) {
    for (ThreadKernelGlobalsCPU &kernel_globals : kernel_thread_globals_) {
      kernel_globals.start_profiling();
    }
  }

  tbb::task_arena local_arena = local_tbb_arena_create(device_);
  local_arena.execute([&]() {
    parallel_for(int64_t(0), total_pixels_num, [&](int64_t work_index) {
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
      work_tile.sample_offset = sample_offset;
      work_tile.num_samples = 1;
      work_tile.offset = effective_buffer_params_.offset;
      work_tile.stride = effective_buffer_params_.stride;

      ThreadKernelGlobalsCPU *kernel_globals = kernel_thread_globals_get(kernel_thread_globals_);

      render_samples_full_pipeline(kernel_globals, work_tile, samples_num);
    });
  });
  if (device_->profiler.active()) {
    for (ThreadKernelGlobalsCPU &kernel_globals : kernel_thread_globals_) {
      kernel_globals.stop_profiling();
    }
  }

  statistics.occupancy = 1.0f;
}

void PathTraceWorkCPU::render_samples_full_pipeline(ThreadKernelGlobalsCPU *kernel_globals,
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

  fast_timer render_timer;

  for (int sample = 0; sample < samples_num; ++sample) {
    if (is_cancel_requested()) {
      break;
    }

    if (has_bake) {
      if (!kernels_.integrator_init_from_bake(
              kernel_globals, state, &sample_work_tile, render_buffer))
      {
        break;
      }
    }
    else {
      if (!kernels_.integrator_init_from_camera(
              kernel_globals, state, &sample_work_tile, render_buffer))
      {
        break;
      }
    }

#if defined(WITH_PATH_GUIDING)
    if (kernel_globals->data.integrator.train_guiding) {
      assert(kernel_globals->opgl_path_segment_storage);
      assert(kernel_globals->opgl_path_segment_storage->GetNumSegments() == 0);

      kernels_.integrator_megakernel(kernel_globals, state, render_buffer);

      /* Push the generated sample data to the global sample data storage. */
      guiding_push_sample_data_to_global_storage(kernel_globals, state, render_buffer);

      /* No training for shadow catcher paths. */
      if (shadow_catcher_state) {
        kernel_globals->data.integrator.train_guiding = false;
        kernels_.integrator_megakernel(kernel_globals, shadow_catcher_state, render_buffer);
        kernel_globals->data.integrator.train_guiding = true;
      }
    }
    else
#endif
    {
      kernels_.integrator_megakernel(kernel_globals, state, render_buffer);
      if (shadow_catcher_state) {
        kernels_.integrator_megakernel(kernel_globals, shadow_catcher_state, render_buffer);
      }
    }

    if (kernel_globals->data.film.pass_render_time != PASS_UNUSED) {
      uint64_t time;
      if (render_timer.lap(time)) {
        ccl_global float *buffer = render_buffer + (uint64_t)state->path.render_pixel_index *
                                                       kernel_globals->data.film.pass_stride;
        *(buffer + kernel_globals->data.film.pass_render_time) += float(time);
      }
    }
    ++sample_work_tile.start_sample;
  }
}

void PathTraceWorkCPU::copy_to_display(PathTraceDisplay *display,
                                       PassMode pass_mode,
                                       const int num_samples)
{
  half4 *rgba_half = display->map_texture_buffer();
  if (!rgba_half) {
    /* TODO(sergey): Look into using copy_to_display() if mapping failed. Might be needed for
     * some implementations of PathTraceDisplay which can not map memory? */
    return;
  }

  const KernelFilm &kfilm = device_scene_->data.film;

  const PassAccessor::PassAccessInfo pass_access_info = get_display_pass_access_info(pass_mode);
  if (pass_access_info.type == PASS_NONE) {
    return;
  }

  const PassAccessorCPU pass_accessor(pass_access_info, kfilm.exposure, num_samples);

  PassAccessor::Destination destination = get_display_destination_template(display, pass_mode);
  destination.pixels_half_rgba = rgba_half;

  tbb::task_arena local_arena = local_tbb_arena_create(device_);
  local_arena.execute([&]() {
    pass_accessor.get_render_tile_pixels(buffers_.get(), effective_buffer_params_, destination);
  });

  display->unmap_texture_buffer();
}

void PathTraceWorkCPU::destroy_gpu_resources(PathTraceDisplay * /*display*/) {}

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

int PathTraceWorkCPU::adaptive_sampling_converge_filter_count_active(const float threshold,
                                                                     bool reset)
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
    parallel_for(full_y, full_y + height, [&](int y) {
      ThreadKernelGlobalsCPU *kernel_globals = kernel_thread_globals_.data();

      bool row_converged = true;
      uint num_row_pixels_active = 0;
      for (int x = 0; x < width; ++x) {
        if (!kernels_.adaptive_sampling_convergence_check(
                kernel_globals, render_buffer, full_x + x, y, threshold, reset, offset, stride))
        {
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
      parallel_for(full_x, full_x + width, [&](int x) {
        ThreadKernelGlobalsCPU *kernel_globals = kernel_thread_globals_.data();
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
    parallel_for(0, height, [&](int y) {
      ThreadKernelGlobalsCPU *kernel_globals = kernel_thread_globals_.data();
      int pixel_index = y * width;

      for (int x = 0; x < width; ++x, ++pixel_index) {
        kernels_.cryptomatte_postprocess(kernel_globals, render_buffer, pixel_index);
      }
    });
  });
}

void PathTraceWorkCPU::denoise_volume_guiding_buffers()
{
  const int min_x = effective_buffer_params_.full_x;
  const int min_y = effective_buffer_params_.full_y;
  const int max_x = effective_buffer_params_.width + min_x;
  const int max_y = effective_buffer_params_.height + min_y;
  const int offset = effective_buffer_params_.offset;
  const int stride = effective_buffer_params_.stride;

  float *render_buffer = buffers_->buffer.data();

  tbb::task_arena local_arena = local_tbb_arena_create(device_);

  const blocked_range2d<int> range(min_x, max_x, min_y, max_y);

  /* Filter in x direction. */
  local_arena.execute([&]() {
    parallel_for(range, [&](const blocked_range2d<int> r) {
      ThreadKernelGlobalsCPU *kernel_globals = kernel_thread_globals_.data();
      for (int y = r.cols().begin(); y < r.cols().end(); ++y) {
        for (int x = r.rows().begin(); x < r.rows().end(); ++x) {
          kernels_.volume_guiding_filter_x(
              kernel_globals, render_buffer, y, x, min_x, max_x, offset, stride);
        }
      }
    });
  });

  /* Filter in y direction. Unlike `filter_x`, the inner loop of `filter_y` is serially run inside
   * the kernel, to avoid the need of intermediate buffers. */
  local_arena.execute([&]() {
    parallel_for(min_x, max_x, [&](int x) {
      ThreadKernelGlobalsCPU *kernel_globals = kernel_thread_globals_.data();
      kernels_.volume_guiding_filter_y(
          kernel_globals, render_buffer, x, min_y, max_y, offset, stride);
    });
  });
}

#if defined(WITH_PATH_GUIDING)
/* NOTE: It seems that this is called before every rendering iteration/progression and not once per
 * rendering. May be we find a way to call it only once per rendering. */
void PathTraceWorkCPU::guiding_init_kernel_globals(void *guiding_field,
                                                   void *sample_data_storage,
                                                   const bool train)
{
  /* Linking the global guiding structures (e.g., Field and SampleStorage) to the per-thread
   * kernel globals. */
  for (int thread_index = 0; thread_index < kernel_thread_globals_.size(); thread_index++) {
    ThreadKernelGlobalsCPU &kg = kernel_thread_globals_[thread_index];
    openpgl::cpp::Field *field = (openpgl::cpp::Field *)guiding_field;

    /* Allocate sampling distributions. */
    kg.opgl_guiding_field = field;

#  if PATH_GUIDING_LEVEL >= 4
    if (kg.opgl_surface_sampling_distribution) {
      kg.opgl_surface_sampling_distribution.reset();
    }
    if (kg.opgl_volume_sampling_distribution) {
      kg.opgl_volume_sampling_distribution.reset();
    }

    if (field) {
      kg.opgl_surface_sampling_distribution =
          make_unique<openpgl::cpp::SurfaceSamplingDistribution>(field);
      kg.opgl_volume_sampling_distribution = make_unique<openpgl::cpp::VolumeSamplingDistribution>(
          field);
    }
#  endif

    /* Reserve storage for training. */
    kg.data.integrator.train_guiding = train;
    kg.opgl_sample_data_storage = (openpgl::cpp::SampleStorage *)sample_data_storage;

    if (train) {
      kg.opgl_path_segment_storage->Reserve(kg.data.integrator.transparent_max_bounce +
                                            kg.data.integrator.max_bounce + 3);
      kg.opgl_path_segment_storage->Clear();
    }
  }
}

void PathTraceWorkCPU::guiding_push_sample_data_to_global_storage(ThreadKernelGlobalsCPU *kg,
                                                                  IntegratorStateCPU *state,
                                                                  ccl_global float *ccl_restrict
                                                                      render_buffer)
{
#  ifdef WITH_CYCLES_DEBUG
  if (LOG_IS_ON(LOG_LEVEL_DEBUG)) {
    /* Check if the generated path segments contain valid values. */
    const bool validSegments = kg->opgl_path_segment_storage->ValidateSegments();
    if (!validSegments) {
      LOG_DEBUG << "Guiding: invalid path segments!";
    }
  }

  /* Write debug render pass to validate it matches combined pass. */
  pgl_vec3f pgl_final_color = kg->opgl_path_segment_storage->CalculatePixelEstimate(false);
  ccl_global float *buffer = film_pass_pixel_render_buffer(kg, state, render_buffer);
  float3 final_color = make_float3(pgl_final_color.x, pgl_final_color.y, pgl_final_color.z);
  if (kernel_data.film.pass_guiding_color != PASS_UNUSED) {
    film_write_pass_float3(buffer + kernel_data.film.pass_guiding_color, final_color);
  }
#  else
  (void)state;
  (void)render_buffer;
#  endif

  /* Convert the path segment representation of the random walk into radiance samples. */
#  if PATH_GUIDING_LEVEL >= 2
  const bool use_direct_light = kernel_data.integrator.use_guiding_direct_light;
  const bool use_mis_weights = kernel_data.integrator.use_guiding_mis_weights;
  kg->opgl_path_segment_storage->PrepareSamples(use_mis_weights, use_direct_light, false);
#  endif

#  ifdef WITH_CYCLES_DEBUG
  /* Check if the training/radiance samples generated by the path segment storage are valid. */
  if (LOG_IS_ON(LOG_LEVEL_DEBUG)) {
    const bool validSamples = kg->opgl_path_segment_storage->ValidateSamples();
    if (!validSamples) {
      LOG_DEBUG
          << "Guiding: path segment storage generated/contains invalid radiance/training samples!";
    }
  }
#  endif

#  if PATH_GUIDING_LEVEL >= 3
  /* Push radiance samples from current random walk/path to the global sample storage. */
  size_t num_samples = 0;
  const openpgl::cpp::SampleData *samples = kg->opgl_path_segment_storage->GetSamples(num_samples);
  kg->opgl_sample_data_storage->AddSamples(samples, num_samples);
#  endif

  /* Clear storage for the current path, to be ready for the next path. */
  kg->opgl_path_segment_storage->Clear();
}
#endif

CCL_NAMESPACE_END
