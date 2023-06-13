/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "integrator/denoiser.h"
#include "integrator/guiding.h"
#include "integrator/pass_accessor.h"
#include "integrator/path_trace_work.h"
#include "integrator/work_balancer.h"

#include "session/buffers.h"

#include "util/function.h"
#include "util/guiding.h"
#include "util/thread.h"
#include "util/unique_ptr.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class AdaptiveSampling;
class Device;
class DeviceScene;
class DisplayDriver;
class Film;
class RenderBuffers;
class RenderScheduler;
class RenderWork;
class PathTraceDisplay;
class OutputDriver;
class Progress;
class TileManager;

/* PathTrace class takes care of kernel graph and scheduling on a (multi)device. It takes care of
 * all the common steps of path tracing which are not device-specific. The list of tasks includes
 * but is not limited to:
 *  - Kernel graph.
 *  - Scheduling logic.
 *  - Queues management.
 *  - Adaptive stopping. */
class PathTrace {
 public:
  /* Render scheduler is used to report timing information and access things like start/finish
   * sample. */
  PathTrace(Device *device,
            Film *film,
            DeviceScene *device_scene,
            RenderScheduler &render_scheduler,
            TileManager &tile_manager);
  ~PathTrace();

  /* Create devices and load kernels which are created on-demand (for example, denoising devices).
   * The progress is reported to the currently configure progress object (via `set_progress`). */
  void load_kernels();

  /* Allocate working memory. This runs before allocating scene memory so that we can estimate
   * more accurately which scene device memory may need to allocated on the host. */
  void alloc_work_memory();

  /* Check whether now it is a good time to reset rendering.
   * Used to avoid very often resets in the viewport, giving it a chance to draw intermediate
   * render result. */
  bool ready_to_reset();

  void reset(const BufferParams &full_params,
             const BufferParams &big_tile_params,
             bool reset_rendering);

  void device_free();

  /* Set progress tracker.
   * Used to communicate details about the progress to the outer world, check whether rendering is
   * to be canceled.
   *
   * The path tracer writes to this object, and then at a convenient moment runs
   * progress_update_cb() callback. */
  void set_progress(Progress *progress);

  /* NOTE: This is a blocking call. Meaning, it will not return until given number of samples are
   * rendered (or until rendering is requested to be canceled). */
  void render(const RenderWork &render_work);

  /* TODO(sergey): Decide whether denoiser is really a part of path tracer. Currently it is
   * convenient to have it here because then its easy to access render buffer. But the downside is
   * that this adds too much of entities which can live separately with some clear API. */

  /* Set denoiser parameters.
   * Use this to configure the denoiser before rendering any samples. */
  void set_denoiser_params(const DenoiseParams &params);

  /* Set parameters used for adaptive sampling.
   * Use this to configure the adaptive sampler before rendering any samples. */
  void set_adaptive_sampling(const AdaptiveSampling &adaptive_sampling);

  /* Set the parameters for guiding.
   * Use to setup the guiding structures before each rendering iteration. */
  void set_guiding_params(const GuidingParams &params, const bool reset);

  /* Sets output driver for render buffer output. */
  void set_output_driver(unique_ptr<OutputDriver> driver);

  /* Set display driver for interactive render buffer display. */
  void set_display_driver(unique_ptr<DisplayDriver> driver);

  /* Clear the display buffer by filling it in with all zeroes. */
  void clear_display();

  /* Perform drawing of the current state of the DisplayDriver. */
  void draw();

  /* Flush outstanding display commands before ending the render loop. */
  void flush_display();

  /* Cancel rendering process as soon as possible, without waiting for full tile to be sampled.
   * Used in cases like reset of render session.
   *
   * This is a blocking call, which returns as soon as there is no running `render_samples()` call.
   */
  void cancel();

  /* Copy an entire render buffer to/from the path trace. */

  /* Copy happens via CPU side buffer: data will be copied from every device of the path trace, and
   * the data will be copied to the device of the given render buffers. */
  void copy_to_render_buffers(RenderBuffers *render_buffers);

  /* Copy happens via CPU side buffer: data will be copied from the device of the given render
   * buffers and will be copied to all devices of the path trace. */
  void copy_from_render_buffers(RenderBuffers *render_buffers);

  /* Copy render buffers of the big tile from the device to host.
   * Return true if all copies are successful. */
  bool copy_render_tile_from_device();

  /* Read given full-frame file from disk, perform needed processing and write it to the software
   * via the write callback. */
  void process_full_buffer_from_disk(string_view filename);

  /* Get number of samples in the current big tile render buffers. */
  int get_num_render_tile_samples() const;

  /* Get pass data of the entire big tile.
   * This call puts pass render result from all devices into the final pixels storage.
   *
   * NOTE: Expects buffers to be copied to the host using `copy_render_tile_from_device()`.
   *
   * Returns false if any of the accessor's `get_render_tile_pixels()` returned false. */
  bool get_render_tile_pixels(const PassAccessor &pass_accessor,
                              const PassAccessor::Destination &destination);

  /* Set pass data for baking. */
  bool set_render_tile_pixels(PassAccessor &pass_accessor, const PassAccessor::Source &source);

  /* Check whether denoiser was run and denoised passes are available. */
  bool has_denoised_result() const;

  /* Get size and offset (relative to the buffer's full x/y) of the currently rendering tile.
   * In the case of tiled rendering this will return full-frame after all tiles has been rendered.
   *
   * NOTE: If the full-frame buffer processing is in progress, returns parameters of the full-frame
   * instead. */
  int2 get_render_tile_size() const;
  int2 get_render_tile_offset() const;
  int2 get_render_size() const;

  /* Get buffer parameters of the current tile.
   *
   * NOTE: If the full-frame buffer processing is in progress, returns parameters of the full-frame
   * instead. */
  const BufferParams &get_render_tile_params() const;

  /* Generate full multi-line report of the rendering process, including rendering parameters,
   * times, and so on. */
  string full_report() const;

  /* Callback which is called to report current rendering progress.
   *
   * It is supposed to be cheaper than buffer update/write, hence can be called more often.
   * Additionally, it might be called form the middle of wavefront (meaning, it is not guaranteed
   * that the buffer is "uniformly" sampled at the moment of this callback). */
  function<void(void)> progress_update_cb;

 protected:
  /* Actual implementation of the rendering pipeline.
   * Calls steps in order, checking for the cancel to be requested in between.
   *
   * Is separate from `render()` to simplify dealing with the early outputs and keeping
   * `render_cancel_` in the consistent state. */
  void render_pipeline(RenderWork render_work);

  /* Initialize kernel execution on all integrator queues. */
  void render_init_kernel_execution();

  /* Make sure both allocated and effective buffer parameters of path tracer works are up to date
   * with the current big tile parameters, performance-dependent slicing, and resolution divider.
   */
  void update_work_buffer_params_if_needed(const RenderWork &render_work);
  void update_allocated_work_buffer_params();
  void update_effective_work_buffer_params(const RenderWork &render_work);

  /* Perform various steps of the render work.
   *
   * Note that some steps might modify the work, forcing some steps to happen within this iteration
   * of rendering. */
  void init_render_buffers(const RenderWork &render_work);
  void path_trace(RenderWork &render_work);
  void adaptive_sample(RenderWork &render_work);
  void denoise(const RenderWork &render_work);
  void cryptomatte_postprocess(const RenderWork &render_work);
  void update_display(const RenderWork &render_work);
  void rebalance(const RenderWork &render_work);
  void write_tile_buffer(const RenderWork &render_work);
  void finalize_full_buffer_on_disk(const RenderWork &render_work);

  /* Updates/initializes the guiding structures after a rendering iteration.
   * The structures are updated using the training data/samples generated during the previous
   * rendering iteration */
  void guiding_update_structures();

  /* Prepares the per-kernel thread related guiding structures (e.g., PathSegmentStorage,
   * pointers to the global Field and SegmentStorage)*/
  void guiding_prepare_structures();

  /* Get number of samples in the current state of the render buffers. */
  int get_num_samples_in_buffer();

  /* Check whether user requested to cancel rendering, so that path tracing is to be finished as
   * soon as possible. */
  bool is_cancel_requested();

  /* Write the big tile render buffer via the write callback. */
  void tile_buffer_write();

  /* Read the big tile render buffer via the read callback. */
  void tile_buffer_read();

  /* Write current tile into the file on disk. */
  void tile_buffer_write_to_disk();

  /* Run the progress_update_cb callback if it is needed. */
  void progress_update_if_needed(const RenderWork &render_work);

  void progress_set_status(const string &status, const string &substatus = "");

  /* Destroy GPU resources (such as graphics interop) used by work. */
  void destroy_gpu_resources();

  /* Pointer to a device which is configured to be used for path tracing. If multiple devices
   * are configured this is a `MultiDevice`. */
  Device *device_ = nullptr;

  /* CPU device for creating temporary render buffers on the CPU side. */
  unique_ptr<Device> cpu_device_;

  Film *film_;
  DeviceScene *device_scene_;

  RenderScheduler &render_scheduler_;
  TileManager &tile_manager_;

  /* Display driver for interactive render buffer display. */
  unique_ptr<PathTraceDisplay> display_;

  /* Output driver to write render buffer to. */
  unique_ptr<OutputDriver> output_driver_;

  /* Per-compute device descriptors of work which is responsible for path tracing on its configured
   * device. */
  vector<unique_ptr<PathTraceWork>> path_trace_works_;

  /* Per-path trace work information needed for multi-device balancing. */
  vector<WorkBalanceInfo> work_balance_infos_;

  /* Render buffer parameters of the full frame and current big tile. */
  BufferParams full_params_;
  BufferParams big_tile_params_;

  /* Denoiser which takes care of denoising the big tile. */
  unique_ptr<Denoiser> denoiser_;

  /* Denoiser device descriptor which holds the denoised big tile for multi-device workloads. */
  unique_ptr<PathTraceWork> big_tile_denoise_work_;

#ifdef WITH_PATH_GUIDING
  /* Guiding related attributes */
  GuidingParams guiding_params_;

  /* The guiding field which holds the representation of the incident radiance field for the
   * complete scene. */
  unique_ptr<openpgl::cpp::Field> guiding_field_;

  /* The storage container which holds the training data/samples generated during the last
   * rendering iteration. */
  unique_ptr<openpgl::cpp::SampleStorage> guiding_sample_data_storage_;

  /* The number of already performed training iterations for the guiding field. */
  int guiding_update_count = 0;
#endif

  /* State which is common for all the steps of the render work.
   * Is brought up to date in the `render()` call and is accessed from all the steps involved into
   * rendering the work. */
  struct {
    /* Denotes whether render buffers parameters of path trace works are to be reset for the new
     * value of the big tile parameters. */
    bool need_reset_params = false;

    /* Divider of the resolution for faster previews.
     *
     * Allows to re-use same render buffer, but have less pixels rendered into in it. The way to
     * think of render buffer in this case is as an over-allocated array: the resolution divider
     * affects both resolution and stride as visible by the integrator kernels. */
    int resolution_divider = 0;

    /* Parameters of the big tile with the current resolution divider applied. */
    BufferParams effective_big_tile_params;

    /* Denoiser was run and there are denoised versions of the passes in the render buffers. */
    bool has_denoised_result = false;

    /* Current tile has been written (to either disk or callback.
     * Indicates that no more work will be done on this tile. */
    bool tile_written = false;
  } render_state_;

  /* Progress object which is used to communicate sample progress. */
  Progress *progress_;

  /* Fields required for canceling render on demand, as quickly as possible. */
  struct {
    /* Indicates whether there is an on-going `render_samples()` call. */
    bool is_rendering = false;

    /* Indicates whether rendering is requested to be canceled by `cancel()`. */
    bool is_requested = false;

    /* Synchronization between thread which does `render_samples()` and thread which does
     * `cancel()`. */
    thread_mutex mutex;
    thread_condition_variable condition;
  } render_cancel_;

  /* Indicates whether a render result was drawn after latest session reset.
   * Used by `ready_to_reset()` to implement logic which feels the most interactive. */
  bool did_draw_after_reset_ = true;

  /* State of the full frame processing and writing to the software. */
  struct {
    RenderBuffers *render_buffers = nullptr;
  } full_frame_state_;
};

CCL_NAMESPACE_END
