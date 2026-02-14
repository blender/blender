/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/image_cache.h"

#include "scene/devicescene.h"
#include "scene/image_loader.h"
#include "scene/stats.h"

#include "util/image.h"
#include "util/image_metadata.h"
#include "util/image_impl.h"
#include "util/log.h"
#include "util/types_image.h"

#include <algorithm>

CCL_NAMESPACE_BEGIN

/* ImageCache */

ImageCache::ImageCache() = default;

ImageCache::~ImageCache()
{
  assert(full_images.empty());
}

void ImageCache::device_free(DeviceScene & /*dscene*/)
{
  full_images.clear();
}

/* Full image management. */

device_image &ImageCache::alloc_full(Device &device,
                                     ImageDataType type,
                                     InterpolationType interpolation,
                                     ExtensionType extension,
                                     const int64_t width,
                                     const int64_t height,
                                     const uint slot)
{
  thread_scoped_lock device_lock(device_mutex);

  unique_ptr<device_image> img = make_unique<device_image>(
      &device, "tex_image", slot, type, interpolation, extension);

  img->alloc(width, height);

  full_images.resize(std::max(size_t(slot + 1), full_images.size()));
  full_images.replace(slot, std::move(img));

  device_image &mem = *full_images[slot];
  updated_device_images.insert(&mem);

  return mem;
}

void ImageCache::free_full(const uint slot)
{
  thread_scoped_lock device_lock(device_mutex);
  full_images.steal(slot);
  full_images.trim();
}

void ImageCache::free_image(DeviceScene & /*dscene*/, const uint slot)
{
  free_full(slot);
}

template<TypeDesc::BASETYPE FileFormat, typename StorageType>
device_image *ImageCache::load_full(Device &device,
                                    ImageLoader &loader,
                                    const ImageMetaData &metadata,
                                    const InterpolationType interpolation,
                                    const ExtensionType extension,
                                    const int texture_limit,
                                    const uint slot)
{
  /* Ignore empty images. */
  if (!(metadata.channels > 0)) {
    return nullptr;
  }

  /* Get metadata. */
  const int width = metadata.width;
  const int height = metadata.height;

  /* Read pixels. */
  vector<StorageType> pixels_storage;
  StorageType *pixels;
  const int64_t max_size = max(width, height);
  if (max_size == 0) {
    /* Don't bother with empty images. */
    return nullptr;
  }

  /* Allocate memory as needed, may be smaller to resize down. */
  device_image *mem;
  if (texture_limit > 0 && max_size > texture_limit) {
    pixels_storage.resize(int64_t(width) * height * 4);
    pixels = &pixels_storage[0];
    mem = nullptr;
  }
  else {
    mem = &alloc_full(device, metadata.type, interpolation, extension, width, height, slot);
    pixels = mem->data<StorageType>();
  }

  if (pixels == nullptr) {
    /* Could be that we've run out of memory. */
    return nullptr;
  }

  if (!loader.load_pixels(metadata, pixels)) {
    return nullptr;
  }

  /* Scale image down if needed. */
  if (!pixels_storage.empty()) {
    float scale_factor = 1.0f;
    while (max_size * scale_factor > texture_limit) {
      scale_factor *= 0.5f;
    }
    LOG_DEBUG << "Scaling image " << loader.name() << " by a factor of " << scale_factor << ".";
    vector<StorageType> scaled_pixels;
    int64_t scaled_width;
    int64_t scaled_height;

    util_image_resize_pixels(pixels_storage,
                             width,
                             height,
                             metadata.is_rgba() ? 4 : 1,
                             scale_factor,
                             &scaled_pixels,
                             &scaled_width,
                             &scaled_height);

    mem = &alloc_full(
        device, metadata.type, interpolation, extension, scaled_width, scaled_height, slot);
    StorageType *texture_pixels = mem->data<StorageType>();
    std::copy_n(scaled_pixels.data(), scaled_pixels.size(), texture_pixels);
  }

  return mem;
}

device_image *ImageCache::load_image_full(Device &device,
                                          ImageLoader &loader,
                                          const ImageMetaData &metadata,
                                          const InterpolationType interpolation,
                                          const ExtensionType extension,
                                          const int texture_limit,
                                          const uint slot)
{
  const ImageDataType type = metadata.type;
  device_image *mem = nullptr;

  /* Create new texture. */
  switch (type) {
    case IMAGE_DATA_TYPE_FLOAT4:
      mem = load_full<TypeDesc::FLOAT, float>(
          device, loader, metadata, interpolation, extension, texture_limit, slot);
      break;
    case IMAGE_DATA_TYPE_FLOAT:
      mem = load_full<TypeDesc::FLOAT, float>(
          device, loader, metadata, interpolation, extension, texture_limit, slot);
      break;
    case IMAGE_DATA_TYPE_BYTE4:
      mem = load_full<TypeDesc::UINT8, uchar>(
          device, loader, metadata, interpolation, extension, texture_limit, slot);
      break;
    case IMAGE_DATA_TYPE_BYTE:
      mem = load_full<TypeDesc::UINT8, uchar>(
          device, loader, metadata, interpolation, extension, texture_limit, slot);
      break;
    case IMAGE_DATA_TYPE_HALF4:
      mem = load_full<TypeDesc::HALF, half>(
          device, loader, metadata, interpolation, extension, texture_limit, slot);
      break;
    case IMAGE_DATA_TYPE_HALF:
      mem = load_full<TypeDesc::HALF, half>(
          device, loader, metadata, interpolation, extension, texture_limit, slot);
      break;
    case IMAGE_DATA_TYPE_USHORT:
      mem = load_full<TypeDesc::USHORT, uint16_t>(
          device, loader, metadata, interpolation, extension, texture_limit, slot);
      break;
    case IMAGE_DATA_TYPE_USHORT4:
      mem = load_full<TypeDesc::USHORT, uint16_t>(
          device, loader, metadata, interpolation, extension, texture_limit, slot);
      break;
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT:
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT3:
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT4:
    case IMAGE_DATA_TYPE_NANOVDB_FPN:
    case IMAGE_DATA_TYPE_NANOVDB_FP16:
    case IMAGE_DATA_TYPE_NANOVDB_EMPTY: {
#ifdef WITH_NANOVDB
      mem = &alloc_full(
          device, type, interpolation, extension, metadata.nanovdb_byte_size, 0, slot);

      uint8_t *pixels = mem->data<uint8_t>();
      if (pixels) {
        loader.load_pixels(metadata, pixels);
      }
#endif
      break;
    }
    case IMAGE_DATA_NUM_TYPES:
      break;
  }

  /* On failure to load, create a 1x1 pink image. */
  if (mem == nullptr && !is_nanovdb_type(type)) {
    mem = &alloc_full(device, type, interpolation, extension, 1, 1, slot);
    const int channels = (type == IMAGE_DATA_TYPE_FLOAT4 || type == IMAGE_DATA_TYPE_BYTE4 ||
                          type == IMAGE_DATA_TYPE_HALF4 || type == IMAGE_DATA_TYPE_USHORT4) ?
                             4 :
                             1;

    if (type == IMAGE_DATA_TYPE_FLOAT4 || type == IMAGE_DATA_TYPE_FLOAT) {
      float *pixels = mem->data<float>();
      pixels[0] = IMAGE_MISSING_RGBA.x;
      if (channels == 4) {
        pixels[1] = IMAGE_MISSING_RGBA.y;
        pixels[2] = IMAGE_MISSING_RGBA.z;
        pixels[3] = IMAGE_MISSING_RGBA.w;
      }
    }
    else if (type == IMAGE_DATA_TYPE_BYTE4 || type == IMAGE_DATA_TYPE_BYTE) {
      uchar *pixels = mem->data<uchar>();
      pixels[0] = uchar(IMAGE_MISSING_RGBA.x * 255);
      if (channels == 4) {
        pixels[1] = uchar(IMAGE_MISSING_RGBA.y * 255);
        pixels[2] = uchar(IMAGE_MISSING_RGBA.z * 255);
        pixels[3] = uchar(IMAGE_MISSING_RGBA.w * 255);
      }
    }
    else if (type == IMAGE_DATA_TYPE_HALF4 || type == IMAGE_DATA_TYPE_HALF) {
      half *pixels = mem->data<half>();
      pixels[0] = IMAGE_MISSING_RGBA.x;
      if (channels == 4) {
        pixels[1] = IMAGE_MISSING_RGBA.y;
        pixels[2] = IMAGE_MISSING_RGBA.z;
        pixels[3] = IMAGE_MISSING_RGBA.w;
      }
    }
    else if (type == IMAGE_DATA_TYPE_USHORT4 || type == IMAGE_DATA_TYPE_USHORT) {
      uint16_t *pixels = mem->data<uint16_t>();
      pixels[0] = uint16_t(IMAGE_MISSING_RGBA.x * 65535);
      if (channels == 4) {
        pixels[1] = uint16_t(IMAGE_MISSING_RGBA.y * 65535);
        pixels[2] = uint16_t(IMAGE_MISSING_RGBA.z * 65535);
        pixels[3] = uint16_t(IMAGE_MISSING_RGBA.w * 65535);
      }
    }
  }

  /* Set 3D transform info for volumes. */
  if (mem) {
    mem->info.use_transform_3d = metadata.use_transform_3d;
    mem->info.transform_3d = metadata.transform_3d;
  }

  return mem;
}

size_t ImageCache::memory_size(DeviceScene & /*dscene*/) const
{
  return 0;
}

/* Copy to device. */

void ImageCache::copy_to_device_if_modified(DeviceScene & /*dscene*/)
{
  thread_scoped_lock device_lock(device_mutex);

  for (device_image *mem : updated_device_images) {
    mem->copy_to_device();
  }

  updated_device_images.clear();
}

CCL_NAMESPACE_END
