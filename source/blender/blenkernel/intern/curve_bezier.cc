/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"

namespace blender::bke::curves::bezier {

bool segment_is_vector(const Span<int8_t> handle_types_left,
                       const Span<int8_t> handle_types_right,
                       const int segment_index)
{
  BLI_assert(handle_types_left.index_range().drop_back(1).contains(segment_index));
  return handle_types_right[segment_index] == BEZIER_HANDLE_VECTOR &&
         handle_types_left[segment_index + 1] == BEZIER_HANDLE_VECTOR;
}

bool last_cyclic_segment_is_vector(const Span<int8_t> handle_types_left,
                                   const Span<int8_t> handle_types_right)
{
  return handle_types_right.last() == BEZIER_HANDLE_VECTOR &&
         handle_types_left.first() == BEZIER_HANDLE_VECTOR;
}

void calculate_evaluated_offsets(const Span<int8_t> handle_types_left,
                                 const Span<int8_t> handle_types_right,
                                 const bool cyclic,
                                 const int resolution,
                                 MutableSpan<int> evaluated_offsets)
{
  const int size = handle_types_left.size();
  BLI_assert(evaluated_offsets.size() == size);

  if (size == 1) {
    evaluated_offsets.first() = 1;
    return;
  }

  int offset = 0;

  for (const int i : IndexRange(size - 1)) {
    offset += segment_is_vector(handle_types_left, handle_types_right, i) ? 1 : resolution;
    evaluated_offsets[i] = offset;
  }

  if (cyclic) {
    offset += last_cyclic_segment_is_vector(handle_types_left, handle_types_right) ? 1 : resolution;
  }
  else {
    offset++;
  }

  evaluated_offsets.last() = offset;
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

    /* This magic number is unfortunate, but comes from elsewhere in Blender. */
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
    left = math::interpolate(position, prev_position, 1.0f / 3.0f);
  }

  if (type_right == BEZIER_HANDLE_VECTOR) {
    right = math::interpolate(position, next_position, 1.0f / 3.0f);
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

void evaluate_segment(const float3 &point_0,
                      const float3 &point_1,
                      const float3 &point_2,
                      const float3 &point_3,
                      MutableSpan<float3> result)
{
  BLI_assert(result.size() > 0);
  const float inv_len = 1.0f / static_cast<float>(result.size());
  const float inv_len_squared = inv_len * inv_len;
  const float inv_len_cubed = inv_len_squared * inv_len;

  const float3 rt1 = 3.0f * (point_1 - point_0) * inv_len;
  const float3 rt2 = 3.0f * (point_0 - 2.0f * point_1 + point_2) * inv_len_squared;
  const float3 rt3 = (point_3 - point_0 + 3.0f * (point_1 - point_2)) * inv_len_cubed;

  float3 q0 = point_0;
  float3 q1 = rt1 + rt2 + rt3;
  float3 q2 = 2.0f * rt2 + 6.0f * rt3;
  float3 q3 = 6.0f * rt3;
  for (const int i : result.index_range()) {
    result[i] = q0;
    q0 += q1;
    q1 += q2;
    q2 += q3;
  }
}

void calculate_evaluated_positions(const Span<float3> positions,
                                   const Span<float3> handles_left,
                                   const Span<float3> handles_right,
                                   const Span<int> evaluated_offsets,
                                   MutableSpan<float3> evaluated_positions)
{
  BLI_assert(evaluated_offsets.last() == evaluated_positions.size());
  BLI_assert(evaluated_offsets.size() == positions.size());
  if (evaluated_offsets.last() == 1) {
    evaluated_positions.first() = positions.first();
    return;
  }

  /* Evaluate the first segment. */
  evaluate_segment(positions.first(),
                   handles_right.first(),
                   handles_left[1],
                   positions[1],
                   evaluated_positions.take_front(evaluated_offsets.first()));

  /* Give each task fewer segments as the resolution gets larger. */
  const int grain_size = std::max<int>(evaluated_positions.size() / positions.size() * 32, 1);
  threading::parallel_for(
      positions.index_range().drop_back(1).drop_front(1), grain_size, [&](IndexRange range) {
        for (const int i : range) {
          const IndexRange evaluated_range = offsets_to_range(evaluated_offsets, i - 1);
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
  const IndexRange last_segment_points = offsets_to_range(evaluated_offsets, positions.size() - 2);
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
                                     const Span<int> evaluated_offsets,
                                     MutableSpan<T> dst)
{
  BLI_assert(!src.is_empty());
  BLI_assert(evaluated_offsets.size() == src.size());
  BLI_assert(evaluated_offsets.last() == dst.size());
  if (src.size() == 1) {
    BLI_assert(dst.size() == 1);
    dst.first() = src.first();
    return;
  }

  linear_interpolation(src.first(), src[1], dst.take_front(evaluated_offsets.first()));

  threading::parallel_for(
      src.index_range().drop_back(1).drop_front(1), 512, [&](IndexRange range) {
        for (const int i : range) {
          const IndexRange segment_points = offsets_to_range(evaluated_offsets, i - 1);
          linear_interpolation(src[i], src[i + 1], dst.slice(segment_points));
        }
      });

  const IndexRange last_segment_points(evaluated_offsets.last(1),
                                       evaluated_offsets.last() - evaluated_offsets.last(1));
  linear_interpolation(src.last(), src.first(), dst.slice(last_segment_points));
}

void interpolate_to_evaluated(const GSpan src, const Span<int> evaluated_offsets, GMutableSpan dst)
{
  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      interpolate_to_evaluated(src.typed<T>(), evaluated_offsets, dst.typed<T>());
    }
  });
}

}  // namespace blender::bke::curves::bezier
