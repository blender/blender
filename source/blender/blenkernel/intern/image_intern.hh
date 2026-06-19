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

/** Cache key for an image buffer within an #Image datablock. */
struct ImageCacheKey {
  int index;
};

/* Index to indicate we don't store sequences in ibuf. */
#define IMA_NO_INDEX 0x7FEFEFEF
/* Index for UDIM aggregate textures. */
#define IMA_INDEX_UDIM_ATLAS 0x7FEFEFE0
#define IMA_INDEX_UDIM_TILE_MAPPING 0x7FEFEFE1

/* Encode and decode animation frame in index. */
#define IMA_MAKE_INDEX(entry, index) (((entry) << 10) + (index))
#define IMA_INDEX_ENTRY(index) ((index) >> 10)

/** Insert image buffer into image cache. */
void imagecache_put(Image *image, ImageCacheKey key, ImBuf *ibuf);

}  // namespace blender
