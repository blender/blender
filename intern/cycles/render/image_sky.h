/*
 * Copyright 2011-2020 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "render/image.h"

CCL_NAMESPACE_BEGIN

class SkyLoader : public ImageLoader {
 private:
  float sun_elevation;
  int altitude;
  float air_density;
  float dust_density;
  float ozone_density;

 public:
  SkyLoader(float sun_elevation,
            int altitude,
            float air_density,
            float dust_density,
            float ozone_density);
  ~SkyLoader();

  bool load_metadata(ImageMetaData &metadata) override;

  bool load_pixels(const ImageMetaData &metadata,
                   void *pixels,
                   const size_t /*pixels_size*/,
                   const bool /*associate_alpha*/) override;

  string name() const override;

  bool equals(const ImageLoader & /*other*/) const override;
};

CCL_NAMESPACE_END
