/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "imbuf.hh"

bool IMB_rotate_orthogonal(ImBuf *ibuf, int degrees)
{
  if (!ELEM(degrees, 90, 180, 270)) {
    return false;
  }

  const int size_x = ibuf->x;
  const int size_y = ibuf->y;

  if (ibuf->float_buffer.data) {
    float *float_pixels = ibuf->float_buffer.data;
    float *orig_float_pixels = static_cast<float *>(MEM_dupallocN(float_pixels));
    const int channels = ibuf->channels;
    if (degrees == 90) {
      std::swap(ibuf->x, ibuf->y);
      for (int y = 0; y < size_y; y++) {
        for (int x = 0; x < size_x; x++) {
          const float *source_pixel = &orig_float_pixels[(y * size_x + x) * channels];
          memcpy(&float_pixels[(y + ((size_x - x - 1) * size_y)) * channels],
                 source_pixel,
                 sizeof(float) * channels);
        }
      }
    }
    else if (degrees == 180) {
      for (int y = 0; y < size_y; y++) {
        for (int x = 0; x < size_x; x++) {
          const float *source_pixel = &orig_float_pixels[(y * size_x + x) * channels];
          memcpy(&float_pixels[(((size_y - y - 1) * size_x) + (size_x - x - 1)) * channels],
                 source_pixel,
                 sizeof(float) * channels);
        }
      }
    }
    else if (degrees == 270) {
      std::swap(ibuf->x, ibuf->y);
      for (int y = 0; y < size_y; y++) {
        for (int x = 0; x < size_x; x++) {
          const float *source_pixel = &orig_float_pixels[(y * size_x + x) * channels];
          memcpy(&float_pixels[((size_y - y - 1) + (x * size_y)) * channels],
                 source_pixel,
                 sizeof(float) * channels);
        }
      }
    }
    MEM_freeN(orig_float_pixels);
    if (ibuf->byte_buffer.data) {
      IMB_rect_from_float(ibuf);
    }
  }
  else if (ibuf->byte_buffer.data) {
    uchar *char_pixels = ibuf->byte_buffer.data;
    uchar *orig_char_pixels = static_cast<uchar *>(MEM_dupallocN(char_pixels));
    if (degrees == 90) {
      std::swap(ibuf->x, ibuf->y);
      for (int y = 0; y < size_y; y++) {
        for (int x = 0; x < size_x; x++) {
          const uchar *source_pixel = &orig_char_pixels[(y * size_x + x) * 4];
          memcpy(
              &char_pixels[(y + ((size_x - x - 1) * size_y)) * 4], source_pixel, sizeof(uchar[4]));
        }
      }
    }
    else if (degrees == 180) {
      for (int y = 0; y < size_y; y++) {
        for (int x = 0; x < size_x; x++) {
          const uchar *source_pixel = &orig_char_pixels[(y * size_x + x) * 4];
          memcpy(&char_pixels[(((size_y - y - 1) * size_x) + (size_x - x - 1)) * 4],
                 source_pixel,
                 sizeof(uchar[4]));
        }
      }
    }
    else if (degrees == 270) {
      std::swap(ibuf->x, ibuf->y);
      for (int y = 0; y < size_y; y++) {
        for (int x = 0; x < size_x; x++) {
          const uchar *source_pixel = &orig_char_pixels[(y * size_x + x) * 4];
          memcpy(
              &char_pixels[((size_y - y - 1) + (x * size_y)) * 4], source_pixel, sizeof(uchar[4]));
        }
      }
    }
    MEM_freeN(orig_char_pixels);
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
    line = static_cast<uint *>(MEM_mallocN(stride, "linebuf"));

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
    linef = static_cast<float *>(MEM_mallocN(stride, "linebuf"));

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
