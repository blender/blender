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

bool VolumeModule::GridAABB::init(Object *ob, const Camera &camera, const VolumesInfoData &data)
{
  /* Returns the unified volume grid cell index of a world space coordinate. */
  auto to_global_grid_coords = [&](float3 wP) -> int3 {
    const float4x4 &view_matrix = camera.data_get().viewmat;
    const float4x4 &projection_matrix = camera.data_get().winmat;

    float3 ndc_coords = math::project_point(projection_matrix * view_matrix, wP);
    ndc_coords = (ndc_coords * 0.5f) + float3(0.5f);

    float3 grid_coords = ndc_to_volume(projection_matrix,
                                       data.depth_near,
                                       data.depth_far,
                                       data.depth_distribution,
                                       data.coord_scale,
                                       ndc_coords);

    return int3(grid_coords * float3(data.tex_size));
  };

  const BoundBox &bbox = *BKE_object_boundbox_get(ob);
  min = int3(INT32_MAX);
  max = int3(INT32_MIN);

  for (float3 corner : bbox.vec) {
    corner = math::transform_point(float4x4(ob->object_to_world), corner);
    int3 grid_coord = to_global_grid_coords(corner);
    min = math::min(min, grid_coord);
    max = math::max(max, grid_coord);
  }

  bool is_visible = false;
  for (int i : IndexRange(3)) {
    is_visible = is_visible || (min[i] >= 0 && min[i] < data.tex_size[i]);
    is_visible = is_visible || (max[i] >= 0 && max[i] < data.tex_size[i]);
  }

  min = math::clamp(min, int3(0), data.tex_size);
  max = math::clamp(max, int3(0), data.tex_size);

  return is_visible;
}

bool VolumeModule::GridAABB::overlaps(const GridAABB &aabb)
{
  for (int i : IndexRange(3)) {
    if (min[i] > aabb.max[i] || max[i] < aabb.min[i]) {
      return false;
    }
  }
  return true;
}

void VolumeModule::init()
{
  enabled_ = false;
  subpass_aabbs_.clear();

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

void VolumeModule::sync_object(Object *ob,
                               ObjectHandle & /*ob_handle*/,
                               ResourceHandle res_handle,
                               MaterialPass *material_pass /*= nullptr*/)
{
  float3 size = math::to_scale(float4x4(ob->object_to_world));
  /* Check if any of the axes have 0 length. (see #69070) */
  const float epsilon = 1e-8f;
  if (size.x < epsilon || size.y < epsilon || size.z < epsilon) {
    return;
  }

  if (material_pass == nullptr) {
    Material material = inst_.materials.material_get(
        ob, false, VOLUME_MATERIAL_NR, MAT_GEOM_VOLUME_OBJECT);
    material_pass = &material.volume;
  }

  /* If shader failed to compile or is currently compiling. */
  if (material_pass->gpumat == nullptr) {
    return;
  }

  GPUShader *shader = GPU_material_get_shader(material_pass->gpumat);
  if (shader == nullptr) {
    return;
  }

  GridAABB aabb;
  if (!aabb.init(ob, inst_.camera, data_)) {
    return;
  }

  PassMain::Sub *object_pass = volume_sub_pass(
      *material_pass->sub_pass, inst_.scene, ob, material_pass->gpumat);
  if (object_pass) {
    enabled_ = true;

    /* Add a barrier at the start of a subpass or when 2 volumes overlaps. */
    if (!subpass_aabbs_.contains_as(shader)) {
      object_pass->barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
      subpass_aabbs_.add(shader, {aabb});
    }
    else {
      Vector<GridAABB> &aabbs = subpass_aabbs_.lookup(shader);
      for (GridAABB &_aabb : aabbs) {
        if (aabb.overlaps(_aabb)) {
          object_pass->barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
          aabbs.clear();
          break;
        }
      }
      aabbs.append(aabb);
    }

    int3 grid_size = aabb.max - aabb.min + int3(1);

    object_pass->push_constant("drw_ResourceID", int(res_handle.resource_index()));
    object_pass->push_constant("grid_coords_min", aabb.min);
    object_pass->dispatch(math::divide_ceil(grid_size, int3(VOLUME_GROUP_SIZE)));
  }
}

void VolumeModule::end_sync()
{
  if (!enabled_) {
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

  scatter_tx_.ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);
  extinction_tx_.ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);

  integrated_scatter_tx_.ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);
  integrated_transmit_tx_.ensure_3d(GPU_R11F_G11F_B10F, data_.tex_size, usage);

  transparent_pass_scatter_tx_ = integrated_scatter_tx_;
  transparent_pass_transmit_tx_ = integrated_transmit_tx_;

  scatter_ps_.init();
  scatter_ps_.shader_set(inst_.shaders.static_shader_get(
      data_.use_lights ? VOLUME_SCATTER_WITH_LIGHTS : VOLUME_SCATTER));
  inst_.lights.bind_resources(&scatter_ps_);
  inst_.irradiance_cache.bind_resources(&scatter_ps_);
  inst_.shadows.bind_resources(&scatter_ps_);
  inst_.sampling.bind_resources(&scatter_ps_);
  scatter_ps_.bind_image("in_scattering_img", &prop_scattering_tx_);
  scatter_ps_.bind_image("in_extinction_img", &prop_extinction_tx_);
  scatter_ps_.bind_texture("extinction_tx", &prop_extinction_tx_);
  scatter_ps_.bind_image("in_emission_img", &prop_emission_tx_);
  scatter_ps_.bind_image("in_phase_img", &prop_phase_tx_);
  scatter_ps_.bind_image("out_scattering_img", &scatter_tx_);
  scatter_ps_.bind_image("out_extinction_img", &extinction_tx_);
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
  /* Sync with the integration pass. */
  resolve_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH);
  resolve_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void VolumeModule::draw_prepass(View &view)
{
  if (!enabled_) {
    return;
  }

  inst_.pipelines.world_volume.render(view);
  inst_.pipelines.volume.render(view);
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
