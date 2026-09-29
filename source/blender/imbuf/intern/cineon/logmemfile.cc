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

namespace blender {

int logimage_fseek(LogImageFile *logFile, uintptr_t offset)
{
  if (offset > logFile->memBufferSize) {
    return 1;
  }
  logFile->memCursor = logFile->memBuffer + offset;
  return 0;
}

int logimage_fwrite(const void *buffer, size_t size, uint count, LogImageFile *logFile)
{
  return fwrite(buffer, size, count, logFile->file);
}

int logimage_fread(void *buffer, size_t size, uint count, LogImageFile *logFile)
{
  uchar *buf = static_cast<uchar *>(buffer);
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

  *x = *static_cast<uchar *>(logFile->memCursor);
  logFile->memCursor += sizeof(uchar);
  return 0;
}

int logimage_read_ushort(ushort *x, LogImageFile *logFile)
{
  uintptr_t pos = uintptr_t(logFile->memCursor) - uintptr_t(logFile->memBuffer);
  if (pos + sizeof(ushort) > logFile->memBufferSize) {
    return 1;
  }

  *x = *reinterpret_cast<ushort *>(logFile->memCursor);
  logFile->memCursor += sizeof(ushort);
  return 0;
}

int logimage_read_uint(uint *x, LogImageFile *logFile)
{
  uintptr_t pos = uintptr_t(logFile->memCursor) - uintptr_t(logFile->memBuffer);
  if (pos + sizeof(uint) > logFile->memBufferSize) {
    return 1;
  }

  *x = *reinterpret_cast<uint *>(logFile->memCursor);
  logFile->memCursor += sizeof(uint);
  return 0;
}

}  // namespace blender
