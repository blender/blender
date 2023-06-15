/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/image_sky.h"

#include "sky_model.h"

#include "util/image.h"
#include "util/log.h"
#include "util/path.h"
#include "util/task.h"

CCL_NAMESPACE_BEGIN

SkyLoader::SkyLoader(float sun_elevation,
                     float altitude,
                     float air_density,
                     float dust_density,
                     float ozone_density)
    : sun_elevation(sun_elevation),
      altitude(altitude),
      air_density(air_density),
      dust_density(dust_density),
      ozone_density(ozone_density)
{
}

SkyLoader::~SkyLoader(){};

bool SkyLoader::load_metadata(const ImageDeviceFeatures &, ImageMetaData &metadata)
{
  metadata.width = 512;
  metadata.height = 128;
  metadata.channels = 3;
  metadata.depth = 1;
  metadata.type = IMAGE_DATA_TYPE_FLOAT4;
  metadata.compress_as_srgb = false;
  return true;
}

bool SkyLoader::load_pixels(const ImageMetaData &metadata,
                            void *pixels,
                            const size_t /*pixels_size*/,
                            const bool /*associate_alpha*/)
{
  /* definitions */
  int width = metadata.width;
  int height = metadata.height;
  float *pixel_data = (float *)pixels;

  /* precompute sky texture */
  const int rows_per_task = divide_up(1024, width);
  parallel_for(blocked_range<size_t>(0, height, rows_per_task),
               [&](const blocked_range<size_t> &r) {
                 SKY_nishita_skymodel_precompute_texture(pixel_data,
                                                         metadata.channels,
                                                         r.begin(),
                                                         r.end(),
                                                         width,
                                                         height,
                                                         sun_elevation,
                                                         altitude,
                                                         air_density,
                                                         dust_density,
                                                         ozone_density);
               });

  return true;
}

string SkyLoader::name() const
{
  return "sky_nishita";
}

bool SkyLoader::equals(const ImageLoader & /*other*/) const
{
  return false;
}

CCL_NAMESPACE_END
