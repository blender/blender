/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * A special set of icons to represent input devices,
 * this is a mix of text (via fonts) and a handful of custom glyphs for special keys.
 *
 * Event codes are used as identifiers.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "GPU_batch.h"
#include "GPU_immediate.h"
#include "GPU_state.h"

#include "BLI_blenlib.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_curve_types.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_workspace_types.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "BKE_appdir.h"
#include "BKE_icons.h"
#include "BKE_studiolight.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "BLF_api.h"

#include "DEG_depsgraph.h"

#include "DRW_engine.h"

#include "ED_datafiles.h"
#include "ED_keyframes_draw.h"
#include "ED_render.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

static void icon_draw_rect_input_text(
    const rctf *rect, const float color[4], const char *str, float font_size, float v_offset)
{
  BLF_batch_draw_flush();
  const int font_id = BLF_default();
  BLF_color4fv(font_id, color);
  BLF_size(font_id, font_size * U.pixelsize, U.dpi);
  float width, height;
  BLF_width_and_height(font_id, str, BLF_DRAW_STR_DUMMY_MAX, &width, &height);
  const float x = trunc(rect->xmin + (((rect->xmax - rect->xmin) - width) / 2.0f));
  const float y = rect->ymin + (((rect->ymax - rect->ymin) - height) / 2.0f) +
                  (v_offset * U.dpi_fac);
  BLF_position(font_id, x, y, 0.0f);
  BLF_draw(font_id, str, BLF_DRAW_STR_DUMMY_MAX);
  BLF_batch_draw_flush();
}

void icon_draw_rect_input(float x,
                          float y,
                          int w,
                          int h,
                          float UNUSED(alpha),
                          short event_type,
                          short UNUSED(event_value))
{
  rctf rect = {
      .xmin = (int)x - U.pixelsize,
      .xmax = (int)(x + w + U.pixelsize),
      .ymin = (int)(y),
      .ymax = (int)(y + h),
  };
  float color[4];
  GPU_line_width(1.0f);
  UI_GetThemeColor4fv(TH_TEXT, color);
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_aa(&rect, false, 3.0f * U.pixelsize, color);

  const enum {
    UNIX,
    MACOS,
    MSWIN,
  } platform =

#if defined(__APPLE__)
      MACOS
#elif defined(_WIN32)
      MSWIN
#else
      UNIX
#endif
      ;

  if ((event_type >= EVT_AKEY) && (event_type <= EVT_ZKEY)) {
    const char str[2] = {'A' + (event_type - EVT_AKEY), '\0'};
    icon_draw_rect_input_text(&rect, color, str, 13.0f, 0.0f);
  }
  else if ((event_type >= EVT_F1KEY) && (event_type <= EVT_F24KEY)) {
    char str[4];
    SNPRINTF(str, "F%d", 1 + (event_type - EVT_F1KEY));
    icon_draw_rect_input_text(&rect, color, str, event_type > EVT_F9KEY ? 8.5f : 11.5f, 0.0f);
  }
  else if (event_type == EVT_LEFTSHIFTKEY) { /* Right Shift has already been converted to left. */
    icon_draw_rect_input_text(&rect, color, (const char[]){0xe2, 0x87, 0xa7, 0x0}, 16.0f, 0.0f);
  }
  else if (event_type == EVT_LEFTCTRLKEY) { /* Right Shift has already been converted to left. */
    if (platform == MACOS) {
      icon_draw_rect_input_text(&rect, color, (const char[]){0xe2, 0x8c, 0x83, 0x0}, 21.0f, -8.0f);
    }
    else {
      icon_draw_rect_input_text(&rect, color, "Ctrl", 9.0f, 0.0f);
    }
  }
  else if (event_type == EVT_LEFTALTKEY) { /* Right Alt has already been converted to left. */
    if (platform == MACOS) {
      icon_draw_rect_input_text(&rect, color, (const char[]){0xe2, 0x8c, 0xa5, 0x0}, 13.0f, 0.0f);
    }
    else {
      icon_draw_rect_input_text(&rect, color, "Alt", 10.0f, 0.0f);
    }
  }
  else if (event_type == EVT_OSKEY) {
    if (platform == MACOS) {
      icon_draw_rect_input_text(&rect, color, (const char[]){0xe2, 0x8c, 0x98, 0x0}, 16.0f, 0.0f);
    }
    else if (platform == MSWIN) {
      icon_draw_rect_input_text(&rect, color, (const char[]){0xe2, 0x9d, 0x96, 0x0}, 16.0f, 0.0f);
    }
    else {
      icon_draw_rect_input_text(&rect, color, "OS", 10.0f, 0.0f);
    }
  }
  else if (event_type == EVT_DELKEY) {
    icon_draw_rect_input_text(&rect, color, "Del", 9.0f, 0.0f);
  }
  else if (event_type == EVT_TABKEY) {
    icon_draw_rect_input_text(&rect, color, (const char[]){0xe2, 0xad, 0xbe, 0x0}, 18.0f, -1.5f);
  }
  else if (event_type == EVT_HOMEKEY) {
    icon_draw_rect_input_text(&rect, color, "Home", 6.0f, 0.0f);
  }
  else if (event_type == EVT_ENDKEY) {
    icon_draw_rect_input_text(&rect, color, "End", 8.0f, 0.0f);
  }
  else if (event_type == EVT_RETKEY) {
    icon_draw_rect_input_text(&rect, color, (const char[]){0xe2, 0x8f, 0x8e, 0x0}, 17.0f, -1.0f);
  }
  else if (event_type == EVT_ESCKEY) {
    if (platform == MACOS) {
      icon_draw_rect_input_text(&rect, color, (const char[]){0xe2, 0x8e, 0x8b, 0x0}, 21.0f, -1.0f);
    }
    else {
      icon_draw_rect_input_text(&rect, color, "Esc", 8.5f, 0.0f);
    }
  }
  else if (event_type == EVT_PAGEUPKEY) {
    icon_draw_rect_input_text(
        &rect, color, (const char[]){'P', 0xe2, 0x86, 0x91, 0x0}, 12.0f, 0.0f);
  }
  else if (event_type == EVT_PAGEDOWNKEY) {
    icon_draw_rect_input_text(
        &rect, color, (const char[]){'P', 0xe2, 0x86, 0x93, 0x0}, 12.0f, 0.0f);
  }
  else if (event_type == EVT_LEFTARROWKEY) {
    icon_draw_rect_input_text(&rect, color, (const char[]){0xe2, 0x86, 0x90, 0x0}, 18.0f, -1.5f);
  }
  else if (event_type == EVT_UPARROWKEY) {
    icon_draw_rect_input_text(&rect, color, (const char[]){0xe2, 0x86, 0x91, 0x0}, 16.0f, 0.0f);
  }
  else if (event_type == EVT_RIGHTARROWKEY) {
    icon_draw_rect_input_text(&rect, color, (const char[]){0xe2, 0x86, 0x92, 0x0}, 18.0f, -1.5f);
  }
  else if (event_type == EVT_DOWNARROWKEY) {
    icon_draw_rect_input_text(&rect, color, (const char[]){0xe2, 0x86, 0x93, 0x0}, 16.0f, 0.0f);
  }
  else if (event_type == EVT_SPACEKEY) {
    icon_draw_rect_input_text(&rect, color, (const char[]){0xe2, 0x90, 0xa3, 0x0}, 20.0f, 2.0f);
  }
}
