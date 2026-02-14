/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/image_sky.h"

#include "util/image_metadata.h"

#include "sky_nishita.h"

CCL_NAMESPACE_BEGIN

SkyLoader::SkyLoader(const bool multiple_scattering,
                     const float sun_elevation,
                     const float altitude,
                     const float air_density,
                     const float aerosol_density,
                     const float ozone_density)
    : multiple_scattering(multiple_scattering),
      sun_elevation(sun_elevation),
      altitude(altitude),
      air_density(air_density),
      aerosol_density(aerosol_density),
      ozone_density(ozone_density)
{
}

SkyLoader::~SkyLoader() = default;

bool SkyLoader::load_metadata(ImageMetaData &metadata)
{
  metadata.width = 512;
  metadata.height = 256;
  metadata.channels = 3;
  metadata.type = IMAGE_DATA_TYPE_FLOAT4;
  metadata.is_compressible_as_srgb = false;
  return true;
}

bool SkyLoader::load_pixels(const ImageMetaData &metadata, void *pixels)
{
  /* Precompute Sky LUT */
  int width = metadata.width;
  int height = metadata.height;
  float *pixel_data = (float *)pixels;
  if (multiple_scattering) {
    SKY_multiple_scattering_precompute_texture(pixel_data,
                                               metadata.channels,
                                               width,
                                               height,
                                               sun_elevation,
                                               altitude,
                                               air_density,
                                               aerosol_density,
                                               ozone_density);
  }
  else {
    SKY_single_scattering_precompute_texture(pixel_data,
                                             metadata.channels,
                                             width,
                                             height,
                                             sun_elevation,
                                             altitude,
                                             air_density,
                                             aerosol_density,
                                             ozone_density);
  }

  metadata.conform_pixels(pixels);
  return true;
}

string SkyLoader::name() const
{
  return "sky_multiple_scattering";
}

bool SkyLoader::equals(const ImageLoader &other) const
{
  const SkyLoader &other_sky = (const SkyLoader &)other;
  return multiple_scattering == other_sky.multiple_scattering &&
         sun_elevation == other_sky.sun_elevation && altitude == other_sky.altitude &&
         air_density == other_sky.air_density && aerosol_density == other_sky.aerosol_density &&
         ozone_density == other_sky.ozone_density;
}

CCL_NAMESPACE_END
