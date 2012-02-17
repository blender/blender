/** \file blender/imbuf/intern/cineon/logImageLib.h
 *  \ingroup imbcineon
 */
/*
 *	 Common library definitions for Cineon and DPX image files.
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
 *	 or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU General Public License
 *	 for more details.
 *
 *	 You should have received a copy of the GNU General Public License
 *	 along with this program; if not, write to the Free Software
 *	 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __LOGIMAGELIB_H__
#define __LOGIMAGELIB_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Image structure. You don't care what this is.
 */

typedef struct _Log_Image_File_t_ LogImageFile;

/*
 * Magic numbers for normal and byte-swapped Cineon and Dpx files
 */

#define CINEON_FILE_MAGIC 0x802A5FD7
#define DPX_FILE_MAGIC 0x53445058

/*
 * Image 8 bit <-> 10 bit conversion parameters.
 */

typedef struct {
	float gamma;
	int blackPoint;
	int whitePoint;
	int doLogarithm;
} LogImageByteConversionParameters;

/* int functions return 0 for OK */

void logImageSetVerbose(int);

LogImageFile* logImageOpenFromMem(unsigned char *buffer, unsigned int size, int cineon);
LogImageFile* logImageOpen(const char* filename, int cineon);
int logImageGetSize(const LogImageFile* logImage, int* xsize, int* ysize, int* channels);
LogImageFile* logImageCreate(const char* filename, int cineon, int xsize, int ysize, int channels);

/* get / set header block NYI */
int logImageGetHeader(LogImageFile*, int*, void**);
int logImageSetHeader(LogImageFile*, int, void*);

/* byte conversion routines for mapping logImage (usually) 10 bit values to 8 bit */
/* see Kodak docs for details... */

int logImageGetByteConversionDefaults(LogImageByteConversionParameters* params);
int logImageGetByteConversion(const LogImageFile* logImage, LogImageByteConversionParameters* params);
int logImageSetByteConversion(LogImageFile* logImage, const LogImageByteConversionParameters* params);

/* get/set scanline of converted bytes */
int logImageGetRowBytes(LogImageFile* logImage, unsigned short* row, int y);
int logImageSetRowBytes(LogImageFile* logImage, const unsigned short* row, int y);

/* closes file and deletes data */
void logImageClose(LogImageFile* logImage);

/* read file and dump header info */
void logImageDump(const char* filename);

#ifdef __cplusplus
}
#endif

#endif /* __LOGIMAGELIB_H__ */
