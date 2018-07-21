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
 * The Original Code is Copyright (C) 2015 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/stereoimbuf.c
 *  \ingroup imbuf
 */

#include <stddef.h>

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "IMB_allocimbuf.h"
#include "IMB_filetype.h"
#include "IMB_metadata.h"
#include "IMB_colormanagement_intern.h"

#include "imbuf.h"

#include "MEM_guardedalloc.h"

#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLI_math.h"

#include "DNA_userdef_types.h"
#include "DNA_scene_types.h"

/* prototypes */
struct Stereo3DData;
static void imb_stereo3d_write_doit(struct Stereo3DData *s3d_data, struct Stereo3dFormat *s3d);
static void imb_stereo3d_read_doit(struct Stereo3DData *s3d_data, struct Stereo3dFormat *s3d);

typedef struct Stereo3DData {
	struct { float *left, *right, *stereo; } rectf;
	struct { uchar *left, *right, *stereo; } rect;
	size_t x, y, channels;
	bool is_float;
} Stereo3DData;

static void imb_stereo3d_write_anaglyph(Stereo3DData *s3d, enum eStereo3dAnaglyphType mode)
{
	int x, y;
	size_t width = s3d->x;
	size_t height = s3d->y;
	const size_t channels = s3d->channels;

	const int stride_from = width;
	const int stride_to = width;

	int anaglyph_encoding[3][3] = {
	    {0, 1, 1},
	    {1, 0, 1},
	    {0, 0, 1},
	};

	int r, g, b;

	r = anaglyph_encoding[mode][0];
	g = anaglyph_encoding[mode][1];
	b = anaglyph_encoding[mode][2];

	if (s3d->is_float) {
		float *rect_left  = s3d->rectf.left;
		float *rect_right = s3d->rectf.right;
		float *rect_to    = s3d->rectf.stereo;

		if (channels == 3) {
			for (y = 0; y < height; y++) {
				float *to = rect_to + stride_to * y * 3;
				float *from[2] = {
				        rect_left  + stride_from * y * 3,
				        rect_right + stride_from * y * 3,
				};

				for (x = 0; x < width; x++, from[0] += 3, from[1] += 3, to += 3) {
					to[0] = from[r][0];
					to[1] = from[g][1];
					to[2] = from[b][2];
				}
			}
		}
		else if (channels == 4) {
			for (y = 0; y < height; y++) {
				float *to = rect_to + stride_to * y * 4;
				float *from[2] = {
				        rect_left  + stride_from * y * 4,
				        rect_right + stride_from * y * 4,
				};

				for (x = 0; x < width; x++, from[0] += 4, from[1] += 4, to += 4) {
					to[0] = from[r][0];
					to[1] = from[g][1];
					to[2] = from[b][2];
					to[3] = MAX2(from[0][3], from[1][3]);
				}
			}
		}
	}
	else {
		uchar *rect_left  = s3d->rect.left;
		uchar *rect_right = s3d->rect.right;
		uchar *rect_to    = s3d->rect.stereo;

		if (channels == 3) {
			for (y = 0; y < height; y++) {
				uchar *to = rect_to + stride_to * y * 3;
				uchar *from[2] = {
				        rect_left  + stride_from * y * 3,
				        rect_right + stride_from * y * 3,
				};

				for (x = 0; x < width; x++, from[0] += 3, from[1] += 3, to += 3) {
					to[0] = from[r][0];
					to[1] = from[g][1];
					to[2] = from[b][2];
				}
			}
		}
		else if (channels == 4) {
			for (y = 0; y < height; y++) {
				uchar *to = rect_to + stride_to * y * 4;
				uchar *from[2] = {
				        rect_left  + stride_from * y * 4,
				        rect_right + stride_from * y * 4,
				};

				for (x = 0; x < width; x++, from[0] += 4, from[1] += 4, to += 4) {
					to[0] = from[r][0];
					to[1] = from[g][1];
					to[2] = from[b][2];
					to[3] = MAX2(from[0][3], from[1][3]);
				}
			}
		}
	}
}

static void imb_stereo3d_write_interlace(Stereo3DData *s3d, enum eStereo3dInterlaceType mode, const bool swap)
{
	int x, y;
	size_t width = s3d->x;
	size_t height = s3d->y;
	const size_t channels = s3d->channels;

	const int stride_from = width;
	const int stride_to = width;

	if (s3d->is_float) {
		const float *rect_left  = s3d->rectf.left;
		const float *rect_right = s3d->rectf.right;
		float       *rect_to    = s3d->rectf.stereo;

		switch (mode) {
			case S3D_INTERLACE_ROW:
			{
				char i = (char) swap;
				for (y = 0; y < height; y++) {
					float *to = rect_to + stride_to * y * channels;
					const float *from[2] = {
					      rect_left  + stride_from * y * channels,
					      rect_right + stride_from * y * channels,
					};
					memcpy(to, from[i], sizeof(float) * channels * stride_from);
					i = !i;
				}
				break;
			}
			case S3D_INTERLACE_COLUMN:
			{
				if (channels == 1) {
					for (y = 0; y < height; y++) {
						float *to = rect_to + stride_to * y;
						const float *from[2] = {
						      rect_left  + stride_from * y,
						      rect_right + stride_from * y,
						};

						char i = (char) swap;
						for (x = 0; x < width; x++, from[0] += 1, from[1] += 1, to += 1) {
							to[0] = from[i][0];
							i = !i;
						}
					}
				}
				else if (channels == 3) {
					for (y = 0; y < height; y++) {
						float *to = rect_to + stride_to * y * 3;
						const float *from[2] = {
						      rect_left  + stride_from * y * 3,
						      rect_right + stride_from * y * 3,
						};

						char i = (char) swap;
						for (x = 0; x < width; x++, from[0] += 3, from[1] += 3, to += 3) {
							copy_v3_v3(to, from[i]);
							i = !i;
						}
					}
				}
				else if (channels == 4) {
					for (y = 0; y < height; y++) {
						float *to = rect_to + stride_to * y * channels;
						const float *from[2] = {
						      rect_left  + stride_from * y * channels,
						      rect_right + stride_from * y * channels,
						};

						char i = (char) swap;
						for (x = 0; x < width; x++, from[0] += 4, from[1] += 4, to += 4) {
							copy_v4_v4(to, from[i]);
							i = !i;
						}
					}
				}
				break;
			}
			case S3D_INTERLACE_CHECKERBOARD:
			{
				if (channels == 1) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						float *to = rect_to + stride_to * y;
						const float *from[2] = {
						        rect_left  + stride_from * y,
						        rect_right + stride_from * y,
						};
						char j = i;
						for (x = 0; x < width; x++, from[0] += 1, from[1] += 1, to += 1) {
							to[0] = from[j][0];
							j = !j;
						}
						i = !i;
					}
				}
				else if (channels == 3) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						float *to = rect_to + stride_to * y * 3;
						const float *from[2] = {
						        rect_left  + stride_from * y * 3,
						        rect_right + stride_from * y * 3,
						};
						char j = i;
						for (x = 0; x < width; x++, from[0] += 3, from[1] += 3, to += 3) {
							copy_v3_v3(to, from[j]);
							j = !j;
						}
						i = !i;
					}
				}
				else if (channels == 4) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						float *to = rect_to + stride_to * y * 4;
						const float *from[2] = {
						        rect_left  + stride_from * y * 4,
						        rect_right + stride_from * y * 4,
						};
						char j = i;
						for (x = 0; x < width; x++, from[0] += 4, from[1] += 4, to += 4) {
							copy_v4_v4(to, from[j]);
							j = !j;
						}
						i = !i;
					}
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}
	else {
		const uchar *rect_left  = s3d->rect.left;
		const uchar *rect_right = s3d->rect.right;
		uchar       *rect_to    = s3d->rect.stereo;

		switch (mode) {
			case S3D_INTERLACE_ROW:
			{
				char i = (char) swap;
				for (y = 0; y < height; y++) {
					uchar *to = rect_to + stride_to * y * channels;
					const uchar *from[2] = {
					      rect_left  + stride_from * y * channels,
					      rect_right + stride_from * y * channels,
					};
					memcpy(to, from[i], sizeof(uchar) * channels * stride_from);
					i = !i;
				}
				break;
			}
			case S3D_INTERLACE_COLUMN:
			{
				if (channels == 1) {
					for (y = 0; y < height; y++) {
						uchar *to = rect_to + stride_to * y;
						const uchar *from[2] = {
						        rect_left  + stride_from * y,
						        rect_right + stride_from * y,
						};
						char i = (char) swap;
						for (x = 0; x < width; x++, from[0] += 1, from[1] += 1, to += 1) {
							to[0] = from[i][0];
							i = !i;
						}
					}
				}
				else if (channels == 3) {
					for (y = 0; y < height; y++) {
						uchar *to = rect_to + stride_to * y * 3;
						const uchar *from[2] = {
						        rect_left  + stride_from * y * 3,
						        rect_right + stride_from * y * 3,
						};
						char i = (char) swap;
						for (x = 0; x < width; x++, from[0] += 3, from[1] += 3, to += 3) {
							copy_v3_v3_uchar(to, from[i]);
							i = !i;
						}
					}
				}
				else if (channels == 4) {
					for (y = 0; y < height; y++) {
						uchar *to = rect_to + stride_to * y * 4;
						const uchar *from[2] = {
						        rect_left  + stride_from * y * 4,
						        rect_right + stride_from * y * 4,
						};
						char i = (char) swap;
						for (x = 0; x < width; x++, from[0] += 4, from[1] += 4, to += 4) {
							copy_v4_v4_uchar(to, from[i]);
							i = !i;
						}
					}
				}
				break;
			}
			case S3D_INTERLACE_CHECKERBOARD:
			{
				if (channels == 1) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						uchar *to = rect_to + stride_to * y;
						const uchar *from[2] = {
						        rect_left  + stride_from * y,
						        rect_right + stride_from * y,
						};
						char j = i;
						for (x = 0; x < width; x++, from[0] += 1, from[1] += 1, to += 1) {
							to[0] = from[j][0];
							j = !j;
						}
						i = !i;
					}
				}
				else if (channels == 3) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						uchar *to = rect_to + stride_to * y * 3;
						const uchar *from[2] = {
						        rect_left  + stride_from * y * 3,
						        rect_right + stride_from * y * 3,
						};
						char j = i;
						for (x = 0; x < width; x++, from[0] += 3, from[1] += 3, to += 3) {
							copy_v3_v3_uchar(to, from[j]);
							j = !j;
						}
						i = !i;
					}
				}
				else if (channels == 4) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						uchar *to = rect_to + stride_to * y * 4;
						const uchar *from[2] = {
						        rect_left  + stride_from * y * 4,
						        rect_right + stride_from * y * 4,
						};
						char j = i;
						for (x = 0; x < width; x++, from[0] += 4, from[1] += 4, to += 4) {
							copy_v4_v4_uchar(to, from[j]);
							j = !j;
						}
						i = !i;
					}
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

/* stereo3d output (s3d->rectf.stereo) is always unsqueezed */
static void imb_stereo3d_write_sidebyside(Stereo3DData *s3d, const bool crosseyed)
{
	int y;
	size_t width = s3d->x;
	size_t height = s3d->y;
	const size_t channels = s3d->channels;

	const int stride_from = width;
	const int stride_to = width * 2;

	const int l =  (int) crosseyed;
	const int r = !l;

	if (s3d->is_float) {
		const float *rect_left  = s3d->rectf.left;
		const float *rect_right = s3d->rectf.right;
		float       *rect_to    = s3d->rectf.stereo;

		for (y = 0; y < height; y++) {
			float *to = rect_to + stride_to * y * channels;
			const float *from[2] = {
			    rect_left  + stride_from * y * channels,
			    rect_right + stride_from * y * channels,
			};

			memcpy(to, from[l], sizeof(float) * channels * stride_from);
			memcpy(to + channels * stride_from, from[r], sizeof(float) * channels * stride_from);
		}
	}
	else {
		const uchar *rect_left  = s3d->rect.left;
		const uchar *rect_right = s3d->rect.right;
		uchar       *rect_to    = s3d->rect.stereo;

		for (y = 0; y < height; y++) {
			uchar *to = rect_to + stride_to * y * channels;
			const uchar *from[2] = {
			    rect_left  + stride_from * y * channels,
			    rect_right + stride_from * y * channels,
			};

			memcpy(to, from[l], sizeof(uchar) * channels * stride_from);
			memcpy(to + channels * stride_from, from[r], sizeof(uchar) * channels * stride_from);
		}
	}
}

/* stereo3d output (s3d->rectf.stereo) is always unsqueezed */
static void imb_stereo3d_write_topbottom(Stereo3DData *s3d)
{
	int y;
	size_t width = s3d->x;
	size_t height = s3d->y;
	const size_t channels = s3d->channels;

	const int stride_from = width;
	const int stride_to = width;

	if (s3d->is_float) {
		const float *rect_left  = s3d->rectf.left;
		const float *rect_right = s3d->rectf.right;
		float       *rect_to    = s3d->rectf.stereo;

		for (y = 0; y < height; y++) {
			float *to = rect_to + stride_to * y * channels;
			const float *from[2] = {
			    rect_left  + stride_from * y * channels,
			    rect_right + stride_from * y * channels,
			};

			memcpy(to, from[1], sizeof(float) * channels * stride_from);
			memcpy(to + channels * height * stride_from, from[0], sizeof(float) * channels * stride_from);
		}
	}
	else {
		const uchar *rect_left  = s3d->rect.left;
		const uchar *rect_right = s3d->rect.right;
		uchar       *rect_to    = s3d->rect.stereo;

		for (y = 0; y < height; y++) {
			uchar *to = rect_to + stride_to * y * channels;
			const uchar *from[2] = {
			    rect_left  + stride_from * y * channels,
			    rect_right + stride_from * y * channels,
			};

			memcpy(to, from[1], sizeof(uchar) * channels * stride_from);
			memcpy(to + channels * height * stride_from, from[0], sizeof(uchar) * channels * stride_from);
		}
	}
}

/**************************** dimension utils ****************************************/

void IMB_stereo3d_write_dimensions(
        const char mode, const bool is_squeezed, const size_t width, const size_t height,
        size_t *r_width, size_t *r_height)
{
	switch (mode) {
		case S3D_DISPLAY_SIDEBYSIDE:
		{
			*r_width = is_squeezed ? width : width * 2;
			*r_height = height;
			break;
		}
		case S3D_DISPLAY_TOPBOTTOM:
		{
			*r_width = width;
			*r_height = is_squeezed ? height : height * 2;
			break;
		}
		case S3D_DISPLAY_ANAGLYPH:
		case S3D_DISPLAY_INTERLACE:
		default:
		{
			*r_width = width;
			*r_height = height;
			break;
		}
	}
}

void IMB_stereo3d_read_dimensions(
        const char mode, const bool is_squeezed, const size_t width, const size_t height,
        size_t *r_width, size_t *r_height)
{
	switch (mode) {
		case S3D_DISPLAY_SIDEBYSIDE:
		{
			*r_width = is_squeezed ? width / 2 : width;
			*r_height = height;
			break;
		}
		case S3D_DISPLAY_TOPBOTTOM:
		{
			*r_width = width;
			*r_height = is_squeezed ? height / 2 : height;
			break;
		}
		case S3D_DISPLAY_ANAGLYPH:
		case S3D_DISPLAY_INTERLACE:
		default:
		{
			*r_width = width;
			*r_height = height;
			break;
		}
	}
}

/**************************** un/squeeze frame ****************************************/

static void imb_stereo3d_squeeze_ImBuf(ImBuf *ibuf, Stereo3dFormat *s3d, const size_t x, const size_t y)
{
	if (ELEM(s3d->display_mode, S3D_DISPLAY_SIDEBYSIDE, S3D_DISPLAY_TOPBOTTOM) == false)
		return;

	if ((s3d->flag & S3D_SQUEEZED_FRAME) == 0)
		return;

	IMB_scaleImBuf_threaded(ibuf, x, y);
}

static void imb_stereo3d_unsqueeze_ImBuf(ImBuf *ibuf, Stereo3dFormat *s3d, const size_t x, const size_t y)
{
	if (ELEM(s3d->display_mode, S3D_DISPLAY_SIDEBYSIDE, S3D_DISPLAY_TOPBOTTOM) == false)
		return;

	if ((s3d->flag & S3D_SQUEEZED_FRAME) == 0)
		return;

	IMB_scaleImBuf_threaded(ibuf, x, y);
}

static void imb_stereo3d_squeeze_rectf(float *rectf, Stereo3dFormat *s3d, const size_t x, const size_t y, const size_t channels)
{
	ImBuf *ibuf;
	size_t width, height;

	if (ELEM(s3d->display_mode, S3D_DISPLAY_SIDEBYSIDE, S3D_DISPLAY_TOPBOTTOM) == false)
		return;

	if ((s3d->flag & S3D_SQUEEZED_FRAME) == 0)
		return;

	/* creates temporary imbuf to store the rectf */
	IMB_stereo3d_write_dimensions(s3d->display_mode, false, x, y, &width, &height);
	ibuf = IMB_allocImBuf(width, height, channels, IB_rectfloat);

	IMB_buffer_float_from_float(
	        ibuf->rect_float, rectf, channels,
	        IB_PROFILE_LINEAR_RGB, IB_PROFILE_LINEAR_RGB, false,
	        width, height, width, width);

	IMB_scaleImBuf_threaded(ibuf, x, y);
	memcpy(rectf, ibuf->rect_float, x * y * sizeof(float[4]));
	IMB_freeImBuf(ibuf);
}

static void imb_stereo3d_squeeze_rect(int *rect, Stereo3dFormat *s3d, const size_t x, const size_t y, const size_t channels)
{
	ImBuf *ibuf;
	size_t width, height;

	if (ELEM(s3d->display_mode, S3D_DISPLAY_SIDEBYSIDE, S3D_DISPLAY_TOPBOTTOM) == false)
		return;

	if ((s3d->flag & S3D_SQUEEZED_FRAME) == 0)
		return;

	/* creates temporary imbuf to store the rectf */
	IMB_stereo3d_write_dimensions(s3d->display_mode, false, x, y, &width, &height);
	ibuf = IMB_allocImBuf(width, height, channels, IB_rect);

	IMB_buffer_byte_from_byte(
	        (unsigned char *)ibuf->rect, (unsigned char *)rect,
	        IB_PROFILE_SRGB, IB_PROFILE_SRGB, false,
	        width, height, width, width);

	IMB_scaleImBuf_threaded(ibuf, x, y);
	memcpy(rect, ibuf->rect, x * y * sizeof(unsigned int));
	IMB_freeImBuf(ibuf);
}


/*************************** preparing to call the write functions **************************/

static void imb_stereo3d_data_initialize(
        Stereo3DData *s3d_data, const bool is_float,
        const size_t x, const size_t y, const size_t channels,
        int *rect_left, int *rect_right, int *rect_stereo,
        float *rectf_left, float *rectf_right, float *rectf_stereo)
{
	s3d_data->is_float = is_float;
	s3d_data->x = x;
	s3d_data->y = y;
	s3d_data->channels = channels;
	s3d_data->rect.left    = (uchar *)rect_left;
	s3d_data->rect.right   = (uchar *)rect_right;
	s3d_data->rect.stereo  = (uchar *)rect_stereo;
	s3d_data->rectf.left   = rectf_left;
	s3d_data->rectf.right  = rectf_right;
	s3d_data->rectf.stereo = rectf_stereo;
}

int *IMB_stereo3d_from_rect(
        ImageFormatData *im_format, const size_t x, const size_t y, const size_t channels,
        int *rect_left, int *rect_right)
{
	int *r_rect;
	Stereo3DData s3d_data = {{NULL}};
	size_t width, height;
	const bool is_float = im_format->depth > 8;

	IMB_stereo3d_write_dimensions(im_format->stereo3d_format.display_mode, false, x, y, &width, &height);
	r_rect = MEM_mallocN(channels * sizeof(int) * width * height, __func__);

	imb_stereo3d_data_initialize(&s3d_data, is_float, x, y, channels, rect_left, rect_right, r_rect, NULL, NULL, NULL);
	imb_stereo3d_write_doit(&s3d_data, &im_format->stereo3d_format);
	imb_stereo3d_squeeze_rect(r_rect, &im_format->stereo3d_format, x, y, channels);

	return r_rect;
}

float *IMB_stereo3d_from_rectf(
        ImageFormatData *im_format, const size_t x, const size_t y, const size_t channels,
        float *rectf_left, float *rectf_right)
{
	float *r_rectf;
	Stereo3DData s3d_data = {{NULL}};
	size_t width, height;
	const bool is_float = im_format->depth > 8;

	IMB_stereo3d_write_dimensions(im_format->stereo3d_format.display_mode, false, x, y, &width, &height);
	r_rectf = MEM_mallocN(channels * sizeof(float) * width * height, __func__);

	imb_stereo3d_data_initialize(&s3d_data, is_float, x, y, channels, NULL, NULL, NULL, rectf_left, rectf_right, r_rectf);
	imb_stereo3d_write_doit(&s3d_data, &im_format->stereo3d_format);
	imb_stereo3d_squeeze_rectf(r_rectf, &im_format->stereo3d_format, x, y, channels);

	return r_rectf;
}

/* left/right are always float */
ImBuf *IMB_stereo3d_ImBuf(ImageFormatData *im_format, ImBuf *ibuf_left, ImBuf *ibuf_right)
{
	ImBuf *ibuf_stereo = NULL;
	Stereo3DData s3d_data = {{NULL}};
	size_t width, height;
	const bool is_float = im_format->depth > 8;

	IMB_stereo3d_write_dimensions(im_format->stereo3d_format.display_mode, false, ibuf_left->x, ibuf_left->y, &width, &height);
	ibuf_stereo = IMB_allocImBuf(width, height, ibuf_left->planes, (is_float ? IB_rectfloat : IB_rect));

	ibuf_stereo->rect_colorspace = ibuf_left->rect_colorspace;
	ibuf_stereo->float_colorspace = ibuf_left->float_colorspace;

	ibuf_stereo->flags = ibuf_left->flags;

	imb_stereo3d_data_initialize(
	        &s3d_data, is_float, ibuf_left->x, ibuf_left->y, 4,
	        (int *)ibuf_left->rect, (int *)ibuf_right->rect, (int *)ibuf_stereo->rect,
	        ibuf_left->rect_float, ibuf_right->rect_float, ibuf_stereo->rect_float);

	imb_stereo3d_write_doit(&s3d_data, &im_format->stereo3d_format);
	imb_stereo3d_squeeze_ImBuf(ibuf_stereo, &im_format->stereo3d_format, ibuf_left->x, ibuf_left->y);

	return ibuf_stereo;
}

static void imb_stereo3d_write_doit(Stereo3DData *s3d_data, Stereo3dFormat *s3d)
{
	switch (s3d->display_mode) {
		case S3D_DISPLAY_ANAGLYPH:
			imb_stereo3d_write_anaglyph(s3d_data, s3d->anaglyph_type);
			break;
		case S3D_DISPLAY_INTERLACE:
			imb_stereo3d_write_interlace(s3d_data, s3d->interlace_type, (s3d->flag & S3D_INTERLACE_SWAP) != 0);
			break;
		case S3D_DISPLAY_SIDEBYSIDE:
			imb_stereo3d_write_sidebyside(s3d_data, (s3d->flag & S3D_SIDEBYSIDE_CROSSEYED) != 0);
			break;
		case S3D_DISPLAY_TOPBOTTOM:
			imb_stereo3d_write_topbottom(s3d_data);
			break;
		default:
			break;
	}
}

/******************************** reading stereo imbufs **********************/

static void imb_stereo3d_read_anaglyph(Stereo3DData *s3d, enum eStereo3dAnaglyphType mode)
{
	int x, y;
	size_t width = s3d->x;
	size_t height = s3d->y;
	const size_t channels = s3d->channels;

	const int stride_from = width;
	const int stride_to = width;

	int anaglyph_encoding[3][3] = {
	    {0, 1, 1},
	    {1, 0, 1},
	    {0, 0, 1},
	};

	int r, g, b;

	r = anaglyph_encoding[mode][0];
	g = anaglyph_encoding[mode][1];
	b = anaglyph_encoding[mode][2];

	if (s3d->is_float) {
		float *rect_left  = s3d->rectf.left;
		float *rect_right = s3d->rectf.right;
		float *rect_from  = s3d->rectf.stereo;

		if (channels == 3) {
			for (y = 0; y < height; y++) {
				float *from = rect_from + stride_from * y * 3;
				float *to[2] = {
					rect_left  + stride_to * y * 3,
					rect_right + stride_to * y * 3,
				};

				for (x = 0; x < width; x++, from += 3, to[0] += 3, to[1] += 3) {
					to[r][0] = from[0];
					to[g][1] = from[1];
					to[b][2] = from[2];
				}
			}
		}
		else if (channels == 4) {
			for (y = 0; y < height; y++) {
				float *from = rect_from + stride_from * y * 4;
				float *to[2] = {
					rect_left  + stride_to * y * 4,
					rect_right + stride_to * y * 4,
				};

				for (x = 0; x < width; x++, from += 4, to[0] += 4, to[1] += 4) {
					to[r][0] = from[0];
					to[g][1] = from[1];
					to[b][2] = from[2];
					to[0][3] = to[1][3] = from[3];
				}
			}
		}
	}
	else {
		uchar *rect_left  = s3d->rect.left;
		uchar *rect_right = s3d->rect.right;
		uchar *rect_from  = s3d->rect.stereo;

		if (channels == 3) {
			for (y = 0; y < height; y++) {
				uchar *from = rect_from + stride_from * y * 3;
				uchar *to[2] = {
					rect_left  + stride_to * y * 3,
					rect_right + stride_to * y * 3,
				};

				for (x = 0; x < width; x++, from += 3, to[0] += 3, to[1] += 3) {
					to[r][0] = from[0];
					to[g][1] = from[1];
					to[b][2] = from[2];
				}
			}
		}
		else if (channels == 4) {
			for (y = 0; y < height; y++) {
				uchar *from = rect_from + stride_from * y * 4;
				uchar *to[2] = {
					rect_left  + stride_to * y * 4,
					rect_right + stride_to * y * 4,
				};

				for (x = 0; x < width; x++, from += 4, to[0] += 4, to[1] += 4) {
					to[r][0] = from[0];
					to[g][1] = from[1];
					to[b][2] = from[2];
					to[0][3] = to[1][3] = from[3];
				}
			}
		}
	}
}

static void imb_stereo3d_read_interlace(Stereo3DData *s3d, enum eStereo3dInterlaceType mode, const bool swap)
{
	int x, y;
	size_t width = s3d->x;
	size_t height = s3d->y;
	const size_t channels = s3d->channels;

	const int stride_from = width;
	const int stride_to = width;

	if (s3d->is_float) {
		float *rect_left  = s3d->rectf.left;
		float *rect_right = s3d->rectf.right;
		const float *rect_from = s3d->rectf.stereo;

		switch (mode) {
			case S3D_INTERLACE_ROW:
			{
				char i = (char) swap;
				for (y = 0; y < height; y++) {
					const float *from = rect_from + stride_from * y * channels;
					float *to[2] = {
						rect_left  + stride_to * y * channels,
						rect_right + stride_to * y * channels,
					};
					memcpy(to[i], from, sizeof(float) * channels * stride_to);
					i = !i;
				}
				break;
			}
			case S3D_INTERLACE_COLUMN:
			{
				if (channels == 1) {
					for (y = 0; y < height; y++) {
						const float *from = rect_from + stride_from * y;
						float *to[2] = {
							rect_left  + stride_to * y,
							rect_right + stride_to * y,
						};

						char i = (char) swap;
						for (x = 0; x < width; x++, from += 1, to[0] += 1, to[1] += 1) {
							to[i][0] = from[0];
							i = !i;
						}
					}
				}
				else if (channels == 3) {
					for (y = 0; y < height; y++) {
						const float *from = rect_from + stride_from * y * 3;
						float *to[2] = {
							rect_left  + stride_to * y * 3,
							rect_right + stride_to * y * 3,
						};

						char i = (char) swap;
						for (x = 0; x < width; x++, from += 3, to[0] += 3, to[1] += 3) {
							copy_v3_v3(to[i], from);
							i = !i;
						}
					}
				}
				else if (channels == 4) {
					for (y = 0; y < height; y++) {
						const float *from = rect_from + stride_from * y * channels;
						float *to[2] = {
							rect_left  + stride_to * y * channels,
							rect_right + stride_to * y * channels,
						};

						char i = (char) swap;
						for (x = 0; x < width; x++, from += 4, to[0] += 4, to[1] += 4) {
							copy_v4_v4(to[i], from);
							i = !i;
						}
					}
				}
				break;
			}
			case S3D_INTERLACE_CHECKERBOARD:
			{
				if (channels == 1) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						const float *from = rect_from + stride_from * y;
						float *to[2] = {
							rect_left  + stride_to * y,
							rect_right + stride_to * y,
						};
						char j = i;
						for (x = 0; x < width; x++, from += 1, to[0] += 1, to[1] += 1) {
							to[j][0] = from[0];
							j = !j;
						}
						i = !i;
					}
				}
				else if (channels == 3) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						const float *from = rect_from + stride_from * y * 3;
						float *to[2] = {
							rect_left  + stride_to * y * 3,
							rect_right + stride_to * y * 3,
						};
						char j = i;
						for (x = 0; x < width; x++, from += 3, to[0] += 3, to[1] += 3) {
							copy_v3_v3(to[j], from);
							j = !j;
						}
						i = !i;
					}
				}
				else if (channels == 4) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						const float *from = rect_from + stride_from * y * 4;
						float *to[2] = {
							rect_left  + stride_to * y * 4,
							rect_right + stride_to * y * 4,
						};
						char j = i;
						for (x = 0; x < width; x++, from += 4, to[0] += 4, to[1] += 4) {
							copy_v4_v4(to[j], from);
							j = !j;
						}
						i = !i;
					}
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}
	else {
		uchar       *rect_left  = s3d->rect.right;
		uchar       *rect_right = s3d->rect.left;
		const uchar *rect_from  = s3d->rect.stereo;

		switch (mode) {
			case S3D_INTERLACE_ROW:
			{
				char i = (char) swap;
				for (y = 0; y < height; y++) {
					const uchar *from = rect_from + stride_from * y * channels;
					uchar *to[2] = {
						rect_left  + stride_to * y * channels,
						rect_right + stride_to * y * channels,
					};
					memcpy(to[i], from, sizeof(uchar) * channels * stride_to);
					i = !i;
				}
				break;
			}
			case S3D_INTERLACE_COLUMN:
			{
				if (channels == 1) {
					for (y = 0; y < height; y++) {
						const uchar *from = rect_from + stride_from * y;
						uchar *to[2] = {
							rect_left  + stride_to * y,
							rect_right + stride_to * y,
						};
						char i = (char) swap;
						for (x = 0; x < width; x++, from += 1, to[0] += 1, to[1] += 1) {
							to[i][0] = from[0];
							i = !i;
						}
					}
				}
				else if (channels == 3) {
					for (y = 0; y < height; y++) {
						const uchar *from = rect_from + stride_from * y * 3;
						uchar *to[2] = {
							rect_left  + stride_to * y * 3,
							rect_right + stride_to * y * 3,
						};
						char i = (char) swap;
						for (x = 0; x < width; x++, from += 3, to[0] += 3, to[1] += 3) {
							copy_v3_v3_uchar(to[i], from);
							i = !i;
						}
					}
				}
				else if (channels == 4) {
					for (y = 0; y < height; y++) {
						const uchar *from = rect_from + stride_from * y * 4;
						uchar *to[2] = {
							rect_left  + stride_to * y * 4,
							rect_right + stride_to * y * 4,
						};
						char i = (char) swap;
						for (x = 0; x < width; x++, from += 4, to[0] += 4, to[1] += 4) {
							copy_v4_v4_uchar(to[i], from);
							i = !i;
						}
					}
				}
				break;
			}
			case S3D_INTERLACE_CHECKERBOARD:
			{
				if (channels == 1) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						const uchar *from = rect_from + stride_from * y;
						uchar *to[2] = {
							rect_left  + stride_to * y,
							rect_right + stride_to * y,
						};
						char j = i;
						for (x = 0; x < width; x++, from += 1, to[0] += 1, to[1] += 1) {
							to[j][0] = from[0];
							j = !j;
						}
						i = !i;
					}
				}
				else if (channels == 3) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						const uchar *from = rect_from + stride_from * y * 3;
						uchar *to[2] = {
							rect_left  + stride_to * y * 3,
							rect_right + stride_to * y * 3,
						};
						char j = i;
						for (x = 0; x < width; x++, from += 3, to[0] += 3, to[1] += 3) {
							copy_v3_v3_uchar(to[j], from);
							j = !j;
						}
						i = !i;
					}
				}
				else if (channels == 4) {
					char i = (char) swap;
					for (y = 0; y < height; y++) {
						const uchar *from = rect_from + stride_from * y * 4;
						uchar *to[2] = {
							rect_left  + stride_to * y * 4,
							rect_right + stride_to * y * 4,
						};
						char j = i;
						for (x = 0; x < width; x++, from += 4, to[0] += 4, to[1] += 4) {
							copy_v4_v4_uchar(to[j], from);
							j = !j;
						}
						i = !i;
					}
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

/* stereo input (s3d->rectf.stereo) is always unsqueezed */
static void imb_stereo3d_read_sidebyside(Stereo3DData *s3d, const bool crosseyed)
{
	int y;
	size_t width = s3d->x;
	size_t height = s3d->y;
	const size_t channels = s3d->channels;

	const int stride_from = width * 2;
	const int stride_to = width;

	const int l =  (int) crosseyed;
	const int r = !l;

	if (s3d->is_float) {
		float *rect_left = s3d->rectf.left;
		float *rect_right = s3d->rectf.right;
		const float *rect_from = s3d->rectf.stereo;

		for (y = 0; y < height; y++) {
			const float *from = rect_from + stride_from * y * channels;
			float *to[2] = {
			    rect_left  + stride_to * y * channels,
			    rect_right + stride_to * y * channels,
			};

			memcpy(to[l], from, sizeof(float) * channels * stride_to);
			memcpy(to[r], from + channels * stride_to, sizeof(float) * channels * stride_to);
		}
	}
	else {
		uchar       *rect_left  = s3d->rect.left;
		uchar       *rect_right = s3d->rect.right;
		const uchar *rect_from  = s3d->rect.stereo;

		/* always RGBA input/output */
		for (y = 0; y < height; y++) {
			const uchar *from = rect_from + stride_from * y * channels;
			uchar *to[2] = {
			    rect_left  + stride_to * y * channels,
			    rect_right + stride_to * y * channels,
			};

			memcpy(to[l], from, sizeof(uchar) * channels * stride_to);
			memcpy(to[r], from + channels * stride_to, sizeof(uchar) * channels * stride_to);
		}
	}
}

/* stereo input (s3d->rectf.stereo) is always unsqueezed */
static void imb_stereo3d_read_topbottom(Stereo3DData *s3d)
{
	int y;
	size_t width = s3d->x;
	size_t height = s3d->y;
	const size_t channels = s3d->channels;

	const int stride_from = width;
	const int stride_to = width;

	if (s3d->is_float) {
		float *rect_left = s3d->rectf.left;
		float *rect_right = s3d->rectf.right;
		const float *rect_from = s3d->rectf.stereo;

		for (y = 0; y < height; y++) {
			const float *from = rect_from + stride_from * y * channels;
			float *to[2] = {
			    rect_left  + stride_to * y * channels,
			    rect_right + stride_to * y * channels,
			};

			memcpy(to[1], from, sizeof(float) * channels * stride_to);
			memcpy(to[0], from + channels * height * stride_to, sizeof(float) * channels * stride_to);
		}
	}
	else {
		uchar       *rect_left  = s3d->rect.left;
		uchar       *rect_right = s3d->rect.right;
		const uchar *rect_from  = s3d->rect.stereo;

		for (y = 0; y < height; y++) {
			const uchar *from = rect_from + stride_from * y * channels;
			uchar *to[2] = {
			    rect_left  + stride_to * y * channels,
			    rect_right + stride_to * y * channels,
			};

			memcpy(to[1], from, sizeof(uchar) * channels * stride_to);
			memcpy(to[0], from + channels * height * stride_to, sizeof(uchar) * channels * stride_to);
		}
	}
}


/*************************** preparing to call the read functions **************************/

/* reading a stereo encoded ibuf (*left) and generating two ibufs from it (*left and *right) */
void IMB_ImBufFromStereo3d(
        Stereo3dFormat *s3d, ImBuf *ibuf_stereo3d,
        ImBuf **r_ibuf_left, ImBuf **r_ibuf_right)
{
	Stereo3DData s3d_data = {{NULL}};
	ImBuf *ibuf_left, *ibuf_right;
	size_t width, height;
	const bool is_float = (ibuf_stereo3d->rect_float != NULL);

	IMB_stereo3d_read_dimensions(
	        s3d->display_mode, ((s3d->flag & S3D_SQUEEZED_FRAME) == 0), ibuf_stereo3d->x, ibuf_stereo3d->y,
	        &width, &height);

	ibuf_left = IMB_allocImBuf(width, height, ibuf_stereo3d->planes, (is_float ? IB_rectfloat : IB_rect));
	ibuf_right = IMB_allocImBuf(width, height, ibuf_stereo3d->planes, (is_float ? IB_rectfloat : IB_rect));

	ibuf_left->flags = ibuf_stereo3d->flags;
	ibuf_right->flags = ibuf_stereo3d->flags;

	/* we always work with unsqueezed formats */
	IMB_stereo3d_write_dimensions(
	        s3d->display_mode, ((s3d->flag & S3D_SQUEEZED_FRAME) == 0), ibuf_stereo3d->x, ibuf_stereo3d->y,
	        &width, &height);
	imb_stereo3d_unsqueeze_ImBuf(ibuf_stereo3d, s3d, width, height);

	imb_stereo3d_data_initialize(
	        &s3d_data, is_float, ibuf_left->x, ibuf_left->y, 4,
	        (int *)ibuf_left->rect, (int *)ibuf_right->rect, (int *)ibuf_stereo3d->rect,
	        ibuf_left->rect_float, ibuf_right->rect_float, ibuf_stereo3d->rect_float);

	imb_stereo3d_read_doit(&s3d_data, s3d);

	if (ibuf_stereo3d->flags & (IB_zbuf | IB_zbuffloat)) {
		if (is_float) {
			addzbuffloatImBuf(ibuf_left);
			addzbuffloatImBuf(ibuf_right);
		}
		else {
			addzbufImBuf(ibuf_left);
			addzbufImBuf(ibuf_right);
		}

		imb_stereo3d_data_initialize(
		        &s3d_data, is_float, ibuf_left->x, ibuf_left->y, 1,
		        (int *)ibuf_left->zbuf, (int *)ibuf_right->zbuf, (int *)ibuf_stereo3d->zbuf,
		        ibuf_left->zbuf_float, ibuf_right->zbuf_float, ibuf_stereo3d->zbuf_float);

		imb_stereo3d_read_doit(&s3d_data, s3d);
	}

	IMB_freeImBuf(ibuf_stereo3d);

	*r_ibuf_left = ibuf_left;
	*r_ibuf_right = ibuf_right;
}

static void imb_stereo3d_read_doit(Stereo3DData *s3d_data, Stereo3dFormat *s3d)
{
	switch (s3d->display_mode) {
		case S3D_DISPLAY_ANAGLYPH:
			imb_stereo3d_read_anaglyph(s3d_data, s3d->anaglyph_type);
			break;
		case S3D_DISPLAY_INTERLACE:
			imb_stereo3d_read_interlace(s3d_data, s3d->interlace_type, (s3d->flag & S3D_INTERLACE_SWAP) != 0);
			break;
		case S3D_DISPLAY_SIDEBYSIDE:
			imb_stereo3d_read_sidebyside(s3d_data, (s3d->flag & S3D_SIDEBYSIDE_CROSSEYED) != 0);
			break;
		case S3D_DISPLAY_TOPBOTTOM:
			imb_stereo3d_read_topbottom(s3d_data);
			break;
		default:
			break;
	}
}
