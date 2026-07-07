/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include "DNA_image_types.h"

#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_memory_utils.hh"

#include "IMB_imbuf_types.hh"

namespace blender::bke::image {

/** Type to use for UDIM tile numbers (1001). */
using TileNumber = int32_t;

struct ImageTileWrapper {
  ImageTile *image_tile;
  ImageTileWrapper(ImageTile *image_tile) : image_tile(image_tile) {}

  TileNumber get_tile_number() const
  {
    return image_tile->tile_number;
  }

  int2 get_tile_offset() const
  {
    return int2(get_tile_x_offset(), get_tile_y_offset());
  }

  int get_tile_x_offset() const
  {
    TileNumber tile_number = get_tile_number();
    return (tile_number - 1001) % 10;
  }

  int get_tile_y_offset() const
  {
    TileNumber tile_number = get_tile_number();
    return (tile_number - 1001) / 10;
  }
};

template<typename T, int Channels = 4> struct ImageBufferAccessor {
  static_assert(is_same_any_v<T, int, float4>);

  ImBuf &image_buffer;

  ImageBufferAccessor(ImBuf &image_buffer) : image_buffer(image_buffer) {}

  float4 read_pixel(const int2 coordinate)
  {
    if constexpr ((std::is_same_v<T, float4>)) {
      int offset = (coordinate.y * image_buffer.x + coordinate.x) * Channels;
      return float4(&image_buffer.float_buffer.data[offset]);
    }
    if constexpr ((std::is_same_v<T, int>)) {
      int offset = (coordinate.y * image_buffer.x + coordinate.x) * Channels;
      float4 result;
      rgba_uchar_to_float(
          result,
          static_cast<uchar *>(static_cast<void *>(&image_buffer.byte_buffer.data[offset])));
      return result;
    }
    return float4();
  }

  void write_pixel(const int2 coordinate, float4 new_value)
  {
    if constexpr ((std::is_same_v<T, float>)) {
      int offset = (coordinate.y * image_buffer.x + coordinate.x) * Channels;
      copy_v4_v4(&image_buffer.float_buffer.data[offset], new_value);
    }
    if constexpr ((std::is_same_v<T, int>)) {
      int offset = (coordinate.y * image_buffer.x + coordinate.x) * Channels;
      rgba_float_to_uchar(
          static_cast<uchar *>(static_cast<void *>(&image_buffer.byte_buffer.data[offset])),
          new_value);
    }
  }
};

}  // namespace blender::bke::image
