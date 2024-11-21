/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_lib.glsl"

void main()
{
  /* `pos` contains the coordinates of a quad (-1..1). but we need the coordinates of an image
   * plane (0..1) */
  vec3 image_pos = pos * 0.5 + 0.5;
#ifdef OVERLAY_NEXT
  gl_Position = point_world_to_ndc(vec3(image_pos.xy * brush_scale + brush_offset, 0.0));
#else
  gl_Position = point_object_to_ndc(image_pos);
#endif
  uvs = image_pos.xy;
}
