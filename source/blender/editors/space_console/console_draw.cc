/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spconsole
 */

#include <cstring>

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "GPU_immediate.hh"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "console_intern.hh"

#include "../space_info/textview.hh"

static enum eTextViewContext_LineFlag console_line_data(TextViewContext *tvc,
                                                        uchar fg[4],
                                                        uchar /*bg*/[4],
                                                        int * /*icon*/,
                                                        uchar /*icon_fg*/[4],
                                                        uchar /*icon_bg*/[4])
{
  const ConsoleLine *cl_iter = static_cast<const ConsoleLine *>(tvc->iter);
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

void console_scrollback_prompt_begin(SpaceConsole *sc, ConsoleLine *cl_dummy)
{
  /* fake the edit line being in the scroll buffer */
  ConsoleLine *cl = static_cast<ConsoleLine *>(sc->history.last);
  int prompt_len = strlen(sc->prompt);

  cl_dummy->type = CONSOLE_LINE_INPUT;
  cl_dummy->len = prompt_len + cl->len;
  cl_dummy->len_alloc = cl_dummy->len + 1;
  cl_dummy->line = MEM_malloc_arrayN<char>(cl_dummy->len_alloc, "cl_dummy");
  memcpy(cl_dummy->line, sc->prompt, prompt_len);
  memcpy(cl_dummy->line + prompt_len, cl->line, cl->len + 1);
  BLI_addtail(&sc->scrollback, cl_dummy);
}
void console_scrollback_prompt_end(SpaceConsole *sc, ConsoleLine *cl_dummy)
{
  MEM_freeN(cl_dummy->line);
  BLI_remlink(&sc->scrollback, cl_dummy);
}

/* console textview callbacks */
static int console_textview_begin(TextViewContext *tvc)
{
  SpaceConsole *sc = (SpaceConsole *)tvc->arg1;
  tvc->sel_start = sc->sel_start;
  tvc->sel_end = sc->sel_end;

  /* iterator */
  tvc->iter = sc->scrollback.last;

  return (tvc->iter != nullptr);
}

static void console_textview_end(TextViewContext *tvc)
{
  SpaceConsole *sc = (SpaceConsole *)tvc->arg1;
  (void)sc;
}

static int console_textview_step(TextViewContext *tvc)
{
  return ((tvc->iter = (void *)((Link *)tvc->iter)->prev) != nullptr);
}

static void console_textview_line_get(TextViewContext *tvc, const char **r_line, int *r_len)
{
  const ConsoleLine *cl = static_cast<const ConsoleLine *>(tvc->iter);
  *r_line = cl->line;
  *r_len = cl->len;
  // printf("'%s' %d\n", *line, cl->len);
  BLI_assert(cl->line[cl->len] == '\0' && (cl->len == 0 || cl->line[cl->len - 1] != '\0'));
}

static void console_cursor_wrap_offset(
    const char *str, int width, int *row, int *column, const char *end)
{
  int col;
  const int tab_width = 4;

  for (; *str; str += BLI_str_utf8_size_safe(str)) {
    col = UNLIKELY(*str == '\t') ? (tab_width - (*column % tab_width)) :
                                   BLI_str_utf8_char_width_safe(str);

    if (*column + col > width) {
      (*row)++;
      *column = 0;
    }

    if (end && str >= end) {
      break;
    }

    *column += col;
  }
}

static void console_textview_draw_cursor(TextViewContext *tvc, int cwidth, int columns)
{
  int pen[2];
  {
    const SpaceConsole *sc = (SpaceConsole *)tvc->arg1;
    const ConsoleLine *cl = (ConsoleLine *)sc->history.last;
    int offl = 0, offc = 0;

    console_cursor_wrap_offset(sc->prompt, columns, &offl, &offc, nullptr);
    console_cursor_wrap_offset(cl->line, columns, &offl, &offc, cl->line + cl->cursor);
    pen[0] = cwidth * offc;
    pen[1] = -tvc->lheight * offl;

    console_cursor_wrap_offset(cl->line + cl->cursor, columns, &offl, &offc, nullptr);
    pen[1] += tvc->lheight * offl;

    pen[0] += tvc->draw_rect.xmin;
    pen[1] += tvc->draw_rect.ymin;
  }

  /* cursor */
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformThemeColor(TH_CONSOLE_CURSOR);

  immRectf(pos, pen[0] - U.pixelsize, pen[1], pen[0] + U.pixelsize, pen[1] + tvc->lheight);

  immUnbindProgram();
}

static void console_textview_const_colors(TextViewContext * /*tvc*/, uchar bg_sel[4])
{
  UI_GetThemeColor4ubv(TH_CONSOLE_SELECT, bg_sel);
}

static void console_textview_draw_rect_calc(const ARegion *region,
                                            rcti *r_draw_rect,
                                            rcti *r_draw_rect_outer)
{
  const int margin = 4 * UI_SCALE_FAC;
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

static int console_textview_main__internal(SpaceConsole *sc,
                                           const ARegion *region,
                                           const bool do_draw,
                                           const int mval[2],
                                           void **r_mval_pick_item,
                                           int *r_mval_pick_offset)
{
  ConsoleLine cl_dummy = {nullptr};
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
  tvc.arg2 = nullptr;

  /* view */
  tvc.sel_start = sc->sel_start;
  tvc.sel_end = sc->sel_end;
  tvc.lheight = sc->lheight * UI_SCALE_FAC;
  tvc.scroll_ymin = v2d->cur.ymin;
  tvc.scroll_ymax = v2d->cur.ymax;

  console_textview_draw_rect_calc(region, &tvc.draw_rect, &tvc.draw_rect_outer);

  /* Nudge right by half a column to break selection mid-character. */
  int m_pos[2] = {mval[0], mval[1]};
  /* Mouse position is initialized with max int. */
  if (m_pos[0] != INT_MAX) {
    m_pos[0] += tvc.lheight / 4;
  }

  console_scrollback_prompt_begin(sc, &cl_dummy);
  ret = textview_draw(&tvc, do_draw, m_pos, r_mval_pick_item, r_mval_pick_offset);
  console_scrollback_prompt_end(sc, &cl_dummy);

  return ret;
}

void console_textview_main(SpaceConsole *sc, const ARegion *region)
{
  const int mval[2] = {INT_MAX, INT_MAX};
  console_textview_main__internal(sc, region, true, mval, nullptr, nullptr);
}

int console_textview_height(SpaceConsole *sc, const ARegion *region)
{
  const int mval[2] = {INT_MAX, INT_MAX};
  return console_textview_main__internal(sc, region, false, mval, nullptr, nullptr);
}

int console_char_pick(SpaceConsole *sc, const ARegion *region, const int mval[2])
{
  int mval_pick_offset = 0;
  void *mval_pick_item = nullptr;

  console_textview_main__internal(sc, region, false, mval, &mval_pick_item, &mval_pick_offset);
  return mval_pick_offset;
}
