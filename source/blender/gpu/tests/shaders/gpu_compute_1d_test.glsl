/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_test_infos.hh"

COMPUTE_SHADER_CREATE_INFO(gpu_compute_1d_test)

void main()
{
  int index = int(gl_GlobalInvocationID.x);
  float4 pos = float4(gl_GlobalInvocationID.x);
  imageStore(img_output, index, pos);
}
