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

#include "overlay_private.h"

void OVERLAY_facing_init(OVERLAY_Data *UNUSED(vedata))
{
}

void OVERLAY_facing_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND_ALPHA;
  DRW_PASS_CREATE(psl->facing_ps, state | pd->clipping_state);

  GPUShader *sh = OVERLAY_shader_facing();
  pd->facing_grp = DRW_shgroup_create(sh, psl->facing_ps);
}

void OVERLAY_facing_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  struct GPUBatch *geom = DRW_cache_object_surface_get(ob);
  if (geom) {
    DRW_shgroup_call(pd->facing_grp, geom, ob);
  }
}

void OVERLAY_facing_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  /* We need to match underlying geometry pass, at the cost of bypassing TAA. */
  DRW_view_set_active(NULL);

  DRW_draw_pass(psl->facing_ps);

  DRW_view_set_active(pd->view_default);
}
