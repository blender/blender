/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <functional>

#include "device/device.h"
#include "integrator/render_scheduler.h"
#include "scene/shader.h"
#include "scene/stats.h"
#include "session/buffers.h"
#include "session/tile.h"

#include "util/progress.h"
#include "util/stats.h"
#include "util/thread.h"
#include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

class BufferParams;
class Device;
class DeviceScene;
class DisplayDriver;
class OutputDriver;
class PathTrace;
class Progress;
class RenderBuffers;
class Scene;
class SceneParams;

/* Session Parameters */

class SessionParams {
 public:
  /* Device, which is chosen based on Blender Cycles preferences, as well as Scene settings and
   * command line arguments. */
  DeviceInfo device;
  /* Device from Cycles preferences for denoising. */
  DeviceInfo denoise_device;

  bool headless;
  bool background;

  int samples;
  bool use_sample_subset;
  int sample_subset_offset;
  int sample_subset_length;
  int pixel_size;
  int threads;

  /* Limit in seconds for how long path tracing is allowed to happen.
   * Zero means no limit is applied. */
  double time_limit;

  bool use_profiling;

  bool use_auto_tile;
  int tile_size;

  bool use_resolution_divider;

  ShadingSystem shadingsystem;

  /* Session-specific temporary directory to store in-progress EXR files in. */
  string temp_dir;

  SessionParams()
  {
    headless = false;
    background = false;

    samples = 1024;
    use_sample_subset = false;
    sample_subset_offset = 0;
    sample_subset_length = 1024;
    pixel_size = 1;
    threads = 0;
    time_limit = 0.0;

    use_profiling = false;

    use_auto_tile = true;
    tile_size = 2048;

    use_resolution_divider = true;

    shadingsystem = SHADINGSYSTEM_SVM;
  }

  bool modified(const SessionParams &params) const
  {
    /* Modified means we have to recreate the session, any parameter changes
     * that can be handled by an existing Session are omitted. */
    return !(device == params.device && headless == params.headless &&
             background == params.background && pixel_size == params.pixel_size &&
             threads == params.threads && use_profiling == params.use_profiling &&
             shadingsystem == params.shadingsystem && use_auto_tile == params.use_auto_tile &&
             tile_size == params.tile_size);
  }
};

/* Session
 *
 * This is the class that contains the session thread, running the render
 * control loop and dispatching tasks. */

class Session {
 public:
  unique_ptr<Device> device;
  /* Denoiser device. Could be the same as the path trace device. */
  unique_ptr<Device> denoise_device_;
  unique_ptr<Scene> scene;
  Progress progress;
  SessionParams params;
  Stats stats;
  Profiler profiler;

  /* Callback is invoked by tile manager whenever on-dist tiles storage file is closed after
   * writing. Allows an engine integration to keep track of those files without worry about
   * transferring the information when it needs to re-create session during rendering. */
  std::function<void(string_view)> full_buffer_written_cb;

  explicit Session(const SessionParams &params, const SceneParams &scene_params);
  ~Session();

  void start();

  /* When quick cancel is requested path tracing is cancels as soon as possible, without waiting
   * for the buffer to be uniformly sampled. */
  void cancel(bool quick = false);

  void draw();
  void wait();

  bool ready_to_reset();
  void reset(const SessionParams &session_params, const BufferParams &buffer_params);

  void set_pause(bool pause);

  void set_samples(const int samples);
  void set_time_limit(const double time_limit);

  void set_output_driver(unique_ptr<OutputDriver> driver);
  void set_display_driver(unique_ptr<DisplayDriver> driver);

  double get_estimated_remaining_time() const;

  void device_free();

  /* Returns the rendering progress or 0 if no progress can be determined
   * (for example, when rendering with unlimited samples). */
  float get_progress();

  void collect_statistics(RenderStats *stats);

  /* --------------------------------------------------------------------
   * Full-frame on-disk storage.
   */

  /* Read given full-frame file from disk, perform needed processing and write it to the software
   * via the write callback. */
  void process_full_buffer_from_disk(string_view filename);

 protected:
  struct DelayedReset {
    thread_mutex mutex;
    bool do_reset;
    SessionParams session_params;
    BufferParams buffer_params;
  } delayed_reset_;

  void thread_run();
  void thread_render();

  /* Check whether the session thread is in `SESSION_THREAD_RENDER` state.
   * Returns true if it is so. */
  bool is_session_thread_rendering();

  /* Update for the new iteration of the main loop in run implementation (run_cpu and run_gpu).
   *
   * Will take care of the following things:
   *  - Delayed reset
   *  - Scene update
   *  - Tile manager advance
   *  - Render scheduler work request
   *
   * The updates are done in a proper order with proper locking around them, which guarantees
   * that the device side of scene and render buffers are always in a consistent state.
   *
   * Returns render work which is to be rendered next. */
  RenderWork run_update_for_next_iteration();

  /* Wait for rendering to be unpaused, or for new tiles for render to arrive.
   * Returns true if new main render loop iteration is required after this function call.
   *
   * The `render_work` is the work which was scheduled by the render scheduler right before
   * checking the pause. */
  bool run_wait_for_work(const RenderWork &render_work);

  void run_main_render_loop();

  bool update_scene(const bool reset_samples);

  void update_status_time(bool show_pause = false, bool show_done = false);

  bool delayed_reset_buffer_params();
  void update_buffers_for_params();

  int2 get_effective_tile_size() const;

  /* Get device used for denoising, may be the same as render device. */
  Device *denoise_device()
  {
    return (denoise_device_) ? denoise_device_.get() : device.get();
  }

  /* Session thread that performs rendering tasks decoupled from the thread
   * controlling the sessions. The thread is created and destroyed along with
   * the session. */
  unique_ptr<thread> session_thread_ = nullptr;
  thread_condition_variable session_thread_cond_;
  thread_mutex session_thread_mutex_;
  enum {
    SESSION_THREAD_WAIT,
    SESSION_THREAD_RENDER,
    SESSION_THREAD_END,
  } session_thread_state_ = SESSION_THREAD_WAIT;

  bool pause_ = false;
  bool new_work_added_ = false;

  thread_condition_variable pause_cond_;
  thread_mutex pause_mutex_;
  thread_mutex tile_mutex_;
  thread_mutex buffers_mutex_;

  TileManager tile_manager_;
  BufferParams buffer_params_;

  /* Render scheduler is used to get work to be rendered with the current big tile. */
  RenderScheduler render_scheduler_;

  /* Path tracer object.
   *
   * Is a single full-frame path tracer for interactive viewport rendering.
   * A path tracer for the current big-tile for an offline rendering. */
  unique_ptr<PathTrace> path_trace_;
};

CCL_NAMESPACE_END
