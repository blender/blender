/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "DNA_image_types.h"

#include "RNA_blender_cpp.hh"

#include "scene/image.h"

struct Image;
struct ImageUser;

CCL_NAMESPACE_BEGIN

class BlenderImageLoader : public ImageLoader {
 public:
  BlenderImageLoader(::Image *b_image,
                     ::ImageUser *b_iuser,
                     const int frame,
                     const int tile_number,
                     const bool is_preview_render);

  bool load_metadata(const ImageDeviceFeatures &features, ImageMetaData &metadata) override;
  bool load_pixels(const ImageMetaData &metadata,
                   void *pixels,
                   const size_t pixels_size,
                   const bool associate_alpha) override;
  string name() const override;
  bool equals(const ImageLoader &other) const override;

  int get_tile_number() const override;

  ::Image *b_image;
  ::ImageUser b_iuser;
  bool free_cache;
};

class BlenderPointDensityLoader : public ImageLoader {
 public:
  BlenderPointDensityLoader(BL::Depsgraph depsgraph, BL::ShaderNodeTexPointDensity b_node);

  bool load_metadata(const ImageDeviceFeatures &features, ImageMetaData &metadata) override;
  bool load_pixels(const ImageMetaData &metadata,
                   void *pixels,
                   const size_t pixels_size,
                   const bool associate_alpha) override;
  string name() const override;
  bool equals(const ImageLoader &other) const override;

  BL::Depsgraph b_depsgraph;
  BL::ShaderNodeTexPointDensity b_node;
};

CCL_NAMESPACE_END
