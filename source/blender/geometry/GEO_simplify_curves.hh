/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_curves.hh"

namespace blender::geometry {

/**
 * Compute an index masks of points to remove to simplify the curve attribute using the
 * Ramer-Douglas-Peucker algorithm.
 */
IndexMask simplify_curve_attribute(const Span<float3> positions,
                                   const IndexMask &curves_selection,
                                   const OffsetIndices<int> points_by_curve,
                                   const VArray<bool> &cyclic,
                                   float epsilon,
                                   GSpan attribute_data,
                                   IndexMaskMemory &memory);

}  // namespace blender::geometry
