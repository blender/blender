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

#include <limits.h>
#include <string.h>

#include "device/cpu/device.h"
#include "device/device.h"
#include "integrator/pass_accessor_cpu.h"
#include "integrator/path_trace.h"
#include "render/background.h"
#include "render/bake.h"
#include "render/buffers.h"
#include "render/camera.h"
#include "render/gpu_display.h"
#include "render/graph.h"
#include "render/integrator.h"
#include "render/light.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/scene.h"
#include "render/session.h"

#include "util/util_foreach.h"
#include "util/util_function.h"
#include "util/util_logging.h"
#include "util/util_math.h"
#include "util/util_opengl.h"
#include "util/util_task.h"
#include "util/util_time.h"

CCL_NAMESPACE_BEGIN

Session::Session(const SessionParams &params_, const SceneParams &scene_params)
    : params(params_), render_scheduler_(tile_manager_, params)
{
  TaskScheduler::init(params.threads);

  session_thread_ = nullptr;

  delayed_reset_.do_reset = false;

  pause_ = false;
  cancel_ = false;
  new_work_added_ = false;

  device = Device::create(params.device, stats, profiler);

  scene = new Scene(scene_params, device);

  /* Configure path tracer. */
  path_trace_ = make_unique<PathTrace>(
      device, scene->film, &scene->dscene, render_scheduler_, tile_manager_);
  path_trace_->set_progress(&progress);
  path_trace_->tile_buffer_update_cb = [&]() {
    if (!update_render_tile_cb) {
      return;
    }
    update_render_tile_cb();
  };
  path_trace_->tile_buffer_write_cb = [&]() {
    if (!write_render_tile_cb) {
      return;
    }
    write_render_tile_cb();
  };
  path_trace_->tile_buffer_read_cb = [&]() -> bool {
    if (!read_render_tile_cb) {
      return false;
    }
    read_render_tile_cb();
    return true;
  };
  path_trace_->progress_update_cb = [&]() { update_status_time(); };

  tile_manager_.full_buffer_written_cb = [&](string_view filename) {
    if (!full_buffer_written_cb) {
      return;
    }
    full_buffer_written_cb(filename);
  };
}

Session::~Session()
{
  cancel();

  /* TODO(sergey): Bring the passes in viewport back.
   * It is unclear why there is such an exception needed though. */
#if 0
  if (buffers && params.write_render_cb) {
    /* Copy to display buffer and write out image if requested */
    delete display;

    display = new DisplayBuffer(device, false);
    display->reset(buffers->params);
    copy_to_display_buffer(params.samples);

    int w = display->draw_width;
    int h = display->draw_height;
    uchar4 *pixels = display->rgba_byte.copy_from_device(0, w, h);
    params.write_render_cb((uchar *)pixels, w, h, 4);
  }
#endif

  /* Make sure path tracer is destroyed before the deviec. This is needed because destruction might
   * need to access device for device memory free. */
  /* TODO(sergey): Convert device to be unique_ptr, and rely on C++ to destruct objects in the
   * pre-defined order. */
  path_trace_.reset();

  delete scene;
  delete device;

  TaskScheduler::exit();
}

void Session::start()
{
  if (!session_thread_) {
    session_thread_ = new thread(function_bind(&Session::run, this));
  }
}

void Session::cancel(bool quick)
{
  if (quick && path_trace_) {
    path_trace_->cancel();
  }

  if (session_thread_) {
    /* wait for session thread to end */
    progress.set_cancel("Exiting");

    {
      thread_scoped_lock pause_lock(pause_mutex_);
      pause_ = false;
      cancel_ = true;
    }
    pause_cond_.notify_all();

    wait();
  }
}

bool Session::ready_to_reset()
{
  return path_trace_->ready_to_reset();
}

void Session::run_main_render_loop()
{
  path_trace_->clear_gpu_display();

  while (true) {
    RenderWork render_work = run_update_for_next_iteration();

    if (!render_work) {
      if (VLOG_IS_ON(2)) {
        double total_time, render_time;
        progress.get_time(total_time, render_time);
        VLOG(2) << "Rendering in main loop is done in " << render_time << " seconds.";
        VLOG(2) << path_trace_->full_report();
      }

      if (params.background) {
        /* if no work left and in background mode, we can stop immediately. */
        progress.set_status("Finished");
        break;
      }
    }

    const bool did_cancel = progress.get_cancel();
    if (did_cancel) {
      render_scheduler_.render_work_reschedule_on_cancel(render_work);
      if (!render_work) {
        break;
      }
    }
    else if (run_wait_for_work(render_work)) {
      continue;
    }

    {
      /* buffers mutex is locked entirely while rendering each
       * sample, and released/reacquired on each iteration to allow
       * reset and draw in between */
      thread_scoped_lock buffers_lock(buffers_mutex_);

      /* update status and timing */
      update_status_time();

      /* render */
      path_trace_->render(render_work);

      /* update status and timing */
      update_status_time();

      if (device->have_error()) {
        const string &error_message = device->error_message();
        progress.set_error(error_message);
        progress.set_cancel(error_message);
        break;
      }
    }

    progress.set_update();

    if (did_cancel) {
      break;
    }
  }
}

void Session::run()
{
  if (params.use_profiling && (params.device.type == DEVICE_CPU)) {
    profiler.start();
  }

  /* session thread loop */
  progress.set_status("Waiting for render to start");

  /* run */
  if (!progress.get_cancel()) {
    /* reset number of rendered samples */
    progress.reset_sample();

    run_main_render_loop();
  }

  profiler.stop();

  /* progress update */
  if (progress.get_cancel())
    progress.set_status(progress.get_cancel_message());
  else
    progress.set_update();
}

RenderWork Session::run_update_for_next_iteration()
{
  RenderWork render_work;

  thread_scoped_lock scene_lock(scene->mutex);
  thread_scoped_lock reset_lock(delayed_reset_.mutex);

  bool have_tiles = true;
  bool switched_to_new_tile = false;

  if (delayed_reset_.do_reset) {
    thread_scoped_lock buffers_lock(buffers_mutex_);
    do_delayed_reset();

    /* After reset make sure the tile manager is at the first big tile. */
    have_tiles = tile_manager_.next();
    switched_to_new_tile = true;
  }

  /* Update number of samples in the integrator.
   * Ideally this would need to happen once in `Session::set_samples()`, but the issue there is
   * the initial configuration when Session is created where the `set_samples()` is not used. */
  scene->integrator->set_aa_samples(params.samples);

  /* Update denoiser settings. */
  {
    const DenoiseParams denoise_params = scene->integrator->get_denoise_params();
    path_trace_->set_denoiser_params(denoise_params);
  }

  /* Update adaptive sampling. */
  {
    const AdaptiveSampling adaptive_sampling = scene->integrator->get_adaptive_sampling();
    path_trace_->set_adaptive_sampling(adaptive_sampling);
  }

  render_scheduler_.set_num_samples(params.samples);
  render_scheduler_.set_time_limit(params.time_limit);

  while (have_tiles) {
    render_work = render_scheduler_.get_render_work();
    if (render_work) {
      break;
    }

    progress.add_finished_tile(false);

    have_tiles = tile_manager_.next();
    if (have_tiles) {
      render_scheduler_.reset_for_next_tile();
      switched_to_new_tile = true;
    }
  }

  if (render_work) {
    scoped_timer update_timer;

    if (switched_to_new_tile) {
      BufferParams tile_params = buffer_params_;

      const Tile &tile = tile_manager_.get_current_tile();
      tile_params.width = tile.width;
      tile_params.height = tile.height;
      tile_params.full_x = tile.x + buffer_params_.full_x;
      tile_params.full_y = tile.y + buffer_params_.full_y;
      tile_params.full_width = buffer_params_.full_width;
      tile_params.full_height = buffer_params_.full_height;
      tile_params.update_offset_stride();

      path_trace_->reset(buffer_params_, tile_params);
    }

    const int resolution = render_work.resolution_divider;
    const int width = max(1, buffer_params_.full_width / resolution);
    const int height = max(1, buffer_params_.full_height / resolution);

    if (update_scene(width, height)) {
      profiler.reset(scene->shaders.size(), scene->objects.size());
    }
    progress.add_skip_time(update_timer, params.background);
  }

  return render_work;
}

bool Session::run_wait_for_work(const RenderWork &render_work)
{
  /* In an offline rendering there is no pause, and no tiles will mean the job is fully done. */
  if (params.background) {
    return false;
  }

  thread_scoped_lock pause_lock(pause_mutex_);

  if (!pause_ && render_work) {
    /* Rendering is not paused and there is work to be done. No need to wait for anything. */
    return false;
  }

  const bool no_work = !render_work;
  update_status_time(pause_, no_work);

  /* Only leave the loop when rendering is not paused. But even if the current render is un-paused
   * but there is nothing to render keep waiting until new work is added. */
  while (!cancel_) {
    scoped_timer pause_timer;

    if (!pause_ && (render_work || new_work_added_ || delayed_reset_.do_reset)) {
      break;
    }

    /* Wait for either pause state changed, or extra samples added to render. */
    pause_cond_.wait(pause_lock);

    if (pause_) {
      progress.add_skip_time(pause_timer, params.background);
    }

    update_status_time(pause_, no_work);
    progress.set_update();
  }

  new_work_added_ = false;

  return no_work;
}

void Session::draw()
{
  path_trace_->draw();
}

int2 Session::get_effective_tile_size() const
{
  /* No support yet for baking with tiles. */
  if (!params.use_auto_tile || scene->bake_manager->get_baking()) {
    return make_int2(buffer_params_.width, buffer_params_.height);
  }

  /* TODO(sergey): Take available memory into account, and if there is enough memory do not tile
   * and prefer optimal performance. */

  return make_int2(params.tile_size, params.tile_size);
}

void Session::do_delayed_reset()
{
  if (!delayed_reset_.do_reset) {
    return;
  }
  delayed_reset_.do_reset = false;

  params = delayed_reset_.session_params;
  buffer_params_ = delayed_reset_.buffer_params;

  /* Store parameters used for buffers access outside of scene graph.  */
  buffer_params_.exposure = scene->film->get_exposure();
  buffer_params_.use_approximate_shadow_catcher =
      scene->film->get_use_approximate_shadow_catcher();
  buffer_params_.use_transparent_background = scene->background->get_transparent();

  /* Tile and work scheduling. */
  tile_manager_.reset_scheduling(buffer_params_, get_effective_tile_size());
  render_scheduler_.reset(buffer_params_, params.samples);

  /* Passes. */
  /* When multiple tiles are used SAMPLE_COUNT pass is used to keep track of possible partial
   * tile results. It is safe to use generic update function here which checks for changes since
   * changes in tile settings re-creates session, which ensures film is fully updated on tile
   * changes. */
  scene->film->update_passes(scene, tile_manager_.has_multiple_tiles());

  /* Update for new state of scene and passes. */
  buffer_params_.update_passes(scene->passes);
  tile_manager_.update(buffer_params_, scene);

  /* Progress. */
  progress.reset_sample();
  progress.set_total_pixel_samples(buffer_params_.width * buffer_params_.height * params.samples);

  if (!params.background) {
    progress.set_start_time();
  }
  progress.set_render_start_time();
}

void Session::reset(const SessionParams &session_params, const BufferParams &buffer_params)
{
  {
    thread_scoped_lock reset_lock(delayed_reset_.mutex);
    thread_scoped_lock pause_lock(pause_mutex_);

    delayed_reset_.do_reset = true;
    delayed_reset_.session_params = session_params;
    delayed_reset_.buffer_params = buffer_params;

    path_trace_->cancel();
  }

  pause_cond_.notify_all();
}

void Session::set_samples(int samples)
{
  if (samples == params.samples) {
    return;
  }

  params.samples = samples;

  {
    thread_scoped_lock pause_lock(pause_mutex_);
    new_work_added_ = true;
  }

  pause_cond_.notify_all();
}

void Session::set_time_limit(double time_limit)
{
  if (time_limit == params.time_limit) {
    return;
  }

  params.time_limit = time_limit;

  {
    thread_scoped_lock pause_lock(pause_mutex_);
    new_work_added_ = true;
  }

  pause_cond_.notify_all();
}

void Session::set_pause(bool pause)
{
  bool notify = false;

  {
    thread_scoped_lock pause_lock(pause_mutex_);

    if (pause != pause_) {
      pause_ = pause;
      notify = true;
    }
  }

  if (session_thread_) {
    if (notify) {
      pause_cond_.notify_all();
    }
  }
  else if (pause_) {
    update_status_time(pause_);
  }
}

void Session::set_gpu_display(unique_ptr<GPUDisplay> gpu_display)
{
  path_trace_->set_gpu_display(move(gpu_display));
}

void Session::wait()
{
  if (session_thread_) {
    session_thread_->join();
    delete session_thread_;
  }

  session_thread_ = nullptr;
}

bool Session::update_scene(int width, int height)
{
  /* Update camera if dimensions changed for progressive render. the camera
   * knows nothing about progressive or cropped rendering, it just gets the
   * image dimensions passed in. */
  Camera *cam = scene->camera;
  cam->set_screen_size(width, height);

  /* First detect which kernel features are used and allocate working memory.
   * This helps estimate how may device memory is available for the scene and
   * how much we need to allocate on the host instead. */
  scene->update_kernel_features();

  path_trace_->load_kernels();
  path_trace_->alloc_work_memory();

  if (scene->update(progress)) {
    return true;
  }

  return false;
}

static string status_append(const string &status, const string &suffix)
{
  string prefix = status;
  if (!prefix.empty()) {
    prefix += ", ";
  }
  return prefix + suffix;
}

void Session::update_status_time(bool show_pause, bool show_done)
{
  string status, substatus;

  const int current_tile = progress.get_rendered_tiles();
  const int num_tiles = tile_manager_.get_num_tiles();

  const int current_sample = progress.get_current_sample();
  const int num_samples = render_scheduler_.get_num_samples();

  /* TIle. */
  if (tile_manager_.has_multiple_tiles()) {
    substatus = status_append(substatus,
                              string_printf("Rendered %d/%d Tiles", current_tile, num_tiles));
  }

  /* Sample. */
  if (num_samples == Integrator::MAX_SAMPLES) {
    substatus = status_append(substatus, string_printf("Sample %d", current_sample));
  }
  else {
    substatus = status_append(substatus,
                              string_printf("Sample %d/%d", current_sample, num_samples));
  }

  /* TODO(sergey): Denoising status from the path trace. */

  if (show_pause) {
    status = "Rendering Paused";
  }
  else if (show_done) {
    status = "Rendering Done";
    progress.set_end_time(); /* Save end time so that further calls to get_time are accurate. */
  }
  else {
    status = substatus;
    substatus.clear();
  }

  progress.set_status(status, substatus);
}

void Session::device_free()
{
  scene->device_free();
  path_trace_->device_free();
}

void Session::collect_statistics(RenderStats *render_stats)
{
  scene->collect_statistics(render_stats);
  if (params.use_profiling && (params.device.type == DEVICE_CPU)) {
    render_stats->collect_profiling(scene, profiler);
  }
}

/* --------------------------------------------------------------------
 * Tile and tile pixels aceess.
 */

bool Session::has_multiple_render_tiles() const
{
  return tile_manager_.has_multiple_tiles();
}

int2 Session::get_render_tile_size() const
{
  return path_trace_->get_render_tile_size();
}

int2 Session::get_render_tile_offset() const
{
  return path_trace_->get_render_tile_offset();
}

string_view Session::get_render_tile_layer() const
{
  const BufferParams &buffer_params = path_trace_->get_render_tile_params();
  return buffer_params.layer;
}

string_view Session::get_render_tile_view() const
{
  const BufferParams &buffer_params = path_trace_->get_render_tile_params();
  return buffer_params.view;
}

bool Session::copy_render_tile_from_device()
{
  return path_trace_->copy_render_tile_from_device();
}

bool Session::get_render_tile_pixels(const string &pass_name, int num_components, float *pixels)
{
  /* NOTE: The code relies on a fact that session is fully update and no scene/buffer modification
   * is happenning while this function runs. */

  const BufferParams &buffer_params = path_trace_->get_render_tile_params();

  const BufferPass *pass = buffer_params.find_pass(pass_name);
  if (pass == nullptr) {
    return false;
  }

  const bool has_denoised_result = path_trace_->has_denoised_result();
  if (pass->mode == PassMode::DENOISED && !has_denoised_result) {
    pass = buffer_params.find_pass(pass->type);
    if (pass == nullptr) {
      /* Happens when denoised result pass is requested but is never written by the kernel. */
      return false;
    }
  }

  pass = buffer_params.get_actual_display_pass(pass);

  const float exposure = buffer_params.exposure;
  const int num_samples = path_trace_->get_num_render_tile_samples();

  PassAccessor::PassAccessInfo pass_access_info(*pass);
  pass_access_info.use_approximate_shadow_catcher = buffer_params.use_approximate_shadow_catcher;
  pass_access_info.use_approximate_shadow_catcher_background =
      pass_access_info.use_approximate_shadow_catcher && !buffer_params.use_transparent_background;

  const PassAccessorCPU pass_accessor(pass_access_info, exposure, num_samples);
  const PassAccessor::Destination destination(pixels, num_components);

  return path_trace_->get_render_tile_pixels(pass_accessor, destination);
}

bool Session::set_render_tile_pixels(const string &pass_name,
                                     int num_components,
                                     const float *pixels)
{
  /* NOTE: The code relies on a fact that session is fully update and no scene/buffer modification
   * is happenning while this function runs. */

  const BufferPass *pass = buffer_params_.find_pass(pass_name);
  if (!pass) {
    return false;
  }

  const float exposure = scene->film->get_exposure();
  const int num_samples = render_scheduler_.get_num_rendered_samples();

  const PassAccessor::PassAccessInfo pass_access_info(*pass);
  PassAccessorCPU pass_accessor(pass_access_info, exposure, num_samples);
  PassAccessor::Source source(pixels, num_components);

  return path_trace_->set_render_tile_pixels(pass_accessor, source);
}

/* --------------------------------------------------------------------
 * Full-frame on-disk storage.
 */

void Session::process_full_buffer_from_disk(string_view filename)
{
  path_trace_->process_full_buffer_from_disk(filename);
}

CCL_NAMESPACE_END
