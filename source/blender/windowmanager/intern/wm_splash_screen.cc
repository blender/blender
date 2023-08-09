/* SPDX-FileCopyrightText: 2007 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include <cstring>

#include "CLG_log.h"

#include "DNA_ID.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"
#include "BKE_blender_version.h"
#include "BKE_context.h"
#include "BKE_screen.h"

#include "BLT_translation.h"

#include "BLF_api.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "ED_datafiles.h"
#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm.hh"

static void wm_block_close(bContext *C, void *arg_block, void * /*arg*/)
{
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, static_cast<uiBlock *>(arg_block));
}

static void wm_block_splash_add_label(uiBlock *block, const char *label, int x, int y)
{
  if (!(label && label[0])) {
    return;
  }

  UI_block_emboss_set(block, UI_EMBOSS_NONE);

  uiBut *but = uiDefBut(
      block, UI_BTYPE_LABEL, 0, label, 0, y, x, UI_UNIT_Y, nullptr, 0, 0, 0, 0, nullptr);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
  UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);

  /* 1 = UI_SELECT, internal flag to draw in white. */
  UI_but_flag_enable(but, 1);
  UI_block_emboss_set(block, UI_EMBOSS);
}

#ifndef WITH_HEADLESS
static void wm_block_splash_image_roundcorners_add(ImBuf *ibuf)
{
  uchar *rct = ibuf->byte_buffer.data;
  if (!rct) {
    return;
  }

  bTheme *btheme = UI_GetTheme();
  const float roundness = btheme->tui.wcol_menu_back.roundness * UI_SCALE_FAC;
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
        /* NOTE: the left corner is flipped in the X-axis. */
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
#endif /* WITH_HEADLESS */

static ImBuf *wm_block_splash_image(int width, int *r_height)
{
  ImBuf *ibuf = nullptr;
  int height = 0;
#ifndef WITH_HEADLESS
  if (U.app_template[0] != '\0') {
    char splash_filepath[FILE_MAX];
    char template_directory[FILE_MAX];
    if (BKE_appdir_app_template_id_search(
            U.app_template, template_directory, sizeof(template_directory)))
    {
      BLI_path_join(splash_filepath, sizeof(splash_filepath), template_directory, "splash.png");
      ibuf = IMB_loadiffname(splash_filepath, IB_rect, nullptr);
    }
  }

  if (ibuf == nullptr) {
    const uchar *splash_data = (const uchar *)datatoc_splash_png;
    size_t splash_data_size = datatoc_splash_png_size;
    ibuf = IMB_ibImageFromMemory(
        splash_data, splash_data_size, IB_rect, nullptr, "<splash screen>");
  }

  if (ibuf) {
    height = (width * ibuf->y) / ibuf->x;
    if (width != ibuf->x || height != ibuf->y) {
      IMB_scaleImBuf(ibuf, width, height);
    }

    wm_block_splash_image_roundcorners_add(ibuf);
    IMB_premultiply_alpha(ibuf);
  }

#else
  UNUSED_VARS(width);
#endif
  *r_height = height;
  return ibuf;
}

static uiBlock *wm_block_create_splash(bContext *C, ARegion *region, void * /*arg*/)
{
  const uiStyle *style = UI_style_get_dpi();

  uiBlock *block = UI_block_begin(C, region, "splash", UI_EMBOSS);

  /* note on UI_BLOCK_NO_WIN_CLIP, the window size is not always synchronized
   * with the OS when the splash shows, window clipping in this case gives
   * ugly results and clipping the splash isn't useful anyway, just disable it #32938. */
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_KEEP_OPEN | UI_BLOCK_NO_WIN_CLIP);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  const int text_points_max = MAX2(style->widget.points, style->widgetlabel.points);
  int splash_width = text_points_max * 45 * UI_SCALE_FAC;
  CLAMP_MAX(splash_width, CTX_wm_window(C)->sizex * 0.7f);
  int splash_height;

  /* Would be nice to support caching this, so it only has to be re-read (and likely resized) on
   * first draw or if the image changed. */
  ImBuf *ibuf = wm_block_splash_image(splash_width, &splash_height);
  /* This should never happen, if it does - don't crash. */
  if (LIKELY(ibuf)) {
    uiBut *but = uiDefButImage(
        block, ibuf, 0, 0.5f * U.widget_unit, splash_width, splash_height, nullptr);

    UI_but_func_set(but, wm_block_close, block, nullptr);

    wm_block_splash_add_label(block,
                              BKE_blender_version_string(),
                              splash_width - 8.0 * UI_SCALE_FAC,
                              splash_height - 13.0 * UI_SCALE_FAC);
  }

  const int layout_margin_x = UI_SCALE_FAC * 26;
  uiLayout *layout = UI_block_layout(block,
                                     UI_LAYOUT_VERTICAL,
                                     UI_LAYOUT_PANEL,
                                     layout_margin_x,
                                     0,
                                     splash_width - (layout_margin_x * 2),
                                     UI_SCALE_FAC * 110,
                                     0,
                                     style);

  MenuType *mt;
  char userpref[FILE_MAX];
  const char *const cfgdir = BKE_appdir_folder_id(BLENDER_USER_CONFIG, nullptr);

  if (cfgdir) {
    BLI_path_join(userpref, sizeof(userpref), cfgdir, BLENDER_USERPREF_FILE);
  }

  /* Draw setup screen if no preferences have been saved yet. */
  if (!BLI_exists(userpref)) {
    mt = WM_menutype_find("WM_MT_splash_quick_setup", true);

    /* The #UI_BLOCK_QUICK_SETUP flag prevents the button text from being left-aligned,
     * as it is for all menus due to the #UI_BLOCK_LOOP flag, see in #ui_def_but. */
    UI_block_flag_enable(block, UI_BLOCK_QUICK_SETUP);
  }
  else {
    mt = WM_menutype_find("WM_MT_splash", true);
  }

  if (mt) {
    UI_menutype_draw(C, mt, layout);
  }

  UI_block_bounds_set_centered(block, 0);

  return block;
}

static int wm_splash_invoke(bContext *C, wmOperator * /*op*/, const wmEvent * /*event*/)
{
  UI_popup_block_invoke(C, wm_block_create_splash, nullptr, nullptr);

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

static uiBlock *wm_block_create_about(bContext *C, ARegion *region, void * /*arg*/)
{
  const uiStyle *style = UI_style_get_dpi();
  const int text_points_max = MAX2(style->widget.points, style->widgetlabel.points);
  const int dialog_width = text_points_max * 42 * UI_SCALE_FAC;

  uiBlock *block = UI_block_begin(C, region, "about", UI_EMBOSS);

  UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_LOOP | UI_BLOCK_NO_WIN_CLIP);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  uiLayout *layout = UI_block_layout(
      block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, dialog_width, 0, 0, style);

/* Blender logo. */
#ifndef WITH_HEADLESS

  const uchar *blender_logo_data = (const uchar *)datatoc_blender_logo_png;
  size_t blender_logo_data_size = datatoc_blender_logo_png_size;
  ImBuf *ibuf = IMB_ibImageFromMemory(
      blender_logo_data, blender_logo_data_size, IB_rect, nullptr, "blender_logo");

  if (ibuf) {
    int width = 0.5 * dialog_width;
    int height = (width * ibuf->y) / ibuf->x;

    IMB_premultiply_alpha(ibuf);
    IMB_scaleImBuf(ibuf, width, height);

    bTheme *btheme = UI_GetTheme();
    const uchar *color = btheme->tui.wcol_menu_back.text_sel;

    /* The top margin. */
    uiLayout *row = uiLayoutRow(layout, false);
    uiItemS_ex(row, 0.2f);

    /* The logo image. */
    row = uiLayoutRow(layout, false);
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_LEFT);
    uiDefButImage(block, ibuf, 0, U.widget_unit, width, height, color);

    /* Padding below the logo. */
    row = uiLayoutRow(layout, false);
    uiItemS_ex(row, 2.7f);
  }
#endif /* WITH_HEADLESS */

  uiLayout *col = uiLayoutColumn(layout, true);

  uiItemL_ex(col, IFACE_("Blender"), ICON_NONE, true, false);

  MenuType *mt = WM_menutype_find("WM_MT_splash_about", true);
  if (mt) {
    UI_menutype_draw(C, mt, col);
  }

  UI_block_bounds_set_centered(block, 22 * UI_SCALE_FAC);

  return block;
}

static int wm_about_invoke(bContext *C, wmOperator * /*op*/, const wmEvent * /*event*/)
{
  UI_popup_block_invoke(C, wm_block_create_about, nullptr, nullptr);

  return OPERATOR_FINISHED;
}

void WM_OT_splash_about(wmOperatorType *ot)
{
  ot->name = "About Blender";
  ot->idname = "WM_OT_splash_about";
  ot->description = "Open a window with information about Blender";

  ot->invoke = wm_about_invoke;
  ot->poll = WM_operator_winactive;
}
