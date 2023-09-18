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
  RenderBuffers &render_buffers = inst_.render_buffers;

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
    hiz_update_ps_.init();
    hiz_update_ps_.shader_set(inst_.shaders.static_shader_get(HIZ_UPDATE));
    hiz_update_ps_.bind_ssbo("finished_tile_counter", atomic_tile_counter_);
    hiz_update_ps_.bind_texture("depth_tx", &render_buffers.depth_tx, with_filter);
    hiz_update_ps_.bind_image("out_mip_0", hiz_tx_.mip_view(0));
    hiz_update_ps_.bind_image("out_mip_1", hiz_tx_.mip_view(1));
    hiz_update_ps_.bind_image("out_mip_2", hiz_tx_.mip_view(2));
    hiz_update_ps_.bind_image("out_mip_3", hiz_tx_.mip_view(3));
    hiz_update_ps_.bind_image("out_mip_4", hiz_tx_.mip_view(4));
    hiz_update_ps_.bind_image("out_mip_5", hiz_tx_.mip_view(5));
    hiz_update_ps_.bind_image("out_mip_6", hiz_tx_.mip_view(6));
    hiz_update_ps_.bind_image("out_mip_7", hiz_tx_.mip_view(7));
    /* TODO(@fclem): There might be occasions where we might not want to
     * copy mip 0 for performance reasons if there is no need for it. */
    hiz_update_ps_.push_constant("update_mip_0", true);
    hiz_update_ps_.dispatch(int3(dispatch_size, 1));
    hiz_update_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH);
  }

  if (inst_.debug_mode == eDebugMode::DEBUG_HIZ_VALIDATION) {
    debug_draw_ps_.init();
    debug_draw_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
    debug_draw_ps_.shader_set(inst_.shaders.static_shader_get(HIZ_DEBUG));
    this->bind_resources(&debug_draw_ps_);
    debug_draw_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

void HiZBuffer::update()
{
  if (!is_dirty_) {
    return;
  }

  /* Bind another framebuffer in order to avoid triggering the feedback loop check.
   * This is safe because we only use compute shaders in this section of the code.
   * Ideally the check should be smarter. */
  GPUFrameBuffer *fb = GPU_framebuffer_active_get();
  if (G.debug & G_DEBUG_GPU) {
    GPU_framebuffer_restore();
  }

  inst_.manager->submit(hiz_update_ps_);

  if (G.debug & G_DEBUG_GPU) {
    GPU_framebuffer_bind(fb);
  }
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
