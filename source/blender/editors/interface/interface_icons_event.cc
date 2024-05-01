/* SPDX-FileCopyrightText: 2023 Blender Authors
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

#include "GPU_state.hh"

#include "BLI_string.h"

#include "BLF_api.hh"

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
    icon_draw_rect_input_text(&rect, color, str, 14.0f, 0.0f);
  }
  else if (event_type == EVT_LEFTCTRLKEY) { /* Right Ctrl has already been converted to left. */
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
      icon_draw_rect_input_text(&rect, color, "Alt", 11.0f, 0.0f);
    }
  }
  else if (event_type == EVT_OSKEY) {
    if (platform == MACOS) {
      const char str[] = BLI_STR_UTF8_PLACE_OF_INTEREST_SIGN;
      icon_draw_rect_input_text(&rect, color, str, 13.0f, 0.0f);
    }
    else if (platform == MSWIN) {
      const char str[] = BLI_STR_UTF8_BLACK_DIAMOND_MINUS_WHITE_X;
      icon_draw_rect_input_text(&rect, color, str, 12.0f, 1.5f);
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
    icon_draw_rect_input_text(&rect, color, "Home", 5.5f, 0.0f);
  }
  else if (event_type == EVT_ENDKEY) {
    icon_draw_rect_input_text(&rect, color, "End", 8.0f, 0.0f);
  }
  else if (event_type == EVT_RETKEY) {
    const char str[] = BLI_STR_UTF8_RETURN_SYMBOL;
    icon_draw_rect_input_text(&rect, color, str, 16.0f, -2.0f);
  }
  else if (event_type == EVT_ESCKEY) {
    if (platform == MACOS) {
      const char str[] = BLI_STR_UTF8_BROKEN_CIRCLE_WITH_NORTHWEST_ARROW;
      icon_draw_rect_input_text(&rect, color, str, 16.0f, 0.0f);
    }
    else {
      icon_draw_rect_input_text(&rect, color, "Esc", 9.0f, 0.0f);
    }
  }
  else if (event_type == EVT_PAGEUPKEY) {
    const char str[] = "P" BLI_STR_UTF8_UPWARDS_ARROW;
    icon_draw_rect_input_text(&rect, color, str, 10.0f, 0.0f);
  }
  else if (event_type == EVT_PAGEDOWNKEY) {
    const char str[] = "P" BLI_STR_UTF8_DOWNWARDS_ARROW;
    icon_draw_rect_input_text(&rect, color, str, 10.0f, 0.0f);
  }
  else if (event_type == EVT_LEFTARROWKEY) {
    const char str[] = BLI_STR_UTF8_LEFTWARDS_ARROW;
    icon_draw_rect_input_text(&rect, color, str, 18.0f, 0.0f);
  }
  else if (event_type == EVT_UPARROWKEY) {
    const char str[] = BLI_STR_UTF8_UPWARDS_ARROW;
    icon_draw_rect_input_text(&rect, color, str, 16.0f, 0.0f);
  }
  else if (event_type == EVT_RIGHTARROWKEY) {
    const char str[] = BLI_STR_UTF8_RIGHTWARDS_ARROW;
    icon_draw_rect_input_text(&rect, color, str, 18.0f, 0.0f);
  }
  else if (event_type == EVT_DOWNARROWKEY) {
    const char str[] = BLI_STR_UTF8_DOWNWARDS_ARROW;
    icon_draw_rect_input_text(&rect, color, str, 16.0f, 0.0f);
  }
  else if (event_type == EVT_SPACEKEY) {
    const char str[] = BLI_STR_UTF8_OPEN_BOX;
    icon_draw_rect_input_text(&rect, color, str, 20.0f, 2.0f);
  }
  else if (event_type == BUTTON4MOUSE) {
    icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_BLACK_VERTICAL_ELLIPSE "4", 12.0f, 0.0f);
  }
  else if (event_type == BUTTON5MOUSE) {
    icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_BLACK_VERTICAL_ELLIPSE "5", 12.0f, 0.0f);
  }
  else if (event_type == BUTTON6MOUSE) {
    icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_BLACK_VERTICAL_ELLIPSE "6", 12.0f, 0.0f);
  }
  else if (event_type == BUTTON7MOUSE) {
    icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_BLACK_VERTICAL_ELLIPSE "7", 12.0f, 0.0f);
  }
  else if (event_type == TABLET_STYLUS) {
    icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_LOWER_RIGHT_PENCIL, 16.0f, 0.0f);
  }
  else if (event_type == TABLET_ERASER) {
    icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_UPPER_RIGHT_PENCIL, 16.0f, 0.0f);
  }
  else if ((event_type >= EVT_ZEROKEY) && (event_type <= EVT_NINEKEY)) {
    const char str[2] = {char('0' + (event_type - EVT_ZEROKEY)), '\0'};
    icon_draw_rect_input_text(&rect, color, str, 13.0f, 0.0f);
  }
  else if ((event_type >= EVT_PAD0) && (event_type <= EVT_PAD9)) {
    char str[5];
    SNPRINTF(str, "%s%i", BLI_STR_UTF8_SQUARE_WITH_ORTHOGONAL_CROSSHATCH, event_type - EVT_PAD0);
    icon_draw_rect_input_text(&rect, color, str, 9.0f, 0.0f);
  }
  else if (event_type == EVT_PADASTERKEY) {
    icon_draw_rect_input_text(
        &rect, color, BLI_STR_UTF8_SQUARE_WITH_ORTHOGONAL_CROSSHATCH "6", 9.0f, 0.0f);
  }
  else if (event_type == EVT_PADSLASHKEY) {
    icon_draw_rect_input_text(
        &rect, color, BLI_STR_UTF8_SQUARE_WITH_ORTHOGONAL_CROSSHATCH "/", 9.0f, 0.0f);
  }
  else if (event_type == EVT_PADMINUS) {
    icon_draw_rect_input_text(
        &rect, color, BLI_STR_UTF8_SQUARE_WITH_ORTHOGONAL_CROSSHATCH "-", 9.0f, 0.0f);
  }
  else if (event_type == EVT_PADENTER) {
    icon_draw_rect_input_text(
        &rect,
        color,
        BLI_STR_UTF8_SQUARE_WITH_ORTHOGONAL_CROSSHATCH BLI_STR_UTF8_RETURN_SYMBOL,
        8.0f,
        0.0f);
  }
  else if (event_type == EVT_PADPLUSKEY) {
    icon_draw_rect_input_text(
        &rect, color, BLI_STR_UTF8_SQUARE_WITH_ORTHOGONAL_CROSSHATCH "+", 9.0f, 0.0f);
  }
  else if (event_type == EVT_PAUSEKEY) {
    icon_draw_rect_input_text(&rect, color, "Pause", 5.0f, 0.0f);
  }
  else if (event_type == EVT_INSERTKEY) {
    icon_draw_rect_input_text(&rect, color, "Insert", 5.5f, 0.0f);
  }
  else if (event_type == EVT_UNKNOWNKEY) {
    icon_draw_rect_input_text(&rect, color, " ", 12.0f, 0.0f);
  }
  else if (event_type == EVT_GRLESSKEY) {
    icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_GREATER_THAN_OR_LESS_THAN, 16.0f, 0.0f);
  }
  else if (event_type == EVT_MEDIAPLAY) {
    icon_draw_rect_input_text(&rect,
                              color,
                              BLI_STR_UTF8_BLACK_RIGHT_POINTING_TRIANGLE_WITH_DOUBLE_VERTICAL_BAR,
                              10.0f,
                              1.0f);
  }
  else if (event_type == EVT_MEDIASTOP) {
    icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_BLACK_SQUARE_FOR_STOP, 10.0f, 1.0f);
  }
  else if (event_type == EVT_MEDIAFIRST) {
    icon_draw_rect_input_text(&rect,
                              color,
                              BLI_STR_UTF8_BLACK_LEFT_POINTING_DOUBLE_TRIANGLE_WITH_VERTICAL_BAR,
                              11.0f,
                              1.0f);
  }
  else if (event_type == EVT_MEDIALAST) {
    icon_draw_rect_input_text(&rect,
                              color,
                              BLI_STR_UTF8_BLACK_RIGHT_POINTING_DOUBLE_TRIANGLE_WITH_VERTICAL_BAR,
                              10.0f,
                              1.0f);
  }
  else if (event_type == EVT_APPKEY) {
    icon_draw_rect_input_text(&rect, color, "App", 8.0f, 1.0f);
  }
  else if (event_type == EVT_PADPERIOD) {
    icon_draw_rect_input_text(
        &rect, color, BLI_STR_UTF8_SQUARE_WITH_ORTHOGONAL_CROSSHATCH ".", 9.0f, 0.0f);
  }
  else if (event_type == EVT_CAPSLOCKKEY) {
    icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_UPWARDS_UP_ARROW_FROM_BAR, 14.0f, 2.0f);
  }
  else if (event_type == EVT_LINEFEEDKEY) {
    icon_draw_rect_input_text(&rect, color, "LF", 12.0f, 0.0f);
  }
  else if (event_type == EVT_BACKSPACEKEY) {
    const char str[] = BLI_STR_UTF8_ERASE_TO_THE_LEFT;
    icon_draw_rect_input_text(&rect, color, str, 14.0f, 0.0f);
  }
  else if (event_type == EVT_SEMICOLONKEY) {
    icon_draw_rect_input_text(&rect, color, ";", 16.0f, 1.5f);
  }
  else if (event_type == EVT_PERIODKEY) {
    icon_draw_rect_input_text(&rect, color, ".", 18.0f, -2.0f);
  }
  else if (event_type == EVT_COMMAKEY) {
    icon_draw_rect_input_text(&rect, color, ",", 18.0f, 0.0f);
  }
  else if (event_type == EVT_QUOTEKEY) {
    icon_draw_rect_input_text(&rect, color, "'", 18.0f, -6.0f);
  }
  else if (event_type == EVT_ACCENTGRAVEKEY) {
    icon_draw_rect_input_text(&rect, color, "`", 18.0f, -7.0f);
  }
  else if (event_type == EVT_MINUSKEY) {
    icon_draw_rect_input_text(&rect, color, "-", 18.0f, -5.0f);
  }
  else if (event_type == EVT_PLUSKEY) {
    icon_draw_rect_input_text(&rect, color, "+", 18.0f, -1.0f);
  }
  else if (event_type == EVT_SLASHKEY) {
    icon_draw_rect_input_text(&rect, color, "/", 13.0f, 1.0f);
  }
  else if (event_type == EVT_BACKSLASHKEY) {
    icon_draw_rect_input_text(&rect, color, "\\", 13.0f, 1.0f);
  }
  else if (event_type == EVT_EQUALKEY) {
    icon_draw_rect_input_text(&rect, color, "=", 18.0f, -2.5f);
  }
  else if (event_type == EVT_LEFTBRACKETKEY) {
    icon_draw_rect_input_text(&rect, color, "[", 12.0f, 1.5f);
  }
  else if (event_type == EVT_RIGHTBRACKETKEY) {
    icon_draw_rect_input_text(&rect, color, "]", 12.0f, 1.5f);
  }
  else if ((event_type >= NDOF_BUTTON_MENU) && (event_type <= NDOF_BUTTON_C)) {
    if ((event_type >= NDOF_BUTTON_V1) && (event_type <= NDOF_BUTTON_V3)) {
      char str[7];
      SNPRINTF(str, "%sv%i", BLI_STR_UTF8_CIRCLED_WHITE_BULLET, 1 + event_type - NDOF_BUTTON_V1);
      icon_draw_rect_input_text(&rect, color, str, 7.5f, 0.0f);
    }
    else if ((event_type >= NDOF_BUTTON_1) && (event_type <= NDOF_BUTTON_9)) {
      char str[6];
      SNPRINTF(str, "%s%i", BLI_STR_UTF8_CIRCLED_WHITE_BULLET, 1 + event_type - NDOF_BUTTON_1);
      icon_draw_rect_input_text(&rect, color, str, 9.0f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_10) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "10", 7.5f, 0.0f);
    }
    else if ((event_type >= NDOF_BUTTON_A) && (event_type <= NDOF_BUTTON_C)) {
      char str[6];
      SNPRINTF(str, "%s%c", BLI_STR_UTF8_CIRCLED_WHITE_BULLET, 'A' + event_type - NDOF_BUTTON_A);
      icon_draw_rect_input_text(&rect, color, str, 9.0f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_MENU) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Me", 6.5f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_FIT) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Ft", 7.5f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_TOP) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Tp", 7.0f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_BOTTOM) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Bt", 7.5f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_LEFT) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Le", 7.0f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_RIGHT) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Ri", 7.5f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_FRONT) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Fr", 7.5f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_BACK) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Bk", 7.0f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_ISO1) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "I1", 7.5f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_ISO2) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "I2", 7.5f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_ROLL_CW) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Rl", 7.5f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_ROLL_CCW) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Rc", 7.0f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_SPIN_CW) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Sp", 7.0f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_SPIN_CCW) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Sc", 7.0f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_TILT_CW) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Ti", 8.0f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_TILT_CCW) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Tc", 7.0f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_ROTATE) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Ro", 7.0f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_PANZOOM) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "PZ", 7.0f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_DOMINANT) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "Dm", 6.5f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_PLUS) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "+", 10.0f, 0.0f);
    }
    else if (event_type == NDOF_BUTTON_MINUS) {
      icon_draw_rect_input_text(&rect, color, BLI_STR_UTF8_CIRCLED_WHITE_BULLET "-", 10.0f, 0.0f);
    }
  }
}
