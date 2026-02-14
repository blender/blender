/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "device/memory.h"

#include "scene/image_cache.h"
#include "scene/image_loader.h"

#include "util/colorspace.h"
#include "util/image_metadata.h"
#include "util/set.h"
#include "util/thread.h"
#include "util/unique_ptr.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceInfo;
class ImageLoader;
class ImageSingle;
class ImageHandle;
class ImageKey;
class ImageManager;
class ImageUDIM;
class ImageTexture;
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

/* Image Handle
 *
 * Access handle for image in the image manager. Multiple shader nodes may
 * share the same image, and this class handles reference counting for that.
 *
 * This may reference a single image, or a UDIM with multiple images. */
class ImageHandle {
 public:
  ImageHandle();
  ImageHandle(ImageTexture *image_texture, ImageManager *manager);
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
  bool all_udim_tiled(Progress &progress);
  int kernel_id() const;

  device_image *vdb_image_memory() const;
  VDBImageLoader *vdb_loader() const;

  ImageManager *get_manager() const;

  void add_to_set(set<const ImageSingle *> &images) const;

 protected:
  ImageTexture *image_texture = nullptr;
  ImageManager *manager = nullptr;

  friend class ImageManager;
};

/* Image Texture
 *
 * Base class for an entry in the image manager, which can either be
 * a single image or a UDIM. */
class ImageTexture {
 public:
  std::atomic<int> users = 0;
  enum { SINGLE, UDIM } type = SINGLE;
  bool need_load = true;
};

/* Image Single
 *
 * Representation of single image texture in the image manager. */
class ImageSingle : public ImageTexture {
 public:
  ~ImageSingle();

  /* Index into ImageManager::images and DeviceScene::image_textures. */
  int image_texture_id = KERNEL_IMAGE_NONE;

  ImageParams params;
  ImageMetaData metadata;
  unique_ptr<ImageLoader> loader;

  bool need_metadata = true;
  bool builtin = false;

  thread_mutex mutex;

  device_image *vdb_memory = nullptr;
};

/* Image UDIM
 *
 * Representation of an UDIM image in the image manager. */
class ImageUDIM : public ImageTexture {
 public:
  /* Negative kernel ID encoding offset into DeviceScene::image_texture_udims. */
  int id = KERNEL_IMAGE_NONE;

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

  bool set_animation_frame_update(const int frame);

  void collect_statistics(RenderStats *stats, Scene *scene);

  void tag_update();

  bool need_update() const;

  bool get_use_texture_cache() const;
  bool get_auto_texture_cache() const;

 private:
  bool need_update_ = true;

  thread_mutex device_mutex;
  thread_mutex images_mutex;
  int animation_frame = 0;

  unique_ptr_vector<ImageSingle> images;
  unique_ptr_vector<ImageUDIM> image_udims;
  int num_udim_tiles = 0;

  ImageCache image_cache;

  bool use_texture_cache = true;
  bool auto_texture_cache = false;
  std::string texture_cache_path;

  ImageSingle *add_image_texture(unique_ptr<ImageLoader> &&loader,
                                 const ImageParams &params,
                                 const bool builtin);
  ImageUDIM *add_image_texture(vector<std::pair<int, ImageHandle>> &&tiles);

  void load_image_metadata(ImageSingle *img, Progress &progress);

  void device_gpu_load_requested(Device *device, DeviceQueue &queue, Scene *scene);
  void device_cpu_load_requested(Device *device,
                                 Scene *scene,
                                 size_t image_texture_id,
                                 int miplevel,
                                 int x,
                                 int y,
                                 KernelTileDescriptor &tile_descriptor);

  void device_load_image(Device *device,
                         Scene *scene,
                         const size_t image_texture_id,
                         Progress &progress);
  void device_free_image(Scene *scene, const size_t image_texture_id);

  void device_update_udims(Device *device, Scene *scene);

  void device_resize_image_textures(Scene *scene);
  void device_copy_image_textures(Device *device, Scene *scene);

  friend class ImageHandle;
};

CCL_NAMESPACE_END
