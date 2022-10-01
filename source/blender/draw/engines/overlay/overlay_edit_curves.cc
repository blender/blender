/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "ED_view3d.h"

#include "draw_cache_impl.h"

#include "overlay_private.hh"

void OVERLAY_edit_curves_init(OVERLAY_Data *vedata)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  pd->edit_curves.do_zbufclip = XRAY_FLAG_ENABLED(draw_ctx->v3d);

  /* Create view with depth offset. */
  DRWView *default_view = (DRWView *)DRW_view_default_get();
  pd->view_edit_curves_points = default_view;
}

void OVERLAY_edit_curves_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_TextureList *txl = vedata->txl;
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  /* Desired masks (write to color and depth) and blend mode for rendering. */
  DRWState state = (DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                    DRW_STATE_WRITE_DEPTH);

  /* Common boilerplate for shading groups. */
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const View3D *v3d = draw_ctx->v3d;
  GPUTexture **depth_tex = (pd->edit_curves.do_zbufclip) ? &dtxl->depth : &txl->dummy_depth_tx;
  const float backwire_opacity = (pd->edit_curves.do_zbufclip) ? v3d->overlay.backwire_opacity :
                                                                 1.0f;

  /* Run Twice for in-front passes. */
  for (int i = 0; i < 2; i++) {
    DRW_PASS_CREATE(psl->edit_curves_points_ps[i], (state | pd->clipping_state));

    GPUShader *sh = OVERLAY_shader_edit_curve_point();
    DRWShadingGroup *grp = pd->edit_curves_points_grp[i] = DRW_shgroup_create(
        sh, psl->edit_curves_points_ps[i]);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_float_copy(grp, "alpha", backwire_opacity);
    DRW_shgroup_uniform_texture_ref(grp, "depthTex", depth_tex);
  }
}

static void overlay_edit_curves_add_ob_to_pass(OVERLAY_PrivateData *pd, Object *ob, bool in_front)
{
  Curves *curves = static_cast<Curves *>(ob->data);
  DRWShadingGroup *point_shgrp = pd->edit_curves_points_grp[in_front];
  struct GPUBatch *geom_points = DRW_curves_batch_cache_get_edit_points(curves);
  DRW_shgroup_call_no_cull(point_shgrp, geom_points, ob);
}

void OVERLAY_edit_curves_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  if (pd->edit_curves.do_zbufclip) {
    overlay_edit_curves_add_ob_to_pass(pd, ob, false);
  }
  else {
    overlay_edit_curves_add_ob_to_pass(pd, ob, true);
  }
}

void OVERLAY_edit_curves_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  if (pd->edit_curves.do_zbufclip) {
    DRW_view_set_active(pd->view_edit_curves_points);
    DRW_draw_pass(psl->edit_curves_points_ps[NOT_IN_FRONT]);
  }
  else {
    DRW_view_set_active(pd->view_edit_curves_points);
    DRW_draw_pass(psl->edit_curves_points_ps[IN_FRONT]);
  }
}
