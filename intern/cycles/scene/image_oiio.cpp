/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "scene/image_oiio.h"

#include "util/image.h"
#include "util/log.h"
#include "util/path.h"

CCL_NAMESPACE_BEGIN

OIIOImageLoader::OIIOImageLoader(const string &filepath) : filepath(filepath) {}

OIIOImageLoader::~OIIOImageLoader() {}

bool OIIOImageLoader::load_metadata(const ImageDeviceFeatures & /*features*/,
                                    ImageMetaData &metadata)
{
  /* Perform preliminary checks, with meaningful logging. */
  if (!path_exists(filepath.string())) {
    VLOG_WARNING << "File '" << filepath.string() << "' does not exist.";
    return false;
  }
  if (path_is_directory(filepath.string())) {
    VLOG_WARNING << "File '" << filepath.string() << "' is a directory, can't use as image.";
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
  metadata.colorspace_file_hint = spec.get_string_attribute("oiio:ColorSpace");

  in->close();

  return true;
}

template<TypeDesc::BASETYPE FileFormat, typename StorageType>
static void oiio_load_pixels(const ImageMetaData &metadata,
                             const unique_ptr<ImageInput> &in,
                             const bool associate_alpha,
                             StorageType *pixels)
{
  const size_t width = metadata.width;
  const size_t height = metadata.height;
  const int depth = metadata.depth;
  const int components = metadata.channels;

  /* Read pixels through OpenImageIO. */
  StorageType *readpixels = pixels;
  vector<StorageType> tmppixels;
  if (components > 4) {
    tmppixels.resize(width * height * components);
    readpixels = &tmppixels[0];
  }

  if (depth <= 1) {
    size_t scanlinesize = width * components * sizeof(StorageType);
    in->read_image(0,
                   0,
                   0,
                   components,
                   FileFormat,
                   (uchar *)readpixels + (height - 1) * scanlinesize,
                   AutoStride,
                   -scanlinesize,
                   AutoStride);
  }
  else {
    in->read_image(0, 0, 0, components, FileFormat, (uchar *)readpixels);
  }

  if (components > 4) {
    size_t dimensions = width * height;
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

    const size_t num_pixels = width * height * depth;
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

  if (components == 4 && associate_alpha) {
    size_t dimensions = width * height;
    for (size_t i = dimensions - 1, pixel = 0; pixel < dimensions; pixel++, i--) {
      const StorageType alpha = pixels[i * 4 + 3];
      pixels[i * 4 + 0] = util_image_multiply_native(pixels[i * 4 + 0], alpha);
      pixels[i * 4 + 1] = util_image_multiply_native(pixels[i * 4 + 1], alpha);
      pixels[i * 4 + 2] = util_image_multiply_native(pixels[i * 4 + 2], alpha);
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

  /* Load without automatic OIIO alpha conversion, we do it ourselves. OIIO
   * will associate alpha in the 8bit buffer for PNGs, which leads to too
   * much precision loss when we load it as half float to do a color-space transform. */
  config.attribute("oiio:UnassociatedAlpha", 1);

  if (!in->open(filepath.string(), spec, config)) {
    return false;
  }

  bool do_associate_alpha = false;
  if (associate_alpha) {
    do_associate_alpha = spec.get_int_attribute("oiio:UnassociatedAlpha", 0);

    if (!do_associate_alpha && spec.alpha_channel != -1) {
      /* Workaround OIIO not detecting TGA file alpha the same as Blender (since #3019).
       * We want anything not marked as premultiplied alpha to get associated. */
      if (strcmp(in->format_name(), "targa") == 0) {
        do_associate_alpha = spec.get_int_attribute("targa:alpha_type", -1) != 4;
      }
      /* OIIO DDS reader never sets UnassociatedAlpha attribute. */
      if (strcmp(in->format_name(), "dds") == 0) {
        do_associate_alpha = true;
      }
      /* Workaround OIIO bug that sets oiio:UnassociatedAlpha on the last layer
       * but not composite image that we read. */
      if (strcmp(in->format_name(), "psd") == 0) {
        do_associate_alpha = true;
      }
    }
  }

  switch (metadata.type) {
    case IMAGE_DATA_TYPE_BYTE:
    case IMAGE_DATA_TYPE_BYTE4:
      oiio_load_pixels<TypeDesc::UINT8, uchar>(metadata, in, do_associate_alpha, (uchar *)pixels);
      break;
    case IMAGE_DATA_TYPE_USHORT:
    case IMAGE_DATA_TYPE_USHORT4:
      oiio_load_pixels<TypeDesc::USHORT, uint16_t>(
          metadata, in, do_associate_alpha, (uint16_t *)pixels);
      break;
    case IMAGE_DATA_TYPE_HALF:
    case IMAGE_DATA_TYPE_HALF4:
      oiio_load_pixels<TypeDesc::HALF, half>(metadata, in, do_associate_alpha, (half *)pixels);
      break;
    case IMAGE_DATA_TYPE_FLOAT:
    case IMAGE_DATA_TYPE_FLOAT4:
      oiio_load_pixels<TypeDesc::FLOAT, float>(metadata, in, do_associate_alpha, (float *)pixels);
      break;
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT:
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT3:
    case IMAGE_DATA_TYPE_NANOVDB_FPN:
    case IMAGE_DATA_TYPE_NANOVDB_FP16:
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
