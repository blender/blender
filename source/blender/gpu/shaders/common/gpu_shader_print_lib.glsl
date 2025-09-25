/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/gpu_shader_print_infos.hh"

SHADER_LIBRARY_CREATE_INFO(gpu_print)

uint print_data(uint offset, uint data)
{
  if (offset < GPU_SHADER_PRINTF_MAX_CAPACITY) {
    gpu_print_buf[offset] = data;
  }
  return offset + 1u;
}

uint print_data(uint offset, int data)
{
  return print_data(offset, uint(data));
}

uint print_data(uint offset, float data)
{
  return print_data(offset, floatBitsToUint(data));
}

uint print_header(const uint data_len, uint format_hash)
{
  uint offset = atomicAdd(gpu_print_buf[0], 1u + data_len) + 1u;
  return print_data(offset, format_hash);
}
