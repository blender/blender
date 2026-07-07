/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Motion blur is done by accumulating scene samples over shutter time.
 * Since the number of step is discrete, quite low, and not per pixel randomized,
 * we couple this with a post processing motion blur.
 *
 * The post-fx motion blur is done in two directions, from the previous step and to the next.
 *
 * For a scene with 3 motion steps, a flat shutter curve and shutter time of 2 frame
 * centered on frame we have:
 *
 * |--------------------|--------------------|
 * -1                   0                    1  Frames
 *
 * |-------------|-------------|-------------|
 *        1             2             3         Motion steps
 *
 * |------|------|------|------|------|------|
 * 0      1      2      4      5      6      7  Time Steps
 *
 * |-------------| One motion step blurs this range.
 * -1     |     +1 Objects and geometry steps are recorded here.
 *        0 Scene is rendered here.
 *
 * Since motion step N and N+1 share one time step we reuse it to avoid an extra scene evaluation.
 *
 * Note that we have to evaluate -1 and +1 time steps before rendering so eval order is -1, +1, 0.
 * This is because all GPUBatches from the DRWCache are being free when changing a frame.
 *
 * For viewport, we only have the current and previous step data to work with. So we center the
 * blur on the current frame and extrapolate the motion.
 *
 * The Post-FX motion blur is based on:
 * "A Fast and Stable Feature-Aware Motion Blur Filter"
 * by Jean-Philippe Guertin, Morgan McGuire, Derek Nowrouzezahrai
 */

#pragma once

#include "DRW_gpu_wrapper.hh"

#include "eevee_motion_blur_shared.hh"
#include "eevee_sampling.hh"

#include "draw_pass.hh"

namespace blender::eevee {

using namespace draw;

/* -------------------------------------------------------------------- */
/** \name MotionBlur
 *
 * \{ */

using MotionBlurDataBuf = draw::UniformBuffer<MotionBlurData>;
using MotionBlurTileIndirectionBuf = draw::StorageBuffer<MotionBlurTileIndirection, true>;

/**
 * Manages time-steps evaluations and accumulation Motion blur.
 * Also handles Post process motion blur.
 */
class MotionBlurModule {
 private:
  Instance &inst_;

  /**
   * Array containing all steps (in scene time) we need to evaluate (not render).
   * Only odd steps are rendered. The even ones are evaluated for fx motion blur.
   */
  Vector<float> time_steps_;

  /** Copy of input frame and sub-frame to restore after render. */
  int initial_frame_;
  float initial_subframe_;
  /** Time of the frame we are rendering. */
  float frame_time_;
  /** Enum controlling when the shutter opens. See RenderData.motion_blur_position. */
  int shutter_position_;
  /** Time in scene frame the shutter is open. Controls the amount of blur. */
  float shutter_time_;

  /** True if motion blur is enabled as a module. */
  bool enabled_ = false;
  /** True if motion blur post-fx is enabled. */
  bool motion_blur_fx_enabled_ = false;
  /** True if last viewport redraw state was already in navigation state. */
  bool was_navigating_ = false;

  int step_id_ = 0;

  /** Velocity tiles used to guide and speedup the gather pass. */
  TextureFromPool tiles_tx_;

  gpu::Texture *input_color_tx_ = nullptr;
  gpu::Texture *output_color_tx_ = nullptr;

  PassSimple motion_blur_ps_ = {"MotionBlur"};

  MotionBlurTileIndirectionBuf tile_indirection_buf_;
  MotionBlurDataBuf data_;
  /** Dispatch size for full-screen passes. */
  int3 dispatch_flatten_size_ = int3(0);
  int3 dispatch_dilate_size_ = int3(0);
  int3 dispatch_gather_size_ = int3(0);

 public:
  MotionBlurModule(Instance &inst) : inst_(inst) {};
  ~MotionBlurModule() {};

  void init();

  /* Runs after rendering a sample. */
  void step();

  void sync();

  bool postfx_enabled() const
  {
    return motion_blur_fx_enabled_;
  }

  void render(View &view, gpu::Texture **input_tx, gpu::Texture **output_tx);

 private:
  float shutter_time_to_scene_time(float time);
};

/** \} */

}  // namespace blender::eevee
