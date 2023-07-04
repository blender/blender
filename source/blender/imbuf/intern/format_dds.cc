/* SPDX-License-Identifier: GPL-2.0-or-later AND BSD-3-Clause
 * Copyright 2009 Google Inc. All rights reserved. (BSD-3-Clause)
 * Copyright 2023 Blender Foundation (GPL-2.0-or-later). */

/**
 * Some portions of this file are from the Chromium project and have been adapted
 * for Blender use when flipping DDS images to the OpenGL convention.
 */

#include <algorithm>
#include <memory>

#include "oiio/openimageio_support.hh"

#include "IMB_filetype.h"
#include "IMB_imbuf_types.h"

#include "BLI_path_util.h"
#include "BLI_string.h"

#ifdef __BIG_ENDIAN__
#  include "BLI_endian_switch.h"
#endif

OIIO_NAMESPACE_USING
using namespace blender::imbuf;

using std::unique_ptr;

extern "C" {

static void LoadDXTCImage(ImBuf *ibuf, Filesystem::IOMemReader &mem_reader);

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

bool imb_is_a_dds(const uchar *buf, size_t size)
{
  return imb_oiio_check(buf, size, "dds");
}

ImBuf *imb_load_dds(const uchar *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
  ImageSpec config, spec;
  ReadContext ctx{mem, size, "dds", IMB_FTYPE_DDS, flags};

  ImBuf *ibuf = imb_oiio_read(ctx, config, colorspace, spec);

  /* Load compressed DDS information if available. */
  if (ibuf && (flags & IB_test) == 0) {
    Filesystem::IOMemReader mem_reader(cspan<uchar>(mem, size));
    LoadDXTCImage(ibuf, mem_reader);
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

/**
 * Flips a DXTC image, by flipping and swapping DXTC blocks as appropriate.
 *
 * Use to flip vertically to fit OpenGL convention.
 */
static void FlipDXTCImage(ImBuf *ibuf)
{
  uint32_t width = ibuf->x;
  uint32_t height = ibuf->y;
  uint32_t levels = ibuf->dds_data.nummipmaps;
  int fourcc = ibuf->dds_data.fourcc;
  uint8_t *data = ibuf->dds_data.data;
  int data_size = ibuf->dds_data.size;

  uint32_t *num_valid_levels = &ibuf->dds_data.nummipmaps;
  *num_valid_levels = 0;

  /* Must have valid dimensions. */
  if (width == 0 || height == 0) {
    return;
  }
  /* Height must be a power-of-two. */
  if ((height & (height - 1)) != 0) {
    return;
  }

  FlipBlockFunction full_block_function;
  FlipBlockFunction half_block_function;
  uint block_bytes = 0;

  switch (fourcc) {
    case FOURCC_DXT1:
      full_block_function = FlipDXT1BlockFull;
      half_block_function = FlipDXT1BlockHalf;
      block_bytes = 8;
      break;
    case FOURCC_DXT3:
      full_block_function = FlipDXT3BlockFull;
      half_block_function = FlipDXT3BlockHalf;
      block_bytes = 16;
      break;
    case FOURCC_DXT5:
      full_block_function = FlipDXT5BlockFull;
      half_block_function = FlipDXT5BlockHalf;
      block_bytes = 16;
      break;
    default:
      return;
  }

  *num_valid_levels = levels;

  uint mip_width = width;
  uint mip_height = height;

  const uint8_t *data_end = data + data_size;

  for (uint level = 0; level < levels; level++) {
    uint blocks_per_row = (mip_width + 3) / 4;
    uint blocks_per_col = (mip_height + 3) / 4;
    uint blocks = blocks_per_row * blocks_per_col;

    if (data + block_bytes * blocks > data_end) {
      /* Stop flipping when running out of data to be modified, avoiding possible buffer overrun
       * on a malformed files. */
      *num_valid_levels = level;
      break;
    }

    if (mip_height == 1) {
      /* no flip to do, and we're done. */
      break;
    }
    if (mip_height == 2) {
      /* flip the first 2 lines in each block. */
      for (uint i = 0; i < blocks_per_row; i++) {
        half_block_function(data + i * block_bytes);
      }
    }
    else {
      /* flip each block. */
      for (uint i = 0; i < blocks; i++) {
        full_block_function(data + i * block_bytes);
      }

      /* Swap each block line in the first half of the image with the
       * corresponding one in the second half.
       * note that this is a no-op if mip_height is 4. */
      uint row_bytes = block_bytes * blocks_per_row;
      uint8_t *temp_line = new uint8_t[row_bytes];

      for (uint y = 0; y < blocks_per_col / 2; y++) {
        uint8_t *line1 = data + y * row_bytes;
        uint8_t *line2 = data + (blocks_per_col - y - 1) * row_bytes;

        memcpy(temp_line, line1, row_bytes);
        memcpy(line1, line2, row_bytes);
        memcpy(line2, temp_line, row_bytes);
      }

      delete[] temp_line;
    }

    /* mip levels are contiguous. */
    data += block_bytes * blocks;
    mip_width = std::max(1U, mip_width >> 1);
    mip_height = std::max(1U, mip_height >> 1);
  }
}

static void LoadDXTCImage(ImBuf *ibuf, Filesystem::IOMemReader &mem_reader)
{
  /* Reach into memory and pull out the pixel format flags and mipmap counts. This is safe if
   * we've made it this far. */
  uint32_t flags = 0;
  mem_reader.pread(&flags, sizeof(uint32_t), 8);
  mem_reader.pread(&ibuf->dds_data.nummipmaps, sizeof(uint32_t), 28);
  mem_reader.pread(&ibuf->dds_data.fourcc, sizeof(uint32_t), 84);

#ifdef __BIG_ENDIAN__
  BLI_endian_switch_uint32(&ibuf->dds_data.nummipmaps);
#endif

  const uint32_t DDSD_MIPMAPCOUNT = 0x00020000U;
  if ((flags & DDSD_MIPMAPCOUNT) == 0) {
    ibuf->dds_data.nummipmaps = 1;
  }

  /* Load the compressed data. */
  if (ibuf->dds_data.fourcc != FOURCC_DDS) {
    uint32_t dds_header_size = 128;
    if (ibuf->dds_data.fourcc == FOURCC_DX10) {
      dds_header_size += 20;
    }

    ibuf->dds_data.size = mem_reader.size() - dds_header_size;
    ibuf->dds_data.data = (uchar *)malloc(ibuf->dds_data.size);
    mem_reader.pread(ibuf->dds_data.data, ibuf->dds_data.size, dds_header_size);

    /* Flip compressed image data to match OpenGL convention. */
    FlipDXTCImage(ibuf);
  }
}
}
