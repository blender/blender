/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <limits.h>
#include <string.h>

#include "device/cpu/device.h"
#include "device/device.h"
#include "integrator/pass_accessor_cpu.h"
#include "integrator/path_trace.h"
#include "scene/background.h"
#include "scene/bake.h"
#include "scene/camera.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "scene/shader_graph.h"
#include "session/buffers.h"
#include "session/display_driver.h"
#include "session/output_driver.h"
#include "session/session.h"

#include "util/foreach.h"
#include "util/function.h"
#include "util/log.h"
#include "util/math.h"
#include "util/task.h"
#include "util/time.h"

CCL_NAMESPACE_BEGIN

Session::Session(const SessionParams &params_, const SceneParams &scene_params)
    : params(params_), render_scheduler_(tile_manager_, params)
{
  TaskScheduler::init(params.threads);

  delayed_reset_.do_reset = false;

  pause_ = false;
  new_work_added_ = false;

  device = Device::create(params.device, stats, profiler);

  if (device->have_error()) {
    progress.set_error(device->error_message());
  }

  scene = new Scene(scene_params, device);

  /* Configure path tracer. */
  path_trace_ = make_unique<PathTrace>(
      device, scene->film, &scene->dscene, render_scheduler_, tile_manager_);
  path_trace_->set_progress(&progress);
  path_trace_->progress_update_cb = [&]() { update_status_time(); };

  tile_manager_.full_buffer_written_cb = [&](string_view filename) {
    if (!full_buffer_written_cb) {
      return;
    }
    full_buffer_written_cb(filename);
  };

  /* Create session thread. */
  session_thread_ = new thread(function_bind(&Session::thread_run, this));
}

Session::~Session()
{
  /* Cancel any ongoing render operation. */
  cancel();

  /* Signal session thread to end. */
  {
    thread_scoped_lock session_thread_lock(session_thread_mutex_);
    session_thread_state_ = SESSION_THREAD_END;
  }
  session_thread_cond_.notify_all();

  /* Destroy session thread. */
  session_thread_->join();
  delete session_thread_;

  /* Destroy path tracer, before the device. This is needed because destruction might need to
   * access device for device memory free.
   * TODO(sergey): Convert device to be unique_ptr, and rely on C++ to destruct objects in the
   * pre-defined order. */
  path_trace_.reset();

  /* Destroy scene and device. */
  delete scene;
  delete device;

  /* Stop task scheduler. */
  TaskScheduler::exit();
}

void Session::start()
{
  {
    /* Signal session thread to start rendering. */
    thread_scoped_lock session_thread_lock(session_thread_mutex_);
    if (session_thread_state_ == SESSION_THREAD_RENDER) {
      /* Already rendering, nothing to do. */
      return;
    }
    session_thread_state_ = SESSION_THREAD_RENDER;
  }

  session_thread_cond_.notify_all();
}

void Session::cancel(bool quick)
{
  /* Cancel any long running device operations (e.g. shader compilations). */
  device->cancel();

  /* Check if session thread is rendering. */
  const bool rendering = is_session_thread_rendering();

  if (rendering) {
    /* Cancel path trace operations. */
    if (quick && path_trace_) {
      path_trace_->cancel();
    }

    /* Cancel other operations. */
    progress.set_cancel("Exiting");

    /* Signal unpause in case the render was paused. */
    {
      thread_scoped_lock pause_lock(pause_mutex_);
      pause_ = false;
    }
    pause_cond_.notify_all();

    /* Wait for render thread to be cancelled or finished. */
    wait();
  }
}

bool Session::ready_to_reset()
{
  return path_trace_->ready_to_reset();
}

void Session::run_main_render_loop()
{
  path_trace_->clear_display();

  while (true) {
    RenderWork render_work = run_update_for_next_iteration();

    if (!render_work) {
      if (VLOG_INFO_IS_ON) {
        double total_time, render_time;
        progress.get_time(total_time, render_time);
        VLOG_INFO << "Rendering in main loop is done in " << render_time << " seconds.";
        VLOG_INFO << path_trace_->full_report();
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

    /* Stop rendering if error happened during scene update or other step of preparing scene
     * for render. */
    if (device->have_error()) {
      progress.set_error(device->error_message());
      break;
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

      /* Stop rendering if error happened during path tracing. */
      if (device->have_error()) {
        progress.set_error(device->error_message());
        break;
      }
    }

    progress.set_update();

    if (did_cancel) {
      break;
    }
  }
}

void Session::thread_run()
{
  while (true) {
    {
      thread_scoped_lock session_thread_lock(session_thread_mutex_);

      if (session_thread_state_ == SESSION_THREAD_WAIT) {
        /* Continue waiting for any signal from the main thread. */
        session_thread_cond_.wait(session_thread_lock);
        continue;
      }
      else if (session_thread_state_ == SESSION_THREAD_END) {
        /* End thread immediately. */
        break;
      }
    }

    /* Execute a render. */
    thread_render();

    /* Go back from rendering to waiting. */
    {
      thread_scoped_lock session_thread_lock(session_thread_mutex_);
      if (session_thread_state_ == SESSION_THREAD_RENDER) {
        session_thread_state_ = SESSION_THREAD_WAIT;
      }
    }
    session_thread_cond_.notify_all();
  }

  /* Flush any remaining operations and destroy display driver here. This ensure
   * graphics API resources are created and destroyed all in the session thread,
   * which can avoid problems contexts and multiple threads. */
  path_trace_->flush_display();
  path_trace_->set_display_driver(nullptr);
}

void Session::thread_render()
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

bool Session::is_session_thread_rendering()
{
  thread_scoped_lock session_thread_lock(session_thread_mutex_);
  return (session_thread_state_ == SESSION_THREAD_RENDER);
}

RenderWork Session::run_update_for_next_iteration()
{
  RenderWork render_work;

  thread_scoped_lock scene_lock(scene->mutex);

  bool have_tiles = true;
  bool switched_to_new_tile = false;
  bool did_reset = false;

  /* Perform delayed reset if requested. */
  {
    thread_scoped_lock reset_lock(delayed_reset_.mutex);
    if (delayed_reset_.do_reset) {
      did_reset = true;

      thread_scoped_lock buffers_lock(buffers_mutex_);
      do_delayed_reset();

      /* After reset make sure the tile manager is at the first big tile. */
      have_tiles = tile_manager_.next();
      switched_to_new_tile = true;
    }
  }

  /* Update number of samples in the integrator.
   * Ideally this would need to happen once in `Session::set_samples()`, but the issue there is
   * the initial configuration when Session is created where the `set_samples()` is not used.
   *
   * NOTE: Unless reset was requested only allow increasing number of samples. */
  if (did_reset || scene->integrator->get_aa_samples() < params.samples) {
    scene->integrator->set_aa_samples(params.samples);
  }

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

  /* Update path guiding. */
  {
    const GuidingParams guiding_params = scene->integrator->get_guiding_params(device);
    const bool guiding_reset = (guiding_params.use) ? scene->need_reset(false) : false;
    path_trace_->set_guiding_params(guiding_params, guiding_reset);
  }

  render_scheduler_.set_num_samples(params.samples);
  render_scheduler_.set_start_sample(params.sample_offset);
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

      tile_params.window_x = tile.window_x;
      tile_params.window_y = tile.window_y;
      tile_params.window_width = tile.window_width;
      tile_params.window_height = tile.window_height;

      tile_params.full_x = tile.x + buffer_params_.full_x;
      tile_params.full_y = tile.y + buffer_params_.full_y;
      tile_params.full_width = buffer_params_.full_width;
      tile_params.full_height = buffer_params_.full_height;

      tile_params.update_offset_stride();

      path_trace_->reset(buffer_params_, tile_params, did_reset);
    }

    const int resolution = render_work.resolution_divider;
    const int width = max(1, buffer_params_.full_width / resolution);
    const int height = max(1, buffer_params_.full_height / resolution);

    {
      /* Load render kernels, before device update where we upload data to the GPU.
       * Do it outside of the scene mutex since the heavy part of the loading (i.e. kernel
       * compilation) does not depend on the scene and some other functionality (like display
       * driver) might be waiting on the scene mutex to synchronize display pass.
       *
       * The scene will lock itself for the short period if it needs to update kernel features. */
      scene_lock.unlock();
      scene->load_kernels(progress);
      scene_lock.lock();
    }

    if (update_scene(width, height)) {
      profiler.reset(scene->shaders.size(), scene->objects.size());
    }

    /* Unlock scene mutex before loading denoiser kernels, since that may attempt to activate
     * graphics interop, which can deadlock when the scene mutex is still being held. */
    scene_lock.unlock();

    path_trace_->load_kernels();
    path_trace_->alloc_work_memory();

    /* Wait for device to be ready (e.g. finish any background compilations). */
    string device_status;
    while (!device->is_ready(device_status)) {
      progress.set_status(device_status);
      if (progress.get_cancel()) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
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

  /* Only leave the loop when rendering is not paused. But even if the current render is
   * un-paused but there is nothing to render keep waiting until new work is added. */
  while (!progress.get_cancel()) {
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
  const int image_width = buffer_params_.width;
  const int image_height = buffer_params_.height;

  if (!params.use_auto_tile) {
    return make_int2(image_width, image_height);
  }

  const int64_t image_area = static_cast<int64_t>(image_width) * image_height;

  /* TODO(sergey): Take available memory into account, and if there is enough memory do not
   * tile and prefer optimal performance. */

  const int tile_size = tile_manager_.compute_render_tile_size(params.tile_size);
  const int64_t actual_tile_area = static_cast<int64_t>(tile_size) * tile_size;

  if (actual_tile_area >= image_area && image_width <= TileManager::MAX_TILE_SIZE &&
      image_height <= TileManager::MAX_TILE_SIZE)
  {
    return make_int2(image_width, image_height);
  }

  return make_int2(tile_size, tile_size);
}

void Session::do_delayed_reset()
{
  if (!delayed_reset_.do_reset) {
    return;
  }
  delayed_reset_.do_reset = false;

  params = delayed_reset_.session_params;
  buffer_params_ = delayed_reset_.buffer_params;

  /* Store parameters used for buffers access outside of scene graph. */
  buffer_params_.samples = params.samples;
  buffer_params_.exposure = scene->film->get_exposure();
  buffer_params_.use_approximate_shadow_catcher =
      scene->film->get_use_approximate_shadow_catcher();
  buffer_params_.use_transparent_background = scene->background->get_transparent();

  /* Tile and work scheduling. */
  tile_manager_.reset_scheduling(buffer_params_, get_effective_tile_size());
  render_scheduler_.reset(buffer_params_, params.samples, params.sample_offset);

  /* Passes. */
  /* When multiple tiles are used SAMPLE_COUNT pass is used to keep track of possible partial
   * tile results. It is safe to use generic update function here which checks for changes since
   * changes in tile settings re-creates session, which ensures film is fully updated on tile
   * changes. */
  scene->film->update_passes(scene, tile_manager_.has_multiple_tiles());

  /* Update for new state of scene and passes. */
  buffer_params_.update_passes(scene->passes);
  tile_manager_.update(buffer_params_, scene);

  /* Update temp directory on reset.
   * This potentially allows to finish the existing rendering with a previously configure
   * temporary
   * directory in the host software and switch to a new temp directory when new render starts. */
  tile_manager_.set_temp_dir(params.temp_dir);

  /* Progress. */
  progress.reset_sample();
  progress.set_total_pixel_samples(static_cast<uint64_t>(buffer_params_.width) *
                                   buffer_params_.height * params.samples);

  if (!params.background) {
    progress.set_start_time();
  }
  const double time_limit = params.time_limit * ((double)tile_manager_.get_num_tiles());
  progress.set_render_start_time();
  progress.set_time_limit(time_limit);
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

  if (is_session_thread_rendering()) {
    if (notify) {
      pause_cond_.notify_all();
    }
  }
  else if (pause_) {
    update_status_time(pause_);
  }
}

void Session::set_output_driver(unique_ptr<OutputDriver> driver)
{
  path_trace_->set_output_driver(std::move(driver));
}

void Session::set_display_driver(unique_ptr<DisplayDriver> driver)
{
  path_trace_->set_display_driver(std::move(driver));
}

double Session::get_estimated_remaining_time() const
{
  const double completed = progress.get_progress();
  if (completed == 0.0) {
    return 0.0;
  }

  double total_time, render_time;
  progress.get_time(total_time, render_time);
  double remaining = (1.0 - (double)completed) * (render_time / (double)completed);

  const double time_limit = render_scheduler_.get_time_limit() *
                            ((double)tile_manager_.get_num_tiles());
  if (time_limit != 0.0) {
    remaining = min(remaining, max(time_limit - render_time, 0.0));
  }

  return remaining;
}

void Session::wait()
{
  /* Wait until session thread either is waiting or ending. */
  while (true) {
    thread_scoped_lock session_thread_lock(session_thread_mutex_);
    if (session_thread_state_ != SESSION_THREAD_RENDER) {
      break;
    }
    session_thread_cond_.wait(session_thread_lock);
  }
}

bool Session::update_scene(int width, int height)
{
  /* Update camera if dimensions changed for progressive render. the camera
   * knows nothing about progressive or cropped rendering, it just gets the
   * image dimensions passed in. */
  Camera *cam = scene->camera;
  cam->set_screen_size(width, height);

  return scene->update(progress);
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
  if (!params.background && num_samples == Integrator::MAX_SAMPLES) {
    substatus = status_append(substatus, string_printf("Sample %d", current_sample));
  }
  else {
    substatus = status_append(substatus,
                              string_printf("Sample %d/%d", current_sample, num_samples));
  }

  /* Append any device-specific status (such as background kernel optimization) */
  string device_status;
  if (device->is_ready(device_status) && !device_status.empty()) {
    substatus += string_printf(" (%s)", device_status.c_str());
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
 * Full-frame on-disk storage.
 */

void Session::process_full_buffer_from_disk(string_view filename)
{
  path_trace_->process_full_buffer_from_disk(filename);
}

CCL_NAMESPACE_END
