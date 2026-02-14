/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "scene/image_loader.h"

#include "util/cache_limiter.h"
#include "util/image.h"
#include "util/progress.h"
#include "util/string.h"

CCL_NAMESPACE_BEGIN

class OIIOImageLoader : public ImageLoader {
 public:
  OIIOImageLoader(const string &filepath);
  ~OIIOImageLoader() override;

  bool load_metadata(ImageMetaData &metadata,
                     const ImageLoaderParams &params,
                     Progress &progress) override;

  bool load_pixels(const ImageMetaData &metadata, void *pixels) override;

  bool load_pixels_tile(const ImageMetaData &metadata,
                        int miplevel,
                        int64_t x,
                        int64_t y,
                        int64_t w,
                        int64_t h,
                        int64_t x_stride,
                        int64_t y_stride,
                        int64_t padding,
                        ExtensionType extension,
                        uint8_t *pixels) override;

  string name() const override;

  bool equals(const ImageLoader &other) const override;

 protected:
  const string &get_filepath() const;

  string original_filepath_;
  string texture_cache_filepath_;
  CacheHandle<ImageInput> texture_cache_file_handle;
  bool texture_cache_file_handle_failed = false;
};

CCL_NAMESPACE_END
