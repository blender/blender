/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

void ShadingView::sync()
{
  int2 render_extent = inst_.film.render_extent_get();

  if (false /* inst_.camera.is_panoramic() */) {
    int64_t render_pixel_count = render_extent.x * int64_t(render_extent.y);
    /* Divide pixel count between the 6 views. Rendering to a square target. */
    extent_[0] = extent_[1] = ceilf(sqrtf(1 + (render_pixel_count / 6)));
    /* TODO(@fclem): Clip unused views here. */
    is_enabled_ = true;
  }
  else {
    extent_ = render_extent;
    /* Only enable -Z view. */
    is_enabled_ = (StringRefNull(name_) == "negZ_view");
  }

  if (!is_enabled_) {
    return;
  }

  /* Create views. */
  const CameraData &cam = inst_.camera.data_get();

  float4x4 viewmat, winmat;
  const float(*viewmat_p)[4] = viewmat.ptr(), (*winmat_p)[4] = winmat.ptr();
  if (false /* inst_.camera.is_panoramic() */) {
    /* TODO(@fclem) Over-scans. */
    /* For now a mandatory 5% over-scan for DoF. */
    float side = cam.clip_near * 1.05f;
    float near = cam.clip_near;
    float far = cam.clip_far;
    perspective_m4(winmat.ptr(), -side, side, -side, side, near, far);
    viewmat = face_matrix_ * cam.viewmat;
  }
  else {
    viewmat_p = cam.viewmat.ptr();
    winmat_p = cam.winmat.ptr();
  }

  main_view_ = DRW_view_create(viewmat_p, winmat_p, nullptr, nullptr, nullptr);
  sub_view_ = DRW_view_create_sub(main_view_, viewmat_p, winmat_p);
  render_view_ = DRW_view_create_sub(main_view_, viewmat_p, winmat_p);

  // dof_.sync(winmat_p, extent_);
  // rt_buffer_opaque_.sync(extent_);
  // rt_buffer_refract_.sync(extent_);
  // inst_.hiz_back.view_sync(extent_);
  // inst_.hiz_front.view_sync(extent_);
  // inst_.gbuffer.view_sync(extent_);
}

void ShadingView::render()
{
  if (!is_enabled_) {
    return;
  }

  /* Query temp textures and create frame-buffers. */
  RenderBuffers &rbufs = inst_.render_buffers;
  rbufs.acquire(extent_);
  combined_fb_.ensure(GPU_ATTACHMENT_TEXTURE(rbufs.depth_tx),
                      GPU_ATTACHMENT_TEXTURE(rbufs.combined_tx));
  prepass_fb_.ensure(GPU_ATTACHMENT_TEXTURE(rbufs.depth_tx),
                     GPU_ATTACHMENT_TEXTURE(rbufs.vector_tx));

  update_view();

  inst_.hiz_buffer.set_dirty();

  DRW_stats_group_start(name_);
  DRW_view_set_active(render_view_);

  /* If camera has any motion, compute motion vector in the film pass. Otherwise, we avoid float
   * precision issue by setting the motion of all static geometry to 0. */
  float4 clear_velocity = float4(inst_.velocity.camera_has_motion() ? VELOCITY_INVALID : 0.0f);

  GPU_framebuffer_bind(prepass_fb_);
  GPU_framebuffer_clear_color(prepass_fb_, clear_velocity);
  /* Alpha stores transmittance. So start at 1. */
  float4 clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
  GPU_framebuffer_bind(combined_fb_);
  GPU_framebuffer_clear_color_depth(combined_fb_, clear_color, 1.0f);

  inst_.pipelines.world.render(render_view_new_);

  /* TODO(fclem): Move it after the first prepass (and hiz update) once pipeline is stabilized. */
  inst_.lights.set_view(render_view_new_, extent_);

  inst_.pipelines.deferred.render(render_view_new_, prepass_fb_, combined_fb_, extent_);

  // inst_.lightprobes.draw_cache_display();

  // inst_.lookdev.render_overlay(view_fb_);

  inst_.pipelines.forward.render(render_view_new_, prepass_fb_, combined_fb_, rbufs.combined_tx);

  inst_.lights.debug_draw(render_view_new_, combined_fb_);
  inst_.hiz_buffer.debug_draw(render_view_new_, combined_fb_);
  inst_.shadows.debug_draw(render_view_new_, combined_fb_);
  inst_.irradiance_cache.viewport_draw(render_view_new_, combined_fb_);

  GPUTexture *combined_final_tx = render_postfx(rbufs.combined_tx);

  inst_.film.accumulate(sub_view_, combined_final_tx);

  rbufs.release();
  postfx_tx_.release();

  DRW_stats_group_end();
}

GPUTexture *ShadingView::render_postfx(GPUTexture *input_tx)
{
  if (!inst_.depth_of_field.postfx_enabled() && !inst_.motion_blur.postfx_enabled()) {
    return input_tx;
  }
  postfx_tx_.acquire(extent_, GPU_RGBA16F);

  GPUTexture *output_tx = postfx_tx_;

  /* Swapping is done internally. Actual output is set to the next input. */
  inst_.depth_of_field.render(render_view_new_, &input_tx, &output_tx, dof_buffer_);
  inst_.motion_blur.render(render_view_new_, &input_tx, &output_tx);

  return input_tx;
}

void ShadingView::update_view()
{
  float4x4 viewmat, winmat;
  DRW_view_viewmat_get(main_view_, viewmat.ptr(), false);
  DRW_view_winmat_get(main_view_, winmat.ptr(), false);

  /* TODO(fclem): Mixed-resolution rendering: We need to make sure we render with exactly the same
   * distances between pixels to line up render samples and target pixels.
   * So if the target resolution is not a multiple of the resolution divisor, we need to make the
   * projection window bigger in the +X and +Y directions. */

  /* Anti-Aliasing / Super-Sampling jitter. */
  float2 jitter = inst_.film.pixel_jitter_get() / float2(extent_);
  /* Transform to NDC space. */
  jitter *= 2.0f;

  window_translate_m4(winmat.ptr(), winmat.ptr(), UNPACK2(jitter));
  DRW_view_update_sub(sub_view_, viewmat.ptr(), winmat.ptr());

  /* FIXME(fclem): The offset may be noticeably large and the culling might make object pop
   * out of the blurring radius. To fix this, use custom enlarged culling matrix. */
  inst_.depth_of_field.jitter_apply(winmat, viewmat);
  DRW_view_update_sub(render_view_, viewmat.ptr(), winmat.ptr());

  render_view_new_.sync(viewmat, winmat);
}

/** \} */

}  // namespace blender::eevee
