/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_rotation_conversion_lib.glsl"

[[node]]
void rotation_to_axis_angle(float4 rotation, out float3 axis, out float angle)
{
  const AxisAngle aa = to_axis_angle(Quaternion{UNPACK4(rotation)});
  axis = aa.axis;
  angle = aa.angle;
}
