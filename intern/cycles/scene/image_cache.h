/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "device/device.h"
#include "device/memory.h"

#include "util/hash.h"
#include "util/set.h"
#include "util/unique_ptr_vector.h"

#include <span>

CCL_NAMESPACE_BEGIN

class DeviceQueue;
class DeviceScene;
class ImageLoader;
class ImageMetaData;
struct ImageTileStats;

/* Eviction statistics tracking for ImageCache. All public methods are
 * thread-safe; an internal mutex protects the counters and the evicted mask
 * so callers can invoke these from parallel tile loaders without holding any
 * other lock. */
struct ImageCacheStats {
  /* Per-tile-descriptor flag set when a tile is evicted, to detect reloads. */
  vector<uint8_t> evicted_mask;

  /* Current state. */
  int64_t current_loaded = 0;
  int64_t current_tiled_bytes = 0;

  /* Cumulative counters. */
  int64_t total_loaded = 0;
  int64_t total_evicted = 0;
  int64_t total_reloaded = 0;

  int64_t peak_loaded = 0;
  int64_t peak_tiled_bytes = 0;

  /* Reset all counters and the evicted mask. */
  void reset();

  /* Ensure the evicted mask can hold at least `size` entries. */
  void resize(const size_t size);

  /* Clear the evicted mask for a tile descriptor range. */
  void clear_range(const size_t begin, const size_t end);

  /* Record successful tile load. */
  void load_tile(const size_t bit_index);

  /* Record a tile eviction. */
  void evict_tile(const size_t bit_index);

  /* Record tiled memory allocation and free. */
  void add_tiled_bytes(const size_t bytes);
  void remove_tiled_bytes(const size_t bytes);

 private:
  thread_mutex mutex_;
};

class ImageCache {
  struct DeviceImageKey {
    ImageDataType type;
    InterpolationType interpolation;
    int tile_size;

    bool operator==(const DeviceImageKey &other) const
    {
      return type == other.type && interpolation == other.interpolation &&
             tile_size == other.tile_size;
    }

    struct Hash {
      size_t operator()(const DeviceImageKey &key) const
      {
        return hash_uint3(key.type, key.interpolation, key.tile_size);
      }
    };
  };

  /* TODO: doesn't this mutex need to be shared with image manager? */
  thread_mutex device_mutex;

  /* A device image may either contain a full resolution image, or a set of tiles
   * from one or images packed together. */
  struct DeviceImage : public device_image {
    using device_image::device_image;

    uint64_t occupancy = 0;

    DeviceImageKey key() const;
  };

  static const int TILE_IMAGE_MAX_TILES = 64;
  static_assert(TILE_IMAGE_MAX_TILES <= sizeof(uint64_t) * 8);

  unique_ptr_vector<DeviceImage> images;
  unordered_map<DeviceImageKey, size_t, DeviceImageKey::Hash> images_first_free;

  unordered_set<device_image *> deferred_updates;
  unordered_set<device_image *> deferred_gpu_updates;

 public:
  ImageCache();
  ~ImageCache();

  /* Image management. */
  device_image *load_image_full(Device &device,
                                ImageLoader &loader,
                                const ImageMetaData &metadata,
                                const float texture_resolution,
                                KernelImageTexture &tex);

  void load_image_tiled(DeviceScene &dscene,
                        const ImageMetaData &metadata,
                        KernelImageTexture &tex);

  void free_image(DeviceScene &dscene, const KernelImageTexture &tex);

  /* Tile request processing. */
  void load_requested_tiles(Device &device,
                            DeviceScene &dscene,
                            const KernelImageTexture &tex,
                            ImageLoader &loader,
                            const ImageMetaData &metadata,
                            int miplevel_offset,
                            const uint8_t *access_state);

  void load_requested_tile(Device &device,
                           DeviceScene &dscene,
                           const KernelImageTexture &tex,
                           KernelTileDescriptor &r_tile_descriptor,
                           int miplevel,
                           int x,
                           int y,
                           ImageLoader &loader,
                           const ImageMetaData &metadata,
                           int miplevel_offset);

  /* Copy image cache data to device if modified. Either for all devices, or a single
   * device whose queue is provided. */
  void copy_to_device(DeviceScene &dscene);
  void copy_to_device(DeviceScene &dscene, DeviceQueue &queue);
  void copy_images_to_device(const bool for_cpu_cache_miss = false);

  /* Collect statistics about image cache. */
  void collect_statistics(DeviceScene &dscene,
                          const KernelImageTexture &tex,
                          const ImageMetaData &metadata,
                          ImageTileStats &tile_stats);

  const ImageCacheStats &get_stats() const
  {
    return stats;
  }

  void evict_unused(const Device &device,
                    DeviceScene &dscene,
                    std::span<KernelImageTexture> image_textures,
                    const uint8_t *access_state);

  size_t memory_size(DeviceScene &dscene) const;

  /* Free image cache device data. */
  void device_free(DeviceScene &dscene);

 private:
  /* Full image */
  device_image &alloc_full(Device &device,
                           const ImageDataType type,
                           const InterpolationType interpolation,
                           const ExtensionType extension,
                           const int64_t width,
                           const int64_t height,
                           uint &image_info_id);
  void free_full(const uint image_info_id);

  template<TypeDesc::BASETYPE FileFormat, typename StorageType>
  device_image *load_full(Device &device,
                          ImageLoader &loader,
                          const ImageMetaData &metadata,
                          const InterpolationType interpolation,
                          const ExtensionType extension,
                          const float texture_resolution,
                          uint &image_info_id);

  /* Tile descriptor management. */
  void free_tiled_image(DeviceScene &dscene, const KernelImageTexture &tex);

  /* Tiled image */
  device_image &alloc_tile(Device &device,
                           ImageDataType type,
                           InterpolationType interpolation,
                           const int tile_size_padded,
                           KernelTileDescriptor &r_tile_descriptor);
  void free_tile(const KernelTileDescriptor tile);

  KernelTileDescriptor load_tile(Device &device,
                                 DeviceScene &dscene,
                                 ImageLoader &loader,
                                 const ImageMetaData &metadata,
                                 InterpolationType interpolation,
                                 ExtensionType extension,
                                 int miplevel,
                                 int x,
                                 int y,
                                 const bool for_cpu_cache_miss,
                                 const size_t bit_index);

  ImageCacheStats stats;
};

CCL_NAMESPACE_END
