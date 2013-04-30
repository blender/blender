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

#ifndef __BLI_MATH_COLOR_BLEND_H__
#define __BLI_MATH_COLOR_BLEND_H__

/** \file BLI_math_color_blend.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_math_inline.h"

/******************** Blending Modes **********************
 * - byte function assume straight alpha
 * - float functions assume premultiplied alpha
 */

MINLINE void blend_color_mix_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4]);
MINLINE void blend_color_add_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4]);
MINLINE void blend_color_sub_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4]);
MINLINE void blend_color_mul_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4]);
MINLINE void blend_color_lighten_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4]);
MINLINE void blend_color_darken_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4]);
MINLINE void blend_color_erase_alpha_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4]);
MINLINE void blend_color_add_alpha_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4]);
MINLINE void blend_color_interpolate_byte(unsigned char dst[4], const unsigned char src1[4], const unsigned char src2[4], float t);

MINLINE void blend_color_mix_float(float dst[4], const float src1[4], const float src2[4]);
MINLINE void blend_color_add_float(float dst[4], const float src1[4], const float src2[4]);
MINLINE void blend_color_sub_float(float dst[4], const float src1[4], const float src2[4]);
MINLINE void blend_color_mul_float(float dst[4], const float src1[4], const float src2[4]);
MINLINE void blend_color_lighten_float(float dst[4], const float src1[4], const float src2[4]);
MINLINE void blend_color_darken_float(float dst[4], const float src1[4], const float src2[4]);
MINLINE void blend_color_erase_alpha_float(float dst[4], const float src1[4], const float src2[4]);
MINLINE void blend_color_add_alpha_float(float dst[4], const float src1[4], const float src2[4]);
MINLINE void blend_color_interpolate_float(float dst[4], const float src1[4], const float src2[4], float t);

#if BLI_MATH_DO_INLINE
#include "intern/math_color_blend_inline.c"
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MATH_COLOR_BLEND_H__ */

