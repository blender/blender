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

#include "DNA_lightprobe_types.h"

#include "UI_resources.h"

#include "overlay_private.h"

void OVERLAY_outline_init(OVERLAY_Data *vedata)
{
  OVERLAY_FramebufferList *fbl = vedata->fbl;
  OVERLAY_TextureList *txl = vedata->txl;

  if (DRW_state_is_fbo()) {
    /* TODO only alloc if needed. */
    /* XXX TODO GPU_R16UI can overflow, it would cause no harm
     * (only bad colored or missing outlines) but we should
     * use 32bits only if the scene have that many objects */
    DRW_texture_ensure_fullscreen_2d(&txl->temp_depth_tx, GPU_DEPTH24_STENCIL8, 0);
    DRW_texture_ensure_fullscreen_2d(&txl->outlines_id_tx, GPU_R16UI, 0);
    GPU_framebuffer_ensure_config(
        &fbl->outlines_prepass_fb,
        {GPU_ATTACHMENT_TEXTURE(txl->temp_depth_tx), GPU_ATTACHMENT_TEXTURE(txl->outlines_id_tx)});

    for (int i = 0; i < 2; i++) {
      DRW_texture_ensure_fullscreen_2d(&txl->outlines_color_tx[i], GPU_RGBA8, DRW_TEX_FILTER);
      GPU_framebuffer_ensure_config(
          &fbl->outlines_process_fb[i],
          {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->outlines_color_tx[i])});
    }
  }
}

static int shgroup_theme_id_to_outline_id(int theme_id, const int base_flag)
{
  if (UNLIKELY(base_flag & BASE_FROM_DUPLI)) {
    switch (theme_id) {
      case TH_ACTIVE:
      case TH_SELECT:
        return 2;
      case TH_TRANSFORM:
        return 0;
      default:
        return -1;
    }
  }

  switch (theme_id) {
    case TH_ACTIVE:
      return 3;
    case TH_SELECT:
      return 1;
    case TH_TRANSFORM:
      return 0;
    default:
      return -1;
  }
}

static DRWShadingGroup *shgroup_theme_id_to_outline_or_null(OVERLAY_PrivateData *pd,
                                                            int theme_id,
                                                            const int base_flag)
{
  int outline_id = shgroup_theme_id_to_outline_id(theme_id, base_flag);
  switch (outline_id) {
    case 3: /* TH_ACTIVE */
      return pd->outlines_active_grp;
    case 2: /* Duplis */
      return pd->outlines_select_dupli_grp;
    case 1: /* TH_SELECT */
      return pd->outlines_select_grp;
    case 0: /* TH_TRANSFORM */
      return pd->outlines_transform_grp;
    default:
      return NULL;
  }
}

static DRWShadingGroup *shgroup_theme_id_to_probe_outline_or_null(OVERLAY_PrivateData *pd,
                                                                  int theme_id,
                                                                  const int base_flag)
{
  int outline_id = shgroup_theme_id_to_outline_id(theme_id, base_flag);
  switch (outline_id) {
    case 3: /* TH_ACTIVE */
      return pd->outlines_probe_active_grp;
    case 2: /* Duplis */
      return pd->outlines_probe_select_dupli_grp;
    case 1: /* TH_SELECT */
      return pd->outlines_probe_select_grp;
    case 0: /* TH_TRANSFORM */
      return pd->outlines_probe_transform_grp;
    default:
      return NULL;
  }
}

static DRWShadingGroup *outline_shgroup(DRWPass *pass, int outline_id, GPUShader *sh)
{
  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  DRW_shgroup_uniform_int_copy(grp, "outlineId", outline_id);
  return grp;
}

void OVERLAY_outline_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_TextureList *txl = vedata->txl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  DRWShadingGroup *grp = NULL;

  const float outline_width = UI_GetThemeValuef(TH_OUTLINE_WIDTH);
  const bool do_outline_expand = (U.pixelsize > 1.0) || (outline_width > 2.0f);
  const bool do_large_expand = ((U.pixelsize > 1.0) && (outline_width > 2.0f)) ||
                               (outline_width > 4.0f);
  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    DRW_PASS_CREATE(psl->outlines_prepass_ps, state | pd->clipping_state);

    GPUShader *sh_grid = OVERLAY_shader_outline_prepass_grid();
    GPUShader *sh_geom = OVERLAY_shader_outline_prepass(pd->xray_enabled_and_not_wire);
    GPUShader *sh = OVERLAY_shader_outline_prepass(false);

    pd->outlines_transform_grp = outline_shgroup(psl->outlines_prepass_ps, 0, sh_geom);
    pd->outlines_select_grp = outline_shgroup(psl->outlines_prepass_ps, 1, sh_geom);
    pd->outlines_select_dupli_grp = outline_shgroup(psl->outlines_prepass_ps, 2, sh_geom);
    pd->outlines_active_grp = outline_shgroup(psl->outlines_prepass_ps, 3, sh_geom);

    pd->outlines_probe_transform_grp = outline_shgroup(psl->outlines_prepass_ps, 0, sh);
    pd->outlines_probe_select_grp = outline_shgroup(psl->outlines_prepass_ps, 1, sh);
    pd->outlines_probe_select_dupli_grp = outline_shgroup(psl->outlines_prepass_ps, 2, sh);
    pd->outlines_probe_active_grp = outline_shgroup(psl->outlines_prepass_ps, 3, sh);

    pd->outlines_probe_grid_grp = grp = DRW_shgroup_create(sh_grid, psl->outlines_prepass_ps);
    DRW_shgroup_uniform_block_persistent(grp, "globalsBlock", G_draw.block_ubo);
  }

  /* outlines_prepass_ps is still needed for selection of probes. */
  if (!(pd->v3d_flag & V3D_SELECT_OUTLINE)) {
    return;
  }

  {
    DRW_PASS_CREATE(psl->outlines_detect_ps, DRW_STATE_WRITE_COLOR);
    DRW_PASS_CREATE(psl->outlines_expand_ps, DRW_STATE_WRITE_COLOR);
    DRW_PASS_CREATE(psl->outlines_bleed_ps, DRW_STATE_WRITE_COLOR);
    DRW_PASS_CREATE(psl->outlines_resolve_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA);

    GPUShader *sh = OVERLAY_shader_outline_detect(pd->xray_enabled_and_not_wire);

    grp = DRW_shgroup_create(sh, psl->outlines_detect_ps);
    /* Don't occlude the "outline" detection pass if in xray mode (too much flickering). */
    DRW_shgroup_uniform_float_copy(grp, "alphaOcclu", (pd->xray_enabled) ? 1.0f : 0.35f);
    DRW_shgroup_uniform_texture_ref(grp, "outlineId", &txl->outlines_id_tx);
    DRW_shgroup_uniform_texture_ref(grp, "outlineDepth", &txl->temp_depth_tx);
    DRW_shgroup_uniform_texture_ref(grp, "sceneDepth", &dtxl->depth);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

    if (do_outline_expand) {
      sh = OVERLAY_shader_outline_expand(do_large_expand);
      grp = DRW_shgroup_create(sh, psl->outlines_expand_ps);
      DRW_shgroup_uniform_texture_ref(grp, "outlineColor", &txl->outlines_color_tx[0]);
      DRW_shgroup_uniform_bool_copy(grp, "doExpand", true);
      DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

      sh = OVERLAY_shader_outline_expand(false);
      grp = DRW_shgroup_create(sh, psl->outlines_bleed_ps);
      DRW_shgroup_uniform_texture_ref(grp, "outlineColor", &txl->outlines_color_tx[1]);
      DRW_shgroup_uniform_bool_copy(grp, "doExpand", false);
      DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
    }
    else {
      sh = OVERLAY_shader_outline_expand(false);
      grp = DRW_shgroup_create(sh, psl->outlines_expand_ps);
      DRW_shgroup_uniform_texture_ref(grp, "outlineColor", &txl->outlines_color_tx[0]);
      DRW_shgroup_uniform_bool_copy(grp, "doExpand", false);
      DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
    }

    GPUTexture **outline_tx = &txl->outlines_color_tx[do_outline_expand ? 0 : 1];
    sh = OVERLAY_shader_outline_resolve();

    grp = DRW_shgroup_create(sh, psl->outlines_resolve_ps);
    DRW_shgroup_uniform_texture_ref(grp, "outlineBluredColor", outline_tx);
    DRW_shgroup_uniform_vec2_copy(grp, "rcpDimensions", DRW_viewport_invert_size_get());
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }
}

static void outline_lightprobe(OVERLAY_PrivateData *pd, Object *ob, ViewLayer *view_layer)
{
  DRWShadingGroup *grp;
  LightProbe *prb = (LightProbe *)ob->data;
  int theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);

  if (prb->type == LIGHTPROBE_TYPE_GRID) {
    float corner[3];
    float increment[3][3];
    /* Update transforms */
    float cell_dim[3], half_cell_dim[3];
    cell_dim[0] = 2.0f / (float)(prb->grid_resolution_x);
    cell_dim[1] = 2.0f / (float)(prb->grid_resolution_y);
    cell_dim[2] = 2.0f / (float)(prb->grid_resolution_z);

    mul_v3_v3fl(half_cell_dim, cell_dim, 0.5f);

    /* First cell. */
    copy_v3_fl(corner, -1.0f);
    add_v3_v3(corner, half_cell_dim);
    mul_m4_v3(ob->obmat, corner);

    /* Opposite neighbor cell. */
    copy_v3_fl3(increment[0], cell_dim[0], 0.0f, 0.0f);
    copy_v3_fl3(increment[1], 0.0f, cell_dim[1], 0.0f);
    copy_v3_fl3(increment[2], 0.0f, 0.0f, cell_dim[2]);

    for (int i = 0; i < 3; i++) {
      add_v3_v3(increment[i], half_cell_dim);
      add_v3_fl(increment[i], -1.0f);
      mul_m4_v3(ob->obmat, increment[i]);
      sub_v3_v3(increment[i], corner);
    }

    int outline_id = shgroup_theme_id_to_outline_id(theme_id, ob->base_flag);
    uint cell_count = prb->grid_resolution_x * prb->grid_resolution_y * prb->grid_resolution_z;
    grp = DRW_shgroup_create_sub(pd->outlines_probe_grid_grp);
    DRW_shgroup_uniform_int_copy(grp, "outlineId", outline_id);
    DRW_shgroup_uniform_vec3_copy(grp, "corner", corner);
    DRW_shgroup_uniform_vec3_copy(grp, "increment_x", increment[0]);
    DRW_shgroup_uniform_vec3_copy(grp, "increment_y", increment[1]);
    DRW_shgroup_uniform_vec3_copy(grp, "increment_z", increment[2]);
    DRW_shgroup_uniform_ivec3_copy(grp, "grid_resolution", &prb->grid_resolution_x);
    DRW_shgroup_call_procedural_points(grp, NULL, cell_count);
  }
  else if (prb->type == LIGHTPROBE_TYPE_PLANAR && (prb->flag & LIGHTPROBE_FLAG_SHOW_DATA)) {
    grp = shgroup_theme_id_to_probe_outline_or_null(pd, theme_id, ob->base_flag);
    DRW_shgroup_call_no_cull(grp, DRW_cache_quad_get(), ob);
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

  if (ob->type == OB_LIGHTPROBE) {
    outline_lightprobe(pd, ob, draw_ctx->view_layer);
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
      int theme_id = DRW_object_wire_theme_get(ob, draw_ctx->view_layer, NULL);
      shgroup = shgroup_theme_id_to_outline_or_null(pd, theme_id, ob->base_flag);
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
    GPU_framebuffer_clear_color_depth(fbl->outlines_prepass_fb, clearcol, 1.0f);
    DRW_draw_pass(psl->outlines_prepass_ps);

    /* Search outline pixels */
    GPU_framebuffer_bind(fbl->outlines_process_fb[0]);
    DRW_draw_pass(psl->outlines_detect_ps);

    /* Expand outline to form a 3px wide line */
    GPU_framebuffer_bind(fbl->outlines_process_fb[1]);
    DRW_draw_pass(psl->outlines_expand_ps);

    /* Bleed color so the AA can do it's stuff */
    GPU_framebuffer_bind(fbl->outlines_process_fb[0]);
    DRW_draw_pass(psl->outlines_bleed_ps);

    /* restore main framebuffer */
    GPU_framebuffer_bind(fbl->overlay_default_fb);
    DRW_draw_pass(psl->outlines_resolve_ps);

    DRW_stats_group_end();
  }
  else if (DRW_state_is_select()) {
    /* Render probes spheres/planes so we can select them. */
    DRW_draw_pass(psl->outlines_prepass_ps);
  }
}
