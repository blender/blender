/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blendthumb
 *
 * Expose #blendthumb_create_thumb_from_file that creates the PNG data
 * but does not write it to a file.
 */

#include <cstring>

#include "BLI_alloca.h"
#include "BLI_endian_defines.h"
#include "BLI_endian_switch.h"
#include "BLI_fileops.h"
#include "BLI_filereader.h"
#include "BLI_string.h"

#include "blendthumb.hh"

static bool blend_header_check_magic(const char header[12])
{
  /* Check magic string at start of file. */
  if (!BLI_str_startswith(header, "BLENDER")) {
    return false;
  }
  /* Check pointer size and endianness indicators. */
  if (!ELEM(header[7], '_', '-') || !ELEM(header[8], 'v', 'V')) {
    return false;
  }
  /* Check version number. */
  if (!isdigit(header[9]) || !isdigit(header[10]) || !isdigit(header[11])) {
    return false;
  }
  return true;
}

static bool blend_header_is_version_valid(const char header[12])
{
  /* Thumbnails are only in files with version >= 2.50 */
  char num[4];
  memcpy(num, header + 9, 3);
  num[3] = 0;
  return atoi(num) >= 250;
}

static int blend_header_pointer_size(const char header[12])
{
  return header[7] == '_' ? 4 : 8;
}

static bool blend_header_is_endian_switch_needed(const char header[12])
{
  return (((header[8] == 'v') ? L_ENDIAN : B_ENDIAN) != ENDIAN_ORDER);
}

static void thumb_data_vertical_flip(Thumbnail *thumb)
{
  uint32_t *rect = (uint32_t *)thumb->data.data();
  int x = thumb->width, y = thumb->height;
  uint32_t *top = rect;
  uint32_t *bottom = top + ((y - 1) * x);
  uint32_t *line = (uint32_t *)malloc(x * sizeof(uint32_t));

  y >>= 1;
  for (; y > 0; y--) {
    memcpy(line, top, x * sizeof(uint32_t));
    memcpy(top, bottom, x * sizeof(uint32_t));
    memcpy(bottom, line, x * sizeof(uint32_t));
    bottom -= x;
    top += x;
  }
  free(line);
}

static int32_t bytes_to_native_i32(const uint8_t bytes[4], bool endian_switch)
{
  int32_t data;
  memcpy(&data, bytes, 4);
  if (endian_switch) {
    BLI_endian_switch_int32(&data);
  }
  return data;
}

static bool file_read(FileReader *file, uint8_t *buf, size_t buf_len)
{
  return (file->read(file, buf, buf_len) == buf_len);
}

static bool file_seek(FileReader *file, size_t len)
{
  if (file->seek != nullptr) {
    if (file->seek(file, len, SEEK_CUR) == -1) {
      return false;
    }
    return true;
  }

  /* File doesn't support seeking (e.g. gzip), so read and discard in chunks. */
  constexpr size_t dummy_data_size = 4096;
  blender::Array<char> dummy_data(dummy_data_size);
  while (len > 0) {
    const size_t len_chunk = std::min(len, dummy_data_size);
    if (size_t(file->read(file, dummy_data.data(), len_chunk)) != len_chunk) {
      return false;
    }
    len -= len_chunk;
  }
  return true;
}

static eThumbStatus blendthumb_extract_from_file_impl(FileReader *file,
                                                      Thumbnail *thumb,
                                                      const size_t bhead_size,
                                                      const bool endian_switch)
{
  /* Iterate over file blocks until we find the thumbnail or run out of data. */
  uint8_t *bhead_data = (uint8_t *)BLI_array_alloca(bhead_data, bhead_size);
  while (file_read(file, bhead_data, bhead_size)) {
    /* Parse type and size from `BHead`. */
    const int32_t block_size = bytes_to_native_i32(&bhead_data[4], endian_switch);
    if (UNLIKELY(block_size < 0)) {
      return BT_INVALID_THUMB;
    }

    /* We're looking for the thumbnail, so skip any other block. */
    switch (*((int32_t *)bhead_data)) {
      case MAKE_ID('T', 'E', 'S', 'T'): {
        uint8_t shape[8];
        if (!file_read(file, shape, sizeof(shape))) {
          return BT_INVALID_THUMB;
        }
        thumb->width = bytes_to_native_i32(&shape[0], endian_switch);
        thumb->height = bytes_to_native_i32(&shape[4], endian_switch);

        /* Verify that image dimensions and data size make sense. */
        size_t data_size = block_size - sizeof(shape);
        const uint64_t expected_size = uint64_t(thumb->width) * uint64_t(thumb->height) * 4;
        if (thumb->width < 0 || thumb->height < 0 || data_size != expected_size) {
          return BT_INVALID_THUMB;
        }

        thumb->data = blender::Array<uint8_t>(data_size);
        if (!file_read(file, thumb->data.data(), data_size)) {
          return BT_INVALID_THUMB;
        }
        return BT_OK;
      }
      case MAKE_ID('R', 'E', 'N', 'D'): {
        if (!file_seek(file, block_size)) {
          return BT_INVALID_THUMB;
        }
        /* Check the next block. */
        break;
      }
      default: {
        /* Early exit if there are no `TEST` or `REND` blocks.
         * This saves scanning the entire blend file which could be slow. */
        return BT_INVALID_THUMB;
      }
    }
  }

  return BT_INVALID_THUMB;
}

eThumbStatus blendthumb_create_thumb_from_file(FileReader *rawfile, Thumbnail *thumb)
{
  /* Read header in order to identify file type. */
  char header[12];
  if (rawfile->read(rawfile, header, sizeof(header)) != sizeof(header)) {
    rawfile->close(rawfile);
    return BT_ERROR;
  }

  /* Rewind the file after reading the header. */
  rawfile->seek(rawfile, 0, SEEK_SET);

  /* Try to identify the file type from the header. */
  FileReader *file = nullptr;
  if (BLI_str_startswith(header, "BLENDER")) {
    file = rawfile;
    rawfile = nullptr;
  }
  else if (BLI_file_magic_is_gzip(header)) {
    file = BLI_filereader_new_gzip(rawfile);
    if (file != nullptr) {
      rawfile = nullptr; /* The Gzip #FileReader takes ownership of raw-file. */
    }
  }
  else if (BLI_file_magic_is_zstd(header)) {
    file = BLI_filereader_new_zstd(rawfile);
    if (file != nullptr) {
      rawfile = nullptr; /* The Zstd #FileReader takes ownership of raw-file. */
    }
  }

  /* Clean up rawfile if it wasn't taken over. */
  if (rawfile != nullptr) {
    rawfile->close(rawfile);
  }

  if (file == nullptr) {
    return BT_ERROR;
  }

  /* Re-read header in case we had compression. */
  if (file->read(file, header, sizeof(header)) != sizeof(header)) {
    file->close(file);
    return BT_ERROR;
  }

  /* Check if the header format is valid for a .blend file. */
  if (!blend_header_check_magic(header)) {
    file->close(file);
    return BT_INVALID_FILE;
  }

  /* Check if the file is new enough to contain a thumbnail. */
  if (!blend_header_is_version_valid(header)) {
    file->close(file);
    return BT_EARLY_VERSION;
  }

  /* Depending on where it was saved, the file can use different pointer size or endianness. */
  int bhead_size = 16 + blend_header_pointer_size(header);
  const bool endian_switch = blend_header_is_endian_switch_needed(header);

  /* Read the thumbnail. */
  eThumbStatus err = blendthumb_extract_from_file_impl(file, thumb, bhead_size, endian_switch);
  file->close(file);
  if (err != BT_OK) {
    return err;
  }

  thumb_data_vertical_flip(thumb);
  return BT_OK;
}
