/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_background_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_clipbound)

#include "draw_view_lib.glsl"

void main()
{
  float3 world_pos = boundbox[gl_VertexID];
  gl_Position = drw_point_world_to_homogenous(world_pos);

  /* Result in a position at 1.0 (far plane). Small epsilon to avoid precision issue.
   * This mimics the effect of infinite projection matrix
   * (see http://www.terathon.com/gdc07_lengyel.pdf). */
  gl_Position.z = gl_Position.w - 2.4e-7f;
}
