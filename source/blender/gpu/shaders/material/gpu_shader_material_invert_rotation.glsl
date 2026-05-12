/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_quaternion_lib.glsl"

[[node]]
void invert_rotation(float4 rotation, out float4 result)
{
  result = quaternion_conjugate(Quaternion{UNPACK4(rotation)}).as_float4();
}
