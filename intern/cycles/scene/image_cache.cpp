/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/image_cache.h"

#include "device/device.h"
#include "device/queue.h"

#include "scene/devicescene.h"
#include "scene/image_loader.h"
#include "scene/stats.h"

#include "util/atomic.h"
#include "util/debug.h"
#include "util/image.h"
#include "util/image_impl.h"
#include "util/image_metadata.h"
#include "util/log.h"
#include "util/simd.h"

#include <OpenImageIO/thread.h>

#include <algorithm>
#include <span>

CCL_NAMESPACE_BEGIN

/* ImageCache::DeviceImage */

ImageCache::DeviceImageKey ImageCache::DeviceImage::key() const
{
  return {.type = ImageDataType(info.data_type),
          .interpolation = InterpolationType(info.interpolation),
          .tile_size = int(info.height)};
}

/* ImageCache */

ImageCache::ImageCache() = default;

ImageCache::~ImageCache()
{
  assert(images.empty());
}

void ImageCache::device_free(DeviceScene &dscene)
{
  images.clear();
  images_first_free.clear();
  dscene.image_texture_tile_descriptors.free();
  dscene.image_texture_tile_request_bits.free();
  dscene.image_texture_tile_used_bits.free();
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

  image_info_id = images.size();

  unique_ptr<DeviceImage> img = make_unique<DeviceImage>(
      &device, "full_image", image_info_id, type, interpolation, extension);

  img->occupancy = ~uint64_t(0);
  img->alloc(width, height);

  images.push_back(std::move(img));

  device_image &mem = *images.back();
  deferred_updates.insert(&mem);

  return mem;
}

void ImageCache::free_full(const uint image_info_id)
{
  thread_scoped_lock device_lock(device_mutex);
  deferred_updates.erase(images[image_info_id]);
  deferred_gpu_updates.erase(images[image_info_id]);
  images.steal(image_info_id);
}

void ImageCache::free_image(DeviceScene &dscene, const KernelImageTexture &tex)
{
  if (tex.tile_descriptor_offset != KERNEL_TILE_LOAD_NONE) {
    free_tiled_image(dscene, tex);
  }
  else if (tex.image_info_id != KERNEL_IMAGE_NONE) {
    free_full(tex.image_info_id);
  }
}

void ImageCache::free_tiled_image(DeviceScene &dscene, const KernelImageTexture &tex)
{
  KernelTileDescriptor *descriptors = dscene.image_texture_tile_descriptors.data() +
                                      tex.tile_descriptor_offset + tex.tile_levels;

  for (int i = 0; i < tex.tile_num; i++) {
    if (kernel_tile_descriptor_loaded(descriptors[i])) {
      free_tile(descriptors[i]);
    }
  }
}
template<TypeDesc::BASETYPE FileFormat, typename StorageType>
device_image *ImageCache::load_full(Device &device,
                                    ImageLoader &loader,
                                    const ImageMetaData &metadata,
                                    const InterpolationType interpolation,
                                    const ExtensionType extension,
                                    const float texture_resolution,
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

  /* Compute scale factor rounded down to nearest power of 2. */
  float scale_factor = 1.0f;
  if (texture_resolution < 1.0f) {
    scale_factor = powf(2.0f, floorf(log2f(texture_resolution)));
  }

  /* Allocate memory as needed, may be smaller to resize down. */
  device_image *mem;
  if (scale_factor < 1.0f) {
    pixels_storage.resize(int64_t(width) * height * 4);
    pixels = &pixels_storage[0];
    mem = nullptr;
  }
  else {
    mem = &alloc_full(
        device, metadata.type, interpolation, extension, width, height, image_info_id);
    pixels = mem->data<StorageType>();
  }

  if (pixels == nullptr || !loader.load_pixels(metadata, pixels)) {
    /* Out of memory or failed to load image. */
    if (mem) {
      free_full(image_info_id);
      image_info_id = KERNEL_IMAGE_NONE;
    }
    return nullptr;
  }

  /* Scale image down if needed. */
  if (!pixels_storage.empty()) {
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
                                          const float texture_resolution,
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
          device, loader, metadata, interpolation, extension, texture_resolution, image_info_id);
      break;
    case IMAGE_DATA_TYPE_FLOAT:
      mem = load_full<TypeDesc::FLOAT, float>(
          device, loader, metadata, interpolation, extension, texture_resolution, image_info_id);
      break;
    case IMAGE_DATA_TYPE_BYTE4:
      mem = load_full<TypeDesc::UINT8, uchar>(
          device, loader, metadata, interpolation, extension, texture_resolution, image_info_id);
      break;
    case IMAGE_DATA_TYPE_BYTE:
      mem = load_full<TypeDesc::UINT8, uchar>(
          device, loader, metadata, interpolation, extension, texture_resolution, image_info_id);
      break;
    case IMAGE_DATA_TYPE_HALF4:
      mem = load_full<TypeDesc::HALF, half>(
          device, loader, metadata, interpolation, extension, texture_resolution, image_info_id);
      break;
    case IMAGE_DATA_TYPE_HALF:
      mem = load_full<TypeDesc::HALF, half>(
          device, loader, metadata, interpolation, extension, texture_resolution, image_info_id);
      break;
    case IMAGE_DATA_TYPE_USHORT:
      mem = load_full<TypeDesc::USHORT, uint16_t>(
          device, loader, metadata, interpolation, extension, texture_resolution, image_info_id);
      break;
    case IMAGE_DATA_TYPE_USHORT4:
      mem = load_full<TypeDesc::USHORT, uint16_t>(
          device, loader, metadata, interpolation, extension, texture_resolution, image_info_id);
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

/* Tile image pool management. */

device_image &ImageCache::alloc_tile(Device &device,
                                     ImageDataType type,
                                     InterpolationType interpolation,
                                     const int tile_size_padded,
                                     const bool for_cpu_cache_miss,
                                     KernelTileDescriptor &r_tile_descriptor)
{
  thread_scoped_lock device_lock(device_mutex);

  DeviceImage *img = nullptr;
  int tile_offset = -1;

  /* Find image with free space by iterating pooled images. */
  const DeviceImageKey key = {
      .type = type, .interpolation = interpolation, .tile_size = tile_size_padded};

  size_t first_free = 0;
  auto it = images_first_free.find(key);
  if (it != images_first_free.end()) {
    first_free = it->second;
  }

  for (size_t i = first_free; i < images.size(); i++) {
    DeviceImage *img_candidate = images[i];
    if (img_candidate && img_candidate->occupancy != ~uint64_t(0) && img_candidate->key() == key) {
      img = img_candidate;

      /* Find unoccupied space in image. */
      tile_offset = bitscan(~img->occupancy);
      break;
    }
  }

  const bool alloc_image = img == nullptr;
  if (alloc_image) {
    /* Allocate new image. */
    uint image_info_id;
    for (image_info_id = 0; image_info_id < images.size(); image_info_id++) {
      if (!images[image_info_id]) {
        break;
      }
    }
    if (image_info_id == images.size()) {
      images.resize(images.size() + 1);
    }

    /* Extension doesn't matter as we do it through padding. */
    unique_ptr<DeviceImage> new_img = make_unique<DeviceImage>(
        &device, "tile_image", image_info_id, type, interpolation, EXTENSION_EXTEND);
    img = new_img.get();
    images.replace(image_info_id, std::move(new_img));

    img->alloc(tile_size_padded * TILE_IMAGE_MAX_TILES, tile_size_padded);
    tile_offset = 0;
  }

  if (alloc_image && device.has_unified_memory()) {
    /* If we allocated a new image and one of the devices is CPU or Metal
     * that uses unified memory, we need to allocate the image immediately
     * as the tile descriptor will be updated and rendering kernels can start
     * using the new image immediately. */
    img->copy_to_device();
    deferred_updates.erase(img);
  }
  else if (for_cpu_cache_miss) {
    if (device.info.type == DEVICE_MULTI) {
      /* For CPU cache miss we don't need to update anything for CPU rendering but
       * other GPUs will need an update the next time they load requested tiles. */
      deferred_gpu_updates.insert(img);
    }
  }
  else {
    /* For GPU cache miss, we defer to copy all tiles packed in ths same image together. */
    deferred_updates.insert(img);
  }

  /* Mark tile as occupied and compute descriptor. */
  img->occupancy |= (uint64_t(1) << tile_offset);

  /* Maintain images_first_free index for this key. */
  if (img->occupancy == ~uint64_t(0)) {
    size_t new_first_free = img->image_info_id + 1;
    while (new_first_free < images.size()) {
      DeviceImage *next = images[new_first_free];
      if (next && next->occupancy != ~uint64_t(0) && next->key() == key) {
        break;
      }
      new_first_free++;
    }
    images_first_free[key] = new_first_free;
  }

  r_tile_descriptor = kernel_tile_descriptor_encode(img->image_info_id, tile_offset);

  return *img;
}

void ImageCache::free_tile(const KernelTileDescriptor tile)
{
  thread_scoped_lock device_lock(device_mutex);

  const uint image_info_id = kernel_tile_descriptor_image_info_id(tile);
  const uint tile_offset = kernel_tile_descriptor_offset(tile);

  /* Look up pooled image by image_info_id. */
  DeviceImage *img = images[image_info_id];
  assert(img && img->image_info_id == image_info_id);

  img->occupancy &= ~(uint64_t(1) << tile_offset);

  /* Reconstruct key to update first_free map. */
  const DeviceImageKey key = img->key();

  images_first_free[key] = std::min(size_t(image_info_id), images_first_free[key]);

  if (img->occupancy == 0) {
    /* All tiles free, remove the device image entirely. */
    deferred_updates.erase(images[image_info_id]);
    deferred_gpu_updates.erase(images[image_info_id]);
    images.replace(image_info_id, nullptr);

    if (image_info_id == images_first_free[key]) {
      /* Search for next free one. */
      size_t new_first_free = image_info_id + 1;
      while (new_first_free < images.size()) {
        DeviceImage *next = images[new_first_free];
        if (next && next->occupancy != ~uint64_t(0) && next->key() == key) {
          break;
        }
        new_first_free++;
      }
      images_first_free[key] = new_first_free;
    }
  }
}
/* Tile descriptor management. */

void ImageCache::load_image_tiled(DeviceScene &dscene,
                                  const ImageMetaData &metadata,
                                  KernelImageTexture &tex)
{
  assert(is_power_of_two(metadata.tile_size));

  tex.image_info_id = KERNEL_IMAGE_NONE;
  tex.tile_size_shift = __bsr(metadata.tile_size);

  const int tile_size = metadata.tile_size;
  const InterpolationType interpolation = InterpolationType(tex.interpolation);
  const int max_miplevels = interpolation != INTERPOLATION_CLOSEST ? 1 : INT_MAX;

  vector<KernelTileDescriptor> levels;
  int num_tiles = 0;

  for (int miplevel = 0; max_miplevels; miplevel++) {
    const int mip_width = metadata.width >> miplevel;
    const int mip_height = metadata.height >> miplevel;

    levels.push_back(num_tiles);

    num_tiles += divide_up(mip_width, tile_size) * divide_up(mip_height, tile_size);

    if (mip_width <= tile_size && mip_height <= tile_size) {
      break;
    }
  }

  {
    /* TODO: Make this more efficient with geometric growth or other methods. */
    const thread_scoped_lock device_lock(device_mutex);

    device_vector<KernelTileDescriptor> &tile_descriptors = dscene.image_texture_tile_descriptors;
    device_vector<uint> &tile_request_bits = dscene.image_texture_tile_request_bits;

    const int tile_descriptor_offset = tile_descriptors.size();
    tile_descriptors.resize(tile_descriptor_offset + levels.size() + num_tiles);

    /* Resize request bitmap to match tile descriptors (1 bit per tile, stored in uint32 words). */
    const size_t num_bits = tile_descriptors.size();
    const size_t num_words = divide_up(num_bits, 32u);
    const size_t old_num_words = tile_request_bits.size();
    if (num_words > old_num_words) {
      tile_request_bits.resize(num_words);
      std::fill_n(tile_request_bits.data() + old_num_words, num_words - old_num_words, 0u);
    }

    device_vector<uint> &tile_used_bits = dscene.image_texture_tile_used_bits;
    const size_t old_used_words = tile_used_bits.size();
    if (num_words > old_used_words) {
      tile_used_bits.resize(num_words);
      std::fill_n(tile_used_bits.data() + old_used_words, num_words - old_used_words, 0u);
    }

    KernelTileDescriptor *descr_data = tile_descriptors.data() + tile_descriptor_offset;

    for (int i = 0; i < levels.size(); i++) {
      descr_data[i] = levels.size() + levels[i];
    }
    std::fill_n(descr_data + levels.size(), num_tiles, KERNEL_TILE_LOAD_NONE);

    tex.tile_descriptor_offset = tile_descriptor_offset;
    tex.tile_levels = levels.size();
    tex.tile_num = num_tiles;
  }
}

/* Tile request processing. */

KernelTileDescriptor ImageCache::load_tile(Device &device,
                                           DeviceScene &dscene,
                                           ImageLoader &loader,
                                           const ImageMetaData &metadata,
                                           const InterpolationType interpolation,
                                           const ExtensionType extension,
                                           const int miplevel,
                                           const int x,
                                           const int y,
                                           const bool for_cpu_cache_miss)
{
  const int width = metadata.width >> miplevel;
  const int height = metadata.height >> miplevel;
  const int tile_size = metadata.tile_size;
  const size_t w = min(size_t(width - x), size_t(tile_size));
  const size_t h = min(size_t(height - y), size_t(tile_size));
  const size_t tile_size_padded = tile_size + KERNEL_IMAGE_TEX_PADDING * 2;

  KernelTileDescriptor tile_descriptor;

  device_image &mem = alloc_tile(
      device, metadata.type, interpolation, tile_size_padded, for_cpu_cache_miss, tile_descriptor);

  const size_t pixel_bytes = mem.data_elements * datatype_size(mem.data_type);
  const size_t x_stride = pixel_bytes;
  const size_t y_stride = mem.data_width * pixel_bytes;
  const size_t x_offset = kernel_tile_descriptor_offset(tile_descriptor) * tile_size_padded *
                          pixel_bytes;

  uint8_t *pixels = mem.data<uint8_t>() + x_offset;

  const bool ok = loader.load_pixels_tile(metadata,
                                          miplevel,
                                          x,
                                          y,
                                          w,
                                          h,
                                          x_stride,
                                          y_stride,
                                          KERNEL_IMAGE_TEX_PADDING,
                                          extension,
                                          pixels);

  dscene.image_texture_tile_descriptors.tag_modified();

  if (ok) {
    LOG_TRACE << "Load image tile: " << loader.name() << ", mip level " << miplevel << " (" << x
              << " " << y << ")";
  }
  else {
    LOG_WARNING << "Failed to load image tile: " << loader.name() << ", mip level " << miplevel
                << " (" << x << " " << y << ")";
  }

  return (ok) ? tile_descriptor : KERNEL_TILE_LOAD_FAILED;
}

void ImageCache::load_requested_tiles(Device &device,
                                      DeviceScene &dscene,
                                      const KernelImageTexture &tex,
                                      ImageLoader &loader,
                                      const ImageMetaData &metadata,
                                      const uint *request_bits)
{
  const int tile_size = metadata.tile_size;
  const InterpolationType interpolation = InterpolationType(tex.interpolation);
  const ExtensionType extension = ExtensionType(tex.extension);
  const size_t base_offset = tex.tile_descriptor_offset + tex.tile_levels;

  KernelTileDescriptor *descriptors = dscene.image_texture_tile_descriptors.data() + base_offset;
  const KernelTileDescriptor *levels = dscene.image_texture_tile_descriptors.data() +
                                       tex.tile_descriptor_offset;

  /* Scan bitmap for this image's tiles. */
  const size_t start_bit = base_offset;
  const size_t end_bit = base_offset + tex.tile_num;
  const size_t start_word = start_bit >> 5;
  const size_t end_word = (end_bit + 31) >> 5;

  for (size_t w = start_word; w < end_word; w++) {
    uint word = request_bits[w];
    if (word == 0) {
      continue;
    }

    while (word) {
      const uint bit_in_word = bitscan(word);
      const size_t global_bit = (w << 5) + bit_in_word;
      word &= word - 1; /* Clear lowest set bit. */

      /* Check if bit is within this image's range. */
      if (global_bit < start_bit || global_bit >= end_bit) {
        continue;
      }

      const size_t tile_idx = global_bit - base_offset;

      /* Skip if tile is already loaded or failed. */
      const KernelTileDescriptor existing = descriptors[tile_idx];
      if (kernel_tile_descriptor_loaded(existing) || existing == KERNEL_TILE_LOAD_FAILED) {
        continue;
      }

      /* Atomically claim this tile slot. If another thread or GPU callback wins the race,
       * skip and let the winner load it. We don't require KERNEL_TILE_LOAD_REQUEST because
       * the request bit might be set without it even if that race condition is unlikely. */
      const KernelTileDescriptor old = atomic_cas_uint32(
          &descriptors[tile_idx], KERNEL_TILE_LOAD_NONE, KERNEL_TILE_LOAD_REQUEST);
      if (old != KERNEL_TILE_LOAD_NONE && old != KERNEL_TILE_LOAD_REQUEST) {
        continue;
      }

      /* Find miplevel for this tile index. The stored level values are offsets from
       * tile_descriptor_offset, so subtract tile_levels to get the tile index. */
      int miplevel = 0;
      size_t level_start = 0;
      for (int m = 0; m < tex.tile_levels; m++) {
        const size_t level_offset = levels[m] - tex.tile_levels;
        if (tile_idx < level_offset) {
          break;
        }
        level_start = level_offset;
        miplevel = m;
      }

      /* Compute tile pixel coordinates within miplevel. */
      const size_t idx_in_level = tile_idx - level_start;
      const int mip_width = metadata.width >> miplevel;
      const size_t tiles_x = divide_up(mip_width, tile_size);
      const size_t tile_y = idx_in_level / tiles_x;
      const size_t tile_x = idx_in_level % tiles_x;
      const size_t x = tile_x * tile_size;
      const size_t y = tile_y * tile_size;

      KernelTileDescriptor result = load_tile(
          device, dscene, loader, metadata, interpolation, extension, miplevel, x, y, false);
      descriptors[tile_idx] = result;
    }
  }
}

void ImageCache::load_requested_tile(Device &device,
                                     DeviceScene &dscene,
                                     const KernelImageTexture &tex,
                                     KernelTileDescriptor &r_tile_descriptor,
                                     int miplevel,
                                     int x,
                                     int y,
                                     ImageLoader &loader,
                                     const ImageMetaData &metadata)
{
  /* This is called by the CPU kernel to immediately load a tile. */

  /* If we can atomically set KERNEL_TILE_LOAD_REQUEST, this thread is responsible
   * for loading the tile. */
  KernelTileDescriptor tile_descriptor_old = r_tile_descriptor;
  if (tile_descriptor_old != KERNEL_TILE_LOAD_REQUEST &&
      tile_descriptor_old ==
          atomic_cas_uint32(&r_tile_descriptor, tile_descriptor_old, KERNEL_TILE_LOAD_REQUEST))
  {
    const InterpolationType interpolation = InterpolationType(tex.interpolation);
    const ExtensionType extension = ExtensionType(tex.extension);
    KernelTileDescriptor tile_descriptor_new = load_tile(
        device, dscene, loader, metadata, interpolation, extension, miplevel, x, y, true);
    r_tile_descriptor = tile_descriptor_new;
    return;
  }

  /* Wait for other thread to load the tile. */
  OIIO::atomic_backoff backoff;
  while (r_tile_descriptor == KERNEL_TILE_LOAD_REQUEST) {
    backoff();
  }
}

/* Statistics. */

void ImageCache::collect_statistics(DeviceScene &dscene,
                                    const KernelImageTexture &tex,
                                    const ImageMetaData &metadata,
                                    ImageTileStats &tile_stats)
{
  if (tex.tile_descriptor_offset == KERNEL_TILE_LOAD_NONE) {
    return;
  }

  const KernelTileDescriptor *tile_descriptors = dscene.image_texture_tile_descriptors.data() +
                                                 tex.tile_descriptor_offset + tex.tile_levels;
  const KernelTileDescriptor *levels = dscene.image_texture_tile_descriptors.data() +
                                       tex.tile_descriptor_offset;

  /* Compute per-mip-level statistics. */
  const int tile_size = metadata.tile_size;
  const size_t pixel_bytes = metadata.pixel_memory_size();
  const size_t tile_bytes = tile_size * tile_size * pixel_bytes;

  for (int miplevel = 0; miplevel < tex.tile_levels; miplevel++) {
    const int width = metadata.width >> miplevel;
    const int height = metadata.height >> miplevel;
    const int tiles_x = divide_up(width, tile_size);
    const int tiles_y = divide_up(height, tile_size);
    const int tiles_total = tiles_x * tiles_y;

    /* Count loaded tiles for this mip level. */
    const size_t level_start = levels[miplevel] - tex.tile_levels;
    int tiles_loaded = 0;
    for (int i = 0; i < tiles_total; i++) {
      if (kernel_tile_descriptor_loaded(tile_descriptors[level_start + i])) {
        tiles_loaded++;
      }
    }

    ImageMipLevelStats mip_stats;
    mip_stats.width = width;
    mip_stats.height = height;
    mip_stats.tiles_total = tiles_total;
    mip_stats.tiles_loaded = tiles_loaded;
    tile_stats.mip_levels.push_back(mip_stats);

    tile_stats.size += tiles_loaded * tile_bytes;
  }
}

void ImageCache::evict_unused_tiles(DeviceScene &dscene,
                                    std::span<KernelImageTexture> image_textures,
                                    const uint *used_bits)
{
  device_vector<KernelTileDescriptor> &tile_descriptors = dscene.image_texture_tile_descriptors;

  int num_evicted = 0;

  const size_t preserve_budget = size_t(DebugFlags().texture_cache.preserve_unused) * 1024 * 1024;

  /* Collect all unused tiles. */
  struct UnusedTile {
    size_t global_idx;
    uint tile_descriptor_offset;
    size_t tile_bytes;
    bool is_host_mapped;
  };
  vector<UnusedTile> unused_tiles;

  int num_remaining = 0;

  for (size_t img_idx = 0; img_idx < image_textures.size(); img_idx++) {
    const KernelImageTexture &tex = image_textures[img_idx];
    if (tex.tile_descriptor_offset == KERNEL_TILE_LOAD_NONE) {
      continue;
    }

    const size_t base_offset = tex.tile_descriptor_offset + tex.tile_levels;
    for (int i = 0; i < tex.tile_num; i++) {
      const size_t global_idx = base_offset + i;
      KernelTileDescriptor &descriptor = tile_descriptors[global_idx];

      if (!kernel_tile_descriptor_loaded(descriptor)) {
        continue;
      }

      const uint word = global_idx >> 5;
      const uint bit = 1u << (global_idx & 31);
      if (!(used_bits[word] & bit)) {
        /* Look up the DeviceImage to determine host-mapped status and tile size. */
        const uint image_info_id = kernel_tile_descriptor_image_info_id(descriptor);
        DeviceImage *img = images[image_info_id];

        bool is_host_mapped = false;
        size_t tile_bytes = 0;
        if (img) {
          /* TODO: does this work for CPU only case? */
          is_host_mapped = (img->shared_pointer != nullptr);
          const size_t pixel_bytes = img->data_elements * datatype_size(img->data_type);
          tile_bytes = img->data_height * img->data_height * pixel_bytes;
        }

        unused_tiles.push_back(
            {global_idx, tex.tile_descriptor_offset, tile_bytes, is_host_mapped});
      }
      else {
        num_remaining++;
      }
    }
  }

  if (unused_tiles.empty()) {
    return;
  }

  /* Sort: host-mapped tiles first (always evict these), then device-resident. */
  std::sort(
      unused_tiles.begin(), unused_tiles.end(), [](const UnusedTile &a, const UnusedTile &b) {
        return a.is_host_mapped > b.is_host_mapped;
      });

  /* Compute total bytes of unused device-resident tiles. */
  size_t preserved_bytes = 0;
  for (const UnusedTile &tile : unused_tiles) {
    if (!tile.is_host_mapped) {
      preserved_bytes += tile.tile_bytes;
    }
  }

  /* Evict tiles: host-mapped unconditionally, device-resident until within budget. */
  int num_preserved = 0;

  for (const UnusedTile &tile : unused_tiles) {
    bool should_evict;
    if (tile.is_host_mapped) {
      /* Host-mapped tiles are slower, always evict. */
      should_evict = true;
    }
    else if (preserved_bytes <= preserve_budget) {
      /* Within budget, preserve remaining device-resident tiles. */
      should_evict = false;
    }
    else {
      /* Over budget, evict to bring preserved bytes down. */
      should_evict = true;
    }

    if (should_evict) {
      KernelTileDescriptor &descriptor = tile_descriptors[tile.global_idx];
      free_tile(descriptor);
      descriptor = KERNEL_TILE_LOAD_NONE;
      num_evicted++;

      if (!tile.is_host_mapped) {
        preserved_bytes -= tile.tile_bytes;
      }
    }
    else {
      num_preserved++;
    }
  }

  if (num_evicted > 0) {
    dscene.image_texture_tile_descriptors.tag_modified();

    /* TODO: This is broken for GPU rendering. Also unclear how useful it is
     * versus the cost, maybe only at the end of rendering and not for every tile? */
#if 0
    compact(dscene, image_textures);
#endif

    LOG_DEBUG << "Texture cache tile eviction: " << num_evicted << " evicted, " << num_remaining
              << " used, " << num_preserved << " preserved (" << preserved_bytes / (1024 * 1024)
              << " MB).";
  }
}

/* Compaction. */

void ImageCache::compact(DeviceScene &dscene, std::span<KernelImageTexture> image_textures)
{
  thread_scoped_lock device_lock(device_mutex);

  /* Phase A: Consolidate partially-occupied device images with matching keys. */
  compact_image_tiles(dscene, image_textures);

  /* Phase B: Compact tile descriptor array. */
  compact_tile_descriptors(dscene, image_textures);
}

void ImageCache::compact_image_tiles(DeviceScene &dscene,
                                     std::span<KernelImageTexture> image_textures)
{
  /* Group partially-occupied images by key. */
  unordered_map<DeviceImageKey, vector<size_t>, DeviceImageKey::Hash> groups;
  for (size_t i = 0; i < images.size(); i++) {
    DeviceImage *img = images[i];
    if (img && img->occupancy != 0 && img->occupancy != ~uint64_t(0)) {
      groups[img->key()].push_back(i);
    }
  }

  /* For each group with 2+ images, consolidate. */
  unordered_map<KernelTileDescriptor, KernelTileDescriptor> remap;

  for (auto &[key, indices] : groups) {
    if (indices.size() < 2) {
      continue;
    }

    /* Use two-pointer approach: dst fills gaps, src donates tiles. */
    size_t dst_idx = 0;
    size_t src_idx = indices.size() - 1;

    while (dst_idx < src_idx) {
      DeviceImage *dst = images[indices[dst_idx]];
      DeviceImage *src = images[indices[src_idx]];

      if (dst->occupancy == ~uint64_t(0)) {
        dst_idx++;
        continue;
      }
      if (src->occupancy == 0) {
        src_idx--;
        continue;
      }

      /* Move one tile from src to dst. */
      int src_slot = bitscan(src->occupancy);  /* first occupied in src */
      int dst_slot = bitscan(~dst->occupancy); /* first free in dst */

      /* Copy pixel data. */
      const size_t tile_size_padded = dst->data_height; /* height = tile_size_padded */
      const size_t pixel_bytes = dst->data_elements * datatype_size(dst->data_type);
      const size_t col_bytes = tile_size_padded * pixel_bytes;
      const size_t row_bytes = dst->data_width * pixel_bytes;

      uint8_t *dst_data = dst->data<uint8_t>();
      uint8_t *src_data = src->data<uint8_t>();

      for (size_t row = 0; row < tile_size_padded; row++) {
        memcpy(dst_data + row * row_bytes + dst_slot * col_bytes,
               src_data + row * row_bytes + src_slot * col_bytes,
               col_bytes);
      }

      /* Record remap. */
      KernelTileDescriptor old_desc = kernel_tile_descriptor_encode(src->image_info_id, src_slot);
      KernelTileDescriptor new_desc = kernel_tile_descriptor_encode(dst->image_info_id, dst_slot);
      remap[old_desc] = new_desc;

      /* Update occupancy. */
      src->occupancy &= ~(uint64_t(1) << src_slot);
      dst->occupancy |= (uint64_t(1) << dst_slot);

      /* Mark for device update. */
      deferred_updates.insert(dst);
      deferred_updates.insert(src);

      /* If src is now empty, free it. */
      if (src->occupancy == 0) {
        deferred_updates.erase(src);
        deferred_gpu_updates.erase(src);
        images.replace(indices[src_idx], nullptr);
        src_idx--;
      }
    }
  }

  if (remap.empty()) {
    return;
  }

  /* Apply remap to all tile descriptors. We must only remap actual tile
   * descriptors, not the mip level offsets (headers) at the start of each
   * texture's descriptor range. */
  device_vector<KernelTileDescriptor> &tile_descriptors = dscene.image_texture_tile_descriptors;
  for (const KernelImageTexture &tex : image_textures) {
    if (tex.tile_descriptor_offset == KERNEL_TILE_LOAD_NONE) {
      continue;
    }

    /* Skip headers (offsets to levels), only remap tiles. */
    const size_t start = tex.tile_descriptor_offset + tex.tile_levels;
    const size_t end = start + tex.tile_num;

    for (size_t i = start; i < end; i++) {
      auto it = remap.find(tile_descriptors[i]);
      if (it != remap.end()) {
        tile_descriptors[i] = it->second;
      }
    }
  }
  tile_descriptors.tag_modified();

  /* Rebuild images_first_free after occupancy changes.
   * Note: we do NOT move or reindex entries in the images array. DeviceImages
   * stay at their original image_info_id slots. Empty images become null but
   * their slots remain. This avoids having to update image_info_id in tile
   * descriptors or KernelImageTexture entries for full images. */
  images_first_free.clear();
  for (size_t i = 0; i < images.size(); i++) {
    DeviceImage *img = images[i];
    if (img && img->occupancy != ~uint64_t(0)) {
      DeviceImageKey k = img->key();
      if (images_first_free.find(k) == images_first_free.end()) {
        images_first_free[k] = i;
      }
    }
  }
}

void ImageCache::compact_tile_descriptors(DeviceScene &dscene,
                                          std::span<KernelImageTexture> image_textures)
{
  device_vector<KernelTileDescriptor> &tile_descriptors = dscene.image_texture_tile_descriptors;

  if (tile_descriptors.size() == 0) {
    return;
  }

  /* Build sorted list of live descriptor ranges. */
  struct LiveRange {
    size_t old_offset;
    size_t length;
    size_t image_idx;
  };
  vector<LiveRange> live_ranges;

  for (size_t i = 0; i < image_textures.size(); i++) {
    KernelImageTexture &tex = image_textures[i];
    if (tex.tile_descriptor_offset == KERNEL_TILE_LOAD_NONE) {
      continue;
    }
    live_ranges.push_back(
        {size_t(tex.tile_descriptor_offset), size_t(tex.tile_levels + tex.tile_num), i});
  }

  std::sort(live_ranges.begin(), live_ranges.end(), [](const LiveRange &a, const LiveRange &b) {
    return a.old_offset < b.old_offset;
  });

  /* Compact: shift ranges down using std::copy. */
  size_t write_pos = 0;
  bool compacted = false;
  KernelTileDescriptor *data = tile_descriptors.data();

  for (LiveRange &range : live_ranges) {
    if (range.old_offset != write_pos) {
      std::copy(data + range.old_offset, data + range.old_offset + range.length, data + write_pos);
      image_textures[range.image_idx].tile_descriptor_offset = write_pos;
      compacted = true;
    }
    write_pos += range.length;
  }

  if (!compacted) {
    return;
  }

  tile_descriptors.resize(write_pos);
  tile_descriptors.tag_modified();

  /* Tag image textures as modified since tile_descriptor_offset changed. */
  dscene.image_textures.tag_modified();

  /* Shrink bit arrays. */
  const size_t num_words = divide_up(write_pos, size_t(32));

  device_vector<uint> &request_bits = dscene.image_texture_tile_request_bits;
  if (request_bits.size() > num_words) {
    request_bits.resize(num_words);
  }

  device_vector<uint> &used_bits = dscene.image_texture_tile_used_bits;
  if (used_bits.size() > num_words) {
    used_bits.resize(num_words);
  }
}

size_t ImageCache::memory_size(DeviceScene &dscene) const
{
  return dscene.image_texture_tile_request_bits.memory_size() +
         dscene.image_texture_tile_used_bits.memory_size() +
         dscene.image_texture_tile_descriptors.memory_size();
}

/* Copy to device. */

void ImageCache::copy_to_device(DeviceScene &dscene)
{
  /* Copy all device memory managed by the image cache to all devices, for updates
   * outside of rendering. */
  copy_images_to_device();

  thread_scoped_lock device_lock(device_mutex);
  dscene.image_texture_tile_descriptors.copy_to_device_if_modified();
  dscene.image_texture_tile_request_bits.copy_to_device_if_modified();
  dscene.image_texture_tile_used_bits.copy_to_device_if_modified();

  dscene.image_texture_tile_descriptors.clear_modified();
  dscene.image_texture_tile_request_bits.clear_modified();
  dscene.image_texture_tile_used_bits.clear_modified();
}

void ImageCache::copy_to_device(DeviceScene &dscene, DeviceQueue &queue)
{
  /* Copy data for a single GPU device during rendering. Note this may run
   * concurrently for multiple GPU devices, or CPU and GPU devices.
   *
   * We run copy_device_images which will allocate image memory on all devices,
   * which is safe to do while kernels are executing because we only allocate
   * without freeing, and load_image_info() is delayed. */
  copy_images_to_device();

  /* Copy updated tile descriptors and zero request bits for this GPU device only. */
  queue.copy_to_device(dscene.image_texture_tile_descriptors);
  queue.zero_to_device(dscene.image_texture_tile_request_bits);

  /* Update image info only for this GPU device. */
  queue.load_image_info();
}

void ImageCache::copy_images_to_device(const bool for_cpu_cache_miss)
{
  /* For CPU cache miss we skip deferred updates that were only meant for the GPU,
   * to avoid repeated copies to the GPU. */
  thread_scoped_lock device_lock(device_mutex);
  if (!for_cpu_cache_miss) {
    deferred_updates.merge(deferred_gpu_updates);
    deferred_gpu_updates.clear();
  }
  for (device_image *mem : deferred_updates) {
    mem->copy_to_device();
  }
  deferred_updates.clear();
}

CCL_NAMESPACE_END
