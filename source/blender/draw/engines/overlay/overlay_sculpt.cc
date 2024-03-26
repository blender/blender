/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.hh"

#include "draw_cache_impl.hh"
#include "overlay_private.hh"

#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_subdiv_ccg.hh"

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
  using namespace blender::draw;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  blender::gpu::Batch *sculpt_overlays;
  PBVH *pbvh = ob->sculpt->pbvh;

  const bool use_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->rv3d);

  if (!pbvh) {
    /* It is possible to have SculptSession without PBVH. This happens, for example, when toggling
     * object mode to sculpt then to edit mode. */
    return;
  }

  if (!pbvh_has_mask(pbvh) && !pbvh_has_face_sets(pbvh)) {
    /* The SculptSession and the PBVH can be created without a Mask data-layer or Face Set
     * data-layer. (masks data-layers are created after using a mask tool), so in these cases there
     * is nothing to draw. */
    return;
  }

  if (use_pbvh) {
    DRW_shgroup_call_sculpt(pd->sculpt_mask_grp, ob, false, true, true, false, false);
  }
  else {
    sculpt_overlays = DRW_mesh_batch_cache_get_sculpt_overlays(static_cast<Mesh *>(ob->data));
    if (sculpt_overlays) {
      DRW_shgroup_call(pd->sculpt_mask_grp, sculpt_overlays, ob);
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
