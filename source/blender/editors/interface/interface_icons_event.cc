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

#include "BLI_rect.h"
#include "BLI_string.h"

#include "BLF_api.hh"

#include "BLT_translation.hh"

#include "UI_interface.hh"

#include "interface_intern.hh"

static int inverted_icon(int icon_id)
{
  switch (icon_id) {
    case ICON_KEY_BACKSPACE:
      return ICON_KEY_BACKSPACE_FILLED;
    case ICON_KEY_COMMAND:
      return ICON_KEY_COMMAND_FILLED;
    case ICON_KEY_CONTROL:
      return ICON_KEY_CONTROL_FILLED;
    case ICON_KEY_EMPTY1:
      return ICON_KEY_EMPTY1_FILLED;
    case ICON_KEY_EMPTY2:
      return ICON_KEY_EMPTY2_FILLED;
    case ICON_KEY_EMPTY3:
      return ICON_KEY_EMPTY3_FILLED;
    case ICON_KEY_MENU:
      return ICON_KEY_MENU_FILLED;
    case ICON_KEY_OPTION:
      return ICON_KEY_OPTION_FILLED;
    case ICON_KEY_RETURN:
      return ICON_KEY_RETURN_FILLED;
    case ICON_KEY_RING:
      return ICON_KEY_RING_FILLED;
    case ICON_KEY_SHIFT:
      return ICON_KEY_SHIFT_FILLED;
    case ICON_KEY_TAB:
      return ICON_KEY_TAB_FILLED;
    case ICON_KEY_WINDOWS:
      return ICON_KEY_WINDOWS_FILLED;
    default:
      return icon_id;
  }
}

static void icon_draw_icon(const rctf *rect,
                           const int icon_id,
                           const float aspect,
                           const float alpha,
                           const bool inverted)
{
  float color[4];
  UI_GetThemeColor4fv(TH_TEXT, color);
  if (alpha < 1.0f) {
    color[3] *= alpha;
  }

  BLF_draw_svg_icon(uint(inverted ? inverted_icon(icon_id) : icon_id),
                    rect->xmin,
                    rect->ymin,
                    float(ICON_DEFAULT_HEIGHT) / aspect,
                    color,
                    0.0f);
}

static void icon_draw_rect_input_text(const rctf *rect,
                                      const char *str,
                                      const float aspect,
                                      const float alpha,
                                      const bool inverted,
                                      const int icon_bg = ICON_KEY_EMPTY1)
{
  icon_draw_icon(rect, icon_bg, aspect, alpha, inverted);

  const float available_width = BLI_rctf_size_x(rect) - (2.0f * UI_SCALE_FAC);
  const int font_id = BLF_default();
  float color[4];
  UI_GetThemeColor4fv(inverted ? TH_BACK : TH_TEXT, color);
  if (alpha < 1.0f) {
    color[3] *= alpha;
  }
  BLF_color4fv(font_id, color);

  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
  float font_size = std::min(15.0f, fstyle->points) * UI_SCALE_FAC;
  BLF_size(font_id, font_size);

  rcti str_bounds;
  BLF_boundbox(font_id, str, BLF_DRAW_STR_DUMMY_MAX, &str_bounds);
  float width = float(BLI_rcti_size_x(&str_bounds));
  float height = float(BLI_rcti_size_y(&str_bounds));
  if (width > (available_width - (2.0f * UI_SCALE_FAC))) {
    font_size *= (available_width - (2.0f * UI_SCALE_FAC)) / width;
    BLF_size(font_id, font_size);
    BLF_boundbox(font_id, str, BLF_DRAW_STR_DUMMY_MAX, &str_bounds);
    width = float(BLI_rcti_size_x(&str_bounds));
    height = float(BLI_rcti_size_y(&str_bounds));
  }

  const float x = rect->xmin + UI_SCALE_FAC + ((available_width - width) / 2.0f);
  const float v_offset = (BLI_rctf_size_y(rect) - height) * 0.5f - str_bounds.ymin;
  BLF_position(font_id, x, rect->ymin + v_offset, 0.0f);
  BLF_draw(font_id, str, BLF_DRAW_STR_DUMMY_MAX);
}

float ui_event_icon_offset(const int icon_id)
{
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

  if (ELEM(icon_id,
           ICON_EVENT_ESC,
           ICON_EVENT_DEL,
           ICON_EVENT_HOME,
           ICON_EVENT_END,
           ICON_EVENT_BACKSPACE,
           ICON_EVENT_PAUSE,
           ICON_EVENT_INSERT,
           ICON_EVENT_APP))
  {
    return 1.5f;
  }
  if (icon_id >= ICON_EVENT_PAD0 && icon_id <= ICON_EVENT_PADPERIOD) {
    return 1.5f;
  }
  if (icon_id >= ICON_EVENT_F10 && icon_id <= ICON_EVENT_F24) {
    return 1.5f;
  }
  if (platform != MACOS && ELEM(icon_id, ICON_EVENT_CTRL, ICON_EVENT_ALT, ICON_EVENT_OS)) {
    return 1.5f;
  }
  if (icon_id == ICON_EVENT_OS && platform != MACOS && platform != MSWIN) {
    return 1.5f;
  }
  if (icon_id == ICON_EVENT_SPACEKEY) {
    return 3.0f;
  }
  return 0.0f;
}

void icon_draw_rect_input(const float x,
                          const float y,
                          const int w,
                          const int h,
                          const int icon_id,
                          const float aspect,
                          const float alpha,
                          const bool inverted)
{
  rctf rect{};
  rect.xmin = int(x);
  rect.xmax = int(x + w);
  rect.ymin = int(y);
  rect.ymax = int(y + h);

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

  const float offset = ui_event_icon_offset(icon_id);
  if (offset >= 3.0f) {
    rect.xmax = rect.xmin + BLI_rctf_size_x(&rect) * 2.0f;
  }
  else if (offset >= 1.5f) {
    rect.xmax = rect.xmin + BLI_rctf_size_x(&rect) * 1.5f;
  }

  if ((icon_id >= ICON_EVENT_A) && (icon_id <= ICON_EVENT_Z)) {
    const char str[2] = {char('A' + (icon_id - ICON_EVENT_A)), '\0'};
    icon_draw_rect_input_text(&rect, str, aspect, alpha, inverted);
  }
  else if ((icon_id >= ICON_EVENT_ZEROKEY) && (icon_id <= ICON_EVENT_NINEKEY)) {
    const char str[2] = {char('0' + (icon_id - ICON_EVENT_ZEROKEY)), '\0'};
    icon_draw_rect_input_text(&rect, str, aspect, alpha, inverted);
  }
  else if ((icon_id >= ICON_EVENT_F1) && (icon_id <= ICON_EVENT_F24)) {
    char str[4];
    SNPRINTF(str, "F%d", 1 + (icon_id - ICON_EVENT_F1));
    icon_draw_rect_input_text(&rect,
                              str,
                              aspect,
                              alpha,
                              inverted,
                              (icon_id >= ICON_EVENT_F10) ? ICON_KEY_EMPTY2 : ICON_KEY_EMPTY1);
  }
  if (icon_id == ICON_EVENT_SHIFT) {
    icon_draw_icon(&rect, ICON_KEY_SHIFT, aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_CTRL) {
    if (platform == MACOS) {
      icon_draw_icon(&rect, ICON_KEY_CONTROL, aspect, alpha, inverted);
    }
    else {
      icon_draw_rect_input_text(&rect, IFACE_("Ctrl"), aspect, alpha, inverted, ICON_KEY_EMPTY2);
    }
  }
  else if (icon_id == ICON_EVENT_ALT) {
    if (platform == MACOS) {
      icon_draw_icon(&rect, ICON_KEY_OPTION, aspect, alpha, inverted);
    }
    else {
      icon_draw_rect_input_text(&rect, IFACE_("Alt"), aspect, alpha, inverted, ICON_KEY_EMPTY2);
    }
  }
  else if (icon_id == ICON_EVENT_OS) {
    if (platform == MACOS) {
      icon_draw_icon(&rect, ICON_KEY_COMMAND, aspect, alpha, inverted);
    }
    else if (platform == MSWIN) {
      icon_draw_icon(&rect, ICON_KEY_WINDOWS, aspect, alpha, inverted);
    }
    else {
      icon_draw_rect_input_text(&rect, IFACE_("OS"), aspect, alpha, inverted, ICON_KEY_EMPTY2);
    }
  }
  else if (icon_id == ICON_EVENT_DEL) {
    icon_draw_rect_input_text(&rect, IFACE_("Del"), aspect, alpha, inverted, ICON_KEY_EMPTY2);
  }
  else if (icon_id == ICON_EVENT_TAB) {
    icon_draw_icon(&rect, ICON_KEY_TAB, aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_HOME) {
    icon_draw_rect_input_text(&rect, IFACE_("Home"), aspect, alpha, inverted, ICON_KEY_EMPTY2);
  }
  else if (icon_id == ICON_EVENT_END) {
    icon_draw_rect_input_text(&rect, IFACE_("End"), aspect, alpha, inverted, ICON_KEY_EMPTY2);
  }
  else if (icon_id == ICON_EVENT_RETURN) {
    icon_draw_icon(&rect, ICON_KEY_RETURN, aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_ESC) {
    icon_draw_rect_input_text(&rect, IFACE_("Esc"), aspect, alpha, inverted, ICON_KEY_EMPTY2);
  }
  else if (icon_id == ICON_EVENT_PAGEUP) {
    icon_draw_rect_input_text(&rect, "P" BLI_STR_UTF8_UPWARDS_ARROW, aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_PAGEDOWN) {
    icon_draw_rect_input_text(&rect, "P" BLI_STR_UTF8_DOWNWARDS_ARROW, aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_LEFT_ARROW) {
    icon_draw_rect_input_text(&rect, BLI_STR_UTF8_LEFTWARDS_ARROW, aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_UP_ARROW) {
    icon_draw_rect_input_text(&rect, BLI_STR_UTF8_UPWARDS_ARROW, aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_RIGHT_ARROW) {
    icon_draw_rect_input_text(&rect, BLI_STR_UTF8_RIGHTWARDS_ARROW, aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_DOWN_ARROW) {
    icon_draw_rect_input_text(&rect, BLI_STR_UTF8_DOWNWARDS_ARROW, aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_SPACEKEY) {
    icon_draw_rect_input_text(&rect, IFACE_("Space"), aspect, alpha, inverted, ICON_KEY_EMPTY3);
  }
  else if (icon_id == ICON_EVENT_MOUSE_4) {
    icon_draw_rect_input_text(
        &rect, BLI_STR_UTF8_BLACK_VERTICAL_ELLIPSE "4", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_MOUSE_5) {
    icon_draw_rect_input_text(
        &rect, BLI_STR_UTF8_BLACK_VERTICAL_ELLIPSE "5", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_MOUSE_6) {
    icon_draw_rect_input_text(
        &rect, BLI_STR_UTF8_BLACK_VERTICAL_ELLIPSE "6", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_MOUSE_7) {
    icon_draw_rect_input_text(
        &rect, BLI_STR_UTF8_BLACK_VERTICAL_ELLIPSE "7", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_TABLET_STYLUS) {
    icon_draw_rect_input_text(&rect, BLI_STR_UTF8_LOWER_RIGHT_PENCIL, aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_TABLET_ERASER) {
    icon_draw_rect_input_text(&rect, BLI_STR_UTF8_UPPER_RIGHT_PENCIL, aspect, alpha, inverted);
  }
  else if ((icon_id >= ICON_EVENT_PAD0) && (icon_id <= ICON_EVENT_PAD9)) {
    char str[5];
    SNPRINTF(
        str, "%s%i", BLI_STR_UTF8_SQUARE_WITH_ORTHOGONAL_CROSSHATCH, icon_id - ICON_EVENT_PAD0);
    icon_draw_rect_input_text(&rect, str, aspect, alpha, inverted, ICON_KEY_EMPTY2);
  }
  else if (icon_id == ICON_EVENT_PADASTER) {
    icon_draw_rect_input_text(&rect,
                              BLI_STR_UTF8_SQUARE_WITH_ORTHOGONAL_CROSSHATCH "6",
                              aspect,
                              alpha,
                              inverted,
                              ICON_KEY_EMPTY2);
  }
  else if (icon_id == ICON_EVENT_PADSLASH) {
    icon_draw_rect_input_text(&rect,
                              BLI_STR_UTF8_SQUARE_WITH_ORTHOGONAL_CROSSHATCH "/",
                              aspect,
                              alpha,
                              inverted,
                              ICON_KEY_EMPTY2);
  }
  else if (icon_id == ICON_EVENT_PADMINUS) {
    icon_draw_rect_input_text(&rect,
                              BLI_STR_UTF8_SQUARE_WITH_ORTHOGONAL_CROSSHATCH "-",
                              aspect,
                              alpha,
                              inverted,
                              ICON_KEY_EMPTY2);
  }
  else if (icon_id == ICON_EVENT_PADENTER) {
    icon_draw_rect_input_text(
        &rect,
        BLI_STR_UTF8_SQUARE_WITH_ORTHOGONAL_CROSSHATCH BLI_STR_UTF8_RETURN_SYMBOL,
        aspect,
        alpha,
        inverted,
        ICON_KEY_EMPTY2);
  }
  else if (icon_id == ICON_EVENT_PADPLUS) {
    icon_draw_rect_input_text(&rect,
                              BLI_STR_UTF8_SQUARE_WITH_ORTHOGONAL_CROSSHATCH "+",
                              aspect,
                              alpha,
                              inverted,
                              ICON_KEY_EMPTY2);
  }
  else if (icon_id == ICON_EVENT_PADPERIOD) {
    icon_draw_rect_input_text(&rect,
                              BLI_STR_UTF8_SQUARE_WITH_ORTHOGONAL_CROSSHATCH ".",
                              aspect,
                              alpha,
                              inverted,
                              ICON_KEY_EMPTY2);
  }
  else if (icon_id == ICON_EVENT_PAUSE) {
    icon_draw_rect_input_text(&rect, IFACE_("Pause"), aspect, alpha, inverted, ICON_KEY_EMPTY2);
  }
  else if (icon_id == ICON_EVENT_INSERT) {
    icon_draw_rect_input_text(&rect, IFACE_("Insert"), aspect, alpha, inverted, ICON_KEY_EMPTY2);
  }
  else if (icon_id == ICON_EVENT_UNKNOWN) {
    icon_draw_rect_input_text(&rect, " ", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_GRLESS) {
    icon_draw_rect_input_text(
        &rect, BLI_STR_UTF8_GREATER_THAN_OR_LESS_THAN, aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_MEDIAPLAY) {
    icon_draw_rect_input_text(&rect,
                              BLI_STR_UTF8_BLACK_RIGHT_POINTING_TRIANGLE_WITH_DOUBLE_VERTICAL_BAR,
                              aspect,
                              alpha,
                              inverted);
  }
  else if (icon_id == ICON_EVENT_MEDIASTOP) {
    icon_draw_rect_input_text(&rect, BLI_STR_UTF8_BLACK_SQUARE_FOR_STOP, aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_MEDIAFIRST) {
    icon_draw_rect_input_text(&rect,
                              BLI_STR_UTF8_BLACK_LEFT_POINTING_DOUBLE_TRIANGLE_WITH_VERTICAL_BAR,
                              aspect,
                              alpha,
                              inverted);
  }
  else if (icon_id == ICON_EVENT_MEDIALAST) {
    icon_draw_rect_input_text(&rect,
                              BLI_STR_UTF8_BLACK_RIGHT_POINTING_DOUBLE_TRIANGLE_WITH_VERTICAL_BAR,
                              aspect,
                              alpha,
                              inverted);
  }
  else if (icon_id == ICON_EVENT_APP) {
    icon_draw_rect_input_text(&rect, IFACE_("App"), aspect, alpha, inverted, ICON_KEY_EMPTY2);
  }
  else if (icon_id == ICON_EVENT_CAPSLOCK) {
    icon_draw_rect_input_text(
        &rect, BLI_STR_UTF8_UPWARDS_UP_ARROW_FROM_BAR, aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_BACKSPACE) {
    icon_draw_icon(&rect, ICON_KEY_BACKSPACE, aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_SEMICOLON) {
    icon_draw_rect_input_text(&rect, ";", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_PERIOD) {
    icon_draw_rect_input_text(&rect, ".", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_COMMA) {
    icon_draw_rect_input_text(&rect, ",", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_QUOTE) {
    icon_draw_rect_input_text(&rect, "'", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_ACCENTGRAVE) {
    icon_draw_rect_input_text(&rect, "`", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_MINUS) {
    icon_draw_rect_input_text(&rect, "-", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_PLUS) {
    icon_draw_rect_input_text(&rect, "+", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_SLASH) {
    icon_draw_rect_input_text(&rect, "/", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_BACKSLASH) {
    icon_draw_rect_input_text(&rect, "\\", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_EQUAL) {
    icon_draw_rect_input_text(&rect, "=", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_LEFTBRACKET) {
    icon_draw_rect_input_text(&rect, "[", aspect, alpha, inverted);
  }
  else if (icon_id == ICON_EVENT_RIGHTBRACKET) {
    icon_draw_rect_input_text(&rect, "]", aspect, alpha, inverted);
  }
  else if (icon_id >= ICON_EVENT_NDOF_BUTTON_V1 && icon_id <= ICON_EVENT_NDOF_BUTTON_MINUS) {
    if (/* `(icon_id >= ICON_EVENT_NDOF_BUTTON_V1) &&` */ (icon_id <= ICON_EVENT_NDOF_BUTTON_V3)) {
      char str[7];
      SNPRINTF(str, "v%i", (icon_id + 1) - ICON_EVENT_NDOF_BUTTON_V1);
      icon_draw_rect_input_text(&rect, str, aspect, alpha, inverted, ICON_KEY_RING);
    }
    if ((icon_id >= ICON_EVENT_NDOF_BUTTON_SAVE_V1) && (icon_id <= ICON_EVENT_NDOF_BUTTON_SAVE_V3))
    {
      char str[7];
      SNPRINTF(str, "s%i", (icon_id + 1) - ICON_EVENT_NDOF_BUTTON_SAVE_V1);
      icon_draw_rect_input_text(&rect, str, aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if ((icon_id >= ICON_EVENT_NDOF_BUTTON_1) && (icon_id <= ICON_EVENT_NDOF_BUTTON_12)) {
      char str[7];
      SNPRINTF(str, "%i", (1 + icon_id) - ICON_EVENT_NDOF_BUTTON_1);
      icon_draw_rect_input_text(&rect, str, aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_MENU) {
      icon_draw_rect_input_text(&rect, "Me", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_FIT) {
      icon_draw_rect_input_text(&rect, "Ft", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_TOP) {
      icon_draw_rect_input_text(&rect, "Tp", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_BOTTOM) {
      icon_draw_rect_input_text(&rect, "Bt", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_LEFT) {
      icon_draw_rect_input_text(&rect, "Le", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_RIGHT) {
      icon_draw_rect_input_text(&rect, "Ri", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_FRONT) {
      icon_draw_rect_input_text(&rect, "Fr", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_BACK) {
      icon_draw_rect_input_text(&rect, "Bk", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_ISO1) {
      icon_draw_rect_input_text(&rect, "I1", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_ISO2) {
      icon_draw_rect_input_text(&rect, "I2", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_ROLL_CW) {
      icon_draw_rect_input_text(&rect, "Rl", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_ROLL_CCW) {
      icon_draw_rect_input_text(&rect, "Rc", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_SPIN_CW) {
      icon_draw_rect_input_text(&rect, "Sp", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_SPIN_CCW) {
      icon_draw_rect_input_text(&rect, "Sc", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_TILT_CW) {
      icon_draw_rect_input_text(&rect, "Ti", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_TILT_CCW) {
      icon_draw_rect_input_text(&rect, "Tc", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_ROTATE) {
      icon_draw_rect_input_text(&rect, "Ro", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_PANZOOM) {
      icon_draw_rect_input_text(&rect, "PZ", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_DOMINANT) {
      icon_draw_rect_input_text(&rect, "Dm", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_PLUS) {
      icon_draw_rect_input_text(&rect, "+", aspect, alpha, inverted, ICON_KEY_RING);
    }
    else if (icon_id == ICON_EVENT_NDOF_BUTTON_MINUS) {
      icon_draw_rect_input_text(&rect, "-", aspect, alpha, inverted, ICON_KEY_RING);
    }
  }
}
