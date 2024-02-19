/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Contains everything about light baking.
 */

#include <mutex>

#include "DRW_render.hh"

#include "BKE_global.hh"
#include "BKE_lightprobe.h"

#include "DNA_lightprobe_types.h"

#include "BLI_threads.h"
#include "BLI_time.h"

#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "GPU_capabilities.h"
#include "GPU_context.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm_window.hh"

#include "eevee_engine.h"
#include "eevee_instance.hh"

#include "eevee_lightcache.hh"

/* -------------------------------------------------------------------- */
/** \name Light Probe Baking
 * \{ */

namespace blender::eevee {

class LightBake {
 private:
  Depsgraph *depsgraph_;

  /** Scene frame to evaluate the depsgraph at. */
  int frame_;
  /** Milliseconds. Delay the start of the baking to not slowdown interactions (TODO: remove). */
  int delay_ms_;

  /**
   * If running in parallel (in a separate thread), use this context.
   * Created on main thread but first bound in worker thread.
   */
  void *gl_context_ = nullptr;
  /** Context associated to `gl_context_`. Created in the worker thread. */
  GPUContext *gpu_context_ = nullptr;

  /** Baking instance. Created and freed in the worker thread. */
  Instance *instance_ = nullptr;
  /** Manager used for command submission. Created and freed in the worker thread. */
  draw::Manager *manager_ = nullptr;

  /** Light-probe original objects to bake. */
  Vector<Object *> original_probes_;
  /** Frame to copy to original objects during update. This is needed to avoid race conditions. */
  Vector<LightProbeGridCacheFrame *> bake_result_;
  std::mutex result_mutex_;

 public:
  LightBake(Main *bmain,
            ViewLayer *view_layer,
            Scene *scene,
            Span<Object *> probes,
            bool run_as_job,
            int frame,
            int delay_ms = 0)
      : depsgraph_(DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_RENDER)),
        frame_(frame),
        delay_ms_(delay_ms),
        original_probes_(probes)
  {
    BLI_assert(BLI_thread_is_main());
    bake_result_.resize(probes.size());
    bake_result_.fill(nullptr);

    if (run_as_job && !GPU_use_main_context_workaround()) {
      /* This needs to happen in main thread. */
      gl_context_ = WM_system_gpu_context_create();
      wm_window_reset_drawable();
    }
  }

  ~LightBake()
  {
    BLI_assert(BLI_thread_is_main());
    DEG_graph_free(depsgraph_);
  }

  /**
   * Called from main thread.
   * Copy result to original scene data.
   * Note that since this is in the main thread, the viewport cannot be using the light cache.
   * So there is no race condition here.
   */
  void update()
  {
    BLI_assert(BLI_thread_is_main());

    for (auto i : bake_result_.index_range()) {
      if (bake_result_[i] == nullptr) {
        continue;
      }
      Object *orig_ob = original_probes_[i];

      {
        std::scoped_lock lock(result_mutex_);

        LightProbeObjectCache *cache = orig_ob->lightprobe_cache;
        /* Delete any existing cache. */
        if (cache->grid_static_cache != nullptr) {
          BKE_lightprobe_grid_cache_frame_free(cache->grid_static_cache);
        }
        /* Pass ownership to original object. */
        cache->grid_static_cache = bake_result_[i];
        bake_result_[i] = nullptr;
      }
      /* Propagate the cache to evaluated object. */
      DEG_id_tag_update(&orig_ob->id, ID_RECALC_SYNC_TO_EVAL | ID_RECALC_SHADING);
    }
  }

  /**
   * Called from worker thread.
   */
  void run(bool *stop = nullptr, bool *do_update = nullptr, float *progress = nullptr)
  {
    DEG_graph_relations_update(depsgraph_);
    DEG_evaluate_on_framechange(depsgraph_, frame_);

    if (delay_ms_ > 0) {
      BLI_time_sleep_ms(delay_ms_);
    }

    context_enable();
    manager_ = new draw::Manager();
    instance_ = new eevee::Instance();
    instance_->init_light_bake(depsgraph_, manager_);
    context_disable();

    for (auto i : original_probes_.index_range()) {
      Object *eval_ob = DEG_get_evaluated_object(depsgraph_, original_probes_[i]);

      instance_->light_bake_irradiance(
          *eval_ob,
          [this]() { context_enable(); },
          [this]() { context_disable(); },
          [&]() { return (G.is_break == true) || ((stop != nullptr) ? *stop : false); },
          [&](LightProbeGridCacheFrame *cache_frame, float grid_progress) {
            {
              std::scoped_lock lock(result_mutex_);
              /* Delete any existing cache that wasn't transferred to the original object. */
              if (bake_result_[i] != nullptr) {
                BKE_lightprobe_grid_cache_frame_free(bake_result_[i]);
              }
              bake_result_[i] = cache_frame;
            }

            if (do_update) {
              *do_update = true;
            }

            if (progress) {
              *progress = (i + grid_progress) / original_probes_.size();
            }
          });

      if (instance_->info != "") {
        /** TODO: Print to the Status Bar UI. */
        printf("%s\n", instance_->info.c_str());
      }

      if ((G.is_break == true) || (stop != nullptr && *stop == true)) {
        break;
      }
    }

    delete_resources();
  }

 private:
  void context_enable(bool render_begin = true)
  {
    if (GPU_use_main_context_workaround() && !BLI_thread_is_main()) {
      /* Reuse main draw context. */
      GPU_context_main_lock();
      DRW_gpu_context_enable();
    }
    else if (gl_context_ == nullptr) {
      /* Main thread case. */
      DRW_gpu_context_enable();
    }
    else {
      /* Worker thread case. */
      DRW_system_gpu_render_context_enable(gl_context_);
      if (gpu_context_ == nullptr) {
        /* Create GPUContext in worker thread as it needs the correct gl context bound (which can
         * only be bound in worker thread because of some GL driver requirements). */
        gpu_context_ = GPU_context_create(nullptr, gl_context_);
      }
      DRW_blender_gpu_render_context_enable(gpu_context_);
    }

    if (render_begin) {
      GPU_render_begin();
    }
  }

  void context_disable()
  {
    if (GPU_use_main_context_workaround() && !BLI_thread_is_main()) {
      /* Reuse main draw context. */
      DRW_gpu_context_disable();
      GPU_render_end();
      GPU_context_main_unlock();
    }
    else if (gl_context_ == nullptr) {
      /* Main thread case. */
      DRW_gpu_context_disable();
      GPU_render_end();
    }
    else {
      /* Worker thread case. */
      DRW_blender_gpu_render_context_disable(gpu_context_);
      GPU_render_end();
      DRW_system_gpu_render_context_disable(gl_context_);
    }
  }

  /**
   * Delete the engine instance and the optional contexts.
   * This needs to run on the worker thread because the OpenGL context can only be ever bound to a
   * single thread (because of some driver implementation), and the resources (textures,
   * buffers,...) need to be freed with the right context bound.
   */
  void delete_resources()
  {
    /* Bind context without GPU_render_begin(). */
    context_enable(false);

    /* Free GPU data (Textures, Frame-buffers, etc...). */
    delete instance_;
    delete manager_;

    /* Delete / unbind the GL & GPU context. Assumes it is currently bound. */
    if (GPU_use_main_context_workaround() && !BLI_thread_is_main()) {
      /* Reuse main draw context. */
      DRW_gpu_context_disable();
      GPU_context_main_unlock();
    }
    else if (gl_context_ == nullptr) {
      /* Main thread case. */
      DRW_gpu_context_disable();
    }
    else {
      /* Worker thread case. */
      if (gpu_context_ != nullptr) {
        GPU_context_discard(gpu_context_);
      }
      DRW_system_gpu_render_context_disable(gl_context_);
      WM_system_gpu_context_dispose(gl_context_);
    }
  }
};

}  // namespace blender::eevee

/** \} */

/* -------------------------------------------------------------------- */
/** \name Light Bake Job
 * \{ */

using namespace blender::eevee;

wmJob *EEVEE_NEXT_lightbake_job_create(wmWindowManager *wm,
                                       wmWindow *win,
                                       Main *bmain,
                                       ViewLayer *view_layer,
                                       Scene *scene,
                                       blender::Vector<Object *> original_probes,
                                       int delay_ms,
                                       int frame)
{
  /* Do not bake if there is a render going on. */
  if (WM_jobs_test(wm, scene, WM_JOB_TYPE_RENDER)) {
    return nullptr;
  }

  /* Stop existing baking job. */
  WM_jobs_stop(wm, nullptr, EEVEE_NEXT_lightbake_job);

  wmJob *wm_job = WM_jobs_get(wm,
                              win,
                              scene,
                              "Bake Lighting",
                              WM_JOB_EXCL_RENDER | WM_JOB_PRIORITY | WM_JOB_PROGRESS,
                              WM_JOB_TYPE_LIGHT_BAKE);

  LightBake *bake = new LightBake(
      bmain, view_layer, scene, std::move(original_probes), true, frame, delay_ms);

  WM_jobs_customdata_set(wm_job, bake, EEVEE_NEXT_lightbake_job_data_free);
  WM_jobs_timer(wm_job, 0.4, NC_SCENE | NA_EDITED, 0);
  WM_jobs_callbacks(wm_job,
                    EEVEE_NEXT_lightbake_job,
                    nullptr,
                    EEVEE_NEXT_lightbake_update,
                    EEVEE_NEXT_lightbake_update);

  G.is_break = false;

  return wm_job;
}

void *EEVEE_NEXT_lightbake_job_data_alloc(Main *bmain,
                                          ViewLayer *view_layer,
                                          Scene *scene,
                                          blender::Vector<Object *> original_probes,
                                          int frame)
{
  LightBake *bake = new LightBake(
      bmain, view_layer, scene, std::move(original_probes), false, frame);
  /* TODO(fclem): Can remove this cast once we remove the previous EEVEE light cache. */
  return reinterpret_cast<void *>(bake);
}

void EEVEE_NEXT_lightbake_job_data_free(void *job_data)
{
  delete static_cast<LightBake *>(job_data);
}

void EEVEE_NEXT_lightbake_update(void *job_data)
{
  static_cast<LightBake *>(job_data)->update();
}

void EEVEE_NEXT_lightbake_job(void *job_data, wmJobWorkerStatus *worker_status)
{
  static_cast<LightBake *>(job_data)->run(
      &worker_status->stop, &worker_status->do_update, &worker_status->progress);
}

/** \} */
