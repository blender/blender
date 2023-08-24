/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_displacement_object(float height, float midlevel, float scale, vec3 N, out vec3 result)
{
  N = transform_direction(ModelMatrixInverse, N);
  result = (height - midlevel) * scale * normalize(N);
  /* Apply object scale and orientation. */
  result = transform_direction(ModelMatrix, result);
}

void node_displacement_world(float height, float midlevel, float scale, vec3 N, out vec3 result)
{
  result = (height - midlevel) * scale * normalize(N);
}
