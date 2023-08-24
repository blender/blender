/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vec3 world_pos = point_object_to_world(vec3(au, 0.0));
  gl_Position = point_world_to_ndc(world_pos);

  finalColor = ((flag & FACE_UV_SELECT) != 0u) ? colorFaceDot : vec4(colorWire.rgb, 1.0);
  gl_PointSize = pointSize;
}
