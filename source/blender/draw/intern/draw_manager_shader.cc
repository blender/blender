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

#include <atomic>
#include <condition_variable>
#include <mutex>

extern "C" char datatoc_gpu_shader_depth_only_frag_glsl[];
extern "C" char datatoc_common_fullscreen_vert_glsl[];

#define USE_DEFERRED_COMPILATION 1

using namespace blender;

/* -------------------------------------------------------------------- */
/** \name Deferred Compilation (DRW_deferred)
 *
 * Since compiling shader can take a long time, we do it in a non blocking
 * manner in another thread.
 *
 * \{ */

struct DRWShaderCompiler {
  /** Default compilation queue. */
  Vector<GPUMaterial *> queue;
  /** Optimization queue. */
  Vector<GPUMaterial *> optimize_queue;

  std::mutex queue_mutex;
  std::condition_variable queue_cv;

  void *system_gpu_context;
  GPUContext *blender_gpu_context;

  std::atomic<bool> stop;
};

/** NOTE: While the `BLI_threads` API requires a List,
 * we only create a single thread at application startup and delete it at exit. */
static ListBase &compilation_threadpool()
{
  static ListBase compilation_threadpool_ = {};
  return compilation_threadpool_;
}

static DRWShaderCompiler &compiler_data()
{
  static DRWShaderCompiler compiler_data_ = {};
  return compiler_data_;
}

static void *drw_deferred_shader_compilation_exec(void *)
{
  using namespace blender;

  void *system_gpu_context = compiler_data().system_gpu_context;
  GPUContext *blender_gpu_context = compiler_data().blender_gpu_context;
  BLI_assert(system_gpu_context != nullptr);
  BLI_assert(blender_gpu_context != nullptr);
  GPU_render_begin();
  WM_system_gpu_context_activate(system_gpu_context);
  GPU_context_active_set(blender_gpu_context);

  const bool use_parallel_compilation = GPU_use_parallel_compilation();
  Vector<GPUMaterial *> async_mats;

  while (true) {
    if (compiler_data().stop) {
      break;
    }

    compiler_data().queue_mutex.lock();
    /* Pop last because it will be less likely to lock the main thread
     * if all GPUMaterials are to be freed (see DRW_deferred_shader_remove()). */
    GPUMaterial *mat = compiler_data().queue.is_empty() ? nullptr :
                                                          compiler_data().queue.pop_last();
    if (mat) {
      /* Avoid another thread freeing the material mid compilation. */
      GPU_material_acquire(mat);
    }
    compiler_data().queue_mutex.unlock();

    if (mat) {
      /* We have a new material that must be compiled,
       * we either compile it directly or add it to the async compilation list. */
      if (use_parallel_compilation) {
        GPU_material_async_compile(mat);
        async_mats.append(mat);
      }
      else {
        GPU_material_compile(mat);
        GPU_material_release(mat);
      }
    }
    else if (!async_mats.is_empty()) {
      /* (only if use_parallel_compilation == true)
       * Keep querying the requested materials until all of them are ready. */
      async_mats.remove_if([](GPUMaterial *mat) {
        if (GPU_material_async_try_finalize(mat)) {
          GPU_material_release(mat);
          return true;
        }
        return false;
      });
    }
    else {
      /* Check for Material Optimization job once there are no more
       * shaders to compile. */
      compiler_data().queue_mutex.lock();
      /* Pop last because it will be less likely to lock the main thread
       * if all GPUMaterials are to be freed (see DRW_deferred_shader_remove()). */
      GPUMaterial *optimize_mat = compiler_data().optimize_queue.is_empty() ?
                                      nullptr :
                                      compiler_data().optimize_queue.pop_last();
      if (optimize_mat) {
        /* Avoid another thread freeing the material during optimization. */
        GPU_material_acquire(optimize_mat);
      }
      compiler_data().queue_mutex.unlock();

      if (optimize_mat) {
        /* Compile optimized material shader. */
        GPU_material_optimize(optimize_mat);
        GPU_material_release(optimize_mat);
      }
      else {
        /* No more materials to optimize, or shaders to compile. */
        std::unique_lock lock(compiler_data().queue_mutex);
        compiler_data().queue_cv.wait(lock);
      }
    }

    if (GPU_type_matches_ex(GPU_DEVICE_ANY, GPU_OS_ANY, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL)) {
      GPU_flush();
    }
  }

  /* We have to wait until all the requested batches are ready,
   * even if compiler_data().stop is true. */
  while (!async_mats.is_empty()) {
    async_mats.remove_if([](GPUMaterial *mat) {
      if (GPU_material_async_try_finalize(mat)) {
        GPU_material_release(mat);
        return true;
      }
      return false;
    });
  }

  GPU_context_active_set(nullptr);
  WM_system_gpu_context_release(system_gpu_context);
  GPU_render_end();

  return nullptr;
}

void DRW_shader_init()
{
  if (GPU_use_main_context_workaround()) {
    /* Deferred compilation is not supported. */
    return;
  }
  static bool initialized = false;
  if (initialized) {
    BLI_assert_unreachable();
    return;
  }
  initialized = true;

  compiler_data().stop = false;

  compiler_data().system_gpu_context = WM_system_gpu_context_create();
  compiler_data().blender_gpu_context = GPU_context_create(nullptr,
                                                           compiler_data().system_gpu_context);
  GPU_context_active_set(nullptr);
  WM_system_gpu_context_activate(DST.system_gpu_context);
  GPU_context_active_set(DST.blender_gpu_context);

  BLI_threadpool_init(&compilation_threadpool(), drw_deferred_shader_compilation_exec, 1);
  BLI_threadpool_insert(&compilation_threadpool(), nullptr);
}

void DRW_shader_exit()
{
  if (GPU_use_main_context_workaround()) {
    /* Deferred compilation is not supported. */
    return;
  }

  compiler_data().stop = true;
  compiler_data().queue_cv.notify_one();
  BLI_threadpool_end(&compilation_threadpool());

  /* Revert the queued state for the materials that has not been compiled.
   * Note that this is not strictly needed since this function is called at program exit. */
  {
    std::scoped_lock queue_lock(compiler_data().queue_mutex);

    while (!compiler_data().queue.is_empty()) {
      GPU_material_status_set(compiler_data().queue.pop_last(), GPU_MAT_CREATED);
    }
    while (!compiler_data().optimize_queue.is_empty()) {
      GPU_material_optimization_status_set(compiler_data().optimize_queue.pop_last(),
                                           GPU_MAT_OPTIMIZATION_READY);
    }
  }

  WM_system_gpu_context_activate(compiler_data().system_gpu_context);
  GPU_context_active_set(compiler_data().blender_gpu_context);
  GPU_context_discard(compiler_data().blender_gpu_context);
  WM_system_gpu_context_dispose(compiler_data().system_gpu_context);
}

/**
 * Append either shader compilation or optimization job to deferred queue.
 * We keep two separate queue's to ensure core compilations always complete before optimization.
 */
static void drw_deferred_queue_append(GPUMaterial *mat, bool is_optimization_job)
{
  std::scoped_lock queue_lock(compiler_data().queue_mutex);

  /* Add to either compilation or optimization queue. */
  if (is_optimization_job) {
    BLI_assert(GPU_material_optimization_status(mat) != GPU_MAT_OPTIMIZATION_QUEUED);
    GPU_material_optimization_status_set(mat, GPU_MAT_OPTIMIZATION_QUEUED);
    compiler_data().optimize_queue.append(mat);
  }
  else {
    GPU_material_status_set(mat, GPU_MAT_QUEUED);
    compiler_data().queue.append(mat);
  }

  compiler_data().queue_cv.notify_one();
}

static void drw_deferred_shader_add(GPUMaterial *mat, bool deferred)
{
  if (ELEM(GPU_material_status(mat), GPU_MAT_SUCCESS, GPU_MAT_FAILED)) {
    return;
  }

  if (GPU_use_main_context_workaround()) {
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
  if (GPU_use_main_context_workaround()) {
    /* Deferred compilation is not supported. */
    return;
  }

  std::scoped_lock queue_lock(compiler_data().queue_mutex);

  /* Search for compilation job in queue. */
  if (compiler_data().queue.contains(mat)) {
    compiler_data().queue.remove_first_occurrence_and_reorder(mat);
    GPU_material_status_set(mat, GPU_MAT_CREATED);
  }

  /* Search for optimization job in queue. */
  if (compiler_data().optimize_queue.contains(mat)) {
    compiler_data().optimize_queue.remove_first_occurrence_and_reorder(mat);
    GPU_material_optimization_status_set(mat, GPU_MAT_OPTIMIZATION_READY);
  }
}

void DRW_deferred_shader_optimize_remove(GPUMaterial *mat)
{
  if (GPU_use_main_context_workaround()) {
    /* Deferred compilation is not supported. */
    return;
  }

  std::scoped_lock queue_lock(compiler_data().queue_mutex);

  /* Search for optimization job in queue. */
  if (compiler_data().optimize_queue.contains(mat)) {
    compiler_data().optimize_queue.remove_first_occurrence_and_reorder(mat);
    GPU_material_optimization_status_set(mat, GPU_MAT_OPTIMIZATION_READY);
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
