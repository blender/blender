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

/** \file blender/imbuf/intern/divers.c
 *  \ingroup imbuf
 */


#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_allocimbuf.h"

#include "MEM_guardedalloc.h"

/**************************** Interlace/Deinterlace **************************/

void IMB_de_interlace(struct ImBuf *ibuf)
{
	struct ImBuf * tbuf1, * tbuf2;
	
	if (ibuf == NULL) return;
	if (ibuf->flags & IB_fields) return;
	ibuf->flags |= IB_fields;
	
	if (ibuf->rect) {
		/* make copies */
		tbuf1 = IMB_allocImBuf(ibuf->x, ibuf->y / 2, 32, IB_rect);
		tbuf2 = IMB_allocImBuf(ibuf->x, ibuf->y / 2, 32, IB_rect);
		
		ibuf->x *= 2;	
		IMB_rectcpy(tbuf1, ibuf, 0, 0, 0, 0, ibuf->x, ibuf->y);
		IMB_rectcpy(tbuf2, ibuf, 0, 0, tbuf2->x, 0, ibuf->x, ibuf->y);
	
		ibuf->x /= 2;
		IMB_rectcpy(ibuf, tbuf1, 0, 0, 0, 0, tbuf1->x, tbuf1->y);
		IMB_rectcpy(ibuf, tbuf2, 0, tbuf2->y, 0, 0, tbuf2->x, tbuf2->y);
		
		IMB_freeImBuf(tbuf1);
		IMB_freeImBuf(tbuf2);
	}
	ibuf->y /= 2;
}

void IMB_interlace(struct ImBuf *ibuf)
{
	struct ImBuf * tbuf1, * tbuf2;

	if (ibuf == NULL) return;
	ibuf->flags &= ~IB_fields;

	ibuf->y *= 2;

	if (ibuf->rect) {
		/* make copies */
		tbuf1 = IMB_allocImBuf(ibuf->x, ibuf->y / 2, 32, IB_rect);
		tbuf2 = IMB_allocImBuf(ibuf->x, ibuf->y / 2, 32, IB_rect);

		IMB_rectcpy(tbuf1, ibuf, 0, 0, 0, 0, ibuf->x, ibuf->y);
		IMB_rectcpy(tbuf2, ibuf, 0, 0, 0, tbuf2->y, ibuf->x, ibuf->y);

		ibuf->x *= 2;
		IMB_rectcpy(ibuf, tbuf1, 0, 0, 0, 0, tbuf1->x, tbuf1->y);
		IMB_rectcpy(ibuf, tbuf2, tbuf2->x, 0, 0, 0, tbuf2->x, tbuf2->y);
		ibuf->x /= 2;

		IMB_freeImBuf(tbuf1);
		IMB_freeImBuf(tbuf2);
	}
}

/************************* Generic Buffer Conversion *************************/

MINLINE void byte_to_float_v4(float f[4], const uchar b[4])
{
	f[0] = b[0] * (1.0f/255.0f);
	f[1] = b[1] * (1.0f/255.0f);
	f[2] = b[2] * (1.0f/255.0f);
	f[3] = b[3] * (1.0f/255.0f);
}

MINLINE void float_to_byte_v4(uchar b[4], const float f[4])
{
	F4TOCHAR4(f, b);
}

MINLINE void float_to_byte_dither_v4(uchar b[4], const float f[4], float dither)
{
	float tmp[4] = {f[0]+dither, f[1]+dither, f[2]+dither, f[3]+dither};
	float_to_byte_v4(b, tmp);
}

/* float to byte pixels, output 4-channel RGBA */
void IMB_buffer_byte_from_float(uchar *rect_to, const float *rect_from,
	int channels_from, int dither, int profile_to, int profile_from, int predivide,
	int width, int height, int stride_to, int stride_from)
{
	float tmp[4];
	float dither_fac = dither/255.0f;
	int x, y;

	/* we need valid profiles */
	BLI_assert(profile_to != IB_PROFILE_NONE);
	BLI_assert(profile_from != IB_PROFILE_NONE);

	if(channels_from==1) {
		/* single channel input */
		for(y = 0; y < height; y++) {
			const float *from = rect_from + stride_from*y;
			uchar *to = rect_to + stride_to*y*4;

			for(x = 0; x < width; x++, from++, to+=4)
				to[0] = to[1] = to[2] = to[3] = FTOCHAR(from[0]);
		}
	}
	else if(channels_from == 3) {
		/* RGB input */
		for(y = 0; y < height; y++) {
			const float *from = rect_from + stride_from*y*3;
			uchar *to = rect_to + stride_to*y*4;

			if(profile_to == profile_from) {
				/* no color space conversion */
				for(x = 0; x < width; x++, from+=3, to+=4) {
					F3TOCHAR3(from, to);
					to[3] = 255;
				}
			}
			else if(profile_to == IB_PROFILE_SRGB) {
				/* convert from linear to sRGB */
				for(x = 0; x < width; x++, from+=3, to+=4) {
					linearrgb_to_srgb_v3_v3(tmp, from);
					F3TOCHAR3(tmp, to);
					to[3] = 255;
				}
			}
			else if(profile_to == IB_PROFILE_LINEAR_RGB) {
				/* convert from sRGB to linear */
				for(x = 0; x < width; x++, from+=3, to+=4) {
					srgb_to_linearrgb_v3_v3(tmp, from);
					F3TOCHAR3(tmp, to);
					to[3] = 255;
				}
			}
		}
	}
	else if(channels_from == 4) {
		/* RGBA input */
		for(y = 0; y < height; y++) {
			const float *from = rect_from + stride_from*y*4;
			uchar *to = rect_to + stride_to*y*4;

			if(profile_to == profile_from) {
				/* no color space conversion */
				if(dither) {
					for(x = 0; x < width; x++, from+=4, to+=4)
						float_to_byte_dither_v4(to, from, (BLI_frand()-0.5f)*dither_fac);
				}
				else {
					for(x = 0; x < width; x++, from+=4, to+=4)
						float_to_byte_v4(to, from);
				}
			}
			else if(profile_to == IB_PROFILE_SRGB) {
				/* convert from linear to sRGB */
				if(dither && predivide) {
					for(x = 0; x < width; x++, from+=4, to+=4) {
						linearrgb_to_srgb_predivide_v4(tmp, from);
						float_to_byte_dither_v4(to, tmp, (BLI_frand()-0.5f)*dither_fac);
					}
				}
				else if(dither) {
					for(x = 0; x < width; x++, from+=4, to+=4) {
						linearrgb_to_srgb_v4(tmp, from);
						float_to_byte_dither_v4(to, tmp, (BLI_frand()-0.5f)*dither_fac);
					}
				}
				else if(predivide) {
					for(x = 0; x < width; x++, from+=4, to+=4) {
						linearrgb_to_srgb_predivide_v4(tmp, from);
						float_to_byte_v4(to, tmp);
					}
				}
				else {
					for(x = 0; x < width; x++, from+=4, to+=4) {
						linearrgb_to_srgb_v4(tmp, from);
						float_to_byte_v4(to, tmp);
					}
				}
			}
			else if(profile_to == IB_PROFILE_LINEAR_RGB) {
				/* convert from sRGB to linear */
				if(dither && predivide) {
					for(x = 0; x < width; x++, from+=4, to+=4) {
						srgb_to_linearrgb_predivide_v4(tmp, from);
						float_to_byte_dither_v4(to, tmp, (BLI_frand()-0.5f)*dither_fac);
					}
				}
				else if(dither) {
					for(x = 0; x < width; x++, from+=4, to+=4) {
						srgb_to_linearrgb_v4(tmp, from);
						float_to_byte_dither_v4(to, tmp, (BLI_frand()-0.5f)*dither_fac);
					}
				}
				else if(predivide) {
					for(x = 0; x < width; x++, from+=4, to+=4) {
						srgb_to_linearrgb_predivide_v4(tmp, from);
						float_to_byte_v4(to, tmp);
					}
				}
				else {
					for(x = 0; x < width; x++, from+=4, to+=4) {
						srgb_to_linearrgb_v4(tmp, from);
						float_to_byte_v4(to, tmp);
					}
				}
			}
		}
	}
}

/* byte to float pixels, input and output 4-channel RGBA  */
void IMB_buffer_float_from_byte(float *rect_to, const uchar *rect_from,
	int profile_to, int profile_from, int predivide,
	int width, int height, int stride_to, int stride_from)
{
	float tmp[4];
	int x, y;

	/* we need valid profiles */
	BLI_assert(profile_to != IB_PROFILE_NONE);
	BLI_assert(profile_from != IB_PROFILE_NONE);

	/* RGBA input */
	for(y = 0; y < height; y++) {
		const uchar *from = rect_from + stride_from*y*4;
		float *to = rect_to + stride_to*y*4;

		if(profile_to == profile_from) {
			/* no color space conversion */
			for(x = 0; x < width; x++, from+=4, to+=4)
				byte_to_float_v4(to, from);
		}
		else if(profile_to == IB_PROFILE_LINEAR_RGB) {
			/* convert sRGB to linear */
			if(predivide) {
				for(x = 0; x < width; x++, from+=4, to+=4) {
					byte_to_float_v4(tmp, from);
					srgb_to_linearrgb_predivide_v4(to, tmp);
				}
			}
			else {
				for(x = 0; x < width; x++, from+=4, to+=4) {
					byte_to_float_v4(tmp, from);
					srgb_to_linearrgb_v4(to, tmp);
				}
			}
		}
		else if(profile_to == IB_PROFILE_SRGB) {
			/* convert linear to sRGB */
			if(predivide) {
				for(x = 0; x < width; x++, from+=4, to+=4) {
					byte_to_float_v4(tmp, from);
					linearrgb_to_srgb_predivide_v4(to, tmp);
				}
			}
			else {
				for(x = 0; x < width; x++, from+=4, to+=4) {
					byte_to_float_v4(tmp, from);
					linearrgb_to_srgb_v4(to, tmp);
				}
			}
		}
	}
}

/* float to float pixels, output 4-channel RGBA */
void IMB_buffer_float_from_float(float *rect_to, const float *rect_from,
	int channels_from, int profile_to, int profile_from, int predivide,
	int width, int height, int stride_to, int stride_from)
{
	int x, y;

	/* we need valid profiles */
	BLI_assert(profile_to != IB_PROFILE_NONE);
	BLI_assert(profile_from != IB_PROFILE_NONE);

	if(channels_from==1) {
		/* single channel input */
		for(y = 0; y < height; y++) {
			const float *from = rect_from + stride_from*y;
			float *to = rect_to + stride_to*y*4;

			for(x = 0; x < width; x++, from++, to+=4)
				to[0] = to[1] = to[2] = to[3] = from[0];
		}
	}
	else if(channels_from == 3) {
		/* RGB input */
		for(y = 0; y < height; y++) {
			const float *from = rect_from + stride_from*y*3;
			float *to = rect_to + stride_to*y*4;

			if(profile_to == profile_from) {
				/* no color space conversion */
				for(x = 0; x < width; x++, from+=3, to+=4) {
					copy_v3_v3(to, from);
					to[3] = 1.0f;
				}
			}
			else if(profile_to == IB_PROFILE_LINEAR_RGB) {
				/* convert from sRGB to linear */
				for(x = 0; x < width; x++, from+=3, to+=4) {
					srgb_to_linearrgb_v3_v3(to, from);
					to[3] = 1.0f;
				}
			}
			else if(profile_to == IB_PROFILE_SRGB) {
				/* convert from linear to sRGB */
				for(x = 0; x < width; x++, from+=3, to+=4) {
					linearrgb_to_srgb_v3_v3(to, from);
					to[3] = 1.0f;
				}
			}
		}
	}
	else if(channels_from == 4) {
		/* RGBA input */
		for(y = 0; y < height; y++) {
			const float *from = rect_from + stride_from*y*4;
			float *to = rect_to + stride_to*y*4;

			if(profile_to == profile_from) {
				/* same profile, copy */
				memcpy(to, from, sizeof(float)*4*width);
			}
			else if(profile_to == IB_PROFILE_LINEAR_RGB) {
				/* convert to sRGB to linear */
				if(predivide) {
					for(x = 0; x < width; x++, from+=4, to+=4)
						srgb_to_linearrgb_predivide_v4(to, from);
				}
				else {
					for(x = 0; x < width; x++, from+=4, to+=4)
						srgb_to_linearrgb_v4(to, from);
				}
			}
			else if(profile_to == IB_PROFILE_SRGB) {
				/* convert from linear to sRGB */
				if(predivide) {
					for(x = 0; x < width; x++, from+=4, to+=4)
						linearrgb_to_srgb_predivide_v4(to, from);
				}
				else {
					for(x = 0; x < width; x++, from+=4, to+=4)
						linearrgb_to_srgb_v4(to, from);
				}
			}
		}
	}
}

/* byte to byte pixels, input and output 4-channel RGBA */
void IMB_buffer_byte_from_byte(uchar *rect_to, const uchar *rect_from,
	int profile_to, int profile_from, int predivide,
	int width, int height, int stride_to, int stride_from)
{
	float tmp[4];
	int x, y;

	/* we need valid profiles */
	BLI_assert(profile_to != IB_PROFILE_NONE);
	BLI_assert(profile_from != IB_PROFILE_NONE);

	/* always RGBA input */
	for(y = 0; y < height; y++) {
		const uchar *from = rect_from + stride_from*y*4;
		uchar *to = rect_to + stride_to*y*4;

		if(profile_to == profile_from) {
			/* same profile, copy */
			memcpy(to, from, sizeof(uchar)*4*width);
		}
		else if(profile_to == IB_PROFILE_LINEAR_RGB) {
			/* convert to sRGB to linear */
			if(predivide) {
				for(x = 0; x < width; x++, from+=4, to+=4) {
					byte_to_float_v4(tmp, from);
					srgb_to_linearrgb_predivide_v4(tmp, tmp);
					float_to_byte_v4(to, tmp);
				}
			}
			else {
				for(x = 0; x < width; x++, from+=4, to+=4) {
					byte_to_float_v4(tmp, from);
					srgb_to_linearrgb_v4(tmp, tmp);
					float_to_byte_v4(to, tmp);
				}
			}
		}
		else if(profile_to == IB_PROFILE_SRGB) {
			/* convert from linear to sRGB */
			if(predivide) {
				for(x = 0; x < width; x++, from+=4, to+=4) {
					byte_to_float_v4(tmp, from);
					linearrgb_to_srgb_predivide_v4(tmp, tmp);
					float_to_byte_v4(to, tmp);
				}
			}
			else {
				for(x = 0; x < width; x++, from+=4, to+=4) {
					byte_to_float_v4(tmp, from);
					linearrgb_to_srgb_v4(tmp, tmp);
					float_to_byte_v4(to, tmp);
				}
			}
		}
	}
}

/****************************** ImBuf Conversion *****************************/

void IMB_rect_from_float(struct ImBuf *ibuf)
{
	int predivide= (ibuf->flags & IB_cm_predivide);
	int profile_from;

	/* verify we have a float buffer */
	if(ibuf->rect_float==NULL)
		return;

	/* create byte rect if it didn't exist yet */
	if(ibuf->rect==NULL)
		imb_addrectImBuf(ibuf);

	/* determine profiles */
	if(ibuf->profile == IB_PROFILE_LINEAR_RGB)
		profile_from = IB_PROFILE_LINEAR_RGB;
	else if(ELEM(ibuf->profile, IB_PROFILE_SRGB, IB_PROFILE_NONE))
		profile_from = IB_PROFILE_SRGB;
	else
		BLI_assert(0);

	/* do conversion */
	IMB_buffer_byte_from_float((uchar*)ibuf->rect, ibuf->rect_float,
		ibuf->channels, ibuf->dither, IB_PROFILE_SRGB, profile_from, predivide,
		ibuf->x, ibuf->y, ibuf->x, ibuf->x);

	/* ensure user flag is reset */
	ibuf->userflags &= ~IB_RECT_INVALID;
}

/* converts from linear float to sRGB byte for part of the texture, buffer will hold the changed part */
void IMB_partial_rect_from_float(struct ImBuf *ibuf, float *buffer, int x, int y, int w, int h)
{
	float *rect_float;
	uchar *rect_byte;
	int predivide= (ibuf->flags & IB_cm_predivide);
	int profile_from;

	/* verify we have a float buffer */
	if(ibuf->rect_float==NULL || buffer==NULL)
		return;

	/* create byte rect if it didn't exist yet */
	if(ibuf->rect==NULL)
		imb_addrectImBuf(ibuf);

	/* determine profiles */
	if(ibuf->profile == IB_PROFILE_LINEAR_RGB)
		profile_from = IB_PROFILE_LINEAR_RGB;
	else if(ELEM(ibuf->profile, IB_PROFILE_SRGB, IB_PROFILE_NONE))
		profile_from = IB_PROFILE_SRGB;
	else
		BLI_assert(0);

	/* do conversion */
	rect_float= ibuf->rect_float + (x + y*ibuf->x)*ibuf->channels;
	rect_byte= (uchar*)ibuf->rect + (x + y*ibuf->x)*4;

	IMB_buffer_float_from_float(buffer, rect_float,
		ibuf->channels, IB_PROFILE_SRGB, profile_from, predivide,
		w, h, w, ibuf->x);

	IMB_buffer_byte_from_float(rect_byte, buffer,
		4, ibuf->dither, IB_PROFILE_SRGB, IB_PROFILE_SRGB, 0,
		w, h, ibuf->x, w);

	/* ensure user flag is reset */
	ibuf->userflags &= ~IB_RECT_INVALID;
}

void IMB_float_from_rect(struct ImBuf *ibuf)
{
	int predivide= (ibuf->flags & IB_cm_predivide);
	int profile_from;

	/* verify if we byte and float buffers */
	if(ibuf->rect==NULL)
		return;

	if(ibuf->rect_float==NULL)
		if(imb_addrectfloatImBuf(ibuf) == 0)
			return;
	
	/* determine profiles */
	if(ibuf->profile == IB_PROFILE_NONE)
		profile_from = IB_PROFILE_LINEAR_RGB;
	else
		profile_from = IB_PROFILE_SRGB;
	
	/* do conversion */
	IMB_buffer_float_from_byte(ibuf->rect_float, (uchar*)ibuf->rect,
		IB_PROFILE_LINEAR_RGB, profile_from, predivide,
		ibuf->x, ibuf->y, ibuf->x, ibuf->x);
}

/* no profile conversion */
void IMB_float_from_rect_simple(struct ImBuf *ibuf)
{
	int predivide= (ibuf->flags & IB_cm_predivide);

	if(ibuf->rect_float==NULL)
		imb_addrectfloatImBuf(ibuf);

	IMB_buffer_float_from_byte(ibuf->rect_float, (uchar*)ibuf->rect,
		IB_PROFILE_SRGB, IB_PROFILE_SRGB, predivide,
		ibuf->x, ibuf->y, ibuf->x, ibuf->x);
}

void IMB_convert_profile(struct ImBuf *ibuf, int profile)
{
	int predivide= (ibuf->flags & IB_cm_predivide);
	int profile_from, profile_to;

	if(ibuf->profile == profile)
		return;

	/* determine profiles */
	if(ibuf->profile == IB_PROFILE_LINEAR_RGB)
		profile_from = IB_PROFILE_LINEAR_RGB;
	else if(ELEM(ibuf->profile, IB_PROFILE_SRGB, IB_PROFILE_NONE))
		profile_from = IB_PROFILE_SRGB;
	else
		BLI_assert(0);

	if(profile == IB_PROFILE_LINEAR_RGB)
		profile_to = IB_PROFILE_LINEAR_RGB;
	else if(ELEM(profile, IB_PROFILE_SRGB, IB_PROFILE_NONE))
		profile_to = IB_PROFILE_SRGB;
	else
		BLI_assert(0);
	
	/* do conversion */
	if(ibuf->rect_float) {
		IMB_buffer_float_from_float(ibuf->rect_float, ibuf->rect_float,
			4, profile_to, profile_from, predivide,
			ibuf->x, ibuf->y, ibuf->x, ibuf->x);
	}

	if(ibuf->rect) {
		IMB_buffer_byte_from_byte((uchar*)ibuf->rect, (uchar*)ibuf->rect,
			profile_to, profile_from, predivide,
			ibuf->x, ibuf->y, ibuf->x, ibuf->x);
	}

	/* set new profile */
	ibuf->profile= profile;
}

/* use when you need to get a buffer with a certain profile
 * if the return  */
float *IMB_float_profile_ensure(struct ImBuf *ibuf, int profile, int *alloc)
{
	int predivide= (ibuf->flags & IB_cm_predivide);
	int profile_from, profile_to;

	/* determine profiles */
	if(ibuf->profile == IB_PROFILE_NONE)
		profile_from = IB_PROFILE_LINEAR_RGB;
	else
		profile_from = IB_PROFILE_SRGB;

	if(profile == IB_PROFILE_NONE)
		profile_to = IB_PROFILE_LINEAR_RGB;
	else
		profile_to = IB_PROFILE_SRGB;
	
	if(profile_from == profile_to) {
		/* simple case, just allocate the buffer and return */
		*alloc= 0;

		if(ibuf->rect_float == NULL)
			IMB_float_from_rect(ibuf);

		return ibuf->rect_float;
	}
	else {
		/* conversion is needed, first check */
		float *fbuf= MEM_mallocN(ibuf->x * ibuf->y * sizeof(float) * 4, "IMB_float_profile_ensure");
		*alloc= 1;

		if(ibuf->rect_float == NULL) {
			IMB_buffer_float_from_byte(fbuf, (uchar*)ibuf->rect,
				profile_to, profile_from, predivide,
				ibuf->x, ibuf->y, ibuf->x, ibuf->x);
		}
		else {
			IMB_buffer_float_from_float(fbuf, ibuf->rect_float,
				4, profile_to, profile_from, predivide,
				ibuf->x, ibuf->y, ibuf->x, ibuf->x);
		}

		return fbuf;
	}
}

/**************************** Color to Grayscale *****************************/

/* no profile conversion */
void IMB_color_to_bw(struct ImBuf *ibuf)
{
	float *rctf= ibuf->rect_float;
	uchar *rct= (uchar*)ibuf->rect;
	int i;

	if(rctf) {
		for(i = ibuf->x * ibuf->y; i > 0; i--, rctf+=4)
			rctf[0]= rctf[1]= rctf[2]= rgb_to_grayscale(rctf);
	}

	if(rct) {
		for(i = ibuf->x * ibuf->y; i > 0; i--, rct+=4)
			rct[0]= rct[1]= rct[2]= rgb_to_grayscale_byte(rct);
	}
}
