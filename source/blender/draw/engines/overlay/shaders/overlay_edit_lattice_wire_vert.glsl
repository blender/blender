/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_lattice_wire)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

#define no_active_weight 666.0f

float3 weight_to_rgb(float t)
{
  if (t == no_active_weight) {
    /* No weight. */
    return theme.colors.wire.rgb;
  }
  if (t > 1.0f || t < 0.0f) {
    /* Error color */
    return float3(1.0f, 0.0f, 1.0f);
  }
  else {
    return texture(weight_tx, t).rgb;
  }
}

void main()
{
  final_color = float4(weight_to_rgb(weight), 1.0f);

  float3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  view_clipping_distances(world_pos);
}
