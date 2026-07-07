/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_paint_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_paint_face)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  float3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);

#ifdef GPU_METAL
  /* Small bias to always be on top of the geom. */
  gl_Position.z -= 5e-5f;
#endif

  bool is_select = (paint_overlay_flag > 0);
  bool is_hidden = (paint_overlay_flag < 0);

  /* Don't draw faces that are selected. */
  if (is_hidden || is_select) {
    gl_Position = float4(-2.0f, -2.0f, -2.0f, 1.0f);
  }
  else {
    view_clipping_distances(world_pos);
  }
}
