/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_task.hh"

#include "BKE_curves_utils.hh"
#include "BKE_deform.hh"

#include "GEO_fit_curves.hh"

extern "C" {
#include "curve_fit_nd.h"
}

namespace blender::geometry {

bke::CurvesGeometry fit_poly_to_bezier_curves(const bke::CurvesGeometry &src_curves,
                                              const IndexMask &curve_selection,
                                              const VArray<float> &thresholds,
                                              const VArray<bool> &corners,
                                              const FitMethod method,
                                              const bke::AttributeFilter &attribute_filter)
{
  if (curve_selection.is_empty()) {
    return src_curves;
  }

  BLI_assert(thresholds.size() == src_curves.curves_num());
  BLI_assert(corners.size() == src_curves.points_num());

  const OffsetIndices src_points_by_curve = src_curves.offsets();
  const Span<float3> src_positions = src_curves.positions();
  const VArray<bool> src_cyclic = src_curves.cyclic();

  bke::CurvesGeometry dst_curves = bke::curves::copy_only_curve_domain(src_curves);
  BKE_defgroup_copy_list(&dst_curves.vertex_group_names, &src_curves.vertex_group_names);

  IndexMaskMemory memory;
  const IndexMask unselected_curves = curve_selection.complement(src_curves.curves_range(),
                                                                 memory);

  /* Write the new sizes to the dst_curve_sizes, they will be accumulated later to offsets. */
  MutableSpan<int> dst_curve_sizes = dst_curves.offsets_for_write();
  offset_indices::copy_group_sizes(src_points_by_curve, unselected_curves, dst_curve_sizes);
  MutableSpan<int8_t> dst_curve_types = dst_curves.curve_types_for_write();

  /* NOTE: These spans own the data from the curve fit C-API. */
  Array<MutableSpan<float3>> cubic_array_per_curve(curve_selection.size());
  Array<MutableSpan<int>> corner_indices_per_curve(curve_selection.size());
  Array<MutableSpan<int>> original_indices_per_curve(curve_selection.size());

  std::atomic<bool> success = false;
  curve_selection.foreach_index(GrainSize(32), [&](const int64_t curve_i, const int64_t pos) {
    const IndexRange points = src_points_by_curve[curve_i];
    const Span<float3> curve_positions = src_positions.slice(points);
    const bool is_cyclic = src_cyclic[curve_i];
    const float epsilon = thresholds[curve_i];

    /* Both curve fitting algorithms expect the first and last points for non-cyclic curves to be
     * treated as if they were corners. */
    const bool use_first_as_corner = !is_cyclic && !corners[points.first()];
    const bool use_last_as_corner = !is_cyclic && !corners[points.last()];
    Vector<int, 32> src_corners;
    if (use_first_as_corner) {
      src_corners.append(0);
    }
    if (points.size() > 2) {
      for (const int i : IndexRange::from_begin_end(1, points.size() - 1)) {
        if (corners[points[i]]) {
          src_corners.append(i);
        }
      }
    }
    if (use_last_as_corner) {
      src_corners.append(points.last());
    }
    const uint *src_corners_ptr = src_corners.is_empty() ?
                                      nullptr :
                                      reinterpret_cast<uint *>(src_corners.data());

    const uint8_t flag = CURVE_FIT_CALC_HIGH_QUALIY | ((is_cyclic) ? CURVE_FIT_CALC_CYCLIC : 0);

    float *cubic_array = nullptr;
    uint32_t *orig_index_map = nullptr;
    uint32_t cubic_array_size = 0;
    uint32_t *corner_index_array = nullptr;
    uint32_t corner_index_array_size = 0;
    int error = 1;
    if (method == FitMethod::Split) {
      error = curve_fit_cubic_to_points_fl(curve_positions.cast<float>().data(),
                                           curve_positions.size(),
                                           3,
                                           epsilon,
                                           flag,
                                           src_corners_ptr,
                                           src_corners.size(),
                                           &cubic_array,
                                           &cubic_array_size,
                                           &orig_index_map,
                                           &corner_index_array,
                                           &corner_index_array_size);
    }
    else if (method == FitMethod::Refit) {
      error = curve_fit_cubic_to_points_refit_fl(curve_positions.cast<float>().data(),
                                                 curve_positions.size(),
                                                 3,
                                                 epsilon,
                                                 flag,
                                                 src_corners_ptr,
                                                 src_corners.size(),
                                                 /* Don't use automatic corner detection. */
                                                 FLT_MAX,
                                                 &cubic_array,
                                                 &cubic_array_size,
                                                 &orig_index_map,
                                                 &corner_index_array,
                                                 &corner_index_array_size);
    }

    if (error) {
      /* Some error occurred. Fall back to using the input positions as the (poly) curve. */
      dst_curve_sizes[curve_i] = points.size();
      dst_curve_types[curve_i] = CURVE_TYPE_POLY;
      return;
    }

    success.store(true, std::memory_order_relaxed);

    const int dst_points_num = cubic_array_size;
    BLI_assert(dst_points_num > 0);

    dst_curve_sizes[curve_i] = dst_points_num;
    dst_curve_types[curve_i] = CURVE_TYPE_BEZIER;

    cubic_array_per_curve[pos] = MutableSpan<float3>(reinterpret_cast<float3 *>(cubic_array),
                                                     dst_points_num * 3);
    corner_indices_per_curve[pos] = MutableSpan<int>(reinterpret_cast<int *>(corner_index_array),
                                                     corner_index_array_size);
    original_indices_per_curve[pos] = MutableSpan<int>(reinterpret_cast<int *>(orig_index_map),
                                                       dst_points_num);
  });

  if (!success) {
    /* None of the curve fittings succeeded. */
    return src_curves;
  }

  const OffsetIndices dst_points_by_curve = offset_indices::accumulate_counts_to_offsets(
      dst_curve_sizes);
  dst_curves.resize(dst_curves.offsets().last(), dst_curves.curves_num());

  const Span<float3> src_handles_left = src_curves.handle_positions_left();
  const Span<float3> src_handles_right = src_curves.handle_positions_right();
  const VArraySpan<int8_t> src_handle_types_left = src_curves.handle_types_left();
  const VArraySpan<int8_t> src_handle_types_right = src_curves.handle_types_right();

  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();
  MutableSpan<float3> dst_handles_left = dst_curves.handle_positions_left_for_write();
  MutableSpan<float3> dst_handles_right = dst_curves.handle_positions_right_for_write();
  MutableSpan<int8_t> dst_handle_types_left = dst_curves.handle_types_left_for_write();
  MutableSpan<int8_t> dst_handle_types_right = dst_curves.handle_types_right_for_write();

  /* First handle the unselected curves. */
  if (!src_handles_left.is_empty()) {
    array_utils::copy_group_to_group(src_points_by_curve,
                                     dst_points_by_curve,
                                     unselected_curves,
                                     src_handles_left,
                                     dst_handles_left);
  }
  array_utils::copy_group_to_group(
      src_points_by_curve, dst_points_by_curve, unselected_curves, src_positions, dst_positions);
  if (!src_handles_right.is_empty()) {
    array_utils::copy_group_to_group(src_points_by_curve,
                                     dst_points_by_curve,
                                     unselected_curves,
                                     src_handles_right,
                                     dst_handles_right);
  }
  if (!src_handle_types_left.is_empty()) {
    array_utils::copy_group_to_group(src_points_by_curve,
                                     dst_points_by_curve,
                                     unselected_curves,
                                     src_handle_types_left,
                                     dst_handle_types_left);
  }
  if (!src_handle_types_right.is_empty()) {
    array_utils::copy_group_to_group(src_points_by_curve,
                                     dst_points_by_curve,
                                     unselected_curves,
                                     src_handle_types_right,
                                     dst_handle_types_right);
  }

  Array<int> old_by_new_map(dst_curves.points_num());
  unselected_curves.foreach_index(GrainSize(1024), [&](const int64_t curve_i) {
    const IndexRange src_points = src_points_by_curve[curve_i];
    const IndexRange dst_points = dst_points_by_curve[curve_i];
    array_utils::fill_index_range<int>(old_by_new_map.as_mutable_span().slice(dst_points),
                                       src_points.start());
  });

  /* Now copy the data of the newly fitted curves. */
  curve_selection.foreach_index(GrainSize(1024), [&](const int64_t curve_i, const int64_t pos) {
    const IndexRange src_points = src_points_by_curve[curve_i];
    const IndexRange dst_points = dst_points_by_curve[curve_i];
    MutableSpan<float3> positions = dst_positions.slice(dst_points);
    MutableSpan<int> old_by_new = old_by_new_map.as_mutable_span().slice(dst_points);

    if (dst_curve_types[curve_i] == CURVE_TYPE_POLY) {
      /* Handle the curves for which the curve fitting has failed. */
      BLI_assert(src_points.size() == dst_points.size());
      positions.copy_from(src_positions.slice(src_points));
      dst_handles_left.slice(dst_points).copy_from(src_positions.slice(src_points));
      dst_handles_right.slice(dst_points).copy_from(src_positions.slice(src_points));
      dst_handle_types_left.slice(dst_points).fill(BEZIER_HANDLE_FREE);
      dst_handle_types_right.slice(dst_points).fill(BEZIER_HANDLE_FREE);
      array_utils::fill_index_range<int>(old_by_new, src_points.start());
      return;
    }

    const Span<float3> cubic_array = cubic_array_per_curve[pos];
    BLI_assert(dst_points.size() * 3 == cubic_array.size());
    MutableSpan<float3> left_handles = dst_handles_left.slice(dst_points);
    MutableSpan<float3> right_handles = dst_handles_right.slice(dst_points);
    threading::parallel_for(dst_points.index_range(), 8192, [&](const IndexRange range) {
      for (const int i : range) {
        const int index = i * 3;
        positions[i] = cubic_array[index + 1];
        left_handles[i] = cubic_array[index];
        right_handles[i] = cubic_array[index + 2];
      }
    });

    const Span<int> corner_indices = corner_indices_per_curve[pos];
    dst_handle_types_left.slice(dst_points).fill_indices(corner_indices, BEZIER_HANDLE_FREE);
    dst_handle_types_right.slice(dst_points).fill_indices(corner_indices, BEZIER_HANDLE_FREE);

    const Span<int> original_indices = original_indices_per_curve[pos];
    threading::parallel_for(dst_points.index_range(), 8192, [&](const IndexRange range) {
      for (const int i : range) {
        old_by_new[i] = src_points[original_indices[i]];
      }
    });
  });

  dst_curves.update_curve_types();

  bke::gather_attributes(
      src_curves.attributes(),
      bke::AttrDomain::Point,
      bke::AttrDomain::Point,
      bke::attribute_filter_with_skip_ref(
          attribute_filter,
          {"position", "handle_left", "handle_right", "handle_type_left", "handle_type_right"}),
      old_by_new_map,
      dst_curves.attributes_for_write());

  /* Free all the data from the C-API. */
  for (MutableSpan<float3> cubic_array : cubic_array_per_curve) {
    free(cubic_array.data());
  }
  for (MutableSpan<int> corner_indices : corner_indices_per_curve) {
    free(corner_indices.data());
  }
  for (MutableSpan<int> original_indices : original_indices_per_curve) {
    free(original_indices.data());
  }

  return dst_curves;
}

}  // namespace blender::geometry
