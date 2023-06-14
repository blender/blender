/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __IMAGE_OIIO__
#define __IMAGE_OIIO__

#include "scene/image.h"

CCL_NAMESPACE_BEGIN

class OIIOImageLoader : public ImageLoader {
 public:
  OIIOImageLoader(const string &filepath);
  ~OIIOImageLoader();

  bool load_metadata(const ImageDeviceFeatures &features, ImageMetaData &metadata) override;

  bool load_pixels(const ImageMetaData &metadata,
                   void *pixels,
                   const size_t pixels_size,
                   const bool associate_alpha) override;

  string name() const override;

  ustring osl_filepath() const override;

  bool equals(const ImageLoader &other) const override;

 protected:
  ustring filepath;
};

CCL_NAMESPACE_END

#endif /* __IMAGE_OIIO__ */
