/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Display characters using an ascii table. Outputs one point per character.
 */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  /* Skip first 4 chars containing header data. */
  uint char_data = drw_debug_print_buf[gl_VertexID + 8];
  char_index = (char_data & 0xFFu) - 0x20u;

  /* Discard invalid chars. */
  if (char_index >= 96u) {
    gl_Position = vec4(-1);
    gl_PointSize = 0.0;
    return;
  }
  uint row = (char_data >> 16u) & 0xFFu;
  uint col = (char_data >> 8u) & 0xFFu;

  float char_size = 16.0;
  /* Change anchor point to the top left. */
  vec2 pos_on_screen = char_size * vec2(col, row) + char_size * 4;
  gl_Position = vec4((pos_on_screen / viewport_size) * vec2(2.0, -2.0) - vec2(1.0, -1.0), 0, 1);
  gl_PointSize = char_size;
}
