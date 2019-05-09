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
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_pbvh.h"
#include "BKE_paint.h"
#include "BKE_subdiv_ccg.h"

/* If builtin shaders are needed */
#include "GPU_shader.h"

#include "draw_common.h"
#include "draw_mode_engines.h"

extern char datatoc_common_view_lib_glsl[];
extern char datatoc_sculpt_mask_vert_glsl[];
extern char datatoc_gpu_shader_3D_smooth_color_frag_glsl[];

/* *********** LISTS *********** */
/* All lists are per viewport specific datas.
 * They are all free when viewport changes engines
 * or is free itself. Use SCULPT_engine_init() to
 * initialize most of them and SCULPT_cache_init()
 * for SCULPT_PassList */

typedef struct SCULPT_PassList {
  /* Declare all passes here and init them in
   * SCULPT_cache_init().
   * Only contains (DRWPass *) */
  struct DRWPass *pass;
} SCULPT_PassList;

typedef struct SCULPT_FramebufferList {
  /* Contains all framebuffer objects needed by this engine.
   * Only contains (GPUFrameBuffer *) */
  struct GPUFrameBuffer *fb;
} SCULPT_FramebufferList;

typedef struct SCULPT_TextureList {
  /* Contains all framebuffer textures / utility textures
   * needed by this engine. Only viewport specific textures
   * (not per object). Only contains (GPUTexture *) */
  struct GPUTexture *texture;
} SCULPT_TextureList;

typedef struct SCULPT_StorageList {
  /* Contains any other memory block that the engine needs.
   * Only directly MEM_(m/c)allocN'ed blocks because they are
   * free with MEM_freeN() when viewport is freed.
   * (not per object) */
  struct CustomStruct *block;
  struct SCULPT_PrivateData *g_data;
} SCULPT_StorageList;

typedef struct SCULPT_Data {
  /* Struct returned by DRW_viewport_engine_data_ensure.
   * If you don't use one of these, just make it a (void *) */
  // void *fbl;
  void *engine_type; /* Required */
  SCULPT_FramebufferList *fbl;
  SCULPT_TextureList *txl;
  SCULPT_PassList *psl;
  SCULPT_StorageList *stl;
} SCULPT_Data;

/* *********** STATIC *********** */

static struct {
  struct GPUShader *shader_mask;
} e_data = {NULL}; /* Engine data */

typedef struct SCULPT_PrivateData {
  DRWShadingGroup *mask_overlay_grp;
} SCULPT_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

/* Init Textures, Framebuffers, Storage and Shaders.
 * It is called for every frames.
 * (Optional) */
static void SCULPT_engine_init(void *vedata)
{
  SCULPT_TextureList *txl = ((SCULPT_Data *)vedata)->txl;
  SCULPT_FramebufferList *fbl = ((SCULPT_Data *)vedata)->fbl;
  SCULPT_StorageList *stl = ((SCULPT_Data *)vedata)->stl;

  UNUSED_VARS(txl, fbl, stl);

  if (!e_data.shader_mask) {
    e_data.shader_mask = DRW_shader_create_with_lib(datatoc_sculpt_mask_vert_glsl,
                                                    NULL,
                                                    datatoc_gpu_shader_3D_smooth_color_frag_glsl,
                                                    datatoc_common_view_lib_glsl,
                                                    NULL);
  }
}

/* Here init all passes and shading groups
 * Assume that all Passes are NULL */
static void SCULPT_cache_init(void *vedata)
{
  SCULPT_PassList *psl = ((SCULPT_Data *)vedata)->psl;
  SCULPT_StorageList *stl = ((SCULPT_Data *)vedata)->stl;

  if (!stl->g_data) {
    stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
  }

  {
    const DRWContextState *draw_ctx = DRW_context_state_get();
    View3D *v3d = draw_ctx->v3d;

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_MULTIPLY;
    psl->pass = DRW_pass_create("Sculpt Pass", state);

    DRWShadingGroup *shgrp = DRW_shgroup_create(e_data.shader_mask, psl->pass);
    DRW_shgroup_uniform_float(shgrp, "maskOpacity", &v3d->overlay.sculpt_mode_mask_opacity, 1);
    stl->g_data->mask_overlay_grp = shgrp;
  }
}

/* Add geometry to shadingGroups. Execute for each objects */
static void SCULPT_cache_populate(void *vedata, Object *ob)
{
  SCULPT_PassList *psl = ((SCULPT_Data *)vedata)->psl;
  SCULPT_StorageList *stl = ((SCULPT_Data *)vedata)->stl;

  UNUSED_VARS(psl, stl);

  if (ob->type == OB_MESH) {
    const DRWContextState *draw_ctx = DRW_context_state_get();

    if (ob->sculpt && (ob == draw_ctx->obact)) {
      PBVH *pbvh = ob->sculpt->pbvh;
      if (pbvh && pbvh_has_mask(pbvh)) {
        DRW_shgroup_call_sculpt_add(stl->g_data->mask_overlay_grp, ob, false, true, false);
      }
    }
  }
}

static void SCULPT_draw_scene(void *vedata)
{
  SCULPT_PassList *psl = ((SCULPT_Data *)vedata)->psl;

  DRW_draw_pass(psl->pass);
}

static void SCULPT_engine_free(void)
{
  DRW_SHADER_FREE_SAFE(e_data.shader_mask);
}

static const DrawEngineDataSize SCULPT_data_size = DRW_VIEWPORT_DATA_SIZE(SCULPT_Data);

DrawEngineType draw_engine_sculpt_type = {
    NULL,
    NULL,
    N_("SculptMode"),
    &SCULPT_data_size,
    &SCULPT_engine_init,
    &SCULPT_engine_free,
    &SCULPT_cache_init,
    &SCULPT_cache_populate,
    NULL,
    NULL, /* draw_background but not needed by mode engines */
    &SCULPT_draw_scene,
    NULL,
    NULL,
};
