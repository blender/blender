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
 */

/** \file
 * \ingroup draw_engine
 */

#include "DNA_volume_types.h"

#include "DRW_render.h"
#include "GPU_shader.h"

#include "overlay_private.h"

void OVERLAY_volume_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const bool is_select = DRW_state_is_select();

  if (is_select) {
    DRWState state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    DRW_PASS_CREATE(psl->volume_ps, state | pd->clipping_state);
    GPUShader *sh = OVERLAY_shader_depth_only();
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->volume_ps);
    pd->volume_selection_surface_grp = grp;
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  }
  else {
    psl->volume_ps = NULL;
    pd->volume_selection_surface_grp = NULL;
  }
}

void OVERLAY_volume_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const bool is_select = DRW_state_is_select();

  if (is_select) {
    struct GPUBatch *geom = DRW_cache_volume_selection_surface_get(ob);
    if (geom != NULL) {
      DRW_shgroup_call(pd->volume_selection_surface_grp, geom, ob);
    }
  }
}

void OVERLAY_volume_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  if (psl->volume_ps) {
    DRW_draw_pass(psl->volume_ps);
  }
}
