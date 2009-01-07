/*
 *	 Cineon image file format library definitions.
 *	 Cineon and DPX common structures.
 *
 *	 This header file contains private details.
 *	 User code should generally use cineonlib.h and dpxlib.h only.
 *	 Hmm. I thought the two formats would have more in common!
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
 *	 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _LOG_IMAGE_CORE_H_
#define _LOG_IMAGE_CORE_H_

#include <stdio.h>
#include "logImageLib.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "BLO_sys_types.h" // for intptr_t support
#undef ntohl
#undef htonl
typedef int (GetRowFn)(LogImageFile* logImage, unsigned short* row, int lineNum);
typedef int (SetRowFn)(LogImageFile* logImage, const unsigned short* row, int lineNum);
typedef void (CloseFn)(LogImageFile* logImage);

struct _Log_Image_File_t_
{
	/* specified in header */
	int width;
	int height;
	int depth;
	int bitsPerPixel;
	int imageOffset;

	/* file buffer, measured in longwords (4 byte) */
	int lineBufferLength;
	unsigned int* lineBuffer;

	/* pixel buffer, holds 10 bit pixel values */
	unsigned short* pixelBuffer;
	int pixelBufferUsed;

	/* io stuff */
	FILE* file;
	int reading;
	int fileYPos;

	/* byte conversion stuff */
	LogImageByteConversionParameters params;
#if 0
	float gamma;
	int blackPoint;
	int whitePoint;
#endif
	unsigned char lut10[1024];
	unsigned short lut8[256];

	unsigned short lut10_16[1024];
	unsigned short lut16_16[65536];

	/* pixel access functions */
	GetRowFn* getRow;
	SetRowFn* setRow;
	CloseFn* close;
	
	unsigned char *membuffer;
	uintptr_t membuffersize;
	unsigned char *memcursor;
};

void setupLut(LogImageFile*);
void setupLut16(LogImageFile*);

int pixelsToLongs(int numPixels);

/* typedefs used in original docs */
/* note size assumptions! */

typedef unsigned int U32;
typedef unsigned short U16;
typedef unsigned char U8;
typedef signed int S32;
typedef float R32;
typedef char ASCII;

R32 htonf(R32 f);
R32 ntohf(R32 f);
R32 undefined();
U16 reverseU16(U16 value);
U32 reverseU32(U32 value);
R32 reverseR32(R32 value);

#ifdef __cplusplus
}
#endif

#endif /* _LOG_IMAGE_CORE_H_ */
