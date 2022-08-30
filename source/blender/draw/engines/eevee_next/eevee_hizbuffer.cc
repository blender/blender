/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation.
 */

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

  hiz_tx_.ensure_2d(GPU_R32F, hiz_extent, nullptr, HIZ_MIP_COUNT);
  hiz_tx_.ensure_mip_views();
  GPU_texture_mipmap_mode(hiz_tx_, true, false);

  data_.uv_scale = float2(render_extent) / float2(hiz_extent);
  data_.push_update();

  {
    hiz_update_ps_ = DRW_pass_create("HizUpdate", DRW_STATE_NO_DRAW);
    GPUShader *sh = inst_.shaders.static_shader_get(HIZ_UPDATE);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, hiz_update_ps_);
    DRW_shgroup_storage_block(grp, "finished_tile_counter", atomic_tile_counter_);
    DRW_shgroup_uniform_texture_ref_ex(grp, "depth_tx", &render_buffers.depth_tx, with_filter);
    DRW_shgroup_uniform_image(grp, "out_mip_0", hiz_tx_.mip_view(0));
    DRW_shgroup_uniform_image(grp, "out_mip_1", hiz_tx_.mip_view(1));
    DRW_shgroup_uniform_image(grp, "out_mip_2", hiz_tx_.mip_view(2));
    DRW_shgroup_uniform_image(grp, "out_mip_3", hiz_tx_.mip_view(3));
    DRW_shgroup_uniform_image(grp, "out_mip_4", hiz_tx_.mip_view(4));
    DRW_shgroup_uniform_image(grp, "out_mip_5", hiz_tx_.mip_view(5));
    DRW_shgroup_uniform_image(grp, "out_mip_6", hiz_tx_.mip_view(6));
    DRW_shgroup_uniform_image(grp, "out_mip_7", hiz_tx_.mip_view(7));
    /* TODO(@fclem): There might be occasions where we might not want to
     * copy mip 0 for performance reasons if there is no need for it. */
    DRW_shgroup_uniform_bool_copy(grp, "update_mip_0", true);
    DRW_shgroup_call_compute(grp, UNPACK2(dispatch_size), 1);
    DRW_shgroup_barrier(grp, GPU_BARRIER_TEXTURE_FETCH);
  }

  if (inst_.debug_mode == eDebugMode::DEBUG_HIZ_VALIDATION) {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM;
    debug_draw_ps_ = DRW_pass_create("HizUpdate.Debug", state);
    GPUShader *sh = inst_.shaders.static_shader_get(HIZ_DEBUG);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, debug_draw_ps_);
    this->bind_resources(grp);
    DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);
  }
  else {
    debug_draw_ps_ = nullptr;
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

  DRW_draw_pass(hiz_update_ps_);

  if (G.debug & G_DEBUG_GPU) {
    GPU_framebuffer_bind(fb);
  }
}

void HiZBuffer::debug_draw(GPUFrameBuffer *view_fb)
{
  if (debug_draw_ps_ == nullptr) {
    return;
  }
  inst_.info = "Debug Mode: HiZ Validation";
  inst_.hiz_buffer.update();
  GPU_framebuffer_bind(view_fb);
  DRW_draw_pass(debug_draw_ps_);
}

/** \} */

}  // namespace blender::eevee
