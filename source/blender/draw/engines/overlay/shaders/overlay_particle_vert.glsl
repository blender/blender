/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "select_lib.glsl"

void main()
{
  select_id_set(drw_CustomID);

  /* Draw-size packed in alpha. */
  float draw_size = ucolor.a;

  vec3 world_pos = part_pos;

  gl_Position = drw_point_world_to_homogenous(world_pos);
  /* World sized points. */
  gl_PointSize = sizePixel * draw_size * drw_view.winmat[1][1] * sizeViewport.y / gl_Position.w;

  /* Coloring */
  if (part_val < 0.0) {
    finalColor = vec4(ucolor.rgb, 1.0);
  }
  else {
    finalColor = vec4(texture(weightTex, part_val).rgb, 1.0);
  }

  view_clipping_distances(world_pos);
}
