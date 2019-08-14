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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 *
 * This file contains the splash screen logic (the `WM_OT_splash` operator).
 *
 * - Loads the splash image.
 * - Displaying version information.
 * - Lists New Files (application templates).
 * - Lists Recent files.
 * - Links to web sites.
 */

#include <string.h>

#include "CLG_log.h"

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"
#include "BKE_blender_version.h"
#include "BKE_context.h"
#include "BKE_screen.h"

#include "BLF_api.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "ED_screen.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm.h"

static void wm_block_splash_close(bContext *C, void *arg_block, void *UNUSED(arg))
{
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, arg_block);
}

static uiBlock *wm_block_create_splash(bContext *C, ARegion *ar, void *arg_unused);

static void wm_block_splash_refreshmenu(bContext *C, void *UNUSED(arg_block), void *UNUSED(arg))
{
  ARegion *ar_menu = CTX_wm_menu(C);
  ED_region_tag_refresh_ui(ar_menu);
}

static void wm_block_splash_add_label(uiBlock *block, const char *label, int x, int *y)
{
  if (!(label && label[0])) {
    return;
  }

  uiStyle *style = UI_style_get();

  BLF_size(style->widgetlabel.uifont_id, style->widgetlabel.points, U.pixelsize * U.dpi);
  int label_width = BLF_width(style->widgetlabel.uifont_id, label, strlen(label));
  label_width = label_width + U.widget_unit;

  UI_block_emboss_set(block, UI_EMBOSS_NONE);

  uiBut *but = uiDefBut(block,
                        UI_BTYPE_LABEL,
                        0,
                        label,
                        x - label_width,
                        *y,
                        label_width,
                        UI_UNIT_Y,
                        NULL,
                        0,
                        0,
                        0,
                        0,
                        NULL);

  /* 1 = UI_SELECT, internal flag to draw in white. */
  UI_but_flag_enable(but, 1);
  UI_block_emboss_set(block, UI_EMBOSS);
  *y -= 12 * U.dpi_fac;
}

static void wm_block_splash_add_labels(uiBlock *block, int x, int y)
{
  /* Version number. */
  const char *version_suffix = NULL;
  bool show_build_info = true;

  if (STREQ(STRINGIFY(BLENDER_VERSION_CYCLE), "alpha")) {
    version_suffix = " Alpha";
  }
  else if (STREQ(STRINGIFY(BLENDER_VERSION_CYCLE), "beta")) {
    version_suffix = " Beta";
  }
  else if (STREQ(STRINGIFY(BLENDER_VERSION_CYCLE), "rc")) {
    version_suffix = " Release Candidate";
    show_build_info = false;
  }
  else if (STREQ(STRINGIFY(BLENDER_VERSION_CYCLE), "release")) {
    version_suffix = STRINGIFY(BLENDER_VERSION_CHAR);
    show_build_info = false;
  }

  char version_buf[256] = "\0";
  BLI_snprintf(version_buf,
               sizeof(version_buf),
               "v %d.%d%s",
               BLENDER_VERSION / 100,
               BLENDER_VERSION % 100,
               version_suffix);

  wm_block_splash_add_label(block, version_buf, x, &y);

#ifdef WITH_BUILDINFO
  if (show_build_info) {
    extern unsigned long build_commit_timestamp;
    extern char build_hash[], build_commit_date[], build_commit_time[], build_branch[];

    /* Date, hidden for builds made from tag. */
    if (build_commit_timestamp != 0) {
      char date_buf[256] = "\0";
      BLI_snprintf(
          date_buf, sizeof(date_buf), "Date: %s %s", build_commit_date, build_commit_time);
      wm_block_splash_add_label(block, date_buf, x, &y);
    }

    /* Hash. */
    char hash_buf[256] = "\0";
    BLI_snprintf(hash_buf, sizeof(hash_buf), "Hash: %s", build_hash);
    wm_block_splash_add_label(block, hash_buf, x, &y);

    /* Branch. */
    if (!STREQ(build_branch, "master")) {
      char branch_buf[256] = "\0";
      BLI_snprintf(branch_buf, sizeof(branch_buf), "Branch: %s", build_branch);

      wm_block_splash_add_label(block, branch_buf, x, &y);
    }
  }
#else
  UNUSED_VARS(show_build_info);
#endif /* WITH_BUILDINFO */
}

static ImBuf *wm_block_splash_image(void)
{
#ifndef WITH_HEADLESS
  extern char datatoc_splash_png[];
  extern int datatoc_splash_png_size;
  extern char datatoc_splash_2x_png[];
  extern int datatoc_splash_2x_png_size;

  ImBuf *ibuf = NULL;

  if (U.dpi_fac > 1.0) {
    ibuf = IMB_ibImageFromMemory((const uchar *)datatoc_splash_2x_png,
                                 datatoc_splash_2x_png_size,
                                 IB_rect,
                                 NULL,
                                 "<splash screen>");
  }
  else {
    ibuf = IMB_ibImageFromMemory((const uchar *)datatoc_splash_png,
                                 datatoc_splash_png_size,
                                 IB_rect,
                                 NULL,
                                 "<splash screen>");
  }

  /* overwrite splash with template image */
  if (U.app_template[0] != '\0') {
    ImBuf *ibuf_template = NULL;
    char splash_filepath[FILE_MAX];
    char template_directory[FILE_MAX];

    if (BKE_appdir_app_template_id_search(
            U.app_template, template_directory, sizeof(template_directory))) {
      BLI_join_dirfile(splash_filepath,
                       sizeof(splash_filepath),
                       template_directory,
                       (U.dpi_fac > 1.0) ? "splash_2x.png" : "splash.png");
      ibuf_template = IMB_loadiffname(splash_filepath, IB_rect, NULL);
      if (ibuf_template) {
        const int x_expect = ibuf->x;
        const int y_expect = 250 * (int)U.dpi_fac;
        /* don't cover the header text */
        if (ibuf_template->x == x_expect && ibuf_template->y == y_expect) {
          memcpy(ibuf->rect,
                 ibuf_template->rect,
                 ibuf_template->x * ibuf_template->y * sizeof(char[4]));
        }
        else {
          CLOG_ERROR(WM_LOG_OPERATORS,
                     "Splash expected %dx%d found %dx%d, ignoring: %s\n",
                     x_expect,
                     y_expect,
                     ibuf_template->x,
                     ibuf_template->y,
                     splash_filepath);
        }
        IMB_freeImBuf(ibuf_template);
      }
    }
  }
  return ibuf;
#else
  return NULL;
#endif
}

static uiBlock *wm_block_create_splash(bContext *C, ARegion *ar, void *UNUSED(arg))
{
  uiBlock *block;
  uiBut *but;
  uiStyle *style = UI_style_get();

  block = UI_block_begin(C, ar, "splash", UI_EMBOSS);

  /* note on UI_BLOCK_NO_WIN_CLIP, the window size is not always synchronized
   * with the OS when the splash shows, window clipping in this case gives
   * ugly results and clipping the splash isn't useful anyway, just disable it [#32938] */
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_KEEP_OPEN | UI_BLOCK_NO_WIN_CLIP);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  ImBuf *ibuf = wm_block_splash_image();
  but = uiDefBut(block,
                 UI_BTYPE_IMAGE,
                 0,
                 "",
                 0,
                 0.5f * U.widget_unit,
                 U.dpi_fac * 501,
                 U.dpi_fac * 250,
                 /* Button owns the imbuf now. */
                 ibuf,
                 0.0,
                 0.0,
                 0,
                 0,
                 "");
  UI_but_func_set(but, wm_block_splash_close, block, NULL);
  UI_block_func_set(block, wm_block_splash_refreshmenu, block, NULL);

  int x = U.dpi_fac * 502;
  int y = U.dpi_fac * 237;

  wm_block_splash_add_labels(block, x, y);

  uiLayout *layout = UI_block_layout(block,
                                     UI_LAYOUT_VERTICAL,
                                     UI_LAYOUT_PANEL,
                                     U.dpi_fac * 26,
                                     0,
                                     U.dpi_fac * 450,
                                     U.dpi_fac * 110,
                                     0,
                                     style);

  MenuType *mt = WM_menutype_find("WM_MT_splash", true);
  if (mt) {
    UI_menutype_draw(C, mt, layout);
  }

  UI_block_bounds_set_centered(block, 0);

  return block;
}

static int wm_splash_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *UNUSED(event))
{
  UI_popup_block_invoke(C, wm_block_create_splash, NULL, NULL);

  return OPERATOR_FINISHED;
}

void WM_OT_splash(wmOperatorType *ot)
{
  ot->name = "Splash Screen";
  ot->idname = "WM_OT_splash";
  ot->description = "Open the splash screen with release info";

  ot->invoke = wm_splash_invoke;
  ot->poll = WM_operator_winactive;
}
