/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * GPU computed length and intercept attribute.
 * One thread processes one curve.
 */

#pragma once

#include "gpu_shader_offset_indices_lib.glsl"

namespace curves {

struct EvalIntercept {
  [[storage(EVALUATED_POINT_SLOT, read)]] int (&evaluated_points_by_curve_buf)[];
  [[storage(EVALUATED_POS_RAD_SLOT, read)]] float4 (&evaluated_positions_radii_buf)[];
  [[storage(EVALUATED_TIME_SLOT, read_write)]] float (&evaluated_time_buf)[];
  [[storage(CURVES_LENGTH_SLOT, write)]] float (&curves_length_buf)[];

  [[push_constant]] int curves_start;
  [[push_constant]] int curves_count;
  [[push_constant]] bool use_cyclic;

  /* Run on the evaluated position and compute the intercept time with the curve and the total
   * curve length. */
  void evaluate_length_and_time(const IndexRange evaluated_points, const int curve_index)
  {
    float distance_along_curve = 0.0f;
    evaluated_time_buf[evaluated_points.first()] = 0.0f;
    for (int i = 1; i < evaluated_points.size(); i++) {
      int p = evaluated_points.start() + i;
      distance_along_curve += distance(evaluated_positions_radii_buf[p].xyz,
                                       evaluated_positions_radii_buf[p - 1].xyz);
      evaluated_time_buf[p] = distance_along_curve;
    }
    for (int i = 1; i < evaluated_points.size(); i++) {
      int p = evaluated_points.start() + i;
      evaluated_time_buf[p] /= distance_along_curve;
    }
    curves_length_buf[curve_index] = distance_along_curve;
  }
};

[[compute, local_size(CURVES_PER_THREADGROUP)]] void eval_intercept(
    [[resource_table]] EvalIntercept &srt, [[global_invocation_id]] const uint3 global_id)
{
  if (global_id.x >= uint(srt.curves_count)) {
    return;
  }
  int curve_index = int(global_id.x) + srt.curves_start;

  IndexRange evaluated_points = offset_indices::load_range_from_buffer(
      srt.evaluated_points_by_curve_buf, curve_index);

  if (srt.use_cyclic) {
    evaluated_points = IndexRange{evaluated_points.start() + curve_index,
                                  evaluated_points.size() + 1};
  }

  srt.evaluate_length_and_time(evaluated_points, curve_index);
}

}  // namespace curves

PipelineCompute draw_curves_evaluate_length_intercept(curves::eval_intercept);
