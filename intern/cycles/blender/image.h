/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __BLENDER_IMAGE_H__
#define __BLENDER_IMAGE_H__

#include "RNA_blender_cpp.h"

#include "scene/image.h"

CCL_NAMESPACE_BEGIN

class BlenderImageLoader : public ImageLoader {
 public:
  BlenderImageLoader(BL::Image b_image,
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

  BL::Image b_image;
  int frame;
  int tile_number;
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

#endif /* __BLENDER_IMAGE_H__ */
