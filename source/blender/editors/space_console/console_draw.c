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
 * \ingroup spconsole
 */

#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "GPU_immediate.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "console_intern.h"

#include "../space_info/textview.h"

static int console_line_data(struct TextViewContext *tvc,
                             uchar fg[4],
                             uchar UNUSED(bg[4]),
                             int *UNUSED(icon),
                             uchar UNUSED(icon_fg[4]),
                             uchar UNUSED(icon_bg[4]))
{
  ConsoleLine *cl_iter = (ConsoleLine *)tvc->iter;
  int fg_id = TH_TEXT;

  switch (cl_iter->type) {
    case CONSOLE_LINE_OUTPUT:
      fg_id = TH_CONSOLE_OUTPUT;
      break;
    case CONSOLE_LINE_INPUT:
      fg_id = TH_CONSOLE_INPUT;
      break;
    case CONSOLE_LINE_INFO:
      fg_id = TH_CONSOLE_INFO;
      break;
    case CONSOLE_LINE_ERROR:
      fg_id = TH_CONSOLE_ERROR;
      break;
  }

  UI_GetThemeColor4ubv(fg_id, fg);
  return TVC_LINE_FG;
}

void console_scrollback_prompt_begin(struct SpaceConsole *sc, ConsoleLine *cl_dummy)
{
  /* fake the edit line being in the scroll buffer */
  ConsoleLine *cl = sc->history.last;
  int prompt_len = strlen(sc->prompt);

  cl_dummy->type = CONSOLE_LINE_INPUT;
  cl_dummy->len = prompt_len + cl->len;
  cl_dummy->len_alloc = cl_dummy->len + 1;
  cl_dummy->line = MEM_mallocN(cl_dummy->len_alloc, "cl_dummy");
  memcpy(cl_dummy->line, sc->prompt, prompt_len);
  memcpy(cl_dummy->line + prompt_len, cl->line, cl->len + 1);
  BLI_addtail(&sc->scrollback, cl_dummy);
}
void console_scrollback_prompt_end(struct SpaceConsole *sc, ConsoleLine *cl_dummy)
{
  MEM_freeN(cl_dummy->line);
  BLI_remlink(&sc->scrollback, cl_dummy);
}

/* console textview callbacks */
static int console_textview_begin(TextViewContext *tvc)
{
  SpaceConsole *sc = (SpaceConsole *)tvc->arg1;
  tvc->lheight = sc->lheight * UI_DPI_FAC;
  tvc->sel_start = sc->sel_start;
  tvc->sel_end = sc->sel_end;

  /* iterator */
  tvc->iter = sc->scrollback.last;

  return (tvc->iter != NULL);
}

static void console_textview_end(TextViewContext *tvc)
{
  SpaceConsole *sc = (SpaceConsole *)tvc->arg1;
  (void)sc;
}

static int console_textview_step(TextViewContext *tvc)
{
  return ((tvc->iter = (void *)((Link *)tvc->iter)->prev) != NULL);
}

static int console_textview_line_get(struct TextViewContext *tvc, const char **line, int *len)
{
  ConsoleLine *cl = (ConsoleLine *)tvc->iter;
  *line = cl->line;
  *len = cl->len;
  // printf("'%s' %d\n", *line, cl->len);
  BLI_assert(cl->line[cl->len] == '\0' && (cl->len == 0 || cl->line[cl->len - 1] != '\0'));
  return 1;
}

static void console_cursor_wrap_offset(
    const char *str, int width, int *row, int *column, const char *end)
{
  int col;

  for (; *str; str += BLI_str_utf8_size_safe(str)) {
    col = BLI_str_utf8_char_width_safe(str);

    if (*column + col > width) {
      (*row)++;
      *column = 0;
    }

    if (end && str >= end) {
      break;
    }

    *column += col;
  }
  return;
}

static void console_textview_draw_cursor(struct TextViewContext *tvc,
                                         int cwidth,
                                         int columns,
                                         int descender)
{
  int pen[2];
  {
    const SpaceConsole *sc = (SpaceConsole *)tvc->arg1;
    const ConsoleLine *cl = (ConsoleLine *)sc->history.last;
    int offl = 0, offc = 0;

    console_cursor_wrap_offset(sc->prompt, columns, &offl, &offc, NULL);
    console_cursor_wrap_offset(cl->line, columns, &offl, &offc, cl->line + cl->cursor);
    pen[0] = cwidth * offc;
    pen[1] = -2 - (tvc->lheight + descender) * offl;

    console_cursor_wrap_offset(cl->line + cl->cursor, columns, &offl, &offc, NULL);
    pen[1] += (tvc->lheight + descender) * offl;

    pen[0] += tvc->draw_rect.xmin;
    pen[1] += tvc->draw_rect.ymin;
  }

  /* cursor */
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformThemeColor(TH_CONSOLE_CURSOR);

  immRectf(
      pos, pen[0] - U.pixelsize, pen[1], pen[0] + U.pixelsize, pen[1] + tvc->lheight + descender);

  immUnbindProgram();
}

static void console_textview_const_colors(TextViewContext *UNUSED(tvc), uchar bg_sel[4])
{
  UI_GetThemeColor4ubv(TH_CONSOLE_SELECT, bg_sel);
}

static void console_textview_draw_rect_calc(const ARegion *region,
                                            rcti *r_draw_rect,
                                            rcti *r_draw_rect_outer)
{
  const int margin = 4 * UI_DPI_FAC;
  r_draw_rect->xmin = margin;
  r_draw_rect->xmax = region->winx - V2D_SCROLL_WIDTH;
  r_draw_rect->ymin = margin;
  /* No margin at the top (allow text to scroll off the window). */
  r_draw_rect->ymax = region->winy;

  r_draw_rect_outer->xmin = 0;
  r_draw_rect_outer->xmax = region->winx;
  r_draw_rect_outer->ymin = 0;
  r_draw_rect_outer->ymax = region->winy;
}

static int console_textview_main__internal(struct SpaceConsole *sc,
                                           const ARegion *region,
                                           const bool do_draw,
                                           const int mval[2],
                                           void **r_mval_pick_item,
                                           int *r_mval_pick_offset)
{
  ConsoleLine cl_dummy = {NULL};
  int ret = 0;

  const View2D *v2d = &region->v2d;

  TextViewContext tvc = {0};

  tvc.begin = console_textview_begin;
  tvc.end = console_textview_end;

  tvc.step = console_textview_step;
  tvc.line_get = console_textview_line_get;
  tvc.line_data = console_line_data;
  tvc.draw_cursor = console_textview_draw_cursor;
  tvc.const_colors = console_textview_const_colors;

  tvc.arg1 = sc;
  tvc.arg2 = NULL;

  /* view */
  tvc.sel_start = sc->sel_start;
  tvc.sel_end = sc->sel_end;
  tvc.lheight = sc->lheight * 1.2f * UI_DPI_FAC;
  tvc.scroll_ymin = v2d->cur.ymin;
  tvc.scroll_ymax = v2d->cur.ymax;

  console_textview_draw_rect_calc(region, &tvc.draw_rect, &tvc.draw_rect_outer);

  console_scrollback_prompt_begin(sc, &cl_dummy);
  ret = textview_draw(&tvc, do_draw, mval, r_mval_pick_item, r_mval_pick_offset);
  console_scrollback_prompt_end(sc, &cl_dummy);

  return ret;
}

void console_textview_main(struct SpaceConsole *sc, const ARegion *region)
{
  const int mval[2] = {INT_MAX, INT_MAX};
  console_textview_main__internal(sc, region, true, mval, NULL, NULL);
}

int console_textview_height(struct SpaceConsole *sc, const ARegion *region)
{
  const int mval[2] = {INT_MAX, INT_MAX};
  return console_textview_main__internal(sc, region, false, mval, NULL, NULL);
}

int console_char_pick(struct SpaceConsole *sc, const ARegion *region, const int mval[2])
{
  int r_mval_pick_offset = 0;
  void *mval_pick_item = NULL;

  console_textview_main__internal(sc, region, false, mval, &mval_pick_item, &r_mval_pick_offset);
  return r_mval_pick_offset;
}
