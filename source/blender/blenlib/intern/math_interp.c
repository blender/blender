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
 * The Original Code is Copyright (C) 2012 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#include <math.h>

#include "BLI_math.h"

/**************************************************************************
 *                            INTERPOLATIONS
 *
 * Reference and docs:
 * http://wiki.blender.org/index.php/User:Damiles#Interpolations_Algorithms
 ***************************************************************************/

/* BICUBIC Interpolation functions
 *  More info: http://wiki.blender.org/index.php/User:Damiles#Bicubic_pixel_interpolation
 * function assumes out to be zero'ed, only does RGBA */

static float P(float k)
{
	float p1, p2, p3, p4;
	p1 = MAX2(k + 2.0f, 0);
	p2 = MAX2(k + 1.0f, 0);
	p3 = MAX2(k, 0);
	p4 = MAX2(k - 1.0f, 0);
	return (float)(1.0f / 6.0f) * (p1 * p1 * p1 - 4.0f * p2 * p2 * p2 + 6.0f * p3 * p3 * p3 - 4.0f * p4 * p4 * p4);
}


#if 0
/* older, slower function, works the same as above */
static float P(float k)
{
	return (float)(1.0f / 6.0f) * (pow(MAX2(k + 2.0f, 0), 3.0f) - 4.0f * pow(MAX2(k + 1.0f, 0), 3.0f) + 6.0f * pow(MAX2(k, 0), 3.0f) - 4.0f * pow(MAX2(k - 1.0f, 0), 3.0f));
}
#endif

/* BICUBIC INTERPOLATION */
void BLI_bicubic_interpolation(const float *buffer, float *output, int width, int height, int components, float u, float v)
{
	int i, j, n, m, x1, y1;
	float a, b, w, wx, wy[4], out[4];
	const float *data;

	/* sample area entirely outside image? */
	if (ceil(u) < 0 || floor(u) > width - 1 || ceil(v) < 0 || floor(v) > height - 1) {
		return;
	}

	i = (int)floor(u);
	j = (int)floor(v);
	a = u - i;
	b = v - j;

	zero_v4(out);

/* Optimized and not so easy to read */

	/* avoid calling multiple times */
	wy[0] = P(b - (-1));
	wy[1] = P(b -  0);
	wy[2] = P(b -  1);
	wy[3] = P(b -  2);

	for (n = -1; n <= 2; n++) {
		x1 = i + n;
		CLAMP(x1, 0, width - 1);
		wx = P(n - a);
		for (m = -1; m <= 2; m++) {
			y1 = j + m;
			CLAMP(y1, 0, height - 1);
			/* normally we could do this */
			/* w = P(n-a) * P(b-m); */
			/* except that would call P() 16 times per pixel therefor pow() 64 times, better precalc these */
			w = wx * wy[m + 1];

			data = buffer + width * y1 * 4 + 4 * x1;

			if (components == 1) {
				out[0] += data[0] * w;
			}
			else if (components == 2) {
				out[0] += data[0] * w;
				out[1] += data[1] * w;
			}
			else if (components == 3) {
				out[0] += data[0] * w;
				out[1] += data[1] * w;
				out[2] += data[2] * w;
			}
			else {
				out[0] += data[0] * w;
				out[1] += data[1] * w;
				out[2] += data[2] * w;
				out[3] += data[3] * w;
			}
		}
	}

/* Done with optimized part */

#if 0
	/* older, slower function, works the same as above */
	for (n = -1; n <= 2; n++) {
		for (m = -1; m <= 2; m++) {
			x1 = i + n;
			y1 = j + m;
			if (x1 > 0 && x1 < width && y1 > 0 && y1 < height) {
				data = in->rect_float + width * y1 * 4 + 4 * x1;

				if (components == 1) {
					out[0] += data[0] * P(n - a) * P(b - m);
				}
				else if (components == 2) {
					out[0] += data[0] * P(n - a) * P(b - m);
					out[1] += data[1] * P(n - a) * P(b - m);
				}
				else if (components == 3) {
					out[0] += data[0] * P(n - a) * P(b - m);
					out[1] += data[1] * P(n - a) * P(b - m);
					out[2] += data[2] * P(n - a) * P(b - m);
				}
				else {
					out[0] += data[0] * P(n - a) * P(b - m);
					out[1] += data[1] * P(n - a) * P(b - m);
					out[2] += data[2] * P(n - a) * P(b - m);
					out[3] += data[3] * P(n - a) * P(b - m);
				}
			}
		}
	}
#endif

	if (components == 1) {
		output[0] = out[0];
	}
	else if (components == 2) {
		output[0] = out[0];
		output[1] = out[1];
	}
	else if (components == 3) {
		output[0] = out[0];
		output[1] = out[1];
		output[2] = out[2];
	}
	else {
		output[0] = out[0];
		output[1] = out[1];
		output[2] = out[2];
		output[3] = out[3];
	}
}

/* BILINEAR INTERPOLATION */
void BLI_bilinear_interpolation(const float *buffer, float *output, int width, int height, int components, float u, float v)
{
	const float *row1, *row2, *row3, *row4;
	float a, b;
	float a_b, ma_b, a_mb, ma_mb;
	float empty[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	int y1, y2, x1, x2;

	/* ImBuf in must have a valid rect or rect_float, assume this is already checked */

	x1 = (int)floor(u);
	x2 = (int)ceil(u);
	y1 = (int)floor(v);
	y2 = (int)ceil(v);

	/* sample area entirely outside image? */
	if (x2 < 0 || x1 > width - 1 || y2 < 0 || y1 > height - 1) {
		return;
	}

	/* sample including outside of edges of image */
	if (x1 < 0 || y1 < 0) row1 = empty;
	else row1 = buffer + width * y1 * 4 + 4 * x1;

	if (x1 < 0 || y2 > height - 1) row2 = empty;
	else row2 = buffer + width * y2 * 4 + 4 * x1;

	if (x2 > width - 1 || y1 < 0) row3 = empty;
	else row3 = buffer + width * y1 * 4 + 4 * x2;

	if (x2 > width - 1 || y2 > height - 1) row4 = empty;
	else row4 = buffer + width * y2 * 4 + 4 * x2;

	a = u - floorf(u);
	b = v - floorf(v);
	a_b = a * b; ma_b = (1.0f - a) * b; a_mb = a * (1.0f - b); ma_mb = (1.0f - a) * (1.0f - b);

	if (components == 1) {
		output[0] = ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0];
	}
	else if (components == 2) {
		output[0] = ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0];
		output[1] = ma_mb * row1[1] + a_mb * row3[1] + ma_b * row2[1] + a_b * row4[1];
	}
	else if (components == 3) {
		output[0] = ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0];
		output[1] = ma_mb * row1[1] + a_mb * row3[1] + ma_b * row2[1] + a_b * row4[1];
		output[2] = ma_mb * row1[2] + a_mb * row3[2] + ma_b * row2[2] + a_b * row4[2];
	}
	else {
		output[0] = ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0];
		output[1] = ma_mb * row1[1] + a_mb * row3[1] + ma_b * row2[1] + a_b * row4[1];
		output[2] = ma_mb * row1[2] + a_mb * row3[2] + ma_b * row2[2] + a_b * row4[2];
		output[3] = ma_mb * row1[3] + a_mb * row3[3] + ma_b * row2[3] + a_b * row4[3];
	}
}
