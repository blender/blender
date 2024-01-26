/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#include "tile_highlight.h"

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_hash.hh"

#include "RE_pipeline.h"

namespace blender::render {

TilesHighlight::Tile::Tile(const RenderResult *result) : rect(result->tilerect) {}

TilesHighlight::Tile::Tile(const int x, const int y, const int width, const int height)
{
  BLI_rcti_init(&rect, x, x + width, y, y + height);
}

uint64_t TilesHighlight::Tile::hash() const
{
  return get_default_hash(rect.xmin, rect.xmax, rect.ymin, rect.ymax);
}

void TilesHighlight::highlight_tile_for_result(const RenderResult *result)
{
  const Tile tile(result);
  highlight_tile(tile);
}

void TilesHighlight::unhighlight_tile_for_result(const RenderResult *result)
{
  const Tile tile(result);
  unhighlight_tile(tile);
}

void TilesHighlight::highlight_tile(const int x, const int y, const int width, const int height)
{
  const Tile tile(x, y, width, height);
  highlight_tile(tile);
}

void TilesHighlight::unhighlight_tile(const int x, const int y, const int width, const int height)
{
  const Tile tile(x, y, width, height);
  unhighlight_tile(tile);
}

void TilesHighlight::highlight_tile(const Tile &tile)
{
  std::unique_lock lock(mutex_);

  highlighted_tiles_set_.add(tile);
  did_tiles_change_ = true;
}

void TilesHighlight::unhighlight_tile(const Tile &tile)
{
  std::unique_lock lock(mutex_);

  highlighted_tiles_set_.remove(tile);
  did_tiles_change_ = true;
}

void TilesHighlight::clear()
{
  std::unique_lock lock(mutex_);

  highlighted_tiles_set_.clear();
  cached_highlighted_tiles_.clear_and_shrink();
}

Span<rcti> TilesHighlight::get_all_highlighted_tiles() const
{
  std::unique_lock lock(mutex_);

  /* Updated cached flat list if needed. */
  if (did_tiles_change_) {
    if (highlighted_tiles_set_.is_empty()) {
      cached_highlighted_tiles_.clear_and_shrink();
    }
    else {
      cached_highlighted_tiles_.reserve(highlighted_tiles_set_.size());
      for (const Tile &tile : highlighted_tiles_set_) {
        cached_highlighted_tiles_.append(tile.rect);
      }
    }

    did_tiles_change_ = false;
  }

  return cached_highlighted_tiles_;
}

}  // namespace blender::render
