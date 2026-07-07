/* SPDX-FileCopyrightText: 2024-2025 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Define macro flags for code adaption. */
#define ADAPT_TO_SVM

/* The rounded polygon calculation functions are defined in radial_tiling_shared.h. */
#include "radial_tiling_shared.h"

/* Undefine macro flags used for code adaption. */
#undef ADAPT_TO_SVM

struct RoundedPolygonStackOffsets {
  uint vector;
  uint r_gon_sides;
  uint r_gon_roundness;
  uint segment_coordinates;
  uint segment_id;
  uint max_unit_parameter;
  uint x_axis_A_angle_bisector;
};

template<uint node_feature_mask>
ccl_device_noinline int svm_node_radial_tiling(ccl_private float *stack, uint4 node, int offset)
{
  RoundedPolygonStackOffsets so;

  uint normalize_r_gon_parameter = node.y;

  svm_unpack_node_uchar4(
      node.z, &(so.vector), &(so.r_gon_sides), &(so.r_gon_roundness), &(so.segment_coordinates));
  svm_unpack_node_uchar3(
      node.w, &(so.segment_id), &(so.max_unit_parameter), &(so.x_axis_A_angle_bisector));

  bool calculate_r_gon_parameter_field = stack_valid(so.segment_coordinates);
  bool calculate_segment_id = stack_valid(so.segment_id);
  bool calculate_max_unit_parameter = stack_valid(so.max_unit_parameter);
  bool calculate_x_axis_A_angle_bisector = stack_valid(so.x_axis_A_angle_bisector);

  float3 coord = stack_load_float3(stack, so.vector);
  float r_gon_sides = stack_load_float(stack, so.r_gon_sides);
  float r_gon_roundness = stack_load_float(stack, so.r_gon_roundness);

  if (calculate_r_gon_parameter_field || calculate_max_unit_parameter ||
      calculate_x_axis_A_angle_bisector)
  {
    float4 out_variables = calculate_out_variables(calculate_r_gon_parameter_field,
                                                   calculate_max_unit_parameter,
                                                   normalize_r_gon_parameter,
                                                   fmaxf(r_gon_sides, 2.0f),
                                                   clamp(r_gon_roundness, 0.0f, 1.0f),
                                                   make_float2(coord.x, coord.y));

    if (calculate_r_gon_parameter_field) {
      stack_store_float3(
          stack, so.segment_coordinates, make_float3(out_variables.y, out_variables.x, 0.0f));
    }
    if (calculate_max_unit_parameter) {
      stack_store_float(stack, so.max_unit_parameter, out_variables.z);
    }
    if (calculate_x_axis_A_angle_bisector) {
      stack_store_float(stack, so.x_axis_A_angle_bisector, out_variables.w);
    }
  }

  if (calculate_segment_id) {
    stack_store_float(
        stack,
        so.segment_id,
        calculate_out_segment_id(fmaxf(r_gon_sides, 2.0f), make_float2(coord.x, coord.y)));
  }

  return offset;
}

CCL_NAMESPACE_END
