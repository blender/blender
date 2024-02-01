/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#if defined(GPU_COMPUTE_SHADER) || defined(GPU_VERTEX_SHADER)

void main()
{
  data_out[0] = int(float_in);
  data_out[1] = int(uint_in);
  data_out[2] = int(int_in);
  data_out[3] = int(bool_in);

#  if defined(GPU_VERTEX_SHADER)
  gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
  gl_PointSize = 1.0;
#  endif
}

#else

void main() {}

#endif
