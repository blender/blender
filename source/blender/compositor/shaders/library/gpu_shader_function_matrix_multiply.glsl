/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_function_matrix_multiply(const float4x4 a, const float4x4 b, float4x4 &result)
{
  result = a * b;
}
