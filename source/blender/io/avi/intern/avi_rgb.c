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
 * \ingroup avi
 *
 * This is external code. Converts rgb-type avi-s.
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "AVI_avi.h"
#include "avi_rgb.h"

#include "IMB_imbuf.h"

#include "BLI_utildefines.h"

/* implementation */

void *avi_converter_from_avi_rgb(AviMovie *movie,
                                 int stream,
                                 unsigned char *buffer,
                                 const size_t *size)
{
  unsigned char *buf;
  AviBitmapInfoHeader *bi;
  short bits = 32;

  (void)size; /* unused */

  bi = (AviBitmapInfoHeader *)movie->streams[stream].sf;
  if (bi) {
    bits = bi->BitCount;
  }

  if (bits == 16) {
    unsigned short *pxl;
    unsigned char *to;
#ifdef __BIG_ENDIAN__
    unsigned char *pxla;
#endif

    buf = imb_alloc_pixels(
        movie->header->Height, movie->header->Width, 3, sizeof(unsigned char), "fromavirgbbuf");

    if (buf) {
      size_t y = movie->header->Height;
      to = buf;

      while (y--) {
        pxl = (unsigned short *)(buffer + y * movie->header->Width * 2);

#ifdef __BIG_ENDIAN__
        pxla = (unsigned char *)pxl;
#endif

        size_t x = movie->header->Width;
        while (x--) {
#ifdef __BIG_ENDIAN__
          int i = pxla[0];
          pxla[0] = pxla[1];
          pxla[1] = i;

          pxla += 2;
#endif

          *(to++) = ((*pxl >> 10) & 0x1f) * 8;
          *(to++) = ((*pxl >> 5) & 0x1f) * 8;
          *(to++) = (*pxl & 0x1f) * 8;
          pxl++;
        }
      }
    }

    MEM_freeN(buffer);

    return buf;
  }

  buf = imb_alloc_pixels(
      movie->header->Height, movie->header->Width, 3, sizeof(unsigned char), "fromavirgbbuf");

  if (buf) {
    size_t rowstride = movie->header->Width * 3;
    BLI_assert(bits != 16);
    if (movie->header->Width % 2) {
      rowstride++;
    }

    for (size_t y = 0; y < movie->header->Height; y++) {
      memcpy(&buf[y * movie->header->Width * 3],
             &buffer[((movie->header->Height - 1) - y) * rowstride],
             movie->header->Width * 3);
    }

    for (size_t y = 0; y < (size_t)movie->header->Height * (size_t)movie->header->Width * 3;
         y += 3) {
      int i = buf[y];
      buf[y] = buf[y + 2];
      buf[y + 2] = i;
    }
  }

  MEM_freeN(buffer);

  return buf;
}

void *avi_converter_to_avi_rgb(AviMovie *movie, int stream, unsigned char *buffer, size_t *size)
{
  unsigned char *buf;

  (void)stream; /* unused */

  size_t rowstride = movie->header->Width * 3;
  /* AVI files has uncompressed lines 4-byte aligned */
  rowstride = (rowstride + 3) & ~3;

  *size = movie->header->Height * rowstride;
  buf = MEM_mallocN(*size, "toavirgbbuf");

  for (size_t y = 0; y < movie->header->Height; y++) {
    memcpy(&buf[y * rowstride],
           &buffer[((movie->header->Height - 1) - y) * movie->header->Width * 3],
           movie->header->Width * 3);
  }

  for (size_t y = 0; y < movie->header->Height; y++) {
    for (size_t x = 0; x < movie->header->Width * 3; x += 3) {
      int i = buf[y * rowstride + x];
      buf[y * rowstride + x] = buf[y * rowstride + x + 2];
      buf[y * rowstride + x + 2] = i;
    }
  }

  MEM_freeN(buffer);

  return buf;
}
