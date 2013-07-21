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

/** \file blender/imbuf/intern/cineon/logImageCore.c
 *  \ingroup imbcineon
 */


#include "logmemfile.h"
#include "logImageCore.h"
#include "dpxlib.h"
#include "cineonlib.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "BLI_fileops.h"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

/*
 * Declaration of static functions
 */

static int logImageSetData8(LogImageFile *logImage, LogImageElement logElement, float *data);
static int logImageSetData10(LogImageFile *logImage, LogImageElement logElement, float *data);
static int logImageSetData12(LogImageFile *logImage, LogImageElement logElement, float *data);
static int logImageSetData16(LogImageFile *logImage, LogImageElement logElement, float *data);
static int logImageElementGetData(LogImageFile *dpx, LogImageElement logElement, float *data);
static int logImageElementGetData1(LogImageFile *dpx, LogImageElement logElement, float *data);
static int logImageElementGetData8(LogImageFile *dpx, LogImageElement logElement, float *data);
static int logImageElementGetData10(LogImageFile *dpx, LogImageElement logElement, float *data);
static int logImageElementGetData10Packed(LogImageFile *dpx, LogImageElement logElement, float *data);
static int logImageElementGetData12(LogImageFile *dpx, LogImageElement logElement, float *data);
static int logImageElementGetData12Packed(LogImageFile *dpx, LogImageElement logElement, float *data);
static int logImageElementGetData16(LogImageFile *dpx, LogImageElement logElement, float *data);
static int convertLogElementToRGBA(float *src, float *dst, LogImageFile *logImage, LogImageElement logElement, int dstIsLinearRGB);
static int convertRGBAToLogElement(float *src, float *dst, LogImageFile *logImage, LogImageElement logElement, int srcIsLinearRGB);


/*
 * For debug purpose
 */

static int verbose = 0;

void logImageSetVerbose(int verbosity)
{
	verbose = verbosity;
	cineonSetVerbose(verbosity);
	dpxSetVerbose(verbosity);
}


/*
 * IO stuff
 */

int logImageIsDpx(const void *buffer)
{
	unsigned int magicNum = *(unsigned int *)buffer;
	return (magicNum == DPX_FILE_MAGIC || magicNum == swap_uint(DPX_FILE_MAGIC, 1));
}

int logImageIsCineon(const void *buffer)
{
	unsigned int magicNum = *(unsigned int *)buffer;
	return (magicNum == CINEON_FILE_MAGIC || magicNum == swap_uint(CINEON_FILE_MAGIC, 1));
}

LogImageFile *logImageOpenFromFile(const char *filename, int cineon)
{
	unsigned int magicNum;
	FILE *f = BLI_fopen(filename, "rb");

	(void)cineon;

	if (f == NULL)
		return NULL;

	if (fread(&magicNum, sizeof(unsigned int), 1, f) != 1) {
		fclose(f);
		return NULL;
	}

	fclose(f);

	if (logImageIsDpx(&magicNum))
		return dpxOpen((const unsigned char *)filename, 0, 0);
	else if (logImageIsCineon(&magicNum))
		return cineonOpen((const unsigned char *)filename, 0, 0);

	return NULL;
}

LogImageFile *logImageOpenFromMemory(const unsigned char *buffer, unsigned int size)
{
	if (logImageIsDpx(buffer))
		return dpxOpen(buffer, 1, size);
	else if (logImageIsCineon(buffer))
		return cineonOpen(buffer, 1, size);

	return NULL;
}

LogImageFile *logImageCreate(const char *filename, int cineon, int width, int height, int bitsPerSample,
                             int isLogarithmic, int hasAlpha, int referenceWhite, int referenceBlack,
                             float gamma, const char *creator)
{
	/* referenceWhite, referenceBlack and gamma values are only supported for DPX file */
	if (cineon)
		return cineonCreate(filename, width, height, bitsPerSample, creator);
	else
		return dpxCreate(filename, width, height, bitsPerSample, isLogarithmic, hasAlpha,
		                 referenceWhite, referenceBlack, gamma, creator);

	return NULL;
}

void logImageClose(LogImageFile *logImage)
{
	if (logImage != NULL) {
		if (logImage->file) {
			fclose(logImage->file);
			logImage->file = NULL;
		}
		MEM_freeN(logImage);
	}
}

void logImageGetSize(LogImageFile *logImage, int *width, int *height, int *depth)
{
	*width = logImage->width;
	*height = logImage->height;
	*depth = logImage->depth;
}


/*
 * Helper
 */

unsigned int getRowLength(int width, LogImageElement logElement)
{
	/* return the row length in bytes according to width and packing method */
	switch (logElement.bitsPerSample) {
		case 1:
			return ((width * logElement.depth - 1) / 32 + 1) * 4;

		case 8:
			return ((width * logElement.depth - 1) / 4 + 1) * 4;

		case 10:
			if (logElement.packing == 0)
				return ((width * logElement.depth * 10 - 1) / 32 + 1) * 4;
			else if (logElement.packing == 1 || logElement.packing == 2)
				return ((width * logElement.depth - 1) / 3 + 1) * 4;

		case 12:
			if (logElement.packing == 0)
				return ((width * logElement.depth * 12 - 1) / 32 + 1) * 4;
			else if (logElement.packing == 1 || logElement.packing == 2)
				return width * logElement.depth * 2;

		case 16:
			return width * logElement.depth * 2;

		default:
			return 0;
	}
}


/*
 * Data writing
 */

int logImageSetDataRGBA(LogImageFile *logImage, float *data, int dataIsLinearRGB)
{
	float *elementData;
	int returnValue;

	elementData = (float *)MEM_mallocN(logImage->width * logImage->height * logImage->depth * sizeof(float), __func__);
	if (elementData == NULL)
		return 1;

	if (convertRGBAToLogElement(data, elementData, logImage, logImage->element[0], dataIsLinearRGB) != 0) {
		MEM_freeN(elementData);
		return 1;
	}

	switch (logImage->element[0].bitsPerSample) {
		case 8:
			returnValue = logImageSetData8(logImage, logImage->element[0], elementData);
			break;

		case 10:
			returnValue = logImageSetData10(logImage, logImage->element[0], elementData);
			break;

		case 12:
			returnValue = logImageSetData12(logImage, logImage->element[0], elementData);
			break;

		case 16:
			returnValue = logImageSetData16(logImage, logImage->element[0], elementData);
			break;

		default:
			returnValue = 1;
			break;
	}

	MEM_freeN(elementData);
	return returnValue;
}

static int logImageSetData8(LogImageFile *logImage, LogImageElement logElement, float *data)
{
	unsigned int rowLength = getRowLength(logImage->width, logElement);
	unsigned char *row;
	int x, y;

	row = (unsigned char *)MEM_mallocN(rowLength, __func__);
	if (row == NULL) {
		if (verbose) printf("DPX/Cineon: Cannot allocate row.\n");
		return 1;
	}
	memset(row, 0, rowLength);

	for (y = 0; y < logImage->height; y++) {
		for (x = 0; x < logImage->width * logImage->depth; x++)
			row[x] = (unsigned char)float_uint(data[y * logImage->width * logImage->depth + x], 255);

		if (logimage_fwrite(row, rowLength, 1, logImage) == 0) {
			if (verbose) printf("DPX/Cineon: Error while writing file.\n");
			MEM_freeN(row);
			return 1;
		}
	}
	MEM_freeN(row);
	return 0;
}

static int logImageSetData10(LogImageFile *logImage, LogImageElement logElement, float *data)
{
	unsigned int rowLength = getRowLength(logImage->width, logElement);
	unsigned int pixel, index;
	unsigned int *row;
	int x, y, offset;

	row = (unsigned int *)MEM_mallocN(rowLength, __func__);
	if (row == NULL) {
		if (verbose) printf("DPX/Cineon: Cannot allocate row.\n");
		return 1;
	}

	for (y = 0; y < logImage->height; y++) {
		offset = 22;
		index = 0;
		pixel = 0;

		for (x = 0; x < logImage->width * logImage->depth; x++) {
			pixel |= (unsigned int)float_uint(data[y * logImage->width * logImage->depth + x], 1023) << offset;
			offset -= 10;
			if (offset < 0) {
				row[index] = swap_uint(pixel, logImage->isMSB);
				index++;
				pixel = 0;
				offset = 22;
			}
		}
		if (pixel != 0)
			row[index] = swap_uint(pixel, logImage->isMSB);

		if (logimage_fwrite(row, rowLength, 1, logImage) == 0) {
			if (verbose) printf("DPX/Cineon: Error while writing file.\n"); {
				MEM_freeN(row);
				return 1;
			}
		}
	}
	MEM_freeN(row);
	return 0;
}

static int logImageSetData12(LogImageFile *logImage, LogImageElement logElement, float *data)
{
	unsigned int rowLength = getRowLength(logImage->width, logElement);
	unsigned short *row;
	int x, y;

	row = (unsigned short *)MEM_mallocN(rowLength, __func__);
	if (row == NULL) {
		if (verbose) printf("DPX/Cineon: Cannot allocate row.\n");
		return 1;
	}

	for (y = 0; y < logImage->height; y++) {
		for (x = 0; x < logImage->width * logImage->depth; x++)
			row[x] = swap_ushort(((unsigned short)float_uint(data[y * logImage->width * logImage->depth + x], 4095)) << 4, logImage->isMSB);

		if (logimage_fwrite(row, rowLength, 1, logImage) == 0) {
			if (verbose) printf("DPX/Cineon: Error while writing file.\n");
			MEM_freeN(row);
			return 1;
		}
	}
	MEM_freeN(row);
	return 0;
}

static int logImageSetData16(LogImageFile *logImage, LogImageElement logElement, float *data)
{
	unsigned int rowLength = getRowLength(logImage->width, logElement);
	unsigned short *row;
	int x, y;

	row = (unsigned short *)MEM_mallocN(rowLength, __func__);
	if (row == NULL) {
		if (verbose) printf("DPX/Cineon: Cannot allocate row.\n");
		return 1;
	}

	for (y = 0; y < logImage->height; y++) {
		for (x = 0; x < logImage->width * logImage->depth; x++)
			row[x] = swap_ushort((unsigned short)float_uint(data[y * logImage->width * logImage->depth + x], 65535), logImage->isMSB);

		if (logimage_fwrite(row, rowLength, 1, logImage) == 0) {
			if (verbose) printf("DPX/Cineon: Error while writing file.\n");
			MEM_freeN(row);
			return 1;
		}
	}
	MEM_freeN(row);
	return 0;
}


/*
 * Data reading
 */

int logImageGetDataRGBA(LogImageFile *logImage, float *data, int dataIsLinearRGB)
{
	/* Fills data with 32 bits float RGBA values */
	int i, j, returnValue, sortedElementData[8], hasAlpha;
	float *elementData[8];
	float *elementData_ptr[8];
	float *mergedData;
	unsigned int sampleIndex;
	LogImageElement mergedElement;

	/* Determine the depth of the picture and if there's a separate alpha element.
	 * If the element is supported, load it into an unsigned ints array. */
	memset(&elementData, 0, 8 * sizeof(float *));
	hasAlpha = 0;

	for (i = 0; i < logImage->numElements; i++) {
		/* descriptor_Depth and descriptor_Composite are not supported */
		if (logImage->element[i].descriptor != descriptor_Depth && logImage->element[i].descriptor != descriptor_Composite) {
			/* Allocate memory */
			elementData[i] = (float *)MEM_mallocN(logImage->width * logImage->height * logImage->element[i].depth * sizeof(float), __func__);
			if (elementData[i] == NULL) {
				if (verbose) printf("DPX/Cineon: Cannot allocate memory for elementData[%d]\n.", i);
				for (j = 0; j < i; j++)
					if (elementData[j] != NULL)
						MEM_freeN(elementData[j]);
				return 1;
			}
			elementData_ptr[i] = elementData[i];

			/* Load data */
			if (logImageElementGetData(logImage, logImage->element[i], elementData[i]) != 0) {
				if (verbose) printf("DPX/Cineon: Cannot read elementData[%d]\n.", i);
				for (j = 0; j < i; j++)
					if (elementData[j] != NULL)
						MEM_freeN(elementData[j]);
				return 1;
			}
		}

		if (logImage->element[i].descriptor == descriptor_Alpha)
			hasAlpha = 1;
	}

	/* only one element, easy case, no need to do anything  */
	if (logImage->numElements == 1) {
		returnValue = convertLogElementToRGBA(elementData[0], data, logImage, logImage->element[0], dataIsLinearRGB);
		MEM_freeN(elementData[0]);
	}
	else {
		/* The goal here is to merge every elements into only one
		 * to recreate a classic 16 bits RGB, RGBA or YCbCr element.
		 * Unsupported elements are skipped (depth, composite) */

		memcpy(&mergedElement, &logImage->element[0], sizeof(LogImageElement));
		mergedElement.descriptor = -1;
		mergedElement.depth = logImage->depth;
		memset(&sortedElementData, -1, 8 * sizeof(int));

		/* Try to know how to assemble the elements */
		for (i = 0; i < logImage->numElements; i++) {
			switch (logImage->element[i].descriptor) {
				case descriptor_Red:
				case descriptor_RGB:
					if (hasAlpha == 0)
						mergedElement.descriptor = descriptor_RGB;
					else
						mergedElement.descriptor = descriptor_RGBA;

					sortedElementData[0] = i;
					break;

				case descriptor_Green:
					if (hasAlpha == 0)
						mergedElement.descriptor = descriptor_RGB;
					else
						mergedElement.descriptor = descriptor_RGBA;

					sortedElementData[1] = i;
					break;

				case descriptor_Blue:
					if (hasAlpha == 0)
						mergedElement.descriptor = descriptor_RGB;
					else
						mergedElement.descriptor = descriptor_RGBA;

					sortedElementData[2] = i;
					break;

				case descriptor_Alpha:
					/* Alpha component is always the last one */
					sortedElementData[mergedElement.depth - 1] = i;
					break;

				case descriptor_Luminance:
					if (mergedElement.descriptor == -1)
						if (hasAlpha == 0)
							mergedElement.descriptor = descriptor_Luminance;
						else
							mergedElement.descriptor = descriptor_YA;
					else if (mergedElement.descriptor == descriptor_Chrominance) {
						if (mergedElement.depth == 2)
							mergedElement.descriptor = descriptor_CbYCrY;
						else if (mergedElement.depth == 3)
							if (hasAlpha == 0)
								mergedElement.descriptor = descriptor_CbYCr;
							else
								mergedElement.descriptor = descriptor_CbYACrYA;
						else if (mergedElement.depth == 4)
							mergedElement.descriptor = descriptor_CbYCrA;
					}

					/* Y component always in 1 except if it's alone or with alpha */
					if (mergedElement.depth == 1 || (mergedElement.depth == 2 && hasAlpha == 1))
						sortedElementData[0] = i;
					else
						sortedElementData[1] = i;
					break;

				case descriptor_Chrominance:
					if (mergedElement.descriptor == -1)
						mergedElement.descriptor = descriptor_Chrominance;
					else if (mergedElement.descriptor == descriptor_Luminance) {
						if (mergedElement.depth == 2)
							mergedElement.descriptor = descriptor_CbYCrY;
						else if (mergedElement.depth == 3)
							if (hasAlpha == 0)
								mergedElement.descriptor = descriptor_CbYCr;
							else
								mergedElement.descriptor = descriptor_CbYACrYA;
						else if (mergedElement.depth == 4)
							mergedElement.descriptor = descriptor_CbYCrA;
					}

					/* Cb and Cr always in 0 or 2 */
					if (sortedElementData[0] == -1)
						sortedElementData[0] = i;
					else
						sortedElementData[2] = i;
					break;

				case descriptor_CbYCr:
					if (hasAlpha == 0)
						mergedElement.descriptor = descriptor_CbYCr;
					else
						mergedElement.descriptor = descriptor_CbYCrA;

					sortedElementData[0] = i;
					break;

				case descriptor_RGBA:
				case descriptor_ABGR:
				case descriptor_CbYACrYA:
				case descriptor_CbYCrY:
				case descriptor_CbYCrA:
					/* I don't think these ones can be seen in a planar image */
					mergedElement.descriptor = logImage->element[i].descriptor;
					sortedElementData[0] = i;
					break;

				case descriptor_Depth:
				case descriptor_Composite:
					/* Not supported */
					break;
			}
		}

		mergedData = (float *)MEM_mallocN(logImage->width * logImage->height * mergedElement.depth * sizeof(float), __func__);
		if (mergedData == NULL) {
			if (verbose) printf("DPX/Cineon: Cannot allocate mergedData.\n");
			for (i = 0; i < logImage->numElements; i++)
				if (elementData[i] != NULL)
					MEM_freeN(elementData[i]);
			return 1;
		}

		sampleIndex = 0;
		while (sampleIndex < logImage->width * logImage->height * mergedElement.depth) {
			for (i = 0; i < logImage->numElements; i++)
				for (j = 0; j < logImage->element[sortedElementData[i]].depth; j++)
					mergedData[sampleIndex++] = *(elementData_ptr[sortedElementData[i]]++);
		}

		/* Done with elements data, clean-up */
		for (i = 0; i < logImage->numElements; i++)
			if (elementData[i] != NULL)
				MEM_freeN(elementData[i]);

		returnValue = convertLogElementToRGBA(mergedData, data, logImage, mergedElement, dataIsLinearRGB);
		MEM_freeN(mergedData);
	}
	return returnValue;
}

static int logImageElementGetData(LogImageFile *logImage, LogImageElement logElement, float *data)
{
	switch (logElement.bitsPerSample) {
		case 1:
			return logImageElementGetData1(logImage, logElement, data);

		case 8:
			return logImageElementGetData8(logImage, logElement, data);

		case 10:
			if (logElement.packing == 0)
				return logImageElementGetData10Packed(logImage, logElement, data);
			else if (logElement.packing == 1 || logElement.packing == 2)
				return logImageElementGetData10(logImage, logElement, data);

		case 12:
			if (logElement.packing == 0)
				return logImageElementGetData12Packed(logImage, logElement, data);
			else if (logElement.packing == 1 || logElement.packing == 2)
				return logImageElementGetData12(logImage, logElement, data);

		case 16:
			return logImageElementGetData16(logImage, logElement, data);

		default:
			/* format not supported */
			return 1;
	}
}

static int logImageElementGetData1(LogImageFile *logImage, LogImageElement logElement, float *data)
{
	unsigned int pixel;
	int x, y, offset;

	/* seek at the right place */
	if (logimage_fseek(logImage, logElement.dataOffset, SEEK_SET) != 0) {
		if (verbose) printf("DPX/Cineon: Couldn't seek at %d\n", logElement.dataOffset);
		return 1;
	}

	/* read 1 bit data padded to 32 bits */
	for (y = 0; y < logImage->height; y++) {
		for (x = 0; x < logImage->width * logElement.depth; x += 32) {
			if (logimage_read_uint(&pixel, logImage) != 0) {
				if (verbose) printf("DPX/Cineon: EOF reached\n");
				return 1;
			}
			pixel = swap_uint(pixel, logImage->isMSB);
			for (offset = 0; offset < 32 && x + offset < logImage->width; offset++)
				data[y * logImage->width * logElement.depth + x + offset] = (float)((pixel >> offset) & 0x01);
		}
	}
	return 0;
}

static int logImageElementGetData8(LogImageFile *logImage, LogImageElement logElement, float *data)
{
	unsigned int rowLength = getRowLength(logImage->width, logElement);
	unsigned char pixel;
	int x, y;

	/* extract required pixels */
	for (y = 0; y < logImage->height; y++) {
		/* 8 bits are 32-bits padded so we need to seek at each row */
		if (logimage_fseek(logImage, logElement.dataOffset + y * rowLength, SEEK_SET) != 0) {
			if (verbose) printf("DPX/Cineon: Couldn't seek at %d\n", logElement.dataOffset + y * rowLength);
			return 1;
		}

		for (x = 0; x < logImage->width * logElement.depth; x++) {
			if (logimage_read_uchar(&pixel, logImage) != 0) {
				if (verbose) printf("DPX/Cineon: EOF reached\n");
				return 1;
			}
			data[y * logImage->width * logElement.depth + x] = (float)pixel / 255.0f;
		}
	}
	return 0;
}

static int logImageElementGetData10(LogImageFile *logImage, LogImageElement logElement, float *data)
{
	unsigned int pixel;
	int x, y, offset;

	/* seek to data */
	if (logimage_fseek(logImage, logElement.dataOffset, SEEK_SET) != 0) {
		if (verbose) printf("DPX/Cineon: Couldn't seek at %d\n", logElement.dataOffset);
		return 1;
	}

	if (logImage->depth == 1 && logImage->srcFormat == format_DPX) {
		for (y = 0; y < logImage->height; y++) {
			offset = 32;
			for (x = 0; x < logImage->width * logElement.depth; x++) {
				/* we need to read the next long */
				if (offset >= 30) {
					if (logElement.packing == 1)
						offset = 2;
					else if (logElement.packing == 2)
						offset = 0;

					if (logimage_read_uint(&pixel, logImage) != 0) {
						if (verbose) printf("DPX/Cineon: EOF reached\n");
						return 1;
					}
					pixel = swap_uint(pixel, logImage->isMSB);
				}
				data[y * logImage->width * logElement.depth + x] = (float)((pixel >> offset) & 0x3ff) / 1023.0f;
				offset += 10;
			}
		}
	}
	else {
		for (y = 0; y < logImage->height; y++) {
			offset = -1;
			for (x = 0; x < logImage->width * logElement.depth; x++) {
				/* we need to read the next long */
				if (offset < 0) {
					if (logElement.packing == 1)
						offset = 22;
					else if (logElement.packing == 2)
						offset = 20;

					if (logimage_read_uint(&pixel, logImage) != 0) {
						if (verbose) printf("DPX/Cineon: EOF reached\n");
						return 1;
					}
					pixel = swap_uint(pixel, logImage->isMSB);
				}
				data[y * logImage->width * logElement.depth + x] = (float)((pixel >> offset) & 0x3ff) / 1023.0f;
				offset -= 10;
			}
		}
	}

	return 0;
}

static int logImageElementGetData10Packed(LogImageFile *logImage, LogImageElement logElement, float *data)
{
	unsigned int rowLength = getRowLength(logImage->width, logElement);
	unsigned int pixel, oldPixel;
	int offset, offset2, x, y;

	/* converting bytes to pixels */
	for (y = 0; y < logImage->height; y++) {
		/* seek to data */
		if (logimage_fseek(logImage, y * rowLength + logElement.dataOffset, SEEK_SET) != 0) {
			if (verbose) printf("DPX/Cineon: Couldn't seek at %u\n", y * rowLength + logElement.dataOffset);
			return 1;
		}

		oldPixel = 0;
		offset = 0;
		offset2 = 0;

		for (x = 0; x < logImage->width * logElement.depth; x++) {
			if (offset2 != 0) {
				offset = 10 - offset2;
				offset2 = 0;
				oldPixel = 0;
			}
			else if (offset == 32) {
				offset = 0;
			}
			else if (offset + 10 > 32) {
				/* next pixel is on two different longs */
				oldPixel = (pixel >> offset);
				offset2 = 32 - offset;
				offset = 0;
			}

			if (offset == 0) {
				/* we need to read the next long */
				if (logimage_read_uint(&pixel, logImage) != 0) {
					if (verbose) printf("DPX/Cineon: EOF reached\n");
					return 1;
				}
				pixel = swap_uint(pixel, logImage->isMSB);
			}
			data[y * logImage->width * logElement.depth + x] = (float)((((pixel << offset2) >> offset) & 0x3ff) | oldPixel) / 1023.0f;
			offset += 10;
		}
	}
	return 0;
}

static int logImageElementGetData12(LogImageFile *logImage, LogImageElement logElement, float *data)
{
	unsigned int sampleIndex;
	unsigned int numSamples = logImage->width * logImage->height * logElement.depth;
	unsigned short pixel;

	/* seek to data */
	if (logimage_fseek(logImage, logElement.dataOffset, SEEK_SET) != 0) {
		if (verbose) printf("DPX/Cineon: Couldn't seek at %d\n", logElement.dataOffset);
		return 1;
	}

	/* convert bytes to pixels */
	sampleIndex = 0;

	for (sampleIndex = 0; sampleIndex < numSamples; sampleIndex++) {
		if (logimage_read_ushort(&pixel, logImage) != 0) {
			if (verbose) printf("DPX/Cineon: EOF reached\n");
			return 1;
		}
		pixel = swap_ushort(pixel, logImage->isMSB);

		if (logElement.packing == 1) /* padded to the right */
			data[sampleIndex] = (float)(pixel >> 4) / 4095.0f;
		else if (logElement.packing == 2) /* padded to the left */
			data[sampleIndex] = (float)pixel / 4095.0f;
	}
	return 0;
}

static int logImageElementGetData12Packed(LogImageFile *logImage, LogImageElement logElement, float *data)
{
	unsigned int rowLength = getRowLength(logImage->width, logElement);
	unsigned int pixel, oldPixel;
	int offset, offset2, x, y;

	/* converting bytes to pixels */
	for (y = 0; y < logImage->height; y++) {
		/* seek to data */
		if (logimage_fseek(logImage, y * rowLength + logElement.dataOffset, SEEK_SET) != 0) {
			if (verbose) printf("DPX/Cineon: Couldn't seek at %u\n", y * rowLength + logElement.dataOffset);
			return 1;
		}

		oldPixel = 0;
		offset = 0;
		offset2 = 0;

		for (x = 0; x < logImage->width * logElement.depth; x++) {
			if (offset2 != 0) {
				offset = 12 - offset2;
				offset2 = 0;
				oldPixel = 0;
			}
			else if (offset == 32) {
				offset = 0;
			}
			else if (offset + 12 > 32) {
				/* next pixel is on two different longs */
				oldPixel = (pixel >> offset);
				offset2 = 32 - offset;
				offset = 0;
			}

			if (offset == 0) {
				/* we need to read the next long */
				if (logimage_read_uint(&pixel, logImage) != 0) {
					if (verbose) printf("DPX/Cineon: EOF reached\n");
					return 1;
				}
				pixel = swap_uint(pixel, logImage->isMSB);
			}
			data[y * logImage->width * logElement.depth + x] = (float)((((pixel << offset2) >> offset) & 0xfff) | oldPixel) / 4095.0f;
			offset += 12;
		}
	}
	return 0;
}

static int logImageElementGetData16(LogImageFile *logImage, LogImageElement logElement, float *data)
{
	unsigned int numSamples = logImage->width * logImage->height * logElement.depth;
	unsigned int sampleIndex;
	unsigned short pixel;

	/* seek to data */
	if (logimage_fseek(logImage, logElement.dataOffset, SEEK_SET) != 0) {
		if (verbose) printf("DPX/Cineon: Couldn't seek at %d\n", logElement.dataOffset);
		return 1;
	}

	for (sampleIndex = 0; sampleIndex < numSamples; sampleIndex++) {
		if (logimage_read_ushort(&pixel, logImage) != 0) {
			if (verbose) printf("DPX/Cineon: EOF reached\n");
			return 1;
		}
		pixel = swap_ushort(pixel, logImage->isMSB);
		data[sampleIndex] = (float)pixel / 65535.0f;
	}

	return 0;
}


/*
 * Color conversion
 */

static int getYUVtoRGBMatrix(float *matrix, LogImageElement logElement)
{
	float scaleY, scaleCbCr;
	float refHighData = (float)logElement.refHighData / logElement.maxValue;
	float refLowData = (float)logElement.refLowData / logElement.maxValue;

	scaleY = 1.0f / (refHighData - refLowData);
	scaleCbCr = scaleY * ((940.0f - 64.0f) / (960.0f - 64.0f));

	switch (logElement.transfer) {
		case 2: /* linear */
			matrix[0] =  1.0f * scaleY;
			matrix[1] =  1.0f * scaleCbCr;
			matrix[2] =  1.0f * scaleCbCr;
			matrix[3] =  1.0f * scaleY;
			matrix[4] =  1.0f * scaleCbCr;
			matrix[5] =  1.0f * scaleCbCr;
			matrix[6] =  1.0f * scaleY;
			matrix[7] =  1.0f * scaleCbCr;
			matrix[8] =  1.0f * scaleCbCr;
			return 0;

		case 5: /* SMPTE 240M */
			matrix[0] =  1.0000f * scaleY;
			matrix[1] =  0.0000f * scaleCbCr;
			matrix[2] =  1.5756f * scaleCbCr;
			matrix[3] =  1.0000f * scaleY;
			matrix[4] = -0.2253f * scaleCbCr;
			matrix[5] = -0.5000f * scaleCbCr;
			matrix[6] =  1.0000f * scaleY;
			matrix[7] =  1.8270f * scaleCbCr;
			matrix[8] =  0.0000f * scaleCbCr;
			return 0;

		case 6: /* CCIR 709-1 */
			matrix[0] =  1.000000f * scaleY;
			matrix[1] =  0.000000f * scaleCbCr;
			matrix[2] =  1.574800f * scaleCbCr;
			matrix[3] =  1.000000f * scaleY;
			matrix[4] = -0.187324f * scaleCbCr;
			matrix[5] = -0.468124f * scaleCbCr;
			matrix[6] =  1.000000f * scaleY;
			matrix[7] =  1.855600f * scaleCbCr;
			matrix[8] =  0.000000f * scaleCbCr;
			return 0;

		case 7: /* CCIR 601 */
		case 8: /* I'm not sure 7 and 8 should share the same matrix */
			matrix[0] =  1.000000f * scaleY;
			matrix[1] =  0.000000f * scaleCbCr;
			matrix[2] =  1.402000f * scaleCbCr;
			matrix[3] =  1.000000f * scaleY;
			matrix[4] = -0.344136f * scaleCbCr;
			matrix[5] = -0.714136f * scaleCbCr;
			matrix[6] =  1.000000f * scaleY;
			matrix[7] =  1.772000f * scaleCbCr;
			matrix[8] =  0.000000f * scaleCbCr;
			return 0;

		default:
			return 1;
	}
}

static float *getLinToLogLut(LogImageFile *logImage, LogImageElement logElement)
{
	float *lut;
	float gain, negativeFilmGamma, offset, step;
	unsigned int lutsize = (unsigned int)(logElement.maxValue + 1);
	unsigned int i;
	
	lut = MEM_mallocN(sizeof(float) * lutsize, "getLinToLogLut");

	negativeFilmGamma = 0.6;
	step = logElement.refHighQuantity / logElement.maxValue;
	gain = logElement.maxValue / (1.0f - powf(10, (logImage->referenceBlack - logImage->referenceWhite) * step / negativeFilmGamma * logImage->gamma / 1.7f));
	offset = gain - logElement.maxValue;

	for (i = 0; i < lutsize; i++)
		lut[i] = (logImage->referenceWhite + log10f(powf((i + offset) / gain, 1.7f / logImage->gamma)) / (step / negativeFilmGamma)) / logElement.maxValue;
	
	return lut;
}

static float *getLogToLinLut(LogImageFile *logImage, LogImageElement logElement)
{
	float *lut;
	float breakPoint, gain, kneeGain, kneeOffset, negativeFilmGamma, offset, step, softClip;
	/* float filmGamma; unused */
	unsigned int lutsize = (unsigned int)(logElement.maxValue + 1);
	unsigned int i;
	
	lut = MEM_mallocN(sizeof(float) * lutsize, "getLogToLinLut");

	/* Building the Log -> Lin LUT */
	step = logElement.refHighQuantity / logElement.maxValue;
	negativeFilmGamma = 0.6;

	/* these are default values */
	/* filmGamma = 2.2f;  unused */
	softClip = 0;

	breakPoint = logImage->referenceWhite - softClip;
	gain = logElement.maxValue / (1.0f - powf(10, (logImage->referenceBlack - logImage->referenceWhite) * step / negativeFilmGamma * logImage->gamma / 1.7f));
	offset = gain - logElement.maxValue;
	kneeOffset = powf(10, (breakPoint - logImage->referenceWhite) * step / negativeFilmGamma * logImage->gamma / 1.7f) * gain - offset;
	kneeGain = (logElement.maxValue - kneeOffset) / powf(5 * softClip, softClip / 100);

	for (i = 0; i < lutsize; i++) {
		if (i < logImage->referenceBlack)
			lut[i] = 0.0f;
		else if (i > breakPoint)
			lut[i] = (powf(i - breakPoint, softClip / 100) * kneeGain + kneeOffset) / logElement.maxValue;
		else
			lut[i] = (powf(10, ((float)i - logImage->referenceWhite) * step / negativeFilmGamma * logImage->gamma / 1.7f) * gain - offset) / logElement.maxValue;
	}

	return lut;
}

static float *getLinToSrgbLut(LogImageElement logElement)
{
	float col, *lut;
	unsigned int lutsize = (unsigned int)(logElement.maxValue + 1);
	unsigned int i;

	lut = MEM_mallocN(sizeof(float) * lutsize, "getLogToLinLut");

	for (i = 0; i < lutsize; i++) {
		col = (float)i / logElement.maxValue;
		if (col < 0.0031308f)
			lut[i] = (col < 0.0f) ? 0.0f : col * 12.92f;
		else
			lut[i] = 1.055f * powf(col, 1.0f / 2.4f) - 0.055f;
	}

	return lut;
}

static float *getSrgbToLinLut(LogImageElement logElement)
{
	float col, *lut;
	unsigned int lutsize = (unsigned int)(logElement.maxValue + 1);
	unsigned int i;

	lut = MEM_mallocN(sizeof(float) * lutsize, "getLogToLinLut");

	for (i = 0; i < lutsize; i++) {
		col = (float)i / logElement.maxValue;
		if (col < 0.04045f)
			lut[i] = (col < 0.0f) ? 0.0f : col * (1.0f / 12.92f);
		else
			lut[i] = powf((col + 0.055f) * (1.0f / 1.055f), 2.4f);
	}

	return lut;
}

static int convertRGBA_RGB(float *src, float *dst, LogImageFile *logImage,
                           LogImageElement logElement, int elementIsSource)
{
	unsigned int i;
	float *src_ptr = src;
	float *dst_ptr = dst;

	switch (logElement.transfer) {
		case transfer_UserDefined:
		case transfer_Linear:
		case transfer_Logarithmic: {
			for (i = 0; i < logImage->width * logImage->height; i++) {
				*(dst_ptr++) = *(src_ptr++);
				*(dst_ptr++) = *(src_ptr++);
				*(dst_ptr++) = *(src_ptr++);
				src_ptr++;
			}

			return 0;
		}

		case transfer_PrintingDensity: {
			float *lut;

			if (elementIsSource == 1)
				lut = getLogToLinLut(logImage, logElement);
			else
				lut = getLinToLogLut(logImage, logElement);

			for (i = 0; i < logImage->width * logImage->height; i++) {
				*(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
				*(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
				*(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
				src_ptr++;
			}

			MEM_freeN(lut);

			return 0;
		}

		default:
			return 1;
	}
}

static int convertRGB_RGBA(float *src, float *dst, LogImageFile *logImage,
                           LogImageElement logElement, int elementIsSource)
{
	unsigned int i;
	float *src_ptr = src;
	float *dst_ptr = dst;

	switch (logElement.transfer) {
		case transfer_UserDefined:
		case transfer_Linear:
		case transfer_Logarithmic: {
			for (i = 0; i < logImage->width * logImage->height; i++) {
				*(dst_ptr++) = *(src_ptr++);
				*(dst_ptr++) = *(src_ptr++);
				*(dst_ptr++) = *(src_ptr++);
				*(dst_ptr++) = 1.0f;
			}

			return 0;
		}

		case transfer_PrintingDensity: {
			float *lut;

			if (elementIsSource == 1)
				lut = getLogToLinLut(logImage, logElement);
			else
				lut = getLinToLogLut(logImage, logElement);

			for (i = 0; i < logImage->width * logImage->height; i++) {
				*(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
				*(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
				*(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
				*(dst_ptr++) = 1.0f;
			}

			MEM_freeN(lut);

			return 0;
		}

		default:
			return 1;
	}
}

static int convertRGBA_RGBA(float *src, float *dst, LogImageFile *logImage,
                            LogImageElement logElement, int elementIsSource)
{
	unsigned int i;
	float *src_ptr = src;
	float *dst_ptr = dst;

	switch (logElement.transfer) {
		case transfer_UserDefined:
		case transfer_Linear:
		case transfer_Logarithmic: {
			memcpy(dst, src, 4 * logImage->width * logImage->height * sizeof(float));
			return 0;
		}

		case transfer_PrintingDensity: {
			float *lut;

			if (elementIsSource == 1)
				lut = getLogToLinLut(logImage, logElement);
			else
				lut = getLinToLogLut(logImage, logElement);

			for (i = 0; i < logImage->width * logImage->height; i++) {
				*(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
				*(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
				*(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
				*(dst_ptr++) = *(src_ptr++);
			}

			MEM_freeN(lut);

			return 0;
		}

		default:
			return 1;
	}
}

static int convertABGR_RGBA(float *src, float *dst, LogImageFile *logImage,
                            LogImageElement logElement, int elementIsSource)
{
	unsigned int i;
	float *src_ptr = src;
	float *dst_ptr = dst;

	switch (logElement.transfer) {
		case transfer_UserDefined:
		case transfer_Linear:
		case transfer_Logarithmic: {
			for (i = 0; i < logImage->width * logImage->height; i++) {
				src_ptr += 4;
				*(dst_ptr++) = *(src_ptr--);
				*(dst_ptr++) = *(src_ptr--);
				*(dst_ptr++) = *(src_ptr--);
				*(dst_ptr++) = *(src_ptr--);
				src_ptr += 4;
			}
			return 0;
		}

		case transfer_PrintingDensity: {
			float *lut;

			if (elementIsSource == 1)
				lut = getLogToLinLut(logImage, logElement);
			else
				lut = getLinToLogLut(logImage, logElement);

			for (i = 0; i < logImage->width * logImage->height; i++) {
				src_ptr += 4;
				*(dst_ptr++) = lut[float_uint(*(src_ptr--), logElement.maxValue)];
				*(dst_ptr++) = lut[float_uint(*(src_ptr--), logElement.maxValue)];
				*(dst_ptr++) = lut[float_uint(*(src_ptr--), logElement.maxValue)];
				*(dst_ptr++) = *(src_ptr--);
				src_ptr += 4;
			}

			MEM_freeN(lut);

			return 0;
		}

		default:
			return 1;
	}
}

static int convertCbYCr_RGBA(float *src, float *dst, LogImageFile *logImage, LogImageElement logElement)
{
	unsigned int i;
	float conversionMatrix[9], refLowData, y, cb, cr;
	float *src_ptr = src;
	float *dst_ptr = dst;

	if (getYUVtoRGBMatrix((float *)&conversionMatrix, logElement) != 0)
		return 1;

	refLowData = (float)logElement.refLowData / logElement.maxValue;

	for (i = 0; i < logImage->width * logImage->height; i++) {
		cb = *(src_ptr++) - 0.5f;
		y = *(src_ptr++) - refLowData;
		cr = *(src_ptr++) - 0.5f;

		*(dst_ptr++) = clamp_float(y * conversionMatrix[0] + cb * conversionMatrix[1] + cr * conversionMatrix[2], 0.0f, 1.0f);
		*(dst_ptr++) = clamp_float(y * conversionMatrix[3] + cb * conversionMatrix[4] + cr * conversionMatrix[5], 0.0f, 1.0f);
		*(dst_ptr++) = clamp_float(y * conversionMatrix[6] + cb * conversionMatrix[7] + cr * conversionMatrix[8], 0.0f, 1.0f);
		*(dst_ptr++) = 1.0f;
	}
	return 0;
}

static int convertCbYCrA_RGBA(float *src, float *dst, LogImageFile *logImage, LogImageElement logElement)
{
	unsigned int i;
	float conversionMatrix[9], refLowData, y, cb, cr, a;
	float *src_ptr = src;
	float *dst_ptr = dst;

	if (getYUVtoRGBMatrix((float *)&conversionMatrix, logElement) != 0)
		return 1;

	refLowData = (float)logElement.refLowData / logElement.maxValue;

	for (i = 0; i < logImage->width * logImage->height; i++) {
		cb = *(src_ptr++) - 0.5f;
		y = *(src_ptr++) - refLowData;
		cr = *(src_ptr++) - 0.5f;
		a = *(src_ptr++);

		*(dst_ptr++) = clamp_float(y * conversionMatrix[0] + cb * conversionMatrix[1] + cr * conversionMatrix[2], 0.0f, 1.0f);
		*(dst_ptr++) = clamp_float(y * conversionMatrix[3] + cb * conversionMatrix[4] + cr * conversionMatrix[5], 0.0f, 1.0f);
		*(dst_ptr++) = clamp_float(y * conversionMatrix[6] + cb * conversionMatrix[7] + cr * conversionMatrix[8], 0.0f, 1.0f);
		*(dst_ptr++) = a;
	}
	return 0;
}

static int convertCbYCrY_RGBA(float *src, float *dst, LogImageFile *logImage, LogImageElement logElement)
{
	unsigned int i;
	float conversionMatrix[9], refLowData, y1, y2, cb, cr;
	float *src_ptr = src;
	float *dst_ptr = dst;

	if (getYUVtoRGBMatrix((float *)&conversionMatrix, logElement) != 0)
		return 1;

	refLowData = (float)logElement.refLowData / logElement.maxValue;

	for (i = 0; i < logImage->width * logImage->height / 2; i++) {
		cb = *(src_ptr++) - 0.5f;
		y1 = *(src_ptr++) - refLowData;
		cr = *(src_ptr++) - 0.5f;
		y2 = *(src_ptr++) - refLowData;

		*(dst_ptr++) = clamp_float(y1 * conversionMatrix[0] + cb * conversionMatrix[1] + cr * conversionMatrix[2], 0.0f, 1.0f);
		*(dst_ptr++) = clamp_float(y1 * conversionMatrix[3] + cb * conversionMatrix[4] + cr * conversionMatrix[5], 0.0f, 1.0f);
		*(dst_ptr++) = clamp_float(y1 * conversionMatrix[6] + cb * conversionMatrix[7] + cr * conversionMatrix[8], 0.0f, 1.0f);
		*(dst_ptr++) = 1.0f;
		*(dst_ptr++) = clamp_float(y2 * conversionMatrix[0] + cb * conversionMatrix[1] + cr * conversionMatrix[2], 0.0f, 1.0f);
		*(dst_ptr++) = clamp_float(y2 * conversionMatrix[3] + cb * conversionMatrix[4] + cr * conversionMatrix[5], 0.0f, 1.0f);
		*(dst_ptr++) = clamp_float(y2 * conversionMatrix[6] + cb * conversionMatrix[7] + cr * conversionMatrix[8], 0.0f, 1.0f);
		*(dst_ptr++) = 1.0f;
	}
	return 0;
}

static int convertCbYACrYA_RGBA(float *src, float *dst, LogImageFile *logImage, LogImageElement logElement)
{
	unsigned int i;
	float conversionMatrix[9], refLowData, y1, y2, cb, cr, a1, a2;
	float *src_ptr = src;
	float *dst_ptr = dst;

	if (getYUVtoRGBMatrix((float *)&conversionMatrix, logElement) != 0)
		return 1;

	refLowData = (float)logElement.refLowData / logElement.maxValue;

	for (i = 0; i < logImage->width * logImage->height / 2; i++) {
		cb = *(src_ptr++) - 0.5f;
		y1 = *(src_ptr++) - refLowData;
		a1 = *(src_ptr++);
		cr = *(src_ptr++) - 0.5f;
		y2 = *(src_ptr++) - refLowData;
		a2 = *(src_ptr++);

		*(dst_ptr++) = clamp_float(y1 * conversionMatrix[0] + cb * conversionMatrix[1] + cr * conversionMatrix[2], 0.0f, 1.0f);
		*(dst_ptr++) = clamp_float(y1 * conversionMatrix[3] + cb * conversionMatrix[4] + cr * conversionMatrix[5], 0.0f, 1.0f);
		*(dst_ptr++) = clamp_float(y1 * conversionMatrix[6] + cb * conversionMatrix[7] + cr * conversionMatrix[8], 0.0f, 1.0f);
		*(dst_ptr++) = a1;
		*(dst_ptr++) = clamp_float(y2 * conversionMatrix[0] + cb * conversionMatrix[1] + cr * conversionMatrix[2], 0.0f, 1.0f);
		*(dst_ptr++) = clamp_float(y2 * conversionMatrix[3] + cb * conversionMatrix[4] + cr * conversionMatrix[5], 0.0f, 1.0f);
		*(dst_ptr++) = clamp_float(y2 * conversionMatrix[6] + cb * conversionMatrix[7] + cr * conversionMatrix[8], 0.0f, 1.0f);
		*(dst_ptr++) = a2;
	}
	return 0;
}

static int convertLuminance_RGBA(float *src, float *dst, LogImageFile *logImage, LogImageElement logElement)
{
	unsigned int i;
	float conversionMatrix[9], value, refLowData;
	float *src_ptr = src;
	float *dst_ptr = dst;

	if (getYUVtoRGBMatrix((float *)&conversionMatrix, logElement) != 0)
		return 1;

	refLowData = (float)logElement.refLowData / logElement.maxValue;

	for (i = 0; i < logImage->width * logImage->height; i++) {
		value = clamp_float((*(src_ptr++) - refLowData) * conversionMatrix[0], 0.0f, 1.0f);
		*(dst_ptr++) = value;
		*(dst_ptr++) = value;
		*(dst_ptr++) = value;
		*(dst_ptr++) = 1.0f;
	}
	return 0;
}

static int convertYA_RGBA(float *src, float *dst, LogImageFile *logImage, LogImageElement logElement)
{
	unsigned int i;
	float conversionMatrix[9], value, refLowData;
	float *src_ptr = src;
	float *dst_ptr = dst;

	if (getYUVtoRGBMatrix((float *)&conversionMatrix, logElement) != 0)
		return 1;

	refLowData = (float)logElement.refLowData / logElement.maxValue;

	for (i = 0; i < logImage->width * logImage->height; i++) {
		value = clamp_float((*(src_ptr++) - refLowData) * conversionMatrix[0], 0.0f, 1.0f);
		*(dst_ptr++) = value;
		*(dst_ptr++) = value;
		*(dst_ptr++) = value;
		*(dst_ptr++) = *(src_ptr++);
	}
	return 0;
}

static int convertLogElementToRGBA(float *src, float *dst, LogImageFile *logImage,
                                   LogImageElement logElement, int dstIsLinearRGB)
{
	int rvalue;
	unsigned int i;
	float *src_ptr;
	float *dst_ptr;

	/* Convert data in src to linear RGBA in dst */
	switch (logElement.descriptor) {
		case descriptor_RGB:
			rvalue = convertRGB_RGBA(src, dst, logImage, logElement, 1);
			break;

		case descriptor_RGBA:
			rvalue = convertRGBA_RGBA(src, dst, logImage, logElement, 1);
			break;

		case descriptor_ABGR:
			rvalue = convertABGR_RGBA(src, dst, logImage, logElement, 1);
			break;

		case descriptor_Luminance:
			rvalue = convertLuminance_RGBA(src, dst, logImage, logElement);
			break;

		case descriptor_CbYCr:
			rvalue = convertCbYCr_RGBA(src, dst, logImage, logElement);
			break;

		case descriptor_CbYCrY:
			rvalue = convertCbYCrY_RGBA(src, dst, logImage, logElement);
			break;

		case descriptor_CbYACrYA:
			rvalue = convertCbYACrYA_RGBA(src, dst, logImage, logElement);
			break;

		case descriptor_CbYCrA:
			rvalue = convertCbYCrA_RGBA(src, dst, logImage, logElement);
			break;

		case descriptor_YA: /* this descriptor is for internal use only */
			rvalue = convertYA_RGBA(src, dst, logImage, logElement);
			break;

		default:
			return 1;
	}

	if (rvalue == 1)
		return 1;
	else if (dstIsLinearRGB) {
		/* convert data from sRGB to Linear RGB via lut */
		float *lut = getSrgbToLinLut(logElement);
		src_ptr = dst; // no error here
		dst_ptr = dst;
		for (i = 0; i < logImage->width * logImage->height; i++) {
			*(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
			*(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
			*(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
			dst_ptr++; src_ptr++;
		}
		MEM_freeN(lut);
	}
	return 0;
}

static int convertRGBAToLogElement(float *src, float *dst, LogImageFile *logImage,
                                   LogImageElement logElement, int srcIsLinearRGB)
{
	unsigned int i;
	int rvalue;
	float *srgbSrc;
	float *srgbSrc_ptr;
	float *src_ptr = src;
	float *lut;

	if (srcIsLinearRGB != 0) {
		/* we need to convert src to sRGB */
		srgbSrc = (float *)MEM_mallocN(4 * logImage->width * logImage->height * sizeof(float), __func__);
		if (srgbSrc == NULL)
			return 1;

		memcpy(srgbSrc, src, 4 * logImage->width * logImage->height * sizeof(float));
		srgbSrc_ptr = srgbSrc;

		/* convert data from Linear RGB to sRGB via lut */
		lut = getLinToSrgbLut(logElement);
		for (i = 0; i < logImage->width * logImage->height; i++) {
			*(srgbSrc_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
			*(srgbSrc_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
			*(srgbSrc_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
			srgbSrc_ptr++; src_ptr++;
		}
		MEM_freeN(lut);
	}
	else
		srgbSrc = src;

	/* Convert linear RGBA data in src to format described by logElement in dst */
	switch (logElement.descriptor) {
		case descriptor_RGB:
			rvalue = convertRGBA_RGB(srgbSrc, dst, logImage, logElement, 0);
			break;

		case descriptor_RGBA:
			rvalue = convertRGBA_RGBA(srgbSrc, dst, logImage, logElement, 0);
			break;

		/* these ones are not supported for the moment */
		case descriptor_ABGR:
		case descriptor_Luminance:
		case descriptor_CbYCr:
		case descriptor_CbYCrY:
		case descriptor_CbYACrYA:
		case descriptor_CbYCrA:
		case descriptor_YA: /* this descriptor is for internal use only */
		default:
			rvalue = 1;
			break;
	}

	if (srcIsLinearRGB != 0) {
		MEM_freeN(srgbSrc);
	}

	return rvalue;
}
