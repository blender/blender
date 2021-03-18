/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
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
 * \ingroup sptext
 */

#include "MEM_guardedalloc.h"

#include "BLF_api.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_text.h"
#include "BKE_text_suggestions.h"

#include "ED_text.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "text_format.h"
#include "text_intern.h"

/******************** text font drawing ******************/

typedef struct TextDrawContext {
  int font_id;
  int cwidth_px;
  int lheight_px;
  bool syntax_highlight;
} TextDrawContext;

static void text_draw_context_init(const SpaceText *st, TextDrawContext *tdc)
{
  tdc->font_id = blf_mono_font;
  tdc->cwidth_px = 0;
  tdc->lheight_px = st->runtime.lheight_px;
  tdc->syntax_highlight = st->showsyntax && ED_text_is_syntax_highlight_supported(st->text);
}

static void text_font_begin(const TextDrawContext *tdc)
{
  BLF_size(tdc->font_id, tdc->lheight_px, 72);
}

static void text_font_end(const TextDrawContext *UNUSED(tdc))
{
}

static int text_font_draw(const TextDrawContext *tdc, int x, int y, const char *str)
{
  int columns;

  BLF_position(tdc->font_id, x, y, 0);
  columns = BLF_draw_mono(tdc->font_id, str, BLF_DRAW_STR_DUMMY_MAX, tdc->cwidth_px);

  return tdc->cwidth_px * columns;
}

static int text_font_draw_character(const TextDrawContext *tdc, int x, int y, char c)
{
  BLF_position(tdc->font_id, x, y, 0);
  BLF_draw(tdc->font_id, &c, 1);

  return tdc->cwidth_px;
}

static int text_font_draw_character_utf8(const TextDrawContext *tdc, int x, int y, const char *c)
{
  int columns;

  const size_t len = BLI_str_utf8_size_safe(c);
  BLF_position(tdc->font_id, x, y, 0);
  columns = BLF_draw_mono(tdc->font_id, c, len, tdc->cwidth_px);

  return tdc->cwidth_px * columns;
}

#if 0
/* Formats every line of the current text */
static void txt_format_text(SpaceText *st)
{
  TextLine *linep;

  if (!st->text) {
    return;
  }

  for (linep = st->text->lines.first; linep; linep = linep->next) {
    txt_format_line(st, linep, 0);
  }
}
#endif

/* Sets the current drawing color based on the format character specified */
static void format_draw_color(const TextDrawContext *tdc, char formatchar)
{
  switch (formatchar) {
    case FMT_TYPE_WHITESPACE:
      break;
    case FMT_TYPE_SYMBOL:
      UI_FontThemeColor(tdc->font_id, TH_SYNTAX_S);
      break;
    case FMT_TYPE_COMMENT:
      UI_FontThemeColor(tdc->font_id, TH_SYNTAX_C);
      break;
    case FMT_TYPE_NUMERAL:
      UI_FontThemeColor(tdc->font_id, TH_SYNTAX_N);
      break;
    case FMT_TYPE_STRING:
      UI_FontThemeColor(tdc->font_id, TH_SYNTAX_L);
      break;
    case FMT_TYPE_DIRECTIVE:
      UI_FontThemeColor(tdc->font_id, TH_SYNTAX_D);
      break;
    case FMT_TYPE_SPECIAL:
      UI_FontThemeColor(tdc->font_id, TH_SYNTAX_V);
      break;
    case FMT_TYPE_RESERVED:
      UI_FontThemeColor(tdc->font_id, TH_SYNTAX_R);
      break;
    case FMT_TYPE_KEYWORD:
      UI_FontThemeColor(tdc->font_id, TH_SYNTAX_B);
      break;
    case FMT_TYPE_DEFAULT:
    default:
      UI_FontThemeColor(tdc->font_id, TH_TEXT);
      break;
  }
}

/************************** draw text *****************************/

/**
 * Notes on word-wrap
 * --
 * All word-wrap functions follow the algorithm below to maintain consistency:
 * - line:
 *   The line to wrap (tabs converted to spaces)
 * - view_width:
 *   The maximum number of characters displayable in the region
 *   This equals region_width/font_width for the region
 * - wrap_chars:
 *   Characters that allow wrapping. This equals [' ', '\t', '-']
 *
 * \code{.py}
 * def wrap(line, view_width, wrap_chars):
 *     draw_start = 0
 *     draw_end = view_width
 *     pos = 0
 *     for c in line:
 *         if pos-draw_start >= view_width:
 *             print line[draw_start:draw_end]
 *             draw_start = draw_end
 *             draw_end += view_width
 *         elif c in wrap_chars:
 *             draw_end = pos+1
 *         pos += 1
 *     print line[draw_start:]
 * \encode
 */

int wrap_width(const SpaceText *st, ARegion *region)
{
  int winx = region->winx - TXT_SCROLL_WIDTH;
  int x, max;

  x = TXT_BODY_LEFT(st);
  max = st->runtime.cwidth_px ? (winx - x) / st->runtime.cwidth_px : 0;
  return max > 8 ? max : 8;
}

/* Sets (offl, offc) for transforming (line, curs) to its wrapped position */
void wrap_offset(
    const SpaceText *st, ARegion *region, TextLine *linein, int cursin, int *offl, int *offc)
{
  Text *text;
  TextLine *linep;
  int i, j, start, end, max, chop;
  char ch;

  *offl = *offc = 0;

  if (!st->text) {
    return;
  }
  if (!st->wordwrap) {
    return;
  }

  text = st->text;

  /* Move pointer to first visible line (top) */
  linep = text->lines.first;
  i = st->top;
  while (i > 0 && linep) {
    int lines = text_get_visible_lines(st, region, linep->line);

    /* Line before top */
    if (linep == linein) {
      if (lines <= i) {
        /* no visible part of line */
        return;
      }
    }

    if (i - lines < 0) {
      break;
    }

    linep = linep->next;
    (*offl) += lines - 1;
    i -= lines;
  }

  max = wrap_width(st, region);
  cursin = BLI_str_utf8_offset_to_column(linein->line, cursin);

  while (linep) {
    start = 0;
    end = max;
    chop = 1;
    *offc = 0;
    for (i = 0, j = 0; linep->line[j]; j += BLI_str_utf8_size_safe(linep->line + j)) {
      int chars;
      int columns = BLI_str_utf8_char_width_safe(linep->line + j); /* = 1 for tab */

      /* Mimic replacement of tabs */
      ch = linep->line[j];
      if (ch == '\t') {
        chars = st->tabnumber - i % st->tabnumber;
        if (linep == linein && i < cursin) {
          cursin += chars - 1;
        }
        ch = ' ';
      }
      else {
        chars = 1;
      }

      while (chars--) {
        if (i + columns - start > max) {
          end = MIN2(end, i);

          if (chop && linep == linein && i >= cursin) {
            if (i == cursin) {
              (*offl)++;
              *offc -= end - start;
            }

            return;
          }

          (*offl)++;
          *offc -= end - start;

          start = end;
          end += max;
          chop = 1;
        }
        else if (ELEM(ch, ' ', '-')) {
          end = i + 1;
          chop = 0;
          if (linep == linein && i >= cursin) {
            return;
          }
        }
        i += columns;
      }
    }
    if (linep == linein) {
      break;
    }
    linep = linep->next;
  }
}

/* cursin - mem, offc - view */
void wrap_offset_in_line(
    const SpaceText *st, ARegion *region, TextLine *linein, int cursin, int *offl, int *offc)
{
  int i, j, start, end, chars, max, chop;
  char ch;

  *offl = *offc = 0;

  if (!st->text) {
    return;
  }
  if (!st->wordwrap) {
    return;
  }

  max = wrap_width(st, region);

  start = 0;
  end = max;
  chop = 1;
  *offc = 0;
  cursin = BLI_str_utf8_offset_to_column(linein->line, cursin);

  for (i = 0, j = 0; linein->line[j]; j += BLI_str_utf8_size_safe(linein->line + j)) {
    int columns = BLI_str_utf8_char_width_safe(linein->line + j); /* = 1 for tab */

    /* Mimic replacement of tabs */
    ch = linein->line[j];
    if (ch == '\t') {
      chars = st->tabnumber - i % st->tabnumber;
      if (i < cursin) {
        cursin += chars - 1;
      }
      ch = ' ';
    }
    else {
      chars = 1;
    }

    while (chars--) {
      if (i + columns - start > max) {
        end = MIN2(end, i);

        if (chop && i >= cursin) {
          if (i == cursin) {
            (*offl)++;
            *offc -= end - start;
          }

          return;
        }

        (*offl)++;
        *offc -= end - start;

        start = end;
        end += max;
        chop = 1;
      }
      else if (ELEM(ch, ' ', '-')) {
        end = i + 1;
        chop = 0;
        if (i >= cursin) {
          return;
        }
      }
      i += columns;
    }
  }
}

int text_get_char_pos(const SpaceText *st, const char *line, int cur)
{
  int a = 0, i;

  for (i = 0; i < cur && line[i]; i += BLI_str_utf8_size_safe(line + i)) {
    if (line[i] == '\t') {
      a += st->tabnumber - a % st->tabnumber;
    }
    else {
      a += BLI_str_utf8_char_width_safe(line + i);
    }
  }
  return a;
}

static const char *txt_utf8_forward_columns(const char *str, int columns, int *padding)
{
  int col;
  const char *p = str;
  while (*p) {
    col = BLI_str_utf8_char_width(p);
    if (columns - col < 0) {
      break;
    }
    columns -= col;
    p += BLI_str_utf8_size_safe(p);
    if (columns == 0) {
      break;
    }
  }
  if (padding) {
    *padding = *p ? columns : 0;
  }
  return p;
}

static int text_draw_wrapped(const SpaceText *st,
                             const TextDrawContext *tdc,
                             const char *str,
                             int x,
                             int y,
                             int w,
                             const char *format,
                             int skip)
{
  const bool use_syntax = (tdc->syntax_highlight && format);
  FlattenString fs;
  int basex, lines;
  int i, wrap, end, max, columns, padding; /* column */
  /* warning, only valid when 'use_syntax' is set */
  int a, fstart, fpos;      /* utf8 chars */
  int mi, ma, mstart, mend; /* mem */
  char fmt_prev = 0xff;
  /* don't draw lines below this */
  const int clip_min_y = -(int)(st->runtime.lheight_px - 1);

  flatten_string(st, &fs, str);
  str = fs.buf;
  max = w / st->runtime.cwidth_px;
  if (max < 8) {
    max = 8;
  }
  basex = x;
  lines = 1;

  fpos = fstart = 0;
  mstart = 0;
  mend = txt_utf8_forward_columns(str, max, &padding) - str;
  end = wrap = max - padding;

  for (i = 0, mi = 0; str[mi]; i += columns, mi += BLI_str_utf8_size_safe(str + mi)) {
    columns = BLI_str_utf8_char_width_safe(str + mi);
    if (i + columns > end) {
      /* skip hidden part of line */
      if (skip) {
        skip--;
        if (use_syntax) {
          /* currently fpos only used when formatting */
          fpos += BLI_strnlen_utf8(str + mstart, mend - mstart);
        }
        fstart = fpos;
        mstart = mend;
        mend = txt_utf8_forward_columns(str + mend, max, &padding) - str;
        end = (wrap += max - padding);
        continue;
      }

      /* Draw the visible portion of text on the overshot line */
      for (a = fstart, ma = mstart; ma < mend; a++, ma += BLI_str_utf8_size_safe(str + ma)) {
        if (use_syntax) {
          if (fmt_prev != format[a]) {
            format_draw_color(tdc, fmt_prev = format[a]);
          }
        }
        x += text_font_draw_character_utf8(tdc, x, y, str + ma);
        fpos++;
      }
      y -= TXT_LINE_HEIGHT(st);
      x = basex;
      lines++;
      fstart = fpos;
      mstart = mend;
      mend = txt_utf8_forward_columns(str + mend, max, &padding) - str;
      end = (wrap += max - padding);

      if (y <= clip_min_y) {
        break;
      }
    }
    else if (ELEM(str[mi], ' ', '-')) {
      wrap = i + 1;
      mend = mi + 1;
    }
  }

  /* Draw the remaining text */
  for (a = fstart, ma = mstart; str[ma] && y > clip_min_y;
       a++, ma += BLI_str_utf8_size_safe(str + ma)) {
    if (use_syntax) {
      if (fmt_prev != format[a]) {
        format_draw_color(tdc, fmt_prev = format[a]);
      }
    }

    x += text_font_draw_character_utf8(tdc, x, y, str + ma);
  }

  flatten_string_free(&fs);

  return lines;
}

static void text_draw(const SpaceText *st,
                      const TextDrawContext *tdc,
                      char *str,
                      int cshift,
                      int maxwidth,
                      int x,
                      int y,
                      const char *format)
{
  const bool use_syntax = (tdc->syntax_highlight && format);
  FlattenString fs;
  int columns, size, n, w = 0, padding, amount = 0;
  const char *in = NULL;

  for (n = flatten_string(st, &fs, str), str = fs.buf; n > 0; n--) {
    columns = BLI_str_utf8_char_width_safe(str);
    size = BLI_str_utf8_size_safe(str);

    if (!in) {
      if (w >= cshift) {
        padding = w - cshift;
        in = str;
      }
      else if (format) {
        format++;
      }
    }
    if (in) {
      if (maxwidth && w + columns > cshift + maxwidth) {
        break;
      }
      amount++;
    }

    w += columns;
    str += size;
  }
  if (!in) {
    flatten_string_free(&fs);
    return; /* String is shorter than shift or ends with a padding */
  }

  x += tdc->cwidth_px * padding;

  if (use_syntax) {
    int a, str_shift = 0;
    char fmt_prev = 0xff;

    for (a = 0; a < amount; a++) {
      if (format[a] != fmt_prev) {
        format_draw_color(tdc, fmt_prev = format[a]);
      }
      x += text_font_draw_character_utf8(tdc, x, y, in + str_shift);
      str_shift += BLI_str_utf8_size_safe(in + str_shift);
    }
  }
  else {
    text_font_draw(tdc, x, y, in);
  }

  flatten_string_free(&fs);
}

/************************ cache utilities *****************************/

typedef struct DrawCache {
  int *line_height;
  int total_lines, nlines;

  /* this is needed to check cache relevance */
  int winx, wordwrap, showlinenrs, tabnumber;
  short lheight;
  char cwidth_px;
  char text_id[MAX_ID_NAME];

  /* for partial lines recalculation */
  short update_flag;
  int valid_head, valid_tail; /* amount of unchanged lines */
} DrawCache;

static void text_drawcache_init(SpaceText *st)
{
  DrawCache *drawcache = MEM_callocN(sizeof(DrawCache), "text draw cache");

  drawcache->winx = -1;
  drawcache->nlines = BLI_listbase_count(&st->text->lines);
  drawcache->text_id[0] = '\0';

  st->runtime.drawcache = drawcache;
}

static void text_update_drawcache(SpaceText *st, ARegion *region)
{
  DrawCache *drawcache;
  int full_update = 0, nlines = 0;
  Text *txt = st->text;

  if (st->runtime.drawcache == NULL) {
    text_drawcache_init(st);
  }

  text_update_character_width(st);

  drawcache = st->runtime.drawcache;
  nlines = drawcache->nlines;

  /* check if full cache update is needed */

  /* area was resized */
  full_update |= drawcache->winx != region->winx;
  /* word-wrapping option was toggled */
  full_update |= drawcache->wordwrap != st->wordwrap;
  /* word-wrapping option was toggled */
  full_update |= drawcache->showlinenrs != st->showlinenrs;
  /* word-wrapping option was toggled */
  full_update |= drawcache->tabnumber != st->tabnumber;
  /* word-wrapping option was toggled */
  full_update |= drawcache->lheight != st->runtime.lheight_px;
  /* word-wrapping option was toggled */
  full_update |= drawcache->cwidth_px != st->runtime.cwidth_px;
  /* text datablock was changed */
  full_update |= !STREQLEN(drawcache->text_id, txt->id.name, MAX_ID_NAME);

  if (st->wordwrap) {
    /* update line heights */
    if (full_update || !drawcache->line_height) {
      drawcache->valid_head = 0;
      drawcache->valid_tail = 0;
      drawcache->update_flag = 1;
    }

    if (drawcache->update_flag) {
      TextLine *line = st->text->lines.first;
      int lineno = 0, size, lines_count;
      int *fp = drawcache->line_height, *new_tail, *old_tail;

      nlines = BLI_listbase_count(&txt->lines);
      size = sizeof(int) * nlines;

      if (fp) {
        fp = MEM_reallocN(fp, size);
      }
      else {
        fp = MEM_callocN(size, "text drawcache line_height");
      }

      drawcache->valid_tail = drawcache->valid_head = 0;
      old_tail = fp + drawcache->nlines - drawcache->valid_tail;
      new_tail = fp + nlines - drawcache->valid_tail;
      memmove(new_tail, old_tail, drawcache->valid_tail);

      drawcache->total_lines = 0;

      if (st->showlinenrs) {
        st->runtime.line_number_display_digits = integer_digits_i(nlines);
      }

      while (line) {
        if (drawcache->valid_head) { /* we're inside valid head lines */
          lines_count = fp[lineno];
          drawcache->valid_head--;
        }
        else if (lineno > new_tail - fp) { /* we-re inside valid tail lines */
          lines_count = fp[lineno];
        }
        else {
          lines_count = text_get_visible_lines(st, region, line->line);
        }

        fp[lineno] = lines_count;

        line = line->next;
        lineno++;
        drawcache->total_lines += lines_count;
      }

      drawcache->line_height = fp;
    }
  }
  else {
    if (drawcache->line_height) {
      MEM_freeN(drawcache->line_height);
      drawcache->line_height = NULL;
    }

    if (full_update || drawcache->update_flag) {
      nlines = BLI_listbase_count(&txt->lines);

      if (st->showlinenrs) {
        st->runtime.line_number_display_digits = integer_digits_i(nlines);
      }
    }

    drawcache->total_lines = nlines;
  }

  drawcache->nlines = nlines;

  /* store settings */
  drawcache->winx = region->winx;
  drawcache->wordwrap = st->wordwrap;
  drawcache->lheight = st->runtime.lheight_px;
  drawcache->cwidth_px = st->runtime.cwidth_px;
  drawcache->showlinenrs = st->showlinenrs;
  drawcache->tabnumber = st->tabnumber;

  strncpy(drawcache->text_id, txt->id.name, MAX_ID_NAME);

  /* clear update flag */
  drawcache->update_flag = 0;
  drawcache->valid_head = 0;
  drawcache->valid_tail = 0;
}

void text_drawcache_tag_update(SpaceText *st, int full)
{
  /* This happens if text editor ops are called from Python. */
  if (st == NULL) {
    return;
  }

  if (st->runtime.drawcache != NULL) {
    DrawCache *drawcache = st->runtime.drawcache;
    Text *txt = st->text;

    if (drawcache->update_flag) {
      /* happens when tagging update from space listener */
      /* should do nothing to prevent locally tagged cache be fully recalculated */
      return;
    }

    if (!full) {
      int sellno = BLI_findindex(&txt->lines, txt->sell);
      int curlno = BLI_findindex(&txt->lines, txt->curl);

      if (curlno < sellno) {
        drawcache->valid_head = curlno;
        drawcache->valid_tail = drawcache->nlines - sellno - 1;
      }
      else {
        drawcache->valid_head = sellno;
        drawcache->valid_tail = drawcache->nlines - curlno - 1;
      }

      /* quick cache recalculation is also used in delete operator,
       * which could merge lines which are adjacent to current selection lines
       * expand recalculate area to this lines */
      if (drawcache->valid_head > 0) {
        drawcache->valid_head--;
      }
      if (drawcache->valid_tail > 0) {
        drawcache->valid_tail--;
      }
    }
    else {
      drawcache->valid_head = 0;
      drawcache->valid_tail = 0;
    }

    drawcache->update_flag = 1;
  }
}

void text_free_caches(SpaceText *st)
{
  DrawCache *drawcache = st->runtime.drawcache;

  if (drawcache) {
    if (drawcache->line_height) {
      MEM_freeN(drawcache->line_height);
    }

    MEM_freeN(drawcache);
  }
}

/************************ word-wrap utilities *****************************/

/* cache should be updated in caller */
static int text_get_visible_lines_no(const SpaceText *st, int lineno)
{
  const DrawCache *drawcache = st->runtime.drawcache;

  return drawcache->line_height[lineno];
}

int text_get_visible_lines(const SpaceText *st, ARegion *region, const char *str)
{
  int i, j, start, end, max, lines, chars;
  char ch;

  max = wrap_width(st, region);
  lines = 1;
  start = 0;
  end = max;
  for (i = 0, j = 0; str[j]; j += BLI_str_utf8_size_safe(str + j)) {
    int columns = BLI_str_utf8_char_width_safe(str + j); /* = 1 for tab */

    /* Mimic replacement of tabs */
    ch = str[j];
    if (ch == '\t') {
      chars = st->tabnumber - i % st->tabnumber;
      ch = ' ';
    }
    else {
      chars = 1;
    }

    while (chars--) {
      if (i + columns - start > max) {
        lines++;
        start = MIN2(end, i);
        end += max;
      }
      else if (ELEM(ch, ' ', '-')) {
        end = i + 1;
      }

      i += columns;
    }
  }

  return lines;
}

int text_get_span_wrap(const SpaceText *st, ARegion *region, TextLine *from, TextLine *to)
{
  if (st->wordwrap) {
    int ret = 0;
    TextLine *tmp = from;

    /* Look forwards */
    while (tmp) {
      if (tmp == to) {
        return ret;
      }
      ret += text_get_visible_lines(st, region, tmp->line);
      tmp = tmp->next;
    }

    return ret;
  }
  return txt_get_span(from, to);
}

int text_get_total_lines(SpaceText *st, ARegion *region)
{
  DrawCache *drawcache;

  text_update_drawcache(st, region);
  drawcache = st->runtime.drawcache;

  return drawcache->total_lines;
}

/************************ draw scrollbar *****************************/

static void calc_text_rcts(SpaceText *st, ARegion *region, rcti *scroll, rcti *back)
{
  int lhlstart, lhlend, ltexth, sell_off, curl_off;
  short barheight, barstart, hlstart, hlend, blank_lines;
  short pix_available, pix_top_margin, pix_bottom_margin, pix_bardiff;

  pix_top_margin = (0.4 * U.widget_unit);
  pix_bottom_margin = (0.4 * U.widget_unit);
  pix_available = region->winy - pix_top_margin - pix_bottom_margin;
  ltexth = text_get_total_lines(st, region);
  blank_lines = st->runtime.viewlines / 2;

  /* nicer code: use scroll rect for entire bar */
  back->xmin = region->winx - (0.6 * U.widget_unit);
  back->xmax = region->winx;
  back->ymin = 0;
  back->ymax = region->winy;

  scroll->xmax = region->winx - (0.2 * U.widget_unit);
  scroll->xmin = scroll->xmax - (0.4 * U.widget_unit);
  scroll->ymin = pix_top_margin;
  scroll->ymax = pix_available;

  /* when re-sizing a 2D Viewport with the bar at the bottom to a greater height
   * more blank lines will be added */
  if (ltexth + blank_lines < st->top + st->runtime.viewlines) {
    blank_lines = st->top + st->runtime.viewlines - ltexth;
  }

  ltexth += blank_lines;

  barheight = (ltexth > 0) ? (st->runtime.viewlines * pix_available) / ltexth : 0;
  pix_bardiff = 0;
  if (barheight < 20) {
    pix_bardiff = 20 - barheight; /* take into account the now non-linear sizing of the bar */
    barheight = 20;
  }
  barstart = (ltexth > 0) ? ((pix_available - pix_bardiff) * st->top) / ltexth : 0;

  st->runtime.scroll_region_handle = *scroll;
  st->runtime.scroll_region_handle.ymax -= barstart;
  st->runtime.scroll_region_handle.ymin = st->runtime.scroll_region_handle.ymax - barheight;

  CLAMP(st->runtime.scroll_region_handle.ymin, pix_bottom_margin, region->winy - pix_top_margin);
  CLAMP(st->runtime.scroll_region_handle.ymax, pix_bottom_margin, region->winy - pix_top_margin);

  st->runtime.scroll_px_per_line = (pix_available > 0) ? (float)ltexth / pix_available : 0;
  if (st->runtime.scroll_px_per_line < 0.1f) {
    st->runtime.scroll_px_per_line = 0.1f;
  }

  curl_off = text_get_span_wrap(st, region, st->text->lines.first, st->text->curl);
  sell_off = text_get_span_wrap(st, region, st->text->lines.first, st->text->sell);
  lhlstart = MIN2(curl_off, sell_off);
  lhlend = MAX2(curl_off, sell_off);

  if (ltexth > 0) {
    hlstart = (lhlstart * pix_available) / ltexth;
    hlend = (lhlend * pix_available) / ltexth;

    /* The scrollbar is non-linear sized. */
    if (pix_bardiff > 0) {
      /* the start of the highlight is in the current viewport */
      if (st->runtime.viewlines && lhlstart >= st->top &&
          lhlstart <= st->top + st->runtime.viewlines) {
        /* Speed the progression of the start of the highlight through the scrollbar. */
        hlstart = (((pix_available - pix_bardiff) * lhlstart) / ltexth) +
                  (pix_bardiff * (lhlstart - st->top) / st->runtime.viewlines);
      }
      else if (lhlstart > st->top + st->runtime.viewlines && hlstart < barstart + barheight &&
               hlstart > barstart) {
        /* push hl start down */
        hlstart = barstart + barheight;
      }
      else if (lhlend > st->top && lhlstart < st->top && hlstart > barstart) {
        /*fill out start */
        hlstart = barstart;
      }

      if (hlend <= hlstart) {
        hlend = hlstart + 2;
      }

      /* the end of the highlight is in the current viewport */
      if (st->runtime.viewlines && lhlend >= st->top &&
          lhlend <= st->top + st->runtime.viewlines) {
        /* Speed the progression of the end of the highlight through the scrollbar. */
        hlend = (((pix_available - pix_bardiff) * lhlend) / ltexth) +
                (pix_bardiff * (lhlend - st->top) / st->runtime.viewlines);
      }
      else if (lhlend < st->top && hlend >= barstart - 2 && hlend < barstart + barheight) {
        /* push hl end up */
        hlend = barstart;
      }
      else if (lhlend > st->top + st->runtime.viewlines &&
               lhlstart < st->top + st->runtime.viewlines && hlend < barstart + barheight) {
        /* fill out end */
        hlend = barstart + barheight;
      }

      if (hlend <= hlstart) {
        hlstart = hlend - 2;
      }
    }
  }
  else {
    hlstart = 0;
    hlend = 0;
  }

  if (hlend - hlstart < 2) {
    hlend = hlstart + 2;
  }

  st->runtime.scroll_region_select = *scroll;
  st->runtime.scroll_region_select.ymax = region->winy - pix_top_margin - hlstart;
  st->runtime.scroll_region_select.ymin = region->winy - pix_top_margin - hlend;

  CLAMP(st->runtime.scroll_region_select.ymin, pix_bottom_margin, region->winy - pix_top_margin);
  CLAMP(st->runtime.scroll_region_select.ymax, pix_bottom_margin, region->winy - pix_top_margin);
}

static void draw_textscroll(const SpaceText *st, rcti *scroll, rcti *back)
{
  bTheme *btheme = UI_GetTheme();
  uiWidgetColors wcol = btheme->tui.wcol_scroll;
  float col[4];
  float rad;

  /* background so highlights don't go behind the scrollbar */
  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformThemeColor(TH_BACK);
  immRecti(pos, back->xmin, back->ymin, back->xmax, back->ymax);
  immUnbindProgram();

  UI_draw_widget_scroll(&wcol,
                        scroll,
                        &st->runtime.scroll_region_handle,
                        (st->flags & ST_SCROLL_SELECT) ? UI_SCROLL_PRESSED : 0);

  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  rad = 0.4f * min_ii(BLI_rcti_size_x(&st->runtime.scroll_region_select),
                      BLI_rcti_size_y(&st->runtime.scroll_region_select));
  UI_GetThemeColor3fv(TH_HILITE, col);
  col[3] = 0.18f;
  UI_draw_roundbox_aa(
      &(const rctf){
          .xmin = st->runtime.scroll_region_select.xmin + 1,
          .xmax = st->runtime.scroll_region_select.xmax - 1,
          .ymin = st->runtime.scroll_region_select.ymin,
          .ymax = st->runtime.scroll_region_select.ymax,
      },
      true,
      rad,
      col);
}

/*********************** draw documentation *******************************/

#if 0
static void draw_documentation(const SpaceText *st, ARegion *region)
{
  TextDrawContext tdc = {0};
  TextLine *tmp;
  char *docs, buf[DOC_WIDTH + 1], *p;
  int i, br, lines;
  int boxw, boxh, l, x, y /* , top */ /* UNUSED */;

  if (!st || !st->text) {
    return;
  }
  if (!texttool_text_is_active(st->text)) {
    return;
  }

  docs = texttool_docs_get();

  if (!docs) {
    return;
  }

  text_draw_context_init(st, &tdc);

  /* Count the visible lines to the cursor */
  for (tmp = st->text->curl, l = -st->top; tmp; tmp = tmp->prev, l++) {
    /* pass */
  }

  if (l < 0) {
    return;
  }

  x = TXT_BODY_LEFT(st) + (st->runtime.cwidth_px * (st->text->curc - st->left));
  if (texttool_suggest_first()) {
    x += SUGG_LIST_WIDTH * st->runtime.cwidth_px + 50;
  }

  /* top = */ /* UNUSED */ y = region->winy - st->runtime.lheight_px * l - 2;
  boxw = DOC_WIDTH * st->runtime.cwidth_px + 20;
  boxh = (DOC_HEIGHT + 1) * TXT_LINE_HEIGHT(st);

  /* Draw panel */
  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformThemeColor(TH_BACK);
  immRecti(pos, x, y, x + boxw, y - boxh);
  immUniformThemeColor(TH_SHADE1);
  immBegin(GPU_PRIM_LINE_LOOP, 4);
  immVertex2i(pos, x, y);
  immVertex2i(pos, x + boxw, y);
  immVertex2i(pos, x + boxw, y - boxh);
  immVertex2i(pos, x, y - boxh);
  immEnd();
  immBegin(GPU_PRIM_LINE_LOOP, 3);
  immVertex2i(pos, x + boxw - 10, y - 7);
  immVertex2i(pos, x + boxw - 4, y - 7);
  immVertex2i(pos, x + boxw - 7, y - 2);
  immEnd();
  immBegin(GPU_PRIM_LINE_LOOP, 3);
  immVertex2i(pos, x + boxw - 10, y - boxh + 7);
  immVertex2i(pos, x + boxw - 4, y - boxh + 7);
  immVertex2i(pos, x + boxw - 7, y - boxh + 2);
  immEnd();

  immUnbindProgram();

  UI_FontThemeColor(tdc.font_id, TH_TEXT);

  i = 0;
  br = DOC_WIDTH;
  lines = 0;  /* XXX -doc_scroll; */
  for (p = docs; *p; p++) {
    if (*p == '\r' && *(++p) != '\n') {
      *(--p) = '\n'; /* Fix line endings */
    }
    if (*p == ' ' || *p == '\t') {
      br = i;
    }
    else if (*p == '\n') {
      buf[i] = '\0';
      if (lines >= 0) {
        y -= st->runtime.lheight_px;
        text_draw(st, &tdc, buf, 0, 0, x + 4, y - 3, NULL);
      }
      i = 0;
      br = DOC_WIDTH;
      lines++;
    }
    buf[i++] = *p;
    if (i == DOC_WIDTH) { /* Reached the width, go to last break and wrap there */
      buf[br] = '\0';
      if (lines >= 0) {
        y -= st->runtime.lheight_px;
        text_draw(st, &tdc, buf, 0, 0, x + 4, y - 3, NULL);
      }
      p -= i - br - 1; /* Rewind pointer to last break */
      i = 0;
      br = DOC_WIDTH;
      lines++;
    }
    if (lines >= DOC_HEIGHT) {
      break;
    }
  }
}
#endif

/*********************** draw suggestion list *******************************/

static void draw_suggestion_list(const SpaceText *st, const TextDrawContext *tdc, ARegion *region)
{
  SuggItem *item, *first, *last, *sel;
  char str[SUGG_LIST_WIDTH * BLI_UTF8_MAX + 1];
  int offl, offc, vcurl, vcurc;
  int w, boxw = 0, boxh, i, x, y, *top;
  const int lheight = TXT_LINE_HEIGHT(st);
  const int margin_x = 2;

  if (!st->text) {
    return;
  }
  if (!texttool_text_is_active(st->text)) {
    return;
  }

  first = texttool_suggest_first();
  last = texttool_suggest_last();

  if (!first || !last) {
    return;
  }

  text_pop_suggest_list();
  sel = texttool_suggest_selected();
  top = texttool_suggest_top();

  wrap_offset(st, region, st->text->curl, st->text->curc, &offl, &offc);
  vcurl = txt_get_span(st->text->lines.first, st->text->curl) - st->top + offl;
  vcurc = text_get_char_pos(st, st->text->curl->line, st->text->curc) - st->left + offc;

  x = TXT_BODY_LEFT(st) + (vcurc * st->runtime.cwidth_px);
  y = region->winy - (vcurl + 1) * lheight - 2;

  /* offset back so the start of the text lines up with the suggestions,
   * not essential but makes suggestions easier to follow */
  x -= st->runtime.cwidth_px *
       (st->text->curc - text_find_identifier_start(st->text->curl->line, st->text->curc));

  boxw = SUGG_LIST_WIDTH * st->runtime.cwidth_px + 20;
  boxh = SUGG_LIST_SIZE * lheight + 8;

  if (x + boxw > region->winx) {
    x = MAX2(0, region->winx - boxw);
  }

  /* not needed but stands out nicer */
  UI_draw_box_shadow(
      &(const rctf){
          .xmin = x,
          .xmax = x + boxw,
          .ymin = y - boxh,
          .ymax = y,
      },
      220);

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformThemeColor(TH_SHADE1);
  immRecti(pos, x - 1, y + 1, x + boxw + 1, y - boxh - 1);
  immUniformThemeColorShade(TH_BACK, 16);
  immRecti(pos, x, y, x + boxw, y - boxh);

  immUnbindProgram();

  /* Set the top 'item' of the visible list */
  for (i = 0, item = first; i < *top && item->next; i++, item = item->next) {
    /* pass */
  }

  for (i = 0; i < SUGG_LIST_SIZE && item; i++, item = item->next) {
    int len = txt_utf8_forward_columns(item->name, SUGG_LIST_WIDTH, NULL) - item->name;

    y -= lheight;

    BLI_strncpy(str, item->name, len + 1);

    w = st->runtime.cwidth_px * text_get_char_pos(st, str, len);

    if (item == sel) {
      uint posi = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

      immUniformThemeColor(TH_SHADE2);
      immRecti(posi, x + margin_x, y - 3, x + margin_x + w, y + lheight - 3);

      immUnbindProgram();
    }

    format_draw_color(tdc, item->type);
    text_draw(st, tdc, str, 0, 0, x + margin_x, y - 1, NULL);

    if (item == last) {
      break;
    }
  }
}

/*********************** draw cursor ************************/

static void draw_text_decoration(SpaceText *st, ARegion *region)
{
  Text *text = st->text;
  int vcurl, vcurc, vsell, vselc, hidden = 0;
  int x, y, w, i;
  int offl, offc;
  const int lheight = TXT_LINE_HEIGHT(st);

  /* Convert to view space character coordinates to determine if cursor is hidden */
  wrap_offset(st, region, text->sell, text->selc, &offl, &offc);
  vsell = txt_get_span(text->lines.first, text->sell) - st->top + offl;
  vselc = text_get_char_pos(st, text->sell->line, text->selc) - st->left + offc;

  if (vselc < 0) {
    vselc = 0;
    hidden = 1;
  }

  if (text->curl == text->sell && text->curc == text->selc && !st->line_hlight && hidden) {
    /* Nothing to draw here */
    return;
  }

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  /* Draw the selection */
  if (text->curl != text->sell || text->curc != text->selc) {
    /* Convert all to view space character coordinates */
    wrap_offset(st, region, text->curl, text->curc, &offl, &offc);
    vcurl = txt_get_span(text->lines.first, text->curl) - st->top + offl;
    vcurc = text_get_char_pos(st, text->curl->line, text->curc) - st->left + offc;

    if (vcurc < 0) {
      vcurc = 0;
    }

    immUniformThemeColor(TH_SHADE2);

    x = TXT_BODY_LEFT(st);
    y = region->winy;
    if (st->flags & ST_SCROLL_SELECT) {
      y += st->runtime.scroll_ofs_px[1];
    }

    if (vcurl == vsell) {
      y -= vcurl * lheight;

      if (vcurc < vselc) {
        immRecti(pos,
                 x + vcurc * st->runtime.cwidth_px,
                 y,
                 x + vselc * st->runtime.cwidth_px,
                 y - lheight);
      }
      else {
        immRecti(pos,
                 x + vselc * st->runtime.cwidth_px,
                 y,
                 x + vcurc * st->runtime.cwidth_px,
                 y - lheight);
      }
    }
    else {
      int froml, fromc, tol, toc;

      if (vcurl < vsell) {
        froml = vcurl;
        tol = vsell;
        fromc = vcurc;
        toc = vselc;
      }
      else {
        froml = vsell;
        tol = vcurl;
        fromc = vselc;
        toc = vcurc;
      }

      y -= froml * lheight;

      immRecti(pos, x + fromc * st->runtime.cwidth_px - U.pixelsize, y, region->winx, y - lheight);
      y -= lheight;

      for (i = froml + 1; i < tol; i++) {
        immRecti(pos, x - U.pixelsize, y, region->winx, y - lheight);
        y -= lheight;
      }

      if (x + toc * st->runtime.cwidth_px > x) {
        immRecti(pos, x - U.pixelsize, y, x + toc * st->runtime.cwidth_px, y - lheight);
      }
      y -= lheight;
    }
  }

  if (st->line_hlight) {
    int y1, y2;

    if (st->wordwrap) {
      int visible_lines = text_get_visible_lines(st, region, text->sell->line);

      wrap_offset_in_line(st, region, text->sell, text->selc, &offl, &offc);

      y1 = region->winy - (vsell - offl) * lheight;
      if (st->flags & ST_SCROLL_SELECT) {
        y1 += st->runtime.scroll_ofs_px[1];
      }
      y2 = y1 - (lheight * visible_lines);
    }
    else {
      y1 = region->winy - vsell * lheight;
      if (st->flags & ST_SCROLL_SELECT) {
        y1 += st->runtime.scroll_ofs_px[1];
      }
      y2 = y1 - (lheight);
    }

    if (!(y1 < 0 || y2 > region->winy)) { /* check we need to draw */
      float highlight_color[4];
      UI_GetThemeColor4fv(TH_TEXT, highlight_color);
      highlight_color[3] = 0.1f;
      immUniformColor4fv(highlight_color);
      GPU_blend(GPU_BLEND_ALPHA);
      immRecti(pos, 0, y1, region->winx, y2);
      GPU_blend(GPU_BLEND_NONE);
    }
  }

  if (!hidden) {
    /* Draw the cursor itself (we draw the sel. cursor as this is the leading edge) */
    x = TXT_BODY_LEFT(st) + (vselc * st->runtime.cwidth_px);
    y = region->winy - vsell * lheight;
    if (st->flags & ST_SCROLL_SELECT) {
      y += st->runtime.scroll_ofs_px[1];
    }

    immUniformThemeColor(TH_HILITE);

    if (st->overwrite) {
      char ch = text->sell->line[text->selc];

      y += TXT_LINE_SPACING(st);
      w = st->runtime.cwidth_px;
      if (ch == '\t') {
        w *= st->tabnumber - (vselc + st->left) % st->tabnumber;
      }

      immRecti(
          pos, x, y - lheight - U.pixelsize, x + w + U.pixelsize, y - lheight - (3 * U.pixelsize));
    }
    else {
      immRecti(pos, x - U.pixelsize, y, x + U.pixelsize, y - lheight);
    }
  }

  immUnbindProgram();
}

/******************* draw matching brackets *********************/

static void draw_brackets(const SpaceText *st, const TextDrawContext *tdc, ARegion *region)
{
  TextLine *startl, *endl, *linep;
  Text *text = st->text;
  int b, fc, find, stack, viewc, viewl, offl, offc, x, y;
  int startc, endc, c;

  char ch;

  /* syntax_highlight must be on or else the format string will be null */
  if (!text->curl || !tdc->syntax_highlight) {
    return;
  }

  startl = text->curl;
  startc = text->curc;
  b = text_check_bracket(startl->line[startc]);
  if (b == 0 && startc > 0) {
    b = text_check_bracket(startl->line[--startc]);
  }
  if (b == 0) {
    return;
  }

  linep = startl;
  c = startc;
  fc = BLI_str_utf8_offset_to_index(linep->line, startc);
  endl = NULL;
  endc = -1;
  find = -b;
  stack = 0;

  /* Don't highlight brackets if syntax HL is off or bracket in string or comment. */
  if (!linep->format || linep->format[fc] == FMT_TYPE_STRING ||
      linep->format[fc] == FMT_TYPE_COMMENT) {
    return;
  }

  if (b > 0) {
    /* opening bracket, search forward for close */
    fc++;
    c += BLI_str_utf8_size_safe(linep->line + c);
    while (linep) {
      while (c < linep->len) {
        if (linep->format && linep->format[fc] != FMT_TYPE_STRING &&
            linep->format[fc] != FMT_TYPE_COMMENT) {
          b = text_check_bracket(linep->line[c]);
          if (b == find) {
            if (stack == 0) {
              endl = linep;
              endc = c;
              break;
            }
            stack--;
          }
          else if (b == -find) {
            stack++;
          }
        }
        fc++;
        c += BLI_str_utf8_size_safe(linep->line + c);
      }
      if (endl) {
        break;
      }
      linep = linep->next;
      c = 0;
      fc = 0;
    }
  }
  else {
    /* closing bracket, search backward for open */
    fc--;
    if (c > 0) {
      c -= linep->line + c - BLI_str_prev_char_utf8(linep->line + c);
    }
    while (linep) {
      while (fc >= 0) {
        if (linep->format && linep->format[fc] != FMT_TYPE_STRING &&
            linep->format[fc] != FMT_TYPE_COMMENT) {
          b = text_check_bracket(linep->line[c]);
          if (b == find) {
            if (stack == 0) {
              endl = linep;
              endc = c;
              break;
            }
            stack--;
          }
          else if (b == -find) {
            stack++;
          }
        }
        fc--;
        if (c > 0) {
          c -= linep->line + c - BLI_str_prev_char_utf8(linep->line + c);
        }
      }
      if (endl) {
        break;
      }
      linep = linep->prev;
      if (linep) {
        if (linep->format) {
          fc = strlen(linep->format) - 1;
        }
        else {
          fc = -1;
        }
        if (linep->len) {
          c = BLI_str_prev_char_utf8(linep->line + linep->len) - linep->line;
        }
        else {
          fc = -1;
        }
      }
    }
  }

  if (!endl || endc == -1) {
    return;
  }

  UI_FontThemeColor(tdc->font_id, TH_HILITE);
  x = TXT_BODY_LEFT(st);
  y = region->winy - st->runtime.lheight_px;
  if (st->flags & ST_SCROLL_SELECT) {
    y += st->runtime.scroll_ofs_px[1];
  }

  /* draw opening bracket */
  ch = startl->line[startc];
  wrap_offset(st, region, startl, startc, &offl, &offc);
  viewc = text_get_char_pos(st, startl->line, startc) - st->left + offc;

  if (viewc >= 0) {
    viewl = txt_get_span(text->lines.first, startl) - st->top + offl;

    text_font_draw_character(
        tdc, x + viewc * st->runtime.cwidth_px, y - viewl * TXT_LINE_HEIGHT(st), ch);
    text_font_draw_character(
        tdc, x + viewc * st->runtime.cwidth_px + 1, y - viewl * TXT_LINE_HEIGHT(st), ch);
  }

  /* draw closing bracket */
  ch = endl->line[endc];
  wrap_offset(st, region, endl, endc, &offl, &offc);
  viewc = text_get_char_pos(st, endl->line, endc) - st->left + offc;

  if (viewc >= 0) {
    viewl = txt_get_span(text->lines.first, endl) - st->top + offl;

    text_font_draw_character(
        tdc, x + viewc * st->runtime.cwidth_px, y - viewl * TXT_LINE_HEIGHT(st), ch);
    text_font_draw_character(
        tdc, x + viewc * st->runtime.cwidth_px + 1, y - viewl * TXT_LINE_HEIGHT(st), ch);
  }
}

/*********************** main region drawing *************************/

void draw_text_main(SpaceText *st, ARegion *region)
{
  TextDrawContext tdc = {0};
  Text *text = st->text;
  TextFormatType *tft;
  TextLine *tmp;
  rcti scroll, back;
  char linenr[12];
  int i, x, y, winx, linecount = 0, lineno = 0;
  int wraplinecount = 0, wrap_skip = 0;
  int margin_column_x;

  /* if no text, nothing to do */
  if (!text) {
    return;
  }

  /* dpi controlled line height and font size */
  st->runtime.lheight_px = (U.widget_unit * st->lheight) / 20;

  /* don't draw lines below this */
  const int clip_min_y = -(int)(st->runtime.lheight_px - 1);

  st->runtime.viewlines = (st->runtime.lheight_px) ?
                              (int)(region->winy - clip_min_y) / TXT_LINE_HEIGHT(st) :
                              0;

  text_draw_context_init(st, &tdc);

  text_update_drawcache(st, region);

  /* make sure all the positional pointers exist */
  if (!text->curl || !text->sell || !text->lines.first || !text->lines.last) {
    txt_clean_text(text);
  }

  /* update rects for scroll */
  calc_text_rcts(st, region, &scroll, &back); /* scroll will hold the entire bar size */

  /* update syntax formatting if needed */
  tft = ED_text_format_get(text);
  tmp = text->lines.first;
  lineno = 0;
  for (i = 0; i < st->top && tmp; i++) {
    if (tdc.syntax_highlight && !tmp->format) {
      tft->format_line(st, tmp, false);
    }

    if (st->wordwrap) {
      int lines = text_get_visible_lines_no(st, lineno);

      if (wraplinecount + lines > st->top) {
        wrap_skip = st->top - wraplinecount;
        break;
      }

      wraplinecount += lines;
      tmp = tmp->next;
      linecount++;
    }
    else {
      tmp = tmp->next;
      linecount++;
    }

    lineno++;
  }

  text_font_begin(&tdc);

  tdc.cwidth_px = max_ii((int)BLF_fixed_width(tdc.font_id), 1);
  st->runtime.cwidth_px = tdc.cwidth_px;

  /* draw line numbers background */
  if (st->showlinenrs) {
    uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    immUniformThemeColor(TH_GRID);
    immRecti(pos, 0, 0, TXT_NUMCOL_WIDTH(st), region->winy);
    immUnbindProgram();
  }
  else {
    st->runtime.line_number_display_digits = 0; /* not used */
  }

  x = TXT_BODY_LEFT(st);
  y = region->winy - st->runtime.lheight_px;
  int viewlines = st->runtime.viewlines;
  if (st->flags & ST_SCROLL_SELECT) {
    y += st->runtime.scroll_ofs_px[1];
    viewlines += 1;
  }

  winx = region->winx - TXT_SCROLL_WIDTH;

  /* draw cursor, margin, selection and highlight */
  draw_text_decoration(st, region);

  /* draw the text */
  UI_FontThemeColor(tdc.font_id, TH_TEXT);

  for (i = 0; y > clip_min_y && i < viewlines && tmp; i++, tmp = tmp->next) {
    if (tdc.syntax_highlight && !tmp->format) {
      tft->format_line(st, tmp, false);
    }

    if (st->showlinenrs && !wrap_skip) {
      /* Draw line number. */
      UI_FontThemeColor(tdc.font_id, (tmp == text->curl) ? TH_HILITE : TH_LINENUMBERS);
      BLI_snprintf(linenr,
                   sizeof(linenr),
                   "%*d",
                   st->runtime.line_number_display_digits,
                   i + linecount + 1);
      text_font_draw(&tdc, TXT_NUMCOL_PAD * st->runtime.cwidth_px, y, linenr);
      /* Change back to text color. */
      UI_FontThemeColor(tdc.font_id, TH_TEXT);
    }

    if (st->wordwrap) {
      /* draw word wrapped text */
      int lines = text_draw_wrapped(st, &tdc, tmp->line, x, y, winx - x, tmp->format, wrap_skip);
      y -= lines * TXT_LINE_HEIGHT(st);
    }
    else {
      /* draw unwrapped text */
      text_draw(
          st, &tdc, tmp->line, st->left, region->winx / st->runtime.cwidth_px, x, y, tmp->format);
      y -= TXT_LINE_HEIGHT(st);
    }

    wrap_skip = 0;
  }

  if (st->flags & ST_SHOW_MARGIN) {
    margin_column_x = x + st->runtime.cwidth_px * (st->margin_column - st->left);
    if (margin_column_x >= x) {
      uint pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
      float margin_color[4];
      UI_GetThemeColor4fv(TH_TEXT, margin_color);
      margin_color[3] = 0.2f;
      immUniformColor4fv(margin_color);
      GPU_blend(GPU_BLEND_ALPHA);
      immRecti(pos, margin_column_x, 0, margin_column_x + U.pixelsize, region->winy);
      GPU_blend(GPU_BLEND_NONE);
      immUnbindProgram();
    }
  }

  /* draw other stuff */
  draw_brackets(st, &tdc, region);
  draw_textscroll(st, &scroll, &back);
  /* draw_documentation(st, region); - No longer supported */
  draw_suggestion_list(st, &tdc, region);

  text_font_end(&tdc);
}

/************************** update ***************************/

void text_update_character_width(SpaceText *st)
{
  TextDrawContext tdc = {0};

  text_draw_context_init(st, &tdc);

  text_font_begin(&tdc);
  st->runtime.cwidth_px = BLF_fixed_width(tdc.font_id);
  st->runtime.cwidth_px = MAX2(st->runtime.cwidth_px, (char)1);
  text_font_end(&tdc);
}

/* Moves the view to the cursor location,
 * also used to make sure the view isn't outside the file */
void text_scroll_to_cursor(SpaceText *st, ARegion *region, const bool center)
{
  Text *text;
  int i, x, winx = region->winx;

  if (ELEM(NULL, st, st->text, st->text->curl)) {
    return;
  }

  text = st->text;

  text_update_character_width(st);

  i = txt_get_span(text->lines.first, text->sell);
  if (st->wordwrap) {
    int offl, offc;
    wrap_offset(st, region, text->sell, text->selc, &offl, &offc);
    i += offl;
  }

  if (center) {
    if (st->top + st->runtime.viewlines <= i || st->top > i) {
      st->top = i - st->runtime.viewlines / 2;
    }
  }
  else {
    if (st->top + st->runtime.viewlines <= i) {
      st->top = i - (st->runtime.viewlines - 1);
    }
    else if (st->top > i) {
      st->top = i;
    }
  }

  if (st->wordwrap) {
    st->left = 0;
  }
  else {
    x = st->runtime.cwidth_px * (text_get_char_pos(st, text->sell->line, text->selc) - st->left);
    winx -= TXT_BODY_LEFT(st) + TXT_SCROLL_WIDTH;

    if (center) {
      if (x <= 0 || x > winx) {
        st->left += (x - winx / 2) / st->runtime.cwidth_px;
      }
    }
    else {
      if (x <= 0) {
        st->left += ((x + 1) / st->runtime.cwidth_px) - 1;
      }
      else if (x > winx) {
        st->left += ((x - (winx + 1)) / st->runtime.cwidth_px) + 1;
      }
    }
  }

  if (st->top < 0) {
    st->top = 0;
  }
  if (st->left < 0) {
    st->left = 0;
  }

  st->runtime.scroll_ofs_px[0] = 0;
  st->runtime.scroll_ofs_px[1] = 0;
}

/* takes an area instead of a region, use for listeners */
void text_scroll_to_cursor__area(SpaceText *st, ScrArea *area, const bool center)
{
  ARegion *region;

  if (ELEM(NULL, st, st->text, st->text->curl)) {
    return;
  }

  region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);

  if (region) {
    text_scroll_to_cursor(st, region, center);
  }
}

void text_update_cursor_moved(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  SpaceText *st = CTX_wm_space_text(C);

  text_scroll_to_cursor__area(st, area, true);
}

/**
 * Takes a cursor (row, character) and returns x,y pixel coords.
 */
bool ED_text_region_location_from_cursor(SpaceText *st,
                                         ARegion *region,
                                         const int cursor_co[2],
                                         int r_pixel_co[2])
{
  TextLine *line = NULL;

  if (!st->text) {
    goto error;
  }

  line = BLI_findlink(&st->text->lines, cursor_co[0]);
  if (!line || (cursor_co[1] < 0) || (cursor_co[1] > line->len)) {
    goto error;
  }
  else {
    int offl, offc;
    int linenr_offset = TXT_BODY_LEFT(st);
    /* handle tabs as well! */
    int char_pos = text_get_char_pos(st, line->line, cursor_co[1]);

    wrap_offset(st, region, line, cursor_co[1], &offl, &offc);
    r_pixel_co[0] = (char_pos + offc - st->left) * st->runtime.cwidth_px + linenr_offset;
    r_pixel_co[1] = (cursor_co[0] + offl - st->top) * TXT_LINE_HEIGHT(st);
    r_pixel_co[1] = (region->winy - (r_pixel_co[1] + (TXT_BODY_LPAD * st->runtime.cwidth_px))) -
                    st->runtime.lheight_px;
  }
  return true;

error:
  r_pixel_co[0] = r_pixel_co[1] = -1;
  return false;
}
