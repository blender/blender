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

/** \file blender/blenlib/intern/math_color_blend_inline.c
 *  \ingroup bli
 */

#include "BLI_math_base.h"
#include "BLI_math_color.h"
#include "BLI_math_color_blend.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#ifndef __MATH_COLOR_BLEND_INLINE_C__
#define __MATH_COLOR_BLEND_INLINE_C__

/* don't add any saturation to a completly black and white image */
#define EPS_SATURATION 0.0005f
#define EPS_ALPHA 0.0005f

/***************************** Color Blending ********************************
 *
 * - byte colors are assumed to be straight alpha
 * - byte colors uses to do >>8 (same as /256) but actually should do /255,
 *   otherwise get quick darkening due to rounding
 * - divide_round_i is also used to avoid darkening due to integers always
 *   rounding down
 * - float colors are assumed to be premultiplied alpha
 */

/* straight alpha byte blending modes */

MINLINE void blend_color_mix_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4])
{
	if (src2[3] != 0) {
		/* straight over operation */
		const int t = src2[3];
		const int mt = 255 - t;
		int tmp[4];

		tmp[0] = (mt * src1[3] * src1[0]) + (t * 255 * src2[0]);
		tmp[1] = (mt * src1[3] * src1[1]) + (t * 255 * src2[1]);
		tmp[2] = (mt * src1[3] * src1[2]) + (t * 255 * src2[2]);
		tmp[3] = (mt * src1[3]) + (t * 255);

		dst[0] = (unsigned char)divide_round_i(tmp[0], tmp[3]);
		dst[1] = (unsigned char)divide_round_i(tmp[1], tmp[3]);
		dst[2] = (unsigned char)divide_round_i(tmp[2], tmp[3]);
		dst[3] = (unsigned char)divide_round_i(tmp[3], 255);
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}

MINLINE void blend_color_add_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4])
{
	if (src2[3] != 0) {
		/* straight add operation */
		const int t = src2[3];
		int tmp[3];

		tmp[0] = (src1[0] * 255) + (src2[0] * t);
		tmp[1] = (src1[1] * 255) + (src2[1] * t);
		tmp[2] = (src1[2] * 255) + (src2[2] * t);

		dst[0] = (unsigned char)min_ii(divide_round_i(tmp[0], 255), 255);
		dst[1] = (unsigned char)min_ii(divide_round_i(tmp[1], 255), 255);
		dst[2] = (unsigned char)min_ii(divide_round_i(tmp[2], 255), 255);
		dst[3] = src1[3];
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}

MINLINE void blend_color_sub_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4])
{
	if (src2[3] != 0) {
		/* straight sub operation */
		const int t = src2[3];
		int tmp[3];

		tmp[0] = (src1[0] * 255) - (src2[0] * t);
		tmp[1] = (src1[1] * 255) - (src2[1] * t);
		tmp[2] = (src1[2] * 255) - (src2[2] * t);

		dst[0] = (unsigned char)max_ii(divide_round_i(tmp[0], 255), 0);
		dst[1] = (unsigned char)max_ii(divide_round_i(tmp[1], 255), 0);
		dst[2] = (unsigned char)max_ii(divide_round_i(tmp[2], 255), 0);
		dst[3] = src1[3];
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}

MINLINE void blend_color_mul_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4])
{
	if (src2[3] != 0) {
		/* straight multiply operation */
		const int t = src2[3];
		const int mt = 255 - t;
		int tmp[3];

		tmp[0] = (mt * src1[0] * 255) + (t * src1[0] * src2[0]);
		tmp[1] = (mt * src1[1] * 255) + (t * src1[1] * src2[1]);
		tmp[2] = (mt * src1[2] * 255) + (t * src1[2] * src2[2]);

		dst[0] = (unsigned char)divide_round_i(tmp[0], 255 * 255);
		dst[1] = (unsigned char)divide_round_i(tmp[1], 255 * 255);
		dst[2] = (unsigned char)divide_round_i(tmp[2], 255 * 255);
		dst[3] = src1[3];
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}

MINLINE void blend_color_lighten_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4])
{
	if (src2[3] != 0) {
		/* straight lighten operation */
		const int t = src2[3];
		const int mt = 255 - t;
		int tmp[3];

		tmp[0] = (mt * src1[0]) + (t * max_ii(src1[0], src2[0]));
		tmp[1] = (mt * src1[1]) + (t * max_ii(src1[1], src2[1]));
		tmp[2] = (mt * src1[2]) + (t * max_ii(src1[2], src2[2]));

		dst[0] = (unsigned char)divide_round_i(tmp[0], 255);
		dst[1] = (unsigned char)divide_round_i(tmp[1], 255);
		dst[2] = (unsigned char)divide_round_i(tmp[2], 255);
		dst[3] = src1[3];
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}

MINLINE void blend_color_darken_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4])
{
	if (src2[3] != 0) {
		/* straight darken operation */
		const int t = src2[3];
		const int mt = 255 - t;
		int tmp[3];

		tmp[0] = (mt * src1[0]) + (t * min_ii(src1[0], src2[0]));
		tmp[1] = (mt * src1[1]) + (t * min_ii(src1[1], src2[1]));
		tmp[2] = (mt * src1[2]) + (t * min_ii(src1[2], src2[2]));

		dst[0] = (unsigned char)divide_round_i(tmp[0], 255);
		dst[1] = (unsigned char)divide_round_i(tmp[1], 255);
		dst[2] = (unsigned char)divide_round_i(tmp[2], 255);
		dst[3] = src1[3];
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}

MINLINE void blend_color_erase_alpha_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4])
{
	if (src2[3] != 0) {
		/* straight so just modify alpha channel */
		const int t = src2[3];

		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = (unsigned char)max_ii(src1[3] - divide_round_i(t * src2[3], 255), 0);
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}

MINLINE void blend_color_add_alpha_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4])
{
	if (src2[3] != 0) {
		/* straight so just modify alpha channel */
		const int t = src2[3];

		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = (unsigned char)min_ii(src1[3] + divide_round_i(t * src2[3], 255), 255);
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}

MINLINE void blend_color_overlay_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = (int)src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		int i = 3;

		while (i--) {
			int temp;

			if (src1[i] > 127) {
				temp = 255 - ((255 - 2 * (src1[i] - 127)) * (255 - src2[i]) / 255);
			}
			else {
				temp = (2 * src1[i] * src2[i]) >> 8;
			}
			dst[i] = (unsigned char)min_ii((temp * fac + src1[i] * mfac) / 255, 255);
		}
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}


MINLINE void blend_color_hardlight_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = (int)src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		int i = 3;

		while (i--) {
			int temp;

			if (src2[i] > 127) {
				temp = 255 - ((255 - 2 * (src2[i] - 127)) * (255 - src1[i]) / 255);
			}
			else {
				temp = (2 * src2[i] * src1[i]) >> 8;
			}
			dst[i] = (unsigned char)min_ii((temp * fac + src1[i] * mfac) / 255, 255);
		}
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}


MINLINE void blend_color_burn_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		int i = 3;

		while (i--) {
			const int temp = (src2[i] == 0) ? 0 : max_ii(255 - ((255 - src1[i]) * 255) / src2[i], 0);
			dst[i] = (unsigned char)((temp * fac + src1[i] * mfac) / 255);
		}
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}


MINLINE void blend_color_linearburn_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		int i = 3;

		while (i--) {
			const int temp = max_ii(src1[i] + src2[i] - 255, 0);
			dst[i] = (unsigned char)((temp * fac + src1[i] * mfac) / 255);
		}
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}


MINLINE void blend_color_dodge_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		int i = 3;

		while (i--) {
			const int temp = (src2[i] == 255) ? 255 : min_ii((src1[i] * 255) / (255 - src2[i]), 255);
			dst[i] = (unsigned char)((temp * fac + src1[i] * mfac) / 255);
		}
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}

MINLINE void blend_color_screen_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		int i = 3;

		while (i--) {
			const int temp = max_ii(255 - (((255 - src1[i]) * (255 - src2[i])) / 255), 0);
			dst[i] = (unsigned char)((temp * fac + src1[i] * mfac) / 255);
		}
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}


MINLINE void blend_color_softlight_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		int i = 3;

		while (i--) {
			int temp;

			if (src1[i] < 127) {
				temp = ((2 * ((src2[i] / 2) + 64)) * src1[i]) / 255;
			}
			else {
				temp = 255 - (2 * (255 - ((src2[i] / 2) + 64)) * (255 - src1[i]) / 255);
			}
			dst[i] = (unsigned char)((temp * fac + src1[i] * mfac) / 255);
		}
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}


MINLINE void blend_color_pinlight_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		int i = 3;

		while (i--) {
			int temp;

			if (src2[i] > 127) {
				temp = max_ii(2 * (src2[i] - 127), src1[i]);
			}
			else {
				temp = min_ii(2 * src2[i], src1[i]);
			}
			dst[i] = (unsigned char)((temp * fac + src1[i] * mfac) / 255);
		}
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}


MINLINE void blend_color_linearlight_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		int i = 3;

		while (i--) {
			int temp;

			if (src2[i] > 127) {
				temp = min_ii(src1[i] + 2 * (src2[i] - 127), 255);
			}
			else {
				temp = max_ii(src1[i] + 2 * src2[i] - 255, 0);
			}
			dst[i] = (unsigned char)((temp * fac + src1[i] * mfac) / 255);
		}
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}


MINLINE void blend_color_vividlight_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		int i = 3;

		while (i--) {
			int temp;

			if (src2[i] == 255) {
				temp = 255;
			}
			else if (src2[i] == 0) {
				temp = 0;
			}
			else if (src2[i] > 127)  {
				temp = min_ii(((src1[i]) * 255) / (2 * (255 - src2[i])), 255);
			}
			else {
				temp = max_ii(255 - ((255 - src1[i]) * 255 / (2 * src2[i])), 0);
			}
			dst[i] = (unsigned char)((temp * fac + src1[i] * mfac) / 255);
		}
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}



MINLINE void blend_color_difference_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		int i = 3;

		while (i--) {
			const int temp = abs(src1[i] - src2[i]);
			dst[i] = (unsigned char)((temp * fac + src1[i] * mfac) / 255);
		}
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}


MINLINE void blend_color_exclusion_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		int i = 3;

		while (i--) {
			const int temp = 127 - ((2 * (src1[i] - 127) * (src2[i] - 127)) / 255);
			dst[i] = (unsigned char)((temp * fac + src1[i] * mfac) / 255);
		}
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}

MINLINE void blend_color_color_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		float h1, s1, v1;
		float h2, s2, v2;
		float r, g, b;
		rgb_to_hsv(src1[0] / 255.0f, src1[1] / 255.0f, src1[2] / 255.0f, &h1, &s1, &v1);
		rgb_to_hsv(src2[0] / 255.0f, src2[1] / 255.0f, src2[2] / 255.0f, &h2, &s2, &v2);


		h1 = h2;
		s1 = s2;

		hsv_to_rgb(h1, s1, v1, &r, &g, &b);

		dst[0] = (unsigned char)(((int)(r * 255.0f) * fac + src1[0] * mfac) / 255);
		dst[1] = (unsigned char)(((int)(g * 255.0f) * fac + src1[1] * mfac) / 255);
		dst[2] = (unsigned char)(((int)(b * 255.0f) * fac + src1[2] * mfac) / 255);
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}

MINLINE void blend_color_hue_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		float h1, s1, v1;
		float h2, s2, v2;
		float r, g, b;
		rgb_to_hsv(src1[0] / 255.0f, src1[1] / 255.0f, src1[2] / 255.0f, &h1, &s1, &v1);
		rgb_to_hsv(src2[0] / 255.0f, src2[1] / 255.0f, src2[2] / 255.0f, &h2, &s2, &v2);


		h1 = h2;

		hsv_to_rgb(h1, s1, v1, &r, &g, &b);

		dst[0] = (unsigned char)(((int)(r * 255.0f) * fac + src1[0] * mfac) / 255);
		dst[1] = (unsigned char)(((int)(g * 255.0f) * fac + src1[1] * mfac) / 255);
		dst[2] = (unsigned char)(((int)(b * 255.0f) * fac + src1[2] * mfac) / 255);
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}

}

MINLINE void blend_color_saturation_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		float h1, s1, v1;
		float h2, s2, v2;
		float r, g, b;
		rgb_to_hsv(src1[0] / 255.0f, src1[1] / 255.0f, src1[2] / 255.0f, &h1, &s1, &v1);
		rgb_to_hsv(src2[0] / 255.0f, src2[1] / 255.0f, src2[2] / 255.0f, &h2, &s2, &v2);

		if (s1 > EPS_SATURATION) {
			s1 = s2;
		}

		hsv_to_rgb(h1, s1, v1, &r, &g, &b);

		dst[0] = (unsigned char)(((int)(r * 255.0f) * fac + src1[0] * mfac) / 255);
		dst[1] = (unsigned char)(((int)(g * 255.0f) * fac + src1[1] * mfac) / 255);
		dst[2] = (unsigned char)(((int)(b * 255.0f) * fac + src1[2] * mfac) / 255);
	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}

MINLINE void blend_color_luminosity_byte(unsigned char dst[4], unsigned const char src1[4], unsigned const char src2[4])
{
	const int fac = src2[3];
	if (fac != 0) {
		const int mfac = 255 - fac;
		float h1, s1, v1;
		float h2, s2, v2;
		float r, g, b;
		rgb_to_hsv(src1[0] / 255.0f, src1[1] / 255.0f, src1[2] / 255.0f, &h1, &s1, &v1);
		rgb_to_hsv(src2[0] / 255.0f, src2[1] / 255.0f, src2[2] / 255.0f, &h2, &s2, &v2);

		v1 = v2;

		hsv_to_rgb(h1, s1, v1, &r, &g, &b);

		dst[0] = (unsigned char)(((int)(r * 255.0f) * fac + src1[0] * mfac) / 255);
		dst[1] = (unsigned char)(((int)(g * 255.0f) * fac + src1[1] * mfac) / 255);
		dst[2] = (unsigned char)(((int)(b * 255.0f) * fac + src1[2] * mfac) / 255);

	}
	else {
		/* no op */
		copy_v4_v4_char((char *)dst, (char *)src1);
	}

}

MINLINE void blend_color_interpolate_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4], float ft)
{
	/* do color interpolation, but in premultiplied space so that RGB colors
	 * from zero alpha regions have no influence */
	const int t = (int)(255 * ft);
	const int mt = 255 - t;
	int tmp = (mt * src1[3] + t * src2[3]);

	if (tmp > 0) {
		dst[0] = (unsigned char)divide_round_i(mt * src1[0] * src1[3] + t * src2[0] * src2[3], tmp);
		dst[1] = (unsigned char)divide_round_i(mt * src1[1] * src1[3] + t * src2[1] * src2[3], tmp);
		dst[2] = (unsigned char)divide_round_i(mt * src1[2] * src1[3] + t * src2[2] * src2[3], tmp);
		dst[3] = (unsigned char)divide_round_i(tmp, 255);
	}
	else {
		copy_v4_v4_char((char *)dst, (char *)src1);
	}
}

/* premultiplied alpha float blending modes */

MINLINE void blend_color_mix_float(float dst[4], const float src1[4], const float src2[4])
{
	if (src2[3] != 0.0f) {
		/* premul over operation */
		const float t = src2[3];
		const float mt = 1.0f - t;

		dst[0] = mt * src1[0] + src2[0];
		dst[1] = mt * src1[1] + src2[1];
		dst[2] = mt * src1[2] + src2[2];
		dst[3] = mt * src1[3] + t;
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_add_float(float dst[4], const float src1[4], const float src2[4])
{
	if (src2[3] != 0.0f) {
		/* unpremul > add > premul, simplified */
		dst[0] = src1[0] + src2[0] * src1[3];
		dst[1] = src1[1] + src2[1] * src1[3];
		dst[2] = src1[2] + src2[2] * src1[3];
		dst[3] = src1[3];
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_sub_float(float dst[4], const float src1[4], const float src2[4])
{
	if (src2[3] != 0.0f) {
		/* unpremul > subtract > premul, simplified */
		dst[0] = max_ff(src1[0] - src2[0] * src1[3], 0.0f);
		dst[1] = max_ff(src1[1] - src2[1] * src1[3], 0.0f);
		dst[2] = max_ff(src1[2] - src2[2] * src1[3], 0.0f);
		dst[3] = src1[3];
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_mul_float(float dst[4], const float src1[4], const float src2[4])
{
	if (src2[3] != 0.0f) {
		/* unpremul > multiply > premul, simplified */
		const float t = src2[3];
		const float mt = 1.0f - t;

		dst[0] = mt * src1[0] + src1[0] * src2[0] * src1[3];
		dst[1] = mt * src1[1] + src1[1] * src2[1] * src1[3];
		dst[2] = mt * src1[2] + src1[2] * src2[2] * src1[3];
		dst[3] = src1[3];
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_lighten_float(float dst[4], const float src1[4], const float src2[4])
{
	if (src2[3] != 0.0f) {
		/* remap src2 to have same alpha as src1 premultiplied, take maximum of
		 * src1 and src2, then blend it with src1 */
		const float t = src2[3];
		const float mt = 1.0f - t;
		const float map_alpha = src1[3] / src2[3];

		dst[0] = mt * src1[0] + t * max_ff(src1[0], src2[0] * map_alpha);
		dst[1] = mt * src1[1] + t * max_ff(src1[1], src2[1] * map_alpha);
		dst[2] = mt * src1[2] + t * max_ff(src1[2], src2[2] * map_alpha);
		dst[3] = src1[3];
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_darken_float(float dst[4], const float src1[4], const float src2[4])
{
	if (src2[3] != 0.0f) {
		/* remap src2 to have same alpha as src1 premultiplied, take minimum of
		 * src1 and src2, then blend it with src1 */
		const float t = src2[3];
		const float mt = 1.0f - t;
		const float map_alpha = src1[3] / src2[3];

		dst[0] = mt * src1[0] + t * min_ff(src1[0], src2[0] * map_alpha);
		dst[1] = mt * src1[1] + t * min_ff(src1[1], src2[1] * map_alpha);
		dst[2] = mt * src1[2] + t * min_ff(src1[2], src2[2] * map_alpha);
		dst[3] = src1[3];
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_erase_alpha_float(float dst[4], const float src1[4], const float src2[4])
{
	if (src2[3] != 0.0f && src1[3] > 0.0f) {
		/* subtract alpha and remap RGB channels to match */
		float alpha = max_ff(src1[3] - src2[3], 0.0f);
		float map_alpha;

		if (alpha <= EPS_ALPHA) {
			alpha = 0.0f;
		}

		map_alpha = alpha / src1[3];

		dst[0] = src1[0] * map_alpha;
		dst[1] = src1[1] * map_alpha;
		dst[2] = src1[2] * map_alpha;
		dst[3] = alpha;
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_add_alpha_float(float dst[4], const float src1[4], const float src2[4])
{
	if (src2[3] != 0.0f && src1[3] < 1.0f) {
		/* add alpha and remap RGB channels to match */
		float alpha = min_ff(src1[3] + src2[3], 1.0f);
		float map_alpha;

		if (alpha >= 1.0f - EPS_ALPHA) {
			alpha = 1.0f;
		}

		map_alpha = (src1[3] > 0.0f) ? alpha / src1[3] : 1.0f;

		dst[0] = src1[0] * map_alpha;
		dst[1] = src1[1] * map_alpha;
		dst[2] = src1[2] * map_alpha;
		dst[3] = alpha;
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_overlay_float(float dst[3], const float src1[3], const float src2[3])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		int i = 3;

		while (i--) {
			float temp;

			if (src1[i] > 0.5f) {
				temp = 1.0f - (1.0f - 2.0f * (src1[i] - 0.5f)) * (1.0f - src2[i]);
			}
			else {
				temp = 2.0f * src1[i] * src2[i];
			}
			dst[i] = min_ff(temp * fac + src1[i] * mfac, 1.0f);
		}
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}


MINLINE void blend_color_hardlight_float(float dst[4], const float src1[4], const float src2[2])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		int i = 3;

		while (i--) {
			float temp;

			if (src2[i] > 0.5f) {
				temp = 1.0f - ((1.0f - 2.0f * (src2[i] - 0.5f)) * (1.0f - src1[i]));
			}
			else {
				temp = 2.0f * src2[i] * src1[i];
			}
			dst[i] = min_ff((temp * fac + src1[i] * mfac) / 1.0f, 1.0f);
		}
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_burn_float(float dst[3], const float src1[3], const float src2[3])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		int i = 3;

		while (i--) {
			const float temp = (src2[i] == 0.0f) ? 0.0f : max_ff(1.0f - ((1.0f - src1[i]) / src2[i]), 0.0f);
			dst[i] = (temp * fac + src1[i] * mfac);
		}
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_linearburn_float(float dst[3], const float src1[3], const float src2[3])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		int i = 3;

		while (i--) {
			const float temp = max_ff(src1[i] + src2[i] - 1.0f, 0.0f);
			dst[i] = (temp * fac + src1[i] * mfac);
		}
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}


MINLINE void blend_color_dodge_float(float dst[3], const float src1[3], const float src2[3])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		int i = 3;

		while (i--) {
			const float temp = (src2[i] >= 1.0f) ? 1.0f : min_ff(src1[i] / (1.0f - src2[i]), 1.0f);
			dst[i] = (temp * fac + src1[i] * mfac);
		}
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_screen_float(float dst[3], const float src1[3], const float src2[3])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		int i = 3;

		while (i--) {
			const float temp = max_ff(1.0f - ((1.0f - src1[i]) * (1.0f - src2[i])), 0.0f);
			dst[i] = (temp * fac + src1[i] * mfac);
		}
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_softlight_float(float dst[3], const float src1[3], const float src2[3])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		int i = 3;

		while (i--) {
			float temp;

			if (src1[i] < 0.5f) {
				temp = (src2[i] + 0.5f) * src1[i];
			}
			else {
				temp = 1.0f - ((1.0f - (src2[i] + 0.5f)) * (1.0f - src1[i]));
			}
			dst[i] = (temp * fac + src1[i] * mfac);
		}
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_pinlight_float(float dst[3], const float src1[3], const float src2[3])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		int i = 3;

		while (i--) {
			float temp;

			if (src2[i] > 0.5f) {
				temp = max_ff(2.0f * (src2[i] - 0.5f), src1[i]);
			}
			else {
				temp = min_ff(2.0f * src2[i], src1[i]);
			}
			dst[i] = (temp * fac + src1[i] * mfac);
		}
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}


MINLINE void blend_color_linearlight_float(float dst[3], const float src1[3], const float src2[3])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		int i = 3;

		while (i--) {
			float temp;

			if (src2[i] > 0.5f) {
				temp = min_ff(src1[i] + 2.0f * (src2[i] - 0.5f), 1.0f);
			}
			else {
				temp = max_ff(src1[i] + 2.0f * src2[i] - 1.0f, 0.0f);
			}
			dst[i] = (temp * fac + src1[i] * mfac);
		}
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}


MINLINE void blend_color_vividlight_float(float dst[3], const float src1[3], const float src2[3])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		int i = 3;

		while (i--) {
			float temp;

			if (src2[i] == 1.0f) {
				temp = 1.0f;
			}
			else if (src2[i] == 0.0f) {
				temp = 0.0f;
			}
			else if (src2[i] > 0.5f) {
				temp = min_ff(((src1[i]) * 1.0f) / (2.0f * (1.0f - src2[i])), 1.0f);
			}
			else {
				temp = max_ff(1.0f - ((1.0f - src1[i]) * 1.0f / (2.0f * src2[i])), 0.0f);
			}
			dst[i] = (temp * fac + src1[i] * mfac);
		}
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_difference_float(float dst[3], const float src1[3], const float src2[3])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		int i = 3;

		while (i--) {
			dst[i] = (fabsf(src1[i] - src2[i]) * fac + src1[i] * mfac);
		}
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}


MINLINE void blend_color_exclusion_float(float dst[3], const float src1[3], const float src2[3])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		int i = 3;

		while (i--) {
			const float temp = 0.5f - ((2 * (src1[i] - 0.5f) * (src2[i] - 0.5f)));
			dst[i] = (temp * fac + src1[i] * mfac);
		}
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}

}

MINLINE void blend_color_color_float(float dst[3], const float src1[3], const float src2[3])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		float h1, s1, v1;
		float h2, s2, v2;
		float r, g, b;

		rgb_to_hsv(src1[0], src1[1], src1[2], &h1, &s1, &v1);
		rgb_to_hsv(src2[0], src2[1], src2[2], &h2, &s2, &v2);

		h1 = h2;
		s1 = s2;

		hsv_to_rgb(h1, s1, v1, &r, &g, &b);

		dst[0] = (r * fac + src1[0] * mfac);
		dst[1] = (g * fac + src1[1] * mfac);
		dst[2] = (b * fac + src1[2] * mfac);
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}


MINLINE void blend_color_hue_float(float dst[3], const float src1[3], const float src2[3])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		float h1, s1, v1;
		float h2, s2, v2;
		float r, g, b;

		rgb_to_hsv(src1[0], src1[1], src1[2], &h1, &s1, &v1);
		rgb_to_hsv(src2[0], src2[1], src2[2], &h2, &s2, &v2);

		h1 = h2;

		hsv_to_rgb(h1, s1, v1, &r, &g, &b);

		dst[0] = (r * fac + src1[0] * mfac);
		dst[1] = (g * fac + src1[1] * mfac);
		dst[2] = (b * fac + src1[2] * mfac);
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_saturation_float(float dst[3], const float src1[3], const float src2[3])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		float h1, s1, v1;
		float h2, s2, v2;
		float r, g, b;

		rgb_to_hsv(src1[0], src1[1], src1[2], &h1, &s1, &v1);
		rgb_to_hsv(src2[0], src2[1], src2[2], &h2, &s2, &v2);

		if (s1 > EPS_SATURATION) {
			s1 = s2;
		}
		hsv_to_rgb(h1, s1, v1, &r, &g, &b);

		dst[0] = (r * fac + src1[0] * mfac);
		dst[1] = (g * fac + src1[1] * mfac);
		dst[2] = (b * fac + src1[2] * mfac);
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}

MINLINE void blend_color_luminosity_float(float dst[3], const float src1[3], const float src2[3])
{
	const float fac = src2[3];
	if (fac != 0.0f && fac < 1.0f) {
		const float mfac = 1.0f - fac;
		float h1, s1, v1;
		float h2, s2, v2;
		float r, g, b;

		rgb_to_hsv(src1[0], src1[1], src1[2], &h1, &s1, &v1);
		rgb_to_hsv(src2[0], src2[1], src2[2], &h2, &s2, &v2);

		v1 = v2;
		hsv_to_rgb(h1, s1, v1, &r, &g, &b);

		dst[0] = (r * fac + src1[0] * mfac);
		dst[1] = (g * fac + src1[1] * mfac);
		dst[2] = (b * fac + src1[2] * mfac);
	}
	else {
		/* no op */
		copy_v4_v4(dst, src1);
	}
}


MINLINE void blend_color_interpolate_float(float dst[4], const float src1[4], const float src2[4], float t)
{
	/* interpolation, colors are premultiplied so it goes fine */
	const float mt = 1.0f - t;

	dst[0] = mt * src1[0] + t * src2[0];
	dst[1] = mt * src1[1] + t * src2[1];
	dst[2] = mt * src1[2] + t * src2[2];
	dst[3] = mt * src1[3] + t * src2[3];
}

#undef EPS_SATURATION
#undef EPS_ALPHA

#endif /* __MATH_COLOR_BLEND_INLINE_C__ */
