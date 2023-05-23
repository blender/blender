/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 Blender Foundation. */

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
#include "BKE_curves.h"
#include "BKE_duplilist.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_gpencil_legacy.h"
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
#include "BKE_screen.h"
#include "BKE_subdiv_modifier.h"
#include "BKE_viewer_path.h"
#include "BKE_volume.h"

#include "DNA_camera_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "ED_gpencil_legacy.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"

#include "GPU_capabilities.h"
#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_platform.h"
#include "GPU_shader_shared.h"
#include "GPU_state.h"
#include "GPU_uniform_buffer.h"
#include "GPU_viewport.h"

#include "IMB_colormanagement.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "WM_api.h"
#include "wm_window.h"

#include "draw_color_management.h"
#include "draw_manager.h"
#include "draw_manager_profiling.hh"
#include "draw_manager_testing.h"
#include "draw_manager_text.h"
#include "draw_shader.h"
#include "draw_subdivision.h"
#include "draw_texture_pool.h"

/* only for callbacks */
#include "draw_cache_impl.h"

#include "engines/basic/basic_engine.h"
#include "engines/compositor/compositor_engine.h"
#include "engines/eevee/eevee_engine.h"
#include "engines/eevee_next/eevee_engine.h"
#include "engines/external/external_engine.h"
#include "engines/gpencil/gpencil_engine.h"
#include "engines/image/image_engine.h"
#include "engines/overlay/overlay_engine.h"
#include "engines/select/select_engine.h"
#include "engines/workbench/workbench_engine.h"

#include "GPU_context.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DRW_select_buffer.h"

/** Render State: No persistent data between draw calls. */
DRWManager DST = {NULL};

static struct {
  ListBase /*DRWRegisteredDrawEngine*/ engines;
  int len;
} g_registered_engines = {{NULL}};

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

static bool drw_draw_show_annotation(void)
{
  if (DST.draw_ctx.space_data == NULL) {
    View3D *v3d = DST.draw_ctx.v3d;
    return (v3d && ((v3d->flag2 & V3D_SHOW_ANNOTATION) != 0) &&
            ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0));
  }

  switch (DST.draw_ctx.space_data->spacetype) {
    case SPACE_IMAGE: {
      SpaceImage *sima = (SpaceImage *)DST.draw_ctx.space_data;
      return (sima->flag & SI_SHOW_GPENCIL) != 0;
    }
    case SPACE_NODE:
      /* Don't draw the annotation for the node editor. Annotations are handled by space_image as
       * the draw manager is only used to draw the background. */
      return false;
    default:
      BLI_assert(0);
      return false;
  }
}

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

  BLI_gset_free(DST.delayed_extraction,
                (void (*)(void *key))drw_batch_cache_generate_requested_evaluated_mesh_or_curve);
  DST.delayed_extraction = NULL;
  BLI_task_graph_work_and_wait(DST.task_graph);

  BLI_task_graph_free(DST.task_graph);
  DST.task_graph = NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Settings
 * \{ */

bool DRW_object_is_renderable(const Object *ob)
{
  BLI_assert((ob->base_flag & BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT) != 0);

  if (ob->type == OB_MESH) {
    if ((ob == DST.draw_ctx.object_edit) || DRW_object_is_in_edit_mode(ob)) {
      View3D *v3d = DST.draw_ctx.v3d;
      if (v3d && RETOPOLOGY_ENABLED(v3d)) {
        return false;
      }
    }
  }

  return true;
}

bool DRW_object_is_in_edit_mode(const Object *ob)
{
  if (BKE_object_is_in_editmode(ob)) {
    if (ELEM(ob->type, OB_MESH, OB_CURVES)) {
      if ((ob->mode & OB_MODE_EDIT) == 0) {
        return false;
      }
    }
    return true;
  }
  return false;
}

int DRW_object_visibility_in_active_context(const Object *ob)
{
  const eEvaluationMode mode = DRW_state_is_scene_render() ? DAG_EVAL_RENDER : DAG_EVAL_VIEWPORT;
  return BKE_object_visibility(ob, mode);
}

bool DRW_object_use_hide_faces(const struct Object *ob)
{
  if (ob->type == OB_MESH) {
    switch (ob->mode) {
      case OB_MODE_SCULPT:
      case OB_MODE_TEXTURE_PAINT:
      case OB_MODE_VERTEX_PAINT:
      case OB_MODE_WEIGHT_PAINT:
        return true;
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
          (psys->flag & PSYS_HAIR_DYNAMICS && psys->pointcache->flag & PTCACHE_BAKED) == 0)
      {
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
/** \name Viewport (DRW_viewport)
 * \{ */

void DRW_render_viewport_size_set(const int size[2])
{
  DST.size[0] = size[0];
  DST.size[1] = size[1];
  DST.inv_size[0] = 1.0f / size[0];
  DST.inv_size[1] = 1.0f / size[1];
}

const float *DRW_viewport_size_get(void)
{
  return DST.size;
}

const float *DRW_viewport_invert_size_get(void)
{
  return DST.inv_size;
}

const float *DRW_viewport_pixelsize_get(void)
{
  return &DST.pixsize;
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
  else if (DST.draw_ctx.object_mode & OB_MODE_ALL_WEIGHT_PAINT) {
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

  /* TODO(fclem): get rid of this. */
  culling->bsphere.radius = -1.0f;
  culling->user_data = NULL;

  DRW_handle_increment(&DST.resource_handle);
}

DRWData *DRW_viewport_data_create(void)
{
  DRWData *drw_data = MEM_callocN(sizeof(DRWData), "DRWData");

  drw_data->texture_pool = DRW_texture_pool_create();

  drw_data->idatalist = DRW_instance_data_list_create();

  drw_data->commands = BLI_memblock_create(sizeof(DRWCommandChunk));
  drw_data->commands_small = BLI_memblock_create(sizeof(DRWCommandSmallChunk));
  drw_data->callbuffers = BLI_memblock_create(sizeof(DRWCallBuffer));
  drw_data->shgroups = BLI_memblock_create(sizeof(DRWShadingGroup));
  drw_data->uniforms = BLI_memblock_create(sizeof(DRWUniformChunk));
  drw_data->views = BLI_memblock_create(sizeof(DRWView));
  drw_data->images = BLI_memblock_create(sizeof(GPUTexture *));
  drw_data->obattrs_ubo_pool = DRW_uniform_attrs_pool_new();
  drw_data->vlattrs_name_cache = BLI_ghash_new(
      BLI_ghashutil_inthash_p_simple, BLI_ghashutil_intcmp, "View Layer Attribute names");
  {
    uint chunk_len = sizeof(DRWObjectMatrix) * DRW_RESOURCE_CHUNK_LEN;
    drw_data->obmats = BLI_memblock_create_ex(sizeof(DRWObjectMatrix), chunk_len);
  }
  {
    uint chunk_len = sizeof(DRWObjectInfos) * DRW_RESOURCE_CHUNK_LEN;
    drw_data->obinfos = BLI_memblock_create_ex(sizeof(DRWObjectInfos), chunk_len);
  }
  {
    uint chunk_len = sizeof(DRWCullingState) * DRW_RESOURCE_CHUNK_LEN;
    drw_data->cullstates = BLI_memblock_create_ex(sizeof(DRWCullingState), chunk_len);
  }
  {
    uint chunk_len = sizeof(DRWPass) * DRW_RESOURCE_CHUNK_LEN;
    drw_data->passes = BLI_memblock_create_ex(sizeof(DRWPass), chunk_len);
  }

  for (int i = 0; i < 2; i++) {
    drw_data->view_data[i] = DRW_view_data_create(&g_registered_engines.engines);
  }
  return drw_data;
}

/* Reduce ref count of the textures used by a viewport. */
static void draw_texture_release(DRWData *drw_data)
{
  /* Release Image textures. */
  BLI_memblock_iter iter;
  GPUTexture **tex;
  BLI_memblock_iternew(drw_data->images, &iter);
  while ((tex = BLI_memblock_iterstep(&iter))) {
    GPU_texture_free(*tex);
  }
}

static void draw_prune_vlattrs(DRWData *drw_data)
{
  drw_data->vlattrs_ubo_ready = false;

  /* Forget known attributes after they are unused for a few frames. */
  LISTBASE_FOREACH_MUTABLE (GPULayerAttr *, attr, &drw_data->vlattrs_name_list) {
    if (++attr->users > 10) {
      BLI_ghash_remove(
          drw_data->vlattrs_name_cache, POINTER_FROM_UINT(attr->hash_code), NULL, NULL);
      BLI_freelinkN(&drw_data->vlattrs_name_list, attr);
    }
  }
}

static void drw_viewport_data_reset(DRWData *drw_data)
{
  draw_texture_release(drw_data);
  draw_prune_vlattrs(drw_data);

  BLI_memblock_clear(drw_data->commands, NULL);
  BLI_memblock_clear(drw_data->commands_small, NULL);
  BLI_memblock_clear(drw_data->callbuffers, NULL);
  BLI_memblock_clear(drw_data->obmats, NULL);
  BLI_memblock_clear(drw_data->obinfos, NULL);
  BLI_memblock_clear(drw_data->cullstates, NULL);
  BLI_memblock_clear(drw_data->shgroups, NULL);
  BLI_memblock_clear(drw_data->uniforms, NULL);
  BLI_memblock_clear(drw_data->passes, NULL);
  BLI_memblock_clear(drw_data->views, NULL);
  BLI_memblock_clear(drw_data->images, NULL);
  DRW_uniform_attrs_pool_clear_all(drw_data->obattrs_ubo_pool);
  DRW_instance_data_list_free_unused(drw_data->idatalist);
  DRW_instance_data_list_resize(drw_data->idatalist);
  DRW_instance_data_list_reset(drw_data->idatalist);
  DRW_texture_pool_reset(drw_data->texture_pool);
}

void DRW_viewport_data_free(DRWData *drw_data)
{
  draw_texture_release(drw_data);

  BLI_memblock_destroy(drw_data->commands, NULL);
  BLI_memblock_destroy(drw_data->commands_small, NULL);
  BLI_memblock_destroy(drw_data->callbuffers, NULL);
  BLI_memblock_destroy(drw_data->obmats, NULL);
  BLI_memblock_destroy(drw_data->obinfos, NULL);
  BLI_memblock_destroy(drw_data->cullstates, NULL);
  BLI_memblock_destroy(drw_data->shgroups, NULL);
  BLI_memblock_destroy(drw_data->uniforms, NULL);
  BLI_memblock_destroy(drw_data->views, NULL);
  BLI_memblock_destroy(drw_data->passes, NULL);
  BLI_memblock_destroy(drw_data->images, NULL);
  DRW_uniform_attrs_pool_free(drw_data->obattrs_ubo_pool);
  BLI_ghash_free(drw_data->vlattrs_name_cache, NULL, NULL);
  BLI_freelistN(&drw_data->vlattrs_name_list);
  if (drw_data->vlattrs_ubo) {
    GPU_uniformbuf_free(drw_data->vlattrs_ubo);
    MEM_freeN(drw_data->vlattrs_buf);
  }
  DRW_instance_data_list_free(drw_data->idatalist);
  DRW_texture_pool_free(drw_data->texture_pool);
  for (int i = 0; i < 2; i++) {
    DRW_view_data_free(drw_data->view_data[i]);
  }
  if (drw_data->matrices_ubo != NULL) {
    for (int i = 0; i < drw_data->ubo_len; i++) {
      GPU_uniformbuf_free(drw_data->matrices_ubo[i]);
      GPU_uniformbuf_free(drw_data->obinfos_ubo[i]);
    }
    MEM_freeN(drw_data->matrices_ubo);
    MEM_freeN(drw_data->obinfos_ubo);
  }
  DRW_volume_ubos_pool_free(drw_data->volume_grids_ubos);
  DRW_curves_ubos_pool_free(drw_data->curves_ubos);
  MEM_freeN(drw_data);
}

static DRWData *drw_viewport_data_ensure(GPUViewport *viewport)
{
  DRWData **vmempool_p = GPU_viewport_data_get(viewport);
  DRWData *vmempool = *vmempool_p;

  if (vmempool == NULL) {
    *vmempool_p = vmempool = DRW_viewport_data_create();
  }
  return vmempool;
}

/**
 * Sets DST.viewport, DST.size and a lot of other important variables.
 * Needs to be called before enabling any draw engine.
 * - viewport can be NULL. In this case the data will not be stored and will be free at
 *   drw_manager_exit().
 * - size can be NULL to get it from viewport.
 * - if viewport and size are NULL, size is set to (1, 1).
 *
 * IMPORTANT: #drw_manager_init can be called multiple times before #drw_manager_exit.
 */
static void drw_manager_init(DRWManager *dst, GPUViewport *viewport, const int size[2])
{
  RegionView3D *rv3d = dst->draw_ctx.rv3d;
  ARegion *region = dst->draw_ctx.region;

  int view = (viewport) ? GPU_viewport_active_view_get(viewport) : 0;

  if (!dst->viewport && dst->vmempool) {
    /* Manager was init first without a viewport, created DRWData, but is being re-init.
     * In this case, keep the old data. */
    /* If it is being re-init with a valid viewport, it means there is something wrong. */
    BLI_assert(viewport == NULL);
  }
  else if (viewport) {
    /* Use viewport's persistent DRWData. */
    dst->vmempool = drw_viewport_data_ensure(viewport);
  }
  else {
    /* Create temporary DRWData. Freed in drw_manager_exit(). */
    dst->vmempool = DRW_viewport_data_create();
  }

  dst->viewport = viewport;
  dst->view_data_active = dst->vmempool->view_data[view];
  dst->resource_handle = 0;
  dst->pass_handle = 0;
  dst->primary_view_num = 0;

  drw_viewport_data_reset(dst->vmempool);

  bool do_validation = true;
  if (size == NULL && viewport == NULL) {
    /* Avoid division by 0. Engines will either override this or not use it. */
    dst->size[0] = 1.0f;
    dst->size[1] = 1.0f;
  }
  else if (size == NULL) {
    BLI_assert(viewport);
    GPUTexture *tex = GPU_viewport_color_texture(viewport, 0);
    dst->size[0] = GPU_texture_width(tex);
    dst->size[1] = GPU_texture_height(tex);
  }
  else {
    BLI_assert(size);
    dst->size[0] = size[0];
    dst->size[1] = size[1];
    /* Fix case when used in DRW_cache_restart(). */
    do_validation = false;
  }
  dst->inv_size[0] = 1.0f / dst->size[0];
  dst->inv_size[1] = 1.0f / dst->size[1];

  if (do_validation) {
    DRW_view_data_texture_list_size_validate(dst->view_data_active, (int[2]){UNPACK2(dst->size)});
  }

  if (viewport) {
    DRW_view_data_default_lists_from_viewport(dst->view_data_active, viewport);
  }

  DefaultFramebufferList *dfbl = DRW_view_data_default_framebuffer_list_get(dst->view_data_active);
  dst->default_framebuffer = dfbl->default_fb;

  draw_unit_state_create();

  if (rv3d != NULL) {
    dst->pixsize = rv3d->pixsize;
    dst->view_default = DRW_view_create(rv3d->viewmat, rv3d->winmat, NULL, NULL, NULL);

    if (dst->draw_ctx.sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      int plane_len = (RV3D_LOCK_FLAGS(rv3d) & RV3D_BOXCLIP) ? 4 : 6;
      DRW_view_clip_planes_set(dst->view_default, rv3d->clip, plane_len);
    }

    dst->view_active = dst->view_default;
    dst->view_previous = NULL;
  }
  else if (region) {
    View2D *v2d = &region->v2d;
    float viewmat[4][4];
    float winmat[4][4];

    rctf region_space = {0.0f, 1.0f, 0.0f, 1.0f};
    BLI_rctf_transform_calc_m4_pivot_min(&v2d->cur, &region_space, viewmat);

    unit_m4(winmat);
    winmat[0][0] = 2.0f;
    winmat[1][1] = 2.0f;
    winmat[3][0] = -1.0f;
    winmat[3][1] = -1.0f;

    dst->view_default = DRW_view_create(viewmat, winmat, NULL, NULL, NULL);
    dst->view_active = dst->view_default;
    dst->view_previous = NULL;
  }
  else {
    dst->pixsize = 1.0f;
    dst->view_default = NULL;
    dst->view_active = NULL;
    dst->view_previous = NULL;
  }

  /* fclem: Is this still needed ? */
  if (dst->draw_ctx.object_edit && rv3d) {
    ED_view3d_init_mats_rv3d(dst->draw_ctx.object_edit, rv3d);
  }

  if (G_draw.view_ubo == NULL) {
    G_draw.view_ubo = GPU_uniformbuf_create_ex(sizeof(ViewMatrices), NULL, "G_draw.view_ubo");
  }

  if (G_draw.clipping_ubo == NULL) {
    G_draw.clipping_ubo = GPU_uniformbuf_create_ex(
        sizeof(float4) * 6, NULL, "G_draw.clipping_ubo");
  }

  if (dst->draw_list == NULL) {
    dst->draw_list = GPU_draw_list_create(DRW_DRAWLIST_LEN);
  }

  memset(dst->object_instance_data, 0x0, sizeof(dst->object_instance_data));
}

static void drw_manager_exit(DRWManager *dst)
{
  if (dst->vmempool != NULL && dst->viewport == NULL) {
    DRW_viewport_data_free(dst->vmempool);
  }
  dst->vmempool = NULL;
  dst->viewport = NULL;
#ifdef DEBUG
  /* Avoid accidental reuse. */
  drw_state_ensure_not_reused(dst);
#endif
}

DefaultFramebufferList *DRW_viewport_framebuffer_list_get(void)
{
  return DRW_view_data_default_framebuffer_list_get(DST.view_data_active);
}

DefaultTextureList *DRW_viewport_texture_list_get(void)
{
  return DRW_view_data_default_texture_list_get(DST.view_data_active);
}

void DRW_viewport_request_redraw(void)
{
  if (DST.viewport) {
    GPU_viewport_tag_update(DST.viewport);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplis
 * \{ */

static uint dupli_key_hash(const void *key)
{
  const DupliKey *dupli_key = (const DupliKey *)key;
  return BLI_ghashutil_ptrhash(dupli_key->ob) ^ BLI_ghashutil_ptrhash(dupli_key->ob_data);
}

static bool dupli_key_cmp(const void *key1, const void *key2)
{
  const DupliKey *dupli_key1 = (const DupliKey *)key1;
  const DupliKey *dupli_key2 = (const DupliKey *)key2;
  return dupli_key1->ob != dupli_key2->ob || dupli_key1->ob_data != dupli_key2->ob_data;
}

static void drw_duplidata_load(Object *ob)
{
  DupliObject *dupli = DST.dupli_source;
  if (dupli == NULL) {
    return;
  }

  if (DST.dupli_origin != dupli->ob || (DST.dupli_origin_data != dupli->ob_data)) {
    DST.dupli_origin = dupli->ob;
    DST.dupli_origin_data = dupli->ob_data;
  }
  else {
    /* Same data as previous iter. No need to poll ghash for this. */
    return;
  }

  if (DST.dupli_ghash == NULL) {
    DST.dupli_ghash = BLI_ghash_new(dupli_key_hash, dupli_key_cmp, __func__);
  }

  DupliKey *key = MEM_callocN(sizeof(DupliKey), __func__);
  key->ob = dupli->ob;
  key->ob_data = dupli->ob_data;

  void **value;
  if (!BLI_ghash_ensure_p(DST.dupli_ghash, key, &value)) {
    *value = MEM_callocN(sizeof(void *) * g_registered_engines.len, __func__);

    /* TODO: Meh a bit out of place but this is nice as it is
     * only done once per instance type. */
    drw_batch_cache_validate(ob);
  }
  else {
    MEM_freeN(key);
  }
  DST.dupli_datas = *(void ***)value;
}

static void duplidata_value_free(void *val)
{
  void **dupli_datas = val;
  for (int i = 0; i < g_registered_engines.len; i++) {
    MEM_SAFE_FREE(dupli_datas[i]);
  }
  MEM_freeN(val);
}

static void duplidata_key_free(void *key)
{
  DupliKey *dupli_key = (DupliKey *)key;
  if (dupli_key->ob_data == dupli_key->ob->data) {
    drw_batch_cache_generate_requested(dupli_key->ob);
  }
  else {
    Object temp_object = *dupli_key->ob;
    /* Do not modify the original bound-box. */
    temp_object.runtime.bb = NULL;
    BKE_object_replace_data_on_shallow_copy(&temp_object, dupli_key->ob_data);
    drw_batch_cache_generate_requested(&temp_object);
    MEM_SAFE_FREE(temp_object.runtime.bb);
  }
  MEM_freeN(key);
}

static void drw_duplidata_free(void)
{
  if (DST.dupli_ghash != NULL) {
    BLI_ghash_free(DST.dupli_ghash, duplidata_key_free, duplidata_value_free);
    DST.dupli_ghash = NULL;
  }
}

void **DRW_duplidata_get(void *vedata)
{
  if (DST.dupli_source == NULL) {
    return NULL;
  }
  ViewportEngineData *ved = (ViewportEngineData *)vedata;
  DRWRegisteredDrawEngine *engine_type = (DRWRegisteredDrawEngine *)ved->engine_type;
  return &DST.dupli_datas[engine_type->index];
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
    case ID_SCE:
    case ID_TE:
    case ID_MSK:
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

  return NULL;
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
      DST.object_instance_data[fsize] = DRW_instance_data_request(DST.vmempool->idatalist, fsize);
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
      Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer);
      if (depsgraph == NULL) {
        continue;
      }

      /* TODO(fclem): This is not optimal since it iter over all dupli instances.
       * In this case only the source object should be tagged. */
      DEGObjectIterSettings deg_iter_settings = {0};
      deg_iter_settings.depsgraph = depsgraph;
      deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
      DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
        DRW_batch_cache_free_old(ob, ctime);
      }
      DEG_OBJECT_ITER_END;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rendering (DRW_engines)
 * \{ */

static void drw_engines_init(void)
{
  DRW_ENABLED_ENGINE_ITER (DST.view_data_active, engine, data) {
    PROFILE_START(stime);

    const DrawEngineDataSize *data_size = engine->vedata_size;
    memset(data->psl->passes, 0, sizeof(*data->psl->passes) * data_size->psl_len);

    if (engine->engine_init) {
      engine->engine_init(data);
    }

    PROFILE_END_UPDATE(data->init_time, stime);
  }
}

static void drw_engines_cache_init(void)
{
  DRW_manager_begin_sync();

  DRW_ENABLED_ENGINE_ITER (DST.view_data_active, engine, data) {
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

  DRW_ENABLED_ENGINE_ITER (DST.view_data_active, engine, data) {
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

  DRW_ENABLED_ENGINE_ITER (DST.view_data_active, engine, data) {
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
  DRW_ENABLED_ENGINE_ITER (DST.view_data_active, engine, data) {
    if (engine->cache_finish) {
      engine->cache_finish(data);
    }
  }

  DRW_manager_end_sync();
}

static void drw_engines_draw_scene(void)
{
  DRW_ENABLED_ENGINE_ITER (DST.view_data_active, engine, data) {
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
  DRW_ENABLED_ENGINE_ITER (DST.view_data_active, engine, data) {
    PROFILE_START(stime);

    if (data->text_draw_cache) {
      DRW_text_cache_draw(data->text_draw_cache, DST.draw_ctx.region, DST.draw_ctx.v3d);
    }

    PROFILE_END_UPDATE(data->render_time, stime);
  }
}

void DRW_draw_region_engine_info(int xoffset, int *yoffset, int line_height)
{
  DRW_ENABLED_ENGINE_ITER (DST.view_data_active, engine, data) {
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
  DRW_view_data_use_engine(DST.view_data_active, engine);
}

/* Gather all draw engines needed and store them in DST.view_data_active
 * That also define the rendering order of engines */
static void drw_engines_enable_from_engine(const RenderEngineType *engine_type, eDrawType drawtype)
{
  switch (drawtype) {
    case OB_WIRE:
    case OB_SOLID:
      if (U.experimental.enable_workbench_next &&
          STREQ(engine_type->idname, "BLENDER_WORKBENCH_NEXT")) {
        use_drw_engine(DRW_engine_viewport_workbench_next_type.draw_engine);
        break;
      }
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
  use_drw_engine((U.experimental.enable_overlay_next) ? &draw_engine_overlay_next_type :
                                                        &draw_engine_overlay_type);
}
/**
 * Use for select and depth-drawing.
 */
static void drw_engines_enable_basic(void)
{
  use_drw_engine(&draw_engine_basic_type);
}

static void drw_engine_enable_image_editor(void)
{
  if (DRW_engine_external_acquire_for_image_editor()) {
    use_drw_engine(&draw_engine_external_type);
  }
  else {
    use_drw_engine(&draw_engine_image_type);
  }

  use_drw_engine((U.experimental.enable_overlay_next) ? &draw_engine_overlay_next_type :
                                                        &draw_engine_overlay_type);
}

static void drw_engines_enable_editors(void)
{
  SpaceLink *space_data = DST.draw_ctx.space_data;
  if (!space_data) {
    return;
  }

  if (space_data->spacetype == SPACE_IMAGE) {
    drw_engine_enable_image_editor();
  }
  else if (space_data->spacetype == SPACE_NODE) {
    /* Only enable when drawing the space image backdrop. */
    SpaceNode *snode = (SpaceNode *)space_data;
    if ((snode->flag & SNODE_BACKDRAW) != 0) {
      use_drw_engine(&draw_engine_image_type);
      use_drw_engine((U.experimental.enable_overlay_next) ? &draw_engine_overlay_next_type :
                                                            &draw_engine_overlay_type);
    }
  }
}

static bool is_compositor_enabled(void)
{
  if (DST.draw_ctx.v3d->shading.use_compositor == V3D_SHADING_USE_COMPOSITOR_DISABLED) {
    return false;
  }

  if (!(DST.draw_ctx.v3d->shading.type >= OB_MATERIAL)) {
    return false;
  }

  if (!DST.draw_ctx.scene->use_nodes) {
    return false;
  }

  if (!DST.draw_ctx.scene->nodetree) {
    return false;
  }

  if (DST.draw_ctx.v3d->shading.use_compositor == V3D_SHADING_USE_COMPOSITOR_CAMERA &&
      DST.draw_ctx.rv3d->persp != RV3D_CAMOB)
  {
    return false;
  }

  return true;
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

  if (is_compositor_enabled()) {
    use_drw_engine(&draw_engine_compositor_type);
  }

  drw_engines_enable_overlays();

#ifdef WITH_DRAW_DEBUG
  if (G.debug_value == 31) {
    use_drw_engine(&draw_engine_debug_select_type);
  }
#endif
}

static void drw_engines_disable(void)
{
  DRW_view_data_reset(DST.view_data_active);
}

static void drw_engines_data_validate(void)
{
  DRW_view_data_free_unused(DST.view_data_active);
}

/* Fast check to see if gpencil drawing engine is needed.
 * For slow exact check use `DRW_render_check_grease_pencil` */
static bool drw_gpencil_engine_needed(Depsgraph *depsgraph, View3D *v3d)
{
  const bool exclude_gpencil_rendering = v3d ? (v3d->object_type_exclude_viewport &
                                                (1 << OB_GPENCIL_LEGACY)) != 0 :
                                               false;
  return (!exclude_gpencil_rendering) && DEG_id_type_any_exists(depsgraph, ID_GD_LEGACY);
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

  GPUViewport *viewport = WM_draw_region_get_viewport(region);
  if (!viewport) {
    return;
  }

  const bool gpencil_engine_needed = drw_gpencil_engine_needed(depsgraph, v3d);

  /* XXX Really nasty locking. But else this could
   * be executed by the material previews thread
   * while rendering a viewport. */
  BLI_ticket_mutex_lock(DST.gl_context_mutex);

  /* Reset before using it. */
  drw_state_prepare_clean_for_draw(&DST);

  BKE_view_layer_synced_ensure(scene, view_layer);
  DST.draw_ctx = (DRWContextState){
      .region = region,
      .rv3d = rv3d,
      .v3d = v3d,
      .scene = scene,
      .view_layer = view_layer,
      .obact = BKE_view_layer_active_object_get(view_layer),
      .engine_type = engine_type,
      .depsgraph = depsgraph,
      .object_mode = OB_MODE_OBJECT,
  };

  /* Custom lightweight initialize to avoid resetting the memory-pools. */
  DST.viewport = viewport;
  DST.vmempool = drw_viewport_data_ensure(DST.viewport);

  /* Separate update for each stereo view. */
  int view_count = GPU_viewport_is_stereo_get(viewport) ? 2 : 1;
  for (int view = 0; view < view_count; view++) {
    DST.view_data_active = DST.vmempool->view_data[view];

    drw_engines_enable(view_layer, engine_type, gpencil_engine_needed);
    drw_engines_data_validate();

    DRW_view_data_engines_view_update(DST.view_data_active);

    drw_engines_disable();
  }

  drw_manager_exit(&DST);

  BLI_ticket_mutex_unlock(DST.gl_context_mutex);
}

/* update a viewport which belongs to a GPUOffscreen */
static void drw_notify_view_update_offscreen(struct Depsgraph *depsgraph,
                                             RenderEngineType *engine_type,
                                             ARegion *region,
                                             View3D *v3d,
                                             GPUViewport *viewport)
{

  if (viewport && GPU_viewport_do_update(viewport)) {

    Scene *scene = DEG_get_evaluated_scene(depsgraph);
    ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
    RegionView3D *rv3d = region->regiondata;

    const bool gpencil_engine_needed = drw_gpencil_engine_needed(depsgraph, v3d);

    /* Reset before using it. */
    drw_state_prepare_clean_for_draw(&DST);

    BKE_view_layer_synced_ensure(scene, view_layer);
    DST.draw_ctx = (DRWContextState){
        .region = region,
        .rv3d = rv3d,
        .v3d = v3d,
        .scene = scene,
        .view_layer = view_layer,
        .obact = BKE_view_layer_active_object_get(view_layer),
        .engine_type = engine_type,
        .depsgraph = depsgraph,
    };

    /* Custom lightweight initialize to avoid resetting the memory-pools. */
    DST.viewport = viewport;
    DST.vmempool = drw_viewport_data_ensure(DST.viewport);

    /* Separate update for each stereo view. */
    int view_count = GPU_viewport_is_stereo_get(viewport) ? 2 : 1;
    for (int view = 0; view < view_count; view++) {
      DST.view_data_active = DST.vmempool->view_data[view];

      drw_engines_enable(view_layer, engine_type, gpencil_engine_needed);
      drw_engines_data_validate();

      DRW_view_data_engines_view_update(DST.view_data_active);

      drw_engines_disable();
    }

    drw_manager_exit(&DST);
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
    /* Callback can be nasty and do whatever they want with the state.
     * Don't trust them! */
    DRW_state_reset();
  }
}

void DRW_draw_callbacks_post_scene(void)
{
  RegionView3D *rv3d = DST.draw_ctx.rv3d;
  ARegion *region = DST.draw_ctx.region;
  View3D *v3d = DST.draw_ctx.v3d;
  Depsgraph *depsgraph = DST.draw_ctx.depsgraph;

  const bool do_annotations = drw_draw_show_annotation();

  if (DST.draw_ctx.evil_C) {
    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

    DRW_state_reset();

    GPU_framebuffer_bind(dfbl->overlay_fb);

    GPU_matrix_projection_set(rv3d->winmat);
    GPU_matrix_set(rv3d->viewmat);

    /* annotations - temporary drawing buffer (3d space) */
    /* XXX: Or should we use a proper draw/overlay engine for this case? */
    if (do_annotations) {
      GPU_depth_test(GPU_DEPTH_NONE);
      /* XXX: as `scene->gpd` is not copied for COW yet. */
      ED_annotation_draw_view3d(DEG_get_input_scene(depsgraph), depsgraph, v3d, region, true);
      GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
    }

    drw_debug_draw();

    GPU_depth_test(GPU_DEPTH_NONE);
    /* Apply state for callbacks. */
    GPU_apply_state();

    ED_region_draw_cb_draw(DST.draw_ctx.evil_C, DST.draw_ctx.region, REGION_DRAW_POST_VIEW);

#ifdef WITH_XR_OPENXR
    /* XR callbacks (controllers, custom draw functions) for session mirror. */
    if ((v3d->flag & V3D_XR_SESSION_MIRROR) != 0) {
      if ((v3d->flag2 & V3D_XR_SHOW_CONTROLLERS) != 0) {
        ARegionType *art = WM_xr_surface_controller_region_type_get();
        if (art) {
          ED_region_surface_draw_cb_draw(art, REGION_DRAW_POST_VIEW);
        }
      }
      if ((v3d->flag2 & V3D_XR_SHOW_CUSTOM_OVERLAYS) != 0) {
        SpaceType *st = BKE_spacetype_from_id(SPACE_VIEW3D);
        if (st) {
          ARegionType *art = BKE_regiontype_from_id(st, RGN_TYPE_XR);
          if (art) {
            ED_region_surface_draw_cb_draw(art, REGION_DRAW_POST_VIEW);
          }
        }
      }
    }
#endif

    /* Callback can be nasty and do whatever they want with the state.
     * Don't trust them! */
    DRW_state_reset();

    /* Needed so gizmo isn't occluded. */
    if ((v3d->gizmo_flag & V3D_GIZMO_HIDE) == 0) {
      GPU_depth_test(GPU_DEPTH_NONE);
      DRW_draw_gizmo_3d();
    }

    GPU_depth_test(GPU_DEPTH_NONE);
    drw_engines_draw_text();

    DRW_draw_region_info();

    /* Annotations - temporary drawing buffer (screen-space). */
    /* XXX: Or should we use a proper draw/overlay engine for this case? */
    if (((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0) && (do_annotations)) {
      GPU_depth_test(GPU_DEPTH_NONE);
      /* XXX: as scene->gpd is not copied for COW yet */
      ED_annotation_draw_view3d(DEG_get_input_scene(depsgraph), depsgraph, v3d, region, false);
    }

    if ((v3d->gizmo_flag & V3D_GIZMO_HIDE) == 0) {
      /* Draw 2D after region info so we can draw on top of the camera passepartout overlay.
       * 'DRW_draw_region_info' sets the projection in pixel-space. */
      GPU_depth_test(GPU_DEPTH_NONE);
      DRW_draw_gizmo_2d();
    }

    if (G.debug_value > 20 && G.debug_value < 30) {
      GPU_depth_test(GPU_DEPTH_NONE);
      /* local coordinate visible rect inside region, to accommodate overlapping ui */
      const rcti *rect = ED_region_visible_rect(DST.draw_ctx.region);
      DRW_stats_draw(rect);
    }

    GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
  }
  else {
    if (v3d && ((v3d->flag2 & V3D_SHOW_ANNOTATION) != 0)) {
      GPU_depth_test(GPU_DEPTH_NONE);
      /* XXX: as scene->gpd is not copied for COW yet */
      ED_annotation_draw_view3d(DEG_get_input_scene(depsgraph), depsgraph, v3d, region, true);
      GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
    }

#ifdef WITH_XR_OPENXR
    if ((v3d->flag & V3D_XR_SESSION_SURFACE) != 0) {
      DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

      DRW_state_reset();

      GPU_framebuffer_bind(dfbl->overlay_fb);

      GPU_matrix_projection_set(rv3d->winmat);
      GPU_matrix_set(rv3d->viewmat);

      /* XR callbacks (controllers, custom draw functions) for session surface. */
      if (((v3d->flag2 & V3D_XR_SHOW_CONTROLLERS) != 0) ||
          ((v3d->flag2 & V3D_XR_SHOW_CUSTOM_OVERLAYS) != 0))
      {
        GPU_depth_test(GPU_DEPTH_NONE);
        GPU_apply_state();

        if ((v3d->flag2 & V3D_XR_SHOW_CONTROLLERS) != 0) {
          ARegionType *art = WM_xr_surface_controller_region_type_get();
          if (art) {
            ED_region_surface_draw_cb_draw(art, REGION_DRAW_POST_VIEW);
          }
        }
        if ((v3d->flag2 & V3D_XR_SHOW_CUSTOM_OVERLAYS) != 0) {
          SpaceType *st = BKE_spacetype_from_id(SPACE_VIEW3D);
          if (st) {
            ARegionType *art = BKE_regiontype_from_id(st, RGN_TYPE_XR);
            if (art) {
              ED_region_surface_draw_cb_draw(art, REGION_DRAW_POST_VIEW);
            }
          }
        }

        DRW_state_reset();
      }

      GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
    }
#endif
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

void DRW_draw_view(const bContext *C)
{
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d) {
    Depsgraph *depsgraph = CTX_data_expect_evaluated_depsgraph(C);
    ARegion *region = CTX_wm_region(C);
    Scene *scene = DEG_get_evaluated_scene(depsgraph);
    RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->shading.type);
    GPUViewport *viewport = WM_draw_region_get_bound_viewport(region);

    /* Reset before using it. */
    drw_state_prepare_clean_for_draw(&DST);
    DST.options.draw_text = ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0 &&
                             (v3d->overlay.flag & V3D_OVERLAY_HIDE_TEXT) != 0);
    DST.options.draw_background = (scene->r.alphamode == R_ADDSKY) ||
                                  (v3d->shading.type != OB_RENDER);
    DRW_draw_render_loop_ex(depsgraph, engine_type, region, v3d, viewport, C);
  }
  else {
    Depsgraph *depsgraph = CTX_data_expect_evaluated_depsgraph(C);
    ARegion *region = CTX_wm_region(C);
    GPUViewport *viewport = WM_draw_region_get_bound_viewport(region);
    drw_state_prepare_clean_for_draw(&DST);
    DRW_draw_render_loop_2d_ex(depsgraph, region, viewport, C);
  }
}

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

  BKE_view_layer_synced_ensure(scene, view_layer);
  DST.draw_ctx.evil_C = evil_C;
  DST.draw_ctx = (DRWContextState){
      .region = region,
      .rv3d = rv3d,
      .v3d = v3d,
      .scene = scene,
      .view_layer = view_layer,
      .obact = BKE_view_layer_active_object_get(view_layer),
      .engine_type = engine_type,
      .depsgraph = depsgraph,

      /* reuse if caller sets */
      .evil_C = DST.draw_ctx.evil_C,
  };
  drw_task_graph_init();
  drw_context_state_init();

  drw_manager_init(&DST, viewport, NULL);
  DRW_viewport_colormanagement_set(viewport);

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

  /* Update UBO's */
  DRW_globals_update();

  drw_debug_init();
  DRW_pointcloud_init();
  DRW_curves_init(DST.vmempool);
  DRW_volume_init(DST.vmempool);
  DRW_smoke_init(DST.vmempool);

  /* No frame-buffer allowed before drawing. */
  BLI_assert(GPU_framebuffer_active_get() == GPU_framebuffer_back_get());

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
      DST.dupli_origin_data = NULL;
      DEGObjectIterSettings deg_iter_settings = {0};
      deg_iter_settings.depsgraph = depsgraph;
      deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
      if (v3d->flag2 & V3D_SHOW_VIEWER) {
        deg_iter_settings.viewer_path = &v3d->viewer_path;
      }
      DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
        if ((object_type_exclude_viewport & (1 << ob->type)) != 0) {
          continue;
        }
        if (!BKE_object_is_visible_in_viewport(v3d, ob)) {
          continue;
        }
        DST.dupli_parent = data_.dupli_parent;
        DST.dupli_source = data_.dupli_object_current;
        drw_duplidata_load(ob);
        drw_engines_cache_populate(ob);
      }
      DEG_OBJECT_ITER_END;
    }

    drw_duplidata_free();
    drw_engines_cache_finish();

    drw_task_graph_deinit();
    DRW_render_instance_buffer_finish();

#ifdef USE_PROFILE
    double *cache_time = DRW_view_data_cache_time_get(DST.view_data_active);
    PROFILE_END_UPDATE(*cache_time, stime);
#endif
  }

  DRW_stats_begin();

  GPU_framebuffer_bind(DST.default_framebuffer);

  /* Start Drawing */
  DRW_state_reset();

  GPU_framebuffer_bind(DST.default_framebuffer);
  GPU_framebuffer_clear_depth_stencil(DST.default_framebuffer, 1.0f, 0xFF);

  DRW_curves_update();

  DRW_draw_callbacks_pre_scene();

  drw_engines_draw_scene();

  /* Fix 3D view "lagging" on APPLE and WIN32+NVIDIA. (See #56996, #61474) */
  if (GPU_type_matches_ex(GPU_DEVICE_ANY, GPU_OS_ANY, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL)) {
    GPU_flush();
  }

  DRW_smoke_exit(DST.vmempool);

  DRW_stats_reset();

  DRW_draw_callbacks_post_scene();

  if (WM_draw_region_get_bound_viewport(region)) {
    /* Don't unbind the frame-buffer yet in this case and let
     * GPU_viewport_unbind do it, so that we can still do further
     * drawing of action zones on top. */
  }
  else {
    GPU_framebuffer_restore();
  }

  DRW_state_reset();
  drw_engines_disable();

  drw_manager_exit(&DST);
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
  const bool is_xr_surface = ((v3d->flag & V3D_XR_SESSION_SURFACE) != 0);

  /* Create temporary viewport if needed or update the existing viewport. */
  GPUViewport *render_viewport = viewport;
  if (viewport == NULL) {
    render_viewport = GPU_viewport_create();
  }
  else {
    drw_notify_view_update_offscreen(depsgraph, engine_type, region, v3d, render_viewport);
  }

  GPU_viewport_bind_from_offscreen(render_viewport, ofs, is_xr_surface);

  /* Just here to avoid an assert but shouldn't be required in practice. */
  GPU_framebuffer_restore();

  /* Reset before using it. */
  drw_state_prepare_clean_for_draw(&DST);
  DST.options.is_image_render = is_image_render;
  DST.options.draw_background = draw_background;
  DRW_draw_render_loop_ex(depsgraph, engine_type, region, v3d, render_viewport, NULL);

  if (draw_background) {
    /* HACK(@fclem): In this case we need to make sure the final alpha is 1.
     * We use the blend mode to ensure that. A better way to fix that would
     * be to do that in the color-management shader. */
    GPU_offscreen_bind(ofs, false);
    GPU_clear_color(0.0f, 0.0f, 0.0f, 1.0f);
    /* Pre-multiply alpha over black background. */
    GPU_blend(GPU_BLEND_ALPHA_PREMULT);
  }

  GPU_matrix_identity_set();
  GPU_matrix_identity_projection_set();
  const bool do_overlays = (v3d->flag2 & V3D_HIDE_OVERLAYS) == 0 ||
                           ELEM(v3d->shading.type, OB_WIRE, OB_SOLID) ||
                           (ELEM(v3d->shading.type, OB_MATERIAL) &&
                            (v3d->shading.flag & V3D_SHADING_SCENE_WORLD) == 0) ||
                           (ELEM(v3d->shading.type, OB_RENDER) &&
                            (v3d->shading.flag & V3D_SHADING_SCENE_WORLD_RENDER) == 0);
  GPU_viewport_unbind_from_offscreen(render_viewport, ofs, do_color_management, do_overlays);

  if (draw_background) {
    /* Reset default. */
    GPU_blend(GPU_BLEND_NONE);
  }

  /* Free temporary viewport. */
  if (viewport == NULL) {
    GPU_viewport_free(render_viewport);
  }
}

bool DRW_render_check_grease_pencil(Depsgraph *depsgraph)
{
  if (!drw_gpencil_engine_needed(depsgraph, NULL)) {
    return false;
  }

  DEGObjectIterSettings deg_iter_settings = {0};
  deg_iter_settings.depsgraph = depsgraph;
  deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
    if (ob->type == OB_GPENCIL_LEGACY) {
      if (DRW_object_visibility_in_active_context(ob) & OB_VISIBLE_SELF) {
        return true;
      }
    }
  }
  DEG_OBJECT_ITER_END;

  return false;
}

static void DRW_render_gpencil_to_image(RenderEngine *engine,
                                        struct RenderLayer *render_layer,
                                        const rcti *rect)
{
  if (draw_engine_gpencil_type.render_to_image) {
    ViewportEngineData *gpdata = DRW_view_data_engine_data_get_ensure(DST.view_data_active,
                                                                      &draw_engine_gpencil_type);
    draw_engine_gpencil_type.render_to_image(gpdata, engine, render_layer, rect);
  }
}

void DRW_render_gpencil(struct RenderEngine *engine, struct Depsgraph *depsgraph)
{
  /* This function should only be called if there are grease pencil objects,
   * especially important to avoid failing in background renders without OpenGL context. */
  BLI_assert(DRW_render_check_grease_pencil(depsgraph));

  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  RenderEngineType *engine_type = engine->type;
  Render *render = engine->re;

  DRW_render_context_enable(render);

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

  const int size[2] = {engine->resolution_x, engine->resolution_y};

  drw_manager_init(&DST, NULL, size);

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
       render_view = render_view->next)
  {
    RE_SetActiveRenderView(render, render_view->name);
    DRW_view_reset();
    DST.buffer_finish_called = false;
    DRW_render_gpencil_to_image(engine, render_layer, &render_rect);
  }

  DRW_state_reset();

  GPU_depth_test(GPU_DEPTH_NONE);

  drw_manager_exit(&DST);

  /* Restore Drawing area. */
  GPU_framebuffer_restore();

  DRW_render_context_disable(render);

  DST.buffer_finish_called = false;
}

void DRW_render_to_image(RenderEngine *engine, struct Depsgraph *depsgraph)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  RenderEngineType *engine_type = engine->type;
  DrawEngineType *draw_engine_type = engine_type->draw_engine;
  Render *render = engine->re;

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

  /* Begin GPU workload Boundary */
  GPU_render_begin();

  const int size[2] = {engine->resolution_x, engine->resolution_y};

  drw_manager_init(&DST, NULL, size);

  ViewportEngineData *data = DRW_view_data_engine_data_get_ensure(DST.view_data_active,
                                                                  draw_engine_type);

  /* Main rendering. */
  rctf view_rect;
  rcti render_rect;
  RE_GetViewPlane(render, &view_rect, &render_rect);
  if (BLI_rcti_is_empty(&render_rect)) {
    BLI_rcti_init(&render_rect, 0, size[0], 0, size[1]);
  }

  /* Reset state before drawing */
  DRW_state_reset();

  /* set default viewport */
  GPU_viewport(0, 0, size[0], size[1]);

  /* Init render result. */
  RenderResult *render_result = RE_engine_begin_result(engine,
                                                       0,
                                                       0,
                                                       size[0],
                                                       size[1],
                                                       view_layer->name,
                                                       /*RR_ALL_VIEWS*/ NULL);
  RenderLayer *render_layer = render_result->layers.first;
  for (RenderView *render_view = render_result->views.first; render_view != NULL;
       render_view = render_view->next)
  {
    RE_SetActiveRenderView(render, render_view->name);
    DRW_view_reset();
    engine_type->draw_engine->render_to_image(data, engine, render_layer, &render_rect);
    DST.buffer_finish_called = false;
  }

  RE_engine_end_result(engine, render_result, false, false, false);

  if (engine_type->draw_engine->store_metadata) {
    RenderResult *final_render_result = RE_engine_get_result(engine);
    engine_type->draw_engine->store_metadata(data, final_render_result);
  }

  GPU_framebuffer_restore();

  DRW_smoke_exit(DST.vmempool);

  drw_manager_exit(&DST);
  DRW_cache_free_old_subdiv();

  /* Reset state after drawing */
  DRW_state_reset();

  /* End GPU workload Boundary */
  GPU_render_end();
}

void DRW_render_object_iter(
    void *vedata,
    RenderEngine *engine,
    struct Depsgraph *depsgraph,
    void (*callback)(void *vedata, Object *ob, RenderEngine *engine, struct Depsgraph *depsgraph))
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  DRW_pointcloud_init();
  DRW_curves_init(DST.vmempool);
  DRW_volume_init(DST.vmempool);
  DRW_smoke_init(DST.vmempool);

  drw_task_graph_init();
  const int object_type_exclude_viewport = draw_ctx->v3d ?
                                               draw_ctx->v3d->object_type_exclude_viewport :
                                               0;
  DST.dupli_origin = NULL;
  DST.dupli_origin_data = NULL;
  DEGObjectIterSettings deg_iter_settings = {0};
  deg_iter_settings.depsgraph = depsgraph;
  deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
    if ((object_type_exclude_viewport & (1 << ob->type)) == 0) {
      DST.dupli_parent = data_.dupli_parent;
      DST.dupli_source = data_.dupli_object_current;
      DST.ob_handle = 0;
      drw_duplidata_load(ob);

      if (!DST.dupli_source) {
        drw_batch_cache_validate(ob);
      }
      callback(vedata, ob, engine, depsgraph);
      if (!DST.dupli_source) {
        drw_batch_cache_generate_requested(ob);
      }
    }
  }
  DEG_OBJECT_ITER_END;

  drw_duplidata_free();
  drw_task_graph_deinit();
}

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

  drw_manager_init(&DST, NULL, NULL);

  DRW_pointcloud_init();
  DRW_curves_init(DST.vmempool);
  DRW_volume_init(DST.vmempool);
  DRW_smoke_init(DST.vmempool);

  ViewportEngineData *data = DRW_view_data_engine_data_get_ensure(DST.view_data_active,
                                                                  draw_engine_type);

  /* Execute the callback */
  callback(data, user_data);
  DST.buffer_finish_called = false;

  DRW_smoke_exit(DST.vmempool);

  GPU_framebuffer_restore();

  /* The use of custom pipeline in other thread using the same
   * resources as the main thread (viewport) may lead to data
   * races and undefined behavior on certain drivers. Using
   * GPU_finish to sync seems to fix the issue. (see #62997) */
  eGPUBackendType type = GPU_backend_get_type();
  if (type == GPU_BACKEND_OPENGL) {
    GPU_finish();
  }

  drw_manager_exit(&DST);
}

void DRW_cache_restart(void)
{
  DRW_smoke_exit(DST.vmempool);

  drw_manager_init(&DST, DST.viewport, (int[2]){UNPACK2(DST.size)});

  DST.buffer_finish_called = false;

  DRW_pointcloud_init();
  DRW_curves_init(DST.vmempool);
  DRW_volume_init(DST.vmempool);
  DRW_smoke_init(DST.vmempool);
}

void DRW_draw_render_loop_2d_ex(struct Depsgraph *depsgraph,
                                ARegion *region,
                                GPUViewport *viewport,
                                const bContext *evil_C)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);

  BKE_view_layer_synced_ensure(scene, view_layer);
  DST.draw_ctx.evil_C = evil_C;
  DST.draw_ctx = (DRWContextState){
      .region = region,
      .scene = scene,
      .view_layer = view_layer,
      .obact = BKE_view_layer_active_object_get(view_layer),
      .depsgraph = depsgraph,
      .space_data = CTX_wm_space_data(evil_C),

      /* reuse if caller sets */
      .evil_C = DST.draw_ctx.evil_C,
  };

  drw_context_state_init();
  drw_manager_init(&DST, viewport, NULL);
  DRW_viewport_colormanagement_set(viewport);

  /* TODO(jbakker): Only populate when editor needs to draw object.
   * for the image editor this is when showing UVs. */
  const bool do_populate_loop = (DST.draw_ctx.space_data->spacetype == SPACE_IMAGE);
  const bool do_annotations = drw_draw_show_annotation();
  const bool do_draw_gizmos = (DST.draw_ctx.space_data->spacetype != SPACE_IMAGE);

  /* Get list of enabled engines */
  drw_engines_enable_editors();
  drw_engines_data_validate();

  /* Update UBO's */
  DRW_globals_update();

  drw_debug_init();

  /* No frame-buffer allowed before drawing. */
  BLI_assert(GPU_framebuffer_active_get() == GPU_framebuffer_back_get());
  GPU_framebuffer_bind(DST.default_framebuffer);
  GPU_framebuffer_clear_depth_stencil(DST.default_framebuffer, 1.0f, 0xFF);

  /* Init engines */
  drw_engines_init();
  drw_task_graph_init();

  /* Cache filling */
  {
    PROFILE_START(stime);
    drw_engines_cache_init();

    /* Only iterate over objects when overlay uses object data. */
    if (do_populate_loop) {
      DEGObjectIterSettings deg_iter_settings = {0};
      deg_iter_settings.depsgraph = depsgraph;
      deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
      DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
        drw_engines_cache_populate(ob);
      }
      DEG_OBJECT_ITER_END;
    }

    drw_engines_cache_finish();

    DRW_render_instance_buffer_finish();

#ifdef USE_PROFILE
    double *cache_time = DRW_view_data_cache_time_get(DST.view_data_active);
    PROFILE_END_UPDATE(*cache_time, stime);
#endif
  }
  drw_task_graph_deinit();

  DRW_stats_begin();

  GPU_framebuffer_bind(DST.default_framebuffer);

  /* Start Drawing */
  DRW_state_reset();

  if (DST.draw_ctx.evil_C) {
    ED_region_draw_cb_draw(DST.draw_ctx.evil_C, DST.draw_ctx.region, REGION_DRAW_PRE_VIEW);
  }

  drw_engines_draw_scene();

  /* Fix 3D view being "laggy" on MACOS and MS-Windows+NVIDIA. (See #56996, #61474) */
  if (GPU_type_matches_ex(GPU_DEVICE_ANY, GPU_OS_ANY, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL)) {
    GPU_flush();
  }

  if (DST.draw_ctx.evil_C) {
    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
    DRW_state_reset();

    GPU_framebuffer_bind(dfbl->overlay_fb);

    GPU_depth_test(GPU_DEPTH_NONE);
    GPU_matrix_push_projection();
    wmOrtho2(
        region->v2d.cur.xmin, region->v2d.cur.xmax, region->v2d.cur.ymin, region->v2d.cur.ymax);
    if (do_annotations) {
      ED_annotation_draw_view2d(DST.draw_ctx.evil_C, true);
    }
    GPU_depth_test(GPU_DEPTH_NONE);
    ED_region_draw_cb_draw(DST.draw_ctx.evil_C, DST.draw_ctx.region, REGION_DRAW_POST_VIEW);
    GPU_matrix_pop_projection();
    /* Callback can be nasty and do whatever they want with the state.
     * Don't trust them! */
    DRW_state_reset();

    GPU_depth_test(GPU_DEPTH_NONE);
    drw_engines_draw_text();

    if (do_annotations) {
      GPU_depth_test(GPU_DEPTH_NONE);
      ED_annotation_draw_view2d(DST.draw_ctx.evil_C, false);
    }
  }

  DRW_draw_cursor_2d();
  ED_region_pixelspace(DST.draw_ctx.region);

  if (do_draw_gizmos) {
    GPU_depth_test(GPU_DEPTH_NONE);
    DRW_draw_gizmo_2d();
  }

  DRW_stats_reset();

  if (G.debug_value > 20 && G.debug_value < 30) {
    GPU_depth_test(GPU_DEPTH_NONE);
    /* local coordinate visible rect inside region, to accommodate overlapping ui */
    const rcti *rect = ED_region_visible_rect(DST.draw_ctx.region);
    DRW_stats_draw(rect);
  }

  GPU_depth_test(GPU_DEPTH_LESS_EQUAL);

  if (WM_draw_region_get_bound_viewport(region)) {
    /* Don't unbind the frame-buffer yet in this case and let
     * GPU_viewport_unbind do it, so that we can still do further
     * drawing of action zones on top. */
  }
  else {
    GPU_framebuffer_restore();
  }

  DRW_state_reset();
  drw_engines_disable();

  drw_manager_exit(&DST);
}

static struct DRWSelectBuffer {
  struct GPUFrameBuffer *framebuffer_depth_only;
  struct GPUTexture *texture_depth;
} g_select_buffer = {NULL};

static void draw_select_framebuffer_depth_only_setup(const int size[2])
{
  if (g_select_buffer.framebuffer_depth_only == NULL) {
    g_select_buffer.framebuffer_depth_only = GPU_framebuffer_create("framebuffer_depth_only");
  }

  if ((g_select_buffer.texture_depth != NULL) &&
      ((GPU_texture_width(g_select_buffer.texture_depth) != size[0]) ||
       (GPU_texture_height(g_select_buffer.texture_depth) != size[1])))
  {
    GPU_texture_free(g_select_buffer.texture_depth);
    g_select_buffer.texture_depth = NULL;
  }

  if (g_select_buffer.texture_depth == NULL) {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    g_select_buffer.texture_depth = GPU_texture_create_2d(
        "select_depth", size[0], size[1], 1, GPU_DEPTH_COMPONENT24, usage, NULL);

    GPU_framebuffer_texture_attach(
        g_select_buffer.framebuffer_depth_only, g_select_buffer.texture_depth, 0, 0);

    GPU_framebuffer_check_valid(g_select_buffer.framebuffer_depth_only, NULL);
  }
}

void DRW_render_instance_buffer_finish(void)
{
  BLI_assert_msg(!DST.buffer_finish_called, "DRW_render_instance_buffer_finish called twice!");
  DST.buffer_finish_called = true;
  DRW_instance_buffer_finish(DST.vmempool->idatalist);
  drw_resource_buffer_finish(DST.vmempool);
}

void DRW_render_set_time(RenderEngine *engine, Depsgraph *depsgraph, int frame, float subframe)
{
  RE_engine_frame_set(engine, frame, subframe);
  DST.draw_ctx.scene = DEG_get_evaluated_scene(depsgraph);
  DST.draw_ctx.view_layer = DEG_get_evaluated_view_layer(depsgraph);
}

void DRW_draw_select_loop(struct Depsgraph *depsgraph,
                          ARegion *region,
                          View3D *v3d,
                          bool use_obedit_skip,
                          bool draw_surface,
                          bool UNUSED(use_nearest),
                          const bool do_material_sub_selection,
                          const rcti *rect,
                          DRW_SelectPassFn select_pass_fn,
                          void *select_pass_user_data,
                          DRW_ObjectFilterFn object_filter_fn,
                          void *object_filter_user_data)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->shading.type);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);

  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  Object *obedit = use_obedit_skip ? NULL : OBEDIT_FROM_OBACT(obact);
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
      /* NOTE: don't use "BKE_object_pose_armature_get" here, it breaks selection. */
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

  const int viewport_size[2] = {BLI_rcti_size_x(rect), BLI_rcti_size_y(rect)};
  drw_manager_init(&DST, NULL, viewport_size);

  DST.options.is_select = true;
  DST.options.is_material_select = do_material_sub_selection;
  drw_task_graph_init();
  /* Get list of enabled engines */
  if (U.experimental.enable_overlay_next) {
    use_drw_engine(&draw_engine_select_next_type);
  }
  else if (use_obedit) {
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

  /* Update UBO's */
  DRW_globals_update();

  /* Init engines */
  drw_engines_init();
  DRW_pointcloud_init();
  DRW_curves_init(DST.vmempool);
  DRW_volume_init(DST.vmempool);
  DRW_smoke_init(DST.vmempool);

  {
    drw_engines_cache_init();
    drw_engines_world_update(scene);

    if (use_obedit) {
      FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, object_type, object_mode, ob_iter) {
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
      DST.dupli_origin_data = NULL;
      DEGObjectIterSettings deg_iter_settings = {0};
      deg_iter_settings.depsgraph = depsgraph;
      deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
      if (v3d->flag2 & V3D_SHOW_VIEWER) {
        deg_iter_settings.viewer_path = &v3d->viewer_path;
      }
      DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
        if (!BKE_object_is_visible_in_viewport(v3d, ob)) {
          continue;
        }

        if (use_pose_exception && (ob->mode & OB_MODE_POSE)) {
          if ((ob->base_flag & BASE_ENABLED_AND_VISIBLE_IN_DEFAULT_VIEWPORT) == 0) {
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

          DRW_select_load_id(ob->runtime.select_id);
          DST.dupli_parent = data_.dupli_parent;
          DST.dupli_source = data_.dupli_object_current;
          drw_duplidata_load(ob);
          drw_engines_cache_populate(ob);
        }
      }
      DEG_OBJECT_ITER_END;
    }

    drw_duplidata_free();
    drw_task_graph_deinit();
    drw_engines_cache_finish();

    DRW_render_instance_buffer_finish();
  }

  /* Setup frame-buffer. */
  draw_select_framebuffer_depth_only_setup(viewport_size);
  GPU_framebuffer_bind(g_select_buffer.framebuffer_depth_only);
  GPU_framebuffer_clear_depth(g_select_buffer.framebuffer_depth_only, 1.0f);
  /* WORKAROUND: Needed for Select-Next for keeping the same codeflow as Overlay-Next. */
  BLI_assert(DRW_viewport_texture_list_get()->depth == NULL);
  DRW_viewport_texture_list_get()->depth = g_select_buffer.texture_depth;

  /* Start Drawing */
  DRW_state_reset();
  DRW_draw_callbacks_pre_scene();

  DRW_curves_update();

  /* Only 1-2 passes. */
  while (true) {
    if (!select_pass_fn(DRW_SELECT_PASS_PRE, select_pass_user_data)) {
      break;
    }
    if (!U.experimental.enable_overlay_next) {
      DRW_state_lock(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_TEST_ENABLED);
    }

    drw_engines_draw_scene();

    if (!U.experimental.enable_overlay_next) {
      DRW_state_lock(0);
    }

    if (!select_pass_fn(DRW_SELECT_PASS_POST, select_pass_user_data)) {
      break;
    }
  }

  DRW_smoke_exit(DST.vmempool);

  /* WORKAROUND: Do not leave ownership to the viewport list. */
  DRW_viewport_texture_list_get()->depth = NULL;

  DRW_state_reset();
  drw_engines_disable();

  drw_manager_exit(&DST);

  GPU_framebuffer_restore();

#endif /* USE_GPU_SELECT */
}

void DRW_draw_depth_loop(struct Depsgraph *depsgraph,
                         ARegion *region,
                         View3D *v3d,
                         GPUViewport *viewport,
                         const bool use_gpencil,
                         const bool use_basic,
                         const bool use_overlay)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->shading.type);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  RegionView3D *rv3d = region->regiondata;

  /* Reset before using it. */
  drw_state_prepare_clean_for_draw(&DST);

  DST.options.is_depth = true;

  /* Instead of 'DRW_context_state_init(C, &DST.draw_ctx)', assign from args */
  BKE_view_layer_synced_ensure(scene, view_layer);
  DST.draw_ctx = (DRWContextState){
      .region = region,
      .rv3d = rv3d,
      .v3d = v3d,
      .scene = scene,
      .view_layer = view_layer,
      .obact = BKE_view_layer_active_object_get(view_layer),
      .engine_type = engine_type,
      .depsgraph = depsgraph,
  };
  drw_context_state_init();
  drw_manager_init(&DST, viewport, NULL);

  if (use_gpencil) {
    use_drw_engine(&draw_engine_gpencil_type);
  }
  if (use_basic) {
    drw_engines_enable_basic();
  }
  if (use_overlay) {
    drw_engines_enable_overlays();
  }

  drw_task_graph_init();

  /* Setup frame-buffer. */
  GPUTexture *depth_tx = GPU_viewport_depth_texture(viewport);

  GPUFrameBuffer *depth_fb = NULL;
  GPU_framebuffer_ensure_config(&depth_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(depth_tx),
                                    GPU_ATTACHMENT_NONE,
                                });

  GPU_framebuffer_bind(depth_fb);
  GPU_framebuffer_clear_depth(depth_fb, 1.0f);

  /* Update UBO's */
  DRW_globals_update();

  /* Init engines */
  drw_engines_init();
  DRW_pointcloud_init();
  DRW_curves_init(DST.vmempool);
  DRW_volume_init(DST.vmempool);
  DRW_smoke_init(DST.vmempool);

  {
    drw_engines_cache_init();
    drw_engines_world_update(DST.draw_ctx.scene);

    const int object_type_exclude_viewport = v3d->object_type_exclude_viewport;
    DST.dupli_origin = NULL;
    DST.dupli_origin_data = NULL;
    DEGObjectIterSettings deg_iter_settings = {0};
    deg_iter_settings.depsgraph = DST.draw_ctx.depsgraph;
    deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
    if (v3d->flag2 & V3D_SHOW_VIEWER) {
      deg_iter_settings.viewer_path = &v3d->viewer_path;
    }
    DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
      if ((object_type_exclude_viewport & (1 << ob->type)) != 0) {
        continue;
      }
      if (!BKE_object_is_visible_in_viewport(v3d, ob)) {
        continue;
      }
      DST.dupli_parent = data_.dupli_parent;
      DST.dupli_source = data_.dupli_object_current;
      drw_duplidata_load(ob);
      drw_engines_cache_populate(ob);
    }
    DEG_OBJECT_ITER_END;

    drw_duplidata_free();
    drw_engines_cache_finish();

    drw_task_graph_deinit();
    DRW_render_instance_buffer_finish();
  }

  /* Start Drawing */
  DRW_state_reset();

  DRW_curves_update();

  drw_engines_draw_scene();

  DRW_smoke_exit(DST.vmempool);

  DRW_state_reset();

  /* TODO: Reading depth for operators should be done here. */

  GPU_framebuffer_restore();
  GPU_framebuffer_free(depth_fb);

  drw_engines_disable();

  drw_manager_exit(&DST);
}

void DRW_draw_select_id(Depsgraph *depsgraph, ARegion *region, View3D *v3d, const rcti *rect)
{
  SELECTID_Context *sel_ctx = DRW_select_engine_context_get();
  GPUViewport *viewport = WM_draw_region_get_viewport(region);
  if (!viewport) {
    /* Selection engine requires a viewport.
     * TODO(@germano): This should be done internally in the engine. */
    sel_ctx->is_dirty = true;
    sel_ctx->objects_drawn_len = 0;
    sel_ctx->index_drawn_len = 1;
    return;
  }

  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);

  /* Reset before using it. */
  drw_state_prepare_clean_for_draw(&DST);

  /* Instead of 'DRW_context_state_init(C, &DST.draw_ctx)', assign from args */
  BKE_view_layer_synced_ensure(scene, view_layer);
  DST.draw_ctx = (DRWContextState){
      .region = region,
      .rv3d = region->regiondata,
      .v3d = v3d,
      .scene = scene,
      .view_layer = view_layer,
      .obact = BKE_view_layer_active_object_get(view_layer),
      .depsgraph = depsgraph,
  };
  drw_task_graph_init();
  drw_context_state_init();

  drw_manager_init(&DST, viewport, NULL);

  /* Update UBO's */
  UI_SetTheme(SPACE_VIEW3D, RGN_TYPE_WINDOW);
  DRW_globals_update();

  /* Init Select Engine */
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

    if (RETOPOLOGY_ENABLED(v3d) && !XRAY_ENABLED(v3d)) {
      DEGObjectIterSettings deg_iter_settings = {0};
      deg_iter_settings.depsgraph = depsgraph;
      deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
      DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
        if (ob->type != OB_MESH) {
          /* The iterator has evaluated meshes for all solid objects.
           * It also has non-mesh objects however, which are not supported here. */
          continue;
        }
        if (DRW_object_is_in_edit_mode(ob)) {
          /* Only background (non-edit) objects are used for occlusion. */
          continue;
        }
        if (!BKE_object_is_visible_in_viewport(v3d, ob)) {
          continue;
        }
        drw_engines_cache_populate(ob);
      }
      DEG_OBJECT_ITER_END;
    }

    drw_engines_cache_finish();

    drw_task_graph_deinit();
#if 0 /* This is a workaround to a nasty bug that seems to be a nasty driver bug. (See #69377) */
    DRW_render_instance_buffer_finish();
#else
    DST.buffer_finish_called = true;
    // DRW_instance_buffer_finish(DST.vmempool->idatalist);
    drw_resource_buffer_finish(DST.vmempool);
#endif
  }

  /* Start Drawing */
  DRW_state_reset();
  drw_engines_draw_scene();
  DRW_state_reset();

  drw_engines_disable();

  drw_manager_exit(&DST);
}

void DRW_draw_depth_object(
    Scene *scene, ARegion *region, View3D *v3d, GPUViewport *viewport, Object *object)
{
  RegionView3D *rv3d = region->regiondata;

  GPU_matrix_projection_set(rv3d->winmat);
  GPU_matrix_set(rv3d->viewmat);
  GPU_matrix_mul(object->object_to_world);

  /* Setup frame-buffer. */
  GPUTexture *depth_tx = GPU_viewport_depth_texture(viewport);

  GPUFrameBuffer *depth_fb = NULL;
  GPU_framebuffer_ensure_config(&depth_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(depth_tx),
                                    GPU_ATTACHMENT_NONE,
                                });

  GPU_framebuffer_bind(depth_fb);
  GPU_framebuffer_clear_depth(depth_fb, 1.0f);
  GPU_depth_test(GPU_DEPTH_LESS_EQUAL);

  struct GPUClipPlanes planes;
  const bool use_clipping_planes = RV3D_CLIPPING_ENABLED(v3d, rv3d);
  if (use_clipping_planes) {
    GPU_clip_distances(6);
    ED_view3d_clipping_local(rv3d, object->object_to_world);
    for (int i = 0; i < 6; i++) {
      copy_v4_v4(planes.world[i], rv3d->clip_local[i]);
    }
    copy_m4_m4(planes.ClipModelMatrix, object->object_to_world);
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

      const eGPUShaderConfig sh_cfg = use_clipping_planes ? GPU_SHADER_CFG_CLIPPED :
                                                            GPU_SHADER_CFG_DEFAULT;
      GPU_batch_program_set_builtin_with_config(batch, GPU_SHADER_3D_DEPTH_ONLY, sh_cfg);

      GPUUniformBuf *ubo = NULL;
      if (use_clipping_planes) {
        ubo = GPU_uniformbuf_create_ex(sizeof(struct GPUClipPlanes), &planes, __func__);
        GPU_batch_uniformbuf_bind(batch, "clipPlanes", ubo);
      }

      GPU_batch_draw(batch);
      GPU_uniformbuf_free(ubo);

    } break;
    case OB_CURVES_LEGACY:
    case OB_SURF:
      break;
  }

  if (RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
    GPU_clip_distances(0);
  }

  GPU_matrix_set(rv3d->viewmat);
  GPU_depth_test(GPU_DEPTH_NONE);
  GPU_framebuffer_restore();

  GPU_framebuffer_free(depth_fb);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Manager State (DRW_state)
 * \{ */

bool DRW_state_is_fbo(void)
{
  return ((DST.default_framebuffer != NULL) || DST.options.is_image_render) &&
         !DRW_state_is_depth() && !DRW_state_is_select();
}

bool DRW_state_is_select(void)
{
  return DST.options.is_select;
}

bool DRW_state_is_material_select(void)
{
  return DST.options.is_material_select;
}

bool DRW_state_is_depth(void)
{
  return DST.options.is_depth;
}

bool DRW_state_is_image_render(void)
{
  return DST.options.is_image_render;
}

bool DRW_state_is_scene_render(void)
{
  BLI_assert(DST.options.is_scene_render ? DST.options.is_image_render : true);
  return DST.options.is_scene_render;
}

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

bool DRW_state_is_navigating(void)
{
  const RegionView3D *rv3d = DST.draw_ctx.rv3d;
  return (rv3d) && (rv3d->rflag & (RV3D_NAVIGATING | RV3D_PAINTING));
}

bool DRW_state_show_text(void)
{
  return (DST.options.is_select) == 0 && (DST.options.is_depth) == 0 &&
         (DST.options.is_scene_render) == 0 && (DST.options.draw_text) == 0;
}

bool DRW_state_draw_support(void)
{
  View3D *v3d = DST.draw_ctx.v3d;
  return (DRW_state_is_scene_render() == false) && (v3d != NULL) &&
         ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0);
}

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
  DRWRegisteredDrawEngine *draw_engine = MEM_mallocN(sizeof(DRWRegisteredDrawEngine), __func__);
  draw_engine->draw_engine = draw_engine_type;
  draw_engine->index = g_registered_engines.len;

  BLI_addtail(&g_registered_engines.engines, draw_engine);
  g_registered_engines.len = BLI_listbase_count(&g_registered_engines.engines);
}

void DRW_engines_register_experimental(void)
{
  if (U.experimental.enable_eevee_next) {
    RE_engines_register(&DRW_engine_viewport_eevee_next_type);
  }
  if (U.experimental.enable_workbench_next) {
    RE_engines_register(&DRW_engine_viewport_workbench_next_type);
  }
}

void DRW_engines_register(void)
{
  RE_engines_register(&DRW_engine_viewport_eevee_type);
  RE_engines_register(&DRW_engine_viewport_workbench_type);

  DRW_engine_register(&draw_engine_gpencil_type);

  DRW_engine_register(&draw_engine_overlay_type);
  DRW_engine_register(&draw_engine_overlay_next_type);
  DRW_engine_register(&draw_engine_select_next_type);
  DRW_engine_register(&draw_engine_select_type);
  DRW_engine_register(&draw_engine_basic_type);
  DRW_engine_register(&draw_engine_compositor_type);
#ifdef WITH_DRAW_DEBUG
  DRW_engine_register(&draw_engine_debug_select_type);
#endif

  DRW_engine_register(&draw_engine_image_type);
  DRW_engine_register(DRW_engine_viewport_external_type.draw_engine);

  /* setup callbacks */
  {
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

    BKE_curves_batch_cache_dirty_tag_cb = DRW_curves_batch_cache_dirty_tag;
    BKE_curves_batch_cache_free_cb = DRW_curves_batch_cache_free;

    BKE_pointcloud_batch_cache_dirty_tag_cb = DRW_pointcloud_batch_cache_dirty_tag;
    BKE_pointcloud_batch_cache_free_cb = DRW_pointcloud_batch_cache_free;

    BKE_volume_batch_cache_dirty_tag_cb = DRW_volume_batch_cache_dirty_tag;
    BKE_volume_batch_cache_free_cb = DRW_volume_batch_cache_free;

    BKE_subsurf_modifier_free_gpu_cache_cb = DRW_subdiv_cache_free;
  }
}

static void drw_registered_engines_free(void)
{
  DRWRegisteredDrawEngine *next;
  for (DRWRegisteredDrawEngine *type = g_registered_engines.engines.first; type; type = next) {
    next = type->next;
    BLI_remlink(&R_engines, type);

    if (type->draw_engine->engine_free) {
      type->draw_engine->engine_free();
    }
    MEM_freeN(type);
  }

  BLI_listbase_clear(&g_registered_engines.engines);
  g_registered_engines.len = 0;
}

void DRW_engines_free(void)
{
  drw_registered_engines_free();

  if (DST.gl_context == NULL) {
    /* Nothing has been setup. Nothing to clear.
     * Otherwise, DRW_opengl_context_enable can
     * create a context in background mode. (see #62355) */
    return;
  }

  DRW_opengl_context_enable();

  DRW_TEXTURE_FREE_SAFE(g_select_buffer.texture_depth);
  GPU_FRAMEBUFFER_FREE_SAFE(g_select_buffer.framebuffer_depth_only);

  DRW_shaders_free();
  DRW_pointcloud_free();
  DRW_curves_free();
  DRW_volume_free();
  DRW_shape_cache_free();
  DRW_stats_free();
  DRW_globals_free();

  drw_debug_module_free(DST.debug);
  DST.debug = NULL;

  DRW_UBO_FREE_SAFE(G_draw.block_ubo);
  DRW_UBO_FREE_SAFE(G_draw.view_ubo);
  DRW_UBO_FREE_SAFE(G_draw.clipping_ubo);
  DRW_TEXTURE_FREE_SAFE(G_draw.ramp);
  DRW_TEXTURE_FREE_SAFE(G_draw.weight_ramp);

  if (DST.draw_list) {
    GPU_draw_list_discard(DST.draw_list);
  }

  DRW_opengl_context_disable();
}

void DRW_render_context_enable(Render *render)
{
  if (G.background && DST.gl_context == NULL) {
    WM_init_opengl();
  }

  GPU_render_begin();

  if (GPU_use_main_context_workaround()) {
    GPU_context_main_lock();
    DRW_opengl_context_enable();
    return;
  }

  void *re_gl_context = RE_gl_context_get(render);

  /* Changing Context */
  if (re_gl_context != NULL) {
    DRW_opengl_render_context_enable(re_gl_context);
    /* We need to query gpu context after a gl context has been bound. */
    void *re_gpu_context = NULL;
    re_gpu_context = RE_gpu_context_get(render);
    DRW_gpu_render_context_enable(re_gpu_context);
  }
  else {
    DRW_opengl_context_enable();
  }
}

void DRW_render_context_disable(Render *render)
{
  if (GPU_use_main_context_workaround()) {
    DRW_opengl_context_disable();
    GPU_render_end();
    GPU_context_main_unlock();
    return;
  }

  void *re_gl_context = RE_gl_context_get(render);

  if (re_gl_context != NULL) {
    void *re_gpu_context = NULL;
    re_gpu_context = RE_gpu_context_get(render);
    /* GPU rendering may occur during context disable. */
    DRW_gpu_render_context_disable(re_gpu_context);
    GPU_render_end();
    DRW_opengl_render_context_disable(re_gl_context);
  }
  else {
    DRW_opengl_context_disable();
    GPU_render_end();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Init/Exit (DRW_opengl_ctx)
 * \{ */

void DRW_opengl_context_create(void)
{
  BLI_assert(DST.gl_context == NULL); /* Ensure it's called once */

  DST.gl_context_mutex = BLI_ticket_mutex_alloc();
  /* This changes the active context. */
  DST.gl_context = WM_opengl_context_create();
  WM_opengl_context_activate(DST.gl_context);
  /* Be sure to create gpu_context too. */
  DST.gpu_context = GPU_context_create(0, DST.gl_context);
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

void DRW_opengl_context_enable_ex(bool UNUSED(restore))
{
  if (DST.gl_context != NULL) {
    /* IMPORTANT: We don't support immediate mode in render mode!
     * This shall remain in effect until immediate mode supports
     * multiple threads. */
    BLI_ticket_mutex_lock(DST.gl_context_mutex);
    GPU_render_begin();
    WM_opengl_context_activate(DST.gl_context);
    GPU_context_active_set(DST.gpu_context);
  }
}

void DRW_opengl_context_disable_ex(bool restore)
{
  if (DST.gl_context != NULL) {
#ifdef __APPLE__
    /* Need to flush before disabling draw context, otherwise it does not
     * always finish drawing and viewport can be empty or partially drawn */
    if (GPU_type_matches_ex(GPU_DEVICE_ANY, GPU_OS_MAC, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL)) {
      GPU_flush();
    }
#endif

    if (BLI_thread_is_main() && restore) {
      wm_window_reset_drawable();
    }
    else {
      WM_opengl_context_release(DST.gl_context);
      GPU_context_active_set(NULL);
    }

    /* Render boundaries are opened and closed here as this may be
     * called outside of an existing render loop. */
    GPU_render_end();

    BLI_ticket_mutex_unlock(DST.gl_context_mutex);
  }
}

void DRW_opengl_context_enable(void)
{
  /* TODO: should be replace by a more elegant alternative. */

  if (G.background && DST.gl_context == NULL) {
    WM_init_opengl();
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

  /* TODO: get rid of the blocking. Only here because of the static global DST. */
  BLI_ticket_mutex_lock(DST.gl_context_mutex);
  WM_opengl_context_activate(re_gl_context);
}

void DRW_opengl_render_context_disable(void *re_gl_context)
{
  WM_opengl_context_release(re_gl_context);
  /* TODO: get rid of the blocking. */
  BLI_ticket_mutex_unlock(DST.gl_context_mutex);
}

void DRW_gpu_render_context_enable(void *re_gpu_context)
{
  /* If thread is main you should use DRW_opengl_context_enable(). */
  BLI_assert(!BLI_thread_is_main());

  GPU_context_active_set(re_gpu_context);
}

void DRW_gpu_render_context_disable(void *UNUSED(re_gpu_context))
{
  GPU_flush();
  GPU_context_active_set(NULL);
}

/** \} */

#ifdef WITH_XR_OPENXR

void *DRW_xr_opengl_context_get(void)
{
  /* XXX: There should really be no such getter, but for VR we currently can't easily avoid it.
   * OpenXR needs some low level info for the OpenGL context that will be used for submitting the
   * final frame-buffer. VR could in theory create its own context, but that would mean we have to
   * switch to it just to submit the final frame, which has notable performance impact.
   *
   * We could "inject" a context through DRW_opengl_render_context_enable(), but that would have to
   * work from the main thread, which is tricky to get working too. The preferable solution would
   * be using a separate thread for VR drawing where a single context can stay active. */

  return DST.gl_context;
}

void *DRW_xr_gpu_context_get(void)
{
  /* XXX: See comment on #DRW_xr_opengl_context_get(). */

  return DST.gpu_context;
}

void DRW_xr_drawing_begin(void)
{
  /* XXX: See comment on #DRW_xr_opengl_context_get(). */

  BLI_ticket_mutex_lock(DST.gl_context_mutex);
}

void DRW_xr_drawing_end(void)
{
  /* XXX: See comment on #DRW_xr_opengl_context_get(). */

  BLI_ticket_mutex_unlock(DST.gl_context_mutex);
}

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal testing API for gtests
 * \{ */

#ifdef WITH_OPENGL_DRAW_TESTS

void DRW_draw_state_init_gtests(eGPUShaderConfig sh_cfg)
{
  DST.draw_ctx.sh_cfg = sh_cfg;
}

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw manager context release/activation
 *
 * These functions are used in cases when an OpenGL context creation is needed during the draw.
 * This happens, for example, when an external engine needs to create its own OpenGL context from
 * the engine initialization.
 *
 * Example of context creation:
 *
 *   const bool drw_state = DRW_opengl_context_release();
 *   gl_context = WM_opengl_context_create();
 *   DRW_opengl_context_activate(drw_state);
 *
 * Example of context destruction:
 *
 *   const bool drw_state = DRW_opengl_context_release();
 *   WM_opengl_context_activate(gl_context);
 *   WM_opengl_context_dispose(gl_context);
 *   DRW_opengl_context_activate(drw_state);
 *
 *
 * NOTE: Will only perform context modification when on main thread. This way these functions can
 * be used in an engine without check on whether it is a draw manager which manages OpenGL context
 * on the current thread. The downside of this is that if the engine performs OpenGL creation from
 * a non-main thread, that thread is supposed to not have OpenGL context ever bound by Blender.
 *
 * \{ */

bool DRW_opengl_context_release(void)
{
  if (!BLI_thread_is_main()) {
    return false;
  }

  if (GPU_context_active_get() != DST.gpu_context) {
    /* Context release is requested from the outside of the draw manager main draw loop, indicate
     * this to the `DRW_opengl_context_activate()` so that it restores drawable of the window. */
    return false;
  }

  GPU_context_active_set(NULL);
  WM_opengl_context_release(DST.gl_context);

  return true;
}

void DRW_opengl_context_activate(bool drw_state)
{
  if (!BLI_thread_is_main()) {
    return;
  }

  if (drw_state) {
    WM_opengl_context_activate(DST.gl_context);
    GPU_context_active_set(DST.gpu_context);
  }
  else {
    wm_window_reset_drawable();
  }
}

/** \} */
