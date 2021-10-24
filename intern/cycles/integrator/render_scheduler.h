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

#pragma once

#include "integrator/adaptive_sampling.h"
#include "integrator/denoiser.h" /* For DenoiseParams. */
#include "session/buffers.h"
#include "util/string.h"

CCL_NAMESPACE_BEGIN

class SessionParams;
class TileManager;

class RenderWork {
 public:
  int resolution_divider = 1;

  /* Initialize render buffers.
   * Includes steps like zeroing the buffer on the device, and optional reading of pixels from the
   * baking target. */
  bool init_render_buffers = false;

  /* Path tracing samples information. */
  struct {
    int start_sample = 0;
    int num_samples = 0;
  } path_trace;

  struct {
    /* Check for convergency and filter the mask. */
    bool filter = false;

    float threshold = 0.0f;

    /* Reset convergency flag when filtering, forcing a re-check of whether pixel did converge. */
    bool reset = false;
  } adaptive_sampling;

  struct {
    bool postprocess = false;
  } cryptomatte;

  /* Work related on the current tile. */
  struct {
    /* Write render buffers of the current tile.
     *
     * It is up to the path trace to decide whether writing should happen via user-provided
     * callback into the rendering software, or via tile manager into a partial file. */
    bool write = false;

    bool denoise = false;
  } tile;

  /* Work related on the full-frame render buffer. */
  struct {
    /* Write full render result.
     * Implies reading the partial file from disk. */
    bool write = false;
  } full;

  /* Display which is used to visualize render result. */
  struct {
    /* Display needs to be updated for the new render. */
    bool update = false;

    /* Display can use denoised result if available. */
    bool use_denoised_result = true;
  } display;

  /* Re-balance multi-device scheduling after rendering this work.
   * Note that the scheduler does not know anything about devices, so if there is only a single
   * device used, then it is up for the PathTracer to ignore the balancing. */
  bool rebalance = false;

  /* Conversion to bool, to simplify checks about whether there is anything to be done for this
   * work. */
  inline operator bool() const
  {
    return path_trace.num_samples || adaptive_sampling.filter || display.update || tile.denoise ||
           tile.write || full.write;
  }
};

class RenderScheduler {
 public:
  RenderScheduler(TileManager &tile_manager, const SessionParams &params);

  /* Specify whether cryptomatte-related works are to be scheduled. */
  void set_need_schedule_cryptomatte(bool need_schedule_cryptomatte);

  /* Allows to disable work re-balancing works, allowing to schedule as much to a single device
   * as possible. */
  void set_need_schedule_rebalance(bool need_schedule_rebalance);

  bool is_background() const;

  void set_denoiser_params(const DenoiseParams &params);
  void set_adaptive_sampling(const AdaptiveSampling &adaptive_sampling);

  bool is_adaptive_sampling_used() const;

  /* Start sample for path tracing.
   * The scheduler will schedule work using this sample as the first one. */
  void set_start_sample(int start_sample);
  int get_start_sample() const;

  /* Number of samples to render, starting from start sample.
   * The scheduler will schedule work in the range of
   * [start_sample, start_sample + num_samples - 1], inclusively. */
  void set_num_samples(int num_samples);
  int get_num_samples() const;

  /* Time limit for the path tracing tasks, in minutes.
   * Zero disables the limit. */
  void set_time_limit(double time_limit);
  double get_time_limit() const;

  /* Get sample up to which rendering has been done.
   * This is an absolute 0-based value.
   *
   * For example, if start sample is 10 and and 5 samples were rendered, then this call will
   * return 14.
   *
   * If there were no samples rendered, then the behavior is undefined. */
  int get_rendered_sample() const;

  /* Get number of samples rendered within the current scheduling session.
   *
   * For example, if start sample is 10 and and 5 samples were rendered, then this call will
   * return 5.
   *
   * Note that this is based on the scheduling information. In practice this means that if someone
   * requested for work to render the scheduler considers the work done. */
  int get_num_rendered_samples() const;

  /* Reset scheduler, indicating that rendering will happen from scratch.
   * Resets current rendered state, as well as scheduling information. */
  void reset(const BufferParams &buffer_params, int num_samples);

  /* Reset scheduler upon switching to a next tile.
   * Will keep the same number of samples and full-frame render parameters, but will reset progress
   * and allow schedule renders works from the beginning of the new tile. */
  void reset_for_next_tile();

  /* Reschedule adaptive sampling work when all pixels did converge.
   * If there is nothing else to be done for the adaptive sampling (pixels did converge to the
   * final threshold) then false is returned and the render scheduler will stop scheduling path
   * tracing works. Otherwise will modify the work's adaptive sampling settings to continue with
   * a lower threshold. */
  bool render_work_reschedule_on_converge(RenderWork &render_work);

  /* Reschedule adaptive sampling work when the device is mostly on idle, but not all pixels yet
   * converged.
   * If re-scheduling is not possible (adaptive sampling is happening with the final threshold, and
   * the path tracer is to finish the current pixels) then false is returned. */
  bool render_work_reschedule_on_idle(RenderWork &render_work);

  /* Reschedule work when rendering has been requested to cancel.
   *
   * Will skip all work which is not needed anymore because no more samples will be added (for
   * example, adaptive sampling filtering and convergence check will be skipped).
   * Will enable all work needed to make sure all passes are communicated to the software.
   *
   * NOTE: Should be used before passing work to `PathTrace::render_samples()`. */
  void render_work_reschedule_on_cancel(RenderWork &render_work);

  RenderWork get_render_work();

  /* Report that the path tracer started to work, after scene update and loading kernels. */
  void report_work_begin(const RenderWork &render_work);

  /* Report time (in seconds) which corresponding part of work took. */
  void report_path_trace_time(const RenderWork &render_work, double time, bool is_cancelled);
  void report_path_trace_occupancy(const RenderWork &render_work, float occupancy);
  void report_adaptive_filter_time(const RenderWork &render_work, double time, bool is_cancelled);
  void report_denoise_time(const RenderWork &render_work, double time);
  void report_display_update_time(const RenderWork &render_work, double time);
  void report_rebalance_time(const RenderWork &render_work, double time, bool balance_changed);

  /* Generate full multi-line report of the rendering process, including rendering parameters,
   * times, and so on. */
  string full_report() const;

 protected:
  /* Check whether all work has been scheduled and time limit was not exceeded.
   *
   * NOTE: Tricky bit: if the time limit was reached the done() is considered to be true, but some
   * extra work needs to be scheduled to denoise and write final result. */
  bool done() const;

  /* Update scheduling state for a newly scheduled work.
   * Takes care of things like checking whether work was ever denoised, tile was written and states
   * like that. */
  void update_state_for_render_work(const RenderWork &render_work);

  /* Returns true if any work was scheduled. */
  bool set_postprocess_render_work(RenderWork *render_work);

  /*  Set work which is to be performed after all tiles has been rendered. */
  void set_full_frame_render_work(RenderWork *render_work);

  /* Update start resolution divider based on the accumulated timing information, preserving nice
   * feeling navigation feel. */
  void update_start_resolution_divider();

  /* Calculate desired update interval in seconds based on the current timings and settings.
   * Will give an interval which provides good feeling updates during viewport navigation. */
  double guess_viewport_navigation_update_interval_in_seconds() const;

  /* Check whether denoising is active during interactive update while resolution divider is not
   * unit. */
  bool is_denoise_active_during_update() const;

  /* Heuristic which aims to give perceptually pleasant update of display interval in a way that at
   * lower samples and near the beginning of rendering, updates happen more often, but with higher
   * number of samples and later in the render, updates happen less often but device occupancy
   * goes higher. */
  double guess_display_update_interval_in_seconds() const;
  double guess_display_update_interval_in_seconds_for_num_samples(int num_rendered_samples) const;
  double guess_display_update_interval_in_seconds_for_num_samples_no_limit(
      int num_rendered_samples) const;

  /* Calculate number of samples which can be rendered within current desired update interval which
   * is calculated by `guess_update_interval_in_seconds()`. */
  int calculate_num_samples_per_update() const;

  /* Get start sample and the number of samples which are to be path traces in the current work. */
  int get_start_sample_to_path_trace() const;
  int get_num_samples_to_path_trace() const;

  /* Calculate how many samples there are to be rendered for the very first path trace after reset.
   */
  int get_num_samples_during_navigation(int resolution_divier) const;

  /* Whether adaptive sampling convergence check and filter is to happen. */
  bool work_need_adaptive_filter() const;

  /* Calculate threshold for adaptive sampling. */
  float work_adaptive_threshold() const;

  /* Check whether current work needs denoising.
   * Denoising is not needed if the denoiser is not configured, or when denoising is happening too
   * often.
   *
   * The delayed will be true when the denoiser is configured for use, but it was delayed for a
   * later sample, to reduce overhead.
   *
   * ready_to_display will be false if we may have a denoised result that is outdated due to
   * increased samples. */
  bool work_need_denoise(bool &delayed, bool &ready_to_display);

  /* Check whether current work need to update display.
   *
   * The `denoiser_delayed` is what `work_need_denoise()` returned as delayed denoiser flag. */
  bool work_need_update_display(const bool denoiser_delayed);

  /* Check whether it is time to perform rebalancing for the render work, */
  bool work_need_rebalance();

  /* Check whether timing of the given work are usable to store timings in the `first_render_time_`
   * for the resolution divider calculation. */
  bool work_is_usable_for_first_render_estimation(const RenderWork &render_work);

  /* Check whether timing report about the given work need to reset accumulated average time. */
  bool work_report_reset_average(const RenderWork &render_work);

  /* CHeck whether render time limit has been reached (or exceeded), and if so store related
   * information in the state so that rendering is considered finished, and is possible to report
   * average render time information. */
  void check_time_limit_reached();

  /* Helper class to keep track of task timing.
   *
   * Contains two parts: wall time and average. The wall time is an actual wall time of how long it
   * took to complete all tasks of a type. Is always advanced when PathTracer reports time update.
   *
   * The average time is used for scheduling purposes. It is estimated to be a time of how long it
   * takes to perform task on the final resolution. */
  class TimeWithAverage {
   public:
    inline void reset()
    {
      total_wall_time_ = 0.0;

      average_time_accumulator_ = 0.0;
      num_average_times_ = 0;
    }

    inline void add_wall(double time)
    {
      total_wall_time_ += time;
    }

    inline void add_average(double time, int num_measurements = 1)
    {
      average_time_accumulator_ += time;
      num_average_times_ += num_measurements;
    }

    inline double get_wall() const
    {
      return total_wall_time_;
    }

    inline double get_average() const
    {
      if (num_average_times_ == 0) {
        return 0;
      }
      return average_time_accumulator_ / num_average_times_;
    }

    inline void reset_average()
    {
      average_time_accumulator_ = 0.0;
      num_average_times_ = 0;
    }

   protected:
    double total_wall_time_ = 0.0;

    double average_time_accumulator_ = 0.0;
    int num_average_times_ = 0;
  };

  struct {
    int resolution_divider = 1;

    /* Number of rendered samples on top of the start sample. */
    int num_rendered_samples = 0;

    /* Point in time the latest PathTraceDisplay work has been scheduled. */
    double last_display_update_time = 0.0;
    /* Value of -1 means display was never updated. */
    int last_display_update_sample = -1;

    /* Point in time at which last rebalance has been performed. */
    double last_rebalance_time = 0.0;

    /* Number of rebalance works which has been requested to be performed.
     * The path tracer might ignore the work if there is a single device rendering. */
    int num_rebalance_requested = 0;

    /* Number of rebalance works handled which did change balance across devices. */
    int num_rebalance_changes = 0;

    bool need_rebalance_at_next_work = false;

    /* Denotes whether the latest performed rebalance work cause an actual rebalance of work across
     * devices. */
    bool last_rebalance_changed = false;

    /* Threshold for adaptive sampling which will be scheduled to work when not using progressive
     * noise floor. */
    float adaptive_sampling_threshold = 0.0f;

    bool last_work_tile_was_denoised = false;
    bool tile_result_was_written = false;
    bool postprocess_work_scheduled = false;
    bool full_frame_work_scheduled = false;
    bool full_frame_was_written = false;

    bool path_trace_finished = false;
    bool time_limit_reached = false;

    /* Time at which rendering started and finished. */
    double start_render_time = 0.0;
    double end_render_time = 0.0;

    /* Measured occupancy of the render devices measured normalized to the number of samples.
     *
     * In a way it is "trailing": when scheduling new work this occupancy is measured when the
     * previous work was rendered. */
    int occupancy_num_samples = 0;
    float occupancy = 1.0f;
  } state_;

  /* Timing of tasks which were performed at the very first render work at 100% of the
   * resolution. This timing information is used to estimate resolution divider for fats
   * navigation. */
  struct {
    double path_trace_per_sample;
    double denoise_time;
    double display_update_time;
  } first_render_time_;

  TimeWithAverage path_trace_time_;
  TimeWithAverage adaptive_filter_time_;
  TimeWithAverage denoise_time_;
  TimeWithAverage display_update_time_;
  TimeWithAverage rebalance_time_;

  /* Whether cryptomatte-related work will be scheduled. */
  bool need_schedule_cryptomatte_ = false;

  /* Whether to schedule device load rebalance works.
   * Rebalancing requires some special treatment for update intervals and such, so if it's known
   * that the rebalance will be ignored (due to single-device rendering i.e.) is better to fully
   * ignore rebalancing logic. */
  bool need_schedule_rebalance_works_ = false;

  /* Path tracing work will be scheduled for samples from within
   * [start_sample_, start_sample_ + num_samples_ - 1] range, inclusively. */
  int start_sample_ = 0;
  int num_samples_ = 0;

  /* Limit in seconds for how long path tracing is allowed to happen.
   * Zero means no limit is applied. */
  double time_limit_ = 0.0;

  /* Headless rendering without interface. */
  bool headless_;

  /* Background (offline) rendering. */
  bool background_;

  /* Pixel size is used to force lower resolution render for final pass. Useful for retina or other
   * types of hi-dpi displays. */
  int pixel_size_ = 1;

  TileManager &tile_manager_;

  BufferParams buffer_params_;
  DenoiseParams denoiser_params_;

  AdaptiveSampling adaptive_sampling_;

  /* Progressively lower adaptive sampling threshold level, keeping the image at a uniform noise
   * level. */
  bool use_progressive_noise_floor_ = false;

  /* Default value for the resolution divider which will be used when there is no render time
   * information available yet.
   * It is also what defines the upper limit of the automatically calculated resolution divider. */
  int default_start_resolution_divider_ = 1;

  /* Initial resolution divider which will be used on render scheduler reset. */
  int start_resolution_divider_ = 0;

  /* Calculate smallest resolution divider which will bring down actual rendering time below the
   * desired one. This call assumes linear dependency of render time from number of pixels
   * (quadratic dependency from the resolution divider): resolution divider of 2 brings render time
   * down by a factor of 4. */
  int calculate_resolution_divider_for_time(double desired_time, double actual_time);
};

int calculate_resolution_divider_for_resolution(int width, int height, int resolution);

int calculate_resolution_for_divider(int width, int height, int resolution_divider);

CCL_NAMESPACE_END
