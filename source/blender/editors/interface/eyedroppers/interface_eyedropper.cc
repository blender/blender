/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_math_color.h"
#include "BLI_math_vector.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "interface_intern.hh"

#include "eyedropper_intern.hh" /* own include */

/* -------------------------------------------------------------------- */
/* Keymap
 */
/** \name Modal Keymap
 * \{ */

wmKeyMap *eyedropper_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {EYE_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {EYE_MODAL_SAMPLE_CONFIRM, "SAMPLE_CONFIRM", 0, "Confirm Sampling", ""},
      {EYE_MODAL_SAMPLE_BEGIN, "SAMPLE_BEGIN", 0, "Start Sampling", ""},
      {EYE_MODAL_SAMPLE_RESET, "SAMPLE_RESET", 0, "Reset Sampling", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Eyedropper Modal Map");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return nullptr;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Eyedropper Modal Map", modal_items);

  /* assign to operators */
  WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_colorramp");
  WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_color");
  WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_id");
  WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_bone");
  WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_depth");
  WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_driver");
  WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_grease_pencil_color");

  return keymap;
}

wmKeyMap *eyedropper_colorband_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items_point[] = {
      {EYE_MODAL_POINT_CANCEL, "CANCEL", 0, "Cancel", ""},
      {EYE_MODAL_POINT_SAMPLE, "SAMPLE_SAMPLE", 0, "Sample a Point", ""},
      {EYE_MODAL_POINT_CONFIRM, "SAMPLE_CONFIRM", 0, "Confirm Sampling", ""},
      {EYE_MODAL_POINT_RESET, "SAMPLE_RESET", 0, "Reset Sampling", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Eyedropper ColorRamp PointSampling Map");
  if (keymap && keymap->modal_items) {
    return keymap;
  }

  keymap = WM_modalkeymap_ensure(
      keyconf, "Eyedropper ColorRamp PointSampling Map", modal_items_point);

  /* assign to operators */
  WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_colorramp_point");

  return keymap;
}

/** \} */

/* -------------------------------------------------------------------- */
/* Utility Functions
 */

/** \name Generic Shared Functions
 * \{ */

void eyedropper_draw_cursor_text_region(const int xy[2], const char *name)
{
  if (name[0] == '\0') {
    return;
  }

  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;

  /* Use the theme settings from tooltips. */
  const bTheme *btheme = UI_GetTheme();
  const uiWidgetColors *wcol = &btheme->tui.wcol_tooltip;

  float col_fg[4], col_bg[4];
  rgba_uchar_to_float(col_fg, wcol->text);
  rgba_uchar_to_float(col_bg, wcol->inner);

  UI_fontstyle_draw_simple_backdrop(fstyle, xy[0], xy[1] + U.widget_unit, name, col_fg, col_bg);
}

uiBut *eyedropper_get_property_button_under_mouse(bContext *C, const wmEvent *event)
{
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, event->xy);
  const ARegion *region = BKE_area_find_region_xy(area, RGN_TYPE_ANY, event->xy);

  uiBut *but = UI_but_find_mouse_over(region, event);

  if (ELEM(nullptr, but, but->rnapoin.data, but->rnaprop)) {
    return nullptr;
  }
  return but;
}

void eyedropper_win_area_find(const bContext *C,
                              const int event_xy[2],
                              int r_event_xy[2],
                              wmWindow **r_win,
                              ScrArea **r_area)
{
  bScreen *screen = CTX_wm_screen(C);

  *r_win = CTX_wm_window(C);
  *r_area = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, event_xy);
  if (*r_area == nullptr) {
    *r_win = WM_window_find_under_cursor(*r_win, event_xy, r_event_xy);
    if (*r_win) {
      screen = WM_window_get_active_screen(*r_win);
      *r_area = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, r_event_xy);
    }
  }
  else if (event_xy != r_event_xy) {
    copy_v2_v2_int(r_event_xy, event_xy);
  }
}

/** \} */
