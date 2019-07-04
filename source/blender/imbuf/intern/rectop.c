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
 * allocimbuf.c
 */

/** \file
 * \ingroup imbuf
 */

#include <stdlib.h>

#include "BLI_utildefines.h"
#include "BLI_rect.h"
#include "BLI_math_base.h"
#include "BLI_math_color.h"
#include "BLI_math_color_blend.h"
#include "BLI_math_vector.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_colormanagement.h"

#include "MEM_guardedalloc.h"

void IMB_blend_color_byte(unsigned char dst[4],
                          unsigned char src1[4],
                          unsigned char src2[4],
                          IMB_BlendMode mode)
{
  switch (mode) {
    case IMB_BLEND_MIX:
      blend_color_mix_byte(dst, src1, src2);
      break;
    case IMB_BLEND_ADD:
      blend_color_add_byte(dst, src1, src2);
      break;
    case IMB_BLEND_SUB:
      blend_color_sub_byte(dst, src1, src2);
      break;
    case IMB_BLEND_MUL:
      blend_color_mul_byte(dst, src1, src2);
      break;
    case IMB_BLEND_LIGHTEN:
      blend_color_lighten_byte(dst, src1, src2);
      break;
    case IMB_BLEND_DARKEN:
      blend_color_darken_byte(dst, src1, src2);
      break;
    case IMB_BLEND_ERASE_ALPHA:
      blend_color_erase_alpha_byte(dst, src1, src2);
      break;
    case IMB_BLEND_ADD_ALPHA:
      blend_color_add_alpha_byte(dst, src1, src2);
      break;
    case IMB_BLEND_OVERLAY:
      blend_color_overlay_byte(dst, src1, src2);
      break;
    case IMB_BLEND_HARDLIGHT:
      blend_color_hardlight_byte(dst, src1, src2);
      break;
    case IMB_BLEND_COLORBURN:
      blend_color_burn_byte(dst, src1, src2);
      break;
    case IMB_BLEND_LINEARBURN:
      blend_color_linearburn_byte(dst, src1, src2);
      break;
    case IMB_BLEND_COLORDODGE:
      blend_color_dodge_byte(dst, src1, src2);
      break;
    case IMB_BLEND_SCREEN:
      blend_color_screen_byte(dst, src1, src2);
      break;
    case IMB_BLEND_SOFTLIGHT:
      blend_color_softlight_byte(dst, src1, src2);
      break;
    case IMB_BLEND_PINLIGHT:
      blend_color_pinlight_byte(dst, src1, src2);
      break;
    case IMB_BLEND_LINEARLIGHT:
      blend_color_linearlight_byte(dst, src1, src2);
      break;
    case IMB_BLEND_VIVIDLIGHT:
      blend_color_vividlight_byte(dst, src1, src2);
      break;
    case IMB_BLEND_DIFFERENCE:
      blend_color_difference_byte(dst, src1, src2);
      break;
    case IMB_BLEND_EXCLUSION:
      blend_color_exclusion_byte(dst, src1, src2);
      break;
    case IMB_BLEND_COLOR:
      blend_color_color_byte(dst, src1, src2);
      break;
    case IMB_BLEND_HUE:
      blend_color_hue_byte(dst, src1, src2);
      break;
    case IMB_BLEND_SATURATION:
      blend_color_saturation_byte(dst, src1, src2);
      break;
    case IMB_BLEND_LUMINOSITY:
      blend_color_luminosity_byte(dst, src1, src2);
      break;

    default:
      dst[0] = src1[0];
      dst[1] = src1[1];
      dst[2] = src1[2];
      dst[3] = src1[3];
      break;
  }
}

void IMB_blend_color_float(float dst[4], float src1[4], float src2[4], IMB_BlendMode mode)
{
  switch (mode) {
    case IMB_BLEND_MIX:
      blend_color_mix_float(dst, src1, src2);
      break;
    case IMB_BLEND_ADD:
      blend_color_add_float(dst, src1, src2);
      break;
    case IMB_BLEND_SUB:
      blend_color_sub_float(dst, src1, src2);
      break;
    case IMB_BLEND_MUL:
      blend_color_mul_float(dst, src1, src2);
      break;
    case IMB_BLEND_LIGHTEN:
      blend_color_lighten_float(dst, src1, src2);
      break;
    case IMB_BLEND_DARKEN:
      blend_color_darken_float(dst, src1, src2);
      break;
    case IMB_BLEND_ERASE_ALPHA:
      blend_color_erase_alpha_float(dst, src1, src2);
      break;
    case IMB_BLEND_ADD_ALPHA:
      blend_color_add_alpha_float(dst, src1, src2);
      break;
    case IMB_BLEND_OVERLAY:
      blend_color_overlay_float(dst, src1, src2);
      break;
    case IMB_BLEND_HARDLIGHT:
      blend_color_hardlight_float(dst, src1, src2);
      break;
    case IMB_BLEND_COLORBURN:
      blend_color_burn_float(dst, src1, src2);
      break;
    case IMB_BLEND_LINEARBURN:
      blend_color_linearburn_float(dst, src1, src2);
      break;
    case IMB_BLEND_COLORDODGE:
      blend_color_dodge_float(dst, src1, src2);
      break;
    case IMB_BLEND_SCREEN:
      blend_color_screen_float(dst, src1, src2);
      break;
    case IMB_BLEND_SOFTLIGHT:
      blend_color_softlight_float(dst, src1, src2);
      break;
    case IMB_BLEND_PINLIGHT:
      blend_color_pinlight_float(dst, src1, src2);
      break;
    case IMB_BLEND_LINEARLIGHT:
      blend_color_linearlight_float(dst, src1, src2);
      break;
    case IMB_BLEND_VIVIDLIGHT:
      blend_color_vividlight_float(dst, src1, src2);
      break;
    case IMB_BLEND_DIFFERENCE:
      blend_color_difference_float(dst, src1, src2);
      break;
    case IMB_BLEND_EXCLUSION:
      blend_color_exclusion_float(dst, src1, src2);
      break;
    case IMB_BLEND_COLOR:
      blend_color_color_float(dst, src1, src2);
      break;
    case IMB_BLEND_HUE:
      blend_color_hue_float(dst, src1, src2);
      break;
    case IMB_BLEND_SATURATION:
      blend_color_saturation_float(dst, src1, src2);
      break;
    case IMB_BLEND_LUMINOSITY:
      blend_color_luminosity_float(dst, src1, src2);
      break;
    default:
      dst[0] = src1[0];
      dst[1] = src1[1];
      dst[2] = src1[2];
      dst[3] = src1[3];
      break;
  }
}

/** Crop */

static void rect_crop_4bytes(void **buf_p, const int size_src[2], const rcti *crop)
{
  if (*buf_p == NULL) {
    return;
  }
  const int size_dst[2] = {
      BLI_rcti_size_x(crop) + 1,
      BLI_rcti_size_y(crop) + 1,
  };
  uint *src = *buf_p;
  uint *dst = src + crop->ymin * size_src[0] + crop->xmin;
  for (int y = 0; y < size_dst[1]; y++, src += size_dst[0], dst += size_src[0]) {
    memmove(src, dst, sizeof(uint) * size_dst[0]);
  }
  *buf_p = MEM_reallocN(*buf_p, sizeof(uint) * size_dst[0] * size_dst[1]);
}

static void rect_crop_16bytes(void **buf_p, const int size_src[2], const rcti *crop)
{
  if (*buf_p == NULL) {
    return;
  }
  const int size_dst[2] = {
      BLI_rcti_size_x(crop) + 1,
      BLI_rcti_size_y(crop) + 1,
  };
  uint(*src)[4] = *buf_p;
  uint(*dst)[4] = src + crop->ymin * size_src[0] + crop->xmin;
  for (int y = 0; y < size_dst[1]; y++, src += size_dst[0], dst += size_src[0]) {
    memmove(src, dst, sizeof(uint[4]) * size_dst[0]);
  }
  *buf_p = (void *)MEM_reallocN(*buf_p, sizeof(uint[4]) * size_dst[0] * size_dst[1]);
}

/**
 * In-place image crop.
 */
void IMB_rect_crop(ImBuf *ibuf, const rcti *crop)
{
  const int size_src[2] = {
      ibuf->x,
      ibuf->y,
  };
  const int size_dst[2] = {
      BLI_rcti_size_x(crop) + 1,
      BLI_rcti_size_y(crop) + 1,
  };
  BLI_assert(size_dst[0] > 0 && size_dst[0] > 0);
  BLI_assert(crop->xmin >= 0 && crop->ymin >= 0);
  BLI_assert(crop->xmax < ibuf->x && crop->ymax < ibuf->y);

  if ((size_dst[0] == ibuf->x) && (size_dst[1] == ibuf->y)) {
    return;
  }

  rect_crop_4bytes((void **)&ibuf->rect, size_src, crop);
  rect_crop_4bytes((void **)&ibuf->zbuf, size_src, crop);
  rect_crop_4bytes((void **)&ibuf->zbuf_float, size_src, crop);
  rect_crop_16bytes((void **)&ibuf->rect_float, size_src, crop);

  ibuf->x = size_dst[0];
  ibuf->y = size_dst[1];
}

/* clipping */

void IMB_rectclip(ImBuf *dbuf,
                  ImBuf *sbuf,
                  int *destx,
                  int *desty,
                  int *srcx,
                  int *srcy,
                  int *width,
                  int *height)
{
  int tmp;

  if (dbuf == NULL) {
    return;
  }

  if (*destx < 0) {
    *srcx -= *destx;
    *width += *destx;
    *destx = 0;
  }
  if (*srcx < 0) {
    *destx -= *srcx;
    *width += *srcx;
    *srcx = 0;
  }
  if (*desty < 0) {
    *srcy -= *desty;
    *height += *desty;
    *desty = 0;
  }
  if (*srcy < 0) {
    *desty -= *srcy;
    *height += *srcy;
    *srcy = 0;
  }

  tmp = dbuf->x - *destx;
  if (*width > tmp) {
    *width = tmp;
  }
  tmp = dbuf->y - *desty;
  if (*height > tmp) {
    *height = tmp;
  }

  if (sbuf) {
    tmp = sbuf->x - *srcx;
    if (*width > tmp) {
      *width = tmp;
    }
    tmp = sbuf->y - *srcy;
    if (*height > tmp) {
      *height = tmp;
    }
  }

  if ((*height <= 0) || (*width <= 0)) {
    *width = 0;
    *height = 0;
  }
}

static void imb_rectclip3(ImBuf *dbuf,
                          ImBuf *obuf,
                          ImBuf *sbuf,
                          int *destx,
                          int *desty,
                          int *origx,
                          int *origy,
                          int *srcx,
                          int *srcy,
                          int *width,
                          int *height)
{
  int tmp;

  if (dbuf == NULL) {
    return;
  }

  if (*destx < 0) {
    *srcx -= *destx;
    *origx -= *destx;
    *width += *destx;
    *destx = 0;
  }
  if (*origx < 0) {
    *destx -= *origx;
    *srcx -= *origx;
    *width += *origx;
    *origx = 0;
  }
  if (*srcx < 0) {
    *destx -= *srcx;
    *origx -= *srcx;
    *width += *srcx;
    *srcx = 0;
  }

  if (*desty < 0) {
    *srcy -= *desty;
    *origy -= *desty;
    *height += *desty;
    *desty = 0;
  }
  if (*origy < 0) {
    *desty -= *origy;
    *srcy -= *origy;
    *height += *origy;
    *origy = 0;
  }
  if (*srcy < 0) {
    *desty -= *srcy;
    *origy -= *srcy;
    *height += *srcy;
    *srcy = 0;
  }

  tmp = dbuf->x - *destx;
  if (*width > tmp) {
    *width = tmp;
  }
  tmp = dbuf->y - *desty;
  if (*height > tmp) {
    *height = tmp;
  }

  if (obuf) {
    tmp = obuf->x - *origx;
    if (*width > tmp) {
      *width = tmp;
    }
    tmp = obuf->y - *origy;
    if (*height > tmp) {
      *height = tmp;
    }
  }

  if (sbuf) {
    tmp = sbuf->x - *srcx;
    if (*width > tmp) {
      *width = tmp;
    }
    tmp = sbuf->y - *srcy;
    if (*height > tmp) {
      *height = tmp;
    }
  }

  if ((*height <= 0) || (*width <= 0)) {
    *width = 0;
    *height = 0;
  }
}

/* copy and blend */

void IMB_rectcpy(
    ImBuf *dbuf, ImBuf *sbuf, int destx, int desty, int srcx, int srcy, int width, int height)
{
  IMB_rectblend(dbuf,
                dbuf,
                sbuf,
                NULL,
                NULL,
                NULL,
                0,
                destx,
                desty,
                destx,
                desty,
                srcx,
                srcy,
                width,
                height,
                IMB_BLEND_COPY,
                false);
}

typedef void (*IMB_blend_func)(unsigned char *dst,
                               const unsigned char *src1,
                               const unsigned char *src2);
typedef void (*IMB_blend_func_float)(float *dst, const float *src1, const float *src2);

void IMB_rectblend(ImBuf *dbuf,
                   ImBuf *obuf,
                   ImBuf *sbuf,
                   unsigned short *dmask,
                   unsigned short *curvemask,
                   unsigned short *texmask,
                   float mask_max,
                   int destx,
                   int desty,
                   int origx,
                   int origy,
                   int srcx,
                   int srcy,
                   int width,
                   int height,
                   IMB_BlendMode mode,
                   bool accumulate)
{
  unsigned int *drect = NULL, *orect = NULL, *srect = NULL, *dr, * or, *sr;
  float *drectf = NULL, *orectf = NULL, *srectf = NULL, *drf, *orf, *srf;
  unsigned short *cmaskrect = curvemask, *cmr;
  unsigned short *dmaskrect = dmask, *dmr;
  unsigned short *texmaskrect = texmask, *tmr;
  int do_float, do_char, srcskip, destskip, origskip, x;
  IMB_blend_func func = NULL;
  IMB_blend_func_float func_float = NULL;

  if (dbuf == NULL || obuf == NULL) {
    return;
  }

  imb_rectclip3(dbuf, obuf, sbuf, &destx, &desty, &origx, &origy, &srcx, &srcy, &width, &height);

  if (width == 0 || height == 0) {
    return;
  }
  if (sbuf && sbuf->channels != 4) {
    return;
  }
  if (dbuf->channels != 4) {
    return;
  }

  do_char = (sbuf && sbuf->rect && dbuf->rect && obuf->rect);
  do_float = (sbuf && sbuf->rect_float && dbuf->rect_float && obuf->rect_float);

  if (do_char) {
    drect = dbuf->rect + ((size_t)desty) * dbuf->x + destx;
    orect = obuf->rect + ((size_t)origy) * obuf->x + origx;
  }
  if (do_float) {
    drectf = dbuf->rect_float + (((size_t)desty) * dbuf->x + destx) * 4;
    orectf = obuf->rect_float + (((size_t)origy) * obuf->x + origx) * 4;
  }

  if (dmaskrect) {
    dmaskrect += ((size_t)origy) * obuf->x + origx;
  }

  destskip = dbuf->x;
  origskip = obuf->x;

  if (sbuf) {
    if (do_char) {
      srect = sbuf->rect + ((size_t)srcy) * sbuf->x + srcx;
    }
    if (do_float) {
      srectf = sbuf->rect_float + (((size_t)srcy) * sbuf->x + srcx) * 4;
    }
    srcskip = sbuf->x;

    if (cmaskrect) {
      cmaskrect += ((size_t)srcy) * sbuf->x + srcx;
    }

    if (texmaskrect) {
      texmaskrect += ((size_t)srcy) * sbuf->x + srcx;
    }
  }
  else {
    srect = drect;
    srectf = drectf;
    srcskip = destskip;
  }

  if (mode == IMB_BLEND_COPY) {
    /* copy */
    for (; height > 0; height--) {
      if (do_char) {
        memcpy(drect, srect, width * sizeof(int));
        drect += destskip;
        srect += srcskip;
      }

      if (do_float) {
        memcpy(drectf, srectf, width * sizeof(float) * 4);
        drectf += destskip * 4;
        srectf += srcskip * 4;
      }
    }
  }
  else if (mode == IMB_BLEND_COPY_RGB) {
    /* copy rgb only */
    for (; height > 0; height--) {
      if (do_char) {
        dr = drect;
        sr = srect;
        for (x = width; x > 0; x--, dr++, sr++) {
          ((char *)dr)[0] = ((char *)sr)[0];
          ((char *)dr)[1] = ((char *)sr)[1];
          ((char *)dr)[2] = ((char *)sr)[2];
        }
        drect += destskip;
        srect += srcskip;
      }

      if (do_float) {
        drf = drectf;
        srf = srectf;
        for (x = width; x > 0; x--, drf += 4, srf += 4) {
          float map_alpha = (srf[3] == 0.0f) ? drf[3] : drf[3] / srf[3];

          drf[0] = srf[0] * map_alpha;
          drf[1] = srf[1] * map_alpha;
          drf[2] = srf[2] * map_alpha;
        }
        drectf += destskip * 4;
        srectf += srcskip * 4;
      }
    }
  }
  else if (mode == IMB_BLEND_COPY_ALPHA) {
    /* copy alpha only */
    for (; height > 0; height--) {
      if (do_char) {
        dr = drect;
        sr = srect;
        for (x = width; x > 0; x--, dr++, sr++) {
          ((char *)dr)[3] = ((char *)sr)[3];
        }
        drect += destskip;
        srect += srcskip;
      }

      if (do_float) {
        drf = drectf;
        srf = srectf;
        for (x = width; x > 0; x--, drf += 4, srf += 4) {
          drf[3] = srf[3];
        }
        drectf += destskip * 4;
        srectf += srcskip * 4;
      }
    }
  }
  else {
    switch (mode) {
      case IMB_BLEND_MIX:
      case IMB_BLEND_INTERPOLATE:
        func = blend_color_mix_byte;
        func_float = blend_color_mix_float;
        break;
      case IMB_BLEND_ADD:
        func = blend_color_add_byte;
        func_float = blend_color_add_float;
        break;
      case IMB_BLEND_SUB:
        func = blend_color_sub_byte;
        func_float = blend_color_sub_float;
        break;
      case IMB_BLEND_MUL:
        func = blend_color_mul_byte;
        func_float = blend_color_mul_float;
        break;
      case IMB_BLEND_LIGHTEN:
        func = blend_color_lighten_byte;
        func_float = blend_color_lighten_float;
        break;
      case IMB_BLEND_DARKEN:
        func = blend_color_darken_byte;
        func_float = blend_color_darken_float;
        break;
      case IMB_BLEND_ERASE_ALPHA:
        func = blend_color_erase_alpha_byte;
        func_float = blend_color_erase_alpha_float;
        break;
      case IMB_BLEND_ADD_ALPHA:
        func = blend_color_add_alpha_byte;
        func_float = blend_color_add_alpha_float;
        break;
      case IMB_BLEND_OVERLAY:
        func = blend_color_overlay_byte;
        func_float = blend_color_overlay_float;
        break;
      case IMB_BLEND_HARDLIGHT:
        func = blend_color_hardlight_byte;
        func_float = blend_color_hardlight_float;
        break;
      case IMB_BLEND_COLORBURN:
        func = blend_color_burn_byte;
        func_float = blend_color_burn_float;
        break;
      case IMB_BLEND_LINEARBURN:
        func = blend_color_linearburn_byte;
        func_float = blend_color_linearburn_float;
        break;
      case IMB_BLEND_COLORDODGE:
        func = blend_color_dodge_byte;
        func_float = blend_color_dodge_float;
        break;
      case IMB_BLEND_SCREEN:
        func = blend_color_screen_byte;
        func_float = blend_color_screen_float;
        break;
      case IMB_BLEND_SOFTLIGHT:
        func = blend_color_softlight_byte;
        func_float = blend_color_softlight_float;
        break;
      case IMB_BLEND_PINLIGHT:
        func = blend_color_pinlight_byte;
        func_float = blend_color_pinlight_float;
        break;
      case IMB_BLEND_LINEARLIGHT:
        func = blend_color_linearlight_byte;
        func_float = blend_color_linearlight_float;
        break;
      case IMB_BLEND_VIVIDLIGHT:
        func = blend_color_vividlight_byte;
        func_float = blend_color_vividlight_float;
        break;
      case IMB_BLEND_DIFFERENCE:
        func = blend_color_difference_byte;
        func_float = blend_color_difference_float;
        break;
      case IMB_BLEND_EXCLUSION:
        func = blend_color_exclusion_byte;
        func_float = blend_color_exclusion_float;
        break;
      case IMB_BLEND_COLOR:
        func = blend_color_color_byte;
        func_float = blend_color_color_float;
        break;
      case IMB_BLEND_HUE:
        func = blend_color_hue_byte;
        func_float = blend_color_hue_float;
        break;
      case IMB_BLEND_SATURATION:
        func = blend_color_saturation_byte;
        func_float = blend_color_saturation_float;
        break;
      case IMB_BLEND_LUMINOSITY:
        func = blend_color_luminosity_byte;
        func_float = blend_color_luminosity_float;
        break;
      default:
        break;
    }

    /* blend */
    for (; height > 0; height--) {
      if (do_char) {
        dr = drect;
        or = orect;
        sr = srect;

        if (cmaskrect) {
          /* mask accumulation for painting */
          cmr = cmaskrect;
          tmr = texmaskrect;

          /* destination mask present, do max alpha masking */
          if (dmaskrect) {
            dmr = dmaskrect;
            for (x = width; x > 0; x--, dr++, or ++, sr++, dmr++, cmr++) {
              unsigned char *src = (unsigned char *)sr;
              float mask_lim = mask_max * (*cmr);

              if (texmaskrect) {
                mask_lim *= ((*tmr++) / 65535.0f);
              }

              if (src[3] && mask_lim) {
                float mask;

                if (accumulate) {
                  mask = *dmr + mask_lim;
                }
                else {
                  mask = *dmr + mask_lim - (*dmr * (*cmr / 65535.0f));
                }

                mask = min_ff(mask, 65535.0);

                if (mask > *dmr) {
                  unsigned char mask_src[4];

                  *dmr = mask;

                  mask_src[0] = src[0];
                  mask_src[1] = src[1];
                  mask_src[2] = src[2];

                  if (mode == IMB_BLEND_INTERPOLATE) {
                    mask_src[3] = src[3];
                    blend_color_interpolate_byte(
                        (unsigned char *)dr, (unsigned char *) or, mask_src, mask / 65535.0f);
                  }
                  else {
                    mask_src[3] = divide_round_i(src[3] * mask, 65535);
                    func((unsigned char *)dr, (unsigned char *) or, mask_src);
                  }
                }
              }
            }
            dmaskrect += origskip;
          }
          /* no destination mask buffer, do regular blend with masktexture if present */
          else {
            for (x = width; x > 0; x--, dr++, or ++, sr++, cmr++) {
              unsigned char *src = (unsigned char *)sr;
              float mask = (float)mask_max * ((float)(*cmr));

              if (texmaskrect) {
                mask *= ((float)(*tmr++) / 65535.0f);
              }

              mask = min_ff(mask, 65535.0);

              if (src[3] && (mask > 0.0f)) {
                unsigned char mask_src[4];

                mask_src[0] = src[0];
                mask_src[1] = src[1];
                mask_src[2] = src[2];

                if (mode == IMB_BLEND_INTERPOLATE) {
                  mask_src[3] = src[3];
                  blend_color_interpolate_byte(
                      (unsigned char *)dr, (unsigned char *) or, mask_src, mask / 65535.0f);
                }
                else {
                  mask_src[3] = divide_round_i(src[3] * mask, 65535);
                  func((unsigned char *)dr, (unsigned char *) or, mask_src);
                }
              }
            }
          }

          cmaskrect += srcskip;
          if (texmaskrect) {
            texmaskrect += srcskip;
          }
        }
        else {
          /* regular blending */
          for (x = width; x > 0; x--, dr++, or ++, sr++) {
            if (((unsigned char *)sr)[3]) {
              func((unsigned char *)dr, (unsigned char *) or, (unsigned char *)sr);
            }
          }
        }

        drect += destskip;
        orect += origskip;
        srect += srcskip;
      }

      if (do_float) {
        drf = drectf;
        orf = orectf;
        srf = srectf;

        if (cmaskrect) {
          /* mask accumulation for painting */
          cmr = cmaskrect;
          tmr = texmaskrect;

          /* destination mask present, do max alpha masking */
          if (dmaskrect) {
            dmr = dmaskrect;
            for (x = width; x > 0; x--, drf += 4, orf += 4, srf += 4, dmr++, cmr++) {
              float mask_lim = mask_max * (*cmr);

              if (texmaskrect) {
                mask_lim *= ((*tmr++) / 65535.0f);
              }

              if (srf[3] && mask_lim) {
                float mask;

                if (accumulate) {
                  mask = min_ff(*dmr + mask_lim, 65535.0);
                }
                else {
                  mask = *dmr + mask_lim - (*dmr * (*cmr / 65535.0f));
                }

                mask = min_ff(mask, 65535.0);

                if (mask > *dmr) {
                  *dmr = mask;

                  if (mode == IMB_BLEND_INTERPOLATE) {
                    blend_color_interpolate_float(drf, orf, srf, mask / 65535.0f);
                  }
                  else {
                    float mask_srf[4];
                    mul_v4_v4fl(mask_srf, srf, mask / 65535.0f);
                    func_float(drf, orf, mask_srf);
                  }
                }
              }
            }
            dmaskrect += origskip;
          }
          /* no destination mask buffer, do regular blend with masktexture if present */
          else {
            for (x = width; x > 0; x--, drf += 4, orf += 4, srf += 4, cmr++) {
              float mask = (float)mask_max * ((float)(*cmr));

              if (texmaskrect) {
                mask *= ((float)(*tmr++) / 65535.0f);
              }

              mask = min_ff(mask, 65535.0);

              if (srf[3] && (mask > 0.0f)) {
                if (mode == IMB_BLEND_INTERPOLATE) {
                  blend_color_interpolate_float(drf, orf, srf, mask / 65535.0f);
                }
                else {
                  float mask_srf[4];
                  mul_v4_v4fl(mask_srf, srf, mask / 65535.0f);
                  func_float(drf, orf, mask_srf);
                }
              }
            }
          }

          cmaskrect += srcskip;
          if (texmaskrect) {
            texmaskrect += srcskip;
          }
        }
        else {
          /* regular blending */
          for (x = width; x > 0; x--, drf += 4, orf += 4, srf += 4) {
            if (srf[3] != 0) {
              func_float(drf, orf, srf);
            }
          }
        }

        drectf += destskip * 4;
        orectf += origskip * 4;
        srectf += srcskip * 4;
      }
    }
  }
}

typedef struct RectBlendThreadData {
  ImBuf *dbuf, *obuf, *sbuf;
  unsigned short *dmask, *curvemask, *texmask;
  float mask_max;
  int destx, desty, origx, origy;
  int srcx, srcy, width;
  IMB_BlendMode mode;
  bool accumulate;
} RectBlendThreadData;

static void rectblend_thread_do(void *data_v, int start_scanline, int num_scanlines)
{
  RectBlendThreadData *data = (RectBlendThreadData *)data_v;
  IMB_rectblend(data->dbuf,
                data->obuf,
                data->sbuf,
                data->dmask,
                data->curvemask,
                data->texmask,
                data->mask_max,
                data->destx,
                data->desty + start_scanline,
                data->origx,
                data->origy + start_scanline,
                data->srcx,
                data->srcy + start_scanline,
                data->width,
                num_scanlines,
                data->mode,
                data->accumulate);
}

void IMB_rectblend_threaded(ImBuf *dbuf,
                            ImBuf *obuf,
                            ImBuf *sbuf,
                            unsigned short *dmask,
                            unsigned short *curvemask,
                            unsigned short *texmask,
                            float mask_max,
                            int destx,
                            int desty,
                            int origx,
                            int origy,
                            int srcx,
                            int srcy,
                            int width,
                            int height,
                            IMB_BlendMode mode,
                            bool accumulate)
{
  if (((size_t)width) * height < 64 * 64) {
    IMB_rectblend(dbuf,
                  obuf,
                  sbuf,
                  dmask,
                  curvemask,
                  texmask,
                  mask_max,
                  destx,
                  desty,
                  origx,
                  origy,
                  srcx,
                  srcy,
                  width,
                  height,
                  mode,
                  accumulate);
  }
  else {
    RectBlendThreadData data;
    data.dbuf = dbuf;
    data.obuf = obuf;
    data.sbuf = sbuf;
    data.dmask = dmask;
    data.curvemask = curvemask;
    data.texmask = texmask;
    data.mask_max = mask_max;
    data.destx = destx;
    data.desty = desty;
    data.origx = origx;
    data.origy = origy;
    data.srcx = srcx;
    data.srcy = srcy;
    data.width = width;
    data.mode = mode;
    data.accumulate = accumulate;
    IMB_processor_apply_threaded_scanlines(height, rectblend_thread_do, &data);
  }
}

/* fill */

void IMB_rectfill(ImBuf *drect, const float col[4])
{
  int num;

  if (drect->rect) {
    unsigned int *rrect = drect->rect;
    char ccol[4];

    ccol[0] = (int)(col[0] * 255);
    ccol[1] = (int)(col[1] * 255);
    ccol[2] = (int)(col[2] * 255);
    ccol[3] = (int)(col[3] * 255);

    num = drect->x * drect->y;
    for (; num > 0; num--) {
      *rrect++ = *((unsigned int *)ccol);
    }
  }

  if (drect->rect_float) {
    float *rrectf = drect->rect_float;

    num = drect->x * drect->y;
    for (; num > 0; num--) {
      *rrectf++ = col[0];
      *rrectf++ = col[1];
      *rrectf++ = col[2];
      *rrectf++ = col[3];
    }
  }
}

void buf_rectfill_area(unsigned char *rect,
                       float *rectf,
                       int width,
                       int height,
                       const float col[4],
                       struct ColorManagedDisplay *display,
                       int x1,
                       int y1,
                       int x2,
                       int y2)
{
  int i, j;
  float a;    /* alpha */
  float ai;   /* alpha inverted */
  float aich; /* alpha, inverted, ai/255.0 - Convert char to float at the same time */
  if ((!rect && !rectf) || (!col) || col[3] == 0.0f) {
    return;
  }

  /* sanity checks for coords */
  CLAMP(x1, 0, width);
  CLAMP(x2, 0, width);
  CLAMP(y1, 0, height);
  CLAMP(y2, 0, height);

  if (x1 > x2) {
    SWAP(int, x1, x2);
  }
  if (y1 > y2) {
    SWAP(int, y1, y2);
  }
  if (x1 == x2 || y1 == y2) {
    return;
  }

  a = col[3];
  ai = 1 - a;
  aich = ai / 255.0f;

  if (rect) {
    unsigned char *pixel;
    unsigned char chr = 0, chg = 0, chb = 0;
    float fr = 0, fg = 0, fb = 0;

    const int alphaint = unit_float_to_uchar_clamp(a);

    if (a == 1.0f) {
      chr = unit_float_to_uchar_clamp(col[0]);
      chg = unit_float_to_uchar_clamp(col[1]);
      chb = unit_float_to_uchar_clamp(col[2]);
    }
    else {
      fr = col[0] * a;
      fg = col[1] * a;
      fb = col[2] * a;
    }
    for (j = 0; j < y2 - y1; j++) {
      for (i = 0; i < x2 - x1; i++) {
        pixel = rect + 4 * (((y1 + j) * width) + (x1 + i));
        if (pixel >= rect && pixel < rect + (4 * (width * height))) {
          if (a == 1.0f) {
            pixel[0] = chr;
            pixel[1] = chg;
            pixel[2] = chb;
            pixel[3] = 255;
          }
          else {
            int alphatest;
            pixel[0] = (char)((fr + ((float)pixel[0] * aich)) * 255.0f);
            pixel[1] = (char)((fg + ((float)pixel[1] * aich)) * 255.0f);
            pixel[2] = (char)((fb + ((float)pixel[2] * aich)) * 255.0f);
            pixel[3] = (char)((alphatest = ((int)pixel[3] + alphaint)) < 255 ? alphatest : 255);
          }
        }
      }
    }
  }

  if (rectf) {
    float col_conv[4];
    float *pixel;

    if (display) {
      copy_v4_v4(col_conv, col);
      IMB_colormanagement_display_to_scene_linear_v3(col_conv, display);
    }
    else {
      srgb_to_linearrgb_v4(col_conv, col);
    }

    for (j = 0; j < y2 - y1; j++) {
      for (i = 0; i < x2 - x1; i++) {
        pixel = rectf + 4 * (((y1 + j) * width) + (x1 + i));
        if (a == 1.0f) {
          pixel[0] = col_conv[0];
          pixel[1] = col_conv[1];
          pixel[2] = col_conv[2];
          pixel[3] = 1.0f;
        }
        else {
          float alphatest;
          pixel[0] = (col_conv[0] * a) + (pixel[0] * ai);
          pixel[1] = (col_conv[1] * a) + (pixel[1] * ai);
          pixel[2] = (col_conv[2] * a) + (pixel[2] * ai);
          pixel[3] = (alphatest = (pixel[3] + a)) < 1.0f ? alphatest : 1.0f;
        }
      }
    }
  }
}

void IMB_rectfill_area(ImBuf *ibuf,
                       const float col[4],
                       int x1,
                       int y1,
                       int x2,
                       int y2,
                       struct ColorManagedDisplay *display)
{
  if (!ibuf) {
    return;
  }
  buf_rectfill_area((unsigned char *)ibuf->rect,
                    ibuf->rect_float,
                    ibuf->x,
                    ibuf->y,
                    col,
                    display,
                    x1,
                    y1,
                    x2,
                    y2);
}

void IMB_rectfill_alpha(ImBuf *ibuf, const float value)
{
  int i;

  if (ibuf->rect_float && (ibuf->channels == 4)) {
    float *fbuf = ibuf->rect_float + 3;
    for (i = ibuf->x * ibuf->y; i > 0; i--, fbuf += 4) {
      *fbuf = value;
    }
  }

  if (ibuf->rect) {
    const unsigned char cvalue = value * 255;
    unsigned char *cbuf = ((unsigned char *)ibuf->rect) + 3;
    for (i = ibuf->x * ibuf->y; i > 0; i--, cbuf += 4) {
      *cbuf = cvalue;
    }
  }
}
