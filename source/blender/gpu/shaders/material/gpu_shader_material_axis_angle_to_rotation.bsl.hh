/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_rotation_conversion.bsl.hh"

[[node]]
void axis_angle_to_rotation(float3 axis, float angle, float4 &rotation)
{
  rotation = to_quaternion(axis, angle).as_float4();
}
