/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup bli
 */

#include "BLI_math_base.h"
#include "BLI_math_color.h"
#include "BLI_utildefines.h"

#include <math.h>

#ifndef __MATH_COLOR_INLINE_C__
#  define __MATH_COLOR_INLINE_C__

/******************************** Color Space ********************************/

#  ifdef BLI_HAVE_SSE2

MALWAYS_INLINE __m128 srgb_to_linearrgb_v4_simd(const __m128 c)
{
  __m128 cmp = _mm_cmplt_ps(c, _mm_set1_ps(0.04045f));
  __m128 lt = _mm_max_ps(_mm_mul_ps(c, _mm_set1_ps(1.0f / 12.92f)), _mm_set1_ps(0.0f));
  __m128 gtebase = _mm_mul_ps(_mm_add_ps(c, _mm_set1_ps(0.055f)),
                              _mm_set1_ps(1.0f / 1.055f)); /* FMA. */
  __m128 gte = _bli_math_fastpow24(gtebase);
  return _bli_math_blend_sse(cmp, lt, gte);
}

MALWAYS_INLINE __m128 linearrgb_to_srgb_v4_simd(const __m128 c)
{
  __m128 cmp = _mm_cmplt_ps(c, _mm_set1_ps(0.0031308f));
  __m128 lt = _mm_max_ps(_mm_mul_ps(c, _mm_set1_ps(12.92f)), _mm_set1_ps(0.0f));
  __m128 gte = _mm_add_ps(_mm_mul_ps(_mm_set1_ps(1.055f), _bli_math_fastpow512(c)),
                          _mm_set1_ps(-0.055f));
  return _bli_math_blend_sse(cmp, lt, gte);
}

MINLINE void srgb_to_linearrgb_v3_v3(float linear[3], const float srgb[3])
{
  float r[4] = {srgb[0], srgb[1], srgb[2], 1.0f};
  __m128 *rv = (__m128 *)&r;
  *rv = srgb_to_linearrgb_v4_simd(*rv);
  linear[0] = r[0];
  linear[1] = r[1];
  linear[2] = r[2];
}

MINLINE void linearrgb_to_srgb_v3_v3(float srgb[3], const float linear[3])
{
  float r[4] = {linear[0], linear[1], linear[2], 1.0f};
  __m128 *rv = (__m128 *)&r;
  *rv = linearrgb_to_srgb_v4_simd(*rv);
  srgb[0] = r[0];
  srgb[1] = r[1];
  srgb[2] = r[2];
}

#  else  /* BLI_HAVE_SSE2 */

MINLINE void srgb_to_linearrgb_v3_v3(float linear[3], const float srgb[3])
{
  linear[0] = srgb_to_linearrgb(srgb[0]);
  linear[1] = srgb_to_linearrgb(srgb[1]);
  linear[2] = srgb_to_linearrgb(srgb[2]);
}

MINLINE void linearrgb_to_srgb_v3_v3(float srgb[3], const float linear[3])
{
  srgb[0] = linearrgb_to_srgb(linear[0]);
  srgb[1] = linearrgb_to_srgb(linear[1]);
  srgb[2] = linearrgb_to_srgb(linear[2]);
}
#  endif /* BLI_HAVE_SSE2 */

MINLINE void srgb_to_linearrgb_v4(float linear[4], const float srgb[4])
{
  srgb_to_linearrgb_v3_v3(linear, srgb);
  linear[3] = srgb[3];
}

MINLINE void linearrgb_to_srgb_v4(float srgb[4], const float linear[4])
{
  linearrgb_to_srgb_v3_v3(srgb, linear);
  srgb[3] = linear[3];
}

MINLINE void linearrgb_to_srgb_uchar3(unsigned char srgb[3], const float linear[3])
{
  float srgb_f[3];

  linearrgb_to_srgb_v3_v3(srgb_f, linear);
  unit_float_to_uchar_clamp_v3(srgb, srgb_f);
}

MINLINE void linearrgb_to_srgb_uchar4(unsigned char srgb[4], const float linear[4])
{
  float srgb_f[4];

  linearrgb_to_srgb_v4(srgb_f, linear);
  unit_float_to_uchar_clamp_v4(srgb, srgb_f);
}

/* predivide versions to work on associated/pre-multiplied alpha. if this should
 * be done or not depends on the background the image will be composited over,
 * ideally you would never do color space conversion on an image with alpha
 * because it is ill defined */

MINLINE void srgb_to_linearrgb_predivide_v4(float linear[4], const float srgb[4])
{
  float alpha, inv_alpha;

  if (srgb[3] == 1.0f || srgb[3] == 0.0f) {
    alpha = 1.0f;
    inv_alpha = 1.0f;
  }
  else {
    alpha = srgb[3];
    inv_alpha = 1.0f / alpha;
  }

  linear[0] = srgb[0] * inv_alpha;
  linear[1] = srgb[1] * inv_alpha;
  linear[2] = srgb[2] * inv_alpha;
  linear[3] = srgb[3];
  srgb_to_linearrgb_v3_v3(linear, linear);
  linear[0] *= alpha;
  linear[1] *= alpha;
  linear[2] *= alpha;
}

MINLINE void linearrgb_to_srgb_predivide_v4(float srgb[4], const float linear[4])
{
  float alpha, inv_alpha;

  if (linear[3] == 1.0f || linear[3] == 0.0f) {
    alpha = 1.0f;
    inv_alpha = 1.0f;
  }
  else {
    alpha = linear[3];
    inv_alpha = 1.0f / alpha;
  }

  srgb[0] = linear[0] * inv_alpha;
  srgb[1] = linear[1] * inv_alpha;
  srgb[2] = linear[2] * inv_alpha;
  srgb[3] = linear[3];
  linearrgb_to_srgb_v3_v3(srgb, srgb);
  srgb[0] *= alpha;
  srgb[1] *= alpha;
  srgb[2] *= alpha;
}

/* LUT accelerated conversions */

extern float BLI_color_from_srgb_table[256];
extern unsigned short BLI_color_to_srgb_table[0x10000];

MINLINE unsigned short to_srgb_table_lookup(const float f)
{

  union {
    float f;
    unsigned short us[2];
  } tmp;
  tmp.f = f;
#  ifdef __BIG_ENDIAN__
  return BLI_color_to_srgb_table[tmp.us[0]];
#  else
  return BLI_color_to_srgb_table[tmp.us[1]];
#  endif
}

MINLINE void linearrgb_to_srgb_ushort4(unsigned short srgb[4], const float linear[4])
{
  srgb[0] = to_srgb_table_lookup(linear[0]);
  srgb[1] = to_srgb_table_lookup(linear[1]);
  srgb[2] = to_srgb_table_lookup(linear[2]);
  srgb[3] = unit_float_to_ushort_clamp(linear[3]);
}

MINLINE void srgb_to_linearrgb_uchar4(float linear[4], const unsigned char srgb[4])
{
  linear[0] = BLI_color_from_srgb_table[srgb[0]];
  linear[1] = BLI_color_from_srgb_table[srgb[1]];
  linear[2] = BLI_color_from_srgb_table[srgb[2]];
  linear[3] = srgb[3] * (1.0f / 255.0f);
}

MINLINE void srgb_to_linearrgb_uchar4_predivide(float linear[4], const unsigned char srgb[4])
{
  float fsrgb[4];
  int i;

  if (srgb[3] == 255 || srgb[3] == 0) {
    srgb_to_linearrgb_uchar4(linear, srgb);
    return;
  }

  for (i = 0; i < 4; i++) {
    fsrgb[i] = srgb[i] * (1.0f / 255.0f);
  }

  srgb_to_linearrgb_predivide_v4(linear, fsrgb);
}

MINLINE void rgba_uchar_args_set(
    uchar col[4], const uchar r, const uchar g, const uchar b, const uchar a)
{
  col[0] = r;
  col[1] = g;
  col[2] = b;
  col[3] = a;
}

MINLINE void rgba_float_args_set(
    float col[4], const float r, const float g, const float b, const float a)
{
  col[0] = r;
  col[1] = g;
  col[2] = b;
  col[3] = a;
}

MINLINE void rgba_uchar_args_test_set(
    uchar col[4], const uchar r, const uchar g, const uchar b, const uchar a)
{
  if (col[3] == 0) {
    col[0] = r;
    col[1] = g;
    col[2] = b;
    col[3] = a;
  }
}

MINLINE void cpack_cpy_3ub(unsigned char r_col[3], const unsigned int pack)
{
  r_col[0] = ((pack) >> 0) & 0xFF;
  r_col[1] = ((pack) >> 8) & 0xFF;
  r_col[2] = ((pack) >> 16) & 0xFF;
}

/* -------------------------------------------------------------------- */
/** \name RGB/Gray-Scale Functions
 *
 * \warning
 * These are only an approximation,
 * in almost _all_ cases, #IMB_colormanagement_get_luminance should be used instead. However for
 * screen-only colors which don't depend on the currently loaded profile - this is preferred.
 * Checking theme colors for contrast, etc. Basically anything outside the render pipeline.
 *
 * \{ */

MINLINE float rgb_to_grayscale(const float rgb[3])
{
  return (0.2126f * rgb[0]) + (0.7152f * rgb[1]) + (0.0722f * rgb[2]);
}

MINLINE unsigned char rgb_to_grayscale_byte(const unsigned char rgb[3])
{
  return (unsigned char)(((54 * (unsigned short)rgb[0]) + (182 * (unsigned short)rgb[1]) +
                          (19 * (unsigned short)rgb[2])) /
                         255);
}

/** \} */

MINLINE int compare_rgb_uchar(const unsigned char col_a[3],
                              const unsigned char col_b[3],
                              const int limit)
{
  const int r = (int)col_a[0] - (int)col_b[0];
  if (abs(r) < limit) {
    const int g = (int)col_a[1] - (int)col_b[1];
    if (abs(g) < limit) {
      const int b = (int)col_a[2] - (int)col_b[2];
      if (abs(b) < limit) {
        return 1;
      }
    }
  }

  return 0;
}

MINLINE float dither_random_value(float s, float t)
{
  /* Using a triangle distribution which gives a more final uniform noise.
   * See Banding in Games:A Noisy Rant(revision 5) Mikkel Gjøl, Playdead (slide 27) */

  /* Uniform noise in [0..1[ range, using common GLSL hash function.
   * https://stackoverflow.com/questions/12964279/whats-the-origin-of-this-glsl-rand-one-liner. */
  float hash0 = sinf(s * 12.9898f + t * 78.233f) * 43758.5453f;
  float hash1 = sinf(s * 19.9898f + t * 119.233f) * 43798.5453f;
  hash0 -= floorf(hash0);
  hash1 -= floorf(hash1);
  /* Convert uniform distribution into triangle-shaped distribution. */
  return hash0 + hash1 - 0.5f;
}

MINLINE void float_to_byte_dither_v3(
    unsigned char b[3], const float f[3], float dither, float s, float t)
{
  float dither_value = dither_random_value(s, t) * 0.0033f * dither;

  b[0] = unit_float_to_uchar_clamp(dither_value + f[0]);
  b[1] = unit_float_to_uchar_clamp(dither_value + f[1]);
  b[2] = unit_float_to_uchar_clamp(dither_value + f[2]);
}

/**************** Alpha Transformations *****************/

MINLINE void premul_to_straight_v4_v4(float straight[4], const float premul[4])
{
  if (premul[3] == 0.0f || premul[3] == 1.0f) {
    straight[0] = premul[0];
    straight[1] = premul[1];
    straight[2] = premul[2];
    straight[3] = premul[3];
  }
  else {
    const float alpha_inv = 1.0f / premul[3];
    straight[0] = premul[0] * alpha_inv;
    straight[1] = premul[1] * alpha_inv;
    straight[2] = premul[2] * alpha_inv;
    straight[3] = premul[3];
  }
}

MINLINE void premul_to_straight_v4(float color[4])
{
  premul_to_straight_v4_v4(color, color);
}

MINLINE void straight_to_premul_v4_v4(float premul[4], const float straight[4])
{
  const float alpha = straight[3];
  premul[0] = straight[0] * alpha;
  premul[1] = straight[1] * alpha;
  premul[2] = straight[2] * alpha;
  premul[3] = straight[3];
}

MINLINE void straight_to_premul_v4(float color[4])
{
  straight_to_premul_v4_v4(color, color);
}

MINLINE void straight_uchar_to_premul_float(float result[4], const unsigned char color[4])
{
  const float alpha = color[3] * (1.0f / 255.0f);
  const float fac = alpha * (1.0f / 255.0f);

  result[0] = color[0] * fac;
  result[1] = color[1] * fac;
  result[2] = color[2] * fac;
  result[3] = alpha;
}

MINLINE void premul_float_to_straight_uchar(unsigned char *result, const float color[4])
{
  if (color[3] == 0.0f || color[3] == 1.0f) {
    result[0] = unit_float_to_uchar_clamp(color[0]);
    result[1] = unit_float_to_uchar_clamp(color[1]);
    result[2] = unit_float_to_uchar_clamp(color[2]);
    result[3] = unit_float_to_uchar_clamp(color[3]);
  }
  else {
    const float alpha_inv = 1.0f / color[3];

    /* hopefully this would be optimized */
    result[0] = unit_float_to_uchar_clamp(color[0] * alpha_inv);
    result[1] = unit_float_to_uchar_clamp(color[1] * alpha_inv);
    result[2] = unit_float_to_uchar_clamp(color[2] * alpha_inv);
    result[3] = unit_float_to_uchar_clamp(color[3]);
  }
}

#endif /* __MATH_COLOR_INLINE_C__ */
