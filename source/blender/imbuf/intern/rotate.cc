/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

template<typename T>
static void rotate_pixels(const int degrees,
                          const int size_x,
                          const int size_y,
                          const T *src_pixels,
                          T *dst_pixels,
                          const int channels)
{
  using namespace blender;
  threading::parallel_for(IndexRange(size_y), 256, [&](const IndexRange y_range) {
    const T *src_pixel = src_pixels + y_range.first() * size_x * channels;
    if (degrees == 90) {
      for (int y : y_range) {
        for (int x = 0; x < size_x; x++, src_pixel += channels) {
          memcpy(&dst_pixels[(y + ((size_x - x - 1) * size_y)) * channels],
                 src_pixel,
                 sizeof(T) * channels);
        }
      }
    }
    else if (degrees == 180) {
      for (int y : y_range) {
        for (int x = 0; x < size_x; x++, src_pixel += channels) {
          memcpy(&dst_pixels[(((size_y - y - 1) * size_x) + (size_x - x - 1)) * channels],
                 src_pixel,
                 sizeof(T) * channels);
        }
      }
    }
    else if (degrees == 270) {
      for (int y : y_range) {
        for (int x = 0; x < size_x; x++, src_pixel += channels) {
          memcpy(&dst_pixels[((size_y - y - 1) + (x * size_y)) * channels],
                 src_pixel,
                 sizeof(T) * channels);
        }
      }
    }
  });
}

bool IMB_rotate_orthogonal(ImBuf *ibuf, int degrees)
{
  if (!ELEM(degrees, 90, 180, 270)) {
    return false;
  }

  const int size_x = ibuf->x;
  const int size_y = ibuf->y;

  if (ELEM(degrees, 90, 270)) {
    std::swap(ibuf->x, ibuf->y);
  }
  if (ibuf->float_buffer.data) {
    const int channels = ibuf->channels;
    const float *src_pixels = ibuf->float_buffer.data;
    float *dst_pixels = MEM_malloc_arrayN<float>(
        size_t(channels) * size_t(size_x) * size_t(size_y), __func__);
    rotate_pixels<float>(degrees, size_x, size_y, src_pixels, dst_pixels, ibuf->channels);
    IMB_assign_float_buffer(ibuf, dst_pixels, IB_TAKE_OWNERSHIP);
    if (ibuf->byte_buffer.data) {
      IMB_byte_from_float(ibuf);
    }
  }
  else if (ibuf->byte_buffer.data) {
    const uchar *src_pixels = ibuf->byte_buffer.data;
    uchar *dst_pixels = MEM_malloc_arrayN<uchar>(4 * size_t(size_x) * size_t(size_y), __func__);
    rotate_pixels<uchar>(degrees, size_x, size_y, src_pixels, dst_pixels, 4);
    IMB_assign_byte_buffer(ibuf, dst_pixels, IB_TAKE_OWNERSHIP);
  }

  return true;
}

void IMB_flipy(ImBuf *ibuf)
{
  size_t x_size, y_size;

  if (ibuf == nullptr) {
    return;
  }

  if (ibuf->byte_buffer.data) {
    uint *top, *bottom, *line;

    x_size = ibuf->x;
    y_size = ibuf->y;

    const size_t stride = x_size * sizeof(int);

    top = (uint *)ibuf->byte_buffer.data;
    bottom = top + ((y_size - 1) * x_size);
    line = MEM_malloc_arrayN<uint>(x_size, "linebuf");

    y_size >>= 1;

    for (; y_size > 0; y_size--) {
      memcpy(line, top, stride);
      memcpy(top, bottom, stride);
      memcpy(bottom, line, stride);
      bottom -= x_size;
      top += x_size;
    }

    MEM_freeN(line);
  }

  if (ibuf->float_buffer.data) {
    float *topf = nullptr, *bottomf = nullptr, *linef = nullptr;

    x_size = ibuf->x;
    y_size = ibuf->y;

    const size_t stride = x_size * 4 * sizeof(float);

    topf = ibuf->float_buffer.data;
    bottomf = topf + 4 * ((y_size - 1) * x_size);
    linef = MEM_malloc_arrayN<float>(4 * x_size, "linebuf");

    y_size >>= 1;

    for (; y_size > 0; y_size--) {
      memcpy(linef, topf, stride);
      memcpy(topf, bottomf, stride);
      memcpy(bottomf, linef, stride);
      bottomf -= 4 * x_size;
      topf += 4 * x_size;
    }

    MEM_freeN(linef);
  }
}

void IMB_flipx(ImBuf *ibuf)
{
  int x, y, xr, xl, yi;
  float px_f[4];

  if (ibuf == nullptr) {
    return;
  }

  x = ibuf->x;
  y = ibuf->y;

  if (ibuf->byte_buffer.data) {
    uint *rect = (uint *)ibuf->byte_buffer.data;
    for (yi = y - 1; yi >= 0; yi--) {
      const size_t x_offset = size_t(x) * yi;
      for (xr = x - 1, xl = 0; xr >= xl; xr--, xl++) {
        std::swap(rect[x_offset + xr], rect[x_offset + xl]);
      }
    }
  }

  if (ibuf->float_buffer.data) {
    float *rect_float = ibuf->float_buffer.data;
    for (yi = y - 1; yi >= 0; yi--) {
      const size_t x_offset = size_t(x) * yi;
      for (xr = x - 1, xl = 0; xr >= xl; xr--, xl++) {
        memcpy(&px_f, &rect_float[(x_offset + xr) * 4], sizeof(float[4]));
        memcpy(
            &rect_float[(x_offset + xr) * 4], &rect_float[(x_offset + xl) * 4], sizeof(float[4]));
        memcpy(&rect_float[(x_offset + xl) * 4], &px_f, sizeof(float[4]));
      }
    }
  }
}
