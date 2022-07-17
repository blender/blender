/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_curves.hh"

/** \file
 * \ingroup bke
 * \brief Low-level operations for curves.
 */

#include "BLI_function_ref.hh"
#include "BLI_generic_pointer.hh"

namespace blender::bke::curves {

/**
 * Copy the provided point attribute values between all curves in the #curve_ranges index
 * ranges, assuming that all curves have the same number of control points in #src_curves
 * and #dst_curves.
 */
void copy_point_data(const CurvesGeometry &src_curves,
                     const CurvesGeometry &dst_curves,
                     Span<IndexRange> curve_ranges,
                     GSpan src,
                     GMutableSpan dst);

void copy_point_data(const CurvesGeometry &src_curves,
                     const CurvesGeometry &dst_curves,
                     IndexMask src_curve_selection,
                     GSpan src,
                     GMutableSpan dst);

template<typename T>
void copy_point_data(const CurvesGeometry &src_curves,
                     const CurvesGeometry &dst_curves,
                     const IndexMask src_curve_selection,
                     const Span<T> src,
                     MutableSpan<T> dst)
{
  copy_point_data(src_curves, dst_curves, src_curve_selection, GSpan(src), GMutableSpan(dst));
}

void fill_points(const CurvesGeometry &curves,
                 IndexMask curve_selection,
                 GPointer value,
                 GMutableSpan dst);

template<typename T>
void fill_points(const CurvesGeometry &curves,
                 const IndexMask curve_selection,
                 const T &value,
                 MutableSpan<T> dst)
{
  fill_points(curves, curve_selection, &value, dst);
}

/**
 * Copy the size of every curve in #curve_ranges to the corresponding index in #counts.
 */
void fill_curve_counts(const bke::CurvesGeometry &curves,
                       Span<IndexRange> curve_ranges,
                       MutableSpan<int> counts);

/**
 * Turn an array of sizes into the offset at each index including all previous sizes.
 */
void accumulate_counts_to_offsets(MutableSpan<int> counts_to_offsets, int start_offset = 0);

IndexMask indices_for_type(const VArray<int8_t> &types,
                           const std::array<int, CURVE_TYPES_NUM> &type_counts,
                           const CurveType type,
                           const IndexMask selection,
                           Vector<int64_t> &r_indices);

void foreach_curve_by_type(const VArray<int8_t> &types,
                           const std::array<int, CURVE_TYPES_NUM> &type_counts,
                           IndexMask selection,
                           FunctionRef<void(IndexMask)> catmull_rom_fn,
                           FunctionRef<void(IndexMask)> poly_fn,
                           FunctionRef<void(IndexMask)> bezier_fn,
                           FunctionRef<void(IndexMask)> nurbs_fn);

}  // namespace blender::bke::curves
