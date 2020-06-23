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

#include "DNA_modifier_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_object.h"
#include "BKE_particle.h"

#include "ED_screen.h"

#include "GPU_matrix.h"
#include "GPU_shader.h"
#include "GPU_viewport.h"

#include "external_engine.h" /* own include */

/* Shaders */

#define EXTERNAL_ENGINE "BLENDER_EXTERNAL"

extern char datatoc_depth_frag_glsl[];
extern char datatoc_depth_vert_glsl[];

extern char datatoc_common_view_lib_glsl[];

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
} e_data = {NULL}; /* Engine data */

typedef struct EXTERNAL_PrivateData {
  DRWShadingGroup *depth_shgrp;

  /* Do we need to update the depth or can we reuse the last calculated texture. */
  bool need_depth;
  bool update_depth;
} EXTERNAL_PrivateData; /* Transient data */

/* Functions */

static void external_engine_init(void *vedata)
{
  EXTERNAL_StorageList *stl = ((EXTERNAL_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ARegion *region = draw_ctx->region;

  /* Depth prepass */
  if (!e_data.depth_sh) {
    const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[GPU_SHADER_CFG_DEFAULT];

    e_data.depth_sh = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_depth_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_depth_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });
  }

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
    stl->g_data->need_depth = true;
  }

  stl->g_data->update_depth = true;

  /* Progressive render samples are tagged with no rebuild, in that case we
   * can skip updating the depth buffer */
  if (region && (region->do_draw & RGN_DRAW_NO_REBUILD)) {
    stl->g_data->update_depth = false;
  }
}

static void external_cache_init(void *vedata)
{
  EXTERNAL_PassList *psl = ((EXTERNAL_Data *)vedata)->psl;
  EXTERNAL_StorageList *stl = ((EXTERNAL_Data *)vedata)->stl;
  EXTERNAL_TextureList *txl = ((EXTERNAL_Data *)vedata)->txl;
  EXTERNAL_FramebufferList *fbl = ((EXTERNAL_Data *)vedata)->fbl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const View3D *v3d = draw_ctx->v3d;

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
  stl->g_data->need_depth = (v3d->flag2 & V3D_HIDE_OVERLAYS) == 0;
}

static void external_cache_populate(void *vedata, Object *ob)
{
  EXTERNAL_StorageList *stl = ((EXTERNAL_Data *)vedata)->stl;

  if (!(DRW_object_is_renderable(ob) &&
        DRW_object_visibility_in_active_context(ob) & OB_VISIBLE_SELF)) {
    return;
  }

  if (ob->type == OB_GPENCIL) {
    /* Grease Pencil objects need correct depth to do the blending. */
    stl->g_data->need_depth = true;
    return;
  }

  if (ob->type == OB_MESH && ob->modifiers.first != NULL) {
    LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
      if (md->type != eModifierType_ParticleSystem) {
        continue;
      }
      ParticleSystem *psys = ((ParticleSystemModifierData *)md)->psys;
      if (!DRW_object_is_visible_psys_in_active_context(ob, psys)) {
        continue;
      }
      ParticleSettings *part = psys->part;
      const int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;

      if (draw_as == PART_DRAW_PATH) {
        struct GPUBatch *hairs = DRW_cache_particles_get_hair(ob, psys, NULL);
        DRW_shgroup_call(stl->g_data->depth_shgrp, hairs, NULL);
      }
    }
  }
  struct GPUBatch *geom = DRW_cache_object_surface_get(ob);
  if (geom) {
    /* Depth Prepass */
    DRW_shgroup_call(stl->g_data->depth_shgrp, geom, ob);
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
  ARegion *region = draw_ctx->region;
  const RenderEngineType *type;

  DRW_state_reset_ex(DRW_STATE_DEFAULT & ~DRW_STATE_DEPTH_LESS_EQUAL);

  /* Create render engine. */
  if (!rv3d->render_engine) {
    RenderEngineType *engine_type = draw_ctx->engine_type;

    if (!(engine_type->view_update && engine_type->view_draw)) {
      return;
    }

    RenderEngine *engine = RE_engine_create(engine_type);
    engine->tile_x = scene->r.tilex;
    engine->tile_y = scene->r.tiley;
    engine_type->view_update(engine, draw_ctx->evil_C, draw_ctx->depsgraph);
    rv3d->render_engine = engine;
  }

  /* Rendered draw. */
  GPU_matrix_push_projection();
  GPU_matrix_push();
  ED_region_pixelspace(region);

  /* Render result draw. */
  type = rv3d->render_engine->type;
  type->view_draw(rv3d->render_engine, draw_ctx->evil_C, draw_ctx->depsgraph);

  GPU_matrix_pop();
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
    float clear_col[4] = {0, 0, 0, 0};
    /* This is to keep compatibility with external engine. */
    /* TODO(fclem) remove it eventually. */
    GPU_framebuffer_bind(dfbl->default_fb);
    GPU_framebuffer_clear_color(dfbl->default_fb, clear_col);

    external_draw_scene_do(vedata);
  }

  if (stl->g_data->update_depth && stl->g_data->need_depth) {
    DRW_draw_pass(psl->depth_pass);
    /* Copy main depth buffer to cached framebuffer. */
    GPU_framebuffer_blit(dfbl->depth_only_fb, 0, fbl->depth_buffer_fb, 0, GPU_DEPTH_BIT);
  }

  /* Copy cached depth buffer to main framebuffer. */
  GPU_framebuffer_blit(fbl->depth_buffer_fb, 0, dfbl->depth_only_fb, 0, GPU_DEPTH_BIT);
}

static void external_engine_free(void)
{
  DRW_SHADER_FREE_SAFE(e_data.depth_sh);
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
    &external_draw_scene,
    NULL,
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
    RE_INTERNAL | RE_USE_STEREO_VIEWPORT,
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
