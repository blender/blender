/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_view_lib.glsl"

void main()
{
  /* `pos` contains the coordinates of a quad (-1..1). but we need the coordinates of an image
   * plane (0..1) */
  vec3 image_pos = pos * 0.5 + 0.5;
  gl_Position = drw_point_world_to_homogenous(
      vec3(image_pos.xy * brush_scale + brush_offset, 0.0));
  uvs = image_pos.xy;
}
