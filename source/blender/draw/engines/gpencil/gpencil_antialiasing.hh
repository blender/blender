/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BKE_gpencil_legacy.h"
#include "BKE_image.h"
#include "DRW_gpu_wrapper.hh"
#include "DRW_render.h"

#include "draw_manager.hh"
#include "draw_pass.hh"

#include "gpencil_shader.hh"

#include "BLI_smaa_textures.h"

namespace blender::draw::greasepencil {

using namespace draw;

/** Final anti-aliasing post processing and compositing on top of render. */
class AntiAliasing {
 private:
  ShaderModule &shaders_;

  Texture smaa_search_tx_ = {"smaa_search",
                             GPU_R8,
                             GPU_TEXTURE_USAGE_SHADER_READ,
                             int2(SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT)};
  Texture smaa_area_tx_ = {
      "smaa_area", GPU_RG8, GPU_TEXTURE_USAGE_SHADER_READ, int2(AREATEX_WIDTH, AREATEX_HEIGHT)};

  TextureFromPool edge_detect_tx_ = {"edge_detect_tx"};
  Framebuffer edge_detect_fb_ = {"edge_detect_fb"};
  PassSimple edge_detect_ps_ = {"edge_detect_ps"};

  TextureFromPool blend_weight_tx_ = {"blend_weight_tx"};
  Framebuffer blend_weight_fb_ = {"blend_weight_fb"};
  PassSimple blend_weight_ps_ = {"blend_weight_ps"};

  Framebuffer output_fb_ = {"output_fb"};
  PassSimple resolve_ps_ = {"resolve_ps"};

  bool draw_wireframe_ = false;
  float luma_weight_ = 1.0f;
  bool anti_aliasing_enabled_ = true;

 public:
  AntiAliasing(ShaderModule &shaders) : shaders_(shaders)
  {
    GPU_texture_update(smaa_search_tx_, GPU_DATA_UBYTE, searchTexBytes);
    GPU_texture_update(smaa_area_tx_, GPU_DATA_UBYTE, areaTexBytes);

    GPU_texture_filter_mode(smaa_search_tx_, true);
    GPU_texture_filter_mode(smaa_area_tx_, true);
  }

  void init(const View3D *v3d, const Scene *scene)
  {
    if (v3d) {
      draw_wireframe_ = (v3d->shading.type == OB_WIRE);
    }

    luma_weight_ = scene->grease_pencil_settings.smaa_threshold;
    anti_aliasing_enabled_ = true;  // GPENCIL_SIMPLIFY_AA(scene);
  }

  void begin_sync(TextureFromPool &color_tx, TextureFromPool &reveal_tx)
  {
    /* TODO(fclem): No global access. */
    const float *size = DRW_viewport_size_get();
    const float *sizeinv = DRW_viewport_invert_size_get();
    const float4 metrics = {sizeinv[0], sizeinv[1], size[0], size[1]};

    anti_aliasing_pass(color_tx, reveal_tx, metrics);

    /* Resolve pass. */
    PassSimple &pass = resolve_ps_;
    pass.init();
    pass.framebuffer_set(&output_fb_);
    pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
    pass.shader_set(shaders_.static_shader_get(ANTIALIASING_RESOLVE));
    /** \note use color_tx as dummy if AA is diabled. */
    pass.bind_texture("blendTex", anti_aliasing_enabled_ ? &blend_weight_tx_ : &color_tx);
    pass.bind_texture("colorTex", &color_tx);
    pass.bind_texture("revealTex", &reveal_tx);
    pass.push_constant("doAntiAliasing", anti_aliasing_enabled_);
    pass.push_constant("onlyAlpha", draw_wireframe_);
    pass.push_constant("viewportMetrics", metrics);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }

  void draw(Manager &manager, GPUTexture *dst_color_tx)
  {
    int2 render_size = {GPU_texture_width(dst_color_tx), GPU_texture_height(dst_color_tx)};

    DRW_stats_group_start("Anti-Aliasing");

    if (anti_aliasing_enabled_) {
      edge_detect_tx_.acquire(render_size, GPU_RG8);
      edge_detect_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(edge_detect_tx_));
      manager.submit(edge_detect_ps_);

      blend_weight_tx_.acquire(render_size, GPU_RGBA8);
      blend_weight_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(blend_weight_tx_));
      manager.submit(blend_weight_ps_);
      edge_detect_tx_.release();
    }

    output_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(dst_color_tx));
    manager.submit(resolve_ps_);
    blend_weight_tx_.release();

    DRW_stats_group_end();
  }

 private:
  void anti_aliasing_pass(TextureFromPool &color_tx,
                          TextureFromPool &reveal_tx,
                          const float4 metrics)
  {
    if (!anti_aliasing_enabled_) {
      return;
    }

    /* Stage 1: Edge detection. */
    edge_detect_ps_.init();
    edge_detect_ps_.framebuffer_set(&edge_detect_fb_);
    edge_detect_ps_.state_set(DRW_STATE_WRITE_COLOR);
    edge_detect_ps_.shader_set(shaders_.static_shader_get(ANTIALIASING_EDGE_DETECT));
    edge_detect_ps_.bind_texture("colorTex", &color_tx);
    edge_detect_ps_.bind_texture("revealTex", &reveal_tx);
    edge_detect_ps_.push_constant("viewportMetrics", metrics);
    edge_detect_ps_.push_constant("lumaWeight", luma_weight_);
    edge_detect_ps_.clear_color(float4(0.0f));
    edge_detect_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);

    /* Stage 2: Blend Weight/Coord. */
    blend_weight_ps_.init();
    blend_weight_ps_.framebuffer_set(&blend_weight_fb_);
    blend_weight_ps_.state_set(DRW_STATE_WRITE_COLOR);
    blend_weight_ps_.shader_set(shaders_.static_shader_get(ANTIALIASING_BLEND_WEIGHT));
    blend_weight_ps_.bind_texture("edgesTex", &edge_detect_tx_);
    blend_weight_ps_.bind_texture("areaTex", smaa_area_tx_);
    blend_weight_ps_.bind_texture("searchTex", smaa_search_tx_);
    blend_weight_ps_.push_constant("viewportMetrics", metrics);
    blend_weight_ps_.clear_color(float4(0.0f));
    blend_weight_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
};

}  // namespace blender::draw::greasepencil