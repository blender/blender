/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_lightprobe_sphere.hh"
#include "eevee_instance.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Reflection Probe Module
 * \{ */

int SphereProbeModule::probe_render_extent() const
{
  return instance_.scene->eevee.gi_cubemap_resolution / 2;
}

void SphereProbeModule::init()
{
  if (!instance_.is_viewport()) {
    /* TODO(jbakker): should we check on the subtype as well? Now it also populates even when
     * there are other light probes in the scene. */
    update_probes_next_sample_ = DEG_id_type_any_exists(instance_.depsgraph, ID_LP);
  }
  update_probes_this_sample_ = update_probes_next_sample_;

  do_display_draw_ = false;
}

void SphereProbeModule::begin_sync()
{
  LightProbeModule &light_probes = instance_.light_probes;
  SphereProbeData &world_data = *static_cast<SphereProbeData *>(&light_probes.world_sphere_);
  {
    gpu::Shader *shader = instance_.shaders.static_shader_get(SPHERE_PROBE_REMAP);

    PassSimple &pass = remap_ps_;
    pass.init();
    pass.specialize_constant(shader, "extract_sh", &extract_sh_);
    pass.specialize_constant(shader, "extract_sun", &extract_sh_);
    pass.shader_set(shader);
    pass.bind_texture("cubemap_tx", &cubemap_tx_);
    pass.bind_texture("atlas_tx", &probes_tx_);
    pass.bind_image("atlas_img", &probes_tx_);
    pass.bind_ssbo("out_sh", &tmp_spherical_harmonics_);
    pass.bind_ssbo("out_sun", &tmp_sunlight_);
    pass.push_constant("probe_coord_packed", reinterpret_cast<int4 *>(&probe_sampling_coord_));
    pass.push_constant("write_coord_packed", reinterpret_cast<int4 *>(&probe_write_coord_));
    pass.push_constant("world_coord_packed", reinterpret_cast<int4 *>(&world_data.atlas_coord));
    pass.bind_resources(instance_.uniform_data);
    pass.dispatch(&dispatch_probe_pack_);
  }
  {
    PassSimple &pass = convolve_ps_;
    pass.init();
    pass.shader_set(instance_.shaders.static_shader_get(SPHERE_PROBE_CONVOLVE));
    pass.bind_texture("cubemap_tx", &cubemap_tx_);
    pass.bind_texture("in_atlas_mip_tx", &convolve_input_);
    pass.bind_image("out_atlas_mip_img", &convolve_output_);
    pass.push_constant("probe_coord_packed", reinterpret_cast<int4 *>(&probe_sampling_coord_));
    pass.push_constant("write_coord_packed", reinterpret_cast<int4 *>(&probe_write_coord_));
    pass.push_constant("read_coord_packed", reinterpret_cast<int4 *>(&probe_read_coord_));
    pass.push_constant("read_lod", &convolve_lod_);
    pass.barrier(GPU_BARRIER_TEXTURE_FETCH);
    pass.dispatch(&dispatch_probe_convolve_);
  }
  {
    PassSimple &pass = sum_sh_ps_;
    pass.init();
    pass.shader_set(instance_.shaders.static_shader_get(SPHERE_PROBE_IRRADIANCE));
    pass.push_constant("probe_remap_dispatch_size", &dispatch_probe_pack_);
    pass.bind_ssbo("in_sh", &tmp_spherical_harmonics_);
    pass.bind_ssbo("out_sh", &spherical_harmonics_);
    pass.barrier(GPU_BARRIER_SHADER_STORAGE);
    pass.dispatch(1);
  }
  {
    PassSimple &pass = sum_sun_ps_;
    pass.init();
    pass.shader_set(instance_.shaders.static_shader_get(SPHERE_PROBE_SUNLIGHT));
    pass.push_constant("probe_remap_dispatch_size", &dispatch_probe_pack_);
    pass.bind_ssbo("in_sun", &tmp_sunlight_);
    pass.bind_ssbo("sunlight_buf", &instance_.world.sunlight);
    pass.barrier(GPU_BARRIER_SHADER_STORAGE);
    pass.dispatch(1);
    pass.barrier(GPU_BARRIER_UNIFORM);
  }
  {
    PassSimple &pass = select_ps_;
    pass.init();
    pass.shader_set(instance_.shaders.static_shader_get(SPHERE_PROBE_SELECT));
    pass.push_constant("lightprobe_sphere_count", &lightprobe_sphere_count_);
    pass.bind_ssbo("lightprobe_sphere_buf", &data_buf_);
    instance_.volume_probes.bind_resources(pass);
    instance_.sampling.bind_resources(pass);
    pass.bind_resources(instance_.uniform_data);
    pass.dispatch(&dispatch_probe_select_);
    pass.barrier(GPU_BARRIER_UNIFORM);
  }
}

bool SphereProbeModule::ensure_atlas()
{
  /* Make sure the atlas is always initialized even if there is nothing to render to it to fulfill
   * the resource bindings. */
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_WRITE | GPU_TEXTURE_USAGE_SHADER_READ;

  if (probes_tx_.ensure_2d_array(gpu::TextureFormat::SFLOAT_16_16_16_16,
                                 int2(SPHERE_PROBE_ATLAS_RES),
                                 instance_.light_probes.sphere_layer_count(),
                                 usage,
                                 nullptr,
                                 SPHERE_PROBE_MIPMAP_LEVELS))
  {
    probes_tx_.ensure_mip_views();
    /* TODO(fclem): Clearing means that we need to render all probes again.
     * If existing data exists, copy it using `CopyImageSubData`. */
    for (auto i : IndexRange(SPHERE_PROBE_MIPMAP_LEVELS)) {
      /* Avoid undefined pixel data. Clear all mips. */
      float4 data(0.0f);
      GPU_texture_clear(probes_tx_.mip_view(i), GPU_DATA_FLOAT, &data);
    }
    GPU_texture_mipmap_mode(probes_tx_, true, true);
    return true;
  }
  return false;
}

void SphereProbeModule::end_sync()
{
  const bool atlas_resized = ensure_atlas();
  if (atlas_resized) {
    instance_.light_probes.world_sphere_.do_render = true;
  }
  const bool world_updated = instance_.light_probes.world_sphere_.do_render;
  /* Detect if we need to render probe objects. */
  update_probes_next_sample_ = false;
  for (SphereProbe &probe : instance_.light_probes.sphere_map_.values()) {
    if (atlas_resized || world_updated) {
      /* Last minute tagging. */
      probe.do_render = true;
    }
    if (probe.do_render) {
      /* Tag the next redraw to warm up the probe pipeline.
       * Keep doing this until there is no update.
       * This avoids stuttering when moving a light-probe. */
      update_probes_next_sample_ = true;
    }
  }

  if (instance_.is_viewport()) {
    /* When reflection probes are synced the sampling must be reset.
     *
     * This fixes issues when using a single non-projected sample. Without resetting the
     * previous rendered viewport will be drawn and reflection probes will not be updated.
     * #Instance::render_sample */
    if (instance_.do_lightprobe_sphere_sync()) {
      instance_.sampling.reset();
    }
    /* If we cannot render probes this redraw make sure we request another redraw. */
    if (update_probes_next_sample_ && (instance_.do_lightprobe_sphere_sync() == false)) {
      DRW_viewport_request_redraw();
    }
  }
}

void SphereProbeModule::ensure_cubemap_render_target(int resolution)
{
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_READ;
  cubemap_tx_.ensure_cube(gpu::TextureFormat::SFLOAT_16_16_16_16, resolution, usage);
  /* TODO(fclem): deallocate it. */
}

SphereProbeModule::UpdateInfo SphereProbeModule::update_info_from_probe(SphereProbe &probe)
{
  SphereProbeModule::UpdateInfo info = {};
  info.atlas_coord = probe.atlas_coord;
  info.cube_target_extent = probe.atlas_coord.area_extent() / 2;
  info.clipping_distances = probe.clipping_distances;
  info.probe_pos = probe.location;
  info.do_render = probe.do_render;

  probe.do_render = false;
  probe.use_for_render = true;

  ensure_cubemap_render_target(info.cube_target_extent);
  return info;
}

std::optional<SphereProbeModule::UpdateInfo> SphereProbeModule::world_update_info_pop()
{
  SphereProbe &world_probe = instance_.light_probes.world_sphere_;
  if (world_probe.do_render) {
    return update_info_from_probe(world_probe);
  }
  return std::nullopt;
}

std::optional<SphereProbeModule::UpdateInfo> SphereProbeModule::probe_update_info_pop()
{
  if (!instance_.do_lightprobe_sphere_sync()) {
    /* Do not update probes during this sample as we did not sync the draw::Passes. */
    return std::nullopt;
  }

  for (SphereProbe &probe : instance_.light_probes.sphere_map_.values()) {
    if (!probe.do_render) {
      continue;
    }
    return update_info_from_probe(probe);
  }

  return std::nullopt;
}

void SphereProbeModule::remap_to_octahedral_projection(const SphereProbeAtlasCoord &atlas_coord,
                                                       bool extract_spherical_harmonics)
{
  /* Update shader parameters that change per dispatch. */
  probe_sampling_coord_ = atlas_coord.as_sampling_coord();
  probe_write_coord_ = atlas_coord.as_write_coord(0);
  int resolution = probe_write_coord_.extent;
  dispatch_probe_pack_ = int3(
      int2(math::divide_ceil(int2(resolution), int2(SPHERE_PROBE_REMAP_GROUP_SIZE))), 1);
  extract_sh_ = extract_spherical_harmonics;
  instance_.manager->submit(remap_ps_);

  /* Populate the mip levels */
  for (auto i : IndexRange(SPHERE_PROBE_MIPMAP_LEVELS - 1)) {
    convolve_lod_ = i;
    convolve_input_ = probes_tx_.mip_view(i);
    convolve_output_ = probes_tx_.mip_view(i + 1);
    probe_read_coord_ = atlas_coord.as_write_coord(i);
    probe_write_coord_ = atlas_coord.as_write_coord(i + 1);
    int out_mip_res = probe_write_coord_.extent;
    dispatch_probe_convolve_ = int3(
        math::divide_ceil(int2(out_mip_res), int2(SPHERE_PROBE_GROUP_SIZE)), 1);
    instance_.manager->submit(convolve_ps_);
  }

  if (extract_spherical_harmonics) {
    instance_.manager->submit(sum_sh_ps_);
    instance_.manager->submit(sum_sun_ps_);
    /* All volume probe that needs to composite the world probe need to be updated. */
    instance_.volume_probes.update_world_irradiance();
  }

  /* Sync with atlas usage for shading. */
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);
}

void SphereProbeModule::set_view(View & /*view*/)
{
  Vector<SphereProbe *> probe_active;
  for (auto &probe : instance_.light_probes.sphere_map_.values()) {
    /* Last slot is reserved for the world probe. */
    if (lightprobe_sphere_count_ >= SPHERE_PROBE_MAX - 1) {
      break;
    }
    if (!probe.use_for_render) {
      continue;
    }
    /* TODO(fclem): Culling. */
    probe_active.append(&probe);
  }

  /* Stable sorting of probes. */
  std::sort(
      probe_active.begin(), probe_active.end(), [](const SphereProbe *a, const SphereProbe *b) {
        if (a->volume != b->volume) {
          /* Smallest first. */
          return a->volume < b->volume;
        }
        /* Volumes are identical. Any arbitrary criteria can be used to sort them.
         * Use position to avoid unstable result caused by depsgraph non deterministic eval
         * order. This could also become a priority parameter. */
        float3 _a = a->location;
        float3 _b = b->location;
        if (_a.x != _b.x) {
          return _a.x < _b.x;
        }
        if (_a.y != _b.y) {
          return _a.y < _b.y;
        }
        if (_a.z != _b.z) {
          return _a.z < _b.z;
        }
        /* Fallback to memory address, since there's no good alternative. */
        return a < b;
      });

  /* Push all sorted data to the UBO. */
  int probe_id = 0;
  for (auto &probe : probe_active) {
    data_buf_[probe_id++] = *probe;
  }
  /* Add world probe at the end. */
  data_buf_[probe_id++] = instance_.light_probes.world_sphere_;
  /* Tag the end of the array. */
  if (probe_id < SPHERE_PROBE_MAX) {
    data_buf_[probe_id].atlas_coord.layer = -1;
  }
  data_buf_.push_update();

  lightprobe_sphere_count_ = probe_id;
  dispatch_probe_select_.x = divide_ceil_u(lightprobe_sphere_count_,
                                           SPHERE_PROBE_SELECT_GROUP_SIZE);
  instance_.manager->submit(select_ps_);

  sync_display(probe_active);
}

void SphereProbeModule::sync_display(Vector<SphereProbe *> &probe_active)
{
  do_display_draw_ = false;
  if (!instance_.draw_overlays) {
    return;
  }

  int display_index = 0;
  for (int i : probe_active.index_range()) {
    if (probe_active[i]->viewport_display) {
      SphereProbeDisplayData &sph_data = display_data_buf_.get_or_resize(display_index++);
      sph_data.probe_index = i;
      sph_data.display_size = probe_active[i]->viewport_display_size;
    }
  }

  if (display_index == 0) {
    return;
  }
  do_display_draw_ = true;
  display_data_buf_.resize(display_index);
  display_data_buf_.push_update();
}

void SphereProbeModule::viewport_draw(View &view, gpu::FrameBuffer *view_fb)
{
  if (!do_display_draw_) {
    return;
  }

  viewport_display_ps_.init();
  viewport_display_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                 DRW_STATE_CLIP_CONTROL_UNIT_RANGE |
                                 instance_.film.depth.test_state | DRW_STATE_CULL_BACK);
  viewport_display_ps_.framebuffer_set(&view_fb);
  viewport_display_ps_.shader_set(instance_.shaders.static_shader_get(DISPLAY_PROBE_SPHERE));
  viewport_display_ps_.bind_resources(*this);
  viewport_display_ps_.bind_ssbo("display_data_buf", display_data_buf_);
  viewport_display_ps_.draw_procedural(GPU_PRIM_TRIS, 1, display_data_buf_.size() * 6);

  instance_.manager->submit(viewport_display_ps_, view);
}

/** \} */

}  // namespace blender::eevee
