/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_matrix_lib.glsl"
#include "gpu_shader_math_rotation_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

void mapping_mat4(float3 vec,
                  float4 m0,
                  float4 m1,
                  float4 m2,
                  float4 m3,
                  float3 minvec,
                  float3 maxvec,
                  out float3 outvec)
{
  float4x4 mat = float4x4(m0, m1, m2, m3);
  outvec = (mat * float4(vec, 1.0f)).xyz;
  outvec = clamp(outvec, minvec, maxvec);
}

void mapping_point(
    float3 vector, float3 location, float3 rotation, float3 scale, out float3 result)
{
  result = (from_rotation(as_EulerXYZ(rotation)) * (vector * scale)) + location;
}

void mapping_texture(
    float3 vector, float3 location, float3 rotation, float3 scale, out float3 result)
{
  result = safe_divide(transpose(from_rotation(as_EulerXYZ(rotation))) * (vector - location),
                       scale);
}

void mapping_vector(
    float3 vector, float3 location, float3 rotation, float3 scale, out float3 result)
{
  result = from_rotation(as_EulerXYZ(rotation)) * (vector * scale);
}

void mapping_normal(
    float3 vector, float3 location, float3 rotation, float3 scale, out float3 result)
{
  result = normalize(from_rotation(as_EulerXYZ(rotation)) * safe_divide(vector, scale));
}
