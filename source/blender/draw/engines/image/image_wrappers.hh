/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2022, Blender Foundation.
 */

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
