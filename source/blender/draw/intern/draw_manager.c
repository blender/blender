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

#include <stdio.h>

#include "BLI_alloca.h"
#include "BLI_listbase.h"
#include "BLI_memblock.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_threads.h"

#include "BLF_api.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_duplilist.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_hair.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pbvh.h"
#include "BKE_pointcache.h"
#include "BKE_pointcloud.h"
#include "BKE_volume.h"

#include "DNA_camera_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_world_types.h"
#include "draw_manager.h"

#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"

#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"
#include "GPU_uniformbuffer.h"
#include "GPU_viewport.h"

#include "IMB_colormanagement.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "UI_resources.h"

#include "WM_api.h"
#include "wm_window.h"

#include "draw_color_management.h"
#include "draw_manager_profiling.h"
#include "draw_manager_text.h"

/* only for callbacks */
#include "draw_cache_impl.h"

#include "engines/basic/basic_engine.h"
#include "engines/eevee/eevee_engine.h"
#include "engines/external/external_engine.h"
#include "engines/gpencil/gpencil_engine.h"
#include "engines/overlay/overlay_engine.h"
#include "engines/select/select_engine.h"
#include "engines/workbench/workbench_engine.h"

#include "GPU_context.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DRW_select_buffer.h"

/** Render State: No persistent data between draw calls. */
DRWManager DST = {NULL};

static ListBase DRW_engines = {NULL, NULL};

static void drw_state_prepare_clean_for_draw(DRWManager *dst)
{
  memset(dst, 0x0, offsetof(DRWManager, gl_context));
}

/* This function is used to reset draw manager to a state
 * where we don't re-use data by accident across different
 * draw calls.
 */
#ifdef DEBUG
static void drw_state_ensure_not_reused(DRWManager *dst)
{
  memset(dst, 0xff, offsetof(DRWManager, gl_context));
}
#endif

/* -------------------------------------------------------------------- */
/** \name Threading
 * \{ */
static void drw_task_graph_init(void)
{
  BLI_assert(DST.task_graph == NULL);
  DST.task_graph = BLI_task_graph_create();
  DST.delayed_extraction = BLI_gset_ptr_new(__func__);
}

static void drw_task_graph_deinit(void)
{
  BLI_task_graph_work_and_wait(DST.task_graph);

  BLI_gset_free(DST.delayed_extraction, (void (*)(void *key))drw_batch_cache_generate_requested);
  DST.delayed_extraction = NULL;
  BLI_task_graph_work_and_wait(DST.task_graph);

  BLI_task_graph_free(DST.task_graph);
  DST.task_graph = NULL;
}
/* \} */

/* -------------------------------------------------------------------- */
/** \name Settings
 * \{ */

bool DRW_object_is_renderable(const Object *ob)
{
  BLI_assert((ob->base_flag & BASE_VISIBLE_DEPSGRAPH) != 0);

  if (ob->type == OB_MESH) {
    if ((ob == DST.draw_ctx.object_edit) || DRW_object_is_in_edit_mode(ob)) {
      View3D *v3d = DST.draw_ctx.v3d;
      if (v3d && v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_OCCLUDE_WIRE) {
        return false;
      }
    }
  }

  return true;
}

/* Does `ob` needs to be rendered in edit mode.
 *
 * When using duplicate linked meshes, objects that are not in edit-mode will be drawn as
 * it is in edit mode, when another object with the same mesh is in edit mode.
 * This will not be the case when one of the objects are influenced by modifiers. */
bool DRW_object_is_in_edit_mode(const Object *ob)
{
  if (BKE_object_is_in_editmode(ob)) {
    if (ob->type == OB_MESH) {
      if ((ob->mode & OB_MODE_EDIT) == 0) {
        Mesh *me = (Mesh *)ob->data;
        BMEditMesh *embm = me->edit_mesh;
        /* Sanity check when rendering in multiple windows. */
        if (embm && embm->mesh_eval_final == NULL) {
          return false;
        }
        /* Do not draw ob with edit overlay when edit data is present and is modified. */
        if (embm && embm->mesh_eval_cage && (embm->mesh_eval_cage != embm->mesh_eval_final)) {
          return false;
        }
        /* Check if the object that we are drawing is modified. */
        if (!DEG_is_original_id(&me->id)) {
          return false;
        }
        return true;
      }
    }
    return true;
  }
  return false;
}

/**
 * Return whether this object is visible depending if
 * we are rendering or drawing in the viewport.
 */
int DRW_object_visibility_in_active_context(const Object *ob)
{
  const eEvaluationMode mode = DRW_state_is_scene_render() ? DAG_EVAL_RENDER : DAG_EVAL_VIEWPORT;
  return BKE_object_visibility(ob, mode);
}

bool DRW_object_is_flat_normal(const Object *ob)
{
  if (ob->type == OB_MESH) {
    const Mesh *me = ob->data;
    if (me->mpoly && me->mpoly[0].flag & ME_SMOOTH) {
      return false;
    }
  }
  return true;
}

bool DRW_object_use_hide_faces(const struct Object *ob)
{
  if (ob->type == OB_MESH) {
    const Mesh *me = ob->data;

    switch (ob->mode) {
      case OB_MODE_SCULPT:
        return true;
      case OB_MODE_TEXTURE_PAINT:
        return (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
      case OB_MODE_VERTEX_PAINT:
      case OB_MODE_WEIGHT_PAINT:
        return (me->editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) != 0;
    }
  }

  return false;
}

bool DRW_object_is_visible_psys_in_active_context(const Object *object, const ParticleSystem *psys)
{
  const bool for_render = DRW_state_is_image_render();
  /* NOTE: psys_check_enabled is using object and particle system for only
   * reading, but is using some other functions which are more generic and
   * which are hard to make const-pointer. */
  if (!psys_check_enabled((Object *)object, (ParticleSystem *)psys, for_render)) {
    return false;
  }
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene = draw_ctx->scene;
  if (object == draw_ctx->object_edit) {
    return false;
  }
  const ParticleSettings *part = psys->part;
  const ParticleEditSettings *pset = &scene->toolsettings->particle;
  if (object->mode == OB_MODE_PARTICLE_EDIT) {
    if (psys_in_edit_mode(draw_ctx->depsgraph, psys)) {
      if ((pset->flag & PE_DRAW_PART) == 0) {
        return false;
      }
      if ((part->childtype == 0) &&
          (psys->flag & PSYS_HAIR_DYNAMICS && psys->pointcache->flag & PTCACHE_BAKED) == 0) {
        return false;
      }
    }
  }
  return true;
}

struct Object *DRW_object_get_dupli_parent(const Object *UNUSED(ob))
{
  return DST.dupli_parent;
}

struct DupliObject *DRW_object_get_dupli(const Object *UNUSED(ob))
{
  return DST.dupli_source;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Management
 * \{ */

static void drw_viewport_colormanagement_set(void)
{
  Scene *scene = DST.draw_ctx.scene;
  View3D *v3d = DST.draw_ctx.v3d;

  ColorManagedDisplaySettings *display_settings = &scene->display_settings;
  ColorManagedViewSettings view_settings;
  float dither = 0.0f;

  /* TODO(fclem) This should be a render engine callback to determine if we need CM or not. */
  bool use_workbench = BKE_scene_uses_blender_workbench(scene);

  bool use_scene_lights = (!v3d ||
                           ((v3d->shading.type == OB_MATERIAL) &&
                            (v3d->shading.flag & V3D_SHADING_SCENE_LIGHTS)) ||
                           ((v3d->shading.type == OB_RENDER) &&
                            (v3d->shading.flag & V3D_SHADING_SCENE_LIGHTS_RENDER)));
  bool use_scene_world =
      (!v3d ||
       ((v3d->shading.type == OB_MATERIAL) && (v3d->shading.flag & V3D_SHADING_SCENE_WORLD)) ||
       ((v3d->shading.type == OB_RENDER) && (v3d->shading.flag & V3D_SHADING_SCENE_WORLD_RENDER)));
  bool use_view_transform = v3d && (v3d->shading.type >= OB_MATERIAL);
  bool use_render_settings = v3d && ((use_workbench && use_view_transform) || use_scene_lights ||
                                     use_scene_world);

  if (use_render_settings) {
    /* Use full render settings, for renders with scene lighting. */
    view_settings = scene->view_settings;
    dither = scene->r.dither_intensity;
  }
  else if (use_view_transform) {
    /* Use only view transform + look and nothing else for lookdev without
     * scene lighting, as exposure depends on scene light intensity. */
    BKE_color_managed_view_settings_init_render(&view_settings, display_settings, NULL);
    STRNCPY(view_settings.view_transform, scene->view_settings.view_transform);
    STRNCPY(view_settings.look, scene->view_settings.look);
    dither = scene->r.dither_intensity;
  }
  else {
    /* For workbench use only default view transform in configuration,
     * using no scene settings. */
    BKE_color_managed_view_settings_init_render(&view_settings, display_settings, NULL);
  }

  GPU_viewport_colorspace_set(DST.viewport, &view_settings, display_settings, dither);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Viewport (DRW_viewport)
 * \{ */

void *drw_viewport_engine_data_ensure(void *engine_type)
{
  void *data = GPU_viewport_engine_data_get(DST.viewport, engine_type);

  if (data == NULL) {
    data = GPU_viewport_engine_data_create(DST.viewport, engine_type);
  }
  return data;
}

void DRW_engine_viewport_data_size_get(
    const void *engine_type_v, int *r_fbl_len, int *r_txl_len, int *r_psl_len, int *r_stl_len)
{
  const DrawEngineType *engine_type = engine_type_v;

  if (r_fbl_len) {
    *r_fbl_len = engine_type->vedata_size->fbl_len;
  }
  if (r_txl_len) {
    *r_txl_len = engine_type->vedata_size->txl_len;
  }
  if (r_psl_len) {
    *r_psl_len = engine_type->vedata_size->psl_len;
  }
  if (r_stl_len) {
    *r_stl_len = engine_type->vedata_size->stl_len;
  }
}

/* WARNING: only use for custom pipeline. 99% of the time, you don't want to use this. */
void DRW_render_viewport_size_set(int size[2])
{
  DST.size[0] = size[0];
  DST.size[1] = size[1];
}

const float *DRW_viewport_size_get(void)
{
  return DST.size;
}

const float *DRW_viewport_invert_size_get(void)
{
  return DST.inv_size;
}

const float *DRW_viewport_screenvecs_get(void)
{
  return &DST.screenvecs[0][0];
}

const float *DRW_viewport_pixelsize_get(void)
{
  return &DST.pixsize;
}

static void drw_viewport_cache_resize(void)
{
  /* Release the memiter before clearing the mempools that references them */
  GPU_viewport_cache_release(DST.viewport);

  if (DST.vmempool != NULL) {
    /* Release Image textures. */
    BLI_memblock_iter iter;
    GPUTexture **tex;
    BLI_memblock_iternew(DST.vmempool->images, &iter);
    while ((tex = BLI_memblock_iterstep(&iter))) {
      GPU_texture_free(*tex);
    }

    BLI_memblock_clear(DST.vmempool->commands, NULL);
    BLI_memblock_clear(DST.vmempool->commands_small, NULL);
    BLI_memblock_clear(DST.vmempool->callbuffers, NULL);
    BLI_memblock_clear(DST.vmempool->obmats, NULL);
    BLI_memblock_clear(DST.vmempool->obinfos, NULL);
    BLI_memblock_clear(DST.vmempool->cullstates, NULL);
    BLI_memblock_clear(DST.vmempool->shgroups, NULL);
    BLI_memblock_clear(DST.vmempool->uniforms, NULL);
    BLI_memblock_clear(DST.vmempool->passes, NULL);
    BLI_memblock_clear(DST.vmempool->views, NULL);
    BLI_memblock_clear(DST.vmempool->images, NULL);
  }

  DRW_instance_data_list_free_unused(DST.idatalist);
  DRW_instance_data_list_resize(DST.idatalist);
}

/* Not a viewport variable, we could split this out. */
static void drw_context_state_init(void)
{
  if (DST.draw_ctx.obact) {
    DST.draw_ctx.object_mode = DST.draw_ctx.obact->mode;
  }
  else {
    DST.draw_ctx.object_mode = OB_MODE_OBJECT;
  }

  /* Edit object. */
  if (DST.draw_ctx.object_mode & OB_MODE_EDIT) {
    DST.draw_ctx.object_edit = DST.draw_ctx.obact;
  }
  else {
    DST.draw_ctx.object_edit = NULL;
  }

  /* Pose object. */
  if (DST.draw_ctx.object_mode & OB_MODE_POSE) {
    DST.draw_ctx.object_pose = DST.draw_ctx.obact;
  }
  else if (DST.draw_ctx.object_mode & OB_MODE_WEIGHT_PAINT) {
    DST.draw_ctx.object_pose = BKE_object_pose_armature_get(DST.draw_ctx.obact);
  }
  else {
    DST.draw_ctx.object_pose = NULL;
  }

  DST.draw_ctx.sh_cfg = GPU_SHADER_CFG_DEFAULT;
  if (RV3D_CLIPPING_ENABLED(DST.draw_ctx.v3d, DST.draw_ctx.rv3d)) {
    DST.draw_ctx.sh_cfg = GPU_SHADER_CFG_CLIPPED;
  }
}

static void draw_unit_state_create(void)
{
  DRWObjectInfos *infos = BLI_memblock_alloc(DST.vmempool->obinfos);
  DRWObjectMatrix *mats = BLI_memblock_alloc(DST.vmempool->obmats);
  DRWCullingState *culling = BLI_memblock_alloc(DST.vmempool->cullstates);

  unit_m4(mats->model);
  unit_m4(mats->modelinverse);

  copy_v3_fl(infos->orcotexfac[0], 0.0f);
  copy_v3_fl(infos->orcotexfac[1], 1.0f);

  infos->ob_index = 0;
  infos->ob_random = 0.0f;
  infos->ob_flag = 1.0f;
  copy_v3_fl(infos->ob_color, 1.0f);

  /* TODO(fclem) get rid of this. */
  culling->bsphere.radius = -1.0f;
  culling->user_data = NULL;

  DRW_handle_increment(&DST.resource_handle);
}

/* It also stores viewport variable to an immutable place: DST
 * This is because a cache uniform only store reference
 * to its value. And we don't want to invalidate the cache
 * if this value change per viewport */
static void drw_viewport_var_init(void)
{
  RegionView3D *rv3d = DST.draw_ctx.rv3d;
  /* Refresh DST.size */
  if (DST.viewport) {
    int size[2];
    GPU_viewport_size_get(DST.viewport, size);
    DST.size[0] = size[0];
    DST.size[1] = size[1];
    DST.inv_size[0] = 1.0f / size[0];
    DST.inv_size[1] = 1.0f / size[1];

    DefaultFramebufferList *fbl = (DefaultFramebufferList *)GPU_viewport_framebuffer_list_get(
        DST.viewport);
    DST.default_framebuffer = fbl->default_fb;

    DST.vmempool = GPU_viewport_mempool_get(DST.viewport);

    if (DST.vmempool->commands == NULL) {
      DST.vmempool->commands = BLI_memblock_create(sizeof(DRWCommandChunk));
    }
    if (DST.vmempool->commands_small == NULL) {
      DST.vmempool->commands_small = BLI_memblock_create(sizeof(DRWCommandSmallChunk));
    }
    if (DST.vmempool->callbuffers == NULL) {
      DST.vmempool->callbuffers = BLI_memblock_create(sizeof(DRWCallBuffer));
    }
    if (DST.vmempool->obmats == NULL) {
      uint chunk_len = sizeof(DRWObjectMatrix) * DRW_RESOURCE_CHUNK_LEN;
      DST.vmempool->obmats = BLI_memblock_create_ex(sizeof(DRWObjectMatrix), chunk_len);
    }
    if (DST.vmempool->obinfos == NULL) {
      uint chunk_len = sizeof(DRWObjectInfos) * DRW_RESOURCE_CHUNK_LEN;
      DST.vmempool->obinfos = BLI_memblock_create_ex(sizeof(DRWObjectInfos), chunk_len);
    }
    if (DST.vmempool->cullstates == NULL) {
      uint chunk_len = sizeof(DRWCullingState) * DRW_RESOURCE_CHUNK_LEN;
      DST.vmempool->cullstates = BLI_memblock_create_ex(sizeof(DRWCullingState), chunk_len);
    }
    if (DST.vmempool->shgroups == NULL) {
      DST.vmempool->shgroups = BLI_memblock_create(sizeof(DRWShadingGroup));
    }
    if (DST.vmempool->uniforms == NULL) {
      DST.vmempool->uniforms = BLI_memblock_create(sizeof(DRWUniformChunk));
    }
    if (DST.vmempool->views == NULL) {
      DST.vmempool->views = BLI_memblock_create(sizeof(DRWView));
    }
    if (DST.vmempool->passes == NULL) {
      uint chunk_len = sizeof(DRWPass) * DRW_RESOURCE_CHUNK_LEN;
      DST.vmempool->passes = BLI_memblock_create_ex(sizeof(DRWPass), chunk_len);
    }
    if (DST.vmempool->images == NULL) {
      DST.vmempool->images = BLI_memblock_create(sizeof(GPUTexture *));
    }

    DST.resource_handle = 0;
    DST.pass_handle = 0;

    draw_unit_state_create();

    DST.idatalist = GPU_viewport_instance_data_list_get(DST.viewport);
    DRW_instance_data_list_reset(DST.idatalist);
  }
  else {
    DST.size[0] = 0;
    DST.size[1] = 0;

    DST.inv_size[0] = 0;
    DST.inv_size[1] = 0;

    DST.default_framebuffer = NULL;
    DST.vmempool = NULL;
  }

  DST.primary_view_ct = 0;

  if (rv3d != NULL) {
    normalize_v3_v3(DST.screenvecs[0], rv3d->viewinv[0]);
    normalize_v3_v3(DST.screenvecs[1], rv3d->viewinv[1]);

    DST.pixsize = rv3d->pixsize;
    DST.view_default = DRW_view_create(rv3d->viewmat, rv3d->winmat, NULL, NULL, NULL);
    DRW_view_camtexco_set(DST.view_default, rv3d->viewcamtexcofac);

    if (DST.draw_ctx.sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      int plane_len = (RV3D_LOCK_FLAGS(rv3d) & RV3D_BOXCLIP) ? 4 : 6;
      DRW_view_clip_planes_set(DST.view_default, rv3d->clip, plane_len);
    }

    DST.view_active = DST.view_default;
    DST.view_previous = NULL;
  }
  else {
    zero_v3(DST.screenvecs[0]);
    zero_v3(DST.screenvecs[1]);

    DST.pixsize = 1.0f;
    DST.view_default = NULL;
    DST.view_active = NULL;
    DST.view_previous = NULL;
  }

  /* fclem: Is this still needed ? */
  if (DST.draw_ctx.object_edit) {
    ED_view3d_init_mats_rv3d(DST.draw_ctx.object_edit, rv3d);
  }

  if (G_draw.view_ubo == NULL) {
    G_draw.view_ubo = DRW_uniformbuffer_create(sizeof(DRWViewUboStorage), NULL);
  }

  if (DST.draw_list == NULL) {
    DST.draw_list = GPU_draw_list_create(DRW_DRAWLIST_LEN);
  }

  memset(DST.object_instance_data, 0x0, sizeof(DST.object_instance_data));
}

DefaultFramebufferList *DRW_viewport_framebuffer_list_get(void)
{
  return GPU_viewport_framebuffer_list_get(DST.viewport);
}

DefaultTextureList *DRW_viewport_texture_list_get(void)
{
  return GPU_viewport_texture_list_get(DST.viewport);
}

void DRW_viewport_request_redraw(void)
{
  GPU_viewport_tag_update(DST.viewport);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplis
 * \{ */

static void drw_duplidata_load(DupliObject *dupli)
{
  if (dupli == NULL) {
    return;
  }

  if (DST.dupli_origin != dupli->ob) {
    DST.dupli_origin = dupli->ob;
  }
  else {
    /* Same data as previous iter. No need to poll ghash for this. */
    return;
  }

  if (DST.dupli_ghash == NULL) {
    DST.dupli_ghash = BLI_ghash_ptr_new(__func__);
  }

  void **value;
  if (!BLI_ghash_ensure_p(DST.dupli_ghash, DST.dupli_origin, &value)) {
    *value = MEM_callocN(sizeof(void *) * DST.enabled_engine_count, __func__);

    /* TODO: Meh a bit out of place but this is nice as it is
     * only done once per "original" object. */
    drw_batch_cache_validate(DST.dupli_origin);
  }
  DST.dupli_datas = *(void ***)value;
}

static void duplidata_value_free(void *val)
{
  void **dupli_datas = val;
  for (int i = 0; i < DST.enabled_engine_count; i++) {
    MEM_SAFE_FREE(dupli_datas[i]);
  }
  MEM_freeN(val);
}

static void drw_duplidata_free(void)
{
  if (DST.dupli_ghash != NULL) {
    BLI_ghash_free(DST.dupli_ghash,
                   (void (*)(void *key))drw_batch_cache_generate_requested,
                   duplidata_value_free);
    DST.dupli_ghash = NULL;
  }
}

/* Return NULL if not a dupli or a pointer of pointer to the engine data */
void **DRW_duplidata_get(void *vedata)
{
  if (DST.dupli_source == NULL) {
    return NULL;
  }
  /* XXX Search engine index by using vedata array */
  for (int i = 0; i < DST.enabled_engine_count; i++) {
    if (DST.vedata_array[i] == vedata) {
      return &DST.dupli_datas[i];
    }
  }
  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ViewLayers (DRW_scenelayer)
 * \{ */

void *DRW_view_layer_engine_data_get(DrawEngineType *engine_type)
{
  LISTBASE_FOREACH (ViewLayerEngineData *, sled, &DST.draw_ctx.view_layer->drawdata) {
    if (sled->engine_type == engine_type) {
      return sled->storage;
    }
  }
  return NULL;
}

void **DRW_view_layer_engine_data_ensure_ex(ViewLayer *view_layer,
                                            DrawEngineType *engine_type,
                                            void (*callback)(void *storage))
{
  ViewLayerEngineData *sled;

  for (sled = view_layer->drawdata.first; sled; sled = sled->next) {
    if (sled->engine_type == engine_type) {
      return &sled->storage;
    }
  }

  sled = MEM_callocN(sizeof(ViewLayerEngineData), "ViewLayerEngineData");
  sled->engine_type = engine_type;
  sled->free = callback;
  BLI_addtail(&view_layer->drawdata, sled);

  return &sled->storage;
}

void **DRW_view_layer_engine_data_ensure(DrawEngineType *engine_type,
                                         void (*callback)(void *storage))
{
  return DRW_view_layer_engine_data_ensure_ex(DST.draw_ctx.view_layer, engine_type, callback);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Data (DRW_drawdata)
 * \{ */

/* Used for DRW_drawdata_from_id()
 * All ID-data-blocks which have their own 'local' DrawData
 * should have the same arrangement in their structs.
 */
typedef struct IdDdtTemplate {
  ID id;
  struct AnimData *adt;
  DrawDataList drawdata;
} IdDdtTemplate;

/* Check if ID can have AnimData */
static bool id_type_can_have_drawdata(const short id_type)
{
  /* Only some ID-blocks have this info for now */
  /* TODO: finish adding this for the other blocktypes */
  switch (id_type) {
    /* has DrawData */
    case ID_OB:
    case ID_WO:
      return true;

    /* no DrawData */
    default:
      return false;
  }
}

static bool id_can_have_drawdata(const ID *id)
{
  /* sanity check */
  if (id == NULL) {
    return false;
  }

  return id_type_can_have_drawdata(GS(id->name));
}

/* Get DrawData from the given ID-block. In order for this to work, we assume that
 * the DrawData pointer is stored in the struct in the same fashion as in IdDdtTemplate.
 */
DrawDataList *DRW_drawdatalist_from_id(ID *id)
{
  /* only some ID-blocks have this info for now, so we cast the
   * types that do to be of type IdDdtTemplate, and extract the
   * DrawData that way
   */
  if (id_can_have_drawdata(id)) {
    IdDdtTemplate *idt = (IdDdtTemplate *)id;
    return &idt->drawdata;
  }
  else {
    return NULL;
  }
}

DrawData *DRW_drawdata_get(ID *id, DrawEngineType *engine_type)
{
  DrawDataList *drawdata = DRW_drawdatalist_from_id(id);

  if (drawdata == NULL) {
    return NULL;
  }

  LISTBASE_FOREACH (DrawData *, dd, drawdata) {
    if (dd->engine_type == engine_type) {
      return dd;
    }
  }
  return NULL;
}

DrawData *DRW_drawdata_ensure(ID *id,
                              DrawEngineType *engine_type,
                              size_t size,
                              DrawDataInitCb init_cb,
                              DrawDataFreeCb free_cb)
{
  BLI_assert(size >= sizeof(DrawData));
  BLI_assert(id_can_have_drawdata(id));
  /* Try to re-use existing data. */
  DrawData *dd = DRW_drawdata_get(id, engine_type);
  if (dd != NULL) {
    return dd;
  }

  DrawDataList *drawdata = DRW_drawdatalist_from_id(id);

  /* Allocate new data. */
  if ((GS(id->name) == ID_OB) && (((Object *)id)->base_flag & BASE_FROM_DUPLI) != 0) {
    /* NOTE: data is not persistent in this case. It is reset each redraw. */
    BLI_assert(free_cb == NULL); /* No callback allowed. */
    /* Round to sizeof(float) for DRW_instance_data_request(). */
    const size_t t = sizeof(float) - 1;
    size = (size + t) & ~t;
    size_t fsize = size / sizeof(float);
    BLI_assert(fsize < MAX_INSTANCE_DATA_SIZE);
    if (DST.object_instance_data[fsize] == NULL) {
      DST.object_instance_data[fsize] = DRW_instance_data_request(DST.idatalist, fsize);
    }
    dd = (DrawData *)DRW_instance_data_next(DST.object_instance_data[fsize]);
    memset(dd, 0, size);
  }
  else {
    dd = MEM_callocN(size, "DrawData");
  }
  dd->engine_type = engine_type;
  dd->free = free_cb;
  /* Perform user-side initialization, if needed. */
  if (init_cb != NULL) {
    init_cb(dd);
  }
  /* Register in the list. */
  BLI_addtail((ListBase *)drawdata, dd);
  return dd;
}

void DRW_drawdata_free(ID *id)
{
  DrawDataList *drawdata = DRW_drawdatalist_from_id(id);

  if (drawdata == NULL) {
    return;
  }

  LISTBASE_FOREACH (DrawData *, dd, drawdata) {
    if (dd->free != NULL) {
      dd->free(dd);
    }
  }

  BLI_freelistN((ListBase *)drawdata);
}

/* Unlink (but don't free) the drawdata from the DrawDataList if the ID is an OB from dupli. */
static void drw_drawdata_unlink_dupli(ID *id)
{
  if ((GS(id->name) == ID_OB) && (((Object *)id)->base_flag & BASE_FROM_DUPLI) != 0) {
    DrawDataList *drawdata = DRW_drawdatalist_from_id(id);

    if (drawdata == NULL) {
      return;
    }

    BLI_listbase_clear((ListBase *)drawdata);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Garbage Collection
 * \{ */

void DRW_cache_free_old_batches(Main *bmain)
{
  Scene *scene;
  ViewLayer *view_layer;
  static int lasttime = 0;
  int ctime = (int)PIL_check_seconds_timer();

  if (U.vbotimeout == 0 || (ctime - lasttime) < U.vbocollectrate || ctime == lasttime) {
    return;
  }

  lasttime = ctime;

  for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
    for (view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
      Depsgraph *depsgraph = BKE_scene_get_depsgraph(bmain, scene, view_layer, false);
      if (depsgraph == NULL) {
        continue;
      }

      /* TODO(fclem): This is not optimal since it iter over all dupli instances.
       * In this case only the source object should be tagged. */
      DEG_OBJECT_ITER_FOR_RENDER_ENGINE_BEGIN (depsgraph, ob) {
        DRW_batch_cache_free_old(ob, ctime);
      }
      DEG_OBJECT_ITER_FOR_RENDER_ENGINE_END;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rendering (DRW_engines)
 * \{ */

static void drw_engines_init(void)
{
  LISTBASE_FOREACH (LinkData *, link, &DST.enabled_engines) {
    DrawEngineType *engine = link->data;
    ViewportEngineData *data = drw_viewport_engine_data_ensure(engine);
    PROFILE_START(stime);

    if (engine->engine_init) {
      engine->engine_init(data);
    }

    PROFILE_END_UPDATE(data->init_time, stime);
  }
}

static void drw_engines_cache_init(void)
{
  DST.enabled_engine_count = BLI_listbase_count(&DST.enabled_engines);
  DST.vedata_array = MEM_mallocN(sizeof(void *) * DST.enabled_engine_count, __func__);

  int i = 0;
  for (LinkData *link = DST.enabled_engines.first; link; link = link->next, i++) {
    DrawEngineType *engine = link->data;
    ViewportEngineData *data = drw_viewport_engine_data_ensure(engine);
    DST.vedata_array[i] = data;

    if (data->text_draw_cache) {
      DRW_text_cache_destroy(data->text_draw_cache);
      data->text_draw_cache = NULL;
    }
    if (DST.text_store_p == NULL) {
      DST.text_store_p = &data->text_draw_cache;
    }

    if (engine->cache_init) {
      engine->cache_init(data);
    }
  }
}

static void drw_engines_world_update(Scene *scene)
{
  if (scene->world == NULL) {
    return;
  }

  LISTBASE_FOREACH (LinkData *, link, &DST.enabled_engines) {
    DrawEngineType *engine = link->data;
    ViewportEngineData *data = drw_viewport_engine_data_ensure(engine);

    if (engine->id_update) {
      engine->id_update(data, &scene->world->id);
    }
  }
}

static void drw_engines_cache_populate(Object *ob)
{
  DST.ob_handle = 0;

  /* HACK: DrawData is copied by COW from the duplicated object.
   * This is valid for IDs that cannot be instantiated but this
   * is not what we want in this case so we clear the pointer
   * ourselves here. */
  drw_drawdata_unlink_dupli((ID *)ob);

  /* Validation for dupli objects happen elsewhere. */
  if (!DST.dupli_source) {
    drw_batch_cache_validate(ob);
  }

  int i = 0;
  for (LinkData *link = DST.enabled_engines.first; link; link = link->next, i++) {
    DrawEngineType *engine = link->data;
    ViewportEngineData *data = DST.vedata_array[i];

    if (engine->id_update) {
      engine->id_update(data, &ob->id);
    }

    if (engine->cache_populate) {
      engine->cache_populate(data, ob);
    }
  }

  /* TODO: in the future it would be nice to generate once for all viewports.
   * But we need threaded DRW manager first. */
  if (!DST.dupli_source) {
    drw_batch_cache_generate_requested(ob);
  }

  /* ... and clearing it here too because this draw data is
   * from a mempool and must not be free individually by depsgraph. */
  drw_drawdata_unlink_dupli((ID *)ob);
}

static void drw_engines_cache_finish(void)
{
  int i = 0;
  for (LinkData *link = DST.enabled_engines.first; link; link = link->next, i++) {
    DrawEngineType *engine = link->data;
    ViewportEngineData *data = DST.vedata_array[i];

    if (engine->cache_finish) {
      engine->cache_finish(data);
    }
  }
  MEM_freeN(DST.vedata_array);
}

static void drw_engines_draw_scene(void)
{
  LISTBASE_FOREACH (LinkData *, link, &DST.enabled_engines) {
    DrawEngineType *engine = link->data;
    ViewportEngineData *data = drw_viewport_engine_data_ensure(engine);
    PROFILE_START(stime);

    if (engine->draw_scene) {
      DRW_stats_group_start(engine->idname);
      engine->draw_scene(data);
      /* Restore for next engine */
      if (DRW_state_is_fbo()) {
        GPU_framebuffer_bind(DST.default_framebuffer);
      }
      DRW_stats_group_end();
    }

    PROFILE_END_UPDATE(data->render_time, stime);
  }
  /* Reset state after drawing */
  DRW_state_reset();
}

static void drw_engines_draw_text(void)
{
  LISTBASE_FOREACH (LinkData *, link, &DST.enabled_engines) {
    DrawEngineType *engine = link->data;
    ViewportEngineData *data = drw_viewport_engine_data_ensure(engine);
    PROFILE_START(stime);

    if (data->text_draw_cache) {
      DRW_text_cache_draw(data->text_draw_cache, DST.draw_ctx.region, DST.draw_ctx.v3d);
    }

    PROFILE_END_UPDATE(data->render_time, stime);
  }
}

/* Draw render engine info. */
void DRW_draw_region_engine_info(int xoffset, int *yoffset, int line_height)
{
  LISTBASE_FOREACH (LinkData *, link, &DST.enabled_engines) {
    DrawEngineType *engine = link->data;
    ViewportEngineData *data = drw_viewport_engine_data_ensure(engine);

    if (data->info[0] != '\0') {
      char *chr_current = data->info;
      char *chr_start = chr_current;
      int line_len = 0;

      const int font_id = BLF_default();
      UI_FontThemeColor(font_id, TH_TEXT_HI);

      BLF_enable(font_id, BLF_SHADOW);
      BLF_shadow(font_id, 5, (const float[4]){0.0f, 0.0f, 0.0f, 1.0f});
      BLF_shadow_offset(font_id, 1, -1);

      while (*chr_current++ != '\0') {
        line_len++;
        if (*chr_current == '\n') {
          char info[GPU_INFO_SIZE];
          BLI_strncpy(info, chr_start, line_len + 1);
          *yoffset -= line_height;
          BLF_draw_default(xoffset, *yoffset, 0.0f, info, sizeof(info));

          /* Re-start counting. */
          chr_start = chr_current + 1;
          line_len = -1;
        }
      }

      char info[GPU_INFO_SIZE];
      BLI_strncpy(info, chr_start, line_len + 1);
      *yoffset -= line_height;
      BLF_draw_default(xoffset, *yoffset, 0.0f, info, sizeof(info));

      BLF_disable(font_id, BLF_SHADOW);
    }
  }
}

static void use_drw_engine(DrawEngineType *engine)
{
  LinkData *ld = MEM_callocN(sizeof(LinkData), "enabled engine link data");
  ld->data = engine;
  BLI_addtail(&DST.enabled_engines, ld);
}

/* Gather all draw engines needed and store them in DST.enabled_engines
 * That also define the rendering order of engines */
static void drw_engines_enable_from_engine(const RenderEngineType *engine_type, eDrawType drawtype)
{
  switch (drawtype) {
    case OB_WIRE:
    case OB_SOLID:
      use_drw_engine(DRW_engine_viewport_workbench_type.draw_engine);
      break;
    case OB_MATERIAL:
    case OB_RENDER:
    default:
      if (engine_type->draw_engine != NULL) {
        use_drw_engine(engine_type->draw_engine);
      }
      else if ((engine_type->flag & RE_INTERNAL) == 0) {
        use_drw_engine(DRW_engine_viewport_external_type.draw_engine);
      }
      break;
  }
}

static void drw_engines_enable_overlays(void)
{
  use_drw_engine(&draw_engine_overlay_type);
}
/**
 * Use for select and depth-drawing.
 */
static void drw_engines_enable_basic(void)
{
  use_drw_engine(&draw_engine_basic_type);
}

static void drw_engines_enable(ViewLayer *UNUSED(view_layer),
                               RenderEngineType *engine_type,
                               bool gpencil_engine_needed)
{
  View3D *v3d = DST.draw_ctx.v3d;
  const eDrawType drawtype = v3d->shading.type;
  const bool use_xray = XRAY_ENABLED(v3d);

  drw_engines_enable_from_engine(engine_type, drawtype);
  if (gpencil_engine_needed && ((drawtype >= OB_SOLID) || !use_xray)) {
    use_drw_engine(&draw_engine_gpencil_type);
  }
  drw_engines_enable_overlays();
}

static void drw_engines_disable(void)
{
  BLI_freelistN(&DST.enabled_engines);
}

static void drw_engines_data_validate(void)
{
  int enabled_engines = BLI_listbase_count(&DST.enabled_engines);
  void **engine_handle_array = BLI_array_alloca(engine_handle_array, enabled_engines + 1);
  int i = 0;

  LISTBASE_FOREACH (LinkData *, link, &DST.enabled_engines) {
    DrawEngineType *engine = link->data;
    engine_handle_array[i++] = engine;
  }
  engine_handle_array[i] = NULL;

  GPU_viewport_engines_data_validate(DST.viewport, engine_handle_array);
}

/* Fast check to see if gpencil drawing engine is needed.
 * For slow exact check use `DRW_render_check_grease_pencil` */
static bool drw_gpencil_engine_needed(Depsgraph *depsgraph, View3D *v3d)
{
  const bool exclude_gpencil_rendering = v3d ? (v3d->object_type_exclude_viewport &
                                                (1 << OB_GPENCIL)) != 0 :
                                               false;
  return (!exclude_gpencil_rendering) && DEG_id_type_any_exists(depsgraph, ID_GD);
}

/* -------------------------------------------------------------------- */
/** \name View Update
 * \{ */

void DRW_notify_view_update(const DRWUpdateContext *update_ctx)
{
  RenderEngineType *engine_type = update_ctx->engine_type;
  ARegion *region = update_ctx->region;
  View3D *v3d = update_ctx->v3d;
  RegionView3D *rv3d = region->regiondata;
  Depsgraph *depsgraph = update_ctx->depsgraph;
  Scene *scene = update_ctx->scene;
  ViewLayer *view_layer = update_ctx->view_layer;

  const bool gpencil_engine_needed = drw_gpencil_engine_needed(depsgraph, v3d);

  /* Separate update for each stereo view. */
  for (int view = 0; view < 2; view++) {
    GPUViewport *viewport = WM_draw_region_get_viewport(region);
    if (!viewport) {
      continue;
    }

    /* XXX Really nasty locking. But else this could
     * be executed by the material previews thread
     * while rendering a viewport. */
    BLI_ticket_mutex_lock(DST.gl_context_mutex);

    /* Reset before using it. */
    drw_state_prepare_clean_for_draw(&DST);

    DST.viewport = viewport;
    GPU_viewport_active_view_set(viewport, view);
    DST.draw_ctx = (DRWContextState){
        .region = region,
        .rv3d = rv3d,
        .v3d = v3d,
        .scene = scene,
        .view_layer = view_layer,
        .obact = OBACT(view_layer),
        .engine_type = engine_type,
        .depsgraph = depsgraph,
        .object_mode = OB_MODE_OBJECT,
    };

    drw_engines_enable(view_layer, engine_type, gpencil_engine_needed);
    drw_engines_data_validate();

    LISTBASE_FOREACH (LinkData *, link, &DST.enabled_engines) {
      DrawEngineType *draw_engine = link->data;
      ViewportEngineData *data = drw_viewport_engine_data_ensure(draw_engine);

      if (draw_engine->view_update) {
        draw_engine->view_update(data);
      }
    }

    DST.viewport = NULL;

    drw_engines_disable();

    BLI_ticket_mutex_unlock(DST.gl_context_mutex);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks
 * \{ */

void DRW_draw_callbacks_pre_scene(void)
{
  RegionView3D *rv3d = DST.draw_ctx.rv3d;

  GPU_matrix_projection_set(rv3d->winmat);
  GPU_matrix_set(rv3d->viewmat);

  if (DST.draw_ctx.evil_C) {
    ED_region_draw_cb_draw(DST.draw_ctx.evil_C, DST.draw_ctx.region, REGION_DRAW_PRE_VIEW);
    DRW_state_reset();
  }
}

void DRW_draw_callbacks_post_scene(void)
{
  RegionView3D *rv3d = DST.draw_ctx.rv3d;
  ARegion *region = DST.draw_ctx.region;
  View3D *v3d = DST.draw_ctx.v3d;
  Depsgraph *depsgraph = DST.draw_ctx.depsgraph;

  const bool do_annotations = (v3d && ((v3d->flag2 & V3D_SHOW_ANNOTATION) != 0) &&
                               ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0));

  if (DST.draw_ctx.evil_C) {
    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

    DRW_state_reset();

    GPU_framebuffer_bind(dfbl->overlay_fb);

    GPU_matrix_projection_set(rv3d->winmat);
    GPU_matrix_set(rv3d->viewmat);

    /* annotations - temporary drawing buffer (3d space) */
    /* XXX: Or should we use a proper draw/overlay engine for this case? */
    if (do_annotations) {
      GPU_depth_test(false);
      /* XXX: as scene->gpd is not copied for COW yet */
      ED_annotation_draw_view3d(DEG_get_input_scene(depsgraph), depsgraph, v3d, region, true);
      GPU_depth_test(true);
    }

    drw_debug_draw();

    GPU_depth_test(false);
    ED_region_draw_cb_draw(DST.draw_ctx.evil_C, DST.draw_ctx.region, REGION_DRAW_POST_VIEW);

    /* Callback can be nasty and do whatever they want with the state.
     * Don't trust them! */
    DRW_state_reset();

    /* needed so gizmo isn't obscured */
    if ((v3d->gizmo_flag & V3D_GIZMO_HIDE) == 0) {
      GPU_depth_test(false);
      DRW_draw_gizmo_3d();
    }

    GPU_depth_test(false);
    drw_engines_draw_text();

    DRW_draw_region_info();

    /* annotations - temporary drawing buffer (screenspace) */
    /* XXX: Or should we use a proper draw/overlay engine for this case? */
    if (((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0) && (do_annotations)) {
      GPU_depth_test(false);
      /* XXX: as scene->gpd is not copied for COW yet */
      ED_annotation_draw_view3d(DEG_get_input_scene(depsgraph), depsgraph, v3d, region, false);
    }

    if ((v3d->gizmo_flag & V3D_GIZMO_HIDE) == 0) {
      /* Draw 2D after region info so we can draw on top of the camera passepartout overlay.
       * 'DRW_draw_region_info' sets the projection in pixel-space. */
      GPU_depth_test(false);
      DRW_draw_gizmo_2d();
    }

    if (G.debug_value > 20 && G.debug_value < 30) {
      GPU_depth_test(false);
      /* local coordinate visible rect inside region, to accommodate overlapping ui */
      const rcti *rect = ED_region_visible_rect(DST.draw_ctx.region);
      DRW_stats_draw(rect);
    }

    GPU_depth_test(true);
  }
}

struct DRWTextStore *DRW_text_cache_ensure(void)
{
  BLI_assert(DST.text_store_p);
  if (*DST.text_store_p == NULL) {
    *DST.text_store_p = DRW_text_cache_create();
  }
  return *DST.text_store_p;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Draw Loops (DRW_draw)
 * \{ */

/* Everything starts here.
 * This function takes care of calling all cache and rendering functions
 * for each relevant engine / mode engine. */
void DRW_draw_view(const bContext *C)
{
  Depsgraph *depsgraph = CTX_data_expect_evaluated_depsgraph(C);
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->shading.type);
  GPUViewport *viewport = WM_draw_region_get_bound_viewport(region);

  /* Reset before using it. */
  drw_state_prepare_clean_for_draw(&DST);
  DST.options.draw_text = ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0 &&
                           (v3d->overlay.flag & V3D_OVERLAY_HIDE_TEXT) != 0);
  DST.options.draw_background = (scene->r.alphamode == R_ADDSKY) ||
                                (v3d->shading.type != OB_RENDER);
  DST.options.do_color_management = true;
  DRW_draw_render_loop_ex(depsgraph, engine_type, region, v3d, viewport, C);
}

/**
 * Used for both regular and off-screen drawing.
 * Need to reset DST before calling this function
 */
void DRW_draw_render_loop_ex(struct Depsgraph *depsgraph,
                             RenderEngineType *engine_type,
                             ARegion *region,
                             View3D *v3d,
                             GPUViewport *viewport,
                             const bContext *evil_C)
{

  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  RegionView3D *rv3d = region->regiondata;

  DST.draw_ctx.evil_C = evil_C;
  DST.viewport = viewport;
  /* Setup viewport */
  DST.draw_ctx = (DRWContextState){
      .region = region,
      .rv3d = rv3d,
      .v3d = v3d,
      .scene = scene,
      .view_layer = view_layer,
      .obact = OBACT(view_layer),
      .engine_type = engine_type,
      .depsgraph = depsgraph,

      /* reuse if caller sets */
      .evil_C = DST.draw_ctx.evil_C,
  };
  drw_task_graph_init();
  drw_context_state_init();

  drw_viewport_var_init();
  drw_viewport_colormanagement_set();

  const int object_type_exclude_viewport = v3d->object_type_exclude_viewport;
  /* Check if scene needs to perform the populate loop */
  const bool internal_engine = (engine_type->flag & RE_INTERNAL) != 0;
  const bool draw_type_render = v3d->shading.type == OB_RENDER;
  const bool overlays_on = (v3d->flag2 & V3D_HIDE_OVERLAYS) == 0;
  const bool gpencil_engine_needed = drw_gpencil_engine_needed(depsgraph, v3d);
  const bool do_populate_loop = internal_engine || overlays_on || !draw_type_render ||
                                gpencil_engine_needed;

  /* Get list of enabled engines */
  drw_engines_enable(view_layer, engine_type, gpencil_engine_needed);
  drw_engines_data_validate();

  /* Update ubos */
  DRW_globals_update();

  drw_debug_init();
  DRW_hair_init();

  /* No framebuffer allowed before drawing. */
  BLI_assert(GPU_framebuffer_active_get() == NULL);

  /* Init engines */
  drw_engines_init();

  /* Cache filling */
  {
    PROFILE_START(stime);
    drw_engines_cache_init();
    drw_engines_world_update(scene);

    /* Only iterate over objects for internal engines or when overlays are enabled */
    if (do_populate_loop) {
      DST.dupli_origin = NULL;
      DEG_OBJECT_ITER_FOR_RENDER_ENGINE_BEGIN (depsgraph, ob) {
        if ((object_type_exclude_viewport & (1 << ob->type)) != 0) {
          continue;
        }
        if (!BKE_object_is_visible_in_viewport(v3d, ob)) {
          continue;
        }
        DST.dupli_parent = data_.dupli_parent;
        DST.dupli_source = data_.dupli_object_current;
        drw_duplidata_load(DST.dupli_source);
        drw_engines_cache_populate(ob);
      }
      DEG_OBJECT_ITER_FOR_RENDER_ENGINE_END;
    }

    drw_duplidata_free();
    drw_engines_cache_finish();

    drw_task_graph_deinit();
    DRW_render_instance_buffer_finish();

#ifdef USE_PROFILE
    double *cache_time = GPU_viewport_cache_time_get(DST.viewport);
    PROFILE_END_UPDATE(*cache_time, stime);
#endif
  }

  DRW_stats_begin();

  GPU_framebuffer_bind(DST.default_framebuffer);

  /* Start Drawing */
  DRW_state_reset();

  GPU_framebuffer_bind(DST.default_framebuffer);
  GPU_framebuffer_clear_depth_stencil(DST.default_framebuffer, 1.0f, 0xFF);

  DRW_hair_update();

  DRW_draw_callbacks_pre_scene();

  drw_engines_draw_scene();

  /* Fix 3D view being "laggy" on macos and win+nvidia. (See T56996, T61474) */
  GPU_flush();

  DRW_stats_reset();

  DRW_draw_callbacks_post_scene();

  if (WM_draw_region_get_bound_viewport(region)) {
    /* Don't unbind the framebuffer yet in this case and let
     * GPU_viewport_unbind do it, so that we can still do further
     * drawing of action zones on top. */
  }
  else {
    GPU_framebuffer_restore();
  }

  DRW_state_reset();
  drw_engines_disable();

  drw_viewport_cache_resize();

#ifdef DEBUG
  /* Avoid accidental reuse. */
  drw_state_ensure_not_reused(&DST);
#endif
}

void DRW_draw_render_loop(struct Depsgraph *depsgraph,
                          ARegion *region,
                          View3D *v3d,
                          GPUViewport *viewport)
{
  /* Reset before using it. */
  drw_state_prepare_clean_for_draw(&DST);

  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->shading.type);

  DRW_draw_render_loop_ex(depsgraph, engine_type, region, v3d, viewport, NULL);
}

/**
 * \param viewport: can be NULL, in this case we create one.
 */
void DRW_draw_render_loop_offscreen(struct Depsgraph *depsgraph,
                                    RenderEngineType *engine_type,
                                    ARegion *region,
                                    View3D *v3d,
                                    const bool is_image_render,
                                    const bool draw_background,
                                    const bool do_color_management,
                                    GPUOffScreen *ofs,
                                    GPUViewport *viewport)
{
  /* Create temporary viewport if needed. */
  GPUViewport *render_viewport = viewport;
  if (viewport == NULL) {
    render_viewport = GPU_viewport_create();
  }

  GPU_viewport_bind_from_offscreen(render_viewport, ofs);

  /* Reset before using it. */
  drw_state_prepare_clean_for_draw(&DST);
  DST.options.is_image_render = is_image_render;
  DST.options.do_color_management = do_color_management;
  DST.options.draw_background = draw_background;
  DRW_draw_render_loop_ex(depsgraph, engine_type, region, v3d, render_viewport, NULL);

  if (draw_background) {
    /* HACK(fclem): In this case we need to make sure the final alpha is 1.
     * We use the blend mode to ensure that. A better way to fix that would
     * be to do that in the colormanagmeent shader. */
    GPU_offscreen_bind(ofs, false);
    GPU_clear_color(0.0f, 0.0f, 0.0f, 1.0f);
    GPU_clear(GPU_COLOR_BIT);
    /* Premult Alpha over black background. */
    GPU_blend_set_func(GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    GPU_blend(true);
  }

  GPU_matrix_identity_set();
  GPU_matrix_identity_projection_set();

  GPU_viewport_unbind_from_offscreen(render_viewport, ofs, do_color_management);

  if (draw_background) {
    /* Reset default. */
    GPU_blend_set_func_separate(
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    GPU_blend(false);
  }

  /* Free temporary viewport. */
  if (viewport == NULL) {
    GPU_viewport_free(render_viewport);
  }
}

/* Helper to check if exit object type to render. */
bool DRW_render_check_grease_pencil(Depsgraph *depsgraph)
{
  if (!drw_gpencil_engine_needed(depsgraph, NULL)) {
    return false;
  }

  DEG_OBJECT_ITER_FOR_RENDER_ENGINE_BEGIN (depsgraph, ob) {
    if (ob->type == OB_GPENCIL) {
      if (DRW_object_visibility_in_active_context(ob) & OB_VISIBLE_SELF) {
        return true;
      }
    }
  }
  DEG_OBJECT_ITER_FOR_RENDER_ENGINE_END;

  return false;
}

static void drw_view_reset(void)
{
  DST.view_default = NULL;
  DST.view_active = NULL;
  DST.view_previous = NULL;
}

static void DRW_render_gpencil_to_image(RenderEngine *engine,
                                        struct RenderLayer *render_layer,
                                        const rcti *rect)
{
  if (draw_engine_gpencil_type.render_to_image) {
    ViewportEngineData *gpdata = drw_viewport_engine_data_ensure(&draw_engine_gpencil_type);
    draw_engine_gpencil_type.render_to_image(gpdata, engine, render_layer, rect);
  }
}

void DRW_render_gpencil(struct RenderEngine *engine, struct Depsgraph *depsgraph)
{
  /* Early out if there are no grease pencil objects, especially important
   * to avoid failing in in background renders without OpenGL context. */
  if (!DRW_render_check_grease_pencil(depsgraph)) {
    return;
  }

  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  RenderEngineType *engine_type = engine->type;
  RenderData *r = &scene->r;
  Render *render = engine->re;
  /* Changing Context */
  if (G.background && DST.gl_context == NULL) {
    WM_init_opengl(G_MAIN);
  }

  void *re_gl_context = RE_gl_context_get(render);
  void *re_gpu_context = NULL;

  /* Changing Context */
  if (re_gl_context != NULL) {
    DRW_opengl_render_context_enable(re_gl_context);
    /* We need to query gpu context after a gl context has been bound. */
    re_gpu_context = RE_gpu_context_get(render);
    DRW_gpu_render_context_enable(re_gpu_context);
  }
  else {
    DRW_opengl_context_enable();
  }

  /* Reset before using it. */
  drw_state_prepare_clean_for_draw(&DST);
  DST.options.is_image_render = true;
  DST.options.is_scene_render = true;
  DST.options.draw_background = scene->r.alphamode == R_ADDSKY;
  DST.buffer_finish_called = true;

  DST.draw_ctx = (DRWContextState){
      .scene = scene,
      .view_layer = view_layer,
      .engine_type = engine_type,
      .depsgraph = depsgraph,
      .object_mode = OB_MODE_OBJECT,
  };
  drw_context_state_init();

  DST.viewport = GPU_viewport_create();
  const int size[2] = {(r->size * r->xsch) / 100, (r->size * r->ysch) / 100};
  GPU_viewport_size_set(DST.viewport, size);

  drw_viewport_var_init();

  /* Main rendering. */
  rctf view_rect;
  rcti render_rect;
  RE_GetViewPlane(render, &view_rect, &render_rect);
  if (BLI_rcti_is_empty(&render_rect)) {
    BLI_rcti_init(&render_rect, 0, size[0], 0, size[1]);
  }

  RenderResult *render_result = RE_engine_get_result(engine);
  RenderLayer *render_layer = RE_GetRenderLayer(render_result, view_layer->name);
  for (RenderView *render_view = render_result->views.first; render_view != NULL;
       render_view = render_view->next) {
    RE_SetActiveRenderView(render, render_view->name);
    drw_view_reset();
    DST.buffer_finish_called = false;
    DRW_render_gpencil_to_image(engine, render_layer, &render_rect);
  }

  /* Force cache to reset. */
  drw_viewport_cache_resize();
  GPU_viewport_free(DST.viewport);
  DRW_state_reset();

  glDisable(GL_DEPTH_TEST);

  /* Restore Drawing area. */
  GPU_framebuffer_restore();

  /* Changing Context */
  if (re_gl_context != NULL) {
    DRW_gpu_render_context_disable(re_gpu_context);
    DRW_opengl_render_context_disable(re_gl_context);
  }
  else {
    DRW_opengl_context_disable();
  }

  DST.buffer_finish_called = false;
}

void DRW_render_to_image(RenderEngine *engine, struct Depsgraph *depsgraph)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  RenderEngineType *engine_type = engine->type;
  DrawEngineType *draw_engine_type = engine_type->draw_engine;
  Render *render = engine->re;

  if (G.background && DST.gl_context == NULL) {
    WM_init_opengl(G_MAIN);
  }

  void *re_gl_context = RE_gl_context_get(render);
  void *re_gpu_context = NULL;

  /* Changing Context */
  if (re_gl_context != NULL) {
    DRW_opengl_render_context_enable(re_gl_context);
    /* We need to query gpu context after a gl context has been bound. */
    re_gpu_context = RE_gpu_context_get(render);
    DRW_gpu_render_context_enable(re_gpu_context);
  }
  else {
    DRW_opengl_context_enable();
  }

  /* IMPORTANT: We don't support immediate mode in render mode!
   * This shall remain in effect until immediate mode supports
   * multiple threads. */

  /* Reset before using it. */
  drw_state_prepare_clean_for_draw(&DST);
  DST.options.is_image_render = true;
  DST.options.is_scene_render = true;
  DST.options.draw_background = scene->r.alphamode == R_ADDSKY;
  DST.draw_ctx = (DRWContextState){
      .scene = scene,
      .view_layer = view_layer,
      .engine_type = engine_type,
      .depsgraph = depsgraph,
      .object_mode = OB_MODE_OBJECT,
  };
  drw_context_state_init();

  DST.viewport = GPU_viewport_create();
  const int size[2] = {engine->resolution_x, engine->resolution_y};
  GPU_viewport_size_set(DST.viewport, size);

  drw_viewport_var_init();

  ViewportEngineData *data = drw_viewport_engine_data_ensure(draw_engine_type);

  /* set default viewport */
  glViewport(0, 0, size[0], size[1]);

  /* Main rendering. */
  rctf view_rect;
  rcti render_rect;
  RE_GetViewPlane(render, &view_rect, &render_rect);
  if (BLI_rcti_is_empty(&render_rect)) {
    BLI_rcti_init(&render_rect, 0, size[0], 0, size[1]);
  }

  /* Set the default Blender draw state */
  GPU_state_init();
  /* Reset state before drawing */
  DRW_state_reset();

  /* Init render result. */
  RenderResult *render_result = RE_engine_begin_result(engine,
                                                       0,
                                                       0,
                                                       (int)size[0],
                                                       (int)size[1],
                                                       view_layer->name,
                                                       /* RR_ALL_VIEWS */ NULL);

  RenderLayer *render_layer = render_result->layers.first;
  for (RenderView *render_view = render_result->views.first; render_view != NULL;
       render_view = render_view->next) {
    RE_SetActiveRenderView(render, render_view->name);
    drw_view_reset();
    engine_type->draw_engine->render_to_image(data, engine, render_layer, &render_rect);
    DST.buffer_finish_called = false;
  }

  RE_engine_end_result(engine, render_result, false, false, false);

  /* Force cache to reset. */
  drw_viewport_cache_resize();

  GPU_viewport_free(DST.viewport);
  GPU_framebuffer_restore();

#ifdef DEBUG
  /* Avoid accidental reuse. */
  drw_state_ensure_not_reused(&DST);
#endif

  /* Reset state after drawing */
  DRW_state_reset();

  /* Changing Context */
  if (re_gl_context != NULL) {
    DRW_gpu_render_context_disable(re_gpu_context);
    DRW_opengl_render_context_disable(re_gl_context);
  }
  else {
    DRW_opengl_context_disable();
  }
}

void DRW_render_object_iter(
    void *vedata,
    RenderEngine *engine,
    struct Depsgraph *depsgraph,
    void (*callback)(void *vedata, Object *ob, RenderEngine *engine, struct Depsgraph *depsgraph))
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  DRW_hair_init();

  drw_task_graph_init();
  const int object_type_exclude_viewport = draw_ctx->v3d ?
                                               draw_ctx->v3d->object_type_exclude_viewport :
                                               0;
  DST.dupli_origin = NULL;
  DEG_OBJECT_ITER_FOR_RENDER_ENGINE_BEGIN (depsgraph, ob) {
    if ((object_type_exclude_viewport & (1 << ob->type)) == 0) {
      DST.dupli_parent = data_.dupli_parent;
      DST.dupli_source = data_.dupli_object_current;
      DST.ob_handle = 0;
      drw_duplidata_load(DST.dupli_source);

      if (!DST.dupli_source) {
        drw_batch_cache_validate(ob);
      }
      callback(vedata, ob, engine, depsgraph);
      if (!DST.dupli_source) {
        drw_batch_cache_generate_requested(ob);
      }
    }
  }
  DEG_OBJECT_ITER_FOR_RENDER_ENGINE_END;

  drw_duplidata_free();
  drw_task_graph_deinit();
}

/* Assume a valid gl context is bound (and that the gl_context_mutex has been acquired).
 * This function only setup DST and execute the given function.
 * Warning: similar to DRW_render_to_image you cannot use default lists (dfbl & dtxl). */
void DRW_custom_pipeline(DrawEngineType *draw_engine_type,
                         struct Depsgraph *depsgraph,
                         void (*callback)(void *vedata, void *user_data),
                         void *user_data)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);

  /* Reset before using it. */
  drw_state_prepare_clean_for_draw(&DST);
  DST.options.is_image_render = true;
  DST.options.is_scene_render = true;
  DST.options.draw_background = false;

  DST.draw_ctx = (DRWContextState){
      .scene = scene,
      .view_layer = view_layer,
      .engine_type = NULL,
      .depsgraph = depsgraph,
      .object_mode = OB_MODE_OBJECT,
  };
  drw_context_state_init();

  DST.viewport = GPU_viewport_create();
  const int size[2] = {1, 1};
  GPU_viewport_size_set(DST.viewport, size);

  drw_viewport_var_init();

  DRW_hair_init();

  ViewportEngineData *data = drw_viewport_engine_data_ensure(draw_engine_type);

  /* Execute the callback */
  callback(data, user_data);
  DST.buffer_finish_called = false;

  GPU_viewport_free(DST.viewport);
  GPU_framebuffer_restore();

  /* The use of custom pipeline in other thread using the same
   * resources as the main thread (viewport) may lead to data
   * races and undefined behavior on certain drivers. Using
   * GPU_finish to sync seems to fix the issue. (see T62997) */
  GPU_finish();

#ifdef DEBUG
  /* Avoid accidental reuse. */
  drw_state_ensure_not_reused(&DST);
#endif
}

static struct DRWSelectBuffer {
  struct GPUFrameBuffer *framebuffer_depth_only;
  struct GPUTexture *texture_depth;
} g_select_buffer = {NULL};

static void draw_select_framebuffer_depth_only_setup(const int size[2])
{
  if (g_select_buffer.framebuffer_depth_only == NULL) {
    g_select_buffer.framebuffer_depth_only = GPU_framebuffer_create();
  }

  if ((g_select_buffer.texture_depth != NULL) &&
      ((GPU_texture_width(g_select_buffer.texture_depth) != size[0]) ||
       (GPU_texture_height(g_select_buffer.texture_depth) != size[1]))) {
    GPU_texture_free(g_select_buffer.texture_depth);
    g_select_buffer.texture_depth = NULL;
  }

  if (g_select_buffer.texture_depth == NULL) {
    g_select_buffer.texture_depth = GPU_texture_create_2d(
        size[0], size[1], GPU_DEPTH_COMPONENT24, NULL, NULL);

    GPU_framebuffer_texture_attach(
        g_select_buffer.framebuffer_depth_only, g_select_buffer.texture_depth, 0, 0);

    GPU_framebuffer_check_valid(g_select_buffer.framebuffer_depth_only, NULL);
  }
}

/* Must run after all instance datas have been added. */
void DRW_render_instance_buffer_finish(void)
{
  BLI_assert(!DST.buffer_finish_called && "DRW_render_instance_buffer_finish called twice!");
  DST.buffer_finish_called = true;
  DRW_instance_buffer_finish(DST.idatalist);
  drw_resource_buffer_finish(DST.vmempool);
}

/**
 * object mode select-loop, see: ED_view3d_draw_select_loop (legacy drawing).
 */
void DRW_draw_select_loop(struct Depsgraph *depsgraph,
                          ARegion *region,
                          View3D *v3d,
                          bool UNUSED(use_obedit_skip),
                          bool draw_surface,
                          bool UNUSED(use_nearest),
                          const rcti *rect,
                          DRW_SelectPassFn select_pass_fn,
                          void *select_pass_user_data,
                          DRW_ObjectFilterFn object_filter_fn,
                          void *object_filter_user_data)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->shading.type);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  Object *obact = OBACT(view_layer);
  Object *obedit = OBEDIT_FROM_OBACT(obact);
#ifndef USE_GPU_SELECT
  UNUSED_VARS(scene, view_layer, v3d, region, rect);
#else
  RegionView3D *rv3d = region->regiondata;

  /* Reset before using it. */
  drw_state_prepare_clean_for_draw(&DST);

  bool use_obedit = false;
  /* obedit_ctx_mode is used for selecting the right draw engines */
  // eContextObjectMode obedit_ctx_mode;
  /* object_mode is used for filtering objects in the depsgraph */
  eObjectMode object_mode;
  int object_type = 0;
  if (obedit != NULL) {
    object_type = obedit->type;
    object_mode = obedit->mode;
    if (obedit->type == OB_MBALL) {
      use_obedit = true;
      // obedit_ctx_mode = CTX_MODE_EDIT_METABALL;
    }
    else if (obedit->type == OB_ARMATURE) {
      use_obedit = true;
      // obedit_ctx_mode = CTX_MODE_EDIT_ARMATURE;
    }
  }
  if (v3d->overlay.flag & V3D_OVERLAY_BONE_SELECT) {
    if (!(v3d->flag2 & V3D_HIDE_OVERLAYS)) {
      /* Note: don't use "BKE_object_pose_armature_get" here, it breaks selection. */
      Object *obpose = OBPOSE_FROM_OBACT(obact);
      if (obpose == NULL) {
        Object *obweight = OBWEIGHTPAINT_FROM_OBACT(obact);
        if (obweight) {
          /* Only use Armature pose selection, when connected armature is in pose mode. */
          Object *ob_armature = BKE_modifiers_is_deformed_by_armature(obweight);
          if (ob_armature && ob_armature->mode == OB_MODE_POSE) {
            obpose = ob_armature;
          }
        }
      }

      if (obpose) {
        use_obedit = true;
        object_type = obpose->type;
        object_mode = obpose->mode;
        // obedit_ctx_mode = CTX_MODE_POSE;
      }
    }
  }

  int viewport_size[2] = {BLI_rcti_size_x(rect), BLI_rcti_size_y(rect)};
  struct GPUViewport *viewport = GPU_viewport_create();
  GPU_viewport_size_set(viewport, viewport_size);

  DST.viewport = viewport;
  DST.options.is_select = true;
  drw_task_graph_init();
  /* Get list of enabled engines */
  if (use_obedit) {
    drw_engines_enable_overlays();
  }
  else if (!draw_surface) {
    /* grease pencil selection */
    if (drw_gpencil_engine_needed(depsgraph, v3d)) {
      use_drw_engine(&draw_engine_gpencil_type);
    }

    drw_engines_enable_overlays();
  }
  else {
    /* Draw surface for occlusion. */
    drw_engines_enable_basic();
    /* grease pencil selection */
    if (drw_gpencil_engine_needed(depsgraph, v3d)) {
      use_drw_engine(&draw_engine_gpencil_type);
    }

    drw_engines_enable_overlays();
  }
  drw_engines_data_validate();

  /* Setup viewport */

  /* Instead of 'DRW_context_state_init(C, &DST.draw_ctx)', assign from args */
  DST.draw_ctx = (DRWContextState){
      .region = region,
      .rv3d = rv3d,
      .v3d = v3d,
      .scene = scene,
      .view_layer = view_layer,
      .obact = obact,
      .engine_type = engine_type,
      .depsgraph = depsgraph,
  };
  drw_context_state_init();
  drw_viewport_var_init();

  /* Update ubos */
  DRW_globals_update();

  /* Init engines */
  drw_engines_init();
  DRW_hair_init();

  {
    drw_engines_cache_init();
    drw_engines_world_update(scene);

    if (use_obedit) {
      FOREACH_OBJECT_IN_MODE_BEGIN (view_layer, v3d, object_type, object_mode, ob_iter) {
        drw_engines_cache_populate(ob_iter);
      }
      FOREACH_OBJECT_IN_MODE_END;
    }
    else {
      /* When selecting pose-bones in pose mode, check for visibility not select-ability
       * as pose-bones have their own selection restriction flag. */
      const bool use_pose_exception = (DST.draw_ctx.object_pose != NULL);

      const int object_type_exclude_select = (v3d->object_type_exclude_viewport |
                                              v3d->object_type_exclude_select);
      bool filter_exclude = false;
      DST.dupli_origin = NULL;
      DEG_OBJECT_ITER_FOR_RENDER_ENGINE_BEGIN (depsgraph, ob) {
        if (!BKE_object_is_visible_in_viewport(v3d, ob)) {
          continue;
        }

        if (use_pose_exception && (ob->mode & OB_MODE_POSE)) {
          if ((ob->base_flag & BASE_VISIBLE_VIEWLAYER) == 0) {
            continue;
          }
        }
        else {
          if ((ob->base_flag & BASE_SELECTABLE) == 0) {
            continue;
          }
        }

        if ((object_type_exclude_select & (1 << ob->type)) == 0) {
          if (object_filter_fn != NULL) {
            if (ob->base_flag & BASE_FROM_DUPLI) {
              /* pass (use previous filter_exclude value) */
            }
            else {
              filter_exclude = (object_filter_fn(ob, object_filter_user_data) == false);
            }
            if (filter_exclude) {
              continue;
            }
          }

          /* This relies on dupli instances being after their instancing object. */
          if ((ob->base_flag & BASE_FROM_DUPLI) == 0) {
            Object *ob_orig = DEG_get_original_object(ob);
            DRW_select_load_id(ob_orig->runtime.select_id);
          }
          DST.dupli_parent = data_.dupli_parent;
          DST.dupli_source = data_.dupli_object_current;
          drw_duplidata_load(DST.dupli_source);
          drw_engines_cache_populate(ob);
        }
      }
      DEG_OBJECT_ITER_FOR_RENDER_ENGINE_END;
    }

    drw_duplidata_free();
    drw_task_graph_deinit();
    drw_engines_cache_finish();

    DRW_render_instance_buffer_finish();
  }

  /* Setup framebuffer */
  draw_select_framebuffer_depth_only_setup(viewport_size);
  GPU_framebuffer_bind(g_select_buffer.framebuffer_depth_only);
  GPU_framebuffer_clear_depth(g_select_buffer.framebuffer_depth_only, 1.0f);

  /* Start Drawing */
  DRW_state_reset();
  DRW_draw_callbacks_pre_scene();

  DRW_hair_update();

  DRW_state_lock(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS | DRW_STATE_DEPTH_LESS_EQUAL |
                 DRW_STATE_DEPTH_EQUAL | DRW_STATE_DEPTH_GREATER | DRW_STATE_DEPTH_ALWAYS);

  /* Only 1-2 passes. */
  while (true) {
    if (!select_pass_fn(DRW_SELECT_PASS_PRE, select_pass_user_data)) {
      break;
    }

    drw_engines_draw_scene();

    if (!select_pass_fn(DRW_SELECT_PASS_POST, select_pass_user_data)) {
      break;
    }
  }

  DRW_state_lock(0);

  DRW_state_reset();
  drw_engines_disable();

#  ifdef DEBUG
  /* Avoid accidental reuse. */
  drw_state_ensure_not_reused(&DST);
#  endif
  GPU_framebuffer_restore();

  /* Cleanup for selection state */
  GPU_viewport_free(viewport);
#endif /* USE_GPU_SELECT */
}

/**
 * object mode select-loop, see: ED_view3d_draw_depth_loop (legacy drawing).
 */
static void drw_draw_depth_loop_imp(struct Depsgraph *depsgraph,
                                    ARegion *region,
                                    View3D *v3d,
                                    GPUViewport *viewport,
                                    const bool use_opengl_context)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->shading.type);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  RegionView3D *rv3d = region->regiondata;

  if (use_opengl_context) {
    DRW_opengl_context_enable();
  }

  DST.viewport = viewport;
  DST.options.is_depth = true;

  /* Instead of 'DRW_context_state_init(C, &DST.draw_ctx)', assign from args */
  DST.draw_ctx = (DRWContextState){
      .region = region,
      .rv3d = rv3d,
      .v3d = v3d,
      .scene = scene,
      .view_layer = view_layer,
      .obact = OBACT(view_layer),
      .engine_type = engine_type,
      .depsgraph = depsgraph,
  };
  drw_task_graph_init();
  drw_engines_data_validate();

  /* Setup framebuffer */
  DefaultFramebufferList *fbl = (DefaultFramebufferList *)GPU_viewport_framebuffer_list_get(
      DST.viewport);
  GPU_framebuffer_bind(fbl->depth_only_fb);
  GPU_framebuffer_clear_depth(fbl->depth_only_fb, 1.0f);

  /* Setup viewport */
  drw_context_state_init();
  drw_viewport_var_init();

  /* Update ubos */
  DRW_globals_update();

  /* Init engines */
  drw_engines_init();
  DRW_hair_init();

  {
    drw_engines_cache_init();
    drw_engines_world_update(DST.draw_ctx.scene);

    const int object_type_exclude_viewport = v3d->object_type_exclude_viewport;
    DST.dupli_origin = NULL;
    DEG_OBJECT_ITER_FOR_RENDER_ENGINE_BEGIN (DST.draw_ctx.depsgraph, ob) {
      if ((object_type_exclude_viewport & (1 << ob->type)) != 0) {
        continue;
      }
      if (!BKE_object_is_visible_in_viewport(v3d, ob)) {
        continue;
      }
      DST.dupli_parent = data_.dupli_parent;
      DST.dupli_source = data_.dupli_object_current;
      drw_duplidata_load(DST.dupli_source);
      drw_engines_cache_populate(ob);
    }
    DEG_OBJECT_ITER_FOR_RENDER_ENGINE_END;

    drw_duplidata_free();
    drw_engines_cache_finish();

    drw_task_graph_deinit();
    DRW_render_instance_buffer_finish();
  }

  /* Start Drawing */
  DRW_state_reset();

  DRW_hair_update();

  drw_engines_draw_scene();

  DRW_state_reset();

  /* TODO: Reading depth for operators should be done here. */

  GPU_framebuffer_restore();

  drw_engines_disable();

  drw_viewport_cache_resize();

#ifdef DEBUG
  /* Avoid accidental reuse. */
  drw_state_ensure_not_reused(&DST);
#endif

  /* Changin context */
  if (use_opengl_context) {
    DRW_opengl_context_disable();
  }
}

/**
 * object mode select-loop, see: ED_view3d_draw_depth_loop (legacy drawing).
 */
void DRW_draw_depth_loop(struct Depsgraph *depsgraph,
                         ARegion *region,
                         View3D *v3d,
                         GPUViewport *viewport,
                         bool use_opengl_context)
{
  /* Reset before using it. */
  drw_state_prepare_clean_for_draw(&DST);

  /* Get list of enabled engines */
  {
    /* Required by `DRW_state_draw_support()` */
    DST.draw_ctx.v3d = v3d;

    drw_engines_enable_basic();
    if (DRW_state_draw_support()) {
      drw_engines_enable_overlays();
    }
  }

  drw_draw_depth_loop_imp(depsgraph, region, v3d, viewport, use_opengl_context);
}

/**
 * Converted from ED_view3d_draw_depth_gpencil (legacy drawing).
 */
void DRW_draw_depth_loop_gpencil(struct Depsgraph *depsgraph,
                                 ARegion *region,
                                 View3D *v3d,
                                 GPUViewport *viewport)
{
  /* Reset before using it. */
  drw_state_prepare_clean_for_draw(&DST);

  use_drw_engine(&draw_engine_gpencil_type);

  drw_draw_depth_loop_imp(depsgraph, region, v3d, viewport, true);
}

void DRW_draw_select_id(Depsgraph *depsgraph, ARegion *region, View3D *v3d, const rcti *rect)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);

  /* Reset before using it. */
  drw_state_prepare_clean_for_draw(&DST);

  /* Instead of 'DRW_context_state_init(C, &DST.draw_ctx)', assign from args */
  DST.draw_ctx = (DRWContextState){
      .region = region,
      .rv3d = region->regiondata,
      .v3d = v3d,
      .scene = scene,
      .view_layer = view_layer,
      .obact = OBACT(view_layer),
      .depsgraph = depsgraph,
  };
  drw_task_graph_init();
  drw_context_state_init();

  /* Setup viewport */
  DST.viewport = WM_draw_region_get_viewport(region);
  drw_viewport_var_init();

  /* Update ubos */
  DRW_globals_update();

  /* Init Select Engine */
  struct SELECTID_Context *sel_ctx = DRW_select_engine_context_get();
  sel_ctx->last_rect = *rect;

  use_drw_engine(&draw_engine_select_type);
  drw_engines_init();
  {
    drw_engines_cache_init();

    Object **obj = &sel_ctx->objects[0];
    for (uint remaining = sel_ctx->objects_len; remaining--; obj++) {
      Object *obj_eval = DEG_get_evaluated_object(depsgraph, *obj);
      drw_engines_cache_populate(obj_eval);
    }

    drw_engines_cache_finish();

    drw_task_graph_deinit();
#if 0 /* This is a workaround to a nasty bug that seems to be a nasty driver bug. (See T69377) */
    DRW_render_instance_buffer_finish();
#else
    DST.buffer_finish_called = true;
    // DRW_instance_buffer_finish(DST.idatalist);
    drw_resource_buffer_finish(DST.vmempool);
#endif
  }

  /* Start Drawing */
  DRW_state_reset();
  drw_engines_draw_scene();
  DRW_state_reset();

  drw_engines_disable();

  drw_viewport_cache_resize();

#ifdef DEBUG
  /* Avoid accidental reuse. */
  drw_state_ensure_not_reused(&DST);
#endif
}

/** See #DRW_shgroup_world_clip_planes_from_rv3d. */
static void draw_world_clip_planes_from_rv3d(GPUBatch *batch, const float world_clip_planes[6][4])
{
  GPU_batch_uniform_4fv_array(batch, "WorldClipPlanes", 6, world_clip_planes[0]);
}

/**
 * Clears the Depth Buffer and draws only the specified object.
 */
void DRW_draw_depth_object(
    Scene *scene, ARegion *region, View3D *v3d, GPUViewport *viewport, Object *object)
{
  RegionView3D *rv3d = region->regiondata;

  DRW_opengl_context_enable();
  GPU_matrix_projection_set(rv3d->winmat);
  GPU_matrix_set(rv3d->viewmat);
  GPU_matrix_mul(object->obmat);

  /* Setup framebuffer */
  DefaultFramebufferList *fbl = GPU_viewport_framebuffer_list_get(viewport);

  GPU_framebuffer_bind(fbl->depth_only_fb);
  GPU_framebuffer_clear_depth(fbl->depth_only_fb, 1.0f);
  GPU_depth_test(true);

  const float(*world_clip_planes)[4] = NULL;
  if (RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
    ED_view3d_clipping_set(rv3d);
    ED_view3d_clipping_local(rv3d, object->obmat);
    world_clip_planes = rv3d->clip_local;
  }

  drw_batch_cache_validate(object);

  switch (object->type) {
    case OB_MESH: {
      GPUBatch *batch;

      Mesh *me = object->data;

      if (object->mode & OB_MODE_EDIT) {
        batch = DRW_mesh_batch_cache_get_edit_triangles(me);
      }
      else {
        batch = DRW_mesh_batch_cache_get_surface(me);
      }
      struct TaskGraph *task_graph = BLI_task_graph_create();
      DRW_mesh_batch_cache_create_requested(task_graph, object, me, scene, false, true);
      BLI_task_graph_work_and_wait(task_graph);
      BLI_task_graph_free(task_graph);

      const eGPUShaderConfig sh_cfg = world_clip_planes ? GPU_SHADER_CFG_CLIPPED :
                                                          GPU_SHADER_CFG_DEFAULT;
      GPU_batch_program_set_builtin_with_config(batch, GPU_SHADER_3D_DEPTH_ONLY, sh_cfg);
      if (world_clip_planes != NULL) {
        draw_world_clip_planes_from_rv3d(batch, world_clip_planes);
      }

      GPU_batch_draw(batch);
    } break;
    case OB_CURVE:
    case OB_SURF:
      break;
  }

  if (RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
    ED_view3d_clipping_disable();
  }

  GPU_matrix_set(rv3d->viewmat);
  GPU_depth_test(false);
  GPU_framebuffer_restore();
  DRW_opengl_context_disable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Manager State (DRW_state)
 * \{ */

void DRW_state_dfdy_factors_get(float dfdyfac[2])
{
  GPU_get_dfdy_factors(dfdyfac);
}

/**
 * When false, drawing doesn't output to a pixel buffer
 * eg: Occlusion queries, or when we have setup a context to draw in already.
 */
bool DRW_state_is_fbo(void)
{
  return ((DST.default_framebuffer != NULL) || DST.options.is_image_render) &&
         !DRW_state_is_depth() && !DRW_state_is_select();
}

/**
 * For when engines need to know if this is drawing for selection or not.
 */
bool DRW_state_is_select(void)
{
  return DST.options.is_select;
}

bool DRW_state_is_depth(void)
{
  return DST.options.is_depth;
}

/**
 * Whether we are rendering for an image
 */
bool DRW_state_is_image_render(void)
{
  return DST.options.is_image_render;
}

/**
 * Whether the view transform should be applied.
 */
bool DRW_state_do_color_management(void)
{
  return DST.options.do_color_management;
}

/**
 * Whether we are rendering only the render engine,
 * or if we should also render the mode engines.
 */
bool DRW_state_is_scene_render(void)
{
  BLI_assert(DST.options.is_scene_render ? DST.options.is_image_render : true);
  return DST.options.is_scene_render;
}

/**
 * Whether we are rendering simple opengl render
 */
bool DRW_state_is_opengl_render(void)
{
  return DST.options.is_image_render && !DST.options.is_scene_render;
}

bool DRW_state_is_playback(void)
{
  if (DST.draw_ctx.evil_C != NULL) {
    struct wmWindowManager *wm = CTX_wm_manager(DST.draw_ctx.evil_C);
    return ED_screen_animation_playing(wm) != NULL;
  }
  return false;
}

/**
 * Is the user navigating the region.
 */
bool DRW_state_is_navigating(void)
{
  const RegionView3D *rv3d = DST.draw_ctx.rv3d;
  return (rv3d) && (rv3d->rflag & (RV3D_NAVIGATING | RV3D_PAINTING));
}

/**
 * Should text draw in this mode?
 */
bool DRW_state_show_text(void)
{
  return (DST.options.is_select) == 0 && (DST.options.is_depth) == 0 &&
         (DST.options.is_scene_render) == 0 && (DST.options.draw_text) == 0;
}

/**
 * Should draw support elements
 * Objects center, selection outline, probe data, ...
 */
bool DRW_state_draw_support(void)
{
  View3D *v3d = DST.draw_ctx.v3d;
  return (DRW_state_is_scene_render() == false) && (v3d != NULL) &&
         ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0);
}

/**
 * Whether we should render the background
 */
bool DRW_state_draw_background(void)
{
  return DST.options.draw_background;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context State (DRW_context_state)
 * \{ */

const DRWContextState *DRW_context_state_get(void)
{
  return &DST.draw_ctx;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Init/Exit (DRW_engines)
 * \{ */

bool DRW_engine_render_support(DrawEngineType *draw_engine_type)
{
  return draw_engine_type->render_to_image;
}

void DRW_engine_register(DrawEngineType *draw_engine_type)
{
  BLI_addtail(&DRW_engines, draw_engine_type);
}

void DRW_engines_register(void)
{
  RE_engines_register(&DRW_engine_viewport_eevee_type);
  RE_engines_register(&DRW_engine_viewport_workbench_type);

  DRW_engine_register(&draw_engine_gpencil_type);

  DRW_engine_register(&draw_engine_overlay_type);
  DRW_engine_register(&draw_engine_select_type);
  DRW_engine_register(&draw_engine_basic_type);

  /* setup callbacks */
  {
    BKE_mball_batch_cache_dirty_tag_cb = DRW_mball_batch_cache_dirty_tag;
    BKE_mball_batch_cache_free_cb = DRW_mball_batch_cache_free;

    BKE_curve_batch_cache_dirty_tag_cb = DRW_curve_batch_cache_dirty_tag;
    BKE_curve_batch_cache_free_cb = DRW_curve_batch_cache_free;

    BKE_mesh_batch_cache_dirty_tag_cb = DRW_mesh_batch_cache_dirty_tag;
    BKE_mesh_batch_cache_free_cb = DRW_mesh_batch_cache_free;

    BKE_lattice_batch_cache_dirty_tag_cb = DRW_lattice_batch_cache_dirty_tag;
    BKE_lattice_batch_cache_free_cb = DRW_lattice_batch_cache_free;

    BKE_particle_batch_cache_dirty_tag_cb = DRW_particle_batch_cache_dirty_tag;
    BKE_particle_batch_cache_free_cb = DRW_particle_batch_cache_free;

    BKE_gpencil_batch_cache_dirty_tag_cb = DRW_gpencil_batch_cache_dirty_tag;
    BKE_gpencil_batch_cache_free_cb = DRW_gpencil_batch_cache_free;

    BKE_hair_batch_cache_dirty_tag_cb = DRW_hair_batch_cache_dirty_tag;
    BKE_hair_batch_cache_free_cb = DRW_hair_batch_cache_free;

    BKE_pointcloud_batch_cache_dirty_tag_cb = DRW_pointcloud_batch_cache_dirty_tag;
    BKE_pointcloud_batch_cache_free_cb = DRW_pointcloud_batch_cache_free;

    BKE_volume_batch_cache_dirty_tag_cb = DRW_volume_batch_cache_dirty_tag;
    BKE_volume_batch_cache_free_cb = DRW_volume_batch_cache_free;
  }
}

void DRW_engines_free(void)
{
  if (DST.gl_context == NULL) {
    /* Nothing has been setup. Nothing to clear.
     * Otherwise, DRW_opengl_context_enable can
     * create a context in background mode. (see T62355) */
    return;
  }

  DRW_opengl_context_enable();

  DRW_TEXTURE_FREE_SAFE(g_select_buffer.texture_depth);
  GPU_FRAMEBUFFER_FREE_SAFE(g_select_buffer.framebuffer_depth_only);

  DRW_hair_free();
  DRW_shape_cache_free();
  DRW_stats_free();
  DRW_globals_free();

  DrawEngineType *next;
  for (DrawEngineType *type = DRW_engines.first; type; type = next) {
    next = type->next;
    BLI_remlink(&R_engines, type);

    if (type->engine_free) {
      type->engine_free();
    }
  }

  DRW_UBO_FREE_SAFE(G_draw.block_ubo);
  DRW_UBO_FREE_SAFE(G_draw.view_ubo);
  DRW_TEXTURE_FREE_SAFE(G_draw.ramp);
  DRW_TEXTURE_FREE_SAFE(G_draw.weight_ramp);

  if (DST.draw_list) {
    GPU_draw_list_discard(DST.draw_list);
  }

  DRW_opengl_context_disable();
}

/** \} */

/** \name Init/Exit (DRW_opengl_ctx)
 * \{ */

void DRW_opengl_context_create(void)
{
  BLI_assert(DST.gl_context == NULL); /* Ensure it's called once */

  DST.gl_context_mutex = BLI_ticket_mutex_alloc();
  if (!G.background) {
    immDeactivate();
  }
  /* This changes the active context. */
  DST.gl_context = WM_opengl_context_create();
  WM_opengl_context_activate(DST.gl_context);
  /* Be sure to create gpu_context too. */
  DST.gpu_context = GPU_context_create(0);
  if (!G.background) {
    immActivate();
  }
  /* Set default Blender OpenGL state */
  GPU_state_init();
  /* So we activate the window's one afterwards. */
  wm_window_reset_drawable();
}

void DRW_opengl_context_destroy(void)
{
  BLI_assert(BLI_thread_is_main());
  if (DST.gl_context != NULL) {
    WM_opengl_context_activate(DST.gl_context);
    GPU_context_active_set(DST.gpu_context);
    GPU_context_discard(DST.gpu_context);
    WM_opengl_context_dispose(DST.gl_context);
    BLI_ticket_mutex_free(DST.gl_context_mutex);
  }
}

void DRW_opengl_context_enable_ex(bool restore)
{
  if (DST.gl_context != NULL) {
    /* IMPORTANT: We dont support immediate mode in render mode!
     * This shall remain in effect until immediate mode supports
     * multiple threads. */
    BLI_ticket_mutex_lock(DST.gl_context_mutex);
    if (BLI_thread_is_main() && restore) {
      if (!G.background) {
        immDeactivate();
      }
    }
    WM_opengl_context_activate(DST.gl_context);
    GPU_context_active_set(DST.gpu_context);
    if (BLI_thread_is_main() && restore) {
      if (!G.background) {
        immActivate();
      }
      BLF_batch_reset();
    }
  }
}

void DRW_opengl_context_disable_ex(bool restore)
{
  if (DST.gl_context != NULL) {
#ifdef __APPLE__
    /* Need to flush before disabling draw context, otherwise it does not
     * always finish drawing and viewport can be empty or partially drawn */
    GPU_flush();
#endif

    if (BLI_thread_is_main() && restore) {
      wm_window_reset_drawable();
    }
    else {
      WM_opengl_context_release(DST.gl_context);
      GPU_context_active_set(NULL);
    }

    BLI_ticket_mutex_unlock(DST.gl_context_mutex);
  }
}

void DRW_opengl_context_enable(void)
{
  if (G.background && DST.gl_context == NULL) {
    WM_init_opengl(G_MAIN);
  }
  DRW_opengl_context_enable_ex(true);
}

void DRW_opengl_context_disable(void)
{
  DRW_opengl_context_disable_ex(true);
}

void DRW_opengl_render_context_enable(void *re_gl_context)
{
  /* If thread is main you should use DRW_opengl_context_enable(). */
  BLI_assert(!BLI_thread_is_main());

  /* TODO get rid of the blocking. Only here because of the static global DST. */
  BLI_ticket_mutex_lock(DST.gl_context_mutex);
  WM_opengl_context_activate(re_gl_context);
}

void DRW_opengl_render_context_disable(void *re_gl_context)
{
  GPU_flush();
  WM_opengl_context_release(re_gl_context);
  /* TODO get rid of the blocking. */
  BLI_ticket_mutex_unlock(DST.gl_context_mutex);
}

/* Needs to be called AFTER DRW_opengl_render_context_enable() */
void DRW_gpu_render_context_enable(void *re_gpu_context)
{
  /* If thread is main you should use DRW_opengl_context_enable(). */
  BLI_assert(!BLI_thread_is_main());

  GPU_context_active_set(re_gpu_context);
  DRW_shape_cache_reset(); /* XXX fix that too. */
}

/* Needs to be called BEFORE DRW_opengl_render_context_disable() */
void DRW_gpu_render_context_disable(void *UNUSED(re_gpu_context))
{
  DRW_shape_cache_reset(); /* XXX fix that too. */
  GPU_context_active_set(NULL);
}

#ifdef WITH_XR_OPENXR

/* XXX
 * There should really be no such getter, but for VR we currently can't easily avoid it. OpenXR
 * needs some low level info for the OpenGL context that will be used for submitting the
 * final framebuffer. VR could in theory create its own context, but that would mean we have to
 * switch to it just to submit the final frame, which has notable performance impact.
 *
 * We could "inject" a context through DRW_opengl_render_context_enable(), but that would have to
 * work from the main thread, which is tricky to get working too. The preferable solution would be
 * using a separate thread for VR drawing where a single context can stay active. */
void *DRW_xr_opengl_context_get(void)
{
  return DST.gl_context;
}

/* XXX See comment on DRW_xr_opengl_context_get(). */
void *DRW_xr_gpu_context_get(void)
{
  return DST.gpu_context;
}

/* XXX See comment on DRW_xr_opengl_context_get(). */
void DRW_xr_drawing_begin(void)
{
  BLI_ticket_mutex_lock(DST.gl_context_mutex);
}

/* XXX See comment on DRW_xr_opengl_context_get(). */
void DRW_xr_drawing_end(void)
{
  BLI_ticket_mutex_unlock(DST.gl_context_mutex);
}

#endif
/** \} */
