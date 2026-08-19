/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_get_vector_component(float3 vector, int index, float &result)
{
  if (index >= 0 && index <= 2) {
    result = vector[index];
  }
  else {
    result = 0.0f;
  }
}

[[node]]
void node_vector_component_float(float3 vector, float index, float &result)
{
  node_vector_component(vector, int(index), result);
}
