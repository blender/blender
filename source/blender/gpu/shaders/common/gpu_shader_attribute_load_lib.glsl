/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Library to read packed vertex buffer data of a `gpu::Batch` using a SSBO rather than using input
 * assembly. It is **not** needed to use these macros if the data is known to be aligned and
 * contiguous. Arrays of any 4-byte component vector except 3 component vectors do not need this.
 *
 * Implemented as macros to avoid compiler differences with buffer qualifiers.
 */

/** Returns index in the first component. Needed for non trivially packed data. */
uint gpu_attr_load_index(uint vertex_index, ivec2 stride_and_offset)
{
  return vertex_index * uint(stride_and_offset.x) + uint(stride_and_offset.y);
}

/* TODO(fclem): Once the stride and offset are made obsolete, we can think of wrapping vec3 into
 * structs of floats as they do not have the 16byte alignment restriction. */

#define gpu_attr_load_triplet(_type, _data, _stride_and_offset, _i) \
  _type(_data[gpu_attr_load_index(_i, _stride_and_offset) + 0], \
        _data[gpu_attr_load_index(_i, _stride_and_offset) + 1], \
        _data[gpu_attr_load_index(_i, _stride_and_offset) + 2])

/* Assumes _data is declared as an array of float. */
#define gpu_attr_load_float3(_data, _stride_and_offset, _i) \
  gpu_attr_load_triplet(vec3, _data, _stride_and_offset, _i)
/* Assumes _data is declared as an array of uint. */
#define gpu_attr_load_uint3(_data, _stride_and_offset, _i) \
  gpu_attr_load_triplet(ivec3, _data, _stride_and_offset, _i)
/* Assumes _data is declared as an array of int. */
#define gpu_attr_load_int3(_data, _stride_and_offset, _i) \
  gpu_attr_load_triplet(uvec3, _data, _stride_and_offset, _i)
