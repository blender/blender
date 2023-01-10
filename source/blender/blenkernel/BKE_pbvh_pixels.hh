/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

#pragma once

#include "BLI_math.h"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.h"
#include "BLI_vector.hh"

#include "DNA_image_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_image.h"
#include "BKE_image_wrappers.hh"

#include "IMB_imbuf_types.h"

namespace blender::bke::pbvh::pixels {

/**
 * Data shared between pixels that belong to the same triangle.
 *
 * Data is stored as a list of structs, grouped by usage to improve performance (improves CPU
 * cache prefetching).
 */
struct PaintGeometryPrimitives {
  /** Data accessed by the inner loop of the painting brush. */
  Vector<int3> vert_indices;

 public:
  void append(const int3 vert_indices)
  {
    this->vert_indices.append(vert_indices);
  }

  const int3 &get_vert_indices(const int index) const
  {
    return vert_indices[index];
  }

  void clear()
  {
    vert_indices.clear();
  }

  int64_t size() const
  {
    return vert_indices.size();
  }

  int64_t mem_size() const
  {
    return size() * sizeof(int3);
  }
};

struct UVPrimitivePaintInput {
  /** Corresponding index into PaintGeometryPrimitives */
  int64_t geometry_primitive_index;
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
  UVPrimitivePaintInput(int64_t geometry_primitive_index)
      : geometry_primitive_index(geometry_primitive_index), delta_barycentric_coord_u(0.0f, 0.0f)
  {
  }
};

struct PaintUVPrimitives {
  /** Data accessed by the inner loop of the painting brush. */
  Vector<UVPrimitivePaintInput> paint_input;

  void append(int64_t geometry_primitive_index)
  {
    this->paint_input.append(UVPrimitivePaintInput(geometry_primitive_index));
  }

  UVPrimitivePaintInput &last()
  {
    return paint_input.last();
  }

  const UVPrimitivePaintInput &get_paint_input(uint64_t index) const
  {
    return paint_input[index];
  }

  void clear()
  {
    paint_input.clear();
  }

  int64_t size() const
  {
    return paint_input.size();
  }

  int64_t mem_size() const
  {
    return size() * sizeof(UVPrimitivePaintInput);
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
  ushort uv_primitive_index;
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

struct UDIMTileUndo {
  short tile_number;
  rcti region;

  UDIMTileUndo(short tile_number, rcti &region) : tile_number(tile_number), region(region)
  {
  }
};

struct NodeData {
  struct {
    bool dirty : 1;
  } flags;

  Vector<UDIMTilePixels> tiles;
  Vector<UDIMTileUndo> undo_regions;
  PaintUVPrimitives uv_primitives;

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

  void rebuild_undo_regions()
  {
    undo_regions.clear();
    for (UDIMTilePixels &tile : tiles) {
      rcti region;
      BLI_rcti_init_minmax(&region);
      for (PackedPixelRow &pixel_row : tile.pixel_rows) {
        BLI_rcti_do_minmax_v(
            &region, int2(pixel_row.start_image_coordinate.x, pixel_row.start_image_coordinate.y));
        BLI_rcti_do_minmax_v(&region,
                             int2(pixel_row.start_image_coordinate.x + pixel_row.num_pixels + 1,
                                  pixel_row.start_image_coordinate.y + 1));
      }
      undo_regions.append(UDIMTileUndo(tile.tile_number, region));
    }
  }

  void mark_region(Image &image, const image::ImageTileWrapper &image_tile, ImBuf &image_buffer)
  {
    UDIMTilePixels *tile = find_tile_data(image_tile);
    if (tile && tile->flags.dirty) {
      if (image_buffer.planes == 8) {
        image_buffer.planes = 32;
        BKE_image_partial_update_mark_full_update(&image);
      }
      else {
        BKE_image_partial_update_mark_region(
            &image, image_tile.image_tile, &image_buffer, &tile->dirty_region);
      }
      tile->clear_dirty();
    }
  }

  void clear_data()
  {
    tiles.clear();
    uv_primitives.clear();
  }

  static void free_func(void *instance)
  {
    NodeData *node_data = static_cast<NodeData *>(instance);
    MEM_delete(node_data);
  }
};

struct PBVHData {
  /* Per UVPRimitive contains the paint data. */
  PaintGeometryPrimitives geom_primitives;

  void clear_data()
  {
    geom_primitives.clear();
  }
};

NodeData &BKE_pbvh_pixels_node_data_get(PBVHNode &node);
void BKE_pbvh_pixels_mark_image_dirty(PBVHNode &node, Image &image, ImageUser &image_user);
PBVHData &BKE_pbvh_pixels_data_get(PBVH &pbvh);

}  // namespace blender::bke::pbvh::pixels
