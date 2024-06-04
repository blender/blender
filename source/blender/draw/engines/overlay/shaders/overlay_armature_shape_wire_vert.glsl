/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vec4 bone_color, state_color;
  mat4 model_mat = extract_matrix_packed_data(inst_obmat, state_color, bone_color);

  vec3 world_pos = (model_mat * vec4(pos, 1.0)).xyz;
  gl_Position = point_world_to_ndc(world_pos);

  geometry_in.finalColor.rgb = mix(state_color.rgb, bone_color.rgb, 0.5);
  geometry_in.finalColor.a = 1.0;
  /* Because the packing clamps the value, the wire width is passed in compressed. */
  geometry_in.wire_width = bone_color.a * WIRE_WIDTH_COMPRESSION;

  view_clipping_distances(world_pos);
}
