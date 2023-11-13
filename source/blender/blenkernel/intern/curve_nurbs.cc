/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_task.hh"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"

namespace blender::bke::curves::nurbs {

bool check_valid_num_and_order(const int points_num,
                               const int8_t order,
                               const bool cyclic,
                               const KnotsMode knots_mode)
{
  if (points_num < order) {
    return false;
  }

  if (ELEM(knots_mode, NURBS_KNOT_MODE_BEZIER, NURBS_KNOT_MODE_ENDPOINT_BEZIER)) {
    if (knots_mode == NURBS_KNOT_MODE_BEZIER && points_num <= order) {
      return false;
    }
    return (!cyclic || points_num % (order - 1) == 0);
  }

  return true;
}

int calculate_evaluated_num(const int points_num,
                            const int8_t order,
                            const bool cyclic,
                            const int resolution,
                            const KnotsMode knots_mode)
{
  if (!check_valid_num_and_order(points_num, order, cyclic, knots_mode)) {
    return points_num;
  }
  return resolution * segments_num(points_num, cyclic);
}

int knots_num(const int points_num, const int8_t order, const bool cyclic)
{
  if (cyclic) {
    return points_num + order * 2 - 1;
  }
  return points_num + order;
}

void calculate_knots(const int points_num,
                     const KnotsMode mode,
                     const int8_t order,
                     const bool cyclic,
                     MutableSpan<float> knots)
{
  BLI_assert(knots.size() == knots_num(points_num, order, cyclic));
  UNUSED_VARS_NDEBUG(points_num);

  const bool is_bezier = ELEM(mode, NURBS_KNOT_MODE_BEZIER, NURBS_KNOT_MODE_ENDPOINT_BEZIER);
  const bool is_end_point = ELEM(mode, NURBS_KNOT_MODE_ENDPOINT, NURBS_KNOT_MODE_ENDPOINT_BEZIER);
  /* Inner knots are always repeated once except on Bezier case. */
  const int repeat_inner = is_bezier ? order - 1 : 1;
  /* How many times to repeat 0.0 at the beginning of knot. */
  const int head = is_end_point ? (order - (cyclic ? 1 : 0)) :
                                  (is_bezier ? min_ii(2, repeat_inner) : 1);
  /* Number of knots replicating widths of the starting knots.
   * Covers both Cyclic and EndPoint cases. */
  const int tail = cyclic ? 2 * order - 1 : (is_end_point ? order : 0);

  int r = head;
  float current = 0.0f;

  const int offset = is_end_point && cyclic ? 1 : 0;
  if (offset) {
    knots[0] = current;
    current += 1.0f;
  }

  for (const int i : IndexRange(offset, knots.size() - offset - tail)) {
    knots[i] = current;
    r--;
    if (r == 0) {
      current += 1.0;
      r = repeat_inner;
    }
  }

  const int tail_index = knots.size() - tail;
  for (const int i : IndexRange(tail)) {
    knots[tail_index + i] = current + (knots[i] - knots[0]);
  }
}

static void calculate_basis_for_point(const float parameter,
                                      const int points_num,
                                      const int degree,
                                      const Span<float> knots,
                                      MutableSpan<float> r_weights,
                                      int &r_start_index)
{
  const int order = degree + 1;

  int start = 0;
  int end = 0;
  for (const int i : IndexRange(points_num + degree)) {
    const bool knots_equal = knots[i] == knots[i + 1];
    if (knots_equal || parameter < knots[i] || parameter > knots[i + 1]) {
      continue;
    }

    start = std::max(i - degree, 0);
    end = i;
    break;
  }

  Array<float, 12> buffer(order * 2, 0.0f);

  buffer[end - start] = 1.0f;

  for (const int i_order : IndexRange(2, degree)) {
    if (end + i_order >= knots.size()) {
      end = points_num + degree - i_order;
    }
    for (const int i : IndexRange(end - start + 1)) {
      const int knot_index = start + i;

      float new_basis = 0.0f;
      if (buffer[i] != 0.0f) {
        new_basis += ((parameter - knots[knot_index]) * buffer[i]) /
                     (knots[knot_index + i_order - 1] - knots[knot_index]);
      }

      if (buffer[i + 1] != 0.0f) {
        new_basis += ((knots[knot_index + i_order] - parameter) * buffer[i + 1]) /
                     (knots[knot_index + i_order] - knots[knot_index + 1]);
      }

      buffer[i] = new_basis;
    }
  }

  buffer.as_mutable_span().drop_front(end - start + 1).fill(0.0f);
  r_weights.copy_from(buffer.as_span().take_front(order));
  r_start_index = start;
}

void calculate_basis_cache(const int points_num,
                           const int evaluated_num,
                           const int8_t order,
                           const bool cyclic,
                           const Span<float> knots,
                           BasisCache &basis_cache)
{
  BLI_assert(points_num > 0);

  const int8_t degree = order - 1;

  basis_cache.weights.resize(evaluated_num * order);
  basis_cache.start_indices.resize(evaluated_num);

  if (evaluated_num == 0) {
    return;
  }

  MutableSpan<float> basis_weights(basis_cache.weights);
  MutableSpan<int> basis_start_indices(basis_cache.start_indices);

  const int last_control_point_index = cyclic ? points_num + degree : points_num;
  const int evaluated_segment_num = segments_num(evaluated_num, cyclic);

  const float start = knots[degree];
  const float end = knots[last_control_point_index];
  const float step = (end - start) / evaluated_segment_num;
  for (const int i : IndexRange(evaluated_num)) {
    /* Clamp parameter due to floating point inaccuracy. */
    const float parameter = std::clamp(start + step * i, knots[0], knots[points_num + degree]);

    MutableSpan<float> point_weights = basis_weights.slice(i * order, order);

    calculate_basis_for_point(
        parameter, last_control_point_index, degree, knots, point_weights, basis_start_indices[i]);
  }
}

template<typename T>
static void interpolate_to_evaluated(const BasisCache &basis_cache,
                                     const int8_t order,
                                     const Span<T> src,
                                     MutableSpan<T> dst)
{
  attribute_math::DefaultMixer<T> mixer{dst};

  threading::parallel_for(dst.index_range(), 128, [&](const IndexRange range) {
    for (const int i : range) {
      Span<float> point_weights = basis_cache.weights.as_span().slice(i * order, order);
      for (const int j : point_weights.index_range()) {
        const int point_index = (basis_cache.start_indices[i] + j) % src.size();
        mixer.mix_in(i, src[point_index], point_weights[j]);
      }
    }
    mixer.finalize(range);
  });
}

template<typename T>
static void interpolate_to_evaluated_rational(const BasisCache &basis_cache,
                                              const int8_t order,
                                              const Span<float> control_weights,
                                              const Span<T> src,
                                              MutableSpan<T> dst)
{
  attribute_math::DefaultMixer<T> mixer{dst};

  threading::parallel_for(dst.index_range(), 128, [&](const IndexRange range) {
    for (const int i : range) {
      Span<float> point_weights = basis_cache.weights.as_span().slice(i * order, order);

      for (const int j : point_weights.index_range()) {
        const int point_index = (basis_cache.start_indices[i] + j) % src.size();
        const float weight = point_weights[j] * control_weights[point_index];
        mixer.mix_in(i, src[point_index], weight);
      }
    }
    mixer.finalize(range);
  });
}

void interpolate_to_evaluated(const BasisCache &basis_cache,
                              const int8_t order,
                              const Span<float> control_weights,
                              const GSpan src,
                              GMutableSpan dst)
{
  if (basis_cache.invalid) {
    dst.copy_from(src);
    return;
  }

  BLI_assert(dst.size() == basis_cache.start_indices.size());
  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      if (control_weights.is_empty()) {
        interpolate_to_evaluated(basis_cache, order, src.typed<T>(), dst.typed<T>());
      }
      else {
        interpolate_to_evaluated_rational(
            basis_cache, order, control_weights, src.typed<T>(), dst.typed<T>());
      }
    }
  });
}

}  // namespace blender::bke::curves::nurbs
