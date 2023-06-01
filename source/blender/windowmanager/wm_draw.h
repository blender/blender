/* SPDX-FileCopyrightText: 2007 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#pragma once

struct GPUOffScreen;
struct GPUTexture;
struct GPUViewport;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wmDrawBuffer {
  struct GPUOffScreen *offscreen;
  struct GPUViewport *viewport;
  bool stereo;
  int bound_view;
} wmDrawBuffer;

struct ARegion;
struct ScrArea;
struct bContext;
struct wmWindow;

/* wm_draw.c */

void wm_draw_update(struct bContext *C);
void wm_draw_region_clear(struct wmWindow *win, struct ARegion *region);
void wm_draw_region_blend(struct ARegion *region, int view, bool blend);
void wm_draw_region_test(struct bContext *C, struct ScrArea *area, struct ARegion *region);

struct GPUTexture *wm_draw_region_texture(struct ARegion *region, int view);

#ifdef __cplusplus
}
#endif
