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
 * \ingroup draw_engine
 *
 * Simple engine for drawing color and/or depth.
 * When we only need simple flat shaders.
 */

#include "DRW_render.h"

#include "BKE_particle.h"

#include "DNA_particle_types.h"

#include "GPU_shader.h"

#include "basic_engine.h"
/* Shaders */

#define BASIC_ENGINE "BLENDER_BASIC"

/* *********** LISTS *********** */

/* GPUViewport.storage
 * Is freed everytime the viewport engine changes */
typedef struct BASIC_StorageList {
  struct BASIC_PrivateData *g_data;
} BASIC_StorageList;

typedef struct BASIC_PassList {
  struct DRWPass *depth_pass;
  struct DRWPass *depth_pass_cull;
} BASIC_PassList;

typedef struct BASIC_Data {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  BASIC_PassList *psl;
  BASIC_StorageList *stl;
} BASIC_Data;

typedef struct BASIC_Shaders {
  /* Depth Pre Pass */
  struct GPUShader *depth;
} BASIC_Shaders;

/* *********** STATIC *********** */

static struct {
  BASIC_Shaders sh_data[GPU_SHADER_CFG_LEN];
} e_data = {{{NULL}}}; /* Engine data */

typedef struct BASIC_PrivateData {
  DRWShadingGroup *depth_shgrp;
  DRWShadingGroup *depth_shgrp_cull;
  DRWShadingGroup *depth_shgrp_hair;
} BASIC_PrivateData; /* Transient data */

/* Functions */

static void basic_engine_init(void *UNUSED(vedata))
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  BASIC_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  /* Depth prepass */
  if (!sh_data->depth) {
    sh_data->depth = DRW_shader_create_3d_depth_only(draw_ctx->sh_cfg);
  }
}

static void basic_cache_init(void *vedata)
{
  BASIC_PassList *psl = ((BASIC_Data *)vedata)->psl;
  BASIC_StorageList *stl = ((BASIC_Data *)vedata)->stl;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  BASIC_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];
  const RegionView3D *rv3d = draw_ctx->rv3d;

  if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
    DRW_state_clip_planes_set_from_rv3d(draw_ctx->rv3d);
  }

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
  }

  {
    psl->depth_pass = DRW_pass_create(
        "Depth Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WIRE);
    stl->g_data->depth_shgrp = DRW_shgroup_create(sh_data->depth, psl->depth_pass);
    if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->depth_shgrp, rv3d);
    }

    psl->depth_pass_cull = DRW_pass_create("Depth Pass Cull",
                                           DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                                               DRW_STATE_CULL_BACK);
    stl->g_data->depth_shgrp_cull = DRW_shgroup_create(sh_data->depth, psl->depth_pass_cull);
    if (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->depth_shgrp_cull, rv3d);
    }
  }
}

static void basic_cache_populate(void *vedata, Object *ob)
{
  BASIC_StorageList *stl = ((BASIC_Data *)vedata)->stl;

  /* TODO(fclem) fix selection of smoke domains. */

  if (!DRW_object_is_renderable(ob) || (ob->dt < OB_SOLID)) {
    return;
  }

  const DRWContextState *draw_ctx = DRW_context_state_get();
  if (ob != draw_ctx->object_edit) {
    for (ParticleSystem *psys = ob->particlesystem.first; psys != NULL; psys = psys->next) {
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

  /* Make flat object selectable in ortho view if wireframe is enabled. */
  if ((draw_ctx->v3d->overlay.flag & V3D_OVERLAY_WIREFRAMES) ||
      (draw_ctx->v3d->shading.type == OB_WIRE) || (ob->dtx & OB_DRAWWIRE) || (ob->dt == OB_WIRE)) {
    int flat_axis = 0;
    bool is_flat_object_viewed_from_side = ((draw_ctx->rv3d->persp == RV3D_ORTHO) &&
                                            DRW_object_is_flat(ob, &flat_axis) &&
                                            DRW_object_axis_orthogonal_to_view(ob, flat_axis));

    if (is_flat_object_viewed_from_side) {
      /* Avoid losing flat objects when in ortho views (see T56549) */
      struct GPUBatch *geom = DRW_cache_object_all_edges_get(ob);
      DRW_shgroup_call_object(stl->g_data->depth_shgrp, geom, ob);
      return;
    }
  }

  struct GPUBatch *geom = DRW_cache_object_surface_get(ob);
  if (geom) {
    const bool do_cull = (draw_ctx->v3d &&
                          (draw_ctx->v3d->shading.flag & V3D_SHADING_BACKFACE_CULLING));
    /* Depth Prepass */
    DRW_shgroup_call_object(
        (do_cull) ? stl->g_data->depth_shgrp_cull : stl->g_data->depth_shgrp, geom, ob);
  }
}

static void basic_cache_finish(void *vedata)
{
  BASIC_StorageList *stl = ((BASIC_Data *)vedata)->stl;

  UNUSED_VARS(stl);
}

static void basic_draw_scene(void *vedata)
{
  BASIC_PassList *psl = ((BASIC_Data *)vedata)->psl;

  DRW_draw_pass(psl->depth_pass);
  DRW_draw_pass(psl->depth_pass_cull);
}

static void basic_engine_free(void)
{
  /* all shaders are builtin */
}

static const DrawEngineDataSize basic_data_size = DRW_VIEWPORT_DATA_SIZE(BASIC_Data);

DrawEngineType draw_engine_basic_type = {
    NULL,
    NULL,
    N_("Basic"),
    &basic_data_size,
    &basic_engine_init,
    &basic_engine_free,
    &basic_cache_init,
    &basic_cache_populate,
    &basic_cache_finish,
    NULL,
    &basic_draw_scene,
    NULL,
    NULL,
    NULL,
};

/* Note: currently unused, we may want to register so we can see this when debugging the view. */

RenderEngineType DRW_engine_viewport_basic_type = {
    NULL,
    NULL,
    BASIC_ENGINE,
    N_("Basic"),
    RE_INTERNAL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &draw_engine_basic_type,
    {NULL, NULL, NULL},
};

#undef BASIC_ENGINE
