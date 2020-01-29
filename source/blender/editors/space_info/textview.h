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
 * \ingroup spinfo
 */

#ifndef __TEXTVIEW_H__
#define __TEXTVIEW_H__

typedef struct TextViewContext {
  /** Font size scaled by the interface size. */
  int lheight;
  /** Text selection, when a selection range is in use. */
  int sel_start, sel_end;

  /* view settings */
  int cwidth;  /* shouldnt be needed! */
  int columns; /* shouldnt be needed! */

  int row_vpadding;
  int margin_left_chars;
  int margin_right_chars;

  /** Area to draw: (0, 0, winx, winy) with a margin applied and scroll-bar subtracted. */
  rcti draw_rect;

  /** Scroll offset in pixels. */
  int scroll_ymin, scroll_ymax;

  /* callbacks */
  int (*begin)(struct TextViewContext *tvc);
  void (*end)(struct TextViewContext *tvc);
  void *arg1;
  void *arg2;

  /* iterator */
  int (*step)(struct TextViewContext *tvc);
  int (*line_get)(struct TextViewContext *tvc, const char **, int *);
  int (*line_data)(struct TextViewContext *tvc,
                   unsigned char fg[4],
                   unsigned char bg[4],
                   int *icon,
                   unsigned char icon_fg[4],
                   unsigned char icon_bg[4]);
  void (*draw_cursor)(struct TextViewContext *tvc);
  /* constant theme colors */
  void (*const_colors)(struct TextViewContext *tvc, unsigned char bg_sel[4]);
  void *iter;
  int iter_index;
  /** Char index, used for multi-line report display. */
  int iter_char;
  /** Same as 'iter_char', next new-line. */
  int iter_char_next;
  /** Internal iterator use. */
  int iter_tmp;

} TextViewContext;

int textview_draw(struct TextViewContext *tvc,
                  const bool do_draw,
                  const int mval_init[2],
                  void **r_mval_pick_item,
                  int *r_mval_pick_offset);

enum {
  TVC_LINE_FG = (1 << 0),
  TVC_LINE_BG = (1 << 1),
  TVC_LINE_ICON = (1 << 2),
  TVC_LINE_ICON_FG = (1 << 3),
  TVC_LINE_ICON_BG = (1 << 4)
};

#endif /* __TEXTVIEW_H__ */
