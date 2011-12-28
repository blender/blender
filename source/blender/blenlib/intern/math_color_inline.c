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

#endif /* BLI_MATH_COLOR_INLINE_H */

