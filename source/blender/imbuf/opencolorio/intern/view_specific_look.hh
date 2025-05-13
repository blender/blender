/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

namespace blender::ocio {

/**
 * Split the view-specific look name to a view name and look name for the interface.
 *
 * Look is considered to be view-specific when it contains dash in its name. In this case the part
 * of the look name is considered to be the name of the view the look is specific to.
 *
 * If the look is not view-specific view is an empty string and ui name is the look name.
 *
 * Returns true if the look name is view-specific.
 */
bool split_view_specific_look(StringRef look_name, StringRef &view, StringRef &ui_name);

}  // namespace blender::ocio
