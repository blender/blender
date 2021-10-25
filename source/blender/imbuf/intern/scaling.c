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

/** \file blender/imbuf/intern/scaling.c
 *  \ingroup imbuf
 */


#include "BLI_utildefines.h"
#include "BLI_math_color.h"
#include "BLI_math_interp.h"
#include "MEM_guardedalloc.h"

#include "imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_filter.h"

#include "BLI_sys_types.h" // for intptr_t support

/************************************************************************/
/*								SCALING									*/
/************************************************************************/


static void imb_half_x_no_alloc(struct ImBuf *ibuf2, struct ImBuf *ibuf1)
{
	uchar *p1, *_p1, *dest;
	short a, r, g, b;
	int x, y;
	float af, rf, gf, bf, *p1f, *_p1f, *destf;
	bool do_rect, do_float;

	do_rect = (ibuf1->rect != NULL);
	do_float = (ibuf1->rect_float != NULL && ibuf2->rect_float != NULL);
	
	_p1 = (uchar *) ibuf1->rect;
	dest = (uchar *) ibuf2->rect;

	_p1f = ibuf1->rect_float;
	destf = ibuf2->rect_float;

	for (y = ibuf2->y; y > 0; y--) {
		p1 = _p1;
		p1f = _p1f;
		for (x = ibuf2->x; x > 0; x--) {
			if (do_rect) {
				a = *(p1++);
				b = *(p1++);
				g = *(p1++);
				r = *(p1++);
				a += *(p1++);
				b += *(p1++);
				g += *(p1++);
				r += *(p1++);
				*(dest++) = a >> 1;
				*(dest++) = b >> 1;
				*(dest++) = g >> 1;
				*(dest++) = r >> 1;
			}
			if (do_float) {
				af = *(p1f++);
				bf = *(p1f++);
				gf = *(p1f++);
				rf = *(p1f++);
				af += *(p1f++);
				bf += *(p1f++);
				gf += *(p1f++);
				rf += *(p1f++);
				*(destf++) = 0.5f * af;
				*(destf++) = 0.5f * bf;
				*(destf++) = 0.5f * gf;
				*(destf++) = 0.5f * rf;
			}
		}
		if (do_rect) _p1 += (ibuf1->x << 2);
		if (do_float) _p1f += (ibuf1->x << 2);
	}
}

struct ImBuf *IMB_half_x(struct ImBuf *ibuf1)
{
	struct ImBuf *ibuf2;

	if (ibuf1 == NULL) return (NULL);
	if (ibuf1->rect == NULL && ibuf1->rect_float == NULL) return (NULL);

	if (ibuf1->x <= 1) return(IMB_dupImBuf(ibuf1));
	
	ibuf2 = IMB_allocImBuf((ibuf1->x) / 2, ibuf1->y, ibuf1->planes, ibuf1->flags);
	if (ibuf2 == NULL) return (NULL);

	imb_half_x_no_alloc(ibuf2, ibuf1);
	
	return (ibuf2);
}

struct ImBuf *IMB_double_fast_x(struct ImBuf *ibuf1)
{
	struct ImBuf *ibuf2;
	int *p1, *dest, i, col, do_rect, do_float;
	float *p1f, *destf;

	if (ibuf1 == NULL) return (NULL);
	if (ibuf1->rect == NULL && ibuf1->rect_float == NULL) return (NULL);

	do_rect = (ibuf1->rect != NULL);
	do_float = (ibuf1->rect_float != NULL);
	
	ibuf2 = IMB_allocImBuf(2 * ibuf1->x, ibuf1->y, ibuf1->planes, ibuf1->flags);
	if (ibuf2 == NULL) return (NULL);

	p1 = (int *) ibuf1->rect;
	dest = (int *) ibuf2->rect;
	p1f = (float *)ibuf1->rect_float;
	destf = (float *)ibuf2->rect_float;

	for (i = ibuf1->y * ibuf1->x; i > 0; i--) {
		if (do_rect) {
			col = *p1++;
			*dest++ = col;
			*dest++ = col;
		}
		if (do_float) {
			destf[0] = destf[4] = p1f[0];
			destf[1] = destf[5] = p1f[1];
			destf[2] = destf[6] = p1f[2];
			destf[3] = destf[7] = p1f[3];
			destf += 8;
			p1f += 4;
		}
	}

	return (ibuf2);
}

struct ImBuf *IMB_double_x(struct ImBuf *ibuf1)
{
	struct ImBuf *ibuf2;

	if (ibuf1 == NULL) return (NULL);
	if (ibuf1->rect == NULL && ibuf1->rect_float == NULL) return (NULL);

	ibuf2 = IMB_double_fast_x(ibuf1);

	imb_filterx(ibuf2);
	return (ibuf2);
}


static void imb_half_y_no_alloc(struct ImBuf *ibuf2, struct ImBuf *ibuf1)
{
	uchar *p1, *p2, *_p1, *dest;
	short a, r, g, b;
	int x, y;
	int do_rect, do_float;
	float af, rf, gf, bf, *p1f, *p2f, *_p1f, *destf;

	p1 = p2 = NULL;
	p1f = p2f = NULL;

	do_rect = (ibuf1->rect != NULL);
	do_float = (ibuf1->rect_float != NULL && ibuf2->rect_float != NULL);

	_p1 = (uchar *) ibuf1->rect;
	dest = (uchar *) ibuf2->rect;
	_p1f = (float *) ibuf1->rect_float;
	destf = (float *) ibuf2->rect_float;

	for (y = ibuf2->y; y > 0; y--) {
		if (do_rect) {
			p1 = _p1;
			p2 = _p1 + (ibuf1->x << 2);
		}
		if (do_float) {
			p1f = _p1f;
			p2f = _p1f + (ibuf1->x << 2);
		}
		for (x = ibuf2->x; x > 0; x--) {
			if (do_rect) {
				a = *(p1++);
				b = *(p1++);
				g = *(p1++);
				r = *(p1++);
				a += *(p2++);
				b += *(p2++);
				g += *(p2++);
				r += *(p2++);
				*(dest++) = a >> 1;
				*(dest++) = b >> 1;
				*(dest++) = g >> 1;
				*(dest++) = r >> 1;
			}
			if (do_float) {
				af = *(p1f++);
				bf = *(p1f++);
				gf = *(p1f++);
				rf = *(p1f++);
				af += *(p2f++);
				bf += *(p2f++);
				gf += *(p2f++);
				rf += *(p2f++);
				*(destf++) = 0.5f * af;
				*(destf++) = 0.5f * bf;
				*(destf++) = 0.5f * gf;
				*(destf++) = 0.5f * rf;
			}
		}
		if (do_rect) _p1 += (ibuf1->x << 3);
		if (do_float) _p1f += (ibuf1->x << 3);
	}
}


struct ImBuf *IMB_half_y(struct ImBuf *ibuf1)
{
	struct ImBuf *ibuf2;

	if (ibuf1 == NULL) return (NULL);
	if (ibuf1->rect == NULL && ibuf1->rect_float == NULL) return (NULL);
	
	if (ibuf1->y <= 1) return(IMB_dupImBuf(ibuf1));

	ibuf2 = IMB_allocImBuf(ibuf1->x, (ibuf1->y) / 2, ibuf1->planes, ibuf1->flags);
	if (ibuf2 == NULL) return (NULL);

	imb_half_y_no_alloc(ibuf2, ibuf1);
	
	return (ibuf2);
}


struct ImBuf *IMB_double_fast_y(struct ImBuf *ibuf1)
{
	struct ImBuf *ibuf2;
	int *p1, *dest1, *dest2;
	float *p1f, *dest1f, *dest2f;
	int x, y;
	int do_rect, do_float;

	if (ibuf1 == NULL) return (NULL);
	if (ibuf1->rect == NULL && ibuf1->rect_float == NULL) return (NULL);

	do_rect = (ibuf1->rect != NULL);
	do_float = (ibuf1->rect_float != NULL);

	ibuf2 = IMB_allocImBuf(ibuf1->x, 2 * ibuf1->y, ibuf1->planes, ibuf1->flags);
	if (ibuf2 == NULL) return (NULL);

	p1 = (int *) ibuf1->rect;
	dest1 = (int *) ibuf2->rect;
	p1f = (float *) ibuf1->rect_float;
	dest1f = (float *) ibuf2->rect_float;

	for (y = ibuf1->y; y > 0; y--) {
		if (do_rect) {
			dest2 = dest1 + ibuf2->x;
			for (x = ibuf2->x; x > 0; x--) *dest1++ = *dest2++ = *p1++;
			dest1 = dest2;
		}
		if (do_float) {
			dest2f = dest1f + (4 * ibuf2->x);
			for (x = ibuf2->x * 4; x > 0; x--) *dest1f++ = *dest2f++ = *p1f++;
			dest1f = dest2f;
		}
	}

	return (ibuf2);
}

struct ImBuf *IMB_double_y(struct ImBuf *ibuf1)
{
	struct ImBuf *ibuf2;

	if (ibuf1 == NULL) return (NULL);
	if (ibuf1->rect == NULL) return (NULL);

	ibuf2 = IMB_double_fast_y(ibuf1);
	
	IMB_filtery(ibuf2);
	return (ibuf2);
}

/* pretty much specific functions which converts uchar <-> ushort but assumes
 * ushort range of 255*255 which is more convenient here
 */
MINLINE void straight_uchar_to_premul_ushort(unsigned short result[4], const unsigned char color[4])
{
	unsigned short alpha = color[3];

	result[0] = color[0] * alpha;
	result[1] = color[1] * alpha;
	result[2] = color[2] * alpha;
	result[3] = alpha * 256;
}

MINLINE void premul_ushort_to_straight_uchar(unsigned char *result, const unsigned short color[4])
{
	if (color[3] <= 255) {
		result[0] = USHORTTOUCHAR(color[0]);
		result[1] = USHORTTOUCHAR(color[1]);
		result[2] = USHORTTOUCHAR(color[2]);
		result[3] = USHORTTOUCHAR(color[3]);
	}
	else {
		unsigned short alpha = color[3] / 256;

		result[0] = USHORTTOUCHAR(color[0] / alpha * 256);
		result[1] = USHORTTOUCHAR(color[1] / alpha * 256);
		result[2] = USHORTTOUCHAR(color[2] / alpha * 256);
		result[3] = USHORTTOUCHAR(color[3]);
	}
}

/* result in ibuf2, scaling should be done correctly */
void imb_onehalf_no_alloc(struct ImBuf *ibuf2, struct ImBuf *ibuf1)
{
	int x, y;
	const short do_rect = (ibuf1->rect != NULL);
	const short do_float = (ibuf1->rect_float != NULL) && (ibuf2->rect_float != NULL);

	if (do_rect && (ibuf2->rect == NULL)) {
		imb_addrectImBuf(ibuf2);
	}

	if (ibuf1->x <= 1) {
		imb_half_y_no_alloc(ibuf2, ibuf1);
		return;
	}
	if (ibuf1->y <= 1) {
		imb_half_x_no_alloc(ibuf2, ibuf1);
		return;
	}
	
	if (do_rect) {
		unsigned char *cp1, *cp2, *dest;
		
		cp1 = (unsigned char *) ibuf1->rect;
		dest = (unsigned char *) ibuf2->rect;
		
		for (y = ibuf2->y; y > 0; y--) {
			cp2 = cp1 + (ibuf1->x << 2);
			for (x = ibuf2->x; x > 0; x--) {
				unsigned short p1i[8], p2i[8], desti[4];
				
				straight_uchar_to_premul_ushort(p1i, cp1);
				straight_uchar_to_premul_ushort(p2i, cp2);
				straight_uchar_to_premul_ushort(p1i + 4, cp1 + 4);
				straight_uchar_to_premul_ushort(p2i + 4, cp2 + 4);
				
				desti[0] = ((unsigned int) p1i[0] + p2i[0] + p1i[4] + p2i[4]) >> 2;
				desti[1] = ((unsigned int) p1i[1] + p2i[1] + p1i[5] + p2i[5]) >> 2;
				desti[2] = ((unsigned int) p1i[2] + p2i[2] + p1i[6] + p2i[6]) >> 2;
				desti[3] = ((unsigned int) p1i[3] + p2i[3] + p1i[7] + p2i[7]) >> 2;
				
				premul_ushort_to_straight_uchar(dest, desti);
				
				cp1 += 8;
				cp2 += 8;
				dest += 4;
			}
			cp1 = cp2;
			if (ibuf1->x & 1) cp1 += 4;
		}
	}
	
	if (do_float) {
		float *p1f, *p2f, *destf;
		
		p1f = ibuf1->rect_float;
		destf = ibuf2->rect_float;
		for (y = ibuf2->y; y > 0; y--) {
			p2f = p1f + (ibuf1->x << 2);
			for (x = ibuf2->x; x > 0; x--) {
				destf[0] = 0.25f * (p1f[0] + p2f[0] + p1f[4] + p2f[4]);
				destf[1] = 0.25f * (p1f[1] + p2f[1] + p1f[5] + p2f[5]);
				destf[2] = 0.25f * (p1f[2] + p2f[2] + p1f[6] + p2f[6]);
				destf[3] = 0.25f * (p1f[3] + p2f[3] + p1f[7] + p2f[7]);
				p1f += 8;
				p2f += 8;
				destf += 4;
			}
			p1f = p2f;
			if (ibuf1->x & 1) p1f += 4;
		}
	}
}

ImBuf *IMB_onehalf(struct ImBuf *ibuf1)
{
	struct ImBuf *ibuf2;

	if (ibuf1 == NULL) return (NULL);
	if (ibuf1->rect == NULL && ibuf1->rect_float == NULL) return (NULL);
	
	if (ibuf1->x <= 1) return(IMB_half_y(ibuf1));
	if (ibuf1->y <= 1) return(IMB_half_x(ibuf1));
	
	ibuf2 = IMB_allocImBuf((ibuf1->x) / 2, (ibuf1->y) / 2, ibuf1->planes, ibuf1->flags);
	if (ibuf2 == NULL) return (NULL);
	
	imb_onehalf_no_alloc(ibuf2, ibuf1);
	
	return (ibuf2);
}

/* q_scale_linear_interpolation helper functions */

static void enlarge_picture_byte(
        unsigned char *src, unsigned char *dst, int src_width,
        int src_height, int dst_width, int dst_height)
{
	double ratiox = (double) (dst_width - 1.0)  / (double) (src_width - 1.001);
	double ratioy = (double) (dst_height - 1.0) / (double) (src_height - 1.001);
	uintptr_t x_src, dx_src, x_dst;
	uintptr_t y_src, dy_src, y_dst;

	dx_src = 65536.0 / ratiox;
	dy_src = 65536.0 / ratioy;

	y_src = 0;
	for (y_dst = 0; y_dst < dst_height; y_dst++) {
		unsigned char *line1 = src + (y_src >> 16) * 4 * src_width;
		unsigned char *line2 = line1 + 4 * src_width;
		uintptr_t weight1y = 65536 - (y_src & 0xffff);
		uintptr_t weight2y = 65536 - weight1y;

		if ((y_src >> 16) == src_height - 1) {
			line2 = line1;
		}

		x_src = 0;
		for (x_dst = 0; x_dst < dst_width; x_dst++) {
			uintptr_t weight1x = 65536 - (x_src & 0xffff);
			uintptr_t weight2x = 65536 - weight1x;

			unsigned long x = (x_src >> 16) * 4;

			*dst++ = ((((line1[x]     * weight1y) >> 16) * weight1x) >> 16) +
			         ((((line2[x]     * weight2y) >> 16) * weight1x) >> 16) +
			         ((((line1[4 + x] * weight1y) >> 16) * weight2x) >> 16) +
			         ((((line2[4 + x] * weight2y) >> 16) * weight2x) >> 16);

			*dst++ = ((((line1[x + 1]     * weight1y) >> 16) * weight1x) >> 16) +
			         ((((line2[x + 1]     * weight2y) >> 16) * weight1x) >> 16) +
			         ((((line1[4 + x + 1] * weight1y) >> 16) * weight2x) >> 16) +
			         ((((line2[4 + x + 1] * weight2y) >> 16) * weight2x) >> 16);

			*dst++ = ((((line1[x + 2]     * weight1y) >> 16) * weight1x) >> 16) +
			         ((((line2[x + 2]     * weight2y) >> 16) * weight1x) >> 16) +
			         ((((line1[4 + x + 2] * weight1y) >> 16) * weight2x) >> 16) +
			         ((((line2[4 + x + 2] * weight2y) >> 16) * weight2x) >> 16);

			*dst++ = ((((line1[x + 3]     * weight1y) >> 16) * weight1x) >> 16) +
			         ((((line2[x + 3]     * weight2y) >> 16) * weight1x) >> 16) +
			         ((((line1[4 + x + 3] * weight1y) >> 16) * weight2x) >> 16) +
			         ((((line2[4 + x + 3] * weight2y) >> 16) * weight2x) >> 16);

			x_src += dx_src;
		}
		y_src += dy_src;
	}
}

struct scale_outpix_byte {
	uintptr_t r;
	uintptr_t g;
	uintptr_t b;
	uintptr_t a;

	uintptr_t weight;
};

static void shrink_picture_byte(
        unsigned char *src, unsigned char *dst, int src_width,
        int src_height, int dst_width, int dst_height)
{
	double ratiox = (double) (dst_width) / (double) (src_width);
	double ratioy = (double) (dst_height) / (double) (src_height);
	uintptr_t x_src, dx_dst, x_dst;
	uintptr_t y_src, dy_dst, y_dst;
	intptr_t y_counter;
	unsigned char *dst_begin = dst;

	struct scale_outpix_byte *dst_line1 = NULL;
	struct scale_outpix_byte *dst_line2 = NULL;

	dst_line1 = (struct scale_outpix_byte *) MEM_callocN(
	    (dst_width + 1) * sizeof(struct scale_outpix_byte),
	    "shrink_picture_byte 1");
	dst_line2 = (struct scale_outpix_byte *) MEM_callocN(
	    (dst_width + 1) * sizeof(struct scale_outpix_byte),
	    "shrink_picture_byte 2");

	dx_dst = 65536.0 * ratiox;
	dy_dst = 65536.0 * ratioy;

	y_dst = 0;
	y_counter = 65536;
	for (y_src = 0; y_src < src_height; y_src++) {
		unsigned char *line = src + y_src * 4 * src_width;
		uintptr_t weight1y = 65535 - (y_dst & 0xffff);
		uintptr_t weight2y = 65535 - weight1y;
		x_dst = 0;
		for (x_src = 0; x_src < src_width; x_src++) {
			uintptr_t weight1x = 65535 - (x_dst & 0xffff);
			uintptr_t weight2x = 65535 - weight1x;

			uintptr_t x = x_dst >> 16;

			uintptr_t w;

			w = (weight1y * weight1x) >> 16;

			/* ensure correct rounding, without this you get ugly banding, or too low color values (ton) */
			dst_line1[x].r += (line[0] * w + 32767) >> 16;
			dst_line1[x].g += (line[1] * w + 32767) >> 16;
			dst_line1[x].b += (line[2] * w + 32767) >> 16;
			dst_line1[x].a += (line[3] * w + 32767) >> 16;
			dst_line1[x].weight += w;

			w = (weight2y * weight1x) >> 16;

			dst_line2[x].r += (line[0] * w + 32767) >> 16;
			dst_line2[x].g += (line[1] * w + 32767) >> 16;
			dst_line2[x].b += (line[2] * w + 32767) >> 16;
			dst_line2[x].a += (line[3] * w + 32767) >> 16;
			dst_line2[x].weight += w;

			w = (weight1y * weight2x) >> 16;

			dst_line1[x + 1].r += (line[0] * w + 32767) >> 16;
			dst_line1[x + 1].g += (line[1] * w + 32767) >> 16;
			dst_line1[x + 1].b += (line[2] * w + 32767) >> 16;
			dst_line1[x + 1].a += (line[3] * w + 32767) >> 16;
			dst_line1[x + 1].weight += w;

			w = (weight2y * weight2x) >> 16;

			dst_line2[x + 1].r += (line[0] * w + 32767) >> 16;
			dst_line2[x + 1].g += (line[1] * w + 32767) >> 16;
			dst_line2[x + 1].b += (line[2] * w + 32767) >> 16;
			dst_line2[x + 1].a += (line[3] * w + 32767) >> 16;
			dst_line2[x + 1].weight += w;

			x_dst += dx_dst;
			line += 4;
		}

		y_dst += dy_dst;
		y_counter -= dy_dst;
		if (y_counter < 0) {
			int val;
			uintptr_t x;
			struct scale_outpix_byte *temp;

			y_counter += 65536;
			
			for (x = 0; x < dst_width; x++) {
				uintptr_t f =  0x80000000UL / dst_line1[x].weight;
				*dst++ = (val = (dst_line1[x].r * f) >> 15) > 255 ? 255 : val;
				*dst++ = (val = (dst_line1[x].g * f) >> 15) > 255 ? 255 : val;
				*dst++ = (val = (dst_line1[x].b * f) >> 15) > 255 ? 255 : val;
				*dst++ = (val = (dst_line1[x].a * f) >> 15) > 255 ? 255 : val;
			}
			memset(dst_line1, 0, dst_width *
			       sizeof(struct scale_outpix_byte));
			temp = dst_line1;
			dst_line1 = dst_line2;
			dst_line2 = temp;
		}
	}
	if (dst - dst_begin < dst_width * dst_height * 4) {
		int val;
		uintptr_t x;
		for (x = 0; x < dst_width; x++) {
			uintptr_t f = 0x80000000UL / dst_line1[x].weight;
			*dst++ = (val = (dst_line1[x].r * f) >> 15) > 255 ? 255 : val;
			*dst++ = (val = (dst_line1[x].g * f) >> 15) > 255 ? 255 : val;
			*dst++ = (val = (dst_line1[x].b * f) >> 15) > 255 ? 255 : val;
			*dst++ = (val = (dst_line1[x].a * f) >> 15) > 255 ? 255 : val;
		}
	}
	MEM_freeN(dst_line1);
	MEM_freeN(dst_line2);
}


static void q_scale_byte(unsigned char *in, unsigned char *out, int in_width,
                         int in_height, int dst_width, int dst_height)
{
	if (dst_width > in_width && dst_height > in_height) {
		enlarge_picture_byte(in, out, in_width, in_height,
		                     dst_width, dst_height);
	}
	else if (dst_width < in_width && dst_height < in_height) {
		shrink_picture_byte(in, out, in_width, in_height,
		                    dst_width, dst_height);
	}
}

static void enlarge_picture_float(
        float *src, float *dst, int src_width,
        int src_height, int dst_width, int dst_height)
{
	double ratiox = (double) (dst_width - 1.0) / (double) (src_width - 1.001);
	double ratioy = (double) (dst_height - 1.0) / (double) (src_height - 1.001);
	uintptr_t x_dst;
	uintptr_t y_dst;
	double x_src, dx_src;
	double y_src, dy_src;

	dx_src = 1.0 / ratiox;
	dy_src = 1.0 / ratioy;

	y_src = 0;
	for (y_dst = 0; y_dst < dst_height; y_dst++) {
		float *line1 = src + ((int) y_src) * 4 * src_width;
		const float *line2 = line1 + 4 * src_width;
		const float weight1y = (float)(1.0 - (y_src - (int) y_src));
		const float weight2y = 1.0f - weight1y;

		if ((int) y_src == src_height - 1) {
			line2 = line1;
		}

		x_src = 0;
		for (x_dst = 0; x_dst < dst_width; x_dst++) {
			const float weight1x = (float)(1.0 - (x_src - (int) x_src));
			const float weight2x = (float)(1.0f - weight1x);

			const float w11 = weight1y * weight1x;
			const float w21 = weight2y * weight1x;
			const float w12 = weight1y * weight2x;
			const float w22 = weight2y * weight2x;

			uintptr_t x = ((int) x_src) * 4;

			*dst++ = line1[x]     * w11 +
			         line2[x]     * w21 +
			         line1[4 + x] * w12 +
			         line2[4 + x] * w22;

			*dst++ = line1[x + 1]     * w11 +
			         line2[x + 1]     * w21 +
			         line1[4 + x + 1] * w12 +
			         line2[4 + x + 1] * w22;

			*dst++ = line1[x + 2]     * w11 +
			         line2[x + 2]     * w21 +
			         line1[4 + x + 2] * w12 +
			         line2[4 + x + 2] * w22;

			*dst++ = line1[x + 3]     * w11 +
			         line2[x + 3]     * w21 +
			         line1[4 + x + 3] * w12 +
			         line2[4 + x + 3] * w22;

			x_src += dx_src;
		}
		y_src += dy_src;
	}
}

struct scale_outpix_float {
	float r;
	float g;
	float b;
	float a;

	float weight;
};

static void shrink_picture_float(
        const float *src, float *dst, int src_width,
        int src_height, int dst_width, int dst_height)
{
	double ratiox = (double) (dst_width) / (double) (src_width);
	double ratioy = (double) (dst_height) / (double) (src_height);
	uintptr_t x_src;
	uintptr_t y_src;
	float dx_dst, x_dst;
	float dy_dst, y_dst;
	float y_counter;
	const float *dst_begin = dst;

	struct scale_outpix_float *dst_line1;
	struct scale_outpix_float *dst_line2;

	dst_line1 = (struct scale_outpix_float *) MEM_callocN(
	        (dst_width + 1) * sizeof(struct scale_outpix_float),
	        "shrink_picture_float 1");
	dst_line2 = (struct scale_outpix_float *) MEM_callocN(
	        (dst_width + 1) * sizeof(struct scale_outpix_float),
	        "shrink_picture_float 2");

	dx_dst = ratiox;
	dy_dst = ratioy;

	y_dst = 0;
	y_counter = 1.0;
	for (y_src = 0; y_src < src_height; y_src++) {
		const float *line = src + y_src * 4 * src_width;
		uintptr_t weight1y = 1.0f - (y_dst - (int) y_dst);
		uintptr_t weight2y = 1.0f - weight1y;
		x_dst = 0;
		for (x_src = 0; x_src < src_width; x_src++) {
			uintptr_t weight1x = 1.0f - (x_dst - (int) x_dst);
			uintptr_t weight2x = 1.0f - weight1x;

			uintptr_t x = (int) x_dst;

			float w;

			w = weight1y * weight1x;

			dst_line1[x].r += line[0] * w;
			dst_line1[x].g += line[1] * w;
			dst_line1[x].b += line[2] * w;
			dst_line1[x].a += line[3] * w;
			dst_line1[x].weight += w;

			w = weight2y * weight1x;

			dst_line2[x].r += line[0] * w;
			dst_line2[x].g += line[1] * w;
			dst_line2[x].b += line[2] * w;
			dst_line2[x].a += line[3] * w;
			dst_line2[x].weight += w;

			w = weight1y * weight2x;

			dst_line1[x + 1].r += line[0] * w;
			dst_line1[x + 1].g += line[1] * w;
			dst_line1[x + 1].b += line[2] * w;
			dst_line1[x + 1].a += line[3] * w;
			dst_line1[x + 1].weight += w;

			w = weight2y * weight2x;

			dst_line2[x + 1].r += line[0] * w;
			dst_line2[x + 1].g += line[1] * w;
			dst_line2[x + 1].b += line[2] * w;
			dst_line2[x + 1].a += line[3] * w;
			dst_line2[x + 1].weight += w;

			x_dst += dx_dst;
			line += 4;
		}

		y_dst += dy_dst;
		y_counter -= dy_dst;
		if (y_counter < 0) {
			uintptr_t x;
			struct scale_outpix_float *temp;

			y_counter += 1.0f;
			
			for (x = 0; x < dst_width; x++) {
				float f = 1.0f / dst_line1[x].weight;
				*dst++ = dst_line1[x].r * f;
				*dst++ = dst_line1[x].g * f;
				*dst++ = dst_line1[x].b * f;
				*dst++ = dst_line1[x].a * f;
			}
			memset(dst_line1, 0, dst_width *
			       sizeof(struct scale_outpix_float));
			temp = dst_line1;
			dst_line1 = dst_line2;
			dst_line2 = temp;
		}
	}
	if (dst - dst_begin < dst_width * dst_height * 4) {
		uintptr_t x;
		for (x = 0; x < dst_width; x++) {
			float f = 1.0f / dst_line1[x].weight;
			*dst++ = dst_line1[x].r * f;
			*dst++ = dst_line1[x].g * f;
			*dst++ = dst_line1[x].b * f;
			*dst++ = dst_line1[x].a * f;
		}
	}
	MEM_freeN(dst_line1);
	MEM_freeN(dst_line2);
}


static void q_scale_float(float *in, float *out, int in_width,
                          int in_height, int dst_width, int dst_height)
{
	if (dst_width > in_width && dst_height > in_height) {
		enlarge_picture_float(in, out, in_width, in_height,
		                      dst_width, dst_height);
	}
	else if (dst_width < in_width && dst_height < in_height) {
		shrink_picture_float(in, out, in_width, in_height,
		                     dst_width, dst_height);
	}
}

/**
 * q_scale_linear_interpolation (derived from ppmqscale, http://libdv.sf.net)
 *
 * q stands for quick _and_ quality :)
 *
 * only handles common cases when we either
 *
 * scale  both, x and y or
 * shrink both, x and y
 *
 * but that is pretty fast:
 * - does only blit once instead of two passes like the old code
 *   (fewer cache misses)
 * - uses fixed point integer arithmetic for byte buffers
 * - doesn't branch in tight loops
 *
 * Should be comparable in speed to the ImBuf ..._fast functions at least
 * for byte-buffers.
 *
 * NOTE: disabled, due to unacceptable inaccuracy and quality loss, see bug #18609 (ton)
 */
static bool q_scale_linear_interpolation(
        struct ImBuf *ibuf, int newx, int newy)
{
	if ((newx >= ibuf->x && newy <= ibuf->y) ||
	    (newx <= ibuf->x && newy >= ibuf->y))
	{
		return false;
	}

	if (ibuf->rect) {
		unsigned char *newrect =
		    MEM_mallocN(newx * newy * sizeof(int), "q_scale rect");
		q_scale_byte((unsigned char *)ibuf->rect, newrect, ibuf->x, ibuf->y,
		             newx, newy);

		imb_freerectImBuf(ibuf);
		ibuf->mall |= IB_rect;
		ibuf->rect = (unsigned int *) newrect;
	}
	if (ibuf->rect_float) {
		float *newrect =
		    MEM_mallocN(newx * newy * 4 * sizeof(float),
		                "q_scale rectfloat");
		q_scale_float(ibuf->rect_float, newrect, ibuf->x, ibuf->y,
		              newx, newy);
		imb_freerectfloatImBuf(ibuf);
		ibuf->mall |= IB_rectfloat;
		ibuf->rect_float = newrect;
	}
	ibuf->x = newx;
	ibuf->y = newy;

	return true;
}

static ImBuf *scaledownx(struct ImBuf *ibuf, int newx)
{
	const int do_rect = (ibuf->rect != NULL);
	const int do_float = (ibuf->rect_float != NULL);
	const size_t rect_size = ibuf->x * ibuf->y * 4;

	uchar *rect, *_newrect, *newrect;
	float *rectf, *_newrectf, *newrectf;
	float sample, add, val[4], nval[4], valf[4], nvalf[4];
	int x, y;

	rectf = _newrectf = newrectf = NULL;
	rect = _newrect = newrect = NULL;
	nval[0] =  nval[1] = nval[2] = nval[3] = 0.0f;
	nvalf[0] = nvalf[1] = nvalf[2] = nvalf[3] = 0.0f;

	if (!do_rect && !do_float) return (ibuf);

	if (do_rect) {
		_newrect = MEM_mallocN(newx * ibuf->y * sizeof(uchar) * 4, "scaledownx");
		if (_newrect == NULL) return(ibuf);
	}
	if (do_float) {
		_newrectf = MEM_mallocN(newx * ibuf->y * sizeof(float) * 4, "scaledownxf");
		if (_newrectf == NULL) {
			if (_newrect) MEM_freeN(_newrect);
			return(ibuf);
		}
	}

	add = (ibuf->x - 0.01) / newx;

	if (do_rect) {
		rect = (uchar *) ibuf->rect;
		newrect = _newrect;
	}
	if (do_float) {
		rectf = ibuf->rect_float;
		newrectf = _newrectf;
	}
		
	for (y = ibuf->y; y > 0; y--) {
		sample = 0.0f;
		val[0] =  val[1] = val[2] = val[3] = 0.0f;
		valf[0] = valf[1] = valf[2] = valf[3] = 0.0f;

		for (x = newx; x > 0; x--) {
			if (do_rect) {
				nval[0] = -val[0] * sample;
				nval[1] = -val[1] * sample;
				nval[2] = -val[2] * sample;
				nval[3] = -val[3] * sample;
			}
			if (do_float) {
				nvalf[0] = -valf[0] * sample;
				nvalf[1] = -valf[1] * sample;
				nvalf[2] = -valf[2] * sample;
				nvalf[3] = -valf[3] * sample;
			}
			
			sample += add;

			while (sample >= 1.0f) {
				sample -= 1.0f;
				
				if (do_rect) {
					nval[0] += rect[0];
					nval[1] += rect[1];
					nval[2] += rect[2];
					nval[3] += rect[3];
					rect += 4;
				}
				if (do_float) {
					nvalf[0] += rectf[0];
					nvalf[1] += rectf[1];
					nvalf[2] += rectf[2];
					nvalf[3] += rectf[3];
					rectf += 4;
				}
			}
			
			if (do_rect) {
				val[0] = rect[0]; val[1] = rect[1]; val[2] = rect[2]; val[3] = rect[3];
				rect += 4;
				
				newrect[0] = ((nval[0] + sample * val[0]) / add + 0.5f);
				newrect[1] = ((nval[1] + sample * val[1]) / add + 0.5f);
				newrect[2] = ((nval[2] + sample * val[2]) / add + 0.5f);
				newrect[3] = ((nval[3] + sample * val[3]) / add + 0.5f);
				
				newrect += 4;
			}
			if (do_float) {
				
				valf[0] = rectf[0]; valf[1] = rectf[1]; valf[2] = rectf[2]; valf[3] = rectf[3];
				rectf += 4;
				
				newrectf[0] = ((nvalf[0] + sample * valf[0]) / add);
				newrectf[1] = ((nvalf[1] + sample * valf[1]) / add);
				newrectf[2] = ((nvalf[2] + sample * valf[2]) / add);
				newrectf[3] = ((nvalf[3] + sample * valf[3]) / add);
				
				newrectf += 4;
			}
			
			sample -= 1.0f;
		}
	}

	if (do_rect) {
		// printf("%ld %ld\n", (uchar *)rect - ((uchar *)ibuf->rect), rect_size);
		BLI_assert((uchar *)rect - ((uchar *)ibuf->rect) == rect_size); /* see bug [#26502] */
		imb_freerectImBuf(ibuf);
		ibuf->mall |= IB_rect;
		ibuf->rect = (unsigned int *) _newrect;
	}
	if (do_float) {
		// printf("%ld %ld\n", rectf - ibuf->rect_float, rect_size);
		BLI_assert((rectf - ibuf->rect_float) == rect_size); /* see bug [#26502] */
		imb_freerectfloatImBuf(ibuf);
		ibuf->mall |= IB_rectfloat;
		ibuf->rect_float = _newrectf;
	}
	(void)rect_size; /* UNUSED in release builds */
	
	ibuf->x = newx;
	return(ibuf);
}


static ImBuf *scaledowny(struct ImBuf *ibuf, int newy)
{
	const int do_rect = (ibuf->rect != NULL);
	const int do_float = (ibuf->rect_float != NULL);
	const size_t rect_size = ibuf->x * ibuf->y * 4;

	uchar *rect, *_newrect, *newrect;
	float *rectf, *_newrectf, *newrectf;
	float sample, add, val[4], nval[4], valf[4], nvalf[4];
	int x, y, skipx;

	rectf = _newrectf = newrectf = NULL;
	rect = _newrect = newrect = NULL;
	nval[0] =  nval[1] = nval[2] = nval[3] = 0.0f;
	nvalf[0] = nvalf[1] = nvalf[2] = nvalf[3] = 0.0f;

	if (!do_rect && !do_float) return (ibuf);

	if (do_rect) {
		_newrect = MEM_mallocN(newy * ibuf->x * sizeof(uchar) * 4, "scaledowny");
		if (_newrect == NULL) return(ibuf);
	}
	if (do_float) {
		_newrectf = MEM_mallocN(newy * ibuf->x * sizeof(float) * 4, "scaledownyf");
		if (_newrectf == NULL) {
			if (_newrect) MEM_freeN(_newrect);
			return(ibuf);
		}
	}

	add = (ibuf->y - 0.01) / newy;
	skipx = 4 * ibuf->x;

	for (x = skipx - 4; x >= 0; x -= 4) {
		if (do_rect) {
			rect = ((uchar *) ibuf->rect) + x;
			newrect = _newrect + x;
		}
		if (do_float) {
			rectf = ibuf->rect_float + x;
			newrectf = _newrectf + x;
		}
		
		sample = 0.0f;
		val[0] =  val[1] = val[2] = val[3] = 0.0f;
		valf[0] = valf[1] = valf[2] = valf[3] = 0.0f;

		for (y = newy; y > 0; y--) {
			if (do_rect) {
				nval[0] = -val[0] * sample;
				nval[1] = -val[1] * sample;
				nval[2] = -val[2] * sample;
				nval[3] = -val[3] * sample;
			}
			if (do_float) {
				nvalf[0] = -valf[0] * sample;
				nvalf[1] = -valf[1] * sample;
				nvalf[2] = -valf[2] * sample;
				nvalf[3] = -valf[3] * sample;
			}
			
			sample += add;

			while (sample >= 1.0f) {
				sample -= 1.0f;
				
				if (do_rect) {
					nval[0] += rect[0];
					nval[1] += rect[1];
					nval[2] += rect[2];
					nval[3] += rect[3];
					rect += skipx;
				}
				if (do_float) {
					nvalf[0] += rectf[0];
					nvalf[1] += rectf[1];
					nvalf[2] += rectf[2];
					nvalf[3] += rectf[3];
					rectf += skipx;
				}
			}

			if (do_rect) {
				val[0] = rect[0]; val[1] = rect[1]; val[2] = rect[2]; val[3] = rect[3];
				rect += skipx;
				
				newrect[0] = ((nval[0] + sample * val[0]) / add + 0.5f);
				newrect[1] = ((nval[1] + sample * val[1]) / add + 0.5f);
				newrect[2] = ((nval[2] + sample * val[2]) / add + 0.5f);
				newrect[3] = ((nval[3] + sample * val[3]) / add + 0.5f);
				
				newrect += skipx;
			}
			if (do_float) {
				
				valf[0] = rectf[0]; valf[1] = rectf[1]; valf[2] = rectf[2]; valf[3] = rectf[3];
				rectf += skipx;
				
				newrectf[0] = ((nvalf[0] + sample * valf[0]) / add);
				newrectf[1] = ((nvalf[1] + sample * valf[1]) / add);
				newrectf[2] = ((nvalf[2] + sample * valf[2]) / add);
				newrectf[3] = ((nvalf[3] + sample * valf[3]) / add);
				
				newrectf += skipx;
			}
			
			sample -= 1.0f;
		}
	}

	if (do_rect) {
		// printf("%ld %ld\n", (uchar *)rect - ((uchar *)ibuf->rect), rect_size);
		BLI_assert((uchar *)rect - ((uchar *)ibuf->rect) == rect_size); /* see bug [#26502] */
		imb_freerectImBuf(ibuf);
		ibuf->mall |= IB_rect;
		ibuf->rect = (unsigned int *) _newrect;
	}
	if (do_float) {
		// printf("%ld %ld\n", rectf - ibuf->rect_float, rect_size);
		BLI_assert((rectf - ibuf->rect_float) == rect_size); /* see bug [#26502] */
		imb_freerectfloatImBuf(ibuf);
		ibuf->mall |= IB_rectfloat;
		ibuf->rect_float = (float *) _newrectf;
	}
	(void)rect_size; /* UNUSED in release builds */
	
	ibuf->y = newy;
	return(ibuf);
}


static ImBuf *scaleupx(struct ImBuf *ibuf, int newx)
{
	uchar *rect, *_newrect = NULL, *newrect;
	float *rectf, *_newrectf = NULL, *newrectf;
	float sample, add;
	float val_a, nval_a, diff_a;
	float val_b, nval_b, diff_b;
	float val_g, nval_g, diff_g;
	float val_r, nval_r, diff_r;
	float val_af, nval_af, diff_af;
	float val_bf, nval_bf, diff_bf;
	float val_gf, nval_gf, diff_gf;
	float val_rf, nval_rf, diff_rf;
	int x, y;
	bool do_rect = false, do_float = false;

	val_a = nval_a = diff_a = val_b = nval_b = diff_b = 0;
	val_g = nval_g = diff_g = val_r = nval_r = diff_r = 0;
	val_af = nval_af = diff_af = val_bf = nval_bf = diff_bf = 0;
	val_gf = nval_gf = diff_gf = val_rf = nval_rf = diff_rf = 0;
	if (ibuf == NULL) return(NULL);
	if (ibuf->rect == NULL && ibuf->rect_float == NULL) return (ibuf);

	if (ibuf->rect) {
		do_rect = true;
		_newrect = MEM_mallocN(newx * ibuf->y * sizeof(int), "scaleupx");
		if (_newrect == NULL) return(ibuf);
	}
	if (ibuf->rect_float) {
		do_float = true;
		_newrectf = MEM_mallocN(newx * ibuf->y * sizeof(float) * 4, "scaleupxf");
		if (_newrectf == NULL) {
			if (_newrect) MEM_freeN(_newrect);
			return(ibuf);
		}
	}

	add = (ibuf->x - 1.001) / (newx - 1.0);

	rect = (uchar *) ibuf->rect;
	rectf = (float *) ibuf->rect_float;
	newrect = _newrect;
	newrectf = _newrectf;

	for (y = ibuf->y; y > 0; y--) {

		sample = 0;
		
		if (do_rect) {
			val_a = rect[0];
			nval_a = rect[4];
			diff_a = nval_a - val_a;
			val_a += 0.5f;

			val_b = rect[1];
			nval_b = rect[5];
			diff_b = nval_b - val_b;
			val_b += 0.5f;

			val_g = rect[2];
			nval_g = rect[6];
			diff_g = nval_g - val_g;
			val_g += 0.5f;

			val_r = rect[3];
			nval_r = rect[7];
			diff_r = nval_r - val_r;
			val_r += 0.5f;

			rect += 8;
		}
		if (do_float) {
			val_af = rectf[0];
			nval_af = rectf[4];
			diff_af = nval_af - val_af;
	
			val_bf = rectf[1];
			nval_bf = rectf[5];
			diff_bf = nval_bf - val_bf;

			val_gf = rectf[2];
			nval_gf = rectf[6];
			diff_gf = nval_gf - val_gf;

			val_rf = rectf[3];
			nval_rf = rectf[7];
			diff_rf = nval_rf - val_rf;

			rectf += 8;
		}
		for (x = newx; x > 0; x--) {
			if (sample >= 1.0f) {
				sample -= 1.0f;

				if (do_rect) {
					val_a = nval_a;
					nval_a = rect[0];
					diff_a = nval_a - val_a;
					val_a += 0.5f;

					val_b = nval_b;
					nval_b = rect[1];
					diff_b = nval_b - val_b;
					val_b += 0.5f;

					val_g = nval_g;
					nval_g = rect[2];
					diff_g = nval_g - val_g;
					val_g += 0.5f;

					val_r = nval_r;
					nval_r = rect[3];
					diff_r = nval_r - val_r;
					val_r += 0.5f;
					rect += 4;
				}
				if (do_float) {
					val_af = nval_af;
					nval_af = rectf[0];
					diff_af = nval_af - val_af;
	
					val_bf = nval_bf;
					nval_bf = rectf[1];
					diff_bf = nval_bf - val_bf;

					val_gf = nval_gf;
					nval_gf = rectf[2];
					diff_gf = nval_gf - val_gf;

					val_rf = nval_rf;
					nval_rf = rectf[3];
					diff_rf = nval_rf - val_rf;
					rectf += 4;
				}
			}
			if (do_rect) {
				newrect[0] = val_a + sample * diff_a;
				newrect[1] = val_b + sample * diff_b;
				newrect[2] = val_g + sample * diff_g;
				newrect[3] = val_r + sample * diff_r;
				newrect += 4;
			}
			if (do_float) {
				newrectf[0] = val_af + sample * diff_af;
				newrectf[1] = val_bf + sample * diff_bf;
				newrectf[2] = val_gf + sample * diff_gf;
				newrectf[3] = val_rf + sample * diff_rf;
				newrectf += 4;
			}
			sample += add;
		}
	}

	if (do_rect) {
		imb_freerectImBuf(ibuf);
		ibuf->mall |= IB_rect;
		ibuf->rect = (unsigned int *) _newrect;
	}
	if (do_float) {
		imb_freerectfloatImBuf(ibuf);
		ibuf->mall |= IB_rectfloat;
		ibuf->rect_float = (float *) _newrectf;
	}
	
	ibuf->x = newx;
	return(ibuf);
}

static ImBuf *scaleupy(struct ImBuf *ibuf, int newy)
{
	uchar *rect, *_newrect = NULL, *newrect;
	float *rectf, *_newrectf = NULL, *newrectf;
	float sample, add;
	float val_a, nval_a, diff_a;
	float val_b, nval_b, diff_b;
	float val_g, nval_g, diff_g;
	float val_r, nval_r, diff_r;
	float val_af, nval_af, diff_af;
	float val_bf, nval_bf, diff_bf;
	float val_gf, nval_gf, diff_gf;
	float val_rf, nval_rf, diff_rf;
	int x, y, skipx;
	bool do_rect = false, do_float = false;

	val_a = nval_a = diff_a = val_b = nval_b = diff_b = 0;
	val_g = nval_g = diff_g = val_r = nval_r = diff_r = 0;
	val_af = nval_af = diff_af = val_bf = nval_bf = diff_bf = 0;
	val_gf = nval_gf = diff_gf = val_rf = nval_rf = diff_rf = 0;
	if (ibuf == NULL) return(NULL);
	if (ibuf->rect == NULL && ibuf->rect_float == NULL) return (ibuf);

	if (ibuf->rect) {
		do_rect = true;
		_newrect = MEM_mallocN(ibuf->x * newy * sizeof(int), "scaleupy");
		if (_newrect == NULL) return(ibuf);
	}
	if (ibuf->rect_float) {
		do_float = true;
		_newrectf = MEM_mallocN(ibuf->x * newy * sizeof(float) * 4, "scaleupyf");
		if (_newrectf == NULL) {
			if (_newrect) MEM_freeN(_newrect);
			return(ibuf);
		}
	}

	add = (ibuf->y - 1.001) / (newy - 1.0);
	skipx = 4 * ibuf->x;

	rect = (uchar *) ibuf->rect;
	rectf = (float *) ibuf->rect_float;
	newrect = _newrect;
	newrectf = _newrectf;

	for (x = ibuf->x; x > 0; x--) {

		sample = 0;
		if (do_rect) {
			rect = ((uchar *)ibuf->rect) + 4 * (x - 1);
			newrect = _newrect + 4 * (x - 1);

			val_a = rect[0];
			nval_a = rect[skipx];
			diff_a = nval_a - val_a;
			val_a += 0.5f;

			val_b = rect[1];
			nval_b = rect[skipx + 1];
			diff_b = nval_b - val_b;
			val_b += 0.5f;

			val_g = rect[2];
			nval_g = rect[skipx + 2];
			diff_g = nval_g - val_g;
			val_g += 0.5f;

			val_r = rect[3];
			nval_r = rect[skipx + 3];
			diff_r = nval_r - val_r;
			val_r += 0.5f;

			rect += 2 * skipx;
		}
		if (do_float) {
			rectf = ibuf->rect_float + 4 * (x - 1);
			newrectf = _newrectf + 4 * (x - 1);

			val_af = rectf[0];
			nval_af = rectf[skipx];
			diff_af = nval_af - val_af;
	
			val_bf = rectf[1];
			nval_bf = rectf[skipx + 1];
			diff_bf = nval_bf - val_bf;

			val_gf = rectf[2];
			nval_gf = rectf[skipx + 2];
			diff_gf = nval_gf - val_gf;

			val_rf = rectf[3];
			nval_rf = rectf[skipx + 3];
			diff_rf = nval_rf - val_rf;

			rectf += 2 * skipx;
		}
		
		for (y = newy; y > 0; y--) {
			if (sample >= 1.0f) {
				sample -= 1.0f;

				if (do_rect) {
					val_a = nval_a;
					nval_a = rect[0];
					diff_a = nval_a - val_a;
					val_a += 0.5f;

					val_b = nval_b;
					nval_b = rect[1];
					diff_b = nval_b - val_b;
					val_b += 0.5f;

					val_g = nval_g;
					nval_g = rect[2];
					diff_g = nval_g - val_g;
					val_g += 0.5f;

					val_r = nval_r;
					nval_r = rect[3];
					diff_r = nval_r - val_r;
					val_r += 0.5f;
					rect += skipx;
				}
				if (do_float) {
					val_af = nval_af;
					nval_af = rectf[0];
					diff_af = nval_af - val_af;
	
					val_bf = nval_bf;
					nval_bf = rectf[1];
					diff_bf = nval_bf - val_bf;

					val_gf = nval_gf;
					nval_gf = rectf[2];
					diff_gf = nval_gf - val_gf;

					val_rf = nval_rf;
					nval_rf = rectf[3];
					diff_rf = nval_rf - val_rf;
					rectf += skipx;
				}
			}
			if (do_rect) {
				newrect[0] = val_a + sample * diff_a;
				newrect[1] = val_b + sample * diff_b;
				newrect[2] = val_g + sample * diff_g;
				newrect[3] = val_r + sample * diff_r;
				newrect += skipx;
			}
			if (do_float) {
				newrectf[0] = val_af + sample * diff_af;
				newrectf[1] = val_bf + sample * diff_bf;
				newrectf[2] = val_gf + sample * diff_gf;
				newrectf[3] = val_rf + sample * diff_rf;
				newrectf += skipx;
			}
			sample += add;
		}
	}

	if (do_rect) {
		imb_freerectImBuf(ibuf);
		ibuf->mall |= IB_rect;
		ibuf->rect = (unsigned int *) _newrect;
	}
	if (do_float) {
		imb_freerectfloatImBuf(ibuf);
		ibuf->mall |= IB_rectfloat;
		ibuf->rect_float = (float *) _newrectf;
	}
	
	ibuf->y = newy;
	return(ibuf);
}

static void scalefast_Z_ImBuf(ImBuf *ibuf, int newx, int newy)
{
	int *zbuf, *newzbuf, *_newzbuf = NULL;
	float *zbuf_float, *newzbuf_float, *_newzbuf_float = NULL;
	int x, y;
	int ofsx, ofsy, stepx, stepy;

	if (ibuf->zbuf) {
		_newzbuf = MEM_mallocN(newx * newy * sizeof(int), __func__);
		if (_newzbuf == NULL) {
			IMB_freezbufImBuf(ibuf);
		}
	}

	if (ibuf->zbuf_float) {
		_newzbuf_float = MEM_mallocN((size_t)newx * newy * sizeof(float), __func__);
		if (_newzbuf_float == NULL) {
			IMB_freezbuffloatImBuf(ibuf);
		}
	}

	if (!_newzbuf && !_newzbuf_float) {
		return;
	}

	stepx = (65536.0 * (ibuf->x - 1.0) / (newx - 1.0)) + 0.5;
	stepy = (65536.0 * (ibuf->y - 1.0) / (newy - 1.0)) + 0.5;
	ofsy = 32768;

	newzbuf = _newzbuf;
	newzbuf_float = _newzbuf_float;

	for (y = newy; y > 0; y--, ofsy += stepy) {
		if (newzbuf) {
			zbuf = ibuf->zbuf;
			zbuf += (ofsy >> 16) * ibuf->x;
			ofsx = 32768;
			for (x = newx; x > 0; x--, ofsx += stepx) {
				*newzbuf++ = zbuf[ofsx >> 16];
			}
		}

		if (newzbuf_float) {
			zbuf_float = ibuf->zbuf_float;
			zbuf_float += (ofsy >> 16) * ibuf->x;
			ofsx = 32768;
			for (x = newx; x > 0; x--, ofsx += stepx) {
				*newzbuf_float++ = zbuf_float[ofsx >> 16];
			}
		}
	}

	if (_newzbuf) {
		IMB_freezbufImBuf(ibuf);
		ibuf->mall |= IB_zbuf;
		ibuf->zbuf = _newzbuf;
	}

	if (_newzbuf_float) {
		IMB_freezbuffloatImBuf(ibuf);
		ibuf->mall |= IB_zbuffloat;
		ibuf->zbuf_float = _newzbuf_float;
	}
}

struct ImBuf *IMB_scaleImBuf(struct ImBuf *ibuf, unsigned int newx, unsigned int newy)
{
	if (ibuf == NULL) return (NULL);
	if (ibuf->rect == NULL && ibuf->rect_float == NULL) return (ibuf);
	
	if (newx == ibuf->x && newy == ibuf->y) { return ibuf; }

	/* scaleup / scaledown functions below change ibuf->x and ibuf->y
	 * so we first scale the Z-buffer (if any) */
	scalefast_Z_ImBuf(ibuf, newx, newy);

	/* try to scale common cases in a fast way */
	/* disabled, quality loss is unacceptable, see report #18609  (ton) */
	if (0 && q_scale_linear_interpolation(ibuf, newx, newy)) {
		return ibuf;
	}

	if (newx && (newx < ibuf->x)) scaledownx(ibuf, newx);
	if (newy && (newy < ibuf->y)) scaledowny(ibuf, newy);
	if (newx && (newx > ibuf->x)) scaleupx(ibuf, newx);
	if (newy && (newy > ibuf->y)) scaleupy(ibuf, newy);
	
	return(ibuf);
}

struct imbufRGBA {
	float r, g, b, a;
};

struct ImBuf *IMB_scalefastImBuf(struct ImBuf *ibuf, unsigned int newx, unsigned int newy)
{
	unsigned int *rect, *_newrect, *newrect;
	struct imbufRGBA *rectf, *_newrectf, *newrectf;
	int x, y;
	bool do_float = false, do_rect = false;
	size_t ofsx, ofsy, stepx, stepy;

	rect = NULL; _newrect = NULL; newrect = NULL;
	rectf = NULL; _newrectf = NULL; newrectf = NULL;

	if (ibuf == NULL) return(NULL);
	if (ibuf->rect) do_rect = true;
	if (ibuf->rect_float) do_float = true;
	if (do_rect == false && do_float == false) return(ibuf);
	
	if (newx == ibuf->x && newy == ibuf->y) return(ibuf);
	
	if (do_rect) {
		_newrect = MEM_mallocN(newx * newy * sizeof(int), "scalefastimbuf");
		if (_newrect == NULL) return(ibuf);
		newrect = _newrect;
	}
	
	if (do_float) {
		_newrectf = MEM_mallocN(newx * newy * sizeof(float) * 4, "scalefastimbuf f");
		if (_newrectf == NULL) {
			if (_newrect) MEM_freeN(_newrect);
			return(ibuf);
		}
		newrectf = _newrectf;
	}

	stepx = (65536.0 * (ibuf->x - 1.0) / (newx - 1.0)) + 0.5;
	stepy = (65536.0 * (ibuf->y - 1.0) / (newy - 1.0)) + 0.5;
	ofsy = 32768;

	for (y = newy; y > 0; y--, ofsy += stepy) {
		if (do_rect) {
			rect = ibuf->rect;
			rect += (ofsy >> 16) * ibuf->x;
			ofsx = 32768;

			for (x = newx; x > 0; x--, ofsx += stepx) {
				*newrect++ = rect[ofsx >> 16];
			}
		}

		if (do_float) {
			rectf = (struct imbufRGBA *)ibuf->rect_float;
			rectf += (ofsy >> 16) * ibuf->x;
			ofsx = 32768;

			for (x = newx; x > 0; x--, ofsx += stepx) {
				*newrectf++ = rectf[ofsx >> 16];
			}
		}
	}

	if (do_rect) {
		imb_freerectImBuf(ibuf);
		ibuf->mall |= IB_rect;
		ibuf->rect = _newrect;
	}
	
	if (do_float) {
		imb_freerectfloatImBuf(ibuf);
		ibuf->mall |= IB_rectfloat;
		ibuf->rect_float = (float *)_newrectf;
	}
	
	scalefast_Z_ImBuf(ibuf, newx, newy);
	
	ibuf->x = newx;
	ibuf->y = newy;
	return(ibuf);
}

/* ******** threaded scaling ******** */

typedef struct ScaleTreadInitData {
	ImBuf *ibuf;

	unsigned int newx;
	unsigned int newy;

	unsigned char *byte_buffer;
	float *float_buffer;
} ScaleTreadInitData;

typedef struct ScaleThreadData {
	ImBuf *ibuf;

	unsigned int newx;
	unsigned int newy;

	int start_line;
	int tot_line;

	unsigned char *byte_buffer;
	float *float_buffer;
} ScaleThreadData;

static void scale_thread_init(void *data_v, int start_line, int tot_line, void *init_data_v)
{
	ScaleThreadData *data = (ScaleThreadData *) data_v;
	ScaleTreadInitData *init_data = (ScaleTreadInitData *) init_data_v;

	data->ibuf = init_data->ibuf;

	data->newx = init_data->newx;
	data->newy = init_data->newy;

	data->start_line = start_line;
	data->tot_line = tot_line;

	data->byte_buffer = init_data->byte_buffer;
	data->float_buffer = init_data->float_buffer;
}

static void *do_scale_thread(void *data_v)
{
	ScaleThreadData *data = (ScaleThreadData *) data_v;
	ImBuf *ibuf = data->ibuf;
	int i;
	float factor_x = (float) ibuf->x / data->newx;
	float factor_y = (float) ibuf->y / data->newy;

	for (i = 0; i < data->tot_line; i++) {
		int y = data->start_line + i;
		int x;

		for (x = 0; x < data->newx; x++) {
			float u = (float) x * factor_x;
			float v = (float) y * factor_y;
			int offset = y * data->newx + x;

			if (data->byte_buffer) {
				unsigned char *pixel = data->byte_buffer + 4 * offset;
				BLI_bilinear_interpolation_char((unsigned char *) ibuf->rect, pixel, ibuf->x, ibuf->y, 4, u, v);
			}

			if (data->float_buffer) {
				float *pixel = data->float_buffer + ibuf->channels * offset;
				BLI_bilinear_interpolation_fl(ibuf->rect_float, pixel, ibuf->x, ibuf->y, ibuf->channels, u, v);
			}
		}
	}

	return NULL;
}

void IMB_scaleImBuf_threaded(ImBuf *ibuf, unsigned int newx, unsigned int newy)
{
	ScaleTreadInitData init_data = {NULL};

	/* prepare initialization data */
	init_data.ibuf = ibuf;

	init_data.newx = newx;
	init_data.newy = newy;

	if (ibuf->rect)
		init_data.byte_buffer = MEM_mallocN(4 * newx * newy * sizeof(char), "threaded scale byte buffer");

	if (ibuf->rect_float)
		init_data.float_buffer = MEM_mallocN(ibuf->channels * newx * newy * sizeof(float), "threaded scale float buffer");

	/* actual scaling threads */
	IMB_processor_apply_threaded(newy, sizeof(ScaleThreadData), &init_data,
	                             scale_thread_init, do_scale_thread);

	/* alter image buffer */
	ibuf->x = newx;
	ibuf->y = newy;

	if (ibuf->rect) {
		imb_freerectImBuf(ibuf);
		ibuf->mall |= IB_rect;
		ibuf->rect = (unsigned int *) init_data.byte_buffer;
	}

	if (ibuf->rect_float) {
		imb_freerectfloatImBuf(ibuf);
		ibuf->mall |= IB_rectfloat;
		ibuf->rect_float = init_data.float_buffer;
	}
}
