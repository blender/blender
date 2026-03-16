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

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_screen.hh"

#include "GHOST_IXrContext.hh"
#include "GHOST_Types.hh"
#include "GHOST_Xr-api.hh"

#include "GPU_context.hh"

#include "MEM_guardedalloc.h"

#include "WM_api.hh"

#include "wm_xr_intern.hh"

namespace blender {

struct wmXrErrorHandlerData {
  wmWindowManager *wm;
};

/* -------------------------------------------------------------------- */

static void wm_xr_error_handler(const GHOST_XrError *error)
{
  wmXrErrorHandlerData *handler_data = static_cast<wmXrErrorHandlerData *>(error->customdata);
  wmWindowManager *wm = handler_data->wm;
  wmWindow *xr_root_win = CTX_wm_window(wm->xr.runtime->b_context);

  BKE_reports_clear(&wm->runtime->reports);
  WM_global_report(RPT_ERROR, error->user_message);
  /* Internally rely on the first WM window as a fallback when `xr_root_win` is nullptr. */
  WM_report_banner_show(wm, xr_root_win);

  if (wm->xr.runtime) {
    /* Just play safe and destroy the entire runtime data, including context. */
    wm_xr_runtime_data_free(&wm->xr.runtime);
  }
}

bool wm_xr_init(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm->xr.runtime && wm->xr.runtime->ghost_context) {
    return true;
  }
  static wmXrErrorHandlerData error_customdata;

  /* Set up error handling. */
  error_customdata.wm = wm;
  GHOST_XrErrorHandler(wm_xr_error_handler, &error_customdata);

  {
    Vector<GHOST_TXrGraphicsBinding> gpu_bindings_candidates;
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

    GHOST_IXrContext *ghost_context;
    if (!(ghost_context = GHOST_XrContextCreate(&create_info))) {
      return false;
    }

    /* Set up context callbacks. */
    GHOST_XrGraphicsContextBindFuncs(ghost_context,
                                     wm_xr_session_gpu_binding_context_create,
                                     wm_xr_session_gpu_binding_context_destroy);
    GHOST_XrDrawViewFunc(ghost_context, wm_xr_draw_view);
    GHOST_XrPassthroughEnabledFunc(ghost_context, wm_xr_passthrough_enabled);
    GHOST_XrDisablePassthroughFunc(ghost_context, wm_xr_disable_passthrough);

    if (!wm->xr.runtime) {
      wm->xr.runtime = wm_xr_runtime_data_create();
      wm->xr.runtime->ghost_context = ghost_context;

      /* Create a minimal XR-specific context. */
      wm->xr.runtime->b_context = CTX_create();

      /* Base Main and WM pointers. */
      CTX_wm_manager_set(wm->xr.runtime->b_context, CTX_wm_manager(C));
      CTX_data_main_set(wm->xr.runtime->b_context, CTX_data_main(C));

      /* Create the XR offscreen area (independent of any bScreen). */
      wm->xr.runtime->offscreen_area = ED_area_offscreen_create(CTX_wm_window(C), SPACE_VIEW3D);
      WM_xr_session_context_ensure(&wm->xr, wm);
    }
  }
  BLI_assert(wm->xr.runtime && wm->xr.runtime->ghost_context && wm->xr.runtime->b_context);

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
  if (wm->xr.runtime && wm->xr.runtime->ghost_context) {
    GHOST_XrEventsHandle(wm->xr.runtime->ghost_context);

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
  wmXrRuntimeData *runtime = MEM_new_zeroed<wmXrRuntimeData>(__func__);
  return runtime;
}

void wm_xr_runtime_data_free(wmXrRuntimeData **runtime)
{
  /* This function may be called recursively via the #GHOST_XrContextDestroy session exit callback.
   * Guard against double-free by nulling pointers after freeing. */

  /* Destroy context if still alive. */
  if ((*runtime)->ghost_context != nullptr) {
    GHOST_IXrContext *ghost_context = (*runtime)->ghost_context;
    /* Set to nullptr before calling XrContextDestroy to prevent recursive calls. */
    (*runtime)->ghost_context = nullptr;

    GHOST_XrContextDestroy(ghost_context);
  }

  /* Free remaining runtime data. */
  if (*runtime != nullptr) {
    ScrArea *xr_offscreen_area = (*runtime)->offscreen_area;
    BLI_assert(xr_offscreen_area);

    wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
    wmWindow *xr_win = wm_xr_session_root_window_or_fallback_get(wm, (*runtime));
    WM_event_remove_handlers_by_area(&xr_win->runtime->handlers, xr_offscreen_area);
    ED_area_offscreen_free(wm, xr_win, xr_offscreen_area);

    CTX_free((*runtime)->b_context);

    wm_xr_session_data_free(&(*runtime)->session_state);
    WM_xr_actionmaps_clear(*runtime);

    MEM_SAFE_DELETE(*runtime);
    *runtime = nullptr;
  }
}

/** \} */ /* XR Runtime Data. */

}  // namespace blender
