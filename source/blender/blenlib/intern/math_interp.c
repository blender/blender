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

/**************************************************************************
 * Filtering method based on
 * "Creating raster omnimax images from multiple perspective views using the elliptical weighted average filter"
 * by Ned Greene and Paul S. Heckbert (1986)
 ***************************************************************************/

/* table of (exp(ar) - exp(a)) / (1 - exp(a)) for r in range [0, 1] and a = -2
 * used instead of actual gaussian, otherwise at high texture magnifications circular artifacts are visible */
#define EWA_MAXIDX 255
const float EWA_WTS[EWA_MAXIDX + 1] = {
	1.f, 0.990965f, 0.982f, 0.973105f, 0.96428f, 0.955524f, 0.946836f, 0.938216f, 0.929664f,
	0.921178f, 0.912759f, 0.904405f, 0.896117f, 0.887893f, 0.879734f, 0.871638f, 0.863605f,
	0.855636f, 0.847728f, 0.839883f, 0.832098f, 0.824375f, 0.816712f, 0.809108f, 0.801564f,
	0.794079f, 0.786653f, 0.779284f, 0.771974f, 0.76472f, 0.757523f, 0.750382f, 0.743297f,
	0.736267f, 0.729292f, 0.722372f, 0.715505f, 0.708693f, 0.701933f, 0.695227f, 0.688572f,
	0.68197f, 0.67542f, 0.66892f, 0.662471f, 0.656073f, 0.649725f, 0.643426f, 0.637176f,
	0.630976f, 0.624824f, 0.618719f, 0.612663f, 0.606654f, 0.600691f, 0.594776f, 0.588906f,
	0.583083f, 0.577305f, 0.571572f, 0.565883f, 0.56024f, 0.55464f, 0.549084f, 0.543572f,
	0.538102f, 0.532676f, 0.527291f, 0.521949f, 0.516649f, 0.511389f, 0.506171f, 0.500994f,
	0.495857f, 0.490761f, 0.485704f, 0.480687f, 0.475709f, 0.470769f, 0.465869f, 0.461006f,
	0.456182f, 0.451395f, 0.446646f, 0.441934f, 0.437258f, 0.432619f, 0.428017f, 0.42345f,
	0.418919f, 0.414424f, 0.409963f, 0.405538f, 0.401147f, 0.39679f, 0.392467f, 0.388178f,
	0.383923f, 0.379701f, 0.375511f, 0.371355f, 0.367231f, 0.363139f, 0.359079f, 0.355051f,
	0.351055f, 0.347089f, 0.343155f, 0.339251f, 0.335378f, 0.331535f, 0.327722f, 0.323939f,
	0.320186f, 0.316461f, 0.312766f, 0.3091f, 0.305462f, 0.301853f, 0.298272f, 0.294719f,
	0.291194f, 0.287696f, 0.284226f, 0.280782f, 0.277366f, 0.273976f, 0.270613f, 0.267276f,
	0.263965f, 0.26068f, 0.257421f, 0.254187f, 0.250979f, 0.247795f, 0.244636f, 0.241502f,
	0.238393f, 0.235308f, 0.232246f, 0.229209f, 0.226196f, 0.223206f, 0.220239f, 0.217296f,
	0.214375f, 0.211478f, 0.208603f, 0.20575f, 0.20292f, 0.200112f, 0.197326f, 0.194562f,
	0.191819f, 0.189097f, 0.186397f, 0.183718f, 0.18106f, 0.178423f, 0.175806f, 0.17321f,
	0.170634f, 0.168078f, 0.165542f, 0.163026f, 0.16053f, 0.158053f, 0.155595f, 0.153157f,
	0.150738f, 0.148337f, 0.145955f, 0.143592f, 0.141248f, 0.138921f, 0.136613f, 0.134323f,
	0.132051f, 0.129797f, 0.12756f, 0.125341f, 0.123139f, 0.120954f, 0.118786f, 0.116635f,
	0.114501f, 0.112384f, 0.110283f, 0.108199f, 0.106131f, 0.104079f, 0.102043f, 0.100023f,
	0.0980186f, 0.09603f, 0.094057f, 0.0920994f, 0.0901571f, 0.08823f, 0.0863179f, 0.0844208f,
	0.0825384f, 0.0806708f, 0.0788178f, 0.0769792f, 0.0751551f, 0.0733451f, 0.0715493f, 0.0697676f,
	0.0679997f, 0.0662457f, 0.0645054f, 0.0627786f, 0.0610654f, 0.0593655f, 0.0576789f, 0.0560055f,
	0.0543452f, 0.0526979f, 0.0510634f, 0.0494416f, 0.0478326f, 0.0462361f, 0.0446521f, 0.0430805f,
	0.0415211f, 0.039974f, 0.0384389f, 0.0369158f, 0.0354046f, 0.0339052f, 0.0324175f, 0.0309415f,
	0.029477f, 0.0280239f, 0.0265822f, 0.0251517f, 0.0237324f, 0.0223242f, 0.020927f, 0.0195408f,
	0.0181653f, 0.0168006f, 0.0154466f, 0.0141031f, 0.0127701f, 0.0114476f, 0.0101354f, 0.00883339f,
	0.00754159f, 0.00625989f, 0.00498819f, 0.00372644f, 0.00247454f, 0.00123242f, 0.f
};

static void radangle2imp(float a2, float b2, float th, float *A, float *B, float *C, float *F)
{
	float ct2 = cosf(th);
	const float st2 = 1.0f - ct2 * ct2;	/* <- sin(th)^2 */
	ct2 *= ct2;
	*A = a2*st2 + b2*ct2;
	*B = (b2 - a2)*sinf(2.f*th);
	*C = a2*ct2 + b2*st2;
	*F = a2*b2;
}

/* all tests here are done to make sure possible overflows are hopefully minimized */
void BLI_ewa_imp2radangle(float A, float B, float C, float F, float *a, float *b, float *th, float *ecc)
{
	if (F <= 1e-5f) {	/* use arbitrary major radius, zero minor, infinite eccentricity */
		*a = sqrtf(A > C ? A : C);
		*b = 0.f;
		*ecc = 1e10f;
		*th = 0.5f * (atan2f(B, A - C) + (float)M_PI);
	}
	else {
		const float AmC = A - C, ApC = A + C, F2 = F*2.f;
		const float r = sqrtf(AmC * AmC + B * B);
		float d = ApC - r;
		*a = (d <= 0.f) ? sqrtf(A > C ? A : C) : sqrtf(F2 / d);
		d = ApC + r;
		if (d <= 0.f) {
			*b = 0.f;
			*ecc = 1e10f;
		}
		else {
			*b = sqrtf(F2 / d);
			*ecc = *a / *b;
		}
		/* incr theta by 0.5*pi (angle of major axis) */
		*th = 0.5f * (atan2f(B, AmC) + (float)M_PI);
	}
}

void BLI_ewa_filter(const int width, const int height,
                    const bool intpol,
                    const bool use_alpha,
                    const float uv[2],
                    const float du[2],
                    const float dv[2],
                    ewa_filter_read_pixel_cb read_pixel_cb,
                    void *userdata,
                    float result[4])
{
	/* scaling dxt/dyt by full resolution can cause overflow because of huge A/B/C and esp. F values,
	 * scaling by aspect ratio alone does the opposite, so try something in between instead... */
	const float ff2 = (float)width, ff = sqrtf(ff2), q = (float)height / ff;
	const float Ux = du[0] * ff, Vx = dv[0] * q, Uy = du[1] * ff, Vy = dv[1] * q;
	float A = Vx * Vx + Vy * Vy;
	float B = -2.0f * (Ux * Vx + Uy * Vy);
	float C = Ux * Ux + Uy * Uy;
	float F = A * C - B * B * 0.25f;
	float a, b, th, ecc, a2, b2, ue, ve, U0, V0, DDQ, U, ac1, ac2, BU, d;
	int u, v, u1, u2, v1, v2;

	/* The so-called 'high' quality ewa method simply adds a constant of 1 to both A & C,
	 * so the ellipse always covers at least some texels. But since the filter is now always larger,
	 * it also means that everywhere else it's also more blurry then ideally should be the case.
	 * So instead here the ellipse radii are modified instead whenever either is too low.
	 * Use a different radius based on interpolation switch, just enough to anti-alias when interpolation is off,
	 * and slightly larger to make result a bit smoother than bilinear interpolation when interpolation is on
	 * (minimum values: const float rmin = intpol ? 1.f : 0.5f;) */
	const float rmin = (intpol ? 1.5625f : 0.765625f)/ff2;
	BLI_ewa_imp2radangle(A, B, C, F, &a, &b, &th, &ecc);
	if ((b2 = b * b) < rmin) {
		if ((a2 = a*a) < rmin) {
			B = 0.0f;
			A = C = rmin;
			F = A * C;
		}
		else {
			b2 = rmin;
			radangle2imp(a2, b2, th, &A, &B, &C, &F);
		}
	}

	ue = ff * sqrtf(C);
	ve = ff * sqrtf(A);
	d = (float)(EWA_MAXIDX + 1) / (F * ff2);
	A *= d;
	B *= d;
	C *= d;

	U0 = uv[0] * (float)width;
	V0 = uv[1] * (float)height;
	u1 = (int)(floorf(U0 - ue));
	u2 = (int)(ceilf(U0 + ue));
	v1 = (int)(floorf(V0 - ve));
	v2 = (int)(ceilf(V0 + ve));

	/* sane clamping to avoid unnecessarily huge loops */
	/* note: if eccentricity gets clamped (see above),
	 * the ue/ve limits can also be lowered accordingly
	 */
	if (U0 - (float)u1 > EWA_MAXIDX) u1 = (int)U0 - EWA_MAXIDX;
	if ((float)u2 - U0 > EWA_MAXIDX) u2 = (int)U0 + EWA_MAXIDX;
	if (V0 - (float)v1 > EWA_MAXIDX) v1 = (int)V0 - EWA_MAXIDX;
	if ((float)v2 - V0 > EWA_MAXIDX) v2 = (int)V0 + EWA_MAXIDX;

	/* Early output check for cases the whole region is outside of the buffer. */
	if ((u2 < 0 || u1 >= width) ||  (v2 < 0 || v1 >= height)) {
		zero_v4(result);
		return;
	}

	U0 -= 0.5f;
	V0 -= 0.5f;
	DDQ = 2.0f * A;
	U = (float)u1 - U0;
	ac1 = A * (2.0f * U + 1.0f);
	ac2 = A * U * U;
	BU = B * U;

	d = 0.0f;
	zero_v4(result);
	for (v = v1; v <= v2; ++v) {
		const float V = (float)v - V0;
		float DQ = ac1 + B*V;
		float Q = (C * V + BU) * V + ac2;
		for (u = u1; u <= u2; ++u) {
			if (Q < (float)(EWA_MAXIDX + 1)) {
				float tc[4];
				const float wt = EWA_WTS[(Q < 0.0f) ? 0 : (unsigned int)Q];
				read_pixel_cb(userdata, u, v, tc);
				madd_v3_v3fl(result, tc, wt);
				result[3] += use_alpha ? tc[3] * wt : 0.0f;
				d += wt;
			}
			Q += DQ;
			DQ += DDQ;
		}
	}

	/* d should hopefully never be zero anymore */
	d = 1.0f / d;
	mul_v3_fl(result, d);
	/* clipping can be ignored if alpha used, texr->ta already includes filtered edge */
	result[3] = use_alpha ? result[3] * d : 1.0f;
}
