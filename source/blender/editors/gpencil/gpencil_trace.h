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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edgpencil
 */

#ifndef __GPENCIL_TRACE_H__
#define __GPENCIL_TRACE_H__

/* internal exports only */
struct FILE;
struct ImBuf;
struct Main;
struct Object;
struct bGPDframe;

#include "potracelib.h"

/* Potrace macros for writing individual bitmap pixels. */
#define BM_WORDSIZE ((int)sizeof(potrace_word))
#define BM_WORDBITS (8 * BM_WORDSIZE)
#define BM_HIBIT (((potrace_word)1) << (BM_WORDBITS - 1))
#define BM_ALLBITS (~(potrace_word)0)

#define bm_scanline(bm, y) ((bm)->map + (y) * (bm)->dy)
#define bm_index(bm, x, y) (&bm_scanline(bm, y)[(x) / BM_WORDBITS])
#define bm_mask(x) (BM_HIBIT >> ((x) & (BM_WORDBITS - 1)))
#define bm_range(x, a) ((int)(x) >= 0 && (int)(x) < (a))
#define bm_safe(bm, x, y) (bm_range(x, (bm)->w) && bm_range(y, (bm)->h))

#define BM_UGET(bm, x, y) ((*bm_index(bm, x, y) & bm_mask(x)) != 0)
#define BM_USET(bm, x, y) (*bm_index(bm, x, y) |= bm_mask(x))
#define BM_UCLR(bm, x, y) (*bm_index(bm, x, y) &= ~bm_mask(x))
#define BM_UINV(bm, x, y) (*bm_index(bm, x, y) ^= bm_mask(x))
#define BM_UPUT(bm, x, y, b) ((b) ? BM_USET(bm, x, y) : BM_UCLR(bm, x, y))
#define BM_GET(bm, x, y) (bm_safe(bm, x, y) ? BM_UGET(bm, x, y) : 0)
#define BM_SET(bm, x, y) (bm_safe(bm, x, y) ? BM_USET(bm, x, y) : 0)
#define BM_CLR(bm, x, y) (bm_safe(bm, x, y) ? BM_UCLR(bm, x, y) : 0)
#define BM_INV(bm, x, y) (bm_safe(bm, x, y) ? BM_UINV(bm, x, y) : 0)
#define BM_PUT(bm, x, y, b) (bm_safe(bm, x, y) ? BM_UPUT(bm, x, y, b) : 0)

/* Trace modes */
#define GPENCIL_TRACE_MODE_SINGLE 0
#define GPENCIL_TRACE_MODE_SEQUENCE 1

void ED_gpencil_trace_bitmap_print(FILE *f, const potrace_bitmap_t *bm);

potrace_bitmap_t *ED_gpencil_trace_bitmap_new(int32_t w, int32_t h);
void ED_gpencil_trace_bitmap_free(const potrace_bitmap_t *bm);
void ED_gpencil_trace_bitmap_invert(const potrace_bitmap_t *bm);

void ED_gpencil_trace_image_to_bitmap(struct ImBuf *ibuf,
                                      const potrace_bitmap_t *bm,
                                      const float threshold);

void ED_gpencil_trace_data_to_strokes(struct Main *bmain,
                                      potrace_state_t *st,
                                      struct Object *ob,
                                      struct bGPDframe *gpf,
                                      int32_t offset[2],
                                      const float scale,
                                      const float sample,
                                      const int32_t resolution,
                                      const int32_t thickness);

#endif /* __GPENCIL_TRACE_H__ */
