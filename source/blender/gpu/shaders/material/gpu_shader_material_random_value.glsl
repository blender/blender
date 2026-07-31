/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_hash.glsl"

[[node]]
void random_value_vector(float3 min_value, float3 max_value, int id, int seed, out float3 result)
{
  const float x = hash_uint3_to_float(uint(seed), uint(id), 0u);
  const float y = hash_uint3_to_float(uint(seed), uint(id), 1u);
  const float z = hash_uint3_to_float(uint(seed), uint(id), 2u);
  result = float3(x, y, z) * (max_value - min_value) + min_value;
}

[[node]]
void random_value_float(float min_value, float max_value, int id, int seed, out float result)
{
  const float value = hash_uint2_to_float(uint(seed), uint(id));
  result = value * (max_value - min_value) + min_value;
}

[[node]]
void random_value_int(int min_value, int max_value, int id, int seed, out int result)
{
  if (min_value > max_value) {
    const int tmp = min_value;
    min_value = max_value;
    max_value = tmp;
  }

  const uint hash = hash_uint2(uint(id), uint(seed));

  /* Calculate range using unsigned types to fit the entire 32-bit space. */
  const uint range = uint(max_value) - uint(min_value) + 1u;

  /* Range wraps around to 0 when min_value is INT_MIN and max_value is INT_MAX.
   * so the modulo is unnecessary and would cause a division by zero. */
  const uint modulo_result = (range == 0u) ? hash : (hash % range);

  result = int(uint(min_value) + modulo_result);
}

[[node]]
void random_value_bool(float probability, int id, int seed, out bool result)
{
  result = hash_uint2_to_float(uint(id), uint(seed)) <= probability;
}
