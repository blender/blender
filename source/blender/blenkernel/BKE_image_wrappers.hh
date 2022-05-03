/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include "DNA_image_types.h"

#include "BLI_math_vec_types.hh"

namespace blender::bke::image {

struct ImageTileWrapper {
  ImageTile *image_tile;
  ImageTileWrapper(ImageTile *image_tile) : image_tile(image_tile)
  {
  }

  int get_tile_number() const
  {
    return image_tile->tile_number;
  }

  int2 get_tile_offset() const
  {
    return int2(get_tile_x_offset(), get_tile_y_offset());
  }

  int get_tile_x_offset() const
  {
    int tile_number = get_tile_number();
    return (tile_number - 1001) % 10;
  }

  int get_tile_y_offset() const
  {
    int tile_number = get_tile_number();
    return (tile_number - 1001) / 10;
  }
};
}  // namespace blender::bke::image
