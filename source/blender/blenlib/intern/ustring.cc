/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_ustring.hh"

namespace blender {

UString UString::from_ptr_noinline(const char *str)
{
  return UString(str);
}

}  // namespace blender
