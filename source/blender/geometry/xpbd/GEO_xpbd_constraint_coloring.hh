/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_index_mask.hh"
namespace blender::xpbd {

struct ConstraintColoring {
  /** Indices within the same #IndexMask are independent and can be evaluated in parallel. */
  Vector<IndexMask> colors;
};

}  // namespace blender::xpbd
