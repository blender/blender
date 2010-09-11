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
 *	 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "cineonlib.h"
#include "cineonfile.h"

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

static void
fillCineonFileInfo(CineonFile* cineon, CineonFileInformation* fileInfo, const char* filename) {

	time_t fileClock;
	struct tm* fileTime;

	fileInfo->magic_num = htonl(CINEON_FILE_MAGIC);
	fileInfo->image_offset = htonl(cineon->imageOffset);
	fileInfo->gen_hdr_size = htonl(
		sizeof(CineonFileInformation) +
		sizeof(CineonImageInformation) +
		sizeof(CineonFormatInformation) +
		sizeof(CineonOriginationInformation));
	fileInfo->ind_hdr_size = 0;
	fileInfo->user_data_size = 0;
	fileInfo->file_size = htonl(cineon->imageOffset + cineon->height * cineon->lineBufferLength);
	strcpy(fileInfo->vers, "V4.5");
	strncpy(fileInfo->file_name, filename, 99);
	fileInfo->file_name[99] = 0;

	fileClock = time(0);
	fileTime = localtime(&fileClock);
	strftime(fileInfo->create_date, 12, "%Y:%m:%d", fileTime);
	/* Question: is %Z in strftime guaranteed to return 3 chars? */
	strftime(fileInfo->create_time, 12, "%H:%M:%S%Z", fileTime);
	fileInfo->create_time[11] = 0;
}

static void
dumpCineonFileInfo(CineonFileInformation* fileInfo) {
	d_printf("\n--File Information--\n");
	d_printf("Magic: %8.8lX\n", (uintptr_t)ntohl(fileInfo->magic_num));
	d_printf("Image Offset %ld\n", (intptr_t)ntohl(fileInfo->image_offset));
	d_printf("Generic Header size %ld\n", (intptr_t)ntohl(fileInfo->gen_hdr_size));
	d_printf("Industry Header size %ld\n", (intptr_t)ntohl(fileInfo->ind_hdr_size));
	d_printf("User Data size %ld\n", (intptr_t)ntohl(fileInfo->user_data_size));
	d_printf("File size %ld\n", (intptr_t)ntohl(fileInfo->file_size));
	d_printf("Version \"%s\"\n", fileInfo->vers);
	d_printf("File name \"%s\"\n", fileInfo->file_name);
	d_printf("Creation date \"%s\"\n", fileInfo->create_date);
	d_printf("Creation time \"%s\"\n", fileInfo->create_time);
}

static void
fillCineonChannelInfo(CineonFile* cineon, CineonChannelInformation* chan, int des) {

	chan->designator1 = 0;
	chan->designator2 = des;
	chan->bits_per_pixel = 10;
	chan->pixels_per_line = htonl(cineon->width);
	chan->lines_per_image = htonl(cineon->height);
	chan->ref_low_data = htonl(0);
	chan->ref_low_quantity = htonf(0.0);
	chan->ref_high_data = htonl(1023);
	chan->ref_high_quantity = htonf(2.046);
}

static void
dumpCineonChannelInfo(CineonChannelInformation* chan) {
	d_printf("	Metric selector: %d", chan->designator1);
	switch (chan->designator1) {
		case 0: d_printf(" (Universal)\n"); break;
		default: d_printf(" (Vendor specific)\n"); break;
	}
	d_printf("	Metric: %d,", chan->designator2);
	switch (chan->designator2) {
		case 0: d_printf(" B&W (printing density?)\n"); break;
		case 1: d_printf(" Red printing density\n"); break;
		case 2: d_printf(" Green printing density\n"); break;
		case 3: d_printf(" Blue printing density\n"); break;
		case 4: d_printf(" Red CCIR XA/11\n"); break;
		case 5: d_printf(" Green CCIR XA/11\n"); break;
		case 6: d_printf(" Blue CCIR XA/11\n"); break;
		default: d_printf(" (unknown)\n"); break;
	}
	d_printf("	Bits per pixel %d\n", chan->bits_per_pixel);
	d_printf("	Pixels per line %ld\n", (intptr_t)ntohl(chan->pixels_per_line));
	d_printf("	Lines per image %ld\n", (intptr_t)ntohl(chan->lines_per_image));
	d_printf("	Ref low data %ld\n", (intptr_t)ntohl(chan->ref_low_data));
	d_printf("	Ref low quantity %f\n", ntohf(chan->ref_low_quantity));
	d_printf("	Ref high data %ld\n", (intptr_t)ntohl(chan->ref_high_data));
	d_printf("	Ref high quantity %f\n", ntohf(chan->ref_high_quantity));
}

static void
fillCineonImageInfo(CineonFile* cineon, CineonImageInformation* imageInfo) {

	imageInfo->orientation = 0;
	imageInfo->channels_per_image = cineon->depth;

	if (cineon->depth == 1) {
		fillCineonChannelInfo(cineon, &imageInfo->channel[0], 0);

	} else if (cineon->depth == 3) {
		fillCineonChannelInfo(cineon, &imageInfo->channel[0], 1);
		fillCineonChannelInfo(cineon, &imageInfo->channel[1], 2);
		fillCineonChannelInfo(cineon, &imageInfo->channel[2], 3);
	}

	imageInfo->white_point_x = htonf(undefined());
	imageInfo->white_point_y = htonf(undefined());
	imageInfo->red_primary_x = htonf(undefined());
	imageInfo->red_primary_y = htonf(undefined());
	imageInfo->green_primary_x = htonf(undefined());
	imageInfo->green_primary_y = htonf(undefined());
	imageInfo->blue_primary_x = htonf(undefined());
	imageInfo->blue_primary_y = htonf(undefined());

	strcpy(imageInfo->label, "David's Cineon writer.");

}

static void
dumpCineonImageInfo(CineonImageInformation* imageInfo) {

	int i;
	d_printf("\n--Image Information--\n");
	d_printf("Image orientation %d,", imageInfo->orientation);
	switch (imageInfo->orientation) {
		case 0: d_printf(" LRTB\n"); break;
		case 1: d_printf(" LRBT\n"); break;
		case 2: d_printf(" RLTB\n"); break;
		case 3: d_printf(" RLBT\n"); break;
		case 4: d_printf(" TBLR\n"); break;
		case 5: d_printf(" TBRL\n"); break;
		case 6: d_printf(" BTLR\n"); break;
		case 7: d_printf(" BTRL\n"); break;
		default: d_printf(" (unknown)\n"); break;
	}
	d_printf("Channels %d\n", imageInfo->channels_per_image);
	for (i = 0; i < imageInfo->channels_per_image; ++i) {
		d_printf("	--Channel %d--\n", i);
		dumpCineonChannelInfo(&imageInfo->channel[i]);
	}

	d_printf("White point x %f\n", ntohf(imageInfo->white_point_x));
	d_printf("White point y %f\n", ntohf(imageInfo->white_point_y));
	d_printf("Red primary x %f\n", ntohf(imageInfo->red_primary_x));
	d_printf("Red primary y %f\n", ntohf(imageInfo->red_primary_y));
	d_printf("Green primary x %f\n", ntohf(imageInfo->green_primary_x));
	d_printf("Green primary y %f\n", ntohf(imageInfo->green_primary_y));
	d_printf("Blue primary x %f\n", ntohf(imageInfo->blue_primary_x));
	d_printf("Blue primary y %f\n", ntohf(imageInfo->blue_primary_y));
	d_printf("Label \"%s\"\n", imageInfo->label);
}

static void
fillCineonFormatInfo(CineonFile* cineon, CineonFormatInformation* formatInfo) {

	formatInfo->interleave = 0;
	formatInfo->packing = 5;
	formatInfo->signage = 0;
	formatInfo->sense = 0;
	formatInfo->line_padding = htonl(0);
	formatInfo->channel_padding = htonl(0);
}

static void
dumpCineonFormatInfo(CineonFormatInformation* formatInfo) {
	d_printf("\n--Format Information--\n");
	d_printf("Interleave %d,", formatInfo->interleave);
	switch (formatInfo->interleave) {
		case 0: d_printf(" pixel interleave\n"); break;
		case 1: d_printf(" line interleave\n"); break;
		case 2: d_printf(" channel interleave\n"); break;
		default: d_printf(" (unknown)\n"); break;
	}
	d_printf("Packing %d,", formatInfo->packing);
	if (formatInfo->packing & 0x80) { 
		d_printf(" multi pixel,");
	} else {
		d_printf(" single pixel,");
	}
	switch (formatInfo->packing & 0x7F) {
		case 0: d_printf(" tight\n"); break;
		case 1: d_printf(" byte packed left\n"); break;
		case 2: d_printf(" byte packed right\n"); break;
		case 3: d_printf(" word packed left\n"); break;
		case 4: d_printf(" word packed right\n"); break;
		case 5: d_printf(" long packed left\n"); break;
		case 6: d_printf(" long packed right\n"); break;
		default: d_printf(" (unknown)\n"); break;
	}
	d_printf("Sign %d,", formatInfo->signage);
	if (formatInfo->signage) { 
		d_printf(" signed\n");
	} else {
		d_printf(" unsigned\n");
	}
	d_printf("Sense %d,", formatInfo->signage);
	if (formatInfo->signage) { 
		d_printf(" negative\n");
	} else {
		d_printf(" positive\n");
	}
	d_printf("End of line padding %ld\n", (intptr_t)ntohl(formatInfo->line_padding));
	d_printf("End of channel padding %ld\n", (intptr_t)ntohl(formatInfo->channel_padding));
}

static void
fillCineonOriginationInfo(CineonFile* cineon,
	CineonOriginationInformation* originInfo, CineonFileInformation* fileInfo) {

	originInfo->x_offset = htonl(0);
	originInfo->y_offset = htonl(0);
	strcpy(originInfo->file_name, fileInfo->file_name);
	strcpy(originInfo->create_date, fileInfo->create_date);
	strcpy(originInfo->create_time, fileInfo->create_time);
	strncpy(originInfo->input_device, "David's Cineon writer", 64);
	strncpy(originInfo->model_number, "Software", 32);
	strncpy(originInfo->serial_number, "001", 32);
	originInfo->x_input_samples_per_mm = htonf(undefined());
	originInfo->y_input_samples_per_mm =	htonf(undefined());
	/* this should probably be undefined, too */
	originInfo->input_device_gamma = htonf(1.0);
}

static void
dumpCineonOriginationInfo(CineonOriginationInformation* originInfo) {
	d_printf("\n--Origination Information--\n");
	d_printf("X offset %ld\n", (intptr_t)ntohl(originInfo->x_offset));
	d_printf("Y offset %ld\n", (intptr_t)ntohl(originInfo->y_offset));
	d_printf("File name \"%s\"\n", originInfo->file_name);
	d_printf("Creation date \"%s\"\n", originInfo->create_date);
	d_printf("Creation time \"%s\"\n", originInfo->create_time);
	d_printf("Input device \"%s\"\n", originInfo->input_device);
	d_printf("Model number \"%s\"\n", originInfo->model_number);
	d_printf("Serial number \"%s\"\n", originInfo->serial_number);
	d_printf("Samples per mm in x %f\n", ntohf(originInfo->x_input_samples_per_mm));
	d_printf("Samples per mm in y %f\n", ntohf(originInfo->y_input_samples_per_mm));
	d_printf("Input device gamma %f\n", ntohf(originInfo->input_device_gamma));
}

int
initCineonGenericHeader(CineonFile* cineon, CineonGenericHeader* header, const char* imagename) {

	fillCineonFileInfo(cineon, &header->fileInfo, imagename);
	fillCineonImageInfo(cineon, &header->imageInfo);
	fillCineonFormatInfo(cineon, &header->formatInfo);
	fillCineonOriginationInfo(cineon, &header->originInfo, &header->fileInfo);

	return 0;
}

void
dumpCineonGenericHeader(CineonGenericHeader* header) {
	dumpCineonFileInfo(&header->fileInfo);
	dumpCineonImageInfo(&header->imageInfo);
	dumpCineonFormatInfo(&header->formatInfo);
	dumpCineonOriginationInfo(&header->originInfo);
}

static int verbose = 0;
void
cineonSetVerbose(int verbosity) {
	verbose = verbosity;
}

static void
verboseMe(CineonFile* cineon) {

	d_printf("size %d x %d x %d\n", cineon->width, cineon->height, cineon->depth);
	d_printf("ImageStart %d, lineBufferLength %d, implied length %d\n",
		cineon->imageOffset, cineon->lineBufferLength * 4,
		cineon->imageOffset + cineon->lineBufferLength * 4 * cineon->height);
}

int
cineonGetRowBytes(CineonFile* cineon, unsigned short* row, int y) {

	int longsRead;
	int pixelIndex;
	int longIndex;
	int numPixels = cineon->width * cineon->depth;


	/* only seek if not reading consecutive lines */
	if (y != cineon->fileYPos) {
		int lineOffset = cineon->imageOffset + y * cineon->lineBufferLength * 4;
		if (verbose) d_printf("Seek in getRowBytes\n");
		if (logimage_fseek(cineon, lineOffset, SEEK_SET) != 0) {
			if (verbose) d_printf("Couldn't seek to line %d at %d\n", y, lineOffset);
			return 1;
		}
		cineon->fileYPos = y;
	}

	longsRead = logimage_fread(cineon->lineBuffer, 4, cineon->lineBufferLength, cineon);
	if (longsRead != cineon->lineBufferLength) {
		if (verbose) 
	{	d_printf("Couldn't read line %d length %d\n", y, cineon->lineBufferLength * 4);
		perror("cineonGetRowBytes");
	}
		return 1;
	}

	/* remember where we left the car, honey */
	++cineon->fileYPos;

	/* convert longwords to pixels */
	pixelIndex = 0;
	for (longIndex = 0; longIndex < cineon->lineBufferLength; ++longIndex) {
		unsigned int t = ntohl(cineon->lineBuffer[longIndex]);
		t = t >> 2;
		cineon->pixelBuffer[pixelIndex+2] = (unsigned short) t & 0x3ff;
		t = t >> 10;
		cineon->pixelBuffer[pixelIndex+1] = (unsigned short) t & 0x3ff;
		t = t >> 10;
		cineon->pixelBuffer[pixelIndex] = (unsigned short) t & 0x3ff;
		pixelIndex += 3;
	}

	/* extract required pixels */
	for (pixelIndex = 0; pixelIndex < numPixels; ++pixelIndex) {
		if(cineon->params.doLogarithm)
			row[pixelIndex] = cineon->lut10_16[cineon->pixelBuffer[pixelIndex]];
		else
			row[pixelIndex] = cineon->pixelBuffer[pixelIndex] << 6;
	}

	return 0;
}

int
cineonSetRowBytes(CineonFile* cineon, const unsigned short* row, int y) {

	int pixelIndex;
	int numPixels = cineon->width * cineon->depth;
	int longIndex;
	int longsWritten;

	/* put new pixels into pixelBuffer */
	for (pixelIndex = 0; pixelIndex < numPixels; ++pixelIndex) {
		if(cineon->params.doLogarithm)
			cineon->pixelBuffer[pixelIndex] = cineon->lut16_16[row[pixelIndex]];
		else
			cineon->pixelBuffer[pixelIndex] = row[pixelIndex] >> 6;
	}

	/* pack into longwords */
	pixelIndex = 0;
	for (longIndex = 0; longIndex < cineon->lineBufferLength; ++longIndex) {
		unsigned int t =
				(cineon->pixelBuffer[pixelIndex] << 22) |
				(cineon->pixelBuffer[pixelIndex+1] << 12) |
				(cineon->pixelBuffer[pixelIndex+2] << 2);
		cineon->lineBuffer[longIndex] = htonl(t);
		pixelIndex += 3;
	}

	/* only seek if not reading consecutive lines */
	if (y != cineon->fileYPos) {
		int lineOffset = cineon->imageOffset + y * cineon->lineBufferLength * 4;
		if (verbose) d_printf("Seek in setRowBytes\n");
		if (logimage_fseek(cineon, lineOffset, SEEK_SET) != 0) {
			if (verbose) d_printf("Couldn't seek to line %d at %d\n", y, lineOffset);
			return 1;
		}
		cineon->fileYPos = y;
	}

	longsWritten = fwrite(cineon->lineBuffer, 4, cineon->lineBufferLength, cineon->file);
	if (longsWritten != cineon->lineBufferLength) {
		if (verbose) d_printf("Couldn't write line %d length %d\n", y, cineon->lineBufferLength * 4);
		return 1;
	}

	++cineon->fileYPos;

	return 0;
}

int
cineonGetRow(CineonFile* cineon, unsigned short* row, int y) {

	int longsRead;
	int pixelIndex;
	int longIndex;
/*	int numPixels = cineon->width * cineon->depth;
*/
	/* only seek if not reading consecutive lines */
	if (y != cineon->fileYPos) {
		int lineOffset = cineon->imageOffset + y * cineon->lineBufferLength * 4;
		if (verbose) d_printf("Seek in getRow\n");
		if (logimage_fseek(cineon, lineOffset, SEEK_SET) != 0) {
			if (verbose) d_printf("Couldn't seek to line %d at %d\n", y, lineOffset);
			return 1;
		}
		cineon->fileYPos = y;
	}

	longsRead = logimage_fread(cineon->lineBuffer, 4, cineon->lineBufferLength, cineon);
	if (longsRead != cineon->lineBufferLength) {
		if (verbose) d_printf("Couldn't read line %d length %d\n", y, cineon->lineBufferLength * 4);
		return 1;
	}

	/* remember where we left the car, honey */
	++cineon->fileYPos;

	/* convert longwords to pixels */
	pixelIndex = 0;
	for (longIndex = 0; longIndex < cineon->lineBufferLength; ++longIndex) {
		unsigned int t = ntohl(cineon->lineBuffer[longIndex]);
		t = t >> 2;
		row[pixelIndex+2] = (unsigned short) t & 0x3ff;
		t = t >> 10;
		row[pixelIndex+1] = (unsigned short) t & 0x3ff;
		t = t >> 10;
		row[pixelIndex] = (unsigned short) t & 0x3ff;
		pixelIndex += 3;
	}

	return 0;
}

int
cineonSetRow(CineonFile* cineon, const unsigned short* row, int y) {

	int pixelIndex;
/*	int numPixels = cineon->width * cineon->depth;
*/	int longIndex;
	int longsWritten;

	/* pack into longwords */
	pixelIndex = 0;
	for (longIndex = 0; longIndex < cineon->lineBufferLength; ++longIndex) {
		unsigned int t =
				(row[pixelIndex] << 22) |
				(row[pixelIndex+1] << 12) |
				(row[pixelIndex+2] << 2);
		cineon->lineBuffer[longIndex] = htonl(t);
		pixelIndex += 3;
	}

	/* only seek if not reading consecutive lines */
	if (y != cineon->fileYPos) {
		int lineOffset = cineon->imageOffset + y * cineon->lineBufferLength * 4;
		if (verbose) d_printf("Seek in setRowBytes\n");
		if (logimage_fseek(cineon, lineOffset, SEEK_SET) != 0) {
			if (verbose) d_printf("Couldn't seek to line %d at %d\n", y, lineOffset);
			return 1;
		}
		cineon->fileYPos = y;
	}

	longsWritten = fwrite(cineon->lineBuffer, 4, cineon->lineBufferLength, cineon->file);
	if (longsWritten != cineon->lineBufferLength) {
		if (verbose) d_printf("Couldn't write line %d length %d\n", y, cineon->lineBufferLength * 4);
		return 1;
	}

	++cineon->fileYPos;

	return 0;
}

CineonFile* 
cineonOpen(const char* filename) {

	CineonGenericHeader header;

	CineonFile* cineon = (CineonFile* )malloc(sizeof(CineonFile));
	if (cineon == 0) {
		if (verbose) d_printf("Failed to malloc cineon file structure.\n");
		return 0;
	}

	/* for close routine */
	cineon->file = 0;
	cineon->lineBuffer = 0;
	cineon->pixelBuffer = 0;
	cineon->membuffer = 0;
	cineon->memcursor = 0;
	cineon->membuffersize = 0;
	
	cineon->file = fopen(filename, "rb");
	if (cineon->file == 0) {
		if (verbose) d_printf("Failed to open file \"%s\".\n", filename);
		cineonClose(cineon);
		return 0;
	}
	cineon->reading = 1;

	if (logimage_fread(&header, sizeof(CineonGenericHeader), 1, cineon) == 0) {
		if (verbose) d_printf("Not enough data for header in \"%s\".\n", filename);
		cineonClose(cineon);
		return 0;
	}

	/* let's assume cineon files are always network order */
	if (header.fileInfo.magic_num != ntohl(CINEON_FILE_MAGIC)) {
		if (verbose) d_printf("Bad magic number %8.8lX in \"%s\".\n",
			(uintptr_t)ntohl(header.fileInfo.magic_num), filename);
		cineonClose(cineon);
		return 0;
	}

	if (header.formatInfo.packing != 5) {
		if (verbose) d_printf("Can't understand packing %d\n", header.formatInfo.packing);
		cineonClose(cineon);
		return 0;
	}

	cineon->width = ntohl(header.imageInfo.channel[0].pixels_per_line);
	cineon->height = ntohl(header.imageInfo.channel[0].lines_per_image);
	cineon->depth = header.imageInfo.channels_per_image;
	/* cineon->bitsPerPixel = 10; */
	cineon->bitsPerPixel = header.imageInfo.channel[0].bits_per_pixel;
	cineon->imageOffset = ntohl(header.fileInfo.image_offset);

	cineon->lineBufferLength = pixelsToLongs(cineon->width * cineon->depth);
	cineon->lineBuffer = malloc(cineon->lineBufferLength * 4);
	if (cineon->lineBuffer == 0) {
		if (verbose) d_printf("Couldn't malloc line buffer of size %d\n", cineon->lineBufferLength * 4);
		cineonClose(cineon);
		return 0;
	}

	cineon->pixelBuffer = malloc(cineon->lineBufferLength * 3 * sizeof(unsigned short));
	if (cineon->pixelBuffer == 0) {
		if (verbose) d_printf("Couldn't malloc pixel buffer of size %d\n",
				(cineon->width * cineon->depth) * (int)sizeof(unsigned short));
		cineonClose(cineon);
		return 0;
	}
	cineon->pixelBufferUsed = 0;

	if (logimage_fseek(cineon, cineon->imageOffset, SEEK_SET) != 0) {
		if (verbose) d_printf("Couldn't seek to image data at %d\n", cineon->imageOffset);
		cineonClose(cineon);
		return 0;
	}
	cineon->fileYPos = 0;

	logImageGetByteConversionDefaults(&cineon->params);
	setupLut(cineon);

	cineon->getRow = &cineonGetRowBytes;
	cineon->setRow = 0;
	cineon->close = &cineonClose;

	if (verbose) {
		verboseMe(cineon);
	}

	return cineon;
}

int cineonIsMemFileCineon(unsigned char *mem)
{
	unsigned int num;
	memcpy(&num, mem, sizeof(unsigned int));
	
	if (num != ntohl(CINEON_FILE_MAGIC)) {
		return 0;
	} else return 1;
}

CineonFile* 
cineonOpenFromMem(unsigned char *mem, unsigned int size) {

	CineonGenericHeader header;
	int i;
	
	CineonFile* cineon = (CineonFile* )malloc(sizeof(CineonFile));
	if (cineon == 0) {
		if (verbose) d_printf("Failed to malloc cineon file structure.\n");
		return 0;
	}

	/* for close routine */
	cineon->file = 0;
	cineon->lineBuffer = 0;
	cineon->pixelBuffer = 0;
	cineon->membuffer = mem;
	cineon->membuffersize = size;
	cineon->memcursor = mem;
	
	cineon->file = 0;
	cineon->reading = 1;
	verbose = 0;
	if (size < sizeof(CineonGenericHeader)) {
		if (verbose) d_printf("Not enough data for header!\n");
		cineonClose(cineon);
		return 0;
	}

	logimage_fread(&header, sizeof(CineonGenericHeader), 1, cineon);

	/* let's assume cineon files are always network order */
	if (header.fileInfo.magic_num != ntohl(CINEON_FILE_MAGIC)) {
		if (verbose) d_printf("Bad magic number %8.8lX in\n", (uintptr_t)ntohl(header.fileInfo.magic_num));

		cineonClose(cineon);
		return 0;
	}

	if (header.formatInfo.packing != 5) {
		if (verbose) d_printf("Can't understand packing %d\n", header.formatInfo.packing);
		cineonClose(cineon);
		return 0;
	}

	cineon->width = ntohl(header.imageInfo.channel[0].pixels_per_line);
	cineon->height = ntohl(header.imageInfo.channel[0].lines_per_image);
	cineon->depth = header.imageInfo.channels_per_image;
	/* cineon->bitsPerPixel = 10; */
	cineon->bitsPerPixel = header.imageInfo.channel[0].bits_per_pixel;
	cineon->imageOffset = ntohl(header.fileInfo.image_offset);

	cineon->lineBufferLength = pixelsToLongs(cineon->width * cineon->depth);
	cineon->lineBuffer = malloc(cineon->lineBufferLength * 4);
	if (cineon->lineBuffer == 0) {
		if (verbose) d_printf("Couldn't malloc line buffer of size %d\n", cineon->lineBufferLength * 4);
		cineonClose(cineon);
		return 0;
	}

	cineon->pixelBuffer = malloc(cineon->lineBufferLength * 3 * sizeof(unsigned short));
	if (cineon->pixelBuffer == 0) {
		if (verbose) d_printf("Couldn't malloc pixel buffer of size %d\n",
				(cineon->width * cineon->depth) * (int)sizeof(unsigned short));
		cineonClose(cineon);
		return 0;
	}
	cineon->pixelBufferUsed = 0;

	i = cineon->imageOffset;
	
	if (logimage_fseek(cineon, cineon->imageOffset, SEEK_SET) != 0) {
		if (verbose) d_printf("Couldn't seek to image data at %d\n", cineon->imageOffset);
		cineonClose(cineon);
		return 0;
	}
	
	cineon->fileYPos = 0;

	logImageGetByteConversionDefaults(&cineon->params);
	setupLut(cineon);

	cineon->getRow = &cineonGetRowBytes;
	cineon->setRow = 0;
	cineon->close = &cineonClose;

	if (verbose) {
		verboseMe(cineon);
	}

	return cineon;
}


int
cineonGetSize(const CineonFile* cineon, int* width, int* height, int* depth) {
	*width = cineon->width;
	*height = cineon->height;
	*depth = cineon->depth;
	return 0;
}

CineonFile*
cineonCreate(const char* filename, int width, int height, int depth) {

	/* Note: always write files in network order */
	/* By the spec, it shouldn't matter, but ... */

	CineonGenericHeader header;
	const char* shortFilename = 0;

	CineonFile* cineon = (CineonFile*)malloc(sizeof(CineonFile));
	if (cineon == 0) {
		if (verbose) d_printf("Failed to malloc cineon file structure.\n");
		return 0;
	}

	memset(&header, 0, sizeof(header));

	/* for close routine */
	cineon->file = 0;
	cineon->lineBuffer = 0;
	cineon->pixelBuffer = 0;

	cineon->file = fopen(filename, "wb");
	if (cineon->file == 0) {
		if (verbose) d_printf("Couldn't open file %s\n", filename);
		cineonClose(cineon);
		return 0;
	}
	cineon->reading = 0;

	cineon->width = width;
	cineon->height = height;
	cineon->depth = depth;
	cineon->bitsPerPixel = 10;
	cineon->imageOffset = sizeof(CineonGenericHeader);

	cineon->lineBufferLength = pixelsToLongs(cineon->width * cineon->depth);
	cineon->lineBuffer = malloc(cineon->lineBufferLength * 4);
	if (cineon->lineBuffer == 0) {
		if (verbose) d_printf("Couldn't malloc line buffer of size %d\n", cineon->lineBufferLength * 4);
		cineonClose(cineon);
		return 0;
	}

	cineon->pixelBuffer = malloc(cineon->lineBufferLength * 3 * sizeof(unsigned short));
	if (cineon->pixelBuffer == 0) {
		if (verbose) d_printf("Couldn't malloc pixel buffer of size %d\n",
				(cineon->width * cineon->depth) * (int)sizeof(unsigned short));
		cineonClose(cineon);
		return 0;
	}
	cineon->pixelBufferUsed = 0;

	/* find trailing part of filename */
	shortFilename = strrchr(filename, '/');
	if (shortFilename == 0) {
		shortFilename = filename;
	} else {
		++shortFilename;
	}

	if (initCineonGenericHeader(cineon, &header, shortFilename) != 0) {
		cineonClose(cineon);
		return 0;
	}

	if (fwrite(&header, sizeof(header), 1, cineon->file) == 0) {
		if (verbose) d_printf("Couldn't write image header\n");
		cineonClose(cineon);
		return 0;
	}
	cineon->fileYPos = 0;

	logImageGetByteConversionDefaults(&cineon->params);
	setupLut(cineon);

	cineon->getRow = 0;
	cineon->setRow = &cineonSetRowBytes;
	cineon->close = &cineonClose;

	return cineon;
}

void
cineonClose(CineonFile* cineon) {

	if (cineon == 0) {
		return;
	}

	if (cineon->file) {
		fclose(cineon->file);
		cineon->file = 0;
	}

	if (cineon->lineBuffer) {
		free(cineon->lineBuffer);
		cineon->lineBuffer = 0;
	}

	if (cineon->pixelBuffer) {
		free(cineon->pixelBuffer);
		cineon->pixelBuffer = 0;
	}

	free(cineon);
}
