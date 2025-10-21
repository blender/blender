/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>

#include "BLI_task.hh"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"

namespace blender::bke::curves::bezier {

bool segment_is_vector(const Span<int8_t> handle_types_left,
                       const Span<int8_t> handle_types_right,
                       const int segment_index)
{
  BLI_assert(handle_types_left.index_range().drop_back(1).contains(segment_index));
  return segment_is_vector(handle_types_right[segment_index],
                           handle_types_left[segment_index + 1]);
}

bool last_cyclic_segment_is_vector(const Span<int8_t> handle_types_left,
                                   const Span<int8_t> handle_types_right)
{
  return segment_is_vector(handle_types_right.last(), handle_types_left.first());
}

void calculate_evaluated_offsets(const Span<int8_t> handle_types_left,
                                 const Span<int8_t> handle_types_right,
                                 const bool cyclic,
                                 const int resolution,
                                 MutableSpan<int> evaluated_offsets)
{
  const int size = handle_types_left.size();
  BLI_assert(evaluated_offsets.size() == size + 1);

  evaluated_offsets.first() = 0;
  if (size == 1) {
    evaluated_offsets.last() = 1;
    return;
  }

  const int points_per_segment = std::max(1, resolution);

  int offset = 0;
  for (const int i : IndexRange(size - 1)) {
    evaluated_offsets[i] = offset;
    offset += segment_is_vector(handle_types_left, handle_types_right, i) ? 1 : points_per_segment;
  }

  evaluated_offsets.last(1) = offset;
  if (cyclic) {
    offset += last_cyclic_segment_is_vector(handle_types_left, handle_types_right) ?
                  1 :
                  points_per_segment;
  }
  else {
    offset++;
  }

  evaluated_offsets.last() = offset;
}

Insertion insert(const float3 &point_prev,
                 const float3 &handle_prev,
                 const float3 &handle_next,
                 const float3 &point_next,
                 float parameter)
{
  /* De Casteljau Bezier subdivision. */
  BLI_assert(parameter <= 1.0f && parameter >= 0.0f);

  const float3 center_point = math::interpolate(handle_prev, handle_next, parameter);

  Insertion result;
  result.handle_prev = math::interpolate(point_prev, handle_prev, parameter);
  result.handle_next = math::interpolate(handle_next, point_next, parameter);
  result.left_handle = math::interpolate(result.handle_prev, center_point, parameter);
  result.right_handle = math::interpolate(center_point, result.handle_next, parameter);
  result.position = math::interpolate(result.left_handle, result.right_handle, parameter);
  return result;
}

static float3 calculate_aligned_handle(const float3 &position,
                                       const float3 &other_handle,
                                       const float3 &aligned_handle)
{
  /* Keep track of the old length of the opposite handle. */
  const float length = math::distance(aligned_handle, position);
  /* Set the other handle to directly opposite from the current handle. */
  const float3 dir = math::normalize(other_handle - position);
  return position - dir * length;
}

static void calculate_point_handles(const HandleType type_left,
                                    const HandleType type_right,
                                    const float3 position,
                                    const float3 prev_position,
                                    const float3 next_position,
                                    float3 &left,
                                    float3 &right)
{
  if (ELEM(BEZIER_HANDLE_AUTO, type_left, type_right)) {
    const float3 prev_diff = position - prev_position;
    const float3 next_diff = next_position - position;
    float prev_len = math::length(prev_diff);
    float next_len = math::length(next_diff);
    if (prev_len == 0.0f) {
      prev_len = 1.0f;
    }
    if (next_len == 0.0f) {
      next_len = 1.0f;
    }
    const float3 dir = next_diff / next_len + prev_diff / prev_len;

    /* The magic number 2.5614 is derived from approximating a circular arc at the control point.
     * Given the constraints:
     *
     * - `P0=(0,1),P1=(c,1),P2=(1,c),P3=(1,0)`.
     * - The first derivative of the curve must agree with the circular arc derivative at the
     *   endpoints.
     * - Minimize the maximum radial drift.
     *   one can compute `c ≈ 0.5519150244935105707435627`.
     *   The distance from P0 to P3 is `sqrt(2)`.
     *
     * The magic factor for `len` is `(sqrt(2) / 0.5519150244935105707435627) ≈ 2.562375546255352`.
     * In older code of blender a slightly worse approximation of 2.5614 is used. It's kept
     * for compatibility.
     *
     * See https://spencermortensen.com/articles/bezier-circle/. */
    const float len = math::length(dir) * 2.5614f;
    if (len != 0.0f) {
      if (type_left == BEZIER_HANDLE_AUTO) {
        const float prev_len_clamped = std::min(prev_len, next_len * 5.0f);
        left = position + dir * -(prev_len_clamped / len);
      }
      if (type_right == BEZIER_HANDLE_AUTO) {
        const float next_len_clamped = std::min(next_len, prev_len * 5.0f);
        right = position + dir * (next_len_clamped / len);
      }
    }
  }

  if (type_left == BEZIER_HANDLE_VECTOR) {
    left = calculate_vector_handle(position, prev_position);
  }

  if (type_right == BEZIER_HANDLE_VECTOR) {
    right = calculate_vector_handle(position, next_position);
  }

  /* When one of the handles is "aligned" handle, it must be aligned with the other, i.e. point in
   * the opposite direction. Don't handle the case of two aligned handles, because code elsewhere
   * should keep the pair consistent, and the relative locations aren't affected by other points
   * anyway. */
  if (type_left == BEZIER_HANDLE_ALIGN && type_right != BEZIER_HANDLE_ALIGN) {
    left = calculate_aligned_handle(position, right, left);
  }
  else if (type_left != BEZIER_HANDLE_ALIGN && type_right == BEZIER_HANDLE_ALIGN) {
    right = calculate_aligned_handle(position, left, right);
  }
}

void set_handle_position(const float3 &position,
                         const HandleType type,
                         const HandleType type_other,
                         const float3 &new_handle,
                         float3 &handle,
                         float3 &handle_other)
{
  /* Don't bother when the handle positions are calculated automatically anyway. */
  if (ELEM(type, BEZIER_HANDLE_AUTO, BEZIER_HANDLE_VECTOR)) {
    return;
  }

  handle = new_handle;
  if (type_other == BEZIER_HANDLE_ALIGN) {
    handle_other = calculate_aligned_handle(position, handle, handle_other);
  }
}

void calculate_aligned_handles(const IndexMask &selection,
                               const Span<float3> positions,
                               const Span<float3> align_with,
                               MutableSpan<float3> align_handles)
{
  selection.foreach_index_optimized<int>(GrainSize(4096), [&](const int point) {
    align_handles[point] = calculate_aligned_handle(
        positions[point], align_with[point], align_handles[point]);
  });
}

void calculate_auto_handles(const bool cyclic,
                            const Span<int8_t> types_left,
                            const Span<int8_t> types_right,
                            const Span<float3> positions,
                            MutableSpan<float3> positions_left,
                            MutableSpan<float3> positions_right)
{
  const int points_num = positions.size();
  if (points_num == 1) {
    return;
  }

  calculate_point_handles(HandleType(types_left.first()),
                          HandleType(types_right.first()),
                          positions.first(),
                          cyclic ? positions.last() : 2.0f * positions.first() - positions[1],
                          positions[1],
                          positions_left.first(),
                          positions_right.first());

  threading::parallel_for(IndexRange(1, points_num - 2), 1024, [&](IndexRange range) {
    for (const int i : range) {
      calculate_point_handles(HandleType(types_left[i]),
                              HandleType(types_right[i]),
                              positions[i],
                              positions[i - 1],
                              positions[i + 1],
                              positions_left[i],
                              positions_right[i]);
    }
  });

  calculate_point_handles(HandleType(types_left.last()),
                          HandleType(types_right.last()),
                          positions.last(),
                          positions.last(1),
                          cyclic ? positions.first() : 2.0f * positions.last() - positions.last(1),
                          positions_left.last(),
                          positions_right.last());
}

template<typename T>
void evaluate_segment_ex(
    const T &point_0, const T &point_1, const T &point_2, const T &point_3, MutableSpan<T> result)
{
  BLI_assert(result.size() > 0);
  const float inv_len = 1.0f / float(result.size());
  const float inv_len_squared = inv_len * inv_len;
  const float inv_len_cubed = inv_len_squared * inv_len;

  const T rt1 = 3.0f * (point_1 - point_0) * inv_len;
  const T rt2 = 3.0f * (point_0 - 2.0f * point_1 + point_2) * inv_len_squared;
  const T rt3 = (point_3 - point_0 + 3.0f * (point_1 - point_2)) * inv_len_cubed;

  T q0 = point_0;
  T q1 = rt1 + rt2 + rt3;
  T q2 = 2.0f * rt2 + 6.0f * rt3;
  T q3 = 6.0f * rt3;
  for (const int i : result.index_range()) {
    result[i] = q0;
    q0 += q1;
    q1 += q2;
    q2 += q3;
  }
}
template<>
void evaluate_segment(const float3 &point_0,
                      const float3 &point_1,
                      const float3 &point_2,
                      const float3 &point_3,
                      MutableSpan<float3> result)
{
  evaluate_segment_ex<float3>(point_0, point_1, point_2, point_3, result);
}
template<>
void evaluate_segment(const float2 &point_0,
                      const float2 &point_1,
                      const float2 &point_2,
                      const float2 &point_3,
                      MutableSpan<float2> result)
{
  evaluate_segment_ex<float2>(point_0, point_1, point_2, point_3, result);
}

void calculate_evaluated_positions(const Span<float3> positions,
                                   const Span<float3> handles_left,
                                   const Span<float3> handles_right,
                                   const OffsetIndices<int> evaluated_offsets,
                                   MutableSpan<float3> evaluated_positions)
{
  BLI_assert(evaluated_offsets.total_size() == evaluated_positions.size());
  if (evaluated_offsets.total_size() == 1) {
    evaluated_positions.first() = positions.first();
    return;
  }

  /* Evaluate the first segment. */
  evaluate_segment(positions.first(),
                   handles_right.first(),
                   handles_left[1],
                   positions[1],
                   evaluated_positions.slice(evaluated_offsets[0]));

  /* Give each task fewer segments as the resolution gets larger. */
  const int grain_size = std::max<int>(evaluated_positions.size() / positions.size() * 32, 1);
  const IndexRange inner_segments = positions.index_range().drop_back(1).drop_front(1);
  threading::parallel_for(inner_segments, grain_size, [&](IndexRange range) {
    for (const int i : range) {
      const IndexRange evaluated_range = evaluated_offsets[i];
      if (evaluated_range.size() == 1) {
        evaluated_positions[evaluated_range.first()] = positions[i];
      }
      else {
        evaluate_segment(positions[i],
                         handles_right[i],
                         handles_left[i + 1],
                         positions[i + 1],
                         evaluated_positions.slice(evaluated_range));
      }
    }
  });

  /* Evaluate the final cyclic segment if necessary. */
  const IndexRange last_segment_points = evaluated_offsets[positions.index_range().last()];
  if (last_segment_points.size() == 1) {
    evaluated_positions.last() = positions.last();
  }
  else {
    evaluate_segment(positions.last(),
                     handles_right.last(),
                     handles_left.first(),
                     positions.first(),
                     evaluated_positions.slice(last_segment_points));
  }
}

template<typename T>
static inline void linear_interpolation(const T &a, const T &b, MutableSpan<T> dst)
{
  dst.first() = a;
  const float step = 1.0f / dst.size();
  for (const int i : dst.index_range().drop_front(1)) {
    dst[i] = attribute_math::mix2(i * step, a, b);
  }
}

template<typename T>
static void interpolate_to_evaluated(const Span<T> src,
                                     const OffsetIndices<int> evaluated_offsets,
                                     MutableSpan<T> dst)
{
  BLI_assert(!src.is_empty());
  BLI_assert(evaluated_offsets.total_size() == dst.size());
  if (src.size() == 1) {
    BLI_assert(dst.size() == 1);
    dst.first() = src.first();
    return;
  }

  linear_interpolation(src.first(), src[1], dst.slice(evaluated_offsets[0]));

  threading::parallel_for(
      src.index_range().drop_back(1).drop_front(1), 512, [&](IndexRange range) {
        for (const int i : range) {
          const IndexRange segment = evaluated_offsets[i];
          linear_interpolation(src[i], src[i + 1], dst.slice(segment));
        }
      });

  const IndexRange last_segment = evaluated_offsets[src.index_range().last()];
  linear_interpolation(src.last(), src.first(), dst.slice(last_segment));
}

void interpolate_to_evaluated(const GSpan src,
                              const OffsetIndices<int> evaluated_offsets,
                              GMutableSpan dst)
{
  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      interpolate_to_evaluated(src.typed<T>(), evaluated_offsets, dst.typed<T>());
    }
  });
}

}  // namespace blender::bke::curves::bezier
