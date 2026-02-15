/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "DNA_image_types.h"

#include "blender/CCL_api.h"

#include "util/image_maketx.h"
#include "util/image_metadata.h"
#include "util/types_image.h"

namespace blender {

bool CCL_has_texture_cache(const Image *image,
                           const char *filepath,
                           const char *texture_cache_directory)
{
  std::string tx_filepath;
  ccl::ImageMetaData tx_metadata;
  return ccl::resolve_tx(filepath,
                         texture_cache_directory,
                         ccl::ustring(image->colorspace_settings.name),
                         ccl::ImageAlphaType(image->alpha_mode),
                         ccl::IMAGE_FORMAT_PLAIN,
                         tx_filepath,
                         tx_metadata);
}

bool CCL_generate_texture_cache(const Image *image,
                                const char *filepath,
                                const char *texture_cache_directory)
{
  std::string tx_filepath;
  ccl::ImageMetaData tx_metadata;
  const bool is_valid = ccl::resolve_tx(filepath,
                                        texture_cache_directory,
                                        ccl::ustring(image->colorspace_settings.name),
                                        ccl::ImageAlphaType(image->alpha_mode),
                                        ccl::IMAGE_FORMAT_PLAIN,
                                        tx_filepath,
                                        tx_metadata);

  if (is_valid) {
    return true;
  }

  return make_tx(filepath,
                 tx_filepath,
                 ccl::ustring(image->colorspace_settings.name),
                 ccl::ImageAlphaType(image->alpha_mode),
                 ccl::IMAGE_FORMAT_PLAIN);
}

}  // namespace blender
