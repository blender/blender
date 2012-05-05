/*
 *	 Cineon image file format library definitions.
 *	 Also handles DPX files (almost)
 *
 *	 Copyright 1999,2000,2001 David Hodson <hodsond@acm.org>
 *
 *	 This program is free software; you can redistribute it and/or modify it
 *	 under the terms of the GNU General Public License as published by the Free
 *	 Software Foundation; either version 2 of the License, or (at your option)
 *	 any later version.
 *
 *	 This program is distributed in the hope that it will be useful, but
 *	 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *	 or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *	 for more details.
 *
 *	 You should have received a copy of the GNU General Public License
 *	 along with this program; if not, write to the Free Software
 *	 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __CINEONLIB_H__
#define __CINEONLIB_H__

/** \file blender/imbuf/intern/cineon/cineonlib.h
 *  \ingroup imbcineon
 */

#include "logImageCore.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Cineon image structure. You don't care what this is.
 */

typedef struct _Log_Image_File_t_ CineonFile;

/* int functions return 0 for OK */

void cineonSetVerbose(int);

CineonFile* cineonOpenFromMem(unsigned char *mem, unsigned int size);

CineonFile* cineonOpen(const char* filename);
int cineonGetSize(const CineonFile* cineon, int* xsize, int* ysize, int* channels);
CineonFile* cineonCreate(const char* filename, int xsize, int ysize, int channels);
int cineonIsMemFileCineon(unsigned char *mem);

/* get/set scanline of converted bytes */
int cineonGetRowBytes(CineonFile* cineon, unsigned short* row, int y);
int cineonSetRowBytes(CineonFile* cineon, const unsigned short* row, int y);

/* get/set scanline of unconverted shorts */
int cineonGetRow(CineonFile* cineon, unsigned short* row, int y);
int cineonSetRow(CineonFile* cineon, const unsigned short* row, int y);

/* closes file and deletes data */
void cineonClose(CineonFile* cineon);

#ifdef __cplusplus
}
#endif

#endif /* __CINEONLIB_H__ */
