/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "DNA_image_types.h"

struct ImageTileWrapper {
  ImageTile *image_tile;
  ImageTileWrapper(ImageTile *image_tile) : image_tile(image_tile)
  {
  }

  int get_tile_number() const
  {
    return image_tile->tile_number;
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
