/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_test_infos.hh"

COMPUTE_SHADER_CREATE_INFO(gpu_compute_ssbo_test)

void main()
{
  int store_index = int(gl_GlobalInvocationID.x);
  data_out[store_index] = store_index * 4;
}
