/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_math_half.hh"

#if defined(__ARM_NEON)
#  define USE_HARDWARE_FP16_NEON /* Use ARM FP16 conversion instructions */
#  include <arm_neon.h>
#endif

uint16_t blender::math::float_to_half(float v)
{
#if defined(USE_HARDWARE_FP16_NEON)
  float16x4_t h4 = vcvt_f16_f32(vdupq_n_f32(v));
  float16_t h = vget_lane_f16(h4, 0);
  return *(uint16_t *)&h;
#else
  /* Based on float_to_half_fast3_rtne from public domain https://gist.github.com/rygorous/2156668
   * see corresponding blog post https://fgiesen.wordpress.com/2012/03/28/half-to-float-done-quic/
   */
  union FP32 {
    uint32_t u;
    float f;
  };
  FP32 f;
  f.f = v;
  FP32 f32infty = {255 << 23};
  FP32 f16max = {(127 + 16) << 23};
  FP32 denorm_magic = {((127 - 15) + (23 - 10) + 1) << 23};
  uint32_t sign_mask = 0x80000000u;
  uint16_t o = {0};

  uint32_t sign = f.u & sign_mask;
  f.u ^= sign;

  /*
   * NOTE all the integer compares in this function can be safely
   * compiled into signed compares since all operands are below
   * 0x80000000. Important if you want fast straight SSE2 code
   * (since there's no unsigned PCMPGTD).
   */
  if (f.u >= f16max.u) {
    /* result is Inf or NaN (all exponent bits set) */
    o = (f.u > f32infty.u) ? 0x7e00 : 0x7c00; /* NaN->qNaN and Inf->Inf */
  }
  else {
    /* (De)normalized number or zero */
    if (f.u < (113 << 23)) {
      /* Resulting FP16 is subnormal or zero.
       * Use a magic value to align our 10 mantissa bits at the bottom of
       * the float. as long as FP addition is round-to-nearest-even this
       * just works. */
      f.f += denorm_magic.f;

      /* and one integer subtract of the bias later, we have our final float! */
      o = f.u - denorm_magic.u;
    }
    else {
      uint32_t mant_odd = (f.u >> 13) & 1; /* resulting mantissa is odd */

      /* update exponent, rounding bias part 1 */
      f.u += ((uint32_t)(15 - 127) << 23) + 0xfff;
      /* rounding bias part 2 */
      f.u += mant_odd;
      /* take the bits! */
      o = f.u >> 13;
    }
  }

  o |= sign >> 16;
  return o;
#endif
}

float blender::math::half_to_float(uint16_t v)
{
#if defined(USE_HARDWARE_FP16_NEON)
  uint16x4_t v4 = vdup_n_u16(v);
  float16x4_t h4 = vreinterpret_f16_u16(v4);
  float32x4_t f4 = vcvt_f32_f16(h4);
  return vgetq_lane_f32(f4, 0);
#else
  /* Based on half_to_float_fast4 from public domain https://gist.github.com/rygorous/2144712
   * see corresponding blog post https://fgiesen.wordpress.com/2012/03/28/half-to-float-done-quic/
   */
  union FP32 {
    uint32_t u;
    float f;
  };
  constexpr FP32 magic = {113 << 23};
  constexpr uint32_t shifted_exp = 0x7c00 << 13; /* exponent mask after shift */
  FP32 o;

  o.u = (v & 0x7fff) << 13;         /* exponent/mantissa bits */
  uint32_t exp = shifted_exp & o.u; /* just the exponent */
  o.u += (127 - 15) << 23;          /* exponent adjust */

  /* handle exponent special cases */
  if (exp == shifted_exp) {  /* Inf/NaN? */
    o.u += (128 - 16) << 23; /* extra exp adjust */
  }
  else if (exp == 0) { /* Zero/Denormal? */
    o.u += 1 << 23;    /* extra exp adjust */
    o.f -= magic.f;    /* renormalize */
  }

  o.u |= (v & 0x8000) << 16; /* sign bit */
  return o.f;
#endif
}

#ifdef USE_HARDWARE_FP16_NEON
#  undef USE_HARDWARE_FP16_NEON
#endif
