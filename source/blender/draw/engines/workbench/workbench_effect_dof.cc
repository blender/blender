/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Depth of Field Effect:
 *
 * We use a gather approach by sampling a lowres version of the color buffer.
 * The process can be summarized like this:
 * - down-sample the color buffer using a COC (Circle of Confusion) aware down-sample algorithm.
 * - do a gather pass using the COC computed in the previous pass.
 * - do a median filter to reduce noise amount.
 * - composite on top of main color buffer.
 *
 * This is done after all passes and affects every surfaces.
 */

#include "workbench_private.hh"

#include "BKE_camera.h"
#include "DEG_depsgraph_query.hh"

namespace blender::workbench {
/**
 * Transform [-1..1] square to unit circle.
 */
static void square_to_circle(float x, float y, float &r, float &T)
{
  if (x > -y) {
    if (x > y) {
      r = x;
      T = M_PI_4 * (y / x);
    }
    else {
      r = y;
      T = M_PI_4 * (2 - (x / y));
    }
  }
  else {
    if (x < y) {
      r = -x;
      T = M_PI_4 * (4 + (y / x));
    }
    else {
      r = -y;
      if (y != 0) {
        T = M_PI_4 * (6 - (x / y));
      }
      else {
        T = 0.0f;
      }
    }
  }
}

void DofPass::setup_samples()
{
  float4 *sample = samples_buf_.begin();
  for (int i = 0; i <= kernel_radius_; i++) {
    for (int j = -kernel_radius_; j <= kernel_radius_; j++) {
      for (int k = -kernel_radius_; k <= kernel_radius_; k++) {
        if (abs(j) > i || abs(k) > i) {
          continue;
        }
        if (abs(j) < i && abs(k) < i) {
          continue;
        }

        float2 coord = float2(j, k) / float2(kernel_radius_);
        float r = 0;
        float T = 0;
        square_to_circle(coord.x, coord.y, r, T);
        sample->z = r;

        /* Bokeh shape parameterization. */
        if (blades_ > 1.0f) {
          float denom = T - (2.0 * M_PI / blades_) * floorf((blades_ * T + M_PI) / (2.0 * M_PI));
          r *= math::cos(M_PI / blades_) / math::cos(denom);
        }

        T += rotation_;

        sample->x = r * math::cos(T) * ratio_;
        sample->y = r * math::sin(T);
        sample->w = 0;
        sample++;
      }
    }
  }
  samples_buf_.push_update();
}

void DofPass::init(const SceneState &scene_state)
{
  enabled_ = scene_state.draw_dof;

  if (!enabled_) {
    source_tx_.free();
    coc_halfres_tx_.free();
    return;
  }

  offset_ = scene_state.sample / float(scene_state.samples_len);

  int2 half_res = scene_state.resolution / 2;
  half_res = {max_ii(half_res.x, 1), max_ii(half_res.y, 1)};

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ;
  source_tx_.ensure_2d(GPU_RGBA16F, half_res, usage, nullptr, 3);
  source_tx_.ensure_mip_views();
  source_tx_.filter_mode(true);
  coc_halfres_tx_.ensure_2d(GPU_RG8, half_res, usage, nullptr, 3);
  coc_halfres_tx_.ensure_mip_views();
  coc_halfres_tx_.filter_mode(true);

  const Camera *camera = scene_state.camera;

  /* Parameters */
  float fstop = camera->dof.aperture_fstop;
  float sensor = BKE_camera_sensor_size(camera->sensor_fit, camera->sensor_x, camera->sensor_y);
  float focus_dist = BKE_camera_object_dof_distance(scene_state.camera_object);
  float focal_len = camera->lens;

  /* TODO(fclem): De-duplicate with EEVEE. */
  const float scale_camera = 0.001f;
  /* We want radius here for the aperture number. */
  float aperture = 0.5f * scale_camera * focal_len / fstop;
  float focal_len_scaled = scale_camera * focal_len;
  float sensor_scaled = scale_camera * sensor;

  if (RegionView3D *rv3d = DRW_context_state_get()->rv3d) {
    sensor_scaled *= rv3d->viewcamtexcofac[0];
  }

  aperture_size_ = aperture * fabsf(focal_len_scaled / (focus_dist - focal_len_scaled));
  distance_ = -focus_dist;
  invsensor_size_ = scene_state.resolution.x / sensor_scaled;

  near_ = -camera->clip_start;
  far_ = -camera->clip_end;

  float blades = camera->dof.aperture_blades;
  float rotation = camera->dof.aperture_rotation;
  float ratio = 1.0f / camera->dof.aperture_ratio;

  if (blades_ != blades || rotation_ != rotation || ratio_ != ratio) {
    blades_ = blades;
    rotation_ = rotation;
    ratio_ = ratio;
    setup_samples();
  }
}

void DofPass::sync(SceneResources &resources)
{
  if (!enabled_) {
    return;
  }

  GPUSamplerState sampler_state = {GPU_SAMPLER_FILTERING_LINEAR | GPU_SAMPLER_FILTERING_MIPMAP};

  down_ps_.init();
  down_ps_.state_set(DRW_STATE_WRITE_COLOR);
  down_ps_.shader_set(ShaderCache::get().dof_prepare.get());
  down_ps_.bind_texture("sceneColorTex", &resources.color_tx);
  down_ps_.bind_texture("sceneDepthTex", &resources.depth_tx);
  down_ps_.push_constant("invertedViewportSize", float2(DRW_viewport_invert_size_get()));
  down_ps_.push_constant("dofParams", float3(aperture_size_, distance_, invsensor_size_));
  down_ps_.push_constant("nearFar", float2(near_, far_));
  down_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  down2_ps_.init();
  down2_ps_.state_set(DRW_STATE_WRITE_COLOR);
  down2_ps_.shader_set(ShaderCache::get().dof_downsample.get());
  down2_ps_.bind_texture("sceneColorTex", &source_tx_, sampler_state);
  down2_ps_.bind_texture("inputCocTex", &coc_halfres_tx_, sampler_state);
  down2_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  blur_ps_.init();
  blur_ps_.state_set(DRW_STATE_WRITE_COLOR);
  blur_ps_.shader_set(ShaderCache::get().dof_blur1.get());
  blur_ps_.bind_ubo("samples", samples_buf_);
  blur_ps_.bind_texture("noiseTex", resources.jitter_tx);
  blur_ps_.bind_texture("inputCocTex", &coc_halfres_tx_, sampler_state);
  blur_ps_.bind_texture("halfResColorTex", &source_tx_, sampler_state);
  blur_ps_.push_constant("invertedViewportSize", float2(DRW_viewport_invert_size_get()));
  blur_ps_.push_constant("noiseOffset", offset_);
  blur_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  blur2_ps_.init();
  blur2_ps_.state_set(DRW_STATE_WRITE_COLOR);
  blur2_ps_.shader_set(ShaderCache::get().dof_blur2.get());
  blur2_ps_.bind_texture("inputCocTex", &coc_halfres_tx_, sampler_state);
  blur2_ps_.bind_texture("blurTex", &blur_tx_);
  blur2_ps_.push_constant("invertedViewportSize", float2(DRW_viewport_invert_size_get()));
  blur2_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  resolve_ps_.init();
  resolve_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
  resolve_ps_.shader_set(ShaderCache::get().dof_resolve.get());
  resolve_ps_.bind_texture("halfResColorTex", &source_tx_, sampler_state);
  resolve_ps_.bind_texture("sceneDepthTex", &resources.depth_tx);
  resolve_ps_.push_constant("invertedViewportSize", float2(DRW_viewport_invert_size_get()));
  resolve_ps_.push_constant("dofParams", float3(aperture_size_, distance_, invsensor_size_));
  resolve_ps_.push_constant("nearFar", float2(near_, far_));
  resolve_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void DofPass::draw(Manager &manager, View &view, SceneResources &resources, int2 resolution)
{
  if (!enabled_) {
    return;
  }

  DRW_stats_group_start("Depth Of Field");

  int2 half_res = {max_ii(resolution.x / 2, 1), max_ii(resolution.y / 2, 1)};
  blur_tx_.acquire(
      half_res, GPU_RGBA16F, GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT);

  downsample_fb_.ensure(GPU_ATTACHMENT_NONE,
                        GPU_ATTACHMENT_TEXTURE(source_tx_),
                        GPU_ATTACHMENT_TEXTURE(coc_halfres_tx_));
  downsample_fb_.bind();
  manager.submit(down_ps_, view);

  struct CallbackData {
    Manager &manager;
    View &view;
    PassSimple &pass;
  };
  CallbackData callback_data = {manager, view, down2_ps_};

  auto downsample_level = [](void *callback_data, int /*level*/) {
    CallbackData *cd = static_cast<CallbackData *>(callback_data);
    cd->manager.submit(cd->pass, cd->view);
  };

  GPU_framebuffer_recursive_downsample(
      downsample_fb_, 2, downsample_level, static_cast<void *>(&callback_data));

  blur1_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(blur_tx_));
  blur1_fb_.bind();
  manager.submit(blur_ps_, view);

  blur2_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(source_tx_));
  blur2_fb_.bind();
  manager.submit(blur2_ps_, view);

  resolve_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(resources.color_tx));
  resolve_fb_.bind();
  manager.submit(resolve_ps_, view);

  blur_tx_.release();

  DRW_stats_group_end();
}

bool DofPass::is_enabled()
{
  return enabled_;
}

}  // namespace blender::workbench
