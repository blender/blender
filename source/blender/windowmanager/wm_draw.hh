/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#pragma once

struct ARegion;
struct GPUOffScreen;
struct GPUTexture;
struct GPUViewport;
struct ScrArea;
struct bContext;
struct wmWindow;

struct wmDrawBuffer {
  GPUOffScreen *offscreen;
  GPUViewport *viewport;
  bool stereo;
  int bound_view;
};

/* `wm_draw.cc` */

void wm_draw_update(bContext *C);
void wm_draw_region_clear(wmWindow *win, ARegion *region);
void wm_draw_region_blend(ARegion *region, int view, bool blend);
void wm_draw_region_test(bContext *C, ScrArea *area, ARegion *region);

GPUTexture *wm_draw_region_texture(ARegion *region, int view);
