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
 * rotate.c
 */

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

  if (ibuf == NULL) {
    return;
  }

  if (ibuf->rect) {
    unsigned int *top, *bottom, *line;

    x_size = ibuf->x;
    y_size = ibuf->y;

    const size_t stride = x_size * sizeof(int);

    top = ibuf->rect;
    bottom = top + ((y_size - 1) * x_size);
    line = MEM_mallocN(stride, "linebuf");

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

  if (ibuf->rect_float) {
    float *topf = NULL, *bottomf = NULL, *linef = NULL;

    x_size = ibuf->x;
    y_size = ibuf->y;

    const size_t stride = x_size * 4 * sizeof(float);

    topf = ibuf->rect_float;
    bottomf = topf + 4 * ((y_size - 1) * x_size);
    linef = MEM_mallocN(stride, "linebuf");

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

  if (ibuf == NULL) {
    return;
  }

  x = ibuf->x;
  y = ibuf->y;

  if (ibuf->rect) {
    for (yi = y - 1; yi >= 0; yi--) {
      const size_t x_offset = (size_t)x * yi;
      for (xr = x - 1, xl = 0; xr >= xl; xr--, xl++) {
        SWAP(unsigned int, ibuf->rect[x_offset + xr], ibuf->rect[x_offset + xl]);
      }
    }
  }

  if (ibuf->rect_float) {
    for (yi = y - 1; yi >= 0; yi--) {
      const size_t x_offset = (size_t)x * yi;
      for (xr = x - 1, xl = 0; xr >= xl; xr--, xl++) {
        memcpy(&px_f, &ibuf->rect_float[(x_offset + xr) * 4], sizeof(float[4]));
        memcpy(&ibuf->rect_float[(x_offset + xr) * 4],
               &ibuf->rect_float[(x_offset + xl) * 4],
               sizeof(float[4]));
        memcpy(&ibuf->rect_float[(x_offset + xl) * 4], &px_f, sizeof(float[4]));
      }
    }
  }
}
