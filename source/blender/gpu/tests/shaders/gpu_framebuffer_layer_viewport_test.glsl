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
  /* NOTE: Make it cover more than one viewport to test default scissors. */
  gl_Position = vec4(x * 2.0, y * 2.0, 1.0, 1.0);

  int index = gl_VertexID / 3;
  gpu_ViewportIndex = index % 16;
  gpu_Layer = index / 16;
}
#endif

#ifdef GPU_FRAGMENT_SHADER
void main()
{
  out_value = ivec2(gpu_Layer, gpu_ViewportIndex);
}
#endif
