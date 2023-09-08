/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

// #include "BLI_map.hh"
#include "DEG_depsgraph_query.h"

#include "eevee_instance.hh"
#include "eevee_motion_blur.hh"
// #include "eevee_sampling.hh"
// #include "eevee_shader_shared.hh"
// #include "eevee_velocity.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name MotionBlurModule
 *
 * \{ */

void MotionBlurModule::init()
{
  const Scene *scene = inst_.scene;

  enabled_ = (scene->eevee.flag & SCE_EEVEE_MOTION_BLUR_ENABLED) != 0;

  if (!enabled_) {
    motion_blur_fx_enabled_ = false;
    return;
  }

  /* Take into account the steps needed for fx motion blur. */
  int steps_count = max_ii(1, scene->eevee.motion_blur_steps) * 2 + 1;

  time_steps_.resize(steps_count);

  initial_frame_ = scene->r.cfra;
  initial_subframe_ = scene->r.subframe;
  frame_time_ = initial_frame_ + initial_subframe_;
  shutter_position_ = scene->eevee.motion_blur_position;
  shutter_time_ = scene->eevee.motion_blur_shutter;

  data_.depth_scale = scene->eevee.motion_blur_depth_scale;
  motion_blur_fx_enabled_ = true; /* TODO(fclem): UI option. */

  /* Viewport stops here. We only do Post-FX motion blur. */
  if (inst_.is_viewport()) {
    enabled_ = false;
    return;
  }

  /* Without this there is the possibility of the curve table not being allocated. */
  BKE_curvemapping_changed((CurveMapping *)&scene->r.mblur_shutter_curve, false);

  Vector<float> cdf(CM_TABLE);
  Sampling::cdf_from_curvemapping(scene->r.mblur_shutter_curve, cdf);
  Sampling::cdf_invert(cdf, time_steps_);

  for (float &time : time_steps_) {
    time = this->shutter_time_to_scene_time(time);
  }

  step_id_ = 1;

  if (motion_blur_fx_enabled_) {
    /* A bit weird but we have to sync the first 2 steps here because the step()
     * function is only called after rendering a sample. */
    inst_.velocity.step_sync(STEP_PREVIOUS, time_steps_[0]);
    inst_.velocity.step_sync(STEP_NEXT, time_steps_[2]);
    /* Let the main sync loop handle the current step. */
  }
  inst_.set_time(time_steps_[1]);
}

/* Runs after rendering a sample. */
void MotionBlurModule::step()
{
  if (!enabled_) {
    return;
  }

  if (inst_.sampling.finished()) {
    /* Restore original frame number. This is because the render pipeline expects it. */
    RE_engine_frame_set(inst_.render, initial_frame_, initial_subframe_);
  }
  else if (inst_.sampling.do_render_sync()) {
    /* Time to change motion step. */
    BLI_assert(time_steps_.size() > step_id_ + 2);
    step_id_ += 2;

    if (motion_blur_fx_enabled_) {
      inst_.velocity.step_swap();
      inst_.velocity.step_sync(eVelocityStep::STEP_NEXT, time_steps_[step_id_ + 1]);
    }
    inst_.set_time(time_steps_[step_id_]);
  }
}

float MotionBlurModule::shutter_time_to_scene_time(float time)
{
  switch (shutter_position_) {
    case SCE_EEVEE_MB_START:
      /* No offset. */
      break;
    case SCE_EEVEE_MB_CENTER:
      time -= 0.5f;
      break;
    case SCE_EEVEE_MB_END:
      time -= 1.0;
      break;
    default:
      BLI_assert(!"Invalid motion blur position enum!");
      break;
  }
  time *= shutter_time_;
  time += frame_time_;
  return time;
}

void MotionBlurModule::sync()
{
  /* Disable motion blur in viewport when changing camera projection type.
   * Avoids really high velocities. */
  if (inst_.velocity.camera_changed_projection() ||
      (inst_.is_viewport() && inst_.camera.overscan_changed()))
  {
    motion_blur_fx_enabled_ = false;
  }

  if (!motion_blur_fx_enabled_) {
    return;
  }

  GPUSamplerState no_filter = GPUSamplerState::default_sampler();
  RenderBuffers &render_buffers = inst_.render_buffers;

  motion_blur_ps_.init();
  inst_.velocity.bind_resources(&motion_blur_ps_);
  inst_.sampling.bind_resources(&motion_blur_ps_);
  {
    /* Create max velocity tiles. */
    PassSimple::Sub &sub = motion_blur_ps_.sub("TilesFlatten");
    eGPUTextureFormat vector_tx_format = inst_.render_buffers.vector_tx_format();
    eShaderType shader = vector_tx_format == GPU_RG16F ? MOTION_BLUR_TILE_FLATTEN_RG :
                                                         MOTION_BLUR_TILE_FLATTEN_RGBA;
    sub.shader_set(inst_.shaders.static_shader_get(shader));
    sub.bind_ubo("motion_blur_buf", data_);
    sub.bind_texture("depth_tx", &render_buffers.depth_tx);
    sub.bind_image("velocity_img", &render_buffers.vector_tx);
    sub.bind_image("out_tiles_img", &tiles_tx_);
    sub.dispatch(&dispatch_flatten_size_);
    sub.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS | GPU_BARRIER_TEXTURE_FETCH);
  }
  {
    /* Expand max velocity tiles by spreading them in their neighborhood. */
    PassSimple::Sub &sub = motion_blur_ps_.sub("TilesDilate");
    sub.shader_set(inst_.shaders.static_shader_get(MOTION_BLUR_TILE_DILATE));
    sub.bind_ssbo("tile_indirection_buf", tile_indirection_buf_);
    sub.bind_image("in_tiles_img", &tiles_tx_);
    sub.dispatch(&dispatch_dilate_size_);
    sub.barrier(GPU_BARRIER_SHADER_STORAGE);
  }
  {
    /* Do the motion blur gather algorithm. */
    PassSimple::Sub &sub = motion_blur_ps_.sub("ConvolveGather");
    sub.shader_set(inst_.shaders.static_shader_get(MOTION_BLUR_GATHER));
    sub.bind_ubo("motion_blur_buf", data_);
    sub.bind_ssbo("tile_indirection_buf", tile_indirection_buf_);
    sub.bind_texture("depth_tx", &render_buffers.depth_tx, no_filter);
    sub.bind_texture("velocity_tx", &render_buffers.vector_tx, no_filter);
    sub.bind_texture("in_color_tx", &input_color_tx_, no_filter);
    sub.bind_image("in_tiles_img", &tiles_tx_);
    sub.bind_image("out_color_img", &output_color_tx_);

    sub.dispatch(&dispatch_gather_size_);
    sub.barrier(GPU_BARRIER_TEXTURE_FETCH);
  }
}

void MotionBlurModule::render(View &view, GPUTexture **input_tx, GPUTexture **output_tx)
{
  if (!motion_blur_fx_enabled_) {
    return;
  }

  const Texture &depth_tx = inst_.render_buffers.depth_tx;

  int2 extent = {depth_tx.width(), depth_tx.height()};
  int2 tiles_extent = math::divide_ceil(extent, int2(MOTION_BLUR_TILE_SIZE));

  if (inst_.is_viewport()) {
    float frame_delta = fabsf(inst_.velocity.step_time_delta_get(STEP_PREVIOUS, STEP_CURRENT));
    /* Avoid highly disturbing blurs, during navigation with high shutter time. */
    if (frame_delta > 0.0f && !DRW_state_is_navigating()) {
      /* Rescale motion blur intensity to be shutter time relative and avoid long streak when we
       * have frame skipping. Always try to stick to what the render frame would look like. */
      data_.motion_scale = float2(shutter_time_ / frame_delta);
    }
    else {
      /* There is no time change. Motion only comes from viewport navigation and object transform.
       * Apply motion blur as smoothing and only blur towards last frame. */
      data_.motion_scale = float2(1.0f, 0.0f);

      if (was_navigating_ != DRW_state_is_navigating()) {
        /* Special case for navigation events that only last for one frame (for instance mouse
         * scroll for zooming). For this case we have to wait for the next frame before enabling
         * the navigation motion blur. */
        was_navigating_ = DRW_state_is_navigating();
        return;
      }
    }
    was_navigating_ = DRW_state_is_navigating();
  }
  else {
    data_.motion_scale = float2(1.0f);
  }
  /* Second motion vector is stored inverted. */
  data_.motion_scale.y = -data_.motion_scale.y;
  data_.target_size_inv = 1.0f / float2(extent);
  data_.push_update();

  input_color_tx_ = *input_tx;
  output_color_tx_ = *output_tx;

  dispatch_flatten_size_ = int3(tiles_extent, 1);
  dispatch_dilate_size_ = int3(math::divide_ceil(tiles_extent, int2(MOTION_BLUR_GROUP_SIZE)), 1);
  dispatch_gather_size_ = int3(math::divide_ceil(extent, int2(MOTION_BLUR_GROUP_SIZE)), 1);

  DRW_stats_group_start("Motion Blur");

  tiles_tx_.acquire(tiles_extent, GPU_RGBA16F);

  tile_indirection_buf_.clear_to_zero();

  const bool do_motion_vectors_swizzle = inst_.render_buffers.vector_tx_format() == GPU_RG16F;
  if (do_motion_vectors_swizzle) {
    /* Change texture swizzling to avoid complexity in gather pass shader. */
    GPU_texture_swizzle_set(inst_.render_buffers.vector_tx, "rgrg");
  }

  inst_.manager->submit(motion_blur_ps_, view);

  if (do_motion_vectors_swizzle) {
    /* Reset swizzle since this texture might be reused in other places. */
    GPU_texture_swizzle_set(inst_.render_buffers.vector_tx, "rgba");
  }

  tiles_tx_.release();

  DRW_stats_group_end();

  /* Swap buffers so that next effect has the right input. */
  *input_tx = output_color_tx_;
  *output_tx = input_color_tx_;
}

/** \} */

}  // namespace blender::eevee
