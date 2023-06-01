/* SPDX-FileCopyrightText: 2004-2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_filereader.h"
#include "BLI_mmap.h"

#include "MEM_guardedalloc.h"

/* This file implements both memory-backed and memory-mapped-file-backed reading. */
typedef struct {
  FileReader reader;

  const char *data;
  BLI_mmap_file *mmap;
  size_t length;
} MemoryReader;

static ssize_t memory_read_raw(FileReader *reader, void *buffer, size_t size)
{
  MemoryReader *mem = (MemoryReader *)reader;

  /* Don't read more bytes than there are available in the buffer. */
  size_t readsize = MIN2(size, (size_t)(mem->length - mem->reader.offset));

  memcpy(buffer, mem->data + mem->reader.offset, readsize);
  mem->reader.offset += readsize;

  return readsize;
}

static off64_t memory_seek(FileReader *reader, off64_t offset, int whence)
{
  MemoryReader *mem = (MemoryReader *)reader;

  off64_t new_pos;
  if (whence == SEEK_CUR) {
    new_pos = mem->reader.offset + offset;
  }
  else if (whence == SEEK_SET) {
    new_pos = offset;
  }
  else if (whence == SEEK_END) {
    new_pos = mem->length + offset;
  }
  else {
    return -1;
  }

  if (new_pos < 0 || new_pos > mem->length) {
    return -1;
  }

  mem->reader.offset = new_pos;
  return mem->reader.offset;
}

static void memory_close_raw(FileReader *reader)
{
  MEM_freeN(reader);
}

FileReader *BLI_filereader_new_memory(const void *data, size_t len)
{
  MemoryReader *mem = MEM_callocN(sizeof(MemoryReader), __func__);

  mem->data = (const char *)data;
  mem->length = len;

  mem->reader.read = memory_read_raw;
  mem->reader.seek = memory_seek;
  mem->reader.close = memory_close_raw;

  return (FileReader *)mem;
}

/* Memory-mapped file reading.
 * By using `mmap()`, we can map a file so that it can be treated like normal memory,
 * meaning that we can just read from it with `memcpy()` etc.
 * This avoids system call overhead and can significantly speed up file loading.
 */

static ssize_t memory_read_mmap(FileReader *reader, void *buffer, size_t size)
{
  MemoryReader *mem = (MemoryReader *)reader;

  /* Don't read more bytes than there are available in the buffer. */
  size_t readsize = MIN2(size, (size_t)(mem->length - mem->reader.offset));

  if (!BLI_mmap_read(mem->mmap, buffer, mem->reader.offset, readsize)) {
    return 0;
  }

  mem->reader.offset += readsize;

  return readsize;
}

static void memory_close_mmap(FileReader *reader)
{
  MemoryReader *mem = (MemoryReader *)reader;
  BLI_mmap_free(mem->mmap);
  MEM_freeN(mem);
}

FileReader *BLI_filereader_new_mmap(int filedes)
{
  BLI_mmap_file *mmap = BLI_mmap_open(filedes);
  if (mmap == NULL) {
    return NULL;
  }

  MemoryReader *mem = MEM_callocN(sizeof(MemoryReader), __func__);

  mem->mmap = mmap;
  mem->length = BLI_lseek(filedes, 0, SEEK_END);

  mem->reader.read = memory_read_mmap;
  mem->reader.seek = memory_seek;
  mem->reader.close = memory_close_mmap;

  return (FileReader *)mem;
}
