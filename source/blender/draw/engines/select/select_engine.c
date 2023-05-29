/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. */

/** \file
 * \ingroup draw_engine
 *
 * Engine for drawing a selection map where the pixels indicate the selection indices.
 */

#include "DNA_screen_types.h"

#include "ED_view3d.h"

#include "UI_resources.h"

#include "DRW_engine.h"
#include "DRW_select_buffer.h"

#include "draw_cache_impl.h"
#include "draw_manager.h"

#include "select_engine.h"
#include "select_private.h"

#define SELECT_ENGINE "SELECT_ENGINE"

/* *********** STATIC *********** */

static struct {
  struct GPUFrameBuffer *framebuffer_select_id;
  struct GPUTexture *texture_u32;

  SELECTID_Shaders sh_data[GPU_SHADER_CFG_LEN];
  struct SELECTID_Context context;
  uint runtime_new_objects;
} e_data = {NULL}; /* Engine data */

/* -------------------------------------------------------------------- */
/** \name Utils
 * \{ */

static void select_engine_framebuffer_setup(void)
{
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  int size[2];
  size[0] = GPU_texture_width(dtxl->depth);
  size[1] = GPU_texture_height(dtxl->depth);

  if (e_data.framebuffer_select_id == NULL) {
    e_data.framebuffer_select_id = GPU_framebuffer_create("framebuffer_select_id");
  }

  if ((e_data.texture_u32 != NULL) && ((GPU_texture_width(e_data.texture_u32) != size[0]) ||
                                       (GPU_texture_height(e_data.texture_u32) != size[1])))
  {
    GPU_texture_free(e_data.texture_u32);
    e_data.texture_u32 = NULL;
  }

  /* Make sure the depth texture is attached.
   * It may disappear when loading another Blender session. */
  GPU_framebuffer_texture_attach(e_data.framebuffer_select_id, dtxl->depth, 0, 0);

  if (e_data.texture_u32 == NULL) {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    e_data.texture_u32 = GPU_texture_create_2d(
        "select_buf_ids", size[0], size[1], 1, GPU_R32UI, usage, NULL);
    GPU_framebuffer_texture_attach(e_data.framebuffer_select_id, e_data.texture_u32, 0, 0);

    GPU_framebuffer_check_valid(e_data.framebuffer_select_id, NULL);
  }
}

/** \} */

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
    sh_data->select_id_flat = GPU_shader_create_from_info_name(
        sh_cfg == GPU_SHADER_CFG_CLIPPED ? "select_id_flat_clipped" : "select_id_flat");
  }
  if (!sh_data->select_id_uniform) {
    sh_data->select_id_uniform = GPU_shader_create_from_info_name(
        sh_cfg == GPU_SHADER_CFG_CLIPPED ? "select_id_uniform_clipped" : "select_id_uniform");
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
                           (int[2]){draw_ctx->region->winx, draw_ctx->region->winy},
                           e_data.context.last_rect.xmin,
                           e_data.context.last_rect.xmax,
                           e_data.context.last_rect.ymin,
                           e_data.context.last_rect.ymax,
                           winmat_subregion);

    stl->g_data->view_subregion = DRW_view_create(viewmat, winmat_subregion, NULL, NULL, NULL);

    /* Create view with depth offset */
    stl->g_data->view_faces = (DRWView *)view_default;
    stl->g_data->view_edges = DRW_view_create_with_zoffset(view_default, draw_ctx->rv3d, 1.0f);
    stl->g_data->view_verts = DRW_view_create_with_zoffset(view_default, draw_ctx->rv3d, 1.1f);
  }
}

static void select_cache_init(void *vedata)
{
  SELECTID_PassList *psl = ((SELECTID_Data *)vedata)->psl;
  SELECTID_StorageList *stl = ((SELECTID_Data *)vedata)->stl;
  SELECTID_PrivateData *pd = stl->g_data;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  SELECTID_Shaders *sh = &e_data.sh_data[draw_ctx->sh_cfg];

  if (e_data.context.select_mode == -1) {
    e_data.context.select_mode = select_id_get_object_select_mode(draw_ctx->scene,
                                                                  draw_ctx->obact);
    BLI_assert(e_data.context.select_mode != 0);
  }

  DRWState state = DRW_STATE_DEFAULT;
  state |= RV3D_CLIPPING_ENABLED(draw_ctx->v3d, draw_ctx->rv3d) ? DRW_STATE_CLIP_PLANES : 0;

  bool retopology_occlusion = RETOPOLOGY_ENABLED(draw_ctx->v3d) && !XRAY_ENABLED(draw_ctx->v3d);
  float retopology_offset = RETOPOLOGY_OFFSET(draw_ctx->v3d);

  {
    DRW_PASS_CREATE(psl->depth_only_pass, state);
    pd->shgrp_depth_only = DRW_shgroup_create(sh->select_id_uniform, psl->depth_only_pass);
    /* Not setting ID because this pass only draws to the depth buffer. */
    DRW_shgroup_uniform_float_copy(pd->shgrp_depth_only, "retopologyOffset", retopology_offset);

    if (retopology_occlusion) {
      pd->shgrp_occlude = DRW_shgroup_create(sh->select_id_uniform, psl->depth_only_pass);
      /* Not setting ID because this pass only draws to the depth buffer. */
      DRW_shgroup_uniform_float_copy(pd->shgrp_occlude, "retopologyOffset", 0.0f);
    }

    DRW_PASS_CREATE(psl->select_id_face_pass, state);
    if (e_data.context.select_mode & SCE_SELECT_FACE) {
      pd->shgrp_face_flat = DRW_shgroup_create(sh->select_id_flat, psl->select_id_face_pass);
      DRW_shgroup_uniform_float_copy(pd->shgrp_face_flat, "retopologyOffset", retopology_offset);
    }
    else {
      pd->shgrp_face_unif = DRW_shgroup_create(sh->select_id_uniform, psl->select_id_face_pass);
      DRW_shgroup_uniform_int_copy(pd->shgrp_face_unif, "select_id", 0);
      DRW_shgroup_uniform_float_copy(pd->shgrp_face_unif, "retopologyOffset", retopology_offset);
    }

    if (e_data.context.select_mode & SCE_SELECT_EDGE) {
      DRW_PASS_CREATE(psl->select_id_edge_pass, state | DRW_STATE_FIRST_VERTEX_CONVENTION);

      pd->shgrp_edge = DRW_shgroup_create(sh->select_id_flat, psl->select_id_edge_pass);
      DRW_shgroup_uniform_float_copy(pd->shgrp_edge, "retopologyOffset", retopology_offset);
    }

    if (e_data.context.select_mode & SCE_SELECT_VERTEX) {
      DRW_PASS_CREATE(psl->select_id_vert_pass, state);
      pd->shgrp_vert = DRW_shgroup_create(sh->select_id_flat, psl->select_id_vert_pass);
      DRW_shgroup_uniform_float_copy(pd->shgrp_vert, "sizeVertex", 2 * G_draw.block.size_vertex);
      DRW_shgroup_uniform_float_copy(pd->shgrp_vert, "retopologyOffset", retopology_offset);
    }
  }

  /* Check if the viewport has changed. */
  float(*persmat)[4] = draw_ctx->rv3d->persmat;
  e_data.context.is_dirty = !compare_m4m4(e_data.context.persmat, persmat, FLT_EPSILON);

  if (!e_data.context.is_dirty) {
    /* Check if any of the drawn objects have been transformed. */
    Object **ob = &e_data.context.objects_drawn[0];
    for (uint i = e_data.context.objects_drawn_len; i--; ob++) {
      DrawData *data = DRW_drawdata_get(&(*ob)->id, &draw_engine_select_type);
      if (data && (data->recalc & ID_RECALC_TRANSFORM) != 0) {
        data->recalc &= ~ID_RECALC_TRANSFORM;
        e_data.context.is_dirty = true;
      }
    }
  }

  if (e_data.context.is_dirty) {
    /* Remove all tags from drawn or culled objects. */
    copy_m4_m4(e_data.context.persmat, persmat);
    e_data.context.objects_drawn_len = 0;
    e_data.context.index_drawn_len = 1;
    select_engine_framebuffer_setup();
    GPU_framebuffer_bind(e_data.framebuffer_select_id);
    GPU_framebuffer_clear_color_depth(e_data.framebuffer_select_id, (const float[4]){0.0f}, 1.0f);
  }
  e_data.runtime_new_objects = 0;
}

static void select_cache_populate(void *vedata, Object *ob)
{
  SELECTID_StorageList *stl = ((SELECTID_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  const bool retopology_occlusion = RETOPOLOGY_ENABLED(draw_ctx->v3d) &&
                                    !XRAY_ENABLED(draw_ctx->v3d);
  if (retopology_occlusion && !DRW_object_is_in_edit_mode(ob)) {
    if (ob->dt >= OB_SOLID) {
      struct GPUBatch *geom_faces = DRW_mesh_batch_cache_get_surface(ob->data);
      DRW_shgroup_call_obmat(stl->g_data->shgrp_occlude, geom_faces, ob->object_to_world);
    }
    return;
  }

  SELECTID_ObjectData *sel_data = (SELECTID_ObjectData *)DRW_drawdata_get(
      &ob->id, &draw_engine_select_type);

  if (!e_data.context.is_dirty && sel_data && sel_data->is_drawn) {
    /* The object indices have already been drawn. Fill depth pass.
     * Optimization: Most of the time this depth pass is not used. */
    struct Mesh *me = ob->data;
    if (e_data.context.select_mode & SCE_SELECT_FACE) {
      struct GPUBatch *geom_faces = DRW_mesh_batch_cache_get_triangles_with_select_id(me);
      DRW_shgroup_call_obmat(stl->g_data->shgrp_depth_only, geom_faces, ob->object_to_world);
    }
    else if (ob->dt >= OB_SOLID) {
#ifdef USE_CAGE_OCCLUSION
      struct GPUBatch *geom_faces = DRW_mesh_batch_cache_get_triangles_with_select_id(me);
#else
      struct GPUBatch *geom_faces = DRW_mesh_batch_cache_get_surface(me);
#endif
      DRW_shgroup_call_obmat(stl->g_data->shgrp_depth_only, geom_faces, ob->object_to_world);
    }

    if (e_data.context.select_mode & SCE_SELECT_EDGE) {
      struct GPUBatch *geom_edges = DRW_mesh_batch_cache_get_edges_with_select_id(me);
      DRW_shgroup_call_obmat(stl->g_data->shgrp_depth_only, geom_edges, ob->object_to_world);
    }

    if (e_data.context.select_mode & SCE_SELECT_VERTEX) {
      struct GPUBatch *geom_verts = DRW_mesh_batch_cache_get_verts_with_select_id(me);
      DRW_shgroup_call_obmat(stl->g_data->shgrp_depth_only, geom_verts, ob->object_to_world);
    }
    return;
  }

  float min[3], max[3];
  select_id_object_min_max(ob, min, max);

  if (DRW_culling_min_max_test(stl->g_data->view_subregion, ob->object_to_world, min, max)) {
    if (sel_data == NULL) {
      sel_data = (SELECTID_ObjectData *)DRW_drawdata_ensure(
          &ob->id, &draw_engine_select_type, sizeof(SELECTID_ObjectData), NULL, NULL);
    }
    sel_data->dd.recalc = 0;
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

  DRW_view_set_active(stl->g_data->view_faces);

  if (!DRW_pass_is_empty(psl->depth_only_pass)) {
    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
    GPU_framebuffer_bind(dfbl->depth_only_fb);
    GPU_framebuffer_clear_depth(dfbl->depth_only_fb, 1.0f);
    DRW_draw_pass(psl->depth_only_pass);
  }

  /* Setup framebuffer */
  GPU_framebuffer_bind(e_data.framebuffer_select_id);

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

  DRW_TEXTURE_FREE_SAFE(e_data.texture_u32);
  GPU_FRAMEBUFFER_FREE_SAFE(e_data.framebuffer_select_id);
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
    /*instance_free*/ NULL,
    &select_cache_init,
    &select_cache_populate,
    NULL,
    &select_draw_scene,
    NULL,
    NULL,
    NULL,
    NULL,
};

/* NOTE: currently unused, we may want to register so we can see this when debugging the view. */

RenderEngineType DRW_engine_viewport_select_type = {
    NULL,
    NULL,
    SELECT_ENGINE,
    N_("Select ID"),
    RE_INTERNAL | RE_USE_STEREO_VIEWPORT | RE_USE_GPU_CONTEXT,
    NULL,
    NULL,
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

GPUFrameBuffer *DRW_engine_select_framebuffer_get(void)
{
  return e_data.framebuffer_select_id;
}

GPUTexture *DRW_engine_select_texture_get(void)
{
  return e_data.texture_u32;
}

/** \} */

#undef SELECT_ENGINE
