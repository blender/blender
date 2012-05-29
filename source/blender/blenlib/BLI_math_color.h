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

#ifndef __BLI_MATH_COLOR_H__
#define __BLI_MATH_COLOR_H__

/** \file BLI_math_color.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_math_inline.h"

/* primaries */
#define BLI_XYZ_SMPTE        0
#define BLI_XYZ_REC709_SRGB  1
#define BLI_XYZ_CIE          2

/* built-in profiles */
#define BLI_PR_NONE     0
#define BLI_PR_SRGB     1
#define BLI_PR_REC709   2

/* YCbCr */
#define BLI_YCC_ITU_BT601   0
#define BLI_YCC_ITU_BT709   1
#define BLI_YCC_JFIF_0_255  2

/******************* Conversion to RGB ********************/

void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b);
void hsv_to_rgb_v(const float hsv[3], float r_rgb[3]);
void hex_to_rgb(char *hexcol, float *r, float *g, float *b);
void yuv_to_rgb(float y, float u, float v, float *lr, float *lg, float *lb);
void ycc_to_rgb(float y, float cb, float cr, float *lr, float *lg, float *lb, int colorspace);
void xyz_to_rgb(float x, float y, float z, float *r, float *g, float *b, int colorspace);
void cpack_to_rgb(unsigned int col, float *r, float *g, float *b);

/***************** Conversion from RGB ********************/

void rgb_to_yuv(float r, float g, float b, float *ly, float *lu, float *lv);
void rgb_to_ycc(float r, float g, float b, float *ly, float *lcb, float *lcr, int colorspace);
void rgb_to_hsv(float r, float g, float b, float *lh, float *ls, float *lv);
void rgb_to_hsv_v(const float rgb[3], float r_hsv[3]);
void rgb_to_hsv_compat(float r, float g, float b, float *lh, float *ls, float *lv);
void rgb_to_hsv_compat_v(const float rgb[3], float r_hsv[3]);
void rgb_to_lab(float r, float g, float b, float *ll, float *la, float *lb);
void rgb_to_xyz(float r, float g, float b, float *x, float *y, float *z);
unsigned int rgb_to_cpack(float r, float g, float b);
unsigned int hsv_to_cpack(float h, float s, float v);

float rgb_to_grayscale(const float rgb[3]);
unsigned char rgb_to_grayscale_byte(const unsigned char rgb[3]);
float rgb_to_luma(const float rgb[3]);
unsigned char rgb_to_luma_byte(const unsigned char rgb[3]);

/**************** Profile Transformations *****************/

void gamma_correct(float *c, float gamma);
float rec709_to_linearrgb(float c);
float linearrgb_to_rec709(float c);
float srgb_to_linearrgb(float c);
float linearrgb_to_srgb(float c);

MINLINE void srgb_to_linearrgb_v3_v3(float linear[3], const float srgb[3]);
MINLINE void linearrgb_to_srgb_v3_v3(float srgb[3], const float linear[3]);

MINLINE void srgb_to_linearrgb_v4(float linear[4], const float srgb[4]);
MINLINE void linearrgb_to_srgb_v4(float srgb[4], const float linear[4]);

MINLINE void srgb_to_linearrgb_predivide_v4(float linear[4], const float srgb[4]);
MINLINE void linearrgb_to_srgb_predivide_v4(float srgb[4], const float linear[4]);

MINLINE void linearrgb_to_srgb_uchar3(unsigned char srgb[3], const float linear[3]);
MINLINE void linearrgb_to_srgb_uchar4(unsigned char srgb[4], const float linear[4]);

void BLI_init_srgb_conversion(void);

/************************** Other *************************/

int constrain_rgb(float *r, float *g, float *b);
void minmax_rgb(short c[3]);

void rgb_float_set_hue_float_offset(float *rgb, float hue_offset);
void rgb_byte_set_hue_float_offset(unsigned char *rgb, float hue_offset);

void rgb_uchar_to_float(float col_r[3], const unsigned char col_ub[3]);
void rgba_uchar_to_float(float col_r[4], const unsigned char col_ub[4]);
void rgb_float_to_uchar(unsigned char col_r[3], const float col_f[3]);
void rgba_float_to_uchar(unsigned char col_r[4], const float col_f[4]);

void xyz_to_lab(float x, float y, float z, float *l, float *a, float *b);

/***************** lift/gamma/gain / ASC-CDL conversion *****************/

void lift_gamma_gain_to_asc_cdl(float *lift, float *gamma, float *gain, float *offset, float *slope, float *power);

#ifdef __BLI_MATH_INLINE_H__
#include "intern/math_color_inline.c"
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MATH_COLOR_H__ */

