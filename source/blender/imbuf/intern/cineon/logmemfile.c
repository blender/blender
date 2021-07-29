/*
 * Cineon image file format library routines.
 *
 * Copyright 2006 Joseph Eagar (joeedh@gmail.com)
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

/** \file blender/imbuf/intern/cineon/logmemfile.c
 *  \ingroup imbcineon
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logImageCore.h"
#include "logmemfile.h"

int logimage_fseek(LogImageFile *logFile, intptr_t offset, int origin)
{
	if (logFile->file)
		fseek(logFile->file, offset, origin);
	else { /* we're seeking in memory */
		if (origin == SEEK_SET) {
			if (offset > logFile->memBufferSize)
				return 1;
			logFile->memCursor = logFile->memBuffer + offset;
		}
		else if (origin == SEEK_END) {
			if (offset > logFile->memBufferSize)
				return 1;
			logFile->memCursor = (logFile->memBuffer + logFile->memBufferSize) - offset;
		}
		else if (origin == SEEK_CUR) {
			uintptr_t pos = (uintptr_t)logFile->memCursor - (uintptr_t)logFile->memBuffer;
			if (pos + offset > logFile->memBufferSize)
				return 1;

			logFile->memCursor += offset;
		}
	}
	return 0;
}

int logimage_fwrite(void *buffer, size_t size, unsigned int count, LogImageFile *logFile)
{
	if (logFile->file)
		return fwrite(buffer, size, count, logFile->file);
	else { /* we're writing to memory */
		/* do nothing as this isn't supported yet */
		return count;
	}
}

int logimage_fread(void *buffer, size_t size, unsigned int count, LogImageFile *logFile)
{
	if (logFile->file) {
		return fread(buffer, size, count, logFile->file);
	}
	else { /* we're reading from memory */
		unsigned char *buf = (unsigned char *)buffer;
		uintptr_t pos = (uintptr_t)logFile->memCursor - (uintptr_t)logFile->memBuffer;
		size_t total_size = size * count;
		if (pos + total_size > logFile->memBufferSize) {
			/* how many elements can we read without overflow ? */
			count = (logFile->memBufferSize - pos) / size;
			/* recompute the size */
			total_size = size * count;
		}

		if (total_size != 0)
			memcpy(buf, logFile->memCursor, total_size);

		return count;
	}
}

int logimage_read_uchar(unsigned char *x, LogImageFile *logFile)
{
	uintptr_t pos = (uintptr_t)logFile->memCursor - (uintptr_t)logFile->memBuffer;
	if (pos + sizeof(unsigned char) > logFile->memBufferSize)
		return 1;

	*x = *(unsigned char *)logFile->memCursor;
	logFile->memCursor += sizeof(unsigned char);
	return 0;
}

int logimage_read_ushort(unsigned short *x, LogImageFile *logFile)
{
	uintptr_t pos = (uintptr_t)logFile->memCursor - (uintptr_t)logFile->memBuffer;
	if (pos + sizeof(unsigned short) > logFile->memBufferSize)
		return 1;

	*x = *(unsigned short *)logFile->memCursor;
	logFile->memCursor += sizeof(unsigned short);
	return 0;
}

int logimage_read_uint(unsigned int *x, LogImageFile *logFile)
{
	uintptr_t pos = (uintptr_t)logFile->memCursor - (uintptr_t)logFile->memBuffer;
	if (pos + sizeof(unsigned int) > logFile->memBufferSize)
		return 1;

	*x = *(unsigned int *)logFile->memCursor;
	logFile->memCursor += sizeof(unsigned int);
	return 0;
}
