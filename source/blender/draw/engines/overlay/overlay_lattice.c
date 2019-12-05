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

void OVERLAY_edit_lattice_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  struct GPUShader *sh;
  DRWShadingGroup *grp;

  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    DRW_PASS_CREATE(psl->edit_lattice_ps, state | pd->clipping_state);

    sh = OVERLAY_shader_edit_lattice_wire();
    pd->edit_lattice_wires_grp = grp = DRW_shgroup_create(sh, psl->edit_lattice_ps);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_texture(grp, "weightTex", G_draw.weight_ramp);

    sh = OVERLAY_shader_edit_lattice_point();
    pd->edit_lattice_points_grp = grp = DRW_shgroup_create(sh, psl->edit_lattice_ps);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  }
}

void OVERLAY_edit_lattice_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  struct GPUBatch *geom;

  geom = DRW_cache_lattice_wire_get(ob, true);
  DRW_shgroup_call(pd->edit_lattice_wires_grp, geom, ob);

  geom = DRW_cache_lattice_vert_overlay_get(ob);
  DRW_shgroup_call(pd->edit_lattice_points_grp, geom, ob);
}

void OVERLAY_lattice_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_ExtraCallBuffers *cb = OVERLAY_extra_call_buffer_get(vedata, ob);
  const DRWContextState *draw_ctx = DRW_context_state_get();

  float *color;
  DRW_object_wire_theme_get(ob, draw_ctx->view_layer, &color);

  struct GPUBatch *geom = DRW_cache_lattice_wire_get(ob, false);
  OVERLAY_extra_wire(cb, geom, ob->obmat, color);
}

void OVERLAY_edit_lattice_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_FramebufferList *fbl = vedata->fbl;

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_default_fb);
  }

  DRW_draw_pass(psl->edit_lattice_ps);
}
