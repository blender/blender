/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Volumetric effects rendering using Frostbite's Physically-based & Unified Volumetric Rendering
 * approach.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite
 */

#include "DNA_volume_types.h"
#include "GPU_capabilities.hh"

#include "draw_common.hh"

#include "eevee_instance.hh"
#include "eevee_pipeline.hh"

#include "eevee_volume.hh"
#include <iostream>

namespace blender::eevee {

void VolumeModule::init()
{
  enabled_ = false;

  const Scene *scene_eval = inst_.scene;

  const int2 extent = inst_.film.render_extent_get();
  const int tile_size = scene_eval->eevee.volumetric_tile_size;

  data_.tile_size = tile_size;
  data_.tile_size_lod = int(log2(tile_size));

  /* Find Froxel Texture resolution. */
  int3 tex_size = int3(math::divide_ceil(extent, int2(tile_size)), 0);
  tex_size.z = std::max(1, scene_eval->eevee.volumetric_samples);

  /* Clamp 3D texture size based on device maximum. */
  int3 max_size = int3(GPU_max_texture_3d_size());
  BLI_assert(tex_size == math::min(tex_size, max_size));
  tex_size = math::min(tex_size, max_size);

  data_.coord_scale = float2(extent) / float2(tile_size * tex_size);
  data_.main_view_extent = float2(extent);
  data_.main_view_extent_inv = 1.0f / float2(extent);

  /* TODO: compute snap to maxZBuffer for clustered rendering. */
  if (data_.tex_size != tex_size) {
    data_.tex_size = tex_size;
    data_.inv_tex_size = 1.0f / float3(tex_size);
  }

  if ((scene_eval->eevee.flag & SCE_EEVEE_VOLUMETRIC_SHADOWS) == 0) {
    data_.shadow_steps = 0;
  }
  else {
    data_.shadow_steps = float(scene_eval->eevee.volumetric_shadow_samples);
  }

  data_.light_clamp = scene_eval->eevee.volumetric_light_clamp;
}

void VolumeModule::begin_sync() {}

void VolumeModule::end_sync()
{
  enabled_ = inst_.world.has_volume() || inst_.pipelines.volume.is_enabled();

  const Scene *scene_eval = inst_.scene;

  /* Negate clip values (View matrix forward vector is -Z). */
  const float clip_start = -inst_.camera.data_get().clip_near;
  const float clip_end = -inst_.camera.data_get().clip_far;
  float integration_start = scene_eval->eevee.volumetric_start;
  float integration_end = scene_eval->eevee.volumetric_end;

  if (!inst_.camera.is_camera_object() && inst_.camera.is_orthographic()) {
    integration_start = -integration_end;
  }

  std::optional<Bounds<float>> volume_bounds = inst_.pipelines.volume.object_integration_range();

  if (volume_bounds && !inst_.world.has_volume()) {
    /* Restrict integration range to the object volume range. This increases precision. */
    integration_start = math::max(integration_start, -volume_bounds.value().max);
    integration_end = math::min(integration_end, -volume_bounds.value().min);
  }

  float near = math::min(-integration_start, clip_start + 1e-4f);
  float far = math::max(-integration_end, clip_end - 1e-4f);

  if (inst_.camera.is_perspective()) {
    float sample_distribution = scene_eval->eevee.volumetric_sample_distribution;
    sample_distribution = 4.0f * math::max(1.0f - sample_distribution, 1e-2f);

    data_.depth_near = (far - near * exp2(1.0f / sample_distribution)) / (far - near);
    data_.depth_far = (1.0f - data_.depth_near) / near;
    data_.depth_distribution = sample_distribution;
  }
  else {
    data_.depth_near = near;
    data_.depth_far = far;
    data_.depth_distribution = 0.0f; /* Unused. */
  }

  if (!enabled_) {
    occupancy_tx_.free();
    prop_scattering_tx_.free();
    prop_extinction_tx_.free();
    prop_emission_tx_.free();
    prop_phase_tx_.free();
    scatter_tx_.current().free();
    scatter_tx_.previous().free();
    extinction_tx_.current().free();
    extinction_tx_.previous().free();
    integrated_scatter_tx_.free();
    integrated_transmit_tx_.free();

    /* Update references for bindings. */
    result.scattering_tx_ = dummy_scatter_tx_;
    result.transmittance_tx_ = dummy_transmit_tx_;
    /* These shouldn't be used. */
    properties.scattering_tx_ = nullptr;
    properties.extinction_tx_ = nullptr;
    properties.emission_tx_ = nullptr;
    properties.phase_tx_ = nullptr;
    properties.occupancy_tx_ = nullptr;
    occupancy.occupancy_tx_ = nullptr;
    occupancy.hit_depth_tx_ = nullptr;
    occupancy.hit_count_tx_ = nullptr;
    return;
  }

  bool has_scatter = inst_.world.has_volume_scatter() || inst_.pipelines.volume.has_scatter();
  bool has_absorption = inst_.world.has_volume_absorption() ||
                        inst_.pipelines.volume.has_absorption();
  use_lights_ = has_scatter;
  /* TODO(fclem): Allocate extinction texture as dummy (1px^3) if has_absorption are false. */
  UNUSED_VARS(has_absorption);

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE |
                           GPU_TEXTURE_USAGE_ATTACHMENT;

  prop_scattering_tx_.ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);
  prop_extinction_tx_.ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);
  prop_emission_tx_.ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);
  prop_phase_tx_.ensure_3d(GPU_RG16F, data_.tex_size, usage);

  int occupancy_layers = divide_ceil_u(data_.tex_size.z, 32u);
  eGPUTextureUsage occupancy_usage = GPU_TEXTURE_USAGE_SHADER_READ |
                                     GPU_TEXTURE_USAGE_SHADER_WRITE | GPU_TEXTURE_USAGE_ATOMIC;
  occupancy_tx_.ensure_3d(GPU_R32UI, int3(data_.tex_size.xy(), occupancy_layers), occupancy_usage);

  {
    eGPUTextureUsage hit_count_usage = GPU_TEXTURE_USAGE_SHADER_READ |
                                       GPU_TEXTURE_USAGE_SHADER_WRITE | GPU_TEXTURE_USAGE_ATOMIC;
    eGPUTextureUsage hit_depth_usage = GPU_TEXTURE_USAGE_SHADER_READ |
                                       GPU_TEXTURE_USAGE_SHADER_WRITE;
    int2 hit_list_size = int2(1);
    int hit_list_layer = 1;
    if (inst_.pipelines.volume.use_hit_list()) {
      hit_list_layer = clamp_i(inst_.scene->eevee.volumetric_ray_depth, 1, 16);
      hit_list_size = data_.tex_size.xy();
    }
    hit_depth_tx_.ensure_3d(GPU_R32F, int3(hit_list_size, hit_list_layer), hit_depth_usage);
    if (hit_count_tx_.ensure_2d(GPU_R32UI, hit_list_size, hit_count_usage)) {
      hit_count_tx_.clear(uint4(0u));
    }
  }

  eGPUTextureUsage front_depth_usage = GPU_TEXTURE_USAGE_SHADER_READ |
                                       GPU_TEXTURE_USAGE_ATTACHMENT;
  front_depth_tx_.ensure_2d(GPU_DEPTH24_STENCIL8, data_.tex_size.xy(), front_depth_usage);
  occupancy_fb_.ensure(GPU_ATTACHMENT_TEXTURE(front_depth_tx_));

  scatter_tx_.current().ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);
  extinction_tx_.current().ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);
  scatter_tx_.previous().ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);
  extinction_tx_.previous().ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);

  data_.history_matrix = float4x4::identity();

  integrated_scatter_tx_.ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);
  integrated_transmit_tx_.ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);

  /* Update references for bindings. */
  result.scattering_tx_ = integrated_scatter_tx_;
  result.transmittance_tx_ = integrated_transmit_tx_;
  properties.scattering_tx_ = prop_scattering_tx_;
  properties.extinction_tx_ = prop_extinction_tx_;
  properties.emission_tx_ = prop_emission_tx_;
  properties.phase_tx_ = prop_phase_tx_;
  properties.occupancy_tx_ = occupancy_tx_;
  occupancy.occupancy_tx_ = occupancy_tx_;
  occupancy.hit_depth_tx_ = hit_depth_tx_;
  occupancy.hit_count_tx_ = hit_count_tx_;

  /* Use custom sampler to simplify and speedup the shader.
   * - Set extend mode to clamp to border color to reject samples with invalid re-projection.
   * - Set filtering mode to none to avoid over-blur during re-projection. */
  const GPUSamplerState history_sampler = {GPU_SAMPLER_FILTERING_DEFAULT,
                                           GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER,
                                           GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER};
  scatter_ps_.init();
  scatter_ps_.shader_set(
      inst_.shaders.static_shader_get(use_lights_ ? VOLUME_SCATTER_WITH_LIGHTS : VOLUME_SCATTER));
  scatter_ps_.bind_resources(inst_.lights);
  scatter_ps_.bind_resources(inst_.sphere_probes);
  scatter_ps_.bind_resources(inst_.volume_probes);
  scatter_ps_.bind_resources(inst_.shadows);
  scatter_ps_.bind_resources(inst_.sampling);
  scatter_ps_.bind_image("in_scattering_img", &prop_scattering_tx_);
  scatter_ps_.bind_image("in_extinction_img", &prop_extinction_tx_);
  scatter_ps_.bind_texture("extinction_tx", &prop_extinction_tx_);
  scatter_ps_.bind_image("in_emission_img", &prop_emission_tx_);
  scatter_ps_.bind_image("in_phase_img", &prop_phase_tx_);
  scatter_ps_.bind_texture("scattering_history_tx", &scatter_tx_.previous(), history_sampler);
  scatter_ps_.bind_texture("extinction_history_tx", &extinction_tx_.previous(), history_sampler);
  scatter_ps_.bind_image("out_scattering_img", &scatter_tx_.current());
  scatter_ps_.bind_image("out_extinction_img", &extinction_tx_.current());
  scatter_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
  /* Sync with the property pass. */
  scatter_ps_.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS | GPU_BARRIER_TEXTURE_FETCH);
  scatter_ps_.dispatch(math::divide_ceil(data_.tex_size, int3(VOLUME_GROUP_SIZE)));

  integration_ps_.init();
  integration_ps_.shader_set(inst_.shaders.static_shader_get(VOLUME_INTEGRATION));
  integration_ps_.bind_resources(inst_.uniform_data);
  integration_ps_.bind_resources(inst_.sampling);
  integration_ps_.bind_texture("in_scattering_tx", &scatter_tx_.current());
  integration_ps_.bind_texture("in_extinction_tx", &extinction_tx_.current());
  integration_ps_.bind_image("out_scattering_img", &integrated_scatter_tx_);
  integration_ps_.bind_image("out_transmittance_img", &integrated_transmit_tx_);
  /* Sync with the scatter pass. */
  integration_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH);
  integration_ps_.dispatch(
      math::divide_ceil(int2(data_.tex_size), int2(VOLUME_INTEGRATION_GROUP_SIZE)));

  resolve_ps_.init();
  resolve_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
  resolve_ps_.shader_set(inst_.shaders.static_shader_get(VOLUME_RESOLVE));
  resolve_ps_.bind_resources(inst_.uniform_data);
  resolve_ps_.bind_resources(this->result);
  resolve_ps_.bind_resources(inst_.hiz_buffer.front);
  resolve_ps_.bind_image(RBUFS_COLOR_SLOT, &inst_.render_buffers.rp_color_tx);
  resolve_ps_.bind_image(RBUFS_VALUE_SLOT, &inst_.render_buffers.rp_value_tx);
  /* Sync with the integration pass. */
  resolve_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH);
  resolve_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void VolumeModule::draw_prepass(View &view)
{
  if (!enabled_) {
    return;
  }

  DRW_stats_group_start("Volumes");
  inst_.pipelines.world_volume.render(view);

  float left, right, bottom, top, near, far;
  const float4x4 winmat_view = view.winmat();
  projmat_dimensions(winmat_view.ptr(), &left, &right, &bottom, &top, &near, &far);

  /* Just like up-sampling matrix computation, we have to be careful to where to put the bounds of
   * our froxel volume so that a 2D pixel covers exactly the number of pixel in a tile. */
  float2 render_size = float2(right - left, top - bottom);
  float2 volume_size = render_size * float2(data_.tex_size.xy() * data_.tile_size) /
                       float2(inst_.film.render_extent_get());
  /* Change to the padded extends. */
  right = left + volume_size.x;
  top = bottom + volume_size.y;

  /* TODO(fclem): These new matrices are created from the jittered main view matrix. It should be
   * better to create them from the non-jittered one to avoid over-blurring. */
  float4x4 winmat_infinite, winmat_finite;
  /* Create an infinite projection matrix to avoid far clipping plane clipping the object. This
   * way, surfaces that are further away than the far clip plane will still be voxelized.*/
  winmat_infinite = view.is_persp() ?
                        math::projection::perspective_infinite(left, right, bottom, top, near) :
                        math::projection::orthographic_infinite(left, right, bottom, top);
  /* We still need a bounded projection matrix to get correct froxel location. */
  winmat_finite = view.is_persp() ?
                      math::projection::perspective(left, right, bottom, top, near, far) :
                      math::projection::orthographic(left, right, bottom, top, near, far);
  /* Anti-Aliasing / Super-Sampling jitter. */
  float2 jitter = inst_.sampling.rng_2d_get(SAMPLING_VOLUME_U);
  /* Wrap to keep first sample centered (0,0) and scale to convert to NDC. */
  jitter = math::fract(jitter + 0.5f) * 2.0f - 1.0f;
  /* Convert to pixel size. */
  jitter *= data_.inv_tex_size.xy();
  /* Apply jitter to both matrices. */
  winmat_infinite = math::projection::translate(winmat_infinite, jitter);
  winmat_finite = math::projection::translate(winmat_finite, jitter);

  data_.winmat_finite = winmat_finite;
  data_.wininv_finite = math::invert(winmat_finite);
  inst_.uniform_data.push_update();

  volume_view.sync(view.viewmat(), winmat_infinite);

  if (inst_.pipelines.volume.is_enabled()) {
    occupancy_fb_.bind();
    inst_.pipelines.volume.render(volume_view, occupancy_tx_);
  }
  DRW_stats_group_end();
}

void VolumeModule::draw_compute(View &view)
{
  if (!enabled_) {
    return;
  }
  scatter_tx_.swap();
  extinction_tx_.swap();

  inst_.manager->submit(scatter_ps_, view);
  inst_.manager->submit(integration_ps_, view);
}

void VolumeModule::draw_resolve(View &view)
{
  if (!enabled_) {
    return;
  }

  inst_.hiz_buffer.update();

  resolve_fb_.ensure(GPU_ATTACHMENT_NONE,
                     GPU_ATTACHMENT_TEXTURE(inst_.render_buffers.combined_tx));
  resolve_fb_.bind();
  inst_.manager->submit(resolve_ps_, view);
}

}  // namespace blender::eevee
