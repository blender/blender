/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Joseph Eagar <joeedh@gmail.com>. */

/** \file
 * \ingroup imbcineon
 *
 * Cineon image file format library routines.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logImageCore.h"
#include "logmemfile.h"

int logimage_fseek(LogImageFile *logFile, intptr_t offset, int origin)
{
  if (logFile->file) {
    fseek(logFile->file, offset, origin);
  }
  else { /* we're seeking in memory */
    if (origin == SEEK_SET) {
      if (offset > logFile->memBufferSize) {
        return 1;
      }
      logFile->memCursor = logFile->memBuffer + offset;
    }
    else if (origin == SEEK_END) {
      if (offset > logFile->memBufferSize) {
        return 1;
      }
      logFile->memCursor = (logFile->memBuffer + logFile->memBufferSize) - offset;
    }
    else if (origin == SEEK_CUR) {
      uintptr_t pos = (uintptr_t)logFile->memCursor - (uintptr_t)logFile->memBuffer;
      if (pos + offset > logFile->memBufferSize) {
        return 1;
      }

      logFile->memCursor += offset;
    }
  }
  return 0;
}

int logimage_fwrite(void *buffer, size_t size, unsigned int count, LogImageFile *logFile)
{
  if (logFile->file) {
    return fwrite(buffer, size, count, logFile->file);
  }
  /* we're writing to memory */
  /* do nothing as this isn't supported yet */
  return count;
}

int logimage_fread(void *buffer, size_t size, unsigned int count, LogImageFile *logFile)
{
  if (logFile->file) {
    return fread(buffer, size, count, logFile->file);
  }
  /* we're reading from memory */
  unsigned char *buf = (unsigned char *)buffer;
  uintptr_t pos = (uintptr_t)logFile->memCursor - (uintptr_t)logFile->memBuffer;
  size_t total_size = size * count;
  if (pos + total_size > logFile->memBufferSize) {
    /* how many elements can we read without overflow ? */
    count = (logFile->memBufferSize - pos) / size;
    /* recompute the size */
    total_size = size * count;
  }

  if (total_size != 0) {
    memcpy(buf, logFile->memCursor, total_size);
  }

  return count;
}

int logimage_read_uchar(unsigned char *x, LogImageFile *logFile)
{
  uintptr_t pos = (uintptr_t)logFile->memCursor - (uintptr_t)logFile->memBuffer;
  if (pos + sizeof(unsigned char) > logFile->memBufferSize) {
    return 1;
  }

  *x = *(unsigned char *)logFile->memCursor;
  logFile->memCursor += sizeof(unsigned char);
  return 0;
}

int logimage_read_ushort(unsigned short *x, LogImageFile *logFile)
{
  uintptr_t pos = (uintptr_t)logFile->memCursor - (uintptr_t)logFile->memBuffer;
  if (pos + sizeof(unsigned short) > logFile->memBufferSize) {
    return 1;
  }

  *x = *(unsigned short *)logFile->memCursor;
  logFile->memCursor += sizeof(unsigned short);
  return 0;
}

int logimage_read_uint(unsigned int *x, LogImageFile *logFile)
{
  uintptr_t pos = (uintptr_t)logFile->memCursor - (uintptr_t)logFile->memBuffer;
  if (pos + sizeof(unsigned int) > logFile->memBufferSize) {
    return 1;
  }

  *x = *(unsigned int *)logFile->memCursor;
  logFile->memCursor += sizeof(unsigned int);
  return 0;
}
