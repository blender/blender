/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/device.h"

#include "scene/image.h"
#include "scene/image_oiio.h"
#include "scene/image_vdb.h"
#include "scene/scene.h"
#include "scene/stats.h"

#include "util/colorspace.h"
#include "util/image.h"
#include "util/image_impl.h"
#include "util/log.h"
#include "util/progress.h"
#include "util/task.h"
#include "util/types_image.h"

CCL_NAMESPACE_BEGIN

/* Image Handle */

ImageHandle::ImageHandle() = default;

ImageHandle::ImageHandle(ImageTexture *image_texture, ImageManager *manager)
    : image_texture(image_texture), manager(manager)
{
  if (image_texture) {
    image_texture->users++;
  }
}

ImageHandle::ImageHandle(const ImageHandle &other)
    : image_texture(other.image_texture), manager(other.manager)
{
  if (image_texture) {
    image_texture->users++;
  }
}

ImageHandle::ImageHandle(ImageHandle &&other)
    : image_texture(other.image_texture), manager(other.manager)
{
  if (&other == this) {
    abort();
  }
  other.image_texture = nullptr;
  other.manager = nullptr;
}

ImageHandle &ImageHandle::operator=(const ImageHandle &other)
{
  clear();
  image_texture = other.image_texture;
  manager = other.manager;

  if (image_texture) {
    image_texture->users++;
  }

  return *this;
}

ImageHandle &ImageHandle::operator=(ImageHandle &&other)
{
  clear();
  image_texture = other.image_texture;
  manager = other.manager;
  other.image_texture = nullptr;
  other.manager = nullptr;

  return *this;
}

ImageHandle::~ImageHandle()
{
  clear();
}

void ImageHandle::clear()
{
  /* Don't remove immediately, rather do it all together later on. one of
   * the reasons for this is that on shader changes we add and remove nodes
   * that use them, but we do not want to reload the image all the time. */
  if (image_texture) {
    assert(image_texture->users >= 1);
    image_texture->users--;
    if (image_texture->users == 0) {
      manager->tag_update();
    }
    image_texture = nullptr;
  }

  manager = nullptr;
}

bool ImageHandle::empty() const
{
  return image_texture == nullptr;
}

int ImageHandle::num_tiles() const
{
  if (image_texture && image_texture->type == ImageTexture::UDIM) {
    ImageUDIM *udim = static_cast<ImageUDIM *>(image_texture);
    return udim->tiles.size();
  }

  return 0;
}

ImageMetaData ImageHandle::metadata(Progress &progress)
{
  if (image_texture) {
    if (image_texture->type == ImageTexture::SINGLE) {
      ImageSingle *img = static_cast<ImageSingle *>(image_texture);
      manager->load_image_metadata(img, progress);
      return img->metadata;
    }
    if (image_texture->type == ImageTexture::UDIM) {
      ImageUDIM *udim = static_cast<ImageUDIM *>(image_texture);
      return udim->tiles[0].second.metadata(progress);
    }
  }

  return ImageMetaData();
}

int ImageHandle::kernel_id() const
{
  if (!image_texture) {
    return KERNEL_IMAGE_NONE;
  }
  if (image_texture->type == ImageTexture::SINGLE) {
    return static_cast<const ImageSingle *>(image_texture)->image_texture_id;
  }
  return static_cast<const ImageUDIM *>(image_texture)->id;
}

device_image *ImageHandle::vdb_image_memory() const
{
  if (image_texture == nullptr || image_texture->type != ImageTexture::SINGLE) {
    return nullptr;
  }

  ImageSingle *img = static_cast<ImageSingle *>(image_texture);
  return img->vdb_memory;
}

VDBImageLoader *ImageHandle::vdb_loader() const
{
  if (image_texture == nullptr || image_texture->type != ImageTexture::SINGLE) {
    return nullptr;
  }

  ImageSingle *img = static_cast<ImageSingle *>(image_texture);
  ImageLoader *loader = img->loader.get();
  if (loader == nullptr) {
    return nullptr;
  }

  if (loader->is_vdb_loader()) {
    return dynamic_cast<VDBImageLoader *>(loader);
  }

  return nullptr;
}

ImageManager *ImageHandle::get_manager() const
{
  return manager;
}

bool ImageHandle::operator==(const ImageHandle &other) const
{
  return image_texture == other.image_texture && manager == other.manager;
}

void ImageHandle::add_to_set(set<const ImageSingle *> &images) const
{
  if (empty()) {
    return;
  }

  if (image_texture->type == ImageTexture::SINGLE) {
    images.insert(static_cast<const ImageSingle *>(image_texture));
  }
  else {
    for (const auto &tile : static_cast<const ImageUDIM *>(image_texture)->tiles) {
      images.insert(static_cast<const ImageSingle *>(tile.second.image_texture));
    }
  }
}

/* Image Single */

ImageSingle::~ImageSingle() = default;

/* Image Manager */

ImageManager::ImageManager(const DeviceInfo & /*info*/)
{
  need_update_ = true;
  animation_frame = 0;
}

ImageManager::~ImageManager()
{
  for (ImageSingle *img : images) {
    assert(!img);
    (void)img;
  }
}

bool ImageManager::set_animation_frame_update(const int frame)
{
  if (frame != animation_frame) {
    const thread_scoped_lock device_lock(images_mutex);
    animation_frame = frame;

    for (ImageSingle *img : images) {
      if (img && img->params.animated) {
        return true;
      }
    }
  }

  return false;
}

void ImageManager::load_image_metadata(ImageSingle *img, Progress & /*progress*/)
{
  if (!img->need_metadata) {
    return;
  }

  const thread_scoped_lock image_lock(img->mutex);
  if (!img->need_metadata) {
    return;
  }

  /* Isolate threading since we are holding a mutex lock and metadata loading
   * may involve multi-threading from e.g. the texture cache generation or host
   * application processing. */
  isolate_task([&]() {
    ImageMetaData &metadata = img->metadata;
    metadata = ImageMetaData();
    metadata.colorspace = img->params.colorspace;

    if (img->loader->load_metadata(metadata)) {
      assert(metadata.type != IMAGE_DATA_NUM_TYPES);
    }
    else {
      metadata.type = IMAGE_DATA_TYPE_BYTE4;
    }

    metadata.finalize(img->params.alpha_type);

    img->need_metadata = false;
  });
}

ImageHandle ImageManager::add_image(const string &filename, const ImageParams &params)
{
  ImageSingle *image = add_image_texture(make_unique<OIIOImageLoader>(filename), params, false);
  return ImageHandle(image, this);
}

ImageHandle ImageManager::add_image(const string &filename,
                                    const ImageParams &params,
                                    const array<int> &tiles)
{
  if (tiles.empty()) {
    return add_image(filename, params);
  }

  vector<std::pair<int, ImageHandle>> udim_tiles;
  for (const int tile : tiles) {
    string tile_filename = filename;

    /* Since we don't have information about the exact tile format used in this code location,
     * just attempt all replacement patterns that Blender supports. */
    string_replace(tile_filename, "<UDIM>", string_printf("%04d", tile));

    const int u = ((tile - 1001) % 10);
    const int v = ((tile - 1001) / 10);
    string_replace(tile_filename, "<UVTILE>", string_printf("u%d_v%d", u + 1, v + 1));

    ImageSingle *image = add_image_texture(
        make_unique<OIIOImageLoader>(tile_filename), params, false);
    udim_tiles.emplace_back(tile, ImageHandle(image, this));
  }

  ImageUDIM *udim = add_image_texture(std::move(udim_tiles));
  return ImageHandle(udim, this);
}

ImageHandle ImageManager::add_image(unique_ptr<ImageLoader> &&loader,
                                    const ImageParams &params,
                                    const bool builtin)
{
  ImageSingle *image = add_image_texture(std::move(loader), params, builtin);
  return ImageHandle(image, this);
}

ImageHandle ImageManager::add_image(vector<unique_ptr<ImageLoader>> &&loaders,
                                    const ImageParams &params)
{
  vector<std::pair<int, ImageHandle>> udim_tiles;

  for (unique_ptr<ImageLoader> &loader : loaders) {
    unique_ptr<ImageLoader> local_loader;
    std::swap(loader, local_loader);
    ImageSingle *image = add_image_texture(std::move(local_loader), params, true);
    udim_tiles.emplace_back(image->loader->get_tile_number(), ImageHandle(image, this));
  }

  ImageUDIM *udim = add_image_texture(std::move(udim_tiles));
  return ImageHandle(udim, this);
}

ImageSingle *ImageManager::add_image_texture(unique_ptr<ImageLoader> &&loader,
                                             const ImageParams &params,
                                             const bool builtin)
{
  const thread_scoped_lock device_lock(images_mutex);

  /* Find existing image. */
  size_t image_texture_id;
  for (image_texture_id = 0; image_texture_id < images.size(); image_texture_id++) {
    ImageSingle *img = images[image_texture_id];
    if (img && ImageLoader::equals(img->loader.get(), loader.get()) && img->params == params) {
      return img;
    }
  }

  /* Find free image_texture_id. */
  for (image_texture_id = 0; image_texture_id < images.size(); image_texture_id++) {
    if (!images[image_texture_id]) {
      break;
    }
  }

  if (image_texture_id == images.size()) {
    images.resize(images.size() + 1);
  }

  /* Add new image. */
  unique_ptr<ImageSingle> img = make_unique<ImageSingle>();
  img->type = ImageTexture::SINGLE;
  img->image_texture_id = image_texture_id;
  img->params = params;
  img->loader = std::move(loader);
  img->builtin = builtin;

  images.replace(image_texture_id, std::move(img));

  tag_update();

  return images[image_texture_id];
}

ImageUDIM *ImageManager::add_image_texture(vector<std::pair<int, ImageHandle>> &&tiles)
{
  const thread_scoped_lock device_lock(images_mutex);

  /* Find existing UDIM. */
  size_t image_texture_id;
  for (image_texture_id = 0; image_texture_id < image_udims.size(); image_texture_id++) {
    ImageUDIM *udim = image_udims[image_texture_id];
    if (udim && udim->tiles == tiles) {
      return udim;
    }
  }

  /* Find free image_texture_id. */
  for (image_texture_id = 0; image_texture_id < image_udims.size(); image_texture_id++) {
    if (!image_udims[image_texture_id]) {
      break;
    }
  }

  if (image_texture_id == image_udims.size()) {
    image_udims.resize(image_udims.size() + 1);
  }

  /* Add new image. */
  unique_ptr<ImageUDIM> img = make_unique<ImageUDIM>();
  img->type = ImageTexture::UDIM;
  img->id = -num_udim_tiles - 1;
  img->tiles = std::move(tiles);

  num_udim_tiles += img->tiles.size() + 1;

  image_udims.replace(image_texture_id, std::move(img));

  tag_update();

  return image_udims[image_texture_id];
}

void ImageManager::device_resize_image_textures(Scene *scene)
{
  const thread_scoped_lock device_lock(device_mutex);
  DeviceScene &dscene = scene->dscene;

  if (dscene.image_textures.size() < images.size()) {
    dscene.image_textures.resize(images.size());
  }
}

void ImageManager::device_copy_image_textures(Scene *scene)
{
  image_cache.copy_to_device_if_modified(scene->dscene);

  const thread_scoped_lock device_lock(device_mutex);
  DeviceScene &dscene = scene->dscene;

  dscene.image_textures.copy_to_device_if_modified();
  dscene.image_texture_udims.copy_to_device_if_modified();
}

void ImageManager::device_load_image(Device *device,
                                     Scene *scene,
                                     const size_t image_texture_id,
                                     Progress &progress)
{
  if (progress.get_cancel()) {
    return;
  }

  ImageSingle *img = images[image_texture_id];

  progress.set_status("Updating Images", "Loading " + img->loader->name());

  load_image_metadata(img, progress);

  KernelImageTexture tex;
  tex.width = img->metadata.width;
  tex.height = img->metadata.height;
  tex.interpolation = img->params.interpolation;
  tex.extension = img->params.extension;
  tex.use_transform_3d = img->metadata.use_transform_3d;
  tex.transform_3d = img->metadata.transform_3d;

  img->vdb_memory = image_cache.load_image_full(
      *device, *img->loader, img->metadata, scene->params.texture_limit, tex);

  /* Update image texture device data. */
  scene->dscene.image_textures[image_texture_id] = tex;
  scene->dscene.image_textures.tag_modified();

  /* Cleanup memory in image loader. */
  img->loader->cleanup();
  img->need_load = false;
}

void ImageManager::device_free_image(Scene *scene, size_t image_texture_id)
{
  ImageSingle *img = images[image_texture_id];
  if (img == nullptr) {
    return;
  }

  const KernelImageTexture &tex = scene->dscene.image_textures[image_texture_id];
  image_cache.free_image(scene->dscene, tex);
  img->vdb_memory = nullptr;

  images.steal(image_texture_id);
}

void ImageManager::device_update_udims(Device * /*device*/, Scene *scene)
{
  const thread_scoped_lock device_lock(device_mutex);
  device_vector<KernelImageUDIM> &device_udims = scene->dscene.image_texture_udims;
  if (device_udims.size() == num_udim_tiles) {
    return;
  }

  device_udims.resize(num_udim_tiles);

  for (auto [udim_id, udim] : image_udims.enumerate()) {
    if (udim == nullptr) {
      continue;
    }

    if (udim->users == 0) {
      image_udims.replace(udim_id, nullptr);
    }
    else if (udim->need_load) {
      const uint udim_offset = -udim->id - 1;
      KernelImageUDIM *udim_data = device_udims.data() + udim_offset;

      udim_data[0] = KernelImageUDIM{.tile = int(udim->tiles.size()), .image_texture_id = 0};
      for (int i = 0; i < udim->tiles.size(); i++) {
        const auto &tile = udim->tiles[i];
        udim_data[i + 1] = KernelImageUDIM{.tile = tile.first,
                                           .image_texture_id = tile.second.kernel_id()};
      }
      udim->need_load = false;
    }
  }
}

void ImageManager::device_update(Device *device, Scene *scene, Progress &progress)
{
  if (!need_update()) {
    return;
  }

  const scoped_callback_timer timer([scene](double time) {
    if (scene->update_stats) {
      scene->update_stats->image.times.add_entry({"device_update", time});
    }
  });

  /* Update UDIM ids. */
  device_update_udims(device, scene);

  /* Resize devices arrays to match. */
  device_resize_image_textures(scene);

  /* Free and load images. */
  TaskPool pool;
  for (auto [image_texture_id, img] : images.enumerate()) {
    if (img && img->users == 0) {
      device_free_image(scene, image_texture_id);
    }
    else if (img && img->need_load) {
      pool.push([this, device, scene, image_texture_id, &progress] {
        device_load_image(device, scene, image_texture_id, progress);
      });
    }
  }

  pool.wait_work();

  /* Copy device arrays. */
  device_copy_image_textures(scene);

  need_update_ = false;
}

void ImageManager::device_load_images(Device *device,
                                      Scene *scene,
                                      Progress &progress,
                                      const set<const ImageSingle *> &images)
{
  /* Update UDIM ids. */
  device_update_udims(device, scene);

  /* Resize devices arrays to match number of images. */
  device_resize_image_textures(scene);

  /* Load handles. */
  TaskPool pool;
  for (const ImageSingle *img : images) {
    pool.push([this, device, scene, img, &progress] {
      assert(img != nullptr);
      if (img->users == 0) {
        device_free_image(scene, img->image_texture_id);
      }
      else if (img->need_load) {
        device_load_image(device, scene, img->image_texture_id, progress);
      }
    });
  }
  pool.wait_work();

  /* Copy device arrays. */
  device_copy_image_textures(scene);
}

void ImageManager::device_load_builtin(Device *device, Scene *scene, Progress &progress)
{
  /* Load only builtin images, Blender needs this to load evaluated
   * scene data from depsgraph before it is freed. */
  if (!need_update()) {
    return;
  }

  device_resize_image_textures(scene);

  TaskPool pool;
  for (auto [image_texture_id, img] : images.enumerate()) {
    if (img && img->need_load && img->builtin) {
      pool.push([this, device, scene, image_texture_id, &progress] {
        device_load_image(device, scene, image_texture_id, progress);
      });
    }
  }

  pool.wait_work();
}

void ImageManager::device_free_builtin(Scene *scene)
{
  image_udims.clear();
  for (auto [image_texture_id, img] : images.enumerate()) {
    if (img && img->builtin) {
      device_free_image(scene, image_texture_id);
    }
  }
}

void ImageManager::device_free(Scene *scene)
{
  image_udims.clear();
  for (auto [image_texture_id, img] : images.enumerate()) {
    device_free_image(scene, image_texture_id);
  }
  images.clear();
  image_cache.device_free(scene->dscene);
  /* TODO: missing device lock? */
  scene->dscene.image_textures.free();
  scene->dscene.image_texture_udims.free();
}

void ImageManager::collect_statistics(RenderStats *stats, Scene * /*scene*/)
{
  for (auto [image_texture_id, image] : images.enumerate()) {
    if (!image) {
      continue;
    }

    stats->image.textures.add_entry(
        NamedSizeEntry(image->loader->name(), image->metadata.memory_size()));
  }
}

void ImageManager::tag_update()
{
  need_update_ = true;
}

bool ImageManager::need_update() const
{
  return need_update_;
}

CCL_NAMESPACE_END
