/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

#pragma once

#include <functional>

#include "BLI_math.h"
#include "BLI_math_vector.hh"
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
    return this->vert_indices.as_span().size_in_bytes();
  }
};

struct UVPrimitivePaintInput {
  /** Corresponding index into PaintGeometryPrimitives */
  int64_t geometry_primitive_index;
  /**
   * Delta barycentric coordinates between 2 neighboring UVs in the U direction.
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

  UDIMTileUndo(short tile_number, rcti &region) : tile_number(tile_number), region(region) {}
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
      if (tile.pixel_rows.size() == 0) {
        continue;
      }

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

  void collect_dirty_tiles(Vector<image::TileNumber> &r_dirty_tiles)
  {
    for (UDIMTilePixels &tile : tiles) {
      if (tile.flags.dirty) {
        r_dirty_tiles.append_non_duplicates(tile.tile_number);
      }
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

/* -------------------------------------------------------------------- */
/** \name Fix non-manifold edge bleeding.
 * \{ */

struct DeltaCopyPixelCommand {
  char2 delta_source_1;
  char2 delta_source_2;
  uint8_t mix_factor;

  DeltaCopyPixelCommand(char2 delta_source_1, char2 delta_source_2, uint8_t mix_factor)
      : delta_source_1(delta_source_1), delta_source_2(delta_source_2), mix_factor(mix_factor)
  {
  }
};

struct CopyPixelGroup {
  int2 start_destination;
  int2 start_source_1;
  int64_t start_delta_index;
  int num_deltas;
};

/** Pixel copy command to mix 2 source pixels and write to a destination pixel. */
struct CopyPixelCommand {
  /** Pixel coordinate to write to. */
  int2 destination;
  /** Pixel coordinate to read first source from. */
  int2 source_1;
  /** Pixel coordinate to read second source from. */
  int2 source_2;
  /** Factor to mix between first and second source. */
  float mix_factor;

  CopyPixelCommand() = default;
  CopyPixelCommand(const CopyPixelGroup &group)
      : destination(group.start_destination),
        source_1(group.start_source_1),
        source_2(),
        mix_factor(0.0f)
  {
  }

  template<typename T>
  void mix_source_and_write_destination(image::ImageBufferAccessor<T> &tile_buffer) const
  {
    float4 source_color_1 = tile_buffer.read_pixel(source_1);
    float4 source_color_2 = tile_buffer.read_pixel(source_2);
    float4 destination_color = source_color_1 * (1.0f - mix_factor) + source_color_2 * mix_factor;
    tile_buffer.write_pixel(destination, destination_color);
  }

  void apply(const DeltaCopyPixelCommand &item)
  {
    destination.x += 1;
    source_1 += int2(item.delta_source_1);
    source_2 = source_1 + int2(item.delta_source_2);
    mix_factor = float(item.mix_factor) / 255.0f;
  }

  DeltaCopyPixelCommand encode_delta(const CopyPixelCommand &next_command) const
  {
    return DeltaCopyPixelCommand(char2(next_command.source_1 - source_1),
                                 char2(next_command.source_2 - next_command.source_1),
                                 uint8_t(next_command.mix_factor * 255));
  }

  bool can_be_extended(const CopyPixelCommand &command) const
  {
    /* Can only extend sequential pixels. */
    if (destination.x != command.destination.x - 1 || destination.y != command.destination.y) {
      return false;
    }

    /* Can only extend when the delta between with the previous source fits in a single byte. */
    int2 delta_source_1 = source_1 - command.source_1;
    if (max_ii(UNPACK2(blender::math::abs(delta_source_1))) > 127) {
      return false;
    }
    return true;
  }
};

struct CopyPixelTile {
  image::TileNumber tile_number;
  Vector<CopyPixelGroup> groups;
  Vector<DeltaCopyPixelCommand> command_deltas;

  CopyPixelTile(image::TileNumber tile_number) : tile_number(tile_number) {}

  void copy_pixels(ImBuf &tile_buffer, IndexRange group_range) const
  {
    if (tile_buffer.rect_float) {
      image::ImageBufferAccessor<float4> accessor(tile_buffer);
      copy_pixels<float4>(accessor, group_range);
    }
    else {
      image::ImageBufferAccessor<int> accessor(tile_buffer);
      copy_pixels<int>(accessor, group_range);
    }
  }

  void print_compression_rate()
  {
    int decoded_size = command_deltas.size() * sizeof(CopyPixelCommand);
    int encoded_size = groups.size() * sizeof(CopyPixelGroup) +
                       command_deltas.size() * sizeof(DeltaCopyPixelCommand);
    printf("Tile %d compression rate: %d->%d = %d%%\n",
           tile_number,
           decoded_size,
           encoded_size,
           int(100.0 * float(encoded_size) / float(decoded_size)));
  }

 private:
  template<typename T>
  void copy_pixels(image::ImageBufferAccessor<T> &image_buffer, IndexRange group_range) const
  {
    for (const int64_t group_index : group_range) {
      const CopyPixelGroup &group = groups[group_index];
      CopyPixelCommand copy_command(group);
      for (const DeltaCopyPixelCommand &item : Span<const DeltaCopyPixelCommand>(
               &command_deltas[group.start_delta_index], group.num_deltas))
      {
        copy_command.apply(item);
        copy_command.mix_source_and_write_destination<T>(image_buffer);
      }
    }
  }
};

struct CopyPixelTiles {
  Vector<CopyPixelTile> tiles;

  std::optional<std::reference_wrapper<CopyPixelTile>> find_tile(image::TileNumber tile_number)
  {
    for (CopyPixelTile &tile : tiles) {
      if (tile.tile_number == tile_number) {
        return tile;
      }
    }
    return std::nullopt;
  }

  void clear()
  {
    tiles.clear();
  }
};

/** \} */

struct PBVHData {
  /* Per UVPRimitive contains the paint data. */
  PaintGeometryPrimitives geom_primitives;

  /** Per ImageTile the pixels to copy to fix non-manifold bleeding. */
  CopyPixelTiles tiles_copy_pixels;

  void clear_data()
  {
    geom_primitives.clear();
  }
};

NodeData &BKE_pbvh_pixels_node_data_get(PBVHNode &node);
void BKE_pbvh_pixels_mark_image_dirty(PBVHNode &node, Image &image, ImageUser &image_user);
PBVHData &BKE_pbvh_pixels_data_get(PBVH &pbvh);
void BKE_pbvh_pixels_collect_dirty_tiles(PBVHNode &node, Vector<image::TileNumber> &r_dirty_tiles);

void BKE_pbvh_pixels_copy_pixels(PBVH &pbvh,
                                 Image &image,
                                 ImageUser &image_user,
                                 image::TileNumber tile_number);

}  // namespace blender::bke::pbvh::pixels
