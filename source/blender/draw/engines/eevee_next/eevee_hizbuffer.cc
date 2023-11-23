/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_global.h"

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
  /* Padding to avoid complexity during down-sampling and screen tracing. */
  int2 hiz_extent = math::ceil_to_multiple(render_extent, int2(1u << (HIZ_MIP_COUNT - 1)));
  int2 dispatch_size = math::divide_ceil(hiz_extent, int2(HIZ_GROUP_SIZE));

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE |
                           GPU_TEXTURE_USAGE_MIP_SWIZZLE_VIEW;
  hiz_tx_.ensure_2d(GPU_R32F, hiz_extent, usage, nullptr, HIZ_MIP_COUNT);
  hiz_tx_.ensure_mip_views();
  GPU_texture_mipmap_mode(hiz_tx_, true, false);

  data_.uv_scale = float2(render_extent) / float2(hiz_extent);

  {
    PassSimple &pass = hiz_update_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get(HIZ_UPDATE));
    pass.bind_ssbo("finished_tile_counter", atomic_tile_counter_);
    /* TODO(fclem): Should be a parameter to avoid confusion. */
    pass.bind_texture("depth_tx", &src_tx_, with_filter);
    pass.bind_image("out_mip_0", hiz_tx_.mip_view(0));
    pass.bind_image("out_mip_1", hiz_tx_.mip_view(1));
    pass.bind_image("out_mip_2", hiz_tx_.mip_view(2));
    pass.bind_image("out_mip_3", hiz_tx_.mip_view(3));
    pass.bind_image("out_mip_4", hiz_tx_.mip_view(4));
    pass.bind_image("out_mip_5", hiz_tx_.mip_view(5));
    pass.bind_image("out_mip_6", hiz_tx_.mip_view(6));
    /* TODO(@fclem): There might be occasions where we might not want to
     * copy mip 0 for performance reasons if there is no need for it. */
    pass.push_constant("update_mip_0", true);
    pass.dispatch(int3(dispatch_size, 1));
    pass.barrier(GPU_BARRIER_TEXTURE_FETCH);
  }
  {
    PassSimple &pass = hiz_update_layer_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get(HIZ_UPDATE_LAYER));
    pass.bind_ssbo("finished_tile_counter", atomic_tile_counter_);
    /* TODO(fclem): Should be a parameter to avoid confusion. */
    pass.bind_texture("depth_layered_tx", &src_tx_, with_filter);
    pass.bind_image("out_mip_0", hiz_tx_.mip_view(0));
    pass.bind_image("out_mip_1", hiz_tx_.mip_view(1));
    pass.bind_image("out_mip_2", hiz_tx_.mip_view(2));
    pass.bind_image("out_mip_3", hiz_tx_.mip_view(3));
    pass.bind_image("out_mip_4", hiz_tx_.mip_view(4));
    pass.bind_image("out_mip_5", hiz_tx_.mip_view(5));
    pass.bind_image("out_mip_6", hiz_tx_.mip_view(6));
    /* TODO(@fclem): There might be occasions where we might not want to
     * copy mip 0 for performance reasons if there is no need for it. */
    pass.push_constant("update_mip_0", true);
    pass.push_constant("layer_id", &layer_id_);
    pass.dispatch(int3(dispatch_size, 1));
    pass.barrier(GPU_BARRIER_TEXTURE_FETCH);
  }

  if (inst_.debug_mode == eDebugMode::DEBUG_HIZ_VALIDATION) {
    debug_draw_ps_.init();
    debug_draw_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
    debug_draw_ps_.shader_set(inst_.shaders.static_shader_get(HIZ_DEBUG));
    this->bind_resources(debug_draw_ps_);
    debug_draw_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

void HiZBuffer::update()
{
  if (!is_dirty_) {
    return;
  }

  src_tx_ = *src_tx_ptr_;

  if (layer_id_ == -1) {
    inst_.manager->submit(hiz_update_ps_);
  }
  else {
    inst_.manager->submit(hiz_update_layer_ps_);
  }

  is_dirty_ = false;
}

void HiZBuffer::debug_draw(View &view, GPUFrameBuffer *view_fb)
{
  if (inst_.debug_mode == eDebugMode::DEBUG_HIZ_VALIDATION) {
    inst_.info =
        "Debug Mode: HiZ Validation\n"
        " - Red: pixel in front of HiZ tile value.\n"
        " - Blue: No error.";
    inst_.hiz_buffer.update();
    GPU_framebuffer_bind(view_fb);
    inst_.manager->submit(debug_draw_ps_, view);
  }
}

/** \} */

}  // namespace blender::eevee
