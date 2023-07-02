/* SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "GPU_capabilities.h"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name AmbientOcclusion
 * \{ */

void AmbientOcclusion::init()
{
  render_pass_enabled_ = inst_.film.enabled_passes_get() & EEVEE_RENDER_PASS_AO;

  data_.distance = inst_.scene->eevee.gtao_distance;
  data_.quality = inst_.scene->eevee.gtao_quality;
  /* Size is multiplied by 2 because it is applied in NDC [-1..1] range. */
  data_.pixel_size = float2(2.0f) / float2(inst_.film.render_extent_get());

  data_.push_update();
}

void AmbientOcclusion::sync()
{
  if (!render_pass_enabled_) {
    return;
  }

  render_pass_ps_.init();
  render_pass_ps_.shader_set(inst_.shaders.static_shader_get(AMBIENT_OCCLUSION_PASS));

  render_pass_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, &inst_.pipelines.utility_tx);
  inst_.sampling.bind_resources(&render_pass_ps_);
  inst_.hiz_buffer.bind_resources(&render_pass_ps_);
  bind_resources(&render_pass_ps_);

  render_pass_ps_.bind_image("in_normal_img", &rp_normal_tx_);
  render_pass_ps_.bind_image("out_ao_img", &rp_ao_tx_);

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

  RenderBuffers &rb = inst_.render_buffers;

  rb.rp_color_tx.ensure_layer_views();
  rp_normal_tx_ = rb.rp_color_tx.layer_view(rb.data.normal_id);
  rb.rp_value_tx.ensure_layer_views();
  rp_ao_tx_ = rb.rp_value_tx.layer_view(rb.data.ambient_occlusion_id);

  inst_.manager->submit(render_pass_ps_, view);
}

}  // namespace blender::eevee
