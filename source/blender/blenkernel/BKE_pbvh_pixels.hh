/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

#pragma once

#include "BLI_math.h"
#include "BLI_math_vec_types.hh"
#include "BLI_rect.h"
#include "BLI_vector.hh"

#include "DNA_image_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_image.h"
#include "BKE_image_wrappers.hh"

#include "IMB_imbuf_types.h"

namespace blender::bke::pbvh::pixels {

struct TrianglePaintInput {
  int3 vert_indices;
  /**
   * Delta barycentric coordinates between 2 neighboring UV's in the U direction.
   *
   * Only the first two coordinates are stored. The third should be recalculated
   */
  float2 delta_barycentric_coord_u;

  /**
   * Initially only the vert indices are known.
   *
   * delta_barycentric_coord_u is initialized in a later stage as it requires image tile
   * dimensions.
   */
  TrianglePaintInput(const int3 vert_indices)
      : vert_indices(vert_indices), delta_barycentric_coord_u(0.0f, 0.0f)
  {
  }
};

/**
 * Data shared between pixels that belong to the same triangle.
 *
 * Data is stored as a list of structs, grouped by usage to improve performance (improves CPU
 * cache prefetching).
 */
struct Triangles {
  /** Data accessed by the inner loop of the painting brush. */
  Vector<TrianglePaintInput> paint_input;

 public:
  void append(const int3 vert_indices)
  {
    this->paint_input.append(TrianglePaintInput(vert_indices));
  }

  TrianglePaintInput &get_paint_input(const int index)
  {
    return paint_input[index];
  }

  const TrianglePaintInput &get_paint_input(const int index) const
  {
    return paint_input[index];
  }

  void clear()
  {
    paint_input.clear();
  }

  uint64_t size() const
  {
    return paint_input.size();
  }

  uint64_t mem_size() const
  {
    return paint_input.size() * sizeof(TrianglePaintInput);
  }
};

/**
 * Encode sequential pixels to reduce memory footprint.
 */
struct PackedPixelRow {
  /** Barycentric coordinate of the first pixel. */
  float2 start_barycentric_coord;
  /** Image coordinate starting of the first pixel. */
  ushort2 start_image_coordinate;
  /** Number of sequential pixels encoded in this package. */
  ushort num_pixels;
  /** Reference to the pbvh triangle index. */
  ushort triangle_index;
};

/**
 * Node pixel data containing the pixels for a single UDIM tile.
 */
struct UDIMTilePixels {
  /** UDIM Tile number. */
  short tile_number;

  struct {
    bool dirty : 1;
  } flags;

  /* Dirty region of the tile in image space. */
  rcti dirty_region;

  Vector<PackedPixelRow> pixel_rows;

  UDIMTilePixels()
  {
    flags.dirty = false;
    BLI_rcti_init_minmax(&dirty_region);
  }

  void mark_dirty(const PackedPixelRow &pixel_row)
  {
    int2 start_image_coord(pixel_row.start_image_coordinate.x, pixel_row.start_image_coordinate.y);
    BLI_rcti_do_minmax_v(&dirty_region, start_image_coord);
    BLI_rcti_do_minmax_v(&dirty_region, start_image_coord + int2(pixel_row.num_pixels + 1, 0));
    flags.dirty = true;
  }

  void clear_dirty()
  {
    BLI_rcti_init_minmax(&dirty_region);
    flags.dirty = false;
  }
};

struct NodeData {
  struct {
    bool dirty : 1;
  } flags;

  Vector<UDIMTilePixels> tiles;
  Triangles triangles;

  NodeData()
  {
    flags.dirty = false;
  }

  UDIMTilePixels *find_tile_data(const image::ImageTileWrapper &image_tile)
  {
    for (UDIMTilePixels &tile : tiles) {
      if (tile.tile_number == image_tile.get_tile_number()) {
        return &tile;
      }
    }
    return nullptr;
  }

  void mark_region(Image &image, const image::ImageTileWrapper &image_tile, ImBuf &image_buffer)
  {
    UDIMTilePixels *tile = find_tile_data(image_tile);
    if (tile && tile->flags.dirty) {
      BKE_image_partial_update_mark_region(
          &image, image_tile.image_tile, &image_buffer, &tile->dirty_region);
      tile->clear_dirty();
    }
  }

  void clear_data()
  {
    tiles.clear();
    triangles.clear();
  }

  static void free_func(void *instance)
  {
    NodeData *node_data = static_cast<NodeData *>(instance);
    MEM_delete(node_data);
  }
};

NodeData &BKE_pbvh_pixels_node_data_get(PBVHNode &node);
void BKE_pbvh_pixels_mark_image_dirty(PBVHNode &node, Image &image, ImageUser &image_user);

}  // namespace blender::bke::pbvh::pixels
