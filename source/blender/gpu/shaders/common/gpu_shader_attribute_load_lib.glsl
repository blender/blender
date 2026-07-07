/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/**
 * Library to read packed vertex buffer data of a `gpu::Batch` using a SSBO rather than using input
 * assembly. It is **not** needed to use these macros if the data is known to be aligned and
 * contiguous. Arrays of any 4-byte component vector except 3 component vectors do not need this.
 *
 * Implemented as macros to avoid compiler differences with buffer qualifiers.
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
  /* TODO(fclem): Improve this. */
  uint4 v_data = uint4(data0, data0 >> 16u, data1, data1 >> 16u);
  bool4 v_sign = greaterThan(v_data & uint4(0xFFFF), uint4(0x7FFF));
  uint4 v_data_u = floatBitsToUint(mix(uintBitsToFloat(v_data), uintBitsToFloat(~v_data), v_sign));
  float4 mag = float4(v_data_u & 0x7FFFu) / float(0x7FFF);
  return mix(mag, -mag, v_sign);
}

uint4 gpu_attr_decode_uchar4_to_uint4(uint in_data)
{
  return (uint4(in_data) >> uint4(0, 8, 16, 24)) & uint4(0xFF);
}

/* TODO(fclem): Once the stride and offset are made obsolete, we can think of wrapping vec3 into
 * structs of floats as they do not have the 16byte alignment restriction. */

#define gpu_attr_load_triplet(_type, _data, _stride_and_offset, _i) \
  _type(_data[gpu_attr_load_index(_i, _stride_and_offset) + 0], \
        _data[gpu_attr_load_index(_i, _stride_and_offset) + 1], \
        _data[gpu_attr_load_index(_i, _stride_and_offset) + 2])
#define gpu_attr_load_tuple(_type, _data, _stride_and_offset, _i) \
  _type(_data[gpu_attr_load_index(_i, _stride_and_offset) + 0], \
        _data[gpu_attr_load_index(_i, _stride_and_offset) + 1])

/* Assumes _data is declared as an array of float. */
#define gpu_attr_load_float3(_data, _stride_and_offset, _i) \
  gpu_attr_load_triplet(float3, _data, _stride_and_offset, _i)
#define gpu_attr_load_float2(_data, _stride_and_offset, _i) \
  gpu_attr_load_tuple(float2, _data, _stride_and_offset, _i)
/* Assumes _data is declared as an array of uint. */
#define gpu_attr_load_uint3(_data, _stride_and_offset, _i) \
  gpu_attr_load_triplet(int3, _data, _stride_and_offset, _i)
#define gpu_attr_load_uint2(_data, _stride_and_offset, _i) \
  gpu_attr_load_tuple(int2, _data, _stride_and_offset, _i)
/* Assumes _data is declared as an array of int. */
#define gpu_attr_load_int3(_data, _stride_and_offset, _i) \
  gpu_attr_load_triplet(uint3, _data, _stride_and_offset, _i)
#define gpu_attr_load_int2(_data, _stride_and_offset, _i) \
  gpu_attr_load_tuple(uint2, _data, _stride_and_offset, _i)

/* Assumes _data is declared as an array of uint. */
#define gpu_attr_load_uint_1010102_snorm(_data, _stride_and_offset, _i) \
  gpu_attr_decode_1010102_snorm(_data[gpu_attr_load_index(_i, _stride_and_offset)])

/* TODO(fclem): Once the stride and offset are made obsolete, we can think of wrapping short4 into
 * structs of uint as they do not have the 16byte alignment restriction. */

/* Assumes _data is declared as an array of uint. */
#define gpu_attr_load_short4_snorm(_data, _stride_and_offset, _i) \
  gpu_attr_decode_short4_to_float4_snorm(_data[gpu_attr_load_index(_i, _stride_and_offset) + 0], \
                                         _data[gpu_attr_load_index(_i, _stride_and_offset) + 1])
/* Assumes _data is declared as an array of uint. */
#define gpu_attr_load_uchar4(_data, _stride_and_offset, _i) \
  gpu_attr_decode_uchar4_to_uint4(_data[gpu_attr_load_index(_i, _stride_and_offset)])
/* Assumes _data is declared as an array of uint. */
#define gpu_attr_load_uchar(_data, _i) \
  gpu_attr_decode_uchar4_to_uint4( \
      _data[gpu_attr_load_index(uint(_i) >> 2u, int2(1, 0))])[uint(_i) & 3u]
/* Assumes _data is declared as an array of uint. */
#define gpu_attr_load_bool(_data, _i) (gpu_attr_load_uchar(_data, _i) != 0u)
