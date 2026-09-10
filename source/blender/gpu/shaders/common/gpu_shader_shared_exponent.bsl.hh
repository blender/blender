/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/**
 * Software implementation of encoding and decoding of shared exponent texture as described by the
 * OpenGL extension EXT_texture_shared_exponent Appendix
 * https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_shared_exponent.txt
 *
 * This allows to read and write the RGB9_E5 format in a R32UI texture without explicit support on
 * the hardware for this type. However, filtering is not supported in this case.
 */

#define RGB9E5_EXPONENT_BITS 5
#define RGB9E5_MANTISSA_BITS 9
#define RGB9E5_EXP_BIAS 15
#define RGB9E5_MAX_VALID_BIASED_EXP 31

#define MAX_RGB9E5_EXP (RGB9E5_MAX_VALID_BIASED_EXP - RGB9E5_EXP_BIAS)
#define RGB9E5_MANTISSA_VALUES (1 << RGB9E5_MANTISSA_BITS)
#define MAX_RGB9E5_MANTISSA (RGB9E5_MANTISSA_VALUES - 1)

int rgb9e5_floor_log2(float x)
{
  /* Ok, rgb9e5_floor_log2 is not correct for the denorm and zero values, but we
   * are going to do a max of this value with the minimum rgb9e5 exponent
   * that will hide these problem cases. */
  int biased_exponent = floatBitsToInt(x) >> 23;
  return biased_exponent - 127;
}

float rgb9e5_exponent_factor(int exponent)
{
  /* This pow function could be replaced by a table. There is only 32 values. */
  return exp2(float(exponent - RGB9E5_EXP_BIAS - RGB9E5_MANTISSA_BITS));
}

struct rgb9e5_t {
  uint exp_shared;
  uint3 mantissa;
};

rgb9e5_t rgb9e5_from_float3(float3 color)
{
  constexpr float max_rgb9e5 = float(0xFF80u);
  color = clamp(color, 0.0f, max_rgb9e5);

  float max_component = max(max(color.r, color.g), color.b);
  int log2_floored = rgb9e5_floor_log2(max_component);
  int exp_shared = max(-RGB9E5_EXP_BIAS - 1, log2_floored) + (1 + RGB9E5_EXP_BIAS);
  float denom = rgb9e5_exponent_factor(exp_shared);
  int maxm = int(max_component / denom + 0.5f);
  if (maxm == MAX_RGB9E5_MANTISSA + 1) {
    denom *= 2.0f;
    exp_shared += 1;
  }

  rgb9e5_t result;
  result.exp_shared = uint(exp_shared);
  result.mantissa = uint3(color / denom + 0.5f);
  return result;
}

uint rgb9e5_encode(float3 color)
{
  rgb9e5_t result = rgb9e5_from_float3(color);
  result.exp_shared <<= RGB9E5_MANTISSA_BITS * 3;
  result.mantissa <<= RGB9E5_MANTISSA_BITS * uint3(0, 1, 2);
  return result.mantissa.r | result.mantissa.g | result.mantissa.b | result.exp_shared;
}

float3 rgb9e5_decode(uint data)
{
  int exp_shared = int(data >> (RGB9E5_MANTISSA_BITS * 3));
  uint3 mantissa = (uint3(data) >> (RGB9E5_MANTISSA_BITS * uint3(0, 1, 2))) &
                   uint(MAX_RGB9E5_MANTISSA);
  return float3(mantissa) * rgb9e5_exponent_factor(exp_shared);
}
