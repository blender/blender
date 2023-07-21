/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#include "BKE_context.h"

#include "BLF_api.h"

#include "BLI_listbase.h"
#include "BLI_threads.h"

#include "GHOST_C-api.h"

#include "GPU_batch_presets.h"
#include "GPU_context.h"
#include "GPU_framebuffer.h"
#include "GPU_immediate.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"

#include "wm_surface.h"

static ListBase global_surface_list = {nullptr, nullptr};
static wmSurface *g_drawable = nullptr;

void wm_surfaces_iter(bContext *C, void (*cb)(bContext *C, wmSurface *))
{
  /* Mutable iterator in case a surface is freed. */
  LISTBASE_FOREACH_MUTABLE (wmSurface *, surf, &global_surface_list) {
    cb(C, surf);
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
  if (surface == g_drawable) {
    wm_surface_clear_drawable();
  }
  BLI_remlink(&global_surface_list, surface);
  surface->free_data(surface);
  MEM_freeN(surface);
}

void wm_surfaces_free()
{
  wm_surface_clear_drawable();

  LISTBASE_FOREACH_MUTABLE (wmSurface *, surf, &global_surface_list) {
    wm_surface_remove(surf);
  }

  BLI_assert(BLI_listbase_is_empty(&global_surface_list));
}
