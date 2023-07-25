/* SPDX-FileCopyrightText: 2006 Joseph Eagar <joeedh@gmail.com>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbcineon
 *
 * Cineon image file format library routines.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

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
      uintptr_t pos = uintptr_t(logFile->memCursor) - uintptr_t(logFile->memBuffer);
      if (pos + offset > logFile->memBufferSize) {
        return 1;
      }

      logFile->memCursor += offset;
    }
  }
  return 0;
}

int logimage_fwrite(void *buffer, size_t size, uint count, LogImageFile *logFile)
{
  if (logFile->file) {
    return fwrite(buffer, size, count, logFile->file);
  }
  /* we're writing to memory */
  /* do nothing as this isn't supported yet */
  return count;
}

int logimage_fread(void *buffer, size_t size, uint count, LogImageFile *logFile)
{
  if (logFile->file) {
    return fread(buffer, size, count, logFile->file);
  }
  /* we're reading from memory */
  uchar *buf = (uchar *)buffer;
  uintptr_t pos = uintptr_t(logFile->memCursor) - uintptr_t(logFile->memBuffer);
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

int logimage_read_uchar(uchar *x, LogImageFile *logFile)
{
  uintptr_t pos = uintptr_t(logFile->memCursor) - uintptr_t(logFile->memBuffer);
  if (pos + sizeof(uchar) > logFile->memBufferSize) {
    return 1;
  }

  *x = *(uchar *)logFile->memCursor;
  logFile->memCursor += sizeof(uchar);
  return 0;
}

int logimage_read_ushort(ushort *x, LogImageFile *logFile)
{
  uintptr_t pos = uintptr_t(logFile->memCursor) - uintptr_t(logFile->memBuffer);
  if (pos + sizeof(ushort) > logFile->memBufferSize) {
    return 1;
  }

  *x = *(ushort *)logFile->memCursor;
  logFile->memCursor += sizeof(ushort);
  return 0;
}

int logimage_read_uint(uint *x, LogImageFile *logFile)
{
  uintptr_t pos = uintptr_t(logFile->memCursor) - uintptr_t(logFile->memBuffer);
  if (pos + sizeof(uint) > logFile->memBufferSize) {
    return 1;
  }

  *x = *(uint *)logFile->memCursor;
  logFile->memCursor += sizeof(uint);
  return 0;
}
