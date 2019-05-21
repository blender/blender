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
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 */

#ifndef __WM_DRAW_H__
#define __WM_DRAW_H__

#include "GPU_glew.h"

struct GPUOffScreen;
struct GPUTexture;
struct GPUViewport;

typedef struct wmDrawBuffer {
  struct GPUOffScreen *offscreen[2];
  struct GPUViewport *viewport[2];
  bool stereo;
  int bound_view;
} wmDrawBuffer;

struct ARegion;
struct ScrArea;
struct bContext;
struct wmWindow;

/* wm_draw.c */
void wm_draw_update(struct bContext *C);
void wm_draw_region_clear(struct wmWindow *win, struct ARegion *ar);
void wm_draw_region_blend(struct ARegion *ar, int view, bool blend);
void wm_draw_region_test(struct bContext *C, struct ScrArea *sa, struct ARegion *ar);

struct GPUTexture *wm_draw_region_texture(struct ARegion *ar, int view);

#endif /* __WM_DRAW_H__ */
