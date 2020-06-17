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

#include "render/image_sky.h"

#include "util/util_image.h"
#include "util/util_logging.h"
#include "util/util_path.h"
#include "util/util_sky_model.h"

CCL_NAMESPACE_BEGIN

SkyLoader::SkyLoader(
    float sun_elevation, int altitude, float air_density, float dust_density, float ozone_density)
    : sun_elevation(sun_elevation),
      altitude(altitude),
      air_density(air_density),
      dust_density(dust_density),
      ozone_density(ozone_density)
{
}

SkyLoader::~SkyLoader(){};

bool SkyLoader::load_metadata(ImageMetaData &metadata)
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
  float altitude_f = (float)altitude;

  /* precompute sky texture */
  const int num_chunks = TaskScheduler::num_threads();
  const int chunk_size = height / num_chunks;
  TaskPool pool;
  for (int chunk = 0; chunk < num_chunks; chunk++) {
    const int chunk_start = chunk * chunk_size;
    const int chunk_end = (chunk + 1 < num_chunks) ? (chunk + 1) * chunk_size : height;
    pool.push(function_bind(&nishita_skymodel_precompute_texture,
                            pixel_data,
                            metadata.channels,
                            chunk_start,
                            chunk_end,
                            width,
                            height,
                            sun_elevation,
                            altitude_f,
                            air_density,
                            dust_density,
                            ozone_density));
  }
  pool.wait_work();

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
