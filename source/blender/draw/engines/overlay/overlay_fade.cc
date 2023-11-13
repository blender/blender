/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "BKE_paint.hh"
#include "BLI_math_color.h"
#include "DRW_render.h"

#include "ED_view3d.hh"

#include "overlay_private.hh"

void OVERLAY_fade_init(OVERLAY_Data * /*vedata*/) {}

void OVERLAY_fade_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  for (int i = 0; i < 2; i++) {
    /* Non Meshes Pass (Camera, empties, lights ...) */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND_ALPHA;
    DRW_PASS_CREATE(psl->fade_ps[i], state | pd->clipping_state);

    GPUShader *sh = OVERLAY_shader_uniform_color();
    pd->fade_grp[i] = DRW_shgroup_create(sh, psl->fade_ps[i]);

    const DRWContextState *draw_ctx = DRW_context_state_get();
    float color[4];
    ED_view3d_background_color_get(draw_ctx->scene, draw_ctx->v3d, color);
    color[3] = pd->overlay.fade_alpha;
    if (draw_ctx->v3d->shading.background_type == V3D_SHADING_BACKGROUND_THEME) {
      srgb_to_linearrgb_v4(color, color);
    }
    DRW_shgroup_uniform_vec4_copy(pd->fade_grp[i], "ucolor", color);
  }

  if (!pd->use_in_front) {
    pd->fade_grp[IN_FRONT] = pd->fade_grp[NOT_IN_FRONT];
  }
}

void OVERLAY_fade_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  if (pd->xray_enabled) {
    return;
  }

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->rv3d) &&
                               !DRW_state_is_image_render();
  const bool is_xray = (ob->dtx & OB_DRAW_IN_FRONT) != 0;

  if (use_sculpt_pbvh) {
    DRW_shgroup_call_sculpt(pd->fade_grp[is_xray], ob, false, false, false, false, false);
  }
  else {
    GPUBatch *geom = DRW_cache_object_surface_get(ob);
    if (geom) {
      DRW_shgroup_call(pd->fade_grp[is_xray], geom, ob);
    }
  }
}

void OVERLAY_fade_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->fade_ps[NOT_IN_FRONT]);
}

void OVERLAY_fade_infront_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->fade_ps[IN_FRONT]);
}
