/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * A view is either:
 * - The entire main view.
 * - A fragment of the main view (for panoramic projections).
 * - A shadow map view.
 * - A light-probe view (either planar, cube-map, irradiance grid).
 *
 * A pass is a container for scene data. It is view agnostic but has specific logic depending on
 * its type. Passes are shared between views.
 */

#include "BKE_global.h"
#include "DRW_render.h"

#include "eevee_instance.hh"

#include "eevee_view.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name ShadingView
 * \{ */

void ShadingView::init()
{
  // dof_.init();
  // mb_.init();
}

void ShadingView::sync(int2 render_extent_)
{
  if (false /* inst_.camera.is_panoramic() */) {
    int64_t render_pixel_count = render_extent_.x * (int64_t)render_extent_.y;
    /* Divide pixel count between the 6 views. Rendering to a square target. */
    extent_[0] = extent_[1] = ceilf(sqrtf(1 + (render_pixel_count / 6)));
    /* TODO(@fclem): Clip unused views here. */
    is_enabled_ = true;
  }
  else {
    extent_ = render_extent_;
    /* Only enable -Z view. */
    is_enabled_ = (StringRefNull(name_) == "negZ_view");
  }

  if (!is_enabled_) {
    return;
  }

  /* Create views. */
  // const CameraData &data = inst_.camera.data_get();

  float4x4 viewmat, winmat;
  const float(*viewmat_p)[4] = viewmat.ptr(), (*winmat_p)[4] = winmat.ptr();
#if 0
  if (false /* inst_.camera.is_panoramic() */) {
    /* TODO(@fclem) Over-scans. */
    /* For now a mandatory 5% over-scan for DoF. */
    float side = data.clip_near * 1.05f;
    float near = data.clip_near;
    float far = data.clip_far;
    perspective_m4(winmat.ptr(), -side, side, -side, side, near, far);
    viewmat = face_matrix_ * data.viewmat;
  }
  else {
    viewmat_p = data.viewmat.ptr();
    winmat_p = data.winmat.ptr();
  }
#else
  /* TEMP */
  UNUSED_VARS(face_matrix_);
  const DRWView *default_view = DRW_view_default_get();
  DRW_view_winmat_get(default_view, winmat.ptr(), false);
  DRW_view_viewmat_get(default_view, viewmat.ptr(), false);
#endif

  main_view_ = DRW_view_create(viewmat_p, winmat_p, nullptr, nullptr, nullptr);
  sub_view_ = DRW_view_create_sub(main_view_, viewmat_p, winmat_p);
  render_view_ = DRW_view_create_sub(main_view_, viewmat_p, winmat_p);

  // dof_.sync(winmat_p, extent_);
  // mb_.sync(extent_);
  // velocity_.sync(extent_);
  // rt_buffer_opaque_.sync(extent_);
  // rt_buffer_refract_.sync(extent_);
  // inst_.hiz_back.view_sync(extent_);
  // inst_.hiz_front.view_sync(extent_);
  // inst_.gbuffer.view_sync(extent_);

  combined_tx_.sync();
  postfx_tx_.sync();
}

void ShadingView::render()
{
  if (!is_enabled_) {
    return;
  }

  /* Query temp textures and create framebuffers. */
  /* HACK: View name should be unique and static.
   * With this, we can reuse the same texture across views. */
  DrawEngineType *owner = (DrawEngineType *)name_;

  depth_tx_.ensure_2d(GPU_DEPTH24_STENCIL8, extent_);
  combined_tx_.acquire(extent_, GPU_RGBA16F, owner);
  view_fb_.ensure(GPU_ATTACHMENT_TEXTURE(depth_tx_), GPU_ATTACHMENT_TEXTURE(combined_tx_));

  update_view();

  DRW_stats_group_start(name_);
  // DRW_view_set_active(render_view_);

  /* Alpha stores transmittance. So start at 1. */
  float4 clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
  // GPU_framebuffer_bind(view_fb_);
  // GPU_framebuffer_clear_color_depth(view_fb_, clear_color, 1.0f);
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  GPU_framebuffer_bind(dfbl->default_fb);
  GPU_framebuffer_clear_color_depth(dfbl->default_fb, clear_color, 1.0f);

  inst_.pipelines.world.render();

  // inst_.pipelines.deferred.render(
  //     render_view_, rt_buffer_opaque_, rt_buffer_refract_, depth_tx_, combined_tx_);

  // inst_.lightprobes.draw_cache_display();

  // inst_.lookdev.render_overlay(view_fb_);

  inst_.pipelines.forward.render(render_view_, depth_tx_, combined_tx_);

  // inst_.lights.debug_draw(view_fb_);
  // inst_.shadows.debug_draw(view_fb_);

  // velocity_.render(depth_tx_);

  // if (inst_.render_passes.vector) {
  //   inst_.render_passes.vector->accumulate(velocity_.camera_vectors_get(), sub_view_);
  // }

  // GPUTexture *final_radiance_tx = render_post(combined_tx_);

  // if (inst_.render_passes.combined) {
  //   inst_.render_passes.combined->accumulate(final_radiance_tx, sub_view_);
  // }

  // if (inst_.render_passes.depth) {
  //   inst_.render_passes.depth->accumulate(depth_tx_, sub_view_);
  // }

  DRW_stats_group_end();

  combined_tx_.release();
  postfx_tx_.release();
}

GPUTexture *ShadingView::render_post(GPUTexture *input_tx)
{
#if 0
  if (!dof_.postfx_enabled() && !mb_.enabled()) {
    return input_tx;
  }
  /* HACK: View name should be unique and static.
   * With this, we can reuse the same texture across views. */
  postfx_tx_.acquire(extent_, GPU_RGBA16F, (void *)name_);

  GPUTexture *velocity_tx = velocity_.view_vectors_get();
  GPUTexture *output_tx = postfx_tx_;

  /* Swapping is done internally. Actual output is set to the next input. */
  dof_.render(depth_tx_, &input_tx, &output_tx);
  mb_.render(depth_tx_, velocity_tx, &input_tx, &output_tx);
#endif
  return input_tx;
}

void ShadingView::update_view()
{
  float4x4 viewmat, winmat;
  DRW_view_viewmat_get(main_view_, viewmat.ptr(), false);
  DRW_view_winmat_get(main_view_, winmat.ptr(), false);

  /* Anti-Aliasing / Super-Sampling jitter. */
  // float jitter_u = 2.0f * (inst_.sampling.rng_get(SAMPLING_FILTER_U) - 0.5f) / extent_[0];
  // float jitter_v = 2.0f * (inst_.sampling.rng_get(SAMPLING_FILTER_V) - 0.5f) / extent_[1];

  // window_translate_m4(winmat.ptr(), winmat.ptr(), jitter_u, jitter_v);
  DRW_view_update_sub(sub_view_, viewmat.ptr(), winmat.ptr());

  /* FIXME(fclem): The offset may be is noticeably large and the culling might make object pop
   * out of the blurring radius. To fix this, use custom enlarged culling matrix. */
  // dof_.jitter_apply(winmat, viewmat);
  DRW_view_update_sub(render_view_, viewmat.ptr(), winmat.ptr());

  // inst_.lightprobes.set_view(render_view_, extent_);
  // inst_.lights.set_view(render_view_, extent_, !inst_.use_scene_lights());
}

/** \} */

}  // namespace blender::eevee
