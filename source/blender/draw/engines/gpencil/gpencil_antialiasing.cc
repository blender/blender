/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "DNA_scene_types.h"
#include "DRW_render.hh"

#include "gpencil_engine_private.hh"

#include "BLI_smaa_textures.h"

namespace blender::draw::gpencil {

void Instance::antialiasing_init()
{
  const float2 size_f = this->draw_ctx->viewport_size_get();
  const int2 size(size_f[0], size_f[1]);
  const float4 metrics = {1.0f / size[0], 1.0f / size[1], float(size[0]), float(size[1])};

  if (this->simplify_antialias) {
    /* No AA fallback. */
    PassSimple &pass = this->smaa_resolve_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
    pass.shader_set(ShaderCache::get().antialiasing[2].get());
    pass.bind_texture("blendTex", &this->color_tx);
    pass.bind_texture("colorTex", &this->color_tx);
    pass.bind_texture("revealTex", &this->reveal_tx);
    pass.push_constant("doAntiAliasing", false);
    pass.push_constant("onlyAlpha", this->draw_wireframe);
    pass.push_constant("viewportMetrics", metrics);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    return;
  }

  if (!this->smaa_search_tx.is_valid()) {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ;
    this->smaa_search_tx.ensure_2d(GPU_R8, int2(SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT), usage);
    GPU_texture_update(this->smaa_search_tx, GPU_DATA_UBYTE, searchTexBytes);

    this->smaa_area_tx.ensure_2d(GPU_RG8, int2(AREATEX_WIDTH, AREATEX_HEIGHT), usage);
    GPU_texture_update(this->smaa_area_tx, GPU_DATA_UBYTE, areaTexBytes);

    GPU_texture_filter_mode(this->smaa_search_tx, true);
    GPU_texture_filter_mode(this->smaa_area_tx, true);
  }

  {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    this->smaa_edge_tx.acquire(size, GPU_RG8, usage);
    this->smaa_weight_tx.acquire(size, GPU_RGBA8, usage);

    this->smaa_edge_fb.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(this->smaa_edge_tx));
    this->smaa_weight_fb.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(this->smaa_weight_tx));
  }

  {
    /* Stage 1: Edge detection. */
    PassSimple &pass = this->smaa_edge_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR);
    pass.shader_set(ShaderCache::get().antialiasing[0].get());
    pass.bind_texture("colorTex", &this->color_tx);
    pass.bind_texture("revealTex", &this->reveal_tx);
    pass.push_constant("viewportMetrics", metrics);
    pass.push_constant("lumaWeight", this->scene->grease_pencil_settings.smaa_threshold);
    pass.clear_color(float4(0.0f));
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
  {
    /* Stage 2: Blend Weight/Coord. */
    PassSimple &pass = this->smaa_weight_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR);
    pass.shader_set(ShaderCache::get().antialiasing[1].get());
    pass.bind_texture("edgesTex", &this->smaa_edge_tx);
    pass.bind_texture("areaTex", &this->smaa_area_tx);
    pass.bind_texture("searchTex", &this->smaa_search_tx);
    pass.push_constant("viewportMetrics", metrics);
    pass.clear_color(float4(0.0f));
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
  {
    /* Stage 3: Resolve. */
    PassSimple &pass = this->smaa_resolve_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
    pass.shader_set(ShaderCache::get().antialiasing[2].get());
    pass.bind_texture("blendTex", &this->smaa_weight_tx);
    pass.bind_texture("colorTex", &this->color_tx);
    pass.bind_texture("revealTex", &this->reveal_tx);
    pass.push_constant("doAntiAliasing", true);
    pass.push_constant("onlyAlpha", this->draw_wireframe);
    pass.push_constant("viewportMetrics", metrics);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

void Instance::antialiasing_draw(Manager &manager)
{
  if (!this->simplify_antialias) {
    GPU_framebuffer_bind(this->smaa_edge_fb);
    manager.submit(this->smaa_edge_ps);

    GPU_framebuffer_bind(this->smaa_weight_fb);
    manager.submit(this->smaa_weight_ps);
  }

  GPU_framebuffer_bind(this->scene_fb);
  manager.submit(this->smaa_resolve_ps);
}

}  // namespace blender::draw::gpencil
