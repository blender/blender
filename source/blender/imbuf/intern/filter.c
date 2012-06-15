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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Morten Mikkelsen.
 *
 * ***** END GPL LICENSE BLOCK *****
 * filter.c
 *
 */

/** \file blender/imbuf/intern/filter.c
 *  \ingroup imbuf
 */

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_filter.h"

#include "imbuf.h"

/************************************************************************/
/*				FILTERS					*/
/************************************************************************/

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
			if (ibuf->planes > 24) filtcolum(point, y, skip);
			point++;
			filtcolum(point, y, skip);
			point++;
			filtcolum(point, y, skip);
			point++;
			filtcolum(point, y, skip);
			point++;
		}
		if (pointf) {
			if (ibuf->planes > 24) filtcolumf(pointf, y, skip);
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
			if (ibuf->planes > 24) filtrow(point, x);
			point++;
			filtrow(point, x);
			point++;
			filtrow(point, x);
			point++;
			filtrow(point, x);
			point += skip;
		}
		if (pointf) {
			if (ibuf->planes > 24) filtrowf(pointf, x);
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

void IMB_filterN(ImBuf *out, ImBuf *in)
{
	register char *row1, *row2, *row3;
	register char *cp, *r11, *r13, *r21, *r23, *r31, *r33;
	int rowlen, x, y;
	
	rowlen = in->x;
	
	for (y = 0; y < in->y; y++) {
		/* setup rows */
		row2 = (char *)(in->rect + y * rowlen);
		row1 = (y == 0) ? row2 : row2 - 4 * rowlen;
		row3 = (y == in->y - 1) ? row2 : row2 + 4 * rowlen;
		
		cp = (char *)(out->rect + y * rowlen);
		
		for (x = 0; x < rowlen; x++) {
			if (x == 0) {
				r11 = row1;
				r21 = row1;
				r31 = row1;
			}
			else {
				r11 = row1 - 4;
				r21 = row1 - 4;
				r31 = row1 - 4;
			}

			if (x == rowlen - 1) {
				r13 = row1;
				r23 = row1;
				r33 = row1;
			}
			else {
				r13 = row1 + 4;
				r23 = row1 + 4;
				r33 = row1 + 4;
			}

			cp[0] = (r11[0] + 2 * row1[0] + r13[0] + 2 * r21[0] + 4 * row2[0] + 2 * r23[0] + r31[0] + 2 * row3[0] + r33[0]) >> 4;
			cp[1] = (r11[1] + 2 * row1[1] + r13[1] + 2 * r21[1] + 4 * row2[1] + 2 * r23[1] + r31[1] + 2 * row3[1] + r33[1]) >> 4;
			cp[2] = (r11[2] + 2 * row1[2] + r13[2] + 2 * r21[2] + 4 * row2[2] + 2 * r23[2] + r31[2] + 2 * row3[2] + r33[2]) >> 4;
			cp[3] = (r11[3] + 2 * row1[3] + r13[3] + 2 * r21[3] + 4 * row2[3] + 2 * r23[3] + r31[3] + 2 * row3[3] + r33[3]) >> 4;
			cp += 4; row1 += 4; row2 += 4; row3 += 4;
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
	char *row1, *row2, *row3;
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
		if (y == 1)
			row1 = row2;
		else if (y == height)
			row3 = row2;

		for (x = 0; x < rowlen; x++) {
			if (mask[((y - 1) * rowlen) + x] == 0) {
				if (*row1 || *row2 || *row3 || *(row1 + 1) || *(row3 + 1) ) {
					mask[((y - 1) * rowlen) + x] = FILTER_MASK_MARGIN;
				}
				else if ((x != rowlen - 1) && (*(row1 + 2) || *(row2 + 2) || *(row3 + 2)) ) {
					mask[((y - 1) * rowlen) + x] = FILTER_MASK_MARGIN;
				}
			}

			if (x != 0) {
				row1++; row2++; row3++;
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
	if (x < 0 || x >= w || y < 0 || y >= h) return -1;  /* return bad index */
	else return y * w + x;
}

static int check_pixel_assigned(const void *buffer, const char *mask, const int index, const int depth, const int is_float)
{
	int res = 0;

	if (index >= 0) {
		const int alpha_index = depth * index + (depth - 1);

		if (mask != NULL) {
			res = mask[index] != 0 ? 1 : 0;
		}
		else if ((is_float  && ((const float *) buffer)[alpha_index] != 0.0f) ||
		         (!is_float && ((const unsigned char *) buffer)[alpha_index] != 0) )
		{
			res = 1;
		}
	}

	return res;
}

/* if alpha is zero, it checks surrounding pixels and averages color. sets new alphas to 1.0
 * 
 * When a mask is given, only effect pixels with a mask value of 1, defined as BAKE_MASK_MARGIN in rendercore.c
 * */
void IMB_filter_extend(struct ImBuf *ibuf, char *mask, int filter)
{
	const int width = ibuf->x;
	const int height = ibuf->y;
	const int depth = 4;     /* always 4 channels */
	const int chsize = ibuf->rect_float ? sizeof(float) : sizeof(unsigned char);
	const int bsize = width * height * depth * chsize;
	const int is_float = ibuf->rect_float != NULL;
	void *dstbuf = (void *) MEM_dupallocN(ibuf->rect_float ? (void *) ibuf->rect_float : (void *) ibuf->rect);
	char *dstmask = mask == NULL ? NULL : (char *) MEM_dupallocN(mask);
	void *srcbuf = ibuf->rect_float ? (void *) ibuf->rect_float : (void *) ibuf->rect;
	char *srcmask = mask;
	int cannot_early_out = 1, r, n, k, i, j, c;
	float weight[25];

	/* build a weights buffer */
	n = 1;

#if 0
	k = 0;
	for (i = -n; i <= n; i++)
		for (j = -n; j <= n; j++)
			weight[k++] = sqrt((float) i * i + j * j);
#endif

	weight[0] = 1; weight[1] = 2; weight[2] = 1;
	weight[3] = 2; weight[4] = 0; weight[5] = 2;
	weight[6] = 1; weight[7] = 2; weight[8] = 1;

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

					if (check_pixel_assigned(srcbuf, srcmask, filter_make_index(x - 1, y, width, height), depth, is_float) ||
					    check_pixel_assigned(srcbuf, srcmask, filter_make_index(x + 1, y, width, height), depth, is_float) ||
					    check_pixel_assigned(srcbuf, srcmask, filter_make_index(x, y - 1, width, height), depth, is_float) ||
					    check_pixel_assigned(srcbuf, srcmask, filter_make_index(x, y + 1, width, height), depth, is_float))
					{
						for (i = -n; i <= n; i++) {
							for (j = -n; j <= n; j++) {
								if (i != 0 || j != 0) {
									const int tmpindex = filter_make_index(x + i, y + j, width, height);

									if (check_pixel_assigned(srcbuf, srcmask, tmpindex, depth, is_float)) {
										if (is_float) {
											for (c = 0; c < depth; c++)
												tmp[c] = ((const float *) srcbuf)[depth * tmpindex + c];
										}
										else {
											for (c = 0; c < depth; c++)
												tmp[c] = (float) ((const unsigned char *) srcbuf)[depth * tmpindex + c];
										}

										wsum += weight[k];

										for (c = 0; c < depth; c++)
											acc[c] += weight[k] * tmp[c];
									}
								}
								k++;
							}
						}

						if (wsum != 0) {
							for (c = 0; c < depth; c++)
								acc[c] /= wsum;

							if (is_float) {
								for (c = 0; c < depth; c++)
									((float *) dstbuf)[depth * index + c] = acc[c];
							}
							else {
								for (c = 0; c < depth; c++) {
									((unsigned char *) dstbuf)[depth * index + c] = acc[c] > 255 ? 255 : (acc[c] < 0 ? 0 : ((unsigned char) (acc[c] + 0.5f)));
								}
							}

							if (dstmask != NULL) dstmask[index] = FILTER_MASK_MARGIN;  /* assigned */
							cannot_early_out = 1;
						}
					}
				}
			}
		}

		/* keep the original buffer up to date. */
		memcpy(srcbuf, dstbuf, bsize);
		if (dstmask != NULL) memcpy(srcmask, dstmask, width * height);
	}

	/* free memory */
	MEM_freeN(dstbuf);
	if (dstmask != NULL) MEM_freeN(dstmask);
}

/* threadsafe version, only recreates existing maps */
void IMB_remakemipmap(ImBuf *ibuf, int use_filter)
{
	ImBuf *hbuf = ibuf;
	int curmap = 0;
	
	ibuf->miptot = 1;
	
	while (curmap < IB_MIPMAP_LEVELS) {
		
		if (ibuf->mipmap[curmap]) {
			
			if (use_filter) {
				ImBuf *nbuf = IMB_allocImBuf(hbuf->x, hbuf->y, 32, IB_rect);
				IMB_filterN(nbuf, hbuf);
				imb_onehalf_no_alloc(ibuf->mipmap[curmap], nbuf);
				IMB_freeImBuf(nbuf);
			}
			else
				imb_onehalf_no_alloc(ibuf->mipmap[curmap], hbuf);
		}
		
		ibuf->miptot = curmap + 2;
		hbuf = ibuf->mipmap[curmap];
		if (hbuf)
			hbuf->miplevel = curmap + 1;
		
		if (!hbuf || (hbuf->x <= 2 && hbuf->y <= 2))
			break;
		
		curmap++;
	}
}

/* frees too (if there) and recreates new data */
void IMB_makemipmap(ImBuf *ibuf, int use_filter)
{
	ImBuf *hbuf = ibuf;
	int curmap = 0;

	imb_freemipmapImBuf(ibuf);
	
	ibuf->miptot = 1;

	while (curmap < IB_MIPMAP_LEVELS) {
		if (use_filter) {
			ImBuf *nbuf = IMB_allocImBuf(hbuf->x, hbuf->y, 32, IB_rect);
			IMB_filterN(nbuf, hbuf);
			ibuf->mipmap[curmap] = IMB_onehalf(nbuf);
			IMB_freeImBuf(nbuf);
		}
		else
			ibuf->mipmap[curmap] = IMB_onehalf(hbuf);

		ibuf->miptot = curmap + 2;
		hbuf = ibuf->mipmap[curmap];
		hbuf->miplevel = curmap + 1;

		if (hbuf->x <= 2 && hbuf->y <= 2)
			break;

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

		for (y = 0; y < h; y++)
			for (x = 0; x < w; x++, cp += 4)
				cp[3] = 255;
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

void IMB_premultiply_rect_float(float *rect_float, char planes, int w, int h)
{
	float val, *cp;
	int x, y;

	if (planes == 24) {   /* put alpha at 1.0 */
		cp = rect_float;

		for (y = 0; y < h; y++)
			for (x = 0; x < w; x++, cp += 4)
				cp[3] = 1.0;
	}
	else {
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
	if (ibuf == NULL)
		return;

	if (ibuf->rect)
		IMB_premultiply_rect(ibuf->rect, ibuf->planes, ibuf->x, ibuf->y);

	if (ibuf->rect_float)
		IMB_premultiply_rect_float(ibuf->rect_float, ibuf->planes, ibuf->x, ibuf->y);
}

/* Tonecurve corrections */

// code of rdt_shaper_fwd and ratio_preserving_odt_tonecurve belongs to
// ACES project (https://github.com/ampas/aces-dev)

// === ODT SPLINE === //
//
// Algorithm for applying ODT tone curve in forward direction.
//
// 		vers 1.0  Doug Walker  		2012-01-23
// 		modified by Scott Dyer		2012-02-28

// Input and output are in linear (not log) units.
static float rdt_shaper_fwd( float x)
{
	// B-spline coefficients.
	// The units are density of the output.
	const float COEFS0 = -0.008;
	const float COEFS1 = -0.00616;
	const float COEFS2 =  0.026;
	const float COEFS3 =  0.185;
	const float COEFS4 =  0.521;
	const float COEFS5 =  0.993;
	const float COEFS6 =  1.563;
	const float COEFS7 =  2.218;
	const float COEFS8 =  2.795;
	const float COEFS9 =  3.36;
	const float COEFS10 = 4.0;   // NB: keep this less than or equal to -log10( FLARE)
	// The locations of these control points in OCES density space are:
	// -1., -0.79, -0.44, -0.01, 0.48, 1.01, 1.58, 2.18, 2.82, 3.47, 4.15, 4.85

	// The flare term allows the spline to more rapidly approach zero
	// while keeping the shape of the curve well-behaved in density space.
	const float FLARE = 1e-4;

	// The last control point is fixed to yield a specific density at the
	// end of the knot domain.
	//const float COEFS11 = 2. * ( -log10( FLARE) - 0.001) - COEFS10;
	// Note: Apparently a CTL bug prevents calling log10() here, so
	// you'll need to update this manually if you change FLARE.
	const float COEFS11 = COEFS10 + 2. * ( 4. - COEFS10);

	// The knots are in units of OCES density.
	const unsigned int KNOT_LEN = 11;
	const float KNOT_START = -0.9;
	const float KNOT_END = 4.484256;

	// The KNOT_POW adjusts the spacing to put more knots near the toe (highlights).
	const float KNOT_POW = 1. / 1.3;
	const float OFFS = KNOT_START;
	const float SC = KNOT_END - KNOT_START;

	// KNOT_DENS is density of the spline at the knots.
	const float KNOT_DENS[ 11] = {
		( COEFS0 + COEFS1) / 2.,
		( COEFS1 + COEFS2) / 2.,
		( COEFS2 + COEFS3) / 2.,
		( COEFS3 + COEFS4) / 2.,
		( COEFS4 + COEFS5) / 2.,
		( COEFS5 + COEFS6) / 2.,
		( COEFS6 + COEFS7) / 2.,
		( COEFS7 + COEFS8) / 2.,
		( COEFS8 + COEFS9) / 2.,
		( COEFS9 + COEFS10) / 2.,
		( COEFS10 + COEFS11) / 2.
	};

	// Parameters controlling linear extrapolation.
	const float LIGHT_SLOPE = 0.023;
	const float CROSSOVER = pow(10,-KNOT_END);
	const float REV_CROSSOVER = pow10( -KNOT_DENS[ KNOT_LEN - 1]) - FLARE;
	const float DARK_SLOPE = REV_CROSSOVER / CROSSOVER;

	// Textbook monomial to basis-function conversion matrix.
	/*const*/ float M[ 3][ 3] = {
		{  0.5, -1.0, 0.5 },
		{ -1.0,  1.0, 0.5 },
		{  0.5,  0.0, 0.0 }
	};

    float y;
    // Linear extrapolation in linear space for negative & very dark values.
    if ( x <= CROSSOVER)
        y = x * DARK_SLOPE;
    else {
        float in_dens = -log10( x);
        float out_dens;
        float knot_coord = ( in_dens - OFFS) / SC;

        // Linear extrapolation in log space for very light values.
        if ( knot_coord <= 0.)
            out_dens = KNOT_DENS[ 0] - ( KNOT_START - in_dens) * LIGHT_SLOPE;

        // For typical OCES values, apply a B-spline curve.
        else {
            knot_coord = ( KNOT_LEN - 1) * pow( knot_coord, KNOT_POW);
			{
				int j = knot_coord;
				float t = knot_coord - j;

				// Would like to do this:
				//float cf[ 3] = { COEFS[ j], COEFS[ j + 1], COEFS[ j + 2]};
				// or at least:
				//cf[ 0] = COEFS[ j];
				//cf[ 1] = COEFS[ j + 1];
				//cf[ 2] = COEFS[ j + 2];
				// But apparently CTL bugs prevent it, so we do the following:
				float cf[ 3];
				if ( j <= 0) {
					cf[ 0] = COEFS0;  cf[ 1] = COEFS1;  cf[ 2] = COEFS2;
				}
				else if ( j == 1) {
					cf[ 0] = COEFS1;  cf[ 1] = COEFS2;  cf[ 2] = COEFS3;
				}
				else if ( j == 2) {
					cf[ 0] = COEFS2;  cf[ 1] = COEFS3;  cf[ 2] = COEFS4;
				}
				else if ( j == 3) {
					cf[ 0] = COEFS3;  cf[ 1] = COEFS4;  cf[ 2] = COEFS5;
				}
				else if ( j == 4) {
					cf[ 0] = COEFS4;  cf[ 1] = COEFS5;  cf[ 2] = COEFS6;
				}
				else if ( j == 5) {
					cf[ 0] = COEFS5;  cf[ 1] = COEFS6;  cf[ 2] = COEFS7;
				}
				else if ( j == 6) {
					cf[ 0] = COEFS6;  cf[ 1] = COEFS7;  cf[ 2] = COEFS8;
				}
				else if ( j == 7) {
					cf[ 0] = COEFS7;  cf[ 1] = COEFS8;  cf[ 2] = COEFS9;
				}
				else if ( j == 8) {
					cf[ 0] = COEFS8;  cf[ 1] = COEFS9;  cf[ 2] = COEFS10;
				}
				else {
					cf[ 0] = COEFS9;  cf[ 1] = COEFS10;  cf[ 2] = COEFS11;
				}

				{
					float monomials[ 3] = { t * t, t, 1. };
					float v[3];

					// XXX: check on this! maths could be different here (like row-major vs. column major or so)
					//out_dens = dot_f3_f3( monomials, mult_f3_f33( cf, M));

					mul_v3_m3v3(v, M, cf);
					out_dens = dot_v3v3( monomials, v);
				}
			}
        }
        y = pow10( -out_dens) - FLARE;
    }
    return y;
}

void IMB_ratio_preserving_odt_tonecurve_v3(const float rgbIn[3], float rgbOut[3])
{
	//
	// The "ratio preserving tonecurve" is used to avoid hue/chroma shifts.
	// It sends a norm through the tonecurve and scales the RGB values based on the output.
	//

	const float NTH_POWER = 2.0;
	const float TINY = 1e-12;

	float numerator = ( pow(rgbIn[0],NTH_POWER) + pow(rgbIn[1],NTH_POWER) + pow(rgbIn[2],NTH_POWER) );
	float denominator = MAX2( TINY,
							 ( pow(rgbIn[0],NTH_POWER-1) +
							   pow(rgbIn[1],NTH_POWER-1) +
							   pow(rgbIn[2],NTH_POWER-1)
							 )
						   ); // use of max function to avoid divide by zero
	float normRGB = numerator / denominator;
	if (normRGB <= 0.0) normRGB = TINY;

	{
		float normRGBo = rdt_shaper_fwd( normRGB );

		rgbOut[0] = rgbIn[0] * normRGBo / normRGB;
		rgbOut[1] = rgbIn[1] * normRGBo / normRGB;
		rgbOut[2] = rgbIn[2] * normRGBo / normRGB;
	}
}

void IMB_ratio_preserving_odt_tonecurve_v4(const float rgbIn[4], float rgbOut[4])
{
	IMB_ratio_preserving_odt_tonecurve_v3(rgbIn, rgbOut);

	rgbOut[3] = rgbIn[3];
}
