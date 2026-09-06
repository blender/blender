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

struct ImageData : NonCopyable {
  Image *image = nullptr;
  CanvasImageUser image_user;

  Map<bke::image::TileNumber, ImBuf *> image_buffers = {};
  Map<bke::image::TileNumber, BufferType> data_buffers = {};
  Map<bke::image::TileNumber, TileColorspaceProcessor> processors = {};

  /** Per undo tile, to quickly check if it was already pushed. */
  Map<bke::image::TileNumber, Array<uint32_t>> undo_tile_pushed = {};

  /** Per seam tile modified state, to only do seam bleeding where needed. */
  Map<bke::image::TileNumber, Array<uint8_t>> seam_tile_modified = {};

  ~ImageData();

  static std::unique_ptr<ImageData> init_active_image(Object &ob, ImagePaintSettings &settings);
  const ImageUser &image_user_get() const
  {
    if (std::holds_alternative<ImageUser *>(image_user)) {
      return *std::get<ImageUser *>(image_user);
    }

    return std::get<ImageUser>(image_user);
  }
};

void do_3d_image_paint_brush(const Depsgraph &depsgraph,
                             const Paint &paint,
                             const Brush &brush,
                             Object &ob,
                             ImageData &image_data,
                             const IndexMask &node_mask);
/** \} */
}  // namespace blender::ed::sculpt_paint::image
