/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_cpp_type_make.hh"

#include "COM_closure.hh"
#include "COM_init.hh"

namespace blender::compositor {

static void register_cpp_types()
{
  BLI_CPP_TYPE_REGISTER(ClosurePtr, CPPTypeFlags::None);
}

void init()
{
  static bool initialized = false;
  if (initialized) {
    return;
  }
  initialized = true;

  register_cpp_types();
}

}  // namespace blender::compositor
