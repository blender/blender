/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2019, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#include "DRW_render.h"

#include "gpencil_engine.h"

#include "smaa_textures.h"

void GPENCIL_antialiasing_init(struct GPENCIL_Data *vedata)
{
  GPENCIL_PrivateData *pd = vedata->stl->pd;
  GPENCIL_FramebufferList *fbl = vedata->fbl;
  GPENCIL_TextureList *txl = vedata->txl;
  GPENCIL_PassList *psl = vedata->psl;
  DRWShadingGroup *grp;

  const float *size = DRW_viewport_size_get();
  const float *sizeinv = DRW_viewport_invert_size_get();
  float metrics[4] = {sizeinv[0], sizeinv[1], size[0], size[1]};

  if (pd->simplify_antialias) {
    /* No AA fallback. */
    DRW_PASS_CREATE(psl->smaa_resolve_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);

    GPUShader *sh = GPENCIL_shader_antialiasing(2);
    grp = DRW_shgroup_create(sh, psl->smaa_resolve_ps);
    DRW_shgroup_uniform_texture(grp, "blendTex", pd->color_tx);
    DRW_shgroup_uniform_texture(grp, "colorTex", pd->color_tx);
    DRW_shgroup_uniform_texture(grp, "revealTex", pd->reveal_tx);
    DRW_shgroup_uniform_bool_copy(grp, "doAntiAliasing", false);
    DRW_shgroup_uniform_bool_copy(grp, "onlyAlpha", pd->draw_wireframe);
    DRW_shgroup_uniform_vec4_copy(grp, "viewportMetrics", metrics);

    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
    return;
  }

  if (txl->smaa_search_tx == NULL) {
    txl->smaa_search_tx = GPU_texture_create_nD(SEARCHTEX_WIDTH,
                                                SEARCHTEX_HEIGHT,
                                                0,
                                                2,
                                                searchTexBytes,
                                                GPU_R8,
                                                GPU_DATA_UNSIGNED_BYTE,
                                                0,
                                                false,
                                                NULL);

    txl->smaa_area_tx = GPU_texture_create_nD(AREATEX_WIDTH,
                                              AREATEX_HEIGHT,
                                              0,
                                              2,
                                              areaTexBytes,
                                              GPU_RG8,
                                              GPU_DATA_UNSIGNED_BYTE,
                                              0,
                                              false,
                                              NULL);

    GPU_texture_filter_mode(txl->smaa_search_tx, true);
    GPU_texture_filter_mode(txl->smaa_area_tx, true);
  }

  {
    pd->smaa_edge_tx = DRW_texture_pool_query_2d(
        size[0], size[1], GPU_RG8, &draw_engine_gpencil_type);
    pd->smaa_weight_tx = DRW_texture_pool_query_2d(
        size[0], size[1], GPU_RGBA8, &draw_engine_gpencil_type);

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
    DRW_PASS_CREATE(psl->smaa_edge_ps, DRW_STATE_WRITE_COLOR);

    GPUShader *sh = GPENCIL_shader_antialiasing(0);
    grp = DRW_shgroup_create(sh, psl->smaa_edge_ps);
    DRW_shgroup_uniform_texture(grp, "colorTex", pd->color_tx);
    DRW_shgroup_uniform_texture(grp, "revealTex", pd->reveal_tx);
    DRW_shgroup_uniform_vec4_copy(grp, "viewportMetrics", metrics);
    DRW_shgroup_uniform_float_copy(
        grp, "lumaWeight", pd->scene->grease_pencil_settings.smaa_threshold);

    DRW_shgroup_clear_framebuffer(grp, GPU_COLOR_BIT, 0, 0, 0, 0, 0.0f, 0x0);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }
  {
    /* Stage 2: Blend Weight/Coord. */
    DRW_PASS_CREATE(psl->smaa_weight_ps, DRW_STATE_WRITE_COLOR);

    GPUShader *sh = GPENCIL_shader_antialiasing(1);
    grp = DRW_shgroup_create(sh, psl->smaa_weight_ps);
    DRW_shgroup_uniform_texture(grp, "edgesTex", pd->smaa_edge_tx);
    DRW_shgroup_uniform_texture(grp, "areaTex", txl->smaa_area_tx);
    DRW_shgroup_uniform_texture(grp, "searchTex", txl->smaa_search_tx);
    DRW_shgroup_uniform_vec4_copy(grp, "viewportMetrics", metrics);

    DRW_shgroup_clear_framebuffer(grp, GPU_COLOR_BIT, 0, 0, 0, 0, 0.0f, 0x0);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }
  {
    /* Stage 3: Resolve. */
    DRW_PASS_CREATE(psl->smaa_resolve_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);

    GPUShader *sh = GPENCIL_shader_antialiasing(2);
    grp = DRW_shgroup_create(sh, psl->smaa_resolve_ps);
    DRW_shgroup_uniform_texture(grp, "blendTex", pd->smaa_weight_tx);
    DRW_shgroup_uniform_texture(grp, "colorTex", pd->color_tx);
    DRW_shgroup_uniform_texture(grp, "revealTex", pd->reveal_tx);
    DRW_shgroup_uniform_bool_copy(grp, "doAntiAliasing", true);
    DRW_shgroup_uniform_bool_copy(grp, "onlyAlpha", pd->draw_wireframe);
    DRW_shgroup_uniform_vec4_copy(grp, "viewportMetrics", metrics);

    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }
}

void GPENCIL_antialiasing_draw(struct GPENCIL_Data *vedata)
{
  GPENCIL_FramebufferList *fbl = vedata->fbl;
  GPENCIL_PrivateData *pd = vedata->stl->pd;
  GPENCIL_PassList *psl = vedata->psl;

  if (!pd->simplify_antialias) {
    GPU_framebuffer_bind(fbl->smaa_edge_fb);
    DRW_draw_pass(psl->smaa_edge_ps);

    GPU_framebuffer_bind(fbl->smaa_weight_fb);
    DRW_draw_pass(psl->smaa_weight_ps);
  }

  GPU_framebuffer_bind(pd->scene_fb);
  DRW_draw_pass(psl->smaa_resolve_ps);
}
