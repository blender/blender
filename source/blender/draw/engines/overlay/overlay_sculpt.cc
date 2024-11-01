/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.hh"

#include "draw_cache_impl.hh"
#include "overlay_private.hh"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "DEG_depsgraph_query.hh"

#include "bmesh.hh"

void OVERLAY_sculpt_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DRWShadingGroup *grp;

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_MUL;
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
  const SculptSession &ss = *ob->sculpt;
  blender::bke::pbvh::Tree *pbvh = blender::bke::object::pbvh_get(*ob);

  const bool use_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->rv3d);

  if (!pbvh) {
    /* It is possible to have SculptSession without pbvh::Tree. This happens, for example, when
     * toggling object mode to sculpt then to edit mode. */
    return;
  }

  /* Using the original object/geometry is necessary because we skip depsgraph updates in sculpt
   * mode to improve performance. This means the evaluated mesh doesn't have the latest face set,
   * visibility, and mask data. */
  Object *object_orig = reinterpret_cast<Object *>(DEG_get_original_id(&ob->id));
  if (!object_orig) {
    BLI_assert_unreachable();
    return;
  }

  switch (pbvh->type()) {
    case blender::bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(object_orig->data);
      if (!mesh.attributes().contains(".sculpt_face_set") &&
          !mesh.attributes().contains(".sculpt_mask"))
      {
        return;
      }
      break;
    }
    case blender::bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const Mesh &base_mesh = *static_cast<const Mesh *>(object_orig->data);
      if (subdiv_ccg.masks.is_empty() && !base_mesh.attributes().contains(".sculpt_face_set")) {
        return;
      }
      break;
    }
    case blender::bke::pbvh::Type::BMesh: {
      const BMesh &bm = *ss.bm;
      if (!CustomData_has_layer_named(&bm.pdata, CD_PROP_FLOAT, ".sculpt_face_set") &&
          !CustomData_has_layer_named(&bm.vdata, CD_PROP_FLOAT, ".sculpt_mask"))
      {
        return;
      }
      break;
    }
  }

  if (use_pbvh) {
    DRW_shgroup_call_sculpt(pd->sculpt_mask_grp, ob, false, true, true, false, false);
  }
  else {
    sculpt_overlays = DRW_mesh_batch_cache_get_sculpt_overlays(*static_cast<Mesh *>(ob->data));
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
