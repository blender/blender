/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include <limits>

#include "BLI_math_vector.hh"
#include "BLI_stack.hh"

#include "BKE_grease_pencil.hh"

#include "ED_grease_pencil.hh"

extern "C" {
#include "curve_fit_nd.h"
}

namespace blender::ed::greasepencil {

int64_t ramer_douglas_peucker_simplify(
    const IndexRange range,
    const float epsilon,
    const FunctionRef<float(int64_t, int64_t, int64_t)> dist_function,
    MutableSpan<bool> points_to_delete)
{
  /* Mark all points to not be removed. */
  points_to_delete.slice(range).fill(false);
  int64_t total_points_to_remove = 0;

  Stack<IndexRange> stack;
  stack.push(range);
  while (!stack.is_empty()) {
    const IndexRange sub_range = stack.pop();
    /* Skip ranges with less than 3 points. All points are kept. */
    if (sub_range.size() < 3) {
      continue;
    }
    const IndexRange inside_range = sub_range.drop_front(1).drop_back(1);
    /* Compute the maximum distance and the corresponding index. */
    float max_dist = -1.0f;
    int max_index = -1;
    for (const int64_t index : inside_range) {
      const float dist = dist_function(sub_range.first(), sub_range.last(), index);
      if (dist > max_dist) {
        max_dist = dist;
        max_index = index - sub_range.first();
      }
    }

    if (max_dist > epsilon) {
      /* Found point outside the epsilon-sized strip. The point at `max_index` will be kept, repeat
       * the search on the left & right side. */
      stack.push(sub_range.slice(0, max_index + 1));
      stack.push(sub_range.slice(max_index, sub_range.size() - max_index));
    }
    else {
      /* Points in `sub_range` are inside the epsilon-sized strip. Mark them to be deleted. */
      total_points_to_remove += inside_range.size();
      points_to_delete.slice(inside_range).fill(true);
    }
  }
  return total_points_to_remove;
}

Array<float2> polyline_fit_curve(Span<float2> points,
                                 const float error_threshold,
                                 const IndexMask &corner_mask)
{
  if (points.is_empty()) {
    return {};
  }
  double total_length = 0.0;
  for (const int point_i : points.index_range().drop_front(1)) {
    total_length += math::distance(points[point_i - 1], points[point_i]);
  }
  /* Just return a dot. */
  if (total_length < 1e-8) {
    return Array<float2>({points[0], points[0], points[0]});
  }

  Array<int32_t> indices(corner_mask.size());
  corner_mask.to_indices(indices.as_mutable_span());
  uint *indicies_ptr = corner_mask.is_empty() ? nullptr : reinterpret_cast<uint *>(indices.data());

  float *r_cubic_array;
  uint r_cubic_array_len;
  int error = curve_fit_cubic_to_points_fl(*points.data(),
                                           points.size(),
                                           2,
                                           error_threshold,
                                           CURVE_FIT_CALC_HIGH_QUALIY,
                                           indicies_ptr,
                                           indices.size(),
                                           &r_cubic_array,
                                           &r_cubic_array_len,
                                           nullptr,
                                           nullptr,
                                           nullptr);

  if (error != 0) {
    /* Some error occurred. Return. */
    return {};
  }

  if (r_cubic_array == nullptr) {
    return {};
  }

  Span<float2> r_cubic_array_span(reinterpret_cast<float2 *>(r_cubic_array),
                                  r_cubic_array_len * 3);
  Array<float2> curve_positions(r_cubic_array_span);
  /* Free the c-style array. */
  free(r_cubic_array);
  return curve_positions;
}

IndexMask polyline_detect_corners(Span<float2> points,
                                  const float radius_min,
                                  const float radius_max,
                                  const int samples_max,
                                  const float angle_threshold,
                                  IndexMaskMemory &memory)
{
  if (points.is_empty()) {
    return {};
  }
  if (points.size() == 1) {
    return IndexMask::from_indices<int>({0}, memory);
  }
  uint *r_corners;
  uint r_corner_len;
  const int error = curve_fit_corners_detect_fl(*points.data(),
                                                points.size(),
                                                float2::type_length,
                                                radius_min,
                                                radius_max,
                                                samples_max,
                                                angle_threshold,
                                                &r_corners,
                                                &r_corner_len);
  if (error != 0) {
    /* Error occurred, return. */
    return IndexMask();
  }

  if (r_corners == nullptr) {
    return IndexMask();
  }

  BLI_assert(samples_max < std::numeric_limits<int>::max());
  Span<int> indices(reinterpret_cast<int *>(r_corners), r_corner_len);
  const IndexMask corner_mask = IndexMask::from_indices<int>(indices, memory);
  /* Free the c-style array. */
  free(r_corners);
  return corner_mask;
}

}  // namespace blender::ed::greasepencil
