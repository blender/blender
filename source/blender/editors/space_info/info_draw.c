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

static int report_line_data(struct TextViewContext *tvc,
                                     unsigned char fg[4],
                                     unsigned char bg[4],
                                     int *icon,
                                     unsigned char icon_fg[4],
                                     unsigned char icon_bg[4])
{
  Report *report = (Report *)tvc->iter;

  /* Same text color no matter what type of report. */
  UI_GetThemeColor4ubv((report->flag & SELECT) ? TH_INFO_SELECTED_TEXT : TH_TEXT, fg);

  /* Zebra striping for background. */
  int bg_id = (report->flag & SELECT) ? TH_INFO_SELECTED : TH_BACK;
  int shade = tvc->iter_tmp % 2 ? 4 : -4;
  UI_GetThemeColorShade4ubv(bg_id, shade, bg);

  /* Icon color and backgound depend of report type. */

  int icon_fg_id;
  int icon_bg_id;

  if (report->type & RPT_ERROR_ALL) {
    icon_fg_id = TH_INFO_ERROR_TEXT;
    icon_bg_id = TH_INFO_ERROR;
    *icon = ICON_CANCEL;
  }
  else if (report->type & RPT_WARNING_ALL) {
    icon_fg_id = TH_INFO_WARNING_TEXT;
    icon_bg_id = TH_INFO_WARNING;
    *icon = ICON_ERROR;
  }
  else if (report->type & RPT_INFO_ALL) {
    icon_fg_id = TH_INFO_INFO_TEXT;
    icon_bg_id = TH_INFO_INFO;
    *icon = ICON_INFO;
  }
  else if (report->type & RPT_DEBUG_ALL) {
    icon_fg_id = TH_INFO_DEBUG_TEXT;
    icon_bg_id = TH_INFO_DEBUG;
    *icon = ICON_SYSTEM;
  }
  else if (report->type & RPT_PROPERTY) {
    icon_fg_id = TH_INFO_PROPERTY_TEXT;
    icon_bg_id = TH_INFO_PROPERTY;
    *icon = ICON_OPTIONS;
  }
  else if (report->type & RPT_OPERATOR) {
    icon_fg_id = TH_INFO_OPERATOR_TEXT;
    icon_bg_id = TH_INFO_OPERATOR;
    *icon = ICON_CHECKMARK;
  }
  else {
    *icon = ICON_NONE;
  }

  if (report->flag & SELECT) {
    icon_fg_id = TH_INFO_SELECTED;
    icon_bg_id = TH_INFO_SELECTED_TEXT;
  }

  if (*icon != ICON_NONE) {
    UI_GetThemeColor4ubv(icon_fg_id, icon_fg);
    UI_GetThemeColor4ubv(icon_bg_id, icon_bg);
    return TVC_LINE_FG | TVC_LINE_BG | TVC_LINE_ICON | TVC_LINE_ICON_FG | TVC_LINE_ICON_BG;
  }
  else {
    return TVC_LINE_FG | TVC_LINE_BG;
  }
}

/* reports! */
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

static int report_textview_begin(TextViewContext *tvc)
{
  ReportList *reports = (ReportList *)tvc->arg2;

  tvc->lheight = 14 * UI_DPI_FAC;
  tvc->sel_start = 0;
  tvc->sel_end = 0;

  /* iterator */
  tvc->iter = reports->list.last;

  UI_ThemeClearColor(TH_BACK);
  GPU_clear(GPU_COLOR_BIT);

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
}

static void report_textview_end(TextViewContext *UNUSED(tvc))
{
  /* pass */
}

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

static void info_textview_draw_rect_calc(const ARegion *ar, rcti *draw_rect)
{
  draw_rect->xmin = 0;
  draw_rect->xmax = ar->winx;
  draw_rect->ymin = 0;
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
  tvc.line_data = report_line_data;
  tvc.const_colors = NULL;

  tvc.arg1 = sinfo;
  tvc.arg2 = reports;

  /* view */
  tvc.sel_start = 0;
  tvc.sel_end = 0;
  tvc.lheight = 17 * UI_DPI_FAC;
  tvc.row_vpadding = 0.4 * tvc.lheight;
  tvc.margin_left_chars = 5;
  tvc.margin_right_chars = 2;
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
