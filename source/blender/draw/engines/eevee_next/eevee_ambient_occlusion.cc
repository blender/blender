/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Ground Truth Ambient Occlusion
 *
 * Based on Practical Realtime Strategies for Accurate Indirect Occlusion
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pptx
 *
 * Algorithm Overview:
 *
 * We separate the computation into 2 steps.
 *
 * - First we scan the neighborhood pixels to find the maximum horizon angle.
 *   We save this angle in a RG8 array texture.
 *
 * - Then we use this angle to compute occlusion with the shading normal at
 *   the shading stage. This let us do correct shadowing for each diffuse / specular
 *   lobe present in the shader using the correct normal.
 */

#include "eevee_ambient_occlusion.hh"
#include "eevee_instance.hh"

#include "GPU_capabilities.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name AmbientOcclusion
 * \{ */

void AmbientOcclusion::init()
{
  render_pass_enabled_ = inst_.film.enabled_passes_get() & EEVEE_RENDER_PASS_AO;

  const SceneEEVEE &sce_eevee = inst_.scene->eevee;

  data_.distance = sce_eevee.gtao_distance;
  data_.gi_distance = (sce_eevee.fast_gi_distance > 0.0f) ? sce_eevee.fast_gi_distance : 1e16f;
  data_.lod_factor = 1.0f / (1.0f + sce_eevee.gtao_quality * 4.0f);
  data_.thickness = sce_eevee.gtao_thickness;
  data_.angle_bias = 1.0 / max_ff(1e-8f, 1.0 - sce_eevee.gtao_focus);
  /* Size is multiplied by 2 because it is applied in NDC [-1..1] range. */
  data_.pixel_size = float2(2.0f) / float2(inst_.film.render_extent_get());
}

void AmbientOcclusion::sync()
{
  if (!render_pass_enabled_) {
    return;
  }

  render_pass_ps_.init();
  render_pass_ps_.shader_set(inst_.shaders.static_shader_get(AMBIENT_OCCLUSION_PASS));

  render_pass_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, &inst_.pipelines.utility_tx);
  render_pass_ps_.bind_resources(inst_.uniform_data);
  render_pass_ps_.bind_resources(inst_.sampling);
  render_pass_ps_.bind_resources(inst_.hiz_buffer.front);

  render_pass_ps_.bind_image("in_normal_img", &inst_.render_buffers.rp_color_tx);
  render_pass_ps_.push_constant("in_normal_img_layer_index", &inst_.render_buffers.data.normal_id);
  render_pass_ps_.bind_image("out_ao_img", &inst_.render_buffers.rp_value_tx);
  render_pass_ps_.push_constant("out_ao_img_layer_index",
                                &inst_.render_buffers.data.ambient_occlusion_id);

  render_pass_ps_.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS & GPU_BARRIER_TEXTURE_FETCH);
  render_pass_ps_.dispatch(
      math::divide_ceil(inst_.film.render_extent_get(), int2(AMBIENT_OCCLUSION_PASS_TILE_SIZE)));
}

void AmbientOcclusion::render_pass(View &view)
{
  if (!render_pass_enabled_) {
    return;
  }

  inst_.hiz_buffer.update();
  inst_.manager->submit(render_pass_ps_, view);
}

}  // namespace blender::eevee
