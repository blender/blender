/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include "DNA_image_types.h"

#include "BLI_math_vector_types.hh"

namespace blender::bke::image {

/** Type to use for UDIM tile numbers (1001). */
using TileNumber = int32_t;

struct ImageTileWrapper {
  ImageTile *image_tile;
  ImageTileWrapper(ImageTile *image_tile) : image_tile(image_tile)
  {
  }

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
}  // namespace blender::bke::image
