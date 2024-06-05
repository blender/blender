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

/**
 * Same as above, but only for a single curve. All spans are expected to be the size of the curve.
 */
void curve_simplify(const Span<float3> positions,
                    const bool cyclic,
                    const float epsilon,
                    const GSpan attribute_data,
                    MutableSpan<bool> points_to_delete);

}  // namespace blender::geometry
