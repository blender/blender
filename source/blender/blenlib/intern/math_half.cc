/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_math_half.hh"

#if defined(__ARM_NEON)
/* Use ARM FP16 conversion instructions */
#  define USE_HARDWARE_FP16_NEON
#  include <arm_neon.h>
#endif
#if (defined(__x86_64__) || defined(_M_X64))
/* All AVX2 CPUs have F16C instructions, so use those if we're compiling for AVX2.
 * Otherwise use "manual" SSE2 4x-wide conversion. */
#  if defined(__AVX2__)
#    define USE_HARDWARE_FP16_F16C
#  else
#    define USE_SSE2_FP16
#  endif
#  include <immintrin.h>
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
      f.u += (uint32_t(15 - 127) << 23) + 0xfff;
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

uint16_t blender::math::float_to_half_make_finite(float v)
{
  uint16_t h = float_to_half(v);
  /* Infinity or NaN? */
  if ((h & 0x7c00) == 0x7c00) {
    if ((h & 0x03ff) == 0) {
      /* +/- infinity: +/- max value. */
      h ^= 0x07ff;
    }
    else {
      /* +/- Nan: +/- zero. */
      h &= 0x8000;
    }
  }
  return h;
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

#ifdef USE_SSE2_FP16
/* 4x wide float<->half conversion using SSE2 code, based on
 * https://gist.github.com/rygorous/4d9e9e88cab13c703773dc767a23575f */

/* Float->half conversion with round-to-nearest-even, SSE2+.
 * leaves half-floats in 32-bit lanes (sign extended). */
static inline __m128i F32_to_F16_4x(const __m128 &f)
{
  const __m128 mask_sign = _mm_set1_ps(-0.0f);
  /* all FP32 values >=this round to +inf */
  const __m128i c_f16max = _mm_set1_epi32((127 + 16) << 23);
  const __m128i c_nanbit = _mm_set1_epi32(0x200);
  const __m128i c_nanlobits = _mm_set1_epi32(0x1ff);
  const __m128i c_infty_as_fp16 = _mm_set1_epi32(0x7c00);
  /* smallest FP32 that yields a normalized FP16 */
  const __m128i c_min_normal = _mm_set1_epi32((127 - 14) << 23);
  const __m128i c_subnorm_magic = _mm_set1_epi32(((127 - 15) + (23 - 10) + 1) << 23);
  /* adjust exponent and add mantissa rounding */
  const __m128i c_normal_bias = _mm_set1_epi32(0xfff - ((127 - 15) << 23));

  __m128 justsign = _mm_and_ps(f, mask_sign);
  __m128 absf = _mm_andnot_ps(mask_sign, f); /* f & ~mask_sign */
  /* the cast is "free" (extra bypass latency, but no throughput hit) */
  __m128i absf_int = _mm_castps_si128(absf);
  __m128 b_isnan = _mm_cmpunord_ps(absf, absf);              /* is this a NaN? */
  __m128i b_isregular = _mm_cmpgt_epi32(c_f16max, absf_int); /* (sub)normalized or special? */
  __m128i nan_payload = _mm_and_si128(_mm_srli_epi32(absf_int, 13),
                                      c_nanlobits);        /* payload bits for NaNs */
  __m128i nan_quiet = _mm_or_si128(nan_payload, c_nanbit); /* and set quiet bit */
  __m128i nanfinal = _mm_and_si128(_mm_castps_si128(b_isnan), nan_quiet);
  __m128i inf_or_nan = _mm_or_si128(nanfinal, c_infty_as_fp16); /* output for specials */

  /* subnormal? */
  __m128i b_issub = _mm_cmpgt_epi32(c_min_normal, absf_int);

  /* "result is subnormal" path */
  __m128 subnorm1 = _mm_add_ps(
      absf, _mm_castsi128_ps(c_subnorm_magic)); /* magic value to round output mantissa */
  __m128i subnorm2 = _mm_sub_epi32(_mm_castps_si128(subnorm1),
                                   c_subnorm_magic); /* subtract out bias */

  /* "result is normal" path */
  __m128i mantoddbit = _mm_slli_epi32(absf_int, 31 - 13); /* shift bit 13 (mantissa LSB) to sign */
  __m128i mantodd = _mm_srai_epi32(mantoddbit, 31);       /* -1 if FP16 mantissa odd, else 0 */

  __m128i round1 = _mm_add_epi32(absf_int, c_normal_bias);
  /* if mantissa LSB odd, bias towards rounding up (RTNE) */
  __m128i round2 = _mm_sub_epi32(round1, mantodd);
  __m128i normal = _mm_srli_epi32(round2, 13); /* rounded result */

  /* combine the two non-specials */
  __m128i nonspecial = _mm_or_si128(_mm_and_si128(subnorm2, b_issub),
                                    _mm_andnot_si128(b_issub, normal));

  /* merge in specials as well */
  __m128i joined = _mm_or_si128(_mm_and_si128(nonspecial, b_isregular),
                                _mm_andnot_si128(b_isregular, inf_or_nan));

  __m128i sign_shift = _mm_srai_epi32(_mm_castps_si128(justsign), 16);
  __m128i result = _mm_or_si128(joined, sign_shift);

  return result;
}

/* Half->float conversion, SSE2+. Input in 32-bit lanes. */
static inline __m128 F16_to_F32_4x(const __m128i &h)
{
  const __m128i mask_nosign = _mm_set1_epi32(0x7fff);
  const __m128 magic_mult = _mm_castsi128_ps(_mm_set1_epi32((254 - 15) << 23));
  const __m128i was_infnan = _mm_set1_epi32(0x7bff);
  const __m128 exp_infnan = _mm_castsi128_ps(_mm_set1_epi32(255 << 23));
  const __m128i was_nan = _mm_set1_epi32(0x7c00);
  const __m128i nan_quiet = _mm_set1_epi32(1 << 22);

  __m128i expmant = _mm_and_si128(mask_nosign, h);
  __m128i justsign = _mm_xor_si128(h, expmant);
  __m128i shifted = _mm_slli_epi32(expmant, 13);
  __m128 scaled = _mm_mul_ps(_mm_castsi128_ps(shifted), magic_mult);
  __m128i b_wasinfnan = _mm_cmpgt_epi32(expmant, was_infnan);
  __m128i sign = _mm_slli_epi32(justsign, 16);
  __m128 infnanexp = _mm_and_ps(_mm_castsi128_ps(b_wasinfnan), exp_infnan);
  __m128i b_wasnan = _mm_cmpgt_epi32(expmant, was_nan);
  __m128i nanquiet = _mm_and_si128(b_wasnan, nan_quiet);
  __m128 infnandone = _mm_or_ps(infnanexp, _mm_castsi128_ps(nanquiet));

  __m128 sign_inf = _mm_or_ps(_mm_castsi128_ps(sign), infnandone);
  __m128 result = _mm_or_ps(scaled, sign_inf);

  return result;
}

#endif  // USE_SSE2_FP16

void blender::math::float_to_half_array(const float *src, uint16_t *dst, size_t length)
{
  size_t i = 0;
#if defined(USE_HARDWARE_FP16_F16C) /* 8-wide loop using AVX2 F16C */
  for (; i + 7 < length; i += 8) {
    __m256 src8 = _mm256_loadu_ps(src);
    __m128i h8 = _mm256_cvtps_ph(src8, _MM_FROUND_TO_NEAREST_INT);
    _mm_storeu_epi32(dst, h8);
    src += 8;
    dst += 8;
  }
#elif defined(USE_SSE2_FP16)          /* 4-wide loop using SSE2 */
  for (; i + 3 < length; i += 4) {
    __m128 src4 = _mm_loadu_ps(src);
    __m128i h4 = F32_to_F16_4x(src4);
    __m128i h4_packed = _mm_packs_epi32(h4, h4);
    _mm_storeu_si64(dst, h4_packed);
    src += 4;
    dst += 4;
  }
#elif defined(USE_HARDWARE_FP16_NEON) /* 4-wide loop using NEON */
  for (; i + 3 < length; i += 4) {
    float32x4_t src4 = vld1q_f32(src);
    float16x4_t h4 = vcvt_f16_f32(src4);
    vst1_f16((float16_t *)dst, h4);
    src += 4;
    dst += 4;
  }
#endif
  /* Use scalar path to convert the tail of array (or whole array if none of
   * wider paths above were used). */
  for (; i < length; i++) {
    *dst++ = float_to_half(*src++);
  }
}

void blender::math::float_to_half_make_finite_array(const float *src, uint16_t *dst, size_t length)
{
  size_t i = 0;
#if defined(USE_HARDWARE_FP16_F16C) /* 8-wide loop using AVX2 F16C */
  for (; i + 7 < length; i += 8) {
    __m256 src8 = _mm256_loadu_ps(src);
    __m128i h8 = _mm256_cvtps_ph(src8, _MM_FROUND_TO_NEAREST_INT);
    /* Handle inf/nan. */
    {
      const __m128i exp_mask = _mm_set1_epi16(0x7c00u);
      __m128i exp_all_ones = _mm_cmpeq_epi16(_mm_and_si128(h8, exp_mask), exp_mask);
      const __m128i mant_mask = _mm_set1_epi16(0x03ffu);
      const __m128i zero = _mm_setzero_si128();
      __m128i mant_is_zero = _mm_cmpeq_epi16(_mm_and_si128(h8, mant_mask), zero);
      __m128i is_inf = _mm_and_si128(exp_all_ones, mant_is_zero);
      const __m128i all_ones = _mm_cmpeq_epi16(zero, zero);
      __m128i is_nan = _mm_and_si128(exp_all_ones, _mm_andnot_si128(mant_is_zero, all_ones));
      const __m128i sign_mask = _mm_set1_epi16(0x8000u);
      __m128i signbits = _mm_and_si128(h8, sign_mask);
      __m128i inf_res = _mm_or_si128(signbits, _mm_set1_epi16(0x7bffu)); /* +/- 65504 */
      __m128i nan_res = signbits;                                        /* +/- 0 */
      /* Select final result. */
      h8 = _mm_blendv_epi8(h8, inf_res, is_inf);
      h8 = _mm_blendv_epi8(h8, nan_res, is_nan);
    }
    _mm_storeu_si128((__m128i *)dst, h8);
    src += 8;
    dst += 8;
  }
#elif defined(USE_SSE2_FP16)          /* 4-wide loop using SSE2 */
  for (; i + 3 < length; i += 4) {
    __m128 src4 = _mm_loadu_ps(src);
    __m128i h4 = F32_to_F16_4x(src4);
    /* Handle inf/nan. */
    {
      __m128i hi_part = _mm_and_si128(h4, _mm_set1_epi32(0xffff0000u));
      const __m128i exp_mask = _mm_set1_epi16(0x7c00u);
      __m128i exp_all_ones = _mm_cmpeq_epi16(_mm_and_si128(h4, exp_mask), exp_mask);
      const __m128i mant_mask = _mm_set1_epi16(0x03ffu);
      const __m128i zero = _mm_setzero_si128();
      __m128i mant_is_zero = _mm_cmpeq_epi16(_mm_and_si128(h4, mant_mask), zero);
      __m128i is_inf = _mm_and_si128(exp_all_ones, mant_is_zero);
      const __m128i all_ones = _mm_cmpeq_epi16(zero, zero);
      __m128i is_nan = _mm_and_si128(exp_all_ones, _mm_andnot_si128(mant_is_zero, all_ones));
      const __m128i sign_mask = _mm_set1_epi16(0x8000u);
      __m128i signbits = _mm_and_si128(h4, sign_mask);
      __m128i inf_res = _mm_or_si128(signbits, _mm_set1_epi16(0x7bffu)); /* +/- 65504 */
      __m128i nan_res = signbits;                                        /* +/- 0 */
      /* Select final result. */
      h4 = _mm_blendv_epi8(h4, inf_res, is_inf);
      h4 = _mm_blendv_epi8(h4, nan_res, is_nan);
      h4 = _mm_and_si128(h4, _mm_set1_epi32(0xffff));
      h4 = _mm_or_si128(h4, hi_part);
    }
    __m128i h4_packed = _mm_packs_epi32(h4, h4);
    _mm_storeu_si64(dst, h4_packed);
    src += 4;
    dst += 4;
  }
#elif defined(USE_HARDWARE_FP16_NEON) /* 4-wide loop using NEON */
  for (; i + 3 < length; i += 4) {
    float32x4_t src4 = vld1q_f32(src);
    float16x4_t h4 = vcvt_f16_f32(src4);
    /* Handle inf/nan. */
    {
      uint16x4_t hu4 = vreinterpret_u16_f16(h4);
      const uint16x4_t exp_mask = vdup_n_u16(0x7c00u);
      uint16x4_t exp_all_ones = vceq_u16(vand_u16(hu4, exp_mask), exp_mask);
      const uint16x4_t mant_mask = vdup_n_u16(0x03ffu);
      const uint16x4_t zero = vdup_n_u16(0);
      uint16x4_t mant_is_zero = vceq_u16(vand_u16(hu4, mant_mask), zero);
      uint16x4_t is_inf = vand_u16(exp_all_ones, mant_is_zero);
      uint16x4_t is_nan = vand_u16(exp_all_ones, vmvn_u16(mant_is_zero));
      const uint16x4_t sign_mask = vdup_n_u16(0x8000u);
      uint16x4_t signbits = vand_u16(hu4, sign_mask);
      uint16x4_t inf_res = vorr_u16(signbits, vdup_n_u16(0x7bffu)); /* +/- 65504 */
      uint16x4_t nan_res = signbits;                                /* +/- 0 */
      /* Select final result. */
      hu4 = vbsl_u16(is_inf, inf_res, hu4);
      hu4 = vbsl_u16(is_nan, nan_res, hu4);
      h4 = vreinterpret_f16_u16(hu4);
    }
    vst1_f16((float16_t *)dst, h4);
    src += 4;
    dst += 4;
  }
#endif
  /* Use scalar path to convert the tail of array (or whole array if none of
   * wider paths above were used). */
  for (; i < length; i++) {
    *dst++ = float_to_half_make_finite(*src++);
  }
}

void blender::math::half_to_float_array(const uint16_t *src, float *dst, size_t length)
{
  size_t i = 0;
#if defined(USE_HARDWARE_FP16_F16C) /* 8-wide loop using AVX2 F16C */
  for (; i + 7 < length; i += 8) {
    __m128i src8 = _mm_loadu_epi32(src);
    __m256 f8 = _mm256_cvtph_ps(src8);
    _mm256_storeu_ps(dst, f8);
    src += 8;
    dst += 8;
  }
#elif defined(USE_SSE2_FP16)          /* 4-wide loop using SSE2 */
  for (; i + 3 < length; i += 4) {
    __m128i src4 = _mm_loadu_si64(src);
    src4 = _mm_unpacklo_epi16(src4, src4);
    __m128 f4 = F16_to_F32_4x(src4);
    _mm_storeu_ps(dst, f4);
    src += 4;
    dst += 4;
  }
#elif defined(USE_HARDWARE_FP16_NEON) /* 4-wide loop using NEON */
  for (; i + 3 < length; i += 4) {
    float16x4_t src4 = vld1_f16((const float16_t *)src);
    float32x4_t f4 = vcvt_f32_f16(src4);
    vst1q_f32(dst, f4);
    src += 4;
    dst += 4;
  }
#endif
  /* Use scalar path to convert the tail of array (or whole array if none of
   * wider paths above were used). */
  for (; i < length; i++) {
    *dst++ = half_to_float(*src++);
  }
}

#ifdef USE_HARDWARE_FP16_NEON
#  undef USE_HARDWARE_FP16_NEON
#endif
#ifdef USE_HARDWARE_FP16_F16C
#  undef USE_HARDWARE_FP16_F16C
#endif
#ifdef USE_SSE2_FP16
#  undef USE_SSE2_FP16
#endif
