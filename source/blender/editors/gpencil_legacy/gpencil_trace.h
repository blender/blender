/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 */

#pragma once

/* internal exports only */
struct ImBuf;
struct Main;
struct Object;
struct bGPDframe;

#include "potracelib.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * Print trace bitmap for debugging.
 * \param f: Output handle. Use `stderr` for printing
 * \param bm: Trace bitmap
 */
void ED_gpencil_trace_bitmap_print(FILE *f, const potrace_bitmap_t *bm);

/**
 * Return new un-initialized trace bitmap
 * \param w: Width in pixels
 * \param h: Height in pixels
 * \return Trace bitmap
 */
potrace_bitmap_t *ED_gpencil_trace_bitmap_new(int32_t w, int32_t h);
/**
 * Free a trace bitmap
 * \param bm: Trace bitmap
 */
void ED_gpencil_trace_bitmap_free(const potrace_bitmap_t *bm);
/**
 * Invert the given bitmap (Black to White)
 * \param bm: Trace bitmap
 */
void ED_gpencil_trace_bitmap_invert(const potrace_bitmap_t *bm);

/**
 * Convert image to BW bitmap for tracing
 * \param ibuf: ImBuf of the image
 * \param bm: Trace bitmap
 */
void ED_gpencil_trace_image_to_bitmap(struct ImBuf *ibuf,
                                      const potrace_bitmap_t *bm,
                                      float threshold);

/**
 * Convert Potrace Bitmap to Grease Pencil strokes
 * \param st: Data with traced data
 * \param ob: Target grease pencil object
 * \param offset: Offset to center
 * \param scale: Scale of the output
 * \param sample: Sample distance to distribute points
 */
void ED_gpencil_trace_data_to_strokes(struct Main *bmain,
                                      potrace_state_t *st,
                                      struct Object *ob,
                                      struct bGPDframe *gpf,
                                      int32_t offset[2],
                                      float scale,
                                      float sample,
                                      int32_t resolution,
                                      int32_t thickness);

#ifdef __cplusplus
}
#endif
