/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_matrix_transform_lib.glsl"

[[node]]
void node_function_transform_point(const float3 point,
                                   const float4x4 transform,
                                   float3 &transformed_point)
{
  transformed_point = transform_point(transform, point);
}
