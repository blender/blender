/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/math_fast.h"
#include "util/math_float3.h"
#include "util/types_base.h"

CCL_NAMESPACE_BEGIN

struct RGBE {
  union {
    struct {
      uint8_t r, g, b, e;
    };
    float f;
  };

  RGBE() = default;

  ccl_device_inline_method RGBE(const float f_) : f(f_) {}
};

static_assert(sizeof(RGBE) == 4, "RGBE expected to be exactly 4 bytes");

/**
 * RGBE format represents an RGB value with 4 bytes.
 *
 * The original implementation by Greg Ward uses 8 bits for RGB each, plus 8 bits shared exponent.
 * It has the same relative precision as the 0 to 255 range of standard 24-bit image formats, but
 * offers extended intensity range:
 * https://www.graphics.cornell.edu/~bjw/rgbe.html
 * GL_EXT_texture_shared_exponent uses 9 bits for RGB and 5 bits for exponent instead, with smaller
 * range but higher precision:
 * https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_shared_exponent.txt
 *
 * Our implementation is mostly based on GL_EXT_texture_shared_exponent, but uses 8 bits for RGB
 * each, and adds 3 sign bits to represent negative values. The memory layout is as follows:
 *
 *        xxxxxxxx  xxxxxxxx  xxxxxxxx  xxx  xxxxx
 *          m(R)      m(G)      m(B)    sgn   exp
 *
 * Each float component is interpreted as
 *                sgn          exp - bias
 *        f = (-1)    * 0.m * 2
 *
 * We choose a bias of 15, so that the largest representable value is
 *        RGBE_MAX = 0.11111111 * 2^(31 - 15) = 65280,
 * and the smallest positive representable value is
 *        RGBE_MIN = 0.00000001 * 2^(0 - 15) = 1.1920929e-7
 */

#define RGBE_EXP_BIAS 15
#define RGBE_MANTISSA_BITS 8
#define RGBE_EXPONENT_BITS 5
#define RGBE_MAX 65280.0f

ccl_device RGBE rgb_to_rgbe(float3 rgb)
{
  const float max_v = min(reduce_max(fabs(rgb)), RGBE_MAX);
  if (max_v < ldexpf(0.5f, -RGBE_EXP_BIAS - RGBE_MANTISSA_BITS)) {
    return RGBE(0.0f);
  }

  int e = max(-RGBE_EXP_BIAS - 1, floor_log2f(max_v)) + 1;
  float v = ldexpf(1.0f, RGBE_MANTISSA_BITS - e);

  /* The original implementation by Greg Ward uses `floor`, causing systematic bias when
   * accumulated in a buffer.
   * We use `round` instead, but need to deal with overflow. */
  if (int(roundf(max_v * v)) == power_of_2(RGBE_MANTISSA_BITS)) {
    e += 1;
    v *= 0.5f;
  }

  /* Get sign bits. */
  const uint sign_bits = ((__float_as_uint(rgb.x) >> 31) << 7) |
                         ((__float_as_uint(rgb.y) >> 31) << 6) |
                         ((__float_as_uint(rgb.z) >> 31) << 5);

  RGBE rgbe;
  rgb = min(round(fabs(rgb) * v), make_float3(255.0f));
  rgbe.r = uint8_t(rgb.x);
  rgbe.g = uint8_t(rgb.y);
  rgbe.b = uint8_t(rgb.z);
  rgbe.e = uint8_t(((e + RGBE_EXP_BIAS) & 0x1Fu) | sign_bits);
  return rgbe;
}

ccl_device_inline float3 rgbe_to_rgb(const RGBE rgbe)
{
  if (rgbe.f == 0.0f) {
    return zero_float3();
  }

  const int e = rgbe.e & 0x1Fu;
  const float f = ldexpf(1.0f, e - (int)(RGBE_EXP_BIAS + RGBE_MANTISSA_BITS));
  float3 result = make_float3(rgbe.r, rgbe.g, rgbe.b) * f;

  /* Set sign bits. */
  result.x = or_mask(result.x, (uint(rgbe.e) & 0x80u) << 24);
  result.y = or_mask(result.y, (uint(rgbe.e) & 0x40u) << 25);
  result.z = or_mask(result.z, (uint(rgbe.e) & 0x20u) << 26);
  return result;
}

CCL_NAMESPACE_END
