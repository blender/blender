/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "scene/image_loader.h"

CCL_NAMESPACE_BEGIN

class SkyLoader : public ImageLoader {
 private:
  bool multiple_scattering;
  float sun_elevation;
  float altitude;
  float air_density;
  float aerosol_density;
  float ozone_density;

 public:
  SkyLoader(const bool multiple_scattering,
            const float sun_elevation,
            const float altitude,
            const float air_density,
            const float aerosol_density,
            const float ozone_density);
  ~SkyLoader() override;

  bool load_metadata(ImageMetaData &metadata) override;

  bool load_pixels(const ImageMetaData &metadata, void *pixels) override;

  string name() const override;

  bool equals(const ImageLoader &other) const override;
};

CCL_NAMESPACE_END
