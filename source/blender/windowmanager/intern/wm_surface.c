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

static ListBase global_surface_list = {NULL, NULL};
static wmSurface *g_drawable = NULL;

void wm_surfaces_iter(bContext *C, void (*cb)(bContext *C, wmSurface *))
{
  LISTBASE_FOREACH (wmSurface *, surf, &global_surface_list) {
    cb(C, surf);
  }
}

void wm_surface_clear_drawable(void)
{
  if (g_drawable) {
    WM_opengl_context_release(g_drawable->ghost_ctx);
    GPU_context_active_set(NULL);

    BLF_batch_reset();
    gpu_batch_presets_reset();
    immDeactivate();

    if (g_drawable->deactivate) {
      g_drawable->deactivate();
    }

    g_drawable = NULL;
  }
}

void wm_surface_set_drawable(wmSurface *surface, bool activate)
{
  BLI_assert(ELEM(g_drawable, NULL, surface));

  g_drawable = surface;
  if (activate) {
    if (surface->activate) {
      surface->activate();
    }
    WM_opengl_context_activate(surface->ghost_ctx);
  }

  GPU_context_active_set(surface->gpu_ctx);
  immActivate();
}

void wm_surface_make_drawable(wmSurface *surface)
{
  BLI_assert(GPU_framebuffer_active_get() == NULL);

  if (surface != g_drawable) {
    wm_surface_clear_drawable();
    wm_surface_set_drawable(surface, true);
  }
}

void wm_surface_reset_drawable(void)
{
  BLI_assert(BLI_thread_is_main());
  BLI_assert(GPU_framebuffer_active_get() == NULL);

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
  surface->free_data(surface);
  MEM_freeN(surface);
}

void wm_surfaces_free(void)
{
  wm_surface_clear_drawable();

  for (wmSurface *surf = global_surface_list.first, *surf_next; surf; surf = surf_next) {
    surf_next = surf->next;
    wm_surface_remove(surf);
  }

  BLI_assert(BLI_listbase_is_empty(&global_surface_list));
}
