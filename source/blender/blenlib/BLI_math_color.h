/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

#ifndef BLI_MATH_COLOR
#define BLI_MATH_COLOR

#ifdef __cplusplus
extern "C" {
#endif

/* primaries */
#define BLI_XYZ_SMPTE	0
#define BLI_XYZ_REC709_SRGB	1
#define BLI_XYZ_CIE		2

/* built-in profiles */
#define BLI_PR_NONE		0
#define BLI_PR_SRGB		1
#define BLI_PR_REC709	2
	
/******************* Conversion to RGB ********************/

void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b);
void hex_to_rgb(char *hexcol, float *r, float *g, float *b);
void yuv_to_rgb(float y, float u, float v, float *lr, float *lg, float *lb);
void ycc_to_rgb(float y, float cb, float cr, float *lr, float *lg, float *lb);
void xyz_to_rgb(float x, float y, float z, float *r, float *g, float *b, int colorspace);
void cpack_to_rgb(unsigned int col, float *r, float *g, float *b);

/***************** Conversion from RGB ********************/

void rgb_to_yuv(float r, float g, float b, float *ly, float *lu, float *lv);
void rgb_to_ycc(float r, float g, float b, float *ly, float *lcb, float *lcr);
void rgb_to_hsv(float r, float g, float b, float *lh, float *ls, float *lv);
unsigned int rgb_to_cpack(float r, float g, float b);
unsigned int hsv_to_cpack(float h, float s, float v);

/***************** Profile Transformations ********************/

void gamma_correct(float *c, float gamma);
float rec709_to_linearrgb(float c);
float linearrgb_to_rec709(float c);
float srgb_to_linearrgb(float c);
float linearrgb_to_srgb(float c);
void srgb_to_linearrgb_v3_v3(float *col_to, float *col_from);
void linearrgb_to_srgb_v3_v3(float *col_to, float *col_from);
	
/************************** Other *************************/

int constrain_rgb(float *r, float *g, float *b);
void minmax_rgb(short c[3]);

#ifdef __cplusplus
}
#endif

#endif /* BLI_MATH_COLOR */

