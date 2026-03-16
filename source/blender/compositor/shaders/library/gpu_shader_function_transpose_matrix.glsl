/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_function_transpose_matrix(const float4x4 matrix, float4x4 &transposed_matrix)
{
  transposed_matrix = transpose(matrix);
}
