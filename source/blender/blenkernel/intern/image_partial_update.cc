/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * \file
 * \ingroup bke
 *
 * Utilities for marking all image buffers in an image for update.
 * Temporary until callers are updated to call IMB API directly.
 */

#include <mutex>

#include "BKE_image.hh"

#include "DNA_image_types.h"

#include "IMB_cache.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_partial_update.hh"

#include "atomic_ops.h"

namespace blender {

void BKE_image_partial_update_mark_region(Image * /*image*/,
                                          const ImageTile * /*image_tile*/,
                                          const ImBuf *image_buffer,
                                          const rcti *updated_region)
{
  if (image_buffer != nullptr) {
    IMB_partial_update_mark_region(const_cast<ImBuf *>(image_buffer), *updated_region);
  }
}

static void image_partial_update_mark_buffers_full(Image *image)
{
  if (image->runtime->cache == nullptr) {
    return;
  }
  ImBufCacheIter *iter = IMB_cacheIter_new(image->runtime->cache);
  while (!IMB_cacheIter_done(iter)) {
    if (ImBuf *ibuf = IMB_cacheIter_getImBuf(iter)) {
      IMB_partial_update_mark_full(ibuf);
    }
    IMB_cacheIter_step(iter);
  }
  IMB_cacheIter_free(iter);
}

static void image_partial_update_mark_full_update(Image *image, const bool cache_locked)
{
  if (cache_locked) {
    image_partial_update_mark_buffers_full(image);
  }
  else {
    std::scoped_lock lock(image->runtime->cache_mutex);
    image_partial_update_mark_buffers_full(image);
  }
}

void BKE_image_partial_update_mark_full_update(Image *image)
{
  image_partial_update_mark_full_update(image, false);
}

void BKE_image_partial_update_mark_full_update_cache_locked(Image *image)
{
  image_partial_update_mark_full_update(image, true);
}

}  // namespace blender
