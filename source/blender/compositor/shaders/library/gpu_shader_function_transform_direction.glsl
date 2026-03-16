/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_matrix_transform_lib.glsl"

[[node]]
void node_function_transform_direction(const float3 direction,
                                       const float4x4 transform,
                                       float3 &transformed_direction)
{
  transformed_direction = transform_direction(transform, direction);
}
