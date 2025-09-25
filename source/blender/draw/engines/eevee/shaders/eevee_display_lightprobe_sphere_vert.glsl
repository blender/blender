/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_lightprobe_sphere_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_display_lightprobe_sphere)

#include "draw_view_lib.glsl"
#include "eevee_reverse_z_lib.glsl"

void main()
{
  /* Constant array moved inside function scope.
   * Minimizes local register allocation in MSL. */
  constexpr float2 pos[6] = float2_array(float2(-1.0f, -1.0f),
                                         float2(1.0f, -1.0f),
                                         float2(-1.0f, 1.0f),

                                         float2(1.0f, -1.0f),
                                         float2(1.0f, 1.0f),
                                         float2(-1.0f, 1.0f));

  lP = pos[gl_VertexID % 6];
  int display_index = gl_VertexID / 6;

  probe_index = display_data_buf[display_index].probe_index;
  float sphere_radius = display_data_buf[display_index].display_size;

  float3 ws_probe_pos = lightprobe_sphere_buf[probe_index].location;

  float3 vs_offset = float3(lP, 0.0f) * sphere_radius;
  float3 vP = drw_point_world_to_view(ws_probe_pos) + vs_offset;
  P = drw_point_view_to_world(vP);

  gl_Position = drw_point_view_to_homogenous(vP);
  /* Small bias to let the icon draw without Z-fighting. */
  gl_Position.z += 0.0001f;
  gl_Position = reverse_z::transform(gl_Position);
}
