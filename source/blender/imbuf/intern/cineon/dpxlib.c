/*
 *	 Dpx image file format library routines.
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

/** \file blender/imbuf/intern/cineon/dpxlib.c
 *  \ingroup imbcineon
 */

#include "dpxfile.h"
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
#include "cin_debug_stuff.h"
#include "logmemfile.h"
#include "BLI_fileops.h"

static void
fillDpxChannelInfo(DpxFile* dpx, DpxChannelInformation* chan, int des) {

	(void)dpx; /* unused */
	
	chan->signage = 0;
	chan->ref_low_data = htonl(0);
	chan->ref_low_quantity = htonf(0.0);
	chan->ref_high_data = htonl(1023);
	chan->ref_high_quantity = htonf(2.046);
	chan->designator1 = des;
	chan->transfer_characteristics = 0;
	chan->colourimetry = 0;
	chan->bits_per_pixel = 10;
	chan->packing = htons(1);
	chan->encoding = 0;
	chan->data_offset = 0;
	chan->line_padding = htonl(0);
	chan->channel_padding = htonl(0);
	chan->description[0] = 0;
}

static void
dumpDpxChannelInfo(DpxChannelInformation* chan) {
	d_printf("	Signage %ld", (intptr_t)ntohl(chan->signage));
	d_printf("	Ref low data %ld\n", (intptr_t)ntohl(chan->ref_low_data));
	d_printf("	Ref low quantity %f\n", ntohf(chan->ref_low_quantity));
	d_printf("	Ref high data %ld\n", (intptr_t)ntohl(chan->ref_high_data));
	d_printf("	Ref high quantity %f\n", ntohf(chan->ref_high_quantity));
	d_printf("	Designator1: %d,", chan->designator1);
	d_printf("	Bits per pixel %d\n", chan->bits_per_pixel);
	d_printf("	Packing: %d,", ntohs(chan->packing));
	d_printf("	Data Offset: %ld,", (intptr_t)ntohl(chan->data_offset));
}

static void
fillDpxFileInfo(
	DpxFile* dpx, DpxFileInformation* fileInfo, const char* filename) {

	time_t fileClock;
	struct tm* fileTime;

	/* Note: always write files in network order */
	/* By the spec, it shouldn't matter, but ... */

	fileInfo->magic_num = htonl(DPX_FILE_MAGIC);
	fileInfo->offset = htonl(dpx->imageOffset);
	strcpy(fileInfo->vers, "v1.0");
	fileInfo->file_size = htonl(dpx->imageOffset +
		pixelsToLongs(dpx->height * dpx->width * dpx->depth) * 4);
	fileInfo->ditto_key = 0;
	fileInfo->gen_hdr_size = htonl(
		sizeof(DpxFileInformation) +
		sizeof(DpxImageInformation) +
		sizeof(DpxOriginationInformation));
	fileInfo->ind_hdr_size = htonl(sizeof(DpxMPIInformation));
	fileInfo->user_data_size = 0;
	strncpy(fileInfo->file_name, filename, 99);
	fileInfo->file_name[99] = 0;

	fileClock = time(0);
	fileTime = localtime(&fileClock);
	strftime(fileInfo->create_date, 24, "%Y:%m:%d:%H:%M:%S%Z", fileTime);
	/* Question: is %Z in strftime guaranteed to return 3 chars? */
	fileInfo->create_date[23] = 0;

	strcpy(fileInfo->creator, "David's DPX writer");
	fileInfo->project[0] = 0;
	fileInfo->copyright[0] = 0;
	fileInfo->key = 0xFFFFFFFF; /* same in any byte order */
}

static void
dumpDpxFileInfo(DpxFileInformation* fileInfo) {
	d_printf("\n--File Information--\n");
	d_printf("Magic: %8.8lX\n", (unsigned long)ntohl(fileInfo->magic_num));
	d_printf("Image Offset %ld\n", (intptr_t)ntohl(fileInfo->offset));
	d_printf("Version \"%s\"\n", fileInfo->vers);
	d_printf("File size %ld\n", (intptr_t)ntohl(fileInfo->file_size));
	d_printf("Ditto key %ld\n", (intptr_t)ntohl(fileInfo->ditto_key));
	d_printf("Generic Header size %ld\n", (intptr_t)ntohl(fileInfo->gen_hdr_size));
	d_printf("Industry Header size %ld\n", (intptr_t)ntohl(fileInfo->ind_hdr_size));
	d_printf("User Data size %ld\n", (intptr_t)ntohl(fileInfo->user_data_size));
	d_printf("File name \"%s\"\n", fileInfo->file_name);
	d_printf("Creation date \"%s\"\n", fileInfo->create_date);
	d_printf("Creator \"%s\"\n", fileInfo->creator);
	d_printf("Project \"%s\"\n", fileInfo->project);
	d_printf("Copyright \"%s\"\n", fileInfo->copyright);
	d_printf("Key %ld\n", (intptr_t)ntohl(fileInfo->key));
}

static void
fillDpxImageInfo(
	DpxFile* dpx, DpxImageInformation* imageInfo) {
	imageInfo->orientation = 0;
	imageInfo->channels_per_image = htons(1);
	imageInfo->pixels_per_line = htonl(dpx->width);
	imageInfo->lines_per_image = htonl(dpx->height);

	if (dpx->depth == 1) {
		fillDpxChannelInfo(dpx, &imageInfo->channel[0], 0);

	}
	else if (dpx->depth == 3) {
		fillDpxChannelInfo(dpx, &imageInfo->channel[0], 50);
	}
}

static void
dumpDpxImageInfo(DpxImageInformation* imageInfo) {

	int n;
	int i;
	d_printf("\n--Image Information--\n");
	d_printf("Image orientation %d,", ntohs(imageInfo->orientation));
	n = ntohs(imageInfo->channels_per_image);
	d_printf("Channels %d\n", n);
	d_printf("Pixels per line %ld\n", (intptr_t)ntohl(imageInfo->pixels_per_line));
	d_printf("Lines per image %ld\n", (intptr_t)ntohl(imageInfo->lines_per_image));
	for (i = 0; i < n; ++i) {
		d_printf("	--Channel %d--\n", i);
		dumpDpxChannelInfo(&imageInfo->channel[i]);
	}
}

static void
fillDpxOriginationInfo(
	DpxFile* dpx, DpxOriginationInformation* originInfo, DpxFileInformation* fileInfo)
{
	/* unused */
	(void)dpx;
	(void)originInfo;
	(void)fileInfo;
}

static void
dumpDpxOriginationInfo(DpxOriginationInformation* originInfo) {
	d_printf("\n--Origination Information--\n");
	d_printf("X offset %ld\n", (intptr_t)ntohl(originInfo->x_offset));
	d_printf("Y offset %ld\n", (intptr_t)ntohl(originInfo->y_offset));
	d_printf("X centre %f\n", ntohf(originInfo->x_centre));
	d_printf("Y centre %f\n", ntohf(originInfo->y_centre));
	d_printf("Original X %ld\n", (intptr_t)ntohl(originInfo->x_original_size));
	d_printf("Original Y %ld\n", (intptr_t)ntohl(originInfo->y_original_size));
	d_printf("File name \"%s\"\n", originInfo->file_name);
	d_printf("Creation time \"%s\"\n", originInfo->creation_time);
	d_printf("Input device \"%s\"\n", originInfo->input_device);
	d_printf("Serial number \"%s\"\n", originInfo->input_serial_number);
}

static void
initDpxMainHeader(DpxFile* dpx, DpxMainHeader* header, const char* shortFilename) {
	memset(header, 0, sizeof(DpxMainHeader));
	fillDpxFileInfo(dpx, &header->fileInfo, shortFilename);
	fillDpxImageInfo(dpx, &header->imageInfo);
	fillDpxOriginationInfo(dpx, &header->originInfo, &header->fileInfo);
#if 0
	fillDpxMPIInfo(dpx, &header->filmHeader);
#endif
}

static void
dumpDpxMainHeader(DpxMainHeader* header) {
	dumpDpxFileInfo(&header->fileInfo);
	dumpDpxImageInfo(&header->imageInfo);
	dumpDpxOriginationInfo(&header->originInfo);
#if 0
	dumpDpxMPIInformation(&header->filmHeader);
#endif
}

static int verbose = 0;
void
dpxSetVerbose(int verbosity) {
	verbose = verbosity;
}

static void
verboseMe(DpxFile* dpx) {

	d_printf("size %d x %d x %d\n", dpx->width, dpx->height, dpx->depth);
	d_printf("ImageStart %d, lineBufferLength %d, implied length %d\n",
		dpx->imageOffset, dpx->lineBufferLength * 4,
		dpx->imageOffset + pixelsToLongs(dpx->width * dpx->depth * dpx->height) * 4);
}

int
dpxGetRowBytes(DpxFile* dpx, unsigned short* row, int y) {

	/* Note: this code is bizarre because DPX files can wrap */
	/* packed longwords across line boundaries!!!! */

	size_t readLongs;
	unsigned int longIndex;
	int numPixels = dpx->width * dpx->depth;
	int pixelIndex;

	/* only seek if not reading consecutive lines */
	/* this is not quite right yet, need to account for leftovers */
	if (y != dpx->fileYPos) {
		int lineOffset = pixelsToLongs(y * dpx->width * dpx->depth) * 4;
		if (verbose) d_printf("Seek in getRowBytes\n");
		if (logimage_fseek(dpx, dpx->imageOffset + lineOffset, SEEK_SET) != 0) {
			if (verbose) d_printf("Couldn't seek to line %d at %d\n", y, dpx->imageOffset + lineOffset);
			return 1;
		}
		dpx->fileYPos = y;
	}

	/* read enough longwords */
	readLongs = pixelsToLongs(numPixels - dpx->pixelBufferUsed);
	if (logimage_fread(dpx->lineBuffer, 4, readLongs, dpx) != readLongs) {
		if (verbose) d_printf("Couldn't read line %d length %d\n", y, (int)readLongs * 4);
		return 1;
	}
	++dpx->fileYPos;

	/* convert longwords to pixels */
	pixelIndex = dpx->pixelBufferUsed;

	/* this is just strange */
	if (dpx->depth == 1) {
		for (longIndex = 0; longIndex < readLongs; ++longIndex) {
			unsigned int t = ntohl(dpx->lineBuffer[longIndex]);
			dpx->pixelBuffer[pixelIndex] = t & 0x3ff;
			t = t >> 10;
			dpx->pixelBuffer[pixelIndex+1] = t & 0x3ff;
			t = t >> 10;
			dpx->pixelBuffer[pixelIndex+2] = t & 0x3ff;
			pixelIndex += 3;
		}
	}
	else /* if (dpx->depth == 3) */ {
		for (longIndex = 0; longIndex < readLongs; ++longIndex) {
			unsigned int t = ntohl(dpx->lineBuffer[longIndex]);
			t = t >> 2;
			dpx->pixelBuffer[pixelIndex+2] = t & 0x3ff;
			t = t >> 10;
			dpx->pixelBuffer[pixelIndex+1] = t & 0x3ff;
			t = t >> 10;
			dpx->pixelBuffer[pixelIndex] = t & 0x3ff;
			pixelIndex += 3;
		}
	}
	dpx->pixelBufferUsed = pixelIndex;

	/* extract required pixels */
	for (pixelIndex = 0; pixelIndex < numPixels; ++pixelIndex) {
		if (dpx->params.doLogarithm)
			row[pixelIndex] = dpx->lut10_16[dpx->pixelBuffer[pixelIndex]];
		else
			row[pixelIndex] = dpx->pixelBuffer[pixelIndex] << 6;
	}

	/* save remaining pixels */
	while (pixelIndex < dpx->pixelBufferUsed) {
		dpx->pixelBuffer[pixelIndex - numPixels] = dpx->pixelBuffer[pixelIndex];
		++pixelIndex;
	}
	dpx->pixelBufferUsed -= numPixels;

	/* done! */
	return 0;
}

int
dpxSetRowBytes(DpxFile* dpx, const unsigned short* row, int y) {

	/* Note: this code is bizarre because DPX files can wrap */
	/* packed longwords across line boundaries!!!! */

	size_t writeLongs;
	int longIndex;
	int numPixels = dpx->width * dpx->depth;
	int pixelIndex;
	int pixelIndex2;

	/* only seek if not reading consecutive lines */
	/* this is not quite right yet */
	if (y != dpx->fileYPos) {
		int lineOffset = pixelsToLongs(y * dpx->width * dpx->depth) * 4;
		if (verbose) d_printf("Seek in getRowBytes\n");
		if (logimage_fseek(dpx, dpx->imageOffset + lineOffset, SEEK_SET) != 0) {
			if (verbose) d_printf("Couldn't seek to line %d at %d\n", y, dpx->imageOffset + lineOffset);
			return 1;
		}
		dpx->fileYPos = y;
	}

	/* put new pixels into pixelBuffer */
	for (pixelIndex = 0; pixelIndex < numPixels; ++pixelIndex) {
		if (dpx->params.doLogarithm)
			dpx->pixelBuffer[dpx->pixelBufferUsed + pixelIndex] = dpx->lut16_16[row[pixelIndex]];
		else
			dpx->pixelBuffer[dpx->pixelBufferUsed + pixelIndex] = row[pixelIndex] >> 6;
	}
	dpx->pixelBufferUsed += numPixels;

	/* pack into longwords */
	writeLongs = dpx->pixelBufferUsed / 3;
	/* process whole line at image end */
	if (dpx->fileYPos == (dpx->height - 1)) {
		writeLongs = pixelsToLongs(dpx->pixelBufferUsed);
	}
	pixelIndex = 0;
	if (dpx->depth == 1) {
		for (longIndex = 0; longIndex < writeLongs; ++longIndex) {
			unsigned int t = dpx->pixelBuffer[pixelIndex] |
					(dpx->pixelBuffer[pixelIndex+1] << 10) |
					(dpx->pixelBuffer[pixelIndex+2] << 20);
			dpx->lineBuffer[longIndex] = htonl(t);
			pixelIndex += 3;
		}
	}
	else {
		for (longIndex = 0; longIndex < writeLongs; ++longIndex) {
			unsigned int t = dpx->pixelBuffer[pixelIndex+2] << 2 |
					(dpx->pixelBuffer[pixelIndex+1] << 12) |
					(dpx->pixelBuffer[pixelIndex] << 22);
			dpx->lineBuffer[longIndex] = htonl(t);
			pixelIndex += 3;
		}
	}

	/* write them */
	if (fwrite(dpx->lineBuffer, 4, writeLongs, dpx->file) != writeLongs) {
		if (verbose) d_printf("Couldn't write line %d length %d\n", y, (int)writeLongs * 4);
		return 1;
	}
	++dpx->fileYPos;

	/* save remaining pixels */
	pixelIndex2 = 0;
	while (pixelIndex < dpx->pixelBufferUsed) {
		dpx->pixelBuffer[pixelIndex2] = dpx->pixelBuffer[pixelIndex];
		++pixelIndex;
		++pixelIndex2;
	}
	dpx->pixelBufferUsed = pixelIndex2;

	return 0;
}

#define LFMEMFILE	0
#define LFREALFILE	1

static DpxFile* 
intern_dpxOpen(int mode, const char* bytestuff, int bufsize) {

	DpxMainHeader header;
	const char *filename = bytestuff;
	DpxFile* dpx = (DpxFile*)malloc(sizeof(DpxFile));
	
	if (dpx == 0) {
		if (verbose) d_printf("Failed to malloc dpx file structure.\n");
		return 0;
	}

	/* for close routine */
	dpx->file = 0;
	dpx->lineBuffer = 0;
	dpx->pixelBuffer = 0;

	if (mode == LFREALFILE) {
		filename = bytestuff;
		dpx->file = BLI_fopen(filename, "rb");
		if (dpx->file == 0) {	
			if (verbose) d_printf("Failed to open file \"%s\".\n", filename);
			dpxClose(dpx);
			return 0;
		}
		dpx->membuffer = 0;
		dpx->memcursor = 0;
		dpx->membuffersize = 0;
	}
	else if (mode == LFMEMFILE) {
		dpx->membuffer = (unsigned char *)bytestuff;
		dpx->memcursor = (unsigned char *)bytestuff;
		dpx->membuffersize = bufsize;
	}
	
	dpx->reading = 1;

	if (logimage_fread(&header, sizeof(header), 1, dpx) == 0) {
		if (verbose) d_printf("Not enough data for header in \"%s\".\n", filename);
		dpxClose(dpx);
		return 0;
	}

	/* let's assume dpx files are always network order */
	if (header.fileInfo.magic_num != ntohl(DPX_FILE_MAGIC)) {
		if (verbose) d_printf("Bad magic number %8.8lX in \"%s\".\n",
			(uintptr_t)ntohl(header.fileInfo.magic_num), filename);
		dpxClose(dpx);
		return 0;
	}

	if (ntohs(header.imageInfo.channel[0].packing) != 1) {
		if (verbose) d_printf("Unknown packing %d\n", header.imageInfo.channel[0].packing);
		dpxClose(dpx);
		return 0;
	}


	dpx->width = ntohl(header.imageInfo.pixels_per_line);
	dpx->height = ntohl(header.imageInfo.lines_per_image);
	dpx->depth = ntohs(header.imageInfo.channels_per_image);
	/* Another DPX vs Cineon wierdness */
	if (dpx->depth == 1) {
		switch (header.imageInfo.channel[0].designator1) {
		case 50: dpx->depth = 3; break;
		case 51: dpx->depth = 4; break;
		case 52: dpx->depth = 4; break;
		default: break;
		}
	}
	/* dpx->bitsPerPixel = 10; */
	dpx->bitsPerPixel = header.imageInfo.channel[0].bits_per_pixel;
	if (dpx->bitsPerPixel != 10) {
		if (verbose) d_printf("Don't support depth: %d\n", dpx->bitsPerPixel);
		dpxClose(dpx);
		return 0;
	}

	dpx->imageOffset = ntohl(header.fileInfo.offset);
	dpx->lineBufferLength = pixelsToLongs(dpx->width * dpx->depth);
	dpx->lineBuffer = malloc(dpx->lineBufferLength * 4);
	if (dpx->lineBuffer == 0) {
		if (verbose) d_printf("Couldn't malloc line buffer of size %d\n", dpx->lineBufferLength * 4);
		dpxClose(dpx);
		return 0;
	}

	/* could have 2 pixels left over */
	dpx->pixelBuffer = malloc((dpx->lineBufferLength * 3 + 2) * sizeof(unsigned short));
	if (dpx->pixelBuffer == 0) {
		if (verbose) d_printf("Couldn't malloc pixel buffer of size %d\n",
				(dpx->width * dpx->depth + 2 + 2) * (int)sizeof(unsigned short));
		dpxClose(dpx);
		return 0;
	}
	dpx->pixelBufferUsed = 0;

	if (logimage_fseek(dpx, dpx->imageOffset, SEEK_SET) != 0) {
		if (verbose) d_printf("Couldn't seek to image data start at %d\n", dpx->imageOffset);
		dpxClose(dpx);
		return 0;
	}
	dpx->fileYPos = 0;

	logImageGetByteConversionDefaults(&dpx->params);
	/* The SMPTE define this code:
	 *  0 - User-defined
	 *  1 - Printing density
	 *  2 - Linear
	 *  3 - Logarithmic
	 *  4 - Unspecified video
	 *  5 - SMPTE 240M
	 *  6 - CCIR 709-1
	 *  7 - CCIR 601-2 system B or G
	 *  8 - CCIR 601-2 system M
	 *  9 - NTSC composite video
	 *  10 - PAL composite video
	 *  11 - Z linear
	 *  12 - homogeneous
	 *
	 * Note that transfer_characteristics is U8, don't need
	 * check the byte order.
	 */
	
	switch (header.imageInfo.channel[0].transfer_characteristics) {
		case 1:
		case 2: /* linear */
			dpx->params.doLogarithm= 0;
			break;
		
		case 3:
			dpx->params.doLogarithm= 1;
			break;
		
		/* TODO - Unsupported, but for now just load them,
		 * colors may look wrong, but can solve color conversion later
		 */
		case 4: 
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
			if (verbose) d_printf("Un-supported Transfer Characteristics: %d using linear color conversion\n", header.imageInfo.channel[0].transfer_characteristics);
			dpx->params.doLogarithm= 0;
			break;
		default:
			if (verbose) d_printf("Un-supported Transfer Characteristics: %d\n", header.imageInfo.channel[0].transfer_characteristics);
			dpxClose(dpx);
			return 0;
			break;
	}
	setupLut(dpx);

	dpx->getRow = &dpxGetRowBytes;
	dpx->setRow = 0;
	dpx->close = &dpxClose;

	if (verbose) {
		verboseMe(dpx);
	}

	return dpx;
}

DpxFile* 
dpxOpen(const char *filename) {
	return intern_dpxOpen(LFREALFILE, filename, 0);
}

DpxFile* 
dpxOpenFromMem(unsigned char *buffer, unsigned int size) {
	return intern_dpxOpen(LFMEMFILE, (const char *) buffer, size);
}

int 
dpxIsMemFileCineon(void *buffer) {
	int magicnum = 0;
	magicnum = *((int*)buffer);
	if (magicnum == ntohl(DPX_FILE_MAGIC)) return 1;
	else return 0;
}

DpxFile*
dpxCreate(const char* filename, int width, int height, int depth) {

	/* Note: always write files in network order */
	/* By the spec, it shouldn't matter, but ... */

	DpxMainHeader header;
	const char* shortFilename = 0;

	DpxFile* dpx = (DpxFile*)malloc(sizeof(DpxFile));
	if (dpx == 0) {
		if (verbose) d_printf("Failed to malloc dpx file structure.\n");
		return 0;
	}

	memset(&header, 0, sizeof(header));

	/* for close routine */
	dpx->file = 0;
	dpx->lineBuffer = 0;
	dpx->pixelBuffer = 0;

	dpx->file = BLI_fopen(filename, "wb");
	if (dpx->file == 0) {
		if (verbose) d_printf("Couldn't open file %s\n", filename);
		dpxClose(dpx);
		return 0;
	}
	dpx->reading = 0;

	dpx->width = width;
	dpx->height = height;
	dpx->depth = depth;
	dpx->bitsPerPixel = 10;
	dpx->imageOffset = sizeof(DpxMainHeader);

	dpx->lineBufferLength = pixelsToLongs(dpx->width * dpx->depth);
	dpx->lineBuffer = malloc(dpx->lineBufferLength * 4);
	if (dpx->lineBuffer == 0) {
		if (verbose) d_printf("Couldn't malloc line buffer of size %d\n", dpx->lineBufferLength * 4);
		dpxClose(dpx);
		return 0;
	}

	dpx->pixelBuffer = malloc((dpx->lineBufferLength * 3 + 2) * sizeof(unsigned short));
	if (dpx->pixelBuffer == 0) {
		if (verbose) d_printf("Couldn't malloc pixel buffer of size %d\n",
				(dpx->width * dpx->depth + 2 + 2) * (int)sizeof(unsigned short));
		dpxClose(dpx);
		return 0;
	}
	dpx->pixelBufferUsed = 0;

	/* find trailing part of filename */
	shortFilename = strrchr(filename, '/');
	if (shortFilename == 0) {
		shortFilename = filename;
	}
	else {
		++shortFilename;
	}
	initDpxMainHeader(dpx, &header, shortFilename);
	logImageGetByteConversionDefaults(&dpx->params);
	/* Need set the file type before write the header!
	 *  2 - Linear
	 *  3 - Logarithmic
	 *
	 * Note that transfer characteristics is U8, don't need
	 * check the byte order.
	 */
	if (dpx->params.doLogarithm == 0)
		header.imageInfo.channel[0].transfer_characteristics= 2;
	else
		header.imageInfo.channel[0].transfer_characteristics= 3;

	if (fwrite(&header, sizeof(header), 1, dpx->file) == 0) {
		if (verbose) d_printf("Couldn't write image header\n");
		dpxClose(dpx);
		return 0;
	}
	dpx->fileYPos = 0;
	setupLut(dpx);

	dpx->getRow = 0;
	dpx->setRow = &dpxSetRowBytes;
	dpx->close = &dpxClose;

	return dpx;
}

void
dpxClose(DpxFile* dpx) {

	if (dpx == 0) {
		return;
	}

	if (dpx->file) {
		fclose(dpx->file);
		dpx->file = 0;
	}

	if (dpx->lineBuffer) {
		free(dpx->lineBuffer);
		dpx->lineBuffer = 0;
	}

	if (dpx->pixelBuffer) {
		free(dpx->pixelBuffer);
		dpx->pixelBuffer = 0;
	}

	free(dpx);
}

void
dpxDump(const char* filename) {

	DpxMainHeader header;
	FILE* file;

	file = BLI_fopen(filename, "rb");
	if (file == 0) {
		d_printf("Failed to open file \"%s\".\n", filename);
		return;
	}

	if (fread(&header, sizeof(header), 1, file) == 0) {
		d_printf("Not enough data for header in \"%s\".\n", filename);
		fclose(file);
		return;
	}

	fclose(file);
	dumpDpxMainHeader(&header);
}
