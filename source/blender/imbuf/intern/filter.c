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
 * filter.c
 */

/** \file
 * \ingroup imbuf
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math_base.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_filter.h"

#include "imbuf.h"

static void filtrow(unsigned char *point, int x)
{
  unsigned int c1, c2, c3, error;

  if (x > 1) {
    c1 = c2 = *point;
    error = 2;
    for (x--; x > 0; x--) {
      c3 = point[4];
      c1 += (c2 << 1) + c3 + error;
      error = c1 & 3;
      *point = c1 >> 2;
      point += 4;
      c1 = c2;
      c2 = c3;
    }
    *point = (c1 + (c2 << 1) + c2 + error) >> 2;
  }
}

static void filtrowf(float *point, int x)
{
  float c1, c2, c3;

  if (x > 1) {
    c1 = c2 = *point;
    for (x--; x > 0; x--) {
      c3 = point[4];
      c1 += (c2 * 2) + c3;
      *point = 0.25f * c1;
      point += 4;
      c1 = c2;
      c2 = c3;
    }
    *point = 0.25f * (c1 + (c2 * 2) + c2);
  }
}

static void filtcolum(unsigned char *point, int y, int skip)
{
  unsigned int c1, c2, c3, error;
  unsigned char *point2;

  if (y > 1) {
    c1 = c2 = *point;
    point2 = point;
    error = 2;
    for (y--; y > 0; y--) {
      point2 += skip;
      c3 = *point2;
      c1 += (c2 << 1) + c3 + error;
      error = c1 & 3;
      *point = c1 >> 2;
      point = point2;
      c1 = c2;
      c2 = c3;
    }
    *point = (c1 + (c2 << 1) + c2 + error) >> 2;
  }
}

static void filtcolumf(float *point, int y, int skip)
{
  float c1, c2, c3, *point2;

  if (y > 1) {
    c1 = c2 = *point;
    point2 = point;
    for (y--; y > 0; y--) {
      point2 += skip;
      c3 = *point2;
      c1 += (c2 * 2) + c3;
      *point = 0.25f * c1;
      point = point2;
      c1 = c2;
      c2 = c3;
    }
    *point = 0.25f * (c1 + (c2 * 2) + c2);
  }
}

void IMB_filtery(struct ImBuf *ibuf)
{
  unsigned char *point;
  float *pointf;
  int x, y, skip;

  point = (unsigned char *)ibuf->rect;
  pointf = ibuf->rect_float;

  x = ibuf->x;
  y = ibuf->y;
  skip = x << 2;

  for (; x > 0; x--) {
    if (point) {
      if (ibuf->planes > 24) {
        filtcolum(point, y, skip);
      }
      point++;
      filtcolum(point, y, skip);
      point++;
      filtcolum(point, y, skip);
      point++;
      filtcolum(point, y, skip);
      point++;
    }
    if (pointf) {
      if (ibuf->planes > 24) {
        filtcolumf(pointf, y, skip);
      }
      pointf++;
      filtcolumf(pointf, y, skip);
      pointf++;
      filtcolumf(pointf, y, skip);
      pointf++;
      filtcolumf(pointf, y, skip);
      pointf++;
    }
  }
}

void imb_filterx(struct ImBuf *ibuf)
{
  unsigned char *point;
  float *pointf;
  int x, y, skip;

  point = (unsigned char *)ibuf->rect;
  pointf = ibuf->rect_float;

  x = ibuf->x;
  y = ibuf->y;
  skip = (x << 2) - 3;

  for (; y > 0; y--) {
    if (point) {
      if (ibuf->planes > 24) {
        filtrow(point, x);
      }
      point++;
      filtrow(point, x);
      point++;
      filtrow(point, x);
      point++;
      filtrow(point, x);
      point += skip;
    }
    if (pointf) {
      if (ibuf->planes > 24) {
        filtrowf(pointf, x);
      }
      pointf++;
      filtrowf(pointf, x);
      pointf++;
      filtrowf(pointf, x);
      pointf++;
      filtrowf(pointf, x);
      pointf += skip;
    }
  }
}

static void imb_filterN(ImBuf *out, ImBuf *in)
{
  BLI_assert(out->channels == in->channels);
  BLI_assert(out->x == in->x && out->y == in->y);

  const int channels = in->channels;
  const int rowlen = in->x;

  if (in->rect && out->rect) {
    for (int y = 0; y < in->y; y++) {
      /* setup rows */
      const char *row2 = (const char *)in->rect + y * channels * rowlen;
      const char *row1 = (y == 0) ? row2 : row2 - channels * rowlen;
      const char *row3 = (y == in->y - 1) ? row2 : row2 + channels * rowlen;

      char *cp = (char *)out->rect + y * channels * rowlen;

      for (int x = 0; x < rowlen; x++) {
        const char *r11, *r13, *r21, *r23, *r31, *r33;

        if (x == 0) {
          r11 = row1;
          r21 = row2;
          r31 = row3;
        }
        else {
          r11 = row1 - channels;
          r21 = row2 - channels;
          r31 = row3 - channels;
        }

        if (x == rowlen - 1) {
          r13 = row1;
          r23 = row2;
          r33 = row3;
        }
        else {
          r13 = row1 + channels;
          r23 = row2 + channels;
          r33 = row3 + channels;
        }

        cp[0] = (r11[0] + 2 * row1[0] + r13[0] + 2 * r21[0] + 4 * row2[0] + 2 * r23[0] + r31[0] +
                 2 * row3[0] + r33[0]) >>
                4;
        cp[1] = (r11[1] + 2 * row1[1] + r13[1] + 2 * r21[1] + 4 * row2[1] + 2 * r23[1] + r31[1] +
                 2 * row3[1] + r33[1]) >>
                4;
        cp[2] = (r11[2] + 2 * row1[2] + r13[2] + 2 * r21[2] + 4 * row2[2] + 2 * r23[2] + r31[2] +
                 2 * row3[2] + r33[2]) >>
                4;
        cp[3] = (r11[3] + 2 * row1[3] + r13[3] + 2 * r21[3] + 4 * row2[3] + 2 * r23[3] + r31[3] +
                 2 * row3[3] + r33[3]) >>
                4;
        cp += channels;
        row1 += channels;
        row2 += channels;
        row3 += channels;
      }
    }
  }

  if (in->rect_float && out->rect_float) {
    for (int y = 0; y < in->y; y++) {
      /* setup rows */
      const float *row2 = (const float *)in->rect_float + y * channels * rowlen;
      const float *row1 = (y == 0) ? row2 : row2 - channels * rowlen;
      const float *row3 = (y == in->y - 1) ? row2 : row2 + channels * rowlen;

      float *cp = (float *)out->rect_float + y * channels * rowlen;

      for (int x = 0; x < rowlen; x++) {
        const float *r11, *r13, *r21, *r23, *r31, *r33;

        if (x == 0) {
          r11 = row1;
          r21 = row2;
          r31 = row3;
        }
        else {
          r11 = row1 - channels;
          r21 = row2 - channels;
          r31 = row3 - channels;
        }

        if (x == rowlen - 1) {
          r13 = row1;
          r23 = row2;
          r33 = row3;
        }
        else {
          r13 = row1 + channels;
          r23 = row2 + channels;
          r33 = row3 + channels;
        }

        cp[0] = (r11[0] + 2 * row1[0] + r13[0] + 2 * r21[0] + 4 * row2[0] + 2 * r23[0] + r31[0] +
                 2 * row3[0] + r33[0]) *
                (1.0f / 16.0f);
        cp[1] = (r11[1] + 2 * row1[1] + r13[1] + 2 * r21[1] + 4 * row2[1] + 2 * r23[1] + r31[1] +
                 2 * row3[1] + r33[1]) *
                (1.0f / 16.0f);
        cp[2] = (r11[2] + 2 * row1[2] + r13[2] + 2 * r21[2] + 4 * row2[2] + 2 * r23[2] + r31[2] +
                 2 * row3[2] + r33[2]) *
                (1.0f / 16.0f);
        cp[3] = (r11[3] + 2 * row1[3] + r13[3] + 2 * r21[3] + 4 * row2[3] + 2 * r23[3] + r31[3] +
                 2 * row3[3] + r33[3]) *
                (1.0f / 16.0f);
        cp += channels;
        row1 += channels;
        row2 += channels;
        row3 += channels;
      }
    }
  }
}

void IMB_filter(struct ImBuf *ibuf)
{
  IMB_filtery(ibuf);
  imb_filterx(ibuf);
}

void IMB_mask_filter_extend(char *mask, int width, int height)
{
  const char *row1, *row2, *row3;
  int rowlen, x, y;
  char *temprect;

  rowlen = width;

  /* make a copy, to prevent flooding */
  temprect = MEM_dupallocN(mask);

  for (y = 1; y <= height; y++) {
    /* setup rows */
    row1 = (char *)(temprect + (y - 2) * rowlen);
    row2 = row1 + rowlen;
    row3 = row2 + rowlen;
    if (y == 1) {
      row1 = row2;
    }
    else if (y == height) {
      row3 = row2;
    }

    for (x = 0; x < rowlen; x++) {
      if (mask[((y - 1) * rowlen) + x] == 0) {
        if (*row1 || *row2 || *row3 || *(row1 + 1) || *(row3 + 1)) {
          mask[((y - 1) * rowlen) + x] = FILTER_MASK_MARGIN;
        }
        else if ((x != rowlen - 1) && (*(row1 + 2) || *(row2 + 2) || *(row3 + 2))) {
          mask[((y - 1) * rowlen) + x] = FILTER_MASK_MARGIN;
        }
      }

      if (x != 0) {
        row1++;
        row2++;
        row3++;
      }
    }
  }

  MEM_freeN(temprect);
}

void IMB_mask_clear(ImBuf *ibuf, char *mask, int val)
{
  int x, y;
  if (ibuf->rect_float) {
    for (x = 0; x < ibuf->x; x++) {
      for (y = 0; y < ibuf->y; y++) {
        if (mask[ibuf->x * y + x] == val) {
          float *col = ibuf->rect_float + 4 * (ibuf->x * y + x);
          col[0] = col[1] = col[2] = col[3] = 0.0f;
        }
      }
    }
  }
  else {
    /* char buffer */
    for (x = 0; x < ibuf->x; x++) {
      for (y = 0; y < ibuf->y; y++) {
        if (mask[ibuf->x * y + x] == val) {
          char *col = (char *)(ibuf->rect + ibuf->x * y + x);
          col[0] = col[1] = col[2] = col[3] = 0;
        }
      }
    }
  }
}

static int filter_make_index(const int x, const int y, const int w, const int h)
{
  if (x < 0 || x >= w || y < 0 || y >= h) {
    return -1; /* return bad index */
  }
  else {
    return y * w + x;
  }
}

static int check_pixel_assigned(
    const void *buffer, const char *mask, const int index, const int depth, const bool is_float)
{
  int res = 0;

  if (index >= 0) {
    const int alpha_index = depth * index + (depth - 1);

    if (mask != NULL) {
      res = mask[index] != 0 ? 1 : 0;
    }
    else if ((is_float && ((const float *)buffer)[alpha_index] != 0.0f) ||
             (!is_float && ((const unsigned char *)buffer)[alpha_index] != 0)) {
      res = 1;
    }
  }

  return res;
}

/**
 * if alpha is zero, it checks surrounding pixels and averages color. sets new alphas to 1.0
 *
 * When a mask is given, only effect pixels with a mask value of 1,
 * defined as #BAKE_MASK_MARGIN in rendercore.c
 * */
void IMB_filter_extend(struct ImBuf *ibuf, char *mask, int filter)
{
  const int width = ibuf->x;
  const int height = ibuf->y;
  const int depth = 4; /* always 4 channels */
  const int chsize = ibuf->rect_float ? sizeof(float) : sizeof(unsigned char);
  const size_t bsize = ((size_t)width) * height * depth * chsize;
  const bool is_float = (ibuf->rect_float != NULL);
  void *dstbuf = (void *)MEM_dupallocN(ibuf->rect_float ? (void *)ibuf->rect_float :
                                                          (void *)ibuf->rect);
  char *dstmask = mask == NULL ? NULL : (char *)MEM_dupallocN(mask);
  void *srcbuf = ibuf->rect_float ? (void *)ibuf->rect_float : (void *)ibuf->rect;
  char *srcmask = mask;
  int cannot_early_out = 1, r, n, k, i, j, c;
  float weight[25];

  /* build a weights buffer */
  n = 1;

#if 0
  k = 0;
  for (i = -n; i <= n; i++) {
    for (j = -n; j <= n; j++) {
      weight[k++] = sqrt((float)i * i + j * j);
    }
  }
#endif

  weight[0] = 1;
  weight[1] = 2;
  weight[2] = 1;
  weight[3] = 2;
  weight[4] = 0;
  weight[5] = 2;
  weight[6] = 1;
  weight[7] = 2;
  weight[8] = 1;

  /* run passes */
  for (r = 0; cannot_early_out == 1 && r < filter; r++) {
    int x, y;
    cannot_early_out = 0;

    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
        const int index = filter_make_index(x, y, width, height);

        /* only update unassigned pixels */
        if (!check_pixel_assigned(srcbuf, srcmask, index, depth, is_float)) {
          float tmp[4];
          float wsum = 0;
          float acc[4] = {0, 0, 0, 0};
          k = 0;

          if (check_pixel_assigned(
                  srcbuf, srcmask, filter_make_index(x - 1, y, width, height), depth, is_float) ||
              check_pixel_assigned(
                  srcbuf, srcmask, filter_make_index(x + 1, y, width, height), depth, is_float) ||
              check_pixel_assigned(
                  srcbuf, srcmask, filter_make_index(x, y - 1, width, height), depth, is_float) ||
              check_pixel_assigned(
                  srcbuf, srcmask, filter_make_index(x, y + 1, width, height), depth, is_float)) {
            for (i = -n; i <= n; i++) {
              for (j = -n; j <= n; j++) {
                if (i != 0 || j != 0) {
                  const int tmpindex = filter_make_index(x + i, y + j, width, height);

                  if (check_pixel_assigned(srcbuf, srcmask, tmpindex, depth, is_float)) {
                    if (is_float) {
                      for (c = 0; c < depth; c++) {
                        tmp[c] = ((const float *)srcbuf)[depth * tmpindex + c];
                      }
                    }
                    else {
                      for (c = 0; c < depth; c++) {
                        tmp[c] = (float)((const unsigned char *)srcbuf)[depth * tmpindex + c];
                      }
                    }

                    wsum += weight[k];

                    for (c = 0; c < depth; c++) {
                      acc[c] += weight[k] * tmp[c];
                    }
                  }
                }
                k++;
              }
            }

            if (wsum != 0) {
              for (c = 0; c < depth; c++) {
                acc[c] /= wsum;
              }

              if (is_float) {
                for (c = 0; c < depth; c++) {
                  ((float *)dstbuf)[depth * index + c] = acc[c];
                }
              }
              else {
                for (c = 0; c < depth; c++) {
                  ((unsigned char *)dstbuf)[depth * index + c] =
                      acc[c] > 255 ? 255 : (acc[c] < 0 ? 0 : ((unsigned char)(acc[c] + 0.5f)));
                }
              }

              if (dstmask != NULL) {
                dstmask[index] = FILTER_MASK_MARGIN; /* assigned */
              }
              cannot_early_out = 1;
            }
          }
        }
      }
    }

    /* keep the original buffer up to date. */
    memcpy(srcbuf, dstbuf, bsize);
    if (dstmask != NULL) {
      memcpy(srcmask, dstmask, ((size_t)width) * height);
    }
  }

  /* free memory */
  MEM_freeN(dstbuf);
  if (dstmask != NULL) {
    MEM_freeN(dstmask);
  }
}

/* threadsafe version, only recreates existing maps */
void IMB_remakemipmap(ImBuf *ibuf, int use_filter)
{
  ImBuf *hbuf = ibuf;
  int curmap = 0;

  ibuf->miptot = 1;

  while (curmap < IMB_MIPMAP_LEVELS) {

    if (ibuf->mipmap[curmap]) {

      if (use_filter) {
        ImBuf *nbuf = IMB_allocImBuf(hbuf->x, hbuf->y, hbuf->planes, hbuf->flags);
        imb_filterN(nbuf, hbuf);
        imb_onehalf_no_alloc(ibuf->mipmap[curmap], nbuf);
        IMB_freeImBuf(nbuf);
      }
      else {
        imb_onehalf_no_alloc(ibuf->mipmap[curmap], hbuf);
      }
    }

    ibuf->miptot = curmap + 2;
    hbuf = ibuf->mipmap[curmap];
    if (hbuf) {
      hbuf->miplevel = curmap + 1;
    }

    if (!hbuf || (hbuf->x <= 2 && hbuf->y <= 2)) {
      break;
    }

    curmap++;
  }
}

/* frees too (if there) and recreates new data */
void IMB_makemipmap(ImBuf *ibuf, int use_filter)
{
  ImBuf *hbuf = ibuf;
  int curmap = 0;

  imb_freemipmapImBuf(ibuf);

  /* no mipmap for non RGBA images */
  if (ibuf->rect_float && ibuf->channels < 4) {
    return;
  }

  ibuf->miptot = 1;

  while (curmap < IMB_MIPMAP_LEVELS) {
    if (use_filter) {
      ImBuf *nbuf = IMB_allocImBuf(hbuf->x, hbuf->y, hbuf->planes, hbuf->flags);
      imb_filterN(nbuf, hbuf);
      ibuf->mipmap[curmap] = IMB_onehalf(nbuf);
      IMB_freeImBuf(nbuf);
    }
    else {
      ibuf->mipmap[curmap] = IMB_onehalf(hbuf);
    }

    ibuf->miptot = curmap + 2;
    hbuf = ibuf->mipmap[curmap];
    hbuf->miplevel = curmap + 1;

    if (hbuf->x < 2 && hbuf->y < 2) {
      break;
    }

    curmap++;
  }
}

ImBuf *IMB_getmipmap(ImBuf *ibuf, int level)
{
  CLAMP(level, 0, ibuf->miptot - 1);
  return (level == 0) ? ibuf : ibuf->mipmap[level - 1];
}

void IMB_premultiply_rect(unsigned int *rect, char planes, int w, int h)
{
  char *cp;
  int x, y, val;

  if (planes == 24) { /* put alpha at 255 */
    cp = (char *)(rect);

    for (y = 0; y < h; y++) {
      for (x = 0; x < w; x++, cp += 4) {
        cp[3] = 255;
      }
    }
  }
  else {
    cp = (char *)(rect);

    for (y = 0; y < h; y++) {
      for (x = 0; x < w; x++, cp += 4) {
        val = cp[3];
        cp[0] = (cp[0] * val) >> 8;
        cp[1] = (cp[1] * val) >> 8;
        cp[2] = (cp[2] * val) >> 8;
      }
    }
  }
}

void IMB_premultiply_rect_float(float *rect_float, int channels, int w, int h)
{
  float val, *cp;
  int x, y;

  if (channels == 4) {
    cp = rect_float;
    for (y = 0; y < h; y++) {
      for (x = 0; x < w; x++, cp += 4) {
        val = cp[3];
        cp[0] = cp[0] * val;
        cp[1] = cp[1] * val;
        cp[2] = cp[2] * val;
      }
    }
  }
}

void IMB_premultiply_alpha(ImBuf *ibuf)
{
  if (ibuf == NULL) {
    return;
  }

  if (ibuf->rect) {
    IMB_premultiply_rect(ibuf->rect, ibuf->planes, ibuf->x, ibuf->y);
  }

  if (ibuf->rect_float) {
    IMB_premultiply_rect_float(ibuf->rect_float, ibuf->channels, ibuf->x, ibuf->y);
  }
}

void IMB_unpremultiply_rect(unsigned int *rect, char planes, int w, int h)
{
  char *cp;
  int x, y;
  float val;

  if (planes == 24) { /* put alpha at 255 */
    cp = (char *)(rect);

    for (y = 0; y < h; y++) {
      for (x = 0; x < w; x++, cp += 4) {
        cp[3] = 255;
      }
    }
  }
  else {
    cp = (char *)(rect);

    for (y = 0; y < h; y++) {
      for (x = 0; x < w; x++, cp += 4) {
        val = cp[3] != 0 ? 1.0f / (float)cp[3] : 1.0f;
        cp[0] = unit_float_to_uchar_clamp(cp[0] * val);
        cp[1] = unit_float_to_uchar_clamp(cp[1] * val);
        cp[2] = unit_float_to_uchar_clamp(cp[2] * val);
      }
    }
  }
}

void IMB_unpremultiply_rect_float(float *rect_float, int channels, int w, int h)
{
  float val, *fp;
  int x, y;

  if (channels == 4) {
    fp = rect_float;
    for (y = 0; y < h; y++) {
      for (x = 0; x < w; x++, fp += 4) {
        val = fp[3] != 0.0f ? 1.0f / fp[3] : 1.0f;
        fp[0] = fp[0] * val;
        fp[1] = fp[1] * val;
        fp[2] = fp[2] * val;
      }
    }
  }
}

void IMB_unpremultiply_alpha(ImBuf *ibuf)
{
  if (ibuf == NULL) {
    return;
  }

  if (ibuf->rect) {
    IMB_unpremultiply_rect(ibuf->rect, ibuf->planes, ibuf->x, ibuf->y);
  }

  if (ibuf->rect_float) {
    IMB_unpremultiply_rect_float(ibuf->rect_float, ibuf->channels, ibuf->x, ibuf->y);
  }
}
