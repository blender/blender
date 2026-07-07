/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_test_infos.hh"

COMPUTE_SHADER_CREATE_INFO(gpu_buffer_texture_test)

void main()
{
  int index = int(gl_GlobalInvocationID.x);
  float value = texelFetch(bufferTexture, index).r;
  data_out[index] = value;
}
