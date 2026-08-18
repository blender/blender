/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_rounded_corner_mask_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_2D_rounded_corner_mask)

void main()
{
  /* One quad per corner, counter-clockwise from the top right. */
  int corner_id = gl_InstanceID;
  float radius = radii[corner_id];

  float2 corner_sign = float2((corner_id == 0 || corner_id == 3) ? 1.0f : -1.0f,
                              (corner_id == 0 || corner_id == 1) ? 1.0f : -1.0f);
  float2 corner_co = float2((corner_sign.x > 0.0f) ? rect.y : rect.x,
                            (corner_sign.y > 0.0f) ? rect.w : rect.z);

  /* Distance inside of the corner, spanning the square bounding the arc, expanded by a pixel. */
  float2 quad_co = float2(float(gl_VertexID / 2), float(gl_VertexID % 2));
  float2 inset = float2(-1.0f) + (quad_co * (radius + 1.0f));

  arc_co = float2(radius) - inset;
  arc_radius = radius;

  float2 final_pos = corner_co - (corner_sign * inset);
  gl_Position = ModelViewProjectionMatrix * float4(final_pos, 0.0f, 1.0f);
}
