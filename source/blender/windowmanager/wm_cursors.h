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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 */

#pragma once

struct wmEvent;
struct wmWindow;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum WMCursorType {
  WM_CURSOR_DEFAULT = 1,
  WM_CURSOR_TEXT_EDIT,
  WM_CURSOR_WAIT,
  WM_CURSOR_STOP,
  WM_CURSOR_EDIT,
  WM_CURSOR_COPY,
  WM_CURSOR_HAND,

  WM_CURSOR_CROSS,
  WM_CURSOR_PAINT,
  WM_CURSOR_DOT,
  WM_CURSOR_CROSSC,

  WM_CURSOR_KNIFE,
  WM_CURSOR_VERTEX_LOOP,
  WM_CURSOR_PAINT_BRUSH,
  WM_CURSOR_ERASER,
  WM_CURSOR_EYEDROPPER,

  WM_CURSOR_SWAP_AREA,
  WM_CURSOR_X_MOVE,
  WM_CURSOR_Y_MOVE,
  WM_CURSOR_H_SPLIT,
  WM_CURSOR_V_SPLIT,

  WM_CURSOR_NW_ARROW,
  WM_CURSOR_NS_ARROW,
  WM_CURSOR_EW_ARROW,
  WM_CURSOR_N_ARROW,
  WM_CURSOR_S_ARROW,
  WM_CURSOR_E_ARROW,
  WM_CURSOR_W_ARROW,

  WM_CURSOR_NSEW_SCROLL,
  WM_CURSOR_NS_SCROLL,
  WM_CURSOR_EW_SCROLL,

  WM_CURSOR_ZOOM_IN,
  WM_CURSOR_ZOOM_OUT,

  WM_CURSOR_NONE,

  /* --- ALWAYS LAST ----- */
  WM_CURSOR_NUM,
} WMCursorType;

void wm_init_cursor_data(void);
bool wm_cursor_arrow_move(struct wmWindow *win, const struct wmEvent *event);

#ifdef __cplusplus
}
#endif
