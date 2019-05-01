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
 * Copyright 2017, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 *
 * Base engine for external render engines.
 * We use it for depth and non-mesh objects.
 */

#include "DRW_render.h"

#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "ED_screen.h"

#include "GPU_matrix.h"
#include "GPU_shader.h"
#include "GPU_viewport.h"

#include "external_engine.h" /* own include */

/* Shaders */

#define EXTERNAL_ENGINE "BLENDER_EXTERNAL"

/* *********** LISTS *********** */

/* GPUViewport.storage
 * Is freed everytime the viewport engine changes */
typedef struct EXTERNAL_Storage {
  int dummy;
} EXTERNAL_Storage;

typedef struct EXTERNAL_StorageList {
  struct EXTERNAL_Storage *storage;
  struct EXTERNAL_PrivateData *g_data;
} EXTERNAL_StorageList;

typedef struct EXTERNAL_FramebufferList {
  struct GPUFrameBuffer *depth_buffer_fb;
} EXTERNAL_FramebufferList;

typedef struct EXTERNAL_TextureList {
  /* default */
  struct GPUTexture *depth_buffer_tx;
} EXTERNAL_TextureList;

typedef struct EXTERNAL_PassList {
  struct DRWPass *depth_pass;
} EXTERNAL_PassList;

typedef struct EXTERNAL_Data {
  void *engine_type;
  EXTERNAL_FramebufferList *fbl;
  EXTERNAL_TextureList *txl;
  EXTERNAL_PassList *psl;
  EXTERNAL_StorageList *stl;
  char info[GPU_INFO_SIZE];
} EXTERNAL_Data;

/* *********** STATIC *********** */

static struct {
  /* Depth Pre Pass */
  struct GPUShader *depth_sh;
  bool draw_depth;
} e_data = {NULL}; /* Engine data */

typedef struct EXTERNAL_PrivateData {
  DRWShadingGroup *depth_shgrp;

  /* Do we need to update the depth or can we reuse the last calculated texture. */
  bool update_depth;
  bool view_updated;

  float last_mat[4][4];
  float curr_mat[4][4];
} EXTERNAL_PrivateData; /* Transient data */

/* Functions */

static void external_engine_init(void *vedata)
{
  EXTERNAL_StorageList *stl = ((EXTERNAL_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  RegionView3D *rv3d = draw_ctx->rv3d;

  /* Depth prepass */
  if (!e_data.depth_sh) {
    e_data.depth_sh = DRW_shader_create_3d_depth_only(GPU_SHADER_CFG_DEFAULT);
  }

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
    stl->g_data->update_depth = true;
    stl->g_data->view_updated = false;
  }

  if (stl->g_data->update_depth == false) {
    if (rv3d && rv3d->rflag & RV3D_NAVIGATING) {
      stl->g_data->update_depth = true;
    }
  }

  if (stl->g_data->view_updated) {
    stl->g_data->update_depth = true;
    stl->g_data->view_updated = false;
  }

  {
    float view[4][4];
    float win[4][4];
    DRW_viewport_matrix_get(view, DRW_MAT_VIEW);
    DRW_viewport_matrix_get(win, DRW_MAT_WIN);
    mul_m4_m4m4(stl->g_data->curr_mat, view, win);
    if (!equals_m4m4(stl->g_data->curr_mat, stl->g_data->last_mat)) {
      stl->g_data->update_depth = true;
    }
  }
}

static void external_cache_init(void *vedata)
{
  EXTERNAL_PassList *psl = ((EXTERNAL_Data *)vedata)->psl;
  EXTERNAL_StorageList *stl = ((EXTERNAL_Data *)vedata)->stl;
  EXTERNAL_TextureList *txl = ((EXTERNAL_Data *)vedata)->txl;
  EXTERNAL_FramebufferList *fbl = ((EXTERNAL_Data *)vedata)->fbl;

  {
    DRW_texture_ensure_fullscreen_2d(&txl->depth_buffer_tx, GPU_DEPTH24_STENCIL8, 0);

    GPU_framebuffer_ensure_config(&fbl->depth_buffer_fb,
                                  {
                                      GPU_ATTACHMENT_TEXTURE(txl->depth_buffer_tx),
                                  });
  }

  /* Depth Pass */
  {
    psl->depth_pass = DRW_pass_create("Depth Pass",
                                      DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL);
    stl->g_data->depth_shgrp = DRW_shgroup_create(e_data.depth_sh, psl->depth_pass);
  }

  /* Do not draw depth pass when overlays are turned off. */
  e_data.draw_depth = false;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const View3D *v3d = draw_ctx->v3d;
  if (v3d->flag2 & V3D_HIDE_OVERLAYS) {
    /* mark `update_depth` for when overlays are turned on again. */
    stl->g_data->update_depth = true;
    return;
  }
}

static void external_cache_populate(void *vedata, Object *ob)
{
  EXTERNAL_StorageList *stl = ((EXTERNAL_Data *)vedata)->stl;

  if (!DRW_object_is_renderable(ob)) {
    return;
  }

  /* Do not draw depth pass when overlays are turned off. */
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const View3D *v3d = draw_ctx->v3d;
  if (v3d->flag2 & V3D_HIDE_OVERLAYS) {
    return;
  }

  if (stl->g_data->update_depth) {
    e_data.draw_depth = true;
    struct GPUBatch *geom = DRW_cache_object_surface_get(ob);
    if (geom) {
      /* Depth Prepass */
      DRW_shgroup_call_add(stl->g_data->depth_shgrp, geom, ob->obmat);
    }
  }
}

static void external_cache_finish(void *UNUSED(vedata))
{
}

static void external_draw_scene_do(void *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  RegionView3D *rv3d = draw_ctx->rv3d;
  ARegion *ar = draw_ctx->ar;
  RenderEngineType *type;

  DRW_state_reset_ex(DRW_STATE_DEFAULT & ~DRW_STATE_DEPTH_LESS_EQUAL);

  /* Create render engine. */
  if (!rv3d->render_engine) {
    RenderEngineType *engine_type = draw_ctx->engine_type;

    if (!(engine_type->view_update && engine_type->view_draw)) {
      return;
    }

    RenderEngine *engine = RE_engine_create_ex(engine_type, true);
    engine->tile_x = scene->r.tilex;
    engine->tile_y = scene->r.tiley;
    engine_type->view_update(engine, draw_ctx->evil_C);
    rv3d->render_engine = engine;
  }

  /* Rendered draw. */
  GPU_matrix_push_projection();
  ED_region_pixelspace(ar);

  /* Render result draw. */
  type = rv3d->render_engine->type;
  type->view_draw(rv3d->render_engine, draw_ctx->evil_C);

  GPU_matrix_pop_projection();

  /* Set render info. */
  EXTERNAL_Data *data = vedata;
  if (rv3d->render_engine->text[0] != '\0') {
    BLI_strncpy(data->info, rv3d->render_engine->text, sizeof(data->info));
  }
  else {
    data->info[0] = '\0';
  }
}

static void external_draw_scene(void *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  EXTERNAL_StorageList *stl = ((EXTERNAL_Data *)vedata)->stl;
  EXTERNAL_PassList *psl = ((EXTERNAL_Data *)vedata)->psl;
  EXTERNAL_FramebufferList *fbl = ((EXTERNAL_Data *)vedata)->fbl;
  const DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  /* Will be NULL during OpenGL render.
   * OpenGL render is used for quick preview (thumbnails or sequencer preview)
   * where using the rendering engine to preview doesn't make so much sense. */
  if (draw_ctx->evil_C) {
    external_draw_scene_do(vedata);
  }

  if (e_data.draw_depth) {
    DRW_draw_pass(psl->depth_pass);
    // copy result to tmp buffer
    GPU_framebuffer_blit(dfbl->depth_only_fb, 0, fbl->depth_buffer_fb, 0, GPU_DEPTH_BIT);
    stl->g_data->update_depth = false;
  }
  else {
    // copy tmp buffer to default
    GPU_framebuffer_blit(fbl->depth_buffer_fb, 0, dfbl->depth_only_fb, 0, GPU_DEPTH_BIT);
  }

  copy_m4_m4(stl->g_data->last_mat, stl->g_data->curr_mat);
}

static void external_view_update(void *vedata)
{
  EXTERNAL_Data *data = vedata;
  EXTERNAL_StorageList *stl = data->stl;
  if (stl && stl->g_data) {
    stl->g_data->view_updated = true;
  }
}

static void external_engine_free(void)
{
  /* All shaders are builtin. */
}

static const DrawEngineDataSize external_data_size = DRW_VIEWPORT_DATA_SIZE(EXTERNAL_Data);

static DrawEngineType draw_engine_external_type = {
    NULL,
    NULL,
    N_("External"),
    &external_data_size,
    &external_engine_init,
    &external_engine_free,
    &external_cache_init,
    &external_cache_populate,
    &external_cache_finish,
    NULL,
    &external_draw_scene,
    &external_view_update,
    NULL,
    NULL,
};

/* Note: currently unused,
 * we should not register unless we want to see this when debugging the view. */

RenderEngineType DRW_engine_viewport_external_type = {
    NULL,
    NULL,
    EXTERNAL_ENGINE,
    N_("External"),
    RE_INTERNAL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &draw_engine_external_type,
    {NULL, NULL, NULL},
};

#undef EXTERNAL_ENGINE
