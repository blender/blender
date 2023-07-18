/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Simple engine for drawing color and/or depth.
 * When we only need simple flat shaders.
 */

#include "DRW_render.h"

#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"

#include "BLI_alloca.h"

#include "DNA_particle_types.h"

#include "GPU_shader.h"

#include "basic_engine.h"
#include "basic_private.h"

#define BASIC_ENGINE "BLENDER_BASIC"

/* *********** LISTS *********** */

/* GPUViewport.storage
 * Is freed every time the viewport engine changes. */
typedef struct BASIC_StorageList {
  struct BASIC_PrivateData *g_data;
} BASIC_StorageList;

typedef struct BASIC_PassList {
  DRWPass *depth_pass[2];
  DRWPass *depth_pass_pointcloud[2];
  DRWPass *depth_pass_cull[2];
} BASIC_PassList;

typedef struct BASIC_Data {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  BASIC_PassList *psl;
  BASIC_StorageList *stl;
} BASIC_Data;

/* *********** STATIC *********** */

typedef struct BASIC_PrivateData {
  DRWShadingGroup *depth_shgrp[2];
  DRWShadingGroup *depth_shgrp_cull[2];
  DRWShadingGroup *depth_hair_shgrp[2];
  DRWShadingGroup *depth_curves_shgrp[2];
  DRWShadingGroup *depth_pointcloud_shgrp[2];
  bool use_material_slot_selection;
} BASIC_PrivateData; /* Transient data */

static void basic_cache_init(void *vedata)
{
  BASIC_PassList *psl = ((BASIC_Data *)vedata)->psl;
  BASIC_StorageList *stl = ((BASIC_Data *)vedata)->stl;
  DRWShadingGroup *grp;

  const DRWContextState *draw_ctx = DRW_context_state_get();

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_callocN(sizeof(*stl->g_data), __func__);
  }

  stl->g_data->use_material_slot_selection = DRW_state_is_material_select();

  /* Twice for normal and in front objects. */
  for (int i = 0; i < 2; i++) {
    DRWState clip_state = (draw_ctx->sh_cfg == GPU_SHADER_CFG_CLIPPED) ? DRW_STATE_CLIP_PLANES : 0;
    DRWState infront_state = (DRW_state_is_select() && (i == 1)) ? DRW_STATE_IN_FRONT_SELECT : 0;
    DRWState state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;

    GPUShader *sh = DRW_state_is_select() ?
                        BASIC_shaders_depth_conservative_sh_get(draw_ctx->sh_cfg) :
                        BASIC_shaders_depth_sh_get(draw_ctx->sh_cfg);

    DRW_PASS_CREATE(psl->depth_pass[i], state | clip_state | infront_state);
    stl->g_data->depth_shgrp[i] = grp = DRW_shgroup_create(sh, psl->depth_pass[i]);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);

    sh = DRW_state_is_select() ?
             BASIC_shaders_pointcloud_depth_conservative_sh_get(draw_ctx->sh_cfg) :
             BASIC_shaders_pointcloud_depth_sh_get(draw_ctx->sh_cfg);
    DRW_PASS_CREATE(psl->depth_pass_pointcloud[i], state | clip_state | infront_state);
    stl->g_data->depth_pointcloud_shgrp[i] = grp = DRW_shgroup_create(
        sh, psl->depth_pass_pointcloud[i]);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);

    stl->g_data->depth_hair_shgrp[i] = grp = DRW_shgroup_create(
        BASIC_shaders_depth_sh_get(draw_ctx->sh_cfg), psl->depth_pass[i]);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);

    stl->g_data->depth_curves_shgrp[i] = grp = DRW_shgroup_create(
        BASIC_shaders_curves_depth_sh_get(draw_ctx->sh_cfg), psl->depth_pass[i]);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);

    sh = DRW_state_is_select() ? BASIC_shaders_depth_conservative_sh_get(draw_ctx->sh_cfg) :
                                 BASIC_shaders_depth_sh_get(draw_ctx->sh_cfg);
    state |= DRW_STATE_CULL_BACK;
    DRW_PASS_CREATE(psl->depth_pass_cull[i], state | clip_state | infront_state);
    stl->g_data->depth_shgrp_cull[i] = grp = DRW_shgroup_create(sh, psl->depth_pass_cull[i]);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  }
}

/* TODO(fclem): DRW_cache_object_surface_material_get needs a refactor to allow passing NULL
 * instead of gpumat_array. Avoiding all this boilerplate code. */
static struct GPUBatch **basic_object_surface_material_get(Object *ob)
{
  const int materials_len = DRW_cache_object_material_count_get(ob);
  GPUMaterial **gpumat_array = BLI_array_alloca(gpumat_array, materials_len);
  memset(gpumat_array, 0, sizeof(*gpumat_array) * materials_len);

  return DRW_cache_object_surface_material_get(ob, gpumat_array, materials_len);
}

static void basic_cache_populate_particles(void *vedata, Object *ob)
{
  const bool do_in_front = (ob->dtx & OB_DRAW_IN_FRONT) != 0;
  BASIC_StorageList *stl = ((BASIC_Data *)vedata)->stl;
  for (ParticleSystem *psys = ob->particlesystem.first; psys != NULL; psys = psys->next) {
    if (!DRW_object_is_visible_psys_in_active_context(ob, psys)) {
      continue;
    }
    ParticleSettings *part = psys->part;
    const int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;
    if (draw_as == PART_DRAW_PATH) {
      struct GPUBatch *hairs = DRW_cache_particles_get_hair(ob, psys, NULL);
      if (stl->g_data->use_material_slot_selection) {
        const short material_slot = part->omat;
        DRW_select_load_id(ob->runtime.select_id | (material_slot << 16));
      }
      DRW_shgroup_call(stl->g_data->depth_hair_shgrp[do_in_front], hairs, NULL);
    }
  }
}

static void basic_cache_populate(void *vedata, Object *ob)
{
  BASIC_StorageList *stl = ((BASIC_Data *)vedata)->stl;

  /* TODO(fclem): fix selection of smoke domains. */

  if (!DRW_object_is_renderable(ob) || (ob->dt < OB_SOLID)) {
    return;
  }

  const DRWContextState *draw_ctx = DRW_context_state_get();
  if (ob != draw_ctx->object_edit) {
    basic_cache_populate_particles(vedata, ob);
  }

  const bool do_in_front = (ob->dtx & OB_DRAW_IN_FRONT) != 0;
  if (ob->type == OB_CURVES) {
    DRW_shgroup_curves_create_sub(ob, stl->g_data->depth_curves_shgrp[do_in_front], NULL);
  }

  if (ob->type == OB_POINTCLOUD) {
    DRW_shgroup_pointcloud_create_sub(ob, stl->g_data->depth_pointcloud_shgrp[do_in_front], NULL);
    return;
  }

  /* Make flat object selectable in ortho view if wireframe is enabled. */
  if ((draw_ctx->v3d->overlay.flag & V3D_OVERLAY_WIREFRAMES) ||
      (draw_ctx->v3d->shading.type == OB_WIRE) || (ob->dtx & OB_DRAWWIRE) || (ob->dt == OB_WIRE))
  {
    int flat_axis = 0;
    bool is_flat_object_viewed_from_side = ((draw_ctx->rv3d->persp == RV3D_ORTHO) &&
                                            DRW_object_is_flat(ob, &flat_axis) &&
                                            DRW_object_axis_orthogonal_to_view(ob, flat_axis));

    if (is_flat_object_viewed_from_side) {
      /* Avoid losing flat objects when in ortho views (see #56549) */
      struct GPUBatch *geom = DRW_cache_object_all_edges_get(ob);
      if (geom) {
        DRW_shgroup_call(stl->g_data->depth_shgrp[do_in_front], geom, ob);
      }
      return;
    }
  }

  const bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->rv3d) &&
                               !DRW_state_is_image_render();
  const bool do_cull = (draw_ctx->v3d &&
                        (draw_ctx->v3d->shading.flag & V3D_SHADING_BACKFACE_CULLING));

  DRWShadingGroup *shgrp = (do_cull) ? stl->g_data->depth_shgrp_cull[do_in_front] :
                                       stl->g_data->depth_shgrp[do_in_front];

  if (use_sculpt_pbvh) {
    DRW_shgroup_call_sculpt(shgrp, ob, false, false, false, false, false);
  }
  else {
    if (stl->g_data->use_material_slot_selection && BKE_object_supports_material_slots(ob)) {
      struct GPUBatch **geoms = basic_object_surface_material_get(ob);
      if (geoms) {
        const int materials_len = DRW_cache_object_material_count_get(ob);
        for (int i = 0; i < materials_len; i++) {
          if (geoms[i] == NULL) {
            continue;
          }
          const short material_slot_select_id = i + 1;
          DRW_select_load_id(ob->runtime.select_id | (material_slot_select_id << 16));
          DRW_shgroup_call(shgrp, geoms[i], ob);
        }
      }
    }
    else {
      struct GPUBatch *geom = DRW_cache_object_surface_get(ob);
      if (geom) {
        DRW_shgroup_call(shgrp, geom, ob);
      }
    }

    if (G.debug_value == 889 && ob->sculpt && BKE_object_sculpt_pbvh_get(ob)) {
      int debug_node_nr = 0;
      DRW_debug_modelmat(ob->object_to_world);
      BKE_pbvh_draw_debug_cb(BKE_object_sculpt_pbvh_get(ob), DRW_sculpt_debug_cb, &debug_node_nr);
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
  DRW_draw_pass(psl->depth_pass_pointcloud[0]);
  DRW_draw_pass(psl->depth_pass_cull[0]);
  DRW_draw_pass(psl->depth_pass[1]);
  DRW_draw_pass(psl->depth_pass_pointcloud[1]);
  DRW_draw_pass(psl->depth_pass_cull[1]);
}

static void basic_engine_free(void)
{
  BASIC_shaders_free();
}

static const DrawEngineDataSize basic_data_size = DRW_VIEWPORT_DATA_SIZE(BASIC_Data);

DrawEngineType draw_engine_basic_type = {
    /*next*/ NULL,
    /*prev*/ NULL,
    /*idname*/ N_("Basic"),
    /*vedata_size*/ &basic_data_size,
    /*engine_init*/ NULL,
    /*engine_free*/ &basic_engine_free,
    /*instance_free*/ /*instance_free*/ NULL,
    /*cache_init*/ &basic_cache_init,
    /*cache_populate*/ &basic_cache_populate,
    /*cache_finish*/ &basic_cache_finish,
    /*draw_scene*/ &basic_draw_scene,
    /*view_update*/ NULL,
    /*id_update*/ NULL,
    /*render_to_image*/ NULL,
    /*store_metadata*/ NULL,
};

#undef BASIC_ENGINE
