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

#include "scene/image_oiio.h"

#include "util/image.h"
#include "util/log.h"
#include "util/path.h"

CCL_NAMESPACE_BEGIN

OIIOImageLoader::OIIOImageLoader(const string &filepath) : filepath(filepath)
{
}

OIIOImageLoader::~OIIOImageLoader()
{
}

bool OIIOImageLoader::load_metadata(const ImageDeviceFeatures & /*features*/,
                                    ImageMetaData &metadata)
{
  /* Perform preliminary checks, with meaningful logging. */
  if (!path_exists(filepath.string())) {
    VLOG(1) << "File '" << filepath.string() << "' does not exist.";
    return false;
  }
  if (path_is_directory(filepath.string())) {
    VLOG(1) << "File '" << filepath.string() << "' is a directory, can't use as image.";
    return false;
  }

  unique_ptr<ImageInput> in(ImageInput::create(filepath.string()));

  if (!in) {
    return false;
  }

  ImageSpec spec;
  if (!in->open(filepath.string(), spec)) {
    return false;
  }

  metadata.width = spec.width;
  metadata.height = spec.height;
  metadata.depth = spec.depth;
  metadata.compress_as_srgb = false;

  /* Check the main format, and channel formats. */
  size_t channel_size = spec.format.basesize();

  bool is_float = false;
  bool is_half = false;

  if (spec.format.is_floating_point()) {
    is_float = true;
  }

  for (size_t channel = 0; channel < spec.channelformats.size(); channel++) {
    channel_size = max(channel_size, spec.channelformats[channel].basesize());
    if (spec.channelformats[channel].is_floating_point()) {
      is_float = true;
    }
  }

  /* check if it's half float */
  if (spec.format == TypeDesc::HALF) {
    is_half = true;
  }

  /* set type and channels */
  metadata.channels = spec.nchannels;

  if (is_half) {
    metadata.type = (metadata.channels > 1) ? IMAGE_DATA_TYPE_HALF4 : IMAGE_DATA_TYPE_HALF;
  }
  else if (is_float) {
    metadata.type = (metadata.channels > 1) ? IMAGE_DATA_TYPE_FLOAT4 : IMAGE_DATA_TYPE_FLOAT;
  }
  else if (spec.format == TypeDesc::USHORT) {
    metadata.type = (metadata.channels > 1) ? IMAGE_DATA_TYPE_USHORT4 : IMAGE_DATA_TYPE_USHORT;
  }
  else {
    metadata.type = (metadata.channels > 1) ? IMAGE_DATA_TYPE_BYTE4 : IMAGE_DATA_TYPE_BYTE;
  }

  metadata.colorspace_file_format = in->format_name();

  in->close();

  return true;
}

template<TypeDesc::BASETYPE FileFormat, typename StorageType>
static void oiio_load_pixels(const ImageMetaData &metadata,
                             const unique_ptr<ImageInput> &in,
                             StorageType *pixels)
{
  const int width = metadata.width;
  const int height = metadata.height;
  const int depth = metadata.depth;
  const int components = metadata.channels;

  /* Read pixels through OpenImageIO. */
  StorageType *readpixels = pixels;
  vector<StorageType> tmppixels;
  if (components > 4) {
    tmppixels.resize(((size_t)width) * height * components);
    readpixels = &tmppixels[0];
  }

  if (depth <= 1) {
    size_t scanlinesize = ((size_t)width) * components * sizeof(StorageType);
    in->read_image(FileFormat,
                   (uchar *)readpixels + (height - 1) * scanlinesize,
                   AutoStride,
                   -scanlinesize,
                   AutoStride);
  }
  else {
    in->read_image(FileFormat, (uchar *)readpixels);
  }

  if (components > 4) {
    size_t dimensions = ((size_t)width) * height;
    for (size_t i = dimensions - 1, pixel = 0; pixel < dimensions; pixel++, i--) {
      pixels[i * 4 + 3] = tmppixels[i * components + 3];
      pixels[i * 4 + 2] = tmppixels[i * components + 2];
      pixels[i * 4 + 1] = tmppixels[i * components + 1];
      pixels[i * 4 + 0] = tmppixels[i * components + 0];
    }
    tmppixels.clear();
  }

  /* CMYK to RGBA. */
  const bool cmyk = strcmp(in->format_name(), "jpeg") == 0 && components == 4;
  if (cmyk) {
    const StorageType one = util_image_cast_from_float<StorageType>(1.0f);

    const size_t num_pixels = ((size_t)width) * height * depth;
    for (size_t i = num_pixels - 1, pixel = 0; pixel < num_pixels; pixel++, i--) {
      float c = util_image_cast_to_float(pixels[i * 4 + 0]);
      float m = util_image_cast_to_float(pixels[i * 4 + 1]);
      float y = util_image_cast_to_float(pixels[i * 4 + 2]);
      float k = util_image_cast_to_float(pixels[i * 4 + 3]);
      pixels[i * 4 + 0] = util_image_cast_from_float<StorageType>((1.0f - c) * (1.0f - k));
      pixels[i * 4 + 1] = util_image_cast_from_float<StorageType>((1.0f - m) * (1.0f - k));
      pixels[i * 4 + 2] = util_image_cast_from_float<StorageType>((1.0f - y) * (1.0f - k));
      pixels[i * 4 + 3] = one;
    }
  }
}

bool OIIOImageLoader::load_pixels(const ImageMetaData &metadata,
                                  void *pixels,
                                  const size_t,
                                  const bool associate_alpha)
{
  unique_ptr<ImageInput> in = NULL;

  /* NOTE: Error logging is done in meta data acquisition. */
  if (!path_exists(filepath.string()) || path_is_directory(filepath.string())) {
    return false;
  }

  /* load image from file through OIIO */
  in = unique_ptr<ImageInput>(ImageInput::create(filepath.string()));
  if (!in) {
    return false;
  }

  ImageSpec spec = ImageSpec();
  ImageSpec config = ImageSpec();

  if (!associate_alpha) {
    config.attribute("oiio:UnassociatedAlpha", 1);
  }

  if (!in->open(filepath.string(), spec, config)) {
    return false;
  }

  switch (metadata.type) {
    case IMAGE_DATA_TYPE_BYTE:
    case IMAGE_DATA_TYPE_BYTE4:
      oiio_load_pixels<TypeDesc::UINT8, uchar>(metadata, in, (uchar *)pixels);
      break;
    case IMAGE_DATA_TYPE_USHORT:
    case IMAGE_DATA_TYPE_USHORT4:
      oiio_load_pixels<TypeDesc::USHORT, uint16_t>(metadata, in, (uint16_t *)pixels);
      break;
    case IMAGE_DATA_TYPE_HALF:
    case IMAGE_DATA_TYPE_HALF4:
      oiio_load_pixels<TypeDesc::HALF, half>(metadata, in, (half *)pixels);
      break;
    case IMAGE_DATA_TYPE_FLOAT:
    case IMAGE_DATA_TYPE_FLOAT4:
      oiio_load_pixels<TypeDesc::FLOAT, float>(metadata, in, (float *)pixels);
      break;
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT:
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT3:
    case IMAGE_DATA_NUM_TYPES:
      break;
  }

  in->close();
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
