/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "integrator/path_trace.h"

#include "device/cpu/device.h"
#include "device/device.h"
#include "integrator/pass_accessor.h"
#include "integrator/path_trace_display.h"
#include "integrator/path_trace_tile.h"
#include "integrator/render_scheduler.h"
#include "scene/pass.h"
#include "scene/scene.h"
#include "session/tile.h"
#include "util/algorithm.h"
#include "util/log.h"
#include "util/progress.h"
#include "util/tbb.h"
#include "util/time.h"

CCL_NAMESPACE_BEGIN

PathTrace::PathTrace(Device *device,
                     Film *film,
                     DeviceScene *device_scene,
                     RenderScheduler &render_scheduler,
                     TileManager &tile_manager)
    : device_(device),
      film_(film),
      device_scene_(device_scene),
      render_scheduler_(render_scheduler),
      tile_manager_(tile_manager)
{
  DCHECK_NE(device_, nullptr);

  {
    vector<DeviceInfo> cpu_devices;
    device_cpu_info(cpu_devices);

    cpu_device_.reset(device_cpu_create(cpu_devices[0], device->stats, device->profiler));
  }

  /* Create path tracing work in advance, so that it can be reused by incremental sampling as much
   * as possible. */
  device_->foreach_device([&](Device *path_trace_device) {
    unique_ptr<PathTraceWork> work = PathTraceWork::create(
        path_trace_device, film, device_scene, &render_cancel_.is_requested);
    if (work) {
      path_trace_works_.emplace_back(std::move(work));
    }
  });

  work_balance_infos_.resize(path_trace_works_.size());
  work_balance_do_initial(work_balance_infos_);

  render_scheduler.set_need_schedule_rebalance(path_trace_works_.size() > 1);
}

PathTrace::~PathTrace()
{
  destroy_gpu_resources();
}

void PathTrace::load_kernels()
{
  if (denoiser_) {
    /* Activate graphics interop while denoiser device is created, so that it can choose a device
     * that supports interop for faster display updates. */
    if (display_ && path_trace_works_.size() > 1) {
      display_->graphics_interop_activate();
    }

    denoiser_->load_kernels(progress_);

    if (display_ && path_trace_works_.size() > 1) {
      display_->graphics_interop_deactivate();
    }
  }
}

void PathTrace::alloc_work_memory()
{
  for (auto &&path_trace_work : path_trace_works_) {
    path_trace_work->alloc_work_memory();
  }
}

bool PathTrace::ready_to_reset()
{
  /* The logic here is optimized for the best feedback in the viewport, which implies having a GPU
   * display. Of there is no such display, the logic here will break. */
  DCHECK(display_);

  /* The logic here tries to provide behavior which feels the most interactive feel to artists.
   * General idea is to be able to reset as quickly as possible, while still providing interactive
   * feel.
   *
   * If the render result was ever drawn after previous reset, consider that reset is now possible.
   * This way camera navigation gives the quickest feedback of rendered pixels, regardless of
   * whether CPU or GPU drawing pipeline is used.
   *
   * Consider reset happening after redraw "slow" enough to not clog anything. This is a bit
   * arbitrary, but seems to work very well with viewport navigation in Blender. */

  if (did_draw_after_reset_) {
    return true;
  }

  return false;
}

void PathTrace::reset(const BufferParams &full_params,
                      const BufferParams &big_tile_params,
                      const bool reset_rendering)
{
  if (big_tile_params_.modified(big_tile_params)) {
    big_tile_params_ = big_tile_params;
    render_state_.need_reset_params = true;
  }

  full_params_ = full_params;

  /* NOTE: GPU display checks for buffer modification and avoids unnecessary re-allocation.
   * It is requires to inform about reset whenever it happens, so that the redraw state tracking is
   * properly updated. */
  if (display_) {
    display_->reset(big_tile_params, reset_rendering);
  }

  render_state_.has_denoised_result = false;
  render_state_.tile_written = false;

  did_draw_after_reset_ = false;
}

void PathTrace::device_free()
{
  /* Free render buffers used by the path trace work to reduce memory peak. */
  BufferParams empty_params;
  empty_params.pass_stride = 0;
  empty_params.update_offset_stride();
  for (auto &&path_trace_work : path_trace_works_) {
    path_trace_work->get_render_buffers()->reset(empty_params);
  }
  render_state_.need_reset_params = true;
}

void PathTrace::set_progress(Progress *progress)
{
  progress_ = progress;
}

void PathTrace::render(const RenderWork &render_work)
{
  /* Indicate that rendering has started and that it can be requested to cancel. */
  {
    thread_scoped_lock lock(render_cancel_.mutex);
    if (render_cancel_.is_requested) {
      return;
    }
    render_cancel_.is_rendering = true;
  }

  render_pipeline(render_work);

  /* Indicate that rendering has finished, making it so thread which requested `cancel()` can carry
   * on. */
  {
    thread_scoped_lock lock(render_cancel_.mutex);
    render_cancel_.is_rendering = false;
    render_cancel_.condition.notify_one();
  }
}

void PathTrace::render_pipeline(RenderWork render_work)
{
  /* NOTE: Only check for "instant" cancel here. The user-requested cancel via progress is
   * checked in Session and the work in the event of cancel is to be finished here. */

  render_scheduler_.set_need_schedule_cryptomatte(device_scene_->data.film.cryptomatte_passes !=
                                                  0);

  render_init_kernel_execution();

  render_scheduler_.report_work_begin(render_work);

  init_render_buffers(render_work);

  rebalance(render_work);

  /* Prepare all per-thread guiding structures before we start with the next rendering
   * iteration/progression. */
  const bool use_guiding = device_scene_->data.integrator.use_guiding;
  if (use_guiding) {
    guiding_prepare_structures();
  }

  path_trace(render_work);
  if (render_cancel_.is_requested) {
    return;
  }

  /* Update the guiding field using the training data/samples collected during the rendering
   * iteration/progression. */
  const bool train_guiding = device_scene_->data.integrator.train_guiding;
  if (use_guiding && train_guiding) {
    guiding_update_structures();
  }

  adaptive_sample(render_work);
  if (render_cancel_.is_requested) {
    return;
  }

  cryptomatte_postprocess(render_work);
  if (render_cancel_.is_requested) {
    return;
  }

  denoise(render_work);
  if (render_cancel_.is_requested) {
    return;
  }

  write_tile_buffer(render_work);
  update_display(render_work);

  progress_update_if_needed(render_work);

  finalize_full_buffer_on_disk(render_work);
}

void PathTrace::render_init_kernel_execution()
{
  for (auto &&path_trace_work : path_trace_works_) {
    path_trace_work->init_execution();
  }
}

/* TODO(sergey): Look into `std::function` rather than using a template. Should not be a
 * measurable performance impact at runtime, but will make compilation faster and binary somewhat
 * smaller. */
template<typename Callback>
static void foreach_sliced_buffer_params(const vector<unique_ptr<PathTraceWork>> &path_trace_works,
                                         const vector<WorkBalanceInfo> &work_balance_infos,
                                         const BufferParams &buffer_params,
                                         const int overscan,
                                         const Callback &callback)
{
  const int num_works = path_trace_works.size();
  const int window_height = buffer_params.window_height;

  int current_y = 0;
  for (int i = 0; i < num_works; ++i) {
    const double weight = work_balance_infos[i].weight;
    const int slice_window_full_y = buffer_params.full_y + buffer_params.window_y + current_y;
    const int slice_window_height = max(lround(window_height * weight), 1);

    /* Disallow negative values to deal with situations when there are more compute devices than
     * scan-lines. */
    const int remaining_window_height = max(0, window_height - current_y);

    BufferParams slice_params = buffer_params;

    slice_params.full_y = max(slice_window_full_y - overscan, buffer_params.full_y);
    slice_params.window_y = slice_window_full_y - slice_params.full_y;

    if (i < num_works - 1) {
      slice_params.window_height = min(slice_window_height, remaining_window_height);
    }
    else {
      slice_params.window_height = remaining_window_height;
    }

    slice_params.height = slice_params.window_y + slice_params.window_height + overscan;
    slice_params.height = min(slice_params.height,
                              buffer_params.height + buffer_params.full_y - slice_params.full_y);

    slice_params.update_offset_stride();

    callback(path_trace_works[i].get(), slice_params);

    current_y += slice_params.window_height;
  }
}

void PathTrace::update_allocated_work_buffer_params()
{
  const int overscan = tile_manager_.get_tile_overscan();
  foreach_sliced_buffer_params(path_trace_works_,
                               work_balance_infos_,
                               big_tile_params_,
                               overscan,
                               [](PathTraceWork *path_trace_work, const BufferParams &params) {
                                 RenderBuffers *buffers = path_trace_work->get_render_buffers();
                                 buffers->reset(params);
                               });
}

static BufferParams scale_buffer_params(const BufferParams &params, int resolution_divider)
{
  BufferParams scaled_params = params;

  scaled_params.width = max(1, params.width / resolution_divider);
  scaled_params.height = max(1, params.height / resolution_divider);

  scaled_params.window_x = params.window_x / resolution_divider;
  scaled_params.window_y = params.window_y / resolution_divider;
  scaled_params.window_width = max(1, params.window_width / resolution_divider);
  scaled_params.window_height = max(1, params.window_height / resolution_divider);

  scaled_params.full_x = params.full_x / resolution_divider;
  scaled_params.full_y = params.full_y / resolution_divider;
  scaled_params.full_width = max(1, params.full_width / resolution_divider);
  scaled_params.full_height = max(1, params.full_height / resolution_divider);

  scaled_params.update_offset_stride();

  return scaled_params;
}

void PathTrace::update_effective_work_buffer_params(const RenderWork &render_work)
{
  const int resolution_divider = render_work.resolution_divider;

  const BufferParams scaled_full_params = scale_buffer_params(full_params_, resolution_divider);
  const BufferParams scaled_big_tile_params = scale_buffer_params(big_tile_params_,
                                                                  resolution_divider);

  const int overscan = tile_manager_.get_tile_overscan();

  foreach_sliced_buffer_params(path_trace_works_,
                               work_balance_infos_,
                               scaled_big_tile_params,
                               overscan,
                               [&](PathTraceWork *path_trace_work, const BufferParams params) {
                                 path_trace_work->set_effective_buffer_params(
                                     scaled_full_params, scaled_big_tile_params, params);
                               });

  render_state_.effective_big_tile_params = scaled_big_tile_params;
}

void PathTrace::update_work_buffer_params_if_needed(const RenderWork &render_work)
{
  if (render_state_.need_reset_params) {
    update_allocated_work_buffer_params();
  }

  if (render_state_.need_reset_params ||
      render_state_.resolution_divider != render_work.resolution_divider)
  {
    update_effective_work_buffer_params(render_work);
  }

  render_state_.resolution_divider = render_work.resolution_divider;
  render_state_.need_reset_params = false;
}

void PathTrace::init_render_buffers(const RenderWork &render_work)
{
  update_work_buffer_params_if_needed(render_work);

  /* Handle initialization scheduled by the render scheduler. */
  if (render_work.init_render_buffers) {
    parallel_for_each(path_trace_works_, [&](unique_ptr<PathTraceWork> &path_trace_work) {
      path_trace_work->zero_render_buffers();
    });

    tile_buffer_read();
  }
}

void PathTrace::path_trace(RenderWork &render_work)
{
  if (!render_work.path_trace.num_samples) {
    return;
  }

  VLOG_WORK << "Will path trace " << render_work.path_trace.num_samples
            << " samples at the resolution divider " << render_work.resolution_divider;

  const double start_time = time_dt();

  const int num_works = path_trace_works_.size();

  thread_capture_fp_settings();

  parallel_for(0, num_works, [&](int i) {
    const double work_start_time = time_dt();
    const int num_samples = render_work.path_trace.num_samples;

    PathTraceWork *path_trace_work = path_trace_works_[i].get();
    if (path_trace_work->get_device()->have_error()) {
      return;
    }

    PathTraceWork::RenderStatistics statistics;
    path_trace_work->render_samples(statistics,
                                    render_work.path_trace.start_sample,
                                    num_samples,
                                    render_work.path_trace.sample_offset);

    const double work_time = time_dt() - work_start_time;
    work_balance_infos_[i].time_spent += work_time;
    work_balance_infos_[i].occupancy = statistics.occupancy;

    VLOG_INFO << "Rendered " << num_samples << " samples in " << work_time << " seconds ("
              << work_time / num_samples
              << " seconds per sample), occupancy: " << statistics.occupancy;
  });

  float occupancy_accum = 0.0f;
  for (const WorkBalanceInfo &balance_info : work_balance_infos_) {
    occupancy_accum += balance_info.occupancy;
  }
  const float occupancy = occupancy_accum / num_works;
  render_scheduler_.report_path_trace_occupancy(render_work, occupancy);

  render_scheduler_.report_path_trace_time(
      render_work, time_dt() - start_time, is_cancel_requested());
}

void PathTrace::adaptive_sample(RenderWork &render_work)
{
  if (!render_work.adaptive_sampling.filter) {
    return;
  }

  bool did_reschedule_on_idle = false;

  while (true) {
    VLOG_WORK << "Will filter adaptive stopping buffer, threshold "
              << render_work.adaptive_sampling.threshold;
    if (render_work.adaptive_sampling.reset) {
      VLOG_WORK << "Will re-calculate convergency flag for currently converged pixels.";
    }

    const double start_time = time_dt();

    uint num_active_pixels = 0;
    parallel_for_each(path_trace_works_, [&](unique_ptr<PathTraceWork> &path_trace_work) {
      const uint num_active_pixels_in_work =
          path_trace_work->adaptive_sampling_converge_filter_count_active(
              render_work.adaptive_sampling.threshold, render_work.adaptive_sampling.reset);
      if (num_active_pixels_in_work) {
        atomic_add_and_fetch_u(&num_active_pixels, num_active_pixels_in_work);
      }
    });

    render_scheduler_.report_adaptive_filter_time(
        render_work, time_dt() - start_time, is_cancel_requested());

    if (num_active_pixels == 0) {
      VLOG_WORK << "All pixels converged.";
      if (!render_scheduler_.render_work_reschedule_on_converge(render_work)) {
        break;
      }
      VLOG_WORK << "Continuing with lower threshold.";
    }
    else if (did_reschedule_on_idle) {
      break;
    }
    else if (num_active_pixels < 128 * 128) {
      /* NOTE: The hardcoded value of 128^2 is more of an empirical value to keep GPU busy so that
       * there is no performance loss from the progressive noise floor feature.
       *
       * A better heuristic is possible here: for example, use maximum of 128^2 and percentage of
       * the final resolution. */
      if (!render_scheduler_.render_work_reschedule_on_idle(render_work)) {
        VLOG_WORK << "Rescheduling is not possible: final threshold is reached.";
        break;
      }
      VLOG_WORK << "Rescheduling lower threshold.";
      did_reschedule_on_idle = true;
    }
    else {
      break;
    }
  }
}

void PathTrace::set_denoiser_params(const DenoiseParams &params)
{
  render_scheduler_.set_denoiser_params(params);

  if (!params.use) {
    denoiser_.reset();
    return;
  }

  if (denoiser_) {
    const DenoiseParams old_denoiser_params = denoiser_->get_params();
    if (old_denoiser_params.type == params.type) {
      denoiser_->set_params(params);
      return;
    }
  }

  denoiser_ = Denoiser::create(device_, params);

  /* Only take into account the "immediate" cancel to have interactive rendering responding to
   * navigation as quickly as possible, but allow to run denoiser after user hit Escape key while
   * doing offline rendering. */
  denoiser_->is_cancelled_cb = [this]() { return render_cancel_.is_requested; };
}

void PathTrace::set_adaptive_sampling(const AdaptiveSampling &adaptive_sampling)
{
  render_scheduler_.set_adaptive_sampling(adaptive_sampling);
}

void PathTrace::cryptomatte_postprocess(const RenderWork &render_work)
{
  if (!render_work.cryptomatte.postprocess) {
    return;
  }
  VLOG_WORK << "Perform cryptomatte work.";

  parallel_for_each(path_trace_works_, [&](unique_ptr<PathTraceWork> &path_trace_work) {
    path_trace_work->cryptomatte_postproces();
  });
}

void PathTrace::denoise(const RenderWork &render_work)
{
  if (!render_work.tile.denoise) {
    return;
  }

  if (!denoiser_) {
    /* Denoiser was not configured, so nothing to do here. */
    return;
  }

  VLOG_WORK << "Perform denoising work.";

  const double start_time = time_dt();

  RenderBuffers *buffer_to_denoise = nullptr;
  bool allow_inplace_modification = false;

  Device *denoiser_device = denoiser_->get_denoiser_device();
  if (path_trace_works_.size() > 1 && denoiser_device && !big_tile_denoise_work_) {
    big_tile_denoise_work_ = PathTraceWork::create(denoiser_device, film_, device_scene_, nullptr);
  }

  if (big_tile_denoise_work_) {
    big_tile_denoise_work_->set_effective_buffer_params(render_state_.effective_big_tile_params,
                                                        render_state_.effective_big_tile_params,
                                                        render_state_.effective_big_tile_params);

    buffer_to_denoise = big_tile_denoise_work_->get_render_buffers();
    buffer_to_denoise->reset(render_state_.effective_big_tile_params);

    copy_to_render_buffers(buffer_to_denoise);

    allow_inplace_modification = true;
  }
  else {
    DCHECK_EQ(path_trace_works_.size(), 1);

    buffer_to_denoise = path_trace_works_.front()->get_render_buffers();
  }

  if (denoiser_->denoise_buffer(render_state_.effective_big_tile_params,
                                buffer_to_denoise,
                                get_num_samples_in_buffer(),
                                allow_inplace_modification))
  {
    render_state_.has_denoised_result = true;
  }

  render_scheduler_.report_denoise_time(render_work, time_dt() - start_time);
}

void PathTrace::set_output_driver(unique_ptr<OutputDriver> driver)
{
  output_driver_ = std::move(driver);
}

void PathTrace::set_display_driver(unique_ptr<DisplayDriver> driver)
{
  /* The display driver is the source of the drawing context which might be used by
   * path trace works. Make sure there is no graphics interop using resources from
   * the old display, as it might no longer be available after this call. */
  destroy_gpu_resources();

  if (driver) {
    display_ = make_unique<PathTraceDisplay>(std::move(driver));
  }
  else {
    display_ = nullptr;
  }
}

void PathTrace::clear_display()
{
  if (display_) {
    display_->clear();
  }
}

void PathTrace::draw()
{
  if (!display_) {
    return;
  }

  did_draw_after_reset_ |= display_->draw();
}

void PathTrace::flush_display()
{
  if (!display_) {
    return;
  }

  display_->flush();
}

void PathTrace::update_display(const RenderWork &render_work)
{
  if (!render_work.display.update) {
    return;
  }

  if (!display_ && !output_driver_) {
    VLOG_WORK << "Ignore display update.";
    return;
  }

  if (full_params_.width == 0 || full_params_.height == 0) {
    VLOG_WORK << "Skipping PathTraceDisplay update due to 0 size of the render buffer.";
    return;
  }

  const double start_time = time_dt();

  if (output_driver_) {
    VLOG_WORK << "Invoke buffer update callback.";

    PathTraceTile tile(*this);
    output_driver_->update_render_tile(tile);
  }

  if (display_) {
    VLOG_WORK << "Perform copy to GPUDisplay work.";

    const int texture_width = render_state_.effective_big_tile_params.window_width;
    const int texture_height = render_state_.effective_big_tile_params.window_height;
    if (!display_->update_begin(texture_width, texture_height)) {
      LOG(ERROR) << "Error beginning GPUDisplay update.";
      return;
    }

    const PassMode pass_mode = render_work.display.use_denoised_result &&
                                       render_state_.has_denoised_result ?
                                   PassMode::DENOISED :
                                   PassMode::NOISY;

    /* TODO(sergey): When using multi-device rendering map the GPUDisplay once and copy data from
     * all works in parallel. */
    const int num_samples = get_num_samples_in_buffer();
    if (big_tile_denoise_work_ && render_state_.has_denoised_result) {
      big_tile_denoise_work_->copy_to_display(display_.get(), pass_mode, num_samples);
    }
    else {
      for (auto &&path_trace_work : path_trace_works_) {
        path_trace_work->copy_to_display(display_.get(), pass_mode, num_samples);
      }
    }

    display_->update_end();
  }

  render_scheduler_.report_display_update_time(render_work, time_dt() - start_time);
}

void PathTrace::rebalance(const RenderWork &render_work)
{
  if (!render_work.rebalance) {
    return;
  }

  const int num_works = path_trace_works_.size();

  if (num_works == 1) {
    VLOG_WORK << "Ignoring rebalance work due to single device render.";
    return;
  }

  const double start_time = time_dt();

  if (VLOG_IS_ON(3)) {
    VLOG_WORK << "Perform rebalance work.";
    VLOG_WORK << "Per-device path tracing time (seconds):";
    for (int i = 0; i < num_works; ++i) {
      VLOG_WORK << path_trace_works_[i]->get_device()->info.description << ": "
                << work_balance_infos_[i].time_spent;
    }
  }

  const bool did_rebalance = work_balance_do_rebalance(work_balance_infos_);

  if (VLOG_IS_ON(3)) {
    VLOG_WORK << "Calculated per-device weights for works:";
    for (int i = 0; i < num_works; ++i) {
      VLOG_WORK << path_trace_works_[i]->get_device()->info.description << ": "
                << work_balance_infos_[i].weight;
    }
  }

  if (!did_rebalance) {
    VLOG_WORK << "Balance in path trace works did not change.";
    render_scheduler_.report_rebalance_time(render_work, time_dt() - start_time, false);
    return;
  }

  RenderBuffers big_tile_cpu_buffers(cpu_device_.get());
  big_tile_cpu_buffers.reset(render_state_.effective_big_tile_params);

  copy_to_render_buffers(&big_tile_cpu_buffers);

  render_state_.need_reset_params = true;
  update_work_buffer_params_if_needed(render_work);

  copy_from_render_buffers(&big_tile_cpu_buffers);

  render_scheduler_.report_rebalance_time(render_work, time_dt() - start_time, true);
}

void PathTrace::write_tile_buffer(const RenderWork &render_work)
{
  if (!render_work.tile.write) {
    return;
  }

  VLOG_WORK << "Write tile result.";

  render_state_.tile_written = true;

  const bool has_multiple_tiles = tile_manager_.has_multiple_tiles();

  /* Write render tile result, but only if not using tiled rendering.
   *
   * Tiles are written to a file during rendering, and written to the software at the end
   * of rendering (wither when all tiles are finished, or when rendering was requested to be
   * canceled).
   *
   * Important thing is: tile should be written to the software via callback only once. */
  if (!has_multiple_tiles) {
    VLOG_WORK << "Write tile result via buffer write callback.";
    tile_buffer_write();
  }
  /* Write tile to disk, so that the render work's render buffer can be re-used for the next tile.
   */
  else {
    VLOG_WORK << "Write tile result to disk.";
    tile_buffer_write_to_disk();
  }
}

void PathTrace::finalize_full_buffer_on_disk(const RenderWork &render_work)
{
  if (!render_work.full.write) {
    return;
  }

  VLOG_WORK << "Handle full-frame render buffer work.";

  if (!tile_manager_.has_written_tiles()) {
    VLOG_WORK << "No tiles on disk.";
    return;
  }

  /* Make sure writing to the file is fully finished.
   * This will include writing all possible missing tiles, ensuring validness of the file. */
  tile_manager_.finish_write_tiles();

  /* NOTE: The rest of full-frame post-processing (such as full-frame denoising) will be done after
   * all scenes and layers are rendered by the Session (which happens after freeing Session memory,
   * so that we never hold scene and full-frame buffer in memory at the same time). */
}

void PathTrace::cancel()
{
  thread_scoped_lock lock(render_cancel_.mutex);

  render_cancel_.is_requested = true;

  while (render_cancel_.is_rendering) {
    render_cancel_.condition.wait(lock);
  }

  render_cancel_.is_requested = false;
}

int PathTrace::get_num_samples_in_buffer()
{
  return render_scheduler_.get_num_rendered_samples();
}

bool PathTrace::is_cancel_requested()
{
  if (render_cancel_.is_requested) {
    return true;
  }

  if (progress_ != nullptr) {
    if (progress_->get_cancel()) {
      return true;
    }
  }

  return false;
}

void PathTrace::tile_buffer_write()
{
  if (!output_driver_) {
    return;
  }

  PathTraceTile tile(*this);
  output_driver_->write_render_tile(tile);
}

void PathTrace::tile_buffer_read()
{
  if (!device_scene_->data.bake.use) {
    return;
  }

  if (!output_driver_) {
    return;
  }

  /* Read buffers back from device. */
  parallel_for_each(path_trace_works_, [&](unique_ptr<PathTraceWork> &path_trace_work) {
    path_trace_work->copy_render_buffers_from_device();
  });

  /* Read (subset of) passes from output driver. */
  PathTraceTile tile(*this);
  if (output_driver_->read_render_tile(tile)) {
    /* Copy buffers to device again. */
    parallel_for_each(path_trace_works_, [](unique_ptr<PathTraceWork> &path_trace_work) {
      path_trace_work->copy_render_buffers_to_device();
    });
  }
}

void PathTrace::tile_buffer_write_to_disk()
{
  /* Sample count pass is required to support per-tile partial results stored in the file. */
  DCHECK_NE(big_tile_params_.get_pass_offset(PASS_SAMPLE_COUNT), PASS_UNUSED);

  const int num_rendered_samples = render_scheduler_.get_num_rendered_samples();

  if (num_rendered_samples == 0) {
    /* The tile has zero samples, no need to write it. */
    return;
  }

  /* Get access to the CPU-side render buffers of the current big tile. */
  RenderBuffers *buffers;
  RenderBuffers big_tile_cpu_buffers(cpu_device_.get());

  if (path_trace_works_.size() == 1) {
    path_trace_works_[0]->copy_render_buffers_from_device();
    buffers = path_trace_works_[0]->get_render_buffers();
  }
  else {
    big_tile_cpu_buffers.reset(render_state_.effective_big_tile_params);
    copy_to_render_buffers(&big_tile_cpu_buffers);

    buffers = &big_tile_cpu_buffers;
  }

  if (!tile_manager_.write_tile(*buffers)) {
    device_->set_error("Error writing tile to file");
  }
}

void PathTrace::progress_update_if_needed(const RenderWork &render_work)
{
  if (progress_ != nullptr) {
    const int2 tile_size = get_render_tile_size();
    const uint64_t num_samples_added = uint64_t(tile_size.x) * tile_size.y *
                                       render_work.path_trace.num_samples;
    const int current_sample = render_work.path_trace.start_sample +
                               render_work.path_trace.num_samples -
                               render_work.path_trace.sample_offset;
    progress_->add_samples(num_samples_added, current_sample);
  }

  if (progress_update_cb) {
    progress_update_cb();
  }
}

void PathTrace::progress_set_status(const string &status, const string &substatus)
{
  if (progress_ != nullptr) {
    progress_->set_status(status, substatus);
  }
}

void PathTrace::copy_to_render_buffers(RenderBuffers *render_buffers)
{
  parallel_for_each(path_trace_works_,
                    [&render_buffers](unique_ptr<PathTraceWork> &path_trace_work) {
                      path_trace_work->copy_to_render_buffers(render_buffers);
                    });
  render_buffers->copy_to_device();
}

void PathTrace::copy_from_render_buffers(RenderBuffers *render_buffers)
{
  render_buffers->copy_from_device();
  parallel_for_each(path_trace_works_,
                    [&render_buffers](unique_ptr<PathTraceWork> &path_trace_work) {
                      path_trace_work->copy_from_render_buffers(render_buffers);
                    });
}

bool PathTrace::copy_render_tile_from_device()
{
  if (full_frame_state_.render_buffers) {
    /* Full-frame buffer is always allocated on CPU. */
    return true;
  }

  if (big_tile_denoise_work_ && render_state_.has_denoised_result) {
    return big_tile_denoise_work_->copy_render_buffers_from_device();
  }

  bool success = true;

  parallel_for_each(path_trace_works_, [&](unique_ptr<PathTraceWork> &path_trace_work) {
    if (!success) {
      return;
    }
    if (!path_trace_work->copy_render_buffers_from_device()) {
      success = false;
    }
  });

  return success;
}

static string get_layer_view_name(const RenderBuffers &buffers)
{
  string result;

  if (buffers.params.layer.size()) {
    result += string(buffers.params.layer);
  }

  if (buffers.params.view.size()) {
    if (!result.empty()) {
      result += ", ";
    }
    result += string(buffers.params.view);
  }

  return result;
}

void PathTrace::process_full_buffer_from_disk(string_view filename)
{
  VLOG_WORK << "Processing full frame buffer file " << filename;

  progress_set_status("Reading full buffer from disk");

  RenderBuffers full_frame_buffers(cpu_device_.get());

  DenoiseParams denoise_params;
  if (!tile_manager_.read_full_buffer_from_disk(filename, &full_frame_buffers, &denoise_params)) {
    const string error_message = "Error reading tiles from file";
    if (progress_) {
      progress_->set_error(error_message);
      progress_->set_cancel(error_message);
    }
    else {
      LOG(ERROR) << error_message;
    }
    return;
  }

  const string layer_view_name = get_layer_view_name(full_frame_buffers);

  render_state_.has_denoised_result = false;

  if (denoise_params.use) {
    progress_set_status(layer_view_name, "Denoising");

    /* Re-use the denoiser as much as possible, avoiding possible device re-initialization.
     *
     * It will not conflict with the regular rendering as:
     *  - Rendering is supposed to be finished here.
     *  - The next rendering will go via Session's `run_update_for_next_iteration` which will
     *    ensure proper denoiser is used. */
    set_denoiser_params(denoise_params);

    /* Number of samples doesn't matter too much, since the samples count pass will be used. */
    denoiser_->denoise_buffer(full_frame_buffers.params, &full_frame_buffers, 0, false);

    render_state_.has_denoised_result = true;
  }

  full_frame_state_.render_buffers = &full_frame_buffers;

  progress_set_status(layer_view_name, "Finishing");

  /* Write the full result pretending that there is a single tile.
   * Requires some state change, but allows to use same communication API with the software. */
  tile_buffer_write();

  full_frame_state_.render_buffers = nullptr;
}

int PathTrace::get_num_render_tile_samples() const
{
  if (full_frame_state_.render_buffers) {
    return full_frame_state_.render_buffers->params.samples;
  }

  return render_scheduler_.get_num_rendered_samples();
}

bool PathTrace::get_render_tile_pixels(const PassAccessor &pass_accessor,
                                       const PassAccessor::Destination &destination)
{
  if (full_frame_state_.render_buffers) {
    return pass_accessor.get_render_tile_pixels(full_frame_state_.render_buffers, destination);
  }

  if (big_tile_denoise_work_ && render_state_.has_denoised_result) {
    /* Only use the big tile denoised buffer to access the denoised passes.
     * The guiding passes are allowed to be modified in-place for the needs of the denoiser,
     * so copy those from the original devices buffers. */
    if (pass_accessor.get_pass_access_info().mode == PassMode::DENOISED) {
      return big_tile_denoise_work_->get_render_tile_pixels(pass_accessor, destination);
    }
  }

  bool success = true;

  parallel_for_each(path_trace_works_, [&](unique_ptr<PathTraceWork> &path_trace_work) {
    if (!success) {
      return;
    }
    if (!path_trace_work->get_render_tile_pixels(pass_accessor, destination)) {
      success = false;
    }
  });

  return success;
}

bool PathTrace::set_render_tile_pixels(PassAccessor &pass_accessor,
                                       const PassAccessor::Source &source)
{
  bool success = true;

  parallel_for_each(path_trace_works_, [&](unique_ptr<PathTraceWork> &path_trace_work) {
    if (!success) {
      return;
    }
    if (!path_trace_work->set_render_tile_pixels(pass_accessor, source)) {
      success = false;
    }
  });

  return success;
}

int2 PathTrace::get_render_tile_size() const
{
  if (full_frame_state_.render_buffers) {
    return make_int2(full_frame_state_.render_buffers->params.window_width,
                     full_frame_state_.render_buffers->params.window_height);
  }

  const Tile &tile = tile_manager_.get_current_tile();
  return make_int2(tile.window_width, tile.window_height);
}

int2 PathTrace::get_render_tile_offset() const
{
  if (full_frame_state_.render_buffers) {
    return make_int2(0, 0);
  }

  const Tile &tile = tile_manager_.get_current_tile();
  return make_int2(tile.x + tile.window_x, tile.y + tile.window_y);
}

int2 PathTrace::get_render_size() const
{
  return tile_manager_.get_size();
}

const BufferParams &PathTrace::get_render_tile_params() const
{
  if (full_frame_state_.render_buffers) {
    return full_frame_state_.render_buffers->params;
  }

  return big_tile_params_;
}

bool PathTrace::has_denoised_result() const
{
  return render_state_.has_denoised_result;
}

void PathTrace::destroy_gpu_resources()
{
  /* Destroy any GPU resource which was used for graphics interop.
   * Need to have access to the PathTraceDisplay as it is the only source of drawing context which
   * is used for interop. */
  if (display_) {
    for (auto &&path_trace_work : path_trace_works_) {
      path_trace_work->destroy_gpu_resources(display_.get());
    }

    if (big_tile_denoise_work_) {
      big_tile_denoise_work_->destroy_gpu_resources(display_.get());
    }
  }
}

/* --------------------------------------------------------------------
 * Report generation.
 */

static const char *device_type_for_description(const DeviceType type)
{
  switch (type) {
    case DEVICE_NONE:
      return "None";

    case DEVICE_CPU:
      return "CPU";
    case DEVICE_CUDA:
      return "CUDA";
    case DEVICE_OPTIX:
      return "OptiX";
    case DEVICE_HIP:
      return "HIP";
    case DEVICE_HIPRT:
      return "HIPRT";
    case DEVICE_ONEAPI:
      return "oneAPI";
    case DEVICE_DUMMY:
      return "Dummy";
    case DEVICE_MULTI:
      return "Multi";
    case DEVICE_METAL:
      return "Metal";
  }

  return "UNKNOWN";
}

/* Construct description of the device which will appear in the full report. */
/* TODO(sergey): Consider making it more reusable utility. */
static string full_device_info_description(const DeviceInfo &device_info)
{
  string full_description = device_info.description;

  full_description += " (" + string(device_type_for_description(device_info.type)) + ")";

  if (device_info.display_device) {
    full_description += " (display)";
  }

  if (device_info.type == DEVICE_CPU) {
    full_description += " (" + to_string(device_info.cpu_threads) + " threads)";
  }

  full_description += " [" + device_info.id + "]";

  return full_description;
}

/* Construct string which will contain information about devices, possibly multiple of the devices.
 *
 * In the simple case the result looks like:
 *
 *   Message: Full Device Description
 *
 * If there are multiple devices then the result looks like:
 *
 *   Message: Full First Device Description
 *            Full Second Device Description
 *
 * Note that the newlines are placed in a way so that the result can be easily concatenated to the
 * full report. */
static string device_info_list_report(const string &message, const DeviceInfo &device_info)
{
  string result = "\n" + message + ": ";
  const string pad(message.length() + 2, ' ');

  if (device_info.multi_devices.empty()) {
    result += full_device_info_description(device_info) + "\n";
    return result;
  }

  bool is_first = true;
  for (const DeviceInfo &sub_device_info : device_info.multi_devices) {
    if (!is_first) {
      result += pad;
    }

    result += full_device_info_description(sub_device_info) + "\n";

    is_first = false;
  }

  return result;
}

static string path_trace_devices_report(const vector<unique_ptr<PathTraceWork>> &path_trace_works)
{
  DeviceInfo device_info;
  device_info.type = DEVICE_MULTI;

  for (auto &&path_trace_work : path_trace_works) {
    device_info.multi_devices.push_back(path_trace_work->get_device()->info);
  }

  return device_info_list_report("Path tracing on", device_info);
}

static string denoiser_device_report(const Denoiser *denoiser)
{
  if (!denoiser) {
    return "";
  }

  if (!denoiser->get_params().use) {
    return "";
  }

  const Device *denoiser_device = denoiser->get_denoiser_device();
  if (!denoiser_device) {
    return "";
  }

  return device_info_list_report("Denoising on", denoiser_device->info);
}

string PathTrace::full_report() const
{
  string result = "\nFull path tracing report\n";

  result += path_trace_devices_report(path_trace_works_);
  result += denoiser_device_report(denoiser_.get());

  /* Report from the render scheduler, which includes:
   * - Render mode (interactive, offline, headless)
   * - Adaptive sampling and denoiser parameters
   * - Breakdown of timing. */
  result += render_scheduler_.full_report();

  return result;
}

void PathTrace::set_guiding_params(const GuidingParams &guiding_params, const bool reset)
{
#ifdef WITH_PATH_GUIDING
  if (guiding_params_.modified(guiding_params)) {
    guiding_params_ = guiding_params;

    if (guiding_params_.use) {
      PGLFieldArguments field_args;
      switch (guiding_params_.type) {
        default:
        /* Parallax-aware von Mises-Fisher mixture models. */
        case GUIDING_TYPE_PARALLAX_AWARE_VMM: {
          pglFieldArgumentsSetDefaults(
              field_args,
              PGL_SPATIAL_STRUCTURE_TYPE::PGL_SPATIAL_STRUCTURE_KDTREE,
              PGL_DIRECTIONAL_DISTRIBUTION_TYPE::PGL_DIRECTIONAL_DISTRIBUTION_PARALLAX_AWARE_VMM);
          break;
        }
        /* Directional quad-trees. */
        case GUIDING_TYPE_DIRECTIONAL_QUAD_TREE: {
          pglFieldArgumentsSetDefaults(
              field_args,
              PGL_SPATIAL_STRUCTURE_TYPE::PGL_SPATIAL_STRUCTURE_KDTREE,
              PGL_DIRECTIONAL_DISTRIBUTION_TYPE::PGL_DIRECTIONAL_DISTRIBUTION_QUADTREE);
          break;
        }
        /* von Mises-Fisher mixture models. */
        case GUIDING_TYPE_VMM: {
          pglFieldArgumentsSetDefaults(
              field_args,
              PGL_SPATIAL_STRUCTURE_TYPE::PGL_SPATIAL_STRUCTURE_KDTREE,
              PGL_DIRECTIONAL_DISTRIBUTION_TYPE::PGL_DIRECTIONAL_DISTRIBUTION_VMM);
          break;
        }
      }
      field_args.deterministic = guiding_params.deterministic;
      reinterpret_cast<PGLKDTreeArguments *>(field_args.spatialSturctureArguments)->maxDepth = 16;
      openpgl::cpp::Device *guiding_device = static_cast<openpgl::cpp::Device *>(
          device_->get_guiding_device());
      if (guiding_device) {
        guiding_sample_data_storage_ = make_unique<openpgl::cpp::SampleStorage>();
        guiding_field_ = make_unique<openpgl::cpp::Field>(guiding_device, field_args);
      }
      else {
        guiding_sample_data_storage_ = nullptr;
        guiding_field_ = nullptr;
      }
    }
    else {
      guiding_sample_data_storage_ = nullptr;
      guiding_field_ = nullptr;
    }
  }
  else if (reset) {
    if (guiding_field_) {
      guiding_field_->Reset();
    }
  }
#else
  (void)guiding_params;
  (void)reset;
#endif
}

void PathTrace::guiding_prepare_structures()
{
#ifdef WITH_PATH_GUIDING
  const bool train = (guiding_params_.training_samples == 0) ||
                     (guiding_field_->GetIteration() < guiding_params_.training_samples);

  for (auto &&path_trace_work : path_trace_works_) {
    path_trace_work->guiding_init_kernel_globals(
        guiding_field_.get(), guiding_sample_data_storage_.get(), train);
  }

  if (train) {
    /* For training the guiding distribution we need to force the number of samples
     * per update to be limited, for reproducible results and reasonable training size.
     *
     * Idea: we could stochastically discard samples with a probability of 1/num_samples_per_update
     * we can then update only after the num_samples_per_update iterations are rendered. */
    render_scheduler_.set_limit_samples_per_update(4);
  }
  else {
    render_scheduler_.set_limit_samples_per_update(0);
  }
#endif
}

void PathTrace::guiding_update_structures()
{
#ifdef WITH_PATH_GUIDING
  VLOG_WORK << "Update path guiding structures";

  VLOG_DEBUG << "Number of surface samples: " << guiding_sample_data_storage_->GetSizeSurface();
  VLOG_DEBUG << "Number of volume samples: " << guiding_sample_data_storage_->GetSizeVolume();

  const size_t num_valid_samples = guiding_sample_data_storage_->GetSizeSurface() +
                                   guiding_sample_data_storage_->GetSizeVolume();

  /* we wait until we have at least 1024 samples */
  if (num_valid_samples >= 1024) {
    guiding_field_->Update(*guiding_sample_data_storage_);
    guiding_update_count++;

    VLOG_DEBUG << "Path guiding field valid: " << guiding_field_->Validate();

    guiding_sample_data_storage_->Clear();
  }
#endif
}

CCL_NAMESPACE_END
