/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  vec3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  faceset_color = mix(vec3(1.0), fset, faceSetsOpacity);
  mask_color = 1.0 - (msk * maskOpacity);

  view_clipping_distances(world_pos);
}
