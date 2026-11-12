/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compat.hh"

#include "infos/gpu_shader_test_infos.hh"

#ifdef GPU_VERTEX_SHADER
VERTEX_SHADER_CREATE_INFO(gpu_framebuffer_subpass_input_test)

void main()
{
  /* Full-screen triangle. */
  int v = gl_VertexID % 3;
  float x = -1.0f + float((v & 1) << 2);
  float y = -1.0f + float((v & 2) << 1);
  gl_Position = float4(x, y, 1.0f, 1.0f);
#  ifdef LAYERED
  /* Draw the full-screen triangle once for every layer of a layered attachment. */
  gpu_Layer = gl_VertexID / 3;
#  endif
}
#endif

#ifdef GPU_FRAGMENT_SHADER
FRAGMENT_SHADER_CREATE_INFO(gpu_framebuffer_subpass_input_test)

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

#  ifdef WRITE_LAYERED
void main()
{
  /* The value depends on the layer so a driver that always reads layer 0 of a sub-pass input
   * (documented on Adreno on Windows) is detected: the per-layer term is then missing on the
   * read-back. The value must fit in an int as the target is a signed integer texture. */
  out_value = 0x12345678 + gpu_Layer * 0x1000000;
}
#  endif

#  ifdef READ_LAYERED
void main()
{
  /* The layer is part of the value to check that layered rendering still works. */
  out_value = in_value + 495 + gpu_Layer * 1000;
}
#  endif

#endif
