/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_matrix_construct_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

[[node]]
void combine_transform(float3 translation, float4 rotation, float3 scale, out float4x4 transform)
{
  transform = from_loc_rot_scale(translation, Quaternion{UNPACK4(rotation)}, scale);
}
