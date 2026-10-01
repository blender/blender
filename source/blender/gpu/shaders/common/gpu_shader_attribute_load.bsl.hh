/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/**
 * Library to read packed vertex buffer data of a `gpu::Batch` using a SSBO rather than using input
 * assembly. It is **not** needed to use these macros if the data is known to be aligned and
 * contiguous. Arrays of any 4-byte component vector except 3 component vectors do not need this.
 */

/** Returns index in the first component. Needed for non trivially packed data. */
uint gpu_attr_load_index(uint vertex_index, int2 stride_and_offset)
{
  return vertex_index * uint(stride_and_offset.x) + uint(stride_and_offset.y);
}

float4 gpu_attr_decode_1010102_snorm(uint in_data)
{
  /* TODO(fclem): Improve this. */
  uint4 v_data = uint4(in_data) >> uint4(0, 10, 20, 30);
  bool4 v_sign = greaterThan(v_data & uint4(0x3FF, 0x3FF, 0x3FF, 0x3),
                             uint4(0x1FF, 0x1FF, 0x1FF, 0x1));
  uint4 v_data_u = floatBitsToUint(mix(uintBitsToFloat(v_data), uintBitsToFloat(~v_data), v_sign));
  float4 mag = float4(v_data_u & uint4(0x1FF, 0x1FF, 0x1FF, 0x1)) /
               float4(0x1FF, 0x1FF, 0x1FF, 0x1);
  return mix(mag, -mag, v_sign);
}

float4 gpu_attr_decode_short4_to_float4_snorm(uint data0, uint data1)
{
  return float4(unpackSnorm2x16(data0), unpackSnorm2x16(data1));
}

uint4 gpu_attr_decode_uchar4_to_uint4(uint in_data)
{
  return (uint4(in_data) >> uint4(0, 8, 16, 24)) & uint4(0xFF);
}

/* TODO(fclem): Once the stride and offset are made obsolete, we can think of wrapping vec3 into
 * structs of floats as they do not have the 16byte alignment restriction. */

[[force_inline]] float3 gpu_attr_load_float3(const float (&data_buf)[],
                                             int2 stride_and_offset,
                                             uint i)
{
  uint index = gpu_attr_load_index(i, stride_and_offset);
  float3 v;
  v.x = data_buf[index + 0];
  v.y = data_buf[index + 1];
  v.z = data_buf[index + 2];
  return v;
}

[[force_inline]] float2 gpu_attr_load_float2(const float (&data_buf)[],
                                             int2 stride_and_offset,
                                             uint i)
{
  uint index = gpu_attr_load_index(i, stride_and_offset);
  float2 v;
  v.x = data_buf[index + 0];
  v.y = data_buf[index + 1];
  return v;
}

[[force_inline]] uint3 gpu_attr_load_uint3(const uint (&data_buf)[],
                                           int2 stride_and_offset,
                                           uint i)
{
  uint index = gpu_attr_load_index(i, stride_and_offset);
  uint3 v;
  v.x = data_buf[index + 0];
  v.y = data_buf[index + 1];
  v.z = data_buf[index + 2];
  return v;
}

[[force_inline]] uint2 gpu_attr_load_uint2(const uint (&data_buf)[],
                                           int2 stride_and_offset,
                                           uint i)
{
  uint index = gpu_attr_load_index(i, stride_and_offset);
  uint2 v;
  v.x = data_buf[index + 0];
  v.y = data_buf[index + 1];
  return v;
}

[[force_inline]] int3 gpu_attr_load_int3(const uint (&data_buf)[], int2 stride_and_offset, uint i)
{
  uint index = gpu_attr_load_index(i, stride_and_offset);
  int3 v;
  v.x = data_buf[index + 0];
  v.y = data_buf[index + 1];
  v.z = data_buf[index + 2];
  return v;
}

[[force_inline]] int2 gpu_attr_load_int2(const uint (&data_buf)[], int2 stride_and_offset, uint i)
{
  uint index = gpu_attr_load_index(i, stride_and_offset);
  int2 v;
  v.x = data_buf[index + 0];
  v.y = data_buf[index + 1];
  return v;
}

[[force_inline]] float4 gpu_attr_load_uint_1010102_snorm(const uint (&data_buf)[],
                                                         int2 stride_and_offset,
                                                         uint i)
{
  uint index = gpu_attr_load_index(i, stride_and_offset);
  uint v = data_buf[index];
  return gpu_attr_decode_1010102_snorm(v);
}

/* TODO(fclem): Once the stride and offset are made obsolete, we can think of wrapping short4 into
 * structs of uint as they do not have the 16byte alignment restriction. */

[[force_inline]] float4 gpu_attr_load_short4_snorm(const uint (&data_buf)[],
                                                   int2 stride_and_offset,
                                                   uint i)
{
  uint index = gpu_attr_load_index(i, stride_and_offset);
  uint v0 = data_buf[index + 0];
  uint v1 = data_buf[index + 1];
  return gpu_attr_decode_short4_to_float4_snorm(v0, v1);
}

[[force_inline]] uint4 gpu_attr_load_uchar4(const uint (&data_buf)[], uint i)
{
  uint index = gpu_attr_load_index(i >> 2u, int2(1, 0));
  return gpu_attr_decode_uchar4_to_uint4(data_buf[index]);
}

[[force_inline]] uint gpu_attr_load_uchar(const uint (&data_buf)[], uint i)
{
  uint index = gpu_attr_load_index(i >> 2u, int2(1, 0));
  uint4 v = gpu_attr_decode_uchar4_to_uint4(data_buf[index]);
  return v[i & 3u];
}

[[force_inline]] bool gpu_attr_load_bool(const uint (&data)[], uint i)
{
  uint v = gpu_attr_load_uchar(data, i);
  return v != 0u;
}
