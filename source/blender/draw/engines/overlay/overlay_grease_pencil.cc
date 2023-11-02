/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "ED_grease_pencil.hh"

#include "BKE_grease_pencil.hh"

#include "overlay_private.hh"

void OVERLAY_edit_grease_pencil_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const eAttrDomain selection_domain = ED_grease_pencil_selection_domain_get(
      draw_ctx->scene->toolsettings);

  GPUShader *sh;
  DRWShadingGroup *grp;

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                   DRW_STATE_BLEND_ALPHA;
  DRW_PASS_CREATE(psl->edit_grease_pencil_ps, (state | pd->clipping_state));

  sh = OVERLAY_shader_edit_particle_strand();
  grp = pd->edit_grease_pencil_wires_grp = DRW_shgroup_create(sh, psl->edit_grease_pencil_ps);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);

  if (selection_domain == ATTR_DOMAIN_POINT) {
    sh = OVERLAY_shader_edit_particle_point();
    grp = pd->edit_grease_pencil_points_grp = DRW_shgroup_create(sh, psl->edit_grease_pencil_ps);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  }
}

void OVERLAY_edit_grease_pencil_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  DRWShadingGroup *lines_grp = pd->edit_grease_pencil_wires_grp;
  if (lines_grp) {
    GPUBatch *geom_lines = DRW_cache_grease_pencil_edit_lines_get(draw_ctx->scene, ob);

    DRW_shgroup_call_no_cull(lines_grp, geom_lines, ob);
  }

  DRWShadingGroup *points_grp = pd->edit_grease_pencil_points_grp;
  if (points_grp) {
    GPUBatch *geom_points = DRW_cache_grease_pencil_edit_points_get(draw_ctx->scene, ob);
    DRW_shgroup_call_no_cull(points_grp, geom_points, ob);
  }
}

void OVERLAY_edit_grease_pencil_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  if (psl->edit_grease_pencil_ps) {
    DRW_draw_pass(psl->edit_grease_pencil_ps);
  }
}
