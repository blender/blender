/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "BLI_map.hh"

#include "GPU_texture.hh"

#include "DNA_image_types.h"

#include "COM_cached_resource.hh"

namespace blender::realtime_compositor {

class Context;

/* ------------------------------------------------------------------------------------------------
 * Cached Image Key.
 */
class CachedImageKey {
 public:
  ImageUser image_user;
  std::string pass_name;

  CachedImageKey(ImageUser image_user, std::string pass_name);

  uint64_t hash() const;
};

bool operator==(const CachedImageKey &a, const CachedImageKey &b);

/* -------------------------------------------------------------------------------------------------
 * Cached Image.
 *
 * A cached resource that computes and caches a GPU texture containing the contents of the image
 * with the given image user. */
class CachedImage : public CachedResource {
 private:
  GPUTexture *texture_ = nullptr;

 public:
  CachedImage(Context &context, Image *image, ImageUser *image_user, const char *pass_name);

  ~CachedImage();

  GPUTexture *texture();
};

/* ------------------------------------------------------------------------------------------------
 * Cached Image Container.
 */
class CachedImageContainer : CachedResourceContainer {
 private:
  Map<std::string, Map<CachedImageKey, std::unique_ptr<CachedImage>>> map_;

 public:
  void reset() override;

  /* Check if the given image ID has changed since the last time it was retrieved through its
   * recalculate flag, and if so, invalidate its corresponding cached image and reset the
   * recalculate flag to ready it to track the next change. Then, check if there is an available
   * CachedImage cached resource with the given image user and pass_name in the container, if one
   * exists, return it, otherwise, return a newly created one and add it to the container. In both
   * cases, tag the cached resource as needed to keep it cached for the next evaluation. */
  GPUTexture *get(Context &context,
                  Image *image,
                  const ImageUser *image_user,
                  const char *pass_name);
};

}  // namespace blender::realtime_compositor
