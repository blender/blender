/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_matrix_construct_lib.glsl"

[[node]]
void node_function_invert_matrix(const float4x4 matrix,
                                 float4x4 &inverted_matrix,
                                 float &is_invertable)
{
  const bool is_singular = determinant(matrix) == 0.0f;
  inverted_matrix = is_singular ? mat4x4_identity() : inverse(matrix);
  is_invertable = float(!is_singular);
}
