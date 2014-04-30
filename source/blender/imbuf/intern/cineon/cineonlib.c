/*
 * Cineon image file format library routines.
 *
 * Copyright 1999,2000,2001 David Hodson <hodsond@acm.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Julien Enche.
 *
 */

/** \file blender/imbuf/intern/cineon/cineonlib.c
 *  \ingroup imbcineon
 */


#include "cineonlib.h"
#include "logmemfile.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>

#include "BLI_fileops.h"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

/*
 * For debug purpose
 */

static int verbose = 0;

void cineonSetVerbose(int verbosity)
{
	verbose = verbosity;
}

static void fillCineonMainHeader(LogImageFile *cineon, CineonMainHeader *header,
                                 const char *filename, const char *creator)
{
	time_t fileClock;
	struct tm *fileTime;
	int i;

	memset(header, 0, sizeof(CineonMainHeader));

	/* --- File header --- */
	header->fileHeader.magic_num = swap_uint(CINEON_FILE_MAGIC, cineon->isMSB);
	header->fileHeader.offset = swap_uint(cineon->element[0].dataOffset, cineon->isMSB);
	header->fileHeader.gen_hdr_size = swap_uint(sizeof(CineonFileHeader) + sizeof(CineonImageHeader) +
	                                            sizeof(CineonOriginationHeader), cineon->isMSB);
	header->fileHeader.ind_hdr_size = 0;
	header->fileHeader.user_data_size = 0;
	header->fileHeader.file_size = swap_uint(cineon->element[0].dataOffset + cineon->height * getRowLength(cineon->width, cineon->element[0]), cineon->isMSB);
	strcpy(header->fileHeader.version, "v4.5");
	strncpy(header->fileHeader.file_name, filename, 99);
	header->fileHeader.file_name[99] = 0;
	fileClock = time(NULL);
	fileTime = localtime(&fileClock);
	strftime(header->fileHeader.creation_date, 12, "%Y:%m:%d", fileTime);
	strftime(header->fileHeader.creation_time, 12, "%H:%M:%S%Z", fileTime);
	header->fileHeader.creation_time[11] = 0;

	/* --- Image header --- */
	header->imageHeader.orientation = 0;
	header->imageHeader.elements_per_image = cineon->depth;

	for (i = 0; i < 3; i++) {
		header->imageHeader.element[i].descriptor1 = 0;
		header->imageHeader.element[i].descriptor2 = i;
		header->imageHeader.element[i].bits_per_sample = cineon->element[0].bitsPerSample;
		header->imageHeader.element[i].pixels_per_line = swap_uint(cineon->width, cineon->isMSB);
		header->imageHeader.element[i].lines_per_image = swap_uint(cineon->height, cineon->isMSB);
		header->imageHeader.element[i].ref_low_data = swap_uint(cineon->element[0].refLowData, cineon->isMSB);
		header->imageHeader.element[i].ref_low_quantity = swap_float(cineon->element[0].refLowQuantity, cineon->isMSB);
		header->imageHeader.element[i].ref_high_data = swap_uint(cineon->element[0].refHighData, cineon->isMSB);
		header->imageHeader.element[i].ref_high_quantity = swap_float(cineon->element[0].refHighQuantity, cineon->isMSB);
	}

	header->imageHeader.white_point_x = swap_float(0.0f, cineon->isMSB);
	header->imageHeader.white_point_y = swap_float(0.0f, cineon->isMSB);
	header->imageHeader.red_primary_x = swap_float(0.0f, cineon->isMSB);
	header->imageHeader.red_primary_y = swap_float(0.0f, cineon->isMSB);
	header->imageHeader.green_primary_x = swap_float(0.0f, cineon->isMSB);
	header->imageHeader.green_primary_y = swap_float(0.0f, cineon->isMSB);
	header->imageHeader.blue_primary_x = swap_float(0.0f, cineon->isMSB);
	header->imageHeader.blue_primary_y = swap_float(0.0f, cineon->isMSB);
	strncpy(header->imageHeader.label, creator, 199);
	header->imageHeader.label[199] = 0;
	header->imageHeader.interleave = 0;
	header->imageHeader.data_sign = 0;
	header->imageHeader.sense = 0;
	header->imageHeader.line_padding = swap_uint(0, cineon->isMSB);
	header->imageHeader.element_padding = swap_uint(0, cineon->isMSB);

	switch (cineon->element[0].packing) {
		case 0:
			header->imageHeader.packing = 0;
			break;

		case 1:
			header->imageHeader.packing = 5;
			break;

		case 2:
			header->imageHeader.packing = 6;
			break;
	}

	/* --- Origination header --- */
	/* we leave it blank */

	/* --- Film header --- */
	/* we leave it blank */
}

LogImageFile *cineonOpen(const unsigned char *byteStuff, int fromMemory, size_t bufferSize)
{
	CineonMainHeader header;
	LogImageFile *cineon = (LogImageFile *)MEM_mallocN(sizeof(LogImageFile), __func__);
	char *filename = (char *)byteStuff;
	int i;
	unsigned int dataOffset;

	if (cineon == NULL) {
		if (verbose) printf("Cineon: Failed to malloc cineon file structure.\n");
		return NULL;
	}

	/* zero the header */
	memset(&header, 0, sizeof(CineonMainHeader));

	/* for close routine */
	cineon->file = NULL;

	if (fromMemory == 0) {
		/* byteStuff is then the filename */
		cineon->file = BLI_fopen(filename, "rb");
		if (cineon->file == NULL) {
			if (verbose) printf("Cineon: Failed to open file \"%s\".\n", filename);
			logImageClose(cineon);
			return NULL;
		}
		/* not used in this case */
		cineon->memBuffer = NULL;
		cineon->memCursor = NULL;
		cineon->memBufferSize = 0;
	}
	else {
		cineon->memBuffer = (unsigned char *)byteStuff;
		cineon->memCursor = (unsigned char *)byteStuff;
		cineon->memBufferSize = bufferSize;
	}

	if (logimage_fread(&header, sizeof(header), 1, cineon) == 0) {
		if (verbose) printf("Cineon: Not enough data for header in \"%s\".\n", byteStuff);
		logImageClose(cineon);
		return NULL;
	}

	/* endianness determination */
	if (header.fileHeader.magic_num == swap_uint(CINEON_FILE_MAGIC, 1)) {
		cineon->isMSB = 1;
		if (verbose) printf("Cineon: File is MSB.\n");
	}
	else if (header.fileHeader.magic_num == CINEON_FILE_MAGIC) {
		cineon->isMSB = 0;
		if (verbose) printf("Cineon: File is LSB.\n");
	}
	else {
		if (verbose) printf("Cineon: Bad magic number %lu in \"%s\".\n",
			                (unsigned long)header.fileHeader.magic_num, byteStuff);
		logImageClose(cineon);
		return NULL;
	}

	cineon->width = swap_uint(header.imageHeader.element[0].pixels_per_line, cineon->isMSB);
	cineon->height = swap_uint(header.imageHeader.element[0].lines_per_image, cineon->isMSB);

	if (cineon->width == 0 || cineon->height == 0) {
		if (verbose) printf("Cineon: Wrong image dimension: %dx%d\n", cineon->width, cineon->height);
		logImageClose(cineon);
		return NULL;
	}

	cineon->depth = header.imageHeader.elements_per_image;
	cineon->srcFormat = format_Cineon;

	if (header.imageHeader.interleave == 0)
		cineon->numElements = 1;
	else if (header.imageHeader.interleave == 2)
		cineon->numElements = header.imageHeader.elements_per_image;
	else {
		if (verbose) printf("Cineon: Data interleave not supported: %d\n", header.imageHeader.interleave);
		logImageClose(cineon);
		return NULL;
	}

	if (cineon->depth == 1) {
		/* Grayscale image */
		cineon->element[0].descriptor = descriptor_Luminance;
		cineon->element[0].transfer = transfer_Linear;
		cineon->element[0].depth = 1;
	}
	else if (cineon->depth == 3) {
		/* RGB image */
		if (cineon->numElements == 1) {
			cineon->element[0].descriptor = descriptor_RGB;
			cineon->element[0].transfer = transfer_PrintingDensity;
			cineon->element[0].depth = 3;
		}
		else if (cineon->numElements == 3) {
			cineon->element[0].descriptor = descriptor_Red;
			cineon->element[0].transfer = transfer_PrintingDensity;
			cineon->element[0].depth = 1;
			cineon->element[1].descriptor = descriptor_Green;
			cineon->element[1].transfer = transfer_PrintingDensity;
			cineon->element[1].depth = 1;
			cineon->element[2].descriptor = descriptor_Blue;
			cineon->element[2].transfer = transfer_PrintingDensity;
			cineon->element[2].depth = 1;
		}
	}
	else {
		if (verbose) printf("Cineon: Cineon image depth unsupported: %d\n", cineon->depth);
		logImageClose(cineon);
		return NULL;
	}

	dataOffset = swap_uint(header.fileHeader.offset, cineon->isMSB);

	for (i = 0; i < cineon->numElements; i++) {
		cineon->element[i].bitsPerSample = header.imageHeader.element[i].bits_per_sample;
		cineon->element[i].maxValue = powf(2, cineon->element[i].bitsPerSample) - 1.0f;
		cineon->element[i].refLowData = swap_uint(header.imageHeader.element[i].ref_low_data, cineon->isMSB);
		cineon->element[i].refLowQuantity = swap_float(header.imageHeader.element[i].ref_low_quantity, cineon->isMSB);
		cineon->element[i].refHighData = swap_uint(header.imageHeader.element[i].ref_high_data, cineon->isMSB);
		cineon->element[i].refHighQuantity = swap_float(header.imageHeader.element[i].ref_high_quantity, cineon->isMSB);

		switch (header.imageHeader.packing) {
			case 0:
				cineon->element[i].packing = 0;
				break;

			case 5:
				cineon->element[i].packing = 1;
				break;

			case 6:
				cineon->element[i].packing = 2;
				break;

			default:
				/* Not supported */
				if (verbose) printf("Cineon: packing unsupported: %d\n", header.imageHeader.packing);
				logImageClose(cineon);
				return NULL;
		}

		if (cineon->element[i].refLowData == CINEON_UNDEFINED_U32)
			cineon->element[i].refLowData = 0;

		if (cineon->element[i].refHighData == CINEON_UNDEFINED_U32)
			cineon->element[i].refHighData = (unsigned int)cineon->element[i].maxValue;

		if (cineon->element[i].refLowQuantity == CINEON_UNDEFINED_R32 || isnan(cineon->element[i].refLowQuantity))
			cineon->element[i].refLowQuantity = 0.0f;

		if (cineon->element[i].refHighQuantity == CINEON_UNDEFINED_R32 || isnan(cineon->element[i].refHighQuantity)) {
			if (cineon->element[i].transfer == transfer_PrintingDensity)
				cineon->element[i].refHighQuantity = 2.048f;
			else
				cineon->element[i].refHighQuantity = cineon->element[i].maxValue;
		}

		cineon->element[i].dataOffset = dataOffset;
		dataOffset += cineon->height * getRowLength(cineon->width, cineon->element[i]);
	}

	cineon->referenceBlack = 95.0f / 1023.0f * cineon->element[0].maxValue;
	cineon->referenceWhite = 685.0f / 1023.0f * cineon->element[0].maxValue;
	cineon->gamma = 1.7f;

	if (verbose) {
		printf("size %d x %d x %d elements\n", cineon->width, cineon->height, cineon->numElements);
		for (i = 0; i < cineon->numElements; i++) {
			printf(" Element %d:\n", i);
			printf("  Bits per sample: %d\n", cineon->element[i].bitsPerSample);
			printf("  Depth: %d\n", cineon->element[i].depth);
			printf("  Transfer characteristics: %d\n", cineon->element[i].transfer);
			printf("  Packing: %d\n", cineon->element[i].packing);
			printf("  Descriptor: %d\n", cineon->element[i].descriptor);
			printf("  Data offset: %u\n", cineon->element[i].dataOffset);
			printf("  Reference low data: %u\n", cineon->element[i].refLowData);
			printf("  Reference low quantity: %f\n", cineon->element[i].refLowQuantity);
			printf("  Reference high data: %u\n", cineon->element[i].refHighData);
			printf("  Reference high quantity: %f\n", cineon->element[i].refHighQuantity);
			printf("\n");
		}

		printf("Gamma: %f\n", cineon->gamma);
		printf("Reference black: %f\n", cineon->referenceBlack);
		printf("Reference white: %f\n", cineon->referenceWhite);
		printf("----------------------------\n");
	}
	return cineon;
}

LogImageFile *cineonCreate(const char *filename, int width, int height, int bitsPerSample, const char *creator)
{
	CineonMainHeader header;
	const char *shortFilename = NULL;
	/* unsigned char pad[6044]; */

	LogImageFile *cineon = (LogImageFile *)MEM_mallocN(sizeof(LogImageFile), __func__);
	if (cineon == NULL) {
		if (verbose) printf("cineon: Failed to malloc cineon file structure.\n");
		return NULL;
	}

	/* Only 10 bits Cineon are supported */
	if (bitsPerSample != 10) {
		if (verbose) printf("cineon: Only 10 bits Cineon are supported.\n");
		logImageClose(cineon);
		return NULL;
	}

	cineon->width = width;
	cineon->height = height;
	cineon->element[0].bitsPerSample = 10;
	cineon->element[0].dataOffset = sizeof(CineonMainHeader);
	cineon->element[0].maxValue = 1023;
	cineon->isMSB = 1;
	cineon->numElements = 1;
	cineon->element[0].packing = 1;
	cineon->depth = 3;
	cineon->element[0].depth = 3;
	cineon->element[0].descriptor = descriptor_RGB;
	cineon->element[0].transfer = transfer_PrintingDensity;
	cineon->element[0].refHighQuantity = 2.048f;
	cineon->element[0].refLowQuantity = 0;
	cineon->element[0].refLowData = 0;
	cineon->element[0].refHighData = cineon->element[0].maxValue;
	cineon->referenceWhite = 685.0f;
	cineon->referenceBlack = 95.0f;
	cineon->gamma = 1.7f;

	shortFilename = strrchr(filename, '/');
	if (shortFilename == NULL)
		shortFilename = filename;
	else
		shortFilename++;

	cineon->file = BLI_fopen(filename, "wb");
	if (cineon->file == NULL) {
		if (verbose) printf("cineon: Couldn't open file %s\n", filename);
		logImageClose(cineon);
		return NULL;
	}

	fillCineonMainHeader(cineon, &header, shortFilename, creator);

	if (fwrite(&header, sizeof(header), 1, cineon->file) == 0) {
		if (verbose) printf("cineon: Couldn't write image header\n");
		logImageClose(cineon);
		return NULL;
	}

	return cineon;
}
