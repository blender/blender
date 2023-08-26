/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  int corner_id = (gl_VertexID / cornerLen) % 4;

  vec2 final_pos = pos * scale;

  if (corner_id == 0) {
    uv = pos + vec2(1.0, 1.0);
    final_pos += rect.yw; /* top right */
  }
  else if (corner_id == 1) {
    uv = pos + vec2(-1.0, 1.0);
    final_pos += rect.xw; /* top left */
  }
  else if (corner_id == 2) {
    uv = pos + vec2(-1.0, -1.0);
    final_pos += rect.xz; /* bottom left */
  }
  else {
    uv = pos + vec2(1.0, -1.0);
    final_pos += rect.yz; /* bottom right */
  }

  gl_Position = (ModelViewProjectionMatrix * vec4(final_pos, 0.0, 1.0));
}
