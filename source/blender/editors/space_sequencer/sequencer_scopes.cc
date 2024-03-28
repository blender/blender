/* SPDX-FileCopyrightText: 2006-2008 Peter Schlaile < peter [at] schlaile [dot] de >.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include <cmath>
#include <cstring>

#include "BLI_math_vector.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "sequencer_scopes.hh"

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "BLI_timeit.hh"
#endif

namespace blender::ed::seq {

SeqScopes::~SeqScopes()
{
  cleanup();
}

void SeqScopes::cleanup()
{
  if (zebra_ibuf) {
    IMB_freeImBuf(zebra_ibuf);
    zebra_ibuf = nullptr;
  }
  if (waveform_ibuf) {
    IMB_freeImBuf(waveform_ibuf);
    waveform_ibuf = nullptr;
  }
  if (sep_waveform_ibuf) {
    IMB_freeImBuf(sep_waveform_ibuf);
    sep_waveform_ibuf = nullptr;
  }
  if (vector_ibuf) {
    IMB_freeImBuf(vector_ibuf);
    vector_ibuf = nullptr;
  }
  histogram.data.reinitialize(0);
}

static blender::float2 rgb_to_uv_normalized(const float rgb[3])
{
  /* Exact same math as rgb_to_yuv BT709 case. Duplicated here
   * since this function is called a lot, and non-inline function
   * call plus color-space switch in there overhead does add up. */
  float r = rgb[0], g = rgb[1], b = rgb[2];
  /* We don't need y. */
  float u = -0.09991f * r - 0.33609f * g + 0.436f * b;
  float v = 0.615f * r - 0.55861f * g - 0.05639f * b;

  /* Normalize: possible range is +/- 0.615. */
  u = clamp_f(u * (0.5f / 0.615f) + 0.5f, 0.0f, 1.0f);
  v = clamp_f(v * (0.5f / 0.615f) + 0.5f, 0.0f, 1.0f);
  return float2(u, v);
}

static void scope_put_pixel(const uchar *table, uchar *pos)
{
  uchar newval = table[*pos];
  pos[0] = pos[1] = pos[2] = newval;
  pos[3] = 255;
}

static void scope_put_pixel_single(const uchar *table, uchar *pos, int col)
{
  uint newval = table[pos[col]];
  /* So that the separate waveforms are not just pure RGB primaries, put
   * some amount of value into the other channels too: slightly reduce it,
   * and raise to 4th power. */
  uint other = newval * 31 / 32;
  other = (other * other) >> 8;
  other = (other * other) >> 8;
  pos[0] = pos[1] = pos[2] = uchar(other);
  pos[col] = uchar(newval);
  pos[3] = 255;
}

static void init_wave_table(int height, uchar wtable[256])
{
  /* For each pixel column of the image, waveform plots the intensity values
   * with height proportional to the intensity. So depending on the height of
   * the image, different amount of pixels are expected to hit the same
   * intensity. Adjust the waveform plotting table gamma factor so that
   * the waveform has decent visibility without saturating or being too dark:
   * 0.3 gamma at height=360 and below, 0.9 gamma at height 2160 (4K) and up,
   * and interpolating between those. */
  float alpha = clamp_f(ratiof(360.0f, 2160.0f, height), 0.0f, 1.0f);
  float gamma = interpf(0.9f, 0.3f, alpha);
  for (int x = 0; x < 256; x++) {
    wtable[x] = uchar(pow((float(x) + 1.0f) / 256.0f, gamma) * 255.0f);
  }
}

ImBuf *make_waveform_view_from_ibuf(const ImBuf *ibuf)
{
#ifdef DEBUG_TIME
  SCOPED_TIMER(__func__);
#endif
  const int w = ibuf->x;
  const int h = 256;
  ImBuf *rval = IMB_allocImBuf(w, h, 32, IB_rect);
  uchar *tgt = rval->byte_buffer.data;

  uchar wtable[256];
  init_wave_table(ibuf->y, wtable);

  /* IMB_colormanagement_get_luminance_byte for each pixel is quite a lot of
   * overhead, so instead get luma coefficients as 16-bit integers. */
  float coeffs[3];
  IMB_colormanagement_get_luminance_coefficients(coeffs);
  const int muls[3] = {int(coeffs[0] * 65535), int(coeffs[1] * 65535), int(coeffs[2] * 65535)};

  /* Parallel over x, since each column is easily independent from others. */
  threading::parallel_for(IndexRange(ibuf->x), 32, [&](IndexRange x_range) {
    if (ibuf->float_buffer.data) {
      /* Float image. */
      const float *src = ibuf->float_buffer.data;
      for (int y = 0; y < ibuf->y; y++) {
        for (const int x : x_range) {
          const float *rgb = src + 4 * (ibuf->x * y + x);
          float v = IMB_colormanagement_get_luminance(rgb);
          uchar *p = tgt;

          int iv = clamp_i(int(v * h), 0, h - 1);

          p += 4 * (w * iv + x);
          scope_put_pixel(wtable, p);
        }
      }
    }
    else {
      /* Byte image. */
      const uchar *src = ibuf->byte_buffer.data;
      for (int y = 0; y < ibuf->y; y++) {
        for (const int x : x_range) {
          const uchar *rgb = src + 4 * (ibuf->x * y + x);
          /* +1 is "Sree's solution" from http://stereopsis.com/doubleblend.html */
          int rgb0 = rgb[0] + 1;
          int rgb1 = rgb[1] + 1;
          int rgb2 = rgb[2] + 1;
          int luma = (rgb0 * muls[0] + rgb1 * muls[1] + rgb2 * muls[2]) >> 16;
          int luma_y = clamp_i(luma, 0, 255);
          uchar *p = tgt + 4 * (w * luma_y + x);
          scope_put_pixel(wtable, p);
        }
      }
    }
  });

  return rval;
}

ImBuf *make_sep_waveform_view_from_ibuf(const ImBuf *ibuf)
{
#ifdef DEBUG_TIME
  SCOPED_TIMER(__func__);
#endif
  int w = ibuf->x;
  int h = 256;
  ImBuf *rval = IMB_allocImBuf(w, h, 32, IB_rect);
  uchar *tgt = rval->byte_buffer.data;
  int sw = ibuf->x / 3;

  uchar wtable[256];
  init_wave_table(ibuf->y, wtable);

  /* Parallel over x, since each column is easily independent from others. */
  threading::parallel_for(IndexRange(ibuf->x), 32, [&](IndexRange x_range) {
    if (ibuf->float_buffer.data) {
      /* Float image. */
      const float *src = ibuf->float_buffer.data;
      for (int y = 0; y < ibuf->y; y++) {
        for (const int x : x_range) {
          const float *rgb = src + 4 * (ibuf->x * y + x);
          for (int c = 0; c < 3; c++) {
            uchar *p = tgt;
            float v = rgb[c];
            int iv = clamp_i(int(v * h), 0, h - 1);

            p += 4 * (w * iv + c * sw + x / 3);
            scope_put_pixel_single(wtable, p, c);
          }
        }
      }
    }
    else {
      /* Byte image. */
      const uchar *src = ibuf->byte_buffer.data;
      for (int y = 0; y < ibuf->y; y++) {
        for (const int x : x_range) {
          const uchar *rgb = src + 4 * (ibuf->x * y + x);
          for (int c = 0; c < 3; c++) {
            uchar *p = tgt;
            p += 4 * (w * rgb[c] + c * sw + x / 3);
            scope_put_pixel_single(wtable, p, c);
          }
        }
      }
    }
  });

  return rval;
}

ImBuf *make_zebra_view_from_ibuf(const ImBuf *ibuf, float perc)
{
#ifdef DEBUG_TIME
  SCOPED_TIMER(__func__);
#endif
  ImBuf *res = IMB_allocImBuf(ibuf->x, ibuf->y, 32, IB_rect | IB_uninitialized_pixels);

  threading::parallel_for(IndexRange(ibuf->y), 16, [&](IndexRange y_range) {
    if (ibuf->float_buffer.data) {
      /* Float image. */
      const float limit = perc / 100.0f;
      const float *p = ibuf->float_buffer.data + y_range.first() * ibuf->x * 4;
      uchar *o = res->byte_buffer.data + y_range.first() * ibuf->x * 4;
      for (const int y : y_range) {
        for (int x = 0; x < ibuf->x; x++) {
          float pix[4];
          memcpy(pix, p, sizeof(pix));
          if (pix[0] >= limit || pix[1] >= limit || pix[2] >= limit) {
            if (((x + y) & 0x08) != 0) {
              pix[0] = 1.0f - pix[0];
              pix[1] = 1.0f - pix[1];
              pix[2] = 1.0f - pix[2];
            }
          }
          rgba_float_to_uchar(o, pix);
          p += 4;
          o += 4;
        }
      }
    }
    else {
      /* Byte image. */
      const uint limit = 255.0f * perc / 100.0f;
      const uchar *p = ibuf->byte_buffer.data + y_range.first() * ibuf->x * 4;
      uchar *o = res->byte_buffer.data + y_range.first() * ibuf->x * 4;
      for (const int y : y_range) {
        for (int x = 0; x < ibuf->x; x++) {
          uchar pix[4];
          memcpy(pix, p, sizeof(pix));

          if (pix[0] >= limit || pix[1] >= limit || pix[2] >= limit) {
            if (((x + y) & 0x08) != 0) {
              pix[0] = 255 - pix[0];
              pix[1] = 255 - pix[1];
              pix[2] = 255 - pix[2];
            }
          }
          memcpy(o, pix, sizeof(pix));
          p += 4;
          o += 4;
        }
      }
    }
  });
  return res;
}

static int get_bin_float(float f)
{
  int bin = int(((f - ScopeHistogram::FLOAT_VAL_MIN) /
                 (ScopeHistogram::FLOAT_VAL_MAX - ScopeHistogram::FLOAT_VAL_MIN)) *
                ScopeHistogram::BINS_FLOAT);
  return clamp_i(bin, 0, ScopeHistogram::BINS_FLOAT - 1);
}

void ScopeHistogram::calc_from_ibuf(const ImBuf *ibuf)
{
#ifdef DEBUG_TIME
  SCOPED_TIMER(__func__);
#endif

  const bool is_float = ibuf->float_buffer.data != nullptr;
  const int hist_size = is_float ? BINS_FLOAT : BINS_BYTE;

  Array<uint3> counts(hist_size, uint3(0));
  data = threading::parallel_reduce(
      IndexRange(ibuf->y),
      256,
      counts,
      [&](const IndexRange y_range, const Array<uint3> &init) {
        Array<uint3> res = init;

        if (is_float) {
          for (const int y : y_range) {
            const float *src = ibuf->float_buffer.data + y * ibuf->x * 4;
            for (int x = 0; x < ibuf->x; x++) {
              res[get_bin_float(src[0])].x++;
              res[get_bin_float(src[1])].y++;
              res[get_bin_float(src[2])].z++;
              src += 4;
            }
          }
        }
        else {
          /* Byte images just use 256 histogram bins, directly indexed by value. */
          for (const int y : y_range) {
            const uchar *src = ibuf->byte_buffer.data + y * ibuf->x * 4;
            for (int x = 0; x < ibuf->x; x++) {
              res[src[0]].x++;
              res[src[1]].y++;
              res[src[2]].z++;
              src += 4;
            }
          }
        }
        return res;
      },
      [&](const Array<uint3> &a, const Array<uint3> &b) {
        BLI_assert(a.size() == b.size());
        Array<uint3> res(a.size());
        for (int i = 0; i < a.size(); i++) {
          res[i] = a[i] + b[i];
        }
        return res;
      });

  max_value = uint3(0);
  for (const uint3 &v : data) {
    max_value = math::max(max_value, v);
  }
}

ImBuf *make_vectorscope_view_from_ibuf(const ImBuf *ibuf)
{
#ifdef DEBUG_TIME
  SCOPED_TIMER(__func__);
#endif
  const int size = 512;
  const float size_mul = size - 1.0f;
  ImBuf *rval = IMB_allocImBuf(size, size, 32, IB_rect);

  uchar *dst = rval->byte_buffer.data;
  float rgb[3];

  uchar wtable[256];
  init_wave_table(math::midpoint(ibuf->x, ibuf->y), wtable);

  if (ibuf->float_buffer.data) {
    /* Float image. */
    const float *src = ibuf->float_buffer.data;
    for (int y = 0; y < ibuf->y; y++) {
      for (int x = 0; x < ibuf->x; x++) {
        memcpy(rgb, src, sizeof(float[3]));
        clamp_v3(rgb, 0.0f, 1.0f);

        float2 uv = rgb_to_uv_normalized(rgb) * size_mul;

        uchar *p = dst + 4 * (size * int(uv.y) + int(uv.x));
        scope_put_pixel(wtable, p);

        src += 4;
      }
    }
  }
  else {
    /* Byte image. */
    const uchar *src = ibuf->byte_buffer.data;
    for (int y = 0; y < ibuf->y; y++) {
      for (int x = 0; x < ibuf->x; x++) {
        rgb[0] = float(src[0]) * (1.0f / 255.0f);
        rgb[1] = float(src[1]) * (1.0f / 255.0f);
        rgb[2] = float(src[2]) * (1.0f / 255.0f);

        float2 uv = rgb_to_uv_normalized(rgb) * size_mul;

        uchar *p = dst + 4 * (size * int(uv.y) + int(uv.x));
        scope_put_pixel(wtable, p);

        src += 4;
      }
    }
  }

  return rval;
}

}  // namespace blender::ed::seq
