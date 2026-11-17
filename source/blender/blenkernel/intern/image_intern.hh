/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Internal interface between image code in blenkernel.
 */

#pragma once

namespace blender {

struct Image;
struct ImBuf;

enum class ImageUDIMTexture {
  /** Not a UDIM aggregate. */
  None = 0,
  /** All tiles packed into a single 2D array texture. */
  Atlas,
  /** 1D-array lookup texture mapping tile number to atlas layer + UV offset/scale. */
  TileMapping,
};

#define IMA_NO_INDEX 0x7FEFEFEF
#define IMA_NO_VIEW 0x7FEFEFEF
/* Index for a missing multilayer layer, to cache a failed load. */
#define IMA_INDEX_MISSING_LAYER ((1 << 10) - 1)

struct ImageCacheKey {
  /** UDIM buffer type. */
  ImageUDIMTexture udim_type = ImageUDIMTexture::None;
  /* UDIM tile number. */
  int tile_number = 0;
  /** Image sequence frame. */
  int frame = 0;
  /** Multiview. */
  int view = IMA_NO_VIEW;
  /** Multilayer. */
  int multi_index = IMA_NO_INDEX;

  friend bool operator==(const ImageCacheKey &a, const ImageCacheKey &b) = default;
};

/** Insert image buffer into image cache. */
void imagecache_put(Image *image, ImageCacheKey key, ImBuf *ibuf);

}  // namespace blender
