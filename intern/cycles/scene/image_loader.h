/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/param.h"
#include "util/string.h"
#include "util/types_image.h"

CCL_NAMESPACE_BEGIN

class Progress;
class ImageMetaData;

/* Parameters for image loading. */
struct ImageLoaderParams {
  bool use_texture_cache = true;
  bool auto_texture_cache = true;
  string texture_cache_path;
  ustring colorspace;
  ImageAlphaType alpha_type;
};

/* Image loader base class, that can be subclassed to load image data
 * from custom sources (file, memory, procedurally generated, etc). */
class ImageLoader {
 public:
  ImageLoader();
  virtual ~ImageLoader() = default;

  /* Load metadata without actual image yet, should be fast. */
  virtual bool load_metadata(ImageMetaData &metadata,
                             const ImageLoaderParams &params,
                             Progress &progress) = 0;

  /* Load full image pixels.
   * This is expected to call metadata.conform_pixels(). */
  virtual bool load_pixels(const ImageMetaData &metadata, void *pixels) = 0;

  /* Load pixels for a single tile, if ImageMetaData.tile_size is set.
   * This is expected to call metadata.conform_pixels(). */
  virtual bool load_pixels_tile(const ImageMetaData & /*metadata*/,
                                const int /*miplevel*/,
                                const int64_t /*x*/,
                                const int64_t /*y*/,
                                const int64_t /*w*/,
                                const int64_t /*h*/,
                                const int64_t /*x_stride*/,
                                const int64_t /*y_stride*/,
                                const int64_t /*padding*/,
                                const ExtensionType /*extension*/,
                                uint8_t * /*pixels*/)
  {
    return false;
  }

  /* Name for logs and stats. */
  virtual string name() const = 0;

  /* Optional for tiled textures loaded externally. */
  virtual int get_tile_number() const;

  /* Free any memory used for loading metadata and pixels. */
  virtual void cleanup() {};

  /* Compare avoid loading the same image multiple times. */
  virtual bool equals(const ImageLoader &other) const = 0;
  static bool equals(const ImageLoader *a, const ImageLoader *b);

  virtual bool is_vdb_loader() const;

  /* Work around for no RTTI. */
};

CCL_NAMESPACE_END
