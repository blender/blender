/*
 *	 Cineon image file format library routines.
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

#include "logImageCore.h"

#include <time.h>				 /* strftime() */
#include <math.h>
/* Makes rint consistent in Windows and Linux: */
#define rint(x) floor(x+0.5)

#ifdef WIN32
#include <winsock.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/param.h>
#endif

#if defined(__hpux)
/* These are macros in hpux */
#ifdef htonl
#undef htonl
#undef htons
#undef ntohl
#undef ntohs
#endif
unsigned int htonl(h) unsigned int h; { return(h); }
unsigned short htons(h) unsigned short h; { return(h); }
unsigned int ntohl(n) unsigned int n; { return(n); }
unsigned short ntohs(n) unsigned short n; { return(n); }
#endif
	
	
/* obscure LogImage conversion */
/* from 10 bit int to 0.0 - 1.0 */
/* magic numbers left intact */
static double
convertTo(int inp, int white, float gamma) {
	/*	return pow(pow(10.0, ((inp - white) * 0.002 / 0.6)), gamma); */
	return pow(10.0, (inp - white) * gamma * 0.002 / 0.6);
}

static double
convertFrom(double inp, int white, float gamma) {
	return white + log10(inp) / (gamma * 0.002 / 0.6);
}

/* set up the 10 bit to 8 bit and 8 bit to 10 bit tables */
void
setupLut(LogImageFile *logImage) {

	int i;
	double f_black;
	double scale;

	f_black = convertTo(logImage->params.blackPoint, logImage->params.whitePoint, logImage->params.gamma);
	scale = 255.0 / (1.0 - f_black);

	for (i = 0; i <= logImage->params.blackPoint; ++i) {
		logImage->lut10[i] = 0;
	}
	for (; i < logImage->params.whitePoint; ++i) {
		double f_i;
		f_i = convertTo(i, logImage->params.whitePoint, logImage->params.gamma);
		logImage->lut10[i] = (int)rint(scale * (f_i - f_black));
	}
	for (; i < 1024; ++i) {
		logImage->lut10[i] = 255;
	}

	for (i = 0; i < 256; ++i) {
		double f_i = f_black + (i / 255.0) * (1.0 - f_black);
		logImage->lut8[i] = convertFrom(f_i, logImage->params.whitePoint, logImage->params.gamma);
	}
}

/* set up the 10 bit to 16 bit and 16 bit to 10 bit tables */
void
setupLut16(LogImageFile *logImage) {

	int i;
	double f_black;
	double scale;

	f_black = convertTo(logImage->params.blackPoint, logImage->params.whitePoint, logImage->params.gamma);
	scale = 65535.0 / (1.0 - f_black);

	for (i = 0; i <= logImage->params.blackPoint; ++i) {
		logImage->lut10_16[i] = 0;
	}
	for (; i < logImage->params.whitePoint; ++i) {
		double f_i;
		f_i = convertTo(i, logImage->params.whitePoint, logImage->params.gamma);
		logImage->lut10_16[i] = (int)rint(scale * (f_i - f_black));
	}
	for (; i < 1024; ++i) {
		logImage->lut10_16[i] = 65535;
	}

	for (i = 0; i < 65536; ++i) {
		double f_i = f_black + (i / 65535.0) * (1.0 - f_black);
		logImage->lut16_16[i] = convertFrom(f_i, logImage->params.whitePoint, logImage->params.gamma);
	}
}

/* how many longwords to hold this many pixels? */
int
pixelsToLongs(int numPixels) {
	return (numPixels + 2) / 3;
}

/* byte reversed float */

typedef union {
	U32 i;
	R32 f;
} Hack;

R32
htonf(R32 f) {
	Hack hack;
	hack.f = f;
	hack.i = htonl(hack.i);
	return hack.f;
}

R32
ntohf(R32 f) {
	Hack hack;
	hack.f = f;
	hack.i = ntohl(hack.i);
	return hack.f;
}

#define UNDEF_FLOAT 0x7F800000

R32
undefined() {
	Hack hack;
	hack.i = UNDEF_FLOAT;
	return hack.f;
}

/* reverse an endian-swapped U16 */
U16
reverseU16(U16 value) {

	union {
		U16 whole;
		char part[2];
	} buff;
	char temp;
	buff.whole = value;
	temp = buff.part[0];
	buff.part[0] = buff.part[1];
	buff.part[1] = temp;
	return buff.whole;
}

/* reverse an endian-swapped U32 */
U32
reverseU32(U32 value) {

	union {
		U32 whole;
		char part[4];
	} buff;
	char temp;
	buff.whole = value;
	temp = buff.part[0];
	buff.part[0] = buff.part[3];
	buff.part[3] = temp;
	temp = buff.part[1];
	buff.part[1] = buff.part[2];
	buff.part[2] = temp;
	return buff.whole;
}

/* reverse an endian-swapped R32 */
R32
reverseR32(R32 value) {

	union {
		R32 whole;
		char part[4];
	} buff;
	char temp;
	buff.whole = value;
	temp = buff.part[0];
	buff.part[0] = buff.part[3];
	buff.part[3] = temp;
	temp = buff.part[1];
	buff.part[1] = buff.part[2];
	buff.part[2] = temp;
	return buff.whole;
}

#if 0
/* bytes per line for images packed 3 10 bit pixels to 32 bits, 32 bit aligned */
int
bytesPerLine_10_4(int numPixels) {
	return ((numPixels + 2) / 3) * 4;
}

void
seekLine_noPadding(LogImageFile* logImage, int lineNumber) {
	int fileOffset = bytesPerLine_10_4(lineNumber * logImage->width * logImage->depth);
	int filePos = logImage->imageOffset + fileOffset;
	if (fseek(logImage->file, filePos, SEEK_SET) != 0) {
		/* complain? */
	}
}

void
seekLine_padding(LogImageFile* logImage, int lineNumber) {
	int fileOffset = lineNumber * bytesPerLine_10_4(logImage->width * logImage->depth);
	int filePos = logImage->imageOffset + fileOffset;
	if (fseek(logImage->file, filePos, SEEK_SET) != 0) {
		/* complain? */
	}
}
#endif
