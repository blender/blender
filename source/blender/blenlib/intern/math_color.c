/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup bli
 */

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h"

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
  unsigned int ri, gi, bi;

  if (hexcol[0] == '#') {
    hexcol++;
  }

  if (sscanf(hexcol, "%02x%02x%02x", &ri, &gi, &bi) == 3) {
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
    return;
  }

  *r_r = (float)ri * (1.0f / 255.0f);
  *r_g = (float)gi * (1.0f / 255.0f);
  *r_b = (float)bi * (1.0f / 255.0f);
  CLAMP(*r_r, 0.0f, 1.0f);
  CLAMP(*r_g, 0.0f, 1.0f);
  CLAMP(*r_b, 0.0f, 1.0f);
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
  const float orig_s = *r_s;
  const float orig_h = *r_h;

  rgb_to_hsl(r, g, b, r_h, r_s, r_l);

  if (*r_l <= 0.0f) {
    *r_h = orig_h;
    *r_s = orig_s;
  }
  else if (*r_s <= 0.0f) {
    *r_h = orig_h;
    *r_s = orig_s;
  }

  if (*r_h == 0.0f && orig_h >= 1.0f) {
    *r_h = 1.0f;
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
  const float orig_h = *r_h;
  const float orig_s = *r_s;

  rgb_to_hsv(r, g, b, r_h, r_s, r_v);

  if (*r_v <= 1e-8) {
    /* Very low v values will affect the hs values, correct them in post. */
    *r_h = orig_h;
    *r_s = orig_s;
  }
  else if (*r_s <= 1e-8) {
    *r_h = orig_h;
  }

  if (*r_h == 0.0f && orig_h >= 1.0f) {
    *r_h = 1.0f;
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

unsigned int hsv_to_cpack(float h, float s, float v)
{
  unsigned int r, g, b;
  float rf, gf, bf;
  unsigned int col;

  hsv_to_rgb(h, s, v, &rf, &gf, &bf);

  r = (unsigned int)(rf * 255.0f);
  g = (unsigned int)(gf * 255.0f);
  b = (unsigned int)(bf * 255.0f);

  col = (r + (g * 256) + (b * 256 * 256));
  return col;
}

unsigned int rgb_to_cpack(float r, float g, float b)
{
  unsigned int ir, ig, ib;

  ir = (unsigned int)floorf(255.0f * max_ff(r, 0.0f));
  ig = (unsigned int)floorf(255.0f * max_ff(g, 0.0f));
  ib = (unsigned int)floorf(255.0f * max_ff(b, 0.0f));

  if (ir > 255) {
    ir = 255;
  }
  if (ig > 255) {
    ig = 255;
  }
  if (ib > 255) {
    ib = 255;
  }

  return (ir + (ig * 256) + (ib * 256 * 256));
}

void cpack_to_rgb(unsigned int col, float *r_r, float *r_g, float *r_b)
{
  *r_r = ((float)(((col)) & 0xFF)) * (1.0f / 255.0f);
  *r_g = ((float)(((col) >> 8) & 0xFF)) * (1.0f / 255.0f);
  *r_b = ((float)(((col) >> 16) & 0xFF)) * (1.0f / 255.0f);
}

void rgb_uchar_to_float(float r_col[3], const unsigned char col_ub[3])
{
  r_col[0] = ((float)col_ub[0]) * (1.0f / 255.0f);
  r_col[1] = ((float)col_ub[1]) * (1.0f / 255.0f);
  r_col[2] = ((float)col_ub[2]) * (1.0f / 255.0f);
}

void rgba_uchar_to_float(float r_col[4], const unsigned char col_ub[4])
{
  r_col[0] = ((float)col_ub[0]) * (1.0f / 255.0f);
  r_col[1] = ((float)col_ub[1]) * (1.0f / 255.0f);
  r_col[2] = ((float)col_ub[2]) * (1.0f / 255.0f);
  r_col[3] = ((float)col_ub[3]) * (1.0f / 255.0f);
}

void rgb_float_to_uchar(unsigned char r_col[3], const float col_f[3])
{
  unit_float_to_uchar_clamp_v3(r_col, col_f);
}

void rgba_float_to_uchar(unsigned char r_col[4], const float col_f[4])
{
  unit_float_to_uchar_clamp_v4(r_col, col_f);
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

void minmax_rgb(short c[3])
{
  if (c[0] > 255) {
    c[0] = 255;
  }
  else if (c[0] < 0) {
    c[0] = 0;
  }

  if (c[1] > 255) {
    c[1] = 255;
  }
  else if (c[1] < 0) {
    c[1] = 0;
  }

  if (c[2] > 255) {
    c[2] = 255;
  }
  else if (c[2] < 0) {
    c[2] = 0;
  }
}

int constrain_rgb(float *r, float *g, float *b)
{
  /* Amount of white needed */
  const float w = -min_ffff(0.0f, *r, *g, *b);

  /* Add just enough white to make r, g, b all positive. */
  if (w > 0.0f) {
    *r += w;
    *g += w;
    *b += w;

    return 1; /* Color modified to fit RGB gamut */
  }

  return 0; /* Color within RGB gamut */
}

/* ********************** lift/gamma/gain / ASC-CDL conversion ********************************* */

void lift_gamma_gain_to_asc_cdl(const float *lift,
                                const float *gamma,
                                const float *gain,
                                float *offset,
                                float *slope,
                                float *power)
{
  int c;
  for (c = 0; c < 3; c++) {
    offset[c] = lift[c] * gain[c];
    slope[c] = gain[c] * (1.0f - lift[c]);
    if (gamma[c] == 0) {
      power[c] = FLT_MAX;
    }
    else {
      power[c] = 1.0f / gamma[c];
    }
  }
}

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

void rgb_byte_set_hue_float_offset(unsigned char rgb[3], float hue_offset)
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
unsigned short BLI_color_to_srgb_table[0x10000];

static unsigned short hipart(const float f)
{
  union {
    float f;
    unsigned short us[2];
  } tmp;

  tmp.f = f;

#ifdef __BIG_ENDIAN__
  return tmp.us[0];
#else
  return tmp.us[1];
#endif
}

static float index_to_float(const unsigned short i)
{

  union {
    float f;
    unsigned short us[2];
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

#ifdef __BIG_ENDIAN__
  tmp.us[0] = i;
  tmp.us[1] = 0x8000;
#else
  tmp.us[0] = 0x8000;
  tmp.us[1] = i;
#endif

  return tmp.f;
}

void BLI_init_srgb_conversion(void)
{
  static bool initialized = false;
  unsigned int i, b;

  if (initialized) {
    return;
  }
  initialized = true;

  /* Fill in the lookup table to convert floats to bytes: */
  for (i = 0; i < 0x10000; i++) {
    float f = linearrgb_to_srgb(index_to_float((unsigned short)i)) * 255.0f;
    if (f <= 0) {
      BLI_color_to_srgb_table[i] = 0;
    }
    else if (f < 255) {
      BLI_color_to_srgb_table[i] = (unsigned short)(f * 0x100 + 0.5f);
    }
    else {
      BLI_color_to_srgb_table[i] = 0xff00;
    }
  }

  /* Fill in the lookup table to convert bytes to float: */
  for (b = 0; b <= 255; b++) {
    float f = srgb_to_linearrgb(((float)b) * (1.0f / 255.0f));
    BLI_color_from_srgb_table[b] = f;
    i = hipart(f);
    /* replace entries so byte->float->byte does not change the data: */
    BLI_color_to_srgb_table[i] = (unsigned short)(b * 0x100);
  }
}
