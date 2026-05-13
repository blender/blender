/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_rotation_conversion_lib.glsl"
#include "gpu_shader_math_vector_compare_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

[[node]]
void axes_to_rotation(float3 primary_in,
                      float3 secondary_in,
                      float primary_idx_f,
                      float secondary_idx_f,
                      float tertiary_idx_f,
                      float tertiary_factor,
                      out float4 rotation)
{
  float3 primary = normalize(primary_in);
  float3 secondary = secondary_in;
  float3 tertiary;

  const bool primary_is_non_zero = !is_zero(primary);
  const bool secondary_is_non_zero = !is_zero(secondary);

  if (primary_is_non_zero && secondary_is_non_zero) {
    tertiary = cross(primary, secondary);
    if (is_zero(tertiary)) {
      tertiary = orthogonal<float3>(primary);
    }
    tertiary = normalize(tertiary);
    secondary = cross(tertiary, primary);
  }
  else if (primary_is_non_zero) {
    secondary = orthogonal<float3>(primary);
    secondary = normalize(secondary);
    tertiary = cross(primary, secondary);
  }
  else if (secondary_is_non_zero) {
    secondary = normalize(secondary);
    primary = orthogonal<float3>(secondary);
    primary = normalize(primary);
    tertiary = cross(primary, secondary);
  }
  else {
    rotation = float4(1.0f, 0.0f, 0.0f, 0.0f);
    return;
  }

  const int primary_axis = int(primary_idx_f);
  const int secondary_axis = int(secondary_idx_f);
  const int tertiary_axis = int(tertiary_idx_f);

  float3x3 mat;
  mat[primary_axis] = primary;
  mat[secondary_axis] = secondary;
  mat[tertiary_axis] = tertiary_factor * tertiary;

  rotation = to_quaternion(mat).as_float4();
}

[[node]]
void axes_to_rotation_identity(float3 primary_in, float3 secondary_in, out float4 rotation)
{
  rotation = float4(1.0f, 0.0f, 0.0f, 0.0f);
}
