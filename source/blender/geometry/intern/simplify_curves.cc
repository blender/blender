/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_stack.hh"

#include "BKE_curves_utils.hh"

#include "GEO_simplify_curves.hh"

namespace blender::geometry {

/**
 * Computes a "perpendicular distance" value for the generic attribute data based on the
 * positions of the curve.
 *
 * First, we compute a lambda value that represents a factor from the first point to the last
 * point of the current range. This is the projection of the point of interest onto the vector
 * from the first to the last point.
 *
 * Then this lambda value is used to compute an interpolated value of the first and last point
 * and finally we compute the distance from the interpolated value to the actual value.
 * This is the "perpendicular distance".
 */
template<typename T>
float perpendicular_distance(const Span<float3> positions,
                             const Span<T> attribute_data,
                             const int64_t first_index,
                             const int64_t last_index,
                             const int64_t index)
{
  const float3 ray_dir = positions[last_index] - positions[first_index];
  float lambda = 0.0f;
  if (!math::is_zero(ray_dir)) {
    lambda = math::dot(ray_dir, positions[index] - positions[first_index]) /
             math::dot(ray_dir, ray_dir);
  }
  const T &from = attribute_data[first_index];
  const T &to = attribute_data[last_index];
  const T &value = attribute_data[index];
  const T &interpolated = math::interpolate(from, to, lambda);
  return math::distance(value, interpolated);
}

/**
 * An implementation of the Ramer-Douglas-Peucker algorithm.
 */
template<typename T>
static void ramer_douglas_peucker(const IndexRange range,
                                  const Span<float3> positions,
                                  const float epsilon,
                                  const Span<T> attribute_data,
                                  MutableSpan<bool> points_to_delete)
{
  /* Mark all points to be kept. */
  points_to_delete.slice(range).fill(false);

  Stack<IndexRange, 32> stack;
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
      const float dist = perpendicular_distance(
          positions, attribute_data, sub_range.first(), sub_range.last(), index);
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
      points_to_delete.slice(inside_range).fill(true);
    }
  }
}

template<typename T>
static void curve_simplify(const Span<float3> positions,
                           const bool cyclic,
                           const float epsilon,
                           const Span<T> attribute_data,
                           MutableSpan<bool> points_to_delete)
{
  const Vector<IndexRange> selection_ranges = array_utils::find_all_ranges(
      points_to_delete.as_span(), true);
  threading::parallel_for(
      selection_ranges.index_range(), 512, [&](const IndexRange range_of_ranges) {
        for (const IndexRange range : selection_ranges.as_span().slice(range_of_ranges)) {
          ramer_douglas_peucker(range, positions, epsilon, attribute_data, points_to_delete);
        }
      });

  /* For cyclic curves, handle the last segment separately. */
  const int points_num = positions.size();
  if (cyclic && points_num > 2) {
    const float dist = perpendicular_distance(
        positions, attribute_data, points_num - 2, 0, points_num - 1);
    if (dist <= epsilon) {
      points_to_delete[points_num - 1] = true;
    }
  }
}

IndexMask simplify_curve_attribute(const Span<float3> positions,
                                   const IndexMask &curves_selection,
                                   const OffsetIndices<int> points_by_curve,
                                   const VArray<bool> &cyclic,
                                   const float epsilon,
                                   const GSpan attribute_data,
                                   IndexMaskMemory &memory)
{
  Array<bool> points_to_delete(positions.size(), false);
  if (epsilon <= 0.0f) {
    return IndexMask::from_bools(points_to_delete, memory);
  }
  bke::curves::fill_points(
      points_by_curve, curves_selection, true, points_to_delete.as_mutable_span());
  curves_selection.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
    const IndexRange points = points_by_curve[curve_i];
    bke::attribute_math::convert_to_static_type(attribute_data.type(), [&](auto dummy) {
      using T = decltype(dummy);
      if constexpr (std::is_same_v<T, float> || std::is_same_v<T, float2> ||
                    std::is_same_v<T, float3>)
      {
        curve_simplify(positions.slice(points),
                       cyclic[curve_i],
                       epsilon,
                       attribute_data.typed<T>().slice(points),
                       points_to_delete.as_mutable_span().slice(points));
      }
    });
  });
  return IndexMask::from_bools(points_to_delete, memory);
}

}  // namespace blender::geometry
