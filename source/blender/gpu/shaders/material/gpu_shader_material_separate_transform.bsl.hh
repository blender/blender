/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_quaternion.bsl.hh"
#include "gpu_shader_math_rotation_conversion.bsl.hh"

[[node]]
void separate_transform(float4x4 transform, float3 &translation, float4 &rotation, float3 &scale)
{
  Quaternion quat;
  to_loc_rot_scale(transform, translation, quat, scale, false);
  rotation = quat.as_float4();
}
