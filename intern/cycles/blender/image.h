/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "DNA_image_types.h"

#include "scene/image_loader.h"
#include "scene/image_vdb.h"

struct Image;
struct ImageUser;

CCL_NAMESPACE_BEGIN

class BlenderImageLoader : public ImageLoader {
 public:
  BlenderImageLoader(blender::Image *b_image,
                     blender::ImageUser *b_iuser,
                     const int frame,
                     const int tile_number,
                     const bool is_preview_render);

  bool load_metadata(ImageMetaData &metadata) override;
  bool load_pixels(const ImageMetaData &metadata, void *pixels) override;
  string name() const override;
  bool equals(const ImageLoader &other) const override;

  int get_tile_number() const override;

  blender::Image *b_image;
  blender::ImageUser b_iuser;
  bool free_cache;
};

CCL_NAMESPACE_END
