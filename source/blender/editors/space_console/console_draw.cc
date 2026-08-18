/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spconsole
 */

#include <algorithm>
#include <cstring>

#include "BLI_listbase.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "GPU_immediate.hh"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "console_intern.hh"

#include "../space_info/textview.hh"

namespace blender {

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

  ui::theme::get_color_4ubv(fg_id, fg);
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
  cl_dummy->line = MEM_new_array_uninitialized<char>(cl_dummy->len_alloc, "cl_dummy");
  memcpy(cl_dummy->line, sc->prompt, prompt_len);
  memcpy(cl_dummy->line + prompt_len, cl->line, cl->len + 1);
  BLI_addtail(&sc->scrollback, cl_dummy);
}
void console_scrollback_prompt_end(SpaceConsole *sc, ConsoleLine *cl_dummy)
{
  MEM_delete(cl_dummy->line);
  BLI_remlink(&sc->scrollback, cl_dummy);
}

/* console textview callbacks */
static int console_textview_begin(TextViewContext *tvc)
{
  SpaceConsole *sc = static_cast<SpaceConsole *>(const_cast<void *>(tvc->arg1));
  tvc->sel_start = sc->sel_start;
  tvc->sel_end = sc->sel_end;

  /* iterator */
  tvc->iter = sc->scrollback.last;

  return (tvc->iter != nullptr);
}

static void console_textview_end(TextViewContext *tvc)
{
  SpaceConsole *sc = static_cast<SpaceConsole *>(const_cast<void *>(tvc->arg1));
  (void)sc;
}

static int console_textview_step(TextViewContext *tvc)
{
  return ((tvc->iter = static_cast<void *>(
               (static_cast<Link *>(const_cast<void *>(tvc->iter)))->prev)) != nullptr);
}

static void console_textview_line_get(TextViewContext *tvc, const char **r_line, int *r_len)
{
  const ConsoleLine *cl = static_cast<const ConsoleLine *>(tvc->iter);
  *r_line = cl->line;
  *r_len = cl->len;
  // printf("'%s' %d\n", *line, cl->len);
  BLI_assert(cl->line[cl->len] == '\0' && (cl->len == 0 || cl->line[cl->len - 1] != '\0'));
}

/** A character's place in the wrapped edit line, rows counted from the top. */
struct ConsoleWrapOffset {
  int row = 0;
  int column = 0;
};

/** The wrapped edit line as drawn, enough to map a byte offset to a position. */
struct ConsoleDrawLine {
  const char *str;
  int str_len;
  /** Byte offset each wrapped row starts at. */
  Span<int> offsets;
  rcti draw_rect;
  int char_width;
  int line_height;
};

/**
 * Return the wrap offset of the character at `byte_offset`,
 * where the cursor sits at its leading edge.
 * The column is relative to the row start, matching how drawing converts it
 * (see #textview_draw_sel).
 */
static ConsoleWrapOffset console_line_wrap_offset(const ConsoleDrawLine &line,
                                                  const int byte_offset)
{
  const Span<int> offsets = line.offsets;
  int row = 0;
  while ((row + 1 < offsets.size()) && (offsets[row + 1] <= byte_offset)) {
    row++;
  }
  const int row_start = offsets[row];
  const int row_end = (row + 1 < offsets.size()) ? offsets[row + 1] : line.str_len;
  const int column = BLI_str_utf8_offset_to_column_with_tabs(
      line.str + row_start, row_end - row_start, byte_offset - row_start, TVC_TAB_COLUMNS);
  return {row, column};
}

/**
 * Return the position of a wrap offset: X the left of the character, Y the bottom of its row.
 * Rows count from the top while drawing is anchored to the bottom, hence the flip.
 */
static int2 console_line_wrap_offset_to_xy(const ConsoleDrawLine &line,
                                           const ConsoleWrapOffset &wrap_offset)
{
  const int end_row = int(line.offsets.size()) - 1;
  return {
      line.draw_rect.xmin + (line.char_width * wrap_offset.column),
      line.draw_rect.ymin + (line.line_height * (end_row - wrap_offset.row)),
  };
}

/** Return the position of the character at `byte_offset`. */
static int2 console_line_byte_to_xy(const ConsoleDrawLine &line, const int byte_offset)
{
  return console_line_wrap_offset_to_xy(line, console_line_wrap_offset(line, byte_offset));
}

static void console_textview_draw_cursor(TextViewContext *tvc, int char_width, int columns)
{
  int2 pen;
  {
    const SpaceConsole *sc = static_cast<SpaceConsole *>(const_cast<void *>(tvc->arg1));
    /* Cache the font metrics computed during draw, reused for IME cursor positioning. */
    sc->runtime->char_width_px = char_width;
    sc->runtime->line_height_px = tvc->line_height;
    const ConsoleLine *cl = static_cast<ConsoleLine *>(sc->history.last);

    /* Use the dummy scrollback line built for this draw,
     * see #console_scrollback_prompt_begin. */
    const ConsoleLine *cl_drawn = static_cast<const ConsoleLine *>(sc->scrollback.last);
    const int cursor_byte = int(strlen(sc->prompt)) + cl->cursor;

    /* Rebuild the wrap layout #textview_draw just drew this line with. */
    int tot_rows;
    int *offsets_buf;
    textview_wrap_offsets(cl_drawn->line, cl_drawn->len, columns, &tot_rows, &offsets_buf);
    const ConsoleDrawLine line = {
        cl_drawn->line,
        cl_drawn->len,
        Span<int>(offsets_buf, tot_rows),
        tvc->draw_rect,
        char_width,
        tvc->line_height,
    };

    pen = console_line_byte_to_xy(line, cursor_byte);
    MEM_delete(offsets_buf);
  }

  /* cursor */
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformThemeColor(TH_CONSOLE_CURSOR);

  immRectf(pos, pen[0] - U.pixelsize, pen[1], pen[0] + U.pixelsize, pen[1] + tvc->line_height);

  immUnbindProgram();
}

static void console_textview_const_colors(TextViewContext * /*tvc*/, uchar bg_sel[4])
{
  ui::theme::get_color_4ubv(TH_CONSOLE_SELECT, bg_sel);
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
  tvc.line_height = sc->line_height * UI_SCALE_FAC;
  tvc.scroll_ymin = v2d->cur.ymin;
  tvc.scroll_ymax = v2d->cur.ymax;

  console_textview_draw_rect_calc(region, &tvc.draw_rect, &tvc.draw_rect_outer);

  /* Nudge right by half a column to break selection mid-character. */
  int m_pos[2] = {mval[0], mval[1]};
  /* Mouse position is initialized with max int. */
  if (m_pos[0] != INT_MAX) {
    m_pos[0] += tvc.line_height / 4;
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

std::optional<blender::int2> console_cursor_region_xy_get(const SpaceConsole *sc,
                                                          const ARegion *region,
                                                          const int offset)
{
  const ConsoleLine *cl = static_cast<const ConsoleLine *>(sc->history.last);
  if (cl == nullptr) {
    return std::nullopt;
  }

  rcti draw_rect, draw_rect_outer;
  console_textview_draw_rect_calc(region, &draw_rect, &draw_rect_outer);

  const int char_width = sc->runtime->char_width_px;
  const int columns = std::max((draw_rect.xmax - draw_rect.xmin) / std::max(char_width, 1), 1);

  /* Build the edit line as drawn: prompt + input. */
  const std::string str = StringRef(sc->prompt) + StringRef(cl->line, cl->len);

  int tot_rows;
  int *offsets_buf;
  textview_wrap_offsets(str.c_str(), int(str.size()), columns, &tot_rows, &offsets_buf);
  const ConsoleDrawLine line = {
      str.c_str(),
      int(str.size()),
      Span<int>(offsets_buf, tot_rows),
      draw_rect,
      char_width,
      sc->runtime->line_height_px,
  };
  const int2 xy = console_line_byte_to_xy(line, int(strlen(sc->prompt)) + offset);
  MEM_delete(offsets_buf);

  return xy;
}

}  // namespace blender
