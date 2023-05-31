/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "imbuf.h"

void IMB_flipy(struct ImBuf *ibuf)
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

void IMB_flipx(struct ImBuf *ibuf)
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
        SWAP(uint, rect[x_offset + xr], rect[x_offset + xl]);
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
