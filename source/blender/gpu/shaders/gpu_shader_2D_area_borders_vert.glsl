/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_area_borders_info.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_2D_area_borders)

void main()
{
  int corner_id = (gl_VertexID / cornerLen) % 4;
  bool inner = all(lessThan(abs(pos), vec2(1.0)));

  /* Scale the inner part of the border.
   * Add a sub pixel offset to the outer part to make sure we don't miss a pixel row/column. */
  vec2 final_pos = pos * ((inner) ? (1.0 - width) : 1.05);

  uv = final_pos;
  /* Rescale to the corner size and position the corner. */
  if (corner_id == 0) {
    /* top right */
    final_pos = (final_pos - vec2(1.0, 1.0)) * scale + rect.yw;
  }
  else if (corner_id == 1) {
    /* top left */
    final_pos = (final_pos - vec2(-1.0, 1.0)) * scale + rect.xw;
  }
  else if (corner_id == 2) {
    /* bottom left */
    final_pos = (final_pos - vec2(-1.0, -1.0)) * scale + rect.xz;
  }
  else {
    /* bottom right */
    final_pos = (final_pos - vec2(1.0, -1.0)) * scale + rect.yz;
  }

  gl_Position = (ModelViewProjectionMatrix * vec4(final_pos, 0.0, 1.0));
}
