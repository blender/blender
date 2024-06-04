/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_material_transform_utils.glsl)

void node_vector_displacement_tangent(
    vec4 vector, float midlevel, float scale, vec4 T, out vec3 result)
{
  vec3 oN, oT, oB;
  normal_transform_world_to_object(g_data.N, oN);
  normal_transform_world_to_object(T.xyz, oT);
  oN = normalize(oN);
  oT = normalize(oT);
  oB = T.w * safe_normalize(cross(oN, oT));

  vec3 disp = (vector.xyz - midlevel) * scale;
  disp = disp.x * oT + disp.y * oN + disp.z * oB;
  direction_transform_object_to_world(disp, result);
}

void node_vector_displacement_object(vec4 vector, float midlevel, float scale, out vec3 result)
{
  vec3 disp = (vector.xyz - midlevel) * scale;
  direction_transform_object_to_world(disp, result);
}

void node_vector_displacement_world(vec4 vector, float midlevel, float scale, out vec3 result)
{
  result = (vector.xyz - midlevel) * scale;
}
