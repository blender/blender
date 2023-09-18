/* SPDX-FileCopyrightText: 2004-2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <zlib.h>

#include "BLI_blenlib.h"
#include "BLI_filereader.h"

#include "MEM_guardedalloc.h"

typedef struct {
  FileReader reader;

  FileReader *base;

  z_stream strm;

  void *in_buf;
  size_t in_size;
} GzipReader;

static ssize_t gzip_read(FileReader *reader, void *buffer, size_t size)
{
  GzipReader *gzip = (GzipReader *)reader;

  gzip->strm.avail_out = size;
  gzip->strm.next_out = buffer;

  while (gzip->strm.avail_out > 0) {
    if (gzip->strm.avail_in == 0) {
      /* Ran out of buffered input data, read some more. */
      size_t readsize = gzip->base->read(gzip->base, gzip->in_buf, gzip->in_size);

      if (readsize > 0) {
        /* We got some data, so mark the buffer as refilled. */
        gzip->strm.avail_in = readsize;
        gzip->strm.next_in = gzip->in_buf;
      }
      else {
        /* The underlying file is EOF, so return as much as we can. */
        break;
      }
    }

    int ret = inflate(&gzip->strm, Z_NO_FLUSH);

    if (!ELEM(ret, Z_OK, Z_BUF_ERROR)) {
      break;
    }
  }

  ssize_t read_len = size - gzip->strm.avail_out;
  gzip->reader.offset += read_len;
  return read_len;
}

static void gzip_close(FileReader *reader)
{
  GzipReader *gzip = (GzipReader *)reader;

  if (inflateEnd(&gzip->strm) != Z_OK) {
    printf("close gzip stream error\n");
  }
  MEM_freeN((void *)gzip->in_buf);

  gzip->base->close(gzip->base);
  MEM_freeN(gzip);
}

FileReader *BLI_filereader_new_gzip(FileReader *base)
{
  GzipReader *gzip = MEM_callocN(sizeof(GzipReader), __func__);
  gzip->base = base;

  if (inflateInit2(&gzip->strm, 16 + MAX_WBITS) != Z_OK) {
    MEM_freeN(gzip);
    return NULL;
  }

  gzip->in_size = 256 * 2014;
  gzip->in_buf = MEM_mallocN(gzip->in_size, "gzip in buf");

  gzip->reader.read = gzip_read;
  gzip->reader.seek = NULL;
  gzip->reader.close = gzip_close;

  return (FileReader *)gzip;
}
