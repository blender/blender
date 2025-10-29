/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#if defined(GPU_VERTEX_SHADER)

void main()
{
  gl_Position = float4(0.0f, 0.0f, 0.0f, 1.0f);
  gl_PointSize = 1.0f;
}

#else

void main()
{
  data_out[0] = texture(tex_1, float2(0));
  data_out[1] = texture(tex_2, float2(0));
  data_out[2] = texture(tex_3, float2(0));
  data_out[3] = texture(tex_4, float2(0));
  data_out[4] = texture(tex_5, float2(0));
  data_out[5] = texture(tex_6, float2(0));
  data_out[6] = texture(tex_7, float2(0));
  data_out[7] = texture(tex_8, float2(0));
  data_out[8] = texture(tex_9, float2(0));
  data_out[9] = texture(tex_10, float2(0));
  data_out[10] = texture(tex_11, float2(0));
  data_out[11] = texture(tex_12, float2(0));
  data_out[12] = texture(tex_13, float2(0));
  data_out[13] = texture(tex_14, float2(0));
  data_out[14] = texture(tex_15, float2(0));
  data_out[15] = texture(tex_16, float2(0));
  data_out[16] = texture(tex_17, float2(0));
  data_out[17] = texture(tex_18, float2(0));
}

#endif
