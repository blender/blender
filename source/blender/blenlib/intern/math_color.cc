/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_math_color.h"
#include "BLI_math_color.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_simd.hh"
#include "BLI_utildefines.h"

#include <algorithm>
#include <cstring>

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

void hsv_to_rgb(float h, float s, float v, float *r_r, float *r_g, float *r_b)
{
  float nr, ng, nb;

  nr = fabsf(h * 6.0f - 3.0f) - 1.0f;
  ng = 2.0f - fabsf(h * 6.0f - 2.0f);
  nb = 2.0f - fabsf(h * 6.0f - 4.0f);

  CLAMP(nr, 0.0f, 1.0f);
  CLAMP(nb, 0.0f, 1.0f);
  CLAMP(ng, 0.0f, 1.0f);

  *r_r = ((nr - 1.0f) * s + 1.0f) * v;
  *r_g = ((ng - 1.0f) * s + 1.0f) * v;
  *r_b = ((nb - 1.0f) * s + 1.0f) * v;
}

void hsl_to_rgb(float h, float s, float l, float *r_r, float *r_g, float *r_b)
{
  float nr, ng, nb, chroma;

  nr = fabsf(h * 6.0f - 3.0f) - 1.0f;
  ng = 2.0f - fabsf(h * 6.0f - 2.0f);
  nb = 2.0f - fabsf(h * 6.0f - 4.0f);

  CLAMP(nr, 0.0f, 1.0f);
  CLAMP(nb, 0.0f, 1.0f);
  CLAMP(ng, 0.0f, 1.0f);

  chroma = (1.0f - fabsf(2.0f * l - 1.0f)) * s;

  *r_r = (nr - 0.5f) * chroma + l;
  *r_g = (ng - 0.5f) * chroma + l;
  *r_b = (nb - 0.5f) * chroma + l;
}

void hsv_to_rgb_v(const float hsv[3], float r_rgb[3])
{
  hsv_to_rgb(hsv[0], hsv[1], hsv[2], &r_rgb[0], &r_rgb[1], &r_rgb[2]);
}

void hsl_to_rgb_v(const float hsl[3], float r_rgb[3])
{
  hsl_to_rgb(hsl[0], hsl[1], hsl[2], &r_rgb[0], &r_rgb[1], &r_rgb[2]);
}

void rgb_to_yuv(float r, float g, float b, float *r_y, float *r_u, float *r_v, int colorspace)
{
  float y, u, v;

  switch (colorspace) {
    case BLI_YUV_ITU_BT601:
      y = 0.299f * r + 0.587f * g + 0.114f * b;
      u = -0.147f * r - 0.289f * g + 0.436f * b;
      v = 0.615f * r - 0.515f * g - 0.100f * b;
      break;
    case BLI_YUV_ITU_BT709:
    default:
      BLI_assert(colorspace == BLI_YUV_ITU_BT709);
      y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
      u = -0.09991f * r - 0.33609f * g + 0.436f * b;
      v = 0.615f * r - 0.55861f * g - 0.05639f * b;
      break;
  }

  *r_y = y;
  *r_u = u;
  *r_v = v;
}

void yuv_to_rgb(float y, float u, float v, float *r_r, float *r_g, float *r_b, int colorspace)
{
  float r, g, b;

  switch (colorspace) {
    case BLI_YUV_ITU_BT601:
      r = y + 1.140f * v;
      g = y - 0.394f * u - 0.581f * v;
      b = y + 2.032f * u;
      break;
    case BLI_YUV_ITU_BT709:
    default:
      BLI_assert(colorspace == BLI_YUV_ITU_BT709);
      r = y + 1.28033f * v;
      g = y - 0.21482f * u - 0.38059f * v;
      b = y + 2.12798f * u;
      break;
  }

  *r_r = r;
  *r_g = g;
  *r_b = b;
}

void rgb_to_ycc(float r, float g, float b, float *r_y, float *r_cb, float *r_cr, int colorspace)
{
  float sr, sg, sb;
  float y = 128.0f, cr = 128.0f, cb = 128.0f;

  sr = 255.0f * r;
  sg = 255.0f * g;
  sb = 255.0f * b;

  switch (colorspace) {
    case BLI_YCC_ITU_BT601:
      y = (0.257f * sr) + (0.504f * sg) + (0.098f * sb) + 16.0f;
      cb = (-0.148f * sr) - (0.291f * sg) + (0.439f * sb) + 128.0f;
      cr = (0.439f * sr) - (0.368f * sg) - (0.071f * sb) + 128.0f;
      break;
    case BLI_YCC_ITU_BT709:
      y = (0.183f * sr) + (0.614f * sg) + (0.062f * sb) + 16.0f;
      cb = (-0.101f * sr) - (0.338f * sg) + (0.439f * sb) + 128.0f;
      cr = (0.439f * sr) - (0.399f * sg) - (0.040f * sb) + 128.0f;
      break;
    case BLI_YCC_JFIF_0_255:
      y = (0.299f * sr) + (0.587f * sg) + (0.114f * sb);
      cb = (-0.16874f * sr) - (0.33126f * sg) + (0.5f * sb) + 128.0f;
      cr = (0.5f * sr) - (0.41869f * sg) - (0.08131f * sb) + 128.0f;
      break;
    default:
      BLI_assert_msg(0, "invalid colorspace");
      break;
  }

  *r_y = y;
  *r_cb = cb;
  *r_cr = cr;
}

void ycc_to_rgb(float y, float cb, float cr, float *r_r, float *r_g, float *r_b, int colorspace)
{
  /* FIXME the following comment must be wrong because:
   * BLI_YCC_ITU_BT601 y 16.0 cr 16.0 -> r -0.7009. */

  /* YCC input have a range of 16-235 and 16-240 except with JFIF_0_255 where the range is 0-255
   * RGB outputs are in the range 0 - 1.0f. */

  float r = 128.0f, g = 128.0f, b = 128.0f;

  switch (colorspace) {
    case BLI_YCC_ITU_BT601:
      r = 1.164f * (y - 16.0f) + 1.596f * (cr - 128.0f);
      g = 1.164f * (y - 16.0f) - 0.813f * (cr - 128.0f) - 0.392f * (cb - 128.0f);
      b = 1.164f * (y - 16.0f) + 2.017f * (cb - 128.0f);
      break;
    case BLI_YCC_ITU_BT709:
      r = 1.164f * (y - 16.0f) + 1.793f * (cr - 128.0f);
      g = 1.164f * (y - 16.0f) - 0.534f * (cr - 128.0f) - 0.213f * (cb - 128.0f);
      b = 1.164f * (y - 16.0f) + 2.115f * (cb - 128.0f);
      break;
    case BLI_YCC_JFIF_0_255:
      r = y + 1.402f * cr - 179.456f;
      g = y - 0.34414f * cb - 0.71414f * cr + 135.45984f;
      b = y + 1.772f * cb - 226.816f;
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
  *r_r = r / 255.0f;
  *r_g = g / 255.0f;
  *r_b = b / 255.0f;
}

void hex_to_rgb(const char *hexcol, float *r_r, float *r_g, float *r_b)
{
  hex_to_rgba(hexcol, r_r, r_g, r_b, nullptr);
}

void hex_to_rgba(const char *hexcol, float *r_r, float *r_g, float *r_b, float *r_a)
{
  uint ri, gi, bi, ai;
  bool has_alpha = false;

  if (hexcol[0] == '#') {
    hexcol++;
  }

  if (sscanf(hexcol, "%02x%02x%02x%02x", &ri, &gi, &bi, &ai) == 4) {
    /* height digit hex colors with alpha */
    has_alpha = true;
  }
  else if (sscanf(hexcol, "%02x%02x%02x", &ri, &gi, &bi) == 3) {
    /* six digit hex colors */
  }
  else if (sscanf(hexcol, "%01x%01x%01x", &ri, &gi, &bi) == 3) {
    /* three digit hex colors (#123 becomes #112233) */
    ri += ri << 4;
    gi += gi << 4;
    bi += bi << 4;
  }
  else {
    /* avoid using un-initialized vars */
    *r_r = *r_g = *r_b = 0.0f;
    if (r_a) {
      *r_a = 0.0f;
    }
    return;
  }

  *r_r = float(ri) * (1.0f / 255.0f);
  *r_g = float(gi) * (1.0f / 255.0f);
  *r_b = float(bi) * (1.0f / 255.0f);
  CLAMP(*r_r, 0.0f, 1.0f);
  CLAMP(*r_g, 0.0f, 1.0f);
  CLAMP(*r_b, 0.0f, 1.0f);

  if (r_a && has_alpha) {
    *r_a = float(ai) * (1.0f / 255.0f);
    CLAMP(*r_a, 0.0f, 1.0f);
  }
}

void rgb_to_hsv(float r, float g, float b, float *r_h, float *r_s, float *r_v)
{
  float k = 0.0f;
  float chroma;
  float min_gb;

  if (g < b) {
    SWAP(float, g, b);
    k = -1.0f;
  }
  min_gb = b;
  if (r < g) {
    SWAP(float, r, g);
    k = -2.0f / 6.0f - k;
    min_gb = min_ff(g, b);
  }

  chroma = r - min_gb;

  *r_h = fabsf(k + (g - b) / (6.0f * chroma + 1e-20f));
  *r_s = chroma / (r + 1e-20f);
  *r_v = r;
}

void rgb_to_hsv_v(const float rgb[3], float r_hsv[3])
{
  rgb_to_hsv(rgb[0], rgb[1], rgb[2], &r_hsv[0], &r_hsv[1], &r_hsv[2]);
}

void rgb_to_hsl(float r, float g, float b, float *r_h, float *r_s, float *r_l)
{
  const float cmax = max_fff(r, g, b);
  const float cmin = min_fff(r, g, b);
  float h, s, l = min_ff(1.0f, (cmax + cmin) / 2.0f);

  if (cmax == cmin) {
    h = s = 0.0f; /* achromatic */
  }
  else {
    float d = cmax - cmin;
    s = l > 0.5f ? d / (2.0f - cmax - cmin) : d / (cmax + cmin);
    if (cmax == r) {
      h = (g - b) / d + (g < b ? 6.0f : 0.0f);
    }
    else if (cmax == g) {
      h = (b - r) / d + 2.0f;
    }
    else {
      h = (r - g) / d + 4.0f;
    }
  }
  h /= 6.0f;

  *r_h = h;
  *r_s = s;
  *r_l = l;
}

void rgb_to_hsl_compat(float r, float g, float b, float *r_h, float *r_s, float *r_l)
{
  /* Convert RGB to HSL, while staying as close as possible to existing HSL values.
   * Uses a threshold as there can be small errors introduced by color space conversions
   * or other operations. */
  const float orig_s = *r_s;
  const float orig_h = *r_h;
  const float threshold = 1e-5f;

  rgb_to_hsl(r, g, b, r_h, r_s, r_l);

  /* For (near) zero lightness or saturation, keep the other values unchanged,
   * as they are either undefined or very sensitive to small lightness changes. */
  if (*r_l <= threshold) {
    *r_h = orig_h;
    *r_s = orig_s;
  }
  else if (*r_s <= threshold) {
    *r_h = orig_h;
    *r_s = orig_s;
  }

  /* Hue wraps around, keep it on the same side. */
  if (fabsf(*r_h) <= threshold && fabsf(orig_h - 1.0f) <= threshold) {
    *r_h = 1.0f;
  }
  else if (fabsf(*r_h - 1.0f) <= threshold && fabsf(orig_h) <= threshold) {
    *r_h = 0.0f;
  }
}

void rgb_to_hsl_compat_v(const float rgb[3], float r_hsl[3])
{
  rgb_to_hsl_compat(rgb[0], rgb[1], rgb[2], &r_hsl[0], &r_hsl[1], &r_hsl[2]);
}

void rgb_to_hsl_v(const float rgb[3], float r_hsl[3])
{
  rgb_to_hsl(rgb[0], rgb[1], rgb[2], &r_hsl[0], &r_hsl[1], &r_hsl[2]);
}

void rgb_to_hsv_compat(float r, float g, float b, float *r_h, float *r_s, float *r_v)
{
  /* Convert RGB to HSV, while staying as close as possible to existing HSV values.
   * Uses a threshold as there can be small errors introduced by color space conversions
   * or other operations. */
  const float orig_h = *r_h;
  const float orig_s = *r_s;
  const float threshold = 1e-5f;

  rgb_to_hsv(r, g, b, r_h, r_s, r_v);

  /* For (near) zero values or saturation, keep the other values unchanged,
   * as they are either undefined or very sensitive to small value changes. */
  if (*r_v <= threshold) {
    *r_h = orig_h;
    *r_s = orig_s;
  }
  else if (*r_s <= threshold) {
    *r_h = orig_h;
  }

  /* Hue wraps around, keep it on the same side. */
  if (fabsf(*r_h) <= threshold && fabsf(orig_h - 1.0f) <= threshold) {
    *r_h = 1.0f;
  }
  else if (fabsf(*r_h - 1.0f) <= threshold && fabsf(orig_h) <= threshold) {
    *r_h = 0.0f;
  }
}

void rgb_to_hsv_compat_v(const float rgb[3], float r_hsv[3])
{
  rgb_to_hsv_compat(rgb[0], rgb[1], rgb[2], &r_hsv[0], &r_hsv[1], &r_hsv[2]);
}

void hsv_clamp_v(float hsv[3], float v_max)
{
  if (UNLIKELY(hsv[0] < 0.0f || hsv[0] > 1.0f)) {
    hsv[0] = hsv[0] - floorf(hsv[0]);
  }
  CLAMP(hsv[1], 0.0f, 1.0f);
  CLAMP(hsv[2], 0.0f, v_max);
}

uint hsv_to_cpack(float h, float s, float v)
{
  uint r, g, b;
  float rf, gf, bf;
  uint col;

  hsv_to_rgb(h, s, v, &rf, &gf, &bf);

  r = uint(rf * 255.0f);
  g = uint(gf * 255.0f);
  b = uint(bf * 255.0f);

  col = (r + (g * 256) + (b * 256 * 256));
  return col;
}

uint rgb_to_cpack(float r, float g, float b)
{
  uint ir, ig, ib;

  ir = uint(floorf(255.0f * max_ff(r, 0.0f)));
  ig = uint(floorf(255.0f * max_ff(g, 0.0f)));
  ib = uint(floorf(255.0f * max_ff(b, 0.0f)));

  ir = std::min<uint>(ir, 255);
  ig = std::min<uint>(ig, 255);
  ib = std::min<uint>(ib, 255);

  return (ir + (ig * 256) + (ib * 256 * 256));
}

void cpack_to_rgb(uint col, float *r_r, float *r_g, float *r_b)
{
  *r_r = float(col & 0xFF) * (1.0f / 255.0f);
  *r_g = float((col >> 8) & 0xFF) * (1.0f / 255.0f);
  *r_b = float((col >> 16) & 0xFF) * (1.0f / 255.0f);
}

/* ********************************* color transforms ********************************* */

float srgb_to_linearrgb(float c)
{
  if (c < 0.04045f) {
    return (c < 0.0f) ? 0.0f : c * (1.0f / 12.92f);
  }

  return powf((c + 0.055f) * (1.0f / 1.055f), 2.4f);
}

float linearrgb_to_srgb(float c)
{
  if (c < 0.0031308f) {
    return (c < 0.0f) ? 0.0f : c * 12.92f;
  }

  return 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

/* SIMD code path, with pow 2.4 and 1/2.4 approximations. */
#if BLI_HAVE_SSE2

/**
 * Calculate initial guess for `arg^exp` based on float representation
 * This method gives a constant bias, which can be easily compensated by
 * multiplying with bias_coeff.
 * Gives better results for exponents near 1 (e.g. `4/5`).
 * exp = exponent, encoded as uint32_t
 * `e2coeff = 2^(127/exponent - 127) * bias_coeff^(1/exponent)`, encoded as `uint32_t`.
 *
 * We hope that exp and e2coeff gets properly inlined.
 */
MALWAYS_INLINE __m128 _bli_math_fastpow(const int exp, const int e2coeff, const __m128 arg)
{
  __m128 ret;
  ret = _mm_mul_ps(arg, _mm_castsi128_ps(_mm_set1_epi32(e2coeff)));
  ret = _mm_cvtepi32_ps(_mm_castps_si128(ret));
  ret = _mm_mul_ps(ret, _mm_castsi128_ps(_mm_set1_epi32(exp)));
  ret = _mm_castsi128_ps(_mm_cvtps_epi32(ret));
  return ret;
}

/** Improve `x ^ 1.0f/5.0f` solution with Newton-Raphson method */
MALWAYS_INLINE __m128 _bli_math_improve_5throot_solution(const __m128 old_result, const __m128 x)
{
  __m128 approx2 = _mm_mul_ps(old_result, old_result);
  __m128 approx4 = _mm_mul_ps(approx2, approx2);
  __m128 t = _mm_div_ps(x, approx4);
  __m128 summ = _mm_add_ps(_mm_mul_ps(_mm_set1_ps(4.0f), old_result), t); /* FMA. */
  return _mm_mul_ps(summ, _mm_set1_ps(1.0f / 5.0f));
}

/** Calculate `powf(x, 2.4)`. Working domain: `1e-10 < x < 1e+10`. */
MALWAYS_INLINE __m128 _bli_math_fastpow24(const __m128 arg)
{
  /* max, avg and |avg| errors were calculated in GCC without FMA instructions
   * The final precision should be better than `powf` in GLIBC. */

  /* Calculate x^4/5, coefficient 0.994 was constructed manually to minimize
   * avg error.
   */
  /* 0x3F4CCCCD = 4/5 */
  /* 0x4F55A7FB = 2^(127/(4/5) - 127) * 0.994^(1/(4/5)) */
  /* error max = 0.17, avg = 0.0018, |avg| = 0.05 */
  __m128 x = _bli_math_fastpow(0x3F4CCCCD, 0x4F55A7FB, arg);
  __m128 arg2 = _mm_mul_ps(arg, arg);
  __m128 arg4 = _mm_mul_ps(arg2, arg2);
  /* error max = 0.018        avg = 0.0031    |avg| = 0.0031 */
  x = _bli_math_improve_5throot_solution(x, arg4);
  /* error max = 0.00021    avg = 1.6e-05    |avg| = 1.6e-05 */
  x = _bli_math_improve_5throot_solution(x, arg4);
  /* error max = 6.1e-07    avg = 5.2e-08    |avg| = 1.1e-07 */
  x = _bli_math_improve_5throot_solution(x, arg4);
  return _mm_mul_ps(x, _mm_mul_ps(x, x));
}

MALWAYS_INLINE __m128 _bli_math_rsqrt(__m128 in)
{
  __m128 r = _mm_rsqrt_ps(in);
  /* Only do additional Newton-Raphson iterations when using actual SSE
   * code path. When we are emulating SSE on NEON via sse2neon, the
   * additional NR iterations are already done inside _mm_rsqrt_ps
   * emulation. */
#  if defined(__SSE2__)
  r = _mm_add_ps(_mm_mul_ps(_mm_set1_ps(1.5f), r),
                 _mm_mul_ps(_mm_mul_ps(_mm_mul_ps(in, _mm_set1_ps(-0.5f)), r), _mm_mul_ps(r, r)));
#  endif
  return r;
}

/* Calculate `powf(x, 1.0f / 2.4)`. */
MALWAYS_INLINE __m128 _bli_math_fastpow512(const __m128 arg)
{
  /* 5/12 is too small, so compute the 4th root of 20/12 instead.
   * 20/12 = 5/3 = 1 + 2/3 = 2 - 1/3. 2/3 is a suitable argument for fastpow.
   * weighting coefficient: a^-1/2 = 2 a; a = 2^-2/3
   */
  __m128 xf = _bli_math_fastpow(0x3f2aaaab, 0x5eb504f3, arg);
  __m128 xover = _mm_mul_ps(arg, xf);
  __m128 xfm1 = _bli_math_rsqrt(xf);
  __m128 x2 = _mm_mul_ps(arg, arg);
  __m128 xunder = _mm_mul_ps(x2, xfm1);
  /* sqrt2 * over + 2 * sqrt2 * under */
  __m128 xavg = _mm_mul_ps(_mm_set1_ps(1.0f / (3.0f * 0.629960524947437f) * 0.999852f),
                           _mm_add_ps(xover, xunder));
  xavg = _mm_mul_ps(xavg, _bli_math_rsqrt(xavg));
  xavg = _mm_mul_ps(xavg, _bli_math_rsqrt(xavg));
  return xavg;
}

MALWAYS_INLINE __m128 _bli_math_blend_sse(const __m128 mask, const __m128 a, const __m128 b)
{
#  if BLI_HAVE_SSE4
  return _mm_blendv_ps(b, a, mask);
#  else
  return _mm_or_ps(_mm_and_ps(mask, a), _mm_andnot_ps(mask, b));
#  endif
}

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

void srgb_to_linearrgb_v3_v3(float linear[3], const float srgb[3])
{
  float r[4] = {srgb[0], srgb[1], srgb[2], 1.0f};
  __m128 *rv = (__m128 *)&r;
  *rv = srgb_to_linearrgb_v4_simd(*rv);
  linear[0] = r[0];
  linear[1] = r[1];
  linear[2] = r[2];
}

void linearrgb_to_srgb_v3_v3(float srgb[3], const float linear[3])
{
  float r[4] = {linear[0], linear[1], linear[2], 1.0f};
  __m128 *rv = (__m128 *)&r;
  *rv = linearrgb_to_srgb_v4_simd(*rv);
  srgb[0] = r[0];
  srgb[1] = r[1];
  srgb[2] = r[2];
}

#else /* BLI_HAVE_SSE2 */

/* Non-SIMD code path, with the same pow approximations as SIMD one. */

MALWAYS_INLINE float int_as_float(int32_t v)
{
  float r;
  memcpy(&r, &v, sizeof(v));
  return r;
}

MALWAYS_INLINE int32_t float_as_int(float v)
{
  int32_t r;
  memcpy(&r, &v, sizeof(v));
  return r;
}

MALWAYS_INLINE float _bli_math_fastpow(const int exp, const int e2coeff, const float arg)
{
  float ret = arg * int_as_float(e2coeff);
  ret = float(float_as_int(ret));
  ret = ret * int_as_float(exp);
  ret = int_as_float(int(ret));
  return ret;
}

MALWAYS_INLINE float _bli_math_improve_5throot_solution(const float old_result, const float x)
{
  float approx2 = old_result * old_result;
  float approx4 = approx2 * approx2;
  float t = x / approx4;
  float summ = 4.0f * old_result + t;
  return summ * (1.0f / 5.0f);
}

MALWAYS_INLINE float _bli_math_fastpow24(const float arg)
{
  float x = _bli_math_fastpow(0x3F4CCCCD, 0x4F55A7FB, arg);
  float arg2 = arg * arg;
  float arg4 = arg2 * arg2;
  x = _bli_math_improve_5throot_solution(x, arg4);
  x = _bli_math_improve_5throot_solution(x, arg4);
  x = _bli_math_improve_5throot_solution(x, arg4);
  return x * x * x;
}

MALWAYS_INLINE float _bli_math_rsqrt(float in)
{
  return 1.0f / sqrtf(in);
}

MALWAYS_INLINE float _bli_math_fastpow512(const float arg)
{
  float xf = _bli_math_fastpow(0x3f2aaaab, 0x5eb504f3, arg);
  float xover = arg * xf;
  float xfm1 = _bli_math_rsqrt(xf);
  float x2 = arg * arg;
  float xunder = x2 * xfm1;
  float xavg = (1.0f / (3.0f * 0.629960524947437f) * 0.999852f) * (xover + xunder);
  xavg = xavg * _bli_math_rsqrt(xavg);
  xavg = xavg * _bli_math_rsqrt(xavg);
  return xavg;
}

MALWAYS_INLINE float srgb_to_linearrgb_approx(float c)
{
  if (c < 0.04045f) {
    return (c < 0.0f) ? 0.0f : c * (1.0f / 12.92f);
  }

  return _bli_math_fastpow24((c + 0.055f) * (1.0f / 1.055f));
}

MALWAYS_INLINE float linearrgb_to_srgb_approx(float c)
{
  if (c < 0.0031308f) {
    return (c < 0.0f) ? 0.0f : c * 12.92f;
  }

  return 1.055f * _bli_math_fastpow512(c) - 0.055f;
}

void srgb_to_linearrgb_v3_v3(float linear[3], const float srgb[3])
{
  linear[0] = srgb_to_linearrgb_approx(srgb[0]);
  linear[1] = srgb_to_linearrgb_approx(srgb[1]);
  linear[2] = srgb_to_linearrgb_approx(srgb[2]);
}

void linearrgb_to_srgb_v3_v3(float srgb[3], const float linear[3])
{
  srgb[0] = linearrgb_to_srgb_approx(linear[0]);
  srgb[1] = linearrgb_to_srgb_approx(linear[1]);
  srgb[2] = linearrgb_to_srgb_approx(linear[2]);
}

#endif /* BLI_HAVE_SSE2 */

/* ************************************* other ************************************************* */

void rgb_float_set_hue_float_offset(float rgb[3], float hue_offset)
{
  float hsv[3];

  rgb_to_hsv(rgb[0], rgb[1], rgb[2], hsv, hsv + 1, hsv + 2);

  hsv[0] += hue_offset;
  if (hsv[0] > 1.0f) {
    hsv[0] -= 1.0f;
  }
  else if (hsv[0] < 0.0f) {
    hsv[0] += 1.0f;
  }

  hsv_to_rgb(hsv[0], hsv[1], hsv[2], rgb, rgb + 1, rgb + 2);
}

void rgb_byte_set_hue_float_offset(uchar rgb[3], float hue_offset)
{
  float rgb_float[3];

  rgb_uchar_to_float(rgb_float, rgb);
  rgb_float_set_hue_float_offset(rgb_float, hue_offset);
  rgb_float_to_uchar(rgb, rgb_float);
}

/* fast sRGB conversion
 * LUT from linear float to 16-bit short
 * based on http://mysite.verizon.net/spitzak/conversion/
 */

float BLI_color_from_srgb_table[256];
ushort BLI_color_to_srgb_table[0x10000];

static ushort hipart(const float f)
{
  union {
    float f;
    ushort us[2];
  } tmp;

  tmp.f = f;

  /* NOTE: this is endianness-sensitive. */
  return tmp.us[1];
}

static float index_to_float(const ushort i)
{

  union {
    float f;
    ushort us[2];
  } tmp;

  /* positive and negative zeros, and all gradual underflow, turn into zero: */
  if (i < 0x80 || (i >= 0x8000 && i < 0x8080)) {
    return 0;
  }
  /* All NaN's and infinity turn into the largest possible legal float: */
  if (i >= 0x7f80 && i < 0x8000) {
    return FLT_MAX;
  }
  if (i >= 0xff80) {
    return -FLT_MAX;
  }

  /* NOTE: this is endianness-sensitive. */
  tmp.us[0] = 0x8000;
  tmp.us[1] = i;

  return tmp.f;
}

void BLI_init_srgb_conversion()
{
  static bool initialized = false;
  uint i, b;

  if (initialized) {
    return;
  }
  initialized = true;

  /* Fill in the lookup table to convert floats to bytes: */
  for (i = 0; i < 0x10000; i++) {
    float f = linearrgb_to_srgb(index_to_float(ushort(i))) * 255.0f;
    if (f <= 0) {
      BLI_color_to_srgb_table[i] = 0;
    }
    else if (f < 255) {
      BLI_color_to_srgb_table[i] = ushort(f * 0x100 + 0.5f);
    }
    else {
      BLI_color_to_srgb_table[i] = 0xff00;
    }
  }

  /* Fill in the lookup table to convert bytes to float: */
  for (b = 0; b <= 255; b++) {
    float f = srgb_to_linearrgb(float(b) * (1.0f / 255.0f));
    BLI_color_from_srgb_table[b] = f;
    i = hipart(f);
    /* replace entries so byte->float->byte does not change the data: */
    BLI_color_to_srgb_table[i] = ushort(b * 0x100);
  }
}

namespace blender::math {

struct locus_entry_t {
  float mired; /* Inverse temperature */
  float2 uv;   /* CIE 1960 uv coordinates */
  float t;     /* Isotherm parameter */
  float dist(const float2 p) const
  {
    const float2 diff = p - uv;
    return diff.y - t * diff.x;
  }
};

/* Tabulated approximation of the Planckian locus.
 * Based on http://www.brucelindbloom.com/Eqn_XYZ_to_T.html.
 * Original source:
 * "Color Science: Concepts and Methods, Quantitative Data and Formulae", Second Edition,
 * Gunter Wyszecki and W. S. Stiles, John Wiley & Sons, 1982, pp. 227, 228. */
static const std::array<locus_entry_t, 31> planck_locus{{
    {0.0f, {0.18006f, 0.26352f}, -0.24341f},   {10.0f, {0.18066f, 0.26589f}, -0.25479f},
    {20.0f, {0.18133f, 0.26846f}, -0.26876f},  {30.0f, {0.18208f, 0.27119f}, -0.28539f},
    {40.0f, {0.18293f, 0.27407f}, -0.30470f},  {50.0f, {0.18388f, 0.27709f}, -0.32675f},
    {60.0f, {0.18494f, 0.28021f}, -0.35156f},  {70.0f, {0.18611f, 0.28342f}, -0.37915f},
    {80.0f, {0.18740f, 0.28668f}, -0.40955f},  {90.0f, {0.18880f, 0.28997f}, -0.44278f},
    {100.0f, {0.19032f, 0.29326f}, -0.47888f}, {125.0f, {0.19462f, 0.30141f}, -0.58204f},
    {150.0f, {0.19962f, 0.30921f}, -0.70471f}, {175.0f, {0.20525f, 0.31647f}, -0.84901f},
    {200.0f, {0.21142f, 0.32312f}, -1.0182f},  {225.0f, {0.21807f, 0.32909f}, -1.2168f},
    {250.0f, {0.22511f, 0.33439f}, -1.4512f},  {275.0f, {0.23247f, 0.33904f}, -1.7298f},
    {300.0f, {0.24010f, 0.34308f}, -2.0637f},  {325.0f, {0.24792f, 0.34655f}, -2.4681f},
    {350.0f, {0.25591f, 0.34951f}, -2.9641f},  {375.0f, {0.26400f, 0.35200f}, -3.5814f},
    {400.0f, {0.27218f, 0.35407f}, -4.3633f},  {425.0f, {0.28039f, 0.35577f}, -5.3762f},
    {450.0f, {0.28863f, 0.35714f}, -6.7262f},  {475.0f, {0.29685f, 0.35823f}, -8.5955f},
    {500.0f, {0.30505f, 0.35907f}, -11.324f},  {525.0f, {0.31320f, 0.35968f}, -15.628f},
    {550.0f, {0.32129f, 0.36011f}, -23.325f},  {575.0f, {0.32931f, 0.36038f}, -40.770f},
    {600.0f, {0.33724f, 0.36051f}, -116.45f},
}};

bool whitepoint_to_temp_tint(const float3 &white, float &temperature, float &tint)
{
  /* Convert XYZ -> CIE 1960 uv. */
  const float2 uv = float2{4.0f * white.x, 6.0f * white.y} / dot(white, {1.0f, 15.0f, 3.0f});

  /* Find first entry that's "to the right" of the white point. */
  auto check = [uv](const float val, const locus_entry_t &entry) { return entry.dist(uv) < val; };
  const auto entry = std::upper_bound(planck_locus.begin(), planck_locus.end(), 0.0f, check);
  if (entry == planck_locus.begin() || entry == planck_locus.end()) {
    return false;
  }
  const size_t i = size_t(entry - planck_locus.begin());
  const locus_entry_t &low = planck_locus[i - 1], high = planck_locus[i];

  /* Find closest point on locus. */
  const float d_low = low.dist(uv) / sqrtf(1.0f + low.t * low.t);
  const float d_high = high.dist(uv) / sqrtf(1.0f + high.t * high.t);
  const float f = d_low / (d_low - d_high);

  /* Find tint based on distance to closest point on locus. */
  const float2 uv_temp = interpolate(low.uv, high.uv, f);
  const float abs_tint = length(uv - uv_temp) * 3000.0f;
  if (abs_tint > 150.0f) {
    return false;
  }

  temperature = 1e6f / interpolate(low.mired, high.mired, f);
  tint = abs_tint * ((uv.x < uv_temp.x) ? 1.0f : -1.0f);
  return true;
}

float3 whitepoint_from_temp_tint(const float temperature, const float tint)
{
  /* Find table entry. */
  const float mired = clamp(
      1e6f / temperature, planck_locus[0].mired, planck_locus[planck_locus.size() - 1].mired);
  auto check = [](const locus_entry_t &entry, const float val) { return entry.mired < val; };
  const auto entry = std::lower_bound(planck_locus.begin(), planck_locus.end(), mired, check);
  const size_t i = size_t(entry - planck_locus.begin());
  const locus_entry_t &low = planck_locus[i - 1], high = planck_locus[i];

  /* Find interpolation factor. */
  const float f = (mired - low.mired) / (high.mired - low.mired);

  /* Interpolate point along Planckian locus. */
  float2 uv = interpolate(low.uv, high.uv, f);

  /* Compute and interpolate isotherm. */
  const float2 isotherm0 = normalize(float2(1.0f, low.t));
  const float2 isotherm1 = normalize(float2(1.0f, high.t));
  const float2 isotherm = normalize(interpolate(isotherm0, isotherm1, f));

  /* Offset away from the Planckian locus according to the tint.
   * Tint is parameterized such that +-3000 tint corresponds to +-1 delta UV. */
  uv -= isotherm * tint / 3000.0f;

  /* Convert CIE 1960 uv -> xyY. */
  const float x = 3.0f * uv.x / (2.0f * uv.x - 8.0f * uv.y + 4.0f);
  const float y = 2.0f * uv.y / (2.0f * uv.x - 8.0f * uv.y + 4.0f);

  /* Convert xyY -> XYZ (assuming Y=1). */
  return float3{x / y, 1.0f, (1.0f - x - y) / y};
}

float3x3 chromatic_adaption_matrix(const float3 &from_XYZ, const float3 &to_XYZ)
{
  /* Bradford transformation matrix (XYZ -> LMS). */
  static const float3x3 bradford{
      {0.8951f, -0.7502f, 0.0389f},
      {0.2664f, 1.7135f, -0.0685f},
      {-0.1614f, 0.0367f, 1.0296f},
  };

  /* Compute white points in LMS space. */
  const float3 from_LMS = bradford * from_XYZ / from_XYZ.y;
  const float3 to_LMS = bradford * to_XYZ / to_XYZ.y;

  /* Assemble full transform: XYZ -> LMS -> adapted LMS -> adapted XYZ. */
  return invert(bradford) * from_scale<float3x3>(to_LMS / from_LMS) * bradford;
}

}  // namespace blender::math
