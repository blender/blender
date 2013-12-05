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
#include "BLI_utildefines.h"

#ifndef __MATH_COLOR_BLEND_INLINE_C__
#define __MATH_COLOR_BLEND_INLINE_C__

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
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
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
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
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
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
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
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
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
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
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
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
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
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
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
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
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
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
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
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
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
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
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
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
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
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
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
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
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
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
	}
}

MINLINE void blend_color_erase_alpha_float(float dst[4], const float src1[4], const float src2[4])
{
	if (src2[3] != 0.0f && src1[3] > 0.0f) {
		/* subtract alpha and remap RGB channels to match */
		float alpha = max_ff(src1[3] - src2[3], 0.0f);
		float map_alpha;

		if (alpha <= 0.0005f)
			alpha = 0.0f;

		map_alpha = alpha / src1[3];

		dst[0] = src1[0] * map_alpha;
		dst[1] = src1[1] * map_alpha;
		dst[2] = src1[2] * map_alpha;
		dst[3] = alpha;
	}
	else {
		/* no op */
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
	}
}

MINLINE void blend_color_add_alpha_float(float dst[4], const float src1[4], const float src2[4])
{
	if (src2[3] != 0.0f && src1[3] < 1.0f) {
		/* add alpha and remap RGB channels to match */
		float alpha = min_ff(src1[3] + src2[3], 1.0f);
		float map_alpha;

		if (alpha >= 1.0f - 0.0005f)
			alpha = 1.0f;

		map_alpha = (src1[3] > 0.0f) ? alpha / src1[3] : 1.0f;

		dst[0] = src1[0] * map_alpha;
		dst[1] = src1[1] * map_alpha;
		dst[2] = src1[2] * map_alpha;
		dst[3] = alpha;
	}
	else {
		/* no op */
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
	}
}

MINLINE void blend_color_interpolate_float(float dst[4], const float src1[4], const float src2[4], float t)
{
	/* interpolation, colors are premultiplied so it goes fine */
	float mt = 1.0f - t;

	dst[0] = mt * src1[0] + t * src2[0];
	dst[1] = mt * src1[1] + t * src2[1];
	dst[2] = mt * src1[2] + t * src2[2];
	dst[3] = mt * src1[3] + t * src2[3];
}

#endif /* __MATH_COLOR_BLEND_INLINE_C__ */
