/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_constants_lib.glsl"
#include "gpu_shader_math_rotation_conversion_lib.glsl"
#include "gpu_shader_math_rotation_lib.glsl"
#include "gpu_shader_math_vector_compare_lib.glsl"

float angle_normalized_v3v3(float3 v1, float3 v2)
{
  v1 = normalize(v1);
  v2 = normalize(v2);
  if (dot(v1, v2) >= 0.0f) {
    return 2.0f * asin(clamp(length(v2 - v1) / 2.0f, -1.0f, 1.0f));
  }
  const float3 v2_n = -v2;
  return M_PI - 2.0f * asin(clamp(length(v2_n - v1) / 2.0f, -1.0f, 1.0f));
}

float3 project_plane_normalized_v3_v3v3(float3 p, float3 v_plane)
{
  const float3 v_plane_n = normalize(v_plane);
  const float mul = dot(p, v_plane_n);
  return p - v_plane_n * mul;
}

float angle_signed_on_axis_v3v3_v3(float3 v1, float3 v2, float3 axis)
{
  const float3 axis_n = normalize(axis);
  const float3 v1_proj = project_plane_normalized_v3_v3v3(v1, axis_n);
  const float3 v2_proj = project_plane_normalized_v3_v3v3(v2, axis_n);

  float angle = angle_normalized_v3v3(v1_proj, v2_proj);

  const float3 tproj = cross(v2_proj, v1_proj);
  if (dot(tproj, axis) < 0.0f) {
    angle = M_PI * 2.0f - angle;
  }
  return angle;
}

[[node]]
void align_rotation_to_vector_auto_pivot(float4 rotation_in,
                                         float factor,
                                         float3 input_vector,
                                         float3 local_main_axis,
                                         out float4 rotation)
{
  if (is_zero(input_vector)) {
    rotation = rotation_in;
    return;
  }

  const Quaternion quat_in = Quaternion{UNPACK4(rotation_in)};
  const float3 old_axis = transform_point_by_quaternion(quat_in, local_main_axis);
  const float3 new_axis = normalize(input_vector);

  float3 rotation_axis = cross(old_axis, new_axis);
  if (is_zero(rotation_axis)) {
    /* The vectors are linearly dependent, so we fall back to another axis. */
    rotation_axis = cross(old_axis, float3(1.0f, 0.0f, 0.0f));
    if (is_zero(rotation_axis)) {
      /* This is now guaranteed to not be zero. */
      rotation_axis = cross(old_axis, float3(0.0f, 1.0f, 0.0f));
    }
  }

  const float full_angle = angle_normalized_v3v3(old_axis, new_axis);
  const float angle = factor * full_angle;

  AxisAngle aa;
  aa.axis = normalize(rotation_axis);
  aa.angle = angle;

  rotation = math_quaternion_multiply(to_quaternion(aa), quat_in).as_float4();
}

[[node]]
void align_rotation_to_vector_fixed_pivot(float4 rotation_in,
                                          float factor,
                                          float3 input_vector,
                                          float3 local_main_axis,
                                          float3 local_pivot_axis,
                                          out float4 rotation)
{
  if (all(equal(local_main_axis, local_pivot_axis))) {
    /* Can't compute any meaningful rotation angle in this case. */
    rotation = rotation_in;
    return;
  }
  if (is_zero(input_vector)) {
    rotation = rotation_in;
    return;
  }

  const Quaternion quat_in = Quaternion{UNPACK4(rotation_in)};
  const float3 old_axis = transform_point_by_quaternion(quat_in, local_main_axis);
  const float3 pivot_axis = transform_point_by_quaternion(quat_in, local_pivot_axis);

  float full_angle = angle_signed_on_axis_v3v3_v3(input_vector, old_axis, pivot_axis);
  if (full_angle > M_PI) {
    /* Make sure the point is rotated as little as possible. */
    full_angle -= 2.0f * M_PI;
  }

  const float angle = factor * full_angle;

  AxisAngle aa;
  aa.axis = normalize(pivot_axis);
  aa.angle = angle;

  rotation = math_quaternion_multiply(to_quaternion(aa), quat_in).as_float4();
}
