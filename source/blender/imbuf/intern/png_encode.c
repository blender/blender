/**
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * $Id$
 */


#include "png.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "BLI_blenlib.h"

#include "imbuf.h"
#include "imbuf_patch.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"
#include "IMB_cmap.h"

static void
WriteData(
    png_structp png_ptr,
    png_bytep data,
    png_size_t length);

static void
Flush(
    png_structp png_ptr);

static void
WriteData(
    png_structp png_ptr,
    png_bytep data,
    png_size_t length)
{
	ImBuf *ibuf = (ImBuf *) png_get_io_ptr(png_ptr);

	// if buffer is to small increase it.
	while (ibuf->encodedsize + length > ibuf->encodedbuffersize) {
		imb_enlargeencodedbufferImBuf(ibuf);
	}

	memcpy(ibuf->encodedbuffer + ibuf->encodedsize, data, length);
	ibuf->encodedsize += length;
}

static void
Flush(
    png_structp png_ptr)
{
}

short IMB_png_encode(struct ImBuf *ibuf, int file, int flags)
{
	png_structp png_ptr;
	png_infop info_ptr;
	unsigned char *pixels = 0;
	unsigned char *from, *to;
	png_bytepp row_pointers = 0;
	int i, bytesperpixel, color_type = PNG_COLOR_TYPE_GRAY;
	FILE *fp = 0;

	bytesperpixel = (ibuf->depth + 7) >> 3;
	if ((bytesperpixel > 4) || (bytesperpixel == 2)) {
		printf("imb_png_encode: unsupported bytes per pixel: %d\n", bytesperpixel);
		return (0);
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL);
	if (png_ptr == NULL) {
		printf("Cannot png_create_write_struct\n");
		return 0;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		printf("Cannot png_create_info_struct\n");
		return 0;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		if (pixels) MEM_freeN(pixels);
		if (row_pointers) MEM_freeN(row_pointers);
		// printf("Aborting\n");
		if (fp) {
			fflush(fp);
			fclose(fp);
		}
		return 0;
	}

	// copy image data

	pixels = MEM_mallocN(ibuf->x * ibuf->y * bytesperpixel * sizeof(unsigned char), "pixels");
	if (pixels == NULL) {
		printf("Cannot allocate pixels array\n");
		return 0;
	}

	from = (unsigned char *) ibuf->rect;
	to = pixels;

	switch (bytesperpixel) {
	case 4:
		color_type = PNG_COLOR_TYPE_RGBA;
		for (i = ibuf->x * ibuf->y; i > 0; i--) {
			to[0] = from[0];
			to[1] = from[1];
			to[2] = from[2];
			to[3] = from[3];
			to += 4; from += 4;
		}
		break;
	case 3:
		color_type = PNG_COLOR_TYPE_RGB;
		for (i = ibuf->x * ibuf->y; i > 0; i--) {
			to[0] = from[0];
			to[1] = from[1];
			to[2] = from[2];
			to += 3; from += 4;
		}
		break;
	case 1:
		color_type = PNG_COLOR_TYPE_GRAY;
		for (i = ibuf->x * ibuf->y; i > 0; i--) {
			to[0] = from[0];
			to++; from += 4;
		}
		break;
	}

	if (flags & IB_mem) {
		// create image in memory
		imb_addencodedbufferImBuf(ibuf);
		ibuf->encodedsize = 0;

		png_set_write_fn(png_ptr,
			 (png_voidp) ibuf,
			 WriteData,
			 Flush);
	} else {
		fp = fdopen(file, "wb");
		png_init_io(png_ptr, fp);
	}

	/*
	png_set_filter(png_ptr, 0,
		PNG_FILTER_NONE  | PNG_FILTER_VALUE_NONE |
		PNG_FILTER_SUB   | PNG_FILTER_VALUE_SUB  |
		PNG_FILTER_UP    | PNG_FILTER_VALUE_UP   |
		PNG_FILTER_AVG   | PNG_FILTER_VALUE_AVG  |
		PNG_FILTER_PAETH | PNG_FILTER_VALUE_PAETH|
		PNG_ALL_FILTERS);

	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
	*/

	// png image settings
	png_set_IHDR(png_ptr,
		 info_ptr,
		 ibuf->x,
		 ibuf->y,
		 8,
		 color_type,
		 PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT,
		 PNG_FILTER_TYPE_DEFAULT);

	// write the file header information
	png_write_info(png_ptr, info_ptr);

	// allocate memory for an array of row-pointers
	row_pointers = (png_bytepp) MEM_mallocN(ibuf->y * sizeof(png_bytep), "row_pointers");
	if (row_pointers == NULL) {
			printf("Cannot allocate row-pointers array\n");
			return 0;
	}

	// set the individual row-pointers to point at the correct offsets
	for (i = 0; i < ibuf->y; i++) {
		row_pointers[ibuf->y-1-i] = (png_bytep)
			((unsigned char *)pixels + (i * ibuf->x) * bytesperpixel * sizeof(unsigned char));
	}

	// write out the entire image data in one call
	png_write_image(png_ptr, row_pointers);

	// write the additional chunks to the PNG file (not really needed)
	png_write_end(png_ptr, info_ptr);

	// clean up
	MEM_freeN(pixels);
	MEM_freeN(row_pointers);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	if (fp) {
		fflush(fp);
		fclose(fp);
	}

	return(1);
}

