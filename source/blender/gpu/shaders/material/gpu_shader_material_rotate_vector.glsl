/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_quaternion_lib.glsl"

[[node]]
void rotate_vector(float3 vector, float4 rotation, out float3 result)
{
  result = transform_point_by_quaternion(Quaternion{UNPACK4(rotation)}, vector);
}
