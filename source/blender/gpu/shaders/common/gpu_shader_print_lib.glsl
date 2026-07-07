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

uint print_data(uint offset, string_t data)
{
  return print_data(offset, as_uint(data));
}

uint print_data(uint offset, int data)
{
  return print_data(offset, uint(data));
}

uint print_data(uint offset, float data)
{
  return print_data(offset, floatBitsToUint(data));
}

uint print_start(const uint data_len)
{
  /* Add one to skip the length stored in the first element of the buffer. */
  return atomicAdd(gpu_print_buf[0], data_len) + 1u;
}
