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

#include "BKE_paint.h"
#include "DRW_render.h"

#include "overlay_private.h"

void OVERLAY_facing_init(OVERLAY_Data *UNUSED(vedata))
{
}

void OVERLAY_facing_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  for (int i = 0; i < 2; i++) {
    /* Non Meshes Pass (Camera, empties, lights ...) */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND_ALPHA;
    DRW_PASS_CREATE(psl->facing_ps[i], state | pd->clipping_state);

    GPUShader *sh = OVERLAY_shader_facing();
    pd->facing_grp[i] = DRW_shgroup_create(sh, psl->facing_ps[i]);
    DRW_shgroup_uniform_block(pd->facing_grp[i], "globalsBlock", G_draw.block_ubo);
  }

  if (!pd->use_in_front) {
    pd->facing_grp[IN_FRONT] = pd->facing_grp[NOT_IN_FRONT];
  }
}

void OVERLAY_facing_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  if (pd->xray_enabled) {
    return;
  }

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->v3d) &&
                               !DRW_state_is_image_render();
  const bool is_xray = (ob->dtx & OB_DRAWXRAY) != 0;

  if (use_sculpt_pbvh) {
    DRW_shgroup_call_sculpt(pd->facing_grp[is_xray], ob, false, false);
  }
  else {
    struct GPUBatch *geom = DRW_cache_object_surface_get(ob);
    if (geom) {
      DRW_shgroup_call(pd->facing_grp[is_xray], geom, ob);
    }
  }
}

void OVERLAY_facing_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->facing_ps[NOT_IN_FRONT]);
}

void OVERLAY_facing_infront_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->facing_ps[IN_FRONT]);
}
