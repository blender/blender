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

bool last_cylic_segment_is_vector(const Span<int8_t> handle_types_left,
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
    offset += last_cylic_segment_is_vector(handle_types_left, handle_types_right) ? 1 : resolution;
  }
  else {
    offset++;
  }

  evaluated_offsets.last() = offset;
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
  BLI_assert(dst.size() == src.size());
  BLI_assert(evaluated_offsets.last() == dst.size());

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

/** \} */

}  // namespace blender::bke::curves::bezier
