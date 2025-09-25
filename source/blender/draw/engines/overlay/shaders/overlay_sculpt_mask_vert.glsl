/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_sculpt_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_sculpt_mask)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  float3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  faceset_color = mix(float3(1.0f), fset, face_sets_opacity);
  mask_color = 1.0f - (msk * mask_opacity);

  view_clipping_distances(world_pos);
}
