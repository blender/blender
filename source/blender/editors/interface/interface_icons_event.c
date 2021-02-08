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
 */

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

static void icon_draw_rect_input_text(const rctf *rect,
                                      const float color[4],
                                      const char *str,
                                      int font_size)
{
  BLF_batch_draw_flush();
  const int font_id = BLF_default();
  BLF_color4fv(font_id, color);
  BLF_size(font_id, font_size * U.pixelsize, U.dpi);
  float width, height;
  BLF_width_and_height(font_id, str, BLF_DRAW_STR_DUMMY_MAX, &width, &height);
  const float x = rect->xmin + (((rect->xmax - rect->xmin) - width) / 2.0f);
  const float y = rect->ymin + (((rect->ymax - rect->ymin) - height) / 2.0f);
  BLF_position(font_id, x, y, 0.0f);
  BLF_draw(font_id, str, BLF_DRAW_STR_DUMMY_MAX);
  BLF_batch_draw_flush();
}

static void icon_draw_rect_input_symbol(const rctf *rect, const float color[4], const char *str)
{
  BLF_batch_draw_flush();
  const int font_id = blf_mono_font;
  BLF_color4fv(font_id, color);
  BLF_size(font_id, 19 * U.pixelsize, U.dpi);
  const float x = rect->xmin + (2.0f * U.pixelsize);
  const float y = rect->ymin + (1.0f * U.pixelsize);
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
  float color[4];
  GPU_line_width(1.0f);
  UI_GetThemeColor4fv(TH_TEXT, color);
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_aa(
      &(const rctf){
          .xmin = (int)x - U.pixelsize,
          .xmax = (int)(x + w),
          .ymin = (int)y,
          .ymax = (int)(y + h),
      },
      false,
      3.0f * U.pixelsize,
      color);

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

  const rctf rect = {
      .xmin = x,
      .ymin = y,
      .xmax = x + w,
      .ymax = y + h,
  };

  if ((event_type >= EVT_AKEY) && (event_type <= EVT_ZKEY)) {
    const char str[2] = {'A' + (event_type - EVT_AKEY), '\0'};
    icon_draw_rect_input_text(&rect, color, str, 13);
  }
  else if ((event_type >= EVT_F1KEY) && (event_type <= EVT_F12KEY)) {
    char str[4];
    SNPRINTF(str, "F%d", 1 + (event_type - EVT_F1KEY));
    icon_draw_rect_input_text(&rect, color, str, event_type > EVT_F9KEY ? 8 : 10);
  }
  else if (event_type == EVT_LEFTSHIFTKEY) {
    icon_draw_rect_input_symbol(&rect, color, (const char[]){0xe2, 0x87, 0xa7, 0x0});
  }
  else if (event_type == EVT_LEFTCTRLKEY) {
    if (platform == MACOS) {
      icon_draw_rect_input_symbol(&rect, color, (const char[]){0xe2, 0x8c, 0x83, 0x0});
    }
    else {
      icon_draw_rect_input_text(&rect, color, "Ctrl", 9);
    }
  }
  else if (event_type == EVT_LEFTALTKEY) {
    if (platform == MACOS) {
      icon_draw_rect_input_symbol(&rect, color, (const char[]){0xe2, 0x8c, 0xa5, 0x0});
    }
    else {
      icon_draw_rect_input_text(&rect, color, "Alt", 10);
    }
  }
  else if (event_type == EVT_OSKEY) {
    if (platform == MACOS) {
      icon_draw_rect_input_symbol(&rect, color, (const char[]){0xe2, 0x8c, 0x98, 0x0});
    }
    else if (platform == MSWIN) {
      icon_draw_rect_input_symbol(&rect, color, (const char[]){0xe2, 0x9d, 0x96, 0x0});
    }
    else {
      icon_draw_rect_input_text(&rect, color, "OS", 10);
    }
  }
  else if (event_type == EVT_DELKEY) {
    icon_draw_rect_input_text(&rect, color, "Del", 9);
  }
  else if (event_type == EVT_TABKEY) {
    icon_draw_rect_input_symbol(&rect, color, (const char[]){0xe2, 0xad, 0xbe, 0x0});
  }
  else if (event_type == EVT_HOMEKEY) {
    icon_draw_rect_input_text(&rect, color, "Home", 6);
  }
  else if (event_type == EVT_ENDKEY) {
    icon_draw_rect_input_text(&rect, color, "End", 8);
  }
  else if (event_type == EVT_RETKEY) {
    icon_draw_rect_input_symbol(&rect, color, (const char[]){0xe2, 0x8f, 0x8e, 0x0});
  }
  else if (event_type == EVT_ESCKEY) {
    if (platform == MACOS) {
      icon_draw_rect_input_symbol(&rect, color, (const char[]){0xe2, 0x8e, 0x8b, 0x0});
    }
    else {
      icon_draw_rect_input_text(&rect, color, "Esc", 8);
    }
  }
  else if (event_type == EVT_PAGEUPKEY) {
    icon_draw_rect_input_text(&rect, color, (const char[]){'P', 0xe2, 0x86, 0x91, 0x0}, 8);
  }
  else if (event_type == EVT_PAGEDOWNKEY) {
    icon_draw_rect_input_text(&rect, color, (const char[]){'P', 0xe2, 0x86, 0x93, 0x0}, 8);
  }
  else if (event_type == EVT_LEFTARROWKEY) {
    icon_draw_rect_input_symbol(&rect, color, (const char[]){0xe2, 0x86, 0x90, 0x0});
  }
  else if (event_type == EVT_UPARROWKEY) {
    icon_draw_rect_input_symbol(&rect, color, (const char[]){0xe2, 0x86, 0x91, 0x0});
  }
  else if (event_type == EVT_RIGHTARROWKEY) {
    icon_draw_rect_input_symbol(&rect, color, (const char[]){0xe2, 0x86, 0x92, 0x0});
  }
  else if (event_type == EVT_DOWNARROWKEY) {
    icon_draw_rect_input_symbol(&rect, color, (const char[]){0xe2, 0x86, 0x93, 0x0});
  }
  else if (event_type == EVT_SPACEKEY) {
    icon_draw_rect_input_symbol(&rect, color, (const char[]){0xe2, 0x90, 0xa3, 0x0});
  }
}
