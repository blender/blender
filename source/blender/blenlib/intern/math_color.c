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
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

/** \file blender/blenlib/intern/math_color.c
 *  \ingroup bli
 */

#include <assert.h>


#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h"

void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b)
{
	float nr, ng, nb;

	nr =        fabsf(h * 6.0f - 3.0f) - 1.0f;
	ng = 2.0f - fabsf(h * 6.0f - 2.0f);
	nb = 2.0f - fabsf(h * 6.0f - 4.0f);

	CLAMP(nr, 0.0f, 1.0f);
	CLAMP(nb, 0.0f, 1.0f);
	CLAMP(ng, 0.0f, 1.0f);

	*r = ((nr - 1.0f) * s + 1.0f) * v;
	*g = ((ng - 1.0f) * s + 1.0f) * v;
	*b = ((nb - 1.0f) * s + 1.0f) * v;
}

void hsl_to_rgb(float h, float s, float l, float *r, float *g, float *b)
{
	float nr, ng, nb, chroma;

	nr =        fabsf(h * 6.0f - 3.0f) - 1.0f;
	ng = 2.0f - fabsf(h * 6.0f - 2.0f);
	nb = 2.0f - fabsf(h * 6.0f - 4.0f);

	CLAMP(nr, 0.0f, 1.0f);
	CLAMP(nb, 0.0f, 1.0f);
	CLAMP(ng, 0.0f, 1.0f);

	chroma = (1.0f - fabsf(2.0f * l - 1.0f)) * s;

	*r = (nr - 0.5f) * chroma + l;
	*g = (ng - 0.5f) * chroma + l;
	*b = (nb - 0.5f) * chroma + l;
}

/* convenience function for now */
void hsv_to_rgb_v(const float hsv[3], float r_rgb[3])
{
	hsv_to_rgb(hsv[0], hsv[1], hsv[2], &r_rgb[0], &r_rgb[1], &r_rgb[2]);
}

/* convenience function for now */
void hsl_to_rgb_v(const float hsl[3], float r_rgb[3])
{
	hsl_to_rgb(hsl[0], hsl[1], hsl[2], &r_rgb[0], &r_rgb[1], &r_rgb[2]);
}

void rgb_to_yuv(float r, float g, float b, float *ly, float *lu, float *lv)
{
	float y, u, v;
	y = 0.299f * r + 0.587f * g + 0.114f * b;
	u = -0.147f * r - 0.289f * g + 0.436f * b;
	v = 0.615f * r - 0.515f * g - 0.100f * b;

	*ly = y;
	*lu = u;
	*lv = v;
}

void yuv_to_rgb(float y, float u, float v, float *lr, float *lg, float *lb)
{
	float r, g, b;
	r = y + 1.140f * v;
	g = y - 0.394f * u - 0.581f * v;
	b = y + 2.032f * u;

	*lr = r;
	*lg = g;
	*lb = b;
}

/* The RGB inputs are supposed gamma corrected and in the range 0 - 1.0f
 *
 * Output YCC have a range of 16-235 and 16-240 except with JFIF_0_255 where the range is 0-255 */
void rgb_to_ycc(float r, float g, float b, float *ly, float *lcb, float *lcr, int colorspace)
{
	float sr, sg, sb;
	float y = 128.0f, cr = 128.0f, cb = 128.0f;

	sr = 255.0f * r;
	sg = 255.0f * g;
	sb = 255.0f * b;

	switch (colorspace) {
		case BLI_YCC_ITU_BT601:
			y = (0.257f * sr) + (0.504f * sg) + (0.098f * sb) + 16.0f;
			cb = (-0.148f * sr) - (0.291f * sg) + (0.439f * sb) + 128.0f;
			cr = (0.439f * sr) - (0.368f * sg) - (0.071f * sb) + 128.0f;
			break;
		case BLI_YCC_ITU_BT709:
			y = (0.183f * sr) + (0.614f * sg) + (0.062f * sb) + 16.0f;
			cb = (-0.101f * sr) - (0.338f * sg) + (0.439f * sb) + 128.0f;
			cr = (0.439f * sr) - (0.399f * sg) - (0.040f * sb) + 128.0f;
			break;
		case BLI_YCC_JFIF_0_255:
			y = (0.299f * sr) + (0.587f * sg) + (0.114f * sb);
			cb = (-0.16874f * sr) - (0.33126f * sg) + (0.5f * sb) + 128.0f;
			cr = (0.5f * sr) - (0.41869f * sg) - (0.08131f * sb) + 128.0f;
			break;
		default:
			assert(!"invalid colorspace");
			break;
	}

	*ly = y;
	*lcb = cb;
	*lcr = cr;
}


/* YCC input have a range of 16-235 and 16-240 except with JFIF_0_255 where the range is 0-255 */
/* RGB outputs are in the range 0 - 1.0f */

/* FIXME comment above must be wrong because BLI_YCC_ITU_BT601 y 16.0 cr 16.0 -> r -0.7009 */
void ycc_to_rgb(float y, float cb, float cr, float *lr, float *lg, float *lb, int colorspace)
{
	float r = 128.0f, g = 128.0f, b = 128.0f;

	switch (colorspace) {
		case BLI_YCC_ITU_BT601:
			r = 1.164f * (y - 16.0f) + 1.596f * (cr - 128.0f);
			g = 1.164f * (y - 16.0f) - 0.813f * (cr - 128.0f) - 0.392f * (cb - 128.0f);
			b = 1.164f * (y - 16.0f) + 2.017f * (cb - 128.0f);
			break;
		case BLI_YCC_ITU_BT709:
			r = 1.164f * (y - 16.0f) + 1.793f * (cr - 128.0f);
			g = 1.164f * (y - 16.0f) - 0.534f * (cr - 128.0f) - 0.213f * (cb - 128.0f);
			b = 1.164f * (y - 16.0f) + 2.115f * (cb - 128.0f);
			break;
		case BLI_YCC_JFIF_0_255:
			r = y + 1.402f * cr - 179.456f;
			g = y - 0.34414f * cb - 0.71414f * cr + 135.45984f;
			b = y + 1.772f * cb - 226.816f;
			break;
		default:
			BLI_assert(0);
			break;
	}
	*lr = r / 255.0f;
	*lg = g / 255.0f;
	*lb = b / 255.0f;
}

void hex_to_rgb(char *hexcol, float *r, float *g, float *b)
{
	unsigned int ri, gi, bi;

	if (hexcol[0] == '#') hexcol++;

	if (sscanf(hexcol, "%02x%02x%02x", &ri, &gi, &bi) == 3) {
		/* six digit hex colors */
	}
	else if (sscanf(hexcol, "%01x%01x%01x", &ri, &gi, &bi) == 3) {
		/* three digit hex colors (#123 becomes #112233) */
		ri += ri << 4;
		gi += gi << 4;
		bi += bi << 4;
	}
	else {
		/* avoid using un-initialized vars */
		*r = *g = *b = 0.0f;
		return;
	}

	*r = (float)ri * (1.0f / 255.0f);
	*g = (float)gi * (1.0f / 255.0f);
	*b = (float)bi * (1.0f / 255.0f);
	CLAMP(*r, 0.0f, 1.0f);
	CLAMP(*g, 0.0f, 1.0f);
	CLAMP(*b, 0.0f, 1.0f);
}

void rgb_to_hsv(float r, float g, float b, float *lh, float *ls, float *lv)
{
	float k = 0.0f;
	float chroma;
	float min_gb;

	if (g < b) {
		SWAP(float, g, b);
		k = -1.0f;
	}
	min_gb = b;
	if (r < g) {
		SWAP(float, r, g);
		k = -2.0f / 6.0f - k;
		min_gb = min_ff(g, b);
	}

	chroma = r - min_gb;

	*lh = fabsf(k + (g - b) / (6.0f * chroma + 1e-20f));
	*ls = chroma / (r + 1e-20f);
	*lv = r;
}

/* convenience function for now */
void rgb_to_hsv_v(const float rgb[3], float r_hsv[3])
{
	rgb_to_hsv(rgb[0], rgb[1], rgb[2], &r_hsv[0], &r_hsv[1], &r_hsv[2]);
}

void rgb_to_hsl(float r, float g, float b, float *lh, float *ls, float *ll)
{
	const float cmax = max_fff(r, g, b);
	const float cmin = min_fff(r, g, b);
	float h, s, l = min_ff(1.0, (cmax + cmin) / 2.0f);

	if (cmax == cmin) {
		h = s = 0.0f; // achromatic
	}
	else {
		float d = cmax - cmin;
		s = l > 0.5f ? d / (2.0f - cmax - cmin) : d / (cmax + cmin);
		if (cmax == r) {
			h = (g - b) / d + (g < b ? 6.0f : 0.0f);
		}
		else if (cmax == g) {
			h = (b - r) / d + 2.0f;
		}
		else {
			h = (r - g) / d + 4.0f;
		}
	}
	h /= 6.0f;

	*lh = h;
	*ls = s;
	*ll = l;
}

void rgb_to_hsl_compat(float r, float g, float b, float *lh, float *ls, float *ll)
{
	const float orig_s = *ls;
	const float orig_h = *lh;

	rgb_to_hsl(r, g, b, lh, ls, ll);

	if (*ll <= 0.0f) {
		*lh = orig_h;
		*ls = orig_s;
	}
	else if (*ls <= 0.0f) {
		*lh = orig_h;
		*ls = orig_s;
	}

	if (*lh == 0.0f && orig_h >= 1.0f) {
		*lh = 1.0f;
	}
}

void rgb_to_hsl_compat_v(const float rgb[3], float r_hsl[3])
{
	rgb_to_hsl_compat(rgb[0], rgb[1], rgb[2], &r_hsl[0], &r_hsl[1], &r_hsl[2]);
}


/* convenience function for now */
void rgb_to_hsl_v(const float rgb[3], float r_hsl[3])
{
	rgb_to_hsl(rgb[0], rgb[1], rgb[2], &r_hsl[0], &r_hsl[1], &r_hsl[2]);
}

void rgb_to_hsv_compat(float r, float g, float b, float *lh, float *ls, float *lv)
{
	const float orig_h = *lh;
	const float orig_s = *ls;

	rgb_to_hsv(r, g, b, lh, ls, lv);

	if (*lv <= 0.0f) {
		*lh = orig_h;
		*ls = orig_s;
	}
	else if (*ls <= 0.0f) {
		*lh = orig_h;
	}

	if (*lh == 0.0f && orig_h >= 1.0f) {
		*lh = 1.0f;
	}
}

/* convenience function for now */
void rgb_to_hsv_compat_v(const float rgb[3], float r_hsv[3])
{
	rgb_to_hsv_compat(rgb[0], rgb[1], rgb[2], &r_hsv[0], &r_hsv[1], &r_hsv[2]);
}

/* clamp hsv to usable values */
void hsv_clamp_v(float hsv[3], float v_max)
{
	if (UNLIKELY(hsv[0] < 0.0f || hsv[0] > 1.0f)) {
		hsv[0] = hsv[0] - floorf(hsv[0]);
	}
	CLAMP(hsv[1], 0.0f, 1.0f);
	CLAMP(hsv[2], 0.0f, v_max);
}

/*http://brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html */

void xyz_to_rgb(float xc, float yc, float zc, float *r, float *g, float *b, int colorspace)
{
	switch (colorspace) {
		case BLI_XYZ_SMPTE:
			*r = (3.50570f * xc) + (-1.73964f * yc) + (-0.544011f * zc);
			*g = (-1.06906f * xc) + (1.97781f * yc) + (0.0351720f * zc);
			*b = (0.0563117f * xc) + (-0.196994f * yc) + (1.05005f * zc);
			break;
		case BLI_XYZ_REC709_SRGB:
			*r = (3.240476f * xc) + (-1.537150f * yc) + (-0.498535f * zc);
			*g = (-0.969256f * xc) + (1.875992f * yc) + (0.041556f * zc);
			*b = (0.055648f * xc) + (-0.204043f * yc) + (1.057311f * zc);
			break;
		case BLI_XYZ_CIE:
			*r = (2.28783848734076f * xc) + (-0.833367677835217f * yc) + (-0.454470795871421f * zc);
			*g = (-0.511651380743862f * xc) + (1.42275837632178f * yc) + (0.0888930017552939f * zc);
			*b = (0.00572040983140966f * xc) + (-0.0159068485104036f * yc) + (1.0101864083734f * zc);
			break;
	}
}

/* we define a 'cpack' here as a (3 byte color code) number that can be expressed like 0xFFAA66 or so.
 * for that reason it is sensitive for endianness... with this function it works correctly
 */

unsigned int hsv_to_cpack(float h, float s, float v)
{
	unsigned int r, g, b;
	float rf, gf, bf;
	unsigned int col;

	hsv_to_rgb(h, s, v, &rf, &gf, &bf);

	r = (unsigned int) (rf * 255.0f);
	g = (unsigned int) (gf * 255.0f);
	b = (unsigned int) (bf * 255.0f);

	col = (r + (g * 256) + (b * 256 * 256));
	return col;
}

unsigned int rgb_to_cpack(float r, float g, float b)
{
	unsigned int ir, ig, ib;

	ir = (unsigned int)floorf(255.0f * max_ff(r, 0.0f));
	ig = (unsigned int)floorf(255.0f * max_ff(g, 0.0f));
	ib = (unsigned int)floorf(255.0f * max_ff(b, 0.0f));

	if (ir > 255) ir = 255;
	if (ig > 255) ig = 255;
	if (ib > 255) ib = 255;

	return (ir + (ig * 256) + (ib * 256 * 256));
}

void cpack_to_rgb(unsigned int col, float *r, float *g, float *b)
{
	*r = ((float)(((col)      ) & 0xFF)) * (1.0f / 255.0f);
	*g = ((float)(((col) >>  8) & 0xFF)) * (1.0f / 255.0f);
	*b = ((float)(((col) >> 16) & 0xFF)) * (1.0f / 255.0f);
}

void rgb_uchar_to_float(float r_col[3], const unsigned char col_ub[3])
{
	r_col[0] = ((float)col_ub[0]) * (1.0f / 255.0f);
	r_col[1] = ((float)col_ub[1]) * (1.0f / 255.0f);
	r_col[2] = ((float)col_ub[2]) * (1.0f / 255.0f);
}

void rgba_uchar_to_float(float r_col[4], const unsigned char col_ub[4])
{
	r_col[0] = ((float)col_ub[0]) * (1.0f / 255.0f);
	r_col[1] = ((float)col_ub[1]) * (1.0f / 255.0f);
	r_col[2] = ((float)col_ub[2]) * (1.0f / 255.0f);
	r_col[3] = ((float)col_ub[3]) * (1.0f / 255.0f);
}

void rgb_float_to_uchar(unsigned char r_col[3], const float col_f[3])
{
	F3TOCHAR3(col_f, r_col);
}

void rgba_float_to_uchar(unsigned char r_col[4], const float col_f[4])
{
	F4TOCHAR4(col_f, r_col);
}

/* ********************************* color transforms ********************************* */


float srgb_to_linearrgb(float c)
{
	if (c < 0.04045f)
		return (c < 0.0f) ? 0.0f : c * (1.0f / 12.92f);
	else
		return powf((c + 0.055f) * (1.0f / 1.055f), 2.4f);
}

float linearrgb_to_srgb(float c)
{
	if (c < 0.0031308f)
		return (c < 0.0f) ? 0.0f : c * 12.92f;
	else
		return 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

void minmax_rgb(short c[3])
{
	if (c[0] > 255) c[0] = 255;
	else if (c[0] < 0) c[0] = 0;
	if (c[1] > 255) c[1] = 255;
	else if (c[1] < 0) c[1] = 0;
	if (c[2] > 255) c[2] = 255;
	else if (c[2] < 0) c[2] = 0;
}

/* If the requested RGB shade contains a negative weight for
 * one of the primaries, it lies outside the color gamut
 * accessible from the given triple of primaries.  Desaturate
 * it by adding white, equal quantities of R, G, and B, enough
 * to make RGB all positive.  The function returns 1 if the
 * components were modified, zero otherwise.*/
int constrain_rgb(float *r, float *g, float *b)
{
	/* Amount of white needed */
	const float w = -min_ffff(0.0f, *r, *g, *b);

	/* Add just enough white to make r, g, b all positive. */
	if (w > 0.0f) {
		*r += w;
		*g += w;
		*b += w;

		return 1; /* Color modified to fit RGB gamut */
	}

	return 0; /* Color within RGB gamut */
}

/* ********************************* lift/gamma/gain / ASC-CDL conversion ********************************* */

void lift_gamma_gain_to_asc_cdl(float *lift, float *gamma, float *gain, float *offset, float *slope, float *power)
{
	int c;
	for (c = 0; c < 3; c++) {
		offset[c] = lift[c] * gain[c];
		slope[c] = gain[c] * (1.0f - lift[c]);
		if (gamma[c] == 0)
			power[c] = FLT_MAX;
		else
			power[c] = 1.0f / gamma[c];
	}
}

/* ******************************************** other ************************************************* */

/* Applies an hue offset to a float rgb color */
void rgb_float_set_hue_float_offset(float rgb[3], float hue_offset)
{
	float hsv[3];

	rgb_to_hsv(rgb[0], rgb[1], rgb[2], hsv, hsv + 1, hsv + 2);

	hsv[0] += hue_offset;
	if (hsv[0] > 1.0f) hsv[0] -= 1.0f;
	else if (hsv[0] < 0.0f) hsv[0] += 1.0f;

	hsv_to_rgb(hsv[0], hsv[1], hsv[2], rgb, rgb + 1, rgb + 2);
}

/* Applies an hue offset to a byte rgb color */
void rgb_byte_set_hue_float_offset(unsigned char rgb[3], float hue_offset)
{
	float rgb_float[3];

	rgb_uchar_to_float(rgb_float, rgb);
	rgb_float_set_hue_float_offset(rgb_float, hue_offset);
	rgb_float_to_uchar(rgb, rgb_float);
}


/* fast sRGB conversion
 * LUT from linear float to 16-bit short
 * based on http://mysite.verizon.net/spitzak/conversion/
 */

float BLI_color_from_srgb_table[256];
unsigned short BLI_color_to_srgb_table[0x10000];

static unsigned short hipart(const float f)
{
	union {
		float f;
		unsigned short us[2];
	} tmp;

	tmp.f = f;

#ifdef __BIG_ENDIAN__
	return tmp.us[0];
#else
	return tmp.us[1];
#endif
}

static float index_to_float(const unsigned short i)
{

	union {
		float f;
		unsigned short us[2];
	} tmp;

	/* positive and negative zeros, and all gradual underflow, turn into zero: */
	if (i < 0x80 || (i >= 0x8000 && i < 0x8080)) return 0;
	/* All NaN's and infinity turn into the largest possible legal float: */
	if (i >= 0x7f80 && i < 0x8000) return FLT_MAX;
	if (i >= 0xff80) return -FLT_MAX;

#ifdef __BIG_ENDIAN__
	tmp.us[0] = i;
	tmp.us[1] = 0x8000;
#else
	tmp.us[0] = 0x8000;
	tmp.us[1] = i;
#endif

	return tmp.f;
}

void BLI_init_srgb_conversion(void)
{
	static bool initialized = false;
	unsigned int i, b;

	if (initialized)
		return;
	initialized = true;

	/* Fill in the lookup table to convert floats to bytes: */
	for (i = 0; i < 0x10000; i++) {
		float f = linearrgb_to_srgb(index_to_float((unsigned short)i)) * 255.0f;
		if (f <= 0) BLI_color_to_srgb_table[i] = 0;
		else if (f < 255) BLI_color_to_srgb_table[i] = (unsigned short) (f * 0x100 + 0.5f);
		else BLI_color_to_srgb_table[i] = 0xff00;
	}

	/* Fill in the lookup table to convert bytes to float: */
	for (b = 0; b <= 255; b++) {
		float f = srgb_to_linearrgb(((float)b) * (1.0f / 255.0f));
		BLI_color_from_srgb_table[b] = f;
		i = hipart(f);
		/* replace entries so byte->float->byte does not change the data: */
		BLI_color_to_srgb_table[i] = (unsigned short)(b * 0x100);
	}
}
static float inverse_srgb_companding(float v)
{
	if (v > 0.04045f) {
		return powf((v + 0.055f) / 1.055f, 2.4f);
	}
	else {
		return v / 12.92f;
	}
}

/**
 * \note Does sRGB to linear conversion
 */
void rgb_to_xyz(float r, float g, float b, float *x, float *y, float *z)
{
	r = inverse_srgb_companding(r) * 100.0f;
	g = inverse_srgb_companding(g) * 100.0f;
	b = inverse_srgb_companding(b) * 100.0f;

	*x = r * 0.412453f + g * 0.357580f + b * 0.180423f;
	*y = r * 0.212671f + g * 0.715160f + b * 0.072169f;
	*z = r * 0.019334f + g * 0.119193f + b * 0.950227f;
}

static float xyz_to_lab_component(float v)
{
	const float eps = 0.008856f;
	const float k = 903.3f;

	if (v > eps) {
		return powf(v, 1.0f / 3.0f);
	}
	else {
		return (k * v + 16.0f) / 116.0f;
	}
}

void xyz_to_lab(float x, float y, float z, float *l, float *a, float *b)
{
	const float xr = x / 95.047f;
	const float yr = y / 100.0f;
	const float zr = z / 108.883f;

	const float fx = xyz_to_lab_component(xr);
	const float fy = xyz_to_lab_component(yr);
	const float fz = xyz_to_lab_component(zr);

	*l = 116.0f * fy - 16.0f;
	*a = 500.0f * (fx - fy);
	*b = 200.0f * (fy - fz);
}

void rgb_to_lab(float r, float g, float b, float *ll, float *la, float *lb)
{
	float x, y, z;
	rgb_to_xyz(r, g, b, &x, &y, &z);
	xyz_to_lab(x, y, z, ll, la, lb);
}

static void xyz_to_lms(float x, float y, float z, float *l, float *m, float *s)
{
	*l =  0.3897f * x + 0.6890f * y - 0.0787f * z;
	*m = -0.2298f * x + 1.1834f * y + 0.0464f * z;
	*s = z;
}

static void lms_to_xyz(float l, float m, float s, float *x, float *y, float *z)
{
	*x =  1.9102f * l - 1.1121f * m + 0.2019f * s;
	*y =  0.3709f * l + 0.6290f * m + 0.0000f * s;
	*z = s;
}

static void normalize_rgb(float rgb[3])
{
	const float max = max_fff(rgb[0], rgb[1], rgb[2]);

	if (max > 0.0f) {
		mul_v3_fl(rgb, 1.0f / max);
	}
}

/* Color rendering of spectra, adapted from public domain code by John Walker,
 * http://www.fourmilab.ch/
 */

static void spectrum_to_xyz(float temperature, float xyz[3])
{
	int i;
	float lambda, x = 0.0f, y = 0.0f, z = 0.0f, xyz_sum;

	/* CIE colour matching functions xBar, yBar, and zBar for wavelengths from
	 * 380 through 780 nanometers, every 5 nanometers.
	 * For a wavelength lambda in this range:
	 *
	 * cie_colour_match[(lambda - 380) / 5][0] = xBar
	 * cie_colour_match[(lambda - 380) / 5][1] = yBar
	 * cie_colour_match[(lambda - 380) / 5][2] = zBar
	 */

	const float cie_colour_match[81][3] = {
	    {0.0014f, 0.0000f, 0.0065f}, {0.0022f, 0.0001f, 0.0105f}, {0.0042f, 0.0001f, 0.0201f},
	    {0.0076f, 0.0002f, 0.0362f}, {0.0143f, 0.0004f, 0.0679f}, {0.0232f, 0.0006f, 0.1102f},
	    {0.0435f, 0.0012f, 0.2074f}, {0.0776f, 0.0022f, 0.3713f}, {0.1344f, 0.0040f, 0.6456f},
	    {0.2148f, 0.0073f, 1.0391f}, {0.2839f, 0.0116f, 1.3856f}, {0.3285f, 0.0168f, 1.6230f},
	    {0.3483f, 0.0230f, 1.7471f}, {0.3481f, 0.0298f, 1.7826f}, {0.3362f, 0.0380f, 1.7721f},
	    {0.3187f, 0.0480f, 1.7441f}, {0.2908f, 0.0600f, 1.6692f}, {0.2511f, 0.0739f, 1.5281f},
	    {0.1954f, 0.0910f, 1.2876f}, {0.1421f, 0.1126f, 1.0419f}, {0.0956f, 0.1390f, 0.8130f},
	    {0.0580f, 0.1693f, 0.6162f}, {0.0320f, 0.2080f, 0.4652f}, {0.0147f, 0.2586f, 0.3533f},
	    {0.0049f, 0.3230f, 0.2720f}, {0.0024f, 0.4073f, 0.2123f}, {0.0093f, 0.5030f, 0.1582f},
	    {0.0291f, 0.6082f, 0.1117f}, {0.0633f, 0.7100f, 0.0782f}, {0.1096f, 0.7932f, 0.0573f},
	    {0.1655f, 0.8620f, 0.0422f}, {0.2257f, 0.9149f, 0.0298f}, {0.2904f, 0.9540f, 0.0203f},
	    {0.3597f, 0.9803f, 0.0134f}, {0.4334f, 0.9950f, 0.0087f}, {0.5121f, 1.0000f, 0.0057f},
	    {0.5945f, 0.9950f, 0.0039f}, {0.6784f, 0.9786f, 0.0027f}, {0.7621f, 0.9520f, 0.0021f},
	    {0.8425f, 0.9154f, 0.0018f}, {0.9163f, 0.8700f, 0.0017f}, {0.9786f, 0.8163f, 0.0014f},
	    {1.0263f, 0.7570f, 0.0011f}, {1.0567f, 0.6949f, 0.0010f}, {1.0622f, 0.6310f, 0.0008f},
	    {1.0456f, 0.5668f, 0.0006f}, {1.0026f, 0.5030f, 0.0003f}, {0.9384f, 0.4412f, 0.0002f},
	    {0.8544f, 0.3810f, 0.0002f}, {0.7514f, 0.3210f, 0.0001f}, {0.6424f, 0.2650f, 0.0000f},
	    {0.5419f, 0.2170f, 0.0000f}, {0.4479f, 0.1750f, 0.0000f}, {0.3608f, 0.1382f, 0.0000f},
	    {0.2835f, 0.1070f, 0.0000f}, {0.2187f, 0.0816f, 0.0000f}, {0.1649f, 0.0610f, 0.0000f},
	    {0.1212f, 0.0446f, 0.0000f}, {0.0874f, 0.0320f, 0.0000f}, {0.0636f, 0.0232f, 0.0000f},
	    {0.0468f, 0.0170f, 0.0000f}, {0.0329f, 0.0119f, 0.0000f}, {0.0227f, 0.0082f, 0.0000f},
	    {0.0158f, 0.0057f, 0.0000f}, {0.0114f, 0.0041f, 0.0000f}, {0.0081f, 0.0029f, 0.0000f},
	    {0.0058f, 0.0021f, 0.0000f}, {0.0041f, 0.0015f, 0.0000f}, {0.0029f, 0.0010f, 0.0000f},
	    {0.0020f, 0.0007f, 0.0000f}, {0.0014f, 0.0005f, 0.0000f}, {0.0010f, 0.0004f, 0.0000f},
	    {0.0007f, 0.0002f, 0.0000f}, {0.0005f, 0.0002f, 0.0000f}, {0.0003f, 0.0001f, 0.0000f},
	    {0.0002f, 0.0001f, 0.0000f}, {0.0002f, 0.0001f, 0.0000f}, {0.0001f, 0.0000f, 0.0000f},
	    {0.0001f, 0.0000f, 0.0000f}, {0.0001f, 0.0000f, 0.0000f}, {0.0000f, 0.0000f, 0.0000f}
	};

	for (i = 0, lambda = 380.0f; lambda < 780.1f; i++, lambda += 5.0f) {
		/* wavelength in meter */
		const float wlm = lambda * 1e-9f;
		const float Me = (3.74183e-16f * powf(wlm, -5.0f)) / (expf(1.4388e-2f / (wlm * temperature)) - 1.0f);

		x += Me * cie_colour_match[i][0];
		y += Me * cie_colour_match[i][1];
		z += Me * cie_colour_match[i][2];
	}

	xyz_sum = (x + y + z);

	xyz[0] = x / xyz_sum;
	xyz[1] = y / xyz_sum;
	xyz[2] = z / xyz_sum;
}

void blackbody_temperature_to_rgb_table(float *r_table, int width, float min, float max)
{
	int i, j = 0, dj = 1;
	float rgb[3], xyz[3], lms[3], lms_w[3];
	float bb_temp;

	if (min < max) {
		SWAP(float, min, max);
		j = width - 1;
		dj = -1;
	}

	for (i = 0; i < width; i++, j += dj) {
		bb_temp = min + (max - min) / (float)width * (float)i;

		/* integrate blackbody radiation spectrum to XYZ */
		spectrum_to_xyz(bb_temp, xyz);

		/* normalize highest temperature to white (in LMS system) */
		xyz_to_lms(xyz[0], xyz[1], xyz[2], &lms[0], &lms[1], &lms[2]);

		if (i == 0) {
			lms_w[0] = 1.0f / lms[0];
			lms_w[1] = 1.0f / lms[1];
			lms_w[2] = 1.0f / lms[2];
		}

		mul_v3_v3(lms, lms_w);

		lms_to_xyz(lms[0], lms[1], lms[2], &xyz[0], &xyz[1], &xyz[2]);

		/* convert to RGB */
		xyz_to_rgb(xyz[0], xyz[1], xyz[2], &rgb[0], &rgb[1], &rgb[2], BLI_XYZ_CIE);
		constrain_rgb(&rgb[0], &rgb[1], &rgb[2]);
		normalize_rgb(rgb);

		copy_v3_v3(&r_table[(j << 2)], rgb);

		if (rgb[2] > 0.1f)
			r_table[(j << 2) + 3] = rgb[2];
		else
			r_table[(j << 2) + 3] = 0.0f;
	}
}
