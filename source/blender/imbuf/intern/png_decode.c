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

#ifdef _WIN32
#include "BLI_winstuff.h"
#endif
#include "BLI_blenlib.h"

#include "imbuf.h"
#include "imbuf_patch.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"
#include "IMB_cmap.h"
#include "IMB_png.h"



static int checkpng(unsigned char *mem)
{
	int ret_val = 0;

	if (mem) {
		ret_val = !png_sig_cmp(mem, 0, 8);
	}

	return(ret_val);	
}

int imb_is_a_png(void *buf) {
	
	return checkpng(buf);
}

typedef struct PNGReadStruct {
	unsigned char *data;
	unsigned int size;
	unsigned int seek;
}PNGReadStruct;

static void
ReadData(
    png_structp png_ptr,
    png_bytep data,
    png_size_t length);

static void
ReadData(
    png_structp png_ptr,
    png_bytep data,
    png_size_t length)
{
	PNGReadStruct *rs= (PNGReadStruct *) png_get_io_ptr(png_ptr);

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


struct ImBuf *imb_png_decode(unsigned char *mem, int size, int flags)
{
	struct ImBuf *ibuf = 0;
	png_structp png_ptr;
	png_infop info_ptr;
	unsigned char *pixels = 0;
	png_bytepp row_pointers = 0;
	png_uint_32 width, height;
	int bit_depth, color_type;
	PNGReadStruct ps;

	unsigned char *from, *to;
	int i, bytesperpixel;

	if (checkpng(mem) == 0) return(0);

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL);
	if (png_ptr == NULL) {
		printf("Cannot png_create_read_struct\n");
		return 0;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		printf("Cannot png_create_info_struct\n");
		return 0;
	}

	ps.size = size;
	ps.data = mem;
	ps.seek = 0;

	png_set_read_fn(png_ptr, (void *) &ps, ReadData);

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		if (pixels) MEM_freeN(pixels);
		if (row_pointers) MEM_freeN(row_pointers);
		if (ibuf) IMB_freeImBuf(ibuf);
		return 0;
	}

	// png_set_sig_bytes(png_ptr, 8);

	png_read_info(png_ptr, info_ptr);
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

	if (bit_depth == 16) {
		png_set_strip_16(png_ptr);
		bit_depth = 8;
	}

	bytesperpixel = png_get_channels(png_ptr, info_ptr);

	switch(color_type) {
	case PNG_COLOR_TYPE_RGB:
	case PNG_COLOR_TYPE_RGB_ALPHA:
		break;
	case PNG_COLOR_TYPE_PALETTE:
		png_set_palette_to_rgb(png_ptr);
		if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
			bytesperpixel = 4;
		} else {
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
	}
	
	ibuf = IMB_allocImBuf(width, height, 8 * bytesperpixel, 0, 0);

	if (ibuf) {
		ibuf->ftype = PNG;
	} else {
		printf("Couldn't allocate memory for PNG image\n");
	}

	if (ibuf && ((flags & IB_test) == 0)) {
		imb_addrectImBuf(ibuf);

		pixels = MEM_mallocN(ibuf->x * ibuf->y * bytesperpixel * sizeof(unsigned char), "pixels");
		if (pixels == NULL) {
			printf("Cannot allocate pixels array\n");
			longjmp(png_jmpbuf(png_ptr), 1);
		}

		// allocate memory for an array of row-pointers
		row_pointers = (png_bytepp) MEM_mallocN(ibuf->y * sizeof(png_bytep), "row_pointers");
		if (row_pointers == NULL) {
			printf("Cannot allocate row-pointers array\n");
			longjmp(png_jmpbuf(png_ptr), 1);
		}

		// set the individual row-pointers to point at the correct offsets
		for (i = 0; i < ibuf->y; i++) {
			row_pointers[ibuf->y-1-i] = (png_bytep)
			((unsigned char *)pixels + (i * ibuf->x) * bytesperpixel * sizeof(unsigned char));
		}

		png_read_image(png_ptr, row_pointers);

		// copy image data

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

		png_read_end(png_ptr, info_ptr);
	}

	// clean up
	MEM_freeN(pixels);
	MEM_freeN(row_pointers);
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

	return(ibuf);
}

