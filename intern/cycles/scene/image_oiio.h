/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "scene/image.h"

#include "util/cache_limiter.h"
#include "util/image.h"
#include "util/string.h"
#include "util/thread.h"

CCL_NAMESPACE_BEGIN

class OIIOImageLoader : public ImageLoader {
 public:
  OIIOImageLoader(const string &filepath);
  ~OIIOImageLoader() override;

  bool resolve_texture_cache(const bool auto_generate,
                             const string &texture_cache_path,
                             const ustring &colorspace,
                             const ImageAlphaType alpha_type,
                             Progress &progress) override;

  bool load_metadata(ImageMetaData &metadata) override;

  bool load_pixels(const ImageMetaData &metadata, void *pixels) override;

  bool load_pixels_tile(const ImageMetaData &metadata,
                        const int miplevel,
                        const int64_t x,
                        const int64_t y,
                        const int64_t w,
                        const int64_t h,
                        const int64_t x_stride,
                        const int64_t y_stride,
                        const int64_t padding,
                        const ExtensionType extension,
                        uint8_t *pixels) override;

  string name() const override;

  bool equals(const ImageLoader &other) const override;

 protected:
  const string &get_filepath() const;

  string original_filepath;
  string texture_cache_filepath;
  CacheHandle<ImageInput> texture_cache_file_handle;
  bool texture_cache_file_handle_failed = false;
};

CCL_NAMESPACE_END
