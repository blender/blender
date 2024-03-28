/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DNA_collection_types.h"
#include "DNA_mesh_types.h"
#include "DNA_particle_types.h"
#include "DNA_view3d_types.h"
#include "DNA_volume_types.h"

#include "BKE_curve.hh"
#include "BKE_displist.h"
#include "BKE_duplilist.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.hh"
#include "BKE_mesh_types.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_particle.h"

#include "BLI_hash.h"
#include "BLI_math_base.hh"

#include "DRW_render.hh"
#include "GPU_shader.hh"

#include "ED_view3d.hh"

#include "overlay_private.hh"

using namespace blender::math;

void OVERLAY_wireframe_init(OVERLAY_Data *vedata)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  DRWView *default_view = (DRWView *)DRW_view_default_get();
  pd->view_wires = DRW_view_create_with_zoffset(default_view, draw_ctx->rv3d, 0.5f);
}

void OVERLAY_wireframe_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_TextureList *txl = vedata->txl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  DRWShadingGroup *grp = nullptr;

  View3DShading *shading = &draw_ctx->v3d->shading;

  /* Use `sqrt` since the value stored in the edge is a variation of the cosine, so its square
   * becomes more proportional with a variation of angle. */
  pd->shdata.wire_step_param = sqrt(abs(pd->overlay.wireframe_threshold));

  /* The maximum value (255 in the VBO) is used to force hide the edge. */
  pd->shdata.wire_step_param = interpolate(0.0f, 1.0f - (1.0f / 255), pd->shdata.wire_step_param);

  pd->shdata.wire_opacity = pd->overlay.wireframe_opacity;

  bool is_material_shmode = (shading->type > OB_SOLID);

  int color_type = shading->wire_color_type;

  const bool use_select = (DRW_state_is_select() || DRW_state_is_depth());
  GPUShader *wires_sh = use_select ? OVERLAY_shader_wireframe_select() :
                                     OVERLAY_shader_wireframe(pd->antialiasing.enabled &&
                                                              !pd->xray_enabled);

  for (int xray = 0; xray < (is_material_shmode ? 1 : 2); xray++) {
    DRWState state = DRW_STATE_FIRST_VERTEX_CONVENTION | DRW_STATE_WRITE_COLOR |
                     DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    DRWPass *pass;
    GPUTexture **depth_tx = ((!pd->xray_enabled || pd->xray_opacity > 0.0f) &&
                             DRW_state_is_fbo()) ?
                                &txl->temp_depth_tx :
                                &txl->dummy_depth_tx;

    if (xray == 0) {
      DRW_PASS_CREATE(psl->wireframe_ps, state | pd->clipping_state);
      pass = psl->wireframe_ps;
    }
    else {
      DRW_PASS_CREATE(psl->wireframe_xray_ps, state | pd->clipping_state);
      pass = psl->wireframe_xray_ps;
    }

    for (int use_coloring = 0; use_coloring < 2; use_coloring++) {
      pd->wires_grp[xray][use_coloring] = grp = DRW_shgroup_create(wires_sh, pass);
      DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_texture_ref(grp, "depthTex", depth_tx);
      DRW_shgroup_uniform_float_copy(grp, "wireStepParam", pd->shdata.wire_step_param);
      DRW_shgroup_uniform_float_copy(grp, "wireOpacity", pd->shdata.wire_opacity);
      DRW_shgroup_uniform_bool_copy(grp, "useColoring", use_coloring);
      DRW_shgroup_uniform_bool_copy(grp, "isTransform", (G.moving & G_TRANSFORM_OBJ) != 0);
      DRW_shgroup_uniform_int_copy(grp, "colorType", color_type);
      DRW_shgroup_uniform_bool_copy(grp, "isHair", false);

      pd->wires_all_grp[xray][use_coloring] = grp = DRW_shgroup_create(wires_sh, pass);
      DRW_shgroup_uniform_float_copy(grp, "wireStepParam", 1.0f);

      pd->wires_hair_grp[xray][use_coloring] = grp = DRW_shgroup_create(wires_sh, pass);
      DRW_shgroup_uniform_bool_copy(grp, "isHair", true);
      DRW_shgroup_uniform_float_copy(grp, "wireStepParam", 10.0f);
    }

    pd->wires_sculpt_grp[xray] = grp = DRW_shgroup_create(wires_sh, pass);
    DRW_shgroup_uniform_texture_ref(grp, "depthTex", depth_tx);
    DRW_shgroup_uniform_float_copy(grp, "wireStepParam", 10.0f);
    DRW_shgroup_uniform_bool_copy(grp, "useColoring", false);
    DRW_shgroup_uniform_bool_copy(grp, "isHair", false);
  }

  if (is_material_shmode) {
    /* Make all drawcalls go into the non-xray shading groups. */
    for (int use_coloring = 0; use_coloring < 2; use_coloring++) {
      pd->wires_grp[1][use_coloring] = pd->wires_grp[0][use_coloring];
      pd->wires_all_grp[1][use_coloring] = pd->wires_all_grp[0][use_coloring];
      pd->wires_hair_grp[1][use_coloring] = pd->wires_hair_grp[0][use_coloring];
    }
    pd->wires_sculpt_grp[1] = pd->wires_sculpt_grp[0];
    psl->wireframe_xray_ps = nullptr;
  }
}

static void wireframe_hair_cache_populate(OVERLAY_Data *vedata, Object *ob, ParticleSystem *psys)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const bool is_xray = (ob->dtx & OB_DRAW_IN_FRONT) != 0;

  Object *dupli_parent = DRW_object_get_dupli_parent(ob);
  DupliObject *dupli_object = DRW_object_get_dupli(ob);

  float dupli_mat[4][4];
  if ((dupli_parent != nullptr) && (dupli_object != nullptr)) {
    if (dupli_object->type & OB_DUPLICOLLECTION) {
      unit_m4(dupli_mat);
      Collection *collection = dupli_parent->instance_collection;
      if (collection != nullptr) {
        sub_v3_v3(dupli_mat[3], collection->instance_offset);
      }
      mul_m4_m4m4(dupli_mat, dupli_parent->object_to_world().ptr(), dupli_mat);
    }
    else {
      copy_m4_m4(dupli_mat, dupli_object->ob->object_to_world().ptr());
      invert_m4(dupli_mat);
      mul_m4_m4m4(dupli_mat, ob->object_to_world().ptr(), dupli_mat);
    }
  }
  else {
    unit_m4(dupli_mat);
  }

  blender::gpu::Batch *hairs = DRW_cache_particles_get_hair(ob, psys, nullptr);

  const bool use_coloring = true;
  DRWShadingGroup *shgrp = DRW_shgroup_create_sub(pd->wires_hair_grp[is_xray][use_coloring]);
  DRW_shgroup_uniform_mat4_copy(shgrp, "hairDupliMatrix", dupli_mat);
  DRW_shgroup_call_no_cull(shgrp, hairs, ob);
}

void OVERLAY_wireframe_cache_populate(OVERLAY_Data *vedata,
                                      Object *ob,
                                      OVERLAY_DupliData *dupli,
                                      bool init_dupli)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const bool all_wires = (ob->dtx & OB_DRAW_ALL_EDGES) != 0;
  const bool is_xray = (ob->dtx & OB_DRAW_IN_FRONT) != 0;
  const bool is_mesh = ob->type == OB_MESH;
  const bool is_edit_mode = DRW_object_is_in_edit_mode(ob);
  bool has_edit_mesh_cage = false;
  bool is_mesh_verts_only = false;
  if (is_mesh) {
    /* TODO: Should be its own function. */
    const Mesh *mesh = static_cast<const Mesh *>(ob->data);
    if (is_edit_mode) {
      BLI_assert(mesh->runtime->edit_mesh);
      const Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(ob);
      const Mesh *editmesh_eval_cage = BKE_object_get_editmesh_eval_cage(ob);
      has_edit_mesh_cage = editmesh_eval_cage && (editmesh_eval_cage != editmesh_eval_final);
      if (editmesh_eval_final) {
        mesh = editmesh_eval_final;
      }
    }
    is_mesh_verts_only = mesh->edges_num == 0 && mesh->verts_num > 0;
  }

  const bool use_wire = !is_mesh_verts_only && ((pd->overlay.flag & V3D_OVERLAY_WIREFRAMES) ||
                                                (ob->dtx & OB_DRAWWIRE) || (ob->dt == OB_WIRE));

  if (use_wire && pd->wireframe_mode && ob->particlesystem.first) {
    for (ParticleSystem *psys = static_cast<ParticleSystem *>(ob->particlesystem.first);
         psys != nullptr;
         psys = psys->next)
    {
      if (!DRW_object_is_visible_psys_in_active_context(ob, psys)) {
        continue;
      }
      ParticleSettings *part = psys->part;
      const int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;
      if (draw_as == PART_DRAW_PATH) {
        wireframe_hair_cache_populate(vedata, ob, psys);
      }
    }
  }

  if (ELEM(ob->type, OB_CURVES_LEGACY, OB_FONT, OB_SURF)) {
    OVERLAY_ExtraCallBuffers *cb = OVERLAY_extra_call_buffer_get(vedata, ob);
    float *color;
    DRW_object_wire_theme_get(ob, draw_ctx->view_layer, &color);

    blender::gpu::Batch *geom = nullptr;
    switch (ob->type) {
      case OB_CURVES_LEGACY:
        geom = DRW_cache_curve_edge_wire_get(ob);
        break;
      case OB_FONT:
        geom = DRW_cache_text_edge_wire_get(ob);
        break;
      case OB_SURF:
        geom = DRW_cache_surf_edge_wire_get(ob);
        break;
    }

    if (geom) {
      OVERLAY_extra_wire(cb, geom, ob->object_to_world().ptr(), color);
    }
  }

  /* Fast path for duplis. */
  if (dupli && !init_dupli) {
    if (dupli->wire_shgrp && dupli->wire_geom) {
      if (dupli->base_flag == ob->base_flag) {
        /* Check for the special cases used below, assign specific theme colors to the shaders. */
        OVERLAY_ExtraCallBuffers *cb = OVERLAY_extra_call_buffer_get(vedata, ob);
        if (dupli->wire_shgrp == cb->extra_loose_points) {
          float *color;
          DRW_object_wire_theme_get(ob, draw_ctx->view_layer, &color);
          OVERLAY_extra_loose_points(cb, dupli->wire_geom, ob->object_to_world().ptr(), color);
        }
        else if (dupli->wire_shgrp == cb->extra_wire) {
          float *color;
          DRW_object_wire_theme_get(ob, draw_ctx->view_layer, &color);
          OVERLAY_extra_wire(cb, dupli->wire_geom, ob->object_to_world().ptr(), color);
        }
        else {
          DRW_shgroup_call(dupli->wire_shgrp, dupli->wire_geom, ob);
        }
        return;
      }
    }
    else {
      /* Nothing to draw for this dupli. */
      return;
    }
  }

  if (use_wire && ELEM(ob->type, OB_VOLUME, OB_POINTCLOUD)) {
    bool draw_as_points = true;
    if (ob->type == OB_VOLUME) {
      /* Volume object as points exception. */
      Volume *volume = static_cast<Volume *>(ob->data);
      draw_as_points = volume->display.wireframe_type == VOLUME_WIREFRAME_POINTS;
    }

    if (draw_as_points) {
      float *color;
      OVERLAY_ExtraCallBuffers *cb = OVERLAY_extra_call_buffer_get(vedata, ob);
      DRW_object_wire_theme_get(ob, draw_ctx->view_layer, &color);

      blender::gpu::Batch *geom = DRW_cache_object_face_wireframe_get(ob);
      if (geom) {
        OVERLAY_extra_loose_points(cb, geom, ob->object_to_world().ptr(), color);
      }
      return;
    }
  }

  DRWShadingGroup *shgrp = nullptr;
  blender::gpu::Batch *geom = nullptr;

  /* Don't do that in edit Mesh mode, unless there is a modifier preview. */
  if (use_wire && (!is_mesh || (!is_edit_mode || has_edit_mesh_cage))) {
    const bool is_sculpt_mode = ((ob->mode & OB_MODE_SCULPT) != 0) && (ob->sculpt != nullptr);
    const bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->rv3d) &&
                                 !DRW_state_is_image_render();
    const bool is_instance = (ob->base_flag & BASE_FROM_DUPLI);
    const bool instance_parent_in_edit_mode = is_instance ? DRW_object_is_in_edit_mode(
                                                                DRW_object_get_dupli_parent(ob)) :
                                                            false;
    const bool use_coloring = (use_wire && !is_edit_mode && !is_sculpt_mode &&
                               !has_edit_mesh_cage && !instance_parent_in_edit_mode);
    geom = DRW_cache_object_face_wireframe_get(ob);

    if (geom || use_sculpt_pbvh) {
      if (use_sculpt_pbvh) {
        shgrp = pd->wires_sculpt_grp[is_xray];
      }
      else if (all_wires) {
        shgrp = pd->wires_all_grp[is_xray][use_coloring];
      }
      else {
        shgrp = pd->wires_grp[is_xray][use_coloring];
      }

      if (ob->type == OB_GPENCIL_LEGACY) {
        /* TODO(fclem): Make GPencil objects have correct bound-box. */
        DRW_shgroup_call_no_cull(shgrp, geom, ob);
      }
      else if (use_sculpt_pbvh) {
        DRW_shgroup_call_sculpt(shgrp, ob, true, false, false, false, false);
      }
      else {
        DRW_shgroup_call(shgrp, geom, ob);
      }
    }
  }
  else if (is_mesh && (!is_edit_mode || has_edit_mesh_cage)) {
    OVERLAY_ExtraCallBuffers *cb = OVERLAY_extra_call_buffer_get(vedata, ob);
    float *color;
    DRW_object_wire_theme_get(ob, draw_ctx->view_layer, &color);

    /* Draw loose geometry. */
    if (is_mesh_verts_only) {
      geom = DRW_cache_mesh_all_verts_get(ob);
      if (geom) {
        OVERLAY_extra_loose_points(cb, geom, ob->object_to_world().ptr(), color);
        shgrp = cb->extra_loose_points;
      }
    }
    else {
      geom = DRW_cache_mesh_loose_edges_get(ob);
      if (geom) {
        OVERLAY_extra_wire(cb, geom, ob->object_to_world().ptr(), color);
        shgrp = cb->extra_wire;
      }
    }
  }

  if (dupli) {
    dupli->wire_shgrp = shgrp;
    dupli->wire_geom = geom;
  }
}

void OVERLAY_wireframe_draw(OVERLAY_Data *data)
{
  OVERLAY_PassList *psl = data->psl;
  OVERLAY_PrivateData *pd = data->stl->pd;

  DRW_view_set_active(pd->view_wires);
  DRW_draw_pass(psl->wireframe_ps);

  DRW_view_set_active(nullptr);
}

void OVERLAY_wireframe_in_front_draw(OVERLAY_Data *data)
{
  OVERLAY_PassList *psl = data->psl;
  OVERLAY_PrivateData *pd = data->stl->pd;

  if (psl->wireframe_xray_ps) {
    DRW_view_set_active(pd->view_wires);
    DRW_draw_pass(psl->wireframe_xray_ps);

    DRW_view_set_active(nullptr);
  }
}
