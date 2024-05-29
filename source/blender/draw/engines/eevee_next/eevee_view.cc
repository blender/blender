/* SPDX-FileCopyrightText: 2021 Blender Authors
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

#include "BKE_global.hh"
#include "DRW_render.hh"

#include "eevee_instance.hh"

#include "eevee_view.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name ShadingView
 * \{ */

void ShadingView::init() {}

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
  if (false /* inst_.camera.is_panoramic() */) {
    /* TODO(@fclem) Over-scans. */
    /* For now a mandatory 5% over-scan for DoF. */
    float side = cam.clip_near * 1.05f;
    float near = cam.clip_near;
    float far = cam.clip_far;
    winmat = math::projection::perspective(-side, side, -side, side, near, far);
    viewmat = face_matrix_ * cam.viewmat;
  }
  else {
    viewmat = cam.viewmat;
    winmat = cam.winmat;
  }

  main_view_.sync(viewmat, winmat);
}

void ShadingView::render()
{
  if (!is_enabled_) {
    return;
  }

  update_view();

  DRW_stats_group_start(name_);

  /* Needs to be before planar_probes because it needs correct crypto-matte & render-pass buffers
   * to reuse the same deferred shaders. */
  RenderBuffers &rbufs = inst_.render_buffers;
  rbufs.acquire(extent_);

  /* Needs to be before anything else because it query its own gbuffer. */
  inst_.planar_probes.set_view(render_view_, extent_);

  combined_fb_.ensure(GPU_ATTACHMENT_TEXTURE(rbufs.depth_tx),
                      GPU_ATTACHMENT_TEXTURE(rbufs.combined_tx));
  prepass_fb_.ensure(GPU_ATTACHMENT_TEXTURE(rbufs.depth_tx),
                     GPU_ATTACHMENT_TEXTURE(rbufs.vector_tx));

  GBuffer &gbuf = inst_.gbuffer;
  gbuf.acquire(extent_,
               inst_.pipelines.deferred.closure_layer_count(),
               inst_.pipelines.deferred.normal_layer_count());

  gbuffer_fb_.ensure(GPU_ATTACHMENT_TEXTURE(rbufs.depth_tx),
                     GPU_ATTACHMENT_TEXTURE(rbufs.combined_tx),
                     GPU_ATTACHMENT_TEXTURE(gbuf.header_tx),
                     GPU_ATTACHMENT_TEXTURE_LAYER(gbuf.normal_tx.layer_view(0), 0),
                     GPU_ATTACHMENT_TEXTURE_LAYER(gbuf.closure_tx.layer_view(0), 0),
                     GPU_ATTACHMENT_TEXTURE_LAYER(gbuf.closure_tx.layer_view(1), 0));

  /* If camera has any motion, compute motion vector in the film pass. Otherwise, we avoid float
   * precision issue by setting the motion of all static geometry to 0. */
  float4 clear_velocity = float4(inst_.velocity.camera_has_motion() ? VELOCITY_INVALID : 0.0f);

  GPU_framebuffer_bind(prepass_fb_);
  GPU_framebuffer_clear_color(prepass_fb_, clear_velocity);
  /* Alpha stores transmittance. So start at 1. */
  float4 clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
  GPU_framebuffer_bind(combined_fb_);
  GPU_framebuffer_clear_color_depth(combined_fb_, clear_color, 1.0f);

  /* TODO(fclem): Move it after the first prepass (and hiz update) once pipeline is stabilized. */
  inst_.lights.set_view(render_view_, extent_);

  inst_.pipelines.background.render(render_view_);

  inst_.hiz_buffer.set_source(&inst_.render_buffers.depth_tx);

  inst_.volume.draw_prepass(main_view_);

  /* TODO(Miguel Pozo): Deferred and forward prepass should happen before the GBuffer pass. */
  inst_.pipelines.deferred.render(main_view_,
                                  render_view_,
                                  prepass_fb_,
                                  combined_fb_,
                                  gbuffer_fb_,
                                  extent_,
                                  rt_buffer_opaque_,
                                  rt_buffer_refract_);

  inst_.gbuffer.release();

  inst_.volume.draw_compute(main_view_, extent_);

  // inst_.lookdev.render_overlay(view_fb_);

  inst_.pipelines.forward.render(render_view_, prepass_fb_, combined_fb_, extent_);

  render_transparent_pass(rbufs);

  inst_.lights.debug_draw(render_view_, combined_fb_);
  inst_.hiz_buffer.debug_draw(render_view_, combined_fb_);
  inst_.shadows.debug_draw(render_view_, combined_fb_);
  inst_.volume_probes.viewport_draw(render_view_, combined_fb_);
  inst_.sphere_probes.viewport_draw(render_view_, combined_fb_);
  inst_.planar_probes.viewport_draw(render_view_, combined_fb_);

  inst_.ambient_occlusion.render_pass(render_view_);

  GPUTexture *combined_final_tx = render_postfx(rbufs.combined_tx);
  inst_.film.accumulate(jitter_view_, combined_final_tx);

  rbufs.release();
  postfx_tx_.release();

  DRW_stats_group_end();
}

void ShadingView::render_transparent_pass(RenderBuffers &rbufs)
{
  if (rbufs.data.transparent_id != -1) {
    transparent_fb_.ensure(
        GPU_ATTACHMENT_TEXTURE(rbufs.depth_tx),
        GPU_ATTACHMENT_TEXTURE_LAYER(rbufs.rp_color_tx, rbufs.data.transparent_id));
    /* Alpha stores transmittance. So start at 1. */
    float4 clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
    GPU_framebuffer_bind(transparent_fb_);
    GPU_framebuffer_clear_color(transparent_fb_, clear_color);
    inst_.pipelines.forward.render(render_view_, prepass_fb_, transparent_fb_, rbufs.extent_get());
  }
}

GPUTexture *ShadingView::render_postfx(GPUTexture *input_tx)
{
  if (!inst_.depth_of_field.postfx_enabled() && !inst_.motion_blur.postfx_enabled()) {
    return input_tx;
  }
  postfx_tx_.acquire(extent_, GPU_RGBA16F);

  /* Fix a sync bug on AMD + Mesa when volume + motion blur create artifacts
   * except if there is a clear event between them. */
  if (inst_.volume.enabled() && inst_.motion_blur.postfx_enabled() &&
      !inst_.depth_of_field.postfx_enabled() &&
      GPU_type_matches_ex(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OFFICIAL, GPU_BACKEND_OPENGL))
  {
    postfx_tx_.clear(float4(0.0f));
  }

  GPUTexture *output_tx = postfx_tx_;

  /* Swapping is done internally. Actual output is set to the next input. */
  inst_.depth_of_field.render(render_view_, &input_tx, &output_tx, dof_buffer_);
  inst_.motion_blur.render(render_view_, &input_tx, &output_tx);

  return input_tx;
}

void ShadingView::update_view()
{
  float4x4 viewmat = main_view_.viewmat();
  float4x4 winmat = main_view_.winmat();

  /*
   * Mixed resolution rendering. The center of the display pixels must align with the center of
   * the render pixels. If they don't align, the winmat needs to be re-projected.
   */
  int2 scaling_factor = int2(inst_.film.scaling_factor_get());
  int2 display_extent = inst_.film.display_extent_get();
  int overscan = inst_.film.get_data().overscan;
  int2 rescaled_render_extent = (extent_ - 2 * overscan) * scaling_factor;

  if (rescaled_render_extent != display_extent) {
    float left;
    float right;
    float bottom;
    float top;
    float near;
    float far;
    const bool is_perspective = main_view_.is_persp();
    projmat_dimensions(winmat.ptr(), &left, &right, &bottom, &top, &near, &far);
    float2 scale = (float2(rescaled_render_extent) / float2(display_extent));
    right = left + ((right - left) * scale.x);
    top = bottom + ((top - bottom) * scale.y);

    if (is_perspective) {
      winmat = math::projection::perspective(left, right, bottom, top, near, far);
    }
    else {
      winmat = math::projection::orthographic(left, right, bottom, top, near, far);
    }
  }

  /* Anti-Aliasing / Super-Sampling jitter. */
  float2 jitter = inst_.film.pixel_jitter_get() / float2(extent_);
  /* Transform to NDC space. */
  jitter *= 2.0f;

  window_translate_m4(winmat.ptr(), winmat.ptr(), UNPACK2(jitter));
  jitter_view_.sync(viewmat, winmat);

  /* FIXME(fclem): The offset may be noticeably large and the culling might make object pop
   * out of the blurring radius. To fix this, use custom enlarged culling matrix. */
  inst_.depth_of_field.jitter_apply(winmat, viewmat);
  render_view_.sync(viewmat, winmat);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Capture View
 * \{ */

void CaptureView::render_world()
{
  const auto update_info = inst_.sphere_probes.world_update_info_pop();
  if (!update_info.has_value()) {
    return;
  }

  View view = {"Capture.View"};
  GPU_debug_group_begin("World.Capture");

  if (update_info->do_render) {
    for (int face : IndexRange(6)) {
      float4x4 view_m4 = cubeface_mat(face);
      float4x4 win_m4 = math::projection::perspective(-update_info->clipping_distances.x,
                                                      update_info->clipping_distances.x,
                                                      -update_info->clipping_distances.x,
                                                      update_info->clipping_distances.x,
                                                      update_info->clipping_distances.x,
                                                      update_info->clipping_distances.y);
      view.sync(view_m4, win_m4);

      combined_fb_.ensure(GPU_ATTACHMENT_NONE,
                          GPU_ATTACHMENT_TEXTURE_CUBEFACE(inst_.sphere_probes.cubemap_tx_, face));
      GPU_framebuffer_bind(combined_fb_);
      inst_.pipelines.world.render(view);
    }

    inst_.sphere_probes.remap_to_octahedral_projection(update_info->atlas_coord, true);
  }

  GPU_debug_group_end();
}

void CaptureView::render_probes()
{
  Framebuffer prepass_fb;
  View view = {"Capture.View"};
  while (const auto update_info = inst_.sphere_probes.probe_update_info_pop()) {
    GPU_debug_group_begin("Probe.Capture");

    if (!inst_.pipelines.data.is_sphere_probe) {
      inst_.pipelines.data.is_sphere_probe = true;
      inst_.uniform_data.push_update();
    }

    int2 extent = int2(update_info->cube_target_extent);
    inst_.render_buffers.acquire(extent);

    inst_.render_buffers.vector_tx.clear(float4(0.0f));
    prepass_fb.ensure(GPU_ATTACHMENT_TEXTURE(inst_.render_buffers.depth_tx),
                      GPU_ATTACHMENT_TEXTURE(inst_.render_buffers.vector_tx));

    inst_.gbuffer.acquire(extent,
                          inst_.pipelines.probe.closure_layer_count(),
                          inst_.pipelines.probe.normal_layer_count());

    for (int face : IndexRange(6)) {
      float4x4 view_m4 = cubeface_mat(face);
      view_m4 = math::translate(view_m4, -update_info->probe_pos);
      float4x4 win_m4 = math::projection::perspective(-update_info->clipping_distances.x,
                                                      update_info->clipping_distances.x,
                                                      -update_info->clipping_distances.x,
                                                      update_info->clipping_distances.x,
                                                      update_info->clipping_distances.x,
                                                      update_info->clipping_distances.y);
      view.sync(view_m4, win_m4);

      combined_fb_.ensure(GPU_ATTACHMENT_TEXTURE(inst_.render_buffers.depth_tx),
                          GPU_ATTACHMENT_TEXTURE_CUBEFACE(inst_.sphere_probes.cubemap_tx_, face));

      gbuffer_fb_.ensure(GPU_ATTACHMENT_TEXTURE(inst_.render_buffers.depth_tx),
                         GPU_ATTACHMENT_TEXTURE_CUBEFACE(inst_.sphere_probes.cubemap_tx_, face),
                         GPU_ATTACHMENT_TEXTURE(inst_.gbuffer.header_tx),
                         GPU_ATTACHMENT_TEXTURE_LAYER(inst_.gbuffer.normal_tx.layer_view(0), 0),
                         GPU_ATTACHMENT_TEXTURE_LAYER(inst_.gbuffer.closure_tx.layer_view(0), 0),
                         GPU_ATTACHMENT_TEXTURE_LAYER(inst_.gbuffer.closure_tx.layer_view(1), 0));

      GPU_framebuffer_bind(combined_fb_);
      GPU_framebuffer_clear_color_depth(combined_fb_, float4(0.0f, 0.0f, 0.0f, 1.0f), 1.0f);
      inst_.pipelines.probe.render(view, prepass_fb, combined_fb_, gbuffer_fb_, extent);
    }

    inst_.render_buffers.release();
    inst_.gbuffer.release();
    GPU_debug_group_end();
    inst_.sphere_probes.remap_to_octahedral_projection(update_info->atlas_coord, false);
  }

  if (inst_.pipelines.data.is_sphere_probe) {
    inst_.pipelines.data.is_sphere_probe = false;
    inst_.uniform_data.push_update();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lookdev View
 * \{ */

void LookdevView::render()
{
  if (!inst_.lookdev.enabled_) {
    return;
  }
  GPU_debug_group_begin("Lookdev");

  const float4x4 &view_m4 = inst_.camera.data_get().viewmat;
  const float sphere_scale = inst_.lookdev.sphere_scale;
  const float clip_near = inst_.camera.data_get().clip_near;
  float4x4 win_m4 = math::projection::orthographic(-sphere_scale,
                                                   sphere_scale,
                                                   -sphere_scale,
                                                   sphere_scale,
                                                   clip_near - sphere_scale,
                                                   clip_near + sphere_scale);
  view_.sync(view_m4, win_m4);

  inst_.lookdev.draw(view_);
  inst_.lookdev.display();

  GPU_debug_group_end();
}

/** \} */

}  // namespace blender::eevee
