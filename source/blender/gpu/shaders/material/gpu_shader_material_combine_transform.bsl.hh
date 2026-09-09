/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_matrix_construct.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"

[[node]]
void combine_transform(float3 translation, float4 rotation, float3 scale, float4x4 &transform)
{
  transform = from_loc_rot_scale(translation, Quaternion{UNPACK4(rotation)}, scale);
}
