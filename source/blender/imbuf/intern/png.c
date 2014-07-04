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
 */

/** \file blender/imbuf/intern/png.c
 *  \ingroup imbuf
 *
 * \todo Save floats as 16 bits per channel, currently readonly.
 */

#include "png.h"

#include "BLI_utildefines.h"
#include "BLI_fileops.h"
#include "BLI_math.h"

#include "BKE_global.h"

#include "MEM_guardedalloc.h"

#include "imbuf.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"
#include "IMB_metadata.h"
#include "IMB_filetype.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

typedef struct PNGReadStruct {
	unsigned char *data;
	unsigned int size;
	unsigned int seek;
} PNGReadStruct;

static void ReadData(png_structp png_ptr, png_bytep data, png_size_t length);
static void WriteData(png_structp png_ptr, png_bytep data, png_size_t length);
static void Flush(png_structp png_ptr);

BLI_INLINE unsigned short UPSAMPLE_8_TO_16(const unsigned char _val)
{
	return (_val << 8) + _val;
}

int imb_is_a_png(unsigned char *mem)
{
	int ret_val = 0;

	if (mem) ret_val = !png_sig_cmp(mem, 0, 8);
	return(ret_val);
}

static void Flush(png_structp png_ptr) 
{
	(void)png_ptr;
}

static void WriteData(png_structp png_ptr, png_bytep data, png_size_t length)
{
	ImBuf *ibuf = (ImBuf *) png_get_io_ptr(png_ptr);

	/* if buffer is to small increase it. */
	while (ibuf->encodedsize + length > ibuf->encodedbuffersize) {
		imb_enlargeencodedbufferImBuf(ibuf);
	}

	memcpy(ibuf->encodedbuffer + ibuf->encodedsize, data, length);
	ibuf->encodedsize += length;
}

static void ReadData(png_structp png_ptr, png_bytep data, png_size_t length)
{
	PNGReadStruct *rs = (PNGReadStruct *) png_get_io_ptr(png_ptr);

	if (rs) {
		if (length <= rs->size - rs->seek) {
			memcpy(data, rs->data + rs->seek, length);
			rs->seek += length;
			return;
		}
	}

	printf("Reached EOF while decoding PNG\n");
	longjmp(png_jmpbuf(png_ptr), 1);
}

static float channel_colormanage_noop(float value)
{
	return value;
}

/* wrap to avoid macro calling functions multiple times */
BLI_INLINE unsigned short ftoshort(float val)
{
	return FTOUSHORT(val);
}

int imb_savepng(struct ImBuf *ibuf, const char *name, int flags)
{
	png_structp png_ptr;
	png_infop info_ptr;

	unsigned char *pixels = NULL;
	unsigned char *from, *to;
	unsigned short *pixels16 = NULL, *to16;
	float *from_float, from_straight[4];
	png_bytepp row_pointers = NULL;
	int i, bytesperpixel, color_type = PNG_COLOR_TYPE_GRAY;
	FILE *fp = NULL;

	bool is_16bit  = (ibuf->ftype & PNG_16BIT) != 0;
	bool has_float = (ibuf->rect_float != NULL);
	int channels_in_float = ibuf->channels ? ibuf->channels : 4;

	float (*chanel_colormanage_cb)(float);

	/* use the jpeg quality setting for compression */
	int compression;
	compression = (int)(((float)(ibuf->ftype & 0xff) / 11.1111f));
	compression = compression < 0 ? 0 : (compression > 9 ? 9 : compression);

	if (ibuf->float_colorspace) {
		/* float buffer was managed already, no need in color space conversion */
		chanel_colormanage_cb = channel_colormanage_noop;
	}
	else {
		/* standard linear-to-srgb conversion if float buffer wasn't managed */
		chanel_colormanage_cb = linearrgb_to_srgb;
	}

	/* for prints */
	if (flags & IB_mem)
		name = "<memory>";

	bytesperpixel = (ibuf->planes + 7) >> 3;
	if ((bytesperpixel > 4) || (bytesperpixel == 2)) {
		printf("imb_savepng: Unsupported bytes per pixel: %d for file: '%s'\n", bytesperpixel, name);
		return (0);
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
	                                  NULL, NULL, NULL);
	if (png_ptr == NULL) {
		printf("imb_savepng: Cannot png_create_write_struct for file: '%s'\n", name);
		return 0;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		printf("imb_savepng: Cannot png_create_info_struct for file: '%s'\n", name);
		return 0;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		printf("imb_savepng: Cannot setjmp for file: '%s'\n", name);
		return 0;
	}

	/* copy image data */

	if (is_16bit)
		pixels16 = MEM_mallocN(ibuf->x * ibuf->y * bytesperpixel * sizeof(unsigned short), "png 16bit pixels");
	else
		pixels = MEM_mallocN(ibuf->x * ibuf->y * bytesperpixel * sizeof(unsigned char), "png 8bit pixels");

	if (pixels == NULL && pixels16 == NULL) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		printf("imb_savepng: Cannot allocate pixels array of %dx%d, %d bytes per pixel for file: '%s'\n", ibuf->x, ibuf->y, bytesperpixel, name);
		return 0;
	}

	from = (unsigned char *) ibuf->rect;
	to = pixels;
	from_float = ibuf->rect_float;
	to16 = pixels16;

	switch (bytesperpixel) {
		case 4:
			color_type = PNG_COLOR_TYPE_RGBA;
			if (is_16bit) {
				if (has_float) {
					if (channels_in_float == 4) {
						for (i = ibuf->x * ibuf->y; i > 0; i--) {
							premul_to_straight_v4_v4(from_straight, from_float);
							to16[0] = ftoshort(chanel_colormanage_cb(from_straight[0]));
							to16[1] = ftoshort(chanel_colormanage_cb(from_straight[1]));
							to16[2] = ftoshort(chanel_colormanage_cb(from_straight[2]));
							to16[3] = ftoshort(chanel_colormanage_cb(from_straight[3]));
							to16 += 4; from_float += 4;
						}
					}
					else if (channels_in_float == 3) {
						for (i = ibuf->x * ibuf->y; i > 0; i--) {
							to16[0] = ftoshort(chanel_colormanage_cb(from_float[0]));
							to16[1] = ftoshort(chanel_colormanage_cb(from_float[1]));
							to16[2] = ftoshort(chanel_colormanage_cb(from_float[2]));
							to16[3] = 65535;
							to16 += 4; from_float += 3;
						}
					}
					else {
						for (i = ibuf->x * ibuf->y; i > 0; i--) {
							to16[0] = ftoshort(chanel_colormanage_cb(from_float[0]));
							to16[2] = to16[1] = to16[0];
							to16[3] = 65535;
							to16 += 4; from_float++;
						}
					}
				}
				else {
					for (i = ibuf->x * ibuf->y; i > 0; i--) {
						to16[0] = UPSAMPLE_8_TO_16(from[0]);
						to16[1] = UPSAMPLE_8_TO_16(from[1]);
						to16[2] = UPSAMPLE_8_TO_16(from[2]);
						to16[3] = UPSAMPLE_8_TO_16(from[3]);
						to16 += 4; from += 4;
					}
				}
			}
			else {
				for (i = ibuf->x * ibuf->y; i > 0; i--) {
					to[0] = from[0];
					to[1] = from[1];
					to[2] = from[2];
					to[3] = from[3];
					to += 4; from += 4;
				}
			}
			break;
		case 3:
			color_type = PNG_COLOR_TYPE_RGB;
			if (is_16bit) {
				if (has_float) {
					if (channels_in_float == 4) {
						for (i = ibuf->x * ibuf->y; i > 0; i--) {
							premul_to_straight_v4_v4(from_straight, from_float);
							to16[0] = ftoshort(chanel_colormanage_cb(from_straight[0]));
							to16[1] = ftoshort(chanel_colormanage_cb(from_straight[1]));
							to16[2] = ftoshort(chanel_colormanage_cb(from_straight[2]));
							to16 += 3; from_float += 4;
						}
					}
					else if (channels_in_float == 3) {
						for (i = ibuf->x * ibuf->y; i > 0; i--) {
							to16[0] = ftoshort(chanel_colormanage_cb(from_float[0]));
							to16[1] = ftoshort(chanel_colormanage_cb(from_float[1]));
							to16[2] = ftoshort(chanel_colormanage_cb(from_float[2]));
							to16 += 3; from_float += 3;
						}
					}
					else {
						for (i = ibuf->x * ibuf->y; i > 0; i--) {
							to16[0] = ftoshort(chanel_colormanage_cb(from_float[0]));
							to16[2] = to16[1] = to16[0];
							to16 += 3; from_float++;
						}
					}
				}
				else {
					for (i = ibuf->x * ibuf->y; i > 0; i--) {
						to16[0] = UPSAMPLE_8_TO_16(from[0]);
						to16[1] = UPSAMPLE_8_TO_16(from[1]);
						to16[2] = UPSAMPLE_8_TO_16(from[2]);
						to16 += 3; from += 4;
					}
				}
			}
			else {
				for (i = ibuf->x * ibuf->y; i > 0; i--) {
					to[0] = from[0];
					to[1] = from[1];
					to[2] = from[2];
					to += 3; from += 4;
				}
			}
			break;
		case 1:
			color_type = PNG_COLOR_TYPE_GRAY;
			if (is_16bit) {
				if (has_float) {
					float rgb[3];
					if (channels_in_float == 4) {
						for (i = ibuf->x * ibuf->y; i > 0; i--) {
							premul_to_straight_v4_v4(from_straight, from_float);
							rgb[0] = chanel_colormanage_cb(from_straight[0]);
							rgb[1] = chanel_colormanage_cb(from_straight[1]);
							rgb[2] = chanel_colormanage_cb(from_straight[2]);
							to16[0] = ftoshort(rgb_to_bw(rgb));
							to16++; from_float += 4;
						}
					}
					else if (channels_in_float == 3) {
						for (i = ibuf->x * ibuf->y; i > 0; i--) {
							rgb[0] = chanel_colormanage_cb(from_float[0]);
							rgb[1] = chanel_colormanage_cb(from_float[1]);
							rgb[2] = chanel_colormanage_cb(from_float[2]);
							to16[0] = ftoshort(rgb_to_bw(rgb));
							to16++; from_float += 3;
						}
					}
					else {
						for (i = ibuf->x * ibuf->y; i > 0; i--) {
							to16[0] = ftoshort(chanel_colormanage_cb(from_float[0]));
							to16++; from_float++;
						}
					}
				}
				else {
					for (i = ibuf->x * ibuf->y; i > 0; i--) {
						to16[0] = UPSAMPLE_8_TO_16(from[0]);
						to16++; from += 4;
					}
				}
			}
			else {
				for (i = ibuf->x * ibuf->y; i > 0; i--) {
					to[0] = from[0];
					to++; from += 4;
				}
			}
			break;
	}

	if (flags & IB_mem) {
		/* create image in memory */
		imb_addencodedbufferImBuf(ibuf);
		ibuf->encodedsize = 0;

		png_set_write_fn(png_ptr,
		                 (png_voidp) ibuf,
		                 WriteData,
		                 Flush);
	}
	else {
		fp = BLI_fopen(name, "wb");
		if (!fp) {
			png_destroy_write_struct(&png_ptr, &info_ptr);
			if (pixels)
				MEM_freeN(pixels);
			if (pixels16)
				MEM_freeN(pixels16);
			printf("imb_savepng: Cannot open file for writing: '%s'\n", name);
			return 0;
		}
		png_init_io(png_ptr, fp);
	}

#if 0
	png_set_filter(png_ptr, 0,
	               PNG_FILTER_NONE  | PNG_FILTER_VALUE_NONE  |
	               PNG_FILTER_SUB   | PNG_FILTER_VALUE_SUB   |
	               PNG_FILTER_UP    | PNG_FILTER_VALUE_UP    |
	               PNG_FILTER_AVG   | PNG_FILTER_VALUE_AVG   |
	               PNG_FILTER_PAETH | PNG_FILTER_VALUE_PAETH |
	               PNG_ALL_FILTERS);
#endif

	png_set_compression_level(png_ptr, compression);

	/* png image settings */
	png_set_IHDR(png_ptr,
	             info_ptr,
	             ibuf->x,
	             ibuf->y,
	             is_16bit ? 16 : 8,
	             color_type,
	             PNG_INTERLACE_NONE,
	             PNG_COMPRESSION_TYPE_DEFAULT,
	             PNG_FILTER_TYPE_DEFAULT);

	/* image text info */
	if (ibuf->metadata) {
		png_text *metadata;
		ImMetaData *iptr;
		int num_text = 0;
		iptr = ibuf->metadata;
		while (iptr) {
			num_text++;
			iptr = iptr->next;
		}
		
		metadata = MEM_callocN(num_text * sizeof(png_text), "png_metadata");
		iptr = ibuf->metadata;
		num_text = 0;
		while (iptr) {
			
			metadata[num_text].compression = PNG_TEXT_COMPRESSION_NONE;
			metadata[num_text].key = iptr->key;
			metadata[num_text].text = iptr->value;
			num_text++;
			iptr = iptr->next;
		}
		
		png_set_text(png_ptr, info_ptr, metadata, num_text);
		MEM_freeN(metadata);

	}

	if (ibuf->ppm[0] > 0.0 && ibuf->ppm[1] > 0.0) {
		png_set_pHYs(png_ptr, info_ptr, (unsigned int)(ibuf->ppm[0] + 0.5), (unsigned int)(ibuf->ppm[1] + 0.5), PNG_RESOLUTION_METER);
	}

	/* write the file header information */
	png_write_info(png_ptr, info_ptr);

#ifdef __LITTLE_ENDIAN__
	png_set_swap(png_ptr);
#endif

	/* allocate memory for an array of row-pointers */
	row_pointers = (png_bytepp) MEM_mallocN(ibuf->y * sizeof(png_bytep), "row_pointers");
	if (row_pointers == NULL) {
		printf("imb_savepng: Cannot allocate row-pointers array for file '%s'\n", name);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		if (pixels)
			MEM_freeN(pixels);
		if (pixels16)
			MEM_freeN(pixels16);
		if (fp) {
			fclose(fp);
		}
		return 0;
	}

	/* set the individual row-pointers to point at the correct offsets */
	if (is_16bit) {
		for (i = 0; i < ibuf->y; i++) {
			row_pointers[ibuf->y - 1 - i] = (png_bytep)
			                                ((unsigned short *)pixels16 + (i * ibuf->x) * bytesperpixel);
		}
	}
	else {
		for (i = 0; i < ibuf->y; i++) {
			row_pointers[ibuf->y - 1 - i] = (png_bytep)
			                                ((unsigned char *)pixels + (i * ibuf->x) * bytesperpixel * sizeof(unsigned char));
		}
	}

	/* write out the entire image data in one call */
	png_write_image(png_ptr, row_pointers);

	/* write the additional chunks to the PNG file (not really needed) */
	png_write_end(png_ptr, info_ptr);

	/* clean up */
	if (pixels)
		MEM_freeN(pixels);
	if (pixels16)
		MEM_freeN(pixels16);
	MEM_freeN(row_pointers);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	if (fp) {
		fflush(fp);
		fclose(fp);
	}

	return(1);
}

static void imb_png_warning(png_structp UNUSED(png_ptr), png_const_charp message)
{
	/* We suppress iCCP warnings. That's how Blender always used to behave,
	 * and with new libpng it became too much picky, giving a warning on
	 * the splash screen even.
	 */
	if ((G.debug & G_DEBUG) == 0 && !strncmp(message, "iCCP", 4)) {
		return;
	}
	fprintf(stderr, "libpng warning: %s\n", message);
}

static void imb_png_error(png_structp UNUSED(png_ptr), png_const_charp message)
{
	fprintf(stderr, "libpng error: %s\n", message);
}

ImBuf *imb_loadpng(unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
	struct ImBuf *ibuf = NULL;
	png_structp png_ptr;
	png_infop info_ptr;
	unsigned char *pixels = NULL;
	unsigned short *pixels16 = NULL;
	png_bytepp row_pointers = NULL;
	png_uint_32 width, height;
	int bit_depth, color_type;
	PNGReadStruct ps;

	unsigned char *from, *to;
	unsigned short *from16;
	float *to_float;
	int i, bytesperpixel;

	if (imb_is_a_png(mem) == 0) return(NULL);

	/* both 8 and 16 bit PNGs are default to standard byte colorspace */
	colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_BYTE);

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
	                                 NULL, NULL, NULL);
	if (png_ptr == NULL) {
		printf("Cannot png_create_read_struct\n");
		return NULL;
	}

	png_set_error_fn(png_ptr, NULL, imb_png_error, imb_png_warning);

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, 
		                        (png_infopp)NULL);
		printf("Cannot png_create_info_struct\n");
		return NULL;
	}

	ps.size = size; /* XXX, 4gig limit! */
	ps.data = mem;
	ps.seek = 0;

	png_set_read_fn(png_ptr, (void *) &ps, ReadData);

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		if (pixels) MEM_freeN(pixels);
		if (pixels16) MEM_freeN(pixels16);
		if (row_pointers) MEM_freeN(row_pointers);
		if (ibuf) IMB_freeImBuf(ibuf);
		return NULL;
	}

	// png_set_sig_bytes(png_ptr, 8);

	png_read_info(png_ptr, info_ptr);
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, 
	             &color_type, NULL, NULL, NULL);

	bytesperpixel = png_get_channels(png_ptr, info_ptr);

	switch (color_type) {
		case PNG_COLOR_TYPE_RGB:
		case PNG_COLOR_TYPE_RGB_ALPHA:
			break;
		case PNG_COLOR_TYPE_PALETTE:
			png_set_palette_to_rgb(png_ptr);
			if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
				bytesperpixel = 4;
			}
			else {
				bytesperpixel = 3;
			}
			break;
		case PNG_COLOR_TYPE_GRAY:
		case PNG_COLOR_TYPE_GRAY_ALPHA:
			if (bit_depth < 8) {
				png_set_expand(png_ptr);
				bit_depth = 8;
			}
			break;
		default:
			printf("PNG format not supported\n");
			longjmp(png_jmpbuf(png_ptr), 1);
			break;
	}
	
	ibuf = IMB_allocImBuf(width, height, 8 * bytesperpixel, 0);

	if (ibuf) {
		ibuf->ftype = PNG;
		if (bit_depth == 16)
			ibuf->ftype |= PNG_16BIT;

		if (png_get_valid(png_ptr, info_ptr, PNG_INFO_pHYs)) {
			int unit_type;
			png_uint_32 xres, yres;

			if (png_get_pHYs(png_ptr, info_ptr, &xres, &yres, &unit_type))
				if (unit_type == PNG_RESOLUTION_METER) {
					ibuf->ppm[0] = xres;
					ibuf->ppm[1] = yres;
				}
		}
	}
	else {
		printf("Couldn't allocate memory for PNG image\n");
	}

	if (ibuf && ((flags & IB_test) == 0)) {
		if (bit_depth == 16) {
			imb_addrectfloatImBuf(ibuf);
			png_set_swap(png_ptr);

			pixels16 = MEM_mallocN(ibuf->x * ibuf->y * bytesperpixel * sizeof(png_uint_16), "pixels");
			if (pixels16 == NULL) {
				printf("Cannot allocate pixels array\n");
				longjmp(png_jmpbuf(png_ptr), 1);
			}

			/* allocate memory for an array of row-pointers */
			row_pointers = (png_bytepp) MEM_mallocN(ibuf->y * sizeof(png_uint_16p), "row_pointers");
			if (row_pointers == NULL) {
				printf("Cannot allocate row-pointers array\n");
				longjmp(png_jmpbuf(png_ptr), 1);
			}

			/* set the individual row-pointers to point at the correct offsets */
			for (i = 0; i < ibuf->y; i++) {
				row_pointers[ibuf->y - 1 - i] = (png_bytep)
				                                ((png_uint_16 *)pixels16 + (i * ibuf->x) * bytesperpixel);
			}

			png_read_image(png_ptr, row_pointers);

			/* copy image data */

			to_float = ibuf->rect_float;
			from16 = pixels16;

			switch (bytesperpixel) {
				case 4:
					for (i = ibuf->x * ibuf->y; i > 0; i--) {
						to_float[0] = from16[0] / 65535.0;
						to_float[1] = from16[1] / 65535.0;
						to_float[2] = from16[2] / 65535.0;
						to_float[3] = from16[3] / 65535.0;
						to_float += 4; from16 += 4;
					}
					break;
				case 3:
					for (i = ibuf->x * ibuf->y; i > 0; i--) {
						to_float[0] = from16[0] / 65535.0;
						to_float[1] = from16[1] / 65535.0;
						to_float[2] = from16[2] / 65535.0;
						to_float[3] = 1.0;
						to_float += 4; from16 += 3;
					}
					break;
				case 2:
					for (i = ibuf->x * ibuf->y; i > 0; i--) {
						to_float[0] = to_float[1] = to_float[2] = from16[0] / 65535.0;
						to_float[3] = from16[1] / 65535.0;
						to_float += 4; from16 += 2;
					}
					break;
				case 1:
					for (i = ibuf->x * ibuf->y; i > 0; i--) {
						to_float[0] = to_float[1] = to_float[2] = from16[0] / 65535.0;
						to_float[3] = 1.0;
						to_float += 4; from16++;
					}
					break;
			}
		}
		else {
			imb_addrectImBuf(ibuf);

			pixels = MEM_mallocN(ibuf->x * ibuf->y * bytesperpixel * sizeof(unsigned char), "pixels");
			if (pixels == NULL) {
				printf("Cannot allocate pixels array\n");
				longjmp(png_jmpbuf(png_ptr), 1);
			}

			/* allocate memory for an array of row-pointers */
			row_pointers = (png_bytepp) MEM_mallocN(ibuf->y * sizeof(png_bytep), "row_pointers");
			if (row_pointers == NULL) {
				printf("Cannot allocate row-pointers array\n");
				longjmp(png_jmpbuf(png_ptr), 1);
			}

			/* set the individual row-pointers to point at the correct offsets */
			for (i = 0; i < ibuf->y; i++) {
				row_pointers[ibuf->y - 1 - i] = (png_bytep)
				                                ((unsigned char *)pixels + (i * ibuf->x) * bytesperpixel * sizeof(unsigned char));
			}

			png_read_image(png_ptr, row_pointers);

			/* copy image data */

			to = (unsigned char *) ibuf->rect;
			from = pixels;

			switch (bytesperpixel) {
				case 4:
					for (i = ibuf->x * ibuf->y; i > 0; i--) {
						to[0] = from[0];
						to[1] = from[1];
						to[2] = from[2];
						to[3] = from[3];
						to += 4; from += 4;
					}
					break;
				case 3:
					for (i = ibuf->x * ibuf->y; i > 0; i--) {
						to[0] = from[0];
						to[1] = from[1];
						to[2] = from[2];
						to[3] = 0xff;
						to += 4; from += 3;
					}
					break;
				case 2:
					for (i = ibuf->x * ibuf->y; i > 0; i--) {
						to[0] = to[1] = to[2] = from[0];
						to[3] = from[1];
						to += 4; from += 2;
					}
					break;
				case 1:
					for (i = ibuf->x * ibuf->y; i > 0; i--) {
						to[0] = to[1] = to[2] = from[0];
						to[3] = 0xff;
						to += 4; from++;
					}
					break;
			}
		}

		if (flags & IB_metadata) {
			png_text *text_chunks;
			int count = png_get_text(png_ptr, info_ptr, &text_chunks, NULL);
			for (i = 0; i < count; i++) {
				IMB_metadata_add_field(ibuf, text_chunks[i].key, text_chunks[i].text);
				ibuf->flags |= IB_metadata;
			}
		}

		png_read_end(png_ptr, info_ptr);
	}

	/* clean up */
	if (pixels)
		MEM_freeN(pixels);
	if (pixels16)
		MEM_freeN(pixels16);
	if (row_pointers)
		MEM_freeN(row_pointers);
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

	return(ibuf);
}

