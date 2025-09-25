/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscr
 * Making screenshots of the entire window or sub-regions.
 */

#include <cerrno>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_path_utils.hh"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_image_format.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "screen_intern.hh"

struct ScreenshotData {
  uint8_t *dumprect;
  int dumpsx, dumpsy;
  rcti crop;
  bool use_crop;

  ImageFormatData im_format;
};

/* call from both exec and invoke */
static int screenshot_data_create(bContext *C, wmOperator *op, ScrArea *area)
{
  int dumprect_size[2];

  wmWindow *win = CTX_wm_window(C);

  /* do redraw so we don't show popups/menus */
  WM_redraw_windows(C);

  uint8_t *dumprect = WM_window_pixels_read(C, win, dumprect_size);

  if (dumprect) {
    ScreenshotData *scd = MEM_callocN<ScreenshotData>("screenshot");

    scd->dumpsx = dumprect_size[0];
    scd->dumpsy = dumprect_size[1];
    scd->dumprect = dumprect;
    if (area) {
      scd->crop = area->totrct;
    }

    BKE_image_format_init(&scd->im_format);

    op->customdata = scd;

    return true;
  }
  op->customdata = nullptr;
  return false;
}

static void screenshot_data_free(wmOperator *op)
{
  ScreenshotData *scd = static_cast<ScreenshotData *>(op->customdata);

  if (scd) {
    if (scd->dumprect) {
      MEM_freeN(scd->dumprect);
    }
    MEM_freeN(scd);
    op->customdata = nullptr;
  }
}

static wmOperatorStatus screenshot_exec(bContext *C, wmOperator *op)
{
  const bool use_crop = STREQ(op->idname, "SCREEN_OT_screenshot_area");
  ScreenshotData *scd = static_cast<ScreenshotData *>(op->customdata);
  bool ok = false;

  if (scd == nullptr) {
    /* when running exec directly */
    screenshot_data_create(C, op, use_crop ? CTX_wm_area(C) : nullptr);
    scd = static_cast<ScreenshotData *>(op->customdata);
  }

  if (scd) {
    if (scd->dumprect) {
      ImBuf *ibuf;
      char filepath[FILE_MAX];

      RNA_string_get(op->ptr, "filepath", filepath);
      BLI_path_abs(filepath, BKE_main_blendfile_path_from_global());

      /* operator ensures the extension */
      ibuf = IMB_allocImBuf(scd->dumpsx, scd->dumpsy, 24, 0);
      IMB_assign_byte_buffer(ibuf, scd->dumprect, IB_DO_NOT_TAKE_OWNERSHIP);

      /* crop to show only single editor */
      if (use_crop) {
        IMB_rect_crop(ibuf, &scd->crop);
        scd->dumprect = ibuf->byte_buffer.data;
      }

      if ((scd->im_format.planes == R_IMF_PLANES_BW) &&
          (scd->im_format.imtype != R_IMF_IMTYPE_MULTILAYER))
      {
        /* bw screenshot? - users will notice if it fails! */
        IMB_color_to_bw(ibuf);
      }
      if (BKE_imbuf_write(ibuf, filepath, &scd->im_format)) {
        ok = true;
      }
      else {
        BKE_reportf(op->reports, RPT_ERROR, "Could not write image: %s", strerror(errno));
      }

      IMB_freeImBuf(ibuf);
    }
  }

  screenshot_data_free(op);

  return ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static wmOperatorStatus screenshot_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool use_crop = STREQ(op->idname, "SCREEN_OT_screenshot_area");
  ScrArea *area = nullptr;
  if (use_crop) {
    area = CTX_wm_area(C);
    bScreen *screen = CTX_wm_screen(C);
    ScrArea *area_test = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, event->xy);
    if (area_test != nullptr) {
      area = area_test;
    }
  }

  if (screenshot_data_create(C, op, area)) {
    if (RNA_struct_property_is_set(op->ptr, "filepath")) {
      return screenshot_exec(C, op);
    }

    /* extension is added by 'screenshot_check' after */
    char filepath[FILE_MAX];
    const char *blendfile_path = BKE_main_blendfile_path_from_global();
    if (blendfile_path[0] != '\0') {
      STRNCPY(filepath, blendfile_path);
      BLI_path_extension_strip(filepath); /* Strip `.blend`. */
    }
    else {
      /* As the file isn't saved, only set the name and let the file selector pick a directory. */
      STRNCPY_UTF8(filepath, DATA_("screen"));
    }
    RNA_string_set(op->ptr, "filepath", filepath);

    WM_event_add_fileselect(C, op);

    return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_CANCELLED;
}

static bool screenshot_check(bContext * /*C*/, wmOperator *op)
{
  ScreenshotData *scd = static_cast<ScreenshotData *>(op->customdata);
  return WM_operator_filesel_ensure_ext_imtype(op, &scd->im_format);
}

static void screenshot_cancel(bContext * /*C*/, wmOperator *op)
{
  screenshot_data_free(op);
}

static bool screenshot_draw_check_prop(PointerRNA * /*ptr*/,
                                       PropertyRNA *prop,
                                       void * /*user_data*/)
{
  const char *prop_id = RNA_property_identifier(prop);

  return !STREQ(prop_id, "filepath");
}

static void screenshot_draw(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  ScreenshotData *scd = static_cast<ScreenshotData *>(op->customdata);

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  /* image template */
  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, &RNA_ImageFormatSettings, &scd->im_format);
  uiTemplateImageSettings(layout, C, &ptr, false);

  /* main draw call */
  uiDefAutoButsRNA(layout,
                   op->ptr,
                   screenshot_draw_check_prop,
                   nullptr,
                   nullptr,
                   UI_BUT_LABEL_ALIGN_NONE,
                   false);
}

static bool screenshot_poll(bContext *C)
{
  if (G.background) {
    return false;
  }

  return WM_operator_winactive(C);
}

static void screen_screenshot_impl(wmOperatorType *ot)
{
  ot->invoke = screenshot_invoke;
  ot->check = screenshot_check;
  ot->exec = screenshot_exec;
  ot->cancel = screenshot_cancel;
  ot->ui = screenshot_draw;
  ot->poll = screenshot_poll;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE,
                                 FILE_SPECIAL,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

void SCREEN_OT_screenshot(wmOperatorType *ot)
{
  ot->name = "Save Screenshot";
  ot->idname = "SCREEN_OT_screenshot";
  ot->description = "Capture a picture of the whole Blender window";

  screen_screenshot_impl(ot);

  ot->flag = 0;
}

void SCREEN_OT_screenshot_area(wmOperatorType *ot)
{
  /* NOTE: the term "area" is a Blender internal name, "Editor" makes more sense for the UI. */
  ot->name = "Save Screenshot (Editor)";
  ot->idname = "SCREEN_OT_screenshot_area";
  ot->description = "Capture a picture of an editor";

  screen_screenshot_impl(ot);

  ot->flag = OPTYPE_DEPENDS_ON_CURSOR;
}
