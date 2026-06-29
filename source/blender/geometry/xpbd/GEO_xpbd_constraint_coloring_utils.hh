/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_index_mask.hh"
#include "BLI_math_vector_types.hh"

#include "GEO_xpbd_constraint_coloring.hh"

namespace blender::xpbd {

ConstraintColoring color_constraints__unary(const Span<int> affected_points,
                                            LinearAllocator<> &memory);

ConstraintColoring color_constraints__binary(const Span<int2> affected_points,
                                             LinearAllocator<> &memory);

ConstraintColoring color_constraints__n_ary(const GroupedSpan<int> affected_points,
                                            LinearAllocator<> &memory);

ConstraintColoring color_constraints__all_independent(const int constraints_num);

}  // namespace blender::xpbd
