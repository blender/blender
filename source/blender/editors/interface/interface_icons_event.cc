/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * A special set of icons to represent input devices,
 * this is a mix of text (via fonts) and a handful of custom glyphs for special keys.
 *
 * Event codes are used as identifiers.
 */

#include "GPU_batch.h"
#include "GPU_state.h"

#include "BLI_string.h"

#include "BLF_api.h"

#include "UI_interface.hh"

#include "interface_intern.hh"

static void icon_draw_rect_input_text(
    const rctf *rect, const float color[4], const char *str, float font_size, float v_offset)
{
  BLF_batch_draw_flush();
  const int font_id = BLF_default();
  BLF_color4fv(font_id, color);
  BLF_size(font_id, font_size * UI_SCALE_FAC);
  float width, height;
  BLF_width_and_height(font_id, str, BLF_DRAW_STR_DUMMY_MAX, &width, &height);
  const float x = trunc(rect->xmin + (((rect->xmax - rect->xmin) - width) / 2.0f));
  const float y = rect->ymin + (((rect->ymax - rect->ymin) - height) / 2.0f) +
                  (v_offset * UI_SCALE_FAC);
  BLF_position(font_id, x, y, 0.0f);
  BLF_draw(font_id, str, BLF_DRAW_STR_DUMMY_MAX);
  BLF_batch_draw_flush();
}

void icon_draw_rect_input(
    float x, float y, int w, int h, float /*alpha*/, short event_type, short /*event_value*/)
{
  rctf rect{};
  rect.xmin = int(x) - U.pixelsize;
  rect.xmax = int(x + w + U.pixelsize);
  rect.ymin = int(y);
  rect.ymax = int(y + h);

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
    const char str[2] = {char('A' + (event_type - EVT_AKEY)), '\0'};
    icon_draw_rect_input_text(&rect, color, str, 13.0f, 0.0f);
  }
  else if ((event_type >= EVT_F1KEY) && (event_type <= EVT_F24KEY)) {
    char str[4];
    SNPRINTF(str, "F%d", 1 + (event_type - EVT_F1KEY));
    icon_draw_rect_input_text(&rect, color, str, event_type > EVT_F9KEY ? 8.5f : 11.5f, 0.0f);
  }
  else if (event_type == EVT_LEFTSHIFTKEY) { /* Right Shift has already been converted to left. */
    const char str[] = BLI_STR_UTF8_UPWARDS_WHITE_ARROW;
    icon_draw_rect_input_text(&rect, color, str, 16.0f, 0.0f);
  }
  else if (event_type == EVT_LEFTCTRLKEY) { /* Right Shift has already been converted to left. */
    if (platform == MACOS) {
      const char str[] = BLI_STR_UTF8_UP_ARROWHEAD;
      icon_draw_rect_input_text(&rect, color, str, 21.0f, -8.0f);
    }
    else {
      icon_draw_rect_input_text(&rect, color, "Ctrl", 9.0f, 0.0f);
    }
  }
  else if (event_type == EVT_LEFTALTKEY) { /* Right Alt has already been converted to left. */
    if (platform == MACOS) {
      const char str[] = BLI_STR_UTF8_OPTION_KEY;
      icon_draw_rect_input_text(&rect, color, str, 13.0f, 0.0f);
    }
    else {
      icon_draw_rect_input_text(&rect, color, "Alt", 10.0f, 0.0f);
    }
  }
  else if (event_type == EVT_OSKEY) {
    if (platform == MACOS) {
      const char str[] = BLI_STR_UTF8_PLACE_OF_INTEREST_SIGN;
      icon_draw_rect_input_text(&rect, color, str, 16.0f, 0.0f);
    }
    else if (platform == MSWIN) {
      const char str[] = BLI_STR_UTF8_BLACK_DIAMOND_MINUS_WHITE_X;
      icon_draw_rect_input_text(&rect, color, str, 16.0f, 0.0f);
    }
    else {
      icon_draw_rect_input_text(&rect, color, "OS", 10.0f, 0.0f);
    }
  }
  else if (event_type == EVT_DELKEY) {
    icon_draw_rect_input_text(&rect, color, "Del", 9.0f, 0.0f);
  }
  else if (event_type == EVT_TABKEY) {
    const char str[] = BLI_STR_UTF8_HORIZONTAL_TAB_KEY;
    icon_draw_rect_input_text(&rect, color, str, 18.0f, -1.5f);
  }
  else if (event_type == EVT_HOMEKEY) {
    icon_draw_rect_input_text(&rect, color, "Home", 6.0f, 0.0f);
  }
  else if (event_type == EVT_ENDKEY) {
    icon_draw_rect_input_text(&rect, color, "End", 8.0f, 0.0f);
  }
  else if (event_type == EVT_RETKEY) {
    const char str[] = BLI_STR_UTF8_RETURN_SYMBOL;
    icon_draw_rect_input_text(&rect, color, str, 17.0f, -1.0f);
  }
  else if (event_type == EVT_ESCKEY) {
    if (platform == MACOS) {
      const char str[] = BLI_STR_UTF8_BROKEN_CIRCLE_WITH_NORTHWEST_ARROW;
      icon_draw_rect_input_text(&rect, color, str, 21.0f, -1.0f);
    }
    else {
      icon_draw_rect_input_text(&rect, color, "Esc", 8.5f, 0.0f);
    }
  }
  else if (event_type == EVT_PAGEUPKEY) {
    const char str[] = "P" BLI_STR_UTF8_UPWARDS_ARROW;
    icon_draw_rect_input_text(&rect, color, str, 12.0f, 0.0f);
  }
  else if (event_type == EVT_PAGEDOWNKEY) {
    const char str[] = "P" BLI_STR_UTF8_DOWNWARDS_ARROW;
    icon_draw_rect_input_text(&rect, color, str, 12.0f, 0.0f);
  }
  else if (event_type == EVT_LEFTARROWKEY) {
    const char str[] = BLI_STR_UTF8_LEFTWARDS_ARROW;
    icon_draw_rect_input_text(&rect, color, str, 18.0f, -1.5f);
  }
  else if (event_type == EVT_UPARROWKEY) {
    const char str[] = BLI_STR_UTF8_UPWARDS_ARROW;
    icon_draw_rect_input_text(&rect, color, str, 16.0f, 0.0f);
  }
  else if (event_type == EVT_RIGHTARROWKEY) {
    const char str[] = BLI_STR_UTF8_RIGHTWARDS_ARROW;
    icon_draw_rect_input_text(&rect, color, str, 18.0f, -1.5f);
  }
  else if (event_type == EVT_DOWNARROWKEY) {
    const char str[] = BLI_STR_UTF8_DOWNWARDS_ARROW;
    icon_draw_rect_input_text(&rect, color, str, 16.0f, 0.0f);
  }
  else if (event_type == EVT_SPACEKEY) {
    const char str[] = BLI_STR_UTF8_OPEN_BOX;
    icon_draw_rect_input_text(&rect, color, str, 20.0f, 2.0f);
  }
}
