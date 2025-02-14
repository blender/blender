/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  vec3 final_pos = pos;

  float flip = (gl_InstanceID != 0) ? -1.0 : 1.0;

  if (gl_VertexID % 2 == 0) {
    final_pos += normalSize * rad * (flip * nor - tan);
  }

  vec3 world_pos = drw_point_object_to_world(final_pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  finalColor = colorWireEdit;

  view_clipping_distances(world_pos);
}
