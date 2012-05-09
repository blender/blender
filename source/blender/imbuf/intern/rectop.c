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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * allocimbuf.c
 *
 */

/** \file blender/imbuf/intern/rectop.c
 *  \ingroup imbuf
 */


#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"


/* blend modes */

static void blend_color_mix(char *cp, char *cp1, char *cp2, int fac)
{
	/* this and other blending modes previously used >>8 instead of /255. both
	 * are not equivalent (>>8 is /256), and the former results in rounding
	 * errors that can turn colors black fast after repeated blending */
	int mfac = 255 - fac;

	cp[0] = (mfac * cp1[0] + fac * cp2[0]) / 255;
	cp[1] = (mfac * cp1[1] + fac * cp2[1]) / 255;
	cp[2] = (mfac * cp1[2] + fac * cp2[2]) / 255;
}

static void blend_color_add(char *cp, char *cp1, char *cp2, int fac)
{
	int temp;

	temp = cp1[0] + ((fac * cp2[0]) / 255);
	if (temp > 254) cp[0] = 255; else cp[0] = temp;
	temp = cp1[1] + ((fac * cp2[1]) / 255);
	if (temp > 254) cp[1] = 255; else cp[1] = temp;
	temp = cp1[2] + ((fac * cp2[2]) / 255);
	if (temp > 254) cp[2] = 255; else cp[2] = temp;
}

static void blend_color_sub(char *cp, char *cp1, char *cp2, int fac)
{
	int temp;

	temp = cp1[0] - ((fac * cp2[0]) / 255);
	if (temp < 0) cp[0] = 0; else cp[0] = temp;
	temp = cp1[1] - ((fac * cp2[1]) / 255);
	if (temp < 0) cp[1] = 0; else cp[1] = temp;
	temp = cp1[2] - ((fac * cp2[2]) / 255);
	if (temp < 0) cp[2] = 0; else cp[2] = temp;
}

static void blend_color_mul(char *cp, char *cp1, char *cp2, int fac)
{
	int mfac = 255 - fac;
	
	/* first mul, then blend the fac */
	cp[0] = (mfac * cp1[0] + fac * ((cp1[0] * cp2[0]) / 255)) / 255;
	cp[1] = (mfac * cp1[1] + fac * ((cp1[1] * cp2[1]) / 255)) / 255;
	cp[2] = (mfac * cp1[2] + fac * ((cp1[2] * cp2[2]) / 255)) / 255;
}

static void blend_color_lighten(char *cp, char *cp1, char *cp2, int fac)
{
	/* See if are lighter, if so mix, else don't do anything.
	 * if the paint col is darker then the original, then ignore */
	if (cp1[0] + cp1[1] + cp1[2] > cp2[0] + cp2[1] + cp2[2]) {
		cp[0] = cp1[0];
		cp[1] = cp1[1];
		cp[2] = cp1[2];
	}
	else
		blend_color_mix(cp, cp1, cp2, fac);
}

static void blend_color_darken(char *cp, char *cp1, char *cp2, int fac)
{
	/* See if were darker, if so mix, else don't do anything.
	 * if the paint col is brighter then the original, then ignore */
	if (cp1[0] + cp1[1] + cp1[2] < cp2[0] + cp2[1] + cp2[2]) {
		cp[0] = cp1[0];
		cp[1] = cp1[1];
		cp[2] = cp1[2];
	}
	else
		blend_color_mix(cp, cp1, cp2, fac);
}

unsigned int IMB_blend_color(unsigned int src1, unsigned int src2, int fac, IMB_BlendMode mode)
{
	unsigned int dst;
	int temp;
	char *cp, *cp1, *cp2;

	if (fac == 0)
		return src1;

	cp = (char *)&dst;
	cp1 = (char *)&src1;
	cp2 = (char *)&src2;

	switch (mode) {
		case IMB_BLEND_MIX:
			blend_color_mix(cp, cp1, cp2, fac); break;
		case IMB_BLEND_ADD:
			blend_color_add(cp, cp1, cp2, fac); break;
		case IMB_BLEND_SUB:
			blend_color_sub(cp, cp1, cp2, fac); break;
		case IMB_BLEND_MUL:
			blend_color_mul(cp, cp1, cp2, fac); break;
		case IMB_BLEND_LIGHTEN:
			blend_color_lighten(cp, cp1, cp2, fac); break;
		case IMB_BLEND_DARKEN:
			blend_color_darken(cp, cp1, cp2, fac); break;
		default:
			cp[0] = cp1[0];
			cp[1] = cp1[1];
			cp[2] = cp1[2];
	}

	if (mode == IMB_BLEND_ERASE_ALPHA) {
		temp = (cp1[3] - fac * cp2[3] / 255);
		cp[3] = (temp < 0) ? 0 : temp;
	}
	else { /* this does ADD_ALPHA also */
		temp = (cp1[3] + fac * cp2[3] / 255);
		cp[3] = (temp > 255) ? 255 : temp;
	}

	return dst;
}

static void blend_color_mix_float(float *cp, float *cp1, float *cp2, float fac)
{
	float mfac = 1.0f - fac;
	cp[0] = mfac * cp1[0] + fac * cp2[0];
	cp[1] = mfac * cp1[1] + fac * cp2[1];
	cp[2] = mfac * cp1[2] + fac * cp2[2];
}

static void blend_color_add_float(float *cp, float *cp1, float *cp2, float fac)
{
	cp[0] = cp1[0] + fac * cp2[0];
	cp[1] = cp1[1] + fac * cp2[1];
	cp[2] = cp1[2] + fac * cp2[2];

	if (cp[0] > 1.0f) cp[0] = 1.0f;
	if (cp[1] > 1.0f) cp[1] = 1.0f;
	if (cp[2] > 1.0f) cp[2] = 1.0f;
}

static void blend_color_sub_float(float *cp, float *cp1, float *cp2, float fac)
{
	cp[0] = cp1[0] - fac * cp2[0];
	cp[1] = cp1[1] - fac * cp2[1];
	cp[2] = cp1[2] - fac * cp2[2];

	if (cp[0] < 0.0f) cp[0] = 0.0f;
	if (cp[1] < 0.0f) cp[1] = 0.0f;
	if (cp[2] < 0.0f) cp[2] = 0.0f;
}

static void blend_color_mul_float(float *cp, float *cp1, float *cp2, float fac)
{
	float mfac = 1.0f - fac;
	
	cp[0] = mfac * cp1[0] + fac * (cp1[0] * cp2[0]);
	cp[1] = mfac * cp1[1] + fac * (cp1[1] * cp2[1]);
	cp[2] = mfac * cp1[2] + fac * (cp1[2] * cp2[2]);
}

static void blend_color_lighten_float(float *cp, float *cp1, float *cp2, float fac)
{
	/* See if are lighter, if so mix, else don't do anything.
	 * if the pafloat col is darker then the original, then ignore */
	if (cp1[0] + cp1[1] + cp1[2] > cp2[0] + cp2[1] + cp2[2]) {
		cp[0] = cp1[0];
		cp[1] = cp1[1];
		cp[2] = cp1[2];
	}
	else
		blend_color_mix_float(cp, cp1, cp2, fac);
}

static void blend_color_darken_float(float *cp, float *cp1, float *cp2, float fac)
{
	/* See if were darker, if so mix, else don't do anything.
	 * if the pafloat col is brighter then the original, then ignore */
	if (cp1[0] + cp1[1] + cp1[2] < cp2[0] + cp2[1] + cp2[2]) {
		cp[0] = cp1[0];
		cp[1] = cp1[1];
		cp[2] = cp1[2];
	}
	else
		blend_color_mix_float(cp, cp1, cp2, fac);
}

void IMB_blend_color_float(float *dst, float *src1, float *src2, float fac, IMB_BlendMode mode)
{
	if (fac == 0) {
		dst[0] = src1[0];
		dst[1] = src1[1];
		dst[2] = src1[2];
		dst[3] = src1[3];
		return;
	}

	switch (mode) {
		case IMB_BLEND_MIX:
			blend_color_mix_float(dst, src1, src2, fac); break;
		case IMB_BLEND_ADD:
			blend_color_add_float(dst, src1, src2, fac); break;
		case IMB_BLEND_SUB:
			blend_color_sub_float(dst, src1, src2, fac); break;
		case IMB_BLEND_MUL:
			blend_color_mul_float(dst, src1, src2, fac); break;
		case IMB_BLEND_LIGHTEN:
			blend_color_lighten_float(dst, src1, src2, fac); break;
		case IMB_BLEND_DARKEN:
			blend_color_darken_float(dst, src1, src2, fac); break;
		default:
			dst[0] = src1[0];
			dst[1] = src1[1];
			dst[2] = src1[2];
	}

	if (mode == IMB_BLEND_ERASE_ALPHA) {
		dst[3] = (src1[3] - fac * src2[3]);
		if (dst[3] < 0.0f) dst[3] = 0.0f;
	}
	else { /* this does ADD_ALPHA also */
		dst[3] = (src1[3] + fac * src2[3]);
		if (dst[3] > 1.0f) dst[3] = 1.0f;
	}
}

/* clipping */

void IMB_rectclip(struct ImBuf *dbuf, struct ImBuf *sbuf, int *destx, 
                  int *desty, int *srcx, int *srcy, int *width, int *height)
{
	int tmp;

	if (dbuf == NULL) return;
	
	if (*destx < 0) {
		*srcx -= *destx;
		*width += *destx;
		*destx = 0;
	}
	if (*srcx < 0) {
		*destx -= *srcx;
		*width += *srcx;
		*srcx = 0;
	}
	if (*desty < 0) {
		*srcy -= *desty;
		*height += *desty;
		*desty = 0;
	}
	if (*srcy < 0) {
		*desty -= *srcy;
		*height += *srcy;
		*srcy = 0;
	}

	tmp = dbuf->x - *destx;
	if (*width > tmp) *width = tmp;
	tmp = dbuf->y - *desty;
	if (*height > tmp) *height = tmp;

	if (sbuf) {
		tmp = sbuf->x - *srcx;
		if (*width > tmp) *width = tmp;
		tmp = sbuf->y - *srcy;
		if (*height > tmp) *height = tmp;
	}

	if ((*height <= 0) || (*width <= 0)) {
		*width = 0;
		*height = 0;
	}
}

/* copy and blend */

void IMB_rectcpy(struct ImBuf *dbuf, struct ImBuf *sbuf, int destx, 
                 int desty, int srcx, int srcy, int width, int height)
{
	IMB_rectblend(dbuf, sbuf, destx, desty, srcx, srcy, width, height,
	              IMB_BLEND_COPY);
}

void IMB_rectblend(struct ImBuf *dbuf, struct ImBuf *sbuf, int destx, 
                   int desty, int srcx, int srcy, int width, int height, IMB_BlendMode mode)
{
	unsigned int *drect = NULL, *srect = NULL, *dr, *sr;
	float *drectf = NULL, *srectf = NULL, *drf, *srf;
	int do_float, do_char, srcskip, destskip, x;

	if (dbuf == NULL) return;

	IMB_rectclip(dbuf, sbuf, &destx, &desty, &srcx, &srcy, &width, &height);

	if (width == 0 || height == 0) return;
	if (sbuf && sbuf->channels != 4) return;
	if (dbuf->channels != 4) return;
	
	do_char = (sbuf && sbuf->rect && dbuf->rect);
	do_float = (sbuf && sbuf->rect_float && dbuf->rect_float);

	if (do_char) drect = dbuf->rect + desty * dbuf->x + destx;
	if (do_float) drectf = dbuf->rect_float + (desty * dbuf->x + destx) * 4;

	destskip = dbuf->x;

	if (sbuf) {
		if (do_char) srect = sbuf->rect + srcy * sbuf->x + srcx;
		if (do_float) srectf = sbuf->rect_float + (srcy * sbuf->x + srcx) * 4;
		srcskip = sbuf->x;
	}
	else {
		srect = drect;
		srectf = drectf;
		srcskip = destskip;
	}

	if (mode == IMB_BLEND_COPY) {
		/* copy */
		for (; height > 0; height--) {
			if (do_char) {
				memcpy(drect, srect, width * sizeof(int));
				drect += destskip;
				srect += srcskip;
			}

			if (do_float) {
				memcpy(drectf, srectf, width * sizeof(float) * 4);
				drectf += destskip * 4;
				srectf += srcskip * 4;
			}
		}
	}
	else if (mode == IMB_BLEND_COPY_RGB) {
		/* copy rgb only */
		for (; height > 0; height--) {
			if (do_char) {
				dr = drect;
				sr = srect;
				for (x = width; x > 0; x--, dr++, sr++) {
					((char *)dr)[0] = ((char *)sr)[0];
					((char *)dr)[1] = ((char *)sr)[1];
					((char *)dr)[2] = ((char *)sr)[2];
				}
				drect += destskip;
				srect += srcskip;
			}

			if (do_float) {
				drf = drectf;
				srf = srectf;
				for (x = width; x > 0; x--, drf += 4, srf += 4) {
					drf[0] = srf[0];
					drf[1] = srf[1];
					drf[2] = srf[2];
				}
				drectf += destskip * 4;
				srectf += srcskip * 4;
			}
		}
	}
	else if (mode == IMB_BLEND_COPY_ALPHA) {
		/* copy alpha only */
		for (; height > 0; height--) {
			if (do_char) {
				dr = drect;
				sr = srect;
				for (x = width; x > 0; x--, dr++, sr++)
					((char *)dr)[3] = ((char *)sr)[3];
				drect += destskip;
				srect += srcskip;
			}

			if (do_float) {
				drf = drectf;
				srf = srectf;
				for (x = width; x > 0; x--, drf += 4, srf += 4)
					drf[3] = srf[3];
				drectf += destskip * 4;
				srectf += srcskip * 4;
			}
		}
	}
	else {
		/* blend */
		for (; height > 0; height--) {
			if (do_char) {
				dr = drect;
				sr = srect;
				for (x = width; x > 0; x--, dr++, sr++)
					*dr = IMB_blend_color(*dr, *sr, ((char *)sr)[3], mode);

				drect += destskip;
				srect += srcskip;
			}

			if (do_float) {
				drf = drectf;
				srf = srectf;
				for (x = width; x > 0; x--, drf += 4, srf += 4)
					IMB_blend_color_float(drf, drf, srf, srf[3], mode);

				drectf += destskip * 4;
				srectf += srcskip * 4;
			}		
		}
	}
}

/* fill */

void IMB_rectfill(struct ImBuf *drect, const float col[4])
{
	int num;

	if (drect->rect) {
		unsigned int *rrect = drect->rect;
		char ccol[4];
		
		ccol[0] = (int)(col[0] * 255);
		ccol[1] = (int)(col[1] * 255);
		ccol[2] = (int)(col[2] * 255);
		ccol[3] = (int)(col[3] * 255);
		
		num = drect->x * drect->y;
		for (; num > 0; num--)
			*rrect++ = *((unsigned int *)ccol);
	}
	
	if (drect->rect_float) {
		float *rrectf = drect->rect_float;
		
		num = drect->x * drect->y;
		for (; num > 0; num--) {
			*rrectf++ = col[0];
			*rrectf++ = col[1];
			*rrectf++ = col[2];
			*rrectf++ = col[3];
		}
	}	
}


void buf_rectfill_area(unsigned char *rect, float *rectf, int width, int height, const float col[4], int x1, int y1, int x2, int y2)
{
	int i, j;
	float a; /* alpha */
	float ai; /* alpha inverted */
	float aich; /* alpha, inverted, ai/255.0 - Convert char to float at the same time */
	if ((!rect && !rectf) || (!col) || col[3] == 0.0f)
		return;
	
	/* sanity checks for coords */
	CLAMP(x1, 0, width);
	CLAMP(x2, 0, width);
	CLAMP(y1, 0, height);
	CLAMP(y2, 0, height);

	if (x1 > x2) SWAP(int, x1, x2);
	if (y1 > y2) SWAP(int, y1, y2);
	if (x1 == x2 || y1 == y2) return;
	
	a = col[3];
	ai = 1 - a;
	aich = ai / 255.0f;

	if (rect) {
		unsigned char *pixel; 
		unsigned char chr = 0, chg = 0, chb = 0;
		float fr = 0, fg = 0, fb = 0;

		const int alphaint = FTOCHAR(a);
		
		if (a == 1.0f) {
			chr = FTOCHAR(col[0]);
			chg = FTOCHAR(col[1]);
			chb = FTOCHAR(col[2]);
		}
		else {
			fr = col[0] * a;
			fg = col[1] * a;
			fb = col[2] * a;
		}
		for (j = 0; j < y2 - y1; j++) {
			for (i = 0; i < x2 - x1; i++) {
				pixel = rect + 4 * (((y1 + j) * width) + (x1 + i));
				if (pixel >= rect && pixel < rect + (4 * (width * height))) {
					if (a == 1.0f) {
						pixel[0] = chr;
						pixel[1] = chg;
						pixel[2] = chb;
						pixel[3] = 255;
					}
					else {
						int alphatest;
						pixel[0] = (char)((fr + ((float)pixel[0] * aich)) * 255.0f);
						pixel[1] = (char)((fg + ((float)pixel[1] * aich)) * 255.0f);
						pixel[2] = (char)((fb + ((float)pixel[2] * aich)) * 255.0f);
						pixel[3] = (char)((alphatest = ((int)pixel[3] + alphaint)) < 255 ? alphatest : 255);
					}
				}
			}
		}
	}
	
	if (rectf) {
		float *pixel;
		for (j = 0; j < y2 - y1; j++) {
			for (i = 0; i < x2 - x1; i++) {
				pixel = rectf + 4 * (((y1 + j) * width) + (x1 + i));
				if (a == 1.0f) {
					pixel[0] = col[0];
					pixel[1] = col[1];
					pixel[2] = col[2];
					pixel[3] = 1.0f;
				}
				else {
					float alphatest;
					pixel[0] = (col[0] * a) + (pixel[0] * ai);
					pixel[1] = (col[1] * a) + (pixel[1] * ai);
					pixel[2] = (col[2] * a) + (pixel[2] * ai);
					pixel[3] = (alphatest = (pixel[3] + a)) < 1.0f ? alphatest : 1.0f;
				}
			}
		}
	}
}

void IMB_rectfill_area(struct ImBuf *ibuf, float *col, int x1, int y1, int x2, int y2)
{
	if (!ibuf) return;
	buf_rectfill_area((unsigned char *) ibuf->rect, ibuf->rect_float, ibuf->x, ibuf->y, col, x1, y1, x2, y2);
}


void IMB_rectfill_alpha(ImBuf *ibuf, const float value)
{
	int i;
	if (ibuf->rect_float) {
		float *fbuf = ibuf->rect_float + 3;
		for (i = ibuf->x * ibuf->y; i > 0; i--, fbuf += 4) { *fbuf = value; }
	}
	else {
		const unsigned char cvalue = value * 255;
		unsigned char *cbuf = ((unsigned char *)ibuf->rect) + 3;
		for (i = ibuf->x * ibuf->y; i > 0; i--, cbuf += 4) { *cbuf = cvalue; }
	}
}
