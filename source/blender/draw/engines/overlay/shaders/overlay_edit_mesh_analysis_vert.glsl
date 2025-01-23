/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

vec3 weight_to_rgb(float t)
{
  if (t < 0.0) {
    /* Minimum color, gray */
    return vec3(0.25, 0.25, 0.25);
  }
  else if (t > 1.0) {
    /* Error color. */
    return vec3(1.0, 0.0, 1.0);
  }
  else {
    return texture(weightTex, t).rgb;
  }
}

void main()
{
  vec3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);
  weightColor = vec4(weight_to_rgb(weight), 1.0);

  view_clipping_distances(world_pos);
}
