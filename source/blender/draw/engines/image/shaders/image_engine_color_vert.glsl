/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vec3 image_pos = vec3(pos.x, pos.y, 0.0);
  uv_screen = image_pos.xy;

  vec4 position = point_world_to_ndc(image_pos);
  gl_Position = position;
}
