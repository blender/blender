/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vec4 bone_color, state_color;
  mat4 model_mat = extract_matrix_packed_data(inst_obmat, state_color, bone_color);

  /* This is slow and run per vertex, but it's still faster than
   * doing it per instance on CPU and sending it on via instance attribute. */
  mat3 normal_mat = transpose(inverse(mat3(model_mat)));
  vec3 normal = normalize(normal_world_to_view(normal_mat * nor));

  inverted = int(dot(cross(model_mat[0].xyz, model_mat[1].xyz), model_mat[2].xyz) < 0.0);

  /* Do lighting at an angle to avoid flat shading on front facing bone. */
  const vec3 light = vec3(0.1, 0.1, 0.8);
  float n = dot(normal, light);

  /* Smooth lighting factor. */
  const float s = 0.2; /* [0.0-0.5] range */
  float fac = clamp((n * (1.0 - s)) + s, 0.0, 1.0);
  finalColor.rgb = mix(state_color.rgb, bone_color.rgb, fac * fac);
  finalColor.a = 1.0;

  vec4 world_pos = model_mat * vec4(pos, 1.0);
  gl_Position = drw_view.winmat * (drw_view.viewmat * world_pos);

  view_clipping_distances(world_pos.xyz);
}
