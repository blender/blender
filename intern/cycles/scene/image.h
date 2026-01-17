/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "device/memory.h"

#include "scene/image_cache.h"

#include "util/colorspace.h"
#include "util/image_metadata.h"
#include "util/set.h"
#include "util/string.h"
#include "util/thread.h"
#include "util/types_image.h"
#include "util/unique_ptr.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceInfo;
class ImageSingle;
class ImageHandle;
class ImageKey;
class ImageManager;
class ImageMetaData;
class ImageUDIM;
class ImageSlot;
class Progress;
class RenderStats;
class Scene;
class SceneParams;
class ColorSpaceProcessor;
class VDBImageLoader;

/* Image Parameters */
class ImageParams {
 public:
  bool animated = false;
  InterpolationType interpolation = INTERPOLATION_LINEAR;
  ExtensionType extension = EXTENSION_CLIP;
  ImageAlphaType alpha_type = IMAGE_ALPHA_AUTO;
  ustring colorspace;
  float frame = 0.0f;

  ImageParams() : colorspace(u_colorspace_scene_linear) {}

  bool operator==(const ImageParams &other) const
  {
    return (animated == other.animated && interpolation == other.interpolation &&
            extension == other.extension && alpha_type == other.alpha_type &&
            colorspace == other.colorspace && frame == other.frame);
  }
};

/* Image loader base class, that can be subclassed to load image data
 * from custom sources (file, memory, procedurally generated, etc). */
class ImageLoader {
 public:
  ImageLoader();
  virtual ~ImageLoader() = default;

  /* Enable use of the texture cache for this image, if supported by the image loader. */
  virtual bool resolve_texture_cache(const bool /*auto_generate*/,
                                     const string & /*texture_cache_path*/,
                                     const ustring & /*colorspace*/,
                                     const ImageAlphaType /*alpha_type*/,
                                     Progress & /*progress*/)
  {
    return false;
  }

  /* Load metadata without actual image yet, should be fast. */
  virtual bool load_metadata(ImageMetaData &metadata) = 0;

  /* Load full image pixels.
   * This is expected to call metadata.conform_pixels(). */
  virtual bool load_pixels(const ImageMetaData &metadata, void *pixels) = 0;

  /* Load pixels for a single tile, if ImageMetaData.tile_size is set.
   * This is expected to call metadata.conform_pixels(). */
  virtual bool load_pixels_tile(const ImageMetaData & /*metadata*/,
                                const int /*miplevel*/,
                                const int64_t /*x*/,
                                const int64_t /*y*/,
                                const int64_t /*w*/,
                                const int64_t /*h*/,
                                const int64_t /*x_stride*/,
                                const int64_t /*y_stride*/,
                                const int64_t /*padding*/,
                                const ExtensionType /*extension*/,
                                uint8_t * /*pixels*/)
  {
    return false;
  }

  /* Name for logs and stats. */
  virtual string name() const = 0;

  /* Optional for tiled textures loaded externally. */
  virtual int get_tile_number() const;

  /* Free any memory used for loading metadata and pixels. */
  virtual void cleanup() {};

  /* Compare avoid loading the same image multiple times. */
  virtual bool equals(const ImageLoader &other) const = 0;
  static bool equals(const ImageLoader *a, const ImageLoader *b);

  virtual bool is_vdb_loader() const;

  /* Work around for no RTTI. */
};

/* Image Handle
 *
 * Access handle for image in the image manager. Multiple shader nodes may
 * share the same image, and this class handles reference counting for that.
 *
 * This may reference a single image, or a UDIM with multiple images. */
class ImageHandle {
 public:
  ImageHandle();
  ImageHandle(ImageSlot *image_slot, ImageManager *manager);
  ImageHandle(const ImageHandle &other);
  ImageHandle(ImageHandle &&other);
  ImageHandle &operator=(const ImageHandle &other);
  ImageHandle &operator=(ImageHandle &&other);
  ~ImageHandle();

  bool operator==(const ImageHandle &other) const;

  void clear();

  bool empty() const;
  int num_tiles() const;

  ImageMetaData metadata(Progress &progress);
  int kernel_id() const;

  device_image *vdb_image_memory() const;
  VDBImageLoader *vdb_loader() const;

  ImageManager *get_manager() const;

  void add_to_set(set<const ImageSingle *> &images) const;

 protected:
  ImageSlot *image_slot = nullptr;
  ImageManager *manager = nullptr;

  friend class ImageManager;
};

/* Image Slot
 *
 * Base class for an entry in the image manager, which can either be
 * a single image or a UDIM. */
class ImageSlot {
 public:
  std::atomic<int> users = 0;
  enum { SINGLE, UDIM } type = SINGLE;
  int id = KERNEL_IMAGE_NONE;
  bool need_load = true;
};

/* Image Single
 *
 * Representation of single image texture in the image manager. */
class ImageSingle : public ImageSlot {
 public:
  ImageParams params;
  ImageMetaData metadata;
  unique_ptr<ImageLoader> loader;

  bool need_metadata = true;
  bool builtin = false;

  thread_mutex mutex;

  // TODO: avoid storing these?
  uint texture_slot = KERNEL_IMAGE_NONE;
  uint tile_descriptor_offset = KERNEL_IMAGE_NONE;
  uint tile_descriptor_levels = 0;
  uint tile_descriptor_num = 0;
  device_image *vdb_memory = nullptr;
};

/* Image UDIM
 *
 * Representation of an UDIM image in the image manager. */
class ImageUDIM : public ImageSlot {
 public:
  vector<std::pair<int, ImageHandle>> tiles;
};

/* Image Manager
 *
 * Handles loading and storage of all images in the scene. This includes 2D
 * texture images and 3D volume images. */
class ImageManager {
 public:
  explicit ImageManager(const DeviceInfo &info, const SceneParams &params);
  ~ImageManager();

  ImageHandle add_image(const string &filename, const ImageParams &params);
  ImageHandle add_image(const string &filename,
                        const ImageParams &params,
                        const array<int> &tiles);
  ImageHandle add_image(unique_ptr<ImageLoader> &&loader,
                        const ImageParams &params,
                        const bool builtin = true);
  ImageHandle add_image(vector<unique_ptr<ImageLoader>> &&loaders, const ImageParams &params);

  void device_update(Device *device, Scene *scene, Progress &progress);
  void device_free(Scene *scene);

  void device_load_builtin(Device *device, Scene *scene, Progress &progress);
  void device_free_builtin(Scene *scene);

  void device_load_images(Device *device,
                          Scene *scene,
                          Progress &progress,
                          const set<const ImageSingle *> &images);

  void device_gpu_load_requested(Device *device, Scene *scene);
  void device_cpu_load_requested(Device *device,
                                 Scene *scene,
                                 size_t slot,
                                 int miplevel,
                                 int x,
                                 int y,
                                 KernelTileDescriptor *tile_descriptor);

  bool set_animation_frame_update(const int frame);

  void collect_statistics(RenderStats *stats);

  void tag_update();

  bool need_update() const;

 private:
  bool need_update_ = true;

  thread_mutex device_mutex;
  thread_mutex images_mutex;
  int animation_frame = 0;

  vector<unique_ptr<ImageSingle>> images;
  vector<unique_ptr<ImageUDIM>> image_udims;
  int num_udim_tiles = 0;

  ImageCache image_cache;

  bool use_texture_cache = true;
  bool auto_texture_cache = false;
  std::string texture_cache_path;

  ImageSingle *add_image_slot(unique_ptr<ImageLoader> &&loader,
                              const ImageParams &params,
                              const bool builtin);
  ImageUDIM *add_image_slot(vector<std::pair<int, ImageHandle>> &&tiles);

  void load_image_metadata(ImageSingle *img, Progress &progress);

  template<TypeDesc::BASETYPE FileFormat, typename StorageType>
  bool file_load_image(Device *device, ImageSingle *img, const int texture_limit);

  void device_load_image_tiled(Scene *scene, const size_t slot);
  void device_update_image_requested(Device *device, Scene *scene, ImageSingle *img);
  KernelTileDescriptor device_update_tile_requested(Device *device,
                                                    Scene *scene,
                                                    ImageSingle *img,
                                                    const int miplevel,
                                                    const size_t x,
                                                    const size_t y);

  void device_load_image_full(Device *device, Scene *scene, const size_t slot);
  void device_load_image(Device *device, Scene *scene, const size_t slot, Progress &progress);
  void device_free_image(Scene *scene, const size_t slot);

  void device_update_udims(Device *device, Scene *scene);

  void device_resize_image_textures(Scene *scene);
  void device_copy_image_textures(Device *device, Scene *scene);

  friend class ImageHandle;
};

CCL_NAMESPACE_END
