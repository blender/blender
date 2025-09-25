/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_instance.hh"

#include "eevee_hizbuffer.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Hierarchical-Z buffer
 *
 * \{ */

void HiZBuffer::sync()
{
  int2 render_extent = inst_.film.render_extent_get();
  int2 probe_extent = int2(inst_.sphere_probes.probe_render_extent());
  /* Padding to avoid complexity during down-sampling and screen tracing. */
  int2 hiz_extent = math::ceil_to_multiple(math::max(render_extent, probe_extent),
                                           int2(1u << (HIZ_MIP_COUNT - 1)));
  int2 dispatch_size = math::divide_ceil(hiz_extent, int2(HIZ_GROUP_SIZE));

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE;
  for ([[maybe_unused]] const int i : IndexRange(hiz_tx_.size())) {
    hiz_tx_.current().ensure_2d(
        gpu::TextureFormat::SFLOAT_32, hiz_extent, usage, nullptr, HIZ_MIP_COUNT);
    hiz_tx_.current().ensure_mip_views();
    GPU_texture_mipmap_mode(hiz_tx_.current(), true, false);
    hiz_tx_.swap();
  }

  data_.uv_scale = float2(render_extent) / float2(hiz_extent);

  /* TODO(@fclem): There might be occasions where we might not want to
   * copy mip 0 for performance reasons if there is no need for it. */
  bool update_mip_0 = true;

  {
    PassSimple &pass = hiz_update_ps_;
    gpu::Shader *sh = inst_.shaders.static_shader_get(HIZ_UPDATE);
    pass.init();
    pass.specialize_constant(sh, "update_mip_0", update_mip_0);
    pass.shader_set(sh);
    pass.bind_ssbo("finished_tile_counter", atomic_tile_counter_);
    /* TODO(fclem): Should be a parameter to avoid confusion. */
    pass.bind_texture("depth_tx", &src_tx_);
    pass.bind_image("out_mip_0", &hiz_mip_ref_[0]);
    pass.bind_image("out_mip_1", &hiz_mip_ref_[1]);
    pass.bind_image("out_mip_2", &hiz_mip_ref_[2]);
    pass.bind_image("out_mip_3", &hiz_mip_ref_[3]);
    pass.bind_image("out_mip_4", &hiz_mip_ref_[4]);
    pass.bind_image("out_mip_5", &hiz_mip_ref_[5]);
    pass.bind_image("out_mip_6", &hiz_mip_ref_[6]);
    pass.dispatch(int3(dispatch_size, 1));
    pass.barrier(GPU_BARRIER_TEXTURE_FETCH);
  }
  {
    PassSimple &pass = hiz_update_layer_ps_;
    gpu::Shader *sh = inst_.shaders.static_shader_get(HIZ_UPDATE_LAYER);
    pass.init();
    pass.specialize_constant(sh, "update_mip_0", update_mip_0);
    pass.shader_set(sh);
    pass.bind_ssbo("finished_tile_counter", atomic_tile_counter_);
    /* TODO(fclem): Should be a parameter to avoid confusion. */
    pass.bind_texture("depth_layered_tx", &src_tx_);
    pass.bind_image("out_mip_0", &hiz_mip_ref_[0]);
    pass.bind_image("out_mip_1", &hiz_mip_ref_[1]);
    pass.bind_image("out_mip_2", &hiz_mip_ref_[2]);
    pass.bind_image("out_mip_3", &hiz_mip_ref_[3]);
    pass.bind_image("out_mip_4", &hiz_mip_ref_[4]);
    pass.bind_image("out_mip_5", &hiz_mip_ref_[5]);
    pass.bind_image("out_mip_6", &hiz_mip_ref_[6]);
    pass.push_constant("layer_id", &layer_id_);
    pass.dispatch(int3(dispatch_size, 1));
    pass.barrier(GPU_BARRIER_TEXTURE_FETCH);
  }

  if (inst_.debug_mode == eDebugMode::DEBUG_HIZ_VALIDATION) {
    debug_draw_ps_.init();
    debug_draw_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
    debug_draw_ps_.shader_set(inst_.shaders.static_shader_get(HIZ_DEBUG));
    debug_draw_ps_.bind_resources(this->front);
    debug_draw_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

void HiZBuffer::update()
{
  if (!is_dirty_) {
    return;
  }

  src_tx_ = *src_tx_ptr_;
  for (const int i : IndexRange(HIZ_MIP_COUNT)) {
    hiz_mip_ref_[i] = hiz_tx_.current().mip_view(i);
  }

  if (layer_id_ == -1) {
    inst_.manager->submit(hiz_update_ps_);
  }
  else {
    inst_.manager->submit(hiz_update_layer_ps_);
  }

  is_dirty_ = false;
}

void HiZBuffer::debug_draw(View &view, gpu::FrameBuffer *view_fb)
{
  if (inst_.debug_mode == eDebugMode::DEBUG_HIZ_VALIDATION) {
    inst_.info_append(
        "Debug Mode: HiZ Validation\n"
        " - Red: pixel in front of HiZ tile value.\n"
        " - Blue: No error.");
    inst_.hiz_buffer.update();
    GPU_framebuffer_bind(view_fb);
    inst_.manager->submit(debug_draw_ps_, view);
  }
}

/** \} */

}  // namespace blender::eevee
