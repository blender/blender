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

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "sequencer_scopes.hh"

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "BLI_timeit.hh"
#endif

namespace blender::ed::vse {

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
  /* We don't need Y. */
  float u = -0.09991f * r - 0.33609f * g + 0.436f * b;
  float v = 0.615f * r - 0.55861f * g - 0.05639f * b;

  /* Normalize to 0..1 range. */
  u = clamp_f(u * SeqScopes::VECSCOPE_U_SCALE + 0.5f, 0.0f, 1.0f);
  v = clamp_f(v * SeqScopes::VECSCOPE_V_SCALE + 0.5f, 0.0f, 1.0f);
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

static void rgba_float_to_display_space(ColormanageProcessor *processor,
                                        const ColorSpace *src_colorspace,
                                        MutableSpan<float4> pixels)
{
  IMB_colormanagement_colorspace_to_scene_linear(
      &pixels.data()->x, pixels.size(), 1, 4, src_colorspace, false);
  IMB_colormanagement_processor_apply(processor, &pixels.data()->x, pixels.size(), 1, 4, false);
}

static Array<float4> pixels_to_display_space(ColormanageProcessor *processor,
                                             const ColorSpace *src_colorspace,
                                             int64_t num,
                                             const float *src,
                                             int64_t stride)
{
  Array<float4> result(num, NoInitialization());
  for (int64_t i : result.index_range()) {
    premul_to_straight_v4_v4(result[i], src);
    src += stride;
  }
  rgba_float_to_display_space(processor, src_colorspace, result);
  return result;
}

static Array<float4> pixels_to_display_space(ColormanageProcessor *processor,
                                             const ColorSpace *src_colorspace,
                                             int64_t num,
                                             const uchar *src,
                                             int64_t stride)
{
  Array<float4> result(num, NoInitialization());
  for (int64_t i : result.index_range()) {
    rgba_uchar_to_float(result[i], src);
    src += stride;
  }
  rgba_float_to_display_space(processor, src_colorspace, result);
  return result;
}

ImBuf *make_waveform_view_from_ibuf(const ImBuf *ibuf,
                                    const ColorManagedViewSettings &view_settings,
                                    const ColorManagedDisplaySettings &display_settings)
{
#ifdef DEBUG_TIME
  SCOPED_TIMER(__func__);
#endif
  const int w = ibuf->x;
  const int h = 256;
  ImBuf *rval = IMB_allocImBuf(w, h, 32, IB_byte_data);
  uchar *tgt = rval->byte_buffer.data;

  uchar wtable[256];
  init_wave_table(ibuf->y, wtable);

  ColormanageProcessor *cm_processor = IMB_colormanagement_display_processor_for_imbuf(
      ibuf, &view_settings, &display_settings);

  /* IMB_colormanagement_get_luminance_byte for each pixel is quite a lot of
   * overhead, so instead get luma coefficients as 16-bit integers. */
  float coeffs[3];
  IMB_colormanagement_get_luminance_coefficients(coeffs);
  const int muls[3] = {int(coeffs[0] * 65535), int(coeffs[1] * 65535), int(coeffs[2] * 65535)};

  /* Parallel over x, since each column is easily independent from others. */
  threading::parallel_for_each(IndexRange(ibuf->x), [&](const int x) {
    if (ibuf->float_buffer.data) {
      const float *src = ibuf->float_buffer.data + x * 4;
      if (!cm_processor) {
        /* Float image, no color space conversions needed. */
        for (int y = 0; y < ibuf->y; y++) {
          float4 pixel;
          premul_to_straight_v4_v4(pixel, src);
          float v = dot_v3v3(pixel, coeffs);
          uchar *p = tgt;
          int iv = clamp_i(int(v * h), 0, h - 1);
          p += 4 * (w * iv + x);
          scope_put_pixel(wtable, p);
          src += ibuf->x * 4;
        }
      }
      else {
        /* Float image, with color space conversions. */
        Array<float4> pixels = pixels_to_display_space(
            cm_processor, ibuf->float_buffer.colorspace, ibuf->y, src, ibuf->x * 4);
        for (int y = 0; y < ibuf->y; y++) {
          float v = dot_v3v3(pixels[y], coeffs);
          uchar *p = tgt;
          int iv = clamp_i(int(v * h), 0, h - 1);
          p += 4 * (w * iv + x);
          scope_put_pixel(wtable, p);
        }
      }
    }
    else {
      const uchar *src = ibuf->byte_buffer.data + x * 4;
      if (!cm_processor) {
        /* Byte image, no color space conversions needed. */
        for (int y = 0; y < ibuf->y; y++) {
          /* +1 is "Sree's solution" from http://stereopsis.com/doubleblend.html */
          int rgb0 = src[0] + 1;
          int rgb1 = src[1] + 1;
          int rgb2 = src[2] + 1;
          int luma = (rgb0 * muls[0] + rgb1 * muls[1] + rgb2 * muls[2]) >> 16;
          int luma_y = clamp_i(luma, 0, 255);
          uchar *p = tgt + 4 * (w * luma_y + x);
          scope_put_pixel(wtable, p);
          src += ibuf->x * 4;
        }
      }
      else {
        /* Byte image, with color space conversions. */
        Array<float4> pixels = pixels_to_display_space(
            cm_processor, ibuf->byte_buffer.colorspace, ibuf->y, src, ibuf->x * 4);
        for (int y = 0; y < ibuf->y; y++) {
          float v = dot_v3v3(pixels[y], coeffs);
          uchar *p = tgt;
          int iv = clamp_i(int(v * h), 0, h - 1);
          p += 4 * (w * iv + x);
          scope_put_pixel(wtable, p);
        }
      }
    }
  });

  if (cm_processor) {
    IMB_colormanagement_processor_free(cm_processor);
  }
  return rval;
}

ImBuf *make_sep_waveform_view_from_ibuf(const ImBuf *ibuf,
                                        const ColorManagedViewSettings &view_settings,
                                        const ColorManagedDisplaySettings &display_settings)
{
#ifdef DEBUG_TIME
  SCOPED_TIMER(__func__);
#endif
  int w = ibuf->x;
  int h = 256;
  ImBuf *rval = IMB_allocImBuf(w, h, 32, IB_byte_data);
  uchar *tgt = rval->byte_buffer.data;
  int sw = ibuf->x / 3;

  uchar wtable[256];
  init_wave_table(ibuf->y, wtable);

  ColormanageProcessor *cm_processor = IMB_colormanagement_display_processor_for_imbuf(
      ibuf, &view_settings, &display_settings);

  /* Parallel over x, since each column is easily independent from others. */
  threading::parallel_for_each(IndexRange(ibuf->x), [&](const int x) {
    if (ibuf->float_buffer.data) {
      const float *src = ibuf->float_buffer.data + x * 4;
      if (!cm_processor) {
        /* Float image, no color space conversions needed. */
        for (int y = 0; y < ibuf->y; y++) {
          float4 pixel;
          premul_to_straight_v4_v4(pixel, src);
          for (int c = 0; c < 3; c++) {
            uchar *p = tgt;
            float v = pixel[c];
            int iv = clamp_i(int(v * h), 0, h - 1);
            p += 4 * (w * iv + c * sw + x / 3);
            scope_put_pixel_single(wtable, p, c);
          }
          src += ibuf->x * 4;
        }
      }
      else {
        /* Float image, with color space conversions. */
        Array<float4> pixels = pixels_to_display_space(
            cm_processor, ibuf->float_buffer.colorspace, ibuf->y, src, ibuf->x * 4);
        for (int y = 0; y < ibuf->y; y++) {
          float4 pixel = pixels[y];
          for (int c = 0; c < 3; c++) {
            uchar *p = tgt;
            float v = pixel[c];
            int iv = clamp_i(int(v * h), 0, h - 1);
            p += 4 * (w * iv + c * sw + x / 3);
            scope_put_pixel_single(wtable, p, c);
          }
        }
      }
    }
    else {
      const uchar *src = ibuf->byte_buffer.data + x * 4;
      if (!cm_processor) {
        /* Byte image, no color space conversions needed. */
        for (int y = 0; y < ibuf->y; y++) {
          for (int c = 0; c < 3; c++) {
            uchar *p = tgt;
            p += 4 * (w * src[c] + c * sw + x / 3);
            scope_put_pixel_single(wtable, p, c);
          }
          src += ibuf->x * 4;
        }
      }
      else {
        /* Byte image, with color space conversions. */
        Array<float4> pixels = pixels_to_display_space(
            cm_processor, ibuf->byte_buffer.colorspace, ibuf->y, src, ibuf->x * 4);
        for (int y = 0; y < ibuf->y; y++) {
          float4 pixel = pixels[y];
          for (int c = 0; c < 3; c++) {
            uchar *p = tgt;
            float v = pixel[c];
            int iv = clamp_i(int(v * h), 0, h - 1);
            p += 4 * (w * iv + c * sw + x / 3);
            scope_put_pixel_single(wtable, p, c);
          }
        }
      }
    }
  });

  if (cm_processor) {
    IMB_colormanagement_processor_free(cm_processor);
  }
  return rval;
}

ImBuf *make_zebra_view_from_ibuf(const ImBuf *ibuf, float perc)
{
#ifdef DEBUG_TIME
  SCOPED_TIMER(__func__);
#endif
  ImBuf *res = IMB_allocImBuf(ibuf->x, ibuf->y, 32, IB_byte_data | IB_uninitialized_pixels);

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

void ScopeHistogram::calc_from_ibuf(const ImBuf *ibuf,
                                    const ColorManagedViewSettings &view_settings,
                                    const ColorManagedDisplaySettings &display_settings)
{
#ifdef DEBUG_TIME
  SCOPED_TIMER(__func__);
#endif

  ColormanageProcessor *cm_processor = IMB_colormanagement_display_processor_for_imbuf(
      ibuf, &view_settings, &display_settings);

  const bool is_float = ibuf->float_buffer.data != nullptr;
  const int hist_size = is_float ? BINS_FLOAT : BINS_BYTE;

  Array<uint3> counts(hist_size, uint3(0));
  data = threading::parallel_reduce(
      IndexRange(IMB_get_pixel_count(ibuf)),
      16 * 1024,
      counts,
      [&](const IndexRange range, const Array<uint3> &init) {
        Array<uint3> res = init;

        if (is_float) {
          const float *src = ibuf->float_buffer.data + range.first() * 4;
          if (!cm_processor) {
            /* Float image, no color space conversions needed. */
            for ([[maybe_unused]] const int64_t index : range) {
              float4 pixel;
              premul_to_straight_v4_v4(pixel, src);
              res[get_bin_float(pixel.x)].x++;
              res[get_bin_float(pixel.y)].y++;
              res[get_bin_float(pixel.z)].z++;
              src += 4;
            }
          }
          else {
            /* Float image, with color space conversions. */
            Array<float4> pixels = pixels_to_display_space(
                cm_processor, ibuf->float_buffer.colorspace, range.size(), src, 4);
            for (const float4 &pixel : pixels) {
              res[get_bin_float(pixel.x)].x++;
              res[get_bin_float(pixel.y)].y++;
              res[get_bin_float(pixel.z)].z++;
            }
          }
        }
        else {
          /* Byte images just use 256 histogram bins, directly indexed by value. */
          const uchar *src = ibuf->byte_buffer.data + range.first() * 4;
          if (!cm_processor) {
            /* Byte image, no color space conversions needed. */
            for ([[maybe_unused]] const int64_t index : range) {
              res[src[0]].x++;
              res[src[1]].y++;
              res[src[2]].z++;
              src += 4;
            }
          }
          else {
            /* Byte image, with color space conversions. */
            Array<float4> pixels = pixels_to_display_space(
                cm_processor, ibuf->byte_buffer.colorspace, range.size(), src, 4);
            for (const float4 &pixel : pixels) {
              uchar pixel_b[4];
              rgba_float_to_uchar(pixel_b, pixel);
              res[pixel_b[0]].x++;
              res[pixel_b[1]].y++;
              res[pixel_b[2]].z++;
            }
          }
        }
        return res;
      },
      /* Merge histograms computed per-thread. */
      [&](const Array<uint3> &a, const Array<uint3> &b) {
        BLI_assert(a.size() == b.size());
        Array<uint3> res(a.size());
        for (int i = 0; i < a.size(); i++) {
          res[i] = a[i] + b[i];
        }
        return res;
      });

  if (cm_processor) {
    IMB_colormanagement_processor_free(cm_processor);
  }

  max_value = uint3(0);
  for (const uint3 &v : data) {
    max_value = math::max(max_value, v);
  }
}

ImBuf *make_vectorscope_view_from_ibuf(const ImBuf *ibuf,
                                       const ColorManagedViewSettings &view_settings,
                                       const ColorManagedDisplaySettings &display_settings)
{
#ifdef DEBUG_TIME
  SCOPED_TIMER(__func__);
#endif
  constexpr int size = 512;
  const float size_mul = size - 1.0f;

  ColormanageProcessor *cm_processor = IMB_colormanagement_display_processor_for_imbuf(
      ibuf, &view_settings, &display_settings);

  const bool is_float = ibuf->float_buffer.data != nullptr;
  /* Vector scope is calculated by scattering writes into the resulting scope image. Do it with
   * parallel reduce, by filling a separate image per job and merging them. Since the payload
   * of each job is fairly large, make the jobs large enough too. */
  constexpr int64_t grain_size = 256 * 1024;
  Array<uint8_t> counts(size * size, uint8_t(0));
  Array<uint8_t> data = threading::parallel_reduce(
      IndexRange(IMB_get_pixel_count(ibuf)),
      grain_size,
      counts,
      [&](const IndexRange range, const Array<uint8_t> &init) {
        Array<uint8_t> res = init;

        const float *src_f = is_float ? ibuf->float_buffer.data + range.first() * 4 : nullptr;
        const uchar *src_b = !is_float ? ibuf->byte_buffer.data + range.first() * 4 : nullptr;
        if (cm_processor) {
          /* Byte or float image, color space conversions needed. Do them in smaller chunks
           * than the whole job size, so they fit into CPU cache and can be on the stack. */
          constexpr int64_t chunk_size = 4 * 1024;
          float4 pixels[chunk_size];
          for (int64_t index = 0; index < range.size(); index += chunk_size) {
            const int64_t sub_size = std::min(range.size() - index, chunk_size);
            if (is_float) {
              for (int64_t i = 0; i < sub_size; i++) {
                premul_to_straight_v4_v4(pixels[i], src_f);
                src_f += 4;
              }
            }
            else {
              for (int64_t i = 0; i < sub_size; i++) {
                rgba_uchar_to_float(pixels[i], src_b);
                src_b += 4;
              }
            }
            MutableSpan<float4> pixels_span = MutableSpan<float4>(pixels, sub_size);
            rgba_float_to_display_space(cm_processor,
                                        is_float ? ibuf->float_buffer.colorspace :
                                                   ibuf->byte_buffer.colorspace,
                                        pixels_span);
            for (float4 pixel : pixels_span) {
              clamp_v3(pixel, 0.0f, 1.0f);
              float2 uv = rgb_to_uv_normalized(pixel) * size_mul;
              int offset = size * int(uv.y) + int(uv.x);
              res[offset] = std::min<int>(res[offset] + 1, 255);
            }
          }
        }
        else if (is_float) {
          /* Float image, no color space conversions needed. */
          for ([[maybe_unused]] const int64_t index : range) {
            float4 pixel;
            premul_to_straight_v4_v4(pixel, src_f);
            clamp_v3(pixel, 0.0f, 1.0f);
            float2 uv = rgb_to_uv_normalized(pixel) * size_mul;
            int offset = size * int(uv.y) + int(uv.x);
            res[offset] = std::min<int>(res[offset] + 1, 255);
            src_f += 4;
          }
        }
        else {
          /* Byte image, no color space conversions needed. */
          for ([[maybe_unused]] const int64_t index : range) {
            float4 pixel;
            rgb_uchar_to_float(pixel, src_b);
            float2 uv = rgb_to_uv_normalized(pixel) * size_mul;
            int offset = size * int(uv.y) + int(uv.x);
            res[offset] = std::min<int>(res[offset] + 1, 255);
            src_b += 4;
          }
        }
        return res;
      },
      /* Merge scopes computed per-thread. */
      [&](const Array<uint8_t> &a, const Array<uint8_t> &b) {
        BLI_assert(a.size() == b.size());
        Array<uint8_t> res(a.size(), NoInitialization());
        for (int64_t i : a.index_range()) {
          res[i] = std::min<int>(a[i] + b[i], 255);
        }
        return res;
      });

  /* Fill the vector scope image from the computed data. */
  uchar wtable[256];
  init_wave_table(math::midpoint(ibuf->x, ibuf->y), wtable);

  ImBuf *rval = IMB_allocImBuf(size, size, 32, IB_byte_data | IB_uninitialized_pixels);
  uchar *dst = rval->byte_buffer.data;
  for (int i = 0; i < size * size; i++) {
    uint8_t val = data[i];
    if (val != 0) {
      val = wtable[val];
    }
    dst[0] = val;
    dst[1] = val;
    dst[2] = val;
    dst[3] = 255;
    dst += 4;
  }

  if (cm_processor) {
    IMB_colormanagement_processor_free(cm_processor);
  }
  return rval;
}

}  // namespace blender::ed::vse
