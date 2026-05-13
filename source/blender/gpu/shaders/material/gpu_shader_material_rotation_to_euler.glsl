/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_matrix_construct_lib.glsl"
#include "gpu_shader_math_rotation_conversion_lib.glsl"

[[node]]
void rotation_to_euler(float4 rotation, out float3 euler)
{
  Quaternion quat = Quaternion{UNPACK4(rotation)};
  euler = to_euler(from_rotation(quat)).as_float3();
}
