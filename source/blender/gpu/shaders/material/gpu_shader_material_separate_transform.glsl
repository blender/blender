/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_quaternion_lib.glsl"
#include "gpu_shader_math_rotation_conversion_lib.glsl"

[[node]]
void separate_transform(float4x4 transform,
                        out float3 translation,
                        out float4 rotation,
                        out float3 scale)
{
  Quaternion quat;
  to_loc_rot_scale(transform, translation, quat, scale, false);
  rotation = quat.as_float4();
}
