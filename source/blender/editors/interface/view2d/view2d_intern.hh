/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Share between view2d_*.cc files.
 */

#pragma once

#include "DNA_vec_types.h"

struct bContext;
struct View2D;

struct View2DScrollers {
  /* focus bubbles */
  int vert_min, vert_max; /* vertical scroll-bar */
  int hor_min, hor_max;   /* horizontal scroll-bar */

  /** Exact size of slider backdrop. */
  rcti hor, vert;
};

/**
 * Calculate relevant scroller properties.
 */
void view2d_scrollers_calc(View2D *v2d, const rcti *mask_custom, View2DScrollers *r_scrollers);

/**
 * Change the size of the maximum viewable area (i.e. 'tot' rect).
 */
void view2d_totRect_set_resize(View2D *v2d, int width, int height, bool resize);

bool view2d_edge_pan_poll(bContext *C);

/**
 * For paginated scrolling, get the page height to scroll. This may be a custom height
 * (#View2D.page_size_y) but defaults to the #View2D.mask height.
 */
float view2d_page_size_y(const View2D &v2d);
