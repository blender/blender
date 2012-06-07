/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Contributor(s): 
 * - Blender Foundation, 2003-2009
 * - Peter Schlaile <peter [at] schlaile [dot] de> 2005/2006
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/seqeffects.c
 *  \ingroup bke
 */


#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"
#include "BLI_dynlib.h"

#include "BLI_math.h" /* windows needs for M_PI */
#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_anim_types.h"

#include "BKE_fcurve.h"
#include "BKE_main.h"
#include "BKE_sequencer.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "RNA_access.h"

/* **** XXX **** */

/* Glow effect */
enum {
	GlowR = 0,
	GlowG = 1,
	GlowB = 2,
	GlowA = 3
};

static ImBuf *prepare_effect_imbufs(
        SeqRenderData context,
        struct ImBuf *ibuf1, struct ImBuf *ibuf2,
        struct ImBuf *ibuf3)
{
	struct ImBuf *out;
	int x = context.rectx;
	int y = context.recty;

	if (!ibuf1 && !ibuf2 && !ibuf3) {
		/* hmmm, global float option ? */
		out = IMB_allocImBuf((short)x, (short)y, 32, IB_rect);
	}
	else if ((ibuf1 && ibuf1->rect_float) ||
	         (ibuf2 && ibuf2->rect_float) ||
	         (ibuf3 && ibuf3->rect_float))
	{
		/* if any inputs are rectfloat, output is float too */

		out = IMB_allocImBuf((short)x, (short)y, 32, IB_rectfloat);
	}
	else {
		out = IMB_allocImBuf((short)x, (short)y, 32, IB_rect);
	}
	
	if (ibuf1 && !ibuf1->rect_float && out->rect_float) {
		IMB_float_from_rect_simple(ibuf1);
	}
	if (ibuf2 && !ibuf2->rect_float && out->rect_float) {
		IMB_float_from_rect_simple(ibuf2);
	}
	if (ibuf3 && !ibuf3->rect_float && out->rect_float) {
		IMB_float_from_rect_simple(ibuf3);
	}
	
	if (ibuf1 && !ibuf1->rect && !out->rect_float) {
		IMB_rect_from_float(ibuf1);
	}
	if (ibuf2 && !ibuf2->rect && !out->rect_float) {
		IMB_rect_from_float(ibuf2);
	}
	if (ibuf3 && !ibuf3->rect && !out->rect_float) {
		IMB_rect_from_float(ibuf3);
	}
			
	return out;
}

/* **********************************************************************
 * ALPHA OVER
 * ********************************************************************** */

static void init_alpha_over_or_under(Sequence *seq)
{
	Sequence *seq1 = seq->seq1;
	Sequence *seq2 = seq->seq2;

	seq->seq2 = seq1;
	seq->seq1 = seq2;
}

static void do_alphaover_effect_byte(float facf0, float facf1, int x, int y, 
                                     char *rect1, char *rect2, char *out)
{
	int fac2, mfac, fac, fac4;
	int xo, tempc;
	char *rt1, *rt2, *rt;

	xo = x;
	rt1 = (char *)rect1;
	rt2 = (char *)rect2;
	rt = (char *)out;

	fac2 = (int)(256.0f * facf0);
	fac4 = (int)(256.0f * facf1);

	while (y--) {

		x = xo;
		while (x--) {

			/* rt = rt1 over rt2  (alpha from rt1) */

			fac = fac2;
			mfac = 256 - ( (fac2 * rt1[3]) >> 8);

			if (fac == 0) *( (unsigned int *)rt) = *( (unsigned int *)rt2);
			else if (mfac == 0) *( (unsigned int *)rt) = *( (unsigned int *)rt1);
			else {
				tempc = (fac * rt1[0] + mfac * rt2[0]) >> 8;
				if (tempc > 255) rt[0] = 255; else rt[0] = tempc;
				tempc = (fac * rt1[1] + mfac * rt2[1]) >> 8;
				if (tempc > 255) rt[1] = 255; else rt[1] = tempc;
				tempc = (fac * rt1[2] + mfac * rt2[2]) >> 8;
				if (tempc > 255) rt[2] = 255; else rt[2] = tempc;
				tempc = (fac * rt1[3] + mfac * rt2[3]) >> 8;
				if (tempc > 255) rt[3] = 255; else rt[3] = tempc;
			}
			rt1 += 4; rt2 += 4; rt += 4;
		}

		if (y == 0) break;
		y--;

		x = xo;
		while (x--) {

			fac = fac4;
			mfac = 256 - ( (fac4 * rt1[3]) >> 8);

			if (fac == 0) *( (unsigned int *)rt) = *( (unsigned int *)rt2);
			else if (mfac == 0) *( (unsigned int *)rt) = *( (unsigned int *)rt1);
			else {
				tempc = (fac * rt1[0] + mfac * rt2[0]) >> 8;
				if (tempc > 255) rt[0] = 255; else rt[0] = tempc;
				tempc = (fac * rt1[1] + mfac * rt2[1]) >> 8;
				if (tempc > 255) rt[1] = 255; else rt[1] = tempc;
				tempc = (fac * rt1[2] + mfac * rt2[2]) >> 8;
				if (tempc > 255) rt[2] = 255; else rt[2] = tempc;
				tempc = (fac * rt1[3] + mfac * rt2[3]) >> 8;
				if (tempc > 255) rt[3] = 255; else rt[3] = tempc;
			}
			rt1 += 4; rt2 += 4; rt += 4;
		}
	}
}

static void do_alphaover_effect_float(float facf0, float facf1, int x, int y, 
                                      float *rect1, float *rect2, float *out)
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
			rt1 += 4; rt2 += 4; rt += 4;
		}

		if (y == 0) break;
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
			rt1 += 4; rt2 += 4; rt += 4;
		}
	}
}

static ImBuf *do_alphaover_effect(
        SeqRenderData context, Sequence *UNUSED(seq), float UNUSED(cfra),
        float facf0, float facf1,
        struct ImBuf *ibuf1, struct ImBuf *ibuf2,
        struct ImBuf *ibuf3)
{
	struct ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);

	if (out->rect_float) {
		do_alphaover_effect_float(
		        facf0, facf1, context.rectx, context.recty,
		        ibuf1->rect_float, ibuf2->rect_float,
		        out->rect_float);
	}
	else {
		do_alphaover_effect_byte(
		        facf0, facf1, context.rectx, context.recty,
		        (char *) ibuf1->rect, (char *) ibuf2->rect,
		        (char *) out->rect);
	}
	return out;
}


/* **********************************************************************
 * ALPHA UNDER
 * ********************************************************************** */

static void do_alphaunder_effect_byte(
        float facf0, float facf1, int x, int y, char *rect1,
        char *rect2, char *out)
{
	int fac2, mfac, fac, fac4;
	int xo;
	char *rt1, *rt2, *rt;

	xo = x;
	rt1 = rect1;
	rt2 = rect2;
	rt = out;

	fac2 = (int)(256.0f * facf0);
	fac4 = (int)(256.0f * facf1);

	while (y--) {

		x = xo;
		while (x--) {

			/* rt = rt1 under rt2  (alpha from rt2) */

			/* this complex optimalisation is because the
			 * 'skybuf' can be crossed in
			 */
			if (rt2[3] == 0 && fac2 == 256) *( (unsigned int *)rt) = *( (unsigned int *)rt1);
			else if (rt2[3] == 255) *( (unsigned int *)rt) = *( (unsigned int *)rt2);
			else {
				mfac = rt2[3];
				fac = (fac2 * (256 - mfac)) >> 8;

				if (fac == 0) *( (unsigned int *)rt) = *( (unsigned int *)rt2);
				else {
					rt[0] = (fac * rt1[0] + mfac * rt2[0]) >> 8;
					rt[1] = (fac * rt1[1] + mfac * rt2[1]) >> 8;
					rt[2] = (fac * rt1[2] + mfac * rt2[2]) >> 8;
					rt[3] = (fac * rt1[3] + mfac * rt2[3]) >> 8;
				}
			}
			rt1 += 4; rt2 += 4; rt += 4;
		}

		if (y == 0) break;
		y--;

		x = xo;
		while (x--) {

			if (rt2[3] == 0 && fac4 == 256) *( (unsigned int *)rt) = *( (unsigned int *)rt1);
			else if (rt2[3] == 255) *( (unsigned int *)rt) = *( (unsigned int *)rt2);
			else {
				mfac = rt2[3];
				fac = (fac4 * (256 - mfac)) >> 8;

				if (fac == 0) *( (unsigned int *)rt) = *( (unsigned int *)rt2);
				else {
					rt[0] = (fac * rt1[0] + mfac * rt2[0]) >> 8;
					rt[1] = (fac * rt1[1] + mfac * rt2[1]) >> 8;
					rt[2] = (fac * rt1[2] + mfac * rt2[2]) >> 8;
					rt[3] = (fac * rt1[3] + mfac * rt2[3]) >> 8;
				}
			}
			rt1 += 4; rt2 += 4; rt += 4;
		}
	}
}


static void do_alphaunder_effect_float(float facf0, float facf1, int x, int y, 
                                       float *rect1, float *rect2,
                                       float *out)
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

			/* rt = rt1 under rt2  (alpha from rt2) */

			/* this complex optimalisation is because the
			 * 'skybuf' can be crossed in
			 */
			if (rt2[3] <= 0 && fac2 >= 1.0f) {
				memcpy(rt, rt1, 4 * sizeof(float));
			}
			else if (rt2[3] >= 1.0f) {
				memcpy(rt, rt2, 4 * sizeof(float));
			}
			else {
				mfac = rt2[3];
				fac = fac2 * (1.0f - mfac);

				if (fac == 0) {
					memcpy(rt, rt2, 4 * sizeof(float));
				}
				else {
					rt[0] = fac * rt1[0] + mfac * rt2[0];
					rt[1] = fac * rt1[1] + mfac * rt2[1];
					rt[2] = fac * rt1[2] + mfac * rt2[2];
					rt[3] = fac * rt1[3] + mfac * rt2[3];
				}
			}
			rt1 += 4; rt2 += 4; rt += 4;
		}

		if (y == 0) break;
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
				mfac = rt2[3];
				fac = fac4 * (1.0f - mfac);

				if (fac == 0) {
					memcpy(rt, rt2, 4 * sizeof(float));
				}
				else {
					rt[0] = fac * rt1[0] + mfac * rt2[0];
					rt[1] = fac * rt1[1] + mfac * rt2[1];
					rt[2] = fac * rt1[2] + mfac * rt2[2];
					rt[3] = fac * rt1[3] + mfac * rt2[3];
				}
			}
			rt1 += 4; rt2 += 4; rt += 4;
		}
	}
}

static ImBuf *do_alphaunder_effect(
        SeqRenderData context, Sequence *UNUSED(seq), float UNUSED(cfra),
        float facf0, float facf1,
        struct ImBuf *ibuf1, struct ImBuf *ibuf2,
        struct ImBuf *ibuf3)
{
	struct ImBuf *out = prepare_effect_imbufs(
	        context, ibuf1, ibuf2, ibuf3);

	if (out->rect_float) {
		do_alphaunder_effect_float(
		        facf0, facf1, context.rectx, context.recty,
		        ibuf1->rect_float, ibuf2->rect_float,
		        out->rect_float);
	}
	else {
		do_alphaunder_effect_byte(
		        facf0, facf1, context.rectx, context.recty,
		        (char *) ibuf1->rect, (char *) ibuf2->rect,
		        (char *) out->rect);
	}
	return out;
}


/* **********************************************************************
 * CROSS
 * ********************************************************************** */

static void do_cross_effect_byte(float facf0, float facf1, int x, int y, 
                                 char *rect1, char *rect2,
                                 char *out)
{
	int fac1, fac2, fac3, fac4;
	int xo;
	char *rt1, *rt2, *rt;

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

			rt1 += 4; rt2 += 4; rt += 4;
		}

		if (y == 0) break;
		y--;

		x = xo;
		while (x--) {

			rt[0] = (fac3 * rt1[0] + fac4 * rt2[0]) >> 8;
			rt[1] = (fac3 * rt1[1] + fac4 * rt2[1]) >> 8;
			rt[2] = (fac3 * rt1[2] + fac4 * rt2[2]) >> 8;
			rt[3] = (fac3 * rt1[3] + fac4 * rt2[3]) >> 8;

			rt1 += 4; rt2 += 4; rt += 4;
		}

	}
}

static void do_cross_effect_float(float facf0, float facf1, int x, int y, 
                                  float *rect1, float *rect2, float *out)
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

			rt1 += 4; rt2 += 4; rt += 4;
		}

		if (y == 0) break;
		y--;

		x = xo;
		while (x--) {

			rt[0] = fac3 * rt1[0] + fac4 * rt2[0];
			rt[1] = fac3 * rt1[1] + fac4 * rt2[1];
			rt[2] = fac3 * rt1[2] + fac4 * rt2[2];
			rt[3] = fac3 * rt1[3] + fac4 * rt2[3];

			rt1 += 4; rt2 += 4; rt += 4;
		}

	}
}

/* careful: also used by speed effect! */

static ImBuf *do_cross_effect(
        SeqRenderData context, Sequence *UNUSED(seq), float UNUSED(cfra),
        float facf0, float facf1,
        struct ImBuf *ibuf1, struct ImBuf *ibuf2,
        struct ImBuf *ibuf3)
{
	struct ImBuf *out = prepare_effect_imbufs(
	        context, ibuf1, ibuf2, ibuf3);

	if (out->rect_float) {
		do_cross_effect_float(
		        facf0, facf1, context.rectx, context.recty,
		        ibuf1->rect_float, ibuf2->rect_float,
		        out->rect_float);
	}
	else {
		do_cross_effect_byte(
		        facf0, facf1, context.rectx, context.recty,
		        (char *) ibuf1->rect, (char *) ibuf2->rect,
		        (char *) out->rect);
	}
	return out;
}


/* **********************************************************************
 * GAMMA CROSS
 * ********************************************************************** */

/* copied code from initrender.c */
static unsigned short gamtab[65536];
static unsigned short igamtab1[256];
static int gamma_tabs_init = FALSE;

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

	valid_gamma        = gamma;
	valid_inv_gamma    = 1.0f / gamma;
	color_step        = 1.0f / RE_GAMMA_TABLE_SIZE;
	inv_color_step    = (float) RE_GAMMA_TABLE_SIZE; 

	/* We could squeeze out the two range tables to gain some memory.        */	
	for (i = 0; i < RE_GAMMA_TABLE_SIZE; i++) {
		color_domain_table[i]   = i * color_step;
		gamma_range_table[i]     = pow(color_domain_table[i],
		                               valid_gamma);
		inv_gamma_range_table[i] = pow(color_domain_table[i],
		                               valid_inv_gamma);
	}

	/* The end of the table should match 1.0 carefully. In order to avoid    */
	/* rounding errors, we just set this explicitly. The last segment may    */
	/* have a different length than the other segments, but our              */
	/* interpolation is insensitive to that.                                 */
	color_domain_table[RE_GAMMA_TABLE_SIZE]   = 1.0;
	gamma_range_table[RE_GAMMA_TABLE_SIZE]     = 1.0;
	inv_gamma_range_table[RE_GAMMA_TABLE_SIZE] = 1.0;

	/* To speed up calculations, we make these calc factor tables. They are  */
	/* multiplication factors used in scaling the interpolation.             */
	for (i = 0; i < RE_GAMMA_TABLE_SIZE; i++) {
		gamfactor_table[i] = inv_color_step *
		                     (gamma_range_table[i + 1] - gamma_range_table[i]);
		inv_gamfactor_table[i] = inv_color_step *
		                         (inv_gamma_range_table[i + 1] - inv_gamma_range_table[i]);
	}

} /* end of void makeGammaTables(float gamma) */


static float gammaCorrect(float c)
{
	int i;
	float res = 0.0;
	
	i = floor(c * inv_color_step);
	/* Clip to range [0, 1]: outside, just do the complete calculation.       */
	/* We may have some performance problems here. Stretching up the LUT     */
	/* may help solve that, by exchanging LUT size for the interpolation.    */
	/* Negative colors are explicitly handled.                              */
	if (i < 0) res = -pow(abs(c), valid_gamma);
	else if (i >= RE_GAMMA_TABLE_SIZE) res = pow(c, valid_gamma);
	else res = gamma_range_table[i] + 
		       ( (c - color_domain_table[i]) * gamfactor_table[i]);
	
	return res;
} /* end of float gammaCorrect(float col) */

/* ------------------------------------------------------------------------- */

static float invGammaCorrect(float col)
{
	int i;
	float res = 0.0;

	i = floor(col * inv_color_step);
	/* Negative colors are explicitly handled.                              */
	if (i < 0) res = -pow(abs(col), valid_inv_gamma);
	else if (i >= RE_GAMMA_TABLE_SIZE) res = pow(col, valid_inv_gamma);
	else res = inv_gamma_range_table[i] + 
		       ( (col - color_domain_table[i]) * inv_gamfactor_table[i]);
 
	return res;
} /* end of float invGammaCorrect(float col) */


static void gamtabs(float gamma)
{
	float val, igamma = 1.0f / gamma;
	int a;
	
	/* gamtab: in short, out short */
	for (a = 0; a < 65536; a++) {
		val = a;
		val /= 65535.0f;
		
		if (gamma == 2.0f) val = sqrt(val);
		else if (gamma != 1.0f) val = pow(val, igamma);
		
		gamtab[a] = (65535.99f * val);
	}
	/* inverse gamtab1 : in byte, out short */
	for (a = 1; a <= 256; a++) {
		if (gamma == 2.0f) igamtab1[a - 1] = a * a - 1;
		else if (gamma == 1.0f) igamtab1[a - 1] = 256 * a - 1;
		else {
			val = a / 256.0f;
			igamtab1[a - 1] = (65535.0 * pow(val, gamma)) - 1;
		}
	}

}

static void build_gammatabs(void)
{
	if (gamma_tabs_init == FALSE) {
		gamtabs(2.0f);
		makeGammaTables(2.0f);
		gamma_tabs_init = TRUE;
	}
}

static void init_gammacross(Sequence *UNUSED(seq))
{
}

static void load_gammacross(Sequence *UNUSED(seq))
{
}

static void free_gammacross(Sequence *UNUSED(seq))
{
}

static void do_gammacross_effect_byte(float facf0, float UNUSED(facf1), 
                                      int x, int y,
                                      unsigned char *rect1,
                                      unsigned char *rect2,
                                      unsigned char *out)
{
	int fac1, fac2, col;
	int xo;
	unsigned char *rt1, *rt2, *rt;
	
	xo = x;
	rt1 = (unsigned char *)rect1;
	rt2 = (unsigned char *)rect2;
	rt = (unsigned char *)out;

	fac2 = (int)(256.0f * facf0);
	fac1 = 256 - fac2;

	while (y--) {

		x = xo;
		while (x--) {

			col = (fac1 * igamtab1[rt1[0]] + fac2 * igamtab1[rt2[0]]) >> 8;
			if (col > 65535) rt[0] = 255; else rt[0] = ( (char *)(gamtab + col))[MOST_SIG_BYTE];
			col = (fac1 * igamtab1[rt1[1]] + fac2 * igamtab1[rt2[1]]) >> 8;
			if (col > 65535) rt[1] = 255; else rt[1] = ( (char *)(gamtab + col))[MOST_SIG_BYTE];
			col = (fac1 * igamtab1[rt1[2]] + fac2 * igamtab1[rt2[2]]) >> 8;
			if (col > 65535) rt[2] = 255; else rt[2] = ( (char *)(gamtab + col))[MOST_SIG_BYTE];
			col = (fac1 * igamtab1[rt1[3]] + fac2 * igamtab1[rt2[3]]) >> 8;
			if (col > 65535) rt[3] = 255; else rt[3] = ( (char *)(gamtab + col))[MOST_SIG_BYTE];

			rt1 += 4; rt2 += 4; rt += 4;
		}

		if (y == 0) break;
		y--;

		x = xo;
		while (x--) {

			col = (fac1 * igamtab1[rt1[0]] + fac2 * igamtab1[rt2[0]]) >> 8;
			if (col > 65535) rt[0] = 255; else rt[0] = ( (char *)(gamtab + col))[MOST_SIG_BYTE];
			col = (fac1 * igamtab1[rt1[1]] + fac2 * igamtab1[rt2[1]]) >> 8;
			if (col > 65535) rt[1] = 255; else rt[1] = ( (char *)(gamtab + col))[MOST_SIG_BYTE];
			col = (fac1 * igamtab1[rt1[2]] + fac2 * igamtab1[rt2[2]]) >> 8;
			if (col > 65535) rt[2] = 255; else rt[2] = ( (char *)(gamtab + col))[MOST_SIG_BYTE];
			col = (fac1 * igamtab1[rt1[3]] + fac2 * igamtab1[rt2[3]]) >> 8;
			if (col > 65535) rt[3] = 255; else rt[3] = ( (char *)(gamtab + col))[MOST_SIG_BYTE];

			rt1 += 4; rt2 += 4; rt += 4;
		}
	}

}

static void do_gammacross_effect_float(float facf0, float UNUSED(facf1), 
                                       int x, int y,
                                       float *rect1, float *rect2,
                                       float *out)
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

			*rt = gammaCorrect(
			    fac1 * invGammaCorrect(*rt1) + fac2 * invGammaCorrect(*rt2));
			rt1++; rt2++; rt++;
		}

		if (y == 0) break;
		y--;

		x = xo * 4;
		while (x--) {

			*rt = gammaCorrect(
			    fac1 * invGammaCorrect(*rt1) + fac2 * invGammaCorrect(*rt2));

			rt1++; rt2++; rt++;
		}
	}
}

static ImBuf *do_gammacross_effect(
        SeqRenderData context,
        Sequence *UNUSED(seq), float UNUSED(cfra),
        float facf0, float facf1,
        struct ImBuf *ibuf1, struct ImBuf *ibuf2,
        struct ImBuf *ibuf3)
{
	struct ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);

	build_gammatabs();

	if (out->rect_float) {
		do_gammacross_effect_float(
		        facf0, facf1, context.rectx, context.recty,
		        ibuf1->rect_float, ibuf2->rect_float,
		        out->rect_float);
	}
	else {
		do_gammacross_effect_byte(
		        facf0, facf1, context.rectx, context.recty,
		        (unsigned char *) ibuf1->rect, (unsigned char *) ibuf2->rect,
		        (unsigned char *) out->rect);
	}
	return out;
}


/* **********************************************************************
 * ADD
 * ********************************************************************** */

static void do_add_effect_byte(float facf0, float facf1, int x, int y, 
                               unsigned char *rect1, unsigned char *rect2,
                               unsigned char *out)
{
	int col, xo, fac1, fac3;
	char *rt1, *rt2, *rt;

	xo = x;
	rt1 = (char *)rect1;
	rt2 = (char *)rect2;
	rt = (char *)out;

	fac1 = (int)(256.0f * facf0);
	fac3 = (int)(256.0f * facf1);

	while (y--) {

		x = xo;
		while (x--) {

			col = rt1[0] + ((fac1 * rt2[0]) >> 8);
			if (col > 255) rt[0] = 255; else rt[0] = col;
			col = rt1[1] + ((fac1 * rt2[1]) >> 8);
			if (col > 255) rt[1] = 255; else rt[1] = col;
			col = rt1[2] + ((fac1 * rt2[2]) >> 8);
			if (col > 255) rt[2] = 255; else rt[2] = col;
			col = rt1[3] + ((fac1 * rt2[3]) >> 8);
			if (col > 255) rt[3] = 255; else rt[3] = col;

			rt1 += 4; rt2 += 4; rt += 4;
		}

		if (y == 0) break;
		y--;

		x = xo;
		while (x--) {

			col = rt1[0] + ((fac3 * rt2[0]) >> 8);
			if (col > 255) rt[0] = 255; else rt[0] = col;
			col = rt1[1] + ((fac3 * rt2[1]) >> 8);
			if (col > 255) rt[1] = 255; else rt[1] = col;
			col = rt1[2] + ((fac3 * rt2[2]) >> 8);
			if (col > 255) rt[2] = 255; else rt[2] = col;
			col = rt1[3] + ((fac3 * rt2[3]) >> 8);
			if (col > 255) rt[3] = 255; else rt[3] = col;

			rt1 += 4; rt2 += 4; rt += 4;
		}
	}
}

static void do_add_effect_float(float facf0, float facf1, int x, int y, 
                                float *rect1, float *rect2,
                                float *out)
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

		x = xo * 4;
		while (x--) {
			*rt = *rt1 + fac1 * (*rt2);

			rt1++; rt2++; rt++;
		}

		if (y == 0) break;
		y--;

		x = xo * 4;
		while (x--) {
			*rt = *rt1 + fac3 * (*rt2);

			rt1++; rt2++; rt++;
		}
	}
}

static ImBuf *do_add_effect(SeqRenderData context,
                            Sequence *UNUSED(seq), float UNUSED(cfra),
                            float facf0, float facf1,
                            struct ImBuf *ibuf1, struct ImBuf *ibuf2,
                            struct ImBuf *ibuf3)
{
	struct ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);

	if (out->rect_float) {
		do_add_effect_float(
		        facf0, facf1, context.rectx, context.recty,
		        ibuf1->rect_float, ibuf2->rect_float,
		        out->rect_float);
	}
	else {
		do_add_effect_byte(
		        facf0, facf1, context.rectx, context.recty,
		        (unsigned char *) ibuf1->rect, (unsigned char *) ibuf2->rect,
		        (unsigned char *) out->rect);
	}
	return out;
}


/* **********************************************************************
 * SUB
 * ********************************************************************** */

static void do_sub_effect_byte(float facf0, float facf1, 
                               int x, int y,
                               char *rect1, char *rect2, char *out)
{
	int col, xo, fac1, fac3;
	char *rt1, *rt2, *rt;

	xo = x;
	rt1 = (char *)rect1;
	rt2 = (char *)rect2;
	rt = (char *)out;

	fac1 = (int)(256.0f * facf0);
	fac3 = (int)(256.0f * facf1);

	while (y--) {

		x = xo;
		while (x--) {

			col = rt1[0] - ((fac1 * rt2[0]) >> 8);
			if (col < 0) rt[0] = 0; else rt[0] = col;
			col = rt1[1] - ((fac1 * rt2[1]) >> 8);
			if (col < 0) rt[1] = 0; else rt[1] = col;
			col = rt1[2] - ((fac1 * rt2[2]) >> 8);
			if (col < 0) rt[2] = 0; else rt[2] = col;
			col = rt1[3] - ((fac1 * rt2[3]) >> 8);
			if (col < 0) rt[3] = 0; else rt[3] = col;

			rt1 += 4; rt2 += 4; rt += 4;
		}

		if (y == 0) break;
		y--;

		x = xo;
		while (x--) {

			col = rt1[0] - ((fac3 * rt2[0]) >> 8);
			if (col < 0) rt[0] = 0; else rt[0] = col;
			col = rt1[1] - ((fac3 * rt2[1]) >> 8);
			if (col < 0) rt[1] = 0; else rt[1] = col;
			col = rt1[2] - ((fac3 * rt2[2]) >> 8);
			if (col < 0) rt[2] = 0; else rt[2] = col;
			col = rt1[3] - ((fac3 * rt2[3]) >> 8);
			if (col < 0) rt[3] = 0; else rt[3] = col;

			rt1 += 4; rt2 += 4; rt += 4;
		}
	}
}

static void do_sub_effect_float(float facf0, float facf1, int x, int y, 
                                float *rect1, float *rect2,
                                float *out)
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

		x = xo * 4;
		while (x--) {
			*rt = *rt1 - fac1 * (*rt2);

			rt1++; rt2++; rt++;
		}

		if (y == 0) break;
		y--;

		x = xo * 4;
		while (x--) {
			*rt = *rt1 - fac3 * (*rt2);

			rt1++; rt2++; rt++;
		}
	}
}

static ImBuf *do_sub_effect(
        SeqRenderData context, Sequence *UNUSED(seq), float UNUSED(cfra),
        float facf0, float facf1,
        struct ImBuf *ibuf1, struct ImBuf *ibuf2,
        struct ImBuf *ibuf3)
{
	struct ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);

	if (out->rect_float) {
		do_sub_effect_float(
		    facf0, facf1, context.rectx, context.recty,
		    ibuf1->rect_float, ibuf2->rect_float,
		    out->rect_float);
	}
	else {
		do_sub_effect_byte(
		    facf0, facf1, context.rectx, context.recty,
		    (char *) ibuf1->rect, (char *) ibuf2->rect,
		    (char *) out->rect);
	}
	return out;
}

/* **********************************************************************
 * DROP
 * ********************************************************************** */

/* Must be > 0 or add precopy, etc to the function */
#define XOFF    8
#define YOFF    8

static void do_drop_effect_byte(float facf0, float facf1, int x, int y, 
                                char *rect2i, char *rect1i,
                                char *outi)
{
	int height, width, temp, fac, fac1, fac2;
	char *rt1, *rt2, *out;
	int field = 1;

	width = x;
	height = y;

	fac1 = (int)(70.0f * facf0);
	fac2 = (int)(70.0f * facf1);

	rt2 = (char *) (rect2i + YOFF * width);
	rt1 = (char *) rect1i;
	out = (char *) outi;
	for (y = 0; y < height - YOFF; y++) {
		if (field) fac = fac1;
		else fac = fac2;
		field = !field;

		memcpy(out, rt1, sizeof(int) * XOFF);
		rt1 += XOFF * 4;
		out += XOFF * 4;

		for (x = XOFF; x < width; x++) {
			temp = ((fac * rt2[3]) >> 8);

			*(out++) = MAX2(0, *rt1 - temp); rt1++;
			*(out++) = MAX2(0, *rt1 - temp); rt1++;
			*(out++) = MAX2(0, *rt1 - temp); rt1++;
			*(out++) = MAX2(0, *rt1 - temp); rt1++;
			rt2 += 4;
		}
		rt2 += XOFF * 4;
	}
	memcpy(out, rt1, sizeof(int) * YOFF * width);
}

static void do_drop_effect_float(float facf0, float facf1, int x, int y, 
                                 float *rect2i, float *rect1i,
                                 float *outi)
{
	int height, width;
	float temp, fac, fac1, fac2;
	float *rt1, *rt2, *out;
	int field = 1;

	width = x;
	height = y;

	fac1 = 70.0f * facf0;
	fac2 = 70.0f * facf1;

	rt2 =  (rect2i + YOFF * width);
	rt1 =  rect1i;
	out =  outi;
	for (y = 0; y < height - YOFF; y++) {
		if (field) fac = fac1;
		else fac = fac2;
		field = !field;

		memcpy(out, rt1, 4 * sizeof(float) * XOFF);
		rt1 += XOFF * 4;
		out += XOFF * 4;

		for (x = XOFF; x < width; x++) {
			temp = fac * rt2[3];

			*(out++) = MAX2(0.0f, *rt1 - temp); rt1++;
			*(out++) = MAX2(0.0f, *rt1 - temp); rt1++;
			*(out++) = MAX2(0.0f, *rt1 - temp); rt1++;
			*(out++) = MAX2(0.0f, *rt1 - temp); rt1++;
			rt2 += 4;
		}
		rt2 += XOFF * 4;
	}
	memcpy(out, rt1, 4 * sizeof(float) * YOFF * width);
}

/* **********************************************************************
 * MUL
 * ********************************************************************** */

static void do_mul_effect_byte(float facf0, float facf1, int x, int y, 
                               unsigned char *rect1, unsigned char *rect2,
                               unsigned char *out)
{
	int xo, fac1, fac3;
	char *rt1, *rt2, *rt;

	xo = x;
	rt1 = (char *)rect1;
	rt2 = (char *)rect2;
	rt = (char *)out;

	fac1 = (int)(256.0f * facf0);
	fac3 = (int)(256.0f * facf1);

	/* formula:
	 *		fac * (a * b) + (1-fac)*a  => fac * a * (b - 1) + axaux = c * px + py * s; //+centx
	 *		yaux = -s * px + c * py; //+centy
	 */

	while (y--) {

		x = xo;
		while (x--) {

			rt[0] = rt1[0] + ((fac1 * rt1[0] * (rt2[0] - 256)) >> 16);
			rt[1] = rt1[1] + ((fac1 * rt1[1] * (rt2[1] - 256)) >> 16);
			rt[2] = rt1[2] + ((fac1 * rt1[2] * (rt2[2] - 256)) >> 16);
			rt[3] = rt1[3] + ((fac1 * rt1[3] * (rt2[3] - 256)) >> 16);

			rt1 += 4; rt2 += 4; rt += 4;
		}

		if (y == 0) break;
		y--;

		x = xo;
		while (x--) {

			rt[0] = rt1[0] + ((fac3 * rt1[0] * (rt2[0] - 256)) >> 16);
			rt[1] = rt1[1] + ((fac3 * rt1[1] * (rt2[1] - 256)) >> 16);
			rt[2] = rt1[2] + ((fac3 * rt1[2] * (rt2[2] - 256)) >> 16);
			rt[3] = rt1[3] + ((fac3 * rt1[3] * (rt2[3] - 256)) >> 16);

			rt1 += 4; rt2 += 4; rt += 4;
		}
	}
}

static void do_mul_effect_float(float facf0, float facf1, int x, int y, 
                                float *rect1, float *rect2,
                                float *out)
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
	 *		fac*(a*b) + (1-fac)*a  => fac*a*(b-1)+a
	 */

	while (y--) {

		x = xo;
		while (x--) {

			rt[0] = rt1[0] + fac1 * rt1[0] * (rt2[0] - 1.0f);
			rt[1] = rt1[1] + fac1 * rt1[1] * (rt2[1] - 1.0f);
			rt[2] = rt1[2] + fac1 * rt1[2] * (rt2[2] - 1.0f);
			rt[3] = rt1[3] + fac1 * rt1[3] * (rt2[3] - 1.0f);

			rt1 += 4; rt2 += 4; rt += 4;
		}

		if (y == 0) break;
		y--;

		x = xo;
		while (x--) {

			rt[0] = rt1[0] + fac3 * rt1[0] * (rt2[0] - 1.0f);
			rt[1] = rt1[1] + fac3 * rt1[1] * (rt2[1] - 1.0f);
			rt[2] = rt1[2] + fac3 * rt1[2] * (rt2[2] - 1.0f);
			rt[3] = rt1[3] + fac3 * rt1[3] * (rt2[3] - 1.0f);

			rt1 += 4; rt2 += 4; rt += 4;
		}
	}
}

static ImBuf *do_mul_effect(
        SeqRenderData context, Sequence *UNUSED(seq), float UNUSED(cfra),
        float facf0, float facf1,
        struct ImBuf *ibuf1, struct ImBuf *ibuf2,
        struct ImBuf *ibuf3)
{
	struct ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);

	if (out->rect_float) {
		do_mul_effect_float(
		        facf0, facf1, context.rectx, context.recty,
		        ibuf1->rect_float, ibuf2->rect_float,
		        out->rect_float);
	}
	else {
		do_mul_effect_byte(
		        facf0, facf1, context.rectx, context.recty,
		        (unsigned char *) ibuf1->rect, (unsigned char *) ibuf2->rect,
		        (unsigned char *) out->rect);
	}

	return out;
}

/* **********************************************************************
 * WIPE
 * ********************************************************************** */

typedef struct WipeZone {
	float angle;
	int flip;
	int xo, yo;
	int width;
	float pythangle;
} WipeZone;

static void precalc_wipe_zone(WipeZone *wipezone, WipeVars *wipe, int xo, int yo)
{
	wipezone->flip = (wipe->angle < 0);
	wipezone->angle = tanf(DEG2RADF(fabsf(wipe->angle)));
	wipezone->xo = xo;
	wipezone->yo = yo;
	wipezone->width = (int)(wipe->edgeWidth * ((xo + yo) / 2.0f));
	wipezone->pythangle = 1.0f / sqrtf(wipezone->angle * wipezone->angle + 1.0f);
}

// This function calculates the blur band for the wipe effects
static float in_band(float width, float dist, int side, int dir)
{
	float alpha;

	if (width == 0)
		return (float)side;

	if (width < dist)
		return (float)side;

	if (side == 1)
		alpha = (dist + 0.5f * width) / (width);
	else
		alpha = (0.5f * width - dist) / (width);

	if (dir == 0)
		alpha = 1 - alpha;

	return alpha;
}

static float check_zone(WipeZone *wipezone, int x, int y,
                        Sequence *seq, float facf0)
{
	float posx, posy, hyp, hyp2, angle, hwidth, b1, b2, b3, pointdist;
	/* some future stuff */
	// float hyp3, hyp4, b4, b5
	float temp1, temp2, temp3, temp4; //some placeholder variables
	int xo = wipezone->xo;
	int yo = wipezone->yo;
	float halfx = xo * 0.5f;
	float halfy = yo * 0.5f;
	float widthf, output = 0;
	WipeVars *wipe = (WipeVars *)seq->effectdata;
	int width;

	if (wipezone->flip) x = xo - x;
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
			width = wipezone->width;

			if (angle == 0.0f) {
				b1 = posy;
				b2 = y;
				hyp = fabs(y - posy);
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
				if (b1 < b2)
					output = in_band(width, hyp, 1, 1);
				else
					output = in_band(width, hyp, 0, 1);
			}
			else {
				if (b1 < b2)
					output = in_band(width, hyp, 0, 1);
				else
					output = in_band(width, hyp, 1, 1);
			}
			break;

		case DO_DOUBLE_WIPE:
			if (!wipe->forward)
				facf0 = 1.0f - facf0;  // Go the other direction

			width = wipezone->width;  // calculate the blur width
			hwidth = width * 0.5f;
			if (angle == 0) {
				b1 = posy * 0.5f;
				b3 = yo - posy * 0.5f;
				b2 = y;

				hyp = abs(y - posy * 0.5f);
				hyp2 = abs(y - (yo - posy * 0.5f));
			}
			else {
				b1 = posy * 0.5f - (-angle) * posx * 0.5f;
				b3 = (yo - posy * 0.5f) - (-angle) * (xo - posx * 0.5f);
				b2 = y - (-angle) * x;

				hyp = fabsf(angle * x + y + (-posy * 0.5f - angle * posx * 0.5f)) * wipezone->pythangle;
				hyp2 = fabsf(angle * x + y + (-(yo - posy * 0.5f) - angle * (xo - posx * 0.5f))) * wipezone->pythangle;
			}

			hwidth = minf(hwidth, fabsf(b3 - b1) / 2.0f);

			if (b2 < b1 && b2 < b3) {
				output = in_band(hwidth, hyp, 0, 1);
			}
			else if (b2 > b1 && b2 > b3) {
				output = in_band(hwidth, hyp2, 0, 1);
			}
			else {
				if (hyp < hwidth && hyp2 > hwidth)
					output = in_band(hwidth, hyp, 1, 1);
				else if (hyp > hwidth && hyp2 < hwidth)
					output = in_band(hwidth, hyp2, 1, 1);
				else
					output = in_band(hwidth, hyp2, 1, 1) * in_band(hwidth, hyp, 1, 1);
			}
			if (!wipe->forward) output = 1 - output;
			break;
		case DO_CLOCK_WIPE:
			/*
			 *  temp1: angle of effect center in rads
			 *  temp2: angle of line through (halfx, halfy) and (x, y) in rads
			 *  temp3: angle of low side of blur
			 *  temp4: angle of high side of blur
			 */
			output = 1.0f - facf0;
			widthf = wipe->edgeWidth * 2.0f * (float)M_PI;
			temp1 = 2.0f * (float)M_PI * facf0;

			if (wipe->forward) {
				temp1 = 2.0f * (float)M_PI - temp1;
			}

			x = x - halfx;
			y = y - halfy;

			temp2 = asin(abs(y) / sqrt(x * x + y * y));
			if (x <= 0 && y >= 0) temp2 = (float)M_PI - temp2;
			else if (x <= 0 && y <= 0) temp2 += (float)M_PI;
			else if (x >= 0 && y <= 0) temp2 = 2.0f * (float)M_PI - temp2;

			if (wipe->forward) {
				temp3 = temp1 - (widthf * 0.5f) * facf0;
				temp4 = temp1 + (widthf * 0.5f) * (1 - facf0);
			}
			else {
				temp3 = temp1 - (widthf * 0.5f) * (1 - facf0);
				temp4 = temp1 + (widthf * 0.5f) * facf0;
			}
			if (temp3 < 0) temp3 = 0;
			if (temp4 > 2.0f * (float)M_PI) temp4 = 2.0f * (float)M_PI;


			if (temp2 < temp3) output = 0;
			else if (temp2 > temp4) output = 1;
			else output = (temp2 - temp3) / (temp4 - temp3);
			if (x == 0 && y == 0) output = 1;
			if (output != output) output = 1;
			if (wipe->forward) output = 1 - output;
			break;
			/* BOX WIPE IS NOT WORKING YET */
			/* case DO_CROSS_WIPE: */
			/* BOX WIPE IS NOT WORKING YET */
#if 0
		case DO_BOX_WIPE: 
			if (invert) facf0 = 1 - facf0;

			width = (int)(wipe->edgeWidth * ((xo + yo) / 2.0));
			hwidth = (float)width / 2.0;
			if (angle == 0) angle = 0.000001;
			b1 = posy / 2 - (-angle) * posx / 2;
			b3 = (yo - posy / 2) - (-angle) * (xo - posx / 2);
			b2 = y - (-angle) * x;

			hyp = abs(angle * x + y + (-posy / 2 - angle * posx / 2)) * wipezone->pythangle;
			hyp2 = abs(angle * x + y + (-(yo - posy / 2) - angle * (xo - posx / 2))) * wipezone->pythangle;

			temp1 = xo * (1 - facf0 / 2) - xo * facf0 / 2;
			temp2 = yo * (1 - facf0 / 2) - yo * facf0 / 2;
			pointdist = sqrt(temp1 * temp1 + temp2 * temp2);

			if (b2 < b1 && b2 < b3) {
				if (hwidth < pointdist)
					output = in_band(wipezone, hwidth, hyp, facf0, 0, 1);
			}
			else if (b2 > b1 && b2 > b3) {
				if (hwidth < pointdist)
					output = in_band(wipezone, hwidth, hyp2, facf0, 0, 1);
			}
			else {
				if (hyp < hwidth && hyp2 > hwidth)
					output = in_band(wipezone, hwidth, hyp, facf0, 1, 1);
				else if (hyp > hwidth && hyp2 < hwidth)
					output = in_band(wipezone, hwidth, hyp2, facf0, 1, 1);
				else
					output = in_band(wipezone, hwidth, hyp2, facf0, 1, 1) * in_band(wipezone, hwidth, hyp, facf0, 1, 1);
			}

			if (invert) facf0 = 1 - facf0;
			angle = -1 / angle;
			b1 = posy / 2 - (-angle) * posx / 2;
			b3 = (yo - posy / 2) - (-angle) * (xo - posx / 2);
			b2 = y - (-angle) * x;

			hyp = abs(angle * x + y + (-posy / 2 - angle * posx / 2)) * wipezone->pythangle;
			hyp2 = abs(angle * x + y + (-(yo - posy / 2) - angle * (xo - posx / 2))) * wipezone->pythangle;

			if (b2 < b1 && b2 < b3) {
				if (hwidth < pointdist)
					output *= in_band(wipezone, hwidth, hyp, facf0, 0, 1);
			}
			else if (b2 > b1 && b2 > b3) {
				if (hwidth < pointdist)
					output *= in_band(wipezone, hwidth, hyp2, facf0, 0, 1);
			}
			else {
				if (hyp < hwidth && hyp2 > hwidth)
					output *= in_band(wipezone, hwidth, hyp, facf0, 1, 1);
				else if (hyp > hwidth && hyp2 < hwidth)
					output *= in_band(wipezone, hwidth, hyp2, facf0, 1, 1);
				else
					output *= in_band(wipezone, hwidth, hyp2, facf0, 1, 1) * in_band(wipezone, hwidth, hyp, facf0, 1, 1);
			}

			break;
#endif
		case DO_IRIS_WIPE:
			if (xo > yo) yo = xo;
			else xo = yo;

			if (!wipe->forward) facf0 = 1 - facf0;

			width = wipezone->width;
			hwidth = width * 0.5f;

			temp1 = (halfx - (halfx) * facf0);
			pointdist = sqrt(temp1 * temp1 + temp1 * temp1);

			temp2 = sqrt((halfx - x) * (halfx - x) + (halfy - y) * (halfy - y));
			if (temp2 > pointdist) output = in_band(hwidth, fabs(temp2 - pointdist), 0, 1);
			else output = in_band(hwidth, fabs(temp2 - pointdist), 1, 1);

			if (!wipe->forward) output = 1 - output;
			
			break;
	}
	if (output < 0) output = 0;
	else if (output > 1) output = 1;
	return output;
}

static void init_wipe_effect(Sequence *seq)
{
	if (seq->effectdata) MEM_freeN(seq->effectdata);
	seq->effectdata = MEM_callocN(sizeof(struct WipeVars), "wipevars");
}

static int num_inputs_wipe(void)
{
	return 1;
}

static void free_wipe_effect(Sequence *seq)
{
	if (seq->effectdata) MEM_freeN(seq->effectdata);
	seq->effectdata = NULL;
}

static void copy_wipe_effect(Sequence *dst, Sequence *src)
{
	dst->effectdata = MEM_dupallocN(src->effectdata);
}

static void do_wipe_effect_byte(Sequence *seq, float facf0, float UNUSED(facf1), 
                                int x, int y,
                                unsigned char *rect1,
                                unsigned char *rect2, unsigned char *out)
{
	WipeZone wipezone;
	WipeVars *wipe = (WipeVars *)seq->effectdata;
	int xo, yo;
	char *rt1, *rt2, *rt;

	precalc_wipe_zone(&wipezone, wipe, x, y);

	rt1 = (char *)rect1;
	rt2 = (char *)rect2;
	rt = (char *)out;

	xo = x;
	yo = y;
	for (y = 0; y < yo; y++) {
		for (x = 0; x < xo; x++) {
			float check = check_zone(&wipezone, x, y, seq, facf0);
			if (check) {
				if (rt1) {
					rt[0] = (int)(rt1[0] * check) + (int)(rt2[0] * (1 - check));
					rt[1] = (int)(rt1[1] * check) + (int)(rt2[1] * (1 - check));
					rt[2] = (int)(rt1[2] * check) + (int)(rt2[2] * (1 - check));
					rt[3] = (int)(rt1[3] * check) + (int)(rt2[3] * (1 - check));
				}
				else {
					rt[0] = 0;
					rt[1] = 0;
					rt[2] = 0;
					rt[3] = 255;
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
					rt[3] = 255;
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

static void do_wipe_effect_float(Sequence *seq, float facf0, float UNUSED(facf1), 
                                 int x, int y,
                                 float *rect1,
                                 float *rect2, float *out)
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

static ImBuf *do_wipe_effect(
        SeqRenderData context, Sequence *seq, float UNUSED(cfra),
        float facf0, float facf1,
        struct ImBuf *ibuf1, struct ImBuf *ibuf2,
        struct ImBuf *ibuf3)
{
	struct ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);

	if (out->rect_float) {
		do_wipe_effect_float(seq,
		                     facf0, facf1, context.rectx, context.recty,
		                     ibuf1->rect_float, ibuf2->rect_float,
		                     out->rect_float);
	}
	else {
		do_wipe_effect_byte(seq,
		                    facf0, facf1, context.rectx, context.recty,
		                    (unsigned char *) ibuf1->rect, (unsigned char *) ibuf2->rect,
		                    (unsigned char *) out->rect);
	}

	return out;
}
/* **********************************************************************
 * TRANSFORM
 * ********************************************************************** */
static void init_transform_effect(Sequence *seq)
{
	TransformVars *transform;

	if (seq->effectdata) MEM_freeN(seq->effectdata);
	seq->effectdata = MEM_callocN(sizeof(struct TransformVars), "transformvars");

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

static void free_transform_effect(Sequence *seq)
{
	if (seq->effectdata) MEM_freeN(seq->effectdata);
	seq->effectdata = NULL;
}

static void copy_transform_effect(Sequence *dst, Sequence *src)
{
	dst->effectdata = MEM_dupallocN(src->effectdata);
}

static void transform_image(int x, int y, struct ImBuf *ibuf1, struct ImBuf *out, 
                            float scale_x, float scale_y, float translate_x, float translate_y,
                            float rotate, int interpolation)
{
	int xo, yo, xi, yi;
	float xt, yt, xr, yr;
	float s, c;

	xo = x;
	yo = y;
	
	// Rotate
	s = sin(rotate);
	c = cos(rotate);

	for (yi = 0; yi < yo; yi++) {
		for (xi = 0; xi < xo; xi++) {

			//translate point
			xt = xi - translate_x;
			yt = yi - translate_y;

			//rotate point with center ref
			xr =  c * xt + s * yt;
			yr = -s * xt + c * yt;

			//scale point with center ref
			xt = xr / scale_x;
			yt = yr / scale_y;

			//undo reference center point 
			xt += (xo / 2.0f);
			yt += (yo / 2.0f);

			//interpolate
			switch (interpolation) {
				case 0:
					neareast_interpolation(ibuf1, out, xt, yt, xi, yi);
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

static void do_transform(Scene *scene, Sequence *seq, float UNUSED(facf0), int x, int y, 
                         struct ImBuf *ibuf1, struct ImBuf *out)
{
	TransformVars *transform = (TransformVars *)seq->effectdata;
	float scale_x, scale_y, translate_x, translate_y, rotate_radians;
	
	// Scale
	if (transform->uniform_scale) {
		scale_x = scale_y = transform->ScalexIni;
	}
	else {
		scale_x = transform->ScalexIni;
		scale_y = transform->ScaleyIni;
	}

	// Translate
	if (!transform->percent) {
		float rd_s = (scene->r.size / 100.0f);

		translate_x = transform->xIni * rd_s + (x / 2.0f);
		translate_y = transform->yIni * rd_s + (y / 2.0f);
	}
	else {
		translate_x = x * (transform->xIni / 100.0f) + (x / 2.0f);
		translate_y = y * (transform->yIni / 100.0f) + (y / 2.0f);
	}
	
	// Rotate
	rotate_radians = DEG2RADF(transform->rotIni);

	transform_image(x, y, ibuf1, out, scale_x, scale_y, translate_x, translate_y, rotate_radians, transform->interpolation);
}


static ImBuf *do_transform_effect(
        SeqRenderData context, Sequence *seq, float UNUSED(cfra),
        float facf0, float UNUSED(facf1),
        struct ImBuf *ibuf1, struct ImBuf *ibuf2,
        struct ImBuf *ibuf3)
{
	struct ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);

	do_transform(context.scene, seq, facf0, 
	             context.rectx, context.recty, ibuf1, out);

	return out;
}


/* **********************************************************************
 * GLOW
 * ********************************************************************** */

static void RVBlurBitmap2_byte(unsigned char *map, int width, int height,
                               float blur,
                               int quality)
/*	MUUUCCH better than the previous blur. */
/*	We do the blurring in two passes which is a whole lot faster. */
/*	I changed the math arount to implement an actual Gaussian */
/*	distribution. */
/* */
/*	Watch out though, it tends to misbehaven with large blur values on */
/*	a small bitmap.  Avoid avoid avoid. */
/*=============================== */
{
	unsigned char *temp = NULL, *swap;
	float   *filter = NULL;
	int x, y, i, fx, fy;
	int index, ix, halfWidth;
	float fval, k, curColor[3], curColor2[3], weight = 0;

	/*	If we're not really blurring, bail out */
	if (blur <= 0)
		return;

	/*	Allocate memory for the tempmap and the blur filter matrix */
	temp = MEM_mallocN((width * height * 4), "blurbitmaptemp");
	if (!temp)
		return;

	/*	Allocate memory for the filter elements */
	halfWidth = ((quality + 1) * blur);
	filter = (float *)MEM_mallocN(sizeof(float) * halfWidth * 2, "blurbitmapfilter");
	if (!filter) {
		MEM_freeN(temp);
		return;
	}

	/*	Apparently we're calculating a bell curve */
	/*	based on the standard deviation (or radius) */
	/*	This code is based on an example */
	/*	posted to comp.graphics.algorithms by */
	/*	Blancmange (bmange@airdmhor.gen.nz) */

	k = -1.0f / (2.0f * (float)M_PI * blur * blur);
	for (ix = 0; ix < halfWidth; ix++) {
		weight = (float)exp(k * (ix * ix));
		filter[halfWidth - ix] = weight;
		filter[halfWidth + ix] = weight;
	}
	filter[0] = weight;

	/*	Normalize the array */
	fval = 0;
	for (ix = 0; ix < halfWidth * 2; ix++)
		fval += filter[ix];

	for (ix = 0; ix < halfWidth * 2; ix++)
		filter[ix] /= fval;

	/*	Blur the rows */
	for (y = 0; y < height; y++) {
		/*	Do the left & right strips */
		for (x = 0; x < halfWidth; x++) {
			index = (x + y * width) * 4;
			fx = 0;
			zero_v3(curColor);
			zero_v3(curColor2);

			for (i = x - halfWidth; i < x + halfWidth; i++) {
				if ((i >= 0) && (i < width)) {
					curColor[0] += map[(i + y * width) * 4 + GlowR] * filter[fx];
					curColor[1] += map[(i + y * width) * 4 + GlowG] * filter[fx];
					curColor[2] += map[(i + y * width) * 4 + GlowB] * filter[fx];

					curColor2[0] += map[(width - 1 - i + y * width) * 4 + GlowR] *
					                filter[fx];
					curColor2[1] += map[(width - 1 - i + y * width) * 4 + GlowG] *
					                filter[fx];
					curColor2[2] += map[(width - 1 - i + y * width) * 4 + GlowB] *
					                filter[fx];
				}
				fx++;
			}
			temp[index + GlowR] = curColor[0];
			temp[index + GlowG] = curColor[1];
			temp[index + GlowB] = curColor[2];

			temp[((width - 1 - x + y * width) * 4) + GlowR] = curColor2[0];
			temp[((width - 1 - x + y * width) * 4) + GlowG] = curColor2[1];
			temp[((width - 1 - x + y * width) * 4) + GlowB] = curColor2[2];

		}
		/*	Do the main body */
		for (x = halfWidth; x < width - halfWidth; x++) {
			index = (x + y * width) * 4;
			fx = 0;
			zero_v3(curColor);
			for (i = x - halfWidth; i < x + halfWidth; i++) {
				curColor[0] += map[(i + y * width) * 4 + GlowR] * filter[fx];
				curColor[1] += map[(i + y * width) * 4 + GlowG] * filter[fx];
				curColor[2] += map[(i + y * width) * 4 + GlowB] * filter[fx];
				fx++;
			}
			temp[index + GlowR] = curColor[0];
			temp[index + GlowG] = curColor[1];
			temp[index + GlowB] = curColor[2];
		}
	}

	/*	Swap buffers */
	swap = temp; temp = map; map = swap;


	/*	Blur the columns */
	for (x = 0; x < width; x++) {
		/*	Do the top & bottom strips */
		for (y = 0; y < halfWidth; y++) {
			index = (x + y * width) * 4;
			fy = 0;
			zero_v3(curColor);
			zero_v3(curColor2);
			for (i = y - halfWidth; i < y + halfWidth; i++) {
				if ((i >= 0) && (i < height)) {
					/*	Bottom */
					curColor[0] += map[(x + i * width) * 4 + GlowR] * filter[fy];
					curColor[1] += map[(x + i * width) * 4 + GlowG] * filter[fy];
					curColor[2] += map[(x + i * width) * 4 + GlowB] * filter[fy];

					/*	Top */
					curColor2[0] += map[(x + (height - 1 - i) * width) *
					                    4 + GlowR] * filter[fy];
					curColor2[1] += map[(x + (height - 1 - i) * width) *
					                    4 + GlowG] * filter[fy];
					curColor2[2] += map[(x + (height - 1 - i) * width) *
					                    4 + GlowB] * filter[fy];
				}
				fy++;
			}
			temp[index + GlowR] = curColor[0];
			temp[index + GlowG] = curColor[1];
			temp[index + GlowB] = curColor[2];
			temp[((x + (height - 1 - y) * width) * 4) + GlowR] = curColor2[0];
			temp[((x + (height - 1 - y) * width) * 4) + GlowG] = curColor2[1];
			temp[((x + (height - 1 - y) * width) * 4) + GlowB] = curColor2[2];
		}
		/*	Do the main body */
		for (y = halfWidth; y < height - halfWidth; y++) {
			index = (x + y * width) * 4;
			fy = 0;
			zero_v3(curColor);
			for (i = y - halfWidth; i < y + halfWidth; i++) {
				curColor[0] += map[(x + i * width) * 4 + GlowR] * filter[fy];
				curColor[1] += map[(x + i * width) * 4 + GlowG] * filter[fy];
				curColor[2] += map[(x + i * width) * 4 + GlowB] * filter[fy];
				fy++;
			}
			temp[index + GlowR] = curColor[0];
			temp[index + GlowG] = curColor[1];
			temp[index + GlowB] = curColor[2];
		}
	}


	/*	Swap buffers */
	swap = temp; temp = map; /* map = swap; */ /* UNUSED */

	/*	Tidy up	 */
	MEM_freeN(filter);
	MEM_freeN(temp);
}

static void RVBlurBitmap2_float(float *map, int width, int height,
                                float blur,
                                int quality)
/*	MUUUCCH better than the previous blur. */
/*	We do the blurring in two passes which is a whole lot faster. */
/*	I changed the math arount to implement an actual Gaussian */
/*	distribution. */
/* */
/*	Watch out though, it tends to misbehaven with large blur values on */
/*	a small bitmap.  Avoid avoid avoid. */
/*=============================== */
{
	float *temp = NULL, *swap;
	float   *filter = NULL;
	int x, y, i, fx, fy;
	int index, ix, halfWidth;
	float fval, k, curColor[3], curColor2[3], weight = 0;

	/*	If we're not really blurring, bail out */
	if (blur <= 0)
		return;

	/*	Allocate memory for the tempmap and the blur filter matrix */
	temp = MEM_mallocN((width * height * 4 * sizeof(float)), "blurbitmaptemp");
	if (!temp)
		return;

	/*	Allocate memory for the filter elements */
	halfWidth = ((quality + 1) * blur);
	filter = (float *)MEM_mallocN(sizeof(float) * halfWidth * 2, "blurbitmapfilter");
	if (!filter) {
		MEM_freeN(temp);
		return;
	}

	/*	Apparently we're calculating a bell curve */
	/*	based on the standard deviation (or radius) */
	/*	This code is based on an example */
	/*	posted to comp.graphics.algorithms by */
	/*	Blancmange (bmange@airdmhor.gen.nz) */

	k = -1.0f / (2.0f * (float)M_PI * blur * blur);

	for (ix = 0; ix < halfWidth; ix++) {
		weight = (float)exp(k * (ix * ix));
		filter[halfWidth - ix] = weight;
		filter[halfWidth + ix] = weight;
	}
	filter[0] = weight;

	/*	Normalize the array */
	fval = 0;
	for (ix = 0; ix < halfWidth * 2; ix++)
		fval += filter[ix];

	for (ix = 0; ix < halfWidth * 2; ix++)
		filter[ix] /= fval;

	/*	Blur the rows */
	for (y = 0; y < height; y++) {
		/*	Do the left & right strips */
		for (x = 0; x < halfWidth; x++) {
			index = (x + y * width) * 4;
			fx = 0;
			curColor[0] = curColor[1] = curColor[2] = 0.0f;
			curColor2[0] = curColor2[1] = curColor2[2] = 0.0f;

			for (i = x - halfWidth; i < x + halfWidth; i++) {
				if ((i >= 0) && (i < width)) {
					curColor[0] += map[(i + y * width) * 4 + GlowR] * filter[fx];
					curColor[1] += map[(i + y * width) * 4 + GlowG] * filter[fx];
					curColor[2] += map[(i + y * width) * 4 + GlowB] * filter[fx];

					curColor2[0] += map[(width - 1 - i + y * width) * 4 + GlowR] *
					                filter[fx];
					curColor2[1] += map[(width - 1 - i + y * width) * 4 + GlowG] *
					                filter[fx];
					curColor2[2] += map[(width - 1 - i + y * width) * 4 + GlowB] *
					                filter[fx];
				}
				fx++;
			}
			temp[index + GlowR] = curColor[0];
			temp[index + GlowG] = curColor[1];
			temp[index + GlowB] = curColor[2];

			temp[((width - 1 - x + y * width) * 4) + GlowR] = curColor2[0];
			temp[((width - 1 - x + y * width) * 4) + GlowG] = curColor2[1];
			temp[((width - 1 - x + y * width) * 4) + GlowB] = curColor2[2];

		}
		/*	Do the main body */
		for (x = halfWidth; x < width - halfWidth; x++) {
			index = (x + y * width) * 4;
			fx = 0;
			zero_v3(curColor);
			for (i = x - halfWidth; i < x + halfWidth; i++) {
				curColor[0] += map[(i + y * width) * 4 + GlowR] * filter[fx];
				curColor[1] += map[(i + y * width) * 4 + GlowG] * filter[fx];
				curColor[2] += map[(i + y * width) * 4 + GlowB] * filter[fx];
				fx++;
			}
			temp[index + GlowR] = curColor[0];
			temp[index + GlowG] = curColor[1];
			temp[index + GlowB] = curColor[2];
		}
	}

	/*	Swap buffers */
	swap = temp; temp = map; map = swap;


	/*	Blur the columns */
	for (x = 0; x < width; x++) {
		/*	Do the top & bottom strips */
		for (y = 0; y < halfWidth; y++) {
			index = (x + y * width) * 4;
			fy = 0;
			zero_v3(curColor);
			zero_v3(curColor2);
			for (i = y - halfWidth; i < y + halfWidth; i++) {
				if ((i >= 0) && (i < height)) {
					/*	Bottom */
					curColor[0] += map[(x + i * width) * 4 + GlowR] * filter[fy];
					curColor[1] += map[(x + i * width) * 4 + GlowG] * filter[fy];
					curColor[2] += map[(x + i * width) * 4 + GlowB] * filter[fy];

					/*	Top */
					curColor2[0] += map[(x + (height - 1 - i) * width) *
					                    4 + GlowR] * filter[fy];
					curColor2[1] += map[(x + (height - 1 - i) * width) *
					                    4 + GlowG] * filter[fy];
					curColor2[2] += map[(x + (height - 1 - i) * width) *
					                    4 + GlowB] * filter[fy];
				}
				fy++;
			}
			temp[index + GlowR] = curColor[0];
			temp[index + GlowG] = curColor[1];
			temp[index + GlowB] = curColor[2];
			temp[((x + (height - 1 - y) * width) * 4) + GlowR] = curColor2[0];
			temp[((x + (height - 1 - y) * width) * 4) + GlowG] = curColor2[1];
			temp[((x + (height - 1 - y) * width) * 4) + GlowB] = curColor2[2];
		}
		/*	Do the main body */
		for (y = halfWidth; y < height - halfWidth; y++) {
			index = (x + y * width) * 4;
			fy = 0;
			zero_v3(curColor);
			for (i = y - halfWidth; i < y + halfWidth; i++) {
				curColor[0] += map[(x + i * width) * 4 + GlowR] * filter[fy];
				curColor[1] += map[(x + i * width) * 4 + GlowG] * filter[fy];
				curColor[2] += map[(x + i * width) * 4 + GlowB] * filter[fy];
				fy++;
			}
			temp[index + GlowR] = curColor[0];
			temp[index + GlowG] = curColor[1];
			temp[index + GlowB] = curColor[2];
		}
	}


	/*	Swap buffers */
	swap = temp; temp = map; /* map = swap; */ /* UNUSED */

	/*	Tidy up	 */
	MEM_freeN(filter);
	MEM_freeN(temp);
}


/*	Adds two bitmaps and puts the results into a third map. */
/*	C must have been previously allocated but it may be A or B. */
/*	We clamp values to 255 to prevent weirdness */
/*=============================== */
static void RVAddBitmaps_byte(unsigned char *a, unsigned char *b, unsigned char *c, int width, int height)
{
	int x, y, index;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			index = (x + y * width) * 4;
			c[index + GlowR] = MIN2(255, a[index + GlowR] + b[index + GlowR]);
			c[index + GlowG] = MIN2(255, a[index + GlowG] + b[index + GlowG]);
			c[index + GlowB] = MIN2(255, a[index + GlowB] + b[index + GlowB]);
			c[index + GlowA] = MIN2(255, a[index + GlowA] + b[index + GlowA]);
		}
	}
}

static void RVAddBitmaps_float(float *a, float *b, float *c,
                               int width, int height)
{
	int x, y, index;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			index = (x + y * width) * 4;
			c[index + GlowR] = MIN2(1.0f, a[index + GlowR] + b[index + GlowR]);
			c[index + GlowG] = MIN2(1.0f, a[index + GlowG] + b[index + GlowG]);
			c[index + GlowB] = MIN2(1.0f, a[index + GlowB] + b[index + GlowB]);
			c[index + GlowA] = MIN2(1.0f, a[index + GlowA] + b[index + GlowA]);
		}
	}
}

/*	For each pixel whose total luminance exceeds the threshold, */
/*	Multiply it's value by BOOST and add it to the output map */
static void RVIsolateHighlights_byte(unsigned char *in, unsigned char *out,
                                     int width, int height, int threshold,
                                     float boost, float clamp)
{
	int x, y, index;
	int intensity;


	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			index = (x + y * width) * 4;

			/*	Isolate the intensity */
			intensity = (in[index + GlowR] + in[index + GlowG] + in[index + GlowB] - threshold);
			if (intensity > 0) {
				out[index + GlowR] = MIN2(255 * clamp, (in[index + GlowR] * boost * intensity) / 255);
				out[index + GlowG] = MIN2(255 * clamp, (in[index + GlowG] * boost * intensity) / 255);
				out[index + GlowB] = MIN2(255 * clamp, (in[index + GlowB] * boost * intensity) / 255);
				out[index + GlowA] = MIN2(255 * clamp, (in[index + GlowA] * boost * intensity) / 255);
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

static void RVIsolateHighlights_float(float *in, float *out,
                                      int width, int height, float threshold,
                                      float boost, float clamp)
{
	int x, y, index;
	float intensity;


	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			index = (x + y * width) * 4;

			/*	Isolate the intensity */
			intensity = (in[index + GlowR] + in[index + GlowG] + in[index + GlowB] - threshold);
			if (intensity > 0) {
				out[index + GlowR] = MIN2(clamp, (in[index + GlowR] * boost * intensity));
				out[index + GlowG] = MIN2(clamp, (in[index + GlowG] * boost * intensity));
				out[index + GlowB] = MIN2(clamp, (in[index + GlowB] * boost * intensity));
				out[index + GlowA] = MIN2(clamp, (in[index + GlowA] * boost * intensity));
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

	if (seq->effectdata) MEM_freeN(seq->effectdata);
	seq->effectdata = MEM_callocN(sizeof(struct GlowVars), "glowvars");

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

static void free_glow_effect(Sequence *seq)
{
	if (seq->effectdata) MEM_freeN(seq->effectdata);
	seq->effectdata = NULL;
}

static void copy_glow_effect(Sequence *dst, Sequence *src)
{
	dst->effectdata = MEM_dupallocN(src->effectdata);
}

//void do_glow_effect(Cast *cast, float facf0, float facf1, int xo, int yo, ImBuf *ibuf1, ImBuf *ibuf2, ImBuf *outbuf, ImBuf *use)
static void do_glow_effect_byte(Sequence *seq, int render_size, float facf0, float UNUSED(facf1), 
                                int x, int y, char *rect1,
                                char *UNUSED(rect2), char *out)
{
	unsigned char *outbuf = (unsigned char *)out;
	unsigned char *inbuf = (unsigned char *)rect1;
	GlowVars *glow = (GlowVars *)seq->effectdata;
	
	RVIsolateHighlights_byte(inbuf, outbuf, x, y, glow->fMini * 765, glow->fBoost * facf0, glow->fClamp);
	RVBlurBitmap2_byte(outbuf, x, y, glow->dDist * (render_size / 100.0f), glow->dQuality);
	if (!glow->bNoComp)
		RVAddBitmaps_byte(inbuf, outbuf, outbuf, x, y);
}

static void do_glow_effect_float(Sequence *seq, int render_size, float facf0, float UNUSED(facf1), 
                                 int x, int y,
                                 float *rect1, float *UNUSED(rect2), float *out)
{
	float *outbuf = out;
	float *inbuf = rect1;
	GlowVars *glow = (GlowVars *)seq->effectdata;

	RVIsolateHighlights_float(inbuf, outbuf, x, y, glow->fMini * 3.0f, glow->fBoost * facf0, glow->fClamp);
	RVBlurBitmap2_float(outbuf, x, y, glow->dDist * (render_size / 100.0f), glow->dQuality);
	if (!glow->bNoComp)
		RVAddBitmaps_float(inbuf, outbuf, outbuf, x, y);
}

static ImBuf *do_glow_effect(
        SeqRenderData context, Sequence *seq, float UNUSED(cfra),
        float facf0, float facf1,
        struct ImBuf *ibuf1, struct ImBuf *ibuf2,
        struct ImBuf *ibuf3)
{
	struct ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);

	int render_size = 100 * context.rectx / context.scene->r.xsch;

	if (out->rect_float) {
		do_glow_effect_float(seq, render_size,
		                     facf0, facf1,
		                     context.rectx, context.recty,
		                     ibuf1->rect_float, ibuf2->rect_float,
		                     out->rect_float);
	}
	else {
		do_glow_effect_byte(seq, render_size,
		                    facf0, facf1,
		                    context.rectx, context.recty,
		                    (char *) ibuf1->rect, (char *) ibuf2->rect,
		                    (char *) out->rect);
	}

	return out;
}

/* **********************************************************************
 * SOLID COLOR
 * ********************************************************************** */

static void init_solid_color(Sequence *seq)
{
	SolidColorVars *cv;
	
	if (seq->effectdata) MEM_freeN(seq->effectdata);
	seq->effectdata = MEM_callocN(sizeof(struct SolidColorVars), "solidcolor");
	
	cv = (SolidColorVars *)seq->effectdata;
	cv->col[0] = cv->col[1] = cv->col[2] = 0.5;
}

static int num_inputs_color(void)
{
	return 0;
}

static void free_solid_color(Sequence *seq)
{
	if (seq->effectdata) MEM_freeN(seq->effectdata);
	seq->effectdata = NULL;
}

static void copy_solid_color(Sequence *dst, Sequence *src)
{
	dst->effectdata = MEM_dupallocN(src->effectdata);
}

static int early_out_color(struct Sequence *UNUSED(seq),
                           float UNUSED(facf0), float UNUSED(facf1))
{
	return -1;
}

static ImBuf *do_solid_color(
        SeqRenderData context, Sequence *seq, float UNUSED(cfra),
        float facf0, float facf1,
        struct ImBuf *ibuf1, struct ImBuf *ibuf2,
        struct ImBuf *ibuf3)
{
	struct ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);

	SolidColorVars *cv = (SolidColorVars *)seq->effectdata;

	unsigned char *rect;
	float *rect_float;
	int x; /*= context.rectx;*/ /*UNUSED*/
	int y; /*= context.recty;*/ /*UNUSED*/

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

/* **********************************************************************
 * MULTICAM
 * ********************************************************************** */

/* no effect inputs for multicam, we use give_ibuf_seq */
static int num_inputs_multicam(void)
{
	return 0;
}

static int early_out_multicam(struct Sequence *UNUSED(seq), float UNUSED(facf0), float UNUSED(facf1))
{
	return -1;
}

static ImBuf *do_multicam(
        SeqRenderData context, Sequence *seq, float cfra,
        float UNUSED(facf0), float UNUSED(facf1),
        struct ImBuf *UNUSED(ibuf1), struct ImBuf *UNUSED(ibuf2),
        struct ImBuf *UNUSED(ibuf3))
{
	struct ImBuf *i;
	struct ImBuf *out;
	Editing *ed;
	ListBase *seqbasep;

	if (seq->multicam_source == 0 || seq->multicam_source >= seq->machine) {
		return NULL;
	}

	ed = context.scene->ed;
	if (!ed) {
		return NULL;
	}
	seqbasep = seq_seqbase(&ed->seqbase, seq);
	if (!seqbasep) {
		return NULL;
	}

	i = give_ibuf_seqbase(context, cfra, seq->multicam_source, seqbasep);
	if (!i) {
		return NULL;
	}

	if (input_have_to_preprocess(context, seq, cfra)) {
		out = IMB_dupImBuf(i);
		IMB_freeImBuf(i);
	}
	else {
		out = i;
	}
	
	return out;
}

/* **********************************************************************
 * ADJUSTMENT
 * ********************************************************************** */

/* no effect inputs for adjustment, we use give_ibuf_seq */
static int num_inputs_adjustment(void)
{
	return 0;
}

static int early_out_adjustment(struct Sequence *UNUSED(seq), float UNUSED(facf0), float UNUSED(facf1))
{
	return -1;
}

static ImBuf *do_adjustment_impl(SeqRenderData context, Sequence *seq, float cfra)
{
	Editing *ed;
	ListBase *seqbasep;
	struct ImBuf *i = NULL;

	ed = context.scene->ed;

	seqbasep = seq_seqbase(&ed->seqbase, seq);

	if (seq->machine > 0) {
		i = give_ibuf_seqbase(context, cfra,
		                      seq->machine - 1, seqbasep);
	}

	/* found nothing? so let's work the way up the metastrip stack, so
	 *  that it is possible to group a bunch of adjustment strips into
	 *  a metastrip and have that work on everything below the metastrip
	 */

	if (!i) {
		Sequence *meta;

		meta = seq_metastrip(&ed->seqbase, NULL, seq);

		if (meta) {
			i = do_adjustment_impl(context, meta, cfra);
		}
	}

	return i;
}

static ImBuf *do_adjustment(
        SeqRenderData context, Sequence *seq, float cfra,
        float UNUSED(facf0), float UNUSED(facf1),
        struct ImBuf *UNUSED(ibuf1), struct ImBuf *UNUSED(ibuf2),
        struct ImBuf *UNUSED(ibuf3))
{
	struct ImBuf *i = NULL;
	struct ImBuf *out;
	Editing *ed;

	ed = context.scene->ed;

	if (!ed) {
		return NULL;
	}

	i = do_adjustment_impl(context, seq, cfra);

	if (input_have_to_preprocess(context, seq, cfra)) {
		out = IMB_dupImBuf(i);
		IMB_freeImBuf(i);
	}
	else {
		out = i;
	}
	
	return out;
}

/* **********************************************************************
 * SPEED
 * ********************************************************************** */
static void init_speed_effect(Sequence *seq)
{
	SpeedControlVars *v;

	if (seq->effectdata) MEM_freeN(seq->effectdata);
	seq->effectdata = MEM_callocN(sizeof(struct SpeedControlVars), 
	                              "speedcontrolvars");

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

static void free_speed_effect(Sequence *seq)
{
	SpeedControlVars *v = (SpeedControlVars *)seq->effectdata;
	if (v->frameMap) MEM_freeN(v->frameMap);
	if (seq->effectdata) MEM_freeN(seq->effectdata);
	seq->effectdata = NULL;
}

static void copy_speed_effect(Sequence *dst, Sequence *src)
{
	SpeedControlVars *v;
	dst->effectdata = MEM_dupallocN(src->effectdata);
	v = (SpeedControlVars *)dst->effectdata;
	v->frameMap = NULL;
	v->length = 0;
}

static int early_out_speed(struct Sequence *UNUSED(seq),
                           float UNUSED(facf0), float UNUSED(facf1))
{
	return 1;
}

static void store_icu_yrange_speed(struct Sequence *seq,
                                   short UNUSED(adrcode), float *ymin, float *ymax)
{
	SpeedControlVars *v = (SpeedControlVars *)seq->effectdata;

	/* if not already done, load / initialize data */
	get_sequence_effect(seq);

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
void sequence_effect_speed_rebuild_map(Scene *scene, Sequence *seq, int force)
{
	int cfra;
	float fallback_fac = 1.0f;
	SpeedControlVars *v = (SpeedControlVars *)seq->effectdata;
	FCurve *fcu = NULL;
	int flags = v->flags;

	/* if not already done, load / initialize data */
	get_sequence_effect(seq);

	if ((force == FALSE) &&
	    (seq->len == v->length) &&
	    (v->frameMap != NULL))
	{
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
		if (v->frameMap) MEM_freeN(v->frameMap);

		v->length = seq->len;

		v->frameMap = MEM_callocN(sizeof(float) * v->length, 
		                          "speedcontrol frameMap");
	}

	fallback_fac = 1.0;

	if (seq->flag & SEQ_USE_EFFECT_DEFAULT_FADE) {
		if ((seq->seq1->enddisp != seq->seq1->start) &&
		    (seq->seq1->len != 0))
		{
			fallback_fac = (float) seq->seq1->len / 
			               (float) (seq->seq1->enddisp - seq->seq1->start);
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

/* **********************************************************************
 * sequence effect factory
 * ********************************************************************** */


static void init_noop(struct Sequence *UNUSED(seq))
{

}

static void load_noop(struct Sequence *UNUSED(seq))
{

}

static void free_noop(struct Sequence *UNUSED(seq))
{

}

static int num_inputs_default(void)
{
	return 2;
}

static int early_out_noop(struct Sequence *UNUSED(seq),
                          float UNUSED(facf0), float UNUSED(facf1))
{
	return 0;
}

static int early_out_fade(struct Sequence *UNUSED(seq),
                          float facf0, float facf1)
{
	if (facf0 == 0.0f && facf1 == 0.0f) {
		return 1;
	}
	else if (facf0 == 1.0f && facf1 == 1.0f) {
		return 2;
	}
	return 0;
}

static int early_out_mul_input2(struct Sequence *UNUSED(seq),
                                float facf0, float facf1)
{
	if (facf0 == 0.0f && facf1 == 0.0f) {
		return 1;
	}
	return 0;
}

static void store_icu_yrange_noop(struct Sequence *UNUSED(seq),
                                  short UNUSED(adrcode), float *UNUSED(ymin), float *UNUSED(ymax))
{
	/* defaults are fine */
}

static void get_default_fac_noop(struct Sequence *UNUSED(seq), float UNUSED(cfra),
                                 float *facf0, float *facf1)
{
	*facf0 = *facf1 = 1.0;
}

static void get_default_fac_fade(struct Sequence *seq, float cfra,
                                 float *facf0, float *facf1)
{
	*facf0 = (float)(cfra - seq->startdisp);
	*facf1 = (float)(*facf0 + 0.5f);
	*facf0 /= seq->len;
	*facf1 /= seq->len;
}

static ImBuf *do_overdrop_effect(SeqRenderData context,
                                 Sequence *UNUSED(seq),
                                 float UNUSED(cfra),
                                 float facf0, float facf1,
                                 struct ImBuf *ibuf1,
                                 struct ImBuf *ibuf2,
                                 struct ImBuf *ibuf3)
{
	struct ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2, ibuf3);
	int x = context.rectx;
	int y = context.recty;

	if (out->rect_float) {
		do_drop_effect_float(
		        facf0, facf1, x, y,
		        ibuf1->rect_float, ibuf2->rect_float,
		        out->rect_float);
		do_alphaover_effect_float(
		        facf0, facf1, x, y,
		        ibuf1->rect_float, ibuf2->rect_float,
		        out->rect_float);
	}
	else {
		do_drop_effect_byte(
		        facf0, facf1, x, y,
		        (char *) ibuf1->rect,
		        (char *) ibuf2->rect,
		        (char *) out->rect);
		do_alphaover_effect_byte(
		        facf0, facf1, x, y,
		        (char *) ibuf1->rect, (char *) ibuf2->rect,
		        (char *) out->rect);
	}

	return out;
}

static struct SeqEffectHandle get_sequence_effect_impl(int seq_type)
{
	struct SeqEffectHandle rval;
	int sequence_type = seq_type;

	rval.init = init_noop;
	rval.num_inputs = num_inputs_default;
	rval.load = load_noop;
	rval.free = free_noop;
	rval.early_out = early_out_noop;
	rval.get_default_fac = get_default_fac_noop;
	rval.store_icu_yrange = store_icu_yrange_noop;
	rval.execute = NULL;
	rval.copy = NULL;

	switch (sequence_type) {
		case SEQ_TYPE_CROSS:
			rval.execute = do_cross_effect;
			rval.early_out = early_out_fade;
			rval.get_default_fac = get_default_fac_fade;
			break;
		case SEQ_TYPE_GAMCROSS:
			rval.init = init_gammacross;
			rval.load = load_gammacross;
			rval.free = free_gammacross;
			rval.early_out = early_out_fade;
			rval.get_default_fac = get_default_fac_fade;
			rval.execute = do_gammacross_effect;
			break;
		case SEQ_TYPE_ADD:
			rval.execute = do_add_effect;
			rval.early_out = early_out_mul_input2;
			break;
		case SEQ_TYPE_SUB:
			rval.execute = do_sub_effect;
			rval.early_out = early_out_mul_input2;
			break;
		case SEQ_TYPE_MUL:
			rval.execute = do_mul_effect;
			rval.early_out = early_out_mul_input2;
			break;
		case SEQ_TYPE_ALPHAOVER:
			rval.init = init_alpha_over_or_under;
			rval.execute = do_alphaover_effect;
			break;
		case SEQ_TYPE_OVERDROP:
			rval.execute = do_overdrop_effect;
			break;
		case SEQ_TYPE_ALPHAUNDER:
			rval.init = init_alpha_over_or_under;
			rval.execute = do_alphaunder_effect;
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
			rval.execute = do_cross_effect;
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
			rval.num_inputs = num_inputs_adjustment;
			rval.early_out = early_out_adjustment;
			rval.execute = do_adjustment;
			break;
	}

	return rval;
}


struct SeqEffectHandle get_sequence_effect(Sequence *seq)
{
	struct SeqEffectHandle rval = {NULL};

	if (seq->type & SEQ_TYPE_EFFECT) {
		rval = get_sequence_effect_impl(seq->type);
		if ((seq->flag & SEQ_EFFECT_NOT_LOADED) != 0) {
			rval.load(seq);
			seq->flag &= ~SEQ_EFFECT_NOT_LOADED;
		}
	}

	return rval;
}

struct SeqEffectHandle get_sequence_blend(Sequence *seq)
{
	struct SeqEffectHandle rval = {NULL};

	if (seq->blend_mode != 0) {
		rval = get_sequence_effect_impl(seq->blend_mode);
		if ((seq->flag & SEQ_EFFECT_NOT_LOADED) != 0) {
			rval.load(seq);
			seq->flag &= ~SEQ_EFFECT_NOT_LOADED;
		}
	}

	return rval;
}

int get_sequence_effect_num_inputs(int seq_type)
{
	struct SeqEffectHandle rval = get_sequence_effect_impl(seq_type);

	int cnt = rval.num_inputs();
	if (rval.execute) {
		return cnt;
	}
	return 0;
}
