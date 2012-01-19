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

/** \file blender/blenlib/intern/math_color_inline.c
 *  \ingroup bli
 */


#include "BLI_math_color.h"
#include "BLI_utildefines.h"

#ifndef BLI_MATH_COLOR_INLINE_H
#define BLI_MATH_COLOR_INLINE_H

/******************************** Color Space ********************************/

MINLINE void srgb_to_linearrgb_v3_v3(float linear[3], const float srgb[3])
{
	linear[0] = srgb_to_linearrgb(srgb[0]);
	linear[1] = srgb_to_linearrgb(srgb[1]);
	linear[2] = srgb_to_linearrgb(srgb[2]);
}

MINLINE void linearrgb_to_srgb_v3_v3(float srgb[3], const float linear[3])
{
	srgb[0] = linearrgb_to_srgb(linear[0]);
	srgb[1] = linearrgb_to_srgb(linear[1]);
	srgb[2] = linearrgb_to_srgb(linear[2]);
}

MINLINE void srgb_to_linearrgb_v4(float linear[4], const float srgb[4])
{
	srgb_to_linearrgb_v3_v3(linear, srgb);
	linear[3] = srgb[3];
}

MINLINE void linearrgb_to_srgb_v4(float srgb[4], const float linear[4])
{
	linearrgb_to_srgb_v3_v3(srgb, linear);
	srgb[3] = linear[3];
}

MINLINE void linearrgb_to_srgb_uchar3(unsigned char srgb[4], const float linear[4])
{
	int r, g, b;

	r = 255 * linearrgb_to_srgb(linear[0]) * 255;
	g = 255 * linearrgb_to_srgb(linear[1]) * 255;
	b = 255 * linearrgb_to_srgb(linear[2]) * 255;

	srgb[0] = FTOCHAR(r);
	srgb[1] = FTOCHAR(g);
	srgb[2] = FTOCHAR(b);
}

MINLINE void linearrgb_to_srgb_uchar4(unsigned char srgb[4], const float linear[4])
{
	int r, g, b, a;

	r = 255 * linearrgb_to_srgb(linear[0]) * 255;
	g = 255 * linearrgb_to_srgb(linear[1]) * 255;
	b = 255 * linearrgb_to_srgb(linear[2]) * 255;
	a = 255 * linear[3];

	srgb[0] = FTOCHAR(r);
	srgb[1] = FTOCHAR(g);
	srgb[2] = FTOCHAR(b);
	srgb[3] = FTOCHAR(a);
}

/* predivide versions to work on associated/premultipled alpha. if this should
   be done or not depends on the background the image will be composited over,
   ideally you would never do color space conversion on an image with alpha
   because it is ill defined */

MINLINE void srgb_to_linearrgb_predivide_v4(float linear[4], const float srgb[4])
{
	float alpha, inv_alpha;

	if(srgb[3] == 1.0f || srgb[3] == 0.0f) {
		alpha = 1.0f;
		inv_alpha = 1.0f;
	}
	else {
		alpha = srgb[3];
		inv_alpha = 1.0f/alpha;
	}

	linear[0] = srgb_to_linearrgb(srgb[0] * inv_alpha) * alpha;
	linear[1] = srgb_to_linearrgb(srgb[1] * inv_alpha) * alpha;
	linear[2] = srgb_to_linearrgb(srgb[2] * inv_alpha) * alpha;
	linear[3] = srgb[3];
}

MINLINE void linearrgb_to_srgb_predivide_v4(float srgb[4], const float linear[4])
{
	float alpha, inv_alpha;

	if(linear[3] == 1.0f || linear[3] == 0.0f) {
		alpha = 1.0f;
		inv_alpha = 1.0f;
	}
	else {
		alpha = linear[3];
		inv_alpha = 1.0f/alpha;
	}

	srgb[0] = linearrgb_to_srgb(linear[0] * inv_alpha) * alpha;
	srgb[1] = linearrgb_to_srgb(linear[1] * inv_alpha) * alpha;
	srgb[2] = linearrgb_to_srgb(linear[2] * inv_alpha) * alpha;
	srgb[3] = linear[3];
}

/* LUT accelerated conversions */

extern float BLI_color_from_srgb_table[256];
extern unsigned short BLI_color_to_srgb_table[0x10000];

MINLINE unsigned short to_srgb_table_lookup(const float f)
{
	union {
		float f;
		unsigned short us[2];
	} tmp;
	tmp.f = f;
#ifdef __BIG_ENDIAN__
	return BLI_color_to_srgb_table[tmp.us[0]];
#else
	return BLI_color_to_srgb_table[tmp.us[1]];
#endif
}

MINLINE void linearrgb_to_srgb_ushort4(unsigned short srgb[4], const float linear[4])
{
	srgb[0] = to_srgb_table_lookup(linear[0]);
	srgb[1] = to_srgb_table_lookup(linear[1]);
	srgb[2] = to_srgb_table_lookup(linear[2]);
	srgb[3] = FTOUSHORT(linear[3]);
}

MINLINE void linearrgb_to_srgb_ushort4_predivide(unsigned short srgb[4], const float linear[4])
{
	float alpha, inv_alpha, t;
	int i;

	if(linear[3] == 1.0f || linear[3] == 0.0f) {
		linearrgb_to_srgb_ushort4(srgb, linear);
		return;
	}

	alpha = linear[3];
	inv_alpha = 1.0f/alpha;

	for(i=0; i<3; ++i) {
		t = linear[i] * inv_alpha;
		srgb[i] = (t < 1.0f)? to_srgb_table_lookup(t) * alpha : FTOUSHORT(linearrgb_to_srgb(t) * alpha);
	}

	srgb[3] = FTOUSHORT(linear[3]);
}

MINLINE void srgb_to_linearrgb_uchar4(float linear[4], const unsigned char srgb[4])
{
	linear[0] = BLI_color_from_srgb_table[srgb[0]];
	linear[1] = BLI_color_from_srgb_table[srgb[1]];
	linear[2] = BLI_color_from_srgb_table[srgb[2]];
	linear[3] = srgb[3] * (1.0f/255.0f);
}

MINLINE void srgb_to_linearrgb_uchar4_predivide(float linear[4], const unsigned char srgb[4])
{
	float alpha, inv_alpha;
	int i;

	if(srgb[3] == 255 || srgb[3] == 0) {
		srgb_to_linearrgb_uchar4(linear, srgb);
		return;
	}

	alpha = srgb[3] * (1.0f/255.0f);
	inv_alpha = 1.0f/alpha;

	for(i=0; i<3; ++i)
		linear[i] = linearrgb_to_srgb(srgb[i] * inv_alpha) * alpha;

	linear[3] = alpha;
}

#endif /* BLI_MATH_COLOR_INLINE_H */

