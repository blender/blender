/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_planar_probes.hh"
#include "eevee_instance.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Planar Probe Module
 * \{ */

void PlanarProbeModule::init()
{
  update_probes_ = !probes_.is_empty();
}

void PlanarProbeModule::begin_sync()
{
  for (PlanarProbe &planar_probe : probes_.values()) {
    planar_probe.is_probe_used = false;
  }
}

void PlanarProbeModule::sync_object(Object *ob, ObjectHandle &ob_handle)
{
  const ::LightProbe *light_probe = (::LightProbe *)ob->data;
  if (light_probe->type != LIGHTPROBE_TYPE_PLANAR) {
    return;
  }

  /* TODO Cull out of view planars. */

  PlanarProbe &probe = find_or_insert(ob_handle);
  probe.plane_to_world = float4x4(ob->object_to_world);
  probe.world_to_plane = float4x4(ob->world_to_object);
  probe.clipping_offset = light_probe->clipsta;
  probe.is_probe_used = true;
}

void PlanarProbeModule::end_sync()
{
  remove_unused_probes();

  // if (probes_.is_empty()) {
  //   update_probes_ = true;
  //   instance_.sampling.reset();
  // }
}

float4x4 PlanarProbeModule::reflection_matrix_get(const float4x4 &plane_to_world,
                                                  const float4x4 &world_to_plane)
{
  return math::normalize(plane_to_world) * math::from_scale<float4x4>(float3(1, 1, -1)) *
         math::normalize(world_to_plane);
}

float4 PlanarProbeModule::reflection_clip_plane_get(const float4x4 &plane_to_world,
                                                    float clip_offset)
{
  /* Compute clip plane equation / normal. */
  float4 plane_equation = float4(-math::normalize(plane_to_world.z_axis()));
  plane_equation.w = -math::dot(plane_equation.xyz(), plane_to_world.location());
  plane_equation.w -= clip_offset;
  return plane_equation;
}

void PlanarProbeModule::set_view(const draw::View &main_view, int2 main_view_extent)
{
  const int64_t num_probes = probes_.size();
  if (resources_.size() != num_probes) {
    resources_.reinitialize(num_probes);
  }

  /* TODO resolution percentage. */
  int2 extent = main_view_extent;

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_READ;
  color_tx_.ensure_2d_array(GPU_R11F_G11F_B10F, extent, num_probes, usage);
  depth_tx_.ensure_2d_array(GPU_DEPTH_COMPONENT32F, extent, num_probes, usage);

  int resource_index = 0;
  for (PlanarProbe &probe : probes_.values()) {
    PlanarProbeResources &res = resources_[resource_index];

    float4x4 winmat = main_view.winmat();
    float4x4 viewmat = main_view.viewmat();
    viewmat = viewmat * reflection_matrix_get(probe.plane_to_world, probe.world_to_plane);
    res.view.sync(viewmat, winmat);
    res.view.visibility_test(false);

    world_clip_buf_.plane = reflection_clip_plane_get(probe.plane_to_world, probe.clipping_offset);
    world_clip_buf_.push_update();

    res.combined_fb.ensure(GPU_ATTACHMENT_TEXTURE_LAYER(depth_tx_, resource_index),
                           GPU_ATTACHMENT_TEXTURE_LAYER(color_tx_, resource_index));

    instance_.pipelines.planar.render(res.view, res.combined_fb, main_view_extent);
  }
}

PlanarProbe &PlanarProbeModule::find_or_insert(ObjectHandle &ob_handle)
{
  PlanarProbe &planar_probe = probes_.lookup_or_add_default(ob_handle.object_key.hash());
  return planar_probe;
}

void PlanarProbeModule::remove_unused_probes()
{
  probes_.remove_if(
      [](const PlanarProbes::MutableItem &item) { return !item.value.is_probe_used; });
}

/** \} */

}  // namespace blender::eevee