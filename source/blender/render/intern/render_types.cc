/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#include "render_types.h"

#include <memory>

#include "BKE_colortools.hh"

#include "BLI_assert.h"
#include "RE_compositor.hh"
#include "RE_engine.h"

#include "render_result.h"

#include "GPU_context.hh"

#include "WM_api.hh"
#include "wm_window.hh"

/* -------------------------------------------------------------------- */
/** \name Render
 * \{ */

BaseRender::~BaseRender()
{
  if (engine) {
    RE_engine_free(engine);
  }

  render_result_free(result);

  BLI_rw_mutex_end(&resultmutex);
  BLI_mutex_end(&engine_draw_mutex);
}

Render::Render()
{
  display = std::make_shared<RenderDisplay>();
}

Render::~Render()
{
  RE_compositor_free(*this);

  display.reset();

  BKE_curvemapping_free_data(&r.mblur_shutter_curve);

  render_result_free(pushedresult);
}

bool Render::prepare_viewlayer(ViewLayer *view_layer, Depsgraph *depsgraph)
{
  if (!prepare_viewlayer_cb) {
    return true;
  }

  return prepare_viewlayer_cb(prepare_vl_handle, view_layer, depsgraph);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name RenderDisplay
 * \{ */

RenderDisplay::RenderDisplay(bool create_gpu_context)
{
  if (create_gpu_context) {
    BLI_assert(BLI_thread_is_main());

    if (system_gpu_context == nullptr) {
      /* Needs to be created in the main thread. */
      system_gpu_context = WM_system_gpu_context_create();
      /* The context is activated during creation, so release it here since the function should not
       * have context activation as a side effect. Then activate the drawable's context below. */
      if (system_gpu_context) {
        WM_system_gpu_context_release(system_gpu_context);
      }
      wm_window_reset_drawable();
    }
  }
}

RenderDisplay::~RenderDisplay()
{
  clear();
}

void RenderDisplay::clear()
{
  if (blender_gpu_context) {
    WM_system_gpu_context_activate(system_gpu_context);
    GPU_context_active_set(static_cast<GPUContext *>(blender_gpu_context));
    GPU_context_discard(static_cast<GPUContext *>(blender_gpu_context));
    blender_gpu_context = nullptr;
  }

  if (system_gpu_context) {
    WM_system_gpu_context_dispose(system_gpu_context);
    system_gpu_context = nullptr;

    /* If in main thread, reset window context. */
    if (BLI_thread_is_main()) {
      wm_window_reset_drawable();
    }
  }

  display_update_cb = nullptr;
  current_scene_update_cb = nullptr;
  stats_draw_cb = nullptr;
  progress_cb = nullptr;
  draw_lock_cb = nullptr;
  test_break_cb = nullptr;
}

void *RenderDisplay::ensure_blender_gpu_context()
{
  BLI_assert(system_gpu_context != nullptr);
  if (blender_gpu_context == nullptr) {
    blender_gpu_context = GPU_context_create(nullptr, system_gpu_context);
  }
  return blender_gpu_context;
}

void RenderDisplay::display_update(RenderResult *render_result, rcti *rect)
{
  if (display_update_cb) {
    display_update_cb(duh, render_result, rect);
  }
}

void RenderDisplay::current_scene_update(Scene *scene)
{
  if (current_scene_update_cb) {
    current_scene_update_cb(suh, scene);
  }
}

void RenderDisplay::stats_draw(RenderStats *render_stats)
{
  if (stats_draw_cb) {
    stats_draw_cb(sdh, render_stats);
  }
}

void RenderDisplay::progress(float progress)
{
  if (progress_cb) {
    progress_cb(prh, progress);
  }
}

void RenderDisplay::draw_lock()
{
  if (draw_lock_cb) {
    draw_lock_cb(dlh, true);
  }
}
void RenderDisplay::draw_unlock()
{
  if (draw_lock_cb) {
    draw_lock_cb(dlh, false);
  }
}

bool RenderDisplay::test_break()
{
  if (!test_break_cb) {
    return false;
  }

  return test_break_cb(tbh);
}

/** \} */
