/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * All XR functionality is accessed through a #GHOST_XrContext handle.
 * The lifetime of this context also determines the lifetime of the OpenXR instance, which is the
 * representation of the OpenXR runtime connection within the application.
 */

#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"

#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_screen.h"

#include "GHOST_C-api.h"

#include "GPU_platform.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"

#include "wm_surface.h"
#include "wm_xr_intern.h"

typedef struct {
  wmWindowManager *wm;
} wmXrErrorHandlerData;

/* -------------------------------------------------------------------- */

static void wm_xr_error_handler(const GHOST_XrError *error)
{
  wmXrErrorHandlerData *handler_data = error->customdata;
  wmWindowManager *wm = handler_data->wm;

  BKE_reports_clear(&wm->reports);
  WM_report(RPT_ERROR, error->user_message);
  WM_report_banner_show();

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

  /* Set up error handling */
  error_customdata.wm = wm;
  GHOST_XrErrorHandler(wm_xr_error_handler, &error_customdata);

  {
    const GHOST_TXrGraphicsBinding gpu_bindings_candidates[] = {
        GHOST_kXrGraphicsOpenGL,
#ifdef WIN32
        GHOST_kXrGraphicsD3D11,
#endif
    };
    GHOST_XrContextCreateInfo create_info = {
        .gpu_binding_candidates = gpu_bindings_candidates,
        .gpu_binding_candidates_count = ARRAY_SIZE(gpu_bindings_candidates),
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

    /* Set up context callbacks */
    GHOST_XrGraphicsContextBindFuncs(context,
                                     wm_xr_session_gpu_binding_context_create,
                                     wm_xr_session_gpu_binding_context_destroy);
    GHOST_XrDrawViewFunc(context, wm_xr_draw_view);

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
  if (wm->xr.runtime != NULL) {
    wm_xr_runtime_data_free(&wm->xr.runtime);
  }
  if (wm->xr.session_settings.shading.prop) {
    IDP_FreeProperty(wm->xr.session_settings.shading.prop);
    wm->xr.session_settings.shading.prop = NULL;
  }
}

bool wm_xr_events_handle(wmWindowManager *wm)
{
  if (wm->xr.runtime && wm->xr.runtime->context) {
    GHOST_XrEventsHandle(wm->xr.runtime->context);

    /* Process OpenXR action events. */
    if (WM_xr_session_is_ready(&wm->xr)) {
      wm_xr_session_actions_update(wm);
    }

    /* wm_window_process_events() uses the return value to determine if it can put the main thread
     * to sleep for some milliseconds. We never want that to happen while the VR session runs on
     * the main thread. So always return true. */
    return true;
  }
  return false;
}

/* -------------------------------------------------------------------- */
/** \name XR Runtime Data
 * \{ */

wmXrRuntimeData *wm_xr_runtime_data_create(void)
{
  wmXrRuntimeData *runtime = MEM_callocN(sizeof(*runtime), __func__);
  return runtime;
}

void wm_xr_runtime_data_free(wmXrRuntimeData **runtime)
{
  /* Note that this function may be called twice, because of an indirect recursion: If a session is
   * running while WM-XR calls this function, calling GHOST_XrContextDestroy() will call this
   * again, because it's also set as the session exit callback. So NULL-check and NULL everything
   * that is freed here. */

  /* We free all runtime XR data here, so if the context is still alive, destroy it. */
  if ((*runtime)->context != NULL) {
    GHOST_XrContextHandle context = (*runtime)->context;
    /* Prevent recursive GHOST_XrContextDestroy() call by NULL'ing the context pointer before the
     * first call, see comment above. */
    (*runtime)->context = NULL;

    if ((*runtime)->area) {
      wmWindowManager *wm = G_MAIN->wm.first;
      wmWindow *win = wm_xr_session_root_window_or_fallback_get(wm, (*runtime));
      ED_area_offscreen_free(wm, win, (*runtime)->area);
      (*runtime)->area = NULL;
    }
    wm_xr_session_data_free(&(*runtime)->session_state);
    WM_xr_actionmaps_clear(*runtime);

    GHOST_XrContextDestroy(context);
  }
  MEM_SAFE_FREE(*runtime);
}

/** \} */ /* XR Runtime Data */
