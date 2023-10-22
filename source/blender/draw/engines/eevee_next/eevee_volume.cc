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
#include "GPU_capabilities.h"

#include "draw_common.hh"

#include "eevee_instance.hh"
#include "eevee_pipeline.hh"

#include "eevee_volume.hh"

namespace blender::eevee {

void VolumeModule::init()
{
  enabled_ = false;

  const Scene *scene_eval = inst_.scene;

  const float2 viewport_size = float2(inst_.film.render_extent_get());
  const int tile_size = scene_eval->eevee.volumetric_tile_size;

  data_.tile_size = tile_size;
  data_.tile_size_lod = int(log2(tile_size));

  /* Find Froxel Texture resolution. */
  int3 tex_size = int3(math::ceil(math::max(float2(1.0f), viewport_size / float(tile_size))), 0);
  tex_size.z = std::max(1, scene_eval->eevee.volumetric_samples);

  /* Clamp 3D texture size based on device maximum. */
  int3 max_size = int3(GPU_max_texture_3d_size());
  BLI_assert(tex_size == math::min(tex_size, max_size));
  tex_size = math::min(tex_size, max_size);

  data_.coord_scale = viewport_size / float2(tile_size * tex_size);
  data_.viewport_size_inv = 1.0f / viewport_size;

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

  data_.use_lights = (scene_eval->eevee.flag & SCE_EEVEE_VOLUMETRIC_LIGHTS) != 0;
  data_.use_soft_shadows = (scene_eval->eevee.flag & SCE_EEVEE_SHADOW_SOFT) != 0;

  data_.light_clamp = scene_eval->eevee.volumetric_light_clamp;
}

void VolumeModule::begin_sync()
{
  const Scene *scene_eval = inst_.scene;

  /* Negate clip values (View matrix forward vector is -Z). */
  const float clip_start = -inst_.camera.data_get().clip_near;
  const float clip_end = -inst_.camera.data_get().clip_far;
  float integration_start = scene_eval->eevee.volumetric_start;
  float integration_end = scene_eval->eevee.volumetric_end;

  if (inst_.camera.is_perspective()) {
    float sample_distribution = scene_eval->eevee.volumetric_sample_distribution;
    sample_distribution = 4.0f * std::max(1.0f - sample_distribution, 1e-2f);

    float near = integration_start = std::min(-integration_start, clip_start - 1e-4f);
    float far = integration_end = std::min(-integration_end, near - 1e-4f);

    data_.depth_near = (far - near * exp2(1.0f / sample_distribution)) / (far - near);
    data_.depth_far = (1.0f - data_.depth_near) / near;
    data_.depth_distribution = sample_distribution;
  }
  else {
    integration_start = std::min(integration_end, clip_start);
    integration_end = std::max(-integration_end, clip_end);

    data_.depth_near = integration_start;
    data_.depth_far = integration_end;
    data_.depth_distribution = 1.0f / (integration_end - integration_start);
  }

  enabled_ = inst_.world.has_volume();
}

void VolumeModule::end_sync()
{
  enabled_ = enabled_ || inst_.pipelines.volume.is_enabled();

  if (!enabled_) {
    occupancy_tx_.free();
    prop_scattering_tx_.free();
    prop_extinction_tx_.free();
    prop_emission_tx_.free();
    prop_phase_tx_.free();
    scatter_tx_.free();
    extinction_tx_.free();
    integrated_scatter_tx_.free();
    integrated_transmit_tx_.free();

    transparent_pass_scatter_tx_ = dummy_scatter_tx_;
    transparent_pass_transmit_tx_ = dummy_transmit_tx_;

    return;
  }

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

  if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
    /* Metal requires a dummy attachment. */
    occupancy_fb_.ensure(GPU_ATTACHMENT_NONE,
                         GPU_ATTACHMENT_TEXTURE_LAYER(prop_extinction_tx_, 0));
  }
  else {
    /* Empty frame-buffer. */
    occupancy_fb_.ensure(data_.tex_size.xy());
  }

  if (!inst_.pipelines.world_volume.is_valid()) {
    prop_scattering_tx_.clear(float4(0.0f));
    prop_extinction_tx_.clear(float4(0.0f));
    prop_emission_tx_.clear(float4(0.0f));
    prop_phase_tx_.clear(float4(0.0f));
  }

  scatter_tx_.ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);
  extinction_tx_.ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);

  integrated_scatter_tx_.ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);
  integrated_transmit_tx_.ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);

  transparent_pass_scatter_tx_ = integrated_scatter_tx_;
  transparent_pass_transmit_tx_ = integrated_transmit_tx_;

  scatter_ps_.init();
  scatter_ps_.shader_set(inst_.shaders.static_shader_get(
      data_.use_lights ? VOLUME_SCATTER_WITH_LIGHTS : VOLUME_SCATTER));
  inst_.lights.bind_resources(scatter_ps_);
  inst_.reflection_probes.bind_resources(scatter_ps_);
  inst_.irradiance_cache.bind_resources(scatter_ps_);
  inst_.shadows.bind_resources(scatter_ps_);
  inst_.sampling.bind_resources(scatter_ps_);
  scatter_ps_.bind_image("in_scattering_img", &prop_scattering_tx_);
  scatter_ps_.bind_image("in_extinction_img", &prop_extinction_tx_);
  scatter_ps_.bind_texture("extinction_tx", &prop_extinction_tx_);
  scatter_ps_.bind_image("in_emission_img", &prop_emission_tx_);
  scatter_ps_.bind_image("in_phase_img", &prop_phase_tx_);
  scatter_ps_.bind_image("out_scattering_img", &scatter_tx_);
  scatter_ps_.bind_image("out_extinction_img", &extinction_tx_);
  scatter_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
  /* Sync with the property pass. */
  scatter_ps_.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS | GPU_BARRIER_TEXTURE_FETCH);
  scatter_ps_.dispatch(math::divide_ceil(data_.tex_size, int3(VOLUME_GROUP_SIZE)));

  integration_ps_.init();
  integration_ps_.shader_set(inst_.shaders.static_shader_get(VOLUME_INTEGRATION));
  inst_.bind_uniform_data(&integration_ps_);
  integration_ps_.bind_texture("in_scattering_tx", &scatter_tx_);
  integration_ps_.bind_texture("in_extinction_tx", &extinction_tx_);
  integration_ps_.bind_image("out_scattering_img", &integrated_scatter_tx_);
  integration_ps_.bind_image("out_transmittance_img", &integrated_transmit_tx_);
  /* Sync with the scatter pass. */
  integration_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH);
  integration_ps_.dispatch(
      math::divide_ceil(int2(data_.tex_size), int2(VOLUME_INTEGRATION_GROUP_SIZE)));

  resolve_ps_.init();
  resolve_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
  resolve_ps_.shader_set(inst_.shaders.static_shader_get(VOLUME_RESOLVE));
  inst_.bind_uniform_data(&resolve_ps_);
  bind_resources(resolve_ps_);
  resolve_ps_.bind_texture("depth_tx", &inst_.render_buffers.depth_tx);
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
  float4x4 winmat = view.winmat();
  projmat_dimensions(winmat.ptr(), &left, &right, &bottom, &top, &near, &far);

  float4x4 winmat_infinite = view.is_persp() ?
                                 math::projection::perspective_infinite(
                                     left, right, bottom, top, near) :
                                 math::projection::orthographic_infinite(left, right, bottom, top);

  View volume_view = {"Volume View"};
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

  inst_.manager->submit(scatter_ps_, view);
  inst_.manager->submit(integration_ps_, view);
}

void VolumeModule::draw_resolve(View &view)
{
  if (!enabled_) {
    return;
  }

  resolve_fb_.ensure(GPU_ATTACHMENT_NONE,
                     GPU_ATTACHMENT_TEXTURE(inst_.render_buffers.combined_tx));
  resolve_fb_.bind();
  inst_.manager->submit(resolve_ps_, view);
}

}  // namespace blender::eevee
