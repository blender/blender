/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "description.hh"

#include "BLI_utildefines.h"

namespace blender::ocio {

std::string cleanup_description(const StringRef description)
{
  if (description.is_empty()) {
    return "";
  }

  std::string result = description.trim("\r\n");

  for (char &ch : result) {
    if (ELEM(ch, '\r', '\n')) {
      ch = ' ';
    }
  }

  return result;
}

}  // namespace blender::ocio
