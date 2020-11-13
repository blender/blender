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
 * The Original Code is Copyright (C) 2020 Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup edgpencil
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_material.h"

#include "DNA_gpencil_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "gpencil_trace.h"

/**
 * Print trace bitmap for debugging.
 * \param f: Output handle. Use `stderr` for printing
 * \param bm: Trace bitmap
 */
void ED_gpencil_trace_bitmap_print(FILE *f, const potrace_bitmap_t *bm)
{
  int32_t x, y;
  int32_t xx, yy;
  int32_t d;
  int32_t sw, sh;

  sw = bm->w < 79 ? bm->w : 79;
  sh = bm->w < 79 ? bm->h : bm->h * sw * 44 / (79 * bm->w);

  for (yy = sh - 1; yy >= 0; yy--) {
    for (xx = 0; xx < sw; xx++) {
      d = 0;
      for (x = xx * bm->w / sw; x < (xx + 1) * bm->w / sw; x++) {
        for (y = yy * bm->h / sh; y < (yy + 1) * bm->h / sh; y++) {
          if (BM_GET(bm, x, y)) {
            d++;
          }
        }
      }
      fputc(d ? '*' : ' ', f);
    }
    fputc('\n', f);
  }
}

/**
 * Return new un-initialized trace bitmap
 * \param w: Width in pixels
 * \param h: Height in pixels
 * \return Trace bitmap
 */
potrace_bitmap_t *ED_gpencil_trace_bitmap_new(int32_t w, int32_t h)
{
  potrace_bitmap_t *bm;
  int32_t dy = (w + BM_WORDBITS - 1) / BM_WORDBITS;

  bm = (potrace_bitmap_t *)MEM_mallocN(sizeof(potrace_bitmap_t), __func__);
  if (!bm) {
    return NULL;
  }
  bm->w = w;
  bm->h = h;
  bm->dy = dy;
  bm->map = (potrace_word *)calloc(h, dy * BM_WORDSIZE);
  if (!bm->map) {
    free(bm);
    return NULL;
  }

  return bm;
}

/**
 * Free a trace bitmap
 * \param bm: Trace bitmap
 */
void ED_gpencil_trace_bitmap_free(const potrace_bitmap_t *bm)
{
  if (bm != NULL) {
    free(bm->map);
  }
  MEM_SAFE_FREE(bm);
}

/**
 * Invert the given bitmap (Black to White)
 * \param bm: Trace bitmap
 */
void ED_gpencil_trace_bitmap_invert(const potrace_bitmap_t *bm)
{
  int32_t dy = bm->dy;
  int32_t y;
  int32_t i;
  potrace_word *p;

  if (dy < 0) {
    dy = -dy;
  }

  for (y = 0; y < bm->h; y++) {
    p = bm_scanline(bm, y);
    for (i = 0; i < dy; i++) {
      p[i] ^= BM_ALLBITS;
    }
  }
}

/**
 * Return pixel data (rgba) at index
 * \param ibuf: ImBuf of the image
 * \param idx: Index of the pixel
 * \return RGBA value
 */
static void pixel_at_index(const ImBuf *ibuf, const int32_t idx, float r_col[4])
{
  BLI_assert(idx < (ibuf->x * ibuf->y));

  if (ibuf->rect_float) {
    const float *frgba = &ibuf->rect_float[idx * 4];
    copy_v4_v4(r_col, frgba);
  }
  else {
    unsigned char *cp = (unsigned char *)(ibuf->rect + idx);
    r_col[0] = (float)cp[0] / 255.0f;
    r_col[1] = (float)cp[1] / 255.0f;
    r_col[2] = (float)cp[2] / 255.0f;
    r_col[3] = (float)cp[3] / 255.0f;
  }
}

/**
 * Convert image to BW bitmap for tracing
 * \param ibuf: ImBuf of the image
 * \param bm: Trace bitmap
 */
void ED_gpencil_trace_image_to_bitmap(ImBuf *ibuf,
                                      const potrace_bitmap_t *bm,
                                      const float threshold)
{
  float rgba[4];
  int32_t pixel = 0;

  for (uint32_t y = 0; y < ibuf->y; y++) {
    for (uint32_t x = 0; x < ibuf->x; x++) {
      pixel = (ibuf->x * y) + x;
      pixel_at_index(ibuf, pixel, rgba);
      /* Get a BW color. */
      mul_v3_fl(rgba, rgba[3]);
      float color = (rgba[0] + rgba[1] + rgba[2]) / 3.0f;
      int32_t bw = (color > threshold) ? 0 : 1;
      BM_PUT(bm, x, y, bw);
    }
  }
}

/* Helper to add point to stroke. */
static void add_point(bGPDstroke *gps, float scale, const int32_t offset[2], float x, float y)
{
  int32_t idx = gps->totpoints;
  if (gps->totpoints == 0) {
    gps->points = MEM_callocN(sizeof(bGPDspoint), "gp_stroke_points");
  }
  else {
    gps->points = MEM_recallocN(gps->points, sizeof(bGPDspoint) * (gps->totpoints + 1));
  }
  bGPDspoint *pt = &gps->points[idx];
  pt->x = (x - offset[0]) * scale;
  pt->y = 0;
  pt->z = (y - offset[1]) * scale;
  pt->pressure = 1.0f;
  pt->strength = 1.0f;

  gps->totpoints++;
}

/* helper to generate all points of curve. */
static void add_bezier(bGPDstroke *gps,
                       float scale,
                       int32_t offset[2],
                       int32_t resolution,
                       float bcp1[2],
                       float bcp2[2],
                       float bcp3[2],
                       float bcp4[2],
                       const bool skip)
{
  const float step = 1.0f / (float)(resolution - 1);
  float a = 0.0f;

  for (int32_t i = 0; i < resolution; i++) {
    if ((!skip) || (i > 0)) {
      float fpt[3];
      interp_v2_v2v2v2v2_cubic(fpt, bcp1, bcp2, bcp3, bcp4, a);
      add_point(gps, scale, offset, fpt[0], fpt[1]);
    }
    a += step;
  }
}

/**
 * Convert Potrace Bitmap to Grease Pencil strokes
 * \param st: Data with traced data
 * \param ob: Target grease pencil object
 * \param offset: Offset to center
 * \param scale: Scale of the output
 * \param sample: Sample distance to distribute points
 */
void ED_gpencil_trace_data_to_strokes(Main *bmain,
                                      potrace_state_t *st,
                                      Object *ob,
                                      bGPDframe *gpf,
                                      int32_t offset[2],
                                      const float scale,
                                      const float sample,
                                      const int32_t resolution,
                                      const int32_t thickness)
{
#define MAX_LENGTH 100.0f
  /* Find materials and create them if not found.  */
  int32_t mat_fill_idx = BKE_gpencil_material_find_index_by_name_prefix(ob, "Stroke");
  int32_t mat_mask_idx = BKE_gpencil_material_find_index_by_name_prefix(ob, "Holdout");

  const float default_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  /* Stroke and Fill material. */
  if (mat_fill_idx == -1) {
    int32_t new_idx;
    Material *mat_gp = BKE_gpencil_object_material_new(bmain, ob, "Stroke", &new_idx);
    MaterialGPencilStyle *gp_style = mat_gp->gp_style;

    copy_v4_v4(gp_style->stroke_rgba, default_color);
    gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
    gp_style->flag |= GP_MATERIAL_FILL_SHOW;
    mat_fill_idx = ob->totcol - 1;
  }
  /* Holdout material. */
  if (mat_mask_idx == -1) {
    int32_t new_idx;
    Material *mat_gp = BKE_gpencil_object_material_new(bmain, ob, "Holdout", &new_idx);
    MaterialGPencilStyle *gp_style = mat_gp->gp_style;

    copy_v4_v4(gp_style->stroke_rgba, default_color);
    copy_v4_v4(gp_style->fill_rgba, default_color);
    gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
    gp_style->flag |= GP_MATERIAL_FILL_SHOW;
    gp_style->flag |= GP_MATERIAL_IS_STROKE_HOLDOUT;
    gp_style->flag |= GP_MATERIAL_IS_FILL_HOLDOUT;
    mat_mask_idx = ob->totcol - 1;
  }

  potrace_path_t *path = st->plist;
  int n, *tag;
  potrace_dpoint_t(*c)[3];

  /* There isn't any rule here, only the result of lots of testing to get a value that gets
   * good results using the Potrace data. */
  const float scalef = 0.008f * scale;
  /* Draw each curve. */
  path = st->plist;
  while (path != NULL) {
    n = path->curve.n;
    tag = path->curve.tag;
    c = path->curve.c;
    int mat_idx = path->sign == '+' ? mat_fill_idx : mat_mask_idx;
    /* Create a new stroke. */
    bGPDstroke *gps = BKE_gpencil_stroke_add(gpf, mat_idx, 0, thickness, false);
    /* Last point that is equals to start point. */
    float start_point[2], last[2];
    start_point[0] = c[n - 1][2].x;
    start_point[1] = c[n - 1][2].y;

    for (int32_t i = 0; i < n; i++) {
      switch (tag[i]) {
        case POTRACE_CORNER: {
          if (gps->totpoints == 0) {
            add_point(gps, scalef, offset, c[n - 1][2].x, c[n - 1][2].y);
          }
          add_point(gps, scalef, offset, c[i][1].x, c[i][1].y);

          add_point(gps, scalef, offset, c[i][2].x, c[i][2].y);
          break;
        }
        case POTRACE_CURVETO: {
          float cp1[2], cp2[2], cp3[2], cp4[2];
          if (gps->totpoints == 0) {
            cp1[0] = start_point[0];
            cp1[1] = start_point[1];
          }
          else {
            copy_v2_v2(cp1, last);
          }

          cp2[0] = c[i][0].x;
          cp2[1] = c[i][0].y;

          cp3[0] = c[i][1].x;
          cp3[1] = c[i][1].y;

          cp4[0] = c[i][2].x;
          cp4[1] = c[i][2].y;

          add_bezier(gps,
                     scalef,
                     offset,
                     resolution,
                     cp1,
                     cp2,
                     cp3,
                     cp4,
                     (gps->totpoints == 0) ? false : true);
          copy_v2_v2(last, cp4);
          break;
        }
        default:
          break;
      }
    }
    /* In some situations, Potrace can produce a wrong data and generate a very
     * long stroke. Here the length is checked and removed if the length is too big. */
    float length = BKE_gpencil_stroke_length(gps, true);
    if (length <= MAX_LENGTH) {
      bGPdata *gpd = ob->data;
      if (sample > 0.0f) {
        /* Resample stroke. Don't need to call to BKE_gpencil_stroke_geometry_update() because
         * the sample function already call that. */
        BKE_gpencil_stroke_sample(gpd, gps, sample, false);
      }
      else {
        BKE_gpencil_stroke_geometry_update(gpd, gps);
      }
    }
    else {
      /* Remove too long strokes. */
      BLI_remlink(&gpf->strokes, gps);
      BKE_gpencil_free_stroke(gps);
    }

    path = path->next;
  }
#undef MAX_LENGTH
}
