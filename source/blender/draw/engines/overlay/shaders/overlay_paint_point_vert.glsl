/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_paint_info.hh"

VERTEX_SHADER_CREATE_INFO(overlay_paint_point)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  bool is_select = (nor.w > 0.0f);
  bool is_hidden = (nor.w < 0.0f);

  float3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);
  /* Add offset in Z to avoid Z-fighting and render selected wires on top. */
  /* TODO: scale this bias using Z-near and Z-far range. */
  gl_Position.z -= (is_select ? 2e-4f : 1e-4f);

  if (is_hidden) {
    gl_Position = float4(-2.0f, -2.0f, -2.0f, 1.0f);
  }

  finalColor = (is_select) ? float4(1.0f) : colorWire;
  finalColor.a = nor.w;

  gl_PointSize = sizeVertex * 2.0f;

  view_clipping_distances(world_pos);
}
