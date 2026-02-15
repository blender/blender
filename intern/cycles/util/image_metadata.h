/* SPDX-FileCopyrightText: 2011-2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cstdlib>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/string_view.h>

#include "util/colorspace.h"
#include "util/string.h"
#include "util/types_image.h"

CCL_NAMESPACE_BEGIN

/* Image MetaData
 *
 * Information about the image that is available before the image pixels are loaded.
 * This can be filled by an image loaded through OpenImageIO, but is also used for
 * custom image loaders that may e.g. load an image from memory or auto generate it. */

class ImageMetaData {
 public:
  /* Input image dimensions and format. */
  int channels = 0;
  int64_t width = 0, height = 0;
  ImageDataType type = IMAGE_DATA_NUM_TYPES;

  /* Input colorspace hints. */
  string colorspace_file_hint;
  const char *colorspace_file_format = "";

  /* Input NanoVDB data. */
  int64_t nanovdb_byte_size = 0;
  bool use_transform_3d = false;
  Transform transform_3d = transform_identity();

  /* Auto determined by finalize. */
  ustring colorspace = u_colorspace_scene_linear;
  bool is_compressible_as_srgb = false;
  bool is_unassociated_alpha = false;
  bool ignore_alpha = false;
  bool is_channel_packed = false;
  bool is_cmyk = false;

  /* Constructor */
  ImageMetaData();
  bool operator==(const ImageMetaData &other) const;

  /* Finalize image metadata, with optional user provided alpha type and colorspace. */
  void finalize(const ImageAlphaType alpha_type = IMAGE_ALPHA_AUTO);

  /* Image info. */
  bool is_float() const;
  bool is_half() const;
  bool is_rgba() const;
  TypeDesc typedesc() const;

  /* Memory stats. */
  size_t memory_size() const;
  size_t pixel_memory_size() const;

  /* OpenImageIO metadata and pixel loading. */
  bool oiio_load_metadata(OIIO::string_view filepath, OIIO::ImageSpec *r_spec = nullptr);
  bool oiio_load_pixels(OIIO::string_view filepath, void *pixels, const bool flip_y = true) const;

  /* Change data type to float. */
  void make_float();

  /* Modify pixel data to conform to formats known to the kernel. That means 1 or 4 channels,
   * a known ImageDataType, associated alpha and no NaN or infinite values. */
  void conform_pixels(void *pixels) const;
  void conform_pixels(void *pixels,
                      const int64_t width,
                      const int64_t height,
                      const int64_t x_stride,
                      const int64_t in_y_stride,
                      const int64_t out_y_stride) const;
};

CCL_NAMESPACE_END
