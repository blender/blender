/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blendthumb
 *
 * Expose #blendthumb_create_thumb_from_file that creates the PNG data
 * but does not write it to a file.
 */

#include <cctype>
#include <cstring>

#include "BLI_alloca.h"
#include "BLI_endian_defines.h"
#include "BLI_fileops.h"
#include "BLI_filereader.h"
#include "BLI_string.h"

#include "BLO_core_bhead.hh"
#include "BLO_core_blend_header.hh"

#include "blendthumb.hh"

BLI_STATIC_ASSERT(ENDIAN_ORDER == L_ENDIAN, "Blender only builds on little endian systems")

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

static int32_t bytes_to_native_i32(const uint8_t bytes[4])
{
  int32_t data;
  memcpy(&data, bytes, 4);
  /* NOTE: this is endianness-sensitive. */
  /* PNG is always little-endian, and would require switching on a big-endian system. */
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
                                                      const BlenderHeader &header)
{
  BLI_assert(header.endian == L_ENDIAN);
  /* Iterate over file blocks until we find the thumbnail or run out of data. */
  while (true) {
    /* Read next BHead. */
    const std::optional<BHead> bhead = BLO_readfile_read_bhead(file, header.bhead_type());
    if (!bhead.has_value()) {
      /* File has ended. */
      return BT_INVALID_THUMB;
    }
    if (bhead->len < 0) {
      /* Avoid parsing bad data. */
      return BT_INVALID_THUMB;
    }
    switch (bhead->code) {
      case MAKE_ID('T', 'E', 'S', 'T'): {
        uint8_t shape[8];
        if (!file_read(file, shape, sizeof(shape))) {
          return BT_INVALID_THUMB;
        }
        thumb->width = bytes_to_native_i32(&shape[0]);
        thumb->height = bytes_to_native_i32(&shape[4]);

        /* Verify that image dimensions and data size make sense. */
        size_t data_size = bhead->len - sizeof(shape);
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
        if (!file_seek(file, bhead->len)) {
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
  char magic_bytes[12];
  if (rawfile->read(rawfile, magic_bytes, sizeof(magic_bytes)) != sizeof(magic_bytes)) {
    rawfile->close(rawfile);
    return BT_ERROR;
  }

  /* Rewind the file after reading the header. */
  rawfile->seek(rawfile, 0, SEEK_SET);

  /* Try to identify the file type from the header. */
  FileReader *file = nullptr;
  if (BLI_str_startswith(magic_bytes, "BLENDER")) {
    file = rawfile;
    rawfile = nullptr;
  }
  else if (BLI_file_magic_is_gzip(magic_bytes)) {
    file = BLI_filereader_new_gzip(rawfile);
    if (file != nullptr) {
      rawfile = nullptr; /* The GZIP #FileReader takes ownership of raw-file. */
    }
  }
  else if (BLI_file_magic_is_zstd(magic_bytes)) {
    file = BLI_filereader_new_zstd(rawfile);
    if (file != nullptr) {
      rawfile = nullptr; /* The ZSTD #FileReader takes ownership of raw-file. */
    }
  }

  /* Clean up rawfile if it wasn't taken over. */
  if (rawfile != nullptr) {
    rawfile->close(rawfile);
  }

  if (file == nullptr) {
    return BT_ERROR;
  }

  const BlenderHeaderVariant header_variant = BLO_readfile_blender_header_decode(file);
  if (!std::holds_alternative<BlenderHeader>(header_variant)) {
    file->close(file);
    return BT_ERROR;
  }
  const BlenderHeader &header = std::get<BlenderHeader>(header_variant);

  /* Check if the file is new enough to contain a thumbnail. */
  if (header.file_version < 250) {
    file->close(file);
    return BT_EARLY_VERSION;
  }

  /* Check if the file was written from a big-endian build. */
  if (header.endian != L_ENDIAN) {
    file->close(file);
    return BT_INVALID_FILE;
  }
  BLI_assert(header.endian == ENDIAN_ORDER);

  /* Read the thumbnail. */
  eThumbStatus err = blendthumb_extract_from_file_impl(file, thumb, header);
  file->close(file);
  if (err != BT_OK) {
    return err;
  }

  thumb_data_vertical_flip(thumb);
  return BT_OK;
}
