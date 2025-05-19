/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Custom full-screen triangle with placeholders varyings.
 */

#include "infos/eevee_material_info.hh"

VERTEX_SHADER_CREATE_INFO(eevee_geom_world)

#include "draw_view_lib.glsl"
#include "eevee_nodetree_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "eevee_surf_lib.glsl"

void main()
{
  /* (W)Intel drivers require all varying iface to be written to inside the Vertex shader. */
  drw_ResourceID_iface.resource_index = 0u;

  /* Full-screen triangle. */
  int v = gl_VertexID % 3;
  float x = float((v & 1) << 2) - 1.0f;
  float y = float((v & 2) << 1) - 1.0f;
  gl_Position = float4(x, y, 1.0f, 1.0f);

  /* Pass view position to keep accuracy. */
  interp.P = drw_point_ndc_to_view(gl_Position.xyz);
  interp.N = float3(1);

  gl_Position = reverse_z::transform(gl_Position);
}
