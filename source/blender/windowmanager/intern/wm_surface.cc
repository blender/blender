/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#include "BLI_listbase.h"
#ifndef NDEBUG
#  include "BLI_threads.h"
#endif

#include "BKE_global.hh"
#include "BKE_main.hh"

#include "GPU_context.hh"
#include "GPU_framebuffer.hh"

#include "MEM_guardedalloc.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm_surface.hh"

namespace blender {

static ListBaseT<wmSurface> global_surface_list = {nullptr, nullptr};
static wmSurface *g_drawable = nullptr;

static void wm_surface_constant_dpi_set_userpref()
{
  /* Ensure WM surfaces are always drawn at the same base constant pixel size. No matter the host
   * operating system, monitor, or parent Blender window.
   * NOTE: This function is analogous to #WM_window_dpi_set_userdef. Changes made in this
   *       function might need to be reproduced here. */

  U.dpi = 72.0f;

  U.pixelsize = 1.0f;
  U.virtual_pixel = VIRTUAL_PIXEL_NATIVE;

  U.scale_factor = 1.0f;
  U.inv_scale_factor = 1.0f;

  U.widget_unit = int(roundf(18.0f * U.scale_factor)) + (2 * U.pixelsize);
}

void wm_surfaces_iter(bContext *C, void (*cb)(bContext *C, wmSurface *))
{
  /* Mutable iterator in case a surface is freed. */
  for (wmSurface &surf : global_surface_list.items_mutable()) {
    cb(C, &surf);
  }
}

static void wm_surface_do_depsgraph_fn(bContext *C, wmSurface *surface)
{
  if (surface->do_depsgraph) {
    surface->do_depsgraph(C);
  }
}

void wm_surfaces_do_depsgraph(bContext *C)
{
  wm_surfaces_iter(C, wm_surface_do_depsgraph_fn);
}

void wm_surface_clear_drawable()
{
  if (g_drawable) {
    WM_system_gpu_context_release(g_drawable->system_gpu_context);
    GPU_context_active_set(nullptr);

    if (g_drawable->deactivate) {
      g_drawable->deactivate();
    }

    g_drawable = nullptr;

    /* Workaround: For surface drawing, the Userdef runtime DPI/pixelsize values are set to
     * base constants in #wm_surface_constant_dpi_set_userpref called in #wm_surface_make_drawable.
     * This does not affect window rendering as #WM_window_dpi_set_userdef is called in
     * #wm_window_make_drawable. However, some handlers called before window re-draw (such as
     * window popups) call drawing code and thus rely on correct system DPI runtime values.
     *
     * Workaround this issue by restoring the DPI runtime value on surface drawable clear.
     * To match the previous value, the last window is used (as windows are iterated and set in
     * order in #wm_draw_update before drawing surfaces). */
    wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
    WM_window_dpi_set_userdef(static_cast<wmWindow *>(wm->windows.last));
  }
}

void wm_surface_set_drawable(wmSurface *surface, bool activate)
{
  BLI_assert(ELEM(g_drawable, nullptr, surface));

  g_drawable = surface;
  if (activate) {
    if (surface->activate) {
      surface->activate();
    }
    WM_system_gpu_context_activate(surface->system_gpu_context);
  }

  GPU_context_active_set(surface->blender_gpu_context);
}

void wm_surface_make_drawable(wmSurface *surface)
{
  BLI_assert(GPU_framebuffer_active_get() == GPU_framebuffer_back_get());

  if (surface != g_drawable) {
    wm_surface_clear_drawable();
    wm_surface_set_drawable(surface, true);
    wm_surface_constant_dpi_set_userpref();
  }
}

void wm_surface_reset_drawable()
{
  BLI_assert(BLI_thread_is_main());
  BLI_assert(GPU_framebuffer_active_get() == GPU_framebuffer_back_get());

  if (g_drawable) {
    wm_surface_clear_drawable();
    wm_surface_set_drawable(g_drawable, true);
  }
}

void wm_surface_add(wmSurface *surface)
{
  BLI_addtail(&global_surface_list, surface);
}

void wm_surface_remove(wmSurface *surface)
{
  BLI_remlink(&global_surface_list, surface);
  /* Ensure GPU context is bound to free GPU resources. */
  wm_surface_make_drawable(surface);
  surface->free_data(surface);
  wm_surface_clear_drawable();
  MEM_delete(surface);
}

void wm_surfaces_free()
{
  for (wmSurface &surf : global_surface_list.items_mutable()) {
    wm_surface_remove(&surf);
  }

  BLI_assert(BLI_listbase_is_empty(&global_surface_list));
}

}  // namespace blender
