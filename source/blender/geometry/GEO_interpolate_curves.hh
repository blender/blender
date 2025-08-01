/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_curves.hh"

namespace blender::geometry {

/**
 * Assign source point indices and interpolation factors to target points.
 *
 * \param positions: Source curve positions.
 * \param cyclic: True if the source curve is cyclic.
 * \param r_indices: Output array of point indices of the source curve.
 * \param r_factors: Output array of interpolation factors between a source point and the next.
 */
void sample_curve_padded(const Span<float3> positions,
                         bool cyclic,
                         MutableSpan<int> r_indices,
                         MutableSpan<float> r_factors);

/**
 * Assign source point indices and interpolation factors to target points for a single curve.
 *
 * \param curves: Source curves geometry to sample.
 * \param curve_index: Index of the source curve to sample.
 * \param cyclic: True if the source curve is cyclic.
 * \param reverse: True if the curve should be sampled in reverse direction.
 * \param r_indices: Output array of point indices of the source curve.
 * \param r_factors: Output array of interpolation factors between a source point and the next.
 */
void sample_curve_padded(const bke::CurvesGeometry &curves,
                         int curve_index,
                         bool cyclic,
                         bool reverse,
                         MutableSpan<int> r_indices,
                         MutableSpan<float> r_factors);

/**
 * Create new curves that are interpolated between "from" and "to" curves.
 * \param dst_curve_mask: Set of curves in \a dst_curves that are being filled.
 */
void interpolate_curves(const bke::CurvesGeometry &from_curves,
                        const bke::CurvesGeometry &to_curves,
                        Span<int> from_curve_indices,
                        Span<int> to_curve_indices,
                        const IndexMask &dst_curve_mask,
                        Span<bool> dst_curve_flip_direction,
                        float mix_factor,
                        bke::CurvesGeometry &dst_curves,
                        IndexMaskMemory &memory);

void interpolate_curves_with_samples(const bke::CurvesGeometry &from_curves,
                                     const bke::CurvesGeometry &to_curves,
                                     Span<int> from_curve_indices,
                                     Span<int> to_curve_indices,
                                     Span<int> from_sample_indices,
                                     Span<int> to_sample_indices,
                                     Span<float> from_sample_factors,
                                     Span<float> to_sample_factors,
                                     const IndexMask &dst_curve_mask,
                                     float mix_factor,
                                     bke::CurvesGeometry &dst_curves,
                                     IndexMaskMemory &memory);

}  // namespace blender::geometry
