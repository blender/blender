/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/image_oiio.h"

#include "util/image.h"
#include "util/log.h"
#include "util/path.h"
#include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

OIIOImageLoader::OIIOImageLoader(const string &filepath) : filepath(filepath) {}

OIIOImageLoader::~OIIOImageLoader() = default;

bool OIIOImageLoader::load_metadata(ImageMetaData &metadata)
{
  return metadata.oiio_load_metadata(filepath);
}

bool OIIOImageLoader::load_pixels(const ImageMetaData &metadata, void *pixels)
{
  if (!metadata.oiio_load_pixels(filepath, pixels)) {
    return false;
  }

  metadata.conform_pixels(pixels);
  return true;
}

string OIIOImageLoader::name() const
{
  return path_filename(filepath.string());
}

ustring OIIOImageLoader::osl_filepath() const
{
  return filepath;
}

bool OIIOImageLoader::equals(const ImageLoader &other) const
{
  const OIIOImageLoader &other_loader = (const OIIOImageLoader &)other;
  return filepath == other_loader.filepath;
}

CCL_NAMESPACE_END
