/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_VERTEX_SHADER
void main()
{
  /* Full-screen triangle. */
  int v = gl_VertexID % 3;
  float x = -1.0 + float((v & 1) << 2);
  float y = -1.0 + float((v & 2) << 1);
  gl_Position = vec4(x, y, 1.0, 1.0);
}
#endif

#ifdef GPU_FRAGMENT_SHADER

#  ifdef WRITE
void main()
{
  out_value = 0xDEADBEEF;
}
#  endif

#  ifdef READ
void main()
{
  out_value = in_value + 495;
}
#  endif

#endif
