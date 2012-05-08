/** \file blender/imbuf/intern/cineon/dpxlib.h
 *  \ingroup imbcineon
 */
/*
 *	 DPX image file format library definitions.
 *
 *	 Copyright 1999 - 2002 David Hodson <hodsond@acm.org>
 *
 *	 This program is free software; you can redistribute it and/or modify it
 *	 under the terms of the GNU General Public License as published by the Free
 *	 Software Foundation; either version 2 of the License, or (at your option)
 *	 any later version.
 *
 *	 This program is distributed in the hope that it will be useful, but
 *	 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *	 or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU General Public License
 *	 for more details.
 *
 *	 You should have received a copy of the GNU General Public License
 *	 along with this program; if not, write to the Free Software
 *	 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __DPXLIB_H__
#define __DPXLIB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "logImageCore.h"

typedef struct _Log_Image_File_t_ DpxFile;

/* int functions return 0 for OK */

void dpxSetVerbose(int);

DpxFile* dpxOpen(const char* filename);
DpxFile* dpxCreate(const char* filename, int xsize, int ysize, int channels);
DpxFile* dpxOpenFromMem(unsigned char *buffer, unsigned int size);
int dpxIsMemFileCineon(void *buffer);

/* get/set scanline of converted bytes */
int dpxGetRowBytes(DpxFile* dpx, unsigned short* row, int y);
int dpxSetRowBytes(DpxFile* dpx, const unsigned short* row, int y);

/* closes file and deletes data */
void dpxClose(DpxFile* dpx);

/* dumps file to stdout */
void dpxDump(const char* filename);

#ifdef __cplusplus
}
#endif

#endif /* __DPXLIB_H__ */
