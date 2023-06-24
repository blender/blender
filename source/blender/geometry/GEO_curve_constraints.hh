/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_curves.hh"

namespace blender::geometry::curve_constraints {

void compute_segment_lengths(OffsetIndices<int> points_by_curve,
                             Span<float3> positions,
                             const IndexMask &curve_selection,
                             MutableSpan<float> r_segment_lengths);

void solve_length_constraints(OffsetIndices<int> points_by_curve,
                              const IndexMask &curve_selection,
                              Span<float> segment_lenghts,
                              MutableSpan<float3> positions);

void solve_length_and_collision_constraints(OffsetIndices<int> points_by_curve,
                                            const IndexMask &curve_selection,
                                            Span<float> segment_lengths,
                                            Span<float3> start_positions,
                                            const Mesh &surface,
                                            const bke::CurvesSurfaceTransforms &transforms,
                                            MutableSpan<float3> positions);

}  // namespace blender::geometry::curve_constraints
