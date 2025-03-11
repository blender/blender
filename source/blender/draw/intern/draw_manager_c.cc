/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include <cstdio>

#include "CLG_log.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_threads.h"

#include "BLF_api.hh"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_curves.h"
#include "BKE_duplilist.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_grease_pencil.h"
#include "BKE_lattice.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_mball.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_pointcloud.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_subdiv_modifier.hh"
#include "BKE_volume.hh"

#include "DNA_camera_types.h"
#include "DNA_mesh_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "ED_gpencil_legacy.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_view3d.hh"

#include "GPU_capabilities.hh"
#include "GPU_framebuffer.hh"
#include "GPU_matrix.hh"
#include "GPU_platform.hh"
#include "GPU_shader_shared.hh"
#include "GPU_state.hh"
#include "GPU_uniform_buffer.hh"
#include "GPU_viewport.hh"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"
#include "wm_window.hh"

#include "draw_cache.hh"
#include "draw_color_management.hh"
#include "draw_common_c.hh"
#include "draw_manager_c.hh"
#ifdef WITH_GPU_DRAW_TESTS
#  include "draw_manager_testing.hh"
#endif
#include "draw_manager_text.hh"
#include "draw_shader.hh"
#include "draw_subdivision.hh"
#include "draw_view_c.hh"
#include "draw_view_data.hh"

/* only for callbacks */
#include "draw_cache_impl.hh"

#include "engines/compositor/compositor_engine.h"
#include "engines/eevee_next/eevee_engine.h"
#include "engines/external/external_engine.h"
#include "engines/gpencil/gpencil_engine.h"
#include "engines/image/image_engine.h"
#include "engines/overlay/overlay_engine.h"
#include "engines/select/select_engine.hh"
#include "engines/workbench/workbench_engine.h"

#include "GPU_context.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "BLI_time.h"

#include "DRW_select_buffer.hh"

/* -------------------------------------------------------------------- */
/** \name GPU & System Context
 *
 * A global GPUContext is used for rendering every viewports (even on different windows).
 * This is because some resources cannot be shared between contexts (GPUFramebuffers, GPUBatch).
 * \{ */

/** Unique ghost context used by Viewports. */
static void *system_gpu_context = nullptr;
/** GPUContext associated to the system_gpu_context. */
static GPUContext *blender_gpu_context = nullptr;
/**
 * GPUContext cannot be used concurrently. This isn't required at the moment since viewports
 * aren't rendered in parallel but this could happen in the future. The old implementation of DRW
 * was also locking so this is a bit of a preventive measure. Could eventually be removed.
 */
static TicketMutex *system_gpu_context_mutex = nullptr;
/**
 * The usage of GPUShader objects is currently not thread safe. Since they are shared resources
 * between render engine instances, we cannot allow pass submissions in a concurent manner.
 */
static TicketMutex *submission_mutex = nullptr;

/** \} */

/** Render State: No persistent data between draw calls. */
thread_local DRWContext *g_context = nullptr;

static void drw_set(DRWContext &context)
{
  BLI_assert(g_context == nullptr);
  g_context = &context;
  g_context->prepare_clean_for_draw();
}

DRWContext &drw_get()
{
  return *g_context;
}

GPUFrameBuffer *DRWContext::default_framebuffer()
{
  DefaultFramebufferList *dfbl = DRW_view_data_default_framebuffer_list_get(view_data_active);
  return dfbl->default_fb;
}

void DRWContext::prepare_clean_for_draw()
{
  /* Reset all members to default values. */
  *this = {};
}

/* This function is used to reset draw manager to a state
 * where we don't re-use data by accident across different
 * draw calls. */
void DRWContext::state_ensure_not_reused()
{
#if 0 /* Creates compilation warning. */
  /* Poison the whole module. */
  memset(g_context, 0xff, sizeof(DRWContext));
#endif
  BLI_assert(g_context == this);
  g_context = nullptr;
}

static bool draw_show_annotation()
{
  if (drw_get().draw_ctx.space_data == nullptr) {
    View3D *v3d = drw_get().draw_ctx.v3d;
    return (v3d && ((v3d->flag2 & V3D_SHOW_ANNOTATION) != 0) &&
            ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0));
  }

  switch (drw_get().draw_ctx.space_data->spacetype) {
    case SPACE_IMAGE: {
      SpaceImage *sima = (SpaceImage *)drw_get().draw_ctx.space_data;
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

static void drw_task_graph_init()
{
  BLI_assert(drw_get().task_graph == nullptr);
  drw_get().task_graph = BLI_task_graph_create();
  drw_get().delayed_extraction = BLI_gset_ptr_new(__func__);
}

static void drw_task_graph_deinit()
{
  BLI_task_graph_work_and_wait(drw_get().task_graph);

  BLI_gset_free(drw_get().delayed_extraction,
                (void (*)(void *key))drw_batch_cache_generate_requested_evaluated_mesh_or_curve);
  drw_get().delayed_extraction = nullptr;
  BLI_task_graph_work_and_wait(drw_get().task_graph);

  BLI_task_graph_free(drw_get().task_graph);
  drw_get().task_graph = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Settings
 * \{ */

bool DRW_object_is_renderable(const Object *ob)
{
  BLI_assert((ob->base_flag & BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT) != 0);

  if (ob->type == OB_MESH) {
    if ((ob == drw_get().draw_ctx.object_edit) || ob->mode == OB_MODE_EDIT) {
      View3D *v3d = drw_get().draw_ctx.v3d;
      if (v3d && ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0) && RETOPOLOGY_ENABLED(v3d)) {
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

bool DRW_object_use_hide_faces(const Object *ob)
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Viewport (DRW_viewport)
 * \{ */

blender::float2 DRW_viewport_size_get()
{
  return blender::float2(drw_get().size);
}

/* Not a viewport variable, we could split this out. */
static void drw_context_state_init()
{
  if (drw_get().draw_ctx.obact) {
    drw_get().draw_ctx.object_mode = eObjectMode(drw_get().draw_ctx.obact->mode);
  }
  else {
    drw_get().draw_ctx.object_mode = OB_MODE_OBJECT;
  }

  /* Edit object. */
  if (drw_get().draw_ctx.object_mode & OB_MODE_EDIT) {
    drw_get().draw_ctx.object_edit = drw_get().draw_ctx.obact;
  }
  else {
    drw_get().draw_ctx.object_edit = nullptr;
  }

  /* Pose object. */
  if (drw_get().draw_ctx.object_mode & OB_MODE_POSE) {
    drw_get().draw_ctx.object_pose = drw_get().draw_ctx.obact;
  }
  else if (drw_get().draw_ctx.object_mode & OB_MODE_ALL_WEIGHT_PAINT) {
    drw_get().draw_ctx.object_pose = BKE_object_pose_armature_get(drw_get().draw_ctx.obact);
  }
  else {
    drw_get().draw_ctx.object_pose = nullptr;
  }

  drw_get().draw_ctx.sh_cfg = GPU_SHADER_CFG_DEFAULT;
  if (RV3D_CLIPPING_ENABLED(drw_get().draw_ctx.v3d, drw_get().draw_ctx.rv3d)) {
    drw_get().draw_ctx.sh_cfg = GPU_SHADER_CFG_CLIPPED;
  }
}

DRWData *DRW_viewport_data_create()
{
  DRWData *drw_data = static_cast<DRWData *>(MEM_callocN(sizeof(DRWData), "DRWData"));

  drw_data->default_view = new blender::draw::View("DrawDefaultView");

  for (int i = 0; i < 2; i++) {
    drw_data->view_data[i] = new DRWViewData();
  }
  return drw_data;
}

void DRWData::modules_init()
{
  using namespace blender::draw;
  DRW_pointcloud_init(this);
  DRW_curves_init(this);
  DRW_volume_init(this);
  DRW_smoke_init(this);
}

void DRWData::modules_exit()
{
  DRW_smoke_exit(this);
}

static void drw_viewport_data_reset(DRWData * /*drw_data*/)
{
  blender::gpu::TexturePool::get().reset();
}

void DRW_viewport_data_free(DRWData *drw_data)
{
  for (int i = 0; i < 2; i++) {
    delete drw_data->view_data[i];
  }
  DRW_volume_module_free(drw_data->volume_module);
  DRW_pointcloud_module_free(drw_data->pointcloud_module);
  DRW_curves_module_free(drw_data->curves_module);
  delete drw_data->default_view;
  MEM_freeN(drw_data);
}

static DRWData *drw_viewport_data_ensure(GPUViewport *viewport)
{
  DRWData **vmempool_p = GPU_viewport_data_get(viewport);
  DRWData *vmempool = *vmempool_p;

  if (vmempool == nullptr) {
    *vmempool_p = vmempool = DRW_viewport_data_create();
  }
  return vmempool;
}

/**
 * Sets drw_get().viewport, drw_get().size and a lot of other important variables.
 * Needs to be called before enabling any draw engine.
 * - viewport can be nullptr. In this case the data will not be stored and will be free at
 *   drw_manager_exit().
 * - size can be nullptr to get it from viewport.
 * - if viewport and size are nullptr, size is set to (1, 1).
 *
 * IMPORTANT: #drw_manager_init can be called multiple times before #drw_manager_exit.
 */
static void drw_manager_init(DRWContext *dst, GPUViewport *viewport, const int size[2])
{
  RegionView3D *rv3d = dst->draw_ctx.rv3d;
  ARegion *region = dst->draw_ctx.region;

  dst->in_progress = true;

  int view = (viewport) ? GPU_viewport_active_view_get(viewport) : 0;

  if (!dst->viewport && dst->data) {
    /* Manager was init first without a viewport, created DRWData, but is being re-init.
     * In this case, keep the old data. */
    /* If it is being re-init with a valid viewport, it means there is something wrong. */
    BLI_assert(viewport == nullptr);
  }
  else if (viewport) {
    /* Use viewport's persistent DRWData. */
    dst->data = drw_viewport_data_ensure(viewport);
  }
  else {
    /* Create temporary DRWData. Freed in drw_manager_exit(). */
    dst->data = DRW_viewport_data_create();
  }

  dst->viewport = viewport;
  dst->view_data_active = dst->data->view_data[view];

  drw_viewport_data_reset(dst->data);

  bool do_validation = true;
  if (size == nullptr && viewport == nullptr) {
    /* Avoid division by 0. Engines will either override this or not use it. */
    dst->size[0] = 1.0f;
    dst->size[1] = 1.0f;
  }
  else if (size == nullptr) {
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
    dst->view_data_active->texture_list_size_validate(int2(dst->size));
  }

  if (viewport) {
    DRW_view_data_default_lists_from_viewport(dst->view_data_active, viewport);
  }

  if (rv3d != nullptr) {
    blender::draw::View::default_set(float4x4(rv3d->viewmat), float4x4(rv3d->winmat));
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

    blender::draw::View::default_set(float4x4(viewmat), float4x4(winmat));
  }

  /* fclem: Is this still needed ? */
  if (dst->draw_ctx.object_edit && rv3d) {
    ED_view3d_init_mats_rv3d(dst->draw_ctx.object_edit, rv3d);
  }
}

static void drw_manager_exit(DRWContext *dst)
{
  if (dst->data != nullptr && dst->viewport == nullptr) {
    DRW_viewport_data_free(dst->data);
  }
  dst->data = nullptr;
  dst->viewport = nullptr;
  /* Avoid accidental reuse. */
  dst->state_ensure_not_reused();
  dst->in_progress = false;
}

DefaultFramebufferList *DRW_viewport_framebuffer_list_get()
{
  return DRW_view_data_default_framebuffer_list_get(drw_get().view_data_active);
}

DefaultTextureList *DRW_viewport_texture_list_get()
{
  return DRW_view_data_default_texture_list_get(drw_get().view_data_active);
}

blender::draw::TextureFromPool &DRW_viewport_pass_texture_get(const char *pass_name)
{
  return DRW_view_data_pass_texture_get(drw_get().view_data_active, pass_name);
}

void DRW_viewport_request_redraw()
{
  if (drw_get().viewport) {
    GPU_viewport_tag_update(drw_get().viewport);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplis
 * \{ */

/* The Dupli systems generate a lot of transient objects that share the batch caches.
 * So we ensure to only clear and generate the cache once per source instance type using this
 * set. */
/* TODO(fclem): This should be reconsidered as this has some unneeded overhead and complexity.
 * Maybe it isn't needed at all. */
struct DupliCacheManager {
 private:
  /* Key identifying a single instance source. */
  struct DupliKey {
    Object *ob = nullptr;
    ID *ob_data = nullptr;

    bool operator==(const DupliObject *ob_dupli)
    {
      return this->ob == ob_dupli->ob && this->ob_data == ob_dupli->ob_data;
    }

    uint64_t hash() const
    {
      return blender::get_default_hash(this->ob, this->ob_data);
    }

    friend bool operator==(const DupliKey &a, const DupliKey &b)
    {
      return a.ob == b.ob && a.ob_data == b.ob_data;
    }
  };

  /* Last key used. Allows to avoid the overhead of polling the `dupli_set` for each instance.
   * This helps when a Dupli system generates a lot of similar geometry consecutively. */
  DupliKey last_key_ = {};

  /* Set containing all visited Dupli source object. */
  blender::Set<DupliKey> *dupli_set_ = nullptr;

 public:
  void try_add(blender::draw::ObjectRef &ob_ref);
  void extract_all();
};

void DupliCacheManager::try_add(blender::draw::ObjectRef &ob_ref)
{
  if (ob_ref.is_dupli() == false) {
    return;
  }
  if (last_key_ == ob_ref.dupli_object) {
    /* Same data as previous iteration. No need to perform the check again. */
    return;
  }

  last_key_.ob = ob_ref.dupli_object->ob;
  last_key_.ob_data = ob_ref.dupli_object->ob_data;

  if (dupli_set_ == nullptr) {
    dupli_set_ = MEM_new<blender::Set<DupliKey>>("DupliCacheManager::dupli_set_");
  }

  if (dupli_set_->add(last_key_)) {
    /* Key is newly added. It is the first time we sync this object. */
    /* TODO: Meh a bit out of place but this is nice as it is
     * only done once per instance type. */
    /* Note that this can happen for geometry data whose type is different from the original
     * object (e.g. Text evaluated as Mesh, Geometry node instance etc...).
     * In this case, key.ob is not going to have the same data type as ob_ref.object nor the same
     * data at all. */
    drw_batch_cache_validate(ob_ref.object);
  }
}

void DupliCacheManager::extract_all()
{
  /* Reset for next iter. */
  last_key_ = {};

  if (dupli_set_ == nullptr) {
    return;
  }

  /* Note these can referenced by the temporary object pointer `Object *ob` and needs to have at
   * least the same lifetime. */
  blender::bke::ObjectRuntime tmp_runtime;
  Object tmp_object;

  using Iter = blender::Set<DupliKey>::Iterator;
  Iter begin = dupli_set_->begin();
  Iter end = dupli_set_->end();
  for (Iter iter = begin; iter != end; ++iter) {
    const DupliKey &key = *iter;
    Object *ob = iter->ob;

    if (key.ob_data != ob->data) {
      /* Copy both object data and runtime. */
      tmp_runtime = *ob->runtime;
      tmp_object = blender::dna::shallow_copy(*ob);
      tmp_object.runtime = &tmp_runtime;
      /* Geometry instances shouldn't be rendered with edit mode overlays. */
      tmp_object.mode = OB_MODE_OBJECT;
      /* Do not modify the original bound-box. */
      BKE_object_replace_data_on_shallow_copy(&tmp_object, key.ob_data);

      ob = &tmp_object;
    }

    drw_batch_cache_generate_requested(ob);
  }

  /* TODO(fclem): Could eventually keep the set allocated. */
  MEM_SAFE_DELETE(dupli_set_);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ViewLayers (DRW_scenelayer)
 * \{ */

void *DRW_view_layer_engine_data_get(DrawEngineType *engine_type)
{
  LISTBASE_FOREACH (ViewLayerEngineData *, sled, &drw_get().draw_ctx.view_layer->drawdata) {
    if (sled->engine_type == engine_type) {
      return sled->storage;
    }
  }
  return nullptr;
}

void **DRW_view_layer_engine_data_ensure_ex(ViewLayer *view_layer,
                                            DrawEngineType *engine_type,
                                            void (*callback)(void *storage))
{
  ViewLayerEngineData *sled;

  LISTBASE_FOREACH (ViewLayerEngineData *, sled, &view_layer->drawdata) {
    if (sled->engine_type == engine_type) {
      return &sled->storage;
    }
  }

  sled = static_cast<ViewLayerEngineData *>(
      MEM_callocN(sizeof(ViewLayerEngineData), "ViewLayerEngineData"));
  sled->engine_type = engine_type;
  sled->free = callback;
  BLI_addtail(&view_layer->drawdata, sled);

  return &sled->storage;
}

void **DRW_view_layer_engine_data_ensure(DrawEngineType *engine_type,
                                         void (*callback)(void *storage))
{
  return DRW_view_layer_engine_data_ensure_ex(
      drw_get().draw_ctx.view_layer, engine_type, callback);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Data (DRW_drawdata)
 * \{ */

/* Used for DRW_drawdata_from_id()
 * All ID-data-blocks which have their own 'local' DrawData
 * should have the same arrangement in their structs.
 */
struct IdDdtTemplate {
  ID id;
  AnimData *adt;
  DrawDataList drawdata;
};

/* Check if ID can have AnimData */
static bool id_type_can_have_drawdata(const short id_type)
{
  /* Only some ID-blocks have this info for now */
  /* TODO: finish adding this for the other block-types. */
  switch (id_type) {
    /* has DrawData */
    case ID_OB:
    case ID_WO:
    case ID_SCE:
    case ID_TE:
    case ID_MSK:
    case ID_MC:
    case ID_IM:
      return true;

    /* no DrawData */
    default:
      return false;
  }
}

static bool id_can_have_drawdata(const ID *id)
{
  /* sanity check */
  if (id == nullptr) {
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

  return nullptr;
}

DrawData *DRW_drawdata_get(ID *id, DrawEngineType *engine_type)
{
  DrawDataList *drawdata = DRW_drawdatalist_from_id(id);

  if (drawdata == nullptr) {
    return nullptr;
  }

  LISTBASE_FOREACH (DrawData *, dd, drawdata) {
    if (dd->engine_type == engine_type) {
      return dd;
    }
  }
  return nullptr;
}

DrawData *DRW_drawdata_ensure(ID *id,
                              DrawEngineType *engine_type,
                              size_t size,
                              DrawDataInitCb init_cb,
                              DrawDataFreeCb free_cb)
{
  BLI_assert(size >= sizeof(DrawData));
  BLI_assert(id_can_have_drawdata(id));
  BLI_assert_msg(
      GS(id->name) != ID_OB,
      "Objects should not use DrawData anymore. Use last_update instead for update detection");
  /* Try to re-use existing data. */
  DrawData *dd = DRW_drawdata_get(id, engine_type);
  if (dd != nullptr) {
    return dd;
  }

  DrawDataList *drawdata = DRW_drawdatalist_from_id(id);

  /* Allocate new data. */
  {
    dd = static_cast<DrawData *>(MEM_callocN(size, "DrawData"));
  }
  dd->engine_type = engine_type;
  dd->free = free_cb;
  /* Perform user-side initialization, if needed. */
  if (init_cb != nullptr) {
    init_cb(dd);
  }
  /* Register in the list. */
  BLI_addtail((ListBase *)drawdata, dd);
  return dd;
}

void DRW_drawdata_free(ID *id)
{
  DrawDataList *drawdata = DRW_drawdatalist_from_id(id);

  if (drawdata == nullptr) {
    return;
  }

  LISTBASE_FOREACH (DrawData *, dd, drawdata) {
    if (dd->free != nullptr) {
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

    if (drawdata == nullptr) {
      return;
    }

    BLI_listbase_clear((ListBase *)drawdata);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ObjectRef
 * \{ */

namespace blender::draw {

ObjectRef::ObjectRef(DEGObjectIterData &iter_data, Object *ob)
{
  this->dupli_parent = iter_data.dupli_parent;
  this->dupli_object = iter_data.dupli_object_current;
  this->object = ob;
  /* Set by the first drawcall. */
  this->handle = ResourceHandle(0);
}

ObjectRef::ObjectRef(Object *ob)
{
  this->dupli_parent = nullptr;
  this->dupli_object = nullptr;
  this->object = ob;
  /* Set by the first drawcall. */
  this->handle = ResourceHandle(0);
}

}  // namespace blender::draw

/** \} */

/* -------------------------------------------------------------------- */
/** \name Garbage Collection
 * \{ */

void DRW_cache_free_old_batches(Main *bmain)
{
  using namespace blender::draw;
  Scene *scene;
  static int lasttime = 0;
  int ctime = int(BLI_time_now_seconds());

  if (U.vbotimeout == 0 || (ctime - lasttime) < U.vbocollectrate || ctime == lasttime) {
    return;
  }

  lasttime = ctime;

  for (scene = static_cast<Scene *>(bmain->scenes.first); scene;
       scene = static_cast<Scene *>(scene->id.next))
  {
    LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
      Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer);
      if (depsgraph == nullptr) {
        continue;
      }

      /* TODO(fclem): This is not optimal since it iter over all dupli instances.
       * In this case only the source object should be tagged. */
      DEGObjectIterSettings deg_iter_settings = {nullptr};
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

static void drw_engines_init()
{
  DRWContext &ctx = drw_get();
  ctx.view_data_active->foreach_enabled_engine(
      [&](ViewportEngineData *data, DrawEngineType *engine) {
        if (engine->engine_init) {
          engine->engine_init(data);
        }
      });
}

static void drw_engines_cache_init()
{
  DRW_manager_begin_sync();

  DRWContext &ctx = drw_get();
  ctx.view_data_active->foreach_enabled_engine(
      [&](ViewportEngineData *data, DrawEngineType *engine) {
        if (data->text_draw_cache) {
          DRW_text_cache_destroy(data->text_draw_cache);
          data->text_draw_cache = nullptr;
        }
        if (drw_get().text_store_p == nullptr) {
          drw_get().text_store_p = &data->text_draw_cache;
        }

        if (engine->cache_init) {
          engine->cache_init(data);
        }
      });
}

static void drw_engines_world_update(Scene *scene)
{
  if (scene->world == nullptr) {
    return;
  }

  DRWContext &ctx = drw_get();
  ctx.view_data_active->foreach_enabled_engine(
      [&](ViewportEngineData *data, DrawEngineType *engine) {
        if (engine->id_update) {
          engine->id_update(data, &scene->world->id);
        }
      });
}

static void drw_engines_cache_populate(blender::draw::ObjectRef &ref)
{
  /* HACK: DrawData is copied by copy-on-eval from the duplicated object.
   * This is valid for IDs that cannot be instantiated but this
   * is not what we want in this case so we clear the pointer
   * ourselves here. */
  drw_drawdata_unlink_dupli((ID *)ref.object);

  /* Validation for dupli objects happen elsewhere. */
  if (ref.is_dupli() == false) {
    drw_batch_cache_validate(ref.object);
  }

  DRWContext &ctx = drw_get();
  ctx.view_data_active->foreach_enabled_engine(
      [&](ViewportEngineData *data, DrawEngineType *engine) {
        if (engine->cache_populate) {
          engine->cache_populate(data, ref);
        }
      });

  /* TODO: in the future it would be nice to generate once for all viewports.
   * But we need threaded DRW manager first. */
  if (ref.is_dupli() == false) {
    drw_batch_cache_generate_requested(ref.object);
  }

  /* ... and clearing it here too because this draw data is
   * from a mempool and must not be free individually by depsgraph. */
  drw_drawdata_unlink_dupli((ID *)ref.object);
}

static void drw_engines_cache_finish()
{
  DRWContext &ctx = drw_get();
  ctx.view_data_active->foreach_enabled_engine(
      [&](ViewportEngineData *data, DrawEngineType *engine) {
        if (engine->cache_finish) {
          engine->cache_finish(data);
        }
      });

  DRW_manager_end_sync();
}

static void drw_engines_draw_scene()
{
  DRWContext &ctx = drw_get();
  ctx.view_data_active->foreach_enabled_engine(
      [&](ViewportEngineData *data, DrawEngineType *engine) {
        if (engine->draw_scene) {
          GPU_debug_group_begin(engine->idname);
          engine->draw_scene(data);
          /* Restore for next engine */
          if (DRW_state_is_fbo()) {
            GPU_framebuffer_bind(ctx.default_framebuffer());
          }
          GPU_debug_group_end();
        }
      });
  /* Reset state after drawing */
  blender::draw::command::StateSet::set();
}

static void drw_engines_draw_text()
{
  DRWContext &ctx = drw_get();
  ctx.view_data_active->foreach_enabled_engine(
      [&](ViewportEngineData *data, DrawEngineType * /*engine*/) {
        if (data->text_draw_cache) {
          DRW_text_cache_draw(data->text_draw_cache, ctx.draw_ctx.region, ctx.draw_ctx.v3d);
        }
      });
}

void DRW_draw_region_engine_info(int xoffset, int *yoffset, int line_height)
{
  DRWContext &ctx = drw_get();
  ctx.view_data_active->foreach_enabled_engine(
      [&](ViewportEngineData *data, DrawEngineType * /*engine*/) {
        if (data->info[0] != '\0') {
          const char *buf_step = IFACE_(data->info);
          do {
            const char *buf = buf_step;
            buf_step = BLI_strchr_or_end(buf, '\n');
            const int buf_len = buf_step - buf;
            *yoffset -= line_height;
            BLF_draw_default(xoffset, *yoffset, 0.0f, buf, buf_len);
          } while (*buf_step ? ((void)buf_step++, true) : false);
        }
      });
}

static void use_drw_engine(DrawEngineType *engine)
{
  DRW_view_data_use_engine(drw_get().view_data_active, engine);
}

/* Gather all draw engines needed and store them in drw_get().view_data_active
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
      if (engine_type->draw_engine != nullptr) {
        use_drw_engine(engine_type->draw_engine);
      }
      else if ((engine_type->flag & RE_INTERNAL) == 0) {
        use_drw_engine(DRW_engine_viewport_external_type.draw_engine);
      }
      break;
  }
}

static void drw_engines_enable_overlays()
{
  use_drw_engine(&draw_engine_overlay_next_type);
}

static void drw_engine_enable_image_editor()
{
  if (DRW_engine_external_acquire_for_image_editor()) {
    use_drw_engine(&draw_engine_external_type);
  }
  else {
    use_drw_engine(&draw_engine_image_type);
  }

  use_drw_engine(&draw_engine_overlay_next_type);
}

static void drw_engines_enable_editors()
{
  SpaceLink *space_data = drw_get().draw_ctx.space_data;
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
      use_drw_engine(&draw_engine_overlay_next_type);
    }
  }
}

bool DRW_is_viewport_compositor_enabled()
{
  if (!drw_get().draw_ctx.v3d) {
    return false;
  }

  if (drw_get().draw_ctx.v3d->shading.use_compositor == V3D_SHADING_USE_COMPOSITOR_DISABLED) {
    return false;
  }

  if (!(drw_get().draw_ctx.v3d->shading.type >= OB_MATERIAL)) {
    return false;
  }

  if (!drw_get().draw_ctx.scene->use_nodes) {
    return false;
  }

  if (!drw_get().draw_ctx.scene->nodetree) {
    return false;
  }

  if (!drw_get().draw_ctx.rv3d) {
    return false;
  }

  if (drw_get().draw_ctx.v3d->shading.use_compositor == V3D_SHADING_USE_COMPOSITOR_CAMERA &&
      drw_get().draw_ctx.rv3d->persp != RV3D_CAMOB)
  {
    return false;
  }

  return true;
}

static void drw_engines_enable(ViewLayer * /*view_layer*/,
                               RenderEngineType *engine_type,
                               bool gpencil_engine_needed)
{
  View3D *v3d = drw_get().draw_ctx.v3d;
  const eDrawType drawtype = eDrawType(v3d->shading.type);
  const bool use_xray = XRAY_ENABLED(v3d);

  drw_engines_enable_from_engine(engine_type, drawtype);
  if (gpencil_engine_needed && ((drawtype >= OB_SOLID) || !use_xray)) {
    use_drw_engine(&draw_engine_gpencil_type);
  }

  if (DRW_is_viewport_compositor_enabled()) {
    use_drw_engine(&draw_engine_compositor_type);
  }

  drw_engines_enable_overlays();

#ifdef WITH_DRAW_DEBUG
  if (G.debug_value == 31) {
    use_drw_engine(&draw_engine_debug_select_type);
  }
#endif
}

static void drw_engines_disable()
{
  DRW_view_data_reset(drw_get().view_data_active);
}

static void drw_engines_data_validate()
{
  DRW_view_data_free_unused(drw_get().view_data_active);
}

/* Fast check to see if gpencil drawing engine is needed.
 * For slow exact check use `DRW_render_check_grease_pencil` */
static bool drw_gpencil_engine_needed(Depsgraph *depsgraph, View3D *v3d)
{
  const bool exclude_gpencil_rendering = v3d ? ((v3d->object_type_exclude_viewport &
                                                 (1 << OB_GREASE_PENCIL)) != 0) :
                                               false;
  return (!exclude_gpencil_rendering) && (DEG_id_type_any_exists(depsgraph, ID_GD_LEGACY) ||
                                          DEG_id_type_any_exists(depsgraph, ID_GP));
}

/* -------------------------------------------------------------------- */
/** \name Callbacks
 * \{ */

static void draw_callbacks_pre_scene()
{
  DRW_submission_start();

  RegionView3D *rv3d = drw_get().draw_ctx.rv3d;

  GPU_matrix_projection_set(rv3d->winmat);
  GPU_matrix_set(rv3d->viewmat);

  if (drw_get().draw_ctx.evil_C) {
    ED_region_draw_cb_draw(
        drw_get().draw_ctx.evil_C, drw_get().draw_ctx.region, REGION_DRAW_PRE_VIEW);
    /* Callback can be nasty and do whatever they want with the state.
     * Don't trust them! */
    blender::draw::command::StateSet::set();
  }
  DRW_submission_end();
}

static void draw_callbacks_post_scene()
{
  RegionView3D *rv3d = drw_get().draw_ctx.rv3d;
  ARegion *region = drw_get().draw_ctx.region;
  View3D *v3d = drw_get().draw_ctx.v3d;
  Depsgraph *depsgraph = drw_get().draw_ctx.depsgraph;

  const bool do_annotations = draw_show_annotation();

  DRW_submission_start();
  if (drw_get().draw_ctx.evil_C) {
    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

    blender::draw::command::StateSet::set();

    GPU_framebuffer_bind(dfbl->overlay_fb);

    GPU_matrix_projection_set(rv3d->winmat);
    GPU_matrix_set(rv3d->viewmat);

    /* annotations - temporary drawing buffer (3d space) */
    /* XXX: Or should we use a proper draw/overlay engine for this case? */
    if (do_annotations) {
      GPU_depth_test(GPU_DEPTH_NONE);
      /* XXX: as `scene->gpd` is not copied for copy-on-eval yet. */
      ED_annotation_draw_view3d(DEG_get_input_scene(depsgraph), depsgraph, v3d, region, true);
      GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
    }

    drw_debug_draw();

    GPU_depth_test(GPU_DEPTH_NONE);
    /* Apply state for callbacks. */
    GPU_apply_state();

    ED_region_draw_cb_draw(
        drw_get().draw_ctx.evil_C, drw_get().draw_ctx.region, REGION_DRAW_POST_VIEW);

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
    blender::draw::command::StateSet::set();

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
      /* XXX: as `scene->gpd` is not copied for copy-on-eval yet */
      ED_annotation_draw_view3d(DEG_get_input_scene(depsgraph), depsgraph, v3d, region, false);
    }

    if ((v3d->gizmo_flag & V3D_GIZMO_HIDE) == 0) {
      /* Draw 2D after region info so we can draw on top of the camera passepartout overlay.
       * 'DRW_draw_region_info' sets the projection in pixel-space. */
      GPU_depth_test(GPU_DEPTH_NONE);
      DRW_draw_gizmo_2d();
    }

    GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
  }
  else {
    if (v3d && ((v3d->flag2 & V3D_SHOW_ANNOTATION) != 0)) {
      GPU_depth_test(GPU_DEPTH_NONE);
      /* XXX: as `scene->gpd` is not copied for copy-on-eval yet */
      ED_annotation_draw_view3d(DEG_get_input_scene(depsgraph), depsgraph, v3d, region, true);
      GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
    }

#ifdef WITH_XR_OPENXR
    if ((v3d->flag & V3D_XR_SESSION_SURFACE) != 0) {
      DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

      blender::draw::command::StateSet::set();

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

        blender::draw::command::StateSet::set();
      }

      GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
    }
#endif
  }
  DRW_submission_end();
}

static void draw_callbacks_pre_scene_2D()
{
  DRW_submission_start();

  if (drw_get().draw_ctx.evil_C) {
    ED_region_draw_cb_draw(
        drw_get().draw_ctx.evil_C, drw_get().draw_ctx.region, REGION_DRAW_PRE_VIEW);
  }

  DRW_submission_end();
}

static void draw_callbacks_post_scene_2D(View2D &v2d)
{
  DRW_submission_start();

  const bool do_annotations = draw_show_annotation();
  const bool do_draw_gizmos = (drw_get().draw_ctx.space_data->spacetype != SPACE_IMAGE);

  if (drw_get().draw_ctx.evil_C) {
    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

    blender::draw::command::StateSet::set();

    GPU_framebuffer_bind(dfbl->overlay_fb);

    GPU_depth_test(GPU_DEPTH_NONE);
    GPU_matrix_push_projection();

    wmOrtho2(v2d.cur.xmin, v2d.cur.xmax, v2d.cur.ymin, v2d.cur.ymax);

    if (do_annotations) {
      ED_annotation_draw_view2d(drw_get().draw_ctx.evil_C, true);
    }

    GPU_depth_test(GPU_DEPTH_NONE);

    ED_region_draw_cb_draw(
        drw_get().draw_ctx.evil_C, drw_get().draw_ctx.region, REGION_DRAW_POST_VIEW);

    GPU_matrix_pop_projection();
    /* Callback can be nasty and do whatever they want with the state.
     * Don't trust them! */
    blender::draw::command::StateSet::set();

    GPU_depth_test(GPU_DEPTH_NONE);
    drw_engines_draw_text();

    if (do_annotations) {
      GPU_depth_test(GPU_DEPTH_NONE);
      ED_annotation_draw_view2d(drw_get().draw_ctx.evil_C, false);
    }
  }

  ED_region_pixelspace(drw_get().draw_ctx.region);

  if (do_draw_gizmos) {
    GPU_depth_test(GPU_DEPTH_NONE);
    DRW_draw_gizmo_2d();
  }

  DRW_submission_end();
}

DRWTextStore *DRW_text_cache_ensure()
{
  BLI_assert(drw_get().text_store_p);
  if (*drw_get().text_store_p == nullptr) {
    *drw_get().text_store_p = DRW_text_cache_create();
  }
  return *drw_get().text_store_p;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Draw Loops (DRW_draw)
 * \{ */

/**
 * Used for both regular and off-screen drawing.
 * The global `DRWContext` needs to be set before calling this function.
 */
static void DRW_draw_render_loop_3d(Depsgraph *depsgraph,
                                    RenderEngineType *engine_type,
                                    ARegion *region,
                                    View3D *v3d,
                                    GPUViewport *viewport,
                                    const bContext *evil_C)
{
  using namespace blender::draw;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  BKE_view_layer_synced_ensure(scene, view_layer);
  drw_get().draw_ctx = {};
  drw_get().draw_ctx.region = region;
  drw_get().draw_ctx.rv3d = rv3d;
  drw_get().draw_ctx.v3d = v3d;
  drw_get().draw_ctx.scene = scene;
  drw_get().draw_ctx.view_layer = view_layer;
  drw_get().draw_ctx.obact = BKE_view_layer_active_object_get(view_layer);
  drw_get().draw_ctx.engine_type = engine_type;
  drw_get().draw_ctx.depsgraph = depsgraph;

  /* reuse if caller sets */
  drw_get().draw_ctx.evil_C = evil_C;

  drw_task_graph_init();
  drw_context_state_init();

  drw_manager_init(g_context, viewport, nullptr);
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

  drw_debug_init();
  drw_get().data->modules_init();

  /* No frame-buffer allowed before drawing. */
  BLI_assert(GPU_framebuffer_active_get() == GPU_framebuffer_back_get());

  /* Init engines */
  drw_engines_init();

  /* Cache filling */
  {
    drw_engines_cache_init();
    drw_engines_world_update(scene);
    DupliCacheManager dupli_handler;

    /* Only iterate over objects for internal engines or when overlays are enabled */
    if (do_populate_loop) {
      DEGObjectIterSettings deg_iter_settings = {nullptr};
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
        blender::draw::ObjectRef ob_ref(data_, ob);
        dupli_handler.try_add(ob_ref);
        drw_engines_cache_populate(ob_ref);
      }
      DEG_OBJECT_ITER_END;
    }

    drw_engines_cache_finish();

    dupli_handler.extract_all();
    drw_task_graph_deinit();
  }

  GPU_framebuffer_bind(drw_get().default_framebuffer());

  /* Start Drawing */
  blender::draw::command::StateSet::set();

  GPU_framebuffer_bind(drw_get().default_framebuffer());
  GPU_framebuffer_clear_depth_stencil(drw_get().default_framebuffer(), 1.0f, 0xFF);

  DRW_curves_update(*DRW_manager_get());

  draw_callbacks_pre_scene();

  drw_engines_draw_scene();

  /* Fix 3D view "lagging" on APPLE and WIN32+NVIDIA. (See #56996, #61474) */
  if (GPU_type_matches_ex(GPU_DEVICE_ANY, GPU_OS_ANY, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL)) {
    GPU_flush();
  }

  drw_get().data->modules_exit();

  draw_callbacks_post_scene();

  if (WM_draw_region_get_bound_viewport(region)) {
    /* Don't unbind the frame-buffer yet in this case and let
     * GPU_viewport_unbind do it, so that we can still do further
     * drawing of action zones on top. */
  }
  else {
    GPU_framebuffer_restore();
  }

  blender::draw::command::StateSet::set();
  drw_engines_disable();
}

void DRW_draw_render_loop_offscreen(Depsgraph *depsgraph,
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
  if (viewport == nullptr) {
    render_viewport = GPU_viewport_create();
  }

  GPU_viewport_bind_from_offscreen(render_viewport, ofs, is_xr_surface);

  /* Just here to avoid an assert but shouldn't be required in practice. */
  GPU_framebuffer_restore();

  DRWContext draw_ctx;
  drw_set(draw_ctx);
  drw_get().options.is_image_render = is_image_render;
  drw_get().options.draw_background = draw_background;

  DRW_draw_render_loop_3d(depsgraph, engine_type, region, v3d, render_viewport, nullptr);

  drw_manager_exit(&draw_ctx);

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
  if (viewport == nullptr) {
    GPU_viewport_free(render_viewport);
  }
}

bool DRW_render_check_grease_pencil(Depsgraph *depsgraph)
{
  if (!drw_gpencil_engine_needed(depsgraph, nullptr)) {
    return false;
  }

  DEGObjectIterSettings deg_iter_settings = {nullptr};
  deg_iter_settings.depsgraph = depsgraph;
  deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
    if (ob->type == OB_GREASE_PENCIL) {
      if (BKE_object_visibility(ob, DAG_EVAL_RENDER) & OB_VISIBLE_SELF) {
        return true;
      }
    }
  }
  DEG_OBJECT_ITER_END;

  return false;
}

static void DRW_render_gpencil_to_image(RenderEngine *engine,
                                        RenderLayer *render_layer,
                                        const rcti *rect)
{
  DrawEngineType *draw_engine = &draw_engine_gpencil_type;

  if (draw_engine->render_to_image) {
    ViewportEngineData *gpdata = DRW_view_data_engine_data_get_ensure(drw_get().view_data_active,
                                                                      draw_engine);
    draw_engine->render_to_image(gpdata, engine, render_layer, rect);
  }
}

void DRW_render_gpencil(RenderEngine *engine, Depsgraph *depsgraph)
{
  /* This function should only be called if there are grease pencil objects,
   * especially important to avoid failing in background renders without GPU context. */
  BLI_assert(DRW_render_check_grease_pencil(depsgraph));

  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  RenderResult *render_result = RE_engine_get_result(engine);
  RenderLayer *render_layer = RE_GetRenderLayer(render_result, view_layer->name);
  if (render_layer == nullptr) {
    return;
  }

  RenderEngineType *engine_type = engine->type;
  Render *render = engine->re;

  DRW_render_context_enable(render);

  DRWContext draw_ctx;
  drw_set(draw_ctx);

  drw_get().options.is_image_render = true;
  drw_get().options.is_scene_render = true;
  drw_get().options.draw_background = scene->r.alphamode == R_ADDSKY;

  drw_get().draw_ctx = {};
  drw_get().draw_ctx.scene = scene;
  drw_get().draw_ctx.view_layer = view_layer;
  drw_get().draw_ctx.engine_type = engine_type;
  drw_get().draw_ctx.depsgraph = depsgraph;
  drw_get().draw_ctx.object_mode = OB_MODE_OBJECT;

  drw_context_state_init();

  const int size[2] = {engine->resolution_x, engine->resolution_y};

  drw_manager_init(g_context, nullptr, size);

  /* Main rendering. */
  rctf view_rect;
  rcti render_rect;
  RE_GetViewPlane(render, &view_rect, &render_rect);
  if (BLI_rcti_is_empty(&render_rect)) {
    BLI_rcti_init(&render_rect, 0, size[0], 0, size[1]);
  }

  for (RenderView *render_view = static_cast<RenderView *>(render_result->views.first);
       render_view != nullptr;
       render_view = render_view->next)
  {
    RE_SetActiveRenderView(render, render_view->name);
    DRW_render_gpencil_to_image(engine, render_layer, &render_rect);
  }

  blender::draw::command::StateSet::set();

  GPU_depth_test(GPU_DEPTH_NONE);

  blender::gpu::TexturePool::get().reset(true);
  drw_manager_exit(&draw_ctx);

  /* Restore Drawing area. */
  GPU_framebuffer_restore();

  DRW_render_context_disable(render);
}

void DRW_render_to_image(RenderEngine *engine, Depsgraph *depsgraph)
{
  using namespace blender::draw;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  RenderEngineType *engine_type = engine->type;
  DrawEngineType *draw_engine_type = engine_type->draw_engine;
  Render *render = engine->re;

  /* IMPORTANT: We don't support immediate mode in render mode!
   * This shall remain in effect until immediate mode supports
   * multiple threads. */

  DRWContext draw_ctx;

  drw_set(draw_ctx);
  drw_get().options.is_image_render = true;
  drw_get().options.is_scene_render = true;
  drw_get().options.draw_background = scene->r.alphamode == R_ADDSKY;
  drw_get().draw_ctx = {};
  drw_get().draw_ctx.scene = scene;
  drw_get().draw_ctx.view_layer = view_layer;
  drw_get().draw_ctx.engine_type = engine_type;
  drw_get().draw_ctx.depsgraph = depsgraph;
  drw_get().draw_ctx.object_mode = OB_MODE_OBJECT;

  drw_context_state_init();

  /* Begin GPU workload Boundary */
  GPU_render_begin();

  const int size[2] = {engine->resolution_x, engine->resolution_y};

  drw_manager_init(g_context, nullptr, size);

  ViewportEngineData *data = DRW_view_data_engine_data_get_ensure(drw_get().view_data_active,
                                                                  draw_engine_type);

  /* Main rendering. */
  rctf view_rect;
  rcti render_rect;
  RE_GetViewPlane(render, &view_rect, &render_rect);
  if (BLI_rcti_is_empty(&render_rect)) {
    BLI_rcti_init(&render_rect, 0, size[0], 0, size[1]);
  }

  /* Reset state before drawing */
  blender::draw::command::StateSet::set();

  /* set default viewport */
  GPU_viewport(0, 0, size[0], size[1]);

  /* Init render result. */
  RenderResult *render_result = RE_engine_begin_result(engine,
                                                       0,
                                                       0,
                                                       size[0],
                                                       size[1],
                                                       view_layer->name,
                                                       /*RR_ALL_VIEWS*/ nullptr);
  RenderLayer *render_layer = static_cast<RenderLayer *>(render_result->layers.first);
  for (RenderView *render_view = static_cast<RenderView *>(render_result->views.first);
       render_view != nullptr;
       render_view = render_view->next)
  {
    RE_SetActiveRenderView(render, render_view->name);
    engine_type->draw_engine->render_to_image(data, engine, render_layer, &render_rect);
  }

  RE_engine_end_result(engine, render_result, false, false, false);

  if (engine_type->draw_engine->store_metadata) {
    RenderResult *final_render_result = RE_engine_get_result(engine);
    engine_type->draw_engine->store_metadata(data, final_render_result);
  }

  GPU_framebuffer_restore();

  drw_get().data->modules_exit();

  blender::gpu::TexturePool::get().reset(true);

  /* Reset state after drawing */
  blender::draw::command::StateSet::set();

  drw_manager_exit(&draw_ctx);
  DRW_cache_free_old_subdiv();

  /* End GPU workload Boundary */
  GPU_render_end();
}

void DRW_render_object_iter(void *vedata,
                            RenderEngine *engine,
                            Depsgraph *depsgraph,
                            void (*callback)(void *vedata,
                                             blender::draw::ObjectRef &ob_ref,
                                             RenderEngine *engine,
                                             Depsgraph *depsgraph))
{
  using namespace blender::draw;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  drw_get().data->modules_init();

  DupliCacheManager dupli_handler;

  drw_task_graph_init();
  const int object_type_exclude_viewport = draw_ctx->v3d ?
                                               draw_ctx->v3d->object_type_exclude_viewport :
                                               0;
  DEGObjectIterSettings deg_iter_settings = {nullptr};
  deg_iter_settings.depsgraph = depsgraph;
  deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
    if ((object_type_exclude_viewport & (1 << ob->type)) == 0) {
      blender::draw::ObjectRef ob_ref(data_, ob);
      dupli_handler.try_add(ob_ref);

      if (ob_ref.is_dupli() == false) {
        drw_batch_cache_validate(ob);
      }
      callback(vedata, ob_ref, engine, depsgraph);
      if (ob_ref.is_dupli() == false) {
        drw_batch_cache_generate_requested(ob);
      }
    }
  }
  DEG_OBJECT_ITER_END;

  dupli_handler.extract_all();
  drw_task_graph_deinit();
}

void DRW_custom_pipeline_begin(DRWContext &draw_ctx,
                               DrawEngineType *draw_engine_type,
                               Depsgraph *depsgraph)
{
  using namespace blender::draw;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);

  drw_set(draw_ctx);
  drw_get().options.is_image_render = true;
  drw_get().options.is_scene_render = true;
  drw_get().options.draw_background = false;

  drw_get().draw_ctx = {};
  drw_get().draw_ctx.scene = scene;
  drw_get().draw_ctx.view_layer = view_layer;
  drw_get().draw_ctx.engine_type = nullptr;
  drw_get().draw_ctx.depsgraph = depsgraph;
  drw_get().draw_ctx.object_mode = OB_MODE_OBJECT;

  drw_context_state_init();

  drw_manager_init(g_context, nullptr, nullptr);

  drw_get().data->modules_init();

  DRW_view_data_engine_data_get_ensure(drw_get().view_data_active, draw_engine_type);
}

void DRW_custom_pipeline_end(DRWContext &draw_ctx)
{
  drw_get().data->modules_exit();

  GPU_framebuffer_restore();

  /* The use of custom pipeline in other thread using the same
   * resources as the main thread (viewport) may lead to data
   * races and undefined behavior on certain drivers. Using
   * GPU_finish to sync seems to fix the issue. (see #62997) */
  eGPUBackendType type = GPU_backend_get_type();
  if (type == GPU_BACKEND_OPENGL) {
    GPU_finish();
  }

  blender::gpu::TexturePool::get().reset(true);
  drw_manager_exit(&draw_ctx);
}

void DRW_cache_restart()
{
  using namespace blender::draw;
  drw_get().data->modules_exit();

  drw_manager_init(g_context,
                   drw_get().viewport,
                   blender::int2{int(drw_get().size[0]), int(drw_get().size[1])});

  drw_get().data->modules_init();
}

static void DRW_draw_render_loop_2d(Depsgraph *depsgraph,
                                    ARegion *region,
                                    GPUViewport *viewport,
                                    const bContext *evil_C)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);

  BKE_view_layer_synced_ensure(scene, view_layer);
  drw_get().draw_ctx = {};
  drw_get().draw_ctx.region = region;
  drw_get().draw_ctx.scene = scene;
  drw_get().draw_ctx.view_layer = view_layer;
  drw_get().draw_ctx.obact = BKE_view_layer_active_object_get(view_layer);
  drw_get().draw_ctx.depsgraph = depsgraph;
  drw_get().draw_ctx.space_data = CTX_wm_space_data(evil_C);

  /* reuse if caller sets */
  drw_get().draw_ctx.evil_C = evil_C;

  drw_context_state_init();
  drw_manager_init(g_context, viewport, nullptr);
  DRW_viewport_colormanagement_set(viewport);

  /* TODO(jbakker): Only populate when editor needs to draw object.
   * for the image editor this is when showing UVs. */
  const bool do_populate_loop = (drw_get().draw_ctx.space_data->spacetype == SPACE_IMAGE);

  /* Get list of enabled engines */
  drw_engines_enable_editors();
  drw_engines_data_validate();

  drw_debug_init();

  /* No frame-buffer allowed before drawing. */
  BLI_assert(GPU_framebuffer_active_get() == GPU_framebuffer_back_get());
  GPU_framebuffer_bind(drw_get().default_framebuffer());
  GPU_framebuffer_clear_depth_stencil(drw_get().default_framebuffer(), 1.0f, 0xFF);

  /* Init engines */
  drw_engines_init();
  drw_task_graph_init();

  /* Cache filling */
  {
    drw_engines_cache_init();

    /* Only iterate over objects when overlay uses object data. */
    if (do_populate_loop) {
      DEGObjectIterSettings deg_iter_settings = {nullptr};
      deg_iter_settings.depsgraph = depsgraph;
      deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
      DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
        blender::draw::ObjectRef ob_ref(ob);
        drw_engines_cache_populate(ob_ref);
      }
      DEG_OBJECT_ITER_END;
    }

    drw_engines_cache_finish();
  }
  drw_task_graph_deinit();

  GPU_framebuffer_bind(drw_get().default_framebuffer());

  /* Start Drawing */
  blender::draw::command::StateSet::set();

  draw_callbacks_pre_scene_2D();

  drw_engines_draw_scene();

  /* Fix 3D view being "laggy" on MACOS and MS-Windows+NVIDIA. (See #56996, #61474) */
  if (GPU_type_matches_ex(GPU_DEVICE_ANY, GPU_OS_ANY, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL)) {
    GPU_flush();
  }

  draw_callbacks_post_scene_2D(region->v2d);

  GPU_depth_test(GPU_DEPTH_LESS_EQUAL);

  if (WM_draw_region_get_bound_viewport(region)) {
    /* Don't unbind the frame-buffer yet in this case and let
     * GPU_viewport_unbind do it, so that we can still do further
     * drawing of action zones on top. */
  }
  else {
    GPU_framebuffer_restore();
  }

  blender::draw::command::StateSet::set();
  drw_engines_disable();
}

void DRW_draw_view(const bContext *C)
{
  Depsgraph *depsgraph = CTX_data_expect_evaluated_depsgraph(C);
  ARegion *region = CTX_wm_region(C);
  GPUViewport *viewport = WM_draw_region_get_bound_viewport(region);

  DRWContext draw_ctx;
  drw_set(draw_ctx);

  View3D *v3d = CTX_wm_view3d(C);

  if (v3d) {
    Scene *scene = DEG_get_evaluated_scene(depsgraph);
    RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->shading.type);

    drw_get().options.draw_text = ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0 &&
                                   (v3d->overlay.flag & V3D_OVERLAY_HIDE_TEXT) != 0);
    drw_get().options.draw_background = (scene->r.alphamode == R_ADDSKY) ||
                                        (v3d->shading.type != OB_RENDER);

    DRW_draw_render_loop_3d(depsgraph, engine_type, region, v3d, viewport, C);
  }
  else {
    DRW_draw_render_loop_2d(depsgraph, region, viewport, C);
  }

  drw_manager_exit(&draw_ctx);
}

static struct DRWSelectBuffer {
  GPUFrameBuffer *framebuffer_depth_only;
  GPUTexture *texture_depth;
} g_select_buffer = {nullptr};

static void draw_select_framebuffer_depth_only_setup(const int size[2])
{
  if (g_select_buffer.framebuffer_depth_only == nullptr) {
    g_select_buffer.framebuffer_depth_only = GPU_framebuffer_create("framebuffer_depth_only");
  }

  if ((g_select_buffer.texture_depth != nullptr) &&
      ((GPU_texture_width(g_select_buffer.texture_depth) != size[0]) ||
       (GPU_texture_height(g_select_buffer.texture_depth) != size[1])))
  {
    GPU_texture_free(g_select_buffer.texture_depth);
    g_select_buffer.texture_depth = nullptr;
  }

  if (g_select_buffer.texture_depth == nullptr) {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    g_select_buffer.texture_depth = GPU_texture_create_2d(
        "select_depth", size[0], size[1], 1, GPU_DEPTH_COMPONENT24, usage, nullptr);

    GPU_framebuffer_texture_attach(
        g_select_buffer.framebuffer_depth_only, g_select_buffer.texture_depth, 0, 0);

    GPU_framebuffer_check_valid(g_select_buffer.framebuffer_depth_only, nullptr);
  }
}

void DRW_render_set_time(RenderEngine *engine, Depsgraph *depsgraph, int frame, float subframe)
{
  RE_engine_frame_set(engine, frame, subframe);
  drw_get().draw_ctx.scene = DEG_get_evaluated_scene(depsgraph);
  drw_get().draw_ctx.view_layer = DEG_get_evaluated_view_layer(depsgraph);
}

void DRW_draw_select_loop(Depsgraph *depsgraph,
                          ARegion *region,
                          View3D *v3d,
                          bool use_obedit_skip,
                          bool draw_surface,
                          bool /*use_nearest*/,
                          const bool do_material_sub_selection,
                          const rcti *rect,
                          DRW_SelectPassFn select_pass_fn,
                          void *select_pass_user_data,
                          DRW_ObjectFilterFn object_filter_fn,
                          void *object_filter_user_data)
{
  using namespace blender::draw;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->shading.type);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);

  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  Object *obedit = use_obedit_skip ? nullptr : OBEDIT_FROM_OBACT(obact);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  DRWContext draw_ctx;
  drw_set(draw_ctx);

  bool use_obedit = false;
  /* obedit_ctx_mode is used for selecting the right draw engines */
  // eContextObjectMode obedit_ctx_mode;
  /* object_mode is used for filtering objects in the depsgraph */
  eObjectMode object_mode;
  int object_type = 0;
  if (obedit != nullptr) {
    object_type = obedit->type;
    object_mode = eObjectMode(obedit->mode);
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
      if (obpose == nullptr) {
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
        object_mode = eObjectMode(obpose->mode);
        // obedit_ctx_mode = CTX_MODE_POSE;
      }
    }
  }

  /* Instead of 'DRW_context_state_init(C, &drw_get().draw_ctx)', assign from args */
  drw_get().draw_ctx = {};
  drw_get().draw_ctx.region = region;
  drw_get().draw_ctx.rv3d = rv3d;
  drw_get().draw_ctx.v3d = v3d;
  drw_get().draw_ctx.scene = scene;
  drw_get().draw_ctx.view_layer = view_layer;
  drw_get().draw_ctx.obact = obact;
  drw_get().draw_ctx.engine_type = engine_type;
  drw_get().draw_ctx.depsgraph = depsgraph;

  drw_context_state_init();

  const int viewport_size[2] = {BLI_rcti_size_x(rect), BLI_rcti_size_y(rect)};
  drw_manager_init(g_context, nullptr, viewport_size);

  drw_get().options.is_select = true;
  drw_get().options.is_material_select = do_material_sub_selection;
  drw_task_graph_init();
  /* Get list of enabled engines */
  use_drw_engine(&draw_engine_select_next_type);
  if (use_obedit) {
    /* Noop. */
  }
  else if (!draw_surface) {
    /* grease pencil selection */
    if (drw_gpencil_engine_needed(depsgraph, v3d)) {
      use_drw_engine(&draw_engine_gpencil_type);
    }
  }
  drw_engines_data_validate();

  /* Init engines */
  drw_engines_init();
  drw_get().data->modules_init();

  {
    drw_engines_cache_init();
    drw_engines_world_update(scene);
    DupliCacheManager dupli_handler;

    if (use_obedit) {
      FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, object_type, object_mode, ob_iter) {
        blender::draw::ObjectRef ob_ref(ob_iter);
        drw_engines_cache_populate(ob_ref);
      }
      FOREACH_OBJECT_IN_MODE_END;
    }
    else {
      /* When selecting pose-bones in pose mode, check for visibility not select-ability
       * as pose-bones have their own selection restriction flag. */
      const bool use_pose_exception = (drw_get().draw_ctx.object_pose != nullptr);

      const int object_type_exclude_select = (v3d->object_type_exclude_viewport |
                                              v3d->object_type_exclude_select);
      bool filter_exclude = false;
      DEGObjectIterSettings deg_iter_settings = {nullptr};
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
          if (object_filter_fn != nullptr) {
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

          blender::draw::ObjectRef ob_ref(data_, ob);
          dupli_handler.try_add(ob_ref);
          drw_engines_cache_populate(ob_ref);
        }
      }
      DEG_OBJECT_ITER_END;
    }

    dupli_handler.extract_all();
    drw_task_graph_deinit();
    drw_engines_cache_finish();
  }

  /* Setup frame-buffer. */
  draw_select_framebuffer_depth_only_setup(viewport_size);
  GPU_framebuffer_bind(g_select_buffer.framebuffer_depth_only);
  GPU_framebuffer_clear_depth(g_select_buffer.framebuffer_depth_only, 1.0f);
  /* WORKAROUND: Needed for Select-Next for keeping the same code-flow as Overlay-Next. */
  /* TODO(pragma37): Some engines retrieve the depth texture before this point (See #132922).
   * Check with @fclem. */
  BLI_assert(DRW_viewport_texture_list_get()->depth == nullptr);
  DRW_viewport_texture_list_get()->depth = g_select_buffer.texture_depth;

  /* Start Drawing */
  blender::draw::command::StateSet::set();
  draw_callbacks_pre_scene();

  DRW_curves_update(*DRW_manager_get());

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

  drw_get().data->modules_exit();

  /* WORKAROUND: Do not leave ownership to the viewport list. */
  DRW_viewport_texture_list_get()->depth = nullptr;

  blender::draw::command::StateSet::set();
  drw_engines_disable();

  drw_manager_exit(&draw_ctx);

  GPU_framebuffer_restore();
}

void DRW_draw_depth_loop(Depsgraph *depsgraph,
                         ARegion *region,
                         View3D *v3d,
                         GPUViewport *viewport,
                         const bool use_gpencil,
                         const bool use_only_selected,
                         const bool use_only_active_object)
{
  using namespace blender::draw;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->shading.type);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  DRWContext draw_ctx;
  drw_set(draw_ctx);

  drw_get().options.is_depth = true;

  /* Instead of 'DRW_context_state_init(C, &drw_get().draw_ctx)', assign from args */
  BKE_view_layer_synced_ensure(scene, view_layer);
  drw_get().draw_ctx = {};
  drw_get().draw_ctx.region = region;
  drw_get().draw_ctx.rv3d = rv3d;
  drw_get().draw_ctx.v3d = v3d;
  drw_get().draw_ctx.scene = scene;
  drw_get().draw_ctx.view_layer = view_layer;
  drw_get().draw_ctx.obact = BKE_view_layer_active_object_get(view_layer);
  drw_get().draw_ctx.engine_type = engine_type;
  drw_get().draw_ctx.depsgraph = depsgraph;

  drw_context_state_init();
  drw_manager_init(g_context, viewport, nullptr);

  if (use_gpencil) {
    use_drw_engine(&draw_engine_gpencil_type);
  }
  drw_engines_enable_overlays();

  drw_task_graph_init();

  /* Setup frame-buffer. */
  GPUTexture *depth_tx = GPU_viewport_depth_texture(viewport);

  GPUFrameBuffer *depth_fb = nullptr;
  GPU_framebuffer_ensure_config(&depth_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(depth_tx),
                                    GPU_ATTACHMENT_NONE,
                                });

  GPU_framebuffer_bind(depth_fb);
  GPU_framebuffer_clear_depth(depth_fb, 1.0f);

  /* Init engines */
  drw_engines_init();
  drw_get().data->modules_init();

  {
    drw_engines_cache_init();
    drw_engines_world_update(drw_get().draw_ctx.scene);

    const int object_type_exclude_viewport = v3d->object_type_exclude_viewport;
    DEGObjectIterSettings deg_iter_settings = {nullptr};
    deg_iter_settings.depsgraph = drw_get().draw_ctx.depsgraph;
    deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
    if (v3d->flag2 & V3D_SHOW_VIEWER) {
      deg_iter_settings.viewer_path = &v3d->viewer_path;
    }
    if (use_only_active_object) {
      blender::draw::ObjectRef ob_ref(drw_get().draw_ctx.obact);
      drw_engines_cache_populate(ob_ref);
    }
    else {
      DupliCacheManager dupli_handler;
      DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
        if ((object_type_exclude_viewport & (1 << ob->type)) != 0) {
          continue;
        }
        if (!BKE_object_is_visible_in_viewport(v3d, ob)) {
          continue;
        }
        if (use_only_selected && !(ob->base_flag & BASE_SELECTED)) {
          continue;
        }
        if ((ob->base_flag & BASE_SELECTABLE) == 0) {
          continue;
        }
        blender::draw::ObjectRef ob_ref(data_, ob);
        dupli_handler.try_add(ob_ref);
        drw_engines_cache_populate(ob_ref);
      }
      DEG_OBJECT_ITER_END;
      dupli_handler.extract_all();
    }

    drw_engines_cache_finish();

    drw_task_graph_deinit();
  }

  /* Start Drawing */
  blender::draw::command::StateSet::set();

  DRW_curves_update(*DRW_manager_get());

  drw_engines_draw_scene();

  drw_get().data->modules_exit();

  blender::draw::command::StateSet::set();

  /* TODO: Reading depth for operators should be done here. */

  GPU_framebuffer_restore();
  GPU_framebuffer_free(depth_fb);

  drw_engines_disable();

  drw_manager_exit(&draw_ctx);
}

void DRW_draw_select_id(Depsgraph *depsgraph, ARegion *region, View3D *v3d)
{
  SELECTID_Context *sel_ctx = DRW_select_engine_context_get();
  GPUViewport *viewport = WM_draw_region_get_viewport(region);
  if (!viewport) {
    /* Selection engine requires a viewport.
     * TODO(@germano): This should be done internally in the engine. */
    sel_ctx->max_index_drawn_len = 1;
    return;
  }

  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  DRWContext draw_ctx;
  drw_set(draw_ctx);

  /* Instead of 'DRW_context_state_init(C, &drw_get().draw_ctx)', assign from args */
  BKE_view_layer_synced_ensure(scene, view_layer);
  drw_get().draw_ctx = {};
  drw_get().draw_ctx.region = region;
  drw_get().draw_ctx.rv3d = rv3d;
  drw_get().draw_ctx.v3d = v3d;
  drw_get().draw_ctx.scene = scene;
  drw_get().draw_ctx.view_layer = view_layer;
  drw_get().draw_ctx.obact = BKE_view_layer_active_object_get(view_layer);
  drw_get().draw_ctx.depsgraph = depsgraph;

  drw_task_graph_init();
  drw_context_state_init();

  drw_manager_init(g_context, viewport, nullptr);

  /* Make sure select engine gets the correct vertex size. */
  UI_SetTheme(SPACE_VIEW3D, RGN_TYPE_WINDOW);

  /* Select Engine */
  use_drw_engine(&draw_engine_select_type);
  drw_engines_init();
  {
    drw_engines_cache_init();

    for (Object *obj_eval : sel_ctx->objects) {
      blender::draw::ObjectRef ob_ref(obj_eval);
      drw_engines_cache_populate(ob_ref);
    }

    if (RETOPOLOGY_ENABLED(v3d) && !XRAY_ENABLED(v3d)) {
      DEGObjectIterSettings deg_iter_settings = {nullptr};
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
        blender::draw::ObjectRef ob_ref(data_, ob);
        drw_engines_cache_populate(ob_ref);
      }
      DEG_OBJECT_ITER_END;
    }

    drw_engines_cache_finish();

    drw_task_graph_deinit();
  }

  /* Start Drawing */
  blender::draw::command::StateSet::set();
  drw_engines_draw_scene();
  blender::draw::command::StateSet::set();

  drw_engines_disable();

  drw_manager_exit(&draw_ctx);
}

bool DRW_draw_in_progress()
{
  return drw_get().in_progress;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Manager State (DRW_state)
 * \{ */

bool DRW_state_is_fbo()
{
  return ((drw_get().default_framebuffer() != nullptr) || drw_get().options.is_image_render) &&
         !DRW_state_is_depth() && !DRW_state_is_select();
}

bool DRW_state_is_select()
{
  return drw_get().options.is_select;
}

bool DRW_state_is_material_select()
{
  return drw_get().options.is_material_select;
}

bool DRW_state_is_depth()
{
  return drw_get().options.is_depth;
}

bool DRW_state_is_image_render()
{
  return drw_get().options.is_image_render;
}

bool DRW_state_is_scene_render()
{
  BLI_assert(drw_get().options.is_scene_render ? drw_get().options.is_image_render : true);
  return drw_get().options.is_scene_render;
}

bool DRW_state_is_viewport_image_render()
{
  return drw_get().options.is_image_render && !drw_get().options.is_scene_render;
}

bool DRW_state_is_playback()
{
  if (drw_get().draw_ctx.evil_C != nullptr) {
    wmWindowManager *wm = CTX_wm_manager(drw_get().draw_ctx.evil_C);
    return ED_screen_animation_playing(wm) != nullptr;
  }
  return false;
}

bool DRW_state_is_navigating()
{
  const RegionView3D *rv3d = drw_get().draw_ctx.rv3d;
  return (rv3d) && (rv3d->rflag & (RV3D_NAVIGATING | RV3D_PAINTING));
}

bool DRW_state_is_painting()
{
  const RegionView3D *rv3d = drw_get().draw_ctx.rv3d;
  return (rv3d) && (rv3d->rflag & (RV3D_PAINTING));
}

bool DRW_state_show_text()
{
  return (drw_get().options.is_select) == 0 && (drw_get().options.is_depth) == 0 &&
         (drw_get().options.is_scene_render) == 0 && (drw_get().options.draw_text) == 0;
}

bool DRW_state_draw_support()
{
  View3D *v3d = drw_get().draw_ctx.v3d;
  return (DRW_state_is_scene_render() == false) && (v3d != nullptr) &&
         ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0);
}

bool DRW_state_draw_background()
{
  return drw_get().options.draw_background;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context State (DRW_context_state)
 * \{ */

const DRWContextState *DRW_context_state_get()
{
  return &drw_get().draw_ctx;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Init/Exit (DRW_engines)
 * \{ */

bool DRW_engine_render_support(DrawEngineType *draw_engine_type)
{
  return draw_engine_type->render_to_image;
}

void DRW_engines_register()
{
  using namespace blender::draw;
  RE_engines_register(&DRW_engine_viewport_eevee_next_type);

  RE_engines_register(&DRW_engine_viewport_workbench_type);

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

    BKE_curves_batch_cache_dirty_tag_cb = DRW_curves_batch_cache_dirty_tag;
    BKE_curves_batch_cache_free_cb = DRW_curves_batch_cache_free;

    BKE_pointcloud_batch_cache_dirty_tag_cb = DRW_pointcloud_batch_cache_dirty_tag;
    BKE_pointcloud_batch_cache_free_cb = DRW_pointcloud_batch_cache_free;

    BKE_volume_batch_cache_dirty_tag_cb = DRW_volume_batch_cache_dirty_tag;
    BKE_volume_batch_cache_free_cb = DRW_volume_batch_cache_free;

    BKE_grease_pencil_batch_cache_dirty_tag_cb = DRW_grease_pencil_batch_cache_dirty_tag;
    BKE_grease_pencil_batch_cache_free_cb = DRW_grease_pencil_batch_cache_free;

    BKE_subsurf_modifier_free_gpu_cache_cb = DRW_subdiv_cache_free;
  }
}

void DRW_engines_free()
{
  using namespace blender::draw;

  DRW_engine_viewport_eevee_next_type.draw_engine->engine_free();
  DRW_engine_viewport_workbench_type.draw_engine->engine_free();
  draw_engine_gpencil_type.engine_free();
  draw_engine_image_type.engine_free();
  draw_engine_overlay_next_type.engine_free();
#ifdef WITH_DRAW_DEBUG
  draw_engine_debug_select_type.engine_free();
#endif
  draw_engine_select_type.engine_free();

  if (system_gpu_context == nullptr) {
    /* Nothing has been setup. Nothing to clear.
     * Otherwise, DRW_gpu_context_enable can
     * create a context in background mode. (see #62355) */
    return;
  }

  DRW_gpu_context_enable();

  GPU_TEXTURE_FREE_SAFE(g_select_buffer.texture_depth);
  GPU_FRAMEBUFFER_FREE_SAFE(g_select_buffer.framebuffer_depth_only);

  DRW_shaders_free();

  DRW_gpu_context_disable();
}

void DRW_render_context_enable(Render *render)
{
  if (G.background && system_gpu_context == nullptr) {
    WM_init_gpu();
  }

  GPU_render_begin();

  if (GPU_use_main_context_workaround()) {
    GPU_context_main_lock();
    DRW_gpu_context_enable();
    return;
  }

  void *re_system_gpu_context = RE_system_gpu_context_get(render);

  /* Changing Context */
  if (re_system_gpu_context != nullptr) {
    DRW_system_gpu_render_context_enable(re_system_gpu_context);
    /* We need to query gpu context after a gl context has been bound. */
    void *re_blender_gpu_context = RE_blender_gpu_context_ensure(render);
    DRW_blender_gpu_render_context_enable(re_blender_gpu_context);
  }
  else {
    DRW_gpu_context_enable();
  }
}

void DRW_render_context_disable(Render *render)
{
  if (GPU_use_main_context_workaround()) {
    DRW_gpu_context_disable();
    GPU_render_end();
    GPU_context_main_unlock();
    return;
  }

  void *re_system_gpu_context = RE_system_gpu_context_get(render);

  if (re_system_gpu_context != nullptr) {
    void *re_blender_gpu_context = RE_blender_gpu_context_ensure(render);
    /* GPU rendering may occur during context disable. */
    DRW_blender_gpu_render_context_disable(re_blender_gpu_context);
    GPU_render_end();
    DRW_system_gpu_render_context_disable(re_system_gpu_context);
  }
  else {
    DRW_gpu_context_disable();
    GPU_render_end();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Init/Exit (DRW_gpu_ctx)
 * \{ */

void DRW_gpu_context_create()
{
  BLI_assert(system_gpu_context == nullptr); /* Ensure it's called once */

  /* Setup compilation context. Called first as it changes the active GPUContext. */
  DRW_shader_init();

  system_gpu_context_mutex = BLI_ticket_mutex_alloc();
  submission_mutex = BLI_ticket_mutex_alloc();
  /* This changes the active context. */
  system_gpu_context = WM_system_gpu_context_create();
  WM_system_gpu_context_activate(system_gpu_context);
  /* Be sure to create blender_gpu_context too. */
  blender_gpu_context = GPU_context_create(nullptr, system_gpu_context);
  /* Some part of the code assumes no context is left bound. */
  GPU_context_active_set(nullptr);
  WM_system_gpu_context_release(system_gpu_context);
  /* Activate the window's context if any. */
  wm_window_reset_drawable();
}

void DRW_gpu_context_destroy()
{
  BLI_assert(BLI_thread_is_main());
  if (system_gpu_context != nullptr) {
    DRW_shader_exit();
    WM_system_gpu_context_activate(system_gpu_context);
    GPU_context_active_set(blender_gpu_context);
    GPU_context_discard(blender_gpu_context);
    WM_system_gpu_context_dispose(system_gpu_context);
    BLI_ticket_mutex_free(submission_mutex);
    BLI_ticket_mutex_free(system_gpu_context_mutex);
  }
}

void DRW_submission_start()
{
  bool locked = BLI_ticket_mutex_lock_check_recursive(submission_mutex);
  BLI_assert(locked);
  UNUSED_VARS_NDEBUG(locked);
}

void DRW_submission_end()
{
  BLI_ticket_mutex_unlock(submission_mutex);
}

void DRW_gpu_context_enable_ex(bool /*restore*/)
{
  if (system_gpu_context != nullptr) {
    /* IMPORTANT: We don't support immediate mode in render mode!
     * This shall remain in effect until immediate mode supports
     * multiple threads. */
    BLI_ticket_mutex_lock(system_gpu_context_mutex);
    GPU_render_begin();
    WM_system_gpu_context_activate(system_gpu_context);
    GPU_context_active_set(blender_gpu_context);
    GPU_context_begin_frame(blender_gpu_context);
  }
}

void DRW_gpu_context_disable_ex(bool restore)
{
  if (system_gpu_context != nullptr) {
    GPU_context_end_frame(blender_gpu_context);

    if (BLI_thread_is_main() && restore) {
      wm_window_reset_drawable();
    }
    else {
      WM_system_gpu_context_release(system_gpu_context);
      GPU_context_active_set(nullptr);
    }

    /* Render boundaries are opened and closed here as this may be
     * called outside of an existing render loop. */
    GPU_render_end();

    BLI_ticket_mutex_unlock(system_gpu_context_mutex);
  }
}

void DRW_gpu_context_enable()
{
  /* TODO: should be replace by a more elegant alternative. */

  if (G.background && system_gpu_context == nullptr) {
    WM_init_gpu();
  }
  DRW_gpu_context_enable_ex(true);
}

void DRW_gpu_context_disable()
{
  DRW_gpu_context_disable_ex(true);
}

void DRW_system_gpu_render_context_enable(void *re_system_gpu_context)
{
  /* If thread is main you should use DRW_gpu_context_enable(). */
  BLI_assert(!BLI_thread_is_main());

  WM_system_gpu_context_activate(re_system_gpu_context);
}

void DRW_system_gpu_render_context_disable(void *re_system_gpu_context)
{
  WM_system_gpu_context_release(re_system_gpu_context);
}

void DRW_blender_gpu_render_context_enable(void *re_gpu_context)
{
  /* If thread is main you should use DRW_gpu_context_enable(). */
  BLI_assert(!BLI_thread_is_main());

  GPU_context_active_set(static_cast<GPUContext *>(re_gpu_context));
}

void DRW_blender_gpu_render_context_disable(void * /*re_gpu_context*/)
{
  GPU_flush();
  GPU_context_active_set(nullptr);
}

/** \} */

#ifdef WITH_XR_OPENXR

void *DRW_system_gpu_context_get()
{
  /* XXX: There should really be no such getter, but for VR we currently can't easily avoid it.
   * OpenXR needs some low level info for the GPU context that will be used for submitting the
   * final frame-buffer. VR could in theory create its own context, but that would mean we have to
   * switch to it just to submit the final frame, which has notable performance impact.
   *
   * We could "inject" a context through DRW_system_gpu_render_context_enable(), but that would
   * have to work from the main thread, which is tricky to get working too. The preferable solution
   * would be using a separate thread for VR drawing where a single context can stay active. */

  return system_gpu_context;
}

void *DRW_xr_blender_gpu_context_get()
{
  /* XXX: See comment on #DRW_system_gpu_context_get(). */

  return blender_gpu_context;
}

void DRW_xr_drawing_begin()
{
  /* XXX: See comment on #DRW_system_gpu_context_get(). */

  BLI_ticket_mutex_lock(system_gpu_context_mutex);
}

void DRW_xr_drawing_end()
{
  /* XXX: See comment on #DRW_system_gpu_context_get(). */

  BLI_ticket_mutex_unlock(system_gpu_context_mutex);
}

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal testing API for gtests
 * \{ */

#ifdef WITH_GPU_DRAW_TESTS

void DRW_draw_state_init_gtests(eGPUShaderConfig sh_cfg)
{
  drw_get().draw_ctx.sh_cfg = sh_cfg;
}

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw manager context release/activation
 *
 * These functions are used in cases when an GPU context creation is needed during the draw.
 * This happens, for example, when an external engine needs to create its own GPU context from
 * the engine initialization.
 *
 * Example of context creation:
 *
 *   const bool drw_state = DRW_gpu_context_release();
 *   system_gpu_context = WM_system_gpu_context_create();
 *   DRW_gpu_context_activate(drw_state);
 *
 * Example of context destruction:
 *
 *   const bool drw_state = DRW_gpu_context_release();
 *   WM_system_gpu_context_activate(system_gpu_context);
 *   WM_system_gpu_context_dispose(system_gpu_context);
 *   DRW_gpu_context_activate(drw_state);
 *
 *
 * NOTE: Will only perform context modification when on main thread. This way these functions can
 * be used in an engine without check on whether it is a draw manager which manages GPU context
 * on the current thread. The downside of this is that if the engine performs GPU creation from
 * a non-main thread, that thread is supposed to not have GPU context ever bound by Blender.
 *
 * \{ */

bool DRW_gpu_context_release()
{
  if (!BLI_thread_is_main()) {
    return false;
  }

  if (GPU_context_active_get() != blender_gpu_context) {
    /* Context release is requested from the outside of the draw manager main draw loop, indicate
     * this to the `DRW_gpu_context_activate()` so that it restores drawable of the window.
     */
    return false;
  }

  GPU_context_active_set(nullptr);
  WM_system_gpu_context_release(system_gpu_context);

  return true;
}

void DRW_gpu_context_activate(bool drw_state)
{
  if (!BLI_thread_is_main()) {
    return;
  }

  if (drw_state) {
    WM_system_gpu_context_activate(system_gpu_context);
    GPU_context_active_set(blender_gpu_context);
  }
  else {
    wm_window_reset_drawable();
  }
}

/** \} */
