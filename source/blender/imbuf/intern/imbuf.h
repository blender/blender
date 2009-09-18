/**
 * imbuf.h (mar-2001 nzc)
 *
 * This header might have to become external...
 *
 * $Id$ 
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifndef IMBUF_H
#define IMBUF_H

#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h> 

#ifndef WIN32
#include <unistd.h> 
#endif

#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <math.h>

#ifndef WIN32
#include <sys/mman.h>
#endif

#if !defined(WIN32)
#define O_BINARY 0
#endif

#define SWAP_SHORT(x) (((x & 0xff) << 8) | ((x >> 8) & 0xff))
#define SWAP_LONG(x) (((x) << 24) | (((x) & 0xff00) << 8) | (((x) >> 8) & 0xff00) | (((x) >> 24) & 0xff))

#define ENDIAN_NOP(x) (x)

#if defined(__sgi) || defined(__sparc) || defined(__sparc__) || defined (__PPC__) || defined (__hppa__) || (defined (__APPLE__) && !defined(__LITTLE_ENDIAN__))
#define LITTLE_SHORT SWAP_SHORT
#define LITTLE_LONG SWAP_LONG
#define BIG_SHORT ENDIAN_NOP
#define BIG_LONG ENDIAN_NOP
#else
#define LITTLE_SHORT ENDIAN_NOP
#define LITTLE_LONG ENDIAN_NOP
#define BIG_SHORT SWAP_SHORT
#define BIG_LONG SWAP_LONG
#endif

#define malloc(x) MEM_mallocN(x, __FILE__)
#define free(x) MEM_freeN(x)
#define calloc(x,y) MEM_callocN((x)*(y), __FILE__)
#define freelist(x) BLI_freelistN(x)

#ifdef SHLIB
void *(*ib_calloc)();
#define calloc(x,y) ib_calloc((x),(y))
void *(*ib_malloc)();
#define malloc(x) ib_malloc(x)
void (*ib_free)();
#define free(x) ib_free(x)
void (*ib_memcpy)();
#define memcpy(x,y,z) ib_memcpy((x),(y),(z))
int (*ib_abs)();
#define abs(x) ib_abs(x)
void (*ib_fprin_tf)();
#define fprintf ib_fprin_tf
int (*ib_sprin_tf)();
#define sprintf ib_sprin_tf
void (*ib_prin_tf)();
#define printf ib_prin_tf
int (*ib_lseek)();
#define lseek(x,y,z) ib_lseek((x),(y),(z))
void *(*ib_mmap)();
#define mmap(u,v,w,x,y,z) ib_mmap((u),(v),(w),(x),(y),(z))
int (*ib_munmap)();
#define munmap(x,y) ib_munmap((x),(y))
int (*ib_open)();
#define open(x,y) ib_open((x),(y))
void (*ib_close)();
#define close(x) ib_close(x)
int (*ib_write)();
#define write(x,y,z) ib_write((x),(y),(z))
int (*ib_read)();
#define read(x,y,z) ib_read((x),(y),(z))
int (*ib_fchmod)();
#define fchmod(x,y) ib_fchmod((x),(y))
int (*ib_remove)();
#define remove(x) ib_remove(x)
size_t (*ib_strlen)();
#define strlen(x) ib_strlen(x)
int (*ib_isdigit)();
#define isdigit(x) ib_isdigit(x)
char *(*ib_strcpy)();
#define strcpy(x,y) ib_strcpy((x),(y))
int (*ib_atoi)();
#define atoi(x) ib_atoi(x)
char *(*ib_strcat)();
#define strcat(x,y) ib_strcat((x),(y))
int (*ib_stat)();
/* #define stat(x,y) ib_stat((x),(y)) */
FILE *ib_iob;
#define _iob ib_iob

#else

#define ib_stat stat

#endif /* SHLIB */


#define WIDTHB(x) (((x+15)>>4)<<1)

extern unsigned short *quadr;
extern float dyuvrgb[4][4];
extern float rgbdyuv[4][4];


typedef struct Adat
{
	unsigned short w, h;
	unsigned short type;
	unsigned short xorig, yorig;
	unsigned short pad;
	float gamma;
	float distort;
}Adat;

struct BitMapHeader
{
	unsigned short	w, h;		/* in pixels */
	unsigned short	x, y;
	char	nPlanes;
	char	masking;
	char	compression;
	char	pad1;
	unsigned short	transparentColor;
	char	xAspect, yAspect;
	short	pageWidth, pageHeight;
};

#endif	/* IMBUF_H */

