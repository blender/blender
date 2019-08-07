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

#include "GPU_shader.h"

#include "UI_resources.h"

#include "DRW_engine.h"

#include "select_private.h"
#include "select_engine.h"

#define SELECT_ENGINE "SELECT_ENGINE"

/* *********** STATIC *********** */

static struct {
  SELECTID_Shaders sh_data[GPU_SHADER_CFG_LEN];
  struct SELECTID_Context context;
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
    /* Create view with depth offset */
    stl->g_data->view_faces = (DRWView *)DRW_view_default_get();
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
                                                                  OBACT(draw_ctx->view_layer));
    BLI_assert(e_data.context.select_mode != 0);
  }

  {
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

  e_data.context.last_object_drawn = 0;
  e_data.context.last_index_drawn = 1;
}

static void select_cache_populate(void *vedata, Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  struct BaseOffset *base_ofs = &e_data.context.index_offsets[e_data.context.last_object_drawn++];

  uint offset = e_data.context.last_index_drawn;

  select_id_draw_object(vedata,
                        draw_ctx->v3d,
                        ob,
                        e_data.context.select_mode,
                        offset,
                        &base_ofs->vert,
                        &base_ofs->edge,
                        &base_ofs->face);

  base_ofs->offset = offset;
  e_data.context.last_index_drawn = base_ofs->vert;
}

static void select_draw_scene(void *vedata)
{
  SELECTID_StorageList *stl = ((SELECTID_Data *)vedata)->stl;
  SELECTID_PassList *psl = ((SELECTID_Data *)vedata)->psl;

  /* Setup framebuffer */
  draw_select_framebuffer_select_id_setup(&e_data.context);
  GPU_framebuffer_bind(e_data.context.framebuffer_select_id);

  /* dithering and AA break color coding, so disable */
  glDisable(GL_DITHER);

  GPU_framebuffer_clear_color_depth(
      e_data.context.framebuffer_select_id, (const float[4]){0.0f}, 1.0f);

  DRW_view_set_active(stl->g_data->view_faces);
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
  MEM_SAFE_FREE(e_data.context.index_offsets);
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

struct SELECTID_Context *select_context_get(void)
{
  return &e_data.context;
}

/** \} */

#undef SELECT_ENGINE
