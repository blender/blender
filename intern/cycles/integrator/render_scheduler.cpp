/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "integrator/render_scheduler.h"

#include "session/session.h"
#include "session/tile.h"
#include "util/log.h"
#include "util/math.h"
#include "util/time.h"

CCL_NAMESPACE_BEGIN

/* --------------------------------------------------------------------
 * Render scheduler.
 */

RenderScheduler::RenderScheduler(TileManager &tile_manager, const SessionParams &params)
    : headless_(params.headless),
      background_(params.background),
      pixel_size_(params.pixel_size),
      tile_manager_(tile_manager),
      default_start_resolution_divider_(params.use_resolution_divider ? pixel_size_ * 8 : 0)
{
  use_progressive_noise_floor_ = !background_;
}

void RenderScheduler::set_need_schedule_cryptomatte(bool need_schedule_cryptomatte)
{
  need_schedule_cryptomatte_ = need_schedule_cryptomatte;
}

void RenderScheduler::set_need_schedule_rebalance(bool need_schedule_rebalance)
{
  need_schedule_rebalance_works_ = need_schedule_rebalance;
}

bool RenderScheduler::is_background() const
{
  return background_;
}

void RenderScheduler::set_denoiser_params(const DenoiseParams &params)
{
  denoiser_params_ = params;
}

void RenderScheduler::set_limit_samples_per_update(const int limit_samples)
{
  limit_samples_per_update_ = limit_samples;
}

void RenderScheduler::set_adaptive_sampling(const AdaptiveSampling &adaptive_sampling)
{
  adaptive_sampling_ = adaptive_sampling;
}

bool RenderScheduler::is_adaptive_sampling_used() const
{
  return adaptive_sampling_.use;
}

void RenderScheduler::set_start_sample(int start_sample)
{
  start_sample_ = start_sample;
}

int RenderScheduler::get_start_sample() const
{
  return start_sample_;
}

void RenderScheduler::set_num_samples(int num_samples)
{
  num_samples_ = num_samples;
}

int RenderScheduler::get_num_samples() const
{
  return num_samples_;
}

void RenderScheduler::set_sample_offset(int sample_offset)
{
  sample_offset_ = sample_offset;
}

int RenderScheduler::get_sample_offset() const
{
  return sample_offset_;
}

void RenderScheduler::set_time_limit(double time_limit)
{
  time_limit_ = time_limit;
}

double RenderScheduler::get_time_limit() const
{
  return time_limit_;
}

int RenderScheduler::get_rendered_sample() const
{
  DCHECK_GT(get_num_rendered_samples(), 0);

  return start_sample_ + get_num_rendered_samples() - 1 - sample_offset_;
}

int RenderScheduler::get_num_rendered_samples() const
{
  return state_.num_rendered_samples;
}

void RenderScheduler::reset(const BufferParams &buffer_params, int num_samples, int sample_offset)
{
  buffer_params_ = buffer_params;

  update_start_resolution_divider();

  set_num_samples(num_samples);
  set_start_sample(sample_offset);
  set_sample_offset(sample_offset);

  /* In background mode never do lower resolution render preview, as it is not really supported
   * by the software. */
  if (background_ || start_resolution_divider_ == 0) {
    state_.resolution_divider = 1;
  }
  else {
    /* NOTE: Divide by 2 because of the way how scheduling works: it advances resolution divider
     * first and then initialized render work. */
    state_.resolution_divider = start_resolution_divider_ * 2;
  }

  state_.num_rendered_samples = 0;
  state_.last_display_update_time = 0.0;
  state_.last_display_update_sample = -1;

  state_.last_rebalance_time = 0.0;
  state_.num_rebalance_requested = 0;
  state_.num_rebalance_changes = 0;
  state_.last_rebalance_changed = false;
  state_.need_rebalance_at_next_work = false;

  /* TODO(sergey): Choose better initial value. */
  /* NOTE: The adaptive sampling settings might not be available here yet. */
  state_.adaptive_sampling_threshold = 0.4f;

  state_.last_work_tile_was_denoised = false;
  state_.tile_result_was_written = false;
  state_.postprocess_work_scheduled = false;
  state_.full_frame_work_scheduled = false;
  state_.full_frame_was_written = false;

  state_.path_trace_finished = false;

  state_.start_render_time = 0.0;
  state_.end_render_time = 0.0;
  state_.time_limit_reached = false;

  state_.occupancy_num_samples = 0;
  state_.occupancy = 1.0f;

  first_render_time_.path_trace_per_sample = 0.0;
  first_render_time_.denoise_time = 0.0;
  first_render_time_.display_update_time = 0.0;

  path_trace_time_.reset();
  denoise_time_.reset();
  adaptive_filter_time_.reset();
  display_update_time_.reset();
  rebalance_time_.reset();
}

void RenderScheduler::reset_for_next_tile()
{
  reset(buffer_params_, num_samples_, sample_offset_);
}

bool RenderScheduler::render_work_reschedule_on_converge(RenderWork &render_work)
{
  /* Move to the next resolution divider. Assume adaptive filtering is not needed during
   * navigation. */
  if (state_.resolution_divider != pixel_size_) {
    return false;
  }

  if (render_work_reschedule_on_idle(render_work)) {
    return true;
  }

  state_.path_trace_finished = true;

  bool denoiser_delayed, denoiser_ready_to_display;
  render_work.tile.denoise = work_need_denoise(denoiser_delayed, denoiser_ready_to_display);

  render_work.display.update = work_need_update_display(denoiser_delayed);
  render_work.display.use_denoised_result = denoiser_ready_to_display;

  return false;
}

bool RenderScheduler::render_work_reschedule_on_idle(RenderWork &render_work)
{
  if (!use_progressive_noise_floor_) {
    return false;
  }

  /* Move to the next resolution divider. Assume adaptive filtering is not needed during
   * navigation. */
  if (state_.resolution_divider != pixel_size_) {
    return false;
  }

  if (adaptive_sampling_.use) {
    if (state_.adaptive_sampling_threshold > adaptive_sampling_.threshold) {
      state_.adaptive_sampling_threshold = max(state_.adaptive_sampling_threshold / 2,
                                               adaptive_sampling_.threshold);

      render_work.adaptive_sampling.threshold = state_.adaptive_sampling_threshold;
      render_work.adaptive_sampling.reset = true;

      return true;
    }
  }

  return false;
}

void RenderScheduler::render_work_reschedule_on_cancel(RenderWork &render_work)
{
  VLOG_WORK << "Schedule work for cancel.";

  /* Un-schedule samples: they will not be rendered and should not be counted. */
  state_.num_rendered_samples -= render_work.path_trace.num_samples;

  const bool has_rendered_samples = get_num_rendered_samples() != 0;

  /* Reset all fields of the previous work, canceling things like adaptive sampling filtering and
   * denoising.
   * However, need to preserve write requests, since those will not be possible to recover and
   * writes are only to happen once. */
  const bool tile_write = render_work.tile.write;
  const bool full_write = render_work.full.write;

  render_work = RenderWork();

  render_work.tile.write = tile_write;
  render_work.full.write = full_write;

  /* Do not write tile if it has zero samples in it, treat it similarly to all other tiles which
   * got canceled. */
  if (!state_.tile_result_was_written && has_rendered_samples) {
    render_work.tile.write = true;
  }

  if (!state_.full_frame_was_written) {
    render_work.full.write = true;
  }

  /* Update current tile, but only if any sample was rendered.
   * Allows to have latest state of tile visible while full buffer is being processed.
   *
   * Note that if there are no samples in the current tile its render buffer might have pixels
   * remained from previous state.
   *
   * If the full result was written, then there is no way any updates were made to the render
   * buffers. And the buffers might have been freed from the device, so display update is not
   * possible. */
  if (has_rendered_samples && !state_.full_frame_was_written) {
    render_work.display.update = true;
  }
}

bool RenderScheduler::done() const
{
  if (state_.resolution_divider != pixel_size_) {
    return false;
  }

  if (state_.path_trace_finished || state_.time_limit_reached) {
    return true;
  }

  return get_num_rendered_samples() >= num_samples_;
}

RenderWork RenderScheduler::get_render_work()
{
  check_time_limit_reached();

  const double time_now = time_dt();

  if (done()) {
    RenderWork render_work;
    render_work.resolution_divider = state_.resolution_divider;

    if (!set_postprocess_render_work(&render_work)) {
      set_full_frame_render_work(&render_work);
    }

    if (!render_work) {
      state_.end_render_time = time_now;
    }

    update_state_for_render_work(render_work);

    return render_work;
  }

  RenderWork render_work;

  if (state_.resolution_divider != pixel_size_) {
    state_.resolution_divider = max(state_.resolution_divider / 2, pixel_size_);
    state_.num_rendered_samples = 0;
    state_.last_display_update_sample = -1;
  }

  render_work.resolution_divider = state_.resolution_divider;

  render_work.path_trace.start_sample = get_start_sample_to_path_trace();
  render_work.path_trace.num_samples = get_num_samples_to_path_trace();
  render_work.path_trace.sample_offset = get_sample_offset();

  render_work.init_render_buffers = (render_work.path_trace.start_sample == get_start_sample());

  /* NOTE: Rebalance scheduler requires current number of samples to not be advanced forward. */
  render_work.rebalance = work_need_rebalance();

  /* NOTE: Advance number of samples now, so that filter and denoising check can see that all the
   * samples are rendered. */
  state_.num_rendered_samples += render_work.path_trace.num_samples;

  render_work.adaptive_sampling.filter = work_need_adaptive_filter();
  render_work.adaptive_sampling.threshold = work_adaptive_threshold();
  render_work.adaptive_sampling.reset = false;

  bool denoiser_delayed, denoiser_ready_to_display;
  render_work.tile.denoise = work_need_denoise(denoiser_delayed, denoiser_ready_to_display);

  render_work.tile.write = done();

  render_work.display.update = work_need_update_display(denoiser_delayed);
  render_work.display.use_denoised_result = denoiser_ready_to_display;

  if (done()) {
    set_postprocess_render_work(&render_work);
  }

  update_state_for_render_work(render_work);

  return render_work;
}

void RenderScheduler::update_state_for_render_work(const RenderWork &render_work)
{
  const double time_now = time_dt();

  if (render_work.rebalance) {
    state_.last_rebalance_time = time_now;
    ++state_.num_rebalance_requested;
  }

  /* A fallback display update time, for the case there is an error of display update, or when
   * there is no display at all. */
  if (render_work.display.update) {
    state_.last_display_update_time = time_now;
    state_.last_display_update_sample = state_.num_rendered_samples;
  }

  state_.last_work_tile_was_denoised = render_work.tile.denoise;
  state_.tile_result_was_written |= render_work.tile.write;
  state_.full_frame_was_written |= render_work.full.write;
}

bool RenderScheduler::set_postprocess_render_work(RenderWork *render_work)
{
  if (state_.postprocess_work_scheduled) {
    return false;
  }
  state_.postprocess_work_scheduled = true;

  bool any_scheduled = false;

  if (need_schedule_cryptomatte_) {
    render_work->cryptomatte.postprocess = true;
    any_scheduled = true;
  }

  if (denoiser_params_.use && !state_.last_work_tile_was_denoised) {
    render_work->tile.denoise = !tile_manager_.has_multiple_tiles();
    any_scheduled = true;
  }

  if (!state_.tile_result_was_written) {
    render_work->tile.write = true;
    any_scheduled = true;
  }

  if (any_scheduled) {
    render_work->display.update = true;
  }

  return any_scheduled;
}

void RenderScheduler::set_full_frame_render_work(RenderWork *render_work)
{
  if (state_.full_frame_work_scheduled) {
    return;
  }

  if (!tile_manager_.has_multiple_tiles()) {
    /* There is only single tile, so all work has been performed already. */
    return;
  }

  if (!tile_manager_.done()) {
    /* There are still tiles to be rendered. */
    return;
  }

  if (state_.full_frame_was_written) {
    return;
  }

  state_.full_frame_work_scheduled = true;

  render_work->full.write = true;
}

/* Knowing time which it took to complete a task at the current resolution divider approximate how
 * long it would have taken to complete it at a final resolution. */
static double approximate_final_time(const RenderWork &render_work, double time)
{
  if (render_work.resolution_divider == 1) {
    return time;
  }

  const double resolution_divider_sq = render_work.resolution_divider *
                                       render_work.resolution_divider;
  return time * resolution_divider_sq;
}

void RenderScheduler::report_work_begin(const RenderWork &render_work)
{
  /* Start counting render time when rendering samples at their final resolution.
   *
   * NOTE: The work might have the path trace part be all zero: this happens when a post-processing
   * work is scheduled after the path tracing. Checking for just a start sample doesn't work here
   * because it might be wrongly 0. Check for whether path tracing is actually happening as it is
   * expected to happen in the first work. */
  if (render_work.resolution_divider == pixel_size_ && render_work.path_trace.num_samples != 0 &&
      render_work.path_trace.start_sample == get_start_sample()) {
    state_.start_render_time = time_dt();
  }
}

void RenderScheduler::report_path_trace_time(const RenderWork &render_work,
                                             double time,
                                             bool is_cancelled)
{
  path_trace_time_.add_wall(time);

  if (is_cancelled) {
    return;
  }

  const double final_time_approx = approximate_final_time(render_work, time);

  if (work_is_usable_for_first_render_estimation(render_work)) {
    first_render_time_.path_trace_per_sample = final_time_approx /
                                               render_work.path_trace.num_samples;
  }

  if (work_report_reset_average(render_work)) {
    path_trace_time_.reset_average();
  }

  path_trace_time_.add_average(final_time_approx, render_work.path_trace.num_samples);

  VLOG_WORK << "Average path tracing time: " << path_trace_time_.get_average() << " seconds.";
}

void RenderScheduler::report_path_trace_occupancy(const RenderWork &render_work, float occupancy)
{
  state_.occupancy_num_samples = render_work.path_trace.num_samples;
  state_.occupancy = occupancy;
  VLOG_WORK << "Measured path tracing occupancy: " << occupancy;
}

void RenderScheduler::report_adaptive_filter_time(const RenderWork &render_work,
                                                  double time,
                                                  bool is_cancelled)
{
  adaptive_filter_time_.add_wall(time);

  if (is_cancelled) {
    return;
  }

  const double final_time_approx = approximate_final_time(render_work, time);

  if (work_report_reset_average(render_work)) {
    adaptive_filter_time_.reset_average();
  }

  adaptive_filter_time_.add_average(final_time_approx, render_work.path_trace.num_samples);

  VLOG_WORK << "Average adaptive sampling filter  time: " << adaptive_filter_time_.get_average()
            << " seconds.";
}

void RenderScheduler::report_denoise_time(const RenderWork &render_work, double time)
{
  denoise_time_.add_wall(time);

  const double final_time_approx = approximate_final_time(render_work, time);

  if (work_is_usable_for_first_render_estimation(render_work)) {
    first_render_time_.denoise_time = final_time_approx;
  }

  if (work_report_reset_average(render_work)) {
    denoise_time_.reset_average();
  }

  denoise_time_.add_average(final_time_approx);

  VLOG_WORK << "Average denoising time: " << denoise_time_.get_average() << " seconds.";
}

void RenderScheduler::report_display_update_time(const RenderWork &render_work, double time)
{
  display_update_time_.add_wall(time);

  const double final_time_approx = approximate_final_time(render_work, time);

  if (work_is_usable_for_first_render_estimation(render_work)) {
    first_render_time_.display_update_time = final_time_approx;
  }

  if (work_report_reset_average(render_work)) {
    display_update_time_.reset_average();
  }

  display_update_time_.add_average(final_time_approx);

  VLOG_WORK << "Average display update time: " << display_update_time_.get_average()
            << " seconds.";

  /* Move the display update moment further in time, so that logic which checks when last update
   * did happen have more reliable point in time (without path tracing and denoising parts of the
   * render work). */
  state_.last_display_update_time = time_dt();
}

void RenderScheduler::report_rebalance_time(const RenderWork &render_work,
                                            double time,
                                            bool balance_changed)
{
  rebalance_time_.add_wall(time);

  if (work_report_reset_average(render_work)) {
    rebalance_time_.reset_average();
  }

  rebalance_time_.add_average(time);

  if (balance_changed) {
    ++state_.num_rebalance_changes;
  }

  state_.last_rebalance_changed = balance_changed;

  VLOG_WORK << "Average rebalance time: " << rebalance_time_.get_average() << " seconds.";
}

string RenderScheduler::full_report() const
{
  const double render_wall_time = state_.end_render_time - state_.start_render_time;
  const int num_rendered_samples = get_num_rendered_samples();

  string result = "\nRender Scheduler Summary\n\n";

  {
    string mode;
    if (headless_) {
      mode = "Headless";
    }
    else if (background_) {
      mode = "Background";
    }
    else {
      mode = "Interactive";
    }
    result += "Mode: " + mode + "\n";
  }

  result += "Resolution: " + to_string(buffer_params_.width) + "x" +
            to_string(buffer_params_.height) + "\n";

  result += "\nAdaptive sampling:\n";
  result += "  Use: " + string_from_bool(adaptive_sampling_.use) + "\n";
  if (adaptive_sampling_.use) {
    result += "  Step: " + to_string(adaptive_sampling_.adaptive_step) + "\n";
    result += "  Min Samples: " + to_string(adaptive_sampling_.min_samples) + "\n";
    result += "  Threshold: " + to_string(adaptive_sampling_.threshold) + "\n";
  }

  result += "\nDenoiser:\n";
  result += "  Use: " + string_from_bool(denoiser_params_.use) + "\n";
  if (denoiser_params_.use) {
    result += "  Type: " + string(denoiserTypeToHumanReadable(denoiser_params_.type)) + "\n";
    result += "  Start Sample: " + to_string(denoiser_params_.start_sample) + "\n";

    string passes = "Color";
    if (denoiser_params_.use_pass_albedo) {
      passes += ", Albedo";
    }
    if (denoiser_params_.use_pass_normal) {
      passes += ", Normal";
    }

    result += "  Passes: " + passes + "\n";
  }

  if (state_.num_rebalance_requested) {
    result += "\nRebalancer:\n";
    result += "  Number of requested rebalances: " + to_string(state_.num_rebalance_requested) +
              "\n";
    result += "  Number of performed rebalances: " + to_string(state_.num_rebalance_changes) +
              "\n";
  }

  result += "\nTime (in seconds):\n";
  result += string_printf("  %20s %20s %20s\n", "", "Wall", "Average");
  result += string_printf("  %20s %20f %20f\n",
                          "Path Tracing",
                          path_trace_time_.get_wall(),
                          path_trace_time_.get_average());

  if (adaptive_sampling_.use) {
    result += string_printf("  %20s %20f %20f\n",
                            "Adaptive Filter",
                            adaptive_filter_time_.get_wall(),
                            adaptive_filter_time_.get_average());
  }

  if (denoiser_params_.use) {
    result += string_printf(
        "  %20s %20f %20f\n", "Denoiser", denoise_time_.get_wall(), denoise_time_.get_average());
  }

  result += string_printf("  %20s %20f %20f\n",
                          "Display Update",
                          display_update_time_.get_wall(),
                          display_update_time_.get_average());

  if (state_.num_rebalance_requested) {
    result += string_printf("  %20s %20f %20f\n",
                            "Rebalance",
                            rebalance_time_.get_wall(),
                            rebalance_time_.get_average());
  }

  const double total_time = path_trace_time_.get_wall() + adaptive_filter_time_.get_wall() +
                            denoise_time_.get_wall() + display_update_time_.get_wall();
  result += "\n  Total: " + to_string(total_time) + "\n";

  result += string_printf(
      "\nRendered %d samples in %f seconds\n", num_rendered_samples, render_wall_time);

  /* When adaptive sampling is used the average time becomes meaningless, because different samples
   * will likely render different number of pixels. */
  if (!adaptive_sampling_.use) {
    result += string_printf("Average time per sample: %f seconds\n",
                            render_wall_time / num_rendered_samples);
  }

  return result;
}

double RenderScheduler::guess_display_update_interval_in_seconds() const
{
  return guess_display_update_interval_in_seconds_for_num_samples(state_.num_rendered_samples);
}

double RenderScheduler::guess_display_update_interval_in_seconds_for_num_samples(
    int num_rendered_samples) const
{
  double update_interval = guess_display_update_interval_in_seconds_for_num_samples_no_limit(
      num_rendered_samples);

  if (time_limit_ != 0.0 && state_.start_render_time != 0.0) {
    const double remaining_render_time = max(0.0,
                                             time_limit_ - (time_dt() - state_.start_render_time));

    update_interval = min(update_interval, remaining_render_time);
  }

  return update_interval;
}

/* TODO(sergey): This is just a quick implementation, exact values might need to be tweaked based
 * on a more careful experiments with viewport rendering. */
double RenderScheduler::guess_display_update_interval_in_seconds_for_num_samples_no_limit(
    int num_rendered_samples) const
{
  /* TODO(sergey): Need a decision on whether this should be using number of samples rendered
   * within the current render session, or use absolute number of samples with the start sample
   * taken into account. It will depend on whether the start sample offset clears the render
   * buffer. */

  if (state_.need_rebalance_at_next_work) {
    return 0.1;
  }
  if (state_.last_rebalance_changed) {
    return 0.2;
  }

  if (headless_) {
    /* In headless mode do rare updates, so that the device occupancy is high, but there are still
     * progress messages printed to the logs. */
    return 30.0;
  }

  if (background_) {
    if (num_rendered_samples < 32) {
      return 1.0;
    }
    return 2.0;
  }

  /* Render time and number of samples rendered are used to figure out the display update interval.
   *  Render time is used to allow for fast display updates in the first few seconds of rendering
   *  on fast devices. Number of samples rendered is used to allow for potentially quicker display
   *  updates on slow devices during the first few samples. */
  const double render_time = path_trace_time_.get_wall();
  if (render_time < 1) {
    return 0.1;
  }
  if (render_time < 2) {
    return 0.25;
  }
  if (render_time < 4) {
    return 0.5;
  }
  if (render_time < 8 || num_rendered_samples < 32) {
    return 1.0;
  }
  return 2.0;
}

int RenderScheduler::calculate_num_samples_per_update() const
{
  const double time_per_sample_average = path_trace_time_.get_average();
  /* Fall back to 1 sample if we have not recorded a time yet. */
  if (time_per_sample_average == 0.0) {
    return 1;
  }

  const double num_samples_in_second = pixel_size_ * pixel_size_ / time_per_sample_average;

  const double update_interval_in_seconds = guess_display_update_interval_in_seconds();

  int num_samples_per_update = max(int(num_samples_in_second * update_interval_in_seconds), 1);

  if (limit_samples_per_update_) {
    num_samples_per_update = min(limit_samples_per_update_, num_samples_per_update);
  }

  return num_samples_per_update;
}

int RenderScheduler::get_start_sample_to_path_trace() const
{
  return start_sample_ + state_.num_rendered_samples;
}

/* Round number of samples to the closest power of two.
 * Rounding might happen to higher or lower value depending on which one is closer. Such behavior
 * allows to have number of samples to be power of two without diverging from the planned number of
 * samples too much. */
static inline uint round_num_samples_to_power_of_2(const uint num_samples)
{
  if (num_samples == 1) {
    return 1;
  }

  if (is_power_of_two(num_samples)) {
    return num_samples;
  }

  const uint num_samples_up = next_power_of_two(num_samples);
  const uint num_samples_down = num_samples_up - (num_samples_up >> 1);

  const uint delta_up = num_samples_up - num_samples;
  const uint delta_down = num_samples - num_samples_down;

  if (delta_up <= delta_down) {
    return num_samples_up;
  }

  return num_samples_down;
}

int RenderScheduler::get_num_samples_to_path_trace() const
{
  if (state_.resolution_divider != pixel_size_) {
    return get_num_samples_during_navigation(state_.resolution_divider);
  }

  /* Always start full resolution render  with a single sample. Gives more instant feedback to
   * artists, and allows to gather information for a subsequent path tracing works. Do it in the
   * headless mode as well, to give some estimate of how long samples are taking. */
  if (state_.num_rendered_samples == 0) {
    return 1;
  }

  int num_samples_per_update = calculate_num_samples_per_update();
  const int path_trace_start_sample = get_start_sample_to_path_trace();

  /* Round number of samples to a power of two, so that division of path states into tiles goes in
   * a more integer manner.
   * This might make it so updates happens more rarely due to rounding up. In the test scenes this
   * is not huge deal because it is not seen that more than 8 samples can be rendered between
   * updates. If that becomes a problem we can add some extra rules like never allow to round up
   * more than N samples. */
  const int num_samples_pot = round_num_samples_to_power_of_2(num_samples_per_update);

  const int max_num_samples_to_render = start_sample_ + num_samples_ - path_trace_start_sample;

  int num_samples_to_render = min(num_samples_pot, max_num_samples_to_render);

  /* When enough statistics is available and doing an offline rendering prefer to keep device
   * occupied. */
  if (state_.occupancy_num_samples && (background_ || headless_)) {
    /* Keep occupancy at about 0.5 (this is more of an empirical figure which seems to match scenes
     * with good performance without forcing occupancy to be higher). */
    int num_samples_to_occupy = state_.occupancy_num_samples;
    if (state_.occupancy < 0.5f) {
      num_samples_to_occupy = lround(state_.occupancy_num_samples * 0.7f / state_.occupancy);
    }

    /* When time limit is used clamp the calculated number of samples to keep occupancy.
     * This is because time limit causes the last render iteration to happen with less number of
     * samples, which conflicts with the occupancy (lower number of samples causes lower
     * occupancy, also the calculation is based on number of previously rendered samples).
     *
     * When time limit is not used the number of samples per render iteration is either increasing
     * or stays the same, so there is no need to clamp number of samples calculated for occupancy.
     */
    if (time_limit_ != 0.0 && state_.start_render_time != 0.0) {
      const double remaining_render_time = max(
          0.0, time_limit_ - (time_dt() - state_.start_render_time));
      const double time_per_sample_average = path_trace_time_.get_average();
      const double predicted_render_time = num_samples_to_occupy * time_per_sample_average;

      if (predicted_render_time > remaining_render_time) {
        num_samples_to_occupy = lround(num_samples_to_occupy *
                                       (remaining_render_time / predicted_render_time));
      }
    }

    num_samples_to_render = max(num_samples_to_render,
                                min(num_samples_to_occupy, max_num_samples_to_render));
  }

  /* If adaptive sampling is not use, render as many samples per update as possible, keeping the
   * device fully occupied, without much overhead of display updates. */
  if (!adaptive_sampling_.use) {
    return num_samples_to_render;
  }

  /* TODO(sergey): Add extra "clamping" here so that none of the filtering points is missing. This
   * is to ensure that the final render is pixel-matched regardless of how many samples per second
   * compute device can do. */

  return adaptive_sampling_.align_samples(path_trace_start_sample - sample_offset_,
                                          num_samples_to_render);
}

int RenderScheduler::get_num_samples_during_navigation(int resolution_divider) const
{
  /* Special trick for fast navigation: schedule multiple samples during fast navigation
   * (which will prefer to use lower resolution to keep up with refresh rate). This gives more
   * usable visual feedback for artists. */

  if (is_denoise_active_during_update()) {
    /* When denoising is used during navigation prefer using a higher resolution with less samples
     * (scheduling less samples here will make it so the resolution_divider calculation will use a
     * lower value for the divider). This is because both OpenImageDenoiser and OptiX denoiser
     * give visually better results on a higher resolution image with less samples. */
    return 1;
  }

  /* Schedule samples equal to the resolution divider up to a maximum of 4.
   * The idea is to have enough information on the screen by increasing the sample count as the
   * resolution is decreased. */
  /* NOTE: Changing this formula will change the formula in
   * `RenderScheduler::calculate_resolution_divider_for_time()`. */
  return min(max(1, resolution_divider / pixel_size_), 4);
}

bool RenderScheduler::work_need_adaptive_filter() const
{
  return adaptive_sampling_.need_filter(get_rendered_sample());
}

float RenderScheduler::work_adaptive_threshold() const
{
  if (!use_progressive_noise_floor_) {
    return adaptive_sampling_.threshold;
  }

  return max(state_.adaptive_sampling_threshold, adaptive_sampling_.threshold);
}

bool RenderScheduler::work_need_denoise(bool &delayed, bool &ready_to_display)
{
  delayed = false;
  ready_to_display = true;

  if (!denoiser_params_.use) {
    /* Denoising is disabled, no need to scheduler work for it. */
    return false;
  }

  /* When multiple tiles are used the full frame will be denoised.
   * Avoid per-tile denoising to save up render time. */
  if (tile_manager_.has_multiple_tiles()) {
    return false;
  }

  if (done()) {
    /* Always denoise at the last sample. */
    return true;
  }

  if (background_) {
    /* Background render, only denoise when rendering the last sample. */
    /* TODO(sergey): Follow similar logic to viewport, giving an overview of how final denoised
     * image looks like even for the background rendering. */
    return false;
  }

  /* Viewport render. */

  /* Navigation might render multiple samples at a lower resolution. Those are not to be counted as
   * final samples. */
  const int num_samples_finished = state_.resolution_divider == pixel_size_ ?
                                       state_.num_rendered_samples :
                                       1;

  /* Immediately denoise when we reach the start sample or last sample. */
  if (num_samples_finished == denoiser_params_.start_sample ||
      num_samples_finished == num_samples_) {
    return true;
  }

  /* Do not denoise until the sample at which denoising should start is reached. */
  if (num_samples_finished < denoiser_params_.start_sample) {
    ready_to_display = false;
    return false;
  }

  /* Avoid excessive denoising in viewport after reaching a certain sample count and render time.
   */
  /* TODO(sergey): Consider making time interval and sample configurable. */
  delayed = (path_trace_time_.get_wall() > 4 && num_samples_finished >= 20 &&
             (time_dt() - state_.last_display_update_time) < 1.0);

  return !delayed;
}

bool RenderScheduler::work_need_update_display(const bool denoiser_delayed)
{
  if (headless_) {
    /* Force disable display update in headless mode. There will be nothing to display the
     * in-progress result. */
    return false;
  }

  if (denoiser_delayed) {
    /* If denoiser has been delayed the display can not be updated as it will not contain
     * up-to-date state of the render result. */
    return false;
  }

  if (!adaptive_sampling_.use) {
    /* When adaptive sampling is not used the work is scheduled in a way that they keep render
     * device busy for long enough, so that the display update can happen right after the
     * rendering. */
    return true;
  }

  if (done() || state_.last_display_update_sample == -1) {
    /* Make sure an initial and final results of adaptive sampling is communicated ot the display.
     */
    return true;
  }

  /* For the development purposes of adaptive sampling it might be very useful to see all updates
   * of active pixels after convergence check. However, it would cause a slowdown for regular usage
   * users. Possibly, make it a debug panel option to allow rapid update to ease development
   * without need to re-compiled. */
  // if (work_need_adaptive_filter()) {
  //   return true;
  // }

  /* When adaptive sampling is used, its possible that only handful of samples of a very simple
   * scene will be scheduled to a powerful device (in order to not "miss" any of filtering points).
   * We take care of skipping updates here based on when previous display update did happen. */
  const double update_interval = guess_display_update_interval_in_seconds_for_num_samples(
      state_.last_display_update_sample);
  return (time_dt() - state_.last_display_update_time) > update_interval;
}

bool RenderScheduler::work_need_rebalance()
{
  /* This is the minimum time, as the rebalancing can not happen more often than the path trace
   * work. */
  static const double kRebalanceIntervalInSeconds = 1;

  if (!need_schedule_rebalance_works_) {
    return false;
  }

  if (state_.resolution_divider != pixel_size_) {
    /* Don't rebalance at a non-final resolution divider. Some reasons for this:
     *  - It will introduce unnecessary during navigation.
     *  - Per-render device timing information is not very reliable yet. */
    return false;
  }

  if (state_.num_rendered_samples == 0) {
    state_.need_rebalance_at_next_work = true;
    return false;
  }

  if (state_.need_rebalance_at_next_work) {
    state_.need_rebalance_at_next_work = false;
    return true;
  }

  if (state_.last_rebalance_changed) {
    return true;
  }

  return (time_dt() - state_.last_rebalance_time) > kRebalanceIntervalInSeconds;
}

void RenderScheduler::update_start_resolution_divider()
{
  if (default_start_resolution_divider_ == 0) {
    return;
  }

  if (start_resolution_divider_ == 0) {
    /* Resolution divider has never been calculated before: use default resolution, so that we have
     * somewhat good initial behavior, giving a chance to collect real numbers. */
    start_resolution_divider_ = default_start_resolution_divider_;
    VLOG_WORK << "Initial resolution divider is " << start_resolution_divider_;
    return;
  }

  if (first_render_time_.path_trace_per_sample == 0.0) {
    /* Not enough information to calculate better resolution, keep the existing one. */
    return;
  }

  const double desired_update_interval_in_seconds =
      guess_viewport_navigation_update_interval_in_seconds();

  const double actual_time_per_update = first_render_time_.path_trace_per_sample +
                                        first_render_time_.denoise_time +
                                        first_render_time_.display_update_time;

  /* Allow some percent of tolerance, so that if the render time is close enough to the higher
   * resolution we prefer to use it instead of going way lower resolution and time way below the
   * desired one. */
  const int resolution_divider_for_update = calculate_resolution_divider_for_time(
      desired_update_interval_in_seconds * 1.4, actual_time_per_update);

  /* TODO(sergey): Need to add hysteresis to avoid resolution divider bouncing around when actual
   * render time is somewhere on a boundary between two resolutions. */

  /* Don't let resolution drop below the desired one. It's better to be slow than provide an
   * unreadable viewport render. */
  start_resolution_divider_ = min(resolution_divider_for_update,
                                  default_start_resolution_divider_);

  VLOG_WORK << "Calculated resolution divider is " << start_resolution_divider_;
}

double RenderScheduler::guess_viewport_navigation_update_interval_in_seconds() const
{
  if (is_denoise_active_during_update()) {
    /* Use lower value than the non-denoised case to allow having more pixels to reconstruct the
     * image from. With the faster updates and extra compute required the resolution becomes too
     * low to give usable feedback. */
    /* NOTE: Based on performance of OpenImageDenoiser on CPU. For OptiX denoiser or other denoiser
     * on GPU the value might need to become lower for faster navigation. */
    return 1.0 / 12.0;
  }

  /* For the best match with the Blender's viewport the refresh ratio should be 60fps. This will
   * avoid "jelly" effects. However, on a non-trivial scenes this can only be achieved with high
   * values of the resolution divider which does not give very pleasant updates during navigation.
   * Choose less frequent updates to allow more noise-free and higher resolution updates. */

  /* TODO(sergey): Can look into heuristic which will allow to have 60fps if the resolution divider
   * is not too high. Alternatively, synchronize Blender's overlays updates to Cycles updates. */

  return 1.0 / 30.0;
}

bool RenderScheduler::is_denoise_active_during_update() const
{
  if (!denoiser_params_.use) {
    return false;
  }

  if (denoiser_params_.start_sample > 1) {
    return false;
  }

  return true;
}

bool RenderScheduler::work_is_usable_for_first_render_estimation(const RenderWork &render_work)
{
  return render_work.resolution_divider == pixel_size_ &&
         render_work.path_trace.start_sample == start_sample_;
}

bool RenderScheduler::work_report_reset_average(const RenderWork &render_work)
{
  /* When rendering at a non-final resolution divider time average is not very useful because it
   * will either bias average down (due to lower render times on the smaller images) or will give
   * incorrect result when trying to estimate time which would have spent on the final resolution.
   *
   * So we only accumulate average for the latest resolution divider which was rendered. */
  return render_work.resolution_divider != pixel_size_;
}

void RenderScheduler::check_time_limit_reached()
{
  if (time_limit_ == 0.0) {
    /* No limit is enforced. */
    return;
  }

  if (state_.start_render_time == 0.0) {
    /* Rendering did not start yet. */
    return;
  }

  const double current_time = time_dt();

  if (current_time - state_.start_render_time < time_limit_) {
    /* Time limit is not reached yet. */
    return;
  }

  state_.time_limit_reached = true;
  state_.end_render_time = current_time;
}

/* --------------------------------------------------------------------
 * Utility functions.
 */

int RenderScheduler::calculate_resolution_divider_for_time(double desired_time, double actual_time)
{
  const double ratio_between_times = actual_time / desired_time;

  /* We can pass `ratio_between_times` to `get_num_samples_during_navigation()` to get our
   * navigation samples because the equation for calculating the resolution divider is as follows:
   * `actual_time / desired_time = sqr(resolution_divider) / sample_count`.
   * While `resolution_divider` is less than or equal to 4, `resolution_divider = sample_count`
   * (This relationship is determined in `get_num_samples_during_navigation()`). With some
   * substitution we end up with `actual_time / desired_time = resolution_divider` while the
   * resolution divider is less than or equal to 4. Once the resolution divider increases above 4,
   * the relationship of `actual_time / desired_time = resolution_divider` is no longer true,
   * however the sample count retrieved from `get_num_samples_during_navigation()` is still
   * accurate if we continue using this assumption. It should be noted that the interaction between
   * `pixel_size`, sample count, and resolution divider are automatically accounted for and that's
   * why `pixel_size` isn't included in any of the equations. */
  const int navigation_samples = get_num_samples_during_navigation(
      ceil_to_int(ratio_between_times));

  return ceil_to_int(sqrt(navigation_samples * ratio_between_times));
}

int calculate_resolution_divider_for_resolution(int width, int height, int resolution)
{
  if (resolution == INT_MAX) {
    return 1;
  }

  int resolution_divider = 1;
  while (width * height > resolution * resolution) {
    width = max(1, width / 2);
    height = max(1, height / 2);

    resolution_divider <<= 1;
  }

  return resolution_divider;
}

int calculate_resolution_for_divider(int width, int height, int resolution_divider)
{
  const int pixel_area = width * height;
  const int resolution = lround(sqrt(pixel_area));

  return resolution / resolution_divider;
}

CCL_NAMESPACE_END
