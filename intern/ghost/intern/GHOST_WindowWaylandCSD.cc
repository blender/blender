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
#include <optional>
#include <sstream> /* For `std::stringstream`. */
#include <string>

/* Logging, use `ghost.wl.*` prefix. */
#include "CLG_log.h"

/* -------------------------------------------------------------------- */
/** \name Private CSD Integration
 * \{ */

static CLG_LogRef LOG_WL_CSD = {"ghost.wl.csd"};
#define LOG (&LOG_WL_CSD)

static std::optional<std::string> command_exec(const char *cmd, const size_t output_limit)
{
  std::array<char, 128> buffer;
  std::stringstream result;
  FILE *pipe = popen(cmd, "r");
  if (!pipe) {
    CLOG_DEBUG(LOG, "failed to open: %s", cmd);
    return std::nullopt;
  }

  bool error = false;
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    if (error) {
      /* Read the remaining data and exit, sophisticated process handling
       * could kill the process but that's not possible with `popen`. */
      continue;
    }
    result << buffer.data();
    if (result.tellp() > output_limit) {
      error = true;
      CLOG_DEBUG(LOG, "over-sized output (%zu)", output_limit);
    }
  }
  const int exit_code = pclose(pipe);
  if (exit_code != 0) {
    error = true;
  }
  if (error) {
    return std::nullopt;
  }
  return result.str();
}

static const char *strchr_or_end(const char *str, const char ch)
{
  const char *p = str;
  while (!ELEM(*p, ch, '\0')) {
    p++;
  }
  return p;
}

static bool string_elem_split_by_delim(const char *haystack, const char delim, const char *needle)
{
  /* Local copy of #BLI_string_elem_split_by_delim (would be a bad level call). */

  /* May be zero, returns true when an empty span exists. */
  const size_t needle_len = strlen(needle);
  const char *p = haystack, *p_next;
  while (true) {
    p_next = strchr_or_end(p, delim);
    if ((size_t(p_next - p) == needle_len) && (memcmp(p, needle, needle_len) == 0)) {
      return true;
    }
    if (*p_next == '\0') {
      break;
    }
    p = p_next + 1;
  }
  return false;
}

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

static bool ghost_window_csd_layout_from_gnome(GHOST_CSD_Layout &layout)
{
  /* NOTE(@ideasman42): this could/should use DBUS, although previously
   * DBUS would hang for 5+ seconds when not available. */

  /* Extract a string such as: `'"icon:minimize,maximize,close"'\n`
   * and convert it into an array in #GHOST_CSD_Layout::buttons
   * to follow the systems button layout. */
  std::optional<std::string> output = command_exec(
      "gsettings get org.gnome.desktop.wm.preferences button-layout 2>&1", 512);
  if (!output.has_value()) {
    return false;
  }

  std::string_view output_trim = *output;
  while (output_trim.length() > 0) {
    const char c = output_trim.back();
    if (c != '\n') {
      break;
    }
    output_trim.remove_suffix(1);
  }
  /* Check for surrounding single quotes. */
  if (!((output_trim.length() >= 2) && (output_trim.front() == '\'') &&
        (output_trim.back() == '\'')))
  {
    return false;
  }
  output_trim.remove_prefix(1);
  output_trim.remove_suffix(1);

  /* Access buttons from both sides of the `:` which represents the title bar. */
  std::array<std::string_view, 2> output_pair = string_partition(output_trim, ':');
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
        output_pair[side], &button_mask, &layout.buttons[i], buttons_capacity);
  }
  layout.buttons_num = i;
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public CSD API
 * \{ */

bool GHOST_WindowCSD_LayoutFromSystem(GHOST_CSD_Layout &layout)
{
  /* In the future CSD may be used for KDE and others,
   * currently only GNOME support is needed as CSD is only used with GNOME. */
  bool result = ghost_window_csd_layout_from_gnome(layout);

  return result;
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

bool GHOST_WindowCSD_Check()
{
  bool result = false;
  const char *xdg_current_desktop = [] {
    /* Account for VSCode overriding this value (TSK!), see: #133921. */
    const char *key = "ORIGINAL_XDG_CURRENT_DESKTOP";
    const char *value = getenv(key);
    return value ? value : getenv(key + 9);
  }();

  if (xdg_current_desktop) {
    /* See the free-desktop specifications for details on `XDG_CURRENT_DESKTOP`.
     * https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html
     */
    if (string_elem_split_by_delim(xdg_current_desktop, ':', "GNOME")) {
      result = true;
    }
  }
  return result;
}

/** \} */
