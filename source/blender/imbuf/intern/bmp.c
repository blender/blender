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

/** \file blender/imbuf/intern/bmp.c
 *  \ingroup imbuf
 */

#include "BLI_utildefines.h"
#include "BLI_fileops.h"

#include "imbuf.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_filetype.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

/* some code copied from article on microsoft.com, copied
 * here for enhanced BMP support in the future
 * http://www.microsoft.com/msj/defaultframe.asp?page=/msj/0197/mfcp1/mfcp1.htm&nav=/msj/0197/newnav.htm
 */

typedef struct BMPINFOHEADER {
	unsigned int    biSize;
	unsigned int    biWidth;
	unsigned int    biHeight;
	unsigned short  biPlanes;
	unsigned short  biBitCount;
	unsigned int    biCompression;
	unsigned int    biSizeImage;
	unsigned int    biXPelsPerMeter;
	unsigned int    biYPelsPerMeter;
	unsigned int    biClrUsed;
	unsigned int    biClrImportant;
} BMPINFOHEADER;

#if 0
typedef struct BMPHEADER {
	unsigned short biType;
	unsigned int biSize;
	unsigned short biRes1;
	unsigned short biRes2;
	unsigned int biOffBits;
} BMPHEADER;
#endif

#define BMP_FILEHEADER_SIZE 14

#define CHECK_HEADER_FIELD(_mem, _field) ((_mem[0] == _field[0]) && (_mem[1] == _field[1]))
#define CHECK_HEADER_FIELD_BMP(_mem)  \
	(CHECK_HEADER_FIELD(_mem, "BM") ||  \
	 CHECK_HEADER_FIELD(_mem, "BA") ||  \
	 CHECK_HEADER_FIELD(_mem, "CI") ||  \
	 CHECK_HEADER_FIELD(_mem, "CP") ||  \
	 CHECK_HEADER_FIELD(_mem, "IC") ||  \
	 CHECK_HEADER_FIELD(_mem, "PT"))

static int checkbmp(const unsigned char *mem)
{

	int ret_val = 0;
	BMPINFOHEADER bmi;
	unsigned int u;

	if (mem) {
		if (CHECK_HEADER_FIELD_BMP(mem)) {
			/* skip fileheader */
			mem += BMP_FILEHEADER_SIZE;
		}
		else {
			return 0;
		}

		/* for systems where an int needs to be 4 bytes aligned */
		memcpy(&bmi, mem, sizeof(bmi));

		u = LITTLE_LONG(bmi.biSize);
		/* we only support uncompressed images for now. */
		if (u >= sizeof(BMPINFOHEADER)) {
			if (bmi.biCompression == 0) {
				u = LITTLE_SHORT(bmi.biBitCount);
				if (u > 0 && u <= 32) {
					ret_val = 1;
				}
			}
		}
	}

	return(ret_val);
}

int imb_is_a_bmp(const unsigned char *buf)
{
	return checkbmp(buf);
}

struct ImBuf *imb_bmp_decode(const unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
	struct ImBuf *ibuf = NULL;
	BMPINFOHEADER bmi;
	int x, y, depth, ibuf_depth, skip;
	const unsigned char *bmp;
	unsigned char *rect;
	unsigned short col;
	double xppm, yppm;
	bool top_to_bottom = false;
	
	(void)size; /* unused */

	if (checkbmp(mem) == 0) return(NULL);

	colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_BYTE);

	bmp = mem + LITTLE_LONG(*(int *)(mem + 10));

	if (CHECK_HEADER_FIELD_BMP(mem)) {
		/* skip fileheader */
		mem += BMP_FILEHEADER_SIZE;
	}
	else {
		return NULL;
	}

	/* for systems where an int needs to be 4 bytes aligned */
	memcpy(&bmi, mem, sizeof(bmi));

	skip = LITTLE_LONG(bmi.biSize);
	x = LITTLE_LONG(bmi.biWidth);
	y = LITTLE_LONG(bmi.biHeight);
	depth = LITTLE_SHORT(bmi.biBitCount);
	xppm = LITTLE_LONG(bmi.biXPelsPerMeter);
	yppm = LITTLE_LONG(bmi.biYPelsPerMeter);

	if (depth <= 8) {
		ibuf_depth = 24;
	}
	else {
		ibuf_depth = depth;
	}

	if (y < 0) {
		/* Negative height means bitmap is stored top-to-bottom... */
		y = -y;
		top_to_bottom = true;
	}

#if 0
	printf("skip: %d, x: %d y: %d, depth: %d (%x)\n", skip, x, y, depth, bmi.biBitCount);
#endif

	if (flags & IB_test) {
		ibuf = IMB_allocImBuf(x, y, ibuf_depth, 0);
	}
	else {
		ibuf = IMB_allocImBuf(x, y, ibuf_depth, IB_rect);
		if (!ibuf) {
			return NULL;
		}

		rect = (unsigned char *) ibuf->rect;

		if (depth <= 8) {
			const int rowsize = (depth * x + 31) / 32 * 4;
			const char (*palette)[4] = (void *)(mem + skip);
			const int startmask = ((1 << depth) - 1) << 8;
			for (size_t i = y; i > 0; i--) {
				int index;
				int bitoffs = 8;
				int bitmask = startmask;
				int nbytes = 0;
				const char *pcol;
				if (top_to_bottom) {
					rect = (unsigned char *) &ibuf->rect[(i - 1) * x];
				}
				for (size_t j = x; j > 0; j--) {
					bitoffs -= depth;
					bitmask >>= depth;
					index = (bmp[0] & bitmask) >> bitoffs;
					pcol = palette[index];
					/* intentionally BGR -> RGB */
					rect[0] = pcol[2];
					rect[1] = pcol[1];
					rect[2] = pcol[0];

					rect[3] = 255;
					rect += 4;
					if (bitoffs == 0) {
						/* Advance to the next byte */
						bitoffs = 8;
						bitmask = startmask;
						nbytes += 1;
						bmp += 1;
					}
				}
				/* Advance to the next row */
				bmp += (rowsize - nbytes);
			}
		}
		else if (depth == 16) {
			for (size_t i = y; i > 0; i--) {
				if (top_to_bottom) {
					rect = (unsigned char *) &ibuf->rect[(i - 1) * x];
				}
				for (size_t j = x; j > 0; j--) {
					col = bmp[0] + (bmp[1] << 8);
					rect[0] = ((col >> 10) & 0x1f) << 3;
					rect[1] = ((col >>  5) & 0x1f) << 3;
					rect[2] = ((col >>  0) & 0x1f) << 3;

					rect[3] = 255;
					rect += 4; bmp += 2;
				}
			}
		}
		else if (depth == 24) {
			const int x_pad = x % 4;
			for (size_t i = y; i > 0; i--) {
				if (top_to_bottom) {
					rect = (unsigned char *) &ibuf->rect[(i - 1) * x];
				}
				for (size_t j = x; j > 0; j--) {
					rect[0] = bmp[2];
					rect[1] = bmp[1];
					rect[2] = bmp[0];
					
					rect[3] = 255;
					rect += 4; bmp += 3;
				}
				/* for 24-bit images, rows are padded to multiples of 4 */
				bmp += x_pad;
			}
		}
		else if (depth == 32) {
			for (size_t i = y; i > 0; i--) {
				if (top_to_bottom) {
					rect = (unsigned char *) &ibuf->rect[(i - 1) * x];
				}
				for (size_t j = x; j > 0; j--) {
					rect[0] = bmp[2];
					rect[1] = bmp[1];
					rect[2] = bmp[0];
					rect[3] = bmp[3];
					rect += 4; bmp += 4;
				}
			}
		}
	}

	if (ibuf) {
		ibuf->ppm[0] = xppm;
		ibuf->ppm[1] = yppm;
		ibuf->ftype = IMB_FTYPE_BMP;
	}
	
	return(ibuf);
}

#undef CHECK_HEADER_FIELD_BMP
#undef CHECK_HEADER_FIELD

/* Couple of helper functions for writing our data */
static int putIntLSB(unsigned int ui, FILE *ofile)
{
	putc((ui >> 0) & 0xFF, ofile);
	putc((ui >> 8) & 0xFF, ofile);
	putc((ui >> 16) & 0xFF, ofile);
	return putc((ui >> 24) & 0xFF, ofile);
}

static int putShortLSB(unsigned short us, FILE *ofile)
{
	putc((us >> 0) & 0xFF, ofile);
	return putc((us >> 8) & 0xFF, ofile);
} 

/* Found write info at http://users.ece.gatech.edu/~slabaugh/personal/c/bitmapUnix.c */
int imb_savebmp(struct ImBuf *ibuf, const char *name, int flags)
{
	BMPINFOHEADER infoheader;
	size_t bytesize, extrabytes, ptr;
	uchar *data;
	FILE *ofile;
	
	(void)flags; /* unused */

	extrabytes = (4 - ibuf->x * 3 % 4) % 4;
	bytesize = (ibuf->x * 3 + extrabytes) * ibuf->y;

	data = (uchar *) ibuf->rect;
	ofile = BLI_fopen(name, "wb");
	if (!ofile) return 0;

	putShortLSB(19778, ofile); /* "BM" */
	putIntLSB(bytesize + BMP_FILEHEADER_SIZE + sizeof(infoheader), ofile);  /* Total file size */
	putShortLSB(0, ofile); /* Res1 */
	putShortLSB(0, ofile); /* Res2 */
	putIntLSB(BMP_FILEHEADER_SIZE + sizeof(infoheader), ofile);

	putIntLSB(sizeof(infoheader), ofile);
	putIntLSB(ibuf->x, ofile);
	putIntLSB(ibuf->y, ofile);
	putShortLSB(1, ofile);
	putShortLSB(24, ofile);
	putIntLSB(0, ofile);
	putIntLSB(bytesize, ofile);
	putIntLSB((int)(ibuf->ppm[0] + 0.5), ofile);
	putIntLSB((int)(ibuf->ppm[1] + 0.5), ofile);
	putIntLSB(0, ofile);
	putIntLSB(0, ofile);

	/* Need to write out padded image data in bgr format */
	for (size_t y = 0; y < ibuf->y; y++) {
		for (size_t x = 0; x < ibuf->x; x++) {
			ptr = (x + y * ibuf->x) * 4;
			if (putc(data[ptr + 2], ofile) == EOF) return 0;
			if (putc(data[ptr + 1], ofile) == EOF) return 0;
			if (putc(data[ptr], ofile) == EOF) return 0;
		}
		/* add padding here */
		for (size_t t = 0; t < extrabytes; t++) {
			if (putc(0, ofile) == EOF) return 0;
		}
	}
	if (ofile) {
		fflush(ofile);
		fclose(ofile);
	}
	return 1;
}
