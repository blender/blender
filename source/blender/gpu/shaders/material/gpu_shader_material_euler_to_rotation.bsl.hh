/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_euler_lib.glsl"
#include "gpu_shader_math_rotation_conversion_lib.glsl"

[[node]]
void euler_to_rotation(float3 euler, float4 &rotation)
{
  rotation = to_quaternion(EulerXYZ::from_float3(euler)).as_float4();
}
