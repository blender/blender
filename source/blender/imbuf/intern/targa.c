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

/** \file blender/imbuf/intern/targa.c
 *  \ingroup imbuf
 */


#ifdef WIN32
#  include <io.h>
#endif

#include "BLI_utildefines.h"
#include "BLI_fileops.h"

#include "MEM_guardedalloc.h"

#include "imbuf.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_filetype.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

/* this one is only def-ed once, strangely... related to GS? */
#define GSS(x) (((uchar *)(x))[1] << 8 | ((uchar *)(x))[0])

/***/

typedef struct TARGA {
	unsigned char numid;
	unsigned char maptyp;
	unsigned char imgtyp;
	short maporig;
	short mapsize;
	unsigned char mapbits;
	short xorig;
	short yorig;
	short xsize;
	short ysize;
	unsigned char pixsize;
	unsigned char imgdes;
} TARGA;

/***/

static int tga_out1(unsigned int data, FILE *file)
{
	uchar *p;

	p = (uchar *) &data;
	if (putc(p[0], file) == EOF) return EOF;
	return ~EOF;
}

static int tga_out2(unsigned int data, FILE *file)
{
	uchar *p;

	p = (uchar *) &data;
	if (putc(p[0], file) == EOF) return EOF;
	if (putc(p[1], file) == EOF) return EOF;
	return ~EOF;
}


static int tga_out3(unsigned int data, FILE *file)
{
	uchar *p;

	p = (uchar *) &data;
	if (putc(p[2], file) == EOF) return EOF;
	if (putc(p[1], file) == EOF) return EOF;
	if (putc(p[0], file) == EOF) return EOF;
	return ~EOF;
}


static int tga_out4(unsigned int data, FILE *file)
{
	uchar *p;

	p = (uchar *) &data;
	/* order = bgra */
	if (putc(p[2], file) == EOF) return EOF;
	if (putc(p[1], file) == EOF) return EOF;
	if (putc(p[0], file) == EOF) return EOF;
	if (putc(p[3], file) == EOF) return EOF;
	return ~EOF;
}

static bool makebody_tga(ImBuf *ibuf, FILE *file, int (*out)(unsigned int, FILE *))
{
	register int last, this;
	register int copy, bytes;
	register unsigned int *rect, *rectstart, *temp;
	int y;

	for (y = 0; y < ibuf->y; y++) {
		bytes = ibuf->x - 1;
		rectstart = rect = ibuf->rect + (y * ibuf->x);
		last = *rect++;
		this = *rect++;
		copy = last ^ this;
		while (bytes > 0) {
			if (copy) {
				do {
					last = this;
					this = *rect++;
					if (last == this) {
						if (this == rect[-3]) { /* three the same? */
							bytes--;        /* set bytes */
							break;
						}
					}
				} while (--bytes != 0);

				copy = rect - rectstart;
				copy--;
				if (bytes) copy -= 2;

				temp = rect;
				rect = rectstart;

				while (copy) {
					last = copy;
					if (copy >= 128) last = 128;
					copy -= last;
					if (fputc(last - 1, file) == EOF) return 0;
					do {
						if (out(*rect++, file) == EOF) return 0;
					} while (--last != 0);
				}
				rectstart = rect;
				rect = temp;
				last = this;

				copy = 0;
			}
			else {
				while (*rect++ == this) {       /* seek for first different byte */
					if (--bytes == 0) break;    /* oor end of line */
				}
				rect--;
				copy = rect - rectstart;
				rectstart = rect;
				bytes--;
				this = *rect++;

				while (copy) {
					if (copy > 128) {
						if (fputc(255, file) == EOF) return 0;
						copy -= 128;
					}
					else {
						if (copy == 1) {
							if (fputc(0, file) == EOF) return 0;
						}
						else if (fputc(127 + copy, file) == EOF) return 0;
						copy = 0;
					}
					if (out(last, file) == EOF) return 0;
				}
				copy = 1;
			}
		}
	}
	return 1;
}

static bool dumptarga(struct ImBuf *ibuf, FILE *file)
{
	int size;
	uchar *rect;

	if (ibuf == NULL) return 0;
	if (ibuf->rect == NULL) return 0;

	size = ibuf->x * ibuf->y;
	rect = (uchar *) ibuf->rect;

	if (ibuf->planes <= 8) {
		while (size > 0) {
			if (putc(*rect, file) == EOF) return 0;
			size--;
			rect += 4;
		}
	}
	else if (ibuf->planes <= 16) {
		while (size > 0) {
			putc(rect[0], file);
			if (putc(rect[1], file) == EOF) return 0;
			size--;
			rect += 4;
		}
	}
	else if (ibuf->planes <= 24) {
		while (size > 0) {
			putc(rect[2], file);
			putc(rect[1], file);
			if (putc(rect[0], file) == EOF) return 0;
			size--;
			rect += 4;
		}
	}
	else if (ibuf->planes <= 32) {
		while (size > 0) {
			putc(rect[2], file);
			putc(rect[1], file);
			putc(rect[0], file);
			if (putc(rect[3], file) == EOF) return 0;
			size--;
			rect += 4;
		}
	}
	else {
		return 0;
	}

	return 1;
}


int imb_savetarga(struct ImBuf *ibuf, const char *name, int flags)
{
	char buf[20] = {0};
	FILE *fildes;
	bool ok = false;

	(void)flags; /* unused */

	buf[16] = (ibuf->planes + 0x7) & ~0x7;
	if (ibuf->planes > 8) {
		buf[2] = 10;
	}
	else {
		buf[2] = 11;
	}

	if (ibuf->foptions.flag & RAWTGA) buf[2] &= ~8;

	buf[8] = 0;
	buf[9] = 0;
	buf[10] = 0;
	buf[11] = 0;

	buf[12] = ibuf->x & 0xff;
	buf[13] = ibuf->x >> 8;
	buf[14] = ibuf->y & 0xff;
	buf[15] = ibuf->y >> 8;

	/* Don't forget to indicate that your 32 bit
	 * targa uses 8 bits for the alpha channel! */
	if (ibuf->planes == 32) {
		buf[17] |= 0x08;
	}
	fildes = BLI_fopen(name, "wb");
	if (!fildes) return 0;

	if (fwrite(buf, 1, 18, fildes) != 18) {
		fclose(fildes);
		return 0;
	}

	if (ibuf->foptions.flag & RAWTGA) {
		ok = dumptarga(ibuf, fildes);
	}
	else {
		switch ((ibuf->planes + 7) >> 3) {
			case 1:
				ok = makebody_tga(ibuf, fildes, tga_out1);
				break;
			case 2:
				ok = makebody_tga(ibuf, fildes, tga_out2);
				break;
			case 3:
				ok = makebody_tga(ibuf, fildes, tga_out3);
				break;
			case 4:
				ok = makebody_tga(ibuf, fildes, tga_out4);
				break;
		}
	}

	fclose(fildes);
	return ok;
}


static int checktarga(TARGA *tga, const unsigned char *mem)
{
	tga->numid = mem[0];
	tga->maptyp = mem[1];
	tga->imgtyp = mem[2];

	tga->maporig = GSS(mem + 3);
	tga->mapsize = GSS(mem + 5);
	tga->mapbits = mem[7];
	tga->xorig = GSS(mem + 8);
	tga->yorig = GSS(mem + 10);
	tga->xsize = GSS(mem + 12);
	tga->ysize = GSS(mem + 14);
	tga->pixsize = mem[16];
	tga->imgdes = mem[17];

	if (tga->maptyp > 1) return 0;
	switch (tga->imgtyp) {
		case 1:     /* raw cmap */
		case 2:     /* raw rgb */
		case 3:     /* raw b&w */
		case 9:     /* cmap */
		case 10:        /* rgb */
		case 11:        /* b&w */
			break;
		default:
			return 0;
	}
	if (tga->mapsize && tga->mapbits > 32) return 0;
	if (tga->xsize <= 0) return 0;
	if (tga->ysize <= 0) return 0;
	if (tga->pixsize > 32) return 0;
	if (tga->pixsize == 0) return 0;
	return 1;
}

int imb_is_a_targa(const unsigned char *buf)
{
	TARGA tga;

	return checktarga(&tga, buf);
}

static void complete_partial_load(struct ImBuf *ibuf, unsigned int *rect)
{
	int size = (ibuf->x * ibuf->y) - (rect - ibuf->rect);
	if (size) {
		printf("decodetarga: incomplete file, %.1f%% missing\n", 100 * ((float)size / (ibuf->x * ibuf->y)));

		/* not essential but makes displaying partially rendered TGA's less ugly  */
		memset(rect, 0, size);
	}
	else {
		/* shouldn't happen */
		printf("decodetarga: incomplete file, all pixels written\n");
	}
}

static void decodetarga(struct ImBuf *ibuf, const unsigned char *mem, size_t mem_size, int psize)
{
	const unsigned char *mem_end = mem + mem_size;
	int count, col, size;
	unsigned int *rect;
	uchar *cp = (uchar *) &col;

	if (ibuf == NULL) return;
	if (ibuf->rect == NULL) return;

	size = ibuf->x * ibuf->y;
	rect = ibuf->rect;

	/* set alpha */
	cp[0] = 0xff;
	cp[1] = cp[2] = 0;

	while (size > 0) {
		count = *mem++;

		if (mem > mem_end)
			goto partial_load;

		if (count >= 128) {
			/*if (count == 128) printf("TARGA: 128 in file !\n");*/
			count -= 127;

			if (psize & 2) {
				if (psize & 1) {
					/* order = bgra */
					cp[0] = mem[3];
					cp[1] = mem[0];
					cp[2] = mem[1];
					cp[3] = mem[2];
					/*col = (mem[3] << 24) + (mem[0] << 16) + (mem[1] << 8) + mem[2];*/
					mem += 4;
				}
				else {
					cp[1] = mem[0];
					cp[2] = mem[1];
					cp[3] = mem[2];
					/*col = 0xff000000 + (mem[0] << 16) + (mem[1] << 8) + mem[2];*/
					mem += 3;
				}
			}
			else {
				if (psize & 1) {
					cp[0] = mem[0];
					cp[1] = mem[1];
					mem += 2;
				}
				else {
					col = *mem++;
				}
			}

			size -= count;
			if (size >= 0) {
				while (count > 0) {
					*rect++ = col;
					count--;
				}
			}
		}
		else {
			count++;
			size -= count;
			if (size >= 0) {
				while (count > 0) {
					if (psize & 2) {
						if (psize & 1) {
							/* order = bgra */
							cp[0] = mem[3];
							cp[1] = mem[0];
							cp[2] = mem[1];
							cp[3] = mem[2];
							/*col = (mem[3] << 24) + (mem[0] << 16) + (mem[1] << 8) + mem[2];*/
							mem += 4;
						}
						else {
							cp[1] = mem[0];
							cp[2] = mem[1];
							cp[3] = mem[2];
							/*col = 0xff000000 + (mem[0] << 16) + (mem[1] << 8) + mem[2];*/
							mem += 3;
						}
					}
					else {
						if (psize & 1) {
							cp[0] = mem[0];
							cp[1] = mem[1];
							mem += 2;
						}
						else {
							col = *mem++;
						}
					}
					*rect++ = col;
					count--;

					if (mem > mem_end)
						goto partial_load;
				}

				if (mem > mem_end)
					goto partial_load;
			}
		}
	}
	if (size) {
		printf("decodetarga: count would overwrite %d pixels\n", -size);
	}
	return;

partial_load:
	complete_partial_load(ibuf, rect);
}

static void ldtarga(struct ImBuf *ibuf, const unsigned char *mem, size_t mem_size, int psize)
{
	const unsigned char *mem_end = mem + mem_size;
	int col, size;
	unsigned int *rect;
	uchar *cp = (uchar *) &col;

	if (ibuf == NULL) return;
	if (ibuf->rect == NULL) return;

	size = ibuf->x * ibuf->y;
	rect = ibuf->rect;

	/* set alpha */
	cp[0] = 0xff;
	cp[1] = cp[2] = 0;

	while (size > 0) {
		if (mem > mem_end)
			goto partial_load;

		if (psize & 2) {
			if (psize & 1) {
				/* order = bgra */
				cp[0] = mem[3];
				cp[1] = mem[0];
				cp[2] = mem[1];
				cp[3] = mem[2];
				/*col = (mem[3] << 24) + (mem[0] << 16) + (mem[1] << 8) + mem[2];*/
				mem += 4;
			}
			else {
				/* set alpha for 24 bits colors */
				cp[1] = mem[0];
				cp[2] = mem[1];
				cp[3] = mem[2];
				/*col = 0xff000000 + (mem[0] << 16) + (mem[1] << 8) + mem[2];*/
				mem += 3;
			}
		}
		else {
			if (psize & 1) {
				cp[0] = mem[0];
				cp[1] = mem[1];
				mem += 2;
			}
			else {
				col = *mem++;
			}
		}
		*rect++ = col;
		size--;
	}
	return;

partial_load:
	complete_partial_load(ibuf, rect);
}


ImBuf *imb_loadtarga(const unsigned char *mem, size_t mem_size, int flags, char colorspace[IM_MAX_SPACE])
{
	TARGA tga;
	struct ImBuf *ibuf;
	int count, size;
	unsigned int *rect, *cmap = NULL /*, mincol = 0*/, cmap_max = 0;
	int32_t cp_data;
	uchar *cp = (uchar *) &cp_data;

	if (checktarga(&tga, mem) == 0) {
		return NULL;
	}

	colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_BYTE);

	if (flags & IB_test) ibuf = IMB_allocImBuf(tga.xsize, tga.ysize, tga.pixsize, 0);
	else ibuf = IMB_allocImBuf(tga.xsize, tga.ysize, (tga.pixsize + 0x7) & ~0x7, IB_rect);

	if (ibuf == NULL) return NULL;
	ibuf->ftype = IMB_FTYPE_TGA;
	if (tga.imgtyp < 4)
		ibuf->foptions.flag |= RAWTGA;
	mem = mem + 18 + tga.numid;

	cp[0] = 0xff;
	cp[1] = cp[2] = 0;

	if (tga.mapsize) {
		/* load color map */
		/*mincol = tga.maporig;*/ /*UNUSED*/
		cmap_max = tga.mapsize;
		cmap = MEM_callocN(sizeof(unsigned int) * cmap_max, "targa cmap");

		for (count = 0; count < cmap_max; count++) {
			switch (tga.mapbits >> 3) {
				case 4:
					cp[0] = mem[3];
					cp[1] = mem[0];
					cp[2] = mem[1];
					cp[3] = mem[2];
					mem += 4;
					break;
				case 3:
					cp[1] = mem[0];
					cp[2] = mem[1];
					cp[3] = mem[2];
					mem += 3;
					break;
				case 2:
					cp[1] = mem[1];
					cp[0] = mem[0];
					mem += 2;
					break;
				case 1:
					cp_data = *mem++;
					break;
			}
			cmap[count] = cp_data;
		}

		size = 0;
		for (int cmap_index = cmap_max - 1; cmap_index > 0; cmap_index >>= 1) {
			size++;
		}
		ibuf->planes = size;

		if (tga.mapbits != 32) {    /* set alpha bits  */
			cmap[0] &= BIG_LONG(0x00ffffffl);
		}
	}

	if (flags & IB_test) {
		if (cmap) {
			MEM_freeN(cmap);
		}
		return ibuf;
	}

	if (tga.imgtyp != 1 && tga.imgtyp != 9) { /* happens sometimes (beuh) */
		if (cmap) {
			MEM_freeN(cmap);
			cmap = NULL;
		}
	}

	switch (tga.imgtyp) {
		case 1:
		case 2:
		case 3:
			if (tga.pixsize <= 8) ldtarga(ibuf, mem, mem_size, 0);
			else if (tga.pixsize <= 16) ldtarga(ibuf, mem, mem_size, 1);
			else if (tga.pixsize <= 24) ldtarga(ibuf, mem, mem_size, 2);
			else if (tga.pixsize <= 32) ldtarga(ibuf, mem, mem_size, 3);
			break;
		case 9:
		case 10:
		case 11:
			if (tga.pixsize <= 8) decodetarga(ibuf, mem, mem_size, 0);
			else if (tga.pixsize <= 16) decodetarga(ibuf, mem, mem_size, 1);
			else if (tga.pixsize <= 24) decodetarga(ibuf, mem, mem_size, 2);
			else if (tga.pixsize <= 32) decodetarga(ibuf, mem, mem_size, 3);
			break;
	}

	if (cmap) {
		/* apply color map */
		rect = ibuf->rect;
		for (size = ibuf->x * ibuf->y; size > 0; --size, ++rect) {
			int cmap_index = *rect;
			if (cmap_index >= 0 && cmap_index < cmap_max) {
				*rect = cmap[cmap_index];
			}
		}

		MEM_freeN(cmap);
	}

	if (tga.pixsize == 16) {
		unsigned int col;
		rect = ibuf->rect;
		for (size = ibuf->x * ibuf->y; size > 0; --size, ++rect) {
			col = *rect;
			cp = (uchar *)rect;
			mem = (uchar *)&col;

			cp[3] = ((mem[1] << 1) & 0xf8);
			cp[2] = ((mem[0] & 0xe0) >> 2) + ((mem[1] & 0x03) << 6);
			cp[1] = ((mem[0] << 3) & 0xf8);
			cp[1] += cp[1] >> 5;
			cp[2] += cp[2] >> 5;
			cp[3] += cp[3] >> 5;
			cp[0] = 0xff;
		}
		ibuf->planes = 24;
	}

	if (tga.imgtyp == 3 || tga.imgtyp == 11) {
		uchar *crect;
		unsigned int *lrect, col;

		crect = (uchar *) ibuf->rect;
		lrect = (unsigned int *) ibuf->rect;

		for (size = ibuf->x * ibuf->y; size > 0; size--) {
			col = *lrect++;

			crect[0] = 255;
			crect[1] = crect[2] = crect[3] = col;
			crect += 4;
		}
	}

	if (tga.imgdes & 0x20) {
		IMB_flipy(ibuf);
	}

	if (ibuf->rect)
		IMB_convert_rgba_to_abgr(ibuf);

	return ibuf;
}
