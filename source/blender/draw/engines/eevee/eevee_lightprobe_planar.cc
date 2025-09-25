/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_lightprobe_planar.hh"
#include "eevee_instance.hh"

namespace blender::eevee {

using namespace blender::math;

/* -------------------------------------------------------------------- */
/** \name Planar Probe
 * \{ */

void PlanarProbe::set_view(const draw::View &view, int layer_id)
{
  /* Invert the up axis to avoid changing handedness (see #137022). */
  this->viewmat = from_scale<float4x4>(float3(1, -1, 1)) * view.viewmat() *
                  reflection_matrix_get();
  this->winmat = view.winmat();
  /* Invert Y offset in the projection matrix to compensate the flip above (see #141112). */
  this->winmat[2][1] = -this->winmat[2][1];

  this->wininv = invert(this->winmat);

  this->world_to_object_transposed = float3x4(transpose(world_to_plane));
  this->normal = normalize(plane_to_world.z_axis());

  float3 view_vec = view.is_persp() ? view.location() - plane_to_world.location() : view.forward();
  bool view_is_below_plane = dot(view_vec, plane_to_world.z_axis()) < 0.0;
  if (view_is_below_plane) {
    this->normal = -this->normal;
  }
  this->layer_id = layer_id;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Planar Probe Module
 * \{ */

void PlanarProbeModule::init()
{
  /* This triggers the compilation of clipped shader only if we can detect light-probe planes. */
  if (inst_.is_viewport()) {
    /* This check needs to happen upfront before sync, so we use the previous sync result. */
    update_probes_ = !inst_.light_probes.planar_map_.is_empty();
  }
  else {
    /* TODO(jbakker): should we check on the subtype as well? Now it also populates even when
     * there are other light probes in the scene. */
    update_probes_ = DEG_id_type_any_exists(inst_.depsgraph, ID_LP);
  }

  do_display_draw_ = false;
}

void PlanarProbeModule::end_sync()
{
  /* When first planar probes are enabled it can happen that the first sample is off. */
  if (!update_probes_ && !inst_.light_probes.planar_map_.is_empty()) {
    DRW_viewport_request_redraw();
  }
}

void PlanarProbeModule::set_view(const draw::View &main_view, int2 main_view_extent)
{
  GBuffer &gbuf = inst_.gbuffer;

  const int64_t num_probes = inst_.light_probes.planar_map_.size();

  /* TODO resolution percentage. */
  int2 extent = main_view_extent;
  int layer_count = num_probes;

  if (num_probes == 0) {
    /* Create valid dummy texture. */
    extent = int2(1);
    layer_count = 1;
  }

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_READ;
  radiance_tx_.ensure_2d_array(gpu::TextureFormat::UFLOAT_11_11_10, extent, layer_count, usage);
  depth_tx_.ensure_2d_array(gpu::TextureFormat::SFLOAT_32_DEPTH, extent, layer_count, usage);
  depth_tx_.ensure_layer_views();

  do_display_draw_ = inst_.draw_overlays && num_probes > 0;

  int resource_index = 0;
  int display_index = 0;
  for (PlanarProbe &probe : inst_.light_probes.planar_map_.values()) {
    if (resource_index == PLANAR_PROBE_MAX) {
      break;
    }

    PlanarResources &res = resources_[resource_index];

    /* TODO Cull out of view planars. */

    probe.set_view(main_view, resource_index);
    probe_planar_buf_[resource_index] = probe;

    res.view.sync(probe.viewmat, probe.winmat);

    world_clip_buf_.plane = probe.reflection_clip_plane_get();
    world_clip_buf_.push_update();

    gbuf.acquire(extent,
                 inst_.pipelines.deferred.header_layer_count(),
                 inst_.pipelines.deferred.closure_layer_count(),
                 inst_.pipelines.deferred.normal_layer_count());

    res.combined_fb.ensure(GPU_ATTACHMENT_TEXTURE_LAYER(depth_tx_, resource_index),
                           GPU_ATTACHMENT_TEXTURE_LAYER(radiance_tx_, resource_index));

    res.gbuffer_fb.ensure(GPU_ATTACHMENT_TEXTURE_LAYER(depth_tx_, resource_index),
                          GPU_ATTACHMENT_TEXTURE_LAYER(radiance_tx_, resource_index),
                          GPU_ATTACHMENT_TEXTURE_LAYER(gbuf.header_tx.layer_view(0), 0),
                          GPU_ATTACHMENT_TEXTURE_LAYER(gbuf.normal_tx.layer_view(0), 0),
                          GPU_ATTACHMENT_TEXTURE_LAYER(gbuf.closure_tx.layer_view(0), 0),
                          GPU_ATTACHMENT_TEXTURE_LAYER(gbuf.closure_tx.layer_view(1), 0));

    inst_.pipelines.planar.render(
        res.view, depth_tx_.layer_view(resource_index), res.gbuffer_fb, res.combined_fb, extent);

    if (do_display_draw_ && probe.viewport_display) {
      display_data_buf_.get_or_resize(display_index++) = {probe.plane_to_world, resource_index};
    }

    resource_index++;
  }

  gbuf.release();

  if (resource_index < PLANAR_PROBE_MAX) {
    /* Tag the end of the array. */
    probe_planar_buf_[resource_index].layer_id = -1;
  }
  probe_planar_buf_.push_update();

  do_display_draw_ = display_index > 0;
  if (do_display_draw_) {
    display_data_buf_.resize(display_index);
    display_data_buf_.push_update();
  }
}

void PlanarProbeModule::viewport_draw(View &view, gpu::FrameBuffer *view_fb)
{
  if (!do_display_draw_) {
    return;
  }

  viewport_display_ps_.init();
  viewport_display_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                 DRW_STATE_CLIP_CONTROL_UNIT_RANGE | inst_.film.depth.test_state |
                                 DRW_STATE_CULL_BACK);
  viewport_display_ps_.framebuffer_set(&view_fb);
  viewport_display_ps_.shader_set(inst_.shaders.static_shader_get(DISPLAY_PROBE_PLANAR));
  SphereProbeData &world_data = *static_cast<SphereProbeData *>(&inst_.light_probes.world_sphere_);
  viewport_display_ps_.push_constant("world_coord_packed",
                                     reinterpret_cast<int4 *>(&world_data.atlas_coord));
  viewport_display_ps_.bind_resources(*this);
  viewport_display_ps_.bind_resources(inst_.sphere_probes);
  viewport_display_ps_.bind_ssbo("display_data_buf", display_data_buf_);
  viewport_display_ps_.draw_procedural(GPU_PRIM_TRIS, 1, display_data_buf_.size() * 6);

  inst_.manager->submit(viewport_display_ps_, view);
}

/** \} */

}  // namespace blender::eevee
