/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * All XR functionality is accessed through a #GHOST_XrContext handle.
 * The lifetime of this context also determines the lifetime of the OpenXR instance, which is the
 * representation of the OpenXR runtime connection within the application.
 */

#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_screen.hh"

#include "GHOST_C-api.h"

#include "GPU_context.hh"

#include "MEM_guardedalloc.h"

#include "WM_api.hh"

#include "wm_xr_intern.hh"

struct wmXrErrorHandlerData {
  wmWindowManager *wm;
};

/* -------------------------------------------------------------------- */

static void wm_xr_error_handler(const GHOST_XrError *error)
{
  wmXrErrorHandlerData *handler_data = static_cast<wmXrErrorHandlerData *>(error->customdata);
  wmWindowManager *wm = handler_data->wm;
  wmWindow *root_win = wm->xr.runtime ? wm->xr.runtime->session_root_win : nullptr;

  BKE_reports_clear(&wm->runtime->reports);
  WM_global_report(RPT_ERROR, error->user_message);
  /* Rely on the fallback when `root_win` is nullptr. */
  WM_report_banner_show(wm, root_win);

  if (wm->xr.runtime) {
    /* Just play safe and destroy the entire runtime data, including context. */
    wm_xr_runtime_data_free(&wm->xr.runtime);
  }
}

bool wm_xr_init(wmWindowManager *wm)
{
  if (wm->xr.runtime && wm->xr.runtime->context) {
    return true;
  }
  static wmXrErrorHandlerData error_customdata;

  /* Set up error handling. */
  error_customdata.wm = wm;
  GHOST_XrErrorHandler(wm_xr_error_handler, &error_customdata);

  {
    blender::Vector<GHOST_TXrGraphicsBinding> gpu_bindings_candidates;
    switch (GPU_backend_get_type()) {
#ifdef WITH_OPENGL_BACKEND
      case GPU_BACKEND_OPENGL:
        gpu_bindings_candidates.append(GHOST_kXrGraphicsOpenGL);
#  ifdef WIN32
        gpu_bindings_candidates.append(GHOST_kXrGraphicsOpenGLD3D11);
#  endif
        break;
#endif

#ifdef WITH_VULKAN_BACKEND
      case GPU_BACKEND_VULKAN:
        gpu_bindings_candidates.append(GHOST_kXrGraphicsVulkan);
#  ifdef WIN32
        gpu_bindings_candidates.append(GHOST_kXrGraphicsVulkanD3D11);
#  endif
        break;
#endif

#ifdef WITH_METAL_BACKEND
      case GPU_BACKEND_METAL:
        gpu_bindings_candidates.append(GHOST_kXrGraphicsMetal);
        break;
#endif

      default:
        break;
    }

    GHOST_XrContextCreateInfo create_info{
        /*gpu_binding_candidates*/ gpu_bindings_candidates.data(),
        /*gpu_binding_candidates_count*/ uint32_t(gpu_bindings_candidates.size()),
    };
    GHOST_XrContextHandle context;

    if (G.debug & G_DEBUG_XR) {
      create_info.context_flag |= GHOST_kXrContextDebug;
    }
    if (G.debug & G_DEBUG_XR_TIME) {
      create_info.context_flag |= GHOST_kXrContextDebugTime;
    }
#ifdef WIN32
    if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_WIN, GPU_DRIVER_ANY)) {
      create_info.context_flag |= GHOST_kXrContextGpuNVIDIA;
    }
#endif

    if (!(context = GHOST_XrContextCreate(&create_info))) {
      return false;
    }

    /* Set up context callbacks. */
    GHOST_XrGraphicsContextBindFuncs(context,
                                     wm_xr_session_gpu_binding_context_create,
                                     wm_xr_session_gpu_binding_context_destroy);
    GHOST_XrDrawViewFunc(context, wm_xr_draw_view);
    GHOST_XrPassthroughEnabledFunc(context, wm_xr_passthrough_enabled);
    GHOST_XrDisablePassthroughFunc(context, wm_xr_disable_passthrough);

    if (!wm->xr.runtime) {
      wm->xr.runtime = wm_xr_runtime_data_create();
      wm->xr.runtime->context = context;
    }
  }
  BLI_assert(wm->xr.runtime && wm->xr.runtime->context);

  return true;
}

void wm_xr_exit(wmWindowManager *wm)
{
  if (wm->xr.runtime != nullptr) {
    wm_xr_runtime_data_free(&wm->xr.runtime);
  }

  /* See #wm_xr_data_free for logic that frees window-manager XR data
   * that may exist even when built without XR. */
}

bool wm_xr_events_handle(wmWindowManager *wm)
{
  if (wm->xr.runtime && wm->xr.runtime->context) {
    GHOST_XrEventsHandle(wm->xr.runtime->context);

    /* Process OpenXR action events. */
    if (WM_xr_session_is_ready(&wm->xr)) {
      wm_xr_session_actions_update(wm);
    }

    /* #wm_window_events_process() uses the return value to determine if it can put the main thread
     * to sleep for some milliseconds. We never want that to happen while the VR session runs on
     * the main thread. So always return true. */
    return true;
  }
  return false;
}

/* -------------------------------------------------------------------- */
/** \name XR Runtime Data
 * \{ */

wmXrRuntimeData *wm_xr_runtime_data_create()
{
  wmXrRuntimeData *runtime = MEM_callocN<wmXrRuntimeData>(__func__);
  return runtime;
}

void wm_xr_runtime_data_free(wmXrRuntimeData **runtime)
{
  /* Note that this function may be called twice, because of an indirect recursion: If a session is
   * running while WM-XR calls this function, calling GHOST_XrContextDestroy() will call this
   * again, because it's also set as the session exit callback. So nullptr-check and nullptr
   * everything that is freed here. */

  /* We free all runtime XR data here, so if the context is still alive, destroy it. */
  if ((*runtime)->context != nullptr) {
    GHOST_XrContextHandle context = (*runtime)->context;
    /* Prevent recursive #GHOST_XrContextDestroy() call by nulling the context pointer before
     * the first call, see comment above. */
    (*runtime)->context = nullptr;

    if ((*runtime)->area) {
      wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
      wmWindow *win = wm_xr_session_root_window_or_fallback_get(wm, (*runtime));

      WM_event_remove_handlers_by_area(&win->handlers, (*runtime)->area);
      ED_area_offscreen_free(wm, win, (*runtime)->area);
      (*runtime)->area = nullptr;
    }
    wm_xr_session_data_free(&(*runtime)->session_state);
    WM_xr_actionmaps_clear(*runtime);

    GHOST_XrContextDestroy(context);
  }
  MEM_SAFE_FREE(*runtime);
}

/** \} */ /* XR Runtime Data. */
