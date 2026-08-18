/* SPDX-FileCopyrightText: 2024-2025 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* Define macro flags for code adaption. */
#define ADAPT_TO_SVM

/* The rounded polygon calculation functions are defined in radial_tiling_shared.h. */
#include "radial_tiling_shared.h"

/* Undefine macro flags used for code adaption. */
#undef ADAPT_TO_SVM

template<uint64_t node_feature_mask>
ccl_device_noinline void svm_node_radial_tiling(
    ccl_private float *ccl_restrict stack, const ccl_global SVMNodeRadialTiling &ccl_restrict node)
{
  const bool calculate_r_gon_parameter_field = stack_valid(node.segment_coordinates_offset);
  const bool calculate_segment_id = stack_valid(node.segment_id_offset);
  const bool calculate_max_unit_parameter = stack_valid(node.max_unit_parameter_offset);
  const bool calculate_x_axis_A_angle_bisector = stack_valid(node.x_axis_A_angle_bisector_offset);

  const float3 coord = stack_load(stack, node.vector);
  const float r_gon_sides = stack_load(stack, node.r_gon_sides);
  const float r_gon_roundness = stack_load(stack, node.r_gon_roundness);

  if (calculate_r_gon_parameter_field || calculate_max_unit_parameter ||
      calculate_x_axis_A_angle_bisector)
  {
    float4 out_variables = calculate_out_variables(calculate_r_gon_parameter_field,
                                                   calculate_max_unit_parameter,
                                                   node.normalize_r_gon_parameter,
                                                   fmaxf(r_gon_sides, 2.0f),
                                                   clamp(r_gon_roundness, 0.0f, 1.0f),
                                                   make_float2(coord.x, coord.y));

    if (calculate_r_gon_parameter_field) {
      stack_store_float3(stack,
                         node.segment_coordinates_offset,
                         make_float3(out_variables.y, out_variables.x, 0.0f));
    }
    if (calculate_max_unit_parameter) {
      stack_store_float(stack, node.max_unit_parameter_offset, out_variables.z);
    }
    if (calculate_x_axis_A_angle_bisector) {
      stack_store_float(stack, node.x_axis_A_angle_bisector_offset, out_variables.w);
    }
  }

  if (calculate_segment_id) {
    stack_store_float(
        stack,
        node.segment_id_offset,
        calculate_out_segment_id(fmaxf(r_gon_sides, 2.0f), make_float2(coord.x, coord.y)));
  }
}

CCL_NAMESPACE_END
