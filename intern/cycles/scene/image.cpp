/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/image.h"
#include "device/device.h"
#include "scene/colorspace.h"
#include "scene/image_oiio.h"
#include "scene/image_vdb.h"
#include "scene/scene.h"
#include "scene/stats.h"

#include "util/image.h"
#include "util/image_impl.h"
#include "util/log.h"
#include "util/progress.h"
#include "util/task.h"
#include "util/texture.h"

#ifdef WITH_OSL
#  include <OSL/oslexec.h>
#endif

CCL_NAMESPACE_BEGIN

namespace {

const char *name_from_type(ImageDataType type)
{
  switch (type) {
    case IMAGE_DATA_TYPE_FLOAT4:
      return "float4";
    case IMAGE_DATA_TYPE_BYTE4:
      return "byte4";
    case IMAGE_DATA_TYPE_HALF4:
      return "half4";
    case IMAGE_DATA_TYPE_FLOAT:
      return "float";
    case IMAGE_DATA_TYPE_BYTE:
      return "byte";
    case IMAGE_DATA_TYPE_HALF:
      return "half";
    case IMAGE_DATA_TYPE_USHORT4:
      return "ushort4";
    case IMAGE_DATA_TYPE_USHORT:
      return "ushort";
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT:
      return "nanovdb_float";
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT3:
      return "nanovdb_float3";
    case IMAGE_DATA_TYPE_NANOVDB_FLOAT4:
      return "nanovdb_float4";
    case IMAGE_DATA_TYPE_NANOVDB_FPN:
      return "nanovdb_fpn";
    case IMAGE_DATA_TYPE_NANOVDB_FP16:
      return "nanovdb_fp16";
    case IMAGE_DATA_TYPE_NANOVDB_EMPTY:
      return "nanovdb_empty";
    case IMAGE_DATA_NUM_TYPES:
      assert(!"System enumerator type, should never be used");
      return "";
  }
  assert(!"Unhandled image data type");
  return "";
}

}  // namespace

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

device_texture *ImageHandle::image_memory() const
{
  if (slots.empty()) {
    return nullptr;
  }

  ImageManager::Image *img = manager->get_image_slot(slots[0]);
  return img ? img->mem.get() : nullptr;
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

/* Image MetaData */

ImageMetaData::ImageMetaData()
    : channels(0),
      width(0),
      height(0),
      byte_size(0),
      type(IMAGE_DATA_NUM_TYPES),
      colorspace(u_colorspace_raw),
      colorspace_file_format(""),
      use_transform_3d(false),
      compress_as_srgb(false)
{
}

bool ImageMetaData::operator==(const ImageMetaData &other) const
{
  return channels == other.channels && width == other.width && height == other.height &&
         use_transform_3d == other.use_transform_3d &&
         (!use_transform_3d || transform_3d == other.transform_3d) && type == other.type &&
         colorspace == other.colorspace && compress_as_srgb == other.compress_as_srgb;
}

bool ImageMetaData::is_float() const
{
  return (type == IMAGE_DATA_TYPE_FLOAT || type == IMAGE_DATA_TYPE_FLOAT4 ||
          type == IMAGE_DATA_TYPE_HALF || type == IMAGE_DATA_TYPE_HALF4);
}

void ImageMetaData::detect_colorspace()
{
  /* Convert used specified color spaces to one we know how to handle. */
  colorspace = ColorSpaceManager::detect_known_colorspace(
      colorspace, colorspace_file_hint.c_str(), colorspace_file_format, is_float());

  if (colorspace == u_colorspace_raw) {
    /* Nothing to do. */
  }
  else if (colorspace == u_colorspace_srgb) {
    /* Keep sRGB colorspace stored as sRGB, to save memory and/or loading time
     * for the common case of 8bit sRGB images like PNG. */
    compress_as_srgb = true;
  }
  else {
    /* If colorspace conversion needed, use half instead of short so we can
     * represent HDR values that might result from conversion. */
    if (type == IMAGE_DATA_TYPE_BYTE || type == IMAGE_DATA_TYPE_USHORT) {
      type = IMAGE_DATA_TYPE_HALF;
    }
    else if (type == IMAGE_DATA_TYPE_BYTE4 || type == IMAGE_DATA_TYPE_USHORT4) {
      type = IMAGE_DATA_TYPE_HALF4;
    }
  }
}

/* Image Loader */

ImageLoader::ImageLoader() = default;

ustring ImageLoader::osl_filepath() const
{
  return ustring();
}

int ImageLoader::get_tile_number() const
{
  return 0;
}

bool ImageLoader::equals(const ImageLoader *a, const ImageLoader *b)
{
  if (a == nullptr && b == nullptr) {
    return true;
  }
  return (a && b && typeid(*a) == typeid(*b) && a->equals(*b));
}

bool ImageLoader::is_vdb_loader() const
{
  return false;
}

/* Image Manager */

ImageManager::ImageManager(const DeviceInfo &info)
{
  need_update_ = true;
  osl_texture_system = nullptr;
  animation_frame = 0;

  /* Set image limits */
  features.has_nanovdb = info.has_nanovdb;
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

  ImageMetaData &metadata = img->metadata;
  metadata = ImageMetaData();
  metadata.colorspace = img->params.colorspace;

  if (img->loader->load_metadata(features, metadata)) {
    assert(metadata.type != IMAGE_DATA_NUM_TYPES);
  }
  else {
    metadata.type = IMAGE_DATA_TYPE_BYTE4;
  }

  metadata.detect_colorspace();

  assert(features.has_nanovdb || !is_nanovdb_type(metadata.type));

  img->need_metadata = false;
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

static bool image_associate_alpha(ImageManager::Image *img)
{
  /* For typical RGBA images we let OIIO convert to associated alpha,
   * but some types we want to leave the RGB channels untouched. */
  return !(ColorSpaceManager::colorspace_is_data(img->params.colorspace) ||
           img->params.alpha_type == IMAGE_ALPHA_IGNORE ||
           img->params.alpha_type == IMAGE_ALPHA_CHANNEL_PACKED);
}

template<TypeDesc::BASETYPE FileFormat, typename StorageType>
bool ImageManager::file_load_image(Image *img, const int texture_limit)
{
  /* Ignore empty images. */
  if (!(img->metadata.channels > 0)) {
    return false;
  }

  /* Get metadata. */
  const int width = img->metadata.width;
  const int height = img->metadata.height;
  const int components = img->metadata.channels;

  /* Read pixels. */
  vector<StorageType> pixels_storage;
  StorageType *pixels;
  const size_t max_size = max(width, height);
  if (max_size == 0) {
    /* Don't bother with empty images. */
    return false;
  }

  /* Allocate memory as needed, may be smaller to resize down. */
  if (texture_limit > 0 && max_size > texture_limit) {
    pixels_storage.resize(((size_t)width) * height * 4);
    pixels = &pixels_storage[0];
  }
  else {
    const thread_scoped_lock device_lock(device_mutex);
    pixels = (StorageType *)img->mem->alloc(width, height);
  }

  if (pixels == nullptr) {
    /* Could be that we've run out of memory. */
    return false;
  }

  const size_t num_pixels = ((size_t)width) * height;
  img->loader->load_pixels(
      img->metadata, pixels, num_pixels * components, image_associate_alpha(img));

  /* The kernel can handle 1 and 4 channel images. Anything that is not a single
   * channel image is converted to RGBA format. */
  const bool is_rgba = (img->metadata.type == IMAGE_DATA_TYPE_FLOAT4 ||
                        img->metadata.type == IMAGE_DATA_TYPE_HALF4 ||
                        img->metadata.type == IMAGE_DATA_TYPE_BYTE4 ||
                        img->metadata.type == IMAGE_DATA_TYPE_USHORT4);

  if (is_rgba) {
    const StorageType one = util_image_cast_from_float<StorageType>(1.0f);

    if (components == 2) {
      /* Grayscale + alpha to RGBA. */
      for (size_t i = num_pixels - 1, pixel = 0; pixel < num_pixels; pixel++, i--) {
        pixels[i * 4 + 3] = pixels[i * 2 + 1];
        pixels[i * 4 + 2] = pixels[i * 2 + 0];
        pixels[i * 4 + 1] = pixels[i * 2 + 0];
        pixels[i * 4 + 0] = pixels[i * 2 + 0];
      }
    }
    else if (components == 3) {
      /* RGB to RGBA. */
      for (size_t i = num_pixels - 1, pixel = 0; pixel < num_pixels; pixel++, i--) {
        pixels[i * 4 + 3] = one;
        pixels[i * 4 + 2] = pixels[i * 3 + 2];
        pixels[i * 4 + 1] = pixels[i * 3 + 1];
        pixels[i * 4 + 0] = pixels[i * 3 + 0];
      }
    }
    else if (components == 1) {
      /* Grayscale to RGBA. */
      for (size_t i = num_pixels - 1, pixel = 0; pixel < num_pixels; pixel++, i--) {
        pixels[i * 4 + 3] = one;
        pixels[i * 4 + 2] = pixels[i];
        pixels[i * 4 + 1] = pixels[i];
        pixels[i * 4 + 0] = pixels[i];
      }
    }

    /* Disable alpha if requested by the user. */
    if (img->params.alpha_type == IMAGE_ALPHA_IGNORE) {
      for (size_t i = num_pixels - 1, pixel = 0; pixel < num_pixels; pixel++, i--) {
        pixels[i * 4 + 3] = one;
      }
    }
  }

  if (img->metadata.colorspace != u_colorspace_raw &&
      img->metadata.colorspace != u_colorspace_srgb)
  {
    /* Convert to scene linear. */
    ColorSpaceManager::to_scene_linear(
        img->metadata.colorspace, pixels, num_pixels, is_rgba, img->metadata.compress_as_srgb);
  }

  /* Make sure we don't have buggy values. */
  if constexpr (FileFormat == TypeDesc::FLOAT) {
    /* For RGBA buffers we put all channels to 0 if either of them is not
     * finite. This way we avoid possible artifacts caused by fully changed
     * hue. */
    if (is_rgba) {
      for (size_t i = 0; i < num_pixels; i += 4) {
        StorageType *pixel = &pixels[i * 4];
        if (!isfinite(pixel[0]) || !isfinite(pixel[1]) || !isfinite(pixel[2]) ||
            !isfinite(pixel[3]))
        {
          pixel[0] = 0;
          pixel[1] = 0;
          pixel[2] = 0;
          pixel[3] = 0;
        }
      }
    }
    else {
      for (size_t i = 0; i < num_pixels; ++i) {
        StorageType *pixel = &pixels[i];
        if (!isfinite(pixel[0])) {
          pixel[0] = 0;
        }
      }
    }
  }

  /* Scale image down if needed. */
  if (!pixels_storage.empty()) {
    float scale_factor = 1.0f;
    while (max_size * scale_factor > texture_limit) {
      scale_factor *= 0.5f;
    }
    LOG_WORK << "Scaling image " << img->loader->name() << " by a factor of " << scale_factor
             << ".";
    vector<StorageType> scaled_pixels;
    size_t scaled_width;
    size_t scaled_height;
    util_image_resize_pixels(pixels_storage,
                             width,
                             height,
                             is_rgba ? 4 : 1,
                             scale_factor,
                             &scaled_pixels,
                             &scaled_width,
                             &scaled_height);

    StorageType *texture_pixels;

    {
      const thread_scoped_lock device_lock(device_mutex);
      texture_pixels = (StorageType *)img->mem->alloc(scaled_width, scaled_height);
    }

    memcpy(texture_pixels, &scaled_pixels[0], scaled_pixels.size() * sizeof(StorageType));
  }

  return true;
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

  progress.set_status("Updating Images", "Loading " + img->loader->name());

  const int texture_limit = scene->params.texture_limit;

  load_image_metadata(img);
  const ImageDataType type = img->metadata.type;

  /* Name for debugging. */
  img->mem_name = string_printf("tex_image_%s_%03d", name_from_type(type), (int)slot);

  /* Free previous texture in slot. */
  if (img->mem) {
    const thread_scoped_lock device_lock(device_mutex);
    img->mem.reset();
  }

  img->mem = make_unique<device_texture>(
      device, img->mem_name.c_str(), slot, type, img->params.interpolation, img->params.extension);
  img->mem->info.use_transform_3d = img->metadata.use_transform_3d;
  img->mem->info.transform_3d = img->metadata.transform_3d;

  /* Create new texture. */
  if (type == IMAGE_DATA_TYPE_FLOAT4) {
    if (!file_load_image<TypeDesc::FLOAT, float>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      const thread_scoped_lock device_lock(device_mutex);
      float *pixels = (float *)img->mem->alloc(1, 1);

      pixels[0] = TEX_IMAGE_MISSING_R;
      pixels[1] = TEX_IMAGE_MISSING_G;
      pixels[2] = TEX_IMAGE_MISSING_B;
      pixels[3] = TEX_IMAGE_MISSING_A;
    }
  }
  else if (type == IMAGE_DATA_TYPE_FLOAT) {
    if (!file_load_image<TypeDesc::FLOAT, float>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      const thread_scoped_lock device_lock(device_mutex);
      float *pixels = (float *)img->mem->alloc(1, 1);

      pixels[0] = TEX_IMAGE_MISSING_R;
    }
  }
  else if (type == IMAGE_DATA_TYPE_BYTE4) {
    if (!file_load_image<TypeDesc::UINT8, uchar>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      const thread_scoped_lock device_lock(device_mutex);
      uchar *pixels = (uchar *)img->mem->alloc(1, 1);

      pixels[0] = (TEX_IMAGE_MISSING_R * 255);
      pixels[1] = (TEX_IMAGE_MISSING_G * 255);
      pixels[2] = (TEX_IMAGE_MISSING_B * 255);
      pixels[3] = (TEX_IMAGE_MISSING_A * 255);
    }
  }
  else if (type == IMAGE_DATA_TYPE_BYTE) {
    if (!file_load_image<TypeDesc::UINT8, uchar>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      const thread_scoped_lock device_lock(device_mutex);
      uchar *pixels = (uchar *)img->mem->alloc(1, 1);

      pixels[0] = (TEX_IMAGE_MISSING_R * 255);
    }
  }
  else if (type == IMAGE_DATA_TYPE_HALF4) {
    if (!file_load_image<TypeDesc::HALF, half>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      const thread_scoped_lock device_lock(device_mutex);
      half *pixels = (half *)img->mem->alloc(1, 1);

      pixels[0] = TEX_IMAGE_MISSING_R;
      pixels[1] = TEX_IMAGE_MISSING_G;
      pixels[2] = TEX_IMAGE_MISSING_B;
      pixels[3] = TEX_IMAGE_MISSING_A;
    }
  }
  else if (type == IMAGE_DATA_TYPE_USHORT) {
    if (!file_load_image<TypeDesc::USHORT, uint16_t>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      const thread_scoped_lock device_lock(device_mutex);
      uint16_t *pixels = (uint16_t *)img->mem->alloc(1, 1);

      pixels[0] = (TEX_IMAGE_MISSING_R * 65535);
    }
  }
  else if (type == IMAGE_DATA_TYPE_USHORT4) {
    if (!file_load_image<TypeDesc::USHORT, uint16_t>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      const thread_scoped_lock device_lock(device_mutex);
      uint16_t *pixels = (uint16_t *)img->mem->alloc(1, 1);

      pixels[0] = (TEX_IMAGE_MISSING_R * 65535);
      pixels[1] = (TEX_IMAGE_MISSING_G * 65535);
      pixels[2] = (TEX_IMAGE_MISSING_B * 65535);
      pixels[3] = (TEX_IMAGE_MISSING_A * 65535);
    }
  }
  else if (type == IMAGE_DATA_TYPE_HALF) {
    if (!file_load_image<TypeDesc::HALF, half>(img, texture_limit)) {
      /* on failure to load, we set a 1x1 pixels pink image */
      const thread_scoped_lock device_lock(device_mutex);
      half *pixels = (half *)img->mem->alloc(1, 1);

      pixels[0] = TEX_IMAGE_MISSING_R;
    }
  }
#ifdef WITH_NANOVDB
  else if (is_nanovdb_type(type)) {
    const thread_scoped_lock device_lock(device_mutex);
    void *pixels = img->mem->alloc(img->metadata.byte_size, 0);

    if (pixels != nullptr) {
      img->loader->load_pixels(img->metadata, pixels, img->metadata.byte_size, false);
    }
  }
#endif

  {
    const thread_scoped_lock device_lock(device_mutex);
    img->mem->copy_to_device();
  }

  /* Cleanup memory in image loader. */
  img->loader->cleanup();
  img->need_load = false;
}

void ImageManager::device_free_image(Device * /*unused*/, size_t slot)
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
    img->mem.reset();
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
      device_free_image(device, slot);
    }
    else if (img && img->need_load) {
      pool.push([this, device, scene, slot, &progress] {
        device_load_image(device, scene, slot, progress);
      });
    }
  }

  pool.wait_work();

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
    device_free_image(device, slot);
  }
  else if (img->need_load) {
    device_load_image(device, scene, slot, progress);
  }
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

void ImageManager::device_free_builtin(Device *device)
{
  for (size_t slot = 0; slot < images.size(); slot++) {
    Image *img = images[slot].get();
    if (img && img->builtin) {
      device_free_image(device, slot);
    }
  }
}

void ImageManager::device_free(Device *device)
{
  for (size_t slot = 0; slot < images.size(); slot++) {
    device_free_image(device, slot);
  }
  images.clear();
}

void ImageManager::collect_statistics(RenderStats *stats)
{
  for (const unique_ptr<Image> &image : images) {
    if (!image) {
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
