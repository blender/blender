/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

/* Currently all of the logic is for CSD. */

#include "GHOST_WindowWaylandCSD.hh" /* Own include. */
#include "GHOST_Types.hh"
#include "GHOST_utildefines.hh"

#include <array> /* For `std::array`. */
#include <string_view>

/* -------------------------------------------------------------------- */
/** \name Private CSD Integration
 * \{ */

static std::array<std::string_view, 2> string_partition(std::string_view s, const char delimiter)
{
  std::array<std::string_view, 2> result;
  size_t pos = s.find(delimiter);
  if (pos == std::string_view::npos) {
    /* Follow Firefox in defaulting to the right when there is no delimiter. */
    result[0] = {};
    result[1] = s;
  }
  else {
    result[0] = s.substr(0, pos);
    result[1] = s.substr(pos + 1);
  }
  return result;
}

static int string_parse_buttons(std::string_view buttons,
                                uint32_t *button_mask_p,
                                GHOST_TCSD_Type *output,
                                const int output_capacity)
{
  const char delimiter = ',';
  int i = 0;
  while (!buttons.empty() && (i < output_capacity)) {
    const size_t p = buttons.find(delimiter);
    const std::string_view button_id = buttons.substr(0, p);

    GHOST_TCSD_Type value = GHOST_kCSDTypeBody;
    if (button_id == "close") {
      value = GHOST_kCSDTypeButtonClose;
    }
    else if (button_id == "maximize") {
      value = GHOST_kCSDTypeButtonMaximize;
    }
    else if (button_id == "minimize") {
      value = GHOST_kCSDTypeButtonMinimize;
    }
    else if (button_id == "icon") {
      value = GHOST_kCSDTypeButtonMenu;
    }

    if (value != GHOST_kCSDTypeBody) {
      /* Only allow each button once. */
      const uint32_t value_mask = (1 << uint32_t(value));
      if ((*button_mask_p & value_mask) == 0) {
        *button_mask_p |= value_mask;
        output[i++] = value;
      }
    }

    if (p == std::string_view::npos) {
      break;
    }
    buttons.remove_prefix(p + 1);
  }
  return i;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public CSD API
 * \{ */

bool GHOST_WindowCSD_LayoutFromString(GHOST_CSD_Layout &layout, const std::string_view buttons)
{
  /* Access buttons from both sides of the `:` which represents the title bar. */
  std::array<std::string_view, 2> buttons_pair = string_partition(buttons, ':');
  int i = 0;
  uint32_t button_mask = 0;
  for (int side = 0; side < 2; side++) {
    int buttons_capacity = ARRAY_SIZE(layout.buttons) - i;
    if (side == 1) {
      /* Add the title divider. */
      if (buttons_capacity > 0) {
        layout.buttons[i++] = GHOST_kCSDTypeTitlebar;
        buttons_capacity--;
      }
    }
    i += string_parse_buttons(
        buttons_pair[side], &button_mask, &layout.buttons[i], buttons_capacity);
  }
  layout.buttons_num = i;
  return button_mask != 0;
}

void GHOST_WindowCSD_LayoutDefault(GHOST_CSD_Layout &layout)
{
  int i = 0;
  layout.buttons[i++] = GHOST_kCSDTypeButtonMenu;
  layout.buttons[i++] = GHOST_kCSDTypeTitlebar;
  layout.buttons[i++] = GHOST_kCSDTypeButtonMinimize;
  layout.buttons[i++] = GHOST_kCSDTypeButtonMaximize;
  layout.buttons[i++] = GHOST_kCSDTypeButtonClose;
  layout.buttons_num = i;
}

bool GHOST_WindowCSD_Check(GWL_CurrentDesktopType desktop_type)
{
  if (desktop_type == GWL_CurrentDesktopType::Gnome) {
    return true;
  }
  return false;
}

/** \} */
