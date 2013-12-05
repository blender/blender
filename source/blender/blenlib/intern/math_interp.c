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

/** \file blender/blenlib/intern/math_interp.c
 *  \ingroup bli
 */

#include <math.h>

#include "BLI_math.h"

#include "BLI_strict_flags.h"

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
	p1 = max_ff(k + 2.0f, 0.0f);
	p2 = max_ff(k + 1.0f, 0.0f);
	p3 = max_ff(k, 0.0f);
	p4 = max_ff(k - 1.0f, 0.0f);
	return (float)(1.0f / 6.0f) * (p1 * p1 * p1 - 4.0f * p2 * p2 * p2 + 6.0f * p3 * p3 * p3 - 4.0f * p4 * p4 * p4);
}


#if 0
/* older, slower function, works the same as above */
static float P(float k)
{
	return (float)(1.0f / 6.0f) * (pow(MAX2(k + 2.0f, 0), 3.0f) - 4.0f * pow(MAX2(k + 1.0f, 0), 3.0f) + 6.0f * pow(MAX2(k, 0), 3.0f) - 4.0f * pow(MAX2(k - 1.0f, 0), 3.0f));
}
#endif

static void vector_from_float(const float *data, float vector[4], int components)
{
	if (components == 1) {
		vector[0] = data[0];
	}
	else if (components == 3) {
		copy_v3_v3(vector, data);
	}
	else {
		copy_v4_v4(vector, data);
	}
}

static void vector_from_byte(const unsigned char *data, float vector[4], int components)
{
	if (components == 1) {
		vector[0] = data[0];
	}
	else if (components == 3) {
		vector[0] = data[0];
		vector[1] = data[1];
		vector[2] = data[2];
	}
	else {
		vector[0] = data[0];
		vector[1] = data[1];
		vector[2] = data[2];
		vector[3] = data[3];
	}
}

/* BICUBIC INTERPOLATION */
BLI_INLINE void bicubic_interpolation(const unsigned char *byte_buffer, const float *float_buffer,
                                      unsigned char *byte_output, float *float_output, int width, int height,
                                      int components, float u, float v)
{
	int i, j, n, m, x1, y1;
	float a, b, w, wx, wy[4], out[4];

	/* sample area entirely outside image? */
	if (ceil(u) < 0 || floor(u) > width - 1 || ceil(v) < 0 || floor(v) > height - 1) {
		if (float_output)
			float_output[0] = float_output[1] = float_output[2] = float_output[3] = 0.0f;
		if (byte_output)
			byte_output[0] = byte_output[1] = byte_output[2] = byte_output[3] = 0;
		return;
	}

	i = (int)floor(u);
	j = (int)floor(v);
	a = u - (float)i;
	b = v - (float)j;

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
		wx = P((float)n - a);
		for (m = -1; m <= 2; m++) {
			float data[4];

			y1 = j + m;
			CLAMP(y1, 0, height - 1);
			/* normally we could do this */
			/* w = P(n-a) * P(b-m); */
			/* except that would call P() 16 times per pixel therefor pow() 64 times, better precalc these */
			w = wx * wy[m + 1];

			if (float_output) {
				const float *float_data = float_buffer + width * y1 * components + components * x1;

				vector_from_float(float_data, data, components);
			}
			else {
				const unsigned char *byte_data = byte_buffer + width * y1 * components + components * x1;

				vector_from_byte(byte_data, data, components);
			}

			if (components == 1) {
				out[0] += data[0] * w;
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
				float data[4];

				if (float_output) {
					const float *float_data = float_buffer + width * y1 * components + components * x1;

					vector_from_float(float_data, data, components);
				}
				else {
					const unsigned char *byte_data = byte_buffer + width * y1 * components + components * x1;

					vector_from_byte(byte_data, data, components);
				}

				if (components == 1) {
					out[0] += data[0] * P(n - a) * P(b - m);
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

	if (float_output) {
		if (components == 1) {
			float_output[0] = out[0];
		}
		else if (components == 3) {
			copy_v3_v3(float_output, out);
		}
		else {
			copy_v4_v4(float_output, out);
		}
	}
	else {
		if (components == 1) {
			byte_output[0] = (unsigned char)(out[0] + 0.5f);
		}
		else if (components == 3) {
			byte_output[0] = (unsigned char)(out[0] + 0.5f);
			byte_output[1] = (unsigned char)(out[1] + 0.5f);
			byte_output[2] = (unsigned char)(out[2] + 0.5f);
		}
		else {
			byte_output[0] = (unsigned char)(out[0] + 0.5f);
			byte_output[1] = (unsigned char)(out[1] + 0.5f);
			byte_output[2] = (unsigned char)(out[2] + 0.5f);
			byte_output[3] = (unsigned char)(out[3] + 0.5f);
		}
	}
}

void BLI_bicubic_interpolation_fl(const float *buffer, float *output, int width, int height,
                                  int components, float u, float v)
{
	bicubic_interpolation(NULL, buffer, NULL, output, width, height, components, u, v);
}

void BLI_bicubic_interpolation_char(const unsigned char *buffer, unsigned char *output, int width, int height,
                                    int components, float u, float v)
{
	bicubic_interpolation(buffer, NULL, output, NULL, width, height, components, u, v);
}

/* BILINEAR INTERPOLATION */
BLI_INLINE void bilinear_interpolation(const unsigned char *byte_buffer, const float *float_buffer,
                                       unsigned char *byte_output, float *float_output, int width, int height,
                                       int components, float u, float v)
{
	float a, b;
	float a_b, ma_b, a_mb, ma_mb;
	int y1, y2, x1, x2;

	/* ImBuf in must have a valid rect or rect_float, assume this is already checked */

	x1 = (int)floor(u);
	x2 = (int)ceil(u);
	y1 = (int)floor(v);
	y2 = (int)ceil(v);

	if (float_output) {
		const float *row1, *row2, *row3, *row4;
		float empty[4] = {0.0f, 0.0f, 0.0f, 0.0f};

		/* sample area entirely outside image? */
		if (x2 < 0 || x1 > width - 1 || y2 < 0 || y1 > height - 1) {
			float_output[0] = float_output[1] = float_output[2] = float_output[3] = 0.0f;
			return;
		}

		/* sample including outside of edges of image */
		if (x1 < 0 || y1 < 0) row1 = empty;
		else row1 = float_buffer + width * y1 * components + components * x1;

		if (x1 < 0 || y2 > height - 1) row2 = empty;
		else row2 = float_buffer + width * y2 * components + components * x1;

		if (x2 > width - 1 || y1 < 0) row3 = empty;
		else row3 = float_buffer + width * y1 * components + components * x2;

		if (x2 > width - 1 || y2 > height - 1) row4 = empty;
		else row4 = float_buffer + width * y2 * components + components * x2;

		a = u - floorf(u);
		b = v - floorf(v);
		a_b = a * b; ma_b = (1.0f - a) * b; a_mb = a * (1.0f - b); ma_mb = (1.0f - a) * (1.0f - b);

		if (components == 1) {
			float_output[0] = ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0];
		}
		else if (components == 3) {
			float_output[0] = ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0];
			float_output[1] = ma_mb * row1[1] + a_mb * row3[1] + ma_b * row2[1] + a_b * row4[1];
			float_output[2] = ma_mb * row1[2] + a_mb * row3[2] + ma_b * row2[2] + a_b * row4[2];
		}
		else {
			float_output[0] = ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0];
			float_output[1] = ma_mb * row1[1] + a_mb * row3[1] + ma_b * row2[1] + a_b * row4[1];
			float_output[2] = ma_mb * row1[2] + a_mb * row3[2] + ma_b * row2[2] + a_b * row4[2];
			float_output[3] = ma_mb * row1[3] + a_mb * row3[3] + ma_b * row2[3] + a_b * row4[3];
		}
	}
	else {
		const unsigned char *row1, *row2, *row3, *row4;
		unsigned char empty[4] = {0, 0, 0, 0};

		/* sample area entirely outside image? */
		if (x2 < 0 || x1 > width - 1 || y2 < 0 || y1 > height - 1) {
			byte_output[0] = byte_output[1] = byte_output[2] = byte_output[3] = 0;
			return;
		}

		/* sample including outside of edges of image */
		if (x1 < 0 || y1 < 0) row1 = empty;
		else row1 = byte_buffer + width * y1 * components + components * x1;

		if (x1 < 0 || y2 > height - 1) row2 = empty;
		else row2 = byte_buffer + width * y2 * components + components * x1;

		if (x2 > width - 1 || y1 < 0) row3 = empty;
		else row3 = byte_buffer + width * y1 * components + components * x2;

		if (x2 > width - 1 || y2 > height - 1) row4 = empty;
		else row4 = byte_buffer + width * y2 * components + components * x2;

		a = u - floorf(u);
		b = v - floorf(v);
		a_b = a * b; ma_b = (1.0f - a) * b; a_mb = a * (1.0f - b); ma_mb = (1.0f - a) * (1.0f - b);

		if (components == 1) {
			byte_output[0] = (unsigned char)(ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0] + 0.5f);
		}
		else if (components == 3) {
			byte_output[0] = (unsigned char)(ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0] + 0.5f);
			byte_output[1] = (unsigned char)(ma_mb * row1[1] + a_mb * row3[1] + ma_b * row2[1] + a_b * row4[1] + 0.5f);
			byte_output[2] = (unsigned char)(ma_mb * row1[2] + a_mb * row3[2] + ma_b * row2[2] + a_b * row4[2] + 0.5f);
		}
		else {
			byte_output[0] = (unsigned char)(ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0] + 0.5f);
			byte_output[1] = (unsigned char)(ma_mb * row1[1] + a_mb * row3[1] + ma_b * row2[1] + a_b * row4[1] + 0.5f);
			byte_output[2] = (unsigned char)(ma_mb * row1[2] + a_mb * row3[2] + ma_b * row2[2] + a_b * row4[2] + 0.5f);
			byte_output[3] = (unsigned char)(ma_mb * row1[3] + a_mb * row3[3] + ma_b * row2[3] + a_b * row4[3] + 0.5f);
		}
	}
}

void BLI_bilinear_interpolation_fl(const float *buffer, float *output, int width, int height,
                                   int components, float u, float v)
{
	bilinear_interpolation(NULL, buffer, NULL, output, width, height, components, u, v);
}

void BLI_bilinear_interpolation_char(const unsigned char *buffer, unsigned char *output, int width, int height,
                                     int components, float u, float v)
{
	bilinear_interpolation(buffer, NULL, output, NULL, width, height, components, u, v);
}
