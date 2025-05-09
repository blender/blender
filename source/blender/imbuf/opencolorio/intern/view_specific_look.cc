/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "view_specific_look.hh"

namespace blender::ocio {

bool split_view_specific_look(const StringRef look_name, StringRef &view, StringRef &ui_name)
{
  const int64_t separator_offset = look_name.find(" - ");
  if (separator_offset == -1) {
    view = {};
    ui_name = look_name;
    return false;
  }

  view = look_name.substr(0, separator_offset);
  ui_name = look_name.substr(separator_offset + 3);

  return true;
}

}  // namespace blender::ocio
