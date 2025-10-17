/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spinfo
 */

#pragma once

#include "BLI_enum_flags.hh"

#include "DNA_vec_types.h"

#define TVC_TAB_COLUMNS 4

enum eTextViewContext_LineFlag {
  TVC_LINE_FG = (1 << 0),
  TVC_LINE_BG = (1 << 1),
  TVC_LINE_ICON = (1 << 2),
  TVC_LINE_ICON_FG = (1 << 3),
  TVC_LINE_ICON_BG = (1 << 4)
};
ENUM_OPERATORS(eTextViewContext_LineFlag)

struct TextViewContext {
  /** Font size scaled by the interface size. */
  int lheight;
  /** Text selection, when a selection range is in use. */
  int sel_start, sel_end;

  int row_vpadding;

  /** Area to draw text: `(0, 0, winx, winy)` with a margin applied and scroll-bar subtracted. */
  rcti draw_rect;
  /** Area to draw text background colors (extending beyond text in some cases). */
  rcti draw_rect_outer;

  /** Scroll offset in pixels. */
  int scroll_ymin, scroll_ymax;

  /* callbacks */
  int (*begin)(TextViewContext *tvc);
  void (*end)(TextViewContext *tvc);
  const void *arg1;
  const void *arg2;

  /* iterator */
  int (*step)(TextViewContext *tvc);
  void (*line_get)(TextViewContext *tvc, const char **r_line, int *r_len);
  enum eTextViewContext_LineFlag (*line_data)(TextViewContext *tvc,
                                              uchar fg[4],
                                              uchar bg[4],
                                              int *r_icon,
                                              uchar r_icon_fg[4],
                                              uchar r_icon_bg[4]);
  void (*draw_cursor)(TextViewContext *tvc, int cwidth, int columns);
  /* constant theme colors */
  void (*const_colors)(TextViewContext *tvc, unsigned char bg_sel[4]);
  const void *iter;
  int iter_index;
  /** Used for internal multi-line iteration. */
  int iter_char_begin;
  /** The last character (not inclusive). */
  int iter_char_end;
  /** Internal iterator use. */
  int iter_tmp;
};

/**
 * \param r_mval_pick_item: The resulting item clicked on using \a mval_init.
 * Set from the void pointer which holds the current iterator.
 * Its type depends on the data being iterated over.
 * \param r_mval_pick_offset: The offset in bytes of the \a mval_init.
 * Use for selection.
 */
int textview_draw(TextViewContext *tvc,
                  bool do_draw,
                  const int mval_init[2],
                  void **r_mval_pick_item,
                  int *r_mval_pick_offset);
