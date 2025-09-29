/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_math_rotation_legacy.hh"
#include "BLI_math_vector.hh"

#include "BKE_curves.hh"

namespace blender::bke::curves::poly {

static bool delta_dir(const float3 &pos, const float3 &next, float3 &r_delta_dir)
{
  const float epsilon = 1.0e-9f;

  const float3 delta = next - pos;
  const float norm = math::length(delta);
  if (UNLIKELY(norm < epsilon)) {
    return false;
  }
  r_delta_dir = delta / norm;
  return true;
}

/**
 * Computes an approximate tangent from the normalized sum from
 * the direction vectors to neighboring points on the curve.
 */
static float3 direction_bisect(const float3 &pos,
                               const float3 &next,
                               float3 &other_dir,
                               bool &is_equal)
{
  const float epsilon = 1.0e-9f;
  const bool prev_equal = is_equal;

  const float3 next_delta = next - pos;
  const float next_norm = math::length(next_delta);
  is_equal = next_norm < epsilon;
  if (UNLIKELY(is_equal)) {
    /* Return the direction relative the 'previous' point. If 'prev_equal' is true, this
     * will return the direction of the last non-zero segment.
     */
    return other_dir;
  }

  const float3 prev_dir = other_dir;
  other_dir = next_delta / next_norm;
  if (UNLIKELY(prev_equal)) {
    /* Return the direction of the next segment as previous direction is not an adjacent segment!
     */
    return other_dir;
  }
  const float3 tangent = prev_dir + other_dir;
  const float norm = math::length(tangent);
  if (norm < 0.6627619f) { /* Approximates angle between segments < 45°) */
    if (norm < 2e-7) {     /* Approximately < sin(1e-5) */
      return other_dir;
    }
    /* Compute using the cross product, as catastrophic cancellation occurs in `tangent`
     * when the sum approaches 0, leading to significant numerical errors (see #146332).
     */
    const float3 binormal = math::cross(other_dir, prev_dir);
    const float3 normal = other_dir - prev_dir;
    return math::normalize(math::cross(binormal, normal));
  }
  return tangent / norm;
}

void calculate_tangents(const Span<float3> positions,
                        const bool is_cyclic,
                        MutableSpan<float3> tangents)
{
  BLI_assert(positions.size() == tangents.size());

  if (positions.is_empty()) {
    return;
  }

  if (positions.size() == 1) {
    tangents.first() = float3(0.0f, 0.0f, 1.0f);
    return;
  }

  /* Find an initial valid tangent. */
  int first_valid_index = -1;
  for (const int i : IndexRange(0, positions.size() - 1)) {
    if (delta_dir(positions[i], positions[i + 1], tangents[i])) {
      first_valid_index = i;
      break;
    }
  }

  if (first_valid_index == -1) {
    /* If all tangents used the fallback, it means that all positions are (almost) the same. Just
     * use the up-vector as default tangent. */
    const float3 up_vector{0.0f, 0.0f, 1.0f};
    tangents.fill(up_vector);
    return;
  }
  if (first_valid_index > 0) {
    tangents.slice(0, first_valid_index).fill(tangents[first_valid_index]);
  }

  /* Calculate curve tangents using the delta from previous iteration(s). */
  float3 prev_delta = tangents[first_valid_index];
  bool prev_equal = false;
  for (const int i : positions.index_range().drop_front(first_valid_index + 1).drop_back(1)) {
    tangents[i] = direction_bisect(positions[i], positions[i + 1], prev_delta, prev_equal);
  }

  if (is_cyclic) {
    const float3 &first = positions.first();
    tangents.last() = direction_bisect(positions.last(), first, prev_delta, prev_equal);
    tangents.first() = direction_bisect(first, positions[1], prev_delta, prev_equal);
  }
  else if (!delta_dir(positions.last(1), positions.last(), tangents.last())) {
    tangents.last() = prev_delta;
  }
}

void calculate_normals_z_up(const Span<float3> tangents, MutableSpan<float3> normals)
{
  BLI_assert(normals.size() == tangents.size());

  /* Same as in `vec_to_quat`. */
  const float epsilon = 1e-4f;
  for (const int i : normals.index_range()) {
    const float3 &tangent = tangents[i];
    if (std::abs(tangent.x) + std::abs(tangent.y) < epsilon) {
      normals[i] = {1.0f, 0.0f, 0.0f};
    }
    else {
      normals[i] = math::normalize(float3(tangent.y, -tangent.x, 0.0f));
    }
  }
}

/**
 * Rotate the last normal in the same way the tangent has been rotated.
 */
static float3 calculate_next_normal(const float3 &last_normal,
                                    const float3 &last_tangent,
                                    const float3 &current_tangent)
{
  if (math::is_zero(last_tangent) || math::is_zero(current_tangent)) {
    return last_normal;
  }
  const float angle = angle_normalized_v3v3(last_tangent, current_tangent);
  if (angle != 0.0f) {
    const float3 axis = math::normalize(math::cross(last_tangent, current_tangent));
    if (LIKELY(!math::is_zero(axis))) {
      /* The iterative process here (computing the current normal by rotating the previous one) can
       * accumulate small floating point errors, leading to 'not enough' normalized results at some
       * point (see #121169). */
      return math::normalize(math::rotate_direction_around_axis(last_normal, axis, angle));
    }
  }
  return last_normal;
}

void calculate_normals_minimum(const Span<float3> tangents,
                               const bool cyclic,
                               MutableSpan<float3> normals)
{
  BLI_assert(normals.size() == tangents.size());

  if (normals.is_empty()) {
    return;
  }

  const float epsilon = 1e-4f;

  /* Set initial normal. */
  const float3 &first_tangent = tangents.first();
  if (UNLIKELY(fabs(first_tangent.x) + fabs(first_tangent.y) < epsilon)) {
    normals.first() = {1.0f, 0.0f, 0.0f};
  }
  else {
    normals.first() = math::normalize(float3(first_tangent.y, -first_tangent.x, 0.0f));
  }

  /* Forward normal with minimum twist along the entire curve. */
  for (const int i : IndexRange(1, normals.size() - 1)) {
    normals[i] = calculate_next_normal(normals[i - 1], tangents[i - 1], tangents[i]);
  }

  if (!cyclic) {
    return;
  }

  /* Compute how much the first normal deviates from the normal that has been forwarded along the
   * entire cyclic curve. */
  const float3 uncorrected_last_normal = calculate_next_normal(
      normals.last(), tangents.last(), tangents.first());
  float correction_angle = angle_signed_on_axis_v3v3_v3(
      normals.first(), uncorrected_last_normal, tangents.first());
  if (correction_angle > M_PI) {
    correction_angle = correction_angle - 2 * M_PI;
  }

  /* Gradually apply correction by rotating all normals slightly around their tangents. */
  const float angle_step = correction_angle / normals.size();
  for (const int i : normals.index_range()) {
    const float3 axis = tangents[i];
    if (UNLIKELY(math::is_zero(axis))) {
      continue;
    }
    const float angle = angle_step * i;
    normals[i] = math::rotate_direction_around_axis(normals[i], axis, angle);
  }
}

}  // namespace blender::bke::curves::poly
