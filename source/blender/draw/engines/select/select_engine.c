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
 *
 * Engine for drawing a selection map where the pixels indicate the selection indices.
 */

#include "DNA_screen_types.h"

#include "UI_resources.h"

#include "DRW_engine.h"
#include "DRW_select_buffer.h"

#include "draw_cache_impl.h"
#include "draw_manager.h"

#include "select_private.h"
#include "select_engine.h"

#define SELECT_ENGINE "SELECT_ENGINE"

/* *********** STATIC *********** */

static struct {
  SELECTID_Shaders sh_data[GPU_SHADER_CFG_LEN];
  struct SELECTID_Context context;
  uint runtime_new_objects;
} e_data = {{{NULL}}}; /* Engine data */

/* Shaders */
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_selection_id_3D_vert_glsl[];
extern char datatoc_selection_id_frag_glsl[];

/* -------------------------------------------------------------------- */
/** \name Engine Functions
 * \{ */

static void select_engine_init(void *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  eGPUShaderConfig sh_cfg = draw_ctx->sh_cfg;

  SELECTID_StorageList *stl = ((SELECTID_Data *)vedata)->stl;
  SELECTID_Shaders *sh_data = &e_data.sh_data[sh_cfg];

  /* Prepass */
  if (!sh_data->select_id_flat) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    sh_data->select_id_flat = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_selection_id_3D_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_selection_id_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, NULL},
    });
  }
  if (!sh_data->select_id_uniform) {
    const GPUShaderConfigData *sh_cfg_data = &GPU_shader_cfg_data[sh_cfg];
    sh_data->select_id_uniform = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg_data->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_selection_id_3D_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_selection_id_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg_data->def, "#define UNIFORM_ID\n", NULL},
    });
  }

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
  }

  {
    /* Create view from a subregion */
    const DRWView *view_default = DRW_view_default_get();
    float viewmat[4][4], winmat[4][4], winmat_subregion[4][4];
    DRW_view_viewmat_get(view_default, viewmat, false);
    DRW_view_winmat_get(view_default, winmat, false);
    projmat_from_subregion(winmat,
                           (int[2]){draw_ctx->ar->winx, draw_ctx->ar->winy},
                           e_data.context.last_rect.xmin,
                           e_data.context.last_rect.xmax,
                           e_data.context.last_rect.ymin,
                           e_data.context.last_rect.ymax,
                           winmat_subregion);

    stl->g_data->view_subregion = DRW_view_create(viewmat, winmat_subregion, NULL, NULL, NULL);

    /* Create view with depth offset */
    stl->g_data->view_faces = (DRWView *)view_default;
    stl->g_data->view_edges = DRW_view_create_with_zoffset(draw_ctx->rv3d, 1.0f);
    stl->g_data->view_verts = DRW_view_create_with_zoffset(draw_ctx->rv3d, 1.1f);
  }
}

static void select_cache_init(void *vedata)
{
  SELECTID_PassList *psl = ((SELECTID_Data *)vedata)->psl;
  SELECTID_StorageList *stl = ((SELECTID_Data *)vedata)->stl;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  SELECTID_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  if (e_data.context.select_mode == -1) {
    e_data.context.select_mode = select_id_get_object_select_mode(draw_ctx->scene,
                                                                  draw_ctx->obact);
    BLI_assert(e_data.context.select_mode != 0);
  }

  {
    psl->depth_only_pass = DRW_pass_create("Depth Only Pass", DRW_STATE_DEFAULT);
    stl->g_data->shgrp_depth_only = DRW_shgroup_create(sh_data->select_id_uniform,
                                                       psl->depth_only_pass);

    if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      DRW_shgroup_state_enable(stl->g_data->shgrp_depth_only, DRW_STATE_CLIP_PLANES);
    }

    psl->select_id_face_pass = DRW_pass_create("Face Pass", DRW_STATE_DEFAULT);

    if (e_data.context.select_mode & SCE_SELECT_FACE) {
      stl->g_data->shgrp_face_flat = DRW_shgroup_create(sh_data->select_id_flat,
                                                        psl->select_id_face_pass);

      if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
        DRW_shgroup_state_enable(stl->g_data->shgrp_face_flat, DRW_STATE_CLIP_PLANES);
      }
    }
    else {
      stl->g_data->shgrp_face_unif = DRW_shgroup_create(sh_data->select_id_uniform,
                                                        psl->select_id_face_pass);
      DRW_shgroup_uniform_int_copy(stl->g_data->shgrp_face_unif, "id", 0);

      if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
        DRW_shgroup_state_enable(stl->g_data->shgrp_face_unif, DRW_STATE_CLIP_PLANES);
      }
    }

    if (e_data.context.select_mode & SCE_SELECT_EDGE) {
      psl->select_id_edge_pass = DRW_pass_create(
          "Edge Pass", DRW_STATE_DEFAULT | DRW_STATE_FIRST_VERTEX_CONVENTION);

      stl->g_data->shgrp_edge = DRW_shgroup_create(sh_data->select_id_flat,
                                                   psl->select_id_edge_pass);

      if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
        DRW_shgroup_state_enable(stl->g_data->shgrp_edge, DRW_STATE_CLIP_PLANES);
      }
    }

    if (e_data.context.select_mode & SCE_SELECT_VERTEX) {
      psl->select_id_vert_pass = DRW_pass_create("Vert Pass", DRW_STATE_DEFAULT);
      stl->g_data->shgrp_vert = DRW_shgroup_create(sh_data->select_id_flat,
                                                   psl->select_id_vert_pass);
      DRW_shgroup_uniform_float_copy(
          stl->g_data->shgrp_vert, "sizeVertex", G_draw.block.sizeVertex);

      if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
        DRW_shgroup_state_enable(stl->g_data->shgrp_vert, DRW_STATE_CLIP_PLANES);
      }
    }
  }

  /* Check if the viewport has changed. */
  float(*persmat)[4] = draw_ctx->rv3d->persmat;
  e_data.context.is_dirty = !compare_m4m4(e_data.context.persmat, persmat, FLT_EPSILON);
  if (e_data.context.is_dirty) {
    copy_m4_m4(e_data.context.persmat, persmat);
    select_id_context_clear(&e_data.context);
  }
  e_data.runtime_new_objects = 0;
}

static void select_cache_populate(void *vedata, Object *ob)
{
  SELECTID_StorageList *stl = ((SELECTID_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  SELECTID_ObjectData *sel_data = (SELECTID_ObjectData *)DRW_drawdata_get(
      &ob->id, &draw_engine_select_type);

  if (!e_data.context.is_dirty && sel_data && sel_data->is_drawn) {
    /* The object indices have already been drawn. Fill depth pass.
     * Opti: Most of the time this depth pass is not used. */
    struct Mesh *me = ob->data;
    struct GPUBatch *geom_faces;
    if (e_data.context.select_mode & SCE_SELECT_FACE) {
      geom_faces = DRW_mesh_batch_cache_get_triangles_with_select_id(me);
    }
    else {
      geom_faces = DRW_mesh_batch_cache_get_surface(me);
    }
    DRW_shgroup_call_obmat(stl->g_data->shgrp_depth_only, geom_faces, ob->obmat);

    if (e_data.context.select_mode & SCE_SELECT_EDGE) {
      struct GPUBatch *geom_edges = DRW_mesh_batch_cache_get_edges_with_select_id(me);
      DRW_shgroup_call_obmat(stl->g_data->shgrp_depth_only, geom_edges, ob->obmat);
    }

    if (e_data.context.select_mode & SCE_SELECT_VERTEX) {
      struct GPUBatch *geom_verts = DRW_mesh_batch_cache_get_verts_with_select_id(me);
      DRW_shgroup_call_obmat(stl->g_data->shgrp_depth_only, geom_verts, ob->obmat);
    }
    return;
  }

  float min[3], max[3];
  select_id_object_min_max(ob, min, max);

  if (DRW_culling_min_max_test(stl->g_data->view_subregion, ob->obmat, min, max)) {
    if (sel_data == NULL) {
      sel_data = (SELECTID_ObjectData *)DRW_drawdata_ensure(
          &ob->id, &draw_engine_select_type, sizeof(SELECTID_ObjectData), NULL, NULL);
    }
    sel_data->drawn_index = e_data.context.objects_drawn_len;
    sel_data->is_drawn = true;

    struct ObjectOffsets *ob_offsets =
        &e_data.context.index_offsets[e_data.context.objects_drawn_len];

    uint offset = e_data.context.index_drawn_len;
    select_id_draw_object(vedata,
                          draw_ctx->v3d,
                          ob,
                          e_data.context.select_mode,
                          offset,
                          &ob_offsets->vert,
                          &ob_offsets->edge,
                          &ob_offsets->face);

    ob_offsets->offset = offset;
    e_data.context.index_drawn_len = ob_offsets->vert;
    e_data.context.objects_drawn[e_data.context.objects_drawn_len] = ob;
    e_data.context.objects_drawn_len++;
    e_data.runtime_new_objects++;
  }
  else if (sel_data) {
    sel_data->is_drawn = false;
  }
}

static void select_draw_scene(void *vedata)
{
  SELECTID_StorageList *stl = ((SELECTID_Data *)vedata)->stl;
  SELECTID_PassList *psl = ((SELECTID_Data *)vedata)->psl;

  if (!e_data.runtime_new_objects) {
    /* Nothing new needs to be drawn. */
    return;
  }

  /* dithering and AA break color coding, so disable */
  glDisable(GL_DITHER);

  DRW_view_set_active(stl->g_data->view_faces);

  if (!DRW_pass_is_empty(psl->depth_only_pass)) {
    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
    GPU_framebuffer_bind(dfbl->depth_only_fb);
    GPU_framebuffer_clear_depth(dfbl->depth_only_fb, 1.0f);
    DRW_draw_pass(psl->depth_only_pass);
  }

  /* Setup framebuffer */
  GPU_framebuffer_bind(e_data.context.framebuffer_select_id);

  DRW_draw_pass(psl->select_id_face_pass);

  if (e_data.context.select_mode & SCE_SELECT_EDGE) {
    DRW_view_set_active(stl->g_data->view_edges);
    DRW_draw_pass(psl->select_id_edge_pass);
  }

  if (e_data.context.select_mode & SCE_SELECT_VERTEX) {
    DRW_view_set_active(stl->g_data->view_verts);
    DRW_draw_pass(psl->select_id_vert_pass);
  }
}

static void select_engine_free(void)
{
  for (int sh_data_index = 0; sh_data_index < ARRAY_SIZE(e_data.sh_data); sh_data_index++) {
    SELECTID_Shaders *sh_data = &e_data.sh_data[sh_data_index];
    DRW_SHADER_FREE_SAFE(sh_data->select_id_flat);
    DRW_SHADER_FREE_SAFE(sh_data->select_id_uniform);
  }

  DRW_TEXTURE_FREE_SAFE(e_data.context.texture_u32);
  GPU_FRAMEBUFFER_FREE_SAFE(e_data.context.framebuffer_select_id);
  MEM_SAFE_FREE(e_data.context.objects);
  MEM_SAFE_FREE(e_data.context.index_offsets);
  MEM_SAFE_FREE(e_data.context.objects_drawn);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Engine Type
 * \{ */

static const DrawEngineDataSize select_data_size = DRW_VIEWPORT_DATA_SIZE(SELECTID_Data);

DrawEngineType draw_engine_select_type = {
    NULL,
    NULL,
    N_("Select ID"),
    &select_data_size,
    &select_engine_init,
    &select_engine_free,
    &select_cache_init,
    &select_cache_populate,
    NULL,
    NULL,
    &select_draw_scene,
    NULL,
    NULL,
    NULL,
};

/* Note: currently unused, we may want to register so we can see this when debugging the view. */

RenderEngineType DRW_engine_viewport_select_type = {
    NULL,
    NULL,
    SELECT_ENGINE,
    N_("Select ID"),
    RE_INTERNAL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &draw_engine_select_type,
    {NULL, NULL, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Exposed `select_private.h` functions
 * \{ */

struct SELECTID_Context *DRW_select_engine_context_get(void)
{
  return &e_data.context;
}

/** \} */

#undef SELECT_ENGINE
