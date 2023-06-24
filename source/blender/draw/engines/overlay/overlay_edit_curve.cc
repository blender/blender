/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "DNA_curve_types.h"

#include "overlay_private.hh"

void OVERLAY_edit_curve_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;
  DRWShadingGroup *grp;
  GPUShader *sh;
  DRWState state;

  pd->edit_curve.show_handles = v3d->overlay.handle_display != CURVE_HANDLE_NONE;
  pd->edit_curve.handle_display = v3d->overlay.handle_display;
  pd->shdata.edit_curve_normal_length = v3d->overlay.normals_length;

  /* Run Twice for in-front passes. */
  for (int i = 0; i < 2; i++) {
    state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH;
    state |= ((i == 0) ? DRW_STATE_DEPTH_LESS_EQUAL : DRW_STATE_DEPTH_ALWAYS);
    DRW_PASS_CREATE(psl->edit_curve_wire_ps[i], state | pd->clipping_state);

    sh = OVERLAY_shader_edit_curve_wire();
    pd->edit_curve_normal_grp[i] = grp = DRW_shgroup_create(sh, psl->edit_curve_wire_ps[i]);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_float_copy(grp, "normalSize", pd->shdata.edit_curve_normal_length);

    pd->edit_curve_wire_grp[i] = grp = DRW_shgroup_create(sh, psl->edit_curve_wire_ps[i]);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_float_copy(grp, "normalSize", 0.0f);
  }
  {
    state = DRW_STATE_WRITE_COLOR;
    DRW_PASS_CREATE(psl->edit_curve_handle_ps, state | pd->clipping_state);

    sh = OVERLAY_shader_edit_curve_handle();
    pd->edit_curve_handle_grp = grp = DRW_shgroup_create(sh, psl->edit_curve_handle_ps);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_bool_copy(grp, "showCurveHandles", pd->edit_curve.show_handles);
    DRW_shgroup_uniform_int_copy(grp, "curveHandleDisplay", pd->edit_curve.handle_display);
    DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ALPHA);

    sh = OVERLAY_shader_edit_curve_point();
    pd->edit_curve_points_grp = grp = DRW_shgroup_create(sh, psl->edit_curve_handle_ps);
    DRW_shgroup_uniform_bool_copy(grp, "showCurveHandles", pd->edit_curve.show_handles);
    DRW_shgroup_uniform_int_copy(grp, "curveHandleDisplay", pd->edit_curve.handle_display);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  }
}

void OVERLAY_edit_curve_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  bool draw_normals = (pd->overlay.edit_flag & V3D_OVERLAY_EDIT_CU_NORMALS) != 0;
  bool do_xray = (ob->dtx & OB_DRAW_IN_FRONT) != 0;

  Curve *cu = static_cast<Curve *>(ob->data);
  GPUBatch *geom;

  geom = DRW_cache_curve_edge_wire_get(ob);
  if (geom) {
    DRW_shgroup_call_no_cull(pd->edit_curve_wire_grp[do_xray], geom, ob);
  }

  if ((cu->flag & CU_3D) && draw_normals) {
    geom = DRW_cache_curve_edge_normal_get(ob);
    DRW_shgroup_call_instances(pd->edit_curve_normal_grp[do_xray], ob, geom, 3);
  }

  geom = DRW_cache_curve_edge_overlay_get(ob);
  if (geom) {
    DRW_shgroup_call_no_cull(pd->edit_curve_handle_grp, geom, ob);
  }

  geom = DRW_cache_curve_vert_overlay_get(ob);
  if (geom) {
    DRW_shgroup_call_no_cull(pd->edit_curve_points_grp, geom, ob);
  }
}

void OVERLAY_edit_surf_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  GPUBatch *geom;

  geom = DRW_cache_curve_edge_overlay_get(ob);
  if (geom) {
    DRW_shgroup_call_no_cull(pd->edit_curve_handle_grp, geom, ob);
  }

  geom = DRW_cache_curve_vert_overlay_get(ob);
  if (geom) {
    DRW_shgroup_call_no_cull(pd->edit_curve_points_grp, geom, ob);
  }
}

void OVERLAY_edit_curve_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_FramebufferList *fbl = vedata->fbl;

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_default_fb);
  }

  DRW_draw_pass(psl->edit_curve_wire_ps[0]);
  DRW_draw_pass(psl->edit_curve_wire_ps[1]);

  DRW_draw_pass(psl->edit_curve_handle_ps);
}
