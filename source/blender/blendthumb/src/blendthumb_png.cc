/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blendthumb
 *
 * Expose #blendthumb_create_png_data_from_thumb that creates the PNG data
 * but does not write it to a file.
 */

#include <cstring>
#include <optional>
#include <zlib.h>

#include "blendthumb.hh"

#include "BLI_endian_defines.h"
#include "BLI_endian_switch.h"
#include "BLI_vector.hh"

static void png_extend_native_int32(blender::Vector<uint8_t> &output, int32_t data)
{
  if (ENDIAN_ORDER == L_ENDIAN) {
    BLI_endian_switch_int32(&data);
  }
  output.extend_unchecked(blender::Span((uint8_t *)&data, 4));
}

/** The number of bytes each chunk uses on top of the data that's written. */
#define PNG_CHUNK_EXTRA 12

static void png_chunk_create(blender::Vector<uint8_t> &output,
                             const uint32_t tag,
                             const blender::Vector<uint8_t> &data)
{
  uint32_t crc = crc32(0, nullptr, 0);
  crc = crc32(crc, (uint8_t *)&tag, sizeof(tag));
  crc = crc32(crc, (uint8_t *)data.data(), data.size());

  png_extend_native_int32(output, data.size());
  output.extend_unchecked(blender::Span((uint8_t *)&tag, sizeof(tag)));
  output.extend_unchecked(data);
  png_extend_native_int32(output, crc);
}

static blender::Vector<uint8_t> filtered_rows_from_thumb(const Thumbnail *thumb)
{
  /* In the image data sent to the compression step, each scan-line is preceded by a filter type
   * byte containing the numeric code of the filter algorithm used for that scan-line. */
  const size_t line_size = thumb->width * 4;
  blender::Vector<uint8_t> filtered;
  size_t final_size = thumb->height * (line_size + 1);
  filtered.reserve(final_size);
  for (int i = 0; i < thumb->height; i++) {
    filtered.append_unchecked(0x00);
    filtered.extend_unchecked(blender::Span(&thumb->data[i * line_size], line_size));
  }
  BLI_assert(final_size == filtered.size());
  return filtered;
}

static std::optional<blender::Vector<uint8_t>> zlib_compress(const blender::Vector<uint8_t> &data)
{
  ulong uncompressed_size = data.size();
  uLongf compressed_size = compressBound(uncompressed_size);

  blender::Vector<uint8_t> compressed(compressed_size, 0x00);

  int return_value = compress2((uchar *)compressed.data(),
                               &compressed_size,
                               (uchar *)data.data(),
                               uncompressed_size,
                               Z_NO_COMPRESSION);
  if (return_value != Z_OK) {
    /* Something went wrong with compression of data. */
    return std::nullopt;
  }
  compressed.resize(compressed_size);
  return compressed;
}

std::optional<blender::Vector<uint8_t>> blendthumb_create_png_data_from_thumb(
    const Thumbnail *thumb)
{
  if (thumb->data.is_empty()) {
    return std::nullopt;
  }

  /* Create `IDAT` chunk data. */
  blender::Vector<uint8_t> image_data;
  {
    auto image_data_opt = zlib_compress(filtered_rows_from_thumb(thumb));
    if (image_data_opt == std::nullopt) {
      return std::nullopt;
    }
    image_data = *image_data_opt;
  }

  /* Create the IHDR chunk data. */
  blender::Vector<uint8_t> ihdr_data;
  {
    const size_t ihdr_data_final_size = 4 + 4 + 5;
    ihdr_data.reserve(ihdr_data_final_size);
    png_extend_native_int32(ihdr_data, thumb->width);
    png_extend_native_int32(ihdr_data, thumb->height);
    ihdr_data.extend_unchecked({
        0x08, /* Bit Depth. */
        0x06, /* Color Type. */
        0x00, /* Compression method. */
        0x00, /* Filter method. */
        0x00, /* Interlace method. */
    });
    BLI_assert(size_t(ihdr_data.size()) == ihdr_data_final_size);
  }

  /* Join it all together to create a PNG image. */
  blender::Vector<uint8_t> png_buf;
  {
    const size_t png_buf_final_size = (
        /* Header. */
        8 +
        /* `IHDR` chunk. */
        (ihdr_data.size() + PNG_CHUNK_EXTRA) +
        /* `IDAT` chunk. */
        (image_data.size() + PNG_CHUNK_EXTRA) +
        /* `IEND` chunk. */
        PNG_CHUNK_EXTRA);

    png_buf.reserve(png_buf_final_size);

    /* This is the standard PNG file header. Every PNG file starts with it. */
    png_buf.extend_unchecked({0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A});

    png_chunk_create(png_buf, MAKE_ID('I', 'H', 'D', 'R'), ihdr_data);
    png_chunk_create(png_buf, MAKE_ID('I', 'D', 'A', 'T'), image_data);
    png_chunk_create(png_buf, MAKE_ID('I', 'E', 'N', 'D'), {});

    BLI_assert(size_t(png_buf.size()) == png_buf_final_size);
  }

  return png_buf;
}
