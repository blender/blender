/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_rect.hh"
#include "BLI_vector.hh"

#include "DNA_image_types.h"

#include "BKE_image.hh"
#include "BKE_image_wrappers.hh"
#include "BKE_paint_bvh.hh"

#include "IMB_imbuf_types.hh"
#include "IMB_partial_update.hh"

namespace blender::bke::pbvh::pixels {

/**
 * Encode sequential pixels to reduce memory footprint.
 */
struct PackedPixelRow {
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
  image::TileNumber tile_number;

  struct {
    bool dirty : 1;
  } flags;

  /* Dirty region of the tile in image space. */
  rcti dirty_region;

  Vector<PackedPixelRow> pixel_rows;

  /** Offsets into #pixel_rows grouping it into contiguous runs for batch processing. */
  Vector<int> pixel_row_run_starts;

  /** Image coordinate of the first pixel of each run. */
  Vector<ushort2> pixel_row_run_start_coords;

  UDIMTilePixels()
  {
    flags.dirty = false;
    BLI_rcti_init_minmax(&dirty_region);
  }

  void mark_dirty(const Bounds<int2> &bounds)
  {
    BLI_rcti_do_minmax_v(&dirty_region, bounds.min);
    BLI_rcti_do_minmax_v(&dirty_region, bounds.max);
    flags.dirty = true;
  }

  void clear_dirty()
  {
    BLI_rcti_init_minmax(&dirty_region);
    flags.dirty = false;
  }
};

/**
 * Contains triangle/pixel data used during texture painting.
 */
struct PixelNode {
  struct {
    /* Indicates whether the node data was painted to */
    bool dirty : 1;

    /* Indicates whether the node data should be rebuilt */
    bool rebuild : 1;
  } flags;

  Vector<UDIMTilePixels, 0> tiles;

  struct {
    /** Corresponding index into triangles */
    Array<int, 0> tri_indices;

    /**
     * Per primitive affine map from image pixel coordinate to object space position:
     * P = pixel_to_position * (pixel_x, pixel_y, 1)
     */
    Array<float3x3, 0> pixel_to_position;
  } uv_primitives;

  PixelNode()
  {
    flags.dirty = false;
    flags.rebuild = true;
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

  void mark_region(UDIMTilePixels &tile, ImBuf &image_buffer)
  {
    if (tile.flags.dirty) {
      if (image_buffer.color_mode == ImColorMode::BW) {
        image_buffer.color_mode = ImColorMode::RGBA;
        IMB_partial_update_mark_full(&image_buffer);
      }
      else {
        IMB_partial_update_mark_region(&image_buffer, tile.dirty_region);
      }
      IMB_mark_dirty(&image_buffer);
      tile.clear_dirty();
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
    uv_primitives.tri_indices.reinitialize(0);
    uv_primitives.pixel_to_position.reinitialize(0);
  }
};

/* -------------------------------------------------------------------- */
/** \name Fix non-manifold edge bleeding.
 * \{ */

/**
 * Each UDIM tile is split into smaller (64x64) seam tiles for which we can
 * do seam bleeding. These are tagged as modified during painting, and only
 * the modified subset will be processed.
 */
constexpr int SEAM_TILE_BITS = 6;
constexpr int SEAM_TILE_SIZE = 1 << SEAM_TILE_BITS;

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

struct CopyPixelTile {
  image::TileNumber tile_number;
  Vector<CopyPixelGroup> groups;
  Vector<DeltaCopyPixelCommand> command_deltas;

  /** The groups used by each seam tile, as an index range into #groups which is
   * sorted by seam tile. */
  Map<int, IndexRange> seam_tile_to_groups;

  CopyPixelTile(image::TileNumber tile_number) : tile_number(tile_number) {}

  static int seam_tile_index(const int2 source, const int seam_tiles_x)
  {
    return (source.x >> SEAM_TILE_BITS) + (source.y >> SEAM_TILE_BITS) * seam_tiles_x;
  }

  void build_seam_tile_map(const int2 resolution);

  void copy_pixels(ImBuf &tile_buffer, IndexRange group_range) const;

  void print_compression_rate() const;
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

/**
 * Storage for texture painting on bke::pbvh::Tree level.
 */
struct PixelData {
  struct {
    bool dirty : 1;
  } flags;

  /** Per ImageTile the pixels to copy to fix non-manifold bleeding. */
  CopyPixelTiles tiles_copy_pixels;

  Vector<PixelNode> nodes;
};

void mark_image_dirty(bke::pbvh::Node &node,
                      PixelNode &pixel_node,
                      Image &image,
                      Map<image::TileNumber, ImBuf *> &buffers);
PixelData &data_get(bke::pbvh::Tree &pbvh);
void collect_dirty_tiles(PixelNode &pixel_node, Vector<image::TileNumber> &r_dirty_tiles);

void copy_pixels(bke::pbvh::Tree &pbvh,
                 Map<image::TileNumber, ImBuf *> &buffers,
                 image::TileNumber tile_number,
                 Span<uint8_t> seam_tiles_modified,
                 FunctionRef<void(int x_start, int x_end, int y)> push_undo_tiles);

}  // namespace blender::bke::pbvh::pixels
