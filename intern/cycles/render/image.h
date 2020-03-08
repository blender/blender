/*
 * Copyright 2011-2013 Blender Foundation
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

#ifndef __IMAGE_H__
#define __IMAGE_H__

#include "device/device.h"
#include "device/device_memory.h"

#include "render/colorspace.h"

#include "util/util_image.h"
#include "util/util_string.h"
#include "util/util_thread.h"
#include "util/util_unique_ptr.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class Device;
class ImageHandle;
class ImageKey;
class ImageMetaData;
class ImageManager;
class Progress;
class RenderStats;
class Scene;
class ColorSpaceProcessor;

/* Image MetaData
 *
 * Information about the image that is available before the image pxeisl are loaded. */
class ImageMetaData {
 public:
  /* Must be set by image file or builtin callback. */
  bool is_float, is_half;
  int channels;
  size_t width, height, depth;
  bool builtin_free_cache;

  /* Automatically set. */
  ImageDataType type;
  ustring colorspace;
  bool compress_as_srgb;

  ImageMetaData()
      : is_float(false),
        is_half(false),
        channels(0),
        width(0),
        height(0),
        depth(0),
        builtin_free_cache(false),
        type(IMAGE_DATA_NUM_TYPES),
        colorspace(u_colorspace_raw),
        compress_as_srgb(false)
  {
  }

  bool operator==(const ImageMetaData &other) const
  {
    return is_float == other.is_float && is_half == other.is_half && channels == other.channels &&
           width == other.width && height == other.height && depth == other.depth &&
           type == other.type && colorspace == other.colorspace &&
           compress_as_srgb == other.compress_as_srgb;
  }
};

/* Image Key
 *
 * Image description that uniquely identifies and images. When adding images
 * with the same key, they will be internally deduplicated. */
class ImageKey {
 public:
  string filename;
  void *builtin_data;
  bool animated;
  InterpolationType interpolation;
  ExtensionType extension;
  ImageAlphaType alpha_type;
  ustring colorspace;

  ImageKey()
      : builtin_data(NULL),
        animated(false),
        interpolation(INTERPOLATION_LINEAR),
        extension(EXTENSION_CLIP),
        alpha_type(IMAGE_ALPHA_AUTO),
        colorspace(u_colorspace_raw)
  {
  }

  bool operator==(const ImageKey &other) const
  {
    return (filename == other.filename && builtin_data == other.builtin_data &&
            animated == other.animated && interpolation == other.interpolation &&
            extension == other.extension && alpha_type == other.alpha_type &&
            colorspace == other.colorspace);
  }
};

/* Image Handle
 *
 * Access handle for image in the image manager. Multiple shader nodes may
 * share the same image, and this class handles reference counting for that. */
class ImageHandle {
 public:
  ImageHandle();
  ImageHandle(const ImageHandle &other);
  ImageHandle &operator=(const ImageHandle &other);
  ~ImageHandle();

  void clear();

  bool empty();
  int num_tiles();

  ImageMetaData metadata();
  int svm_slot(const int tile_index = 0);
  device_memory *image_memory(const int tile_index = 0);

 protected:
  vector<int> slots;
  ImageManager *manager;

  friend class ImageManager;
};

/* Image Manager
 *
 * Handles loading and storage of all images in the scene. This includes 2D
 * texture images and 3D volume images. */
class ImageManager {
 public:
  explicit ImageManager(const DeviceInfo &info);
  ~ImageManager();

  ImageHandle add_image(const ImageKey &key, float frame);
  ImageHandle add_image(const ImageKey &key, float frame, const vector<int> &tiles);

  void device_update(Device *device, Scene *scene, Progress &progress);
  void device_update_slot(Device *device, Scene *scene, int slot, Progress *progress);
  void device_free(Device *device);

  void device_load_builtin(Device *device, Scene *scene, Progress &progress);
  void device_free_builtin(Device *device);

  void set_osl_texture_system(void *texture_system);
  bool set_animation_frame_update(int frame);

  void collect_statistics(RenderStats *stats);

  bool need_update;

  /* NOTE: Here pixels_size is a size of storage, which equals to
   *       width * height * depth.
   *       Use this to avoid some nasty memory corruptions.
   */
  function<void(const string &filename, void *data, ImageMetaData &metadata)>
      builtin_image_info_cb;
  function<bool(const string &filename,
                void *data,
                int tile,
                unsigned char *pixels,
                const size_t pixels_size,
                const bool associate_alpha,
                const bool free_cache)>
      builtin_image_pixels_cb;
  function<bool(const string &filename,
                void *data,
                int tile,
                float *pixels,
                const size_t pixels_size,
                const bool associate_alpha,
                const bool free_cache)>
      builtin_image_float_pixels_cb;

  struct Image {
    ImageKey key;
    ImageMetaData metadata;

    float frame;
    bool need_load;

    string mem_name;
    device_memory *mem;

    int users;
  };

 private:
  bool has_half_images;

  thread_mutex device_mutex;
  int animation_frame;

  vector<Image *> images;
  void *osl_texture_system;

  int add_image_slot(const ImageKey &key, float frame);
  void add_image_user(int slot);
  void remove_image_user(int slot);

  bool load_image_metadata(const ImageKey &key, ImageMetaData &metadata);

  bool file_load_image_generic(Image *img, unique_ptr<ImageInput> *in);

  template<TypeDesc::BASETYPE FileFormat, typename StorageType, typename DeviceType>
  bool file_load_image(Image *img, int texture_limit, device_vector<DeviceType> &tex_img);

  void metadata_detect_colorspace(ImageMetaData &metadata, const char *file_format);

  void device_load_image(Device *device, Scene *scene, int slot, Progress *progress);
  void device_free_image(Device *device, int slot);

  friend class ImageHandle;
};

CCL_NAMESPACE_END

#endif /* __IMAGE_H__ */
