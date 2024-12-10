/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "DRW_render.hh"

#include "gpencil_engine.h"

#include "BLI_smaa_textures.h"

void GPENCIL_antialiasing_init(GPENCIL_Data *vedata)
{
  GPENCIL_Instance *inst = vedata->instance;
  GPENCIL_PrivateData *pd = vedata->stl->pd;
  GPENCIL_FramebufferList *fbl = vedata->fbl;

  const float *size = DRW_viewport_size_get();
  const float *sizeinv = DRW_viewport_invert_size_get();
  const float4 metrics = {sizeinv[0], sizeinv[1], size[0], size[1]};

  if (pd->simplify_antialias) {
    /* No AA fallback. */
    blender::draw::PassSimple &pass = inst->smaa_resolve_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
    pass.shader_set(GPENCIL_shader_antialiasing(2));
    pass.bind_texture("blendTex", pd->color_tx);
    pass.bind_texture("colorTex", pd->color_tx);
    pass.bind_texture("revealTex", pd->reveal_tx);
    pass.push_constant("doAntiAliasing", false);
    pass.push_constant("onlyAlpha", pd->draw_wireframe);
    pass.push_constant("viewportMetrics", metrics);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    return;
  }

  if (!inst->smaa_search_tx.is_valid()) {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ;
    inst->smaa_search_tx.ensure_2d(GPU_R8, int2(SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT), usage);
    GPU_texture_update(inst->smaa_search_tx, GPU_DATA_UBYTE, searchTexBytes);

    inst->smaa_area_tx.ensure_2d(GPU_RG8, int2(AREATEX_WIDTH, AREATEX_HEIGHT), usage);
    GPU_texture_update(inst->smaa_area_tx, GPU_DATA_UBYTE, areaTexBytes);

    GPU_texture_filter_mode(inst->smaa_search_tx, true);
    GPU_texture_filter_mode(inst->smaa_area_tx, true);
  }

  {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    pd->smaa_edge_tx = DRW_texture_pool_query_2d_ex(
        size[0], size[1], GPU_RG8, usage, &draw_engine_gpencil_type);
    pd->smaa_weight_tx = DRW_texture_pool_query_2d_ex(
        size[0], size[1], GPU_RGBA8, usage, &draw_engine_gpencil_type);

    GPU_framebuffer_ensure_config(&fbl->smaa_edge_fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(pd->smaa_edge_tx),
                                  });

    GPU_framebuffer_ensure_config(&fbl->smaa_weight_fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(pd->smaa_weight_tx),
                                  });
  }

  {
    /* Stage 1: Edge detection. */
    blender::draw::PassSimple &pass = inst->smaa_edge_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR);
    pass.shader_set(GPENCIL_shader_antialiasing(0));
    pass.bind_texture("colorTex", pd->color_tx);
    pass.bind_texture("revealTex", pd->reveal_tx);
    pass.push_constant("viewportMetrics", metrics);
    pass.push_constant("lumaWeight", pd->scene->grease_pencil_settings.smaa_threshold);
    pass.clear_color(float4(0.0f));
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
  {
    /* Stage 2: Blend Weight/Coord. */
    blender::draw::PassSimple &pass = inst->smaa_weight_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR);
    pass.shader_set(GPENCIL_shader_antialiasing(1));
    pass.bind_texture("edgesTex", pd->smaa_edge_tx);
    pass.bind_texture("areaTex", inst->smaa_area_tx);
    pass.bind_texture("searchTex", inst->smaa_search_tx);
    pass.push_constant("viewportMetrics", metrics);
    pass.clear_color(float4(0.0f));
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
  {
    /* Stage 3: Resolve. */
    blender::draw::PassSimple &pass = inst->smaa_resolve_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
    pass.shader_set(GPENCIL_shader_antialiasing(2));
    pass.bind_texture("blendTex", pd->smaa_weight_tx);
    pass.bind_texture("colorTex", pd->color_tx);
    pass.bind_texture("revealTex", pd->reveal_tx);
    pass.push_constant("doAntiAliasing", true);
    pass.push_constant("onlyAlpha", pd->draw_wireframe);
    pass.push_constant("viewportMetrics", metrics);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

void GPENCIL_antialiasing_draw(GPENCIL_Data *vedata)
{
  GPENCIL_Instance *inst = vedata->instance;

  blender::draw::Manager *manager = DRW_manager_get();

  GPENCIL_FramebufferList *fbl = vedata->fbl;
  GPENCIL_PrivateData *pd = vedata->stl->pd;

  if (!pd->simplify_antialias) {
    GPU_framebuffer_bind(fbl->smaa_edge_fb);
    manager->submit(inst->smaa_edge_ps);

    GPU_framebuffer_bind(fbl->smaa_weight_fb);
    manager->submit(inst->smaa_weight_ps);
  }

  GPU_framebuffer_bind(pd->scene_fb);
  manager->submit(inst->smaa_resolve_ps);
}
