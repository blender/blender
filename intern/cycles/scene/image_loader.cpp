/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/image_loader.h"

CCL_NAMESPACE_BEGIN

/* Image Loader */

ImageLoader::ImageLoader() = default;

ustring ImageLoader::osl_filepath() const
{
  return ustring();
}

int ImageLoader::get_tile_number() const
{
  return 0;
}

bool ImageLoader::equals(const ImageLoader *a, const ImageLoader *b)
{
  if (a == nullptr && b == nullptr) {
    return true;
  }
  return (a && b && typeid(*a) == typeid(*b) && a->equals(*b));
}

bool ImageLoader::is_vdb_loader() const
{
  return false;
}

CCL_NAMESPACE_END
