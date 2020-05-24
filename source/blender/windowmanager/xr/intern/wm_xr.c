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
 */

/** \file
 * \ingroup wm
 *
 * All XR functionality is accessed through a #GHOST_XrContext handle.
 * The lifetime of this context also determines the lifetime of the OpenXR instance, which is the
 * representation of the OpenXR runtime connection within the application.
 */

#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_report.h"

#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "DEG_depsgraph.h"

#include "MEM_guardedalloc.h"

#include "GHOST_C-api.h"

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

    /* wm_window_process_events() uses the return value to determine if it can put the main thread
     * to sleep for some milliseconds. We never want that to happen while the VR session runs on
     * the main thread. So always return true. */
    return true;
  }
  return false;
}

/* -------------------------------------------------------------------- */
/** \name XR Runtime Data
 *
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
    GHOST_XrContextDestroy(context);
  }
  MEM_SAFE_FREE(*runtime);
}

/** \} */ /* XR Runtime Data */
