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

#include "DRW_render.h"

#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "BKE_pointcache.h"

#include "BLI_string_utils.h"

#include "GPU_shader.h"

#include "draw_common.h"
#include "draw_mode_engines.h"

#include "ED_particle.h"

#include "DEG_depsgraph_query.h"

extern char datatoc_particle_strand_vert_glsl[];
extern char datatoc_particle_strand_frag_glsl[];
extern char datatoc_common_globals_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];

/* *********** LISTS *********** */

typedef struct PARTICLE_PassList {
  struct DRWPass *psys_edit_pass;
} PARTICLE_PassList;

typedef struct PARTICLE_FramebufferList {
  struct GPUFrameBuffer *fb;
} PARTICLE_FramebufferList;

typedef struct PARTICLE_TextureList {
  struct GPUTexture *texture;
} PARTICLE_TextureList;

typedef struct PARTICLE_StorageList {
  struct CustomStruct *block;
  struct PARTICLE_PrivateData *g_data;
} PARTICLE_StorageList;

typedef struct PARTICLE_Data {
  void *engine_type; /* Required */
  PARTICLE_FramebufferList *fbl;
  PARTICLE_TextureList *txl;
  PARTICLE_PassList *psl;
  PARTICLE_StorageList *stl;
} PARTICLE_Data;

/* *********** STATIC *********** */

static struct {
  struct GPUShader *strands_shader;
  struct GPUShader *strands_weight_shader;
  struct GPUShader *points_shader;
} e_data = {NULL}; /* Engine data */

typedef struct PARTICLE_PrivateData {
  DRWShadingGroup *strands_group;
  DRWShadingGroup *inner_points_group;
  DRWShadingGroup *tip_points_group;
} PARTICLE_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

static void particle_engine_init(void *UNUSED(vedata))
{
  if (!e_data.strands_shader) {
    char *lib = BLI_string_joinN(datatoc_common_globals_lib_glsl, datatoc_common_view_lib_glsl);

    e_data.strands_shader = DRW_shader_create_with_lib(
        datatoc_particle_strand_vert_glsl, NULL, datatoc_particle_strand_frag_glsl, lib, "");

    e_data.strands_weight_shader = DRW_shader_create_with_lib(datatoc_particle_strand_vert_glsl,
                                                              NULL,
                                                              datatoc_particle_strand_frag_glsl,
                                                              lib,
                                                              "#define USE_WEIGHT");

    e_data.points_shader = DRW_shader_create_with_lib(datatoc_particle_strand_vert_glsl,
                                                      NULL,
                                                      datatoc_particle_strand_frag_glsl,
                                                      lib,
                                                      "#define USE_POINTS");
    MEM_freeN(lib);
  }
}

static void particle_cache_init(void *vedata)
{
  PARTICLE_PassList *psl = ((PARTICLE_Data *)vedata)->psl;
  PARTICLE_StorageList *stl = ((PARTICLE_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ParticleEditSettings *pset = PE_settings(draw_ctx->scene);
  const bool use_weight = (pset->brushtype == PE_BRUSH_WEIGHT);

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
  }

  /* Create a pass */
  psl->psys_edit_pass = DRW_pass_create("PSys Edit Pass",
                                        (DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                         DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WIRE |
                                         DRW_STATE_POINT));

  GPUShader *strand_shader = (use_weight) ? e_data.strands_weight_shader : e_data.strands_shader;
  stl->g_data->strands_group = DRW_shgroup_create(strand_shader, psl->psys_edit_pass);
  stl->g_data->inner_points_group = DRW_shgroup_create(e_data.points_shader, psl->psys_edit_pass);
  stl->g_data->tip_points_group = DRW_shgroup_create(e_data.points_shader, psl->psys_edit_pass);

  DRW_shgroup_uniform_block(stl->g_data->strands_group, "globalsBlock", G_draw.block_ubo);
  DRW_shgroup_uniform_block(stl->g_data->inner_points_group, "globalsBlock", G_draw.block_ubo);
  DRW_shgroup_uniform_block(stl->g_data->tip_points_group, "globalsBlock", G_draw.block_ubo);
}

static void particle_edit_cache_populate(void *vedata,
                                         Object *object,
                                         ParticleSystem *psys,
                                         PTCacheEdit *edit)
{
  PARTICLE_StorageList *stl = ((PARTICLE_Data *)vedata)->stl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ParticleEditSettings *pset = PE_settings(draw_ctx->scene);
  const bool use_weight = (pset->brushtype == PE_BRUSH_WEIGHT);
  {
    struct GPUBatch *strands = DRW_cache_particles_get_edit_strands(
        object, psys, edit, use_weight);
    DRW_shgroup_call_add(stl->g_data->strands_group, strands, NULL);
  }
  if (pset->selectmode == SCE_SELECT_POINT) {
    struct GPUBatch *points = DRW_cache_particles_get_edit_inner_points(object, psys, edit);
    DRW_shgroup_call_add(stl->g_data->inner_points_group, points, NULL);
  }
  if (ELEM(pset->selectmode, SCE_SELECT_POINT, SCE_SELECT_END)) {
    struct GPUBatch *points = DRW_cache_particles_get_edit_tip_points(object, psys, edit);
    DRW_shgroup_call_add(stl->g_data->tip_points_group, points, NULL);
  }
}

static void particle_cache_populate(void *vedata, Object *object)
{
  if (object->mode != OB_MODE_PARTICLE_EDIT) {
    return;
  }
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene_orig = (Scene *)DEG_get_original_id(&draw_ctx->scene->id);
  /* Usually the edit structure is created by Particle Edit Mode Toggle
   * operator, but sometimes it's invoked after tagging hair as outdated
   * (for example, when toggling edit mode). That makes it impossible to
   * create edit structure for until after next dependency graph evaluation.
   *
   * Ideally, the edit structure will be created here already via some
   * dependency graph callback or so, but currently trying to make it nicer
   * only causes bad level calls and breaks design from the past.
   */
  Object *object_orig = DEG_get_original_object(object);
  PTCacheEdit *edit = PE_create_current(draw_ctx->depsgraph, scene_orig, object_orig);
  if (edit == NULL) {
    /* Happens when trying to edit particles in EMITTER mode without
     * having them cached.
     */
    return;
  }
  /* NOTE: We need to pass evaluated particle system, which we need
   * to find first.
   */
  ParticleSystem *psys = object->particlesystem.first;
  ParticleSystem *psys_orig = object_orig->particlesystem.first;
  while (psys_orig != NULL) {
    if (PE_get_current_from_psys(psys_orig) == edit) {
      break;
    }
    psys = psys->next;
    psys_orig = psys_orig->next;
  }
  if (psys == NULL) {
    printf("Error getting evaluated particle system for edit.\n");
    return;
  }
  particle_edit_cache_populate(vedata, object, psys, edit);
}

/* Optional: Post-cache_populate callback */
static void particle_cache_finish(void *UNUSED(vedata))
{
}

/* Draw time ! Control rendering pipeline from here */
static void particle_draw_scene(void *vedata)
{

  PARTICLE_PassList *psl = ((PARTICLE_Data *)vedata)->psl;

  DRW_draw_pass(psl->psys_edit_pass);
}

static void particle_engine_free(void)
{
  DRW_SHADER_FREE_SAFE(e_data.strands_shader);
  DRW_SHADER_FREE_SAFE(e_data.strands_weight_shader);
  DRW_SHADER_FREE_SAFE(e_data.points_shader);
}

static const DrawEngineDataSize particle_data_size = DRW_VIEWPORT_DATA_SIZE(PARTICLE_Data);

DrawEngineType draw_engine_particle_type = {
    NULL,
    NULL,
    N_("Particle Mode"),
    &particle_data_size,
    &particle_engine_init,
    &particle_engine_free,
    &particle_cache_init,
    &particle_cache_populate,
    &particle_cache_finish,
    NULL, /* draw_background but not needed by mode engines */
    &particle_draw_scene,
    NULL,
    NULL,
};
