/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "BKE_grease_pencil.hh"

#include "overlay_private.hh"

void OVERLAY_edit_grease_pencil_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  GPUShader *sh;
  DRWShadingGroup *grp;

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                   DRW_STATE_BLEND_ALPHA;
  DRW_PASS_CREATE(psl->edit_grease_pencil_ps, (state | pd->clipping_state));

  sh = OVERLAY_shader_edit_particle_point();
  grp = pd->edit_grease_pencil_points_grp = DRW_shgroup_create(sh, psl->edit_grease_pencil_ps);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
}

void OVERLAY_edit_grease_pencil_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  DRWShadingGroup *points_grp = pd->edit_grease_pencil_points_grp;
  if (points_grp) {
    GPUBatch *geom = DRW_cache_grease_pencil_edit_points_get(ob, pd->cfra);
    DRW_shgroup_call_no_cull(points_grp, geom, ob);
  }
}

void OVERLAY_edit_grease_pencil_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  if (psl->edit_grease_pencil_ps) {
    DRW_draw_pass(psl->edit_grease_pencil_ps);
  }
}
