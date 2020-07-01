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

#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_subdiv_ccg.h"

void OVERLAY_sculpt_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DRWShadingGroup *grp;

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND_MUL;
  DRW_PASS_CREATE(psl->sculpt_mask_ps, state | pd->clipping_state);

  GPUShader *sh = OVERLAY_shader_sculpt_mask();
  pd->sculpt_mask_grp = grp = DRW_shgroup_create(sh, psl->sculpt_mask_ps);
  DRW_shgroup_uniform_float_copy(grp, "maskOpacity", pd->overlay.sculpt_mode_mask_opacity);
  DRW_shgroup_uniform_float_copy(
      grp, "faceSetsOpacity", pd->overlay.sculpt_mode_face_sets_opacity);
}

void OVERLAY_sculpt_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  PBVH *pbvh = ob->sculpt->pbvh;

  const bool use_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->v3d);

  if (use_pbvh || !ob->sculpt->deform_modifiers_active || ob->sculpt->shapekey_active) {
    if (!use_pbvh || pbvh_has_mask(pbvh) || pbvh_has_face_sets(pbvh)) {
      DRW_shgroup_call_sculpt(pd->sculpt_mask_grp, ob, false, true);
    }
  }
}

void OVERLAY_sculpt_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(pd->painting.in_front ? dfbl->in_front_fb : dfbl->default_fb);
  }

  DRW_draw_pass(psl->sculpt_mask_ps);
}
