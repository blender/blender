/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "device/device.h"
#include "device/memory.h"

#include "util/hash.h"
#include "util/map.h"
#include "util/set.h"
#include "util/unique_ptr_vector.h"

CCL_NAMESPACE_BEGIN

class DeviceScene;
class ImageLoader;
class ImageMetaData;
struct ImageTileStats;

class ImageCache {
  thread_mutex device_mutex;

  /* Full images: one device image per full texture. Indexed by slot. */
  unique_ptr_vector<device_image> full_images;

  set<device_image *> updated_device_images;

 public:
  ImageCache();
  ~ImageCache();

  /* Image management. */
  device_image *load_image_full(Device &device,
                                ImageLoader &loader,
                                const ImageMetaData &metadata,
                                const InterpolationType interpolation,
                                const ExtensionType extension,
                                const int texture_limit,
                                const uint slot);

  void free_image(DeviceScene &dscene, const uint slot);

  void copy_to_device_if_modified(DeviceScene &dscene);

  size_t memory_size(DeviceScene &dscene) const;

  void device_free(DeviceScene &dscene);

 private:
  /* Full image */
  device_image &alloc_full(Device &device,
                           const ImageDataType type,
                           const InterpolationType interpolation,
                           const ExtensionType extension,
                           const int64_t width,
                           const int64_t height,
                           const uint slot);
  void free_full(const uint slot);

  template<TypeDesc::BASETYPE FileFormat, typename StorageType>
  device_image *load_full(Device &device,
                          ImageLoader &loader,
                          const ImageMetaData &metadata,
                          const InterpolationType interpolation,
                          const ExtensionType extension,
                          const int texture_limit,
                          const uint slot);
};

CCL_NAMESPACE_END
