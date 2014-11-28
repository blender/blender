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

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_filter.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"


#include "MEM_guardedalloc.h"

/**************************** Interlace/Deinterlace **************************/

void IMB_de_interlace(ImBuf *ibuf)
{
	ImBuf *tbuf1, *tbuf2;
	
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

void IMB_interlace(ImBuf *ibuf)
{
	ImBuf *tbuf1, *tbuf2;

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

/************************* Floyd-Steinberg dithering *************************/

typedef struct DitherContext {
	float dither;
} DitherContext;

static DitherContext *create_dither_context(float dither)
{
	DitherContext *di;

	di = MEM_mallocN(sizeof(DitherContext), "dithering context");
	di->dither = dither;

	return di;
}

static void clear_dither_context(DitherContext *di)
{
	MEM_freeN(di);
}

MINLINE float dither_random_value(float s, float t)
{
	static float vec[2] = {12.9898f, 78.233f};
	float st[2];
	float value;
	copy_v2_fl2(st, s, t);

	value = sinf(dot_v2v2(st, vec)) * 43758.5453f;
	return value - floorf(value);
}

/************************* Generic Buffer Conversion *************************/

MINLINE void ushort_to_byte_v4(uchar b[4], const unsigned short us[4])
{
	b[0] = USHORTTOUCHAR(us[0]);
	b[1] = USHORTTOUCHAR(us[1]);
	b[2] = USHORTTOUCHAR(us[2]);
	b[3] = USHORTTOUCHAR(us[3]);
}

MINLINE unsigned char ftochar(float value)
{
	return FTOCHAR(value);
}

MINLINE void ushort_to_byte_dither_v4(uchar b[4], const unsigned short us[4], DitherContext *di, float s, float t)
{
#define USHORTTOFLOAT(val) ((float)val / 65535.0f)
	float dither_value = dither_random_value(s, t) * 0.005f * di->dither;

	b[0] = ftochar(dither_value + USHORTTOFLOAT(us[0]));
	b[1] = ftochar(dither_value + USHORTTOFLOAT(us[1]));
	b[2] = ftochar(dither_value + USHORTTOFLOAT(us[2]));
	b[3] = USHORTTOUCHAR(us[3]);

#undef USHORTTOFLOAT
}

MINLINE void float_to_byte_dither_v4(uchar b[4], const float f[4], DitherContext *di, float s, float t)
{
	float dither_value = dither_random_value(s, t) * 0.005f * di->dither;

	b[0] = ftochar(dither_value + f[0]);
	b[1] = ftochar(dither_value + f[1]);
	b[2] = ftochar(dither_value + f[2]);
	b[3] = FTOCHAR(f[3]);
}

/* float to byte pixels, output 4-channel RGBA */
void IMB_buffer_byte_from_float(uchar *rect_to, const float *rect_from,
                                int channels_from, float dither, int profile_to, int profile_from, bool predivide,
                                int width, int height, int stride_to, int stride_from)
{
	float tmp[4];
	int x, y;
	DitherContext *di = NULL;
	float inv_width = 1.0f / width,
	      inv_height = 1.0f / height;

	/* we need valid profiles */
	BLI_assert(profile_to != IB_PROFILE_NONE);
	BLI_assert(profile_from != IB_PROFILE_NONE);

	if (dither)
		di = create_dither_context(dither);

	for (y = 0; y < height; y++) {
		float t = y * inv_height;

		if (channels_from == 1) {
			/* single channel input */
			const float *from = rect_from + stride_from * y;
			uchar *to = rect_to + stride_to * y * 4;

			for (x = 0; x < width; x++, from++, to += 4)
				to[0] = to[1] = to[2] = to[3] = FTOCHAR(from[0]);
		}
		else if (channels_from == 3) {
			/* RGB input */
			const float *from = rect_from + stride_from * y * 3;
			uchar *to = rect_to + stride_to * y * 4;

			if (profile_to == profile_from) {
				/* no color space conversion */
				for (x = 0; x < width; x++, from += 3, to += 4) {
					rgb_float_to_uchar(to, from);
					to[3] = 255;
				}
			}
			else if (profile_to == IB_PROFILE_SRGB) {
				/* convert from linear to sRGB */
				for (x = 0; x < width; x++, from += 3, to += 4) {
					linearrgb_to_srgb_v3_v3(tmp, from);
					rgb_float_to_uchar(to, tmp);
					to[3] = 255;
				}
			}
			else if (profile_to == IB_PROFILE_LINEAR_RGB) {
				/* convert from sRGB to linear */
				for (x = 0; x < width; x++, from += 3, to += 4) {
					srgb_to_linearrgb_v3_v3(tmp, from);
					rgb_float_to_uchar(to, tmp);
					to[3] = 255;
				}
			}
		}
		else if (channels_from == 4) {
			/* RGBA input */
			const float *from = rect_from + stride_from * y * 4;
			uchar *to = rect_to + stride_to * y * 4;

			if (profile_to == profile_from) {
				float straight[4];

				/* no color space conversion */
				if (dither && predivide) {
					for (x = 0; x < width; x++, from += 4, to += 4) {
						premul_to_straight_v4_v4(straight, from);
						float_to_byte_dither_v4(to, straight, di, (float) x * inv_width, t);
					}
				}
				else if (dither) {
					for (x = 0; x < width; x++, from += 4, to += 4)
						float_to_byte_dither_v4(to, from, di, (float) x * inv_width, t);
				}
				else if (predivide) {
					for (x = 0; x < width; x++, from += 4, to += 4) {
						premul_to_straight_v4_v4(straight, from);
						rgba_float_to_uchar(to, straight);
					}
				}
				else {
					for (x = 0; x < width; x++, from += 4, to += 4)
						rgba_float_to_uchar(to, from);
				}
			}
			else if (profile_to == IB_PROFILE_SRGB) {
				/* convert from linear to sRGB */
				unsigned short us[4];
				float straight[4];

				if (dither && predivide) {
					for (x = 0; x < width; x++, from += 4, to += 4) {
						premul_to_straight_v4_v4(straight, from);
						linearrgb_to_srgb_ushort4(us, from);
						ushort_to_byte_dither_v4(to, us, di, (float) x * inv_width, t);
					}
				}
				else if (dither) {
					for (x = 0; x < width; x++, from += 4, to += 4) {
						linearrgb_to_srgb_ushort4(us, from);
						ushort_to_byte_dither_v4(to, us, di, (float) x * inv_width, t);
					}
				}
				else if (predivide) {
					for (x = 0; x < width; x++, from += 4, to += 4) {
						premul_to_straight_v4_v4(straight, from);
						linearrgb_to_srgb_ushort4(us, from);
						ushort_to_byte_v4(to, us);
					}
				}
				else {
					for (x = 0; x < width; x++, from += 4, to += 4) {
						linearrgb_to_srgb_ushort4(us, from);
						ushort_to_byte_v4(to, us);
					}
				}
			}
			else if (profile_to == IB_PROFILE_LINEAR_RGB) {
				/* convert from sRGB to linear */
				if (dither && predivide) {
					for (x = 0; x < width; x++, from += 4, to += 4) {
						srgb_to_linearrgb_predivide_v4(tmp, from);
						float_to_byte_dither_v4(to, tmp, di, (float) x * inv_width, t);
					}
				}
				else if (dither) {
					for (x = 0; x < width; x++, from += 4, to += 4) {
						srgb_to_linearrgb_v4(tmp, from);
						float_to_byte_dither_v4(to, tmp, di, (float) x * inv_width, t);
					}
				}
				else if (predivide) {
					for (x = 0; x < width; x++, from += 4, to += 4) {
						srgb_to_linearrgb_predivide_v4(tmp, from);
						rgba_float_to_uchar(to, tmp);
					}
				}
				else {
					for (x = 0; x < width; x++, from += 4, to += 4) {
						srgb_to_linearrgb_v4(tmp, from);
						rgba_float_to_uchar(to, tmp);
					}
				}
			}
		}
	}

	if (dither)
		clear_dither_context(di);
}


/* float to byte pixels, output 4-channel RGBA */
void IMB_buffer_byte_from_float_mask(uchar *rect_to, const float *rect_from,
                                int channels_from, float dither, bool predivide,
                                int width, int height, int stride_to, int stride_from, char *mask)
{
	int x, y;
	DitherContext *di = NULL;
	float inv_width = 1.0f / width,
	inv_height = 1.0f / height;

	if (dither)
		di = create_dither_context(dither);

	for (y = 0; y < height; y++) {
		float t = y * inv_height;

		if (channels_from == 1) {
			/* single channel input */
			const float *from = rect_from + stride_from * y;
			uchar *to = rect_to + stride_to * y * 4;

			for (x = 0; x < width; x++, from++, to += 4)
				if (*mask++ == FILTER_MASK_USED)
					to[0] = to[1] = to[2] = to[3] = FTOCHAR(from[0]);
		}
		else if (channels_from == 3) {
			/* RGB input */
			const float *from = rect_from + stride_from * y * 3;
			uchar *to = rect_to + stride_to * y * 4;

			for (x = 0; x < width; x++, from += 3, to += 4) {
				if (*mask++ == FILTER_MASK_USED) {
					rgb_float_to_uchar(to, from);
					to[3] = 255;
				}
			}
		}
		else if (channels_from == 4) {
			/* RGBA input */
			const float *from = rect_from + stride_from * y * 4;
			uchar *to = rect_to + stride_to * y * 4;

			float straight[4];

			if (dither && predivide) {
				for (x = 0; x < width; x++, from += 4, to += 4) {
					if (*mask++ == FILTER_MASK_USED) {
						premul_to_straight_v4_v4(straight, from);
						float_to_byte_dither_v4(to, straight, di, (float) x * inv_width, t);
					}
				}
			}
			else if (dither) {
				for (x = 0; x < width; x++, from += 4, to += 4)
					if (*mask++ == FILTER_MASK_USED)
						float_to_byte_dither_v4(to, from, di, (float) x * inv_width, t);
			}
			else if (predivide) {
				for (x = 0; x < width; x++, from += 4, to += 4) {
					if (*mask++ == FILTER_MASK_USED) {
						premul_to_straight_v4_v4(straight, from);
						rgba_float_to_uchar(to, straight);
					}
				}
			}
			else {
				for (x = 0; x < width; x++, from += 4, to += 4)
					if (*mask++ == FILTER_MASK_USED)
						rgba_float_to_uchar(to, from);
			}
		}
	}

	if (dither)
		clear_dither_context(di);
}

/* byte to float pixels, input and output 4-channel RGBA  */
void IMB_buffer_float_from_byte(float *rect_to, const uchar *rect_from,
                                int profile_to, int profile_from, bool predivide,
                                int width, int height, int stride_to, int stride_from)
{
	float tmp[4];
	int x, y;

	/* we need valid profiles */
	BLI_assert(profile_to != IB_PROFILE_NONE);
	BLI_assert(profile_from != IB_PROFILE_NONE);

	/* RGBA input */
	for (y = 0; y < height; y++) {
		const uchar *from = rect_from + stride_from * y * 4;
		float *to = rect_to + stride_to * y * 4;

		if (profile_to == profile_from) {
			/* no color space conversion */
			for (x = 0; x < width; x++, from += 4, to += 4)
				rgba_uchar_to_float(to, from);
		}
		else if (profile_to == IB_PROFILE_LINEAR_RGB) {
			/* convert sRGB to linear */
			if (predivide) {
				for (x = 0; x < width; x++, from += 4, to += 4) {
					srgb_to_linearrgb_uchar4_predivide(to, from);
				}
			}
			else {
				for (x = 0; x < width; x++, from += 4, to += 4) {
					srgb_to_linearrgb_uchar4(to, from);
				}
			}
		}
		else if (profile_to == IB_PROFILE_SRGB) {
			/* convert linear to sRGB */
			if (predivide) {
				for (x = 0; x < width; x++, from += 4, to += 4) {
					rgba_uchar_to_float(tmp, from);
					linearrgb_to_srgb_predivide_v4(to, tmp);
				}
			}
			else {
				for (x = 0; x < width; x++, from += 4, to += 4) {
					rgba_uchar_to_float(tmp, from);
					linearrgb_to_srgb_v4(to, tmp);
				}
			}
		}
	}
}

/* float to float pixels, output 4-channel RGBA */
void IMB_buffer_float_from_float(float *rect_to, const float *rect_from,
                                 int channels_from, int profile_to, int profile_from, bool predivide,
                                 int width, int height, int stride_to, int stride_from)
{
	int x, y;

	/* we need valid profiles */
	BLI_assert(profile_to != IB_PROFILE_NONE);
	BLI_assert(profile_from != IB_PROFILE_NONE);

	if (channels_from == 1) {
		/* single channel input */
		for (y = 0; y < height; y++) {
			const float *from = rect_from + stride_from * y;
			float *to = rect_to + stride_to * y * 4;

			for (x = 0; x < width; x++, from++, to += 4)
				to[0] = to[1] = to[2] = to[3] = from[0];
		}
	}
	else if (channels_from == 3) {
		/* RGB input */
		for (y = 0; y < height; y++) {
			const float *from = rect_from + stride_from * y * 3;
			float *to = rect_to + stride_to * y * 4;

			if (profile_to == profile_from) {
				/* no color space conversion */
				for (x = 0; x < width; x++, from += 3, to += 4) {
					copy_v3_v3(to, from);
					to[3] = 1.0f;
				}
			}
			else if (profile_to == IB_PROFILE_LINEAR_RGB) {
				/* convert from sRGB to linear */
				for (x = 0; x < width; x++, from += 3, to += 4) {
					srgb_to_linearrgb_v3_v3(to, from);
					to[3] = 1.0f;
				}
			}
			else if (profile_to == IB_PROFILE_SRGB) {
				/* convert from linear to sRGB */
				for (x = 0; x < width; x++, from += 3, to += 4) {
					linearrgb_to_srgb_v3_v3(to, from);
					to[3] = 1.0f;
				}
			}
		}
	}
	else if (channels_from == 4) {
		/* RGBA input */
		for (y = 0; y < height; y++) {
			const float *from = rect_from + stride_from * y * 4;
			float *to = rect_to + stride_to * y * 4;

			if (profile_to == profile_from) {
				/* same profile, copy */
				memcpy(to, from, sizeof(float) * 4 * width);
			}
			else if (profile_to == IB_PROFILE_LINEAR_RGB) {
				/* convert to sRGB to linear */
				if (predivide) {
					for (x = 0; x < width; x++, from += 4, to += 4)
						srgb_to_linearrgb_predivide_v4(to, from);
				}
				else {
					for (x = 0; x < width; x++, from += 4, to += 4)
						srgb_to_linearrgb_v4(to, from);
				}
			}
			else if (profile_to == IB_PROFILE_SRGB) {
				/* convert from linear to sRGB */
				if (predivide) {
					for (x = 0; x < width; x++, from += 4, to += 4)
						linearrgb_to_srgb_predivide_v4(to, from);
				}
				else {
					for (x = 0; x < width; x++, from += 4, to += 4)
						linearrgb_to_srgb_v4(to, from);
				}
			}
		}
	}
}

/* float to float pixels, output 4-channel RGBA */
void IMB_buffer_float_from_float_mask(float *rect_to, const float *rect_from, int channels_from,
                                      int width, int height, int stride_to, int stride_from, char *mask)
{
	int x, y;

	if (channels_from == 1) {
		/* single channel input */
		for (y = 0; y < height; y++) {
			const float *from = rect_from + stride_from * y;
			float *to = rect_to + stride_to * y * 4;

			for (x = 0; x < width; x++, from++, to += 4)
				if (*mask++ == FILTER_MASK_USED)
					to[0] = to[1] = to[2] = to[3] = from[0];
		}
	}
	else if (channels_from == 3) {
		/* RGB input */
		for (y = 0; y < height; y++) {
			const float *from = rect_from + stride_from * y * 3;
			float *to = rect_to + stride_to * y * 4;

			for (x = 0; x < width; x++, from += 3, to += 4) {
				if (*mask++ == FILTER_MASK_USED) {
					copy_v3_v3(to, from);
					to[3] = 1.0f;
				}
			}
		}
	}
	else if (channels_from == 4) {
		/* RGBA input */
		for (y = 0; y < height; y++) {
			const float *from = rect_from + stride_from * y * 4;
			float *to = rect_to + stride_to * y * 4;

			for (x = 0; x < width; x++, from += 4, to += 4)
				if (*mask++ == FILTER_MASK_USED)
					copy_v4_v4(to, from);
		}
	}
}

/* byte to byte pixels, input and output 4-channel RGBA */
void IMB_buffer_byte_from_byte(uchar *rect_to, const uchar *rect_from,
                               int profile_to, int profile_from, bool predivide,
                               int width, int height, int stride_to, int stride_from)
{
	float tmp[4];
	int x, y;

	/* we need valid profiles */
	BLI_assert(profile_to != IB_PROFILE_NONE);
	BLI_assert(profile_from != IB_PROFILE_NONE);

	/* always RGBA input */
	for (y = 0; y < height; y++) {
		const uchar *from = rect_from + stride_from * y * 4;
		uchar *to = rect_to + stride_to * y * 4;

		if (profile_to == profile_from) {
			/* same profile, copy */
			memcpy(to, from, sizeof(uchar) * 4 * width);
		}
		else if (profile_to == IB_PROFILE_LINEAR_RGB) {
			/* convert to sRGB to linear */
			if (predivide) {
				for (x = 0; x < width; x++, from += 4, to += 4) {
					rgba_uchar_to_float(tmp, from);
					srgb_to_linearrgb_predivide_v4(tmp, tmp);
					rgba_float_to_uchar(to, tmp);
				}
			}
			else {
				for (x = 0; x < width; x++, from += 4, to += 4) {
					rgba_uchar_to_float(tmp, from);
					srgb_to_linearrgb_v4(tmp, tmp);
					rgba_float_to_uchar(to, tmp);
				}
			}
		}
		else if (profile_to == IB_PROFILE_SRGB) {
			/* convert from linear to sRGB */
			if (predivide) {
				for (x = 0; x < width; x++, from += 4, to += 4) {
					rgba_uchar_to_float(tmp, from);
					linearrgb_to_srgb_predivide_v4(tmp, tmp);
					rgba_float_to_uchar(to, tmp);
				}
			}
			else {
				for (x = 0; x < width; x++, from += 4, to += 4) {
					rgba_uchar_to_float(tmp, from);
					linearrgb_to_srgb_v4(tmp, tmp);
					rgba_float_to_uchar(to, tmp);
				}
			}
		}
	}
}

/****************************** ImBuf Conversion *****************************/

void IMB_rect_from_float(ImBuf *ibuf)
{
	float *buffer;
	const char *from_colorspace;

	/* verify we have a float buffer */
	if (ibuf->rect_float == NULL)
		return;

	/* create byte rect if it didn't exist yet */
	if (ibuf->rect == NULL) {
		if (imb_addrectImBuf(ibuf) == 0)
			return;
	}

	if (ibuf->float_colorspace == NULL)
		from_colorspace = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR);
	else
		from_colorspace = ibuf->float_colorspace->name;

	buffer = MEM_dupallocN(ibuf->rect_float);

	/* first make float buffer in byte space */
	IMB_colormanagement_transform(buffer, ibuf->x, ibuf->y, ibuf->channels, from_colorspace, ibuf->rect_colorspace->name, true);

	/* convert from float's premul alpha to byte's straight alpha */
	IMB_unpremultiply_rect_float(buffer, ibuf->channels, ibuf->x, ibuf->y);

	/* convert float to byte */
	IMB_buffer_byte_from_float((unsigned char *) ibuf->rect, buffer, ibuf->channels, ibuf->dither, IB_PROFILE_SRGB, IB_PROFILE_SRGB,
	                           false, ibuf->x, ibuf->y, ibuf->x, ibuf->x);

	MEM_freeN(buffer);

	/* ensure user flag is reset */
	ibuf->userflags &= ~IB_RECT_INVALID;
}

/* converts from linear float to sRGB byte for part of the texture, buffer will hold the changed part */
void IMB_partial_rect_from_float(ImBuf *ibuf, float *buffer, int x, int y, int w, int h, bool is_data)
{
	const float *rect_float;
	uchar *rect_byte;
	int profile_from = IB_PROFILE_LINEAR_RGB;

	/* verify we have a float buffer */
	if (ibuf->rect_float == NULL || buffer == NULL)
		return;

	/* create byte rect if it didn't exist yet */
	if (ibuf->rect == NULL)
		imb_addrectImBuf(ibuf);

	/* do conversion */
	rect_float = ibuf->rect_float + (x + y * ibuf->x) * ibuf->channels;
	rect_byte = (uchar *)ibuf->rect + (x + y * ibuf->x) * 4;

	if (is_data) {
		/* exception for non-color data, just copy float */
		IMB_buffer_float_from_float(buffer, rect_float,
		                            ibuf->channels, IB_PROFILE_LINEAR_RGB, IB_PROFILE_LINEAR_RGB, 0,
		                            w, h, w, ibuf->x);

		/* and do color space conversion to byte */
		IMB_buffer_byte_from_float(rect_byte, rect_float,
		                           4, ibuf->dither, IB_PROFILE_SRGB, profile_from, true,
		                           w, h, ibuf->x, w);
	}
	else {
		IMB_buffer_float_from_float(buffer, rect_float,
		                            ibuf->channels, IB_PROFILE_SRGB, profile_from, true,
		                            w, h, w, ibuf->x);

		IMB_buffer_float_unpremultiply(buffer, w, h);
		/* XXX: need to convert to image buffer's rect space */
		IMB_buffer_byte_from_float(rect_byte, buffer,
		                           4, ibuf->dither, IB_PROFILE_SRGB, IB_PROFILE_SRGB, 0,
		                           w, h, ibuf->x, w);
	}

	/* ensure user flag is reset */
	ibuf->userflags &= ~IB_RECT_INVALID;
}

void IMB_float_from_rect(ImBuf *ibuf)
{
	float *rect_float;

	/* verify if we byte and float buffers */
	if (ibuf->rect == NULL)
		return;

	/* allocate float buffer outside of image buffer,
	 * so work-in-progress color space conversion doesn't
	 * interfere with other parts of blender
	 */
	rect_float = ibuf->rect_float;
	if (rect_float == NULL) {
		int size;

		size = ibuf->x * ibuf->y;
		size = size * 4 * sizeof(float);
		ibuf->channels = 4;

		rect_float = MEM_mapallocN(size, "IMB_float_from_rect");

		if (rect_float == NULL)
			return;
	}

	/* first, create float buffer in non-linear space */
	IMB_buffer_float_from_byte(rect_float, (unsigned char *) ibuf->rect, IB_PROFILE_SRGB, IB_PROFILE_SRGB,
	                           false, ibuf->x, ibuf->y, ibuf->x, ibuf->x);

	/* then make float be in linear space */
	IMB_colormanagement_colorspace_to_scene_linear(rect_float, ibuf->x, ibuf->y, ibuf->channels,
	                                               ibuf->rect_colorspace, false);

	/* byte buffer is straight alpha, float should always be premul */
	IMB_premultiply_rect_float(rect_float, ibuf->channels, ibuf->x, ibuf->y);

	if (ibuf->rect_float == NULL) {
		ibuf->rect_float = rect_float;
		ibuf->mall |= IB_rectfloat;
		ibuf->flags |= IB_rectfloat;
	}
}

/**************************** Color to Grayscale *****************************/

/* no profile conversion */
void IMB_color_to_bw(ImBuf *ibuf)
{
	float *rct_fl = ibuf->rect_float;
	uchar *rct = (uchar *)ibuf->rect;
	int i;

	if (rct_fl) {
		for (i = ibuf->x * ibuf->y; i > 0; i--, rct_fl += 4)
			rct_fl[0] = rct_fl[1] = rct_fl[2] = rgb_to_grayscale(rct_fl);
	}

	if (rct) {
		for (i = ibuf->x * ibuf->y; i > 0; i--, rct += 4)
			rct[0] = rct[1] = rct[2] = rgb_to_grayscale_byte(rct);
	}
}

void IMB_buffer_float_clamp(float *buf, int width, int height)
{
	int i, total = width * height * 4;
	for (i = 0; i < total; i++) {
		buf[i] = min_ff(1.0, buf[i]);
	}
}

void IMB_buffer_float_unpremultiply(float *buf, int width, int height)
{
	int total = width * height;
	float *fp = buf;
	while (total--) {
		premul_to_straight_v4(fp);
		fp += 4;
	}
}

void IMB_buffer_float_premultiply(float *buf, int width, int height)
{
	int total = width * height;
	float *fp = buf;
	while (total--) {
		straight_to_premul_v4(fp);
		fp += 4;
	}
}

/**************************** alter saturation *****************************/

void IMB_saturation(ImBuf *ibuf, float sat)
{
	int i;
	unsigned char *rct = (unsigned char *)ibuf->rect;
	float *rct_fl = ibuf->rect_float;
	float hsv[3];

	if (rct) {
		float rgb[3];
		for (i = ibuf->x * ibuf->y; i > 0; i--, rct += 4) {
			rgb_uchar_to_float(rgb, rct);
			rgb_to_hsv_v(rgb, hsv);
			hsv_to_rgb(hsv[0], hsv[1] * sat, hsv[2], rgb, rgb + 1, rgb + 2);
			rgb_float_to_uchar(rct, rgb);
		}
	}

	if (rct_fl) {
		for (i = ibuf->x * ibuf->y; i > 0; i--, rct_fl += 4) {
			rgb_to_hsv_v(rct_fl, hsv);
			hsv_to_rgb(hsv[0], hsv[1] * sat, hsv[2], rct_fl, rct_fl + 1, rct_fl + 2);
		}
	}
}
