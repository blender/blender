/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_string_ref.hh"

namespace blender {

struct ID;
struct Main;

/** Handle complex user-facing ID renaming behavior, including user feedback (reporting). */
bool ED_id_rename(Main &bmain, ID &id, StringRefNull name);

}  // namespace blender
