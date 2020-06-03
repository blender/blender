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

#include "BKE_paint.h"
#include "BKE_particle.h"

#include "DNA_particle_types.h"

#include "GPU_shader.h"

#include "basic_engine.h"
/* Shaders */

#define BASIC_ENGINE "BLENDER_BASIC"

extern char datatoc_depth_frag_glsl[];
extern char datatoc_depth_vert_glsl[];
extern char datatoc_conservative_depth_geom_glsl[];

extern char datatoc_common_view_lib_glsl[];

/* *********** LISTS *********** */

/* GPUViewport.storage
 * Is freed everytime the viewport engine changes */
typedef struct BASIC_StorageList {
  struct BASIC_PrivateData *g_data;
} BASIC_StorageList;

typedef struct BASIC_PassList {
  struct DRWPass *depth_pass[2];
  struct DRWPass *depth_pass_cull[2];
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
  struct GPUShader *depth_conservative;
} BASIC_Shaders;

/* *********** STATIC *********** */

static struct {
  BASIC_Shaders sh_data[GPU_SHADER_CFG_LEN];
} e_data = {{{NULL}}}; /* Engine data */

typedef struct BASIC_PrivateData {
  DRWShadingGroup *depth_shgrp[2];
  DRWShadingGroup *depth_shgrp_cull[2];
  DRWShadingGroup *depth_hair_shgrp[2];
} BASIC_PrivateData; /* Transient data */

/* Functions */

static void basic_engine_init(void *UNUSED(vedata))
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  BASIC_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  /* Depth prepass */
  if (!sh_data->depth) {
    const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[draw_ctx->sh_cfg];

    sh_data->depth = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_depth_vert_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_depth_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, NULL},
    });

    sh_data->depth_conservative = GPU_shader_create_from_arrays({
        .vert = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_depth_vert_glsl,
                                 NULL},
        .geom = (const char *[]){sh_cfg->lib,
                                 datatoc_common_view_lib_glsl,
                                 datatoc_conservative_depth_geom_glsl,
                                 NULL},
        .frag = (const char *[]){datatoc_depth_frag_glsl, NULL},
        .defs = (const char *[]){sh_cfg->def, "#define CONSERVATIVE_RASTER\n", NULL},
    });
  }
}

static void basic_cache_init(void *vedata)
{
  BASIC_PassList *psl = ((BASIC_Data *)vedata)->psl;
  BASIC_StorageList *stl = ((BASIC_Data *)vedata)->stl;
  DRWShadingGroup *grp;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  BASIC_Shaders *sh_data = &e_data.sh_data[draw_ctx->sh_cfg];

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_callocN(sizeof(*stl->g_data), __func__);
  }

  /* Twice for normal and infront objects. */
  for (int i = 0; i < 2; i++) {
    DRWState clip_state = (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) ? DRW_STATE_CLIP_PLANES : 0;
    DRWState infront_state = (DRW_state_is_select() && (i == 1)) ? DRW_STATE_IN_FRONT_SELECT : 0;
    DRWState state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;

    GPUShader *sh = DRW_state_is_select() ? sh_data->depth_conservative : sh_data->depth;

    DRW_PASS_CREATE(psl->depth_pass[i], state | clip_state | infront_state);
    stl->g_data->depth_shgrp[i] = grp = DRW_shgroup_create(sh, psl->depth_pass[i]);
    DRW_shgroup_uniform_vec2(grp, "sizeViewport", DRW_viewport_size_get(), 1);
    DRW_shgroup_uniform_vec2(grp, "sizeViewportInv", DRW_viewport_invert_size_get(), 1);

    stl->g_data->depth_hair_shgrp[i] = grp = DRW_shgroup_create(sh_data->depth,
                                                                psl->depth_pass[i]);

    state |= DRW_STATE_CULL_BACK;
    DRW_PASS_CREATE(psl->depth_pass_cull[i], state | clip_state | infront_state);
    stl->g_data->depth_shgrp_cull[i] = grp = DRW_shgroup_create(sh, psl->depth_pass_cull[i]);
    DRW_shgroup_uniform_vec2(grp, "sizeViewport", DRW_viewport_size_get(), 1);
    DRW_shgroup_uniform_vec2(grp, "sizeViewportInv", DRW_viewport_invert_size_get(), 1);
  }
}

static void basic_cache_populate(void *vedata, Object *ob)
{
  BASIC_StorageList *stl = ((BASIC_Data *)vedata)->stl;

  /* TODO(fclem) fix selection of smoke domains. */

  if (!DRW_object_is_renderable(ob) || (ob->dt < OB_SOLID)) {
    return;
  }

  bool do_in_front = (ob->dtx & OB_DRAWXRAY) != 0;

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
        DRW_shgroup_call(stl->g_data->depth_hair_shgrp[do_in_front], hairs, NULL);
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
      if (geom) {
        DRW_shgroup_call(stl->g_data->depth_shgrp[do_in_front], geom, ob);
      }
      return;
    }
  }

  const bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->v3d) &&
                               !DRW_state_is_image_render();
  const bool do_cull = (draw_ctx->v3d &&
                        (draw_ctx->v3d->shading.flag & V3D_SHADING_BACKFACE_CULLING));
  DRWShadingGroup *shgrp = (do_cull) ? stl->g_data->depth_shgrp_cull[do_in_front] :
                                       stl->g_data->depth_shgrp[do_in_front];

  if (use_sculpt_pbvh) {
    DRW_shgroup_call_sculpt(shgrp, ob, false, false);
  }
  else {
    struct GPUBatch *geom = DRW_cache_object_surface_get(ob);
    if (geom) {
      DRW_shgroup_call(shgrp, geom, ob);
    }
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

  DRW_draw_pass(psl->depth_pass[0]);
  DRW_draw_pass(psl->depth_pass_cull[0]);
  DRW_draw_pass(psl->depth_pass[1]);
  DRW_draw_pass(psl->depth_pass_cull[1]);
}

static void basic_engine_free(void)
{
  for (int i = 0; i < GPU_SHADER_CFG_LEN; i++) {
    BASIC_Shaders *sh_data = &e_data.sh_data[i];
    DRW_SHADER_FREE_SAFE(sh_data->depth);
    DRW_SHADER_FREE_SAFE(sh_data->depth_conservative);
  }
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
    &basic_draw_scene,
    NULL,
    NULL,
    NULL,
};

#undef BASIC_ENGINE
