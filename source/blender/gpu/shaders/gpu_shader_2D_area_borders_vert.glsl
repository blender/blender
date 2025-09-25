/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_area_borders_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_2D_area_borders)

void main()
{
  int corner_id = (gl_VertexID / cornerLen) % 4;
  bool inner = all(lessThan(abs(pos), float2(1.0f)));

  /* Scale the inner part of the border.
   * Add a sub pixel offset to the outer part to make sure we don't miss a pixel row/column. */
  float2 final_pos = pos * ((inner) ? (1.0f - width) : 1.05f);

  uv = final_pos;
  /* Rescale to the corner size and position the corner. */
  if (corner_id == 0) {
    /* top right */
    final_pos = (final_pos - float2(1.0f, 1.0f)) * scale + rect.yw;
  }
  else if (corner_id == 1) {
    /* top left */
    final_pos = (final_pos - float2(-1.0f, 1.0f)) * scale + rect.xw;
  }
  else if (corner_id == 2) {
    /* bottom left */
    final_pos = (final_pos - float2(-1.0f, -1.0f)) * scale + rect.xz;
  }
  else {
    /* bottom right */
    final_pos = (final_pos - float2(1.0f, -1.0f)) * scale + rect.yz;
  }

  gl_Position = (ModelViewProjectionMatrix * float4(final_pos, 0.0f, 1.0f));
}
