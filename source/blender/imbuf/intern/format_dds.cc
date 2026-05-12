/* SPDX-FileCopyrightText: 2009 Google Inc. All rights reserved. (BSD-3-Clause)
 * SPDX-FileCopyrightText: 2023-2024 Blender Authors (GPL-2.0-or-later).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later AND BSD-3-Clause */

/** \file
 * \ingroup imbuf
 *
 * Some portions of this file are from the Chromium project and have been adapted
 * for Blender use when flipping DDS images to the OpenGL convention.
 */

#include <algorithm>
#include <fcntl.h>
#if defined(WIN32)
#  include <io.h>
#else
#  include <unistd.h>
#endif

#include "oiio/openimageio_support.hh"

#include "IMB_filetype.hh"
#include "IMB_imbuf_types.hh"

#include "BLI_fileops.h"
#include "BLI_math_base.h"
#include "BLI_mmap.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "MEM_guardedalloc.h"

namespace blender {

const char *imb_file_extensions_dds[] = {".dds", nullptr};

OIIO_NAMESPACE_USING
using namespace blender::imbuf;

void imb_init_dds()
{
  /* To match historical behavior for DDS file loading, tell OpenImageIO
   * to process BC5 compressed textures as normal maps. But only do so
   * if the environment does not already contain a directive that might
   * say otherwise. */
  const char *bc5normal = "dds:bc5normal";
  const char *oiio_env = BLI_getenv("OPENIMAGEIO_OPTIONS");
  if (!oiio_env || !BLI_strcasestr(oiio_env, bc5normal)) {
    OIIO::attribute(bc5normal, 1);
  }
}

bool imb_is_a_dds(const uchar *mem, size_t size)
{
  return imb_oiio_check(mem, size, "dds");
}

ImBuf *imb_load_dds(const uchar *mem, size_t size, int flags, ImFileColorSpace &r_colorspace)
{
  ImageSpec config, spec;
  ReadContext ctx{mem, size, "dds", IMB_FTYPE_DDS, flags};

  ImBuf *ibuf = imb_oiio_read(ctx, config, r_colorspace, spec);
  if (ibuf) {
    std::string compression = spec.get_string_attribute("compression", "");
    if (compression == "DXT1") {
      ibuf->foptions.flag |= DDS_COMPRESSED_DXT1;
    }
    if (compression == "DXT3") {
      ibuf->foptions.flag |= DDS_COMPRESSED_DXT3;
    }
    if (compression == "DXT5") {
      ibuf->foptions.flag |= DDS_COMPRESSED_DXT5;
    }
  }

  return ibuf;
}

/* A function that flips a DXTC block. */
using FlipBlockFunction = void (*)(uint8_t *block);

/* Flips a full DXT1 block in the y direction. */
static void FlipDXT1BlockFull(uint8_t *block)
{
  /* A DXT1 block layout is:
   * [0-1] color0.
   * [2-3] color1.
   * [4-7] color bitmap, 2 bits per pixel.
   * So each of the 4-7 bytes represents one line, flipping a block is just
   * flipping those bytes. */
  uint8_t tmp = block[4];
  block[4] = block[7];
  block[7] = tmp;
  tmp = block[5];
  block[5] = block[6];
  block[6] = tmp;
}

/* Flips the first 2 lines of a DXT1 block in the y direction. */
static void FlipDXT1BlockHalf(uint8_t *block)
{
  /* See layout above. */
  uint8_t tmp = block[4];
  block[4] = block[5];
  block[5] = tmp;
}

/* Flips a full DXT3 block in the y direction. */
static void FlipDXT3BlockFull(uint8_t *block)
{
  /* A DXT3 block layout is:
   * [0-7]    alpha bitmap, 4 bits per pixel.
   * [8-15] a DXT1 block. */

  /* We can flip the alpha bits at the byte level (2 bytes per line). */
  uint8_t tmp = block[0];

  block[0] = block[6];
  block[6] = tmp;
  tmp = block[1];
  block[1] = block[7];
  block[7] = tmp;
  tmp = block[2];
  block[2] = block[4];
  block[4] = tmp;
  tmp = block[3];
  block[3] = block[5];
  block[5] = tmp;

  /* And flip the DXT1 block using the above function. */
  FlipDXT1BlockFull(block + 8);
}

/* Flips the first 2 lines of a DXT3 block in the y direction. */
static void FlipDXT3BlockHalf(uint8_t *block)
{
  /* See layout above. */
  uint8_t tmp = block[0];

  block[0] = block[2];
  block[2] = tmp;
  tmp = block[1];
  block[1] = block[3];
  block[3] = tmp;
  FlipDXT1BlockHalf(block + 8);
}

/* Flips a full DXT5 block in the y direction. */
static void FlipDXT5BlockFull(uint8_t *block)
{
  /* A DXT5 block layout is:
   * [0]      alpha0.
   * [1]      alpha1.
   * [2-7]    alpha bitmap, 3 bits per pixel.
   * [8-15] a DXT1 block. */

  /* The alpha bitmap doesn't easily map lines to bytes, so we have to
   * interpret it correctly.  Extracted from
   * http://www.opengl.org/registry/specs/EXT/texture_compression_s3tc.txt :
   *
   *   The 6 "bits" bytes of the block are decoded into one 48-bit integer:
   *
   *       bits = bits_0 + 256 * (bits_1 + 256 * (bits_2 + 256 * (bits_3 +
   *                                   256 * (bits_4 + 256 * bits_5))))
   *
   *   bits is a 48-bit unsigned-integer, from which a three-bit control code
   *   is extracted for a texel at location (x,y) in the block using:
   *
   *           code(x,y) = bits[3*(4*y+x)+1..3*(4*y+x)+0]
   *
   *   where bit 47 is the most significant and bit 0 is the least
   *   significant bit. */
  uint line_0_1 = block[2] + 256 * (block[3] + 256 * block[4]);
  uint line_2_3 = block[5] + 256 * (block[6] + 256 * block[7]);
  /* swap lines 0 and 1 in line_0_1. */
  uint line_1_0 = ((line_0_1 & 0x000fff) << 12) | ((line_0_1 & 0xfff000) >> 12);
  /* swap lines 2 and 3 in line_2_3. */
  uint line_3_2 = ((line_2_3 & 0x000fff) << 12) | ((line_2_3 & 0xfff000) >> 12);

  block[2] = line_3_2 & 0xff;
  block[3] = (line_3_2 & 0xff00) >> 8;
  block[4] = (line_3_2 & 0xff0000) >> 16;
  block[5] = line_1_0 & 0xff;
  block[6] = (line_1_0 & 0xff00) >> 8;
  block[7] = (line_1_0 & 0xff0000) >> 16;

  /* And flip the DXT1 block using the above function. */
  FlipDXT1BlockFull(block + 8);
}

/* Flips the first 2 lines of a DXT5 block in the y direction. */
static void FlipDXT5BlockHalf(uint8_t *block)
{
  /* See layout above. */
  uint line_0_1 = block[2] + 256 * (block[3] + 256 * block[4]);
  uint line_1_0 = ((line_0_1 & 0x000fff) << 12) | ((line_0_1 & 0xfff000) >> 12);
  block[2] = line_1_0 & 0xff;
  block[3] = (line_1_0 & 0xff00) >> 8;
  block[4] = (line_1_0 & 0xff0000) >> 16;
  FlipDXT1BlockHalf(block + 8);
}

static constexpr uint32_t fourcc_dxt1 = 0x31545844; /* D, X, T, 1 */
static constexpr uint32_t fourcc_dxt3 = 0x33545844; /* D, X, T, 3 */
static constexpr uint32_t fourcc_dxt5 = 0x35545844; /* D, X, T, 5 */

/**
 * Flips a DXTC image, by flipping and swapping DXTC blocks as appropriate.
 * Use to flip vertically to fit OpenGL convention.
 * Returns number of valid mip levels.
 */
static int flip_dxtc_image(
    uint8_t *data, size_t data_size, int width, int height, int levels, uint32_t fourcc)
{
  /* Must have valid dimensions. */
  if (width == 0 || height == 0) {
    return 0;
  }

  /* Height must be a power-of-two: not because something within DXT/S3TC
   * needs it. Only because we want to flip the image upside down, by
   * swapping and flipping block rows, and that in general case (with mipmaps)
   * is only possible for POT height. */
  if (!is_power_of_2_i(height)) {
    return 0;
  }

  FlipBlockFunction full_block_function;
  FlipBlockFunction half_block_function;
  size_t block_bytes = 0;

  switch (fourcc) {
    case fourcc_dxt1:
      full_block_function = FlipDXT1BlockFull;
      half_block_function = FlipDXT1BlockHalf;
      block_bytes = 8;
      break;
    case fourcc_dxt3:
      full_block_function = FlipDXT3BlockFull;
      half_block_function = FlipDXT3BlockHalf;
      block_bytes = 16;
      break;
    case fourcc_dxt5:
      full_block_function = FlipDXT5BlockFull;
      half_block_function = FlipDXT5BlockHalf;
      block_bytes = 16;
      break;
    default:
      return 0;
  }

  int mip_width = width;
  int mip_height = height;

  const uint8_t *data_end = data + data_size;

  for (int level = 0; level < levels; level++) {
    int blocks_per_row = (mip_width + 3) / 4;
    int blocks_per_col = (mip_height + 3) / 4;
    int blocks = blocks_per_row * blocks_per_col;

    if (data + block_bytes * blocks > data_end) {
      /* Stop flipping when running out of data to be modified, avoiding possible buffer overrun
       * on a malformed files. */
      return level;
    }

    if (mip_height == 1) {
      /* no flip to do, and we're done. */
      break;
    }
    if (mip_height == 2) {
      /* flip the first 2 lines in each block. */
      for (int i = 0; i < blocks_per_row; i++) {
        half_block_function(data + i * block_bytes);
      }
    }
    else {
      /* flip each block. */
      for (int i = 0; i < blocks; i++) {
        full_block_function(data + i * block_bytes);
      }

      /* Swap each block line in the first half of the image with the
       * corresponding one in the second half.
       * note that this is a no-op if mip height is 4. */
      size_t row_bytes = block_bytes * blocks_per_row;
      uint8_t *temp_line = new uint8_t[row_bytes];

      for (int y = 0; y < blocks_per_col / 2; y++) {
        uint8_t *line1 = data + y * row_bytes;
        uint8_t *line2 = data + (blocks_per_col - y - 1) * row_bytes;

        memcpy(temp_line, line1, row_bytes);
        memcpy(line1, line2, row_bytes);
        memcpy(line2, temp_line, row_bytes);
      }

      delete[] temp_line;
    }

    /* Advance to next mip level. */
    data += block_bytes * blocks;
    mip_width = std::max(1, mip_width >> 1);
    mip_height = std::max(1, mip_height >> 1);
  }

  return levels;
}

uint8_t *imb_load_dds_compressed_data(const char *filepath, int width, int height, int &r_mipcount)
{
  r_mipcount = 0;

  /* Due to upside down flipping, we only support compressed DDS files with power of two heights.
   */
  if (!is_power_of_2_i(height)) {
    return nullptr;
  }

  const int file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
  if (file == -1) {
    return nullptr;
  }
  BLI_mmap_file *mmap_file = BLI_mmap_open(file);
  close(file);
  if (mmap_file == nullptr) {
    return nullptr;
  }

  uint8_t *result = nullptr;

  const uchar *file_data = static_cast<const uchar *>(BLI_mmap_get_pointer(mmap_file));
  const size_t file_size = BLI_mmap_get_length(mmap_file);

  constexpr size_t dds_header_size = 128;
  if (file_size < dds_header_size) {
    BLI_mmap_free(mmap_file);
    return nullptr;
  }

  /* Pull out pixel format and mipmap count flags from DDS header. */
  uint32_t flags = 0, mipcount = 0, fourcc = 0;
  memcpy(&flags, file_data + 8, 4);
  memcpy(&mipcount, file_data + 28, 4);
  memcpy(&fourcc, file_data + 84, 4);

  /* Due to upside down flipping, we only support DXT1/DXT3/DXT5. Newer formats like BC7 can't
   * be flipped upside down due to non-symmetrical partition shapes. */
  if (ELEM(fourcc, fourcc_dxt1, fourcc_dxt3, fourcc_dxt5)) {

    constexpr uint32_t DDSD_MIPMAPCOUNT = 0x00020000U;
    if ((flags & DDSD_MIPMAPCOUNT) == 0) {
      mipcount = 1;
    }

    size_t pixel_data_size = file_size - dds_header_size;

    result = MEM_new_array_uninitialized<uint8_t>(pixel_data_size, "DDS compressed data");
    memcpy(result, file_data + dds_header_size, pixel_data_size);

    r_mipcount = flip_dxtc_image(result, pixel_data_size, width, height, mipcount, fourcc);
  }

  BLI_mmap_free(mmap_file);

  return result;
}

}  // namespace blender
