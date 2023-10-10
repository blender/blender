/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_planar_probes.hh"
#include "eevee_instance.hh"

namespace blender::eevee {

using namespace blender::math;

/* -------------------------------------------------------------------- */
/** \name Planar Probe
 * \{ */

void PlanarProbe::sync(const float4x4 &world_to_object,
                       float clipping_offset,
                       float influence_distance)
{
  this->plane_to_world = float4x4(world_to_object);
  this->plane_to_world.z_axis() = normalize(this->plane_to_world.z_axis()) * influence_distance;
  this->world_to_plane = invert(this->plane_to_world);
  this->clipping_offset = clipping_offset;
}

void PlanarProbe::set_view(const draw::View &view, int layer_id)
{
  this->viewmat = view.viewmat() * reflection_matrix_get();
  this->winmat = view.winmat();
  this->world_to_object_transposed = float3x4(transpose(world_to_plane));
  this->normal = normalize(plane_to_world.z_axis());

  bool view_is_below_plane = dot(view.location() - plane_to_world.location(),
                                 plane_to_world.z_axis()) < 0.0;
  if (view_is_below_plane) {
    this->normal = -this->normal;
  }
  this->layer_id = layer_id;
}

float4x4 PlanarProbe::reflection_matrix_get()
{
  return plane_to_world * from_scale<float4x4>(float3(1, 1, -1)) * world_to_plane;
}

float4 PlanarProbe::reflection_clip_plane_get()
{
  return float4(-normal, dot(normal, plane_to_world.location()) - clipping_offset);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Planar Probe Module
 * \{ */

void PlanarProbeModule::init()
{
  if (assign_if_different(update_probes_, !probes_.is_empty())) {
    instance_.sampling.reset();
  }
}

void PlanarProbeModule::begin_sync()
{
  for (PlanarProbe &probe : probes_.values()) {
    probe.is_probe_used = false;
  }
}

void PlanarProbeModule::sync_object(Object *ob, ObjectHandle &ob_handle)
{
  const ::LightProbe *light_probe = (::LightProbe *)ob->data;
  if (light_probe->type != LIGHTPROBE_TYPE_PLANAR) {
    return;
  }

  PlanarProbe &probe = find_or_insert(ob_handle);
  probe.sync(float4x4(ob->object_to_world), light_probe->clipsta, light_probe->distinf);
  probe.is_probe_used = true;
}

void PlanarProbeModule::end_sync()
{
  probes_.remove_if(
      [](const PlanarProbes::MutableItem &item) { return !item.value.is_probe_used; });

  /* When first planar probes are enabled it can happen that the first sample is off. */
  if (!update_probes_ && !probes_.is_empty()) {
    DRW_viewport_request_redraw();
  }
}

void PlanarProbeModule::set_view(const draw::View &main_view, int2 main_view_extent)
{
  const int64_t num_probes = probes_.size();

  /* TODO resolution percentage. */
  int2 extent = main_view_extent;
  int layer_count = num_probes;

  if (num_probes == 0) {
    /* Create valid dummy texture. */
    extent = int2(1);
    layer_count = 1;
  }

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_READ;
  radiance_tx_.ensure_2d_array(GPU_R11F_G11F_B10F, extent, layer_count, usage);
  depth_tx_.ensure_2d_array(GPU_DEPTH_COMPONENT32F, extent, layer_count, usage);

  int resource_index = 0;
  for (PlanarProbe &probe : probes_.values()) {
    if (resource_index == PLANAR_PROBES_MAX) {
      break;
    }

    PlanarProbeResources &res = resources_[resource_index];

    /* TODO Cull out of view planars. */

    probe.set_view(main_view, resource_index);
    probe_planar_buf_[resource_index] = probe;

    res.view.sync(probe.viewmat, probe.winmat);

    world_clip_buf_.plane = probe.reflection_clip_plane_get();
    world_clip_buf_.push_update();

    res.combined_fb.ensure(GPU_ATTACHMENT_TEXTURE_LAYER(depth_tx_, resource_index),
                           GPU_ATTACHMENT_TEXTURE_LAYER(radiance_tx_, resource_index));

    instance_.pipelines.planar.render(res.view, res.combined_fb, resource_index, extent);

    resource_index++;
  }

  if (resource_index < PLANAR_PROBES_MAX) {
    /* Tag the end of the array. */
    probe_planar_buf_[resource_index].layer_id = -1;
  }
  probe_planar_buf_.push_update();
}

PlanarProbe &PlanarProbeModule::find_or_insert(ObjectHandle &ob_handle)
{
  PlanarProbe &planar_probe = probes_.lookup_or_add_default(ob_handle.object_key.hash());
  return planar_probe;
}

/** \} */

}  // namespace blender::eevee