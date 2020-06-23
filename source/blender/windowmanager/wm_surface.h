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
 * \name WM-Surface
 *
 * Container to manage painting in an offscreen context.
 */

#ifndef __WM_SURFACE_H__
#define __WM_SURFACE_H__

struct bContext;

typedef struct wmSurface {
  struct wmSurface *next, *prev;

  GHOST_ContextHandle ghost_ctx;
  struct GPUContext *gpu_ctx;

  void *customdata;

  void (*draw)(struct bContext *);
  /** Free customdata, not the surface itself (done by wm_surface API) */
  void (*free_data)(struct wmSurface *);

  /** Called  when surface is activated for drawing (made drawable). */
  void (*activate)(void);
  /** Called  when surface is deactivated for drawing (current drawable cleared). */
  void (*deactivate)(void);
} wmSurface;

/* Create/Free */
void wm_surface_add(wmSurface *surface);
void wm_surface_remove(wmSurface *surface);
void wm_surfaces_free(void);

/* Utils */
void wm_surfaces_iter(struct bContext *C, void (*cb)(struct bContext *, wmSurface *));

/* Drawing */
void wm_surface_make_drawable(wmSurface *surface);
void wm_surface_clear_drawable(void);
void wm_surface_set_drawable(wmSurface *surface, bool activate);
void wm_surface_reset_drawable(void);

#endif /* __WM_SURFACE_H__ */
