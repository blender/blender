/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <string.h>
#include <zstd.h>

#include "BLI_blenlib.h"
#include "BLI_endian_switch.h"
#include "BLI_filereader.h"
#include "BLI_math_base.h"

#include "MEM_guardedalloc.h"

typedef struct {
  FileReader reader;

  FileReader *base;

  ZSTD_DCtx *ctx;
  ZSTD_inBuffer in_buf;
  size_t in_buf_max_size;

  struct {
    int frames_num;
    size_t *compressed_ofs;
    size_t *uncompressed_ofs;

    char *cached_content;
    int cached_frame;
  } seek;
} ZstdReader;

static bool zstd_read_u32(FileReader *base, uint32_t *val)
{
  if (base->read(base, val, sizeof(uint32_t)) != sizeof(uint32_t)) {
    return false;
  }
#ifdef __BIG_ENDIAN__
  BLI_endian_switch_uint32(val);
#endif
  return true;
}

static bool zstd_read_seek_table(ZstdReader *zstd)
{
  FileReader *base = zstd->base;

  /* The seek table frame is at the end of the file, so seek there
   * and verify that there is enough data. */
  if (base->seek(base, -4, SEEK_END) < 13) {
    return false;
  }
  uint32_t magic;
  if (!zstd_read_u32(base, &magic) || magic != 0x8F92EAB1) {
    return false;
  }

  uint8_t flags;
  if (base->seek(base, -5, SEEK_END) < 0 || base->read(base, &flags, 1) != 1) {
    return false;
  }
  /* Bit 7 indicates check-sums. Bits 5 and 6 must be zero. */
  bool has_checksums = (flags & 0x80);
  if (flags & 0x60) {
    return false;
  }

  uint32_t frames_num;
  if (base->seek(base, -9, SEEK_END) < 0 || !zstd_read_u32(base, &frames_num)) {
    return false;
  }

  /* Each frame has either 2 or 3 uint32_t, and after that we have
   * frames_num, flags and magic for another 9 bytes. */
  uint32_t expected_frame_length = frames_num * (has_checksums ? 12 : 8) + 9;
  /* The frame starts with another magic number and its length, but these
   * two fields are not included when counting length. */
  off64_t frame_start_ofs = 8 + expected_frame_length;
  /* Sanity check: Before the start of the seek table frame,
   * there must be frames_num frames, each of which at least 8 bytes long. */
  off64_t seek_frame_start = base->seek(base, -frame_start_ofs, SEEK_END);
  if (seek_frame_start < frames_num * 8) {
    return false;
  }

  if (!zstd_read_u32(base, &magic) || magic != 0x184D2A5E) {
    return false;
  }

  uint32_t frame_length;
  if (!zstd_read_u32(base, &frame_length) || frame_length != expected_frame_length) {
    return false;
  }

  zstd->seek.frames_num = frames_num;
  zstd->seek.compressed_ofs = MEM_malloc_arrayN(frames_num + 1, sizeof(size_t), __func__);
  zstd->seek.uncompressed_ofs = MEM_malloc_arrayN(frames_num + 1, sizeof(size_t), __func__);

  size_t compressed_ofs = 0;
  size_t uncompressed_ofs = 0;
  for (int i = 0; i < frames_num; i++) {
    uint32_t compressed_size, uncompressed_size;
    if (!zstd_read_u32(base, &compressed_size) || !zstd_read_u32(base, &uncompressed_size)) {
      break;
    }
    if (has_checksums && base->seek(base, 4, SEEK_CUR) < 0) {
      break;
    }
    zstd->seek.compressed_ofs[i] = compressed_ofs;
    zstd->seek.uncompressed_ofs[i] = uncompressed_ofs;
    compressed_ofs += compressed_size;
    uncompressed_ofs += uncompressed_size;
  }
  zstd->seek.compressed_ofs[frames_num] = compressed_ofs;
  zstd->seek.uncompressed_ofs[frames_num] = uncompressed_ofs;

  /* Seek to the end of the previous frame for the following #BHead frame detection. */
  if (seek_frame_start != compressed_ofs || base->seek(base, seek_frame_start, SEEK_SET) < 0) {
    MEM_freeN(zstd->seek.compressed_ofs);
    MEM_freeN(zstd->seek.uncompressed_ofs);
    memset(&zstd->seek, 0, sizeof(zstd->seek));
    return false;
  }

  zstd->seek.cached_frame = -1;

  return true;
}

/* Find out which frame contains the given position in the uncompressed stream.
 * Basically just bisection. */
static int zstd_frame_from_pos(ZstdReader *zstd, size_t pos)
{
  int low = 0, high = zstd->seek.frames_num;

  if (pos >= zstd->seek.uncompressed_ofs[zstd->seek.frames_num]) {
    return -1;
  }

  while (low + 1 < high) {
    int mid = low + ((high - low) >> 1);
    if (zstd->seek.uncompressed_ofs[mid] <= pos) {
      low = mid;
    }
    else {
      high = mid;
    }
  }

  return low;
}

/* Ensure that the currently loaded frame is the correct one. */
static const char *zstd_ensure_cache(ZstdReader *zstd, int frame)
{
  if (zstd->seek.cached_frame == frame) {
    /* Cached frame matches, so just return it. */
    return zstd->seek.cached_content;
  }

  /* Cached frame doesn't match, so discard it and cache the wanted one instead. */
  MEM_SAFE_FREE(zstd->seek.cached_content);

  size_t compressed_size = zstd->seek.compressed_ofs[frame + 1] - zstd->seek.compressed_ofs[frame];
  size_t uncompressed_size = zstd->seek.uncompressed_ofs[frame + 1] -
                             zstd->seek.uncompressed_ofs[frame];

  char *uncompressed_data = MEM_mallocN(uncompressed_size, __func__);
  char *compressed_data = MEM_mallocN(compressed_size, __func__);
  if (zstd->base->seek(zstd->base, zstd->seek.compressed_ofs[frame], SEEK_SET) < 0 ||
      zstd->base->read(zstd->base, compressed_data, compressed_size) < compressed_size)
  {
    MEM_freeN(compressed_data);
    MEM_freeN(uncompressed_data);
    return NULL;
  }

  size_t res = ZSTD_decompressDCtx(
      zstd->ctx, uncompressed_data, uncompressed_size, compressed_data, compressed_size);
  MEM_freeN(compressed_data);
  if (ZSTD_isError(res) || res < uncompressed_size) {
    MEM_freeN(uncompressed_data);
    return NULL;
  }

  zstd->seek.cached_frame = frame;
  zstd->seek.cached_content = uncompressed_data;
  return uncompressed_data;
}

static ssize_t zstd_read_seekable(FileReader *reader, void *buffer, size_t size)
{
  ZstdReader *zstd = (ZstdReader *)reader;

  size_t end_offset = zstd->reader.offset + size, read_len = 0;
  while (zstd->reader.offset < end_offset) {
    int frame = zstd_frame_from_pos(zstd, zstd->reader.offset);
    if (frame < 0) {
      /* EOF is reached, so return as much as we can. */
      break;
    }

    const char *framedata = zstd_ensure_cache(zstd, frame);
    if (framedata == NULL) {
      /* Error while reading the frame, so return as much as we can. */
      break;
    }

    size_t frame_end_offset = min_zz(zstd->seek.uncompressed_ofs[frame + 1], end_offset);
    size_t frame_read_len = frame_end_offset - zstd->reader.offset;

    size_t offset_in_frame = zstd->reader.offset - zstd->seek.uncompressed_ofs[frame];
    memcpy((char *)buffer + read_len, framedata + offset_in_frame, frame_read_len);
    read_len += frame_read_len;
    zstd->reader.offset = frame_end_offset;
  }

  return read_len;
}

static off64_t zstd_seek(FileReader *reader, off64_t offset, int whence)
{
  ZstdReader *zstd = (ZstdReader *)reader;
  off64_t new_pos;
  if (whence == SEEK_SET) {
    new_pos = offset;
  }
  else if (whence == SEEK_END) {
    new_pos = zstd->seek.uncompressed_ofs[zstd->seek.frames_num] + offset;
  }
  else {
    new_pos = zstd->reader.offset + offset;
  }

  if (new_pos < 0 || new_pos > zstd->seek.uncompressed_ofs[zstd->seek.frames_num]) {
    return -1;
  }
  zstd->reader.offset = new_pos;
  return zstd->reader.offset;
}

static ssize_t zstd_read(FileReader *reader, void *buffer, size_t size)
{
  ZstdReader *zstd = (ZstdReader *)reader;
  ZSTD_outBuffer output = {buffer, size, 0};

  while (output.pos < output.size) {
    if (zstd->in_buf.pos == zstd->in_buf.size) {
      /* Ran out of buffered input data, read some more. */
      zstd->in_buf.pos = 0;
      ssize_t readsize = zstd->base->read(
          zstd->base, (char *)zstd->in_buf.src, zstd->in_buf_max_size);

      if (readsize > 0) {
        /* We got some data, so mark the buffer as refilled. */
        zstd->in_buf.size = readsize;
      }
      else {
        /* The underlying file is EOF, so return as much as we can. */
        break;
      }
    }

    if (ZSTD_isError(ZSTD_decompressStream(zstd->ctx, &output, &zstd->in_buf))) {
      break;
    }
  }

  zstd->reader.offset += output.pos;
  return output.pos;
}

static void zstd_close(FileReader *reader)
{
  ZstdReader *zstd = (ZstdReader *)reader;

  ZSTD_freeDCtx(zstd->ctx);
  if (zstd->reader.seek) {
    MEM_freeN(zstd->seek.uncompressed_ofs);
    MEM_freeN(zstd->seek.compressed_ofs);
    /* When an error has occurred this may be NULL, see: #99744. */
    if (zstd->seek.cached_content) {
      MEM_freeN(zstd->seek.cached_content);
    }
  }
  else {
    MEM_freeN((void *)zstd->in_buf.src);
  }

  zstd->base->close(zstd->base);
  MEM_freeN(zstd);
}

FileReader *BLI_filereader_new_zstd(FileReader *base)
{
  ZstdReader *zstd = MEM_callocN(sizeof(ZstdReader), __func__);

  zstd->ctx = ZSTD_createDCtx();
  zstd->base = base;

  if (zstd_read_seek_table(zstd)) {
    zstd->reader.read = zstd_read_seekable;
    zstd->reader.seek = zstd_seek;
  }
  else {
    zstd->reader.read = zstd_read;
    zstd->reader.seek = NULL;

    zstd->in_buf_max_size = ZSTD_DStreamInSize();
    zstd->in_buf.src = MEM_mallocN(zstd->in_buf_max_size, "zstd in buf");
    zstd->in_buf.size = zstd->in_buf_max_size;
    /* This signals that the buffer has run out,
     * which will make the read function refill it on the first call. */
    zstd->in_buf.pos = zstd->in_buf_max_size;
  }
  zstd->reader.close = zstd_close;

  /* Rewind after the seek table check so that zstd_read starts at the file's start. */
  zstd->base->seek(zstd->base, 0, SEEK_SET);

  return (FileReader *)zstd;
}
