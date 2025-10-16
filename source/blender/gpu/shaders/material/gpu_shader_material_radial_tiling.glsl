/* SPDX-FileCopyrightText: 2024-2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_constants_lib.glsl"

/* Define macro flags for code adaption. */
/* No macro flags necessary, as code is adapted to GLSL by default. */

/* The rounded polygon calculation functions are defined in
 * gpu_shader_material_radial_tiling_shared.glsl. */
#include "gpu_shader_material_radial_tiling_shared.glsl"

/* Undefine macro flags used for code adaption. */
/* No macro flags necessary, as code is adapted to GLSL by default. */

void node_radial_tiling(float2 coord,
                        float r_gon_sides,
                        float r_gon_roundness,
                        float normalize_r_gon_parameter,
                        float calculate_r_gon_parameter_field,
                        float calculate_segment_id,
                        float calculate_max_unit_parameter,
                        float calculate_x_axis_A_angle_bisector,
                        out float3 out_segment_coordinates,
                        out float out_segment_id,
                        out float out_max_unit_parameter,
                        out float out_x_axis_A_angle_bisector)
{
  if (bool(calculate_r_gon_parameter_field) || bool(calculate_max_unit_parameter) ||
      bool(calculate_x_axis_A_angle_bisector))
  {
    float4 out_variables = calculate_out_variables(bool(calculate_r_gon_parameter_field),
                                                   bool(calculate_max_unit_parameter),
                                                   bool(normalize_r_gon_parameter),
                                                   max(r_gon_sides, 2.0),
                                                   clamp(r_gon_roundness, 0.0, 1.0),
                                                   float2(coord.x, coord.y));

    out_segment_coordinates = float3(out_variables.y, out_variables.x, 0.0);
    out_max_unit_parameter = out_variables.z;
    out_x_axis_A_angle_bisector = out_variables.w;
  }

  if (bool(calculate_segment_id)) {
    out_segment_id = calculate_out_segment_id(max(r_gon_sides, 2.0), float2(coord.x, coord.y));
  }
}
