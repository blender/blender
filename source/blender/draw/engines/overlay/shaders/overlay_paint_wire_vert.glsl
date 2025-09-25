/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_paint_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_paint_wire)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  bool is_select = (paint_overlay_flag > 0) && use_select;
  bool is_hidden = (paint_overlay_flag < 0) && use_select;

  float3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);
  /* Add offset in Z to avoid Z-fighting and render selected wires on top. */
  /* TODO: scale this bias using Z-near and Z-far range. */
  gl_Position.z -= (is_select ? 2e-4f : 1e-4f);

  if (is_hidden) {
    gl_Position = float4(-2.0f, -2.0f, -2.0f, 1.0f);
  }

  constexpr float4 colSel = float4(1.0f);

  final_color = (is_select) ? colSel : theme.colors.wire;

  /* Weight paint needs a light color to contrasts with dark weights. */
  if (!use_select) {
    final_color = float4(1.0f, 1.0f, 1.0f, 0.3f);
  }

  view_clipping_distances(world_pos);
}
