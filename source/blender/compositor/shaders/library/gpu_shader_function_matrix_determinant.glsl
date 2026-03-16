/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_function_matrix_determinant(const float4x4 matrix, float &output_determinant)
{
  output_determinant = determinant(matrix);
}
