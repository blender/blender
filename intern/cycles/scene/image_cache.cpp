/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/image_cache.h"

#include "scene/devicescene.h"
#include "scene/image_loader.h"
#include "scene/stats.h"

#include "util/image.h"
#include "util/image_impl.h"
#include "util/image_metadata.h"
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
                                     uint &image_info_id)
{
  thread_scoped_lock device_lock(device_mutex);

  image_info_id = full_images.size();

  unique_ptr<device_image> img = make_unique<device_image>(
      &device, "tex_image", image_info_id, type, interpolation, extension);

  img->alloc(width, height);

  full_images.push_back(std::move(img));

  device_image &mem = *full_images.back();
  updated_device_images.insert(&mem);

  return mem;
}

void ImageCache::free_full(const uint image_info_id)
{
  thread_scoped_lock device_lock(device_mutex);
  full_images.steal(image_info_id);
  full_images.trim();
}

void ImageCache::free_image(DeviceScene & /*dscene*/, const KernelImageTexture &tex)
{
  if (tex.image_info_id != KERNEL_IMAGE_NONE) {
    free_full(uint(tex.image_info_id));
  }
}

template<TypeDesc::BASETYPE FileFormat, typename StorageType>
device_image *ImageCache::load_full(Device &device,
                                    ImageLoader &loader,
                                    const ImageMetaData &metadata,
                                    const InterpolationType interpolation,
                                    const ExtensionType extension,
                                    const int texture_limit,
                                    uint &image_info_id)
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
    mem = &alloc_full(
        device, metadata.type, interpolation, extension, width, height, image_info_id);
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

    mem = &alloc_full(device,
                      metadata.type,
                      interpolation,
                      extension,
                      scaled_width,
                      scaled_height,
                      image_info_id);
    StorageType *texture_pixels = mem->data<StorageType>();
    std::copy_n(scaled_pixels.data(), scaled_pixels.size(), texture_pixels);
  }

  return mem;
}

device_image *ImageCache::load_image_full(Device &device,
                                          ImageLoader &loader,
                                          const ImageMetaData &metadata,
                                          const int texture_limit,
                                          KernelImageTexture &tex)
{
  const ImageDataType type = metadata.type;
  const InterpolationType interpolation = InterpolationType(tex.interpolation);
  const ExtensionType extension = ExtensionType(tex.extension);
  device_image *mem = nullptr;
  uint image_info_id = KERNEL_IMAGE_NONE;

  /* Create new texture. */
  switch (type) {
    case IMAGE_DATA_TYPE_FLOAT4:
      mem = load_full<TypeDesc::FLOAT, float>(
          device, loader, metadata, interpolation, extension, texture_limit, image_info_id);
      break;
    case IMAGE_DATA_TYPE_FLOAT:
      mem = load_full<TypeDesc::FLOAT, float>(
          device, loader, metadata, interpolation, extension, texture_limit, image_info_id);
      break;
    case IMAGE_DATA_TYPE_BYTE4:
      mem = load_full<TypeDesc::UINT8, uchar>(
          device, loader, metadata, interpolation, extension, texture_limit, image_info_id);
      break;
    case IMAGE_DATA_TYPE_BYTE:
      mem = load_full<TypeDesc::UINT8, uchar>(
          device, loader, metadata, interpolation, extension, texture_limit, image_info_id);
      break;
    case IMAGE_DATA_TYPE_HALF4:
      mem = load_full<TypeDesc::HALF, half>(
          device, loader, metadata, interpolation, extension, texture_limit, image_info_id);
      break;
    case IMAGE_DATA_TYPE_HALF:
      mem = load_full<TypeDesc::HALF, half>(
          device, loader, metadata, interpolation, extension, texture_limit, image_info_id);
      break;
    case IMAGE_DATA_TYPE_USHORT:
      mem = load_full<TypeDesc::USHORT, uint16_t>(
          device, loader, metadata, interpolation, extension, texture_limit, image_info_id);
      break;
    case IMAGE_DATA_TYPE_USHORT4:
      mem = load_full<TypeDesc::USHORT, uint16_t>(
          device, loader, metadata, interpolation, extension, texture_limit, image_info_id);
      break;
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT:
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT3:
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT4:
    case IMAGE_DATA_TYPE_NANOVDB_FPN:
    case IMAGE_DATA_TYPE_NANOVDB_FP16:
    case IMAGE_DATA_TYPE_NANOVDB_EMPTY: {
#ifdef WITH_NANOVDB
      mem = &alloc_full(
          device, type, interpolation, extension, metadata.nanovdb_byte_size, 0, image_info_id);

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

  tex.image_info_id = image_info_id;

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
