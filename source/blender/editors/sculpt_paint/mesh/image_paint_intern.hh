/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BKE_image_wrappers.hh"
#include "BKE_paint.hh"

#include "IMB_colormanagement.hh"

namespace blender::ed::sculpt_paint::image {
/* -------------------------------------------------------------------- */
/** \name 3D Texture Paint (Experimental)
 * \{ */

struct TileColorspaceProcessor : NonCopyable {
  ColormanageProcessor buffer_to_linear_processor = {};
  ColormanageProcessor linear_to_buffer_processor = {};
  bool is_noop = true;
  bool is_srgb_byte = false;
};

using BufferType = std::variant<std::monostate, MutableSpan<float4>, MutableSpan<uchar4>>;
using MaskBufferType = std::variant<std::monostate, Span<float4>, Span<uchar4>>;

constexpr int SCRATCH_TILE_BITS = 6;
constexpr int SCRATCH_TILE_SIZE = (1 << SCRATCH_TILE_BITS);
constexpr int SCRATCH_TILE_AREA = SCRATCH_TILE_SIZE * SCRATCH_TILE_SIZE;

/**
 * The per image tile buffers of brushes that read other pixels, which have to finish reading
 * before anything is written.
 */
struct ScratchTiles {
  Array<uint32_t> allocated;

  /**
   * Intermediate tracking for tiles that have been modified to perform final blending without
   * needing to refer to the BVH nodes being processed.
   */
  Array<bool> modified;

  /**
   * Intermediate storage of color / factor data when it cannot be blended with the image as the
   * pixel is processed.
   *
   * Stored as 64x64 contiguous tiles.
   */
  Vector<Vector<float4>, 0> colors;
  Vector<Vector<float>, 0> factors;

  /**
   * Position of every pixel with a non-zero factor in #factors.
   *
   * Needed because border extension pixels cannot easily map to their original location.
   */
  Vector<Vector<float3>, 0> positions;

  explicit ScratchTiles(const int64_t tiles_num)
      : allocated(tiles_num, 0),
        modified(tiles_num, false),
        colors(tiles_num),
        factors(tiles_num),
        positions(tiles_num)
  {
  }
};

struct ImageData : NonCopyable {
  Image *image = nullptr;
  CanvasImageUser image_user;
  bool is_data_colorspace = false;

  Map<bke::image::TileNumber, ImBuf *> image_buffers = {};
  Map<bke::image::TileNumber, BufferType> data_buffers = {};
  Map<bke::image::TileNumber, MaskBufferType> mask_buffers = {};
  Map<bke::image::TileNumber, TileColorspaceProcessor> processors = {};

  /** Per undo tile, to quickly check if it was already pushed. */
  Map<bke::image::TileNumber, Array<uint32_t>> undo_tile_pushed = {};

  /** Per seam tile modified state, to only do seam bleeding where needed. */
  Map<bke::image::TileNumber, Array<bool>> seam_tile_modified = {};

  /** Intermediate storage helper to allow creating scratch tiles in parallel. */
  Mutex scratch_mutex;
  Map<bke::image::TileNumber, ScratchTiles> scratch_tiles = {};

  ~ImageData();

  static std::unique_ptr<ImageData> init_active_image(Object &ob, ImagePaintSettings &settings);
  static std::unique_ptr<ImageData> init_mask_image(ImagePaintSettings &settings);
  const ImageUser &image_user_get() const
  {
    if (std::holds_alternative<ImageUser *>(image_user)) {
      return *std::get<ImageUser *>(image_user);
    }

    return std::get<ImageUser>(image_user);
  }
};

void do_3d_image_paint_brush(const Depsgraph &depsgraph,
                             const ImagePaintSettings &paint_settings,
                             const Brush &brush,
                             Object &ob,
                             ImageData &image_data,
                             ImageData *mask_data,
                             const IndexMask &node_mask);

void do_3d_image_mask_brush(const Depsgraph &depsgraph,
                            const ImagePaintSettings &paint_settings,
                            const Brush &brush,
                            Object &ob,
                            ImageData &mask_image,
                            const IndexMask &node_mask);

void do_3d_image_smear_brush(const Depsgraph &depsgraph,
                             const ImagePaintSettings &paint,
                             const Brush &brush,
                             Object &ob,
                             ImageData &image_data,
                             ImageData *mask_data,
                             const IndexMask &node_mask);
/** \} */
}  // namespace blender::ed::sculpt_paint::image
