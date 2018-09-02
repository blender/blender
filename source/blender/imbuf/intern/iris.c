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

/** \file blender/imbuf/intern/iris.c
 *  \ingroup imbuf
 */


#include <string.h>

#include "BLI_utildefines.h"
#include "BLI_fileops.h"

#include "MEM_guardedalloc.h"

#include "imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_filetype.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

#define IMAGIC 0732

typedef struct {
	ushort  imagic;      /* stuff saved on disk . . */
	ushort  type;
	ushort  dim;
	ushort  xsize;
	ushort  ysize;
	ushort  zsize;
	uint    min;
	uint    max;
	uchar   _pad1[4];
	char    name[80];
	uint    colormap;
	uchar   _pad2[404];
} IMAGE;

#define HEADER_SIZE 512

BLI_STATIC_ASSERT(sizeof(IMAGE) == HEADER_SIZE, "Invalid header size");

#define RINTLUM (79)
#define GINTLUM (156)
#define BINTLUM (21)

#define ILUM(r, g, b)   ((int)(RINTLUM * (r) + GINTLUM * (g) + BINTLUM * (b)) >> 8)

#define OFFSET_R    0   /* this is byte order dependent */
#define OFFSET_G    1
#define OFFSET_B    2
// #define OFFSET_A    3

#define CHANOFFSET(z)   (3 - (z)) /* this is byte order dependent */

// #define TYPEMASK        0xff00
#define BPPMASK         0x00ff
// #define ITYPE_VERBATIM      0x0000 // UNUSED
#define ITYPE_RLE       0x0100
#define ISRLE(type)     (((type) & 0xff00) == ITYPE_RLE)
// #define ISVERBATIM(type)    (((type) & 0xff00) == ITYPE_VERBATIM)
#define BPP(type)       ((type) & BPPMASK)
#define RLE(bpp)        (ITYPE_RLE | (bpp))
// #define VERBATIM(bpp)       (ITYPE_VERBATIM | (bpp)) // UNUSED
// #define IBUFSIZE(pixels)    ((pixels + (pixels >> 6)) << 2) // UNUSED
// #define RLE_NOP         0x00

/* local struct for mem access */
typedef struct MFileOffset {
	const uchar *_file_data;
	uint _file_offset;
} MFileOffset;

#define MFILE_DATA(inf) ((void)0, ((inf)->_file_data + (inf)->_file_offset))
#define MFILE_STEP(inf, step) { (inf)->_file_offset += step; } ((void)0)
#define MFILE_SEEK(inf, pos)  { (inf)->_file_offset  = pos;  } ((void)0)

/* error flags */
#define DIRTY_FLAG_EOF (1 << 0)
#define DIRTY_FLAG_ENCODING (1 << 1)

/* funcs */
static void readheader(MFileOffset *inf, IMAGE *image);
static int writeheader(FILE *outf, IMAGE *image);

static ushort getshort(MFileOffset *inf);
static uint getlong(MFileOffset *inf);
static void putshort(FILE *outf, ushort val);
static int putlong(FILE *outf, uint val);
static int writetab(FILE *outf, uint *tab, int len);
static void readtab(MFileOffset *inf, uint *tab, int len);

static int expandrow(uchar *optr, const uchar *optr_end, const uchar *iptr, const uchar *iptr_end, int z);
static int expandrow2(float *optr, const float *optr_end, const uchar *iptr, const uchar *iptr_end, int z);
static void interleaverow(uchar *lptr, const uchar *cptr, int z, int n);
static void interleaverow2(float *lptr, const uchar *cptr, int z, int n);
static int compressrow(uchar *lbuf, uchar *rlebuf, int z, int cnt);
static void lumrow(uchar *rgbptr, uchar *lumptr, int n);

/*
 *	byte order independent read/write of shorts and ints.
 *
 */

static ushort getshort(MFileOffset *inf)
{
	const uchar *buf;

	buf = MFILE_DATA(inf);
	MFILE_STEP(inf, 2);

	return ((ushort)buf[0] << 8) + ((ushort)buf[1] << 0);
}

static uint getlong(MFileOffset *mofs)
{
	const uchar *buf;

	buf = MFILE_DATA(mofs);
	MFILE_STEP(mofs, 4);

	return ((uint)buf[0] << 24) + ((uint)buf[1] << 16) + ((uint)buf[2] << 8) + ((uint)buf[3] << 0);
}

static void putshort(FILE *outf, ushort val)
{
	uchar buf[2];

	buf[0] = (val >> 8);
	buf[1] = (val >> 0);
	fwrite(buf, 2, 1, outf);
}

static int putlong(FILE *outf, uint val)
{
	uchar buf[4];

	buf[0] = (val >> 24);
	buf[1] = (val >> 16);
	buf[2] = (val >> 8);
	buf[3] = (val >> 0);
	return fwrite(buf, 4, 1, outf);
}

static void readheader(MFileOffset *inf, IMAGE *image)
{
	memset(image, 0, sizeof(IMAGE));
	image->imagic = getshort(inf);
	image->type = getshort(inf);
	image->dim = getshort(inf);
	image->xsize = getshort(inf);
	image->ysize = getshort(inf);
	image->zsize = getshort(inf);
}

static int writeheader(FILE *outf, IMAGE *image)
{
	IMAGE t = {0};

	fwrite(&t, sizeof(IMAGE), 1, outf);
	fseek(outf, 0, SEEK_SET);
	putshort(outf, image->imagic);
	putshort(outf, image->type);
	putshort(outf, image->dim);
	putshort(outf, image->xsize);
	putshort(outf, image->ysize);
	putshort(outf, image->zsize);
	putlong(outf, image->min);
	putlong(outf, image->max);
	putlong(outf, 0);
	return fwrite("no name", 8, 1, outf);
}

static int writetab(FILE *outf, uint *tab, int len)
{
	int r = 0;

	while (len) {
		r = putlong(outf, *tab++);
		len -= 4;
	}
	return r;
}

static void readtab(MFileOffset *inf, uint *tab, int len)
{
	while (len) {
		*tab++ = getlong(inf);
		len -= 4;
	}
}

static void test_endian_zbuf(struct ImBuf *ibuf)
{
	int len;
	int *zval;

	if (BIG_LONG(1) == 1) return;
	if (ibuf->zbuf == NULL) return;

	len = ibuf->x * ibuf->y;
	zval = ibuf->zbuf;

	while (len--) {
		zval[0] = BIG_LONG(zval[0]);
		zval++;
	}
}

/* from misc_util: flip the bytes from x  */
#define GS(x) (((uchar *)(x))[0] << 8 | ((uchar *)(x))[1])

/* this one is only def-ed once, strangely... */
#define GSS(x) (((uchar *)(x))[1] << 8 | ((uchar *)(x))[0])

int imb_is_a_iris(const uchar *mem)
{
	return ((GS(mem) == IMAGIC) || (GSS(mem) == IMAGIC));
}

/*
 *	longimagedata -
 *		read in a B/W RGB or RGBA iris image file and return a
 *	pointer to an array of ints.
 *
 */

struct ImBuf *imb_loadiris(const uchar *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
	uint *base, *lptr = NULL;
	float *fbase, *fptr = NULL;
	uint *zbase, *zptr;
	const uchar *rledat;
	const uchar *mem_end = mem + size;
	MFileOffset _inf_data = {mem, 0}, *inf = &_inf_data;
	IMAGE image;
	int bpp, rle, cur, badorder;
	ImBuf *ibuf = NULL;
	uchar dirty_flag = 0;

	if (size < HEADER_SIZE) {
		return NULL;
	}

	if (!imb_is_a_iris(mem)) {
		return NULL;
	}

	/* OCIO_TODO: only tested with 1 byte per pixel, not sure how to test with other settings */
	colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_BYTE);

	readheader(inf, &image);
	if (image.imagic != IMAGIC) {
		fprintf(stderr, "longimagedata: bad magic number in image file\n");
		return(NULL);
	}

	rle = ISRLE(image.type);
	bpp = BPP(image.type);
	if (bpp != 1 && bpp != 2) {
		fprintf(stderr, "longimagedata: image must have 1 or 2 byte per pix chan\n");
		return(NULL);
	}
	if ((uint)image.zsize > 8) {
		fprintf(stderr, "longimagedata: channels over 8 not supported\n");
		return(NULL);
	}

	const int xsize = image.xsize;
	const int ysize = image.ysize;
	const int zsize = image.zsize;

	if (flags & IB_test) {
		ibuf = IMB_allocImBuf(image.xsize, image.ysize, 8 * image.zsize, 0);
		if (ibuf) ibuf->ftype = IMB_FTYPE_IMAGIC;
		return(ibuf);
	}

	if (rle) {
		size_t tablen = (size_t)ysize * (size_t)zsize * sizeof(int);
		MFILE_SEEK(inf, HEADER_SIZE);

		uint *starttab = MEM_mallocN(tablen, "iris starttab");
		uint *lengthtab = MEM_mallocN(tablen, "iris endtab");

#define MFILE_CAPACITY_AT_PTR_OK_OR_FAIL(p) \
		if (UNLIKELY((p) > mem_end)) { dirty_flag |= DIRTY_FLAG_EOF; goto fail_rle; } ((void)0)

		MFILE_CAPACITY_AT_PTR_OK_OR_FAIL(MFILE_DATA(inf) + ((4 * 2) * tablen));

		readtab(inf, starttab, tablen);
		readtab(inf, lengthtab, tablen);

		/* check data order */
		cur = 0;
		badorder = 0;
		for (size_t y = 0; y < ysize; y++) {
			for (size_t z = 0; z < zsize; z++) {
				if (starttab[y + z * ysize] < cur) {
					badorder = 1;
					break;
				}
				cur = starttab[y + z * ysize];
			}
			if (badorder)
				break;
		}

		if (bpp == 1) {

			ibuf = IMB_allocImBuf(xsize, ysize, 8 * zsize, IB_rect);
			if (!ibuf) {
				goto fail_rle;
			}
			if (ibuf->planes > 32) ibuf->planes = 32;
			base = ibuf->rect;
			zbase = (uint *)ibuf->zbuf;

			if (badorder) {
				for (size_t z = 0; z < zsize; z++) {
					lptr = base;
					for (size_t y = 0; y < ysize; y++) {
						MFILE_SEEK(inf, starttab[y + z * ysize]);
						rledat = MFILE_DATA(inf);
						MFILE_STEP(inf, lengthtab[y + z * ysize]);
						const uchar *rledat_next = MFILE_DATA(inf);
						uint *lptr_next = lptr + xsize;
						MFILE_CAPACITY_AT_PTR_OK_OR_FAIL(rledat_next);
						dirty_flag |= expandrow((uchar *)lptr, (uchar *)lptr_next, rledat, rledat_next, 3 - z);
						lptr = lptr_next;
					}
				}
			}
			else {
				lptr = base;
				zptr = zbase;
				for (size_t y = 0; y < ysize; y++) {

					uint *lptr_next = lptr + xsize;
					uint *zptr_next = zptr + xsize;

					for (size_t z = 0; z < zsize; z++) {
						MFILE_SEEK(inf, starttab[y + z * ysize]);
						rledat = MFILE_DATA(inf);
						MFILE_STEP(inf, lengthtab[y + z * ysize]);
						const uchar *rledat_next = MFILE_DATA(inf);
						MFILE_CAPACITY_AT_PTR_OK_OR_FAIL(rledat_next);
						if (z < 4) {
							dirty_flag |= expandrow((uchar *)lptr, (uchar *)lptr_next, rledat, rledat_next, 3 - z);
						}
						else if (z < 8) {
							dirty_flag |= expandrow((uchar *)zptr, (uchar *)zptr_next, rledat, rledat_next, 7 - z);
						}
					}
					lptr = lptr_next;
					zptr = zptr_next;
				}
			}


		}
		else {  /* bpp == 2 */

			ibuf = IMB_allocImBuf(xsize, ysize, 32, (flags & IB_rect) | IB_rectfloat);
			if (!ibuf) {
				goto fail_rle;
			}

			fbase = ibuf->rect_float;

			if (badorder) {
				for (size_t z = 0; z < zsize; z++) {
					fptr = fbase;
					for (size_t y = 0; y < ysize; y++) {
						MFILE_SEEK(inf, starttab[y + z * ysize]);
						rledat = MFILE_DATA(inf);
						MFILE_STEP(inf, lengthtab[y + z * ysize]);
						const uchar *rledat_next = MFILE_DATA(inf);
						MFILE_CAPACITY_AT_PTR_OK_OR_FAIL(rledat_next);
						float *fptr_next = fptr + (xsize * 4);
						dirty_flag |= expandrow2(fptr, fptr_next, rledat, rledat_next, 3 - z);
						fptr = fptr_next;
					}
				}
			}
			else {
				fptr = fbase;
				float *fptr_next = fptr + (xsize * 4);

				for (size_t y = 0; y < ysize; y++) {

					for (size_t z = 0; z < zsize; z++) {
						MFILE_SEEK(inf, starttab[y + z * ysize]);
						rledat = MFILE_DATA(inf);
						MFILE_STEP(inf, lengthtab[y + z * ysize]);
						const uchar *rledat_next = MFILE_DATA(inf);
						MFILE_CAPACITY_AT_PTR_OK_OR_FAIL(rledat_next);
						dirty_flag |= expandrow2(fptr, fptr_next, rledat, rledat_next, 3 - z);
					}
					fptr = fptr_next;
				}
			}
		}
#undef MFILE_CAPACITY_AT_PTR_OK_OR_FAIL
fail_rle:
		MEM_freeN(starttab);
		MEM_freeN(lengthtab);

		if (!ibuf) {
			return NULL;
		}
	}
	else {

#define MFILE_CAPACITY_AT_PTR_OK_OR_FAIL(p) \
		if (UNLIKELY((p) > mem_end)) { dirty_flag |= DIRTY_FLAG_EOF; goto fail_uncompressed; } ((void)0)

		if (bpp == 1) {

			ibuf = IMB_allocImBuf(xsize, ysize, 8 * zsize, IB_rect);
			if (!ibuf) {
				goto fail_uncompressed;
			}
			if (ibuf->planes > 32) ibuf->planes = 32;

			base = ibuf->rect;
			zbase = (uint *)ibuf->zbuf;

			MFILE_SEEK(inf, HEADER_SIZE);
			rledat = MFILE_DATA(inf);

			for (size_t z = 0; z < zsize; z++) {

				if (z < 4) lptr = base;
				else if (z < 8) lptr = zbase;

				for (size_t y = 0; y < ysize; y++) {
					const uchar *rledat_next = rledat + xsize;
					const int z_ofs = 3 - z;
					MFILE_CAPACITY_AT_PTR_OK_OR_FAIL(rledat_next + z_ofs);
					interleaverow((uchar *)lptr, rledat, z_ofs, xsize);
					rledat = rledat_next;
					lptr += xsize;
				}
			}

		}
		else {  /* bpp == 2 */

			ibuf = IMB_allocImBuf(xsize, ysize, 32, (flags & IB_rect) | IB_rectfloat);
			if (!ibuf) {
				goto fail_uncompressed;
			}

			fbase = ibuf->rect_float;

			MFILE_SEEK(inf, HEADER_SIZE);
			rledat = MFILE_DATA(inf);

			for (size_t z = 0; z < zsize; z++) {

				fptr = fbase;

				for (size_t y = 0; y < ysize; y++) {
					const uchar *rledat_next = rledat + xsize * 2;
					const int z_ofs = 3 - z;
					MFILE_CAPACITY_AT_PTR_OK_OR_FAIL(rledat_next + z_ofs);
					interleaverow2(fptr, rledat, z_ofs, xsize);
					rledat = rledat_next;
					fptr += xsize * 4;
				}
			}

		}
#undef MFILE_CAPACITY_AT_PTR_OK_OR_FAIL
fail_uncompressed:
		if (!ibuf) {
			return NULL;
		}
	}

	if (bpp == 1) {
		uchar *rect;

		if (image.zsize == 1) {
			rect = (uchar *) ibuf->rect;
			for (size_t x = (size_t)ibuf->x * (size_t)ibuf->y; x > 0; x--) {
				rect[0] = 255;
				rect[1] = rect[2] = rect[3];
				rect += 4;
			}
		}
		else if (image.zsize == 2) {
			/* grayscale with alpha */
			rect = (uchar *) ibuf->rect;
			for (size_t x = (size_t)ibuf->x * (size_t)ibuf->y; x > 0; x--) {
				rect[0] = rect[2];
				rect[1] = rect[2] = rect[3];
				rect += 4;
			}
		}
		else if (image.zsize == 3) {
			/* add alpha */
			rect = (uchar *) ibuf->rect;
			for (size_t x = (size_t)ibuf->x * (size_t)ibuf->y; x > 0; x--) {
				rect[0] = 255;
				rect += 4;
			}
		}

	}
	else {  /* bpp == 2 */

		if (image.zsize == 1) {
			fbase = ibuf->rect_float;
			for (size_t x = (size_t)ibuf->x * (size_t)ibuf->y; x > 0; x--) {
				fbase[0] = 1;
				fbase[1] = fbase[2] = fbase[3];
				fbase += 4;
			}
		}
		else if (image.zsize == 2) {
			/* grayscale with alpha */
			fbase = ibuf->rect_float;
			for (size_t x = (size_t)ibuf->x * (size_t)ibuf->y; x > 0; x--) {
				fbase[0] = fbase[2];
				fbase[1] = fbase[2] = fbase[3];
				fbase += 4;
			}
		}
		else if (image.zsize == 3) {
			/* add alpha */
			fbase = ibuf->rect_float;
			for (size_t x = (size_t)ibuf->x * (size_t)ibuf->y; x > 0; x--) {
				fbase[0] = 1;
				fbase += 4;
			}
		}

		if (flags & IB_rect) {
			IMB_rect_from_float(ibuf);
		}

	}

	if (dirty_flag) {
		fprintf(stderr, "longimagedata: corrupt file content (%d)\n", dirty_flag);
	}
	ibuf->ftype = IMB_FTYPE_IMAGIC;

	test_endian_zbuf(ibuf);

	if (ibuf->rect) {
		IMB_convert_rgba_to_abgr(ibuf);
	}

	return(ibuf);
}

/* static utility functions for longimagedata */

static void interleaverow(uchar *lptr, const uchar *cptr, int z, int n)
{
	lptr += z;
	while (n--) {
		*lptr = *cptr++;
		lptr += 4;
	}
}

static void interleaverow2(float *lptr, const uchar *cptr, int z, int n)
{
	lptr += z;
	while (n--) {
		*lptr = ((cptr[0] << 8) | (cptr[1] << 0)) / (float)0xFFFF;
		cptr += 2;
		lptr += 4;
	}
}

static int expandrow2(
        float *optr, const float *optr_end,
        const uchar *iptr, const uchar *iptr_end, int z)
{
	ushort pixel, count;
	float pixel_f;

#define EXPAND_CAPACITY_AT_INPUT_OK_OR_FAIL(iptr_next) \
	if (UNLIKELY(iptr_next > iptr_end)) { goto fail; }

#define EXPAND_CAPACITY_AT_OUTPUT_OK_OR_FAIL(optr_next) \
	if (UNLIKELY(optr_next > optr_end)) { goto fail; }

	optr += z;
	optr_end += z;
	while (1) {
		const uchar *iptr_next = iptr + 2;
		EXPAND_CAPACITY_AT_INPUT_OK_OR_FAIL(iptr_next);
		pixel = (iptr[0] << 8) | (iptr[1] << 0);
		iptr = iptr_next;

		if (!(count = (pixel & 0x7f)) )
			return false;
		const float *optr_next = optr + count;
		EXPAND_CAPACITY_AT_OUTPUT_OK_OR_FAIL(optr_next);
		if (pixel & 0x80) {
			iptr_next = iptr + (count * 2);
			EXPAND_CAPACITY_AT_INPUT_OK_OR_FAIL(iptr_next);
			while (count >= 8) {
				optr[0 * 4] = ((iptr[0] << 8) | (iptr[1] << 0)) / (float)0xFFFF;
				optr[1 * 4] = ((iptr[2] << 8) | (iptr[3] << 0)) / (float)0xFFFF;
				optr[2 * 4] = ((iptr[4] << 8) | (iptr[5] << 0)) / (float)0xFFFF;
				optr[3 * 4] = ((iptr[6] << 8) | (iptr[7] << 0)) / (float)0xFFFF;
				optr[4 * 4] = ((iptr[8] << 8) | (iptr[9] << 0)) / (float)0xFFFF;
				optr[5 * 4] = ((iptr[10] << 8) | (iptr[11] << 0)) / (float)0xFFFF;
				optr[6 * 4] = ((iptr[12] << 8) | (iptr[13] << 0)) / (float)0xFFFF;
				optr[7 * 4] = ((iptr[14] << 8) | (iptr[15] << 0)) / (float)0xFFFF;
				optr += 8 * 4;
				iptr += 8 * 2;
				count -= 8;
			}
			while (count--) {
				*optr = ((iptr[0] << 8) | (iptr[1] << 0)) / (float)0xFFFF;
				iptr += 2;
				optr += 4;
			}
			BLI_assert(iptr == iptr_next);
		}
		else {
			iptr_next = iptr + 2;
			EXPAND_CAPACITY_AT_INPUT_OK_OR_FAIL(iptr_next);
			pixel_f = ((iptr[0] << 8) | (iptr[1] << 0)) / (float)0xFFFF;
			iptr = iptr_next;

			while (count >= 8) {
				optr[0 * 4] = pixel_f;
				optr[1 * 4] = pixel_f;
				optr[2 * 4] = pixel_f;
				optr[3 * 4] = pixel_f;
				optr[4 * 4] = pixel_f;
				optr[5 * 4] = pixel_f;
				optr[6 * 4] = pixel_f;
				optr[7 * 4] = pixel_f;
				optr += 8 * 4;
				count -= 8;
			}
			while (count--) {
				*optr = pixel_f;
				optr += 4;
			}
			BLI_assert(iptr == iptr_next);
		}
		BLI_assert(optr == optr_next);
	}
	return false;

#undef EXPAND_CAPACITY_AT_INPUT_OK_OR_FAIL
#undef EXPAND_CAPACITY_AT_OUTPUT_OK_OR_FAIL
fail:
	return DIRTY_FLAG_ENCODING;
}

static int expandrow(
        uchar *optr, const uchar *optr_end,
        const uchar *iptr, const uchar *iptr_end, int z)
{
	uchar pixel, count;

#define EXPAND_CAPACITY_AT_INPUT_OK_OR_FAIL(iptr_next) \
	if (UNLIKELY(iptr_next > iptr_end)) { goto fail; }

#define EXPAND_CAPACITY_AT_OUTPUT_OK_OR_FAIL(optr_next) \
	if (UNLIKELY(optr_next > optr_end)) { goto fail; }

	optr += z;
	optr_end += z;
	while (1) {
		const uchar *iptr_next = iptr + 1;
		EXPAND_CAPACITY_AT_INPUT_OK_OR_FAIL(iptr_next);
		pixel = *iptr;
		iptr = iptr_next;
		if (!(count = (pixel & 0x7f)) )
			return false;
		const uchar *optr_next = optr + ((int)count * 4);
		EXPAND_CAPACITY_AT_OUTPUT_OK_OR_FAIL(optr_next);

		if (pixel & 0x80) {
			iptr_next = iptr + count;
			EXPAND_CAPACITY_AT_INPUT_OK_OR_FAIL(iptr_next);
			while (count >= 8) {
				optr[0 * 4] = iptr[0];
				optr[1 * 4] = iptr[1];
				optr[2 * 4] = iptr[2];
				optr[3 * 4] = iptr[3];
				optr[4 * 4] = iptr[4];
				optr[5 * 4] = iptr[5];
				optr[6 * 4] = iptr[6];
				optr[7 * 4] = iptr[7];
				optr += 8 * 4;
				iptr += 8;
				count -= 8;
			}
			while (count--) {
				*optr = *iptr++;
				optr += 4;
			}
			BLI_assert(iptr == iptr_next);
		}
		else {
			iptr_next = iptr + 1;
			EXPAND_CAPACITY_AT_INPUT_OK_OR_FAIL(iptr_next);
			pixel = *iptr++;
			while (count >= 8) {
				optr[0 * 4] = pixel;
				optr[1 * 4] = pixel;
				optr[2 * 4] = pixel;
				optr[3 * 4] = pixel;
				optr[4 * 4] = pixel;
				optr[5 * 4] = pixel;
				optr[6 * 4] = pixel;
				optr[7 * 4] = pixel;
				optr += 8 * 4;
				count -= 8;
			}
			while (count--) {
				*optr = pixel;
				optr += 4;
			}
			BLI_assert(iptr == iptr_next);
		}
		BLI_assert(optr == optr_next);
	}

	return false;

#undef EXPAND_CAPACITY_AT_INPUT_OK_OR_FAIL
#undef EXPAND_CAPACITY_AT_OUTPUT_OK_OR_FAIL
fail:
	return DIRTY_FLAG_ENCODING;
}

/**
 * Copy an array of ints to an iris image file.
 * Each int represents one pixel.  xsize and ysize specify the dimensions of
 * the pixel array.  zsize specifies what kind of image file to
 * write out.  if zsize is 1, the luminance of the pixels are
 * calculated, and a single channel black and white image is saved.
 * If zsize is 3, an RGB image file is saved.  If zsize is 4, an
 * RGBA image file is saved.
 *
 * Added: zbuf write
 */

static int output_iris(uint *lptr, int xsize, int ysize, int zsize, const char *name, int *zptr)
{
	FILE *outf;
	IMAGE *image;
	int tablen, y, z, pos, len = 0;
	uint *starttab, *lengthtab;
	uchar *rlebuf;
	uint *lumbuf;
	int rlebuflen, goodwrite;

	goodwrite = 1;
	outf = BLI_fopen(name, "wb");
	if (!outf) return 0;

	tablen = ysize * zsize * sizeof(int);

	image = (IMAGE *)MEM_mallocN(sizeof(IMAGE), "iris image");
	starttab = (uint *)MEM_mallocN(tablen, "iris starttab");
	lengthtab = (uint *)MEM_mallocN(tablen, "iris lengthtab");
	rlebuflen = 1.05 * xsize + 10;
	rlebuf = (uchar *)MEM_mallocN(rlebuflen, "iris rlebuf");
	lumbuf = (uint *)MEM_mallocN(xsize * sizeof(int), "iris lumbuf");

	memset(image, 0, sizeof(IMAGE));
	image->imagic = IMAGIC;
	image->type = RLE(1);
	if (zsize > 1)
		image->dim = 3;
	else
		image->dim = 2;
	image->xsize = xsize;
	image->ysize = ysize;
	image->zsize = zsize;
	image->min = 0;
	image->max = 255;
	goodwrite *= writeheader(outf, image);
	fseek(outf, HEADER_SIZE + (2 * tablen), SEEK_SET);
	pos = HEADER_SIZE + (2 * tablen);

	for (y = 0; y < ysize; y++) {
		for (z = 0; z < zsize; z++) {

			if (zsize == 1) {
				lumrow((uchar *)lptr, (uchar *)lumbuf, xsize);
				len = compressrow((uchar *)lumbuf, rlebuf, CHANOFFSET(z), xsize);
			}
			else {
				if (z < 4) {
					len = compressrow((uchar *)lptr, rlebuf, CHANOFFSET(z), xsize);
				}
				else if (z < 8 && zptr) {
					len = compressrow((uchar *)zptr, rlebuf, CHANOFFSET(z - 4), xsize);
				}
			}
			if (len > rlebuflen) {
				fprintf(stderr, "output_iris: rlebuf is too small - bad poop\n");
				exit(1);
			}
			goodwrite *= fwrite(rlebuf, len, 1, outf);
			starttab[y + z * ysize] = pos;
			lengthtab[y + z * ysize] = len;
			pos += len;
		}
		lptr += xsize;
		if (zptr) zptr += xsize;
	}

	fseek(outf, HEADER_SIZE, SEEK_SET);
	goodwrite *= writetab(outf, starttab, tablen);
	goodwrite *= writetab(outf, lengthtab, tablen);
	MEM_freeN(image);
	MEM_freeN(starttab);
	MEM_freeN(lengthtab);
	MEM_freeN(rlebuf);
	MEM_freeN(lumbuf);
	fclose(outf);
	if (goodwrite)
		return 1;
	else {
		fprintf(stderr, "output_iris: not enough space for image!!\n");
		return 0;
	}
}

/* static utility functions for output_iris */

static void lumrow(uchar *rgbptr, uchar *lumptr, int n)
{
	lumptr += CHANOFFSET(0);
	while (n--) {
		*lumptr = ILUM(rgbptr[OFFSET_R], rgbptr[OFFSET_G], rgbptr[OFFSET_B]);
		lumptr += 4;
		rgbptr += 4;
	}
}

static int compressrow(uchar *lbuf, uchar *rlebuf, int z, int cnt)
{
	uchar *iptr, *ibufend, *sptr, *optr;
	short todo, cc;
	int count;

	lbuf += z;
	iptr = lbuf;
	ibufend = iptr + cnt * 4;
	optr = rlebuf;

	while (iptr < ibufend) {
		sptr = iptr;
		iptr += 8;
		while ((iptr < ibufend) && ((iptr[-8] != iptr[-4]) || (iptr[-4] != iptr[0])))
			iptr += 4;
		iptr -= 8;
		count = (iptr - sptr) / 4;
		while (count) {
			todo = count > 126 ? 126 : count;
			count -= todo;
			*optr++ = 0x80 | todo;
			while (todo > 8) {
				optr[0] = sptr[0 * 4];
				optr[1] = sptr[1 * 4];
				optr[2] = sptr[2 * 4];
				optr[3] = sptr[3 * 4];
				optr[4] = sptr[4 * 4];
				optr[5] = sptr[5 * 4];
				optr[6] = sptr[6 * 4];
				optr[7] = sptr[7 * 4];

				optr += 8;
				sptr += 8 * 4;
				todo -= 8;
			}
			while (todo--) {
				*optr++ = *sptr;
				sptr += 4;
			}
		}
		sptr = iptr;
		cc = *iptr;
		iptr += 4;
		while ( (iptr < ibufend) && (*iptr == cc) )
			iptr += 4;
		count = (iptr - sptr) / 4;
		while (count) {
			todo = count > 126 ? 126 : count;
			count -= todo;
			*optr++ = todo;
			*optr++ = cc;
		}
	}
	*optr++ = 0;
	return optr - (uchar *)rlebuf;
}

int imb_saveiris(struct ImBuf *ibuf, const char *name, int flags)
{
	short zsize;
	int ret;

	zsize = (ibuf->planes + 7) >> 3;
	if (flags & IB_zbuf &&  ibuf->zbuf != NULL) zsize = 8;

	IMB_convert_rgba_to_abgr(ibuf);
	test_endian_zbuf(ibuf);

	ret = output_iris(ibuf->rect, ibuf->x, ibuf->y, zsize, name, ibuf->zbuf);

	/* restore! Quite clumsy, 2 times a switch... maybe better a malloc ? */
	IMB_convert_rgba_to_abgr(ibuf);
	test_endian_zbuf(ibuf);

	return(ret);
}
