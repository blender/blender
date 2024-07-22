/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_world_types.h"

#include "BLI_dynstr.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_string_utils.hh"
#include "BLI_threads.h"
#include "BLI_time.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_main.hh"

#include "DEG_depsgraph_query.hh"

#include "GPU_capabilities.hh"
#include "GPU_material.hh"
#include "GPU_shader.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm_window.hh"

#include "draw_manager_c.hh"

extern "C" char datatoc_gpu_shader_depth_only_frag_glsl[];
extern "C" char datatoc_common_fullscreen_vert_glsl[];

#define USE_DEFERRED_COMPILATION 1

/* -------------------------------------------------------------------- */
/** \name Deferred Compilation (DRW_deferred)
 *
 * Since compiling shader can take a long time, we do it in a non blocking
 * manner in another thread.
 *
 * \{ */

struct DRWShaderCompiler {
  /** Default compilation queue. */
  ListBase queue; /* GPUMaterial */
  SpinLock list_lock;

  /** Optimization queue. */
  ListBase optimize_queue; /* GPUMaterial */

  void *system_gpu_context;
  GPUContext *blender_gpu_context;
  bool own_context;
};

static void drw_deferred_shader_compilation_exec(void *custom_data,
                                                 wmJobWorkerStatus *worker_status)
{
  using namespace blender;

  GPU_render_begin();
  DRWShaderCompiler *comp = (DRWShaderCompiler *)custom_data;
  void *system_gpu_context = comp->system_gpu_context;
  GPUContext *blender_gpu_context = comp->blender_gpu_context;

  BLI_assert(system_gpu_context != nullptr);
  BLI_assert(blender_gpu_context != nullptr);

  const bool use_main_context_workaround = GPU_use_main_context_workaround();
  if (use_main_context_workaround) {
    BLI_assert(system_gpu_context == DST.system_gpu_context);
    GPU_context_main_lock();
  }

  const bool use_parallel_compilation = GPU_use_parallel_compilation();

  WM_system_gpu_context_activate(system_gpu_context);
  GPU_context_active_set(blender_gpu_context);

  Vector<GPUMaterial *> next_batch;
  Map<BatchHandle, Vector<GPUMaterial *>> batches;

  while (true) {
    if (worker_status->stop) {
      break;
    }

    BLI_spin_lock(&comp->list_lock);
    /* Pop tail because it will be less likely to lock the main thread
     * if all GPUMaterials are to be freed (see DRW_deferred_shader_remove()). */
    LinkData *link = (LinkData *)BLI_poptail(&comp->queue);
    GPUMaterial *mat = link ? (GPUMaterial *)link->data : nullptr;
    if (mat) {
      /* Avoid another thread freeing the material mid compilation. */
      GPU_material_acquire(mat);
      MEM_freeN(link);
    }
    BLI_spin_unlock(&comp->list_lock);

    if (mat) {
      /* We have a new material that must be compiled,
       * we either compile it directly or add it to a parallel compilation batch. */
      if (use_parallel_compilation) {
        next_batch.append(mat);
      }
      else {
        GPU_material_compile(mat);
        GPU_material_release(mat);
      }
    }
    else if (!next_batch.is_empty()) {
      /* (only if use_parallel_compilation == true)
       * We ran out of pending materials. Request the compilation of the current batch. */
      BatchHandle batch_handle = GPU_material_batch_compile(next_batch);
      batches.add(batch_handle, next_batch);
      next_batch.clear();
    }
    else if (!batches.is_empty()) {
      /* (only if use_parallel_compilation == true)
       * Keep querying the requested batches until all of them are ready. */
      Vector<BatchHandle> ready_handles;
      for (BatchHandle handle : batches.keys()) {
        if (GPU_material_batch_is_ready(handle)) {
          ready_handles.append(handle);
        }
      }
      for (BatchHandle handle : ready_handles) {
        Vector<GPUMaterial *> batch = batches.pop(handle);
        GPU_material_batch_finalize(handle, batch);
        for (GPUMaterial *mat : batch) {
          GPU_material_release(mat);
        }
      }
    }
    else {
      /* Check for Material Optimization job once there are no more
       * shaders to compile. */
      BLI_spin_lock(&comp->list_lock);
      /* Pop tail because it will be less likely to lock the main thread
       * if all GPUMaterials are to be freed (see DRW_deferred_shader_remove()). */
      LinkData *link = (LinkData *)BLI_poptail(&comp->optimize_queue);
      GPUMaterial *optimize_mat = link ? (GPUMaterial *)link->data : nullptr;
      if (optimize_mat) {
        /* Avoid another thread freeing the material during optimization. */
        GPU_material_acquire(optimize_mat);
      }
      BLI_spin_unlock(&comp->list_lock);

      if (optimize_mat) {
        /* Compile optimized material shader. */
        GPU_material_optimize(optimize_mat);
        GPU_material_release(optimize_mat);
        MEM_freeN(link);
      }
      else {
        /* No more materials to optimize, or shaders to compile. */
        break;
      }
    }

    if (GPU_type_matches_ex(GPU_DEVICE_ANY, GPU_OS_ANY, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL)) {
      GPU_flush();
    }
  }

  /* We have to wait until all the requested batches are ready,
   * even if worker_status->stop is true. */
  for (BatchHandle handle : batches.keys()) {
    Vector<GPUMaterial *> &batch = batches.lookup(handle);
    GPU_material_batch_finalize(handle, batch);
    for (GPUMaterial *mat : batch) {
      GPU_material_release(mat);
    }
  }

  GPU_context_active_set(nullptr);
  WM_system_gpu_context_release(system_gpu_context);
  if (use_main_context_workaround) {
    GPU_context_main_unlock();
  }
  GPU_render_end();
}

static void drw_deferred_shader_compilation_free(void *custom_data)
{
  DRWShaderCompiler *comp = (DRWShaderCompiler *)custom_data;

  BLI_spin_lock(&comp->list_lock);
  LISTBASE_FOREACH (LinkData *, link, &comp->queue) {
    GPU_material_status_set(static_cast<GPUMaterial *>(link->data), GPU_MAT_CREATED);
  }
  LISTBASE_FOREACH (LinkData *, link, &comp->optimize_queue) {
    GPU_material_optimization_status_set(static_cast<GPUMaterial *>(link->data),
                                         GPU_MAT_OPTIMIZATION_READY);
  }
  BLI_freelistN(&comp->queue);
  BLI_freelistN(&comp->optimize_queue);
  BLI_spin_unlock(&comp->list_lock);

  if (comp->own_context) {
    /* Only destroy if the job owns the context. */
    WM_system_gpu_context_activate(comp->system_gpu_context);
    GPU_context_active_set(comp->blender_gpu_context);
    GPU_context_discard(comp->blender_gpu_context);
    WM_system_gpu_context_dispose(comp->system_gpu_context);

    wm_window_reset_drawable();
  }

  MEM_freeN(comp);
}

/**
 * Append either shader compilation or optimization job to deferred queue and
 * ensure shader compilation worker is active.
 * We keep two separate queue's to ensure core compilations always complete before optimization.
 */
static void drw_deferred_queue_append(GPUMaterial *mat, bool is_optimization_job)
{
  const bool use_main_context = GPU_use_main_context_workaround();
  const bool job_own_context = !use_main_context;

  BLI_assert(DST.draw_ctx.evil_C);
  wmWindowManager *wm = CTX_wm_manager(DST.draw_ctx.evil_C);
  wmWindow *win = CTX_wm_window(DST.draw_ctx.evil_C);

  /* Get the running job or a new one if none is running. Can only have one job per type & owner.
   */
  wmJob *wm_job = WM_jobs_get(
      wm, win, wm, "Shaders Compilation", eWM_JobFlag(0), WM_JOB_TYPE_SHADER_COMPILATION);

  DRWShaderCompiler *old_comp = (DRWShaderCompiler *)WM_jobs_customdata_get(wm_job);

  DRWShaderCompiler *comp = static_cast<DRWShaderCompiler *>(
      MEM_callocN(sizeof(DRWShaderCompiler), "DRWShaderCompiler"));
  BLI_spin_init(&comp->list_lock);

  if (old_comp) {
    BLI_spin_lock(&old_comp->list_lock);
    BLI_movelisttolist(&comp->queue, &old_comp->queue);
    BLI_movelisttolist(&comp->optimize_queue, &old_comp->optimize_queue);
    BLI_spin_unlock(&old_comp->list_lock);
    /* Do not recreate context, just pass ownership. */
    if (old_comp->system_gpu_context) {
      comp->system_gpu_context = old_comp->system_gpu_context;
      comp->blender_gpu_context = old_comp->blender_gpu_context;
      old_comp->own_context = false;
      comp->own_context = job_own_context;
    }
  }

  /* Add to either compilation or optimization queue. */
  if (is_optimization_job) {
    BLI_assert(GPU_material_optimization_status(mat) != GPU_MAT_OPTIMIZATION_QUEUED);
    GPU_material_optimization_status_set(mat, GPU_MAT_OPTIMIZATION_QUEUED);
    LinkData *node = BLI_genericNodeN(mat);
    BLI_addtail(&comp->optimize_queue, node);
  }
  else {
    GPU_material_status_set(mat, GPU_MAT_QUEUED);
    LinkData *node = BLI_genericNodeN(mat);
    BLI_addtail(&comp->queue, node);
  }

  /* Create only one context. */
  if (comp->system_gpu_context == nullptr) {
    if (use_main_context) {
      comp->system_gpu_context = DST.system_gpu_context;
      comp->blender_gpu_context = DST.blender_gpu_context;
    }
    else {
      comp->system_gpu_context = WM_system_gpu_context_create();
      comp->blender_gpu_context = GPU_context_create(nullptr, comp->system_gpu_context);
      GPU_context_active_set(nullptr);

      WM_system_gpu_context_activate(DST.system_gpu_context);
      GPU_context_active_set(DST.blender_gpu_context);
    }
    comp->own_context = job_own_context;
  }

  WM_jobs_customdata_set(wm_job, comp, drw_deferred_shader_compilation_free);
  WM_jobs_timer(wm_job, 0.1, NC_MATERIAL | ND_SHADING_DRAW, 0);
  WM_jobs_delay_start(wm_job, 0.1);
  WM_jobs_callbacks(wm_job, drw_deferred_shader_compilation_exec, nullptr, nullptr, nullptr);

  G.is_break = false;

  WM_jobs_start(wm, wm_job);
}

static void drw_deferred_shader_add(GPUMaterial *mat, bool deferred)
{
  if (ELEM(GPU_material_status(mat), GPU_MAT_SUCCESS, GPU_MAT_FAILED)) {
    return;
  }

  /* Do not defer the compilation if we are rendering for image.
   * deferred rendering is only possible when `evil_C` is available */
  if (DST.draw_ctx.evil_C == nullptr || DRW_state_is_image_render() || !USE_DEFERRED_COMPILATION) {
    deferred = false;
  }

  /* Avoid crashes with RenderDoc on Windows + Nvidia. */
  if (G.debug & G_DEBUG_GPU_RENDERDOC &&
      GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_OFFICIAL))
  {
    deferred = false;
  }

  if (!deferred) {
    DRW_deferred_shader_remove(mat);
    /* Shaders could already be compiling. Have to wait for compilation to finish. */
    while (GPU_material_status(mat) == GPU_MAT_QUEUED) {
      BLI_time_sleep_ms(20);
    }
    if (GPU_material_status(mat) == GPU_MAT_CREATED) {
      GPU_material_compile(mat);
    }
    return;
  }

  /* Don't add material to the queue twice. */
  if (GPU_material_status(mat) == GPU_MAT_QUEUED) {
    return;
  }

  /* Add deferred shader compilation to queue. */
  drw_deferred_queue_append(mat, false);
}

static void drw_register_shader_vlattrs(GPUMaterial *mat)
{
  const ListBase *attrs = GPU_material_layer_attributes(mat);

  if (!attrs) {
    return;
  }

  GHash *hash = DST.vmempool->vlattrs_name_cache;
  ListBase *list = &DST.vmempool->vlattrs_name_list;

  LISTBASE_FOREACH (GPULayerAttr *, attr, attrs) {
    GPULayerAttr **p_val;

    /* Add to the table and list if newly seen. */
    if (!BLI_ghash_ensure_p(hash, POINTER_FROM_UINT(attr->hash_code), (void ***)&p_val)) {
      DST.vmempool->vlattrs_ubo_ready = false;

      GPULayerAttr *new_link = *p_val = static_cast<GPULayerAttr *>(MEM_dupallocN(attr));

      /* Insert into the list ensuring sorted order. */
      GPULayerAttr *link = static_cast<GPULayerAttr *>(list->first);

      while (link && link->hash_code <= attr->hash_code) {
        link = link->next;
      }

      new_link->prev = new_link->next = nullptr;
      BLI_insertlinkbefore(list, link, new_link);
    }

    /* Reset the unused frames counter. */
    (*p_val)->users = 0;
  }
}

void DRW_deferred_shader_remove(GPUMaterial *mat)
{
  LISTBASE_FOREACH (wmWindowManager *, wm, &G_MAIN->wm) {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      DRWShaderCompiler *comp = (DRWShaderCompiler *)WM_jobs_customdata_from_type(
          wm, wm, WM_JOB_TYPE_SHADER_COMPILATION);
      if (comp != nullptr) {
        BLI_spin_lock(&comp->list_lock);

        /* Search for compilation job in queue. */
        LinkData *link = (LinkData *)BLI_findptr(&comp->queue, mat, offsetof(LinkData, data));
        if (link) {
          BLI_remlink(&comp->queue, link);
          GPU_material_status_set(static_cast<GPUMaterial *>(link->data), GPU_MAT_CREATED);
        }

        MEM_SAFE_FREE(link);

        /* Search for optimization job in queue. */
        LinkData *opti_link = (LinkData *)BLI_findptr(
            &comp->optimize_queue, mat, offsetof(LinkData, data));
        if (opti_link) {
          BLI_remlink(&comp->optimize_queue, opti_link);
          GPU_material_optimization_status_set(static_cast<GPUMaterial *>(opti_link->data),
                                               GPU_MAT_OPTIMIZATION_READY);
        }
        BLI_spin_unlock(&comp->list_lock);

        MEM_SAFE_FREE(opti_link);
      }
    }
  }
}

void DRW_deferred_shader_optimize_remove(GPUMaterial *mat)
{
  LISTBASE_FOREACH (wmWindowManager *, wm, &G_MAIN->wm) {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      DRWShaderCompiler *comp = (DRWShaderCompiler *)WM_jobs_customdata_from_type(
          wm, wm, WM_JOB_TYPE_SHADER_COMPILATION);
      if (comp != nullptr) {
        BLI_spin_lock(&comp->list_lock);
        /* Search for optimization job in queue. */
        LinkData *opti_link = (LinkData *)BLI_findptr(
            &comp->optimize_queue, mat, offsetof(LinkData, data));
        if (opti_link) {
          BLI_remlink(&comp->optimize_queue, opti_link);
          GPU_material_optimization_status_set(static_cast<GPUMaterial *>(opti_link->data),
                                               GPU_MAT_OPTIMIZATION_READY);
        }
        BLI_spin_unlock(&comp->list_lock);

        MEM_SAFE_FREE(opti_link);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */

/** \{ */

GPUMaterial *DRW_shader_from_world(World *wo,
                                   bNodeTree *ntree,
                                   eGPUMaterialEngine engine,
                                   const uint64_t shader_id,
                                   const bool is_volume_shader,
                                   bool deferred,
                                   GPUCodegenCallbackFn callback,
                                   void *thunk)
{
  Scene *scene = (Scene *)DEG_get_original_id(&DST.draw_ctx.scene->id);
  GPUMaterial *mat = GPU_material_from_nodetree(scene,
                                                nullptr,
                                                ntree,
                                                &wo->gpumaterial,
                                                wo->id.name,
                                                engine,
                                                shader_id,
                                                is_volume_shader,
                                                false,
                                                callback,
                                                thunk);

  drw_register_shader_vlattrs(mat);

  if (DRW_state_is_image_render()) {
    /* Do not deferred if doing render. */
    deferred = false;
  }

  drw_deferred_shader_add(mat, deferred);
  DRW_shader_queue_optimize_material(mat);
  return mat;
}

GPUMaterial *DRW_shader_from_material(Material *ma,
                                      bNodeTree *ntree,
                                      eGPUMaterialEngine engine,
                                      const uint64_t shader_id,
                                      const bool is_volume_shader,
                                      bool deferred,
                                      GPUCodegenCallbackFn callback,
                                      void *thunk,
                                      GPUMaterialPassReplacementCallbackFn pass_replacement_cb)
{
  Scene *scene = (Scene *)DEG_get_original_id(&DST.draw_ctx.scene->id);
  GPUMaterial *mat = GPU_material_from_nodetree(scene,
                                                ma,
                                                ntree,
                                                &ma->gpumaterial,
                                                ma->id.name,
                                                engine,
                                                shader_id,
                                                is_volume_shader,
                                                false,
                                                callback,
                                                thunk,
                                                pass_replacement_cb);

  drw_register_shader_vlattrs(mat);

  if (DRW_state_is_image_render()) {
    /* Do not deferred if doing render. */
    deferred = false;
  }

  drw_deferred_shader_add(mat, deferred);
  DRW_shader_queue_optimize_material(mat);
  return mat;
}

void DRW_shader_queue_optimize_material(GPUMaterial *mat)
{
  /* Do not perform deferred optimization if performing render.
   * De-queue any queued optimization jobs. */
  if (DRW_state_is_image_render()) {
    if (GPU_material_optimization_status(mat) == GPU_MAT_OPTIMIZATION_QUEUED) {
      /* Remove from pending optimization job queue. */
      DRW_deferred_shader_optimize_remove(mat);
      /* If optimization job had already started, wait for it to complete. */
      while (GPU_material_optimization_status(mat) == GPU_MAT_OPTIMIZATION_QUEUED) {
        BLI_time_sleep_ms(20);
      }
    }
    return;
  }

  /* We do not need to perform optimization on the material if it is already compiled or in the
   * optimization queue. If optimization is not required, the status will be flagged as
   * `GPU_MAT_OPTIMIZATION_SKIP`.
   * We can also skip cases which have already been queued up. */
  if (ELEM(GPU_material_optimization_status(mat),
           GPU_MAT_OPTIMIZATION_SKIP,
           GPU_MAT_OPTIMIZATION_SUCCESS,
           GPU_MAT_OPTIMIZATION_QUEUED))
  {
    return;
  }

  /* Only queue optimization once the original shader has been successfully compiled. */
  if (GPU_material_status(mat) != GPU_MAT_SUCCESS) {
    return;
  }

  /* Defer optimization until sufficient time has passed beyond creation. This avoids excessive
   * recompilation for shaders which are being actively modified. */
  if (!GPU_material_optimization_ready(mat)) {
    return;
  }

  /* Add deferred shader compilation to queue. */
  drw_deferred_queue_append(mat, true);
}

void DRW_shader_free(GPUShader *shader)
{
  GPU_shader_free(shader);
}

/** \} */
