/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

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
  BKE_curvemapping_changed((struct CurveMapping *)&scene->r.mblur_shutter_curve, false);

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
  if (inst_.velocity.camera_changed_projection()) {
    motion_blur_fx_enabled_ = false;
  }

  if (!motion_blur_fx_enabled_) {
    return;
  }

  eGPUSamplerState no_filter = GPU_SAMPLER_DEFAULT;
  RenderBuffers &render_buffers = inst_.render_buffers;

  {
    /* Create max velocity tiles. */
    DRW_PASS_CREATE(tiles_flatten_ps_, DRW_STATE_NO_DRAW);
    eShaderType shader = (inst_.is_viewport()) ? MOTION_BLUR_TILE_FLATTEN_VIEWPORT :
                                                 MOTION_BLUR_TILE_FLATTEN_RENDER;
    GPUShader *sh = inst_.shaders.static_shader_get(shader);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, tiles_flatten_ps_);
    inst_.velocity.bind_resources(grp);
    DRW_shgroup_uniform_block(grp, "motion_blur_buf", data_);
    DRW_shgroup_uniform_texture_ref(grp, "depth_tx", &render_buffers.depth_tx);
    DRW_shgroup_uniform_image_ref(grp, "velocity_img", &render_buffers.vector_tx);
    DRW_shgroup_uniform_image_ref(grp, "out_tiles_img", &tiles_tx_);

    DRW_shgroup_call_compute_ref(grp, dispatch_flatten_size_);
    DRW_shgroup_barrier(grp, GPU_BARRIER_SHADER_IMAGE_ACCESS | GPU_BARRIER_TEXTURE_FETCH);
  }
  {
    /* Expand max velocity tiles by spreading them in their neighborhood. */
    DRW_PASS_CREATE(tiles_dilate_ps_, DRW_STATE_NO_DRAW);
    GPUShader *sh = inst_.shaders.static_shader_get(MOTION_BLUR_TILE_DILATE);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, tiles_dilate_ps_);
    DRW_shgroup_storage_block(grp, "tile_indirection_buf", tile_indirection_buf_);
    DRW_shgroup_uniform_image_ref(grp, "in_tiles_img", &tiles_tx_);

    DRW_shgroup_call_compute_ref(grp, dispatch_dilate_size_);
    DRW_shgroup_barrier(grp, GPU_BARRIER_SHADER_STORAGE);
  }
  {
    /* Do the motion blur gather algorithm. */
    DRW_PASS_CREATE(gather_ps_, DRW_STATE_NO_DRAW);
    GPUShader *sh = inst_.shaders.static_shader_get(MOTION_BLUR_GATHER);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, gather_ps_);
    inst_.sampling.bind_resources(grp);
    DRW_shgroup_uniform_block(grp, "motion_blur_buf", data_);
    DRW_shgroup_storage_block(grp, "tile_indirection_buf", tile_indirection_buf_);
    DRW_shgroup_uniform_texture_ref_ex(grp, "depth_tx", &render_buffers.depth_tx, no_filter);
    DRW_shgroup_uniform_texture_ref_ex(grp, "velocity_tx", &render_buffers.vector_tx, no_filter);
    DRW_shgroup_uniform_texture_ref_ex(grp, "in_color_tx", &input_color_tx_, no_filter);
    DRW_shgroup_uniform_image_ref(grp, "in_tiles_img", &tiles_tx_);
    DRW_shgroup_uniform_image_ref(grp, "out_color_img", &output_color_tx_);

    DRW_shgroup_call_compute_ref(grp, dispatch_gather_size_);
    DRW_shgroup_barrier(grp, GPU_BARRIER_TEXTURE_FETCH);
  }
}

void MotionBlurModule::render(GPUTexture **input_tx, GPUTexture **output_tx)
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

    /* Change texture swizzling to avoid complexity in gather pass shader. */
    GPU_texture_swizzle_set(inst_.render_buffers.vector_tx, "rgrg");
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

  GPU_storagebuf_clear_to_zero(tile_indirection_buf_);

  DRW_draw_pass(tiles_flatten_ps_);
  DRW_draw_pass(tiles_dilate_ps_);
  DRW_draw_pass(gather_ps_);

  tiles_tx_.release();

  DRW_stats_group_end();

  if (inst_.is_viewport()) {
    /* Reset swizzle since this texture might be reused in other places. */
    GPU_texture_swizzle_set(inst_.render_buffers.vector_tx, "rgba");
  }

  /* Swap buffers so that next effect has the right input. */
  *input_tx = output_color_tx_;
  *output_tx = input_color_tx_;
}

/** \} */

}  // namespace blender::eevee
