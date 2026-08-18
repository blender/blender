/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_hash.glsl"

uint hash_value_float_to_uint(float value)
{
  return hash_uint(floatBitsToUint(value));
}

uint hash_value_vec3_to_uint(float3 vec)
{
  return hash_uint3(floatBitsToUint(vec.x), floatBitsToUint(vec.y), floatBitsToUint(vec.z));
}

uint hash_value_vec4_to_uint(float4 vec)
{
  return hash_uint4(floatBitsToUint(vec.x),
                    floatBitsToUint(vec.y),
                    floatBitsToUint(vec.z),
                    floatBitsToUint(vec.w));
}

uint hash_value_mat4_to_uint(float4x4 mat)
{
  return hash_uint4(hash_value_vec4_to_uint(mat[0]),
                    hash_value_vec4_to_uint(mat[1]),
                    hash_value_vec4_to_uint(mat[2]),
                    hash_value_vec4_to_uint(mat[3]));
}

[[node]]
void hash_value_float(float value, int seed, out int hash)
{
  hash = int(hash_uint2(hash_value_float_to_uint(value), uint(seed)));
}

[[node]]
void hash_value_vector(float3 value, int seed, out int hash)
{
  hash = int(hash_uint2(hash_value_vec3_to_uint(value), uint(seed)));
}

[[node]]
void hash_value_color(float4 value, int seed, out int hash)
{
  hash = int(hash_uint2(hash_value_vec4_to_uint(value), uint(seed)));
}

[[node]]
void hash_value_int(int value, int seed, out int hash)
{
  hash = int(hash_uint2(hash_uint(uint(value)), uint(seed)));
}

[[node]]
void hash_value_rotation(float4 value, int seed, out int hash)
{
  hash = int(hash_uint2(hash_value_vec4_to_uint(value), uint(seed)));
}

[[node]]
void hash_value_matrix(float4x4 value, int seed, out int hash)
{
  hash = int(hash_uint2(hash_value_mat4_to_uint(value), uint(seed)));
}
