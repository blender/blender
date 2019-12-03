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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spinfo
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>

#include "BLI_utildefines.h"

#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "BKE_report.h"

#include "UI_resources.h"
#include "UI_interface.h"
#include "UI_view2d.h"

#include "info_intern.h"
#include "textview.h"
#include "GPU_framebuffer.h"

/* complicates things a bit, so leaving in old simple code */
#define USE_INFO_NEWLINE

static void info_report_color(unsigned char *fg,
                              unsigned char *bg,
                              Report *report,
                              const short do_tint)
{
  int bg_id = TH_BACK, fg_id = TH_TEXT;
  int shade = do_tint ? 0 : -6;

  if (report->flag & SELECT) {
    bg_id = TH_INFO_SELECTED;
    fg_id = TH_INFO_SELECTED_TEXT;
  }
  else if (report->type & RPT_ERROR_ALL) {
    bg_id = TH_INFO_ERROR;
    fg_id = TH_INFO_ERROR_TEXT;
  }
  else if (report->type & RPT_WARNING_ALL) {
    bg_id = TH_INFO_WARNING;
    fg_id = TH_INFO_WARNING_TEXT;
  }
  else if (report->type & RPT_INFO_ALL) {
    bg_id = TH_INFO_INFO;
    fg_id = TH_INFO_INFO_TEXT;
  }
  else if (report->type & RPT_DEBUG_ALL) {
    bg_id = TH_INFO_DEBUG;
    fg_id = TH_INFO_DEBUG_TEXT;
  }
  else {
    bg_id = TH_BACK;
    fg_id = TH_TEXT;
  }

  UI_GetThemeColorShade3ubv(bg_id, shade, bg);
  UI_GetThemeColor3ubv(fg_id, fg);
}

/* reports! */
#ifdef USE_INFO_NEWLINE
static void report_textview_init__internal(TextViewContext *tvc)
{
  Report *report = (Report *)tvc->iter;
  const char *str = report->message;
  const char *next_str = strchr(str + tvc->iter_char, '\n');

  if (next_str) {
    tvc->iter_char_next = (int)(next_str - str);
  }
  else {
    tvc->iter_char_next = report->len;
  }
}

static int report_textview_skip__internal(TextViewContext *tvc)
{
  SpaceInfo *sinfo = (SpaceInfo *)tvc->arg1;
  const int report_mask = info_report_mask(sinfo);
  while (tvc->iter && (((Report *)tvc->iter)->type & report_mask) == 0) {
    tvc->iter = (void *)((Link *)tvc->iter)->prev;
  }
  return (tvc->iter != NULL);
}

#endif  // USE_INFO_NEWLINE

static int report_textview_begin(TextViewContext *tvc)
{
  // SpaceConsole *sc = (SpaceConsole *)tvc->arg1;
  ReportList *reports = (ReportList *)tvc->arg2;

  tvc->lheight = 14 * UI_DPI_FAC;  // sc->lheight;
  tvc->sel_start = 0;
  tvc->sel_end = 0;

  /* iterator */
  tvc->iter = reports->list.last;

  UI_ThemeClearColor(TH_BACK);
  GPU_clear(GPU_COLOR_BIT);

#ifdef USE_INFO_NEWLINE
  tvc->iter_tmp = 0;
  if (tvc->iter && report_textview_skip__internal(tvc)) {
    /* init the newline iterator */
    tvc->iter_char = 0;
    report_textview_init__internal(tvc);

    return true;
  }
  else {
    return false;
  }
#else
  return (tvc->iter != NULL);
#endif
}

static void report_textview_end(TextViewContext *UNUSED(tvc))
{
  /* pass */
}

#ifdef USE_INFO_NEWLINE
static int report_textview_step(TextViewContext *tvc)
{
  /* simple case, but no newline support */
  Report *report = (Report *)tvc->iter;

  if (report->len <= tvc->iter_char_next) {
    tvc->iter = (void *)((Link *)tvc->iter)->prev;
    if (tvc->iter && report_textview_skip__internal(tvc)) {
      tvc->iter_tmp++;

      tvc->iter_char = 0; /* reset start */
      report_textview_init__internal(tvc);

      return true;
    }
    else {
      return false;
    }
  }
  else {
    /* step to the next newline */
    tvc->iter_char = tvc->iter_char_next + 1;
    report_textview_init__internal(tvc);

    return true;
  }
}

static int report_textview_line_get(struct TextViewContext *tvc, const char **line, int *len)
{
  Report *report = (Report *)tvc->iter;
  *line = report->message + tvc->iter_char;
  *len = tvc->iter_char_next - tvc->iter_char;
  return 1;
}

static int report_textview_line_color(struct TextViewContext *tvc,
                                      unsigned char fg[3],
                                      unsigned char bg[3])
{
  Report *report = (Report *)tvc->iter;
  info_report_color(fg, bg, report, tvc->iter_tmp % 2);
  return TVC_LINE_FG | TVC_LINE_BG;
}

#else  // USE_INFO_NEWLINE

static int report_textview_step(TextViewContext *tvc)
{
  SpaceInfo *sinfo = (SpaceInfo *)tvc->arg1;
  const int report_mask = info_report_mask(sinfo);
  do {
    tvc->iter = (void *)((Link *)tvc->iter)->prev;
  } while (tvc->iter && (((Report *)tvc->iter)->type & report_mask) == 0);

  return (tvc->iter != NULL);
}

static int report_textview_line_get(struct TextViewContext *tvc, const char **line, int *len)
{
  Report *report = (Report *)tvc->iter;
  *line = report->message;
  *len = report->len;

  return 1;
}

static int report_textview_line_color(struct TextViewContext *tvc,
                                      unsigned char fg[3],
                                      unsigned char bg[3])
{
  Report *report = (Report *)tvc->iter;
  info_report_color(fg, bg, report, tvc->iter_tmp % 2);
  return TVC_LINE_FG | TVC_LINE_BG;
}

#endif  // USE_INFO_NEWLINE

#undef USE_INFO_NEWLINE

static void info_textview_draw_rect_calc(const ARegion *ar, rcti *draw_rect)
{
  const int margin = 4 * UI_DPI_FAC;
  draw_rect->xmin = margin;
  draw_rect->xmax = ar->winx - (V2D_SCROLL_WIDTH + margin);
  draw_rect->ymin = margin;
  /* No margin at the top (allow text to scroll off the window). */
  draw_rect->ymax = ar->winy;
}

static int info_textview_main__internal(struct SpaceInfo *sinfo,
                                        const ARegion *ar,
                                        ReportList *reports,
                                        const bool do_draw,
                                        const int mval[2],
                                        void **r_mval_pick_item,
                                        int *r_mval_pick_offset)
{
  int ret = 0;

  const View2D *v2d = &ar->v2d;

  TextViewContext tvc = {0};
  tvc.begin = report_textview_begin;
  tvc.end = report_textview_end;

  tvc.step = report_textview_step;
  tvc.line_get = report_textview_line_get;
  tvc.line_color = report_textview_line_color;
  tvc.const_colors = NULL;

  tvc.arg1 = sinfo;
  tvc.arg2 = reports;

  /* view */
  tvc.sel_start = 0;
  tvc.sel_end = 0;
  tvc.lheight = 14 * UI_DPI_FAC;  // sc->lheight;
  tvc.scroll_ymin = v2d->cur.ymin;
  tvc.scroll_ymax = v2d->cur.ymax;

  info_textview_draw_rect_calc(ar, &tvc.draw_rect);

  ret = textview_draw(&tvc, do_draw, mval, r_mval_pick_item, r_mval_pick_offset);

  return ret;
}

void *info_text_pick(struct SpaceInfo *sinfo, const ARegion *ar, ReportList *reports, int mval_y)
{
  void *mval_pick_item = NULL;
  const int mval[2] = {0, mval_y};

  info_textview_main__internal(sinfo, ar, reports, false, mval, &mval_pick_item, NULL);
  return (void *)mval_pick_item;
}

int info_textview_height(struct SpaceInfo *sinfo, const ARegion *ar, ReportList *reports)
{
  int mval[2] = {INT_MAX, INT_MAX};
  return info_textview_main__internal(sinfo, ar, reports, false, mval, NULL, NULL);
}

void info_textview_main(struct SpaceInfo *sinfo, const ARegion *ar, ReportList *reports)
{
  int mval[2] = {INT_MAX, INT_MAX};
  info_textview_main__internal(sinfo, ar, reports, true, mval, NULL, NULL);
}
