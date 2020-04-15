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
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"
#include "BKE_blender_version.h"
#include "BKE_context.h"
#include "BKE_screen.h"

#include "BLF_api.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm.h"

static void wm_block_splash_close(bContext *C, void *arg_block, void *UNUSED(arg))
{
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, arg_block);
}

static uiBlock *wm_block_create_splash(bContext *C, ARegion *region, void *arg_unused);

static void wm_block_splash_refreshmenu(bContext *C, void *UNUSED(arg_block), void *UNUSED(arg))
{
  ARegion *region_menu = CTX_wm_menu(C);
  ED_region_tag_refresh_ui(region_menu);
}

static void wm_block_splash_add_label(uiBlock *block, const char *label, int x, int *y)
{
  if (!(label && label[0])) {
    return;
  }

  UI_block_emboss_set(block, UI_EMBOSS_NONE);

  uiBut *but = uiDefBut(
      block, UI_BTYPE_LABEL, 0, label, 0, *y, x, UI_UNIT_Y, NULL, 0, 0, 0, 0, NULL);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
  UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);

  /* 1 = UI_SELECT, internal flag to draw in white. */
  UI_but_flag_enable(but, 1);
  UI_block_emboss_set(block, UI_EMBOSS);
  *y -= 12 * U.dpi_fac;
}

static void wm_block_splash_add_labels(uiBlock *block, int x, int y)
{
  /* Version number. */
  const char *version_cycle = NULL;
  bool show_build_info = true;

  if (STREQ(STRINGIFY(BLENDER_VERSION_CYCLE), "alpha")) {
    version_cycle = " Alpha";
  }
  else if (STREQ(STRINGIFY(BLENDER_VERSION_CYCLE), "beta")) {
    version_cycle = " Beta";
  }
  else if (STREQ(STRINGIFY(BLENDER_VERSION_CYCLE), "rc")) {
    version_cycle = " Release Candidate";
    show_build_info = false;
  }
  else if (STREQ(STRINGIFY(BLENDER_VERSION_CYCLE), "release")) {
    version_cycle = STRINGIFY(BLENDER_VERSION_CHAR);
    show_build_info = false;
  }

  const char *version_cycle_number = "";
  if (strlen(STRINGIFY(BLENDER_VERSION_CYCLE_NUMBER))) {
    version_cycle_number = " " STRINGIFY(BLENDER_VERSION_CYCLE_NUMBER);
  }

  char version_buf[256] = "\0";
  BLI_snprintf(version_buf,
               sizeof(version_buf),
               "v %d.%d%s%s",
               BLENDER_VERSION / 100,
               BLENDER_VERSION % 100,
               version_cycle,
               version_cycle_number);

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

static void wm_block_splash_image_roundcorners_add(ImBuf *ibuf)
{
  uchar *rct = (uchar *)ibuf->rect;

  if (rct) {
    bTheme *btheme = UI_GetTheme();
    const float roundness = btheme->tui.wcol_menu_back.roundness * U.dpi_fac;
    const int size = roundness * 20;

    if (size < ibuf->x && size < ibuf->y) {
      /* Y-axis initial offset. */
      rct += 4 * (ibuf->y - size) * ibuf->x;

      for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++, rct += 4) {
          const float pixel = 1.0 / size;
          const float u = pixel * x;
          const float v = pixel * y;
          const float distance = sqrt(u * u + v * v);

          /* Pointer offset to the alpha value of pixel. */
          /* Note, the left corner is flipped in the X-axis. */
          const int offset_l = 4 * (size - x - x - 1) + 3;
          const int offset_r = 4 * (ibuf->x - size) + 3;

          if (distance > 1.0) {
            rct[offset_l] = 0;
            rct[offset_r] = 0;
          }
          else {
            /* Create a single pixel wide transition for anti-aliasing.
             * Invert the distance and map its range [0, 1] to [0, pixel]. */
            const float fac = (1.0 - distance) * size;

            if (fac > 1.0) {
              continue;
            }

            const uchar alpha = unit_float_to_uchar_clamp(fac);
            rct[offset_l] = alpha;
            rct[offset_r] = alpha;
          }
        }

        /* X-axis offset to the next row. */
        rct += 4 * (ibuf->x - size);
      }
    }
  }
}

static ImBuf *wm_block_splash_image(int width, int *r_height)
{
#ifndef WITH_HEADLESS
  extern char datatoc_splash_png[];
  extern int datatoc_splash_png_size;

  ImBuf *ibuf = NULL;
  int height = 0;

  if (U.app_template[0] != '\0') {
    char splash_filepath[FILE_MAX];
    char template_directory[FILE_MAX];
    if (BKE_appdir_app_template_id_search(
            U.app_template, template_directory, sizeof(template_directory))) {
      BLI_join_dirfile(splash_filepath, sizeof(splash_filepath), template_directory, "splash.png");
      ibuf = IMB_loadiffname(splash_filepath, IB_rect, NULL);
    }
  }

  if (ibuf == NULL) {
    const uchar *splash_data = (const uchar *)datatoc_splash_png;
    size_t splash_data_size = datatoc_splash_png_size;
    ibuf = IMB_ibImageFromMemory(splash_data, splash_data_size, IB_rect, NULL, "<splash screen>");
  }

  if (ibuf) {
    height = (width * ibuf->y) / ibuf->x;
    if (width != ibuf->x || height != ibuf->y) {
      IMB_scaleImBuf(ibuf, width, height);
    }

    wm_block_splash_image_roundcorners_add(ibuf);
    IMB_premultiply_alpha(ibuf);
  }

  *r_height = height;

  return ibuf;
#else
  UNUSED_VARS(r_unit_size);
  return NULL;
#endif
}

static uiBlock *wm_block_create_splash(bContext *C, ARegion *region, void *UNUSED(arg))
{
  uiBlock *block;
  uiBut *but;
  const uiStyle *style = UI_style_get_dpi();

  block = UI_block_begin(C, region, "splash", UI_EMBOSS);

  /* note on UI_BLOCK_NO_WIN_CLIP, the window size is not always synchronized
   * with the OS when the splash shows, window clipping in this case gives
   * ugly results and clipping the splash isn't useful anyway, just disable it [#32938] */
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_KEEP_OPEN | UI_BLOCK_NO_WIN_CLIP);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  int splash_width = 500.0f * U.dpi_fac;
  int splash_height;

  /* Would be nice to support caching this, so it only has to be re-read (and likely resized) on
   * first draw or if the image changed. */
  ImBuf *ibuf = wm_block_splash_image(splash_width, &splash_height);

  but = uiDefButImage(block, ibuf, 0, 0.5f * U.widget_unit, splash_width, splash_height, NULL);

  UI_but_func_set(but, wm_block_splash_close, block, NULL);
  UI_block_func_set(block, wm_block_splash_refreshmenu, block, NULL);

  wm_block_splash_add_labels(block, splash_width, splash_height - 13 * U.dpi_fac);

  const int layout_margin_x = U.dpi_fac * 26;
  uiLayout *layout = UI_block_layout(block,
                                     UI_LAYOUT_VERTICAL,
                                     UI_LAYOUT_PANEL,
                                     layout_margin_x,
                                     0,
                                     splash_width - (layout_margin_x * 2),
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
