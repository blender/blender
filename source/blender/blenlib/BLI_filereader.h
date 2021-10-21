/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bli
 * \brief Wrapper for reading from various sources (e.g. raw files, compressed files, memory...).
 */

#pragma once

#ifdef WIN32
#  include "BLI_winstuff.h"
#else
#  include <sys/types.h>
#endif

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

#if defined(_MSC_VER) || defined(__APPLE__) || defined(__HAIKU__) || defined(__NetBSD__)
typedef int64_t off64_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct FileReader;

typedef ssize_t (*FileReaderReadFn)(struct FileReader *reader, void *buffer, size_t size);
typedef off64_t (*FileReaderSeekFn)(struct FileReader *reader, off64_t offset, int whence);
typedef void (*FileReaderCloseFn)(struct FileReader *reader);

/* General structure for all FileReaders, implementations add custom fields at the end. */
typedef struct FileReader {
  FileReaderReadFn read;
  FileReaderSeekFn seek;
  FileReaderCloseFn close;

  off64_t offset;
} FileReader;

/* Functions for opening the various types of FileReader.
 * They either succeed and return a valid FileReader, or fail and return NULL.
 *
 * If a FileReader is created, it has to be cleaned up and freed by calling its close()
 * function unless another FileReader has taken ownership - for example, `Zstd` & `Gzip`
 * take over the base FileReader and will clean it up when their clean() is called.
 */

/* Create FileReader from raw file descriptor. */
FileReader *BLI_filereader_new_file(int filedes) ATTR_WARN_UNUSED_RESULT;
/* Create FileReader from raw file descriptor using memory-mapped IO. */
FileReader *BLI_filereader_new_mmap(int filedes) ATTR_WARN_UNUSED_RESULT;
/* Create FileReader from a region of memory. */
FileReader *BLI_filereader_new_memory(const void *data, size_t len) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/* Create FileReader from applying `Zstd` decompression on an underlying file. */
FileReader *BLI_filereader_new_zstd(FileReader *base) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/* Create FileReader from applying `Gzip` decompression on an underlying file. */
FileReader *BLI_filereader_new_gzip(FileReader *base) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

#ifdef __cplusplus
}
#endif
