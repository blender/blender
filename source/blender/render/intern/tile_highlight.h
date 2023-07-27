/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#pragma once

#include "BLI_rect.h"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include <mutex>

struct RenderResult;

namespace blender::render {

class TilesHighlight {
 public:
  TilesHighlight() = default;
  ~TilesHighlight() = default;

  void highlight_tile_for_result(const RenderResult *result);
  void unhighlight_tile_for_result(const RenderResult *result);

  void highlight_tile(int x, int y, int width, int height);
  void unhighlight_tile(int x, int y, int width, int height);

  void clear();

  Span<rcti> get_all_highlighted_tiles() const;

 private:
  struct Tile {
    Tile() = default;
    explicit Tile(const RenderResult *result);
    explicit Tile(int x, int y, int width, int height);

    uint64_t hash() const;

    inline bool operator==(const Tile &other) const
    {
      return rect == other.rect;
    }
    inline bool operator!=(const Tile &other) const
    {
      return !(*this == other);
    }

    rcti rect = {0, 0, 0, 0};
  };

  void highlight_tile(const Tile &tile);
  void unhighlight_tile(const Tile &tile);

  mutable std::mutex mutex_;
  Set<Tile> highlighted_tiles_set_;

  /* Cached flat list of currently highlighted tiles for a fast access via API. */
  mutable bool did_tiles_change_ = false;
  mutable Vector<rcti> cached_highlighted_tiles_;
};

}  // namespace blender::render
