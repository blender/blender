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
 */



#include "png.h"

#include "BLI_blenlib.h"
#include "MEM_guardedalloc.h"

#include "imbuf.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"
#include "IMB_metadata.h"
#include "IMB_filetype.h"

typedef struct PNGReadStruct {
	unsigned char *data;
	unsigned int size;
	unsigned int seek;
}PNGReadStruct;

static void ReadData( png_structp png_ptr, png_bytep data, png_size_t length);
static void WriteData( png_structp png_ptr, png_bytep data, png_size_t length);
static void Flush( png_structp png_ptr);

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

static void WriteData( png_structp png_ptr, png_bytep data, png_size_t length)
{
	ImBuf *ibuf = (ImBuf *) png_get_io_ptr(png_ptr);

	// if buffer is to small increase it.
	while (ibuf->encodedsize + length > ibuf->encodedbuffersize) {
		imb_enlargeencodedbufferImBuf(ibuf);
	}

	memcpy(ibuf->encodedbuffer + ibuf->encodedsize, data, length);
	ibuf->encodedsize += length;
}

static void ReadData( png_structp png_ptr, png_bytep data, png_size_t length)
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

int imb_savepng(struct ImBuf *ibuf, const char *name, int flags)
{
	png_structp png_ptr;
	png_infop info_ptr;

	unsigned char *pixels = NULL;
	unsigned char *from, *to;
	png_bytepp row_pointers = NULL;
	int i, bytesperpixel, color_type = PNG_COLOR_TYPE_GRAY;
	FILE *fp = NULL;

	/* use the jpeg quality setting for compression */
	int compression;
	compression= (int)(((float)(ibuf->ftype & 0xff) / 11.1111f));
	compression= compression < 0 ? 0 : (compression > 9 ? 9 : compression);

	/* for prints */
	if(flags & IB_mem)
		name= "<memory>";

	bytesperpixel = (ibuf->planes + 7) >> 3;
	if ((bytesperpixel > 4) || (bytesperpixel == 2)) {
		printf("imb_savepng: Cunsupported bytes per pixel: %d for file: '%s'\n", bytesperpixel, name);
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

	// copy image data

	pixels = MEM_mallocN(ibuf->x * ibuf->y * bytesperpixel * sizeof(unsigned char), "pixels");
	if (pixels == NULL) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		printf("imb_savepng: Cannot allocate pixels array of %dx%d, %d bytes per pixel for file: '%s'\n", ibuf->x, ibuf->y, bytesperpixel, name);
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
	}
	else {
		fp = BLI_fopen(name, "wb");
		if (!fp) {
			png_destroy_write_struct(&png_ptr, &info_ptr);
			MEM_freeN(pixels);
			printf("imb_savepng: Cannot open file for writing: '%s'\n", name);
			return 0;
		}
		png_init_io(png_ptr, fp);
	}

#if 0
	png_set_filter(png_ptr, 0,
	               PNG_FILTER_NONE  | PNG_FILTER_VALUE_NONE |
	               PNG_FILTER_SUB   | PNG_FILTER_VALUE_SUB  |
	               PNG_FILTER_UP    | PNG_FILTER_VALUE_UP   |
	               PNG_FILTER_AVG   | PNG_FILTER_VALUE_AVG  |
	               PNG_FILTER_PAETH | PNG_FILTER_VALUE_PAETH|
	               PNG_ALL_FILTERS);
#endif

	png_set_compression_level(png_ptr, compression);

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

	/* image text info */
	if (ibuf->metadata) {
		png_text*  metadata;
		ImMetaData* iptr;
		int  num_text = 0;
		iptr = ibuf->metadata;
		while (iptr) {
			num_text++;
			iptr = iptr->next;
		}
		
		metadata = MEM_callocN(num_text*sizeof(png_text), "png_metadata");
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

	if(ibuf->ppm[0] > 0.0 && ibuf->ppm[1] > 0.0) {
		png_set_pHYs(png_ptr, info_ptr, (unsigned int)(ibuf->ppm[0] + 0.5), (unsigned int)(ibuf->ppm[1] + 0.5), PNG_RESOLUTION_METER);
	}

	// write the file header information
	png_write_info(png_ptr, info_ptr);

	// allocate memory for an array of row-pointers
	row_pointers = (png_bytepp) MEM_mallocN(ibuf->y * sizeof(png_bytep), "row_pointers");
	if (row_pointers == NULL) {
		printf("imb_savepng: Cannot allocate row-pointers array for file '%s'\n", name);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		MEM_freeN(pixels);
		if (fp) {
			fclose(fp);
		}
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

struct ImBuf *imb_loadpng(unsigned char *mem, size_t size, int flags)
{
	struct ImBuf *ibuf = NULL;
	png_structp png_ptr;
	png_infop info_ptr;
	unsigned char *pixels = NULL;
	png_bytepp row_pointers = NULL;
	png_uint_32 width, height;
	int bit_depth, color_type;
	PNGReadStruct ps;

	unsigned char *from, *to;
	int i, bytesperpixel;

	if (imb_is_a_png(mem) == 0) return(NULL);

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL);
	if (png_ptr == NULL) {
		printf("Cannot png_create_read_struct\n");
		return NULL;
	}

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
		if (row_pointers) MEM_freeN(row_pointers);
		if (ibuf) IMB_freeImBuf(ibuf);
		return NULL;
	}

	// png_set_sig_bytes(png_ptr, 8);

	png_read_info(png_ptr, info_ptr);
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, 
		&color_type, NULL, NULL, NULL);

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
	
	ibuf = IMB_allocImBuf(width, height, 8 * bytesperpixel, 0);

	if (ibuf) {
		ibuf->ftype = PNG;
		ibuf->profile = IB_PROFILE_SRGB;

		if (png_get_valid (png_ptr, info_ptr, PNG_INFO_pHYs)) {
			int unit_type;
			png_uint_32 xres, yres;

			if(png_get_pHYs(png_ptr, info_ptr, &xres, &yres, &unit_type))
			if(unit_type == PNG_RESOLUTION_METER) {
				ibuf->ppm[0]= xres;
				ibuf->ppm[1]= yres;
			}
		}
	}
	else {
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

		if (flags & IB_metadata) {
			png_text* text_chunks;
			int count = png_get_text(png_ptr, info_ptr, &text_chunks, NULL);
			for(i = 0; i < count; i++) {
				IMB_metadata_add_field(ibuf, text_chunks[i].key, text_chunks[i].text);
				ibuf->flags |= IB_metadata;				
			 }
		}

		png_read_end(png_ptr, info_ptr);
	}

	// clean up
	MEM_freeN(pixels);
	MEM_freeN(row_pointers);
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

	return(ibuf);
}

