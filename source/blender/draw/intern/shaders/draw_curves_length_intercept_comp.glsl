/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * GPU computed length and intercept attribute.
 * One thread processes one curve.
 */

#include "draw_curves_infos.hh"

COMPUTE_SHADER_CREATE_INFO(draw_curves_evaluate_length_intercept)

#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_offset_indices_lib.glsl"

/* Run on the evaluated position and compute the intercept time with the curve and the total curve
 * length. */
void evaluate_length_and_time(const IndexRange evaluated_points, const int curve_index)
{
  const auto &evaluated_positions_radii = buffer_get(draw_curves_evaluate_length_intercept,
                                                     evaluated_positions_radii_buf);
  auto &evaluated_time = buffer_get(draw_curves_evaluate_length_intercept, evaluated_time_buf);
  auto &curves_length = buffer_get(draw_curves_evaluate_length_intercept, curves_length_buf);

  float distance_along_curve = 0.0f;
  evaluated_time[evaluated_points.first()] = 0.0f;
  for (int i = 1; i < evaluated_points.size(); i++) {
    int p = evaluated_points.start() + i;
    distance_along_curve += distance(evaluated_positions_radii[p].xyz,
                                     evaluated_positions_radii[p - 1].xyz);
    evaluated_time[p] = distance_along_curve;
  }
  for (int i = 1; i < evaluated_points.size(); i++) {
    int p = evaluated_points.start() + i;
    evaluated_time[p] /= distance_along_curve;
  }
  curves_length[curve_index] = distance_along_curve;
}

void evaluate_length_intercept()
{
  if (gl_GlobalInvocationID.x >= uint(curves_count)) {
    return;
  }
  int curve_index = int(gl_GlobalInvocationID.x) + curves_start;

  IndexRange evaluated_points = offset_indices::load_range_from_buffer(
      evaluated_points_by_curve_buf, curve_index);

  if (use_cyclic) {
    evaluated_points = IndexRange(evaluated_points.start() + curve_index,
                                  evaluated_points.size() + 1);
  }

  evaluate_length_and_time(evaluated_points, curve_index);
}
