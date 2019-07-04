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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 * Making screendumps.
 */

/** \file
 * \ingroup edscr
 */

#include <string.h>
#include <errno.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"

#include "WM_types.h"
#include "WM_api.h"

#include "screen_intern.h"

typedef struct ScreenshotData {
  unsigned int *dumprect;
  int dumpsx, dumpsy;
  rcti crop;

  ImageFormatData im_format;
} ScreenshotData;

/* call from both exec and invoke */
static int screenshot_data_create(bContext *C, wmOperator *op)
{
  int dumprect_size[2];

  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);

  /* do redraw so we don't show popups/menus */
  WM_redraw_windows(C);

  uint *dumprect = WM_window_pixels_read(wm, win, dumprect_size);

  if (dumprect) {
    ScreenshotData *scd = MEM_callocN(sizeof(ScreenshotData), "screenshot");
    ScrArea *sa = CTX_wm_area(C);

    scd->dumpsx = dumprect_size[0];
    scd->dumpsy = dumprect_size[1];
    scd->dumprect = dumprect;
    if (sa) {
      scd->crop = sa->totrct;
    }

    BKE_imformat_defaults(&scd->im_format);

    op->customdata = scd;

    return true;
  }
  else {
    op->customdata = NULL;
    return false;
  }
}

static void screenshot_data_free(wmOperator *op)
{
  ScreenshotData *scd = op->customdata;

  if (scd) {
    if (scd->dumprect) {
      MEM_freeN(scd->dumprect);
    }
    MEM_freeN(scd);
    op->customdata = NULL;
  }
}

static int screenshot_exec(bContext *C, wmOperator *op)
{
  ScreenshotData *scd = op->customdata;
  bool ok = false;

  if (scd == NULL) {
    /* when running exec directly */
    screenshot_data_create(C, op);
    scd = op->customdata;
  }

  if (scd) {
    if (scd->dumprect) {
      ImBuf *ibuf;
      char path[FILE_MAX];

      RNA_string_get(op->ptr, "filepath", path);
      BLI_path_abs(path, BKE_main_blendfile_path_from_global());

      /* operator ensures the extension */
      ibuf = IMB_allocImBuf(scd->dumpsx, scd->dumpsy, 24, 0);
      ibuf->rect = scd->dumprect;

      /* crop to show only single editor */
      if (!RNA_boolean_get(op->ptr, "full")) {
        IMB_rect_crop(ibuf, &scd->crop);
        scd->dumprect = ibuf->rect;
      }

      if (scd->im_format.planes == R_IMF_PLANES_BW) {
        /* bw screenshot? - users will notice if it fails! */
        IMB_color_to_bw(ibuf);
      }
      if (BKE_imbuf_write(ibuf, path, &scd->im_format)) {
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

static int screenshot_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (screenshot_data_create(C, op)) {
    if (RNA_struct_property_is_set(op->ptr, "filepath")) {
      return screenshot_exec(C, op);
    }

    /* extension is added by 'screenshot_check' after */
    char filepath[FILE_MAX] = "//screen";
    if (G.relbase_valid) {
      BLI_strncpy(filepath, BKE_main_blendfile_path_from_global(), sizeof(filepath));
      BLI_path_extension_replace(filepath, sizeof(filepath), ""); /* strip '.blend' */
    }
    RNA_string_set(op->ptr, "filepath", filepath);

    WM_event_add_fileselect(C, op);

    return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_CANCELLED;
}

static bool screenshot_check(bContext *UNUSED(C), wmOperator *op)
{
  ScreenshotData *scd = op->customdata;
  return WM_operator_filesel_ensure_ext_imtype(op, &scd->im_format);
}

static void screenshot_cancel(bContext *UNUSED(C), wmOperator *op)
{
  screenshot_data_free(op);
}

static bool screenshot_draw_check_prop(PointerRNA *UNUSED(ptr),
                                       PropertyRNA *prop,
                                       void *UNUSED(user_data))
{
  const char *prop_id = RNA_property_identifier(prop);

  return !(STREQ(prop_id, "filepath"));
}

static void screenshot_draw(bContext *UNUSED(C), wmOperator *op)
{
  uiLayout *layout = op->layout;
  ScreenshotData *scd = op->customdata;
  PointerRNA ptr;

  /* image template */
  RNA_pointer_create(NULL, &RNA_ImageFormatSettings, &scd->im_format, &ptr);
  uiTemplateImageSettings(layout, &ptr, false);

  /* main draw call */
  RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
  uiDefAutoButsRNA(
      layout, &ptr, screenshot_draw_check_prop, NULL, NULL, UI_BUT_LABEL_ALIGN_NONE, false);
}

static bool screenshot_poll(bContext *C)
{
  if (G.background) {
    return false;
  }

  return WM_operator_winactive(C);
}

void SCREEN_OT_screenshot(wmOperatorType *ot)
{
  /* weak: opname starting with 'save' makes filewindow give save-over */
  ot->name = "Save Screenshot";
  ot->idname = "SCREEN_OT_screenshot";
  ot->description = "Capture a picture of the active area or whole Blender window";

  ot->invoke = screenshot_invoke;
  ot->check = screenshot_check;
  ot->exec = screenshot_exec;
  ot->cancel = screenshot_cancel;
  ot->ui = screenshot_draw;
  ot->poll = screenshot_poll;

  ot->flag = 0;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE,
                                 FILE_SPECIAL,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);
  RNA_def_boolean(ot->srna,
                  "full",
                  1,
                  "Full Screen",
                  "Capture the whole window (otherwise only capture the active area)");
}
