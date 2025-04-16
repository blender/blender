/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_composite_normal(float3 input_vector,
                           float3 input_normal,
                           out float3 result_normal,
                           out float result_dot)
{
  float3 normal = normalize(input_normal);
  result_normal = normal;
  result_dot = -dot(input_vector, normal);
}
