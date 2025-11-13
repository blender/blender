/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Custom full-screen triangle with placeholders varyings.
 */

#include "infos/eevee_geom_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_geom_world)

#include "draw_view_lib.glsl"
#include "eevee_reverse_z_lib.glsl"

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

#ifdef MAT_SHADOW
  /* This shader currently does not support shadow. But the shader validation pipeline still
   * compiles the shadow variant of this shader. Avoid linking error on Intel Windows drivers. */
#  ifdef SHADOW_UPDATE_ATOMIC_RASTER
  shadow_iface.shadow_view_id = 0;
#  endif
  shadow_clip.position = float3(0);
  shadow_clip.vector = float3(0);
#endif
}
