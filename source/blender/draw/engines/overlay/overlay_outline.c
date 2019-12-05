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
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "BKE_global.h"

#include "DNA_lightprobe_types.h"

#include "UI_resources.h"

#include "overlay_private.h"

void OVERLAY_outline_init(OVERLAY_Data *vedata)
{
  OVERLAY_FramebufferList *fbl = vedata->fbl;
  OVERLAY_TextureList *txl = vedata->txl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  if (DRW_state_is_fbo()) {
    /* TODO only alloc if needed. */
    DRW_texture_ensure_fullscreen_2d(&txl->temp_depth_tx, GPU_DEPTH24_STENCIL8, 0);
    DRW_texture_ensure_fullscreen_2d(&txl->outlines_id_tx, GPU_R16UI, 0);

    GPU_framebuffer_ensure_config(
        &fbl->outlines_prepass_fb,
        {GPU_ATTACHMENT_TEXTURE(txl->temp_depth_tx), GPU_ATTACHMENT_TEXTURE(txl->outlines_id_tx)});

    if (pd->antialiasing.enabled) {
      GPU_framebuffer_ensure_config(&fbl->outlines_resolve_fb,
                                    {GPU_ATTACHMENT_NONE,
                                     GPU_ATTACHMENT_TEXTURE(txl->overlay_color_tx),
                                     GPU_ATTACHMENT_TEXTURE(txl->overlay_line_tx)});
    }
    else {
      GPU_framebuffer_ensure_config(
          &fbl->outlines_resolve_fb,
          {GPU_ATTACHMENT_TEXTURE(txl->temp_depth_tx), GPU_ATTACHMENT_TEXTURE(dtxl->color)});
    }
  }
}

void OVERLAY_outline_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_TextureList *txl = vedata->txl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  DRWShadingGroup *grp = NULL;

  const float outline_width = UI_GetThemeValuef(TH_OUTLINE_WIDTH);
  const bool do_expand = (U.pixelsize > 1.0) || (outline_width > 2.0f);

  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    DRW_PASS_CREATE(psl->outlines_prepass_ps, state | pd->clipping_state);

    GPUShader *sh_geom = OVERLAY_shader_outline_prepass(pd->xray_enabled_and_not_wire);

    pd->outlines_grp = grp = DRW_shgroup_create(sh_geom, psl->outlines_prepass_ps);
    DRW_shgroup_uniform_bool_copy(grp, "isTransform", (G.moving & G_TRANSFORM_OBJ) != 0);
  }

  /* outlines_prepass_ps is still needed for selection of probes. */
  if (!(pd->v3d_flag & V3D_SELECT_OUTLINE)) {
    return;
  }

  {
    /* We can only do alpha blending with lineOutput just after clearing the buffer. */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL;
    DRW_PASS_CREATE(psl->outlines_detect_ps, state);

    GPUShader *sh = OVERLAY_shader_outline_detect();

    grp = DRW_shgroup_create(sh, psl->outlines_detect_ps);
    /* Don't occlude the "outline" detection pass if in xray mode (too much flickering). */
    DRW_shgroup_uniform_float_copy(grp, "alphaOcclu", (pd->xray_enabled) ? 1.0f : 0.35f);
    DRW_shgroup_uniform_bool_copy(grp, "doThickOutlines", do_expand);
    DRW_shgroup_uniform_bool_copy(grp, "doAntiAliasing", pd->antialiasing.enabled);
    DRW_shgroup_uniform_bool_copy(grp, "isXrayWires", pd->xray_enabled_and_not_wire);
    DRW_shgroup_uniform_texture_ref(grp, "outlineId", &txl->outlines_id_tx);
    DRW_shgroup_uniform_texture_ref(grp, "sceneDepth", &dtxl->depth);
    DRW_shgroup_uniform_texture_ref(grp, "outlineDepth", &txl->temp_depth_tx);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }
}

void OVERLAY_outline_cache_populate(OVERLAY_Data *vedata,
                                    Object *ob,
                                    OVERLAY_DupliData *dupli,
                                    bool init_dupli)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  struct GPUBatch *geom;
  DRWShadingGroup *shgroup = NULL;
  const bool draw_outline = ob->dt > OB_BOUNDBOX;

  /* Early exit: outlines of bounding boxes are not drawn. */
  if (!draw_outline) {
    return;
  }

  if (dupli && !init_dupli) {
    geom = dupli->outline_geom;
    shgroup = dupli->outline_shgrp;
  }
  else {
    /* This fixes only the biggest case which is a plane in ortho view. */
    int flat_axis = 0;
    bool is_flat_object_viewed_from_side = ((draw_ctx->rv3d->persp == RV3D_ORTHO) &&
                                            DRW_object_is_flat(ob, &flat_axis) &&
                                            DRW_object_axis_orthogonal_to_view(ob, flat_axis));

    if (pd->xray_enabled_and_not_wire || is_flat_object_viewed_from_side) {
      geom = DRW_cache_object_edge_detection_get(ob, NULL);
    }
    else {
      geom = DRW_cache_object_surface_get(ob);
    }

    if (geom) {
      shgroup = pd->outlines_grp;
    }
  }

  if (shgroup && geom) {
    DRW_shgroup_call(shgroup, geom, ob);
  }

  if (init_dupli) {
    dupli->outline_shgrp = shgroup;
    dupli->outline_geom = geom;
  }
}

void OVERLAY_outline_draw(OVERLAY_Data *vedata)
{
  OVERLAY_FramebufferList *fbl = vedata->fbl;
  OVERLAY_PassList *psl = vedata->psl;
  float clearcol[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  bool do_outlines = psl->outlines_prepass_ps != NULL &&
                     !DRW_pass_is_empty(psl->outlines_prepass_ps);

  if (DRW_state_is_fbo() && do_outlines) {
    DRW_stats_group_start("Outlines");

    /* Render filled polygon on a separate framebuffer */
    GPU_framebuffer_bind(fbl->outlines_prepass_fb);
    GPU_framebuffer_clear_color_depth_stencil(fbl->outlines_prepass_fb, clearcol, 1.0f, 0x00);
    DRW_draw_pass(psl->outlines_prepass_ps);

    /* Search outline pixels */
    GPU_framebuffer_bind(fbl->outlines_resolve_fb);
    DRW_draw_pass(psl->outlines_detect_ps);

    DRW_stats_group_end();
  }
}
