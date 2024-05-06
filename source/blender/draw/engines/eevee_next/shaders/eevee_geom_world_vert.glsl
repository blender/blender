/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Custom full-screen triangle with placeholders varyings.
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)

void main()
{
  /* (W)Intel drivers require all varying iface to be written to inside the Vertex shader. */
  drw_ResourceID_iface.resource_index = 0;

  /* Full-screen triangle. */
  int v = gl_VertexID % 3;
  float x = float((v & 1) << 2) - 1.0;
  float y = float((v & 2) << 1) - 1.0;
  gl_Position = vec4(x, y, 1.0, 1.0);

  /* Pass view position to keep accuracy. */
  interp.P = drw_point_ndc_to_view(gl_Position.xyz);
  interp.N = vec3(1);
}
