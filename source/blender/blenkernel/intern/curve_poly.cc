/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>

#include "BLI_math_rotation_legacy.hh"
#include "BLI_math_vector.hh"

#include "BKE_curves.hh"

namespace blender::bke::curves::poly {

static float3 direction_bisect(const float3 &prev,
                               const float3 &middle,
                               const float3 &next,
                               bool &r_used_fallback)
{
  const float epsilon = 1e-6f;
  const bool prev_equal = math::almost_equal_relative(prev, middle, epsilon);
  const bool next_equal = math::almost_equal_relative(middle, next, epsilon);
  if (prev_equal && next_equal) {
    r_used_fallback = true;
    return {0.0f, 0.0f, 0.0f};
  }
  if (prev_equal) {
    return math::normalize(next - middle);
  }
  if (next_equal) {
    return math::normalize(middle - prev);
  }

  const float3 dir_prev = math::normalize(middle - prev);
  const float3 dir_next = math::normalize(next - middle);
  const float3 result = math::normalize(dir_prev + dir_next);
  return result;
}

void calculate_tangents(const Span<float3> positions,
                        const bool is_cyclic,
                        MutableSpan<float3> tangents)
{
  BLI_assert(positions.size() == tangents.size());

  if (positions.size() == 1) {
    tangents.first() = float3(0.0f, 0.0f, 1.0f);
    return;
  }

  bool used_fallback = false;

  for (const int i : IndexRange(1, positions.size() - 2)) {
    tangents[i] = direction_bisect(
        positions[i - 1], positions[i], positions[i + 1], used_fallback);
  }

  if (is_cyclic) {
    const float3 &second_to_last = positions[positions.size() - 2];
    const float3 &last = positions.last();
    const float3 &first = positions.first();
    const float3 &second = positions[1];
    tangents.first() = direction_bisect(last, first, second, used_fallback);
    tangents.last() = direction_bisect(second_to_last, last, first, used_fallback);
  }
  else {
    const float epsilon = 1e-6f;
    if (math::almost_equal_relative(positions[0], positions[1], epsilon)) {
      tangents.first() = {0.0f, 0.0f, 0.0f};
      used_fallback = true;
    }
    else {
      tangents.first() = math::normalize(positions[1] - positions[0]);
    }
    if (math::almost_equal_relative(positions.last(0), positions.last(1), epsilon)) {
      tangents.last() = {0.0f, 0.0f, 0.0f};
      used_fallback = true;
    }
    else {
      tangents.last() = math::normalize(positions.last(0) - positions.last(1));
    }
  }

  if (!used_fallback) {
    return;
  }

  /* Find the first tangent that does not use the fallback. */
  int first_valid_tangent_index = -1;
  for (const int i : tangents.index_range()) {
    if (!math::is_zero(tangents[i])) {
      first_valid_tangent_index = i;
      break;
    }
  }
  if (first_valid_tangent_index == -1) {
    /* If all tangents used the fallback, it means that all positions are (almost) the same. Just
     * use the up-vector as default tangent. */
    const float3 up_vector{0.0f, 0.0f, 1.0f};
    tangents.fill(up_vector);
  }
  else {
    const float3 &first_valid_tangent = tangents[first_valid_tangent_index];
    /* If the first few tangents are invalid, use the tangent from the first point with a valid
     * tangent. */
    tangents.take_front(first_valid_tangent_index).fill(first_valid_tangent);
    /* Use the previous valid tangent for points that had no valid tangent. */
    for (const int i : tangents.index_range().drop_front(first_valid_tangent_index + 1)) {
      float3 &tangent = tangents[i];
      if (math::is_zero(tangent)) {
        const float3 &prev_tangent = tangents[i - 1];
        tangent = prev_tangent;
      }
    }
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
  if (angle != 0.0) {
    const float3 axis = math::normalize(math::cross(last_tangent, current_tangent));
    return math::rotate_direction_around_axis(last_normal, axis, angle);
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
  if (fabs(first_tangent.x) + fabs(first_tangent.y) < epsilon) {
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

  /* Gradually apply correction by rotating all normals slightly. */
  const float angle_step = correction_angle / normals.size();
  for (const int i : normals.index_range()) {
    const float angle = angle_step * i;
    normals[i] = math::rotate_direction_around_axis(normals[i], tangents[i], angle);
  }
}

}  // namespace blender::bke::curves::poly
