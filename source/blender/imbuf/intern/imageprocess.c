/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup imbuf
 *
 * This file was moved here from the `src/` directory.
 * It is meant to deal with endianness. It resided in a general blending lib.
 * The other functions were only used during rendering. This single function remained.
 * It should probably move to `imbuf/intern/util.c`, but we'll keep it here for the time being.
 */

#include <math.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include <math.h>

/* Only this one is used liberally here, and in imbuf */
void IMB_convert_rgba_to_abgr(struct ImBuf *ibuf)
{
  size_t size;
  unsigned char rt, *cp = (unsigned char *)ibuf->rect;
  float rtf, *cpf = ibuf->rect_float;

  if (ibuf->rect) {
    size = ibuf->x * ibuf->y;

    while (size-- > 0) {
      rt = cp[0];
      cp[0] = cp[3];
      cp[3] = rt;
      rt = cp[1];
      cp[1] = cp[2];
      cp[2] = rt;
      cp += 4;
    }
  }

  if (ibuf->rect_float) {
    size = ibuf->x * ibuf->y;

    while (size-- > 0) {
      rtf = cpf[0];
      cpf[0] = cpf[3];
      cpf[3] = rtf;
      rtf = cpf[1];
      cpf[1] = cpf[2];
      cpf[2] = rtf;
      cpf += 4;
    }
  }
}

static void pixel_from_buffer(struct ImBuf *ibuf, unsigned char **outI, float **outF, int x, int y)

{
  size_t offset = ((size_t)ibuf->x) * y * 4 + 4 * x;

  if (ibuf->rect) {
    *outI = (unsigned char *)ibuf->rect + offset;
  }

  if (ibuf->rect_float) {
    *outF = ibuf->rect_float + offset;
  }
}

/* -------------------------------------------------------------------- */
/** \name Bi-Cubic Interpolation
 * \{ */

void bicubic_interpolation_color(
    struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v)
{
  if (outF) {
    BLI_bicubic_interpolation_fl(in->rect_float, outF, in->x, in->y, 4, u, v);
  }
  else {
    BLI_bicubic_interpolation_char((unsigned char *)in->rect, outI, in->x, in->y, 4, u, v);
  }
}

void bicubic_interpolation(ImBuf *in, ImBuf *out, float u, float v, int xout, int yout)
{
  unsigned char *outI = NULL;
  float *outF = NULL;

  if (in == NULL || (in->rect == NULL && in->rect_float == NULL)) {
    return;
  }

  /* GCC warns these could be uninitialized, but its ok. */
  pixel_from_buffer(out, &outI, &outF, xout, yout);

  bicubic_interpolation_color(in, outI, outF, u, v);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bi-Linear Interpolation
 * \{ */

BLI_INLINE void bilinear_interpolation_color_fl(
    struct ImBuf *in, unsigned char UNUSED(outI[4]), float outF[4], float u, float v)
{
  BLI_assert(outF);
  BLI_assert(in->rect_float);
  BLI_bilinear_interpolation_fl(in->rect_float, outF, in->x, in->y, 4, u, v);
}

BLI_INLINE void bilinear_interpolation_color_char(
    struct ImBuf *in, unsigned char outI[4], float UNUSED(outF[4]), float u, float v)
{
  BLI_assert(outI);
  BLI_assert(in->rect);
  BLI_bilinear_interpolation_char((unsigned char *)in->rect, outI, in->x, in->y, 4, u, v);
}

void bilinear_interpolation_color(
    struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v)
{
  if (outF) {
    BLI_bilinear_interpolation_fl(in->rect_float, outF, in->x, in->y, 4, u, v);
  }
  else {
    BLI_bilinear_interpolation_char((unsigned char *)in->rect, outI, in->x, in->y, 4, u, v);
  }
}

/* function assumes out to be zero'ed, only does RGBA */
/* BILINEAR INTERPOLATION */

/* Note about wrapping, the u/v still needs to be within the image bounds,
 * just the interpolation is wrapped.
 * This the same as bilinear_interpolation_color except it wraps
 * rather than using empty and emptyI. */
void bilinear_interpolation_color_wrap(
    struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v)
{
  float *row1, *row2, *row3, *row4, a, b;
  unsigned char *row1I, *row2I, *row3I, *row4I;
  float a_b, ma_b, a_mb, ma_mb;
  int y1, y2, x1, x2;

  /* ImBuf in must have a valid rect or rect_float, assume this is already checked */

  x1 = (int)floor(u);
  x2 = (int)ceil(u);
  y1 = (int)floor(v);
  y2 = (int)ceil(v);

  /* sample area entirely outside image? */
  if (x2 < 0 || x1 > in->x - 1 || y2 < 0 || y1 > in->y - 1) {
    return;
  }

  /* Wrap interpolation pixels - main difference from #bilinear_interpolation_color. */
  if (x1 < 0) {
    x1 = in->x + x1;
  }
  if (y1 < 0) {
    y1 = in->y + y1;
  }

  if (x2 >= in->x) {
    x2 = x2 - in->x;
  }
  if (y2 >= in->y) {
    y2 = y2 - in->y;
  }

  a = u - floorf(u);
  b = v - floorf(v);
  a_b = a * b;
  ma_b = (1.0f - a) * b;
  a_mb = a * (1.0f - b);
  ma_mb = (1.0f - a) * (1.0f - b);

  if (outF) {
    /* sample including outside of edges of image */
    row1 = in->rect_float + ((size_t)in->x) * y1 * 4 + 4 * x1;
    row2 = in->rect_float + ((size_t)in->x) * y2 * 4 + 4 * x1;
    row3 = in->rect_float + ((size_t)in->x) * y1 * 4 + 4 * x2;
    row4 = in->rect_float + ((size_t)in->x) * y2 * 4 + 4 * x2;

    outF[0] = ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0];
    outF[1] = ma_mb * row1[1] + a_mb * row3[1] + ma_b * row2[1] + a_b * row4[1];
    outF[2] = ma_mb * row1[2] + a_mb * row3[2] + ma_b * row2[2] + a_b * row4[2];
    outF[3] = ma_mb * row1[3] + a_mb * row3[3] + ma_b * row2[3] + a_b * row4[3];

    /* clamp here or else we can easily get off-range */
    clamp_v4(outF, 0.0f, 1.0f);
  }
  if (outI) {
    /* sample including outside of edges of image */
    row1I = (unsigned char *)in->rect + ((size_t)in->x) * y1 * 4 + 4 * x1;
    row2I = (unsigned char *)in->rect + ((size_t)in->x) * y2 * 4 + 4 * x1;
    row3I = (unsigned char *)in->rect + ((size_t)in->x) * y1 * 4 + 4 * x2;
    row4I = (unsigned char *)in->rect + ((size_t)in->x) * y2 * 4 + 4 * x2;

    /* Tested with white images and this should not wrap back to zero. */
    outI[0] = roundf(ma_mb * row1I[0] + a_mb * row3I[0] + ma_b * row2I[0] + a_b * row4I[0]);
    outI[1] = roundf(ma_mb * row1I[1] + a_mb * row3I[1] + ma_b * row2I[1] + a_b * row4I[1]);
    outI[2] = roundf(ma_mb * row1I[2] + a_mb * row3I[2] + ma_b * row2I[2] + a_b * row4I[2]);
    outI[3] = roundf(ma_mb * row1I[3] + a_mb * row3I[3] + ma_b * row2I[3] + a_b * row4I[3]);
  }
}

void bilinear_interpolation(ImBuf *in, ImBuf *out, float u, float v, int xout, int yout)
{
  unsigned char *outI = NULL;
  float *outF = NULL;

  if (in == NULL || (in->rect == NULL && in->rect_float == NULL)) {
    return;
  }

  /* gcc warns these could be uninitialized, but its ok. */
  pixel_from_buffer(out, &outI, &outF, xout, yout);

  bilinear_interpolation_color(in, outI, outF, u, v);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Nearest Interpolation
 * \{ */

/* functions assumes out to be zero'ed, only does RGBA */
BLI_INLINE void nearest_interpolation_color_char(
    struct ImBuf *in, unsigned char outI[4], float UNUSED(outF[4]), float u, float v)
{
  BLI_assert(outI);
  BLI_assert(in->rect);
  /* ImBuf in must have a valid rect or rect_float, assume this is already checked */
  int x1 = (int)(u);
  int y1 = (int)(v);

  /* sample area entirely outside image? */
  if (x1 < 0 || x1 >= in->x || y1 < 0 || y1 >= in->y) {
    outI[0] = outI[1] = outI[2] = outI[3] = 0;
    return;
  }

  const size_t offset = (in->x * y1 + x1) * 4;
  const unsigned char *dataI = (unsigned char *)in->rect + offset;
  outI[0] = dataI[0];
  outI[1] = dataI[1];
  outI[2] = dataI[2];
  outI[3] = dataI[3];
}

BLI_INLINE void nearest_interpolation_color_fl(
    struct ImBuf *in, unsigned char UNUSED(outI[4]), float outF[4], float u, float v)
{
  BLI_assert(outF);
  BLI_assert(in->rect_float);
  /* ImBuf in must have a valid rect or rect_float, assume this is already checked */
  int x1 = (int)(u);
  int y1 = (int)(v);

  /* sample area entirely outside image? */
  if (x1 < 0 || x1 >= in->x || y1 < 0 || y1 >= in->y) {
    zero_v4(outF);
    return;
  }

  const size_t offset = (in->x * y1 + x1) * 4;
  const float *dataF = in->rect_float + offset;
  copy_v4_v4(outF, dataF);
}

void nearest_interpolation_color(
    struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v)
{
  if (outF) {
    nearest_interpolation_color_fl(in, outI, outF, u, v);
  }
  else {
    nearest_interpolation_color_char(in, outI, outF, u, v);
  }
}

void nearest_interpolation_color_wrap(
    struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v)
{
  const float *dataF;
  unsigned char *dataI;
  int y, x;

  /* ImBuf in must have a valid rect or rect_float, assume this is already checked */

  x = (int)floor(u);
  y = (int)floor(v);

  x = x % in->x;
  y = y % in->y;

  /* Wrap interpolation pixels - main difference from #nearest_interpolation_color. */
  if (x < 0) {
    x += in->x;
  }
  if (y < 0) {
    y += in->y;
  }

  dataI = (unsigned char *)in->rect + ((size_t)in->x) * y * 4 + 4 * x;
  if (outI) {
    outI[0] = dataI[0];
    outI[1] = dataI[1];
    outI[2] = dataI[2];
    outI[3] = dataI[3];
  }
  dataF = in->rect_float + ((size_t)in->x) * y * 4 + 4 * x;
  if (outF) {
    outF[0] = dataF[0];
    outF[1] = dataF[1];
    outF[2] = dataF[2];
    outF[3] = dataF[3];
  }
}

void nearest_interpolation(ImBuf *in, ImBuf *out, float u, float v, int xout, int yout)
{
  unsigned char *outI = NULL;
  float *outF = NULL;

  if (in == NULL || (in->rect == NULL && in->rect_float == NULL)) {
    return;
  }

  /* gcc warns these could be uninitialized, but its ok. */
  pixel_from_buffer(out, &outI, &outF, xout, yout);

  nearest_interpolation_color(in, outI, outF, u, v);
}

/* -------------------------------------------------------------------- */
/** \name Image transform
 * \{ */
typedef struct TransformUserData {
  ImBuf *src;
  ImBuf *dst;
  float start_uv[2];
  float add_x[2];
  float add_y[2];
  rctf src_crop;
} TransformUserData;

static void imb_transform_calc_start_uv(const float transform_matrix[3][3], float r_start_uv[2])
{
  float orig[2];
  orig[0] = 0.0f;
  orig[1] = 0.0f;
  mul_v2_m3v2(r_start_uv, transform_matrix, orig);
}

static void imb_transform_calc_add_x(const float transform_matrix[3][3],
                                     const float start_uv[2],
                                     const int width,
                                     float r_add_x[2])
{
  float uv_max_x[2];
  uv_max_x[0] = width;
  uv_max_x[1] = 0.0f;
  mul_v2_m3v2(r_add_x, transform_matrix, uv_max_x);
  sub_v2_v2(r_add_x, start_uv);
  mul_v2_fl(r_add_x, 1.0f / width);
}

static void imb_transform_calc_add_y(const float transform_matrix[3][3],
                                     const float start_uv[2],
                                     const int height,
                                     float r_add_y[2])
{
  float uv_max_y[2];
  uv_max_y[0] = 0.0f;
  uv_max_y[1] = height;
  mul_v2_m3v2(r_add_y, transform_matrix, uv_max_y);
  sub_v2_v2(r_add_y, start_uv);
  mul_v2_fl(r_add_y, 1.0f / height);
}

typedef void (*InterpolationColorFunction)(
    struct ImBuf *in, unsigned char outI[4], float outF[4], float u, float v);
BLI_INLINE void imb_transform_scanlines(const TransformUserData *user_data,
                                        int scanline,
                                        InterpolationColorFunction interpolation)
{
  const int width = user_data->dst->x;

  float uv[2];
  madd_v2_v2v2fl(uv, user_data->start_uv, user_data->add_y, scanline);

  unsigned char *outI = NULL;
  float *outF = NULL;
  pixel_from_buffer(user_data->dst, &outI, &outF, 0, scanline);

  for (int xi = 0; xi < width; xi++) {
    if (uv[0] >= user_data->src_crop.xmin && uv[0] < user_data->src_crop.xmax &&
        uv[1] >= user_data->src_crop.ymin && uv[1] < user_data->src_crop.ymax) {
      interpolation(user_data->src, outI, outF, uv[0], uv[1]);
    }
    add_v2_v2(uv, user_data->add_x);
    if (outI) {
      outI += 4;
    }
    if (outF) {
      outF += 4;
    }
  }
}

static void imb_transform_nearest_scanlines(void *custom_data, int scanline)
{
  const TransformUserData *user_data = custom_data;
  InterpolationColorFunction interpolation = NULL;
  if (user_data->dst->rect_float) {
    interpolation = nearest_interpolation_color_fl;
  }
  else {
    interpolation = nearest_interpolation_color_char;
  }
  imb_transform_scanlines(user_data, scanline, interpolation);
}

static void imb_transform_bilinear_scanlines(void *custom_data, int scanline)
{
  const TransformUserData *user_data = custom_data;
  InterpolationColorFunction interpolation = NULL;
  if (user_data->dst->rect_float) {
    interpolation = bilinear_interpolation_color_fl;
  }
  else if (user_data->dst->rect) {
    interpolation = bilinear_interpolation_color_char;
  }
  imb_transform_scanlines(user_data, scanline, interpolation);
}

static ScanlineThreadFunc imb_transform_scanline_func(const eIMBInterpolationFilterMode filter)
{
  ScanlineThreadFunc scanline_func = NULL;
  switch (filter) {
    case IMB_FILTER_NEAREST:
      scanline_func = imb_transform_nearest_scanlines;
      break;
    case IMB_FILTER_BILINEAR:
      scanline_func = imb_transform_bilinear_scanlines;
      break;
  }
  return scanline_func;
}

void IMB_transform(struct ImBuf *src,
                   struct ImBuf *dst,
                   float transform_matrix[3][3],
                   struct rctf *src_crop,
                   const eIMBInterpolationFilterMode filter)
{
  TransformUserData user_data;
  user_data.src = src;
  user_data.dst = dst;
  user_data.src_crop = *src_crop;
  imb_transform_calc_start_uv(transform_matrix, user_data.start_uv);
  imb_transform_calc_add_x(transform_matrix, user_data.start_uv, src->x, user_data.add_x);
  imb_transform_calc_add_y(transform_matrix, user_data.start_uv, src->y, user_data.add_y);
  ScanlineThreadFunc scanline_func = imb_transform_scanline_func(filter);
  IMB_processor_apply_threaded_scanlines(dst->y, scanline_func, &user_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Threaded Image Processing
 * \{ */

static void processor_apply_func(TaskPool *__restrict pool, void *taskdata)
{
  void (*do_thread)(void *) = (void (*)(void *))BLI_task_pool_user_data(pool);
  do_thread(taskdata);
}

void IMB_processor_apply_threaded(
    int buffer_lines,
    int handle_size,
    void *init_customdata,
    void(init_handle)(void *handle, int start_line, int tot_line, void *customdata),
    void *(do_thread)(void *))
{
  const int lines_per_task = 64;

  TaskPool *task_pool;

  void *handles;
  int total_tasks = (buffer_lines + lines_per_task - 1) / lines_per_task;
  int i, start_line;

  task_pool = BLI_task_pool_create(do_thread, TASK_PRIORITY_LOW);

  handles = MEM_callocN(handle_size * total_tasks, "processor apply threaded handles");

  start_line = 0;

  for (i = 0; i < total_tasks; i++) {
    int lines_per_current_task;
    void *handle = ((char *)handles) + handle_size * i;

    if (i < total_tasks - 1) {
      lines_per_current_task = lines_per_task;
    }
    else {
      lines_per_current_task = buffer_lines - start_line;
    }

    init_handle(handle, start_line, lines_per_current_task, init_customdata);

    BLI_task_pool_push(task_pool, processor_apply_func, handle, false, NULL);

    start_line += lines_per_task;
  }

  /* work and wait until tasks are done */
  BLI_task_pool_work_and_wait(task_pool);

  /* Free memory. */
  MEM_freeN(handles);
  BLI_task_pool_free(task_pool);
}

typedef struct ScanlineGlobalData {
  void *custom_data;
  ScanlineThreadFunc do_thread;
} ScanlineGlobalData;

static void processor_apply_parallel(void *__restrict userdata,
                                     const int scanline,
                                     const TaskParallelTLS *__restrict UNUSED(tls))
{
  ScanlineGlobalData *data = userdata;
  data->do_thread(data->custom_data, scanline);
}

void IMB_processor_apply_threaded_scanlines(int total_scanlines,
                                            ScanlineThreadFunc do_thread,
                                            void *custom_data)
{
  TaskParallelSettings settings;
  ScanlineGlobalData data = {
      .do_thread = do_thread,
      .custom_data = custom_data,
  };

  BLI_parallel_range_settings_defaults(&settings);
  BLI_task_parallel_range(0, total_scanlines, &data, processor_apply_parallel, &settings);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Alpha-under
 * \{ */

void IMB_alpha_under_color_float(float *rect_float, int x, int y, float backcol[3])
{
  size_t a = ((size_t)x) * y;
  float *fp = rect_float;

  while (a--) {
    const float mul = 1.0f - fp[3];
    madd_v3_v3fl(fp, backcol, mul);
    fp[3] = 1.0f;

    fp += 4;
  }
}

void IMB_alpha_under_color_byte(unsigned char *rect, int x, int y, const float backcol[3])
{
  size_t a = ((size_t)x) * y;
  unsigned char *cp = rect;

  while (a--) {
    if (cp[3] == 255) {
      /* pass */
    }
    else if (cp[3] == 0) {
      cp[0] = backcol[0] * 255;
      cp[1] = backcol[1] * 255;
      cp[2] = backcol[2] * 255;
    }
    else {
      float alpha = cp[3] / 255.0;
      float mul = 1.0f - alpha;

      cp[0] = (cp[0] * alpha) + mul * backcol[0];
      cp[1] = (cp[1] * alpha) + mul * backcol[1];
      cp[2] = (cp[2] * alpha) + mul * backcol[2];
    }

    cp[3] = 255;

    cp += 4;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sample Pixel
 * \{ */

/* Sample pixel of image using NEAREST method. */
void IMB_sampleImageAtLocation(ImBuf *ibuf, float x, float y, bool make_linear_rgb, float color[4])
{
  if (ibuf->rect_float) {
    nearest_interpolation_color(ibuf, NULL, color, x, y);
  }
  else {
    unsigned char byte_color[4];
    nearest_interpolation_color(ibuf, byte_color, NULL, x, y);
    rgba_uchar_to_float(color, byte_color);
    if (make_linear_rgb) {
      IMB_colormanagement_colorspace_to_scene_linear_v4(color, false, ibuf->rect_colorspace);
    }
  }
}

/** \} */
