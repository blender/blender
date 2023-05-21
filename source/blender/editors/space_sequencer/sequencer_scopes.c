/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006-2008 Peter Schlaile < peter [at] schlaile [dot] de >. */

/** \file
 * \ingroup spseq
 */

#include <math.h>
#include <string.h>

#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "sequencer_intern.h"

/* XXX(@ideasman42): why is this function better than BLI_math version?
 * only difference is it does some normalize after, need to double check on this. */
static void rgb_to_yuv_normalized(const float rgb[3], float yuv[3])
{
  yuv[0] = 0.299f * rgb[0] + 0.587f * rgb[1] + 0.114f * rgb[2];
  yuv[1] = 0.492f * (rgb[2] - yuv[0]);
  yuv[2] = 0.877f * (rgb[0] - yuv[0]);

  /* Normalize. */
  yuv[1] *= 255.0f / (122 * 2.0f);
  yuv[1] += 0.5f;

  yuv[2] *= 255.0f / (157 * 2.0f);
  yuv[2] += 0.5f;
}

static void scope_put_pixel(const uchar *table, uchar *pos)
{
  uchar newval = table[*pos];
  pos[0] = pos[1] = pos[2] = newval;
  pos[3] = 255;
}

static void scope_put_pixel_single(const uchar *table, uchar *pos, int col)
{
  char newval = table[pos[col]];
  pos[col] = newval;
  pos[3] = 255;
}

static void wform_put_line(int w, uchar *last_pos, uchar *new_pos)
{
  if (last_pos > new_pos) {
    uchar *temp = new_pos;
    new_pos = last_pos;
    last_pos = temp;
  }

  while (last_pos < new_pos) {
    if (last_pos[0] == 0) {
      last_pos[0] = last_pos[1] = last_pos[2] = 32;
      last_pos[3] = 255;
    }
    last_pos += 4 * w;
  }
}

static void wform_put_line_single(int w, uchar *last_pos, uchar *new_pos, int col)
{
  if (last_pos > new_pos) {
    uchar *temp = new_pos;
    new_pos = last_pos;
    last_pos = temp;
  }

  while (last_pos < new_pos) {
    if (last_pos[col] == 0) {
      last_pos[col] = 32;
      last_pos[3] = 255;
    }
    last_pos += 4 * w;
  }
}

static void wform_put_border(uchar *tgt, int w, int h)
{
  int x, y;

  for (x = 0; x < w; x++) {
    uchar *p = tgt + 4 * x;
    p[1] = p[3] = 155;
    p[4 * w + 1] = p[4 * w + 3] = 155;
    p = tgt + 4 * (w * (h - 1) + x);
    p[1] = p[3] = 155;
    p[-4 * w + 1] = p[-4 * w + 3] = 155;
  }

  for (y = 0; y < h; y++) {
    uchar *p = tgt + 4 * w * y;
    p[1] = p[3] = 155;
    p[4 + 1] = p[4 + 3] = 155;
    p = tgt + 4 * (w * y + w - 1);
    p[1] = p[3] = 155;
    p[-4 + 1] = p[-4 + 3] = 155;
  }
}

static void wform_put_gridrow(uchar *tgt, float perc, int w, int h)
{
  tgt += (int)(perc / 100.0f * h) * w * 4;

  for (int i = 0; i < w * 2; i++) {
    tgt[0] = 255;

    tgt += 4;
  }
}

static void wform_put_grid(uchar *tgt, int w, int h)
{
  wform_put_gridrow(tgt, 90.0, w, h);
  wform_put_gridrow(tgt, 70.0, w, h);
  wform_put_gridrow(tgt, 10.0, w, h);
}

static ImBuf *make_waveform_view_from_ibuf_byte(ImBuf *ibuf)
{
  ImBuf *rval = IMB_allocImBuf(ibuf->x + 3, 515, 32, IB_rect);
  int x, y;
  const uchar *src = ibuf->byte_buffer.data;
  uchar *tgt = rval->byte_buffer.data;
  int w = ibuf->x + 3;
  int h = 515;
  float waveform_gamma = 0.2;
  uchar wtable[256];

  wform_put_grid(tgt, w, h);
  wform_put_border(tgt, w, h);

  for (x = 0; x < 256; x++) {
    wtable[x] = (uchar)(pow(((float)x + 1) / 256, waveform_gamma) * 255);
  }

  for (y = 0; y < ibuf->y; y++) {
    uchar *last_p = NULL;

    for (x = 0; x < ibuf->x; x++) {
      const uchar *rgb = src + 4 * (ibuf->x * y + x);
      float v = (float)IMB_colormanagement_get_luminance_byte(rgb) / 255.0f;
      uchar *p = tgt;
      p += 4 * (w * ((int)(v * (h - 3)) + 1) + x + 1);

      scope_put_pixel(wtable, p);
      p += 4 * w;
      scope_put_pixel(wtable, p);

      if (last_p != NULL) {
        wform_put_line(w, last_p, p);
      }
      last_p = p;
    }
  }

  return rval;
}

static ImBuf *make_waveform_view_from_ibuf_float(ImBuf *ibuf)
{
  ImBuf *rval = IMB_allocImBuf(ibuf->x + 3, 515, 32, IB_rect);
  int x, y;
  const float *src = ibuf->float_buffer.data;
  uchar *tgt = rval->byte_buffer.data;
  int w = ibuf->x + 3;
  int h = 515;
  float waveform_gamma = 0.2;
  uchar wtable[256];

  wform_put_grid(tgt, w, h);

  for (x = 0; x < 256; x++) {
    wtable[x] = (uchar)(pow(((float)x + 1) / 256, waveform_gamma) * 255);
  }

  for (y = 0; y < ibuf->y; y++) {
    uchar *last_p = NULL;

    for (x = 0; x < ibuf->x; x++) {
      const float *rgb = src + 4 * (ibuf->x * y + x);
      float v = IMB_colormanagement_get_luminance(rgb);
      uchar *p = tgt;

      CLAMP(v, 0.0f, 1.0f);

      p += 4 * (w * ((int)(v * (h - 3)) + 1) + x + 1);

      scope_put_pixel(wtable, p);
      p += 4 * w;
      scope_put_pixel(wtable, p);

      if (last_p != NULL) {
        wform_put_line(w, last_p, p);
      }
      last_p = p;
    }
  }

  wform_put_border(tgt, w, h);

  return rval;
}

ImBuf *make_waveform_view_from_ibuf(ImBuf *ibuf)
{
  if (ibuf->float_buffer.data) {
    return make_waveform_view_from_ibuf_float(ibuf);
  }
  return make_waveform_view_from_ibuf_byte(ibuf);
}

static ImBuf *make_sep_waveform_view_from_ibuf_byte(ImBuf *ibuf)
{
  ImBuf *rval = IMB_allocImBuf(ibuf->x + 3, 515, 32, IB_rect);
  int x, y;
  const uchar *src = ibuf->byte_buffer.data;
  uchar *tgt = rval->byte_buffer.data;
  int w = ibuf->x + 3;
  int sw = ibuf->x / 3;
  int h = 515;
  float waveform_gamma = 0.2;
  uchar wtable[256];

  wform_put_grid(tgt, w, h);

  for (x = 0; x < 256; x++) {
    wtable[x] = (uchar)(pow(((float)x + 1) / 256, waveform_gamma) * 255);
  }

  for (y = 0; y < ibuf->y; y++) {
    uchar *last_p[3] = {NULL, NULL, NULL};

    for (x = 0; x < ibuf->x; x++) {
      int c;
      const uchar *rgb = src + 4 * (ibuf->x * y + x);
      for (c = 0; c < 3; c++) {
        uchar *p = tgt;
        p += 4 * (w * ((rgb[c] * (h - 3)) / 255 + 1) + c * sw + x / 3 + 1);

        scope_put_pixel_single(wtable, p, c);
        p += 4 * w;
        scope_put_pixel_single(wtable, p, c);

        if (last_p[c] != NULL) {
          wform_put_line_single(w, last_p[c], p, c);
        }
        last_p[c] = p;
      }
    }
  }

  wform_put_border(tgt, w, h);

  return rval;
}

static ImBuf *make_sep_waveform_view_from_ibuf_float(ImBuf *ibuf)
{
  ImBuf *rval = IMB_allocImBuf(ibuf->x + 3, 515, 32, IB_rect);
  int x, y;
  const float *src = ibuf->float_buffer.data;
  uchar *tgt = rval->byte_buffer.data;
  int w = ibuf->x + 3;
  int sw = ibuf->x / 3;
  int h = 515;
  float waveform_gamma = 0.2;
  uchar wtable[256];

  wform_put_grid(tgt, w, h);

  for (x = 0; x < 256; x++) {
    wtable[x] = (uchar)(pow(((float)x + 1) / 256, waveform_gamma) * 255);
  }

  for (y = 0; y < ibuf->y; y++) {
    uchar *last_p[3] = {NULL, NULL, NULL};

    for (x = 0; x < ibuf->x; x++) {
      int c;
      const float *rgb = src + 4 * (ibuf->x * y + x);
      for (c = 0; c < 3; c++) {
        uchar *p = tgt;
        float v = rgb[c];

        CLAMP(v, 0.0f, 1.0f);

        p += 4 * (w * ((int)(v * (h - 3)) + 1) + c * sw + x / 3 + 1);

        scope_put_pixel_single(wtable, p, c);
        p += 4 * w;
        scope_put_pixel_single(wtable, p, c);

        if (last_p[c] != NULL) {
          wform_put_line_single(w, last_p[c], p, c);
        }
        last_p[c] = p;
      }
    }
  }

  wform_put_border(tgt, w, h);

  return rval;
}

ImBuf *make_sep_waveform_view_from_ibuf(ImBuf *ibuf)
{
  if (ibuf->float_buffer.data) {
    return make_sep_waveform_view_from_ibuf_float(ibuf);
  }
  return make_sep_waveform_view_from_ibuf_byte(ibuf);
}

static void draw_zebra_byte(ImBuf *src, ImBuf *ibuf, float perc)
{
  uint limit = 255.0f * perc / 100.0f;
  uchar *p = src->byte_buffer.data;
  uchar *o = ibuf->byte_buffer.data;
  int x;
  int y;

  for (y = 0; y < ibuf->y; y++) {
    for (x = 0; x < ibuf->x; x++) {
      uchar r = *p++;
      uchar g = *p++;
      uchar b = *p++;
      uchar a = *p++;

      if (r >= limit || g >= limit || b >= limit) {
        if (((x + y) & 0x08) != 0) {
          r = 255 - r;
          g = 255 - g;
          b = 255 - b;
        }
      }
      *o++ = r;
      *o++ = g;
      *o++ = b;
      *o++ = a;
    }
  }
}

static void draw_zebra_float(ImBuf *src, ImBuf *ibuf, float perc)
{
  float limit = perc / 100.0f;
  const float *p = src->float_buffer.data;
  uchar *o = ibuf->byte_buffer.data;
  int x;
  int y;

  for (y = 0; y < ibuf->y; y++) {
    for (x = 0; x < ibuf->x; x++) {
      float r = *p++;
      float g = *p++;
      float b = *p++;
      float a = *p++;

      if (r >= limit || g >= limit || b >= limit) {
        if (((x + y) & 0x08) != 0) {
          r = -r;
          g = -g;
          b = -b;
        }
      }

      *o++ = unit_float_to_uchar_clamp(r);
      *o++ = unit_float_to_uchar_clamp(g);
      *o++ = unit_float_to_uchar_clamp(b);
      *o++ = unit_float_to_uchar_clamp(a);
    }
  }
}

ImBuf *make_zebra_view_from_ibuf(ImBuf *ibuf, float perc)
{
  ImBuf *new_ibuf = IMB_allocImBuf(ibuf->x, ibuf->y, 32, IB_rect);

  if (ibuf->float_buffer.data) {
    draw_zebra_float(ibuf, new_ibuf, perc);
  }
  else {
    draw_zebra_byte(ibuf, new_ibuf, perc);
  }
  return new_ibuf;
}

static void draw_histogram_marker(ImBuf *ibuf, int x)
{
  uchar *p = ibuf->byte_buffer.data;
  int barh = ibuf->y * 0.1;

  p += 4 * (x + ibuf->x * (ibuf->y - barh + 1));

  for (int i = 0; i < barh - 1; i++) {
    p[0] = p[1] = p[2] = 255;
    p += ibuf->x * 4;
  }
}

static void draw_histogram_bar(ImBuf *ibuf, int x, float val, int col)
{
  uchar *p = ibuf->byte_buffer.data;
  int barh = ibuf->y * val * 0.9f;

  p += 4 * (x + ibuf->x);

  for (int i = 0; i < barh; i++) {
    p[col] = 255;
    p += ibuf->x * 4;
  }
}

#define HIS_STEPS 512

typedef struct MakeHistogramViewData {
  const ImBuf *ibuf;
} MakeHistogramViewData;

static void make_histogram_view_from_ibuf_byte_fn(void *__restrict userdata,
                                                  const int y,
                                                  const TaskParallelTLS *__restrict tls)
{
  MakeHistogramViewData *data = userdata;
  const ImBuf *ibuf = data->ibuf;
  const uchar *src = ibuf->byte_buffer.data;

  uint32_t(*cur_bins)[HIS_STEPS] = tls->userdata_chunk;

  for (int x = 0; x < ibuf->x; x++) {
    const uchar *pixel = src + (y * ibuf->x + x) * 4;

    for (int j = 3; j--;) {
      cur_bins[j][pixel[j]]++;
    }
  }
}

static void make_histogram_view_from_ibuf_reduce(const void *__restrict UNUSED(userdata),
                                                 void *__restrict chunk_join,
                                                 void *__restrict chunk)
{
  uint32_t(*join_bins)[HIS_STEPS] = chunk_join;
  uint32_t(*bins)[HIS_STEPS] = chunk;

  for (int j = 3; j--;) {
    for (int i = 0; i < HIS_STEPS; i++) {
      join_bins[j][i] += bins[j][i];
    }
  }
}

static ImBuf *make_histogram_view_from_ibuf_byte(ImBuf *ibuf)
{
  ImBuf *rval = IMB_allocImBuf(515, 128, 32, IB_rect);
  int x;
  uint nr, ng, nb;

  uint bins[3][HIS_STEPS];

  memset(bins, 0, sizeof(bins));

  MakeHistogramViewData data = {
      .ibuf = ibuf,
  };
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (ibuf->y >= 256);
  settings.userdata_chunk = bins;
  settings.userdata_chunk_size = sizeof(bins);
  settings.func_reduce = make_histogram_view_from_ibuf_reduce;
  BLI_task_parallel_range(0, ibuf->y, &data, make_histogram_view_from_ibuf_byte_fn, &settings);

  nr = nb = ng = 0;
  for (x = 0; x < HIS_STEPS; x++) {
    if (bins[0][x] > nr) {
      nr = bins[0][x];
    }
    if (bins[1][x] > ng) {
      ng = bins[1][x];
    }
    if (bins[2][x] > nb) {
      nb = bins[2][x];
    }
  }

  for (x = 0; x < HIS_STEPS; x++) {
    if (nr) {
      draw_histogram_bar(rval, x * 2 + 1, ((float)bins[0][x]) / nr, 0);
      draw_histogram_bar(rval, x * 2 + 2, ((float)bins[0][x]) / nr, 0);
    }
    if (ng) {
      draw_histogram_bar(rval, x * 2 + 1, ((float)bins[1][x]) / ng, 1);
      draw_histogram_bar(rval, x * 2 + 2, ((float)bins[1][x]) / ng, 1);
    }
    if (nb) {
      draw_histogram_bar(rval, x * 2 + 1, ((float)bins[2][x]) / nb, 2);
      draw_histogram_bar(rval, x * 2 + 2, ((float)bins[2][x]) / nb, 2);
    }
  }

  wform_put_border(rval->byte_buffer.data, rval->x, rval->y);

  return rval;
}

BLI_INLINE int get_bin_float(float f)
{
  if (f < -0.25f) {
    return 0;
  }
  if (f >= 1.25f) {
    return 511;
  }

  return (int)(((f + 0.25f) / 1.5f) * 512);
}

static void make_histogram_view_from_ibuf_float_fn(void *__restrict userdata,
                                                   const int y,
                                                   const TaskParallelTLS *__restrict tls)
{
  const MakeHistogramViewData *data = userdata;
  const ImBuf *ibuf = data->ibuf;
  const float *src = ibuf->float_buffer.data;

  uint32_t(*cur_bins)[HIS_STEPS] = tls->userdata_chunk;

  for (int x = 0; x < ibuf->x; x++) {
    const float *pixel = src + (y * ibuf->x + x) * 4;

    for (int j = 3; j--;) {
      cur_bins[j][get_bin_float(pixel[j])]++;
    }
  }
}

static ImBuf *make_histogram_view_from_ibuf_float(ImBuf *ibuf)
{
  ImBuf *rval = IMB_allocImBuf(515, 128, 32, IB_rect);
  int nr, ng, nb;
  int x;

  uint bins[3][HIS_STEPS];

  memset(bins, 0, sizeof(bins));

  MakeHistogramViewData data = {
      .ibuf = ibuf,
  };
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (ibuf->y >= 256);
  settings.userdata_chunk = bins;
  settings.userdata_chunk_size = sizeof(bins);
  settings.func_reduce = make_histogram_view_from_ibuf_reduce;
  BLI_task_parallel_range(0, ibuf->y, &data, make_histogram_view_from_ibuf_float_fn, &settings);

  nr = nb = ng = 0;
  for (x = 0; x < HIS_STEPS; x++) {
    if (bins[0][x] > nr) {
      nr = bins[0][x];
    }
    if (bins[1][x] > ng) {
      ng = bins[1][x];
    }
    if (bins[2][x] > nb) {
      nb = bins[2][x];
    }
  }

  for (x = 0; x < HIS_STEPS; x++) {
    if (nr) {
      draw_histogram_bar(rval, x + 1, ((float)bins[0][x]) / nr, 0);
    }
    if (ng) {
      draw_histogram_bar(rval, x + 1, ((float)bins[1][x]) / ng, 1);
    }
    if (nb) {
      draw_histogram_bar(rval, x + 1, ((float)bins[2][x]) / nb, 2);
    }
  }

  draw_histogram_marker(rval, get_bin_float(0.0));
  draw_histogram_marker(rval, get_bin_float(1.0));
  wform_put_border(rval->byte_buffer.data, rval->x, rval->y);

  return rval;
}

#undef HIS_STEPS

ImBuf *make_histogram_view_from_ibuf(ImBuf *ibuf)
{
  if (ibuf->float_buffer.data) {
    return make_histogram_view_from_ibuf_float(ibuf);
  }
  return make_histogram_view_from_ibuf_byte(ibuf);
}

static void vectorscope_put_cross(uchar r, uchar g, uchar b, uchar *tgt, int w, int h, int size)
{
  float rgb[3], yuv[3];
  uchar *p;

  rgb[0] = (float)r / 255.0f;
  rgb[1] = (float)g / 255.0f;
  rgb[2] = (float)b / 255.0f;
  rgb_to_yuv_normalized(rgb, yuv);

  p = tgt + 4 * (w * (int)(yuv[2] * (h - 3) + 1) + (int)(yuv[1] * (w - 3) + 1));

  if (r == 0 && g == 0 && b == 0) {
    r = 255;
  }

  for (int y = -size; y <= size; y++) {
    for (int x = -size; x <= size; x++) {
      uchar *q = p + 4 * (y * w + x);
      q[0] = r;
      q[1] = g;
      q[2] = b;
      q[3] = 255;
    }
  }
}

static ImBuf *make_vectorscope_view_from_ibuf_byte(ImBuf *ibuf)
{
  ImBuf *rval = IMB_allocImBuf(515, 515, 32, IB_rect);
  int x, y;
  const uchar *src = ibuf->byte_buffer.data;
  uchar *tgt = rval->byte_buffer.data;
  float rgb[3], yuv[3];
  int w = 515;
  int h = 515;
  float scope_gamma = 0.2;
  uchar wtable[256];

  for (x = 0; x < 256; x++) {
    wtable[x] = (uchar)(pow(((float)x + 1) / 256, scope_gamma) * 255);
  }

  for (x = 0; x < 256; x++) {
    vectorscope_put_cross(255, 0, 255 - x, tgt, w, h, 1);
    vectorscope_put_cross(255, x, 0, tgt, w, h, 1);
    vectorscope_put_cross(255 - x, 255, 0, tgt, w, h, 1);
    vectorscope_put_cross(0, 255, x, tgt, w, h, 1);
    vectorscope_put_cross(0, 255 - x, 255, tgt, w, h, 1);
    vectorscope_put_cross(x, 0, 255, tgt, w, h, 1);
  }

  for (y = 0; y < ibuf->y; y++) {
    for (x = 0; x < ibuf->x; x++) {
      const uchar *src1 = src + 4 * (ibuf->x * y + x);
      uchar *p;

      rgb[0] = (float)src1[0] / 255.0f;
      rgb[1] = (float)src1[1] / 255.0f;
      rgb[2] = (float)src1[2] / 255.0f;
      rgb_to_yuv_normalized(rgb, yuv);

      p = tgt + 4 * (w * (int)(yuv[2] * (h - 3) + 1) + (int)(yuv[1] * (w - 3) + 1));
      scope_put_pixel(wtable, (uchar *)p);
    }
  }

  vectorscope_put_cross(0, 0, 0, tgt, w, h, 3);

  return rval;
}

static ImBuf *make_vectorscope_view_from_ibuf_float(ImBuf *ibuf)
{
  ImBuf *rval = IMB_allocImBuf(515, 515, 32, IB_rect);
  int x, y;
  const float *src = ibuf->float_buffer.data;
  uchar *tgt = rval->byte_buffer.data;
  float rgb[3], yuv[3];
  int w = 515;
  int h = 515;
  float scope_gamma = 0.2;
  uchar wtable[256];

  for (x = 0; x < 256; x++) {
    wtable[x] = (uchar)(pow(((float)x + 1) / 256, scope_gamma) * 255);
  }

  for (x = 0; x <= 255; x++) {
    vectorscope_put_cross(255, 0, 255 - x, tgt, w, h, 1);
    vectorscope_put_cross(255, x, 0, tgt, w, h, 1);
    vectorscope_put_cross(255 - x, 255, 0, tgt, w, h, 1);
    vectorscope_put_cross(0, 255, x, tgt, w, h, 1);
    vectorscope_put_cross(0, 255 - x, 255, tgt, w, h, 1);
    vectorscope_put_cross(x, 0, 255, tgt, w, h, 1);
  }

  for (y = 0; y < ibuf->y; y++) {
    for (x = 0; x < ibuf->x; x++) {
      const float *src1 = src + 4 * (ibuf->x * y + x);
      const uchar *p;

      memcpy(rgb, src1, sizeof(float[3]));

      clamp_v3(rgb, 0.0f, 1.0f);

      rgb_to_yuv_normalized(rgb, yuv);

      p = tgt + 4 * (w * (int)(yuv[2] * (h - 3) + 1) + (int)(yuv[1] * (w - 3) + 1));
      scope_put_pixel(wtable, (uchar *)p);
    }
  }

  vectorscope_put_cross(0, 0, 0, tgt, w, h, 3);

  return rval;
}

ImBuf *make_vectorscope_view_from_ibuf(ImBuf *ibuf)
{
  if (ibuf->float_buffer.data) {
    return make_vectorscope_view_from_ibuf_float(ibuf);
  }
  return make_vectorscope_view_from_ibuf_byte(ibuf);
}
