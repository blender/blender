/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_material_transform_utils.glsl"

void node_displacement_object(float height, float midlevel, float scale, vec3 N, out vec3 result)
{
  vec3 lN;
  direction_transform_world_to_object(N, lN);
  vec3 l_displacement = (height - midlevel) * scale * normalize(lN);
  /* Apply object scale and orientation. */
  direction_transform_object_to_world(l_displacement, result);
}

void node_displacement_world(float height, float midlevel, float scale, vec3 N, out vec3 result)
{
  result = (height - midlevel) * scale * normalize(N);
}
