/* SPDX-FileCopyrightText: 2007 Blender Authors
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

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_path_utils.hh"
#include "BLI_utildefines.h"

#include "BKE_appdir.hh"
#include "BKE_blender_version.h"
#include "BKE_context.hh"
#include "BKE_preferences.h"

#include "BLT_translation.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "ED_datafiles.h"
#include "ED_screen.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm.hh"

/* -------------------------------------------------------------------- */
/** \name Splash Screen
 * \{ */

static void wm_block_splash_close(bContext *C, void *arg_block, void * /*arg*/)
{
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, static_cast<uiBlock *>(arg_block));
}

static void wm_block_splash_add_label(uiBlock *block, const char *label, int x, int y)
{
  if (!(label && label[0])) {
    return;
  }

  UI_block_emboss_set(block, blender::ui::EmbossType::None);

  uiBut *but = uiDefBut(
      block, ButType::Label, 0, label, 0, y, x, UI_UNIT_Y, nullptr, 0, 0, std::nullopt);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
  UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);

  /* Regardless of theme, this text should always be bright white. */
  uchar color[4] = {255, 255, 255, 255};
  UI_but_color_set(but, color);

  UI_block_emboss_set(block, blender::ui::EmbossType::Emboss);
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
#endif /* !WITH_HEADLESS */

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
      ibuf = IMB_load_image_from_filepath(splash_filepath, IB_byte_data);
    }
  }

  if (ibuf == nullptr) {
    const char *custom_splash_path = BLI_getenv("BLENDER_CUSTOM_SPLASH");
    if (custom_splash_path) {
      ibuf = IMB_load_image_from_filepath(custom_splash_path, IB_byte_data);
    }
  }

  if (ibuf == nullptr) {
    const uchar *splash_data = (const uchar *)datatoc_splash_png;
    size_t splash_data_size = datatoc_splash_png_size;
    ibuf = IMB_load_image_from_memory(
        splash_data, splash_data_size, IB_byte_data, "<splash screen>");
  }

  if (ibuf) {
    ibuf->planes = 32; /* The image might not have an alpha channel. */
    height = (width * ibuf->y) / ibuf->x;
    if (width != ibuf->x || height != ibuf->y) {
      IMB_scale(ibuf, width, height, IMBScaleFilter::Box, false);
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

static ImBuf *wm_block_splash_banner_image(int *r_width,
                                           int *r_height,
                                           int max_width,
                                           int max_height)
{
  ImBuf *ibuf = nullptr;
  int height = 0;
  int width = max_width;
#ifndef WITH_HEADLESS

  const char *custom_splash_path = BLI_getenv("BLENDER_CUSTOM_SPLASH_BANNER");
  if (custom_splash_path) {
    ibuf = IMB_load_image_from_filepath(custom_splash_path, IB_byte_data);
  }

  if (!ibuf) {
    return nullptr;
  }

  ibuf->planes = 32; /* The image might not have an alpha channel. */

  width = ibuf->x;
  height = ibuf->y;
  if (width > 0 && height > 0 && (width > max_width || height > max_height)) {
    const float splash_ratio = max_width / float(max_height);
    const float banner_ratio = ibuf->x / float(ibuf->y);

    if (banner_ratio > splash_ratio) {
      /* The banner is wider than the splash image. */
      width = max_width;
      height = max_width / banner_ratio;
    }
    else if (banner_ratio < splash_ratio) {
      /* The banner is taller than the splash image. */
      height = max_height;
      width = max_height * banner_ratio;
    }
    else {
      width = max_width;
      height = max_height;
    }
    if (width != ibuf->x || height != ibuf->y) {
      IMB_scale(ibuf, width, height, IMBScaleFilter::Box, false);
    }
  }

  IMB_premultiply_alpha(ibuf);

#else
  UNUSED_VARS(max_height);
#endif
  *r_height = height;
  *r_width = width;
  return ibuf;
}

/**
 * Close the splash when opening a file-selector.
 */
static void wm_block_splash_close_on_fileselect(bContext *C, void *arg1, void * /*arg2*/)
{
  wmWindow *win = CTX_wm_window(C);
  if (!win) {
    return;
  }

  /* Check for the event as this will run before the new window/area has been created. */
  bool has_fileselect = false;
  LISTBASE_FOREACH (const wmEvent *, event, &win->runtime->event_queue) {
    if (event->type == EVT_FILESELECT) {
      has_fileselect = true;
      break;
    }
  }

  if (has_fileselect) {
    wm_block_splash_close(C, arg1, nullptr);
  }
}

#if defined(__APPLE__)
/* Check if Blender is running under Rosetta for the purpose of displaying a splash screen warning.
 * From Apple's WWDC 2020 Session - Explore the new system architecture of Apple Silicon Macs.
 * Time code: 14:31 - https://developer.apple.com/videos/play/wwdc2020/10686/ */

#  include <sys/sysctl.h>

static int is_using_macos_rosetta()
{
  int ret = 0;
  size_t size = sizeof(ret);

  if (sysctlbyname("sysctl.proc_translated", &ret, &size, nullptr, 0) != -1) {
    return ret;
  }
  /* If "sysctl.proc_translated" is not present then must be native. */
  if (errno == ENOENT) {
    return 0;
  }
  return -1;
}
#endif /* __APPLE__ */

static uiBlock *wm_block_splash_create(bContext *C, ARegion *region, void * /*arg*/)
{
  const uiStyle *style = UI_style_get_dpi();

  uiBlock *block = UI_block_begin(C, region, "splash", blender::ui::EmbossType::Emboss);

  /* Note on #UI_BLOCK_NO_WIN_CLIP, the window size is not always synchronized
   * with the OS when the splash shows, window clipping in this case gives
   * ugly results and clipping the splash isn't useful anyway, just disable it #32938. */
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_KEEP_OPEN | UI_BLOCK_NO_WIN_CLIP);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  int splash_width = style->widget.points * 45 * UI_SCALE_FAC;
  CLAMP_MAX(splash_width, WM_window_native_pixel_x(CTX_wm_window(C)) * 0.7f);
  int splash_height;

  /* Would be nice to support caching this, so it only has to be re-read (and likely resized) on
   * first draw or if the image changed. */
  ImBuf *ibuf = wm_block_splash_image(splash_width, &splash_height);
  /* This should never happen, if it does - don't crash. */
  if (LIKELY(ibuf)) {
    uiBut *but = uiDefButImage(
        block, ibuf, 0, 0.5f * U.widget_unit, splash_width, splash_height, nullptr);

    UI_but_func_set(but, wm_block_splash_close, block, nullptr);

    wm_block_splash_add_label(block,
                              BKE_blender_version_string(),
                              splash_width - 8.0 * UI_SCALE_FAC,
                              splash_height - 13.0 * UI_SCALE_FAC);
  }

  /* Banner image passed through the environment, to overlay on the splash and
   * indicate a custom Blender version. Transparency can be used. To replace the
   * full splash screen, see BLENDER_CUSTOM_SPLASH. */
  int banner_width = 0;
  int banner_height = 0;
  ImBuf *bannerbuf = wm_block_splash_banner_image(
      &banner_width, &banner_height, splash_width, splash_height);
  if (bannerbuf) {
    uiBut *banner_but = uiDefButImage(
        block, bannerbuf, 0, 0.5f * U.widget_unit, banner_width, banner_height, nullptr);

    UI_but_func_set(banner_but, wm_block_splash_close, block, nullptr);
  }

  const int layout_margin_x = UI_SCALE_FAC * 26;
  uiLayout &layout = blender::ui::block_layout(block,
                                               blender::ui::LayoutDirection::Vertical,
                                               blender::ui::LayoutType::Panel,
                                               layout_margin_x,
                                               0,
                                               splash_width - (layout_margin_x * 2),
                                               UI_SCALE_FAC * 110,
                                               0,
                                               style);

  MenuType *mt;

  /* Draw setup screen if no preferences have been saved yet. */
  if (!blender::bke::preferences::exists()) {
    mt = WM_menutype_find("WM_MT_splash_quick_setup", true);

    /* The #UI_BLOCK_QUICK_SETUP flag prevents the button text from being left-aligned,
     * as it is for all menus due to the #UI_BLOCK_LOOP flag, see in #ui_def_but. */
    UI_block_flag_enable(block, UI_BLOCK_QUICK_SETUP);
  }
  else {
    mt = WM_menutype_find("WM_MT_splash", true);
  }

  UI_block_func_set(block, wm_block_splash_close_on_fileselect, block, nullptr);

  if (mt) {
    UI_menutype_draw(C, mt, &layout);
  }

/* Displays a warning if blender is being emulated via Rosetta (macOS) or XTA (Windows) */
#if defined(__APPLE__) || defined(_M_X64)
#  if defined(__APPLE__)
  if (is_using_macos_rosetta() > 0)
#  elif defined(_M_X64)
  const char *proc_id = BLI_getenv("PROCESSOR_IDENTIFIER");
  if (proc_id && strncmp(proc_id, "ARM", 3) == 0)
#  endif
  {
    layout.separator(2.0f, LayoutSeparatorType::Line);

    uiLayout *split = &layout.split(0.725, true);
    uiLayout *row1 = &split->row(true);
    uiLayout *row2 = &split->row(true);

    row1->label(RPT_("Intel binary detected. Expect reduced performance."), ICON_ERROR);

    PointerRNA op_ptr = row2->op("WM_OT_url_open",
                                 CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Learn More"),
                                 ICON_URL,
                                 blender::wm::OpCallContext::InvokeDefault,
                                 UI_ITEM_NONE);
#  if defined(__APPLE__)
    RNA_string_set(
        &op_ptr,
        "url",
        "https://docs.blender.org/manual/en/latest/getting_started/installing/macos.html");
#  elif defined(_M_X64)
    RNA_string_set(
        &op_ptr,
        "url",
        "https://docs.blender.org/manual/en/latest/getting_started/installing/windows.html");
#  endif

    layout.separator();
  }
#endif

  UI_block_bounds_set_centered(block, 0);

  return block;
}

static wmOperatorStatus wm_splash_invoke(bContext *C,
                                         wmOperator * /*op*/,
                                         const wmEvent * /*event*/)
{
  UI_popup_block_invoke(C, wm_block_splash_create, nullptr, nullptr);

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Splash Screen: About
 * \{ */

static uiBlock *wm_block_about_create(bContext *C, ARegion *region, void * /*arg*/)
{
  const uiStyle *style = UI_style_get_dpi();
  const int dialog_width = style->widget.points * 42 * UI_SCALE_FAC;

  uiBlock *block = UI_block_begin(C, region, "about", blender::ui::EmbossType::Emboss);

  UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_LOOP | UI_BLOCK_NO_WIN_CLIP);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  uiLayout &layout = blender::ui::block_layout(block,
                                               blender::ui::LayoutDirection::Vertical,
                                               blender::ui::LayoutType::Panel,
                                               0,
                                               0,
                                               dialog_width,
                                               0,
                                               0,
                                               style);

/* Blender logo. */
#ifndef WITH_HEADLESS
  constexpr bool show_color = false;
  const float size = 0.2f * dialog_width;

  ImBuf *ibuf = UI_svg_icon_bitmap(ICON_BLENDER_LOGO_LARGE, size, show_color);

  if (ibuf) {
    bTheme *btheme = UI_GetTheme();
    const uchar *color = btheme->tui.wcol_menu_back.text_sel;

    /* The top margin. */
    uiLayout *row = &layout.row(false);
    row->separator(0.2f);

    /* The logo image. */
    row = &layout.row(false);
    row->alignment_set(blender::ui::LayoutAlign::Left);
    uiDefButImage(block, ibuf, 0, U.widget_unit, ibuf->x, ibuf->y, show_color ? nullptr : color);

    /* Padding below the logo. */
    row = &layout.row(false);
    row->separator(2.7f);
  }
#endif /* !WITH_HEADLESS */

  uiLayout *col = &layout.column(true);

  uiItemL_ex(col, IFACE_("Blender"), ICON_NONE, true, false);

  MenuType *mt = WM_menutype_find("WM_MT_splash_about", true);
  if (mt) {
    UI_menutype_draw(C, mt, col);
  }

  UI_block_bounds_set_centered(block, 22 * UI_SCALE_FAC);

  return block;
}

static wmOperatorStatus wm_splash_about_invoke(bContext *C,
                                               wmOperator * /*op*/,
                                               const wmEvent * /*event*/)
{
  UI_popup_block_invoke(C, wm_block_about_create, nullptr, nullptr);

  return OPERATOR_FINISHED;
}

void WM_OT_splash_about(wmOperatorType *ot)
{
  ot->name = "About Blender";
  ot->idname = "WM_OT_splash_about";
  ot->description = "Open a window with information about Blender";

  ot->invoke = wm_splash_about_invoke;
  ot->poll = WM_operator_winactive;
}

/** \} */
