/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_quaternion.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"

[[node]]
void rotate_rotation_global(float4 rotation, float4 rotate_by, float4 &result)
{
  result = math_quaternion_multiply(Quaternion::from_float4(rotate_by),
                                    Quaternion::from_float4(rotation))
               .as_float4();
}

[[node]]
void rotate_rotation_local(float4 rotation, float4 rotate_by, float4 &result)
{
  result = math_quaternion_multiply(Quaternion::from_float4(rotation),
                                    Quaternion::from_float4(rotate_by))
               .as_float4();
}
