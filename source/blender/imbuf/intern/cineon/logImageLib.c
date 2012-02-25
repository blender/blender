/** \file blender/imbuf/intern/cineon/logImageLib.c
 *  \ingroup imbcineon
 */
/*
 *	 Cineon and DPX image file format library routines.
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

#include "cineonlib.h"
#include "dpxlib.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>				 /* strftime() */
#include <sys/types.h>
#ifdef WIN32
#include <winsock.h>
#else
#include <netinet/in.h>	 /* htonl() */
#endif
#include <string.h>			 /* memset */

#define MIN_GAMMA 0.01
#define MAX_GAMMA 99.9
#define DEFAULT_GAMMA 1.0
#define DEFAULT_BLACK_POINT 95
#define DEFAULT_WHITE_POINT 685

void
logImageSetVerbose(int verbosity)
{
	cineonSetVerbose(verbosity);
	dpxSetVerbose(verbosity);
}

LogImageFile*
logImageOpen(const char* filename, int cineon)
{
	if (cineon) {
		return cineonOpen(filename);
	} else {
		return dpxOpen(filename);
	}
	return 0;
}

LogImageFile*
logImageOpenFromMem(unsigned char *buffer, unsigned int size, int cineon)
{
	if (cineon) {
		return cineonOpenFromMem(buffer, size);
	} else {
		return dpxOpenFromMem(buffer, size);
	}
	return 0;
}

LogImageFile*
logImageCreate(const char* filename, int cineon, int width, int height, int depth)
{
	if (cineon) {
		return cineonCreate(filename, width, height, depth);
	} else {
		return dpxCreate(filename, width, height, depth);
	}
	return 0;
}

int
logImageGetSize(const LogImageFile* logImage, int* width, int* height, int* depth)
{
	*width = logImage->width;
	*height = logImage->height;
	*depth = logImage->depth;
	return 0;
}

int
logImageGetByteConversionDefaults(LogImageByteConversionParameters* params)
{
	params->gamma = DEFAULT_GAMMA;
	params->blackPoint = DEFAULT_BLACK_POINT;
	params->whitePoint = DEFAULT_WHITE_POINT;
	params->doLogarithm = 0;
	return 0;
}

int
logImageGetByteConversion(const LogImageFile* logImage, LogImageByteConversionParameters* params)
{
	params->gamma = logImage->params.gamma;
	params->blackPoint = logImage->params.blackPoint;
	params->whitePoint = logImage->params.whitePoint;
	params->doLogarithm = 0;
	return 0;
}

int
logImageSetByteConversion(LogImageFile* logImage, const LogImageByteConversionParameters* params)
{
	if ((params->gamma >= MIN_GAMMA) &&
			(params->gamma <= MAX_GAMMA) &&
			(params->blackPoint >= 0) &&
			(params->blackPoint < params->whitePoint) &&
			(params->whitePoint <= 1023)) {
		logImage->params.gamma = params->gamma;
		logImage->params.blackPoint = params->blackPoint;
		logImage->params.whitePoint = params->whitePoint;
		logImage->params.doLogarithm = params->doLogarithm;
		setupLut16(logImage);
		return 0;
	}
	return 1;
}

int
logImageGetRowBytes(LogImageFile* logImage, unsigned short* row, int y)
{
	return logImage->getRow(logImage, row, y);
}

int
logImageSetRowBytes(LogImageFile* logImage, const unsigned short* row, int y)
{
	return logImage->setRow(logImage, row, y);
}

void
logImageClose(LogImageFile* logImage)
{
	logImage->close(logImage);
}

void
logImageDump(const char* filename)
{

	U32 magic;

	FILE* foo = fopen(filename, "rb");
	if (foo == 0) {
		return;
	}

	if (fread(&magic, sizeof(magic), 1, foo) == 0) {
		fclose(foo);
		return;
	}

	fclose(foo);

	if (magic == ntohl(CINEON_FILE_MAGIC)) {
#if 0
		cineonDump(filename);
#endif
	} else if (magic == ntohl(DPX_FILE_MAGIC)) {
		dpxDump(filename);
	}
}
