/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_depth_gpencil)

#include "draw_grease_pencil_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "select_lib.glsl"

void main()
{
  float3 world_pos;
  float3 unused_N;
  float4 unused_color;
  float unused_strength;
  float2 unused_uv;

  gl_Position = gpencil_vertex(float4(uniform_buf.size_viewport, uniform_buf.size_viewport_inv),
                               world_pos,
                               unused_N,
                               unused_color,
                               unused_strength,
                               unused_uv,
                               gp_interp_flat.sspos,
                               gp_interp_flat.sspos_adj,
                               gp_interp_flat.aspect,
                               gp_interp_noperspective.thickness,
                               gp_interp_noperspective.hardness);

  /* Small bias to always be on top of the geom. */
  gl_Position.z -= 1e-3f;

  view_clipping_distances(world_pos);

  select_id_set(drw_custom_id());
}
