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

#include "DNA_mesh_types.h"
#include "DNA_particle_types.h"
#include "DNA_view3d_types.h"
#include "DNA_volume_types.h"

#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_duplilist.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"

#include "BLI_hash.h"

#include "DRW_render.h"
#include "GPU_shader.h"

#include "ED_view3d.h"

#include "overlay_private.h"

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
  DRWShadingGroup *grp = NULL;

  View3DShading *shading = &draw_ctx->v3d->shading;

  pd->shdata.wire_step_param = pd->overlay.wireframe_threshold - 254.0f / 255.0f;

  bool is_wire_shmode = (shading->type == OB_WIRE);
  bool is_material_shmode = (shading->type > OB_SOLID);
  bool is_object_color = is_wire_shmode && (shading->wire_color_type == V3D_SHADING_OBJECT_COLOR);
  bool is_random_color = is_wire_shmode && (shading->wire_color_type == V3D_SHADING_RANDOM_COLOR);

  const bool use_select = (DRW_state_is_select() || DRW_state_is_depth());
  GPUShader *wires_sh = use_select ? OVERLAY_shader_wireframe_select() :
                                     OVERLAY_shader_wireframe(pd->antialiasing.enabled);

  for (int xray = 0; xray < (is_material_shmode ? 1 : 2); xray++) {
    DRWState state = DRW_STATE_FIRST_VERTEX_CONVENTION | DRW_STATE_WRITE_COLOR |
                     DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    DRWPass *pass;
    GPUTexture **depth_tx = ((pd->xray_enabled || pd->xray_opacity > 0.0f) && DRW_state_is_fbo()) ?
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
      DRW_shgroup_uniform_block_persistent(grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_texture_ref(grp, "depthTex", depth_tx);
      DRW_shgroup_uniform_float_copy(grp, "wireStepParam", pd->shdata.wire_step_param);
      DRW_shgroup_uniform_bool_copy(grp, "useColoring", use_coloring);
      DRW_shgroup_uniform_bool_copy(grp, "isTransform", (G.moving & G_TRANSFORM_OBJ) != 0);
      DRW_shgroup_uniform_bool_copy(grp, "isObjectColor", is_object_color);
      DRW_shgroup_uniform_bool_copy(grp, "isRandomColor", is_random_color);
      DRW_shgroup_uniform_bool_copy(grp, "isHair", false);

      pd->wires_all_grp[xray][use_coloring] = grp = DRW_shgroup_create(wires_sh, pass);
      DRW_shgroup_uniform_texture_ref(grp, "depthTex", depth_tx);
      DRW_shgroup_uniform_float_copy(grp, "wireStepParam", 1.0f);

      pd->wires_hair_grp[xray][use_coloring] = grp = DRW_shgroup_create(wires_sh, pass);
      /* TODO(fclem) texture ref persist */
      DRW_shgroup_uniform_texture_ref(grp, "depthTex", depth_tx);
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
    psl->wireframe_xray_ps = NULL;
  }
}

static void wireframe_hair_cache_populate(OVERLAY_Data *vedata, Object *ob, ParticleSystem *psys)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const bool is_xray = (ob->dtx & OB_DRAWXRAY) != 0;

  Object *dupli_parent = DRW_object_get_dupli_parent(ob);
  DupliObject *dupli_object = DRW_object_get_dupli(ob);

  float dupli_mat[4][4];
  if ((dupli_parent != NULL) && (dupli_object != NULL)) {
    if (dupli_object->type & OB_DUPLICOLLECTION) {
      copy_m4_m4(dupli_mat, dupli_parent->obmat);
    }
    else {
      copy_m4_m4(dupli_mat, dupli_object->ob->obmat);
      invert_m4(dupli_mat);
      mul_m4_m4m4(dupli_mat, ob->obmat, dupli_mat);
    }
  }
  else {
    unit_m4(dupli_mat);
  }

  struct GPUBatch *hairs = DRW_cache_particles_get_hair(ob, psys, NULL);

  const bool use_coloring = true;
  DRWShadingGroup *shgrp = DRW_shgroup_create_sub(pd->wires_hair_grp[is_xray][use_coloring]);
  DRW_shgroup_uniform_vec4_array_copy(shgrp, "hairDupliMatrix", dupli_mat, 4);
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
  const bool is_xray = (ob->dtx & OB_DRAWXRAY) != 0;
  const bool is_mesh = ob->type == OB_MESH;
  const bool is_mesh_verts_only = is_mesh && (((Mesh *)ob->data)->totedge == 0 &&
                                              ((Mesh *)ob->data)->totvert > 0);
  const bool use_wire = !is_mesh_verts_only && ((pd->overlay.flag & V3D_OVERLAY_WIREFRAMES) ||
                                                (ob->dtx & OB_DRAWWIRE) || (ob->dt == OB_WIRE));

  if (use_wire && pd->wireframe_mode && ob->particlesystem.first) {
    for (ParticleSystem *psys = ob->particlesystem.first; psys != NULL; psys = psys->next) {
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

  if (ELEM(ob->type, OB_CURVE, OB_FONT, OB_SURF)) {
    OVERLAY_ExtraCallBuffers *cb = OVERLAY_extra_call_buffer_get(vedata, ob);
    float *color;
    DRW_object_wire_theme_get(ob, draw_ctx->view_layer, &color);

    struct GPUBatch *geom = NULL;
    switch (ob->type) {
      case OB_CURVE:
        if (!pd->wireframe_mode && !use_wire && ob->runtime.curve_cache &&
            BKE_displist_has_faces(&ob->runtime.curve_cache->disp)) {
          break;
        }
        geom = DRW_cache_curve_edge_wire_get(ob);
        break;
      case OB_FONT:
        if (!pd->wireframe_mode && !use_wire && ob->runtime.curve_cache &&
            BKE_displist_has_faces(&ob->runtime.curve_cache->disp)) {
          break;
        }
        geom = DRW_cache_text_loose_edges_get(ob);
        break;
      case OB_SURF:
        geom = DRW_cache_surf_edge_wire_get(ob);
        break;
    }

    if (geom) {
      OVERLAY_extra_wire(cb, geom, ob->obmat, color);
    }
  }

  /* Fast path for duplis. */
  if (dupli && !init_dupli) {
    if (dupli->wire_shgrp && dupli->wire_geom) {
      if (dupli->base_flag == ob->base_flag) {
        DRW_shgroup_call(dupli->wire_shgrp, dupli->wire_geom, ob);
        return;
      }
    }
    else {
      /* Nothing to draw for this dupli. */
      return;
    }
  }

  if (use_wire && ob->type == OB_VOLUME) {
    /* Volume object as points exception. */
    Volume *volume = ob->data;
    if (volume->display.wireframe_type == VOLUME_WIREFRAME_POINTS) {
      float *color;
      OVERLAY_ExtraCallBuffers *cb = OVERLAY_extra_call_buffer_get(vedata, ob);
      DRW_object_wire_theme_get(ob, draw_ctx->view_layer, &color);

      struct GPUBatch *geom = DRW_cache_object_face_wireframe_get(ob);
      if (geom) {
        OVERLAY_extra_loose_points(cb, geom, ob->obmat, color);
      }
      return;
    }
  }

  const bool is_edit_mode = DRW_object_is_in_edit_mode(ob);
  bool has_edit_mesh_cage = false;
  if (is_mesh && is_edit_mode) {
    /* TODO: Should be its own function. */
    Mesh *me = (Mesh *)ob->data;
    BMEditMesh *embm = me->edit_mesh;
    if (embm) {
      has_edit_mesh_cage = embm->mesh_eval_cage && (embm->mesh_eval_cage != embm->mesh_eval_final);
    }
  }

  /* Don't do that in edit Mesh mode, unless there is a modifier preview. */
  if (use_wire && (!is_mesh || (!is_edit_mode || has_edit_mesh_cage))) {
    const bool is_sculpt_mode = ((ob->mode & OB_MODE_SCULPT) != 0) && (ob->sculpt != NULL);
    const bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->v3d) &&
                                 !DRW_state_is_image_render();
    const bool use_coloring = (use_wire && !is_edit_mode && !is_sculpt_mode &&
                               !has_edit_mesh_cage);
    DRWShadingGroup *shgrp = NULL;
    struct GPUBatch *geom = DRW_cache_object_face_wireframe_get(ob);

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

      if (ob->type == OB_GPENCIL) {
        /* TODO (fclem) Make GPencil objects have correct boundbox. */
        DRW_shgroup_call_no_cull(shgrp, geom, ob);
      }
      else if (use_sculpt_pbvh) {
        DRW_shgroup_call_sculpt(shgrp, ob, true, false);
      }
      else {
        DRW_shgroup_call(shgrp, geom, ob);
      }
    }

    if (dupli) {
      dupli->wire_shgrp = shgrp;
      dupli->wire_geom = geom;
    }
  }
  else if (is_mesh && (!is_edit_mode || has_edit_mesh_cage)) {
    OVERLAY_ExtraCallBuffers *cb = OVERLAY_extra_call_buffer_get(vedata, ob);
    float *color;
    DRW_object_wire_theme_get(ob, draw_ctx->view_layer, &color);

    /* Draw loose geometry. */
    if (is_mesh_verts_only) {
      struct GPUBatch *geom = DRW_cache_mesh_all_verts_get(ob);
      if (geom) {
        OVERLAY_extra_loose_points(cb, geom, ob->obmat, color);
      }
    }
    else {
      struct GPUBatch *geom = DRW_cache_mesh_loose_edges_get(ob);
      if (geom) {
        OVERLAY_extra_wire(cb, geom, ob->obmat, color);
      }
    }
  }
}

void OVERLAY_wireframe_draw(OVERLAY_Data *data)
{
  OVERLAY_PassList *psl = data->psl;
  OVERLAY_PrivateData *pd = data->stl->pd;

  DRW_view_set_active(pd->view_wires);
  DRW_draw_pass(psl->wireframe_ps);

  DRW_view_set_active(NULL);
}

void OVERLAY_wireframe_in_front_draw(OVERLAY_Data *data)
{
  OVERLAY_PassList *psl = data->psl;
  OVERLAY_PrivateData *pd = data->stl->pd;

  if (psl->wireframe_xray_ps) {
    DRW_view_set_active(pd->view_wires);
    DRW_draw_pass(psl->wireframe_xray_ps);

    DRW_view_set_active(NULL);
  }
}
