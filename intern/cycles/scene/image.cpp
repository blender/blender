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

#ifdef WITH_OSL
#  include <OSL/oslexec.h>
#endif

CCL_NAMESPACE_BEGIN

/* Image Handle */

ImageHandle::ImageHandle() : manager(nullptr) {}

ImageHandle::ImageHandle(const ImageHandle &other)
    : slots(other.slots), is_tiled(other.is_tiled), manager(other.manager)
{
  /* Increase image user count. */
  for (const size_t slot : slots) {
    manager->add_image_user(slot);
  }
}

ImageHandle &ImageHandle::operator=(const ImageHandle &other)
{
  clear();
  manager = other.manager;
  is_tiled = other.is_tiled;
  slots = other.slots;

  for (const size_t slot : slots) {
    manager->add_image_user(slot);
  }

  return *this;
}

ImageHandle::~ImageHandle()
{
  clear();
}

void ImageHandle::clear()
{
  for (const size_t slot : slots) {
    manager->remove_image_user(slot);
  }

  slots.clear();
  manager = nullptr;
}

bool ImageHandle::empty() const
{
  return slots.empty();
}

int ImageHandle::num_tiles() const
{
  return (is_tiled) ? slots.size() : 0;
}

int ImageHandle::num_svm_slots() const
{
  return slots.size();
}

ImageMetaData ImageHandle::metadata()
{
  if (slots.empty()) {
    return ImageMetaData();
  }

  ImageManager::Image *img = manager->get_image_slot(slots.front());
  manager->load_image_metadata(img);
  return img->metadata;
}

int ImageHandle::svm_slot(const int slot_index) const
{
  if (slot_index >= slots.size()) {
    return -1;
  }

  if (manager->osl_texture_system) {
    ImageManager::Image *img = manager->get_image_slot(slots[slot_index]);
    if (!img->loader->osl_filepath().empty()) {
      return -1;
    }
  }

  return slots[slot_index];
}

vector<int4> ImageHandle::get_svm_slots() const
{
  const size_t num_nodes = divide_up(slots.size(), 2);

  vector<int4> svm_slots;
  svm_slots.reserve(num_nodes);
  for (size_t i = 0; i < num_nodes; i++) {
    int4 node;

    size_t slot = slots[2 * i];
    node.x = manager->get_image_slot(slot)->loader->get_tile_number();
    node.y = slot;

    if ((2 * i + 1) < slots.size()) {
      slot = slots[2 * i + 1];
      node.z = manager->get_image_slot(slot)->loader->get_tile_number();
      node.w = slot;
    }
    else {
      node.z = -1;
      node.w = -1;
    }

    svm_slots.push_back(node);
  }

  return svm_slots;
}

device_image *ImageHandle::image_memory() const
{
  if (slots.empty()) {
    return nullptr;
  }

  ImageManager::Image *img = manager->get_image_slot(slots[0]);
  return img ? img->mem : nullptr;
}

VDBImageLoader *ImageHandle::vdb_loader() const
{
  if (slots.empty()) {
    return nullptr;
  }

  ImageManager::Image *img = manager->get_image_slot(slots[0]);

  if (img == nullptr) {
    return nullptr;
  }

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
  return manager == other.manager && is_tiled == other.is_tiled && slots == other.slots;
}

/* Image Manager */

ImageManager::ImageManager(const DeviceInfo & /*info*/)
{
  need_update_ = true;
  osl_texture_system = nullptr;
  animation_frame = 0;
}

ImageManager::~ImageManager()
{
  for (size_t slot = 0; slot < images.size(); slot++) {
    assert(!images[slot]);
  }
}

void ImageManager::set_osl_texture_system(void *texture_system)
{
  osl_texture_system = texture_system;
}

bool ImageManager::set_animation_frame_update(const int frame)
{
  if (frame != animation_frame) {
    const thread_scoped_lock device_lock(images_mutex);
    animation_frame = frame;

    for (size_t slot = 0; slot < images.size(); slot++) {
      if (images[slot] && images[slot]->params.animated) {
        return true;
      }
    }
  }

  return false;
}

void ImageManager::load_image_metadata(Image *img)
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
  const size_t slot = add_image_slot(make_unique<OIIOImageLoader>(filename), params, false);

  ImageHandle handle;
  handle.slots.push_back(slot);
  handle.manager = this;
  return handle;
}

ImageHandle ImageManager::add_image(const string &filename,
                                    const ImageParams &params,
                                    const array<int> &tiles)
{
  ImageHandle handle;
  handle.manager = this;
  handle.is_tiled = !tiles.empty();

  if (!handle.is_tiled) {
    const size_t slot = add_image_slot(make_unique<OIIOImageLoader>(filename), params, false);
    handle.slots.push_back(slot);
    return handle;
  }

  for (const int tile : tiles) {
    string tile_filename = filename;

    /* Since we don't have information about the exact tile format used in this code location,
     * just attempt all replacement patterns that Blender supports. */
    string_replace(tile_filename, "<UDIM>", string_printf("%04d", tile));

    const int u = ((tile - 1001) % 10);
    const int v = ((tile - 1001) / 10);
    string_replace(tile_filename, "<UVTILE>", string_printf("u%d_v%d", u + 1, v + 1));

    const size_t slot = add_image_slot(make_unique<OIIOImageLoader>(tile_filename), params, false);
    handle.slots.push_back(slot);
  }

  return handle;
}

ImageHandle ImageManager::add_image(unique_ptr<ImageLoader> &&loader,
                                    const ImageParams &params,
                                    const bool builtin)
{
  const size_t slot = add_image_slot(std::move(loader), params, builtin);

  ImageHandle handle;
  handle.slots.push_back(slot);
  handle.manager = this;
  return handle;
}

ImageHandle ImageManager::add_image(vector<unique_ptr<ImageLoader>> &&loaders,
                                    const ImageParams &params)
{
  ImageHandle handle;
  handle.is_tiled = true;

  for (unique_ptr<ImageLoader> &loader : loaders) {
    unique_ptr<ImageLoader> local_loader;
    std::swap(loader, local_loader);
    const size_t slot = add_image_slot(std::move(local_loader), params, true);
    handle.slots.push_back(slot);
  }

  handle.manager = this;
  return handle;
}

size_t ImageManager::add_image_slot(unique_ptr<ImageLoader> &&loader,
                                    const ImageParams &params,
                                    const bool builtin)
{
  size_t slot;

  const thread_scoped_lock device_lock(images_mutex);

  /* Find existing image. */
  for (slot = 0; slot < images.size(); slot++) {
    Image *img = images[slot].get();
    if (img && ImageLoader::equals(img->loader.get(), loader.get()) && img->params == params) {
      img->users++;
      return slot;
    }
  }

  /* Find free slot. */
  for (slot = 0; slot < images.size(); slot++) {
    if (!images[slot]) {
      break;
    }
  }

  if (slot == images.size()) {
    images.resize(images.size() + 1);
  }

  /* Add new image. */
  unique_ptr<Image> img = make_unique<Image>();
  img->params = params;
  img->loader = std::move(loader);
  img->need_metadata = true;
  img->need_load = !(osl_texture_system && !img->loader->osl_filepath().empty());
  img->builtin = builtin;
  img->users = 1;
  img->mem = nullptr;

  images[slot] = std::move(img);

  need_update_ = true;

  return slot;
}

void ImageManager::add_image_user(const size_t slot)
{
  const thread_scoped_lock device_lock(images_mutex);
  Image *image = images[slot].get();
  assert(image && image->users >= 1);

  image->users++;
}

void ImageManager::remove_image_user(const size_t slot)
{
  const thread_scoped_lock device_lock(images_mutex);
  Image *image = images[slot].get();
  assert(image && image->users >= 1);

  /* decrement user count */
  image->users--;

  /* don't remove immediately, rather do it all together later on. one of
   * the reasons for this is that on shader changes we add and remove nodes
   * that use them, but we do not want to reload the image all the time. */
  if (image->users == 0) {
    need_update_ = true;
  }
}

ImageManager::Image *ImageManager::get_image_slot(const size_t slot)
{
  /* Need mutex lock, images vector might get resized by another thread. */
  const thread_scoped_lock device_lock(images_mutex);
  return images[slot].get();
}

void ImageManager::device_load_image(Device *device,
                                     Scene *scene,
                                     const size_t slot,
                                     Progress &progress)
{
  if (progress.get_cancel()) {
    return;
  }

  Image *img = images[slot].get();

  if (img->users == 0) {
    return;
  }

  progress.set_status("Updating Images", "Loading " + img->loader->name());

  load_image_metadata(img);

  img->mem = image_cache.load_image_full(*device,
                                         *img->loader,
                                         img->metadata,
                                         img->params.interpolation,
                                         img->params.extension,
                                         scene->params.texture_limit,
                                         slot);

  /* Cleanup memory in image loader. */
  img->loader->cleanup();
  img->need_load = false;
}

void ImageManager::device_free_image(Scene *scene, size_t slot)
{
  Image *img = images[slot].get();
  if (img == nullptr) {
    return;
  }

  if (osl_texture_system) {
#ifdef WITH_OSL
    const ustring filepath = img->loader->osl_filepath();
    if (!filepath.empty()) {
      ((OSL::TextureSystem *)osl_texture_system)->invalidate(filepath);
    }
#endif
  }

  if (img->mem) {
    const thread_scoped_lock device_lock(device_mutex);
    image_cache.free_image(scene->dscene, slot);
    img->mem = nullptr;
  }

  images[slot].reset();
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

  TaskPool pool;
  for (size_t slot = 0; slot < images.size(); slot++) {
    Image *img = images[slot].get();
    if (img && img->users == 0) {
      device_free_image(scene, slot);
    }
    else if (img && img->need_load) {
      pool.push([this, device, scene, slot, &progress] {
        device_load_image(device, scene, slot, progress);
      });
    }
  }

  pool.wait_work();

  /* Copy device arrays. */
  image_cache.copy_to_device_if_modified(scene->dscene);

  need_update_ = false;
}

void ImageManager::device_update_slot(Device *device,
                                      Scene *scene,
                                      const size_t slot,
                                      Progress &progress)
{
  Image *img = images[slot].get();
  assert(img != nullptr);

  if (img->users == 0) {

    device_free_image(scene, slot);
  }

  else if (img->need_load) {
    device_load_image(device, scene, slot, progress);
  }

  /* Copy device arrays. */
  image_cache.copy_to_device_if_modified(scene->dscene);
}

void ImageManager::device_load_builtin(Device *device, Scene *scene, Progress &progress)
{
  /* Load only builtin images, Blender needs this to load evaluated
   * scene data from depsgraph before it is freed. */
  if (!need_update()) {
    return;
  }

  TaskPool pool;
  for (size_t slot = 0; slot < images.size(); slot++) {
    Image *img = images[slot].get();
    if (img && img->need_load && img->builtin) {
      pool.push([this, device, scene, slot, &progress] {
        device_load_image(device, scene, slot, progress);
      });
    }
  }

  pool.wait_work();
}

void ImageManager::device_free_builtin(Scene *scene)
{
  for (size_t slot = 0; slot < images.size(); slot++) {
    Image *img = images[slot].get();
    if (img && img->builtin) {
      device_free_image(scene, slot);
    }
  }
}

void ImageManager::device_free(Scene *scene)
{
  for (size_t slot = 0; slot < images.size(); slot++) {
    device_free_image(scene, slot);
  }
  images.clear();

  const thread_scoped_lock device_lock(device_mutex);
  image_cache.device_free(scene->dscene);
}

void ImageManager::collect_statistics(RenderStats *stats)
{
  for (const unique_ptr<Image> &image : images) {
    if (!image || !image->mem) {
      /* Image may have been freed due to lack of users. */
      continue;
    }
    stats->image.textures.add_entry(
        NamedSizeEntry(image->loader->name(), image->mem->memory_size()));
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
