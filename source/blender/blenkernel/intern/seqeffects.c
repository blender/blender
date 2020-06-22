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
 *
 * - Blender Foundation, 2003-2009
 * - Peter Schlaile <peter [at] schlaile [dot] de> 2005/2006
 */

/** \file
 * \ingroup bke
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h" /* windows needs for M_PI */
#include "BLI_path_util.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "BKE_fcurve.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_sequencer.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_metadata.h"

#include "BLI_math_color_blend.h"

#include "RNA_access.h"

#include "RE_pipeline.h"

#include "BLF_api.h"

static struct SeqEffectHandle get_sequence_effect_impl(int seq_type);

static void slice_get_byte_buffers(const SeqRenderData *context,
                                   const ImBuf *ibuf1,
                                   const ImBuf *ibuf2,
                                   const ImBuf *ibuf3,
                                   const ImBuf *out,
                                   int start_line,
                                   unsigned char **rect1,
                                   unsigned char **rect2,
                                   unsigned char **rect3,
                                   unsigned char **rect_out)
{
  int offset = 4 * start_line * context->rectx;

  *rect1 = (unsigned char *)ibuf1->rect + offset;
  *rect_out = (unsigned char *)out->rect + offset;

  if (ibuf2) {
    *rect2 = (unsigned char *)ibuf2->rect + offset;
  }

  if (ibuf3) {
    *rect3 = (unsigned char *)ibuf3->rect + offset;
  }
}

static void slice_get_float_buffers(const SeqRenderData *context,
                                    const ImBuf *ibuf1,
                                    const ImBuf *ibuf2,
                                    const ImBuf *ibuf3,
                                    const ImBuf *out,
                                    int start_line,
                                    float **rect1,
                                    float **rect2,
                                    float **rect3,
                                    float **rect_out)
{
  int offset = 4 * start_line * context->rectx;

  *rect1 = ibuf1->rect_float + offset;
  *rect_out = out->rect_float + offset;

  if (ibuf2) {
    *rect2 = ibuf2->rect_float + offset;
  }

  if (ibuf3) {
    *rect3 = ibuf3->rect_float + offset;
  }
}

/*********************** Glow effect *************************/

enum {
  GlowR = 0,
  GlowG = 1,
  GlowB = 2,
  GlowA = 3,
};

static ImBuf *prepare_effect_imbufs(const SeqRenderData *context,
                                    ImBuf *ibuf1,
                                    ImBuf *ibuf2,
                                    ImBuf *ibuf3)
{
  ImBuf *out;
  Scene *scene = context->scene;
  int x = context->rectx;
  int y = context->recty;

  if (!ibuf1 && !ibuf2 && !ibuf3) {
    /* hmmm, global float option ? */
    out = IMB_allocImBuf(x, y, 32, IB_rect);
  }
  else if ((ibuf1 && ibuf1->rect_float) || (ibuf2 && ibuf2->rect_float) ||
           (ibuf3 && ibuf3->rect_float)) {
    /* if any inputs are rectfloat, output is float too */

    out = IMB_allocImBuf(x, y, 32, IB_rectfloat);
  }
  else {
    out = IMB_allocImBuf(x, y, 32, IB_rect);
  }

  if (out->rect_float) {
    if (ibuf1 && !ibuf1->rect_float) {
      BKE_sequencer_imbuf_to_sequencer_space(scene, ibuf1, true);
    }

    if (ibuf2 && !ibuf2->rect_float) {
      BKE_sequencer_imbuf_to_sequencer_space(scene, ibuf2, true);
    }

    if (ibuf3 && !ibuf3->rect_float) {
      BKE_sequencer_imbuf_to_sequencer_space(scene, ibuf3, true);
    }

    IMB_colormanagement_assign_float_colorspace(out, scene->sequencer_colorspace_settings.name);
  }
  else {
    if (ibuf1 && !ibuf1->rect) {
      IMB_rect_from_float(ibuf1);
    }

    if (ibuf2 && !ibuf2->rect) {
      IMB_rect_from_float(ibuf2);
    }

    if (ibuf3 && !ibuf3->rect) {
      IMB_rect_from_float(ibuf3);
    }
  }

  /* If effect only affecting a single channel, forward input's metadata to the output. */
  if (ibuf1 != NULL && ibuf1 == ibuf2 && ibuf2 == ibuf3) {
    IMB_metadata_copy(out, ibuf1);
  }

  return out;
}

/*********************** Alpha Over *************************/

static void init_alpha_over_or_under(Sequence *seq)
{
  Sequence *seq1 = seq->seq1;
  Sequence *seq2 = seq->seq2;

  seq->seq2 = seq1;
  seq->seq1 = seq2;
}

static void do_alphaover_effect_byte(float facf0,
                                     float facf1,
                                     int x,
                                     int y,
                                     unsigned char *rect1,
                                     unsigned char *rect2,
                                     unsigned char *out)
{
  float fac2, mfac, fac, fac4;
  int xo;
  unsigned char *cp1, *cp2, *rt;
  float tempc[4], rt1[4], rt2[4];

  xo = x;
  cp1 = rect1;
  cp2 = rect2;
  rt = out;

  fac2 = facf0;
  fac4 = facf1;

  while (y--) {
    x = xo;
    while (x--) {
      /* rt = rt1 over rt2  (alpha from rt1) */

      straight_uchar_to_premul_float(rt1, cp1);
      straight_uchar_to_premul_float(rt2, cp2);

      fac = fac2;
      mfac = 1.0f - fac2 * rt1[3];

      if (fac <= 0.0f) {
        *((unsigned int *)rt) = *((unsigned int *)cp2);
      }
      else if (mfac <= 0.0f) {
        *((unsigned int *)rt) = *((unsigned int *)cp1);
      }
      else {
        tempc[0] = fac * rt1[0] + mfac * rt2[0];
        tempc[1] = fac * rt1[1] + mfac * rt2[1];
        tempc[2] = fac * rt1[2] + mfac * rt2[2];
        tempc[3] = fac * rt1[3] + mfac * rt2[3];

        premul_float_to_straight_uchar(rt, tempc);
      }
      cp1 += 4;
      cp2 += 4;
      rt += 4;
    }

    if (y == 0) {
      break;
    }
    y--;

    x = xo;
    while (x--) {
      straight_uchar_to_premul_float(rt1, cp1);
      straight_uchar_to_premul_float(rt2, cp2);

      fac = fac4;
      mfac = 1.0f - (fac4 * rt1[3]);

      if (fac <= 0.0f) {
        *((unsigned int *)rt) = *((unsigned int *)cp2);
      }
      else if (mfac <= 0.0f) {
        *((unsigned int *)rt) = *((unsigned int *)cp1);
      }
      else {
        tempc[0] = fac * rt1[0] + mfac * rt2[0];
        tempc[1] = fac * rt1[1] + mfac * rt2[1];
        tempc[2] = fac * rt1[2] + mfac * rt2[2];
        tempc[3] = fac * rt1[3] + mfac * rt2[3];

        premul_float_to_straight_uchar(rt, tempc);
      }
      cp1 += 4;
      cp2 += 4;
      rt += 4;
    }
  }
}

static void do_alphaover_effect_float(
    float facf0, float facf1, int x, int y, float *rect1, float *rect2, float *out)
{
  float fac2, mfac, fac, fac4;
  int xo;
  float *rt1, *rt2, *rt;

  xo = x;
  rt1 = rect1;
  rt2 = rect2;
  rt = out;

  fac2 = facf0;
  fac4 = facf1;

  while (y--) {
    x = xo;
    while (x--) {
      /* rt = rt1 over rt2  (alpha from rt1) */

      fac = fac2;
      mfac = 1.0f - (fac2 * rt1[3]);

      if (fac <= 0.0f) {
        memcpy(rt, rt2, 4 * sizeof(float));
      }
      else if (mfac <= 0) {
        memcpy(rt, rt1, 4 * sizeof(float));
      }
      else {
        rt[0] = fac * rt1[0] + mfac * rt2[0];
        rt[1] = fac * rt1[1] + mfac * rt2[1];
        rt[2] = fac * rt1[2] + mfac * rt2[2];
        rt[3] = fac * rt1[3] + mfac * rt2[3];
      }
      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }

    if (y == 0) {
      break;
    }
    y--;

    x = xo;
    while (x--) {
      fac = fac4;
      mfac = 1.0f - (fac4 * rt1[3]);

      if (fac <= 0.0f) {
        memcpy(rt, rt2, 4 * sizeof(float));
      }
      else if (mfac <= 0.0f) {
        memcpy(rt, rt1, 4 * sizeof(float));
      }
      else {
        rt[0] = fac * rt1[0] + mfac * rt2[0];
        rt[1] = fac * rt1[1] + mfac * rt2[1];
        rt[2] = fac * rt1[2] + mfac * rt2[2];
        rt[3] = fac * rt1[3] + mfac * rt2[3];
      }
      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }
  }
}

static void do_alphaover_effect(const SeqRenderData *context,
                                Sequence *UNUSED(seq),
                                float UNUSED(cfra),
                                float facf0,
                                float facf1,
                                ImBuf *ibuf1,
                                ImBuf *ibuf2,
                                ImBuf *UNUSED(ibuf3),
                                int start_line,
                                int total_lines,
                                ImBuf *out)
{
  if (out->rect_float) {
    float *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_float_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_alphaover_effect_float(facf0, facf1, context->rectx, total_lines, rect1, rect2, rect_out);
  }
  else {
    unsigned char *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_byte_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_alphaover_effect_byte(facf0, facf1, context->rectx, total_lines, rect1, rect2, rect_out);
  }
}

/*********************** Alpha Under *************************/

static void do_alphaunder_effect_byte(float facf0,
                                      float facf1,
                                      int x,
                                      int y,
                                      unsigned char *rect1,
                                      unsigned char *rect2,
                                      unsigned char *out)
{
  float fac2, fac, fac4;
  int xo;
  unsigned char *cp1, *cp2, *rt;
  float tempc[4], rt1[4], rt2[4];

  xo = x;
  cp1 = rect1;
  cp2 = rect2;
  rt = out;

  fac2 = facf0;
  fac4 = facf1;

  while (y--) {
    x = xo;
    while (x--) {
      /* rt = rt1 under rt2  (alpha from rt2) */
      straight_uchar_to_premul_float(rt1, cp1);
      straight_uchar_to_premul_float(rt2, cp2);

      /* this complex optimization is because the
       * 'skybuf' can be crossed in
       */
      if (rt2[3] <= 0.0f && fac2 >= 1.0f) {
        *((unsigned int *)rt) = *((unsigned int *)cp1);
      }
      else if (rt2[3] >= 1.0f) {
        *((unsigned int *)rt) = *((unsigned int *)cp2);
      }
      else {
        fac = (fac2 * (1.0f - rt2[3]));

        if (fac <= 0) {
          *((unsigned int *)rt) = *((unsigned int *)cp2);
        }
        else {
          tempc[0] = (fac * rt1[0] + rt2[0]);
          tempc[1] = (fac * rt1[1] + rt2[1]);
          tempc[2] = (fac * rt1[2] + rt2[2]);
          tempc[3] = (fac * rt1[3] + rt2[3]);

          premul_float_to_straight_uchar(rt, tempc);
        }
      }
      cp1 += 4;
      cp2 += 4;
      rt += 4;
    }

    if (y == 0) {
      break;
    }
    y--;

    x = xo;
    while (x--) {
      straight_uchar_to_premul_float(rt1, cp1);
      straight_uchar_to_premul_float(rt2, cp2);

      if (rt2[3] <= 0.0f && fac4 >= 1.0f) {
        *((unsigned int *)rt) = *((unsigned int *)cp1);
      }
      else if (rt2[3] >= 1.0f) {
        *((unsigned int *)rt) = *((unsigned int *)cp2);
      }
      else {
        fac = (fac4 * (1.0f - rt2[3]));

        if (fac <= 0) {
          *((unsigned int *)rt) = *((unsigned int *)cp2);
        }
        else {
          tempc[0] = (fac * rt1[0] + rt2[0]);
          tempc[1] = (fac * rt1[1] + rt2[1]);
          tempc[2] = (fac * rt1[2] + rt2[2]);
          tempc[3] = (fac * rt1[3] + rt2[3]);

          premul_float_to_straight_uchar(rt, tempc);
        }
      }
      cp1 += 4;
      cp2 += 4;
      rt += 4;
    }
  }
}

static void do_alphaunder_effect_float(
    float facf0, float facf1, int x, int y, float *rect1, float *rect2, float *out)
{
  float fac2, fac, fac4;
  int xo;
  float *rt1, *rt2, *rt;

  xo = x;
  rt1 = rect1;
  rt2 = rect2;
  rt = out;

  fac2 = facf0;
  fac4 = facf1;

  while (y--) {
    x = xo;
    while (x--) {
      /* rt = rt1 under rt2  (alpha from rt2) */

      /* this complex optimization is because the
       * 'skybuf' can be crossed in
       */
      if (rt2[3] <= 0 && fac2 >= 1.0f) {
        memcpy(rt, rt1, 4 * sizeof(float));
      }
      else if (rt2[3] >= 1.0f) {
        memcpy(rt, rt2, 4 * sizeof(float));
      }
      else {
        fac = fac2 * (1.0f - rt2[3]);

        if (fac == 0) {
          memcpy(rt, rt2, 4 * sizeof(float));
        }
        else {
          rt[0] = fac * rt1[0] + rt2[0];
          rt[1] = fac * rt1[1] + rt2[1];
          rt[2] = fac * rt1[2] + rt2[2];
          rt[3] = fac * rt1[3] + rt2[3];
        }
      }
      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }

    if (y == 0) {
      break;
    }
    y--;

    x = xo;
    while (x--) {
      if (rt2[3] <= 0 && fac4 >= 1.0f) {
        memcpy(rt, rt1, 4 * sizeof(float));
      }
      else if (rt2[3] >= 1.0f) {
        memcpy(rt, rt2, 4 * sizeof(float));
      }
      else {
        fac = fac4 * (1.0f - rt2[3]);

        if (fac == 0) {
          memcpy(rt, rt2, 4 * sizeof(float));
        }
        else {
          rt[0] = fac * rt1[0] + rt2[0];
          rt[1] = fac * rt1[1] + rt2[1];
          rt[2] = fac * rt1[2] + rt2[2];
          rt[3] = fac * rt1[3] + rt2[3];
        }
      }
      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }
  }
}

static void do_alphaunder_effect(const SeqRenderData *context,
                                 Sequence *UNUSED(seq),
                                 float UNUSED(cfra),
                                 float facf0,
                                 float facf1,
                                 ImBuf *ibuf1,
                                 ImBuf *ibuf2,
                                 ImBuf *UNUSED(ibuf3),
                                 int start_line,
                                 int total_lines,
                                 ImBuf *out)
{
  if (out->rect_float) {
    float *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_float_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_alphaunder_effect_float(facf0, facf1, context->rectx, total_lines, rect1, rect2, rect_out);
  }
  else {
    unsigned char *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_byte_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_alphaunder_effect_byte(facf0, facf1, context->rectx, total_lines, rect1, rect2, rect_out);
  }
}

/*********************** Cross *************************/

static void do_cross_effect_byte(float facf0,
                                 float facf1,
                                 int x,
                                 int y,
                                 unsigned char *rect1,
                                 unsigned char *rect2,
                                 unsigned char *out)
{
  int fac1, fac2, fac3, fac4;
  int xo;
  unsigned char *rt1, *rt2, *rt;

  xo = x;
  rt1 = rect1;
  rt2 = rect2;
  rt = out;

  fac2 = (int)(256.0f * facf0);
  fac1 = 256 - fac2;
  fac4 = (int)(256.0f * facf1);
  fac3 = 256 - fac4;

  while (y--) {
    x = xo;
    while (x--) {
      rt[0] = (fac1 * rt1[0] + fac2 * rt2[0]) >> 8;
      rt[1] = (fac1 * rt1[1] + fac2 * rt2[1]) >> 8;
      rt[2] = (fac1 * rt1[2] + fac2 * rt2[2]) >> 8;
      rt[3] = (fac1 * rt1[3] + fac2 * rt2[3]) >> 8;

      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }

    if (y == 0) {
      break;
    }
    y--;

    x = xo;
    while (x--) {
      rt[0] = (fac3 * rt1[0] + fac4 * rt2[0]) >> 8;
      rt[1] = (fac3 * rt1[1] + fac4 * rt2[1]) >> 8;
      rt[2] = (fac3 * rt1[2] + fac4 * rt2[2]) >> 8;
      rt[3] = (fac3 * rt1[3] + fac4 * rt2[3]) >> 8;

      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }
  }
}

static void do_cross_effect_float(
    float facf0, float facf1, int x, int y, float *rect1, float *rect2, float *out)
{
  float fac1, fac2, fac3, fac4;
  int xo;
  float *rt1, *rt2, *rt;

  xo = x;
  rt1 = rect1;
  rt2 = rect2;
  rt = out;

  fac2 = facf0;
  fac1 = 1.0f - fac2;
  fac4 = facf1;
  fac3 = 1.0f - fac4;

  while (y--) {
    x = xo;
    while (x--) {
      rt[0] = fac1 * rt1[0] + fac2 * rt2[0];
      rt[1] = fac1 * rt1[1] + fac2 * rt2[1];
      rt[2] = fac1 * rt1[2] + fac2 * rt2[2];
      rt[3] = fac1 * rt1[3] + fac2 * rt2[3];

      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }

    if (y == 0) {
      break;
    }
    y--;

    x = xo;
    while (x--) {
      rt[0] = fac3 * rt1[0] + fac4 * rt2[0];
      rt[1] = fac3 * rt1[1] + fac4 * rt2[1];
      rt[2] = fac3 * rt1[2] + fac4 * rt2[2];
      rt[3] = fac3 * rt1[3] + fac4 * rt2[3];

      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }
  }
}

static void do_cross_effect(const SeqRenderData *context,
                            Sequence *UNUSED(seq),
                            float UNUSED(cfra),
                            float facf0,
                            float facf1,
                            ImBuf *ibuf1,
                            ImBuf *ibuf2,
                            ImBuf *UNUSED(ibuf3),
                            int start_line,
                            int total_lines,
                            ImBuf *out)
{
  if (out->rect_float) {
    float *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_float_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_cross_effect_float(facf0, facf1, context->rectx, total_lines, rect1, rect2, rect_out);
  }
  else {
    unsigned char *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_byte_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_cross_effect_byte(facf0, facf1, context->rectx, total_lines, rect1, rect2, rect_out);
  }
}

/*********************** Gamma Cross *************************/

/* copied code from initrender.c */
static unsigned short gamtab[65536];
static unsigned short igamtab1[256];
static bool gamma_tabs_init = false;

#define RE_GAMMA_TABLE_SIZE 400

static float gamma_range_table[RE_GAMMA_TABLE_SIZE + 1];
static float gamfactor_table[RE_GAMMA_TABLE_SIZE];
static float inv_gamma_range_table[RE_GAMMA_TABLE_SIZE + 1];
static float inv_gamfactor_table[RE_GAMMA_TABLE_SIZE];
static float color_domain_table[RE_GAMMA_TABLE_SIZE + 1];
static float color_step;
static float inv_color_step;
static float valid_gamma;
static float valid_inv_gamma;

static void makeGammaTables(float gamma)
{
  /* we need two tables: one forward, one backward */
  int i;

  valid_gamma = gamma;
  valid_inv_gamma = 1.0f / gamma;
  color_step = 1.0f / RE_GAMMA_TABLE_SIZE;
  inv_color_step = (float)RE_GAMMA_TABLE_SIZE;

  /* We could squeeze out the two range tables to gain some memory */
  for (i = 0; i < RE_GAMMA_TABLE_SIZE; i++) {
    color_domain_table[i] = i * color_step;
    gamma_range_table[i] = pow(color_domain_table[i], valid_gamma);
    inv_gamma_range_table[i] = pow(color_domain_table[i], valid_inv_gamma);
  }

  /* The end of the table should match 1.0 carefully. In order to avoid
   * rounding errors, we just set this explicitly. The last segment may
   * have a different length than the other segments, but our
   * interpolation is insensitive to that
   */
  color_domain_table[RE_GAMMA_TABLE_SIZE] = 1.0;
  gamma_range_table[RE_GAMMA_TABLE_SIZE] = 1.0;
  inv_gamma_range_table[RE_GAMMA_TABLE_SIZE] = 1.0;

  /* To speed up calculations, we make these calc factor tables. They are
   * multiplication factors used in scaling the interpolation
   */
  for (i = 0; i < RE_GAMMA_TABLE_SIZE; i++) {
    gamfactor_table[i] = inv_color_step * (gamma_range_table[i + 1] - gamma_range_table[i]);
    inv_gamfactor_table[i] = inv_color_step *
                             (inv_gamma_range_table[i + 1] - inv_gamma_range_table[i]);
  }
}

static float gammaCorrect(float c)
{
  int i;
  float res;

  i = floorf(c * inv_color_step);
  /* Clip to range [0, 1]: outside, just do the complete calculation.
   * We may have some performance problems here. Stretching up the LUT
   * may help solve that, by exchanging LUT size for the interpolation.
   * Negative colors are explicitly handled.
   */
  if (UNLIKELY(i < 0)) {
    res = -powf(-c, valid_gamma);
  }
  else if (i >= RE_GAMMA_TABLE_SIZE) {
    res = powf(c, valid_gamma);
  }
  else {
    res = gamma_range_table[i] + ((c - color_domain_table[i]) * gamfactor_table[i]);
  }

  return res;
}

/* ------------------------------------------------------------------------- */

static float invGammaCorrect(float c)
{
  int i;
  float res = 0.0;

  i = floorf(c * inv_color_step);
  /* Negative colors are explicitly handled */
  if (UNLIKELY(i < 0)) {
    res = -powf(-c, valid_inv_gamma);
  }
  else if (i >= RE_GAMMA_TABLE_SIZE) {
    res = powf(c, valid_inv_gamma);
  }
  else {
    res = inv_gamma_range_table[i] + ((c - color_domain_table[i]) * inv_gamfactor_table[i]);
  }

  return res;
}

static void gamtabs(float gamma)
{
  float val, igamma = 1.0f / gamma;
  int a;

  /* gamtab: in short, out short */
  for (a = 0; a < 65536; a++) {
    val = a;
    val /= 65535.0f;

    if (gamma == 2.0f) {
      val = sqrtf(val);
    }
    else if (gamma != 1.0f) {
      val = powf(val, igamma);
    }

    gamtab[a] = (65535.99f * val);
  }
  /* inverse gamtab1 : in byte, out short */
  for (a = 1; a <= 256; a++) {
    if (gamma == 2.0f) {
      igamtab1[a - 1] = a * a - 1;
    }
    else if (gamma == 1.0f) {
      igamtab1[a - 1] = 256 * a - 1;
    }
    else {
      val = a / 256.0f;
      igamtab1[a - 1] = (65535.0 * pow(val, gamma)) - 1;
    }
  }
}

static void build_gammatabs(void)
{
  if (gamma_tabs_init == false) {
    gamtabs(2.0f);
    makeGammaTables(2.0f);
    gamma_tabs_init = true;
  }
}

static void init_gammacross(Sequence *UNUSED(seq))
{
}

static void load_gammacross(Sequence *UNUSED(seq))
{
}

static void free_gammacross(Sequence *UNUSED(seq), const bool UNUSED(do_id_user))
{
}

static void do_gammacross_effect_byte(float facf0,
                                      float UNUSED(facf1),
                                      int x,
                                      int y,
                                      unsigned char *rect1,
                                      unsigned char *rect2,
                                      unsigned char *out)
{
  float fac1, fac2;
  int xo;
  unsigned char *cp1, *cp2, *rt;
  float rt1[4], rt2[4], tempc[4];

  xo = x;
  cp1 = rect1;
  cp2 = rect2;
  rt = out;

  fac2 = facf0;
  fac1 = 1.0f - fac2;

  while (y--) {
    x = xo;
    while (x--) {
      straight_uchar_to_premul_float(rt1, cp1);
      straight_uchar_to_premul_float(rt2, cp2);

      tempc[0] = gammaCorrect(fac1 * invGammaCorrect(rt1[0]) + fac2 * invGammaCorrect(rt2[0]));
      tempc[1] = gammaCorrect(fac1 * invGammaCorrect(rt1[1]) + fac2 * invGammaCorrect(rt2[1]));
      tempc[2] = gammaCorrect(fac1 * invGammaCorrect(rt1[2]) + fac2 * invGammaCorrect(rt2[2]));
      tempc[3] = gammaCorrect(fac1 * invGammaCorrect(rt1[3]) + fac2 * invGammaCorrect(rt2[3]));

      premul_float_to_straight_uchar(rt, tempc);
      cp1 += 4;
      cp2 += 4;
      rt += 4;
    }

    if (y == 0) {
      break;
    }
    y--;

    x = xo;
    while (x--) {
      straight_uchar_to_premul_float(rt1, cp1);
      straight_uchar_to_premul_float(rt2, cp2);

      tempc[0] = gammaCorrect(fac1 * invGammaCorrect(rt1[0]) + fac2 * invGammaCorrect(rt2[0]));
      tempc[1] = gammaCorrect(fac1 * invGammaCorrect(rt1[1]) + fac2 * invGammaCorrect(rt2[1]));
      tempc[2] = gammaCorrect(fac1 * invGammaCorrect(rt1[2]) + fac2 * invGammaCorrect(rt2[2]));
      tempc[3] = gammaCorrect(fac1 * invGammaCorrect(rt1[3]) + fac2 * invGammaCorrect(rt2[3]));

      premul_float_to_straight_uchar(rt, tempc);
      cp1 += 4;
      cp2 += 4;
      rt += 4;
    }
  }
}

static void do_gammacross_effect_float(
    float facf0, float UNUSED(facf1), int x, int y, float *rect1, float *rect2, float *out)
{
  float fac1, fac2;
  int xo;
  float *rt1, *rt2, *rt;

  xo = x;
  rt1 = rect1;
  rt2 = rect2;
  rt = out;

  fac2 = facf0;
  fac1 = 1.0f - fac2;

  while (y--) {
    x = xo * 4;
    while (x--) {
      *rt = gammaCorrect(fac1 * invGammaCorrect(*rt1) + fac2 * invGammaCorrect(*rt2));
      rt1++;
      rt2++;
      rt++;
    }

    if (y == 0) {
      break;
    }
    y--;

    x = xo * 4;
    while (x--) {
      *rt = gammaCorrect(fac1 * invGammaCorrect(*rt1) + fac2 * invGammaCorrect(*rt2));

      rt1++;
      rt2++;
      rt++;
    }
  }
}

static struct ImBuf *gammacross_init_execution(const SeqRenderData *context,
                                               ImBuf *ibuf1,
                                               ImBuf *ibuf2,
                                               ImBuf *ibuf3)
{
  ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);
  build_gammatabs();

  return out;
}

static void do_gammacross_effect(const SeqRenderData *context,
                                 Sequence *UNUSED(seq),
                                 float UNUSED(cfra),
                                 float facf0,
                                 float facf1,
                                 ImBuf *ibuf1,
                                 ImBuf *ibuf2,
                                 ImBuf *UNUSED(ibuf3),
                                 int start_line,
                                 int total_lines,
                                 ImBuf *out)
{
  if (out->rect_float) {
    float *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_float_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_gammacross_effect_float(facf0, facf1, context->rectx, total_lines, rect1, rect2, rect_out);
  }
  else {
    unsigned char *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_byte_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_gammacross_effect_byte(facf0, facf1, context->rectx, total_lines, rect1, rect2, rect_out);
  }
}

/*********************** Add *************************/

static void do_add_effect_byte(float facf0,
                               float facf1,
                               int x,
                               int y,
                               unsigned char *rect1,
                               unsigned char *rect2,
                               unsigned char *out)
{
  int xo, fac1, fac3;
  unsigned char *cp1, *cp2, *rt;

  xo = x;
  cp1 = rect1;
  cp2 = rect2;
  rt = out;

  fac1 = (int)(256.0f * facf0);
  fac3 = (int)(256.0f * facf1);

  while (y--) {
    x = xo;

    while (x--) {
      const int m = fac1 * (int)cp2[3];
      rt[0] = min_ii(cp1[0] + ((m * cp2[0]) >> 16), 255);
      rt[1] = min_ii(cp1[1] + ((m * cp2[1]) >> 16), 255);
      rt[2] = min_ii(cp1[2] + ((m * cp2[2]) >> 16), 255);
      rt[3] = cp1[3];

      cp1 += 4;
      cp2 += 4;
      rt += 4;
    }

    if (y == 0) {
      break;
    }
    y--;

    x = xo;
    while (x--) {
      const int m = fac3 * (int)cp2[3];
      rt[0] = min_ii(cp1[0] + ((m * cp2[0]) >> 16), 255);
      rt[1] = min_ii(cp1[1] + ((m * cp2[1]) >> 16), 255);
      rt[2] = min_ii(cp1[2] + ((m * cp2[2]) >> 16), 255);
      rt[3] = cp1[3];

      cp1 += 4;
      cp2 += 4;
      rt += 4;
    }
  }
}

static void do_add_effect_float(
    float facf0, float facf1, int x, int y, float *rect1, float *rect2, float *out)
{
  int xo;
  float fac1, fac3;
  float *rt1, *rt2, *rt;

  xo = x;
  rt1 = rect1;
  rt2 = rect2;
  rt = out;

  fac1 = facf0;
  fac3 = facf1;

  while (y--) {
    x = xo;
    while (x--) {
      const float m = (1.0f - (rt1[3] * (1.0f - fac1))) * rt2[3];
      rt[0] = rt1[0] + m * rt2[0];
      rt[1] = rt1[1] + m * rt2[1];
      rt[2] = rt1[2] + m * rt2[2];
      rt[3] = rt1[3];

      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }

    if (y == 0) {
      break;
    }
    y--;

    x = xo;
    while (x--) {
      const float m = (1.0f - (rt1[3] * (1.0f - fac3))) * rt2[3];
      rt[0] = rt1[0] + m * rt2[0];
      rt[1] = rt1[1] + m * rt2[1];
      rt[2] = rt1[2] + m * rt2[2];
      rt[3] = rt1[3];

      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }
  }
}

static void do_add_effect(const SeqRenderData *context,
                          Sequence *UNUSED(seq),
                          float UNUSED(cfra),
                          float facf0,
                          float facf1,
                          ImBuf *ibuf1,
                          ImBuf *ibuf2,
                          ImBuf *UNUSED(ibuf3),
                          int start_line,
                          int total_lines,
                          ImBuf *out)
{
  if (out->rect_float) {
    float *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_float_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_add_effect_float(facf0, facf1, context->rectx, total_lines, rect1, rect2, rect_out);
  }
  else {
    unsigned char *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_byte_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_add_effect_byte(facf0, facf1, context->rectx, total_lines, rect1, rect2, rect_out);
  }
}

/*********************** Sub *************************/

static void do_sub_effect_byte(float facf0,
                               float facf1,
                               int x,
                               int y,
                               unsigned char *rect1,
                               unsigned char *rect2,
                               unsigned char *out)
{
  int xo, fac1, fac3;
  unsigned char *cp1, *cp2, *rt;

  xo = x;
  cp1 = rect1;
  cp2 = rect2;
  rt = out;

  fac1 = (int)(256.0f * facf0);
  fac3 = (int)(256.0f * facf1);

  while (y--) {
    x = xo;
    while (x--) {
      const int m = fac1 * (int)cp2[3];
      rt[0] = max_ii(cp1[0] - ((m * cp2[0]) >> 16), 0);
      rt[1] = max_ii(cp1[1] - ((m * cp2[1]) >> 16), 0);
      rt[2] = max_ii(cp1[2] - ((m * cp2[2]) >> 16), 0);
      rt[3] = cp1[3];

      cp1 += 4;
      cp2 += 4;
      rt += 4;
    }

    if (y == 0) {
      break;
    }
    y--;

    x = xo;
    while (x--) {
      const int m = fac3 * (int)cp2[3];
      rt[0] = max_ii(cp1[0] - ((m * cp2[0]) >> 16), 0);
      rt[1] = max_ii(cp1[1] - ((m * cp2[1]) >> 16), 0);
      rt[2] = max_ii(cp1[2] - ((m * cp2[2]) >> 16), 0);
      rt[3] = cp1[3];

      cp1 += 4;
      cp2 += 4;
      rt += 4;
    }
  }
}

static void do_sub_effect_float(
    float UNUSED(facf0), float facf1, int x, int y, float *rect1, float *rect2, float *out)
{
  int xo;
  float /* fac1, */ fac3_inv;
  float *rt1, *rt2, *rt;

  xo = x;
  rt1 = rect1;
  rt2 = rect2;
  rt = out;

  /* UNUSED */
  // fac1 = facf0;
  fac3_inv = 1.0f - facf1;

  while (y--) {
    x = xo;
    while (x--) {
      const float m = (1.0f - (rt1[3] * fac3_inv)) * rt2[3];
      rt[0] = max_ff(rt1[0] - m * rt2[0], 0.0f);
      rt[1] = max_ff(rt1[1] - m * rt2[1], 0.0f);
      rt[2] = max_ff(rt1[2] - m * rt2[2], 0.0f);
      rt[3] = rt1[3];

      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }

    if (y == 0) {
      break;
    }
    y--;

    x = xo;
    while (x--) {
      const float m = (1.0f - (rt1[3] * fac3_inv)) * rt2[3];
      rt[0] = max_ff(rt1[0] - m * rt2[0], 0.0f);
      rt[1] = max_ff(rt1[1] - m * rt2[1], 0.0f);
      rt[2] = max_ff(rt1[2] - m * rt2[2], 0.0f);
      rt[3] = rt1[3];

      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }
  }
}

static void do_sub_effect(const SeqRenderData *context,
                          Sequence *UNUSED(seq),
                          float UNUSED(cfra),
                          float facf0,
                          float facf1,
                          ImBuf *ibuf1,
                          ImBuf *ibuf2,
                          ImBuf *UNUSED(ibuf3),
                          int start_line,
                          int total_lines,
                          ImBuf *out)
{
  if (out->rect_float) {
    float *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_float_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_sub_effect_float(facf0, facf1, context->rectx, total_lines, rect1, rect2, rect_out);
  }
  else {
    unsigned char *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_byte_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_sub_effect_byte(facf0, facf1, context->rectx, total_lines, rect1, rect2, rect_out);
  }
}

/*********************** Drop *************************/

/* Must be > 0 or add precopy, etc to the function */
#define XOFF 8
#define YOFF 8

static void do_drop_effect_byte(float facf0,
                                float facf1,
                                int x,
                                int y,
                                unsigned char *rect2i,
                                unsigned char *rect1i,
                                unsigned char *outi)
{
  int temp, fac, fac1, fac2;
  unsigned char *rt1, *rt2, *out;
  int field = 1;

  const int width = x;
  const int height = y;
  const int xoff = min_ii(XOFF, width);
  const int yoff = min_ii(YOFF, height);

  fac1 = (int)(70.0f * facf0);
  fac2 = (int)(70.0f * facf1);

  rt2 = rect2i + yoff * 4 * width;
  rt1 = rect1i;
  out = outi;
  for (y = 0; y < height - yoff; y++) {
    if (field) {
      fac = fac1;
    }
    else {
      fac = fac2;
    }
    field = !field;

    memcpy(out, rt1, sizeof(*out) * xoff * 4);
    rt1 += xoff * 4;
    out += xoff * 4;

    for (x = xoff; x < width; x++) {
      temp = ((fac * rt2[3]) >> 8);

      *(out++) = MAX2(0, *rt1 - temp);
      rt1++;
      *(out++) = MAX2(0, *rt1 - temp);
      rt1++;
      *(out++) = MAX2(0, *rt1 - temp);
      rt1++;
      *(out++) = MAX2(0, *rt1 - temp);
      rt1++;
      rt2 += 4;
    }
    rt2 += xoff * 4;
  }
  memcpy(out, rt1, sizeof(*out) * yoff * 4 * width);
}

static void do_drop_effect_float(
    float facf0, float facf1, int x, int y, float *rect2i, float *rect1i, float *outi)
{
  float temp, fac, fac1, fac2;
  float *rt1, *rt2, *out;
  int field = 1;

  const int width = x;
  const int height = y;
  const int xoff = min_ii(XOFF, width);
  const int yoff = min_ii(YOFF, height);

  fac1 = 70.0f * facf0;
  fac2 = 70.0f * facf1;

  rt2 = rect2i + yoff * 4 * width;
  rt1 = rect1i;
  out = outi;
  for (y = 0; y < height - yoff; y++) {
    if (field) {
      fac = fac1;
    }
    else {
      fac = fac2;
    }
    field = !field;

    memcpy(out, rt1, sizeof(*out) * xoff * 4);
    rt1 += xoff * 4;
    out += xoff * 4;

    for (x = xoff; x < width; x++) {
      temp = fac * rt2[3];

      *(out++) = MAX2(0.0f, *rt1 - temp);
      rt1++;
      *(out++) = MAX2(0.0f, *rt1 - temp);
      rt1++;
      *(out++) = MAX2(0.0f, *rt1 - temp);
      rt1++;
      *(out++) = MAX2(0.0f, *rt1 - temp);
      rt1++;
      rt2 += 4;
    }
    rt2 += xoff * 4;
  }
  memcpy(out, rt1, sizeof(*out) * yoff * 4 * width);
}

/*********************** Mul *************************/

static void do_mul_effect_byte(float facf0,
                               float facf1,
                               int x,
                               int y,
                               unsigned char *rect1,
                               unsigned char *rect2,
                               unsigned char *out)
{
  int xo, fac1, fac3;
  unsigned char *rt1, *rt2, *rt;

  xo = x;
  rt1 = rect1;
  rt2 = rect2;
  rt = out;

  fac1 = (int)(256.0f * facf0);
  fac3 = (int)(256.0f * facf1);

  /* formula:
   * fac * (a * b) + (1 - fac) * a  => fac * a * (b - 1) + axaux = c * px + py * s; //+centx
   * yaux = -s * px + c * py; //+centy
   */

  while (y--) {

    x = xo;
    while (x--) {

      rt[0] = rt1[0] + ((fac1 * rt1[0] * (rt2[0] - 255)) >> 16);
      rt[1] = rt1[1] + ((fac1 * rt1[1] * (rt2[1] - 255)) >> 16);
      rt[2] = rt1[2] + ((fac1 * rt1[2] * (rt2[2] - 255)) >> 16);
      rt[3] = rt1[3] + ((fac1 * rt1[3] * (rt2[3] - 255)) >> 16);

      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }

    if (y == 0) {
      break;
    }
    y--;

    x = xo;
    while (x--) {

      rt[0] = rt1[0] + ((fac3 * rt1[0] * (rt2[0] - 255)) >> 16);
      rt[1] = rt1[1] + ((fac3 * rt1[1] * (rt2[1] - 255)) >> 16);
      rt[2] = rt1[2] + ((fac3 * rt1[2] * (rt2[2] - 255)) >> 16);
      rt[3] = rt1[3] + ((fac3 * rt1[3] * (rt2[3] - 255)) >> 16);

      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }
  }
}

static void do_mul_effect_float(
    float facf0, float facf1, int x, int y, float *rect1, float *rect2, float *out)
{
  int xo;
  float fac1, fac3;
  float *rt1, *rt2, *rt;

  xo = x;
  rt1 = rect1;
  rt2 = rect2;
  rt = out;

  fac1 = facf0;
  fac3 = facf1;

  /* formula:
   * fac * (a * b) + (1 - fac) * a  =>  fac * a * (b - 1) + a
   */

  while (y--) {
    x = xo;
    while (x--) {
      rt[0] = rt1[0] + fac1 * rt1[0] * (rt2[0] - 1.0f);
      rt[1] = rt1[1] + fac1 * rt1[1] * (rt2[1] - 1.0f);
      rt[2] = rt1[2] + fac1 * rt1[2] * (rt2[2] - 1.0f);
      rt[3] = rt1[3] + fac1 * rt1[3] * (rt2[3] - 1.0f);

      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }

    if (y == 0) {
      break;
    }
    y--;

    x = xo;
    while (x--) {
      rt[0] = rt1[0] + fac3 * rt1[0] * (rt2[0] - 1.0f);
      rt[1] = rt1[1] + fac3 * rt1[1] * (rt2[1] - 1.0f);
      rt[2] = rt1[2] + fac3 * rt1[2] * (rt2[2] - 1.0f);
      rt[3] = rt1[3] + fac3 * rt1[3] * (rt2[3] - 1.0f);

      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }
  }
}

static void do_mul_effect(const SeqRenderData *context,
                          Sequence *UNUSED(seq),
                          float UNUSED(cfra),
                          float facf0,
                          float facf1,
                          ImBuf *ibuf1,
                          ImBuf *ibuf2,
                          ImBuf *UNUSED(ibuf3),
                          int start_line,
                          int total_lines,
                          ImBuf *out)
{
  if (out->rect_float) {
    float *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_float_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_mul_effect_float(facf0, facf1, context->rectx, total_lines, rect1, rect2, rect_out);
  }
  else {
    unsigned char *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_byte_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_mul_effect_byte(facf0, facf1, context->rectx, total_lines, rect1, rect2, rect_out);
  }
}

/*********************** Blend Mode ***************************************/
typedef void (*IMB_blend_func_byte)(unsigned char *dst,
                                    const unsigned char *src1,
                                    const unsigned char *src2);
typedef void (*IMB_blend_func_float)(float *dst, const float *src1, const float *src2);

BLI_INLINE void apply_blend_function_byte(float facf0,
                                          float facf1,
                                          int x,
                                          int y,
                                          unsigned char *rect1,
                                          unsigned char *rect2,
                                          unsigned char *out,
                                          IMB_blend_func_byte blend_function)
{
  int xo;
  unsigned char *rt1, *rt2, *rt;
  unsigned int achannel;
  xo = x;
  rt1 = rect1;
  rt2 = rect2;
  rt = out;
  while (y--) {
    for (x = xo; x > 0; x--) {
      achannel = rt1[3];
      rt1[3] = (unsigned int)achannel * facf0;
      blend_function(rt, rt1, rt2);
      rt1[3] = achannel;
      rt[3] = rt1[3];
      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }
    if (y == 0) {
      break;
    }
    y--;
    for (x = xo; x > 0; x--) {
      achannel = rt1[3];
      rt1[3] = (unsigned int)achannel * facf1;
      blend_function(rt, rt1, rt2);
      rt1[3] = achannel;
      rt[3] = rt1[3];
      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }
  }
}

BLI_INLINE void apply_blend_function_float(float facf0,
                                           float facf1,
                                           int x,
                                           int y,
                                           float *rect1,
                                           float *rect2,
                                           float *out,
                                           IMB_blend_func_float blend_function)
{
  int xo;
  float *rt1, *rt2, *rt;
  float achannel;
  xo = x;
  rt1 = rect1;
  rt2 = rect2;
  rt = out;
  while (y--) {
    for (x = xo; x > 0; x--) {
      achannel = rt1[3];
      rt1[3] = achannel * facf0;
      blend_function(rt, rt1, rt2);
      rt1[3] = achannel;
      rt[3] = rt1[3];
      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }
    if (y == 0) {
      break;
    }
    y--;
    for (x = xo; x > 0; x--) {
      achannel = rt1[3];
      rt1[3] = achannel * facf1;
      blend_function(rt, rt1, rt2);
      rt1[3] = achannel;
      rt[3] = rt1[3];
      rt1 += 4;
      rt2 += 4;
      rt += 4;
    }
  }
}

static void do_blend_effect_float(
    float facf0, float facf1, int x, int y, float *rect1, float *rect2, int btype, float *out)
{
  switch (btype) {
    case SEQ_TYPE_ADD:
      apply_blend_function_float(facf0, facf1, x, y, rect1, rect2, out, blend_color_add_float);
      break;
    case SEQ_TYPE_SUB:
      apply_blend_function_float(facf0, facf1, x, y, rect1, rect2, out, blend_color_sub_float);
      break;
    case SEQ_TYPE_MUL:
      apply_blend_function_float(facf0, facf1, x, y, rect1, rect2, out, blend_color_mul_float);
      break;
    case SEQ_TYPE_DARKEN:
      apply_blend_function_float(facf0, facf1, x, y, rect1, rect2, out, blend_color_darken_float);
      break;
    case SEQ_TYPE_COLOR_BURN:
      apply_blend_function_float(facf0, facf1, x, y, rect1, rect2, out, blend_color_burn_float);
      break;
    case SEQ_TYPE_LINEAR_BURN:
      apply_blend_function_float(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_linearburn_float);
      break;
    case SEQ_TYPE_SCREEN:
      apply_blend_function_float(facf0, facf1, x, y, rect1, rect2, out, blend_color_screen_float);
      break;
    case SEQ_TYPE_LIGHTEN:
      apply_blend_function_float(facf0, facf1, x, y, rect1, rect2, out, blend_color_lighten_float);
      break;
    case SEQ_TYPE_DODGE:
      apply_blend_function_float(facf0, facf1, x, y, rect1, rect2, out, blend_color_dodge_float);
      break;
    case SEQ_TYPE_OVERLAY:
      apply_blend_function_float(facf0, facf1, x, y, rect1, rect2, out, blend_color_overlay_float);
      break;
    case SEQ_TYPE_SOFT_LIGHT:
      apply_blend_function_float(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_softlight_float);
      break;
    case SEQ_TYPE_HARD_LIGHT:
      apply_blend_function_float(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_hardlight_float);
      break;
    case SEQ_TYPE_PIN_LIGHT:
      apply_blend_function_float(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_pinlight_float);
      break;
    case SEQ_TYPE_LIN_LIGHT:
      apply_blend_function_float(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_linearlight_float);
      break;
    case SEQ_TYPE_VIVID_LIGHT:
      apply_blend_function_float(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_vividlight_float);
      break;
    case SEQ_TYPE_BLEND_COLOR:
      apply_blend_function_float(facf0, facf1, x, y, rect1, rect2, out, blend_color_color_float);
      break;
    case SEQ_TYPE_HUE:
      apply_blend_function_float(facf0, facf1, x, y, rect1, rect2, out, blend_color_hue_float);
      break;
    case SEQ_TYPE_SATURATION:
      apply_blend_function_float(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_saturation_float);
      break;
    case SEQ_TYPE_VALUE:
      apply_blend_function_float(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_luminosity_float);
      break;
    case SEQ_TYPE_DIFFERENCE:
      apply_blend_function_float(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_difference_float);
      break;
    case SEQ_TYPE_EXCLUSION:
      apply_blend_function_float(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_exclusion_float);
      break;
    default:
      break;
  }
}

static void do_blend_effect_byte(float facf0,
                                 float facf1,
                                 int x,
                                 int y,
                                 unsigned char *rect1,
                                 unsigned char *rect2,
                                 int btype,
                                 unsigned char *out)
{
  switch (btype) {
    case SEQ_TYPE_ADD:
      apply_blend_function_byte(facf0, facf1, x, y, rect1, rect2, out, blend_color_add_byte);
      break;
    case SEQ_TYPE_SUB:
      apply_blend_function_byte(facf0, facf1, x, y, rect1, rect2, out, blend_color_sub_byte);
      break;
    case SEQ_TYPE_MUL:
      apply_blend_function_byte(facf0, facf1, x, y, rect1, rect2, out, blend_color_mul_byte);
      break;
    case SEQ_TYPE_DARKEN:
      apply_blend_function_byte(facf0, facf1, x, y, rect1, rect2, out, blend_color_darken_byte);
      break;
    case SEQ_TYPE_COLOR_BURN:
      apply_blend_function_byte(facf0, facf1, x, y, rect1, rect2, out, blend_color_burn_byte);
      break;
    case SEQ_TYPE_LINEAR_BURN:
      apply_blend_function_byte(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_linearburn_byte);
      break;
    case SEQ_TYPE_SCREEN:
      apply_blend_function_byte(facf0, facf1, x, y, rect1, rect2, out, blend_color_screen_byte);
      break;
    case SEQ_TYPE_LIGHTEN:
      apply_blend_function_byte(facf0, facf1, x, y, rect1, rect2, out, blend_color_lighten_byte);
      break;
    case SEQ_TYPE_DODGE:
      apply_blend_function_byte(facf0, facf1, x, y, rect1, rect2, out, blend_color_dodge_byte);
      break;
    case SEQ_TYPE_OVERLAY:
      apply_blend_function_byte(facf0, facf1, x, y, rect1, rect2, out, blend_color_overlay_byte);
      break;
    case SEQ_TYPE_SOFT_LIGHT:
      apply_blend_function_byte(facf0, facf1, x, y, rect1, rect2, out, blend_color_softlight_byte);
      break;
    case SEQ_TYPE_HARD_LIGHT:
      apply_blend_function_byte(facf0, facf1, x, y, rect1, rect2, out, blend_color_hardlight_byte);
      break;
    case SEQ_TYPE_PIN_LIGHT:
      apply_blend_function_byte(facf0, facf1, x, y, rect1, rect2, out, blend_color_pinlight_byte);
      break;
    case SEQ_TYPE_LIN_LIGHT:
      apply_blend_function_byte(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_linearlight_byte);
      break;
    case SEQ_TYPE_VIVID_LIGHT:
      apply_blend_function_byte(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_vividlight_byte);
      break;
    case SEQ_TYPE_BLEND_COLOR:
      apply_blend_function_byte(facf0, facf1, x, y, rect1, rect2, out, blend_color_color_byte);
      break;
    case SEQ_TYPE_HUE:
      apply_blend_function_byte(facf0, facf1, x, y, rect1, rect2, out, blend_color_hue_byte);
      break;
    case SEQ_TYPE_SATURATION:
      apply_blend_function_byte(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_saturation_byte);
      break;
    case SEQ_TYPE_VALUE:
      apply_blend_function_byte(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_luminosity_byte);
      break;
    case SEQ_TYPE_DIFFERENCE:
      apply_blend_function_byte(
          facf0, facf1, x, y, rect1, rect2, out, blend_color_difference_byte);
      break;
    case SEQ_TYPE_EXCLUSION:
      apply_blend_function_byte(facf0, facf1, x, y, rect1, rect2, out, blend_color_exclusion_byte);
      break;
    default:
      break;
  }
}

static void do_blend_mode_effect(const SeqRenderData *context,
                                 Sequence *seq,
                                 float UNUSED(cfra),
                                 float facf0,
                                 float facf1,
                                 ImBuf *ibuf1,
                                 ImBuf *ibuf2,
                                 ImBuf *UNUSED(ibuf3),
                                 int start_line,
                                 int total_lines,
                                 ImBuf *out)
{
  if (out->rect_float) {
    float *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;
    slice_get_float_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);
    do_blend_effect_float(
        facf0, facf1, context->rectx, total_lines, rect1, rect2, seq->blend_mode, rect_out);
  }
  else {
    unsigned char *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;
    slice_get_byte_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);
    do_blend_effect_byte(
        facf0, facf1, context->rectx, total_lines, rect1, rect2, seq->blend_mode, rect_out);
  }
}
/*********************** Color Mix Effect  *************************/
static void init_colormix_effect(Sequence *seq)
{
  ColorMixVars *data;

  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }
  seq->effectdata = MEM_callocN(sizeof(ColorMixVars), "colormixvars");
  data = (ColorMixVars *)seq->effectdata;
  data->blend_effect = SEQ_TYPE_OVERLAY;
  data->factor = 1.0f;
}

static void do_colormix_effect(const SeqRenderData *context,
                               Sequence *seq,
                               float UNUSED(cfra),
                               float UNUSED(facf0),
                               float UNUSED(facf1),
                               ImBuf *ibuf1,
                               ImBuf *ibuf2,
                               ImBuf *UNUSED(ibuf3),
                               int start_line,
                               int total_lines,
                               ImBuf *out)
{
  float facf;

  ColorMixVars *data = seq->effectdata;
  facf = data->factor;

  if (out->rect_float) {
    float *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;
    slice_get_float_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);
    do_blend_effect_float(
        facf, facf, context->rectx, total_lines, rect1, rect2, data->blend_effect, rect_out);
  }
  else {
    unsigned char *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;
    slice_get_byte_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);
    do_blend_effect_byte(
        facf, facf, context->rectx, total_lines, rect1, rect2, data->blend_effect, rect_out);
  }
}

/*********************** Wipe *************************/

typedef struct WipeZone {
  float angle;
  int flip;
  int xo, yo;
  int width;
  float pythangle;
} WipeZone;

static void precalc_wipe_zone(WipeZone *wipezone, WipeVars *wipe, int xo, int yo)
{
  wipezone->flip = (wipe->angle < 0.0f);
  wipezone->angle = tanf(fabsf(wipe->angle));
  wipezone->xo = xo;
  wipezone->yo = yo;
  wipezone->width = (int)(wipe->edgeWidth * ((xo + yo) / 2.0f));
  wipezone->pythangle = 1.0f / sqrtf(wipezone->angle * wipezone->angle + 1.0f);
}

/* This function calculates the blur band for the wipe effects */
static float in_band(float width, float dist, int side, int dir)
{
  float alpha;

  if (width == 0) {
    return (float)side;
  }

  if (width < dist) {
    return (float)side;
  }

  if (side == 1) {
    alpha = (dist + 0.5f * width) / (width);
  }
  else {
    alpha = (0.5f * width - dist) / (width);
  }

  if (dir == 0) {
    alpha = 1 - alpha;
  }

  return alpha;
}

static float check_zone(WipeZone *wipezone, int x, int y, Sequence *seq, float facf0)
{
  float posx, posy, hyp, hyp2, angle, hwidth, b1, b2, b3, pointdist;
  /* some future stuff */
  /* float hyp3, hyp4, b4, b5 */
  float temp1, temp2, temp3, temp4; /* some placeholder variables */
  int xo = wipezone->xo;
  int yo = wipezone->yo;
  float halfx = xo * 0.5f;
  float halfy = yo * 0.5f;
  float widthf, output = 0;
  WipeVars *wipe = (WipeVars *)seq->effectdata;
  int width;

  if (wipezone->flip) {
    x = xo - x;
  }
  angle = wipezone->angle;

  if (wipe->forward) {
    posx = facf0 * xo;
    posy = facf0 * yo;
  }
  else {
    posx = xo - facf0 * xo;
    posy = yo - facf0 * yo;
  }

  switch (wipe->wipetype) {
    case DO_SINGLE_WIPE:
      width = min_ii(wipezone->width, facf0 * yo);
      width = min_ii(width, yo - facf0 * yo);

      if (angle == 0.0f) {
        b1 = posy;
        b2 = y;
        hyp = fabsf(y - posy);
      }
      else {
        b1 = posy - (-angle) * posx;
        b2 = y - (-angle) * x;
        hyp = fabsf(angle * x + y + (-posy - angle * posx)) * wipezone->pythangle;
      }

      if (angle < 0) {
        temp1 = b1;
        b1 = b2;
        b2 = temp1;
      }

      if (wipe->forward) {
        if (b1 < b2) {
          output = in_band(width, hyp, 1, 1);
        }
        else {
          output = in_band(width, hyp, 0, 1);
        }
      }
      else {
        if (b1 < b2) {
          output = in_band(width, hyp, 0, 1);
        }
        else {
          output = in_band(width, hyp, 1, 1);
        }
      }
      break;

    case DO_DOUBLE_WIPE:
      if (!wipe->forward) {
        facf0 = 1.0f - facf0; /* Go the other direction */
      }

      width = wipezone->width; /* calculate the blur width */
      hwidth = width * 0.5f;
      if (angle == 0) {
        b1 = posy * 0.5f;
        b3 = yo - posy * 0.5f;
        b2 = y;

        hyp = fabsf(y - posy * 0.5f);
        hyp2 = fabsf(y - (yo - posy * 0.5f));
      }
      else {
        b1 = posy * 0.5f - (-angle) * posx * 0.5f;
        b3 = (yo - posy * 0.5f) - (-angle) * (xo - posx * 0.5f);
        b2 = y - (-angle) * x;

        hyp = fabsf(angle * x + y + (-posy * 0.5f - angle * posx * 0.5f)) * wipezone->pythangle;
        hyp2 = fabsf(angle * x + y + (-(yo - posy * 0.5f) - angle * (xo - posx * 0.5f))) *
               wipezone->pythangle;
      }

      hwidth = min_ff(hwidth, fabsf(b3 - b1) / 2.0f);

      if (b2 < b1 && b2 < b3) {
        output = in_band(hwidth, hyp, 0, 1);
      }
      else if (b2 > b1 && b2 > b3) {
        output = in_band(hwidth, hyp2, 0, 1);
      }
      else {
        if (hyp < hwidth && hyp2 > hwidth) {
          output = in_band(hwidth, hyp, 1, 1);
        }
        else if (hyp > hwidth && hyp2 < hwidth) {
          output = in_band(hwidth, hyp2, 1, 1);
        }
        else {
          output = in_band(hwidth, hyp2, 1, 1) * in_band(hwidth, hyp, 1, 1);
        }
      }
      if (!wipe->forward) {
        output = 1 - output;
      }
      break;
    case DO_CLOCK_WIPE:
      /*
       * temp1: angle of effect center in rads
       * temp2: angle of line through (halfx, halfy) and (x, y) in rads
       * temp3: angle of low side of blur
       * temp4: angle of high side of blur
       */
      output = 1.0f - facf0;
      widthf = wipe->edgeWidth * 2.0f * (float)M_PI;
      temp1 = 2.0f * (float)M_PI * facf0;

      if (wipe->forward) {
        temp1 = 2.0f * (float)M_PI - temp1;
      }

      x = x - halfx;
      y = y - halfy;

      temp2 = asin(abs(y) / hypot(x, y));
      if (x <= 0 && y >= 0) {
        temp2 = (float)M_PI - temp2;
      }
      else if (x <= 0 && y <= 0) {
        temp2 += (float)M_PI;
      }
      else if (x >= 0 && y <= 0) {
        temp2 = 2.0f * (float)M_PI - temp2;
      }

      if (wipe->forward) {
        temp3 = temp1 - (widthf * 0.5f) * facf0;
        temp4 = temp1 + (widthf * 0.5f) * (1 - facf0);
      }
      else {
        temp3 = temp1 - (widthf * 0.5f) * (1 - facf0);
        temp4 = temp1 + (widthf * 0.5f) * facf0;
      }
      if (temp3 < 0) {
        temp3 = 0;
      }
      if (temp4 > 2.0f * (float)M_PI) {
        temp4 = 2.0f * (float)M_PI;
      }

      if (temp2 < temp3) {
        output = 0;
      }
      else if (temp2 > temp4) {
        output = 1;
      }
      else {
        output = (temp2 - temp3) / (temp4 - temp3);
      }
      if (x == 0 && y == 0) {
        output = 1;
      }
      if (output != output) {
        output = 1;
      }
      if (wipe->forward) {
        output = 1 - output;
      }
      break;
    case DO_IRIS_WIPE:
      if (xo > yo) {
        yo = xo;
      }
      else {
        xo = yo;
      }

      if (!wipe->forward) {
        facf0 = 1 - facf0;
      }

      width = wipezone->width;
      hwidth = width * 0.5f;

      temp1 = (halfx - (halfx)*facf0);
      pointdist = hypotf(temp1, temp1);

      temp2 = hypotf(halfx - x, halfy - y);
      if (temp2 > pointdist) {
        output = in_band(hwidth, fabsf(temp2 - pointdist), 0, 1);
      }
      else {
        output = in_band(hwidth, fabsf(temp2 - pointdist), 1, 1);
      }

      if (!wipe->forward) {
        output = 1 - output;
      }

      break;
  }
  if (output < 0) {
    output = 0;
  }
  else if (output > 1) {
    output = 1;
  }
  return output;
}

static void init_wipe_effect(Sequence *seq)
{
  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }

  seq->effectdata = MEM_callocN(sizeof(WipeVars), "wipevars");
}

static int num_inputs_wipe(void)
{
  return 2;
}

static void free_wipe_effect(Sequence *seq, const bool UNUSED(do_id_user))
{
  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }

  seq->effectdata = NULL;
}

static void copy_wipe_effect(Sequence *dst, Sequence *src, const int UNUSED(flag))
{
  dst->effectdata = MEM_dupallocN(src->effectdata);
}

static void do_wipe_effect_byte(Sequence *seq,
                                float facf0,
                                float UNUSED(facf1),
                                int x,
                                int y,
                                unsigned char *rect1,
                                unsigned char *rect2,
                                unsigned char *out)
{
  WipeZone wipezone;
  WipeVars *wipe = (WipeVars *)seq->effectdata;
  int xo, yo;
  unsigned char *cp1, *cp2, *rt;

  precalc_wipe_zone(&wipezone, wipe, x, y);

  cp1 = rect1;
  cp2 = rect2;
  rt = out;

  xo = x;
  yo = y;
  for (y = 0; y < yo; y++) {
    for (x = 0; x < xo; x++) {
      float check = check_zone(&wipezone, x, y, seq, facf0);
      if (check) {
        if (cp1) {
          float rt1[4], rt2[4], tempc[4];

          straight_uchar_to_premul_float(rt1, cp1);
          straight_uchar_to_premul_float(rt2, cp2);

          tempc[0] = rt1[0] * check + rt2[0] * (1 - check);
          tempc[1] = rt1[1] * check + rt2[1] * (1 - check);
          tempc[2] = rt1[2] * check + rt2[2] * (1 - check);
          tempc[3] = rt1[3] * check + rt2[3] * (1 - check);

          premul_float_to_straight_uchar(rt, tempc);
        }
        else {
          rt[0] = 0;
          rt[1] = 0;
          rt[2] = 0;
          rt[3] = 255;
        }
      }
      else {
        if (cp2) {
          rt[0] = cp2[0];
          rt[1] = cp2[1];
          rt[2] = cp2[2];
          rt[3] = cp2[3];
        }
        else {
          rt[0] = 0;
          rt[1] = 0;
          rt[2] = 0;
          rt[3] = 255;
        }
      }

      rt += 4;
      if (cp1 != NULL) {
        cp1 += 4;
      }
      if (cp2 != NULL) {
        cp2 += 4;
      }
    }
  }
}

static void do_wipe_effect_float(Sequence *seq,
                                 float facf0,
                                 float UNUSED(facf1),
                                 int x,
                                 int y,
                                 float *rect1,
                                 float *rect2,
                                 float *out)
{
  WipeZone wipezone;
  WipeVars *wipe = (WipeVars *)seq->effectdata;
  int xo, yo;
  float *rt1, *rt2, *rt;

  precalc_wipe_zone(&wipezone, wipe, x, y);

  rt1 = rect1;
  rt2 = rect2;
  rt = out;

  xo = x;
  yo = y;
  for (y = 0; y < yo; y++) {
    for (x = 0; x < xo; x++) {
      float check = check_zone(&wipezone, x, y, seq, facf0);
      if (check) {
        if (rt1) {
          rt[0] = rt1[0] * check + rt2[0] * (1 - check);
          rt[1] = rt1[1] * check + rt2[1] * (1 - check);
          rt[2] = rt1[2] * check + rt2[2] * (1 - check);
          rt[3] = rt1[3] * check + rt2[3] * (1 - check);
        }
        else {
          rt[0] = 0;
          rt[1] = 0;
          rt[2] = 0;
          rt[3] = 1.0;
        }
      }
      else {
        if (rt2) {
          rt[0] = rt2[0];
          rt[1] = rt2[1];
          rt[2] = rt2[2];
          rt[3] = rt2[3];
        }
        else {
          rt[0] = 0;
          rt[1] = 0;
          rt[2] = 0;
          rt[3] = 1.0;
        }
      }

      rt += 4;
      if (rt1 != NULL) {
        rt1 += 4;
      }
      if (rt2 != NULL) {
        rt2 += 4;
      }
    }
  }
}

static ImBuf *do_wipe_effect(const SeqRenderData *context,
                             Sequence *seq,
                             float UNUSED(cfra),
                             float facf0,
                             float facf1,
                             ImBuf *ibuf1,
                             ImBuf *ibuf2,
                             ImBuf *ibuf3)
{
  ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);

  if (out->rect_float) {
    do_wipe_effect_float(seq,
                         facf0,
                         facf1,
                         context->rectx,
                         context->recty,
                         ibuf1->rect_float,
                         ibuf2->rect_float,
                         out->rect_float);
  }
  else {
    do_wipe_effect_byte(seq,
                        facf0,
                        facf1,
                        context->rectx,
                        context->recty,
                        (unsigned char *)ibuf1->rect,
                        (unsigned char *)ibuf2->rect,
                        (unsigned char *)out->rect);
  }

  return out;
}

/*********************** Transform *************************/

static void init_transform_effect(Sequence *seq)
{
  TransformVars *transform;

  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }

  seq->effectdata = MEM_callocN(sizeof(TransformVars), "transformvars");

  transform = (TransformVars *)seq->effectdata;

  transform->ScalexIni = 1.0f;
  transform->ScaleyIni = 1.0f;

  transform->xIni = 0.0f;
  transform->yIni = 0.0f;

  transform->rotIni = 0.0f;

  transform->interpolation = 1;
  transform->percent = 1;
  transform->uniform_scale = 0;
}

static int num_inputs_transform(void)
{
  return 1;
}

static void free_transform_effect(Sequence *seq, const bool UNUSED(do_id_user))
{
  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }
  seq->effectdata = NULL;
}

static void copy_transform_effect(Sequence *dst, Sequence *src, const int UNUSED(flag))
{
  dst->effectdata = MEM_dupallocN(src->effectdata);
}

static void transform_image(int x,
                            int y,
                            ImBuf *ibuf1,
                            ImBuf *out,
                            float scale_x,
                            float scale_y,
                            float translate_x,
                            float translate_y,
                            float rotate,
                            int interpolation)
{
  int xo, yo, xi, yi;
  float xt, yt, xr, yr;
  float s, c;

  xo = x;
  yo = y;

  /* Rotate */
  s = sinf(rotate);
  c = cosf(rotate);

  for (yi = 0; yi < yo; yi++) {
    for (xi = 0; xi < xo; xi++) {
      /* translate point */
      xt = xi - translate_x;
      yt = yi - translate_y;

      /* rotate point with center ref */
      xr = c * xt + s * yt;
      yr = -s * xt + c * yt;

      /* scale point with center ref */
      xt = xr / scale_x;
      yt = yr / scale_y;

      /* undo reference center point  */
      xt += (xo / 2.0f);
      yt += (yo / 2.0f);

      /* interpolate */
      switch (interpolation) {
        case 0:
          nearest_interpolation(ibuf1, out, xt, yt, xi, yi);
          break;
        case 1:
          bilinear_interpolation(ibuf1, out, xt, yt, xi, yi);
          break;
        case 2:
          bicubic_interpolation(ibuf1, out, xt, yt, xi, yi);
          break;
      }
    }
  }
}

static void do_transform(
    Scene *scene, Sequence *seq, float UNUSED(facf0), int x, int y, ImBuf *ibuf1, ImBuf *out)
{
  TransformVars *transform = (TransformVars *)seq->effectdata;
  float scale_x, scale_y, translate_x, translate_y, rotate_radians;

  /* Scale */
  if (transform->uniform_scale) {
    scale_x = scale_y = transform->ScalexIni;
  }
  else {
    scale_x = transform->ScalexIni;
    scale_y = transform->ScaleyIni;
  }

  /* Translate */
  if (!transform->percent) {
    float rd_s = (scene->r.size / 100.0f);

    translate_x = transform->xIni * rd_s + (x / 2.0f);
    translate_y = transform->yIni * rd_s + (y / 2.0f);
  }
  else {
    translate_x = x * (transform->xIni / 100.0f) + (x / 2.0f);
    translate_y = y * (transform->yIni / 100.0f) + (y / 2.0f);
  }

  /* Rotate */
  rotate_radians = DEG2RADF(transform->rotIni);

  transform_image(x,
                  y,
                  ibuf1,
                  out,
                  scale_x,
                  scale_y,
                  translate_x,
                  translate_y,
                  rotate_radians,
                  transform->interpolation);
}

static ImBuf *do_transform_effect(const SeqRenderData *context,
                                  Sequence *seq,
                                  float UNUSED(cfra),
                                  float facf0,
                                  float UNUSED(facf1),
                                  ImBuf *ibuf1,
                                  ImBuf *ibuf2,
                                  ImBuf *ibuf3)
{
  ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);

  do_transform(context->scene, seq, facf0, context->rectx, context->recty, ibuf1, out);

  return out;
}

/*********************** Glow *************************/

static void RVBlurBitmap2_float(float *map, int width, int height, float blur, int quality)
{
  /* Much better than the previous blur!
   * We do the blurring in two passes which is a whole lot faster.
   * I changed the math around to implement an actual Gaussian distribution.
   *
   * Watch out though, it tends to misbehave with large blur values on
   * a small bitmap. Avoid avoid! */

  float *temp = NULL, *swap;
  float *filter = NULL;
  int x, y, i, fx, fy;
  int index, ix, halfWidth;
  float fval, k, curColor[4], curColor2[4], weight = 0;

  /* If we're not really blurring, bail out */
  if (blur <= 0) {
    return;
  }

  /* Allocate memory for the tempmap and the blur filter matrix */
  temp = MEM_mallocN((width * height * 4 * sizeof(float)), "blurbitmaptemp");
  if (!temp) {
    return;
  }

  /* Allocate memory for the filter elements */
  halfWidth = ((quality + 1) * blur);
  filter = (float *)MEM_mallocN(sizeof(float) * halfWidth * 2, "blurbitmapfilter");
  if (!filter) {
    MEM_freeN(temp);
    return;
  }

  /* Apparently we're calculating a bell curve based on the standard deviation (or radius)
   * This code is based on an example posted to comp.graphics.algorithms by
   * Blancmange (bmange@airdmhor.gen.nz)
   */

  k = -1.0f / (2.0f * (float)M_PI * blur * blur);

  for (ix = 0; ix < halfWidth; ix++) {
    weight = (float)exp(k * (ix * ix));
    filter[halfWidth - ix] = weight;
    filter[halfWidth + ix] = weight;
  }
  filter[0] = weight;

  /* Normalize the array */
  fval = 0;
  for (ix = 0; ix < halfWidth * 2; ix++) {
    fval += filter[ix];
  }

  for (ix = 0; ix < halfWidth * 2; ix++) {
    filter[ix] /= fval;
  }

  /* Blur the rows */
  for (y = 0; y < height; y++) {
    /* Do the left & right strips */
    for (x = 0; x < halfWidth; x++) {
      fx = 0;
      zero_v4(curColor);
      zero_v4(curColor2);

      for (i = x - halfWidth; i < x + halfWidth; i++) {
        if ((i >= 0) && (i < width)) {
          index = (i + y * width) * 4;
          madd_v4_v4fl(curColor, map + index, filter[fx]);

          index = (width - 1 - i + y * width) * 4;
          madd_v4_v4fl(curColor2, map + index, filter[fx]);
        }
        fx++;
      }
      index = (x + y * width) * 4;
      copy_v4_v4(temp + index, curColor);

      index = (width - 1 - x + y * width) * 4;
      copy_v4_v4(temp + index, curColor2);
    }

    /* Do the main body */
    for (x = halfWidth; x < width - halfWidth; x++) {
      fx = 0;
      zero_v4(curColor);
      for (i = x - halfWidth; i < x + halfWidth; i++) {
        index = (i + y * width) * 4;
        madd_v4_v4fl(curColor, map + index, filter[fx]);
        fx++;
      }
      index = (x + y * width) * 4;
      copy_v4_v4(temp + index, curColor);
    }
  }

  /* Swap buffers */
  swap = temp;
  temp = map;
  map = swap;

  /* Blur the columns */
  for (x = 0; x < width; x++) {
    /* Do the top & bottom strips */
    for (y = 0; y < halfWidth; y++) {
      fy = 0;
      zero_v4(curColor);
      zero_v4(curColor2);
      for (i = y - halfWidth; i < y + halfWidth; i++) {
        if ((i >= 0) && (i < height)) {
          /* Bottom */
          index = (x + i * width) * 4;
          madd_v4_v4fl(curColor, map + index, filter[fy]);

          /* Top */
          index = (x + (height - 1 - i) * width) * 4;
          madd_v4_v4fl(curColor2, map + index, filter[fy]);
        }
        fy++;
      }
      index = (x + y * width) * 4;
      copy_v4_v4(temp + index, curColor);

      index = (x + (height - 1 - y) * width) * 4;
      copy_v4_v4(temp + index, curColor2);
    }

    /* Do the main body */
    for (y = halfWidth; y < height - halfWidth; y++) {
      fy = 0;
      zero_v4(curColor);
      for (i = y - halfWidth; i < y + halfWidth; i++) {
        index = (x + i * width) * 4;
        madd_v4_v4fl(curColor, map + index, filter[fy]);
        fy++;
      }
      index = (x + y * width) * 4;
      copy_v4_v4(temp + index, curColor);
    }
  }

  /* Swap buffers */
  swap = temp;
  temp = map; /* map = swap; */ /* UNUSED */

  /* Tidy up   */
  MEM_freeN(filter);
  MEM_freeN(temp);
}

static void RVAddBitmaps_float(float *a, float *b, float *c, int width, int height)
{
  int x, y, index;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      index = (x + y * width) * 4;
      c[index + GlowR] = min_ff(1.0f, a[index + GlowR] + b[index + GlowR]);
      c[index + GlowG] = min_ff(1.0f, a[index + GlowG] + b[index + GlowG]);
      c[index + GlowB] = min_ff(1.0f, a[index + GlowB] + b[index + GlowB]);
      c[index + GlowA] = min_ff(1.0f, a[index + GlowA] + b[index + GlowA]);
    }
  }
}

static void RVIsolateHighlights_float(
    float *in, float *out, int width, int height, float threshold, float boost, float clamp)
{
  int x, y, index;
  float intensity;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      index = (x + y * width) * 4;

      /* Isolate the intensity */
      intensity = (in[index + GlowR] + in[index + GlowG] + in[index + GlowB] - threshold);
      if (intensity > 0) {
        out[index + GlowR] = min_ff(clamp, (in[index + GlowR] * boost * intensity));
        out[index + GlowG] = min_ff(clamp, (in[index + GlowG] * boost * intensity));
        out[index + GlowB] = min_ff(clamp, (in[index + GlowB] * boost * intensity));
        out[index + GlowA] = min_ff(clamp, (in[index + GlowA] * boost * intensity));
      }
      else {
        out[index + GlowR] = 0;
        out[index + GlowG] = 0;
        out[index + GlowB] = 0;
        out[index + GlowA] = 0;
      }
    }
  }
}

static void init_glow_effect(Sequence *seq)
{
  GlowVars *glow;

  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }

  seq->effectdata = MEM_callocN(sizeof(GlowVars), "glowvars");

  glow = (GlowVars *)seq->effectdata;
  glow->fMini = 0.25;
  glow->fClamp = 1.0;
  glow->fBoost = 0.5;
  glow->dDist = 3.0;
  glow->dQuality = 3;
  glow->bNoComp = 0;
}

static int num_inputs_glow(void)
{
  return 1;
}

static void free_glow_effect(Sequence *seq, const bool UNUSED(do_id_user))
{
  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }

  seq->effectdata = NULL;
}

static void copy_glow_effect(Sequence *dst, Sequence *src, const int UNUSED(flag))
{
  dst->effectdata = MEM_dupallocN(src->effectdata);
}

static void do_glow_effect_byte(Sequence *seq,
                                int render_size,
                                float facf0,
                                float UNUSED(facf1),
                                int x,
                                int y,
                                unsigned char *rect1,
                                unsigned char *UNUSED(rect2),
                                unsigned char *out)
{
  float *outbuf, *inbuf;
  GlowVars *glow = (GlowVars *)seq->effectdata;

  inbuf = MEM_mallocN(4 * sizeof(float) * x * y, "glow effect input");
  outbuf = MEM_mallocN(4 * sizeof(float) * x * y, "glow effect output");

  IMB_buffer_float_from_byte(inbuf, rect1, IB_PROFILE_SRGB, IB_PROFILE_SRGB, false, x, y, x, x);
  IMB_buffer_float_premultiply(inbuf, x, y);

  RVIsolateHighlights_float(
      inbuf, outbuf, x, y, glow->fMini * 3.0f, glow->fBoost * facf0, glow->fClamp);
  RVBlurBitmap2_float(outbuf, x, y, glow->dDist * (render_size / 100.0f), glow->dQuality);
  if (!glow->bNoComp) {
    RVAddBitmaps_float(inbuf, outbuf, outbuf, x, y);
  }

  IMB_buffer_float_unpremultiply(outbuf, x, y);
  IMB_buffer_byte_from_float(
      out, outbuf, 4, 0.0f, IB_PROFILE_SRGB, IB_PROFILE_SRGB, false, x, y, x, x);

  MEM_freeN(inbuf);
  MEM_freeN(outbuf);
}

static void do_glow_effect_float(Sequence *seq,
                                 int render_size,
                                 float facf0,
                                 float UNUSED(facf1),
                                 int x,
                                 int y,
                                 float *rect1,
                                 float *UNUSED(rect2),
                                 float *out)
{
  float *outbuf = out;
  float *inbuf = rect1;
  GlowVars *glow = (GlowVars *)seq->effectdata;

  RVIsolateHighlights_float(
      inbuf, outbuf, x, y, glow->fMini * 3.0f, glow->fBoost * facf0, glow->fClamp);
  RVBlurBitmap2_float(outbuf, x, y, glow->dDist * (render_size / 100.0f), glow->dQuality);
  if (!glow->bNoComp) {
    RVAddBitmaps_float(inbuf, outbuf, outbuf, x, y);
  }
}

static ImBuf *do_glow_effect(const SeqRenderData *context,
                             Sequence *seq,
                             float UNUSED(cfra),
                             float facf0,
                             float facf1,
                             ImBuf *ibuf1,
                             ImBuf *ibuf2,
                             ImBuf *ibuf3)
{
  ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);

  int render_size = 100 * context->rectx / context->scene->r.xsch;

  if (out->rect_float) {
    do_glow_effect_float(seq,
                         render_size,
                         facf0,
                         facf1,
                         context->rectx,
                         context->recty,
                         ibuf1->rect_float,
                         NULL,
                         out->rect_float);
  }
  else {
    do_glow_effect_byte(seq,
                        render_size,
                        facf0,
                        facf1,
                        context->rectx,
                        context->recty,
                        (unsigned char *)ibuf1->rect,
                        NULL,
                        (unsigned char *)out->rect);
  }

  return out;
}

/*********************** Solid color *************************/

static void init_solid_color(Sequence *seq)
{
  SolidColorVars *cv;

  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }

  seq->effectdata = MEM_callocN(sizeof(SolidColorVars), "solidcolor");

  cv = (SolidColorVars *)seq->effectdata;
  cv->col[0] = cv->col[1] = cv->col[2] = 0.5;
}

static int num_inputs_color(void)
{
  return 0;
}

static void free_solid_color(Sequence *seq, const bool UNUSED(do_id_user))
{
  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }

  seq->effectdata = NULL;
}

static void copy_solid_color(Sequence *dst, Sequence *src, const int UNUSED(flag))
{
  dst->effectdata = MEM_dupallocN(src->effectdata);
}

static int early_out_color(Sequence *UNUSED(seq), float UNUSED(facf0), float UNUSED(facf1))
{
  return EARLY_NO_INPUT;
}

static ImBuf *do_solid_color(const SeqRenderData *context,
                             Sequence *seq,
                             float UNUSED(cfra),
                             float facf0,
                             float facf1,
                             ImBuf *ibuf1,
                             ImBuf *ibuf2,
                             ImBuf *ibuf3)
{
  ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);

  SolidColorVars *cv = (SolidColorVars *)seq->effectdata;

  unsigned char *rect;
  float *rect_float;
  int x; /*= context->rectx;*/ /*UNUSED*/
  int y; /*= context->recty;*/ /*UNUSED*/

  if (out->rect) {
    unsigned char col0[3];
    unsigned char col1[3];

    col0[0] = facf0 * cv->col[0] * 255;
    col0[1] = facf0 * cv->col[1] * 255;
    col0[2] = facf0 * cv->col[2] * 255;

    col1[0] = facf1 * cv->col[0] * 255;
    col1[1] = facf1 * cv->col[1] * 255;
    col1[2] = facf1 * cv->col[2] * 255;

    rect = (unsigned char *)out->rect;

    for (y = 0; y < out->y; y++) {
      for (x = 0; x < out->x; x++, rect += 4) {
        rect[0] = col0[0];
        rect[1] = col0[1];
        rect[2] = col0[2];
        rect[3] = 255;
      }
      y++;
      if (y < out->y) {
        for (x = 0; x < out->x; x++, rect += 4) {
          rect[0] = col1[0];
          rect[1] = col1[1];
          rect[2] = col1[2];
          rect[3] = 255;
        }
      }
    }
  }
  else if (out->rect_float) {
    float col0[3];
    float col1[3];

    col0[0] = facf0 * cv->col[0];
    col0[1] = facf0 * cv->col[1];
    col0[2] = facf0 * cv->col[2];

    col1[0] = facf1 * cv->col[0];
    col1[1] = facf1 * cv->col[1];
    col1[2] = facf1 * cv->col[2];

    rect_float = out->rect_float;

    for (y = 0; y < out->y; y++) {
      for (x = 0; x < out->x; x++, rect_float += 4) {
        rect_float[0] = col0[0];
        rect_float[1] = col0[1];
        rect_float[2] = col0[2];
        rect_float[3] = 1.0;
      }
      y++;
      if (y < out->y) {
        for (x = 0; x < out->x; x++, rect_float += 4) {
          rect_float[0] = col1[0];
          rect_float[1] = col1[1];
          rect_float[2] = col1[2];
          rect_float[3] = 1.0;
        }
      }
    }
  }
  return out;
}

/*********************** Mulitcam *************************/

/* no effect inputs for multicam, we use give_ibuf_seq */
static int num_inputs_multicam(void)
{
  return 0;
}

static int early_out_multicam(Sequence *UNUSED(seq), float UNUSED(facf0), float UNUSED(facf1))
{
  return EARLY_NO_INPUT;
}

static ImBuf *do_multicam(const SeqRenderData *context,
                          Sequence *seq,
                          float cfra,
                          float UNUSED(facf0),
                          float UNUSED(facf1),
                          ImBuf *UNUSED(ibuf1),
                          ImBuf *UNUSED(ibuf2),
                          ImBuf *UNUSED(ibuf3))
{
  ImBuf *out;
  Editing *ed;
  ListBase *seqbasep;

  if (seq->multicam_source == 0 || seq->multicam_source >= seq->machine) {
    return NULL;
  }

  ed = context->scene->ed;
  if (!ed) {
    return NULL;
  }
  seqbasep = BKE_sequence_seqbase(&ed->seqbase, seq);
  if (!seqbasep) {
    return NULL;
  }

  out = BKE_sequencer_give_ibuf_seqbase(context, cfra, seq->multicam_source, seqbasep);

  return out;
}

/*********************** Adjustment *************************/

/* no effect inputs for adjustment, we use give_ibuf_seq */
static int num_inputs_adjustment(void)
{
  return 0;
}

static int early_out_adjustment(Sequence *UNUSED(seq), float UNUSED(facf0), float UNUSED(facf1))
{
  return EARLY_NO_INPUT;
}

static ImBuf *do_adjustment_impl(const SeqRenderData *context, Sequence *seq, float cfra)
{
  Editing *ed;
  ListBase *seqbasep;
  ImBuf *i = NULL;

  ed = context->scene->ed;

  seqbasep = BKE_sequence_seqbase(&ed->seqbase, seq);

  if (seq->machine > 1) {
    i = BKE_sequencer_give_ibuf_seqbase(context, cfra, seq->machine - 1, seqbasep);
  }

  /* found nothing? so let's work the way up the metastrip stack, so
   * that it is possible to group a bunch of adjustment strips into
   * a metastrip and have that work on everything below the metastrip
   */

  if (!i) {
    Sequence *meta;

    meta = BKE_sequence_metastrip(&ed->seqbase, NULL, seq);

    if (meta) {
      i = do_adjustment_impl(context, meta, cfra);
    }
  }

  return i;
}

static ImBuf *do_adjustment(const SeqRenderData *context,
                            Sequence *seq,
                            float cfra,
                            float UNUSED(facf0),
                            float UNUSED(facf1),
                            ImBuf *UNUSED(ibuf1),
                            ImBuf *UNUSED(ibuf2),
                            ImBuf *UNUSED(ibuf3))
{
  ImBuf *out;
  Editing *ed;

  ed = context->scene->ed;

  if (!ed) {
    return NULL;
  }

  out = do_adjustment_impl(context, seq, cfra);

  return out;
}

/*********************** Speed *************************/

static void init_speed_effect(Sequence *seq)
{
  SpeedControlVars *v;

  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }

  seq->effectdata = MEM_callocN(sizeof(SpeedControlVars), "speedcontrolvars");

  v = (SpeedControlVars *)seq->effectdata;
  v->globalSpeed = 1.0;
  v->frameMap = NULL;
  v->flags |= SEQ_SPEED_INTEGRATE; /* should be default behavior */
  v->length = 0;
}

static void load_speed_effect(Sequence *seq)
{
  SpeedControlVars *v = (SpeedControlVars *)seq->effectdata;

  v->frameMap = NULL;
  v->length = 0;
}

static int num_inputs_speed(void)
{
  return 1;
}

static void free_speed_effect(Sequence *seq, const bool UNUSED(do_id_user))
{
  SpeedControlVars *v = (SpeedControlVars *)seq->effectdata;
  if (v->frameMap) {
    MEM_freeN(v->frameMap);
  }
  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }
  seq->effectdata = NULL;
}

static void copy_speed_effect(Sequence *dst, Sequence *src, const int UNUSED(flag))
{
  SpeedControlVars *v;
  dst->effectdata = MEM_dupallocN(src->effectdata);
  v = (SpeedControlVars *)dst->effectdata;
  v->frameMap = NULL;
  v->length = 0;
}

static int early_out_speed(Sequence *UNUSED(seq), float UNUSED(facf0), float UNUSED(facf1))
{
  return EARLY_DO_EFFECT;
}

static void store_icu_yrange_speed(Sequence *seq, short UNUSED(adrcode), float *ymin, float *ymax)
{
  SpeedControlVars *v = (SpeedControlVars *)seq->effectdata;

  /* if not already done, load / initialize data */
  BKE_sequence_get_effect(seq);

  if ((v->flags & SEQ_SPEED_INTEGRATE) != 0) {
    *ymin = -100.0;
    *ymax = 100.0;
  }
  else {
    if (v->flags & SEQ_SPEED_COMPRESS_IPO_Y) {
      *ymin = 0.0;
      *ymax = 1.0;
    }
    else {
      *ymin = 0.0;
      *ymax = seq->len;
    }
  }
}

void BKE_sequence_effect_speed_rebuild_map(Scene *scene, Sequence *seq, bool force)
{
  int cfra;
  float fallback_fac = 1.0f;
  SpeedControlVars *v = (SpeedControlVars *)seq->effectdata;
  FCurve *fcu = NULL;
  int flags = v->flags;

  /* if not already done, load / initialize data */
  BKE_sequence_get_effect(seq);

  if ((force == false) && (seq->len == v->length) && (v->frameMap != NULL)) {
    return;
  }
  if ((seq->seq1 == NULL) || (seq->len < 1)) {
    /* make coverity happy and check for (CID 598) input strip ... */
    return;
  }

  /* XXX - new in 2.5x. should we use the animation system this way?
   * The fcurve is needed because many frames need evaluating at once - campbell */
  fcu = id_data_find_fcurve(&scene->id, seq, &RNA_Sequence, "speed_factor", 0, NULL);
  if (!v->frameMap || v->length != seq->len) {
    if (v->frameMap) {
      MEM_freeN(v->frameMap);
    }

    v->length = seq->len;

    v->frameMap = MEM_callocN(sizeof(float) * v->length, "speedcontrol frameMap");
  }

  fallback_fac = 1.0;

  if (seq->flag & SEQ_USE_EFFECT_DEFAULT_FADE) {
    if ((seq->seq1->enddisp != seq->seq1->start) && (seq->seq1->len != 0)) {
      fallback_fac = (float)seq->seq1->len / (float)(seq->seq1->enddisp - seq->seq1->start);
      flags = SEQ_SPEED_INTEGRATE;
      fcu = NULL;
    }
  }
  else {
    /* if there is no fcurve, use value as simple multiplier */
    if (!fcu) {
      fallback_fac = seq->speed_fader; /* same as speed_factor in rna*/
    }
  }

  if (flags & SEQ_SPEED_INTEGRATE) {
    float cursor = 0;
    float facf;

    v->frameMap[0] = 0;
    v->lastValidFrame = 0;

    for (cfra = 1; cfra < v->length; cfra++) {
      if (fcu) {
        facf = evaluate_fcurve(fcu, seq->startdisp + cfra);
      }
      else {
        facf = fallback_fac;
      }
      facf *= v->globalSpeed;

      cursor += facf;

      if (cursor >= seq->seq1->len) {
        v->frameMap[cfra] = seq->seq1->len - 1;
      }
      else {
        v->frameMap[cfra] = cursor;
        v->lastValidFrame = cfra;
      }
    }
  }
  else {
    float facf;

    v->lastValidFrame = 0;
    for (cfra = 0; cfra < v->length; cfra++) {

      if (fcu) {
        facf = evaluate_fcurve(fcu, seq->startdisp + cfra);
      }
      else {
        facf = fallback_fac;
      }

      if (flags & SEQ_SPEED_COMPRESS_IPO_Y) {
        facf *= seq->seq1->len;
      }
      facf *= v->globalSpeed;

      if (facf >= seq->seq1->len) {
        facf = seq->seq1->len - 1;
      }
      else {
        v->lastValidFrame = cfra;
      }
      v->frameMap[cfra] = facf;
    }
  }
}

/* Override cfra when rendering speed effect input. */
float BKE_sequencer_speed_effect_target_frame_get(const SeqRenderData *context,
                                                  Sequence *seq,
                                                  float cfra,
                                                  int input)
{
  int nr = BKE_sequencer_give_stripelem_index(seq, cfra);
  SpeedControlVars *s = (SpeedControlVars *)seq->effectdata;
  BKE_sequence_effect_speed_rebuild_map(context->scene, seq, false);

  /* No interpolation. */
  if ((s->flags & SEQ_SPEED_USE_INTERPOLATION) == 0) {
    return seq->start + s->frameMap[nr];
  }

  /* We need to provide current and next image for interpolation. */
  if (input == 0) { /* Current frame. */
    return floor(seq->start + s->frameMap[nr]);
  }
  else { /* Next frame. */
    return ceil(seq->start + s->frameMap[nr]);
  }
}

static float speed_effect_interpolation_ratio_get(SpeedControlVars *s, Sequence *seq, float cfra)
{
  int nr = BKE_sequencer_give_stripelem_index(seq, cfra);
  return s->frameMap[nr] - floor(s->frameMap[nr]);
}

static ImBuf *do_speed_effect(const SeqRenderData *context,
                              Sequence *seq,
                              float cfra,
                              float facf0,
                              float facf1,
                              ImBuf *ibuf1,
                              ImBuf *ibuf2,
                              ImBuf *ibuf3)
{
  SpeedControlVars *s = (SpeedControlVars *)seq->effectdata;
  struct SeqEffectHandle cross_effect = get_sequence_effect_impl(SEQ_TYPE_CROSS);
  ImBuf *out;

  if (s->flags & SEQ_SPEED_USE_INTERPOLATION) {
    out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);
    facf0 = facf1 = speed_effect_interpolation_ratio_get(s, seq, cfra);
    /* Current frame is ibuf1, next frame is ibuf2. */
    out = BKE_sequencer_effect_execute_threaded(
        &cross_effect, context, NULL, cfra, facf0, facf1, ibuf1, ibuf2, ibuf3);
    return out;
  }

  /* No interpolation. */
  return IMB_dupImBuf(ibuf1);
}

/*********************** overdrop *************************/

static void do_overdrop_effect(const SeqRenderData *context,
                               Sequence *UNUSED(seq),
                               float UNUSED(cfra),
                               float facf0,
                               float facf1,
                               ImBuf *ibuf1,
                               ImBuf *ibuf2,
                               ImBuf *UNUSED(ibuf3),
                               int start_line,
                               int total_lines,
                               ImBuf *out)
{
  int x = context->rectx;
  int y = total_lines;

  if (out->rect_float) {
    float *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_float_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_drop_effect_float(facf0, facf1, x, y, rect1, rect2, rect_out);
    do_alphaover_effect_float(facf0, facf1, x, y, rect1, rect2, rect_out);
  }
  else {
    unsigned char *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_byte_buffers(
        context, ibuf1, ibuf2, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_drop_effect_byte(facf0, facf1, x, y, rect1, rect2, rect_out);
    do_alphaover_effect_byte(facf0, facf1, x, y, rect1, rect2, rect_out);
  }
}

/*********************** Gaussian Blur *************************/

/* NOTE: This gaussian blur implementation accumulates values in the square
 * kernel rather that doing X direction and then Y direction because of the
 * lack of using multiple-staged filters.
 *
 * Once we can we'll implement a way to apply filter as multiple stages we
 * can optimize hell of a lot in here.
 */

static void init_gaussian_blur_effect(Sequence *seq)
{
  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }

  seq->effectdata = MEM_callocN(sizeof(WipeVars), "wipevars");
}

static int num_inputs_gaussian_blur(void)
{
  return 1;
}

static void free_gaussian_blur_effect(Sequence *seq, const bool UNUSED(do_id_user))
{
  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }

  seq->effectdata = NULL;
}

static void copy_gaussian_blur_effect(Sequence *dst, Sequence *src, const int UNUSED(flag))
{
  dst->effectdata = MEM_dupallocN(src->effectdata);
}

static int early_out_gaussian_blur(Sequence *seq, float UNUSED(facf0), float UNUSED(facf1))
{
  GaussianBlurVars *data = seq->effectdata;
  if (data->size_x == 0.0f && data->size_y == 0) {
    return EARLY_USE_INPUT_1;
  }
  return EARLY_DO_EFFECT;
}

/* TODO(sergey): De-duplicate with compositor. */
static float *make_gaussian_blur_kernel(float rad, int size)
{
  float *gausstab, sum, val;
  float fac;
  int i, n;

  n = 2 * size + 1;

  gausstab = (float *)MEM_mallocN(sizeof(float) * n, __func__);

  sum = 0.0f;
  fac = (rad > 0.0f ? 1.0f / rad : 0.0f);
  for (i = -size; i <= size; i++) {
    val = RE_filter_value(R_FILTER_GAUSS, (float)i * fac);
    sum += val;
    gausstab[i + size] = val;
  }

  sum = 1.0f / sum;
  for (i = 0; i < n; i++) {
    gausstab[i] *= sum;
  }

  return gausstab;
}

static void do_gaussian_blur_effect_byte_x(Sequence *seq,
                                           int start_line,
                                           int x,
                                           int y,
                                           int frame_width,
                                           int UNUSED(frame_height),
                                           unsigned char *rect,
                                           unsigned char *out)
{
#define INDEX(_x, _y) (((_y) * (x) + (_x)) * 4)
  GaussianBlurVars *data = seq->effectdata;
  const int size_x = (int)(data->size_x + 0.5f);
  int i, j;

  /* Make gaussian weight table. */
  float *gausstab_x;
  gausstab_x = make_gaussian_blur_kernel(data->size_x, size_x);

  for (i = 0; i < y; i++) {
    for (j = 0; j < x; j++) {
      int out_index = INDEX(j, i);
      float accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      float accum_weight = 0.0f;

      for (int current_x = j - size_x; current_x <= j + size_x; current_x++) {
        if (current_x < 0 || current_x >= frame_width) {
          /* Out of bounds. */
          continue;
        }
        int index = INDEX(current_x, i + start_line);
        float weight = gausstab_x[current_x - j + size_x];
        accum[0] += rect[index] * weight;
        accum[1] += rect[index + 1] * weight;
        accum[2] += rect[index + 2] * weight;
        accum[3] += rect[index + 3] * weight;
        accum_weight += weight;
      }

      float inv_accum_weight = 1.0f / accum_weight;
      out[out_index + 0] = accum[0] * inv_accum_weight;
      out[out_index + 1] = accum[1] * inv_accum_weight;
      out[out_index + 2] = accum[2] * inv_accum_weight;
      out[out_index + 3] = accum[3] * inv_accum_weight;
    }
  }

  MEM_freeN(gausstab_x);
#undef INDEX
}

static void do_gaussian_blur_effect_byte_y(Sequence *seq,
                                           int start_line,
                                           int x,
                                           int y,
                                           int UNUSED(frame_width),
                                           int frame_height,
                                           unsigned char *rect,
                                           unsigned char *out)
{
#define INDEX(_x, _y) (((_y) * (x) + (_x)) * 4)
  GaussianBlurVars *data = seq->effectdata;
  const int size_y = (int)(data->size_y + 0.5f);
  int i, j;

  /* Make gaussian weight table. */
  float *gausstab_y;
  gausstab_y = make_gaussian_blur_kernel(data->size_y, size_y);

  for (i = 0; i < y; i++) {
    for (j = 0; j < x; j++) {
      int out_index = INDEX(j, i);
      float accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      float accum_weight = 0.0f;
      for (int current_y = i - size_y; current_y <= i + size_y; current_y++) {
        if (current_y < -start_line || current_y + start_line >= frame_height) {
          /* Out of bounds. */
          continue;
        }
        int index = INDEX(j, current_y + start_line);
        float weight = gausstab_y[current_y - i + size_y];
        accum[0] += rect[index] * weight;
        accum[1] += rect[index + 1] * weight;
        accum[2] += rect[index + 2] * weight;
        accum[3] += rect[index + 3] * weight;
        accum_weight += weight;
      }
      float inv_accum_weight = 1.0f / accum_weight;
      out[out_index + 0] = accum[0] * inv_accum_weight;
      out[out_index + 1] = accum[1] * inv_accum_weight;
      out[out_index + 2] = accum[2] * inv_accum_weight;
      out[out_index + 3] = accum[3] * inv_accum_weight;
    }
  }

  MEM_freeN(gausstab_y);
#undef INDEX
}

static void do_gaussian_blur_effect_float_x(Sequence *seq,
                                            int start_line,
                                            int x,
                                            int y,
                                            int frame_width,
                                            int UNUSED(frame_height),
                                            float *rect,
                                            float *out)
{
#define INDEX(_x, _y) (((_y) * (x) + (_x)) * 4)
  GaussianBlurVars *data = seq->effectdata;
  const int size_x = (int)(data->size_x + 0.5f);
  int i, j;

  /* Make gaussian weight table. */
  float *gausstab_x;
  gausstab_x = make_gaussian_blur_kernel(data->size_x, size_x);

  for (i = 0; i < y; i++) {
    for (j = 0; j < x; j++) {
      int out_index = INDEX(j, i);
      float accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      float accum_weight = 0.0f;
      for (int current_x = j - size_x; current_x <= j + size_x; current_x++) {
        if (current_x < 0 || current_x >= frame_width) {
          /* Out of bounds. */
          continue;
        }
        int index = INDEX(current_x, i + start_line);
        float weight = gausstab_x[current_x - j + size_x];
        madd_v4_v4fl(accum, &rect[index], weight);
        accum_weight += weight;
      }
      mul_v4_v4fl(&out[out_index], accum, 1.0f / accum_weight);
    }
  }

  MEM_freeN(gausstab_x);
#undef INDEX
}

static void do_gaussian_blur_effect_float_y(Sequence *seq,
                                            int start_line,
                                            int x,
                                            int y,
                                            int UNUSED(frame_width),
                                            int frame_height,
                                            float *rect,
                                            float *out)
{
#define INDEX(_x, _y) (((_y) * (x) + (_x)) * 4)
  GaussianBlurVars *data = seq->effectdata;
  const int size_y = (int)(data->size_y + 0.5f);
  int i, j;

  /* Make gaussian weight table. */
  float *gausstab_y;
  gausstab_y = make_gaussian_blur_kernel(data->size_y, size_y);

  for (i = 0; i < y; i++) {
    for (j = 0; j < x; j++) {
      int out_index = INDEX(j, i);
      float accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      float accum_weight = 0.0f;
      for (int current_y = i - size_y; current_y <= i + size_y; current_y++) {
        if (current_y < -start_line || current_y + start_line >= frame_height) {
          /* Out of bounds. */
          continue;
        }
        int index = INDEX(j, current_y + start_line);
        float weight = gausstab_y[current_y - i + size_y];
        madd_v4_v4fl(accum, &rect[index], weight);
        accum_weight += weight;
      }
      mul_v4_v4fl(&out[out_index], accum, 1.0f / accum_weight);
    }
  }

  MEM_freeN(gausstab_y);
#undef INDEX
}

static void do_gaussian_blur_effect_x_cb(const SeqRenderData *context,
                                         Sequence *seq,
                                         ImBuf *ibuf,
                                         int start_line,
                                         int total_lines,
                                         ImBuf *out)
{
  if (out->rect_float) {
    float *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_float_buffers(
        context, ibuf, NULL, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_gaussian_blur_effect_float_x(seq,
                                    start_line,
                                    context->rectx,
                                    total_lines,
                                    context->rectx,
                                    context->recty,
                                    ibuf->rect_float,
                                    rect_out);
  }
  else {
    unsigned char *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_byte_buffers(
        context, ibuf, NULL, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_gaussian_blur_effect_byte_x(seq,
                                   start_line,
                                   context->rectx,
                                   total_lines,
                                   context->rectx,
                                   context->recty,
                                   (unsigned char *)ibuf->rect,
                                   rect_out);
  }
}

static void do_gaussian_blur_effect_y_cb(const SeqRenderData *context,
                                         Sequence *seq,
                                         ImBuf *ibuf,
                                         int start_line,
                                         int total_lines,
                                         ImBuf *out)
{
  if (out->rect_float) {
    float *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_float_buffers(
        context, ibuf, NULL, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_gaussian_blur_effect_float_y(seq,
                                    start_line,
                                    context->rectx,
                                    total_lines,
                                    context->rectx,
                                    context->recty,
                                    ibuf->rect_float,
                                    rect_out);
  }
  else {
    unsigned char *rect1 = NULL, *rect2 = NULL, *rect_out = NULL;

    slice_get_byte_buffers(
        context, ibuf, NULL, NULL, out, start_line, &rect1, &rect2, NULL, &rect_out);

    do_gaussian_blur_effect_byte_y(seq,
                                   start_line,
                                   context->rectx,
                                   total_lines,
                                   context->rectx,
                                   context->recty,
                                   (unsigned char *)ibuf->rect,
                                   rect_out);
  }
}

typedef struct RenderGaussianBlurEffectInitData {
  const SeqRenderData *context;
  Sequence *seq;
  ImBuf *ibuf;
  ImBuf *out;
} RenderGaussianBlurEffectInitData;

typedef struct RenderGaussianBlurEffectThread {
  const SeqRenderData *context;
  Sequence *seq;
  ImBuf *ibuf;
  ImBuf *out;
  int start_line, tot_line;
} RenderGaussianBlurEffectThread;

static void render_effect_execute_init_handle(void *handle_v,
                                              int start_line,
                                              int tot_line,
                                              void *init_data_v)
{
  RenderGaussianBlurEffectThread *handle = (RenderGaussianBlurEffectThread *)handle_v;
  RenderGaussianBlurEffectInitData *init_data = (RenderGaussianBlurEffectInitData *)init_data_v;

  handle->context = init_data->context;
  handle->seq = init_data->seq;
  handle->ibuf = init_data->ibuf;
  handle->out = init_data->out;

  handle->start_line = start_line;
  handle->tot_line = tot_line;
}

static void *render_effect_execute_do_x_thread(void *thread_data_v)
{
  RenderGaussianBlurEffectThread *thread_data = (RenderGaussianBlurEffectThread *)thread_data_v;
  do_gaussian_blur_effect_x_cb(thread_data->context,
                               thread_data->seq,
                               thread_data->ibuf,
                               thread_data->start_line,
                               thread_data->tot_line,
                               thread_data->out);
  return NULL;
}

static void *render_effect_execute_do_y_thread(void *thread_data_v)
{
  RenderGaussianBlurEffectThread *thread_data = (RenderGaussianBlurEffectThread *)thread_data_v;
  do_gaussian_blur_effect_y_cb(thread_data->context,
                               thread_data->seq,
                               thread_data->ibuf,
                               thread_data->start_line,
                               thread_data->tot_line,
                               thread_data->out);

  return NULL;
}

static ImBuf *do_gaussian_blur_effect(const SeqRenderData *context,
                                      Sequence *seq,
                                      float UNUSED(cfra),
                                      float UNUSED(facf0),
                                      float UNUSED(facf1),
                                      ImBuf *ibuf1,
                                      ImBuf *UNUSED(ibuf2),
                                      ImBuf *UNUSED(ibuf3))
{
  ImBuf *out = prepare_effect_imbufs(context, ibuf1, NULL, NULL);

  RenderGaussianBlurEffectInitData init_data;

  init_data.context = context;
  init_data.seq = seq;
  init_data.ibuf = ibuf1;
  init_data.out = out;

  IMB_processor_apply_threaded(out->y,
                               sizeof(RenderGaussianBlurEffectThread),
                               &init_data,
                               render_effect_execute_init_handle,
                               render_effect_execute_do_x_thread);

  ibuf1 = out;
  init_data.ibuf = ibuf1;
  out = prepare_effect_imbufs(context, ibuf1, NULL, NULL);
  init_data.out = out;

  IMB_processor_apply_threaded(out->y,
                               sizeof(RenderGaussianBlurEffectThread),
                               &init_data,
                               render_effect_execute_init_handle,
                               render_effect_execute_do_y_thread);

  IMB_freeImBuf(ibuf1);

  return out;
}

/*********************** text *************************/

static void init_text_effect(Sequence *seq)
{
  TextVars *data;

  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }

  data = seq->effectdata = MEM_callocN(sizeof(TextVars), "textvars");
  data->text_font = NULL;
  data->text_blf_id = -1;
  data->text_size = 30;

  copy_v4_fl(data->color, 1.0f);
  data->shadow_color[3] = 1.0f;

  BLI_strncpy(data->text, "Text", sizeof(data->text));

  data->loc[0] = 0.5f;
  data->align = SEQ_TEXT_ALIGN_X_CENTER;
  data->align_y = SEQ_TEXT_ALIGN_Y_BOTTOM;
}

void BKE_sequencer_text_font_unload(TextVars *data, const bool do_id_user)
{
  if (data) {
    /* Unlink the VFont */
    if (do_id_user && data->text_font != NULL) {
      id_us_min(&data->text_font->id);
      data->text_font = NULL;
    }

    /* Unload the BLF font. */
    if (data->text_blf_id >= 0) {
      BLF_unload_id(data->text_blf_id);
    }
  }
}

void BKE_sequencer_text_font_load(TextVars *data, const bool do_id_user)
{
  if (data->text_font != NULL) {
    if (do_id_user) {
      id_us_plus(&data->text_font->id);
    }

    char path[FILE_MAX];
    STRNCPY(path, data->text_font->filepath);
    BLI_assert(BLI_thread_is_main());
    BLI_path_abs(path, ID_BLEND_PATH_FROM_GLOBAL(&data->text_font->id));

    data->text_blf_id = BLF_load(path);
  }
}

static void free_text_effect(Sequence *seq, const bool do_id_user)
{
  TextVars *data = seq->effectdata;
  BKE_sequencer_text_font_unload(data, do_id_user);

  if (data) {
    MEM_freeN(data);
    seq->effectdata = NULL;
  }
}

static void load_text_effect(Sequence *seq)
{
  TextVars *data = seq->effectdata;
  BKE_sequencer_text_font_load(data, false);
}

static void copy_text_effect(Sequence *dst, Sequence *src, const int flag)
{
  dst->effectdata = MEM_dupallocN(src->effectdata);
  TextVars *data = dst->effectdata;

  data->text_blf_id = -1;
  BKE_sequencer_text_font_load(data, (flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0);
}

static int num_inputs_text(void)
{
  return 0;
}

static int early_out_text(Sequence *seq, float UNUSED(facf0), float UNUSED(facf1))
{
  TextVars *data = seq->effectdata;
  if (data->text[0] == 0 || data->text_size < 1 ||
      ((data->color[3] == 0.0f) &&
       (data->shadow_color[3] == 0.0f || (data->flag & SEQ_TEXT_SHADOW) == 0))) {
    return EARLY_USE_INPUT_1;
  }
  return EARLY_NO_INPUT;
}

static ImBuf *do_text_effect(const SeqRenderData *context,
                             Sequence *seq,
                             float UNUSED(cfra),
                             float UNUSED(facf0),
                             float UNUSED(facf1),
                             ImBuf *ibuf1,
                             ImBuf *ibuf2,
                             ImBuf *ibuf3)
{
  ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);
  TextVars *data = seq->effectdata;
  int width = out->x;
  int height = out->y;
  struct ColorManagedDisplay *display;
  const char *display_device;
  int font = blf_mono_font_render;
  int line_height;
  int y_ofs, x, y;
  double proxy_size_comp;

  if (data->text_blf_id == SEQ_FONT_NOT_LOADED) {
    data->text_blf_id = -1;

    if (data->text_font) {
      data->text_blf_id = BLF_load(data->text_font->filepath);
    }
  }

  if (data->text_blf_id >= 0) {
    font = data->text_blf_id;
  }

  display_device = context->scene->display_settings.display_device;
  display = IMB_colormanagement_display_get_named(display_device);

  /* Compensate text size for preview render size. */
  proxy_size_comp = context->scene->r.size / 100.0;
  if (context->preview_render_size != SEQ_PROXY_RENDER_SIZE_SCENE) {
    proxy_size_comp *= BKE_sequencer_rendersize_to_scale_factor(context->preview_render_size);
  }

  /* set before return */
  BLF_size(font, proxy_size_comp * data->text_size, 72);

  BLF_enable(font, BLF_WORD_WRAP);

  /* use max width to enable newlines only */
  BLF_wordwrap(font, (data->wrap_width != 0.0f) ? data->wrap_width * width : -1);

  BLF_buffer(
      font, out->rect_float, (unsigned char *)out->rect, width, height, out->channels, display);

  line_height = BLF_height_max(font);

  y_ofs = -BLF_descender(font);

  x = (data->loc[0] * width);
  y = (data->loc[1] * height) + y_ofs;

  if ((data->align == SEQ_TEXT_ALIGN_X_LEFT) && (data->align_y == SEQ_TEXT_ALIGN_Y_TOP)) {
    y -= line_height;
  }
  else {
    /* vars for calculating wordwrap */
    struct {
      struct ResultBLF info;
      rctf rect;
    } wrap;

    BLF_boundbox_ex(font, data->text, sizeof(data->text), &wrap.rect, &wrap.info);

    if (data->align == SEQ_TEXT_ALIGN_X_RIGHT) {
      x -= BLI_rctf_size_x(&wrap.rect);
    }
    else if (data->align == SEQ_TEXT_ALIGN_X_CENTER) {
      x -= BLI_rctf_size_x(&wrap.rect) / 2;
    }

    if (data->align_y == SEQ_TEXT_ALIGN_Y_TOP) {
      y -= line_height;
    }
    else if (data->align_y == SEQ_TEXT_ALIGN_Y_BOTTOM) {
      y += (wrap.info.lines - 1) * line_height;
    }
    else if (data->align_y == SEQ_TEXT_ALIGN_Y_CENTER) {
      y += (((wrap.info.lines - 1) / 2) * line_height) - (line_height / 2);
    }
  }

  /* BLF_SHADOW won't work with buffers, instead use cheap shadow trick */
  if (data->flag & SEQ_TEXT_SHADOW) {
    int fontx, fonty;
    fontx = BLF_width_max(font);
    fonty = line_height;
    BLF_position(font, x + max_ii(fontx / 25, 1), y + max_ii(fonty / 25, 1), 0.0f);
    BLF_buffer_col(font, data->shadow_color);
    BLF_draw_buffer(font, data->text, BLF_DRAW_STR_DUMMY_MAX);
  }

  BLF_position(font, x, y, 0.0f);
  BLF_buffer_col(font, data->color);
  BLF_draw_buffer(font, data->text, BLF_DRAW_STR_DUMMY_MAX);

  BLF_buffer(font, NULL, NULL, 0, 0, 0, NULL);

  BLF_disable(font, BLF_WORD_WRAP);

  return out;
}

/*********************** sequence effect factory *************************/

static void init_noop(Sequence *UNUSED(seq))
{
}

static void load_noop(Sequence *UNUSED(seq))
{
}

static void free_noop(Sequence *UNUSED(seq), const bool UNUSED(do_id_user))
{
}

static int num_inputs_default(void)
{
  return 2;
}

static void copy_effect_default(Sequence *dst, Sequence *src, const int UNUSED(flag))
{
  dst->effectdata = MEM_dupallocN(src->effectdata);
}

static void free_effect_default(Sequence *seq, const bool UNUSED(do_id_user))
{
  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }

  seq->effectdata = NULL;
}

static int early_out_noop(Sequence *UNUSED(seq), float UNUSED(facf0), float UNUSED(facf1))
{
  return EARLY_DO_EFFECT;
}

static int early_out_fade(Sequence *UNUSED(seq), float facf0, float facf1)
{
  if (facf0 == 0.0f && facf1 == 0.0f) {
    return EARLY_USE_INPUT_1;
  }
  else if (facf0 == 1.0f && facf1 == 1.0f) {
    return EARLY_USE_INPUT_2;
  }
  return EARLY_DO_EFFECT;
}

static int early_out_mul_input2(Sequence *UNUSED(seq), float facf0, float facf1)
{
  if (facf0 == 0.0f && facf1 == 0.0f) {
    return EARLY_USE_INPUT_1;
  }
  return EARLY_DO_EFFECT;
}

static void store_icu_yrange_noop(Sequence *UNUSED(seq),
                                  short UNUSED(adrcode),
                                  float *UNUSED(ymin),
                                  float *UNUSED(ymax))
{
  /* defaults are fine */
}

static void get_default_fac_noop(Sequence *UNUSED(seq),
                                 float UNUSED(cfra),
                                 float *facf0,
                                 float *facf1)
{
  *facf0 = *facf1 = 1.0;
}

static void get_default_fac_fade(Sequence *seq, float cfra, float *facf0, float *facf1)
{
  *facf0 = (float)(cfra - seq->startdisp);
  *facf1 = (float)(*facf0 + 0.5f);
  *facf0 /= seq->len;
  *facf1 /= seq->len;
}

static struct ImBuf *init_execution(const SeqRenderData *context,
                                    ImBuf *ibuf1,
                                    ImBuf *ibuf2,
                                    ImBuf *ibuf3)
{
  ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);

  return out;
}

static struct SeqEffectHandle get_sequence_effect_impl(int seq_type)
{
  struct SeqEffectHandle rval;
  int sequence_type = seq_type;

  rval.multithreaded = false;
  rval.supports_mask = false;
  rval.init = init_noop;
  rval.num_inputs = num_inputs_default;
  rval.load = load_noop;
  rval.free = free_noop;
  rval.early_out = early_out_noop;
  rval.get_default_fac = get_default_fac_noop;
  rval.store_icu_yrange = store_icu_yrange_noop;
  rval.execute = NULL;
  rval.init_execution = init_execution;
  rval.execute_slice = NULL;
  rval.copy = NULL;

  switch (sequence_type) {
    case SEQ_TYPE_CROSS:
      rval.multithreaded = true;
      rval.execute_slice = do_cross_effect;
      rval.early_out = early_out_fade;
      rval.get_default_fac = get_default_fac_fade;
      break;
    case SEQ_TYPE_GAMCROSS:
      rval.multithreaded = true;
      rval.init = init_gammacross;
      rval.load = load_gammacross;
      rval.free = free_gammacross;
      rval.early_out = early_out_fade;
      rval.get_default_fac = get_default_fac_fade;
      rval.init_execution = gammacross_init_execution;
      rval.execute_slice = do_gammacross_effect;
      break;
    case SEQ_TYPE_ADD:
      rval.multithreaded = true;
      rval.execute_slice = do_add_effect;
      rval.early_out = early_out_mul_input2;
      break;
    case SEQ_TYPE_SUB:
      rval.multithreaded = true;
      rval.execute_slice = do_sub_effect;
      rval.early_out = early_out_mul_input2;
      break;
    case SEQ_TYPE_MUL:
      rval.multithreaded = true;
      rval.execute_slice = do_mul_effect;
      rval.early_out = early_out_mul_input2;
      break;
    case SEQ_TYPE_SCREEN:
    case SEQ_TYPE_OVERLAY:
    case SEQ_TYPE_COLOR_BURN:
    case SEQ_TYPE_LINEAR_BURN:
    case SEQ_TYPE_DARKEN:
    case SEQ_TYPE_LIGHTEN:
    case SEQ_TYPE_DODGE:
    case SEQ_TYPE_SOFT_LIGHT:
    case SEQ_TYPE_HARD_LIGHT:
    case SEQ_TYPE_PIN_LIGHT:
    case SEQ_TYPE_LIN_LIGHT:
    case SEQ_TYPE_VIVID_LIGHT:
    case SEQ_TYPE_BLEND_COLOR:
    case SEQ_TYPE_HUE:
    case SEQ_TYPE_SATURATION:
    case SEQ_TYPE_VALUE:
    case SEQ_TYPE_DIFFERENCE:
    case SEQ_TYPE_EXCLUSION:
      rval.multithreaded = true;
      rval.execute_slice = do_blend_mode_effect;
      rval.early_out = early_out_mul_input2;
      break;
    case SEQ_TYPE_COLORMIX:
      rval.multithreaded = true;
      rval.init = init_colormix_effect;
      rval.free = free_effect_default;
      rval.copy = copy_effect_default;
      rval.execute_slice = do_colormix_effect;
      rval.early_out = early_out_mul_input2;
      break;
    case SEQ_TYPE_ALPHAOVER:
      rval.multithreaded = true;
      rval.init = init_alpha_over_or_under;
      rval.execute_slice = do_alphaover_effect;
      break;
    case SEQ_TYPE_OVERDROP:
      rval.multithreaded = true;
      rval.execute_slice = do_overdrop_effect;
      break;
    case SEQ_TYPE_ALPHAUNDER:
      rval.multithreaded = true;
      rval.init = init_alpha_over_or_under;
      rval.execute_slice = do_alphaunder_effect;
      break;
    case SEQ_TYPE_WIPE:
      rval.init = init_wipe_effect;
      rval.num_inputs = num_inputs_wipe;
      rval.free = free_wipe_effect;
      rval.copy = copy_wipe_effect;
      rval.early_out = early_out_fade;
      rval.get_default_fac = get_default_fac_fade;
      rval.execute = do_wipe_effect;
      break;
    case SEQ_TYPE_GLOW:
      rval.init = init_glow_effect;
      rval.num_inputs = num_inputs_glow;
      rval.free = free_glow_effect;
      rval.copy = copy_glow_effect;
      rval.execute = do_glow_effect;
      break;
    case SEQ_TYPE_TRANSFORM:
      rval.init = init_transform_effect;
      rval.num_inputs = num_inputs_transform;
      rval.free = free_transform_effect;
      rval.copy = copy_transform_effect;
      rval.execute = do_transform_effect;
      break;
    case SEQ_TYPE_SPEED:
      rval.init = init_speed_effect;
      rval.num_inputs = num_inputs_speed;
      rval.load = load_speed_effect;
      rval.free = free_speed_effect;
      rval.copy = copy_speed_effect;
      rval.execute = do_speed_effect;
      rval.early_out = early_out_speed;
      rval.store_icu_yrange = store_icu_yrange_speed;
      break;
    case SEQ_TYPE_COLOR:
      rval.init = init_solid_color;
      rval.num_inputs = num_inputs_color;
      rval.early_out = early_out_color;
      rval.free = free_solid_color;
      rval.copy = copy_solid_color;
      rval.execute = do_solid_color;
      break;
    case SEQ_TYPE_MULTICAM:
      rval.num_inputs = num_inputs_multicam;
      rval.early_out = early_out_multicam;
      rval.execute = do_multicam;
      break;
    case SEQ_TYPE_ADJUSTMENT:
      rval.supports_mask = true;
      rval.num_inputs = num_inputs_adjustment;
      rval.early_out = early_out_adjustment;
      rval.execute = do_adjustment;
      break;
    case SEQ_TYPE_GAUSSIAN_BLUR:
      rval.init = init_gaussian_blur_effect;
      rval.num_inputs = num_inputs_gaussian_blur;
      rval.free = free_gaussian_blur_effect;
      rval.copy = copy_gaussian_blur_effect;
      rval.early_out = early_out_gaussian_blur;
      rval.execute = do_gaussian_blur_effect;
      break;
    case SEQ_TYPE_TEXT:
      rval.num_inputs = num_inputs_text;
      rval.init = init_text_effect;
      rval.free = free_text_effect;
      rval.load = load_text_effect;
      rval.copy = copy_text_effect;
      rval.early_out = early_out_text;
      rval.execute = do_text_effect;
      break;
  }

  return rval;
}

struct SeqEffectHandle BKE_sequence_get_effect(Sequence *seq)
{
  struct SeqEffectHandle rval = {false, false, NULL};

  if (seq->type & SEQ_TYPE_EFFECT) {
    rval = get_sequence_effect_impl(seq->type);
    if ((seq->flag & SEQ_EFFECT_NOT_LOADED) != 0) {
      rval.load(seq);
      seq->flag &= ~SEQ_EFFECT_NOT_LOADED;
    }
  }

  return rval;
}

struct SeqEffectHandle BKE_sequence_get_blend(Sequence *seq)
{
  struct SeqEffectHandle rval = {false, false, NULL};

  if (seq->blend_mode != 0) {
    if ((seq->flag & SEQ_EFFECT_NOT_LOADED) != 0) {
      /* load the effect first */
      rval = get_sequence_effect_impl(seq->type);
      rval.load(seq);
    }

    rval = get_sequence_effect_impl(seq->blend_mode);
    if ((seq->flag & SEQ_EFFECT_NOT_LOADED) != 0) {
      /* now load the blend and unset unloaded flag */
      rval.load(seq);
      seq->flag &= ~SEQ_EFFECT_NOT_LOADED;
    }
  }

  return rval;
}

int BKE_sequence_effect_get_num_inputs(int seq_type)
{
  struct SeqEffectHandle rval = get_sequence_effect_impl(seq_type);

  int cnt = rval.num_inputs();
  if (rval.execute || (rval.execute_slice && rval.init_execution)) {
    return cnt;
  }
  return 0;
}

int BKE_sequence_effect_get_supports_mask(int seq_type)
{
  struct SeqEffectHandle rval = get_sequence_effect_impl(seq_type);

  return rval.supports_mask;
}
