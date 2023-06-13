/* SPDX-FileCopyrightText: 2022 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "BKE_curves.h"

#include "DRW_render.h"

#include "ED_view3d.h"

#include "DEG_depsgraph_query.h"

#include "draw_cache_impl.h"

#include "overlay_private.hh"

void OVERLAY_edit_curves_init(OVERLAY_Data *vedata)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Object *obact_orig = DEG_get_original_object(draw_ctx->obact);

  const Curves &curves_id = *static_cast<const Curves *>(obact_orig->data);
  pd->edit_curves.do_points = curves_id.selection_domain == ATTR_DOMAIN_POINT;
  pd->edit_curves.do_zbufclip = XRAY_FLAG_ENABLED(draw_ctx->v3d);

  /* Create view with depth offset. */
  DRWView *default_view = (DRWView *)DRW_view_default_get();
  pd->view_edit_curves = DRW_view_create_with_zoffset(default_view, draw_ctx->rv3d, 1.0f);
}

void OVERLAY_edit_curves_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  /* Desired masks (write to color and depth) and blend mode for rendering. */
  DRWState state = (DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA |
                    DRW_STATE_WRITE_DEPTH);

  GPUShader *sh;
  DRWShadingGroup *grp;

  /* Run Twice for in-front passes. */
  for (int i = 0; i < 2; i++) {
    if (pd->edit_curves.do_points) {
      DRW_PASS_CREATE(psl->edit_curves_points_ps[i], (state | pd->clipping_state));
      sh = OVERLAY_shader_edit_particle_point();
      grp = pd->edit_curves_points_grp[i] = DRW_shgroup_create(sh, psl->edit_curves_points_ps[i]);
      DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    }

    DRW_PASS_CREATE(psl->edit_curves_lines_ps[i], (state | pd->clipping_state));
    sh = OVERLAY_shader_edit_particle_strand();
    grp = pd->edit_curves_lines_grp[i] = DRW_shgroup_create(sh, psl->edit_curves_lines_ps[i]);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_bool_copy(grp, "useWeight", false);
  }
}

static void overlay_edit_curves_add_ob_to_pass(OVERLAY_PrivateData *pd, Object *ob, bool in_front)
{
  Curves *curves = static_cast<Curves *>(ob->data);

  if (pd->edit_curves.do_points) {
    DRWShadingGroup *point_shgrp = pd->edit_curves_points_grp[in_front];
    GPUBatch *geom_points = DRW_curves_batch_cache_get_edit_points(curves);
    DRW_shgroup_call_no_cull(point_shgrp, geom_points, ob);
  }

  DRWShadingGroup *lines_shgrp = pd->edit_curves_lines_grp[in_front];
  GPUBatch *geom_lines = DRW_curves_batch_cache_get_edit_lines(curves);
  DRW_shgroup_call_no_cull(lines_shgrp, geom_lines, ob);
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
  OVERLAY_FramebufferList *fbl = vedata->fbl;

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_default_fb);
  }

  if (pd->edit_curves.do_zbufclip) {
    DRW_view_set_active(pd->view_edit_curves);
    if (pd->edit_curves.do_points) {
      DRW_draw_pass(psl->edit_curves_points_ps[NOT_IN_FRONT]);
    }
    DRW_draw_pass(psl->edit_curves_lines_ps[NOT_IN_FRONT]);
  }
  else {
    DRW_view_set_active(pd->view_edit_curves);
    if (pd->edit_curves.do_points) {
      DRW_draw_pass(psl->edit_curves_points_ps[IN_FRONT]);
    }
    DRW_draw_pass(psl->edit_curves_lines_ps[IN_FRONT]);
  }
}
